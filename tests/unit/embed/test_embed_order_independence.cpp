#include "embed_fixture_programs.h"
#include "primec/embed/ScriptEngine.h"

#include "third_party/doctest.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <vector>

using primec::embed::Script;
using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.embed.order_independence");

// TODO-5358: process-global and thread-local compiler state (source-location
// cache, binding-type caches, stdlib surface registry, arena reset callbacks)
// must never make the IR bytes of a program depend on what was compiled before
// it or on which thread compiled it. See docs/CompilerArenaAllocator.md
// ("TODO-5358: compiler global state inventory").

namespace {
struct Fixture {
  std::string name;
  std::string source;
};

std::vector<Fixture> allFixtures() {
  std::vector<Fixture> fixtures;
  for (const auto &program : embedPrograms()) {
    fixtures.push_back({program.name, program.source});
  }
  fixtures.push_back({"bundle", embedBundleSource()});
  return fixtures;
}

std::vector<uint8_t> compileBytes(ScriptEngine &engine, const Fixture &fixture) {
  const auto script = engine.compileSource("/fixture/" + fixture.name + ".prime", fixture.source);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE_MESSAGE(script.saveBytecode(bytes, error), error);
  return bytes;
}

// Reference bytes: each fixture compiled alone in a fresh engine on this thread.
std::map<std::string, std::vector<uint8_t>> referenceBytes(const std::vector<Fixture> &fixtures) {
  std::map<std::string, std::vector<uint8_t>> reference;
  for (const auto &fixture : fixtures) {
    ScriptEngine engine;
    reference[fixture.name] = compileBytes(engine, fixture);
  }
  return reference;
}
} // namespace

TEST_CASE("embed fixtures compile to identical bytes in any order on one engine") {
  const auto fixtures = allFixtures();
  const auto reference = referenceBytes(fixtures);
  std::vector<size_t> order(fixtures.size());
  std::iota(order.begin(), order.end(), size_t{0});
  std::mt19937 rng(5358);
  for (int round = 0; round < 6; ++round) {
    if (round == 1) {
      std::reverse(order.begin(), order.end());
    } else if (round > 1) {
      std::shuffle(order.begin(), order.end(), rng);
    }
    ScriptEngine shared;
    for (const size_t index : order) {
      CAPTURE(fixtures[index].name);
      CHECK(compileBytes(shared, fixtures[index]) == reference.at(fixtures[index].name));
    }
  }
}

TEST_CASE("embed fixtures compile to identical bytes with a fresh engine per compile") {
  const auto fixtures = allFixtures();
  const auto reference = referenceBytes(fixtures);
  for (auto it = fixtures.rbegin(); it != fixtures.rend(); ++it) {
    CAPTURE(it->name);
    ScriptEngine engine;
    CHECK(compileBytes(engine, *it) == reference.at(it->name));
  }
}

TEST_CASE("embed fixtures compile to identical bytes on one worker thread") {
  const auto fixtures = allFixtures();
  const auto reference = referenceBytes(fixtures);
  std::map<std::string, std::vector<uint8_t>> observed;
  std::thread worker([&] {
    ScriptEngine engine;
    for (auto it = fixtures.rbegin(); it != fixtures.rend(); ++it) {
      observed[it->name] = compileBytes(engine, *it);
    }
  });
  worker.join();
  CHECK(observed == reference);
}

TEST_CASE("embed fixtures compile to identical bytes with one thread per fixture") {
  const auto fixtures = allFixtures();
  const auto reference = referenceBytes(fixtures);
  std::vector<std::vector<uint8_t>> observed(fixtures.size());
  std::vector<std::thread> threads;
  for (size_t i = 0; i < fixtures.size(); ++i) {
    threads.emplace_back([&, i] {
      ScriptEngine engine;
      observed[i] = compileBytes(engine, fixtures[i]);
    });
  }
  for (auto &thread : threads) {
    thread.join();
  }
  for (size_t i = 0; i < fixtures.size(); ++i) {
    CAPTURE(fixtures[i].name);
    CHECK(observed[i] == reference.at(fixtures[i].name));
  }
}

TEST_CASE("embed fixtures compile to identical bytes across threads with shuffled work lists") {
  const auto fixtures = allFixtures();
  const auto reference = referenceBytes(fixtures);
  constexpr int ThreadCount = 3;
  std::vector<std::map<std::string, std::vector<uint8_t>>> observed(ThreadCount);
  std::vector<std::thread> threads;
  for (int t = 0; t < ThreadCount; ++t) {
    threads.emplace_back([&, t] {
      std::vector<size_t> order(fixtures.size());
      std::iota(order.begin(), order.end(), size_t{0});
      std::mt19937 rng(5358u + static_cast<unsigned>(t));
      std::shuffle(order.begin(), order.end(), rng);
      ScriptEngine engine;
      for (const size_t index : order) {
        observed[t][fixtures[index].name] = compileBytes(engine, fixtures[index]);
      }
    });
  }
  for (auto &thread : threads) {
    thread.join();
  }
  for (int t = 0; t < ThreadCount; ++t) {
    CHECK(observed[t] == reference);
  }
}

TEST_SUITE_END();
