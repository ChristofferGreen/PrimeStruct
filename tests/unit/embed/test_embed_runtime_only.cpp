// Links only primec_embed_runtime_lib: no parser, semantics, or lowerer.
#include "embed_fixture_bytecode.h"
#include "embed_fixture_programs.h"
#include "primec/embed/Script.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <string>
#include <thread>
#include <vector>

using primec::embed::Script;

TEST_SUITE_BEGIN("primestruct.embed.runtime_only");

TEST_CASE("runtime-only library has one bytecode fixture per program") {
  CHECK(embedProgramBytecode().size() == embedPrograms().size());
}

TEST_CASE("runtime-only library runs every precompiled fixture") {
  REQUIRE(embedProgramBytecode().size() == embedPrograms().size());
  for (size_t i = 0; i < embedPrograms().size(); ++i) {
    const auto &program = embedPrograms()[i];
    CAPTURE(program.name);
    auto script = Script::loadBytecode(embedProgramBytecode()[i], program.name);
    REQUIRE_MESSAGE(script.valid(), script.diagnostics());
    bindEmbedFixtureHosts(script);
    const auto result = script.run(program.args);
    CHECK(result.ok);
    CHECK(result.exitCode == program.expectedExit);
  }
}

TEST_CASE("runtime-only library reports a missing host binding without running") {
  const size_t hostIndex = embedPrograms().size() - 1;
  REQUIRE(embedPrograms()[hostIndex].name == "host_call");
  const auto script = Script::loadBytecode(embedProgramBytecode()[hostIndex]);
  REQUIRE(script.valid());
  const auto result = script.run();
  CHECK_FALSE(result.ok);
  CHECK(result.diagnostics.find("unbound host function: host_add") != std::string::npos);
}

TEST_CASE("runtime-only library rejects empty input") {
  const auto script = Script::loadBytecode({});
  CHECK_FALSE(script.valid());
  CHECK_FALSE(script.diagnostics().empty());
  CHECK_FALSE(script.run().ok);
}

TEST_CASE("runtime-only library rejects every truncation") {
  for (const auto &bytes : embedProgramBytecode()) {
    for (size_t length = 0; length < bytes.size(); ++length) {
      const std::vector<uint8_t> prefix(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(length));
      CHECK_FALSE(Script::loadBytecode(prefix).valid());
    }
  }
}

TEST_CASE("runtime-only library rejects a corrupted header") {
  auto corrupt = embedProgramBytecode()[0];
  for (size_t i = 0; i < corrupt.size() && i < 8; ++i) {
    corrupt[i] = 0xFF;
  }
  CHECK_FALSE(Script::loadBytecode(corrupt).valid());
}

TEST_CASE("runtime-only library cannot save bytecode it never compiled but can re-save loaded modules") {
  const auto script = Script::loadBytecode(embedProgramBytecode()[0]);
  REQUIRE(script.valid());
  std::vector<uint8_t> out;
  std::string error;
  CHECK(script.saveBytecode(out, error));
  CHECK(out == embedProgramBytecode()[0]);
}

TEST_CASE("runtime-only library runs a loaded script from several threads") {
  const auto script = Script::loadBytecode(embedProgramBytecode()[4]);
  REQUIRE(script.valid());
  std::vector<std::thread> threads;
  std::vector<int> results(6, -1);
  for (size_t t = 0; t < results.size(); ++t) {
    threads.emplace_back([&, t] { results[t] = script.run().exitCode; });
  }
  for (auto &thread : threads) {
    thread.join();
  }
  for (const int value : results) {
    CHECK(value == 55);
  }
}

TEST_SUITE_END();
