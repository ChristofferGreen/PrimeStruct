#include "embed_fixture_bytecode.h"
#include "embed_fixture_programs.h"
#include "primec/embed/ScriptEngine.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

using primec::embed::Script;
using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.embed.bytecode");

namespace {
std::vector<uint8_t> saveOrFail(const Script &script) {
  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE_MESSAGE(script.saveBytecode(bytes, error), error);
  return bytes;
}
} // namespace

TEST_CASE("bytecode of every fixture program runs like the compiled script") {
  ScriptEngine engine;
  for (const auto &program : embedPrograms()) {
    CAPTURE(program.name);
    auto compiled = engine.compileSource("/fixture/" + program.name + ".prime", program.source);
    REQUIRE_MESSAGE(compiled.valid(), compiled.diagnostics());
    auto loaded = Script::loadBytecode(saveOrFail(compiled), program.name);
    REQUIRE_MESSAGE(loaded.valid(), loaded.diagnostics());
    bindEmbedFixtureHosts(compiled);
    bindEmbedFixtureHosts(loaded);
    const auto direct = compiled.run(program.args);
    const auto viaBytecode = loaded.run(program.args);
    CHECK(viaBytecode.ok);
    CHECK(viaBytecode.exitCode == direct.exitCode);
    CHECK(viaBytecode.exitCode == program.expectedExit);
  }
}

TEST_CASE("bytecode saving is deterministic") {
  ScriptEngine engine;
  for (const auto &program : embedPrograms()) {
    CAPTURE(program.name);
    const auto first = engine.compileSource("/fixture/" + program.name + ".prime", program.source);
    const auto second = engine.compileSource("/fixture/" + program.name + ".prime", program.source);
    REQUIRE(first.valid());
    REQUIRE(second.valid());
    CHECK(saveOrFail(first) == saveOrFail(second));
    CHECK(saveOrFail(first) == saveOrFail(first));
  }
}

TEST_CASE("bytecode survives a save load save cycle byte for byte") {
  ScriptEngine engine;
  for (const auto &program : embedPrograms()) {
    CAPTURE(program.name);
    const auto compiled = engine.compileSource("/fixture/" + program.name + ".prime", program.source);
    REQUIRE(compiled.valid());
    const auto bytes = saveOrFail(compiled);
    const auto loaded = Script::loadBytecode(bytes);
    REQUIRE(loaded.valid());
    CHECK(saveOrFail(loaded) == bytes);
  }
}

TEST_CASE("bytecode loaded scripts rerun and copy cleanly") {
  const auto loaded = Script::loadBytecode(embedProgramBytecode()[4]);
  REQUIRE(loaded.valid());
  const Script copy = loaded;
  for (int i = 0; i < 20; ++i) {
    CHECK(loaded.run().exitCode == 55);
    CHECK(copy.run().exitCode == 55);
  }
}

TEST_CASE("bytecode has the expected magic and a bumped version is rejected") {
  const auto &bytes = embedProgramBytecode()[0];
  REQUIRE(bytes.size() > 8);
  CHECK(bytes[0] == 'R');
  CHECK(bytes[1] == 'I');
  CHECK(bytes[2] == 'S');
  CHECK(bytes[3] == 'P');
  auto future = bytes;
  future[4] = 0x7f;
  const auto script = Script::loadBytecode(future);
  CHECK_FALSE(script.valid());
  CHECK_FALSE(script.diagnostics().empty());
}

TEST_CASE("bytecode rejects every strict prefix without crashing") {
  for (const auto &bytes : embedProgramBytecode()) {
    for (size_t length = 0; length < bytes.size(); ++length) {
      const std::vector<uint8_t> prefix(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(length));
      const auto script = Script::loadBytecode(prefix);
      CHECK_FALSE(script.valid());
    }
  }
}

TEST_CASE("bytecode with trailing garbage never crashes the loader") {
  for (const auto &bytes : embedProgramBytecode()) {
    auto extended = bytes;
    extended.insert(extended.end(), {0xde, 0xad, 0xbe, 0xef});
    auto script = Script::loadBytecode(extended);
    if (script.valid()) {
      bindEmbedFixtureHosts(script);
      CHECK(script.run().ok);
    } else {
      CHECK_FALSE(script.diagnostics().empty());
    }
  }
}

TEST_CASE("bytecode single byte corruption never crashes the loader") {
  for (const auto &bytes : embedProgramBytecode()) {
    for (size_t i = 0; i < bytes.size(); ++i) {
      auto corrupt = bytes;
      corrupt[i] = static_cast<uint8_t>(corrupt[i] ^ 0xff);
      const auto script = Script::loadBytecode(corrupt);
      // Corruption may still decode to a valid (different) module; the loader
      // must simply stay well-defined. Never run it: it could loop forever.
      if (!script.valid()) {
        CHECK_FALSE(script.diagnostics().empty());
      }
    }
  }
}

TEST_CASE("bytecode with hostile 32-bit counts is rejected without huge allocations") {
  // Regression: a corrupt element count used to reach `reserve` unchecked and
  // threw std::bad_alloc. Overwrite every 4-byte window with 0xFFFFFFFF.
  for (const auto &bytes : embedProgramBytecode()) {
    for (size_t at = 8; at + 4 <= bytes.size(); ++at) {
      auto hostile = bytes;
      for (size_t i = 0; i < 4; ++i) {
        hostile[at + i] = 0xff;
      }
      const auto script = Script::loadBytecode(hostile);
      if (!script.valid()) {
        CHECK_FALSE(script.diagnostics().empty());
      }
    }
  }
}

TEST_CASE("bytecode random garbage is rejected") {
  std::mt19937 rng(0x5eed);
  for (int round = 0; round < 200; ++round) {
    std::vector<uint8_t> garbage(1 + rng() % 256);
    for (auto &byte : garbage) {
      byte = static_cast<uint8_t>(rng());
    }
    CHECK_FALSE(Script::loadBytecode(garbage).valid());
  }
}

TEST_CASE("bytecode with a valid header but random body never crashes") {
  std::mt19937 rng(0xb0d1);
  const auto &seed = embedProgramBytecode()[0];
  for (int round = 0; round < 200; ++round) {
    auto mutated = seed;
    const size_t flips = 1 + rng() % 4;
    for (size_t i = 0; i < flips; ++i) {
      mutated[8 + rng() % (mutated.size() - 8)] = static_cast<uint8_t>(rng());
    }
    const auto script = Script::loadBytecode(mutated);
    if (!script.valid()) {
      CHECK_FALSE(script.diagnostics().empty());
    }
  }
}

TEST_SUITE_END();
