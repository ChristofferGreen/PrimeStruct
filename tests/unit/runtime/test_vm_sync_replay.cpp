#include "primec/ir/Ir.h"
#include "primec/runtime/Vm.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace primec;

// Synchronization record and replay (docs/spec/vm-design.md; `--vm-record-sync` and `--vm-replay-sync`; TODO-5901)
// with hand-built IR through the embedding API. A threaded run writes the order of its synchronization events and a
// run on the deterministic scheduler follows it, so the program prints what the threaded run printed. Nothing here
// sleeps or measures time; every case runs its programs a fixed number of times.
TEST_SUITE_BEGIN("primestruct.runtime.vm_sync_replay");

namespace {

constexpr int Repeats = 20;
constexpr uint64_t Newline = PrintFlagNewline;
constexpr size_t HeaderBytes = 24;
constexpr size_t EventBytes = 8;

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

void append(std::vector<IrInstruction> &out, std::vector<IrInstruction> more) {
  out.insert(out.end(), more.begin(), more.end());
}

struct Run {
  bool ok = false;
  std::string error;
  std::string out;
};

void appendOutput(int fd, std::string_view chunk, void *userData) {
  if (fd == 1) {
    static_cast<std::string *>(userData)->append(chunk);
  }
}

// Runs `module`; `record` makes it a threaded run that writes the log, `replay` a run that follows one.
Run run(const IrModule &module, const std::string &record, const std::string &replay) {
  Run result;
  Vm vm;
  vm.setOutputSink({&appendOutput, &result.out});
  vm.setTaskThreads(!record.empty());
  vm.setSyncRecordPath(record);
  vm.setSyncReplayPath(replay);
  uint64_t raw = 0;
  result.ok = vm.execute(module, raw, result.error, std::vector<std::string_view>{"prog"});
  return result;
}

std::string scratch(const std::string &name) {
  return primec::testing::testScratchPath("vm_sync_replay/" + name).string();
}

std::vector<uint8_t> readFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(file),
                              std::istreambuf_iterator<char>());
}

void writeFile(const std::string &path, const std::vector<uint8_t> &bytes) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(reinterpret_cast<const char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
}

// The events of a log as (task, kind) pairs, read with the layout the spec documents.
std::vector<std::pair<uint32_t, uint8_t>> eventsOf(const std::vector<uint8_t> &bytes) {
  std::vector<std::pair<uint32_t, uint8_t>> events;
  if (bytes.size() < HeaderBytes + EventBytes + 8) {
    return events;
  }
  const size_t count = (bytes.size() - HeaderBytes - EventBytes - 8) / EventBytes;
  for (size_t i = 0; i < count; ++i) {
    const uint8_t *record = bytes.data() + HeaderBytes + i * EventBytes;
    const uint32_t task =
        static_cast<uint32_t>(record[0]) | (static_cast<uint32_t>(record[1]) << 8) |
        (static_cast<uint32_t>(record[2]) << 16) | (static_cast<uint32_t>(record[3]) << 24);
    events.emplace_back(task, record[4]);
  }
  return events;
}

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

std::vector<IrInstruction> print(uint64_t value) {
  return {{IrOpcode::PushI32, value}, {IrOpcode::PrintI32, Newline}};
}

std::vector<IrInstruction> returnZero() {
  return {{IrOpcode::PushI32, 0}, {IrOpcode::ReturnI32, 0}};
}

std::vector<IrInstruction> waitTask(uint64_t handle) {
  return {{IrOpcode::LoadLocal, handle}, {IrOpcode::TaskWait, 0}, {IrOpcode::Pop, 0}};
}

// worker(lock, counter, id): with the lock held, prints its id and then the ticket it drew from the counter, so the
// output is the order in which the workers took the lock. main spawns `workers` of them and waits them.
IrModule ticketModule(uint64_t workers, uint64_t idOffset) {
  std::vector<IrInstruction> body;
  for (uint64_t worker = 0; worker < workers; ++worker) {
    append(body,
           {{IrOpcode::LoadLocal, 0},
            {IrOpcode::LoadLocal, 1},
            {IrOpcode::PushI64, idOffset + worker + 1},
            {IrOpcode::TaskSpawn, 1},
            {IrOpcode::StoreLocal, 2 + worker}});
  }
  for (uint64_t worker = 0; worker < workers; ++worker) {
    append(body, waitTask(2 + worker));
  }
  append(body, returnZero());
  std::vector<IrInstruction> worker = {
      {IrOpcode::StoreLocal, 2}, {IrOpcode::StoreLocal, 1}, {IrOpcode::StoreLocal, 0}};
  append(worker, lockOf(0));
  append(worker, {{IrOpcode::LoadLocal, 2}, {IrOpcode::PrintI64, Newline}});
  append(worker,
         {{IrOpcode::LoadLocal, 1},
          {IrOpcode::PushI64, 1},
          {IrOpcode::AtomicAdd, 64},
          {IrOpcode::PrintI64, Newline}});
  append(worker, unlockOf(0));
  worker.push_back({IrOpcode::ReturnVoid, 0});
  return moduleOf({functionOf("/main", 0, withSlots(2, std::move(body))),
                   functionOf("/worker", 3, std::move(worker))});
}

// waiter(lock, condition, flag): takes the lock, waits on the condition until the flag is set, prints 2 and frees
// the lock. main spawns `waiters` of them, sets the flag with the lock held, prints 5 and notifies.
IrModule notifyModule(uint64_t waiters) {
  std::vector<IrInstruction> task = {
      {IrOpcode::StoreLocal, 2}, {IrOpcode::StoreLocal, 1}, {IrOpcode::StoreLocal, 0}};
  append(task, lockOf(0));
  const uint64_t check = task.size();
  append(task,
         {{IrOpcode::LoadLocal, 2},
          {IrOpcode::AtomicLoad, 64},
          {IrOpcode::PushI64, 0},
          {IrOpcode::CmpEqI64, 0}});
  const uint64_t exitJump = task.size();
  task.push_back({IrOpcode::JumpIfZero, 0});
  append(task, {{IrOpcode::LoadLocal, 1}, {IrOpcode::LoadLocal, 0}, {IrOpcode::ConditionWait, 0}});
  append(task, lockOf(0));
  task.push_back({IrOpcode::Jump, check});
  task[exitJump].imm = task.size();
  append(task, print(2));
  append(task, unlockOf(0));
  task.push_back({IrOpcode::ReturnVoid, 0});

  std::vector<IrInstruction> body;
  for (uint64_t waiter = 0; waiter < waiters; ++waiter) {
    append(body,
           {{IrOpcode::LoadLocal, 0},
            {IrOpcode::LoadLocal, 1},
            {IrOpcode::LoadLocal, 2},
            {IrOpcode::TaskSpawn, 1},
            {IrOpcode::StoreLocal, 3 + waiter}});
  }
  append(body, lockOf(0));
  append(body, {{IrOpcode::LoadLocal, 2}, {IrOpcode::PushI64, 1}, {IrOpcode::AtomicStore, 64}});
  append(body, print(5));
  append(body, {{IrOpcode::LoadLocal, 1}, {IrOpcode::ConditionNotify, 0}});
  append(body, unlockOf(0));
  for (uint64_t waiter = 0; waiter < waiters; ++waiter) {
    append(body, waitTask(3 + waiter));
  }
  append(body, returnZero());
  return moduleOf({functionOf("/main", 0, withSlots(3, std::move(body))),
                   functionOf("/waiter", 3, std::move(task))});
}

} // namespace

TEST_CASE("each of 20 threaded runs replays its record to the same stdout") {
  const IrModule module = ticketModule(4, 0);
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    CAPTURE(repeat);
    const std::string path = scratch("tickets_" + std::to_string(repeat) + ".sync");
    const Run threaded = run(module, path, "");
    REQUIRE_MESSAGE(threaded.ok, threaded.error);
    const Run replayed = run(module, "", path);
    REQUIRE_MESSAGE(replayed.ok, replayed.error);
    CHECK(replayed.out == threaded.out);
    // Four ids and four tickets, one pair per lock holder.
    CHECK(threaded.out.size() == 16);
  }
}

TEST_CASE("a replay follows the lock order the record has, not the eager one") {
  // The eager schedule runs worker 1 to its end first. A record whose lock order is 3, 1, 2 must print that order.
  const IrModule module = ticketModule(3, 0);
  const std::string path = scratch("forced_order.sync");
  Run threaded = run(module, path, "");
  REQUIRE_MESSAGE(threaded.ok, threaded.error);
  std::vector<uint8_t> bytes = readFile(path);
  std::vector<std::pair<uint32_t, uint8_t>> events = eventsOf(bytes);
  // Rewrite the per-worker blocks (lock, atomic add, unlock; 3 events each, after the 3 spawns) in the order 3, 1, 2.
  // Each worker's block is found by its task number: the spawned tasks are numbered 2, 3 and 4 in spawn order.
  std::vector<std::vector<size_t>> blocks(5);
  for (size_t i = 0; i < events.size(); ++i) {
    if (events[i].second != 1 && events[i].second != 2) {
      blocks[events[i].first].push_back(i);
    }
  }
  std::vector<uint8_t> reordered(bytes.begin(), bytes.begin() + HeaderBytes);
  const auto copyEvent = [&](size_t index) {
    const uint8_t *record = bytes.data() + HeaderBytes + index * EventBytes;
    reordered.insert(reordered.end(), record, record + EventBytes);
  };
  size_t kept = 0;
  for (size_t i = 0; i < events.size(); ++i) {
    if (events[i].second == 1) {
      copyEvent(i);
      ++kept;
    }
  }
  for (const size_t task : {size_t{4}, size_t{2}, size_t{3}}) {
    for (const size_t index : blocks[task]) {
      copyEvent(index);
      ++kept;
    }
  }
  for (size_t i = 0; i < events.size(); ++i) {
    if (events[i].second == 2) {
      copyEvent(i);
      ++kept;
    }
  }
  REQUIRE(kept == events.size());
  const size_t trailer = HeaderBytes + events.size() * EventBytes;
  reordered.insert(
      reordered.end(), bytes.begin() + static_cast<std::ptrdiff_t>(trailer), bytes.end());
  const std::string forced = scratch("forced_order_edited.sync");
  writeFile(forced, reordered);
  const Run replayed = run(module, "", forced);
  REQUIRE_MESSAGE(replayed.ok, replayed.error);
  // Worker ids are 1, 2 and 3 in spawn order; the record puts the third task's block first.
  CHECK(replayed.out == "3\n0\n1\n1\n2\n2\n");
}

TEST_CASE("condition waits and notifies replay to the same stdout") {
  for (const uint64_t waiters : {uint64_t{1}, uint64_t{3}}) {
    const IrModule module = notifyModule(waiters);
    std::string expected = "5\n";
    for (uint64_t waiter = 0; waiter < waiters; ++waiter) {
      expected += "2\n";
    }
    for (int repeat = 0; repeat < Repeats; ++repeat) {
      CAPTURE(waiters);
      CAPTURE(repeat);
      const std::string path =
          scratch("notify_" + std::to_string(waiters) + "_" + std::to_string(repeat) + ".sync");
      const Run threaded = run(module, path, "");
      REQUIRE_MESSAGE(threaded.ok, threaded.error);
      CHECK(threaded.out == expected);
      const Run replayed = run(module, "", path);
      REQUIRE_MESSAGE(replayed.ok, replayed.error);
      CHECK(replayed.out == expected);
    }
  }
}

TEST_CASE("the log has a versioned header, the events in order and a trailer") {
  // main spawns one task that adds to an atomic, then waits it: one possible order of events.
  std::vector<IrInstruction> body = {
      {IrOpcode::LoadLocal, 0}, {IrOpcode::TaskSpawn, 1}, {IrOpcode::StoreLocal, 1}};
  append(body, waitTask(1));
  append(body, returnZero());
  const IrModule module = moduleOf({functionOf("/main", 0, withSlots(1, std::move(body))),
                                    functionOf("/add",
                                               1,
                                               {{IrOpcode::StoreLocal, 0},
                                                {IrOpcode::LoadLocal, 0},
                                                {IrOpcode::PushI64, 1},
                                                {IrOpcode::AtomicAdd, 64},
                                                {IrOpcode::Pop, 0},
                                                {IrOpcode::ReturnVoid, 0}})});
  const std::string path = scratch("format.sync");
  const Run threaded = run(module, path, "");
  REQUIRE_MESSAGE(threaded.ok, threaded.error);
  const std::vector<uint8_t> bytes = readFile(path);
  REQUIRE(bytes.size() == HeaderBytes + 3 * EventBytes + EventBytes + 8);
  CHECK(std::string(bytes.begin(), bytes.begin() + 8) == "PSSYNCLG");
  CHECK(bytes[8] == 1);
  CHECK(bytes[9] == 0);
  const std::vector<std::pair<uint32_t, uint8_t>> expected = {{1, 1}, {2, 9}, {1, 2}};
  CHECK(eventsOf(bytes) == expected);
  // The trailer: an end record and the event count.
  CHECK(bytes[HeaderBytes + 3 * EventBytes + 4] == 0);
  CHECK(bytes[HeaderBytes + 4 * EventBytes] == 3);
}

TEST_CASE("a record of a different or changed program is refused with a diagnostic") {
  const IrModule recorded = ticketModule(3, 0);
  const std::string path = scratch("refused.sync");
  REQUIRE(run(recorded, path, "").ok);
  const Run same = run(recorded, "", path);
  CHECK(same.ok);
  // Another program, and the same program with one constant changed.
  const IrModule other = notifyModule(2);
  const IrModule changed = ticketModule(3, 10);
  for (const IrModule *module : {&other, &changed}) {
    const Run refused = run(*module, "", path);
    CHECK_FALSE(refused.ok);
    CHECK(refused.error.find("recorded from a different program") != std::string::npos);
    CHECK(refused.error.find(path) != std::string::npos);
    CHECK(refused.out.empty());
  }
}

TEST_CASE("a file that is not a usable record is refused with a diagnostic") {
  const IrModule module = ticketModule(2, 0);
  const std::string path = scratch("usable.sync");
  REQUIRE(run(module, path, "").ok);
  const std::vector<uint8_t> good = readFile(path);

  const auto refusedWith = [&](const std::vector<uint8_t> &bytes, const std::string &expected) {
    const std::string bad = scratch("unusable.sync");
    writeFile(bad, bytes);
    const Run refused = run(module, "", bad);
    CHECK_FALSE(refused.ok);
    CHECK(refused.error.find(expected) != std::string::npos);
    CHECK(refused.out.empty());
  };
  refusedWith({}, "is not a sync record");
  refusedWith(std::vector<uint8_t>(64, 'x'), "is not a sync record");
  std::vector<uint8_t> newer = good;
  newer[8] = 2;
  refusedWith(newer, "format version 2");
  refusedWith(std::vector<uint8_t>(good.begin(), good.end() - 3), "is truncated");
  refusedWith(std::vector<uint8_t>(good.begin(), good.end() - EventBytes - 8), "is truncated");
  std::vector<uint8_t> unknownKind = good;
  unknownKind[HeaderBytes + 4] = 99;
  refusedWith(unknownKind, "unknown event kind 99");

  const Run missing = run(module, "", scratch("does_not_exist.sync"));
  CHECK_FALSE(missing.ok);
  CHECK(missing.error.find("cannot read the sync record") != std::string::npos);
}

TEST_CASE("a replay that cannot follow its record says where it diverged") {
  const IrModule module = ticketModule(2, 0);
  const std::string path = scratch("diverge.sync");
  REQUIRE(run(module, path, "").ok);
  std::vector<uint8_t> bytes = readFile(path);
  const std::vector<std::pair<uint32_t, uint8_t>> events = eventsOf(bytes);
  // The first worker event in the record is a lock; make it an unlock.
  size_t lock = 0;
  while (lock < events.size() && events[lock].second != 3) {
    ++lock;
  }
  REQUIRE(lock < events.size());
  bytes[HeaderBytes + lock * EventBytes + 4] = 4;
  const std::string edited = scratch("diverge_edited.sync");
  writeFile(edited, bytes);
  const Run replayed = run(module, "", edited);
  CHECK_FALSE(replayed.ok);
  CHECK(replayed.error.find("sync replay diverged at event " + std::to_string(lock + 1)) !=
        std::string::npos);
}

TEST_CASE("a record needs task threads and a module without tasks records no events") {
  const IrModule plain = moduleOf({functionOf("/main", 0, [] {
    std::vector<IrInstruction> body = print(7);
    append(body, returnZero());
    return body;
  }())});
  const std::string path = scratch("plain.sync");
  const Run recorded = run(plain, path, "");
  REQUIRE_MESSAGE(recorded.ok, recorded.error);
  CHECK(recorded.out == "7\n");
  CHECK(readFile(path).size() == HeaderBytes + EventBytes + 8);
  // A replay of a module without tasks only checks that the file belongs to it.
  CHECK(run(plain, "", path).ok);
  CHECK_FALSE(run(ticketModule(1, 0), "", path).ok);
  Vm noThreads;
  noThreads.setSyncRecordPath(scratch("no_threads.sync"));
  uint64_t raw = 0;
  std::string error;
  CHECK_FALSE(
      noThreads.execute(ticketModule(1, 0), raw, error, std::vector<std::string_view>{"prog"}));
  CHECK(error.find("needs task threads") != std::string::npos);
}

TEST_CASE("a deadlock replays as the same deadlock and a record is written for a faulted run") {
  // main locks, spawns a task that wants the lock and waits it: the task sleeps for the lock, main for the task.
  std::vector<IrInstruction> body = lockOf(0);
  append(body, {{IrOpcode::LoadLocal, 0}, {IrOpcode::TaskSpawn, 1}, {IrOpcode::StoreLocal, 1}});
  append(body, waitTask(1));
  append(body, returnZero());
  std::vector<IrInstruction> task = {{IrOpcode::StoreLocal, 0}};
  append(task, lockOf(0));
  task.push_back({IrOpcode::ReturnVoid, 0});
  const IrModule module = moduleOf({functionOf("/main", 0, withSlots(1, std::move(body))),
                                    functionOf("/task", 1, std::move(task))});
  const std::string path = scratch("deadlock.sync");
  const Run threaded = run(module, path, "");
  CHECK_FALSE(threaded.ok);
  CHECK(threaded.error == "deadlock: every task is waiting");
  const std::vector<std::pair<uint32_t, uint8_t>> expected = {{1, 3}, {1, 1}};
  CHECK(eventsOf(readFile(path)) == expected);
  const Run replayed = run(module, "", path);
  CHECK_FALSE(replayed.ok);
  CHECK(replayed.error == "deadlock: every task is waiting");
}
