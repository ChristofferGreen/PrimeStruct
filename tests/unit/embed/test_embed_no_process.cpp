#include "embed_test_support.h"
#include "primec/embed/ScriptEngine.h"
#include "primec/support/ProcessRunner.h"

#include "third_party/doctest.h"

#include <cerrno>
#include <string>

using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.embed.no_process");

TEST_CASE("process spawning availability matches the system runner") {
  const int status = primec::systemProcessRunner().run({"true"});
  if (primec::processSpawningAvailable()) {
    CHECK(status == 0);
  } else {
    // PRIMESTRUCT_EMBED_NO_PROCESS (and iOS): nothing is spawned.
    CHECK(status == ENOSYS);
  }
  CHECK(primec::systemProcessRunner().run({}) != 0);
}

TEST_CASE("a script that needs no process compiles and runs either way") {
  ScriptEngine engine;
  const auto script = engine.compileSource("/no_process.prime", "[return<int>]\nmain() {\n  return(12i32)\n}\n");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  CHECK(script.run().exitCode == 12);
}

TEST_CASE("archive import roots report process spawning problems clearly") {
  if (primec::processSpawningAvailable()) {
    // Normal builds would run unzip, which prints its own errors; the message
    // under test only exists when spawning is compiled out.
    return;
  }
  const auto dir = embedTestDir("no_process_archive");
  const auto archive = embedWriteFile(dir / "lib.zip", "not really a zip");
  ScriptEngine engine;
  engine.addImportPath(archive.string());
  const auto script = engine.compileSource("/no_process_archive.prime", "[return<int>]\nmain() {\n  return(1i32)\n}\n");
  CHECK_FALSE(script.valid());
  CHECK(script.diagnostics().find("failed to extract archive") != std::string::npos);
  CHECK(script.diagnostics().find("need process spawning") != std::string::npos);
}

TEST_SUITE_END();
