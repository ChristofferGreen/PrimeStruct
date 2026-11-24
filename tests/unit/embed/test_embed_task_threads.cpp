#include "primec/embed/ScriptEngine.h"

#include "third_party/doctest.h"

#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>

using primec::embed::HostThreading;
using primec::embed::RunOptions;
using primec::embed::Script;
using primec::embed::ScriptEngine;

// Script::run with RunOptions::threads (TODO-5899): a fault or the cancel flag stops every task thread, the run
// returns after all are joined, and the process and the next run are unaffected. Nothing here sleeps: a spinning
// task ends because the run's stop flag or the cancel flag is set by the program itself.
TEST_SUITE_BEGIN("primestruct.embed.task_threads");

namespace {

constexpr int Repeats = 10;

// Three tasks: two never end by themselves, the third calls a host function that throws.
const char *FaultSource = R"(
[host return<int>]
boom([i32] code) {
}

[return<int> effects(none)]
spin() {
  [i32 mut] n{0i32}
  while(true) {
    n = n + 1i32
  }
  return(n)
}

[return<int> effects(none)]
fail_now() {
  return(boom(7i32))
}

[effects(task), return<int>]
main() {
  [Task<int>] a{[spawn] spin()}
  [Task<int>] b{[spawn] spin()}
  [Task<int>] c{[spawn] fail_now()}
  return(wait(a) + wait(b) + wait(c))
}
)";

// The task raises the cancel flag through a host function, then never ends; main waits for it.
const char *CancelSource = R"(
[host return<int>]
raise_cancel([i32] unused) {
}

[return<int> effects(none)]
spin_after_cancel() {
  [i32 mut] n{raise_cancel(0i32)}
  while(true) {
    n = n + 1i32
  }
  return(n)
}

[effects(task), return<int>]
main() {
  [Task<int>] a{[spawn] spin_after_cancel()}
  return(wait(a))
}
)";

const char *SameThreadSource = R"(
[host return<int>]
where([i32] unused) {
}

[return<int>]
main() {
  return(where(0i32))
}
)";

const char *SpawnedThreadSource = R"(
[host return<int>]
where([i32] unused) {
}

[return<int> effects(none)]
task_where() {
  return(where(0i32))
}

[effects(task), return<int>]
main() {
  [Task<int>] a{[spawn] task_where()}
  return(wait(a))
}
)";

RunOptions threaded() {
  RunOptions options;
  options.threads = true;
  return options;
}

} // namespace

TEST_CASE("a fault in one of three tasks ends a threaded embed run and the harness goes on") {
  ScriptEngine engine;
  Script script = engine.compileSource("/task_threads_fault.prime", FaultSource);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  script.bind("boom", [](int32_t) -> int32_t { throw std::runtime_error("task fault"); });
  const Script after =
      engine.compileSource("/task_threads_after.prime", "[return<int>]\nmain() {\n  return(7i32)\n}\n");
  REQUIRE(after.valid());
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    const auto result = script.run({}, threaded());
    CHECK_FALSE(result.ok);
    CHECK(result.diagnostics.find("VM error: ") == 0);
    CHECK(result.diagnostics.find("task fault") != std::string::npos);
    // Every thread is joined: the same process runs the next script, with and without threads.
    CHECK(after.run().exitCode == 7);
    CHECK(after.run({}, threaded()).exitCode == 7);
  }
}

TEST_CASE("the cancel flag stops a task that loops forever in a threaded embed run") {
  ScriptEngine engine;
  Script script = engine.compileSource("/task_threads_cancel.prime", CancelSource);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  std::atomic<bool> stop{false};
  script.bind("raise_cancel", [&stop](int32_t) -> int32_t {
    stop.store(true);
    return 0;
  });
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    stop.store(false);
    RunOptions options = threaded();
    options.cancel = &stop;
    const auto result = script.run({}, options);
    CHECK_FALSE(result.ok);
    CHECK(result.diagnostics == "VM error: execution stopped");
  }
}

TEST_CASE("a script without tasks starts no thread when threads are asked for") {
  ScriptEngine engine;
  Script plain = engine.compileSource("/task_threads_plain.prime", SameThreadSource);
  Script spawning = engine.compileSource("/task_threads_spawning.prime", SpawnedThreadSource);
  REQUIRE_MESSAGE(plain.valid(), plain.diagnostics());
  REQUIRE_MESSAGE(spawning.valid(), spawning.diagnostics());
  std::atomic<std::thread::id> seen{std::this_thread::get_id()};
  const auto record = [&seen](int32_t) -> int32_t {
    seen.store(std::this_thread::get_id());
    return 5;
  };
  plain.bind("where", record);
  spawning.bind("where", record);
  spawning.setHostThreading("where", HostThreading::Reentrant);
  const std::thread::id caller = std::this_thread::get_id();
  // The probe works: a spawned task's host call comes from another thread.
  auto result = spawning.run({}, threaded());
  CHECK_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.exitCode == 5);
  CHECK(seen.load() != caller);
  // Without task opcodes the call is on the caller's thread, whatever the option says.
  for (const bool threads : {false, true}) {
    seen.store(std::thread::id{});
    RunOptions options;
    options.threads = threads;
    result = plain.run({}, options);
    CHECK_MESSAGE(result.ok, result.diagnostics);
    CHECK(result.exitCode == 5);
    CHECK(seen.load() == caller);
  }
}

TEST_SUITE_END();
