#include "embed_fixture_programs.h"
#include "primec/embed/ScriptEngine.h"

#include "third_party/doctest.h"

#include <atomic>
#include <thread>
#include <vector>

using primec::embed::Script;
using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.embed.threads");

TEST_CASE("embed runs one compiled script from several threads") {
  ScriptEngine engine;
  const auto script = engine.compileSource("/embed_threads.prime", embedPrograms()[4].source);
  REQUIRE(script.valid());
  std::atomic<int> failures{0};
  std::vector<std::thread> threads;
  for (int t = 0; t < 8; ++t) {
    threads.emplace_back([&] {
      for (int i = 0; i < 25; ++i) {
        const auto result = script.run();
        if (!result.ok || result.exitCode != 55) {
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

TEST_CASE("embed keeps several compiled scripts alive and interleaved") {
  ScriptEngine engine;
  std::vector<Script> scripts;
  for (const auto &program : embedPrograms()) {
    scripts.push_back(engine.compileSource("/fixture/" + program.name + ".prime", program.source));
    REQUIRE_MESSAGE(scripts.back().valid(), scripts.back().diagnostics());
    bindEmbedFixtureHosts(scripts.back());
  }
  for (int round = 0; round < 5; ++round) {
    for (size_t i = scripts.size(); i-- > 0;) {
      CHECK(scripts[i].run(embedPrograms()[i].args).exitCode == embedPrograms()[i].expectedExit);
    }
  }
}

TEST_CASE("embed outlives the engine that compiled it") {
  Script script;
  {
    ScriptEngine engine;
    script = engine.compileSource("/embed_outlive.prime", embedPrograms()[3].source);
  }
  REQUIRE(script.valid());
  CHECK(script.run().exitCode == 43);
}

TEST_CASE("embed compiles independent scripts on separate threads") {
  std::atomic<int> failures{0};
  std::vector<std::thread> threads;
  for (int t = 0; t < 4; ++t) {
    threads.emplace_back([&, t] {
      ScriptEngine engine;
      const auto &program = embedPrograms()[static_cast<size_t>(t) % embedPrograms().size()];
      auto script = engine.compileSource("/fixture/" + program.name + ".prime", program.source);
      if (script.valid()) {
        bindEmbedFixtureHosts(script);
      }
      if (!script.valid() || script.run(program.args).exitCode != program.expectedExit) {
        ++failures;
      }
    });
  }
  for (auto &thread : threads) {
    thread.join();
  }
  CHECK(failures.load() == 0);
}

TEST_SUITE_END();
