#include "primec/embed/Script.h"
#include "primec/embed/ScriptEngine.h"
#include "primec/testing/TestScratch.h"
#include "primec/ui/NativeUiBindings.h"
#include "primec/ui/NativeUiHeadless.h"

#include "tests/fixtures/host_calls/scenario.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace headless = primec::ui::headless;
using primec::embed::Script;
using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.backend.cpp_native_ui");

namespace {
const std::string SourceDir = PRIMESTRUCT_SOURCE_DIR;
const std::string Primec = PRIMESTRUCT_PRIMEC_PATH;

std::string scratch(const std::string &name) {
  const std::filesystem::path dir = primec::testing::testScratchDir("cpp_native_ui");
  std::filesystem::create_directories(dir);
  return (dir / name).string();
}

std::string readFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::string shellQuote(const std::string &text) { return "'" + text + "'"; }

int run(const std::string &command) {
  const int status = std::system(command.c_str());
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

std::string joined(const std::vector<std::string> &lines) {
  std::string text;
  for (const std::string &line : lines) {
    text += line + "\n";
  }
  return text;
}

// A native UI program compiled with --emit=optexe and linked with the headless backend and the
// log dump of tests/fixtures/host_calls, built once per program.
const std::string &compiledProgram(const std::string &example, const std::string &name) {
  static std::map<std::string, std::string> programs;
  const auto found = programs.find(example);
  if (found != programs.end()) {
    return found->second;
  }
  const std::string program = scratch(name);
  const std::string log = program + ".log";
  const std::string command = shellQuote(Primec) + " --emit=optexe " + shellQuote(example) + " -o " + shellQuote(program) +
                              " --link-arg " + shellQuote(SourceDir + "/tests/fixtures/host_calls/headless_log_dump.cpp") +
                              " --link-arg " + shellQuote("-I" + SourceDir + "/include") + " --link-arg " +
                              shellQuote(PRIMESTRUCT_UI_HEADLESS_LIB) + " --link-arg " +
                              shellQuote(PRIMESTRUCT_UI_HIGHLIGHT_LIB) + " > " + shellQuote(log) + " 2>&1";
  const int status = run(command);
  REQUIRE_MESSAGE(status == 0, readFile(log));
  return programs.emplace(example, program).first->second;
}

// Runs `example` on the VM and as a compiled program with the same scripted user actions and
// arguments, and checks the backend's call log and the exit code agree.
void checkScenario(const std::string &example,
                   const std::string &name,
                   const std::string &actions,
                   const std::vector<std::string> &args = {}) {
  headless::reset();
  applyHeadlessScenario(actions);
  ScriptEngine engine;
  primec::ui::bindNativeUi(engine);
  const Script script = engine.compileFile(example);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto vm = script.run(args);
  REQUIRE_MESSAGE(vm.ok, vm.diagnostics);
  const std::string vmLog = joined(headless::callLog());
  REQUIRE(vmLog.size() > 100);

  const std::string &program = compiledProgram(example, name);
  const std::string output = scratch(name + ".out");
  std::string command = "HEADLESS_SCENARIO=" + shellQuote(actions) + " " + shellQuote(program);
  for (const std::string &arg : args) {
    command += " " + shellQuote(arg);
  }
  const int exitCode = run(command + " > " + shellQuote(output));
  CHECK_MESSAGE(exitCode == (vm.exitCode & 0xFF), actions);
  CHECK_MESSAGE(readFile(output) == vmLog, actions);
}

std::string editor() { return SourceDir + "/examples/apps/text_editor/main.prime"; }

std::string writeScratch(const std::string &name, const std::string &contents) {
  const std::string path = scratch(name);
  std::ofstream(path, std::ios::binary) << contents;
  return path;
}
} // namespace

TEST_CASE("the native UI hello_window runs through optexe on the headless backend") {
  checkScenario(SourceDir + "/examples/native_ui/hello_window.prime", "hello", "cmd:7;type:0: more;cmd:3;close:1");
}

TEST_CASE("the editor types and saves through optexe with the VM's call log") {
  const std::string out = scratch("typed.txt");
  checkScenario(editor(), "editor", "type:0:hello;cmd:3;save:" + out + ";close:1");
  CHECK(readFile(out) == "hello");
}

TEST_CASE("the editor opens a chosen file through optexe with the VM's call log") {
  const std::string in = writeScratch("existing.txt", "h\xC3\xA9llo\nworld\n");
  checkScenario(editor(), "editor", "cmd:2;open:" + in + ";close:1");
}

TEST_CASE("the editor opens the file named on the command line through optexe") {
  const std::string in = writeScratch("argument.txt", "from the command line\n");
  checkScenario(editor(), "editor", "close:1", {in});
}

TEST_CASE("the editor reports a file that cannot be opened through optexe") {
  checkScenario(editor(), "editor", "cmd:2;open:/definitely/not/here.txt;alert:0;close:1");
}

TEST_CASE("the editor asks before closing unsaved text through optexe") {
  const std::string out = scratch("close_save.txt");
  checkScenario(editor(), "editor", "type:0:unsaved;alert:0;save:" + out + ";close:1");
}
