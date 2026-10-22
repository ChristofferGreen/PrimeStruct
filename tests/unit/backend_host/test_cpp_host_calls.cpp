#include "primec/embed/Script.h"
#include "primec/embed/ScriptEngine.h"
#include "primec/testing/TestScratch.h"
#include "primec/ui/NativeUiBindings.h"
#include "primec/ui/NativeUiHeadless.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace headless = primec::ui::headless;
using primec::embed::Script;
using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.backend.cpp_host_calls");

namespace {
const std::string SourceDir = PRIMESTRUCT_SOURCE_DIR;
const std::string Primec = PRIMESTRUCT_PRIMEC_PATH;

std::string scratch(const std::string &name) {
  const std::filesystem::path dir = primec::testing::testScratchDir("cpp_host_calls");
  std::filesystem::create_directories(dir);
  return (dir / name).string();
}

std::string readFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void writeFile(const std::string &path, const std::string &text) { std::ofstream(path, std::ios::binary) << text; }

std::string quoted(const std::string &text) { return "'" + text + "'"; }

// Runs a shell command; returns its exit status (or -1).
int run(const std::string &command) {
  const int status = std::system(command.c_str());
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

struct CompileResult {
  int status = 0;
  std::string diagnostics;
};

CompileResult compile(const std::string &kind, const std::string &source, const std::string &output,
                      const std::vector<std::string> &extra = {}) {
  const std::string log = output + ".log";
  std::string command = quoted(Primec) + " --emit=" + kind + " " + quoted(source) + " -o " + quoted(output);
  for (const std::string &arg : extra) {
    command += " " + arg;
  }
  CompileResult result;
  result.status = run(command + " > " + quoted(log) + " 2>&1");
  result.diagnostics = readFile(log);
  return result;
}

std::string hostImpl() { return SourceDir + "/tests/fixtures/host_calls/host_impl.cpp"; }
std::string scalarsProgram() { return SourceDir + "/tests/fixtures/host_calls/scalars.prime"; }

// The host side of scalars.prime for the VM, recording what the C++ side prints.
Script compileScalarsForVm(std::vector<std::string> &lines) {
  ScriptEngine engine;
  engine.bind("host_note", [&lines](std::string_view label, uint64_t id) {
    lines.push_back("note " + std::string(label) + " " + std::to_string(id));
  });
  engine.bind("host_add", [](int32_t left, int32_t right) { return left + right; });
  engine.bind("host_widen", [](int64_t value) { return value * 3; });
  engine.bind("host_flag", [](bool on) { return !on; });
  engine.bind("host_half", [](float value) { return value / 2.0f; });
  engine.bind("host_scale", [](double value, double factor) { return value * factor; });
  Script script = engine.compileFile(scalarsProgram());
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  return script;
}

std::string joined(const std::vector<std::string> &lines) {
  std::string text;
  for (const std::string &line : lines) {
    text += line + "\n";
  }
  return text;
}
} // namespace

TEST_CASE("host calls compiled by the C++ emitters match the VM") {
  std::vector<std::string> vmLines;
  const Script script = compileScalarsForVm(vmLines);
  const auto vm = script.run();
  REQUIRE_MESSAGE(vm.ok, vm.diagnostics);
  REQUIRE(vm.exitCode == 42);
  REQUIRE(vmLines.size() == 5);

  for (const char *kind : {"exe", "optexe"}) {
    const std::string program = scratch(std::string("scalars_") + kind);
    const CompileResult compiled = compile(kind, scalarsProgram(), program, {"--link-arg", quoted(hostImpl())});
    REQUIRE_MESSAGE(compiled.status == 0, kind << ": " << compiled.diagnostics);
    const std::string output = program + ".out";
    const int exitCode = run(quoted(program) + " > " + quoted(output));
    CHECK_MESSAGE(exitCode == 42, kind);
    CHECK_MESSAGE(readFile(output) == joined(vmLines), kind);
  }
}

TEST_CASE("the C++ sources declare host functions with C linkage and call them directly") {
  for (const char *kind : {"cpp", "optcpp"}) {
    const std::string source = scratch(std::string("scalars_") + kind + ".cpp");
    const CompileResult compiled = compile(kind, scalarsProgram(), source);
    REQUIRE_MESSAGE(compiled.status == 0, kind << ": " << compiled.diagnostics);
    const std::string text = readFile(source);
    CHECK_MESSAGE(text.find("extern \"C\" {") != std::string::npos, kind);
    CHECK_MESSAGE(text.find("int32_t host_add(int32_t, int32_t);") != std::string::npos, kind);
    CHECK_MESSAGE(text.find("void host_note(const char *, uint64_t);") != std::string::npos, kind);
    CHECK_MESSAGE(text.find("bool host_flag(bool);") != std::string::npos, kind);
    CHECK_MESSAGE(text.find("host_add(") != std::string::npos, kind);
  }
}

TEST_CASE("a program that is never given its host functions does not link") {
  const std::string program = scratch("scalars_unlinked");
  const CompileResult compiled = compile("exe", scalarsProgram(), program);
  CHECK(compiled.status != 0);
  CHECK(compiled.diagnostics.find("Failed to compile output executable") != std::string::npos);
}

TEST_CASE("host functions that return a string view are refused and point at String") {
  const std::string source = scratch("text_result.prime");
  writeFile(source, R"([host return<string>]
host_name() {
}

[return<int>]
main() {
  [string] name{host_name()}
  return(0i32)
}
)");
  for (const char *kind : {"cpp", "optcpp"}) {
    const CompileResult compiled = compile(kind, source, scratch(std::string("text_result_") + kind));
    CHECK_MESSAGE(compiled.status != 0, kind);
    CHECK_MESSAGE(compiled.diagnostics.find("returns a `string`") != std::string::npos, kind << ": " << compiled.diagnostics);
    CHECK_MESSAGE(compiled.diagnostics.find("return a `String` instead") != std::string::npos, kind);
  }
}

TEST_CASE("only host imports the program reaches are declared") {
  const std::string source = scratch("unused_import.prime");
  writeFile(source, R"([host return<string>]
host_never_called() {
}

[host return<i32>]
host_add([i32] left, [i32] right) {
}

[return<int>]
main() {
  return(host_add(1i32, 2i32))
}
)");
  const std::string output = scratch("unused_import.cpp");
  const CompileResult compiled = compile("cpp", source, output);
  REQUIRE_MESSAGE(compiled.status == 0, compiled.diagnostics);
  const std::string text = readFile(output);
  CHECK(text.find("host_add") != std::string::npos);
  CHECK(text.find("host_never_called") == std::string::npos);
}

TEST_CASE("the native backend still rejects host calls") {
  const CompileResult compiled = compile("native", scalarsProgram(), scratch("scalars_native"));
  CHECK(compiled.status != 0);
  CHECK(compiled.diagnostics.find("host calls are only supported") != std::string::npos);
}
