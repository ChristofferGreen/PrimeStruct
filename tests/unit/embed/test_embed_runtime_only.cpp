// Links only primec_embed_runtime_lib: no parser, semantics, or lowerer.
#include "embed_fixture_bytecode.h"
#include "primec/embed/Script.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <string>
#include <vector>

TEST_SUITE_BEGIN("primestruct.embed.runtime_only");


TEST_CASE("runtime-only library runs precompiled bytecode") {
  const auto script = primec::embed::Script::loadBytecode(embedReturnElevenBytecode());
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  CHECK(result.ok);
  CHECK(result.exitCode == 11);
}

TEST_CASE("runtime-only library rejects empty and truncated bytecode") {
  CHECK_FALSE(primec::embed::Script::loadBytecode({}).valid());
  std::vector<uint8_t> truncated = embedReturnElevenBytecode();
  truncated.resize(truncated.size() / 2);
  const auto script = primec::embed::Script::loadBytecode(truncated);
  CHECK_FALSE(script.valid());
  CHECK_FALSE(script.diagnostics().empty());
  CHECK_FALSE(script.run().ok);
}

TEST_CASE("runtime-only library rejects bytes with a corrupted header") {
  std::vector<uint8_t> corrupt = embedReturnElevenBytecode();
  for (size_t i = 0; i < corrupt.size() && i < 8; ++i) {
    corrupt[i] = 0xFF;
  }
  CHECK_FALSE(primec::embed::Script::loadBytecode(corrupt).valid());
}

TEST_SUITE_END();
