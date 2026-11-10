#include "primec/ir/Ir.h"
#include "primec/ir/IrSerializer.h"
#include "primec/ir/IrValidation.h"
#include "primec/runtime/Vm.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace primec;

// The condition opcodes on the VM (docs/spec/vm-design.md, Conditions) with hand-built IR.
TEST_SUITE_BEGIN("primestruct.runtime.conditions");

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

// The instructions every main here starts with: locals 0, 1 and 2 are the addresses of three fresh heap slots, a
// free lock, a condition and a flag that is 0.
std::vector<IrInstruction> withSlots(std::vector<IrInstruction> body) {
  std::vector<IrInstruction> instructions;
  for (uint64_t local = 0; local < 3; ++local) {
    instructions.push_back({IrOpcode::PushI32, 1});
    instructions.push_back({IrOpcode::HeapAlloc, 0});
    instructions.push_back({IrOpcode::StoreLocal, local});
  }
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

std::vector<IrInstruction> waitOn(uint64_t condition, uint64_t lock) {
  return {
      {IrOpcode::LoadLocal, condition}, {IrOpcode::LoadLocal, lock}, {IrOpcode::ConditionWait, 0}};
}

std::vector<IrInstruction> notifyOf(uint64_t condition) {
  return {{IrOpcode::LoadLocal, condition}, {IrOpcode::ConditionNotify, 0}};
}

std::vector<IrInstruction> print(uint64_t value) {
  return {{IrOpcode::PushI32, value}, {IrOpcode::PrintI32, Newline}};
}

std::vector<IrInstruction> returnZero() {
  return {{IrOpcode::PushI32, 0}, {IrOpcode::ReturnI32, 0}};
}

// waiter(lock, condition, flag): takes the lock, waits on the condition until the flag is set (taking the lock
// again after each wait), prints 2 and frees the lock.
IrFunction waiterTask() {
  std::vector<IrInstruction> body = {
      {IrOpcode::StoreLocal, 2}, {IrOpcode::StoreLocal, 1}, {IrOpcode::StoreLocal, 0}};
  append(body, lockOf(0));
  const uint64_t check = body.size();
  // flag != 0 leaves the loop: JumpIfZero jumps when (flag == 0) is 0.
  append(body,
         {{IrOpcode::LoadLocal, 2},
          {IrOpcode::AtomicLoad, 64},
          {IrOpcode::PushI64, 0},
          {IrOpcode::CmpEqI64, 0}});
  const uint64_t exitJump = body.size();
  body.push_back({IrOpcode::JumpIfZero, 0});
  append(body, waitOn(1, 0));
  append(body, lockOf(0));
  body.push_back({IrOpcode::Jump, check});
  body[exitJump].imm = body.size();
  append(body, print(2));
  append(body, unlockOf(0));
  body.push_back({IrOpcode::ReturnVoid, 0});
  return functionOf("/waiter", 3, std::move(body));
}

// Spawns function 1 with the three slots and keeps its handle in local 3.
std::vector<IrInstruction> spawnWaiter() {
  return {{IrOpcode::LoadLocal, 0},
          {IrOpcode::LoadLocal, 1},
          {IrOpcode::LoadLocal, 2},
          {IrOpcode::TaskSpawn, 1},
          {IrOpcode::StoreLocal, 3}};
}

std::vector<IrInstruction> waitTask() {
  return {{IrOpcode::LoadLocal, 3}, {IrOpcode::TaskWait, 0}, {IrOpcode::Pop, 0}};
}

IrModule notifyOnly() {
  std::vector<IrInstruction> body = notifyOf(1);
  append(body, returnZero());
  return moduleOf({functionOf("/main", 0, withSlots(std::move(body)))});
}

} // namespace

TEST_CASE("one call stack notifies nobody and cannot wait") {
  const IrModule notify = notifyOnly();
  std::string error;
  REQUIRE_MESSAGE(validateIrModule(notify, IrValidationTarget::Vm, error), error);
  const Run notified = run(notify);
  REQUIRE_MESSAGE(notified.ok, notified.error);
  CHECK(notified.value == 0);

  // No other task exists to notify the condition.
  std::vector<IrInstruction> waiting = lockOf(0);
  append(waiting, waitOn(1, 0));
  append(waiting, returnZero());
  const IrModule waits = moduleOf({functionOf("/main", 0, withSlots(std::move(waiting)))});
  REQUIRE_MESSAGE(validateIrModule(waits, IrValidationTarget::Vm, error), error);
  const Run waited = run(waits);
  CHECK_FALSE(waited.ok);
  CHECK(waited.error == "deadlock: every task is waiting");

  std::vector<IrInstruction> unheld = waitOn(1, 0);
  append(unheld, returnZero());
  const Run unheldRun = run(moduleOf({functionOf("/main", 0, withSlots(std::move(unheld)))}));
  CHECK_FALSE(unheldRun.ok);
  CHECK(unheldRun.error == "condition wait by a task that does not hold the lock");
}

TEST_CASE("a task waits on a condition until the main task sets the flag and notifies, under every "
          "seed") {
  // main: spawn the waiter; lock, set the flag, print 5, notify, unlock; wait the task. The waiter prints 2 only
  // after it has seen the flag, which is set with the lock held, so 5 is always first. On the default schedule the
  // waiter runs first and waits, so main can take the lock only because the wait freed it.
  std::vector<IrInstruction> body = spawnWaiter();
  append(body, lockOf(0));
  append(body, {{IrOpcode::LoadLocal, 2}, {IrOpcode::PushI64, 1}, {IrOpcode::AtomicStore, 64}});
  append(body, print(5));
  append(body, notifyOf(1));
  append(body, unlockOf(0));
  append(body, waitTask());
  append(body, returnZero());
  const IrModule module =
      moduleOf({functionOf("/main", 0, withSlots(std::move(body))), waiterTask()});
  std::string error;
  REQUIRE_MESSAGE(validateIrModule(module, IrValidationTarget::Vm, error), error);
  const Run eager = run(module);
  REQUIRE_MESSAGE(eager.ok, eager.error);
  CHECK(eager.out == "5\n2\n");
  for (uint64_t seed = 1; seed <= 32; ++seed) {
    const Run seeded = run(module, seed);
    REQUIRE_MESSAGE(seeded.ok, seeded.error);
    CHECK_MESSAGE(seeded.out == "5\n2\n", "seed " << seed);
  }
}

TEST_CASE("a notify wakes every task that waits on the condition") {
  // Two waiters; one notify after the flag is set lets both finish.
  std::vector<IrInstruction> body = spawnWaiter();
  append(body,
         {{IrOpcode::LoadLocal, 0},
          {IrOpcode::LoadLocal, 1},
          {IrOpcode::LoadLocal, 2},
          {IrOpcode::TaskSpawn, 1},
          {IrOpcode::StoreLocal, 4}});
  append(body, lockOf(0));
  append(body, {{IrOpcode::LoadLocal, 2}, {IrOpcode::PushI64, 1}, {IrOpcode::AtomicStore, 64}});
  append(body, notifyOf(1));
  append(body, unlockOf(0));
  append(body, waitTask());
  append(body, {{IrOpcode::LoadLocal, 4}, {IrOpcode::TaskWait, 0}, {IrOpcode::Pop, 0}});
  append(body, returnZero());
  const IrModule module =
      moduleOf({functionOf("/main", 0, withSlots(std::move(body))), waiterTask()});
  std::string error;
  REQUIRE_MESSAGE(validateIrModule(module, IrValidationTarget::Vm, error), error);
  const Run eager = run(module);
  REQUIRE_MESSAGE(eager.ok, eager.error);
  CHECK(eager.out == "2\n2\n");
  for (uint64_t seed = 1; seed <= 32; ++seed) {
    const Run seeded = run(module, seed);
    REQUIRE_MESSAGE(seeded.ok, seeded.error);
    CHECK_MESSAGE(seeded.out == "2\n2\n", "seed " << seed);
  }
}

TEST_CASE(
    "a wait nobody notifies is a deadlock, and a notify on another condition does not wake it") {
  std::vector<IrInstruction> body = spawnWaiter();
  append(body, waitTask());
  append(body, returnZero());
  const IrModule module =
      moduleOf({functionOf("/main", 0, withSlots(std::move(body))), waiterTask()});
  const Run eager = run(module);
  CHECK_FALSE(eager.ok);
  CHECK(eager.error == "deadlock: every task is waiting");

  // The flag slot is used as a second condition here: its notify leaves the waiter asleep.
  std::vector<IrInstruction> other = spawnWaiter();
  append(other, notifyOf(2));
  append(other, waitTask());
  append(other, returnZero());
  const IrModule otherModule =
      moduleOf({functionOf("/main", 0, withSlots(std::move(other))), waiterTask()});
  for (uint64_t seed = 1; seed <= 8; ++seed) {
    for (const IrModule *each : {&module, &otherModule}) {
      const Run seeded = run(*each, seed);
      CHECK_FALSE(seeded.ok);
      CHECK_MESSAGE(seeded.error == "deadlock: every task is waiting", "seed " << seed);
    }
  }
}

TEST_CASE("a task that waits without holding the lock faults") {
  // The main task holds the lock; the task waits with it.
  std::vector<IrInstruction> task = {
      {IrOpcode::StoreLocal, 2}, {IrOpcode::StoreLocal, 1}, {IrOpcode::StoreLocal, 0}};
  append(task, waitOn(1, 0));
  task.push_back({IrOpcode::ReturnVoid, 0});
  std::vector<IrInstruction> body = lockOf(0);
  append(body, spawnWaiter());
  append(body, waitTask());
  append(body, returnZero());
  const IrModule module = moduleOf({functionOf("/main", 0, withSlots(std::move(body))),
                                    functionOf("/unheld", 3, std::move(task))});
  const Run eager = run(module);
  CHECK_FALSE(eager.ok);
  CHECK(eager.error == "condition wait by a task that does not hold the lock");
  for (uint64_t seed = 1; seed <= 8; ++seed) {
    const Run seeded = run(module, seed);
    CHECK_FALSE(seeded.ok);
    CHECK_MESSAGE(seeded.error == "condition wait by a task that does not hold the lock",
                  "seed " << seed);
  }
}

TEST_CASE("a condition operation faults on a bad address and needs its operands") {
  const IrModule badAddress = moduleOf({functionOf("/main",
                                                   0,
                                                   {{IrOpcode::PushI64, 48},
                                                    {IrOpcode::ConditionNotify, 0},
                                                    {IrOpcode::PushI32, 0},
                                                    {IrOpcode::ReturnI32, 0}})});
  const Run addressRun = run(badAddress);
  CHECK_FALSE(addressRun.ok);
  CHECK(addressRun.error.find("invalid indirect address") != std::string::npos);

  const IrModule underflow = moduleOf({functionOf("/main",
                                                  0,
                                                  {{IrOpcode::PushI64, 48},
                                                   {IrOpcode::ConditionWait, 0},
                                                   {IrOpcode::PushI32, 0},
                                                   {IrOpcode::ReturnI32, 0}})});
  std::string error;
  CHECK_FALSE(validateIrModule(underflow, IrValidationTarget::Vm, error));
  const Run underflowRun = run(underflow);
  CHECK_FALSE(underflowRun.ok);
  CHECK(underflowRun.error == "IR stack underflow on condition operation");
}

TEST_CASE("the native, wasm and glsl targets reject a module with condition opcodes as one with "
          "channels") {
  const IrModule module = notifyOnly();
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
    CHECK_MESSAGE(error.find(std::string("channel operations are not supported by the ") + name +
                             " backend") != std::string::npos,
                  error);
  }
}

TEST_CASE("condition opcodes survive serialization and a v36 module cannot hold them") {
  const IrModule module = notifyOnly();
  std::string error;
  std::vector<uint8_t> bytes;
  REQUIRE_MESSAGE(serializeIr(module, bytes, error), error);
  IrModule back;
  REQUIRE_MESSAGE(deserializeIr(bytes, back, error), error);
  CHECK(back.schemaVersion == IrSchemaVersion);
  CHECK(IrSchemaVersion >= 37u);
  const Run result = run(back);
  REQUIRE_MESSAGE(result.ok, result.error);

  // The version field follows the magic. v36 has the mutex opcodes and no condition ones.
  const auto setVersion = [](std::vector<uint8_t> &data, uint32_t version) {
    for (size_t i = 0; i < 4; ++i) {
      data[4 + i] = static_cast<uint8_t>((version >> (8 * i)) & 0xFF);
    }
  };
  setVersion(bytes, 36u);
  IrModule rejected;
  CHECK_FALSE(deserializeIr(bytes, rejected, error));
  CHECK(error == "unsupported IR opcode");

  std::vector<IrInstruction> locks = {
      {IrOpcode::PushI32, 1}, {IrOpcode::HeapAlloc, 0}, {IrOpcode::StoreLocal, 0}};
  append(locks, lockOf(0));
  append(locks, unlockOf(0));
  append(locks, {{IrOpcode::PushI32, 11}, {IrOpcode::ReturnI32, 0}});
  std::vector<uint8_t> old;
  REQUIRE_MESSAGE(serializeIr(moduleOf({functionOf("/main", 0, std::move(locks))}), old, error),
                  error);
  setVersion(old, 36u);
  IrModule loaded;
  REQUIRE_MESSAGE(deserializeIr(old, loaded, error), error);
  CHECK(loaded.schemaVersion == 36u);
  CHECK(run(loaded).value == 11);
}

TEST_SUITE_END();
