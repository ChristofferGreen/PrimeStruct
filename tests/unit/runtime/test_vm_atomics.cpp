#include "primec/ir/Ir.h"
#include "primec/ir/IrSerializer.h"
#include "primec/ir/IrValidation.h"
#include "primec/runtime/Vm.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace primec;

// The atomic opcodes on the VM (docs/spec/vm-design.md, Atomics) with hand-built IR.
TEST_SUITE_BEGIN("primestruct.runtime.atomics");

namespace {

IrFunction
functionOf(std::string name, uint32_t parameterCount, std::vector<IrInstruction> instructions) {
  IrFunction function;
  function.name = std::move(name);
  function.parameterCount = parameterCount;
  function.instructions = std::move(instructions);
  return function;
}

IrModule moduleOf(std::vector<IrFunction> functions) {
  IrModule module;
  module.functions = std::move(functions);
  module.entryIndex = 0;
  return module;
}

struct Run {
  bool ok = false;
  int32_t value = 0;
  std::string error;
  std::string out;
};

void appendOutput(int fd, std::string_view chunk, void *userData) {
  if (fd == 1) {
    static_cast<std::string *>(userData)->append(chunk);
  }
}

Run run(const IrModule &module, std::optional<uint64_t> seed = std::nullopt) {
  Run result;
  Vm vm;
  vm.setOutputSink({&appendOutput, &result.out});
  vm.setScheduleSeed(seed);
  uint64_t raw = 0;
  result.ok = vm.execute(module, raw, result.error, std::vector<std::string_view>{"prog"});
  result.value = static_cast<int32_t>(raw);
  return result;
}

constexpr uint64_t Newline = PrintFlagNewline;

IrInstruction pushI64(int64_t value) {
  return {IrOpcode::PushI64, static_cast<uint64_t>(value)};
}

// The instructions every program here starts with: local 0 is the address of a fresh heap slot.
std::vector<IrInstruction> withCell(std::vector<IrInstruction> body) {
  std::vector<IrInstruction> instructions = {
      {IrOpcode::PushI32, 1}, {IrOpcode::HeapAlloc, 0}, {IrOpcode::StoreLocal, 0}};
  instructions.insert(instructions.end(), body.begin(), body.end());
  return instructions;
}

// One operation of width `width` on the cell, with `operands` pushed after the address; a result is printed.
void appendOperation(std::vector<IrInstruction> &out,
                     IrOpcode op,
                     uint64_t width,
                     std::vector<int64_t> operands) {
  out.push_back({IrOpcode::LoadLocal, 0});
  for (const int64_t operand : operands) {
    out.push_back(pushI64(operand));
  }
  out.push_back({op, width});
  if (op != IrOpcode::AtomicStore) {
    out.push_back({IrOpcode::PrintI64, Newline});
  }
}

// f(cell): prints 1, loads the cell atomically, prints 2.
IrFunction loadingTask() {
  return functionOf("/load",
                    1,
                    {{IrOpcode::StoreLocal, 0},
                     {IrOpcode::PushI32, 1},
                     {IrOpcode::PrintI32, Newline},
                     {IrOpcode::LoadLocal, 0},
                     {IrOpcode::AtomicLoad, 64},
                     {IrOpcode::Pop, 0},
                     {IrOpcode::PushI32, 2},
                     {IrOpcode::PrintI32, Newline},
                     {IrOpcode::ReturnVoid, 0}});
}

// main: two tasks run loadingTask on one cell.
IrModule twoLoadingTasks() {
  return moduleOf({functionOf("/main",
                              0,
                              withCell({{IrOpcode::LoadLocal, 0},
                                        {IrOpcode::TaskSpawn, 1},
                                        {IrOpcode::StoreLocal, 1},
                                        {IrOpcode::LoadLocal, 0},
                                        {IrOpcode::TaskSpawn, 1},
                                        {IrOpcode::StoreLocal, 2},
                                        {IrOpcode::LoadLocal, 1},
                                        {IrOpcode::TaskWait, 0},
                                        {IrOpcode::Pop, 0},
                                        {IrOpcode::LoadLocal, 2},
                                        {IrOpcode::TaskWait, 0},
                                        {IrOpcode::Pop, 0},
                                        {IrOpcode::PushI32, 0},
                                        {IrOpcode::ReturnI32, 0}})),
                   loadingTask()});
}

IrModule oneOperation(IrOpcode op, uint64_t width, std::vector<int64_t> operands) {
  std::vector<IrInstruction> body;
  appendOperation(body, op, width, std::move(operands));
  body.push_back({IrOpcode::PushI32, 0});
  body.push_back({IrOpcode::ReturnI32, 0});
  return moduleOf({functionOf("/main", 0, withCell(std::move(body)))});
}

} // namespace

TEST_CASE("the atomic operations read and write one heap slot") {
  std::vector<IrInstruction> body;
  appendOperation(body, IrOpcode::AtomicStore, 64, {5});
  appendOperation(body, IrOpcode::AtomicAdd, 64, {3});
  appendOperation(body, IrOpcode::AtomicLoad, 64, {});
  appendOperation(body, IrOpcode::AtomicExchange, 64, {20});
  appendOperation(body, IrOpcode::AtomicCompareExchange, 64, {20, 1});
  appendOperation(body, IrOpcode::AtomicCompareExchange, 64, {20, 2});
  appendOperation(body, IrOpcode::AtomicLoad, 64, {});
  // A 64-bit value keeps all its bits.
  appendOperation(body, IrOpcode::AtomicStore, 64, {int64_t{1} << 40});
  appendOperation(body, IrOpcode::AtomicAdd, 64, {-1});
  appendOperation(body, IrOpcode::AtomicLoad, 64, {});
  body.push_back({IrOpcode::PushI32, 7});
  body.push_back({IrOpcode::ReturnI32, 0});
  const IrModule module = moduleOf({functionOf("/main", 0, withCell(std::move(body)))});
  std::string error;
  REQUIRE_MESSAGE(validateIrModule(module, IrValidationTarget::Vm, error), error);
  const Run result = run(module);
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.value == 7);
  CHECK(result.out == "5\n8\n8\n1\n0\n1\n1099511627776\n1099511627775\n");
}

TEST_CASE("a 32-bit atomic operation wraps and keeps the slot sign-extended") {
  std::vector<IrInstruction> body;
  appendOperation(body, IrOpcode::AtomicStore, 32, {2147483647});
  appendOperation(body, IrOpcode::AtomicAdd, 32, {1});
  appendOperation(body, IrOpcode::AtomicLoad, 32, {});
  // The whole slot holds the i32 sign-extended, so a 64-bit read sees the same number.
  appendOperation(body, IrOpcode::AtomicLoad, 64, {});
  appendOperation(body, IrOpcode::AtomicExchange, 32, {-5});
  appendOperation(body, IrOpcode::AtomicCompareExchange, 32, {-5, 9});
  appendOperation(body, IrOpcode::AtomicLoad, 32, {});
  // Operands wider than 32 bits are cut to the width.
  appendOperation(body, IrOpcode::AtomicStore, 32, {(int64_t{1} << 32) + 3});
  appendOperation(body, IrOpcode::AtomicLoad, 64, {});
  body.push_back({IrOpcode::PushI32, 0});
  body.push_back({IrOpcode::ReturnI32, 0});
  const Run result = run(moduleOf({functionOf("/main", 0, withCell(std::move(body)))}));
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.out == "2147483647\n-2147483648\n-2147483648\n-2147483648\n1\n9\n3\n");
}

TEST_CASE("an atomic operation is a switch point only under a schedule seed") {
  const IrModule module = twoLoadingTasks();
  std::string error;
  REQUIRE_MESSAGE(validateIrModule(module, IrValidationTarget::Vm, error), error);
  // The default schedule runs each task to its end.
  const Run eager = run(module);
  REQUIRE_MESSAGE(eager.ok, eager.error);
  CHECK(eager.out == "1\n2\n1\n2\n");
  // Under a seed another task may run after the load: some seed prints both 1s before a 2, and a seed always gives
  // the same run.
  std::set<std::string> outputs;
  for (uint64_t seed = 1; seed <= 16; ++seed) {
    const Run first = run(module, seed);
    REQUIRE_MESSAGE(first.ok, first.error);
    CHECK(run(module, seed).out == first.out);
    CHECK((first.out == "1\n2\n1\n2\n" || first.out == "1\n1\n2\n2\n"));
    outputs.insert(first.out);
  }
  CHECK(outputs.count("1\n1\n2\n2\n") == 1);
}

TEST_CASE("an atomic operation faults on a bad address or width") {
  const IrModule badAddress = moduleOf({functionOf(
      "/main", 0, {pushI64(48), {IrOpcode::AtomicLoad, 64}, {IrOpcode::ReturnI32, 0}})});
  const Run addressRun = run(badAddress);
  CHECK_FALSE(addressRun.ok);
  CHECK(addressRun.error.find("invalid indirect address") != std::string::npos);

  const IrModule badWidth = oneOperation(IrOpcode::AtomicLoad, 16, {});
  std::string error;
  CHECK_FALSE(validateIrModule(badWidth, IrValidationTarget::Vm, error));
  CHECK(error.find("atomic width must be 32 or 64") != std::string::npos);
  const Run widthRun = run(badWidth);
  CHECK_FALSE(widthRun.ok);
  CHECK(widthRun.error == "invalid atomic width in IR");

  const IrModule underflow = moduleOf({functionOf(
      "/main", 0, {pushI64(0), {IrOpcode::AtomicCompareExchange, 64}, {IrOpcode::ReturnI32, 0}})});
  CHECK_FALSE(validateIrModule(underflow, IrValidationTarget::Vm, error));
}

TEST_CASE("the native, wasm and glsl targets reject a module with atomic opcodes by name") {
  const IrModule module = oneOperation(IrOpcode::AtomicAdd, 64, {1});
  std::string error;
  CHECK_MESSAGE(validateIrModule(module, IrValidationTarget::Vm, error), error);
  CHECK_MESSAGE(validateIrModule(module, IrValidationTarget::Serialized, error), error);
  CHECK_MESSAGE(validateIrModule(module, IrValidationTarget::OptCpp, error), error);
  CHECK_MESSAGE(validateIrModule(module, IrValidationTarget::Cpp, error), error);
  const std::pair<IrValidationTarget, const char *> rejecting[] = {
      {IrValidationTarget::Native, "native"},
      {IrValidationTarget::Wasm, "wasm"},
      {IrValidationTarget::WasmBrowser, "wasm-browser"},
      {IrValidationTarget::Glsl, "glsl"},
  };
  for (const auto &[target, name] : rejecting) {
    error.clear();
    CHECK_FALSE(validateIrModule(module, target, error));
    CHECK_MESSAGE(error.find(std::string("atomic operations are not supported by the ") + name +
                             " backend") != std::string::npos,
                  error);
  }
}

TEST_CASE("atomic opcodes survive serialization and a v34 module cannot hold them") {
  const IrModule module = oneOperation(IrOpcode::AtomicExchange, 32, {4});
  std::string error;
  std::vector<uint8_t> bytes;
  REQUIRE_MESSAGE(serializeIr(module, bytes, error), error);
  IrModule back;
  REQUIRE_MESSAGE(deserializeIr(bytes, back, error), error);
  CHECK(back.schemaVersion == IrSchemaVersion);
  CHECK(IrSchemaVersion >= 35u);
  const Run result = run(back);
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.out == "0\n");

  // The version field follows the magic. v34 has the task opcodes and no atomic ones.
  const auto setVersion = [](std::vector<uint8_t> &data, uint32_t version) {
    for (size_t i = 0; i < 4; ++i) {
      data[4 + i] = static_cast<uint8_t>((version >> (8 * i)) & 0xFF);
    }
  };
  setVersion(bytes, 34u);
  IrModule rejected;
  CHECK_FALSE(deserializeIr(bytes, rejected, error));
  CHECK(error == "unsupported IR opcode");

  const IrModule tasks = moduleOf(
      {functionOf("/main",
                  0,
                  {{IrOpcode::TaskSpawn, 1}, {IrOpcode::TaskWait, 0}, {IrOpcode::ReturnI32, 0}}),
       functionOf("/eleven", 0, {{IrOpcode::PushI32, 11}, {IrOpcode::ReturnI32, 0}})});
  std::vector<uint8_t> old;
  REQUIRE_MESSAGE(serializeIr(tasks, old, error), error);
  setVersion(old, 34u);
  IrModule loaded;
  REQUIRE_MESSAGE(deserializeIr(old, loaded, error), error);
  CHECK(loaded.schemaVersion == 34u);
  CHECK(run(loaded).value == 11);
}

TEST_SUITE_END();
