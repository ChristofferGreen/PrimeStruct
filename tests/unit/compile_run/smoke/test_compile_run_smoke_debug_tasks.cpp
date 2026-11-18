#include "../test_compile_run_helpers.h"

#include "test_compile_run_smoke_helpers.h"

// A debug session on a program that spawns tasks (docs/spec/vm-design.md, Tasks in a debug session): DAP stops inside
// the spawned callee, lists the main task and that task as threads, answers a stack for each, and the program then
// runs to the exit code of a plain run.

TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

TEST_CASE("primevm debug-dap stops inside a spawned task and lists both threads") {
  const std::string source = R"(
[return<i32>]
work([i32] n) {
  [i32] doubled{n * 2i32}
  return(doubled)
}

[return<i32> effects(task)]
main() {
  [Task<i32>] t{[spawn] work(21i32)}
  return(wait(t))
}
)";
  const std::string srcPath = writeTemp("primevm_debug_dap_tasks.prime", source);
  CHECK(runCommand("./primevm " + quoteShellArg(srcPath) + " --entry /main") == 42);

  const std::string requests =
      makeDapFrame(R"({"seq":1,"type":"request","command":"initialize","arguments":{}})") +
      makeDapFrame(R"({"seq":2,"type":"request","command":"launch","arguments":{}})") +
      makeDapFrame(
          R"({"seq":3,"type":"request","command":"setBreakpoints","arguments":{"breakpoints":[{"line":4}]}})") +
      makeDapFrame(R"({"seq":4,"type":"request","command":"configurationDone","arguments":{}})") +
      makeDapFrame(
          R"({"seq":5,"type":"request","command":"continue","arguments":{"threadId":1}})") +
      makeDapFrame(R"({"seq":6,"type":"request","command":"threads","arguments":{}})") +
      makeDapFrame(
          R"({"seq":7,"type":"request","command":"stackTrace","arguments":{"threadId":2}})") +
      makeDapFrame(
          R"({"seq":8,"type":"request","command":"stackTrace","arguments":{"threadId":1}})") +
      makeDapFrame(
          R"({"seq":9,"type":"request","command":"stackTrace","arguments":{"threadId":7}})") +
      makeDapFrame(
          R"({"seq":10,"type":"request","command":"setBreakpoints","arguments":{"breakpoints":[]}})") +
      makeDapFrame(
          R"({"seq":11,"type":"request","command":"continue","arguments":{"threadId":2}})") +
      makeDapFrame(R"({"seq":12,"type":"request","command":"disconnect","arguments":{}})");
  const std::string requestPath = writeTemp("primevm_debug_dap_tasks.requests", requests);
  const std::string outPath = (testScratchPath("") / "primevm_debug_dap_tasks.out").string();
  const std::string cmd = "./primevm " + quoteShellArg(srcPath) + " --entry /main --debug-dap < " +
                          quoteShellArg(requestPath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(cmd) == 0);

  bool framingOk = false;
  const std::vector<std::string> frames = parseDapFrames(readFile(outPath), framingOk);
  CHECK(framingOk);
  auto frameWith = [&](const std::string &needle, size_t startAt) -> size_t {
    for (size_t i = startAt; i < frames.size(); ++i) {
      if (frames[i].find(needle) != std::string::npos) {
        return i;
      }
    }
    return frames.size();
  };

  const size_t stopped = frameWith("\"reason\":\"breakpoint\"", 0);
  REQUIRE(stopped < frames.size());
  CHECK(frames[stopped].find("\"threadId\":2") != std::string::npos);

  const size_t threads = frameWith("\"command\":\"threads\"", stopped);
  REQUIRE(threads < frames.size());
  CHECK(frames[threads].find("{\"id\":1,\"name\":\"main\"}") != std::string::npos);
  CHECK(frames[threads].find("{\"id\":2,\"name\":\"task 1: /work\"}") != std::string::npos);

  const size_t taskStack = frameWith("\"name\":\"/work\"", threads);
  REQUIRE(taskStack < frames.size());
  CHECK(frames[taskStack].find("\"line\":4") != std::string::npos);
  CHECK(frames[taskStack].find("\"totalFrames\":1") != std::string::npos);
  const size_t mainStack = frameWith("\"name\":\"/main\"", taskStack);
  REQUIRE(mainStack < frames.size());
  CHECK(frames[mainStack].find("\"totalFrames\":1") != std::string::npos);
  const size_t badThread = frameWith("invalid thread id", mainStack);
  CHECK(badThread < frames.size());

  const size_t exited = frameWith("\"event\":\"exited\"", badThread);
  REQUIRE(exited < frames.size());
  CHECK(frames[exited].find("\"exitCode\":42") != std::string::npos);
}

TEST_SUITE_END();
