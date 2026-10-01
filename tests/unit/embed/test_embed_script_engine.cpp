#include "embed_fixture_bytecode.h"
#include "primec/embed/ScriptEngine.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <string>
#include <vector>

TEST_SUITE_BEGIN("primestruct.embed.script_engine");

TEST_CASE("embed runs main from in-memory source") {
  primec::embed::ScriptEngine engine;
  const auto script = engine.compileSource("/embed_return_seven.prime", R"(
[return<int>]
main() {
  return(7i32)
}
)");
  REQUIRE(script.valid());
  const auto result = script.run();
  CHECK(result.ok);
  CHECK(result.exitCode == 7);
}

TEST_CASE("embed reruns one compiled script") {
  primec::embed::ScriptEngine engine;
  const auto script = engine.compileSource("/embed_rerun.prime", R"(
[return<int>]
main() {
  return(3i32)
}
)");
  REQUIRE(script.valid());
  for (int i = 0; i < 3; ++i) {
    const auto result = script.run();
    CHECK(result.ok);
    CHECK(result.exitCode == 3);
  }
}

TEST_CASE("embed reports semantic errors as data") {
  primec::embed::ScriptEngine engine;
  const auto script = engine.compileSource("/embed_bad.prime", R"(
[return<int>]
main() {
  return(missing_helper())
}
)");
  CHECK_FALSE(script.valid());
  CHECK_FALSE(script.diagnostics().empty());
  const auto result = script.run();
  CHECK_FALSE(result.ok);
  CHECK_FALSE(result.diagnostics.empty());
}

TEST_CASE("embed resolves stdlib imports without caller setup") {
  primec::embed::ScriptEngine engine;
  const auto script = engine.compileSource("/embed_stdlib.prime", R"(
import /std/math/*

[return<int>]
main() {
  return(convert<i32>(abs(-4.0f)))
}
)");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  CHECK(result.ok);
  CHECK(result.exitCode == 4);
}

TEST_CASE("embed bytecode round trips through save and load") {
  primec::embed::ScriptEngine engine;
  // Source and name must match the fixture in embed_fixture_bytecode.h exactly.
  const auto script = engine.compileSource("/x.prime", "[return<int>]\nmain() {\n  return(11i32)\n}\n");
  REQUIRE(script.valid());
  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(script.saveBytecode(bytes, error));
  CHECK_FALSE(bytes.empty());
  // Keeps tests/unit/embed/embed_fixture_bytecode.h in sync with the IR format.
  CHECK_MESSAGE(bytes == embedReturnElevenBytecode(),
                "refresh embed_fixture_bytecode.h from this script's saveBytecode output");
  const auto loaded = primec::embed::Script::loadBytecode(bytes);
  REQUIRE_MESSAGE(loaded.valid(), loaded.diagnostics());
  const auto result = loaded.run();
  CHECK(result.ok);
  CHECK(result.exitCode == 11);
}

TEST_CASE("embed save bytecode rejects an invalid script") {
  primec::embed::ScriptEngine engine;
  const auto script = engine.compileSource("/embed_bytecode_bad.prime", "main() { return(nope()) }");
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_FALSE(script.saveBytecode(bytes, error));
  CHECK_FALSE(error.empty());
}

TEST_SUITE_END();
