#include "primec/ir/Ir.h"
#include "primec/ir/IrSerializer.h"
#include "primec/ir/IrValidation.h"
#include "primec/runtime/Vm.h"
#include "primec/testing/VmKernelSelection.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using namespace primec;

// The VM's task scheduler (docs/spec/vm-design.md, Tasks) with hand-built IR.
TEST_SUITE_BEGIN("primestruct.runtime.tasks");

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

IrInstruction pushI32(int32_t value) {
  return {IrOpcode::PushI32, static_cast<uint64_t>(value)};
}

// f(x): prints x, returns x * 2.
IrFunction doublingTask() {
  return functionOf("/double",
                    1,
                    {{IrOpcode::StoreLocal, 0},
                     {IrOpcode::LoadLocal, 0},
                     {IrOpcode::PrintI32, Newline},
                     {IrOpcode::LoadLocal, 0},
                     {IrOpcode::PushI32, 2},
                     {IrOpcode::MulI32, 0},
                     {IrOpcode::ReturnI32, 0}});
}

// main: spawns double(20) and double(30), prints 1 between the spawns and 2 after them, then waits both and
// returns the sum of the results.
IrModule twoTasks() {
  return moduleOf({functionOf("/main",
                              0,
                              {{IrOpcode::PushI32, 20},
                               {IrOpcode::TaskSpawn, 1},
                               {IrOpcode::StoreLocal, 0},
                               pushI32(1),
                               {IrOpcode::PrintI32, Newline},
                               {IrOpcode::PushI32, 30},
                               {IrOpcode::TaskSpawn, 1},
                               {IrOpcode::StoreLocal, 1},
                               pushI32(2),
                               {IrOpcode::PrintI32, Newline},
                               {IrOpcode::LoadLocal, 0},
                               {IrOpcode::TaskWait, 0},
                               {IrOpcode::LoadLocal, 1},
                               {IrOpcode::TaskWait, 0},
                               {IrOpcode::AddI32, 0},
                               {IrOpcode::ReturnI32, 0}}),
                   doublingTask()});
}

} // namespace

TEST_CASE("the default schedule runs a spawned task before its spawner continues") {
  const Run result = run(twoTasks());
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.value == 100);
  // Exactly what the program prints with each spawned call run inline.
  CHECK(result.out == "20\n1\n30\n2\n");
}

TEST_CASE("a seeded schedule is reproducible and keeps every task's own order") {
  std::set<std::string> outputs;
  for (uint64_t seed = 0; seed < 32; ++seed) {
    CAPTURE(seed);
    const Run first = run(twoTasks(), seed);
    const Run second = run(twoTasks(), seed);
    REQUIRE_MESSAGE(first.ok, first.error);
    CHECK(first.value == 100);
    CHECK(second.out == first.out);
    // main prints 1 before 2, and each task prints once.
    CHECK(first.out.find("1\n") < first.out.find("2\n"));
    CHECK(first.out.find("20\n") != std::string::npos);
    CHECK(first.out.find("30\n") != std::string::npos);
    CHECK(first.out.size() == std::string("20\n1\n30\n2\n").size());
    outputs.insert(first.out);
  }
  // The seeds pick different interleavings, not only the eager one.
  CHECK(outputs.size() > 1);
}

TEST_CASE("a task that spawns and waits its own task resumes before the main task") {
  // main spawns outer and prints 9; outer spawns double(4), waits it, prints the result. Run inline, that prints
  // 4, 8 and then 9.
  const IrModule module = moduleOf({functionOf("/main",
                                               0,
                                               {{IrOpcode::TaskSpawn, 1},
                                                {IrOpcode::StoreLocal, 0},
                                                pushI32(9),
                                                {IrOpcode::PrintI32, Newline},
                                                {IrOpcode::LoadLocal, 0},
                                                {IrOpcode::TaskWait, 0},
                                                {IrOpcode::ReturnI32, 0}}),
                                    functionOf("/outer",
                                               0,
                                               {{IrOpcode::PushI32, 4},
                                                {IrOpcode::TaskSpawn, 2},
                                                {IrOpcode::StoreLocal, 0},
                                                {IrOpcode::LoadLocal, 0},
                                                {IrOpcode::TaskWait, 0},
                                                {IrOpcode::Dup, 0},
                                                {IrOpcode::PrintI32, Newline},
                                                {IrOpcode::ReturnI32, 0}}),
                                    doublingTask()});
  const Run eager = run(module);
  REQUIRE_MESSAGE(eager.ok, eager.error);
  CHECK(eager.out == "4\n8\n9\n");
  CHECK(eager.value == 8);
  for (uint64_t seed = 1; seed <= 8; ++seed) {
    CAPTURE(seed);
    const Run seeded = run(module, seed);
    REQUIRE_MESSAGE(seeded.ok, seeded.error);
    CHECK(seeded.value == 8);
    CHECK(seeded.out.find("4\n") < seeded.out.find("8\n"));
  }
}

TEST_CASE("a task without a result is waited for its effects") {
  const IrModule module = moduleOf({functionOf("/main",
                                               0,
                                               {{IrOpcode::PushI32, 5},
                                                {IrOpcode::TaskSpawn, 1},
                                                {IrOpcode::TaskWait, 0},
                                                {IrOpcode::Pop, 0},
                                                pushI32(6),
                                                {IrOpcode::PrintI32, Newline},
                                                {IrOpcode::PushI32, 0},
                                                {IrOpcode::ReturnI32, 0}}),
                                    functionOf("/hello",
                                               1,
                                               {{IrOpcode::StoreLocal, 0},
                                                {IrOpcode::LoadLocal, 0},
                                                {IrOpcode::PrintI32, Newline},
                                                {IrOpcode::ReturnVoid, 0}})});
  for (const std::optional<uint64_t> seed :
       {std::optional<uint64_t>{}, std::optional<uint64_t>{3}}) {
    const Run result = run(module, seed);
    REQUIRE_MESSAGE(result.ok, result.error);
    CHECK(result.out == "5\n6\n");
  }
}

TEST_CASE("tasks spawned and waited in a loop reuse their slots") {
  // sum of double(i) for i in 0..9999, each spawned and waited in the loop body.
  const IrModule module = moduleOf({functionOf("/main",
                                               0,
                                               {{IrOpcode::PushI32, 0},
                                                {IrOpcode::StoreLocal, 0}, // i
                                                {IrOpcode::PushI64, 0},
                                                {IrOpcode::StoreLocal, 1}, // total
                                                {IrOpcode::LoadLocal, 0},
                                                {IrOpcode::PushI32, 10000},
                                                {IrOpcode::CmpLtI32, 0},
                                                {IrOpcode::JumpIfZero, 18},
                                                {IrOpcode::LoadLocal, 0},
                                                {IrOpcode::TaskSpawn, 1},
                                                {IrOpcode::TaskWait, 0},
                                                {IrOpcode::LoadLocal, 1},
                                                {IrOpcode::AddI64, 0},
                                                {IrOpcode::StoreLocal, 1},
                                                {IrOpcode::LoadLocal, 0},
                                                {IrOpcode::PushI32, 1},
                                                {IrOpcode::AddI32, 0},
                                                {IrOpcode::StoreLocal, 0},
                                                // 18: loop back or leave
                                                {IrOpcode::LoadLocal, 0},
                                                {IrOpcode::PushI32, 10000},
                                                {IrOpcode::CmpLtI32, 0},
                                                {IrOpcode::JumpIfZero, 23},
                                                {IrOpcode::Jump, 8},
                                                // 23
                                                {IrOpcode::LoadLocal, 1},
                                                {IrOpcode::PushI64, 1000},
                                                {IrOpcode::DivI64, 0},
                                                {IrOpcode::ReturnI64, 0}}),
                                    functionOf("/double",
                                               1,
                                               {{IrOpcode::StoreLocal, 0},
                                                {IrOpcode::LoadLocal, 0},
                                                {IrOpcode::PushI32, 2},
                                                {IrOpcode::MulI32, 0},
                                                {IrOpcode::ReturnI32, 0}})});
  std::string error;
  REQUIRE_MESSAGE(validateIrModule(module, IrValidationTarget::Vm, error), error);
  for (const std::optional<uint64_t> seed :
       {std::optional<uint64_t>{}, std::optional<uint64_t>{7}}) {
    const Run result = run(module, seed);
    REQUIRE_MESSAGE(result.ok, result.error);
    CHECK(result.value == 99990); // 2 * (9999 * 10000 / 2) / 1000
  }
}

TEST_CASE("a trap in a task ends the program with its code") {
  const IrModule module = moduleOf(
      {functionOf("/main",
                  0,
                  {{IrOpcode::TaskSpawn, 1}, {IrOpcode::TaskWait, 0}, {IrOpcode::ReturnI32, 0}}),
       functionOf("/fails", 0, {{IrOpcode::Trap, 3}})});
  for (const std::optional<uint64_t> seed :
       {std::optional<uint64_t>{}, std::optional<uint64_t>{1}}) {
    const Run result = run(module, seed);
    REQUIRE_MESSAGE(result.ok, result.error);
    CHECK(result.value == 3);
  }
}

TEST_CASE("waiting a handle twice or a value that is not a handle faults") {
  const IrModule twice = moduleOf({functionOf("/main",
                                              0,
                                              {{IrOpcode::PushI32, 1},
                                               {IrOpcode::TaskSpawn, 1},
                                               {IrOpcode::Dup, 0},
                                               {IrOpcode::TaskWait, 0},
                                               {IrOpcode::Pop, 0},
                                               {IrOpcode::TaskWait, 0},
                                               {IrOpcode::ReturnI32, 0}}),
                                   doublingTask()});
  const Run again = run(twice);
  CHECK_FALSE(again.ok);
  CHECK(again.error == "task handle already waited");

  const IrModule notHandle = moduleOf({functionOf(
      "/main", 0, {{IrOpcode::PushI32, 7}, {IrOpcode::TaskWait, 0}, {IrOpcode::ReturnI32, 0}})});
  const Run invalid = run(notHandle);
  CHECK_FALSE(invalid.ok);
  CHECK(invalid.error == "invalid task handle in IR");

  const IrModule badFunction =
      moduleOf({functionOf("/main", 0, {{IrOpcode::TaskSpawn, 9}, {IrOpcode::ReturnI32, 0}})});
  const Run missing = run(badFunction);
  CHECK_FALSE(missing.ok);
  CHECK(missing.error == "invalid task function in IR");
}

TEST_CASE("validation rejects a wait whose operand is not a task handle on every path") {
  std::string error;
  const IrModule direct = moduleOf({functionOf(
      "/main", 0, {{IrOpcode::PushI32, 7}, {IrOpcode::TaskWait, 0}, {IrOpcode::ReturnI32, 0}})});
  CHECK_FALSE(validateIrModule(direct, IrValidationTarget::Vm, error));
  CHECK(error.find("task wait operand is not a task handle") != std::string::npos);

  // Through a local and a Dup it is still the handle.
  CHECK_MESSAGE(validateIrModule(twoTasks(), IrValidationTarget::Vm, error), error);

  // A local that holds a handle on one path and a number on the other is not a handle after the join.
  const IrModule joined = moduleOf({functionOf("/main",
                                               0,
                                               {{IrOpcode::PushArgc, 0},
                                                {IrOpcode::JumpIfZero, 5},
                                                {IrOpcode::PushI32, 1},
                                                {IrOpcode::TaskSpawn, 1},
                                                {IrOpcode::StoreLocal, 0},
                                                // 5
                                                {IrOpcode::LoadLocal, 0},
                                                {IrOpcode::TaskWait, 0},
                                                {IrOpcode::ReturnI32, 0}}),
                                    doublingTask()});
  error.clear();
  CHECK_FALSE(validateIrModule(joined, IrValidationTarget::Vm, error));
  CHECK(error.find("task wait operand is not a task handle") != std::string::npos);

  const IrModule badFunction =
      moduleOf({functionOf("/main", 0, {{IrOpcode::TaskSpawn, 9}, {IrOpcode::ReturnI32, 0}})});
  error.clear();
  CHECK_FALSE(validateIrModule(badFunction, IrValidationTarget::Vm, error));
  CHECK(error.find("invalid task function") != std::string::npos);
}

TEST_CASE("only the vm, optcpp and arm64 native targets and serialized bytecode accept the task "
          "opcodes") {
  std::string error;
  CHECK(validateIrModule(twoTasks(), IrValidationTarget::Vm, error));
  CHECK(validateIrModule(twoTasks(), IrValidationTarget::Serialized, error));
  CHECK(validateIrModule(twoTasks(), IrValidationTarget::OptCpp, error));
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
  CHECK(validateIrModule(twoTasks(), IrValidationTarget::Native, error));
  const std::vector<IrValidationTarget> rejecting = {
      IrValidationTarget::Cpp, IrValidationTarget::Wasm, IrValidationTarget::Glsl};
#else
  const std::vector<IrValidationTarget> rejecting = {IrValidationTarget::Native,
                                                     IrValidationTarget::Cpp,
                                                     IrValidationTarget::Wasm,
                                                     IrValidationTarget::Glsl};
#endif
  for (const IrValidationTarget target : rejecting) {
    error.clear();
    CHECK_FALSE(validateIrModule(twoTasks(), target, error));
    CHECK(
        error.find(
            "task opcodes are only supported by the vm, optimized C++ and arm64 native targets and "
            "serialized bytecode") != std::string::npos);
  }
}

TEST_CASE("task opcodes survive serialization and a v33 module still loads") {
  const IrModule module = twoTasks();
  std::string error;
  std::vector<uint8_t> bytes;
  REQUIRE_MESSAGE(serializeIr(module, bytes, error), error);
  IrModule back;
  REQUIRE_MESSAGE(deserializeIr(bytes, back, error), error);
  CHECK(back.schemaVersion == IrSchemaVersion);
  const Run result = run(back);
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.value == 100);

  // The version field follows the magic; v33 is the previous version, which has no task opcodes.
  const auto setVersion = [](std::vector<uint8_t> &data, uint32_t version) {
    for (size_t i = 0; i < 4; ++i) {
      data[4 + i] = static_cast<uint8_t>((version >> (8 * i)) & 0xFF);
    }
  };
  CHECK(IrSchemaVersion >= 34u);
  CHECK(IrSchemaMinimumSupportedVersion == 33u);
  const IrModule plain =
      moduleOf({functionOf("/main", 0, {{IrOpcode::PushI32, 12}, {IrOpcode::ReturnI32, 0}})});
  std::vector<uint8_t> old;
  REQUIRE_MESSAGE(serializeIr(plain, old, error), error);
  setVersion(old, 33u);
  IrModule loaded;
  REQUIRE_MESSAGE(deserializeIr(old, loaded, error), error);
  CHECK(loaded.schemaVersion == 33u);
  CHECK(run(loaded).value == 12);
  // It is written back as the current version.
  std::vector<uint8_t> rewritten;
  REQUIRE_MESSAGE(serializeIr(loaded, rewritten, error), error);
  CHECK(rewritten[4] == static_cast<uint8_t>(IrSchemaVersion));

  setVersion(bytes, 33u);
  IrModule rejected;
  CHECK_FALSE(deserializeIr(bytes, rejected, error));
  CHECK(error == "unsupported IR opcode");
  setVersion(bytes, 32u);
  CHECK_FALSE(deserializeIr(bytes, rejected, error));
  CHECK(error == "unsupported IR schema version 32 (supported 33..37)");
}

namespace {

// Turns the flat loop on or off for a scope and restores it.
struct FlatLoopScope {
  explicit FlatLoopScope(bool enabled) {
    testing::setVmFastKernelEnabled(enabled);
  }
  ~FlatLoopScope() {
    testing::setVmFastKernelEnabled(true);
  }
};

// main spawns /outer, which calls /middle, which spawns double(7) and waits it two frames deep, with a local of
// each frame holding a value across the switch points; /middle also writes through the address of /outer's local.
IrModule nestedSwitchPoints() {
  return moduleOf({functionOf("/main",
                              0,
                              {{IrOpcode::TaskSpawn, 1},
                               {IrOpcode::TaskWait, 0},
                               {IrOpcode::PushI32, 1},
                               {IrOpcode::AddI32, 0},
                               {IrOpcode::ReturnI32, 0}}),
                   functionOf("/outer",
                              0,
                              {{IrOpcode::PushI32, 10},
                               {IrOpcode::StoreLocal, 0},
                               {IrOpcode::AddressOfLocal, 1},
                               {IrOpcode::Call, 2},
                               {IrOpcode::LoadLocal, 0},
                               {IrOpcode::AddI32, 0},
                               {IrOpcode::LoadLocal, 1},
                               {IrOpcode::AddI32, 0},
                               {IrOpcode::ReturnI32, 0}}),
                   functionOf("/middle",
                              1,
                              {{IrOpcode::StoreLocal, 0},
                               {IrOpcode::PushI32, 100},
                               {IrOpcode::StoreLocal, 1},
                               {IrOpcode::PushI32, 7},
                               {IrOpcode::TaskSpawn, 3},
                               {IrOpcode::StoreLocal, 2},
                               {IrOpcode::LoadLocal, 2},
                               {IrOpcode::TaskWait, 0},
                               {IrOpcode::StoreLocal, 2},
                               {IrOpcode::LoadLocal, 0},
                               {IrOpcode::LoadLocal, 2},
                               {IrOpcode::StoreIndirect, 0},
                               {IrOpcode::Pop, 0},
                               {IrOpcode::LoadLocal, 1},
                               {IrOpcode::ReturnI32, 0}}),
                   doublingTask()});
}

} // namespace

TEST_CASE("tasks run on the flat loop and agree with the step kernel") {
  const std::vector<IrModule> modules = {twoTasks(), nestedSwitchPoints()};
  for (size_t index = 0; index < modules.size(); ++index) {
    CAPTURE(index);
    for (const std::optional<uint64_t> seed : {std::optional<uint64_t>{},
                                               std::optional<uint64_t>{1},
                                               std::optional<uint64_t>{2},
                                               std::optional<uint64_t>{9}}) {
      CAPTURE(seed.value_or(0));
      Run step;
      {
        const FlatLoopScope off(false);
        const uint64_t before = testing::vmFastTaskRunCount();
        step = run(modules[index], seed);
        CHECK(testing::vmFastTaskRunCount() == before);
      }
      const uint64_t before = testing::vmFastTaskRunCount();
      const Run fast = run(modules[index], seed);
      // The structural claim: the scheduler gave the whole run to the flat loop.
      CHECK(testing::vmFastTaskRunCount() == before + 1);
      REQUIRE_MESSAGE(step.ok, step.error);
      REQUIRE_MESSAGE(fast.ok, fast.error);
      CHECK(fast.value == step.value);
      CHECK(fast.out == step.out);
    }
  }
  const Run nested = run(nestedSwitchPoints());
  REQUIRE_MESSAGE(nested.ok, nested.error);
  // /middle stores double(7) = 14 into /outer's local 1 and returns 100; /outer returns 100 + 10 + 14 and /main
  // adds 1.
  CHECK(nested.value == 125);
}

TEST_CASE("a fault of a task on the flat loop reads as on the step kernel") {
  const IrModule badWait = moduleOf({functionOf(
      "/main", 0, {{IrOpcode::PushI32, 7}, {IrOpcode::TaskWait, 0}, {IrOpcode::ReturnI32, 0}})});
  const IrModule badAddress = moduleOf({functionOf("/main",
                                                   0,
                                                   {{IrOpcode::PushI32, 1},
                                                    {IrOpcode::TaskSpawn, 1},
                                                    {IrOpcode::TaskWait, 0},
                                                    {IrOpcode::ReturnI32, 0}}),
                                        functionOf("/atomic",
                                                   1,
                                                   {{IrOpcode::StoreLocal, 0},
                                                    {IrOpcode::PushI64, 4096},
                                                    {IrOpcode::AtomicLoad, 32},
                                                    {IrOpcode::ReturnI32, 0}})});
  for (const IrModule &module : {badWait, badAddress}) {
    Run step;
    {
      const FlatLoopScope off(false);
      step = run(module);
    }
    const Run fast = run(module);
    CHECK_FALSE(step.ok);
    CHECK_FALSE(fast.ok);
    CHECK(fast.error == step.error);
  }
}

TEST_SUITE_END();
