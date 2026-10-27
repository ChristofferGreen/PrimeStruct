#include "primec/embed/ScriptEngine.h"

#include "third_party/doctest.h"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

using primec::embed::RunOptions;
using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.embed.run_options");

TEST_CASE("embed run captures stdout and stderr in program order") {
  ScriptEngine engine;
  const auto script = engine.compileSource("/run_options_print.prime",
                                           "[return<i32> effects(io_out, io_err)]\n"
                                           "main() {\n"
                                           "  print_line(\"one\"utf8)\n"
                                           "  print_line_error(\"two\"utf8)\n"
                                           "  print(\"three\"utf8)\n"
                                           "  return(3i32)\n"
                                           "}\n");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  std::string out;
  std::string err;
  RunOptions options;
  options.output = [&](int fd, std::string_view chunk) { (fd == 2 ? err : out).append(chunk); };
  const auto result = script.run({}, options);
  CHECK_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.exitCode == 3);
  CHECK(out == "one\nthree");
  CHECK(err == "two\n");
}

TEST_CASE("embed run stops a script that does not end when the flag is set") {
  ScriptEngine engine;
  const auto script = engine.compileSource("/run_options_forever.prime",
                                           "[return<int>]\n"
                                           "main() {\n"
                                           "  [mut] n{0i32}\n"
                                           "  while(true) {\n"
                                           "    n = n + 1i32\n"
                                           "  }\n"
                                           "  return(n)\n"
                                           "}\n");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  std::atomic<bool> stop{false};
  RunOptions options;
  options.cancel = &stop;
  std::thread stopper([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop = true;
  });
  const auto result = script.run({}, options);
  stopper.join();
  CHECK_FALSE(result.ok);
  CHECK(result.diagnostics.find("execution stopped") != std::string::npos);
}

TEST_CASE("embed run with a flag that stays false finishes normally") {
  ScriptEngine engine;
  const auto script =
      engine.compileSource("/run_options_flag.prime", "[return<int>]\nmain() {\n  return(7i32)\n}\n");
  REQUIRE(script.valid());
  std::atomic<bool> stop{false};
  RunOptions options;
  options.cancel = &stop;
  const auto result = script.run({}, options);
  CHECK(result.ok);
  CHECK(result.exitCode == 7);
}

TEST_SUITE_END();
