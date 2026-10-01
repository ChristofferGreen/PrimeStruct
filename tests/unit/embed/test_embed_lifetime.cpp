#include "embed_fixture_programs.h"
#include "embed_test_support.h"
#include "primec/embed/ScriptEngine.h"

#include "third_party/doctest.h"

#include <atomic>
#include <cstdint>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <unistd.h>
#endif

using primec::embed::Script;
using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.embed.lifetime");

namespace {
// Resident set size in bytes, or 0 where it cannot be measured.
uint64_t residentBytes() {
#if defined(__linux__)
  std::ifstream statm("/proc/self/statm");
  uint64_t pages = 0;
  uint64_t resident = 0;
  if (!(statm >> pages >> resident)) {
    return 0;
  }
  return resident * static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
#else
  return 0;
#endif
}

constexpr uint64_t AllowedGrowthBytes = 8ull * 1024ull * 1024ull;

const char *LoopSource = "[return<int>]\nmain() {\n  [mut] sum{0i32}\n  [mut] i{1i32}\n  while(i <= 10i32) {\n"
                         "    sum = sum + i\n    i = i + 1i32\n  }\n  return(sum)\n}\n";
} // namespace

TEST_CASE("one compiled script runs 1000 times with stable results and no memory growth") {
  ScriptEngine engine;
  const auto script = engine.compileSource("/lifetime_run.prime", LoopSource);
  REQUIRE(script.valid());
  for (int i = 0; i < 100; ++i) {  // warm up allocator and caches
    REQUIRE(script.run().exitCode == 55);
  }
  const uint64_t before = residentBytes();
  for (int i = 0; i < 1000; ++i) {
    const auto result = script.run();
    REQUIRE(result.ok);
    REQUIRE(result.exitCode == 55);
  }
  const uint64_t after = residentBytes();
  if (before != 0) {
    CHECK_MESSAGE(after < before + AllowedGrowthBytes, "resident bytes grew from " << before << " to " << after);
  }
}

TEST_CASE("repeated exported calls with host bindings and strings do not grow memory") {
  ScriptEngine engine;
  engine.exportFunction<int32_t(std::string_view, int32_t)>("measure");
  auto script = engine.compileSource("/lifetime_call.prime", R"(
[host return<int>]
host_double([i32] value) {
}

[return<int>]
measure([string] text, [i32] extra) {
  return(host_double(text.count()) + extra)
}

[return<int>]
main() {
  return(0i32)
}
)");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  script.bind("host_double", [](int32_t v) { return v * 2; });
  for (int i = 0; i < 100; ++i) {
    REQUIRE(script.call<int32_t>("measure", "warm up", 1).ok);
  }
  const uint64_t before = residentBytes();
  for (int i = 0; i < 1000; ++i) {
    const std::string text(static_cast<size_t>(i % 50), 'x');
    const auto result = script.call<int32_t>("measure", text, 3);
    REQUIRE(result.ok);
    REQUIRE(result.value == static_cast<int32_t>(text.size()) * 2 + 3);
  }
  const uint64_t after = residentBytes();
  if (before != 0) {
    CHECK_MESSAGE(after < before + AllowedGrowthBytes, "resident bytes grew from " << before << " to " << after);
  }
}

TEST_CASE("compiling the same script repeatedly does not leak or grow caches") {
  ScriptEngine engine;
  for (int i = 0; i < 20; ++i) {
    REQUIRE(engine.compileSource("/lifetime_compile.prime", LoopSource).valid());
  }
  const uint64_t before = residentBytes();
  for (int i = 0; i < 200; ++i) {
    const auto script = engine.compileSource("/lifetime_compile.prime", LoopSource);
    REQUIRE(script.valid());
    REQUIRE(script.run().exitCode == 55);
  }
  const uint64_t after = residentBytes();
  if (before != 0) {
    CHECK_MESSAGE(after < before + AllowedGrowthBytes, "resident bytes grew from " << before << " to " << after);
  }
}

TEST_CASE("failed compiles do not leak or poison later compiles") {
  ScriptEngine engine;
  for (int i = 0; i < 20; ++i) {
    (void)engine.compileSource("/lifetime_bad.prime", "[return<int>]\nmain() {\n  return(nope())\n}\n");
  }
  const uint64_t before = residentBytes();
  for (int i = 0; i < 200; ++i) {
    const auto bad = engine.compileSource("/lifetime_bad.prime", "[return<int>]\nmain() {\n  return(nope())\n}\n");
    REQUIRE_FALSE(bad.valid());
    REQUIRE(engine.compileSource("/lifetime_good.prime", LoopSource).run().exitCode == 55);
  }
  const uint64_t after = residentBytes();
  if (before != 0) {
    CHECK_MESSAGE(after < before + AllowedGrowthBytes, "resident bytes grew from " << before << " to " << after);
  }
}

TEST_CASE("several engines and scripts coexist and survive each other's destruction") {
  std::vector<ScriptEngine> engines(4);
  std::vector<Script> scripts;
  for (size_t i = 0; i < engines.size(); ++i) {
    const auto &program = embedPrograms()[i];
    scripts.push_back(engines[i].compileSource("/fixture/" + program.name + ".prime", program.source));
    REQUIRE(scripts.back().valid());
  }
  engines.erase(engines.begin());  // destroy an engine while its script lives
  scripts.erase(scripts.begin() + 1);
  for (size_t i = 0; i < scripts.size(); ++i) {
    CHECK(scripts[i].run().ok);
  }
}

TEST_CASE("engines on separate threads compile identical bytecode to a single threaded compile") {
  const auto &programs = embedPrograms();
  std::vector<std::vector<uint8_t>> reference;
  for (const auto &program : programs) {
    ScriptEngine engine;
    std::vector<uint8_t> bytes;
    std::string error;
    REQUIRE(engine.compileSource("/fixture/" + program.name + ".prime", program.source).saveBytecode(bytes, error));
    reference.push_back(std::move(bytes));
  }
  std::atomic<int> mismatches{0};
  std::vector<std::thread> threads;
  for (size_t t = 0; t < 4; ++t) {
    threads.emplace_back([&, t] {
      ScriptEngine engine;
      for (int round = 0; round < 5; ++round) {
        for (size_t i = 0; i < programs.size(); ++i) {
          const size_t index = (i + t) % programs.size();
          std::vector<uint8_t> bytes;
          std::string error;
          const auto script =
              engine.compileSource("/fixture/" + programs[index].name + ".prime", programs[index].source);
          if (!script.valid() || !script.saveBytecode(bytes, error) || bytes != reference[index]) {
            ++mismatches;
          }
        }
      }
    });
  }
  for (auto &thread : threads) {
    thread.join();
  }
  CHECK(mismatches.load() == 0);
}

TEST_CASE("scripts compiled on one thread run on another") {
  ScriptEngine engine;
  const auto script = engine.compileSource("/lifetime_cross_thread.prime", LoopSource);
  REQUIRE(script.valid());
  int exitCode = -1;
  std::thread runner([&] { exitCode = script.run().exitCode; });
  runner.join();
  CHECK(exitCode == 55);
}

TEST_SUITE_END();
