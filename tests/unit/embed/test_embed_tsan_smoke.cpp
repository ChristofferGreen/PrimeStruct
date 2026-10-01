// ThreadSanitizer smoke for the embedding API: independent engines compile and
// run on separate threads, and one compiled script is shared by several
// threads. Built only with PRIMESTRUCT_ENABLE_TSAN_SEMANTICS_SMOKE=ON.
#include "embed_fixture_programs.h"
#include "primec/embed/ScriptEngine.h"

#include "third_party/doctest.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

using primec::embed::Script;
using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.embed.tsan_smoke");

TEST_CASE("two engines compile and run concurrently") {
  std::atomic<int> failures{0};
  std::vector<std::thread> threads;
  for (size_t t = 0; t < 2; ++t) {
    threads.emplace_back([&, t] {
      ScriptEngine engine;
      for (int round = 0; round < 3; ++round) {
        for (const auto &program : embedPrograms()) {
          auto script = engine.compileSource("/fixture/" + program.name + ".prime", program.source);
          if (!script.valid()) {
            ++failures;
            continue;
          }
          bindEmbedFixtureHosts(script);
          if (script.run(program.args).exitCode != program.expectedExit) {
            ++failures;
          }
        }
        (void)t;
      }
    });
  }
  for (auto &thread : threads) {
    thread.join();
  }
  CHECK(failures.load() == 0);
}

TEST_CASE("one compiled script is shared by several threads") {
  ScriptEngine engine;
  engine.exportFunction<int32_t(int32_t, int32_t)>("add");
  engine.exportFunction<int32_t(std::string_view)>("count_chars");
  auto script = engine.compileSource("/tsan_shared.prime", R"(
[return<int>]
add([i32] a, [i32] b) {
  return(a + b)
}

[return<int>]
count_chars([string] text) {
  return(text.count())
}

[return<int>]
main() {
  return(3i32)
}
)");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  std::atomic<int> failures{0};
  std::vector<std::thread> threads;
  for (int t = 0; t < 4; ++t) {
    threads.emplace_back([&, t] {
      for (int i = 0; i < 25; ++i) {
        if (script.run().exitCode != 3 || script.call<int32_t>("add", t, i).value != t + i ||
            script.call<int32_t>("count_chars", std::string(static_cast<size_t>(i), 'a')).value != i) {
          ++failures;
        }
      }
    });
  }
  for (auto &thread : threads) {
    thread.join();
  }
  CHECK(failures.load() == 0);
}

TEST_SUITE_END();
