#include "primec/embed/ScriptEngine.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <string>
#include <string_view>

using primec::embed::RunOptions;
using primec::embed::Script;
using primec::embed::ScriptEngine;

// Synchronization record and replay through `RunOptions` (TODO-5901; docs/spec/vm-design.md, Synchronization record
// and replay): a program with a `Mutex` and a `Channel` runs on task threads with a record, and the deterministic
// scheduler runs it again in the recorded order. The program compiles once and runs Repeats times, each record
// replayed. Nothing here sleeps or measures time.
TEST_SUITE_BEGIN("primestruct.embed.sync_replay");

namespace {

constexpr int Repeats = 20;

// Two producers each add their values to a `Mutex<Tally>` and send them through a channel; main prints the values in
// the order they arrive, which depends on how the producers interleave, then the tally, which does not.
const char *const ChannelSource = R"(
import /std/sync/*

[struct]
Tally() {
  [i64 mut] total{0i64}
}

[effects(channel, lock, heap_alloc), return<void>]
produce([Channel<i64>] jobs, [Mutex<Tally>] tally, [i64] first) {
  [i64 mut] i{0i64}
  while(i < 3i64) {
    [i64] value{first + i}
    tally.locked([value]([Reference<Tally>] state) {
      state.total = state.total + value
    })
    jobs.send(value)
    i = i + 1i64
  }
}

[effects(task, channel, lock, heap_alloc, io_out), return<int>]
main() {
  jobs{Channel<i64>{[capacity] 2i32}}
  tally{Mutex<Tally>{Tally{}}}
  [Task<void>] a{[spawn] produce(jobs, tally, 10i64)}
  [Task<void>] b{[spawn] produce(jobs, tally, 20i64)}
  [i32 mut] received{0i32}
  while(received < 6i32) {
    pick(jobs.receive()) {
      none {
        received = 6i32
      }
      some(value) {
        print_line(value)
        received = received + 1i32
      }
    }
  }
  wait(a)
  wait(b)
  print_line(tally.locked([]([Reference<Tally>] state) {
    return(state.total)
  }))
  return(0i32)
}
)";

std::string scratch(const std::string &name) {
  return primec::testing::testScratchPath("embed_sync_replay/" + name).string();
}

} // namespace

TEST_CASE("each of 20 threaded runs of a Mutex and Channel program replays its record to the same "
          "stdout") {
  ScriptEngine engine;
  const Script script = engine.compileSource("/sync_replay_channel.prime", ChannelSource);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    CAPTURE(repeat);
    const std::string path = scratch("channel_" + std::to_string(repeat) + ".sync");
    std::string recorded;
    RunOptions record;
    record.threads = true;
    record.syncRecord = path;
    record.output = [&recorded](int fd, std::string_view chunk) {
      if (fd == 1) {
        recorded.append(chunk);
      }
    };
    const auto threaded = script.run({}, record);
    REQUIRE_MESSAGE(threaded.ok, threaded.diagnostics);
    // Six values in some order, then the tally 10 + 11 + 12 + 20 + 21 + 22.
    CHECK(recorded.size() > 6);
    CHECK(recorded.substr(recorded.size() - 3) == "96\n");

    std::string replayed;
    RunOptions replay;
    replay.syncReplay = path;
    replay.output = [&replayed](int fd, std::string_view chunk) {
      if (fd == 1) {
        replayed.append(chunk);
      }
    };
    const auto again = script.run({}, replay);
    REQUIRE_MESSAGE(again.ok, again.diagnostics);
    CHECK(replayed == recorded);
    CHECK(again.exitCode == threaded.exitCode);
  }
}

TEST_CASE("a record of a changed program is refused with a diagnostic and nothing runs") {
  ScriptEngine engine;
  const Script script = engine.compileSource("/sync_replay_original.prime", ChannelSource);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  std::string changedSource = ChannelSource;
  changedSource.replace(changedSource.find("20i64"), 5, "30i64");
  const Script changed = engine.compileSource("/sync_replay_changed.prime", changedSource);
  REQUIRE_MESSAGE(changed.valid(), changed.diagnostics());

  const std::string path = scratch("original.sync");
  RunOptions record;
  record.threads = true;
  record.syncRecord = path;
  REQUIRE(script.run({}, record).ok);

  std::string out;
  RunOptions replay;
  replay.syncReplay = path;
  replay.output = [&out](int, std::string_view chunk) { out.append(chunk); };
  const auto refused = changed.run({}, replay);
  CHECK_FALSE(refused.ok);
  CHECK(refused.diagnostics.find("recorded from a different program") != std::string::npos);
  CHECK(out.empty());
  // The record still replays the program it came from.
  CHECK(script.run({}, replay).ok);
}

TEST_CASE("a record without task threads is refused") {
  ScriptEngine engine;
  const Script script = engine.compileSource("/sync_replay_no_threads.prime", ChannelSource);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  RunOptions record;
  record.syncRecord = scratch("no_threads.sync");
  const auto refused = script.run({}, record);
  CHECK_FALSE(refused.ok);
  CHECK(refused.diagnostics.find("needs task threads") != std::string::npos);
}
