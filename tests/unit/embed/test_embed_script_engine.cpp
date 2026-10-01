#include "primec/embed/ScriptEngine.h"

#include "third_party/doctest.h"

#include <fstream>

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

TEST_SUITE_END();
