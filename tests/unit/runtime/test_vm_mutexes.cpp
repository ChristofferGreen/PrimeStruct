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

// The mutex opcodes on the VM (docs/spec/vm-design.md, Mutexes) with hand-built IR.
TEST_SUITE_BEGIN("primestruct.runtime.mutexes");

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

// The instructions every main here starts with: locals 0 and 1 are the addresses of two fresh heap slots (two free
// locks).
std::vector<IrInstruction> withLocks(std::vector<IrInstruction> body) {
  std::vector<IrInstruction> instructions = {{IrOpcode::PushI32, 1},
                                             {IrOpcode::HeapAlloc, 0},
                                             {IrOpcode::StoreLocal, 0},
                                             {IrOpcode::PushI32, 1},
                                             {IrOpcode::HeapAlloc, 0},
                                             {IrOpcode::StoreLocal, 1}};
  instructions.insert(instructions.end(), body.begin(), body.end());
  return instructions;
}

void append(std::vector<IrInstruction> &out, std::vector<IrInstruction> more) {
  out.insert(out.end(), more.begin(), more.end());
}

std::vector<IrInstruction> lockOf(uint64_t local) {
  return {{IrOpcode::LoadLocal, local}, {IrOpcode::MutexLock, 0}};
}

std::vector<IrInstruction> unlockOf(uint64_t local) {
  return {{IrOpcode::LoadLocal, local}, {IrOpcode::MutexUnlock, 0}};
}

std::vector<IrInstruction> print(uint64_t value) {
  return {{IrOpcode::PushI32, value}, {IrOpcode::PrintI32, Newline}};
}

// section(lock): takes the lock, prints 1, reads the slot atomically (a switch point under a seed), prints 2 and
// frees the lock.
IrFunction sectionTask() {
  std::vector<IrInstruction> body = {{IrOpcode::StoreLocal, 0}};
  append(body, lockOf(0));
  append(body, print(1));
  append(body, {{IrOpcode::LoadLocal, 0}, {IrOpcode::AtomicLoad, 64}, {IrOpcode::Pop, 0}});
  append(body, print(2));
  append(body, unlockOf(0));
  body.push_back({IrOpcode::ReturnVoid, 0});
  return functionOf("/section", 1, std::move(body));
}

// both(first, second): takes first, then second, and frees them in reverse.
IrFunction bothTask() {
  std::vector<IrInstruction> body = {{IrOpcode::StoreLocal, 1}, {IrOpcode::StoreLocal, 0}};
  append(body, lockOf(0));
  append(body, lockOf(1));
  append(body, unlockOf(1));
  append(body, unlockOf(0));
  body.push_back({IrOpcode::ReturnVoid, 0});
  return functionOf("/both", 2, std::move(body));
}

// Spawns function 1 twice with the given argument locals, waits both and returns 0.
std::vector<IrInstruction> spawnTwoAndWait(std::vector<uint64_t> firstArguments,
                                           std::vector<uint64_t> secondArguments) {
  std::vector<IrInstruction> body;
  for (const uint64_t local : firstArguments) {
    body.push_back({IrOpcode::LoadLocal, local});
  }
  append(body, {{IrOpcode::TaskSpawn, 1}, {IrOpcode::StoreLocal, 2}});
  for (const uint64_t local : secondArguments) {
    body.push_back({IrOpcode::LoadLocal, local});
  }
  append(body,
         {{IrOpcode::TaskSpawn, 1},
          {IrOpcode::StoreLocal, 3},
          {IrOpcode::LoadLocal, 2},
          {IrOpcode::TaskWait, 0},
          {IrOpcode::Pop, 0},
          {IrOpcode::LoadLocal, 3},
          {IrOpcode::TaskWait, 0},
          {IrOpcode::Pop, 0},
          {IrOpcode::PushI32, 0},
          {IrOpcode::ReturnI32, 0}});
  return body;
}

IrModule lockOnly() {
  std::vector<IrInstruction> body;
  append(body, lockOf(0));
  append(body, unlockOf(0));
  append(body, {{IrOpcode::PushI32, 0}, {IrOpcode::ReturnI32, 0}});
  return moduleOf({functionOf("/main", 0, withLocks(std::move(body)))});
}

} // namespace

TEST_CASE("one call stack takes and frees a lock") {
  std::vector<IrInstruction> body;
  append(body, lockOf(0));
  append(body, print(1));
  // The other lock is free while this one is held.
  append(body, lockOf(1));
  append(body, unlockOf(1));
  append(body, unlockOf(0));
  // A freed lock can be taken again.
  append(body, lockOf(0));
  append(body, print(2));
  append(body, unlockOf(0));
  append(body, {{IrOpcode::PushI32, 7}, {IrOpcode::ReturnI32, 0}});
  const IrModule module = moduleOf({functionOf("/main", 0, withLocks(std::move(body)))});
  std::string error;
  REQUIRE_MESSAGE(validateIrModule(module, IrValidationTarget::Vm, error), error);
  const Run result = run(module);
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.value == 7);
  CHECK(result.out == "1\n2\n");
}

TEST_CASE("locking a held lock again or freeing a lock not held faults") {
  std::vector<IrInstruction> again;
  append(again, lockOf(0));
  append(again, lockOf(0));
  append(again, {{IrOpcode::PushI32, 0}, {IrOpcode::ReturnI32, 0}});
  const Run againRun = run(moduleOf({functionOf("/main", 0, withLocks(std::move(again)))}));
  CHECK_FALSE(againRun.ok);
  CHECK(againRun.error == "mutex locked again by the task that holds it");

  std::vector<IrInstruction> freeUnlock;
  append(freeUnlock, unlockOf(0));
  append(freeUnlock, {{IrOpcode::PushI32, 0}, {IrOpcode::ReturnI32, 0}});
  const Run freeRun = run(moduleOf({functionOf("/main", 0, withLocks(std::move(freeUnlock)))}));
  CHECK_FALSE(freeRun.ok);
  CHECK(freeRun.error == "mutex unlocked by a task that does not hold it");

  // The same faults with tasks in the module, where the scheduler runs the lock: a task locks twice, and a task
  // frees the lock the main task holds.
  std::vector<IrInstruction> twice = {{IrOpcode::StoreLocal, 0}};
  append(twice, lockOf(0));
  append(twice, lockOf(0));
  twice.push_back({IrOpcode::ReturnVoid, 0});
  const Run taskAgain = run(moduleOf({functionOf("/main", 0, withLocks(spawnTwoAndWait({0}, {1}))),
                                      functionOf("/twice", 1, std::move(twice))}));
  CHECK_FALSE(taskAgain.ok);
  CHECK(taskAgain.error == "mutex locked again by the task that holds it");

  std::vector<IrInstruction> steal = {{IrOpcode::StoreLocal, 0}};
  append(steal, unlockOf(0));
  steal.push_back({IrOpcode::ReturnVoid, 0});
  std::vector<IrInstruction> holder = lockOf(0);
  append(holder, spawnTwoAndWait({0}, {0}));
  const Run stolen = run(moduleOf({functionOf("/main", 0, withLocks(std::move(holder))),
                                   functionOf("/steal", 1, std::move(steal))}));
  CHECK_FALSE(stolen.ok);
  CHECK(stolen.error == "mutex unlocked by a task that does not hold it");
}

TEST_CASE("a lock keeps two tasks out of each other's section under every seed") {
  const IrModule module =
      moduleOf({functionOf("/main", 0, withLocks(spawnTwoAndWait({0}, {0}))), sectionTask()});
  std::string error;
  REQUIRE_MESSAGE(validateIrModule(module, IrValidationTarget::Vm, error), error);
  const Run eager = run(module);
  REQUIRE_MESSAGE(eager.ok, eager.error);
  CHECK(eager.out == "1\n2\n1\n2\n");
  // The atomic read between the prints is a switch point, so without the lock some seed prints 1 1 2 2.
  for (uint64_t seed = 1; seed <= 32; ++seed) {
    const Run seeded = run(module, seed);
    REQUIRE_MESSAGE(seeded.ok, seeded.error);
    CHECK_MESSAGE(seeded.out == "1\n2\n1\n2\n", "seed " << seed);
  }
  // Two tasks with a lock each do interleave.
  const IrModule apart =
      moduleOf({functionOf("/main", 0, withLocks(spawnTwoAndWait({0}, {1}))), sectionTask()});
  std::set<std::string> outputs;
  for (uint64_t seed = 1; seed <= 32; ++seed) {
    const Run seeded = run(apart, seed);
    REQUIRE_MESSAGE(seeded.ok, seeded.error);
    outputs.insert(seeded.out);
  }
  CHECK(outputs.count("1\n1\n2\n2\n") == 1);
}

TEST_CASE("a task waits for a lock the main task holds and gets it after the unlock") {
  // main: lock, spawn section (it waits), print 5, unlock, wait the task.
  std::vector<IrInstruction> body = lockOf(0);
  append(body, {{IrOpcode::LoadLocal, 0}, {IrOpcode::TaskSpawn, 1}, {IrOpcode::StoreLocal, 2}});
  append(body, print(5));
  append(body, unlockOf(0));
  append(body,
         {{IrOpcode::LoadLocal, 2},
          {IrOpcode::TaskWait, 0},
          {IrOpcode::Pop, 0},
          {IrOpcode::PushI32, 0},
          {IrOpcode::ReturnI32, 0}});
  const IrModule module =
      moduleOf({functionOf("/main", 0, withLocks(std::move(body))), sectionTask()});
  std::string error;
  REQUIRE_MESSAGE(validateIrModule(module, IrValidationTarget::Vm, error), error);
  const Run eager = run(module);
  REQUIRE_MESSAGE(eager.ok, eager.error);
  CHECK(eager.out == "5\n1\n2\n");
  for (uint64_t seed = 1; seed <= 16; ++seed) {
    const Run seeded = run(module, seed);
    REQUIRE_MESSAGE(seeded.ok, seeded.error);
    CHECK_MESSAGE(seeded.out == "5\n1\n2\n", "seed " << seed);
  }
}

TEST_CASE("waiting a task that needs a lock the waiter holds is a deadlock") {
  std::vector<IrInstruction> body = lockOf(0);
  append(body,
         {{IrOpcode::LoadLocal, 0},
          {IrOpcode::TaskSpawn, 1},
          {IrOpcode::TaskWait, 0},
          {IrOpcode::Pop, 0},
          {IrOpcode::PushI32, 0},
          {IrOpcode::ReturnI32, 0}});
  const IrModule module =
      moduleOf({functionOf("/main", 0, withLocks(std::move(body))), sectionTask()});
  const Run eager = run(module);
  CHECK_FALSE(eager.ok);
  CHECK(eager.error == "deadlock: every task is waiting");
  for (uint64_t seed = 1; seed <= 8; ++seed) {
    const Run seeded = run(module, seed);
    CHECK_FALSE(seeded.ok);
    CHECK_MESSAGE(seeded.error == "deadlock: every task is waiting", "seed " << seed);
  }
}

TEST_CASE("two tasks taking two locks in opposite order deadlock on a seed that interleaves them") {
  const IrModule module =
      moduleOf({functionOf("/main", 0, withLocks(spawnTwoAndWait({0, 1}, {1, 0}))), bothTask()});
  std::string error;
  REQUIRE_MESSAGE(validateIrModule(module, IrValidationTarget::Vm, error), error);
  // The default schedule runs each task to its end, so the orders never meet.
  const Run eager = run(module);
  REQUIRE_MESSAGE(eager.ok, eager.error);
  size_t deadlocks = 0;
  size_t finished = 0;
  for (uint64_t seed = 1; seed <= 32; ++seed) {
    const Run first = run(module, seed);
    const Run second = run(module, seed);
    CHECK(first.ok == second.ok);
    CHECK(first.error == second.error);
    if (first.ok) {
      ++finished;
    } else {
      CHECK_MESSAGE(first.error == "deadlock: every task is waiting", "seed " << seed);
      ++deadlocks;
    }
  }
  CHECK(deadlocks > 0);
  CHECK(finished > 0);
}

TEST_CASE("a mutex operation faults on a bad address and needs its operand") {
  const IrModule badAddress = moduleOf({functionOf("/main",
                                                   0,
                                                   {{IrOpcode::PushI64, 48},
                                                    {IrOpcode::MutexLock, 0},
                                                    {IrOpcode::PushI32, 0},
                                                    {IrOpcode::ReturnI32, 0}})});
  const Run addressRun = run(badAddress);
  CHECK_FALSE(addressRun.ok);
  CHECK(addressRun.error.find("invalid indirect address") != std::string::npos);

  const IrModule underflow = moduleOf({functionOf(
      "/main", 0, {{IrOpcode::MutexUnlock, 0}, {IrOpcode::PushI32, 0}, {IrOpcode::ReturnI32, 0}})});
  std::string error;
  CHECK_FALSE(validateIrModule(underflow, IrValidationTarget::Vm, error));
  const Run underflowRun = run(underflow);
  CHECK_FALSE(underflowRun.ok);
  CHECK(underflowRun.error == "IR stack underflow on mutex operation");
}

TEST_CASE("the wasm and glsl targets (and native off arm64 macOS) reject a module with mutex "
          "opcodes by name") {
  const IrModule module = lockOnly();
  std::string error;
  CHECK_MESSAGE(validateIrModule(module, IrValidationTarget::Vm, error), error);
  CHECK_MESSAGE(validateIrModule(module, IrValidationTarget::Serialized, error), error);
  CHECK_MESSAGE(validateIrModule(module, IrValidationTarget::OptCpp, error), error);
  CHECK_MESSAGE(validateIrModule(module, IrValidationTarget::Cpp, error), error);
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
  CHECK_MESSAGE(validateIrModule(module, IrValidationTarget::Native, error), error);
#endif
  const std::pair<IrValidationTarget, const char *> rejecting[] = {
#if !(defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__)))
      {IrValidationTarget::Native, "native"},
#endif
      {IrValidationTarget::Wasm, "wasm"},
      {IrValidationTarget::WasmBrowser, "wasm-browser"},
      {IrValidationTarget::Glsl, "glsl"},
  };
  for (const auto &[target, name] : rejecting) {
    error.clear();
    CHECK_FALSE(validateIrModule(module, target, error));
    CHECK_MESSAGE(error.find(std::string("mutex operations are not supported by the ") + name +
                             " backend") != std::string::npos,
                  error);
  }
}

TEST_CASE("mutex opcodes survive serialization and a v35 module cannot hold them") {
  const IrModule module = lockOnly();
  std::string error;
  std::vector<uint8_t> bytes;
  REQUIRE_MESSAGE(serializeIr(module, bytes, error), error);
  IrModule back;
  REQUIRE_MESSAGE(deserializeIr(bytes, back, error), error);
  CHECK(back.schemaVersion == IrSchemaVersion);
  CHECK(IrSchemaVersion >= 36u);
  const Run result = run(back);
  REQUIRE_MESSAGE(result.ok, result.error);

  // The version field follows the magic. v35 has the atomic opcodes and no mutex ones.
  const auto setVersion = [](std::vector<uint8_t> &data, uint32_t version) {
    for (size_t i = 0; i < 4; ++i) {
      data[4 + i] = static_cast<uint8_t>((version >> (8 * i)) & 0xFF);
    }
  };
  setVersion(bytes, 35u);
  IrModule rejected;
  CHECK_FALSE(deserializeIr(bytes, rejected, error));
  CHECK(error == "unsupported IR opcode");

  const IrModule atomics = moduleOf({functionOf("/main",
                                                0,
                                                {{IrOpcode::PushI32, 1},
                                                 {IrOpcode::HeapAlloc, 0},
                                                 {IrOpcode::PushI64, 11},
                                                 {IrOpcode::AtomicExchange, 64},
                                                 {IrOpcode::Pop, 0},
                                                 {IrOpcode::PushI32, 11},
                                                 {IrOpcode::ReturnI32, 0}})});
  std::vector<uint8_t> old;
  REQUIRE_MESSAGE(serializeIr(atomics, old, error), error);
  setVersion(old, 35u);
  IrModule loaded;
  REQUIRE_MESSAGE(deserializeIr(old, loaded, error), error);
  CHECK(loaded.schemaVersion == 35u);
  CHECK(run(loaded).value == 11);
}

TEST_SUITE_END();
