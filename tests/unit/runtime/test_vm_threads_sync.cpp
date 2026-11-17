#include "primec/ir/Ir.h"
#include "primec/runtime/Vm.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace primec;

// Atomics, mutexes and conditions under the VM's threads mode (docs/spec/vm-design.md, Tasks on threads;
// `--vm-threads`) with hand-built IR. Nothing here sleeps or measures time: every program either has one answer
// whatever the interleaving or faults with a diagnostic, and each case runs it Repeats times, so a race or a lost
// wake-up shows as a failure of the whole entry.
TEST_SUITE_BEGIN("primestruct.runtime.vm_threads_sync");

namespace {

constexpr int Repeats = 100;
constexpr uint64_t Newline = PrintFlagNewline;
constexpr const char *Deadlock = "deadlock: every task is waiting";

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

Run run(const IrModule &module, bool threads) {
  Run result;
  Vm vm;
  vm.setOutputSink({&appendOutput, &result.out});
  vm.setTaskThreads(threads);
  uint64_t raw = 0;
  result.ok = vm.execute(module, raw, result.error, std::vector<std::string_view>{"prog"});
  result.value = static_cast<int32_t>(raw);
  return result;
}

void append(std::vector<IrInstruction> &out, std::vector<IrInstruction> more) {
  out.insert(out.end(), more.begin(), more.end());
}

// The instructions every main here starts with: locals 0 to count-1 are the addresses of fresh heap slots, all 0.
std::vector<IrInstruction> withSlots(uint64_t count, std::vector<IrInstruction> body) {
  std::vector<IrInstruction> instructions;
  for (uint64_t local = 0; local < count; ++local) {
    append(instructions,
           {{IrOpcode::PushI32, 1}, {IrOpcode::HeapAlloc, 0}, {IrOpcode::StoreLocal, local}});
  }
  append(instructions, std::move(body));
  return instructions;
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

// Spawns function `function` with the locals `arguments` and keeps the handle in local `handle`.
std::vector<IrInstruction>
spawnOf(uint64_t function, const std::vector<uint64_t> &arguments, uint64_t handle) {
  std::vector<IrInstruction> out;
  for (const uint64_t local : arguments) {
    out.push_back({IrOpcode::LoadLocal, local});
  }
  append(out, {{IrOpcode::TaskSpawn, function}, {IrOpcode::StoreLocal, handle}});
  return out;
}

std::vector<IrInstruction> waitTask(uint64_t handle) {
  return {{IrOpcode::LoadLocal, handle}, {IrOpcode::TaskWait, 0}, {IrOpcode::Pop, 0}};
}

// A task of `parameters` parameters whose last one is a repeat count: runs `body` (stack neutral, parameters in
// locals 0 to parameters-1) that many times.
IrFunction loopTask(std::string name, uint32_t parameters, std::vector<IrInstruction> body) {
  std::vector<IrInstruction> code;
  for (uint32_t local = parameters; local-- > 0;) {
    code.push_back({IrOpcode::StoreLocal, local});
  }
  append(code, {{IrOpcode::PushI64, 0}, {IrOpcode::StoreLocal, parameters}});
  const uint64_t head = code.size();
  append(code,
         {{IrOpcode::LoadLocal, parameters},
          {IrOpcode::LoadLocal, parameters - 1},
          {IrOpcode::CmpLtI64, 0}});
  const uint64_t exitJump = code.size();
  code.push_back({IrOpcode::JumpIfZero, 0});
  append(code, std::move(body));
  append(code,
         {{IrOpcode::LoadLocal, parameters},
          {IrOpcode::PushI64, 1},
          {IrOpcode::AddI64, 0},
          {IrOpcode::StoreLocal, parameters},
          {IrOpcode::Jump, head}});
  code[exitJump].imm = code.size();
  code.push_back({IrOpcode::ReturnVoid, 0});
  return functionOf(std::move(name), parameters, std::move(code));
}

// A task body fragment: spins until the atomic slot in `local` is not 0, written for code that starts at `start`.
std::vector<IrInstruction> spinFragment(uint64_t local, uint64_t start) {
  return {{IrOpcode::LoadLocal, local},
          {IrOpcode::AtomicLoad, 64},
          {IrOpcode::PushI64, 0},
          {IrOpcode::CmpEqI64, 0},
          {IrOpcode::JumpIfZero, start + 6},
          {IrOpcode::Jump, start}};
}

void spinThen(std::vector<IrInstruction> &code, uint64_t local) {
  append(code, spinFragment(local, code.size()));
}

} // namespace

TEST_CASE("four tasks add to one atomic and none is lost") {
  // main: four tasks of 1000 AtomicAdds on one slot; prints the sum.
  std::vector<IrInstruction> body;
  for (uint64_t task = 0; task < 4; ++task) {
    append(body, {{IrOpcode::LoadLocal, 0}, {IrOpcode::PushI64, 1000}});
    append(body, {{IrOpcode::TaskSpawn, 1}, {IrOpcode::StoreLocal, 1 + task}});
  }
  for (uint64_t task = 0; task < 4; ++task) {
    append(body, waitTask(1 + task));
  }
  append(body,
         {{IrOpcode::LoadLocal, 0}, {IrOpcode::AtomicLoad, 64}, {IrOpcode::PrintI64, Newline}});
  append(body, returnZero());
  const IrModule module = moduleOf({functionOf("/main", 0, withSlots(1, std::move(body))),
                                    loopTask("/add",
                                             2,
                                             {{IrOpcode::LoadLocal, 0},
                                              {IrOpcode::PushI64, 1},
                                              {IrOpcode::AtomicAdd, 64},
                                              {IrOpcode::Pop, 0}})});
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    CAPTURE(repeat);
    const Run threaded = run(module, true);
    REQUIRE_MESSAGE(threaded.ok, threaded.error);
    CHECK(threaded.out == "4000\n");
  }
}

TEST_CASE("two tasks that finish only if they run at the same time finish") {
  // first(a, b): spins until a is set, then sets b. second(a, b): sets a, then spins until b is set. Neither can
  // finish unless the other runs while it spins.
  std::vector<IrInstruction> first = {{IrOpcode::StoreLocal, 1}, {IrOpcode::StoreLocal, 0}};
  spinThen(first, 0);
  append(first, {{IrOpcode::LoadLocal, 1}, {IrOpcode::PushI64, 1}, {IrOpcode::AtomicStore, 64}});
  first.push_back({IrOpcode::ReturnVoid, 0});
  std::vector<IrInstruction> second = {{IrOpcode::StoreLocal, 1}, {IrOpcode::StoreLocal, 0}};
  append(second, {{IrOpcode::LoadLocal, 0}, {IrOpcode::PushI64, 1}, {IrOpcode::AtomicStore, 64}});
  spinThen(second, 1);
  second.push_back({IrOpcode::ReturnVoid, 0});
  std::vector<IrInstruction> body = spawnOf(1, {0, 1}, 2);
  append(body, spawnOf(2, {0, 1}, 3));
  append(body, waitTask(2));
  append(body, waitTask(3));
  append(body, print(1));
  append(body, returnZero());
  const IrModule module = moduleOf({functionOf("/main", 0, withSlots(2, std::move(body))),
                                    functionOf("/first", 2, std::move(first)),
                                    functionOf("/second", 2, std::move(second))});
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    CAPTURE(repeat);
    const Run threaded = run(module, true);
    REQUIRE_MESSAGE(threaded.ok, threaded.error);
    CHECK(threaded.out == "1\n");
  }
}

TEST_CASE("a lock keeps a plain counter whole across four tasks") {
  // Each task takes the lock, reads the counter slot plainly, adds one and stores it; without mutual exclusion
  // increments would be lost.
  std::vector<IrInstruction> body;
  for (uint64_t task = 0; task < 4; ++task) {
    append(body, {{IrOpcode::LoadLocal, 0}, {IrOpcode::LoadLocal, 1}, {IrOpcode::PushI64, 250}});
    append(body, {{IrOpcode::TaskSpawn, 1}, {IrOpcode::StoreLocal, 2 + task}});
  }
  for (uint64_t task = 0; task < 4; ++task) {
    append(body, waitTask(2 + task));
  }
  append(body,
         {{IrOpcode::LoadLocal, 1}, {IrOpcode::LoadIndirect, 0}, {IrOpcode::PrintI64, Newline}});
  append(body, returnZero());
  std::vector<IrInstruction> increment = lockOf(0);
  append(increment,
         {{IrOpcode::LoadLocal, 1},
          {IrOpcode::LoadLocal, 1},
          {IrOpcode::LoadIndirect, 0},
          {IrOpcode::PushI64, 1},
          {IrOpcode::AddI64, 0},
          {IrOpcode::StoreIndirect, 0},
          {IrOpcode::Pop, 0}});
  append(increment, unlockOf(0));
  const IrModule module = moduleOf({functionOf("/main", 0, withSlots(2, std::move(body))),
                                    loopTask("/increment", 3, std::move(increment))});
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    CAPTURE(repeat);
    const Run threaded = run(module, true);
    REQUIRE_MESSAGE(threaded.ok, threaded.error);
    CHECK(threaded.out == "1000\n");
  }
}

namespace {

// waiter(lock, condition, flag): takes the lock, waits on the condition until the flag is set (taking the lock again
// after each wait), prints 2 and frees the lock.
IrFunction waiterTask() {
  std::vector<IrInstruction> body = {
      {IrOpcode::StoreLocal, 2}, {IrOpcode::StoreLocal, 1}, {IrOpcode::StoreLocal, 0}};
  append(body, lockOf(0));
  const uint64_t check = body.size();
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

// main: spawns `waiters` waiters, locks, sets the flag, prints 5, notifies, unlocks and waits them.
IrModule notifyModule(uint64_t waiters) {
  std::vector<IrInstruction> body;
  for (uint64_t waiter = 0; waiter < waiters; ++waiter) {
    append(body, spawnOf(1, {0, 1, 2}, 3 + waiter));
  }
  append(body, lockOf(0));
  append(body, {{IrOpcode::LoadLocal, 2}, {IrOpcode::PushI64, 1}, {IrOpcode::AtomicStore, 64}});
  append(body, print(5));
  append(body, notifyOf(1));
  append(body, unlockOf(0));
  for (uint64_t waiter = 0; waiter < waiters; ++waiter) {
    append(body, waitTask(3 + waiter));
  }
  append(body, returnZero());
  return moduleOf({functionOf("/main", 0, withSlots(3, std::move(body))), waiterTask()});
}

} // namespace

TEST_CASE("waiters on a condition wake after the flag is set and the notify") {
  // The flag is set with the lock held and the waiters read it with the lock held, so 5 comes first and every waiter
  // prints 2 whether it started waiting before or after the notify.
  for (const uint64_t waiters : {uint64_t{1}, uint64_t{3}}) {
    const IrModule module = notifyModule(waiters);
    std::string expected = "5\n";
    for (uint64_t waiter = 0; waiter < waiters; ++waiter) {
      expected += "2\n";
    }
    for (int repeat = 0; repeat < Repeats; ++repeat) {
      CAPTURE(waiters);
      CAPTURE(repeat);
      const Run threaded = run(module, true);
      REQUIRE_MESSAGE(threaded.ok, threaded.error);
      CHECK(threaded.out == expected);
    }
  }
}

TEST_CASE("a task that waits for a lock the main task holds is a deadlock") {
  // main: lock, spawn a task that locks the same lock, wait it. The task sleeps for the lock and main sleeps for
  // the task, so nothing can change.
  std::vector<IrInstruction> body = lockOf(0);
  append(body, spawnOf(1, {0}, 1));
  append(body, waitTask(1));
  append(body, returnZero());
  std::vector<IrInstruction> task = {{IrOpcode::StoreLocal, 0}};
  append(task, lockOf(0));
  task.push_back({IrOpcode::ReturnVoid, 0});
  const IrModule module = moduleOf({functionOf("/main", 0, withSlots(1, std::move(body))),
                                    functionOf("/task", 1, std::move(task))});
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    CAPTURE(repeat);
    const Run threaded = run(module, true);
    CHECK_FALSE(threaded.ok);
    CHECK(threaded.error == Deadlock);
  }
}

TEST_CASE("two tasks that hold one lock each and want the other's are a deadlock") {
  // first(a, b, fa, fb): takes a, raises fa, spins until fb is raised, then wants b. second mirrors it, so both hold
  // one lock before either asks for the other.
  const auto crossTask =
      [](uint64_t firstLock, uint64_t secondLock, uint64_t mine, uint64_t theirs) {
        std::vector<IrInstruction> code = {{IrOpcode::StoreLocal, 3},
                                           {IrOpcode::StoreLocal, 2},
                                           {IrOpcode::StoreLocal, 1},
                                           {IrOpcode::StoreLocal, 0}};
        append(code, lockOf(firstLock));
        append(code,
               {{IrOpcode::LoadLocal, mine}, {IrOpcode::PushI64, 1}, {IrOpcode::AtomicStore, 64}});
        spinThen(code, theirs);
        append(code, lockOf(secondLock));
        code.push_back({IrOpcode::ReturnVoid, 0});
        return code;
      };
  // Locals of main: 0 and 1 the locks, 2 and 3 the flags.
  std::vector<IrInstruction> body = spawnOf(1, {0, 1, 2, 3}, 4);
  append(body, spawnOf(2, {0, 1, 2, 3}, 5));
  append(body, waitTask(4));
  append(body, returnZero());
  const IrModule module = moduleOf({functionOf("/main", 0, withSlots(4, std::move(body))),
                                    functionOf("/first", 4, crossTask(0, 1, 2, 3)),
                                    functionOf("/second", 4, crossTask(1, 0, 3, 2))});
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    CAPTURE(repeat);
    const Run threaded = run(module, true);
    CHECK_FALSE(threaded.ok);
    CHECK(threaded.error == Deadlock);
  }
}

TEST_CASE("a condition wait nobody notifies is a deadlock") {
  // The waiter sleeps for a notify, main waits for the waiter.
  std::vector<IrInstruction> body = spawnOf(1, {0, 1, 2}, 3);
  append(body, waitTask(3));
  append(body, returnZero());
  const IrModule module =
      moduleOf({functionOf("/main", 0, withSlots(3, std::move(body))), waiterTask()});
  // The main task alone, waiting on a condition.
  std::vector<IrInstruction> alone = lockOf(0);
  append(alone, waitOn(1, 0));
  append(alone, returnZero());
  const IrModule lone = moduleOf({functionOf("/main", 0, withSlots(2, std::move(alone)))});
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    CAPTURE(repeat);
    for (const IrModule *each : {&module, &lone}) {
      const Run threaded = run(*each, true);
      CHECK_FALSE(threaded.ok);
      CHECK(threaded.error == Deadlock);
    }
  }
}

TEST_CASE("a fault stops a task that spins and leaves no thread running") {
  // main: spawn a task that spins on an atomic nobody sets, then another that frees a lock it does not hold. The
  // run ends with that fault; it returns only after both threads ended.
  std::vector<IrInstruction> spinner = {{IrOpcode::StoreLocal, 0}};
  spinThen(spinner, 0);
  spinner.push_back({IrOpcode::ReturnVoid, 0});
  std::vector<IrInstruction> faulty = {{IrOpcode::StoreLocal, 0}};
  append(faulty, unlockOf(0));
  faulty.push_back({IrOpcode::ReturnVoid, 0});
  std::vector<IrInstruction> body = spawnOf(1, {0}, 2);
  append(body, spawnOf(2, {1}, 3));
  append(body, waitTask(3));
  append(body, waitTask(2));
  append(body, returnZero());
  const IrModule module = moduleOf({functionOf("/main", 0, withSlots(2, std::move(body))),
                                    functionOf("/spinner", 1, std::move(spinner)),
                                    functionOf("/faulty", 1, std::move(faulty))});
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    CAPTURE(repeat);
    const Run threaded = run(module, true);
    CHECK_FALSE(threaded.ok);
    CHECK(threaded.error == "mutex unlocked by a task that does not hold it");
  }
}

TEST_CASE("the main task returning stops a task that spins") {
  std::vector<IrInstruction> spinner = {{IrOpcode::StoreLocal, 0}};
  spinThen(spinner, 0);
  spinner.push_back({IrOpcode::ReturnVoid, 0});
  std::vector<IrInstruction> body = spawnOf(1, {0}, 1);
  append(body, print(3));
  append(body, returnZero());
  const IrModule module = moduleOf({functionOf("/main", 0, withSlots(1, std::move(body))),
                                    functionOf("/spinner", 1, std::move(spinner))});
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    CAPTURE(repeat);
    const Run threaded = run(module, true);
    REQUIRE_MESSAGE(threaded.ok, threaded.error);
    CHECK(threaded.out == "3\n");
  }
}

TEST_CASE("lock misuse faults in threads mode as it does on the default schedule") {
  std::vector<IrInstruction> again = lockOf(0);
  append(again, lockOf(0));
  append(again, returnZero());
  std::vector<IrInstruction> unheld = unlockOf(0);
  append(unheld, returnZero());
  std::vector<IrInstruction> noLock = waitOn(1, 0);
  append(noLock, returnZero());
  for (std::vector<IrInstruction> *code : {&again, &unheld, &noLock}) {
    const IrModule module = moduleOf({functionOf("/main", 0, withSlots(2, *code))});
    const Run serial = run(module, false);
    const Run threaded = run(module, true);
    CHECK_FALSE(serial.ok);
    CHECK_FALSE(threaded.ok);
    CHECK(threaded.error == serial.error);
  }
}

TEST_CASE("32-bit and 64-bit atomics answer as they do on the default schedule") {
  // Store, add (wrapping), exchange and compare-exchange (a hit and a miss) on both widths, printing each result.
  std::vector<IrInstruction> body;
  for (const uint64_t width : {uint64_t{32}, uint64_t{64}}) {
    const IrOpcode push = width == 32 ? IrOpcode::PushI32 : IrOpcode::PushI64;
    const IrOpcode printer = width == 32 ? IrOpcode::PrintI32 : IrOpcode::PrintI64;
    const uint64_t big = width == 32 ? 2147483647u : 9223372036854775807ull;
    append(body, {{IrOpcode::LoadLocal, 0}, {push, big}, {IrOpcode::AtomicStore, width}});
    append(body,
           {{IrOpcode::LoadLocal, 0}, {push, 1}, {IrOpcode::AtomicAdd, width}, {printer, Newline}});
    append(body, {{IrOpcode::LoadLocal, 0}, {IrOpcode::AtomicLoad, width}, {printer, Newline}});
    append(body,
           {{IrOpcode::LoadLocal, 0},
            {push, 7},
            {IrOpcode::AtomicExchange, width},
            {printer, Newline}});
    append(body,
           {{IrOpcode::LoadLocal, 0},
            {push, 7},
            {push, 9},
            {IrOpcode::AtomicCompareExchange, width},
            {IrOpcode::PrintI32, Newline}});
    append(body,
           {{IrOpcode::LoadLocal, 0},
            {push, 7},
            {push, 11},
            {IrOpcode::AtomicCompareExchange, width},
            {IrOpcode::PrintI32, Newline}});
    append(body, {{IrOpcode::LoadLocal, 0}, {IrOpcode::AtomicLoad, width}, {printer, Newline}});
  }
  append(body, returnZero());
  const IrModule module = moduleOf({functionOf("/main", 0, withSlots(1, std::move(body)))});
  const Run serial = run(module, false);
  REQUIRE_MESSAGE(serial.ok, serial.error);
  const Run threaded = run(module, true);
  REQUIRE_MESSAGE(threaded.ok, threaded.error);
  CHECK(threaded.out == serial.out);
}

TEST_SUITE_END();
