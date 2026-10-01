#include "embed_test_support.h"
#include "primec/embed/ScriptEngine.h"

#include "third_party/doctest.h"

#include <string>

using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.embed.diagnostics");

namespace {
struct BadProgram {
  const char *label;
  const char *source;
};

const BadProgram BadPrograms[] = {
    {"undefined helper", "[return<int>]\nmain() {\n  return(missing_helper())\n}\n"},
    {"parse error", "[return<int>]\nmain( {\n  return(1i32)\n}\n"},
    {"type mismatch", "[return<int>]\nmain() {\n  return(\"text\")\n}\n"},
    {"missing entry", "[return<int>]\nother() {\n  return(1i32)\n}\n"},
    {"unknown import", "import /std/does_not_exist/*\n\n[return<int>]\nmain() {\n  return(1i32)\n}\n"},
    {"duplicate definition",
     "[return<int>]\nmain() {\n  return(1i32)\n}\n\n[return<int>]\nmain() {\n  return(2i32)\n}\n"},
    {"unterminated block", "[return<int>]\nmain() {\n  return(1i32)\n"},
    {"empty source", ""},
};
} // namespace

TEST_CASE("embed reports every bad program as data") {
  ScriptEngine engine;
  for (const auto &bad : BadPrograms) {
    CAPTURE(bad.label);
    const auto script = engine.compileSource("/embed_bad.prime", bad.source);
    CHECK_FALSE(script.valid());
    CHECK_FALSE(script.diagnostics().empty());
    const auto result = script.run();
    CHECK_FALSE(result.ok);
    CHECK_FALSE(result.diagnostics.empty());
  }
}

TEST_CASE("embed failure paths write nothing to stdout or stderr") {
  ScriptEngine engine;
  EmbedStreamCapture capture;
  for (const auto &bad : BadPrograms) {
    const auto script = engine.compileSource("/embed_quiet.prime", bad.source);
    (void)script.run();
  }
  (void)engine.compileFile("/definitely/not/here/missing.prime");
  CHECK(capture.finish().empty());
}

TEST_CASE("embed success paths write nothing to stdout or stderr") {
  ScriptEngine engine;
  EmbedStreamCapture capture;
  const auto script = engine.compileSource("/embed_quiet_ok.prime", "[return<int>]\nmain() {\n  return(5i32)\n}\n");
  REQUIRE(script.valid());
  CHECK(script.run().exitCode == 5);
  CHECK(capture.finish().empty());
}

TEST_CASE("embed diagnostics name the source and the problem") {
  ScriptEngine engine;
  const auto script = engine.compileSource("/embed_named.prime", BadPrograms[0].source);
  CHECK_FALSE(script.valid());
  CHECK(script.diagnostics().find("embed_named.prime") != std::string::npos);
  CHECK(script.diagnostics().find("missing_helper") != std::string::npos);
}

TEST_CASE("embed engine recovers and compiles after a failure") {
  ScriptEngine engine;
  CHECK_FALSE(engine.compileSource("/embed_after_fail.prime", BadPrograms[0].source).valid());
  const auto good = engine.compileSource("/embed_after_fail.prime", "[return<int>]\nmain() {\n  return(8i32)\n}\n");
  REQUIRE_MESSAGE(good.valid(), good.diagnostics());
  CHECK(good.run().exitCode == 8);
}

TEST_CASE("embed handles unusual but valid source text") {
  ScriptEngine engine;
  const auto crlf = engine.compileSource("/embed_crlf.prime", "[return<int>]\r\nmain() {\r\n  return(6i32)\r\n}\r\n");
  REQUIRE_MESSAGE(crlf.valid(), crlf.diagnostics());
  CHECK(crlf.run().exitCode == 6);
  const auto comments = engine.compileSource(
      "/embed_comments.prime", "// leading comment\n[return<int>]\nmain() {\n  /* inline */ return(3i32)\n}\n// trailing\n");
  REQUIRE_MESSAGE(comments.valid(), comments.diagnostics());
  CHECK(comments.run().exitCode == 3);
  const auto noNewline = engine.compileSource("/embed_nonl.prime", "[return<int>]\nmain() {\n  return(2i32)\n}");
  REQUIRE_MESSAGE(noNewline.valid(), noNewline.diagnostics());
  CHECK(noNewline.run().exitCode == 2);
}

TEST_SUITE_END();
