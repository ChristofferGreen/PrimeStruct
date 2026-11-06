// The `net_io` effect (docs/spec/language-core.md, Effects) and /std/net (docs/spec/stdlib-reference.md, Network).
#include "primec/embed/Script.h"
#include "primec/embed/ScriptEngine.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.backend.net");

namespace {
const std::string Primec = PRIMESTRUCT_PRIMEC_PATH;

std::string scratch(const std::string &name) {
  const std::filesystem::path dir = primec::testing::testScratchDir("net");
  std::filesystem::create_directories(dir);
  return (dir / name).string();
}

std::string readFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void writeFile(const std::string &path, const std::string &text) {
  std::ofstream file(path, std::ios::binary);
  file << text;
}

std::string shellQuote(const std::string &text) { return "'" + text + "'"; }

int run(const std::string &command) {
  const int status = std::system(command.c_str());
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

struct PrimecResult {
  int status = 0;
  std::string output;
};

// Runs primec with `arguments` on `source` written to the scratch file `name`.prime; output holds stdout and stderr.
PrimecResult runPrimec(const std::string &name, const std::string &source, const std::string &arguments) {
  const std::string path = scratch(name + ".prime");
  writeFile(path, source);
  const std::string log = scratch(name + ".log");
  PrimecResult result;
  result.status = run(shellQuote(Primec) + " " + arguments + " " + shellQuote(path) + " > " + shellQuote(log) + " 2>&1");
  result.output = readFile(log);
  return result;
}

const std::string Fixtures = std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/fixtures/host_calls/";
const std::string NetShim = std::string(PRIMESTRUCT_SOURCE_DIR) + "/stdlib/std/net/native/net_shim.cpp";

// Builds the fixture `name`.prime with --emit=optexe and the /std/net shim; returns the executable path.
std::string buildWithNetShim(const std::string &name) {
  const std::string exe = scratch(name);
  const std::string log = exe + ".log";
  const int status = run(shellQuote(Primec) + " --emit=optexe " + shellQuote(Fixtures + name + ".prime") + " -o " +
                         shellQuote(exe) + " --link-arg " + shellQuote(NetShim) + " > " + shellQuote(log) + " 2>&1");
  REQUIRE_MESSAGE(status == 0, readFile(log));
  return exe;
}

const std::string NetEffectProgram = R"(
[return<int> effects(net_io)]
main() {
  return(7i32)
}
)";
} // namespace

TEST_CASE("the net_io effect sets bit 12 of the IR effect mask and runs on the VM") {
  const PrimecResult dump = runPrimec("net_effect_dump", NetEffectProgram, "--dump-stage ir-lowered");
  REQUIRE_MESSAGE(dump.status == 0, dump.output);
  CHECK_MESSAGE(dump.output.find("function 0 /main parameters=0 effects=0x1000 ") != std::string::npos, dump.output);

  ScriptEngine engine;
  const auto script = engine.compileSource("/net_effect.prime", NetEffectProgram);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.exitCode == 7);
}

TEST_CASE("wasm and glsl reject the net_io effect") {
  const PrimecResult wasm =
      runPrimec("net_effect_wasm", NetEffectProgram, "--emit=wasm -o " + shellQuote(scratch("net_effect.wasm")));
  CHECK(wasm.status != 0);
  CHECK_MESSAGE(wasm.output.find("unsupported effect mask bits for wasm target") != std::string::npos, wasm.output);

  const PrimecResult browser = runPrimec("net_effect_browser",
                                      NetEffectProgram,
                                      "--emit=wasm --wasm-profile browser -o " +
                                          shellQuote(scratch("net_effect_browser.wasm")));
  CHECK(browser.status != 0);
  CHECK_MESSAGE(browser.output.find("unsupported effect mask bits for wasm-browser target") != std::string::npos,
                browser.output);

  const PrimecResult glsl =
      runPrimec("net_effect_glsl", NetEffectProgram, "--emit=glsl -o " + shellQuote(scratch("net_effect.glsl")));
  CHECK(glsl.status != 0);
  CHECK_MESSAGE(glsl.output.find("unsupported effect mask bits for glsl target") != std::string::npos, glsl.output);
}

TEST_CASE("std/net builds with the POSIX shim on optexe and is unbound on the VM") {
  CHECK(std::filesystem::exists(buildWithNetShim("net_echo")));

  ScriptEngine engine;
  const auto script = engine.compileFile(Fixtures + "net_echo.prime");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto vm = script.run();
  CHECK_FALSE(vm.ok);
  CHECK_MESSAGE(vm.diagnostics.find("unbound host function: net_listen") != std::string::npos, vm.diagnostics);
}

TEST_CASE("a loopback echo through std/net round-trips bytes with timeouts on every call") {
  // One process, one thread, an ephemeral port on 127.0.0.1 (no firewall prompt, no window), and a timeout on every
  // call, so a broken step fails within seconds instead of hanging. The program's exit code names the failed step
  // (tests/fixtures/host_calls/net_echo.prime); 77 means the environment forbids sockets.
  const std::string exe = buildWithNetShim("net_echo");
  const int exitCode = run(shellQuote(exe));
  if (exitCode == 77) {
    MESSAGE("skipped: socket() is not permitted here (EPERM or EACCES)");
    return;
  }
  CHECK_MESSAGE(exitCode == 0, "net_echo failed at step " << exitCode);
}

TEST_SUITE_END();
