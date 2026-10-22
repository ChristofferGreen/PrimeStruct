#include "primec/embed/Script.h"
#include "primec/embed/ScriptEngine.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.backend.cpp_strings");

namespace {
const std::string Primec = PRIMESTRUCT_PRIMEC_PATH;

std::string scratch(const std::string &name) {
  const std::filesystem::path dir = primec::testing::testScratchDir("cpp_strings");
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

const std::string Imports = "import /std/text/*\nimport /std/collections/*\n";

std::string programOf(const std::string &body, const std::string &definitions) {
  return Imports + definitions + "\n[return<int> effects(heap_alloc)]\nmain() {\n" + body + "\n}\n";
}

// The exit code of the program on the VM, truncated to what a process exit reports.
int vmExit(const std::string &source) {
  ScriptEngine engine;
  const auto script = engine.compileSource("/cpp_strings.prime", source);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  return result.exitCode & 0xFF;
}

struct Compiled {
  int status = 0;
  std::string diagnostics;
  std::string program;
};

Compiled compileWith(const std::string &kind, const std::string &name, const std::string &source) {
  Compiled compiled;
  const std::string file = scratch(name + ".prime");
  std::ofstream(file, std::ios::binary) << source;
  compiled.program = scratch(name + "_" + kind);
  const std::string log = compiled.program + ".log";
  compiled.status = run(shellQuote(Primec) + " --emit=" + kind + " " + shellQuote(file) + " -o " + shellQuote(compiled.program) +
                        " > " + shellQuote(log) + " 2>&1");
  compiled.diagnostics = readFile(log);
  return compiled;
}

// Runs one program on the VM and through the optimized C++ emitter and checks they agree.
void matches(const std::string &name, const std::string &body, int expected, const std::string &definitions = "") {
  const std::string source = programOf(body, definitions);
  const int vm = vmExit(source);
  CHECK_MESSAGE(vm == (expected & 0xFF), name << ": the VM returned " << vm);
  for (const char *kind : {"optexe"}) {
    const Compiled compiled = compileWith(kind, name, source);
    REQUIRE_MESSAGE(compiled.status == 0, name << " " << kind << ": " << compiled.diagnostics);
    const int exitCode = run(shellQuote(compiled.program) + " > /dev/null 2>&1");
    CHECK_MESSAGE(exitCode == vm, name << " " << kind << " returned " << exitCode << " but the VM returned " << vm);
  }
}

} // namespace

TEST_CASE("u8 arithmetic and conversions agree with the VM") {
  matches("u8_math",
          "  [u8] a{200u8}\n"
          "  [u8] b{100u8}\n"
          "  [u8] sum{a + b}\n"
          "  [i32] widened{convert<i32>(sum)}\n"
          "  [u8] masked{convert<u8>(300i32)}\n"
          "  return(widened + convert<i32>(masked))",
          44 + 44);
}

TEST_CASE("vector<u8> stores packed bytes") {
  matches("vector_u8",
          "  [vector<u8> mut] bytes{vector<u8>{}}\n"
          "  for([i32 mut] i{0i32}, i < 100i32, i++) {\n"
          "    bytes.push(convert<u8>(i * 7i32))\n"
          "  }\n"
          "  [i32 mut] total{0i32}\n"
          "  for([i32 mut] i{0i32}, i < 100i32, i++) {\n"
          "    total = total + convert<i32>(bytes.at(i))\n"
          "  }\n"
          "  return(total - total / 251i32 * 251i32)",
          [] {
            int total = 0;
            for (int i = 0; i < 100; ++i) {
              total += (i * 7) & 0xFF;
            }
            return total % 251;
          }());
}

TEST_CASE("String text, growth, editing and comparison agree with the VM") {
  matches("string_edit",
          "  [String mut] s{/std/text/stringFrom(\"hello, world\")}\n"
          "  s.insert(5i32, \"!!\")\n"
          "  s.erase(0i32, 1i32)\n"
          "  for([i32 mut] i{0i32}, i < 3000i32, i++) {\n"
          "    s.pushByte(convert<u8>(65i32 + (i - i / 26i32 * 26i32)))\n"
          "  }\n"
          "  [bool] ok{s.byteAt(0i32) == 101u8}\n"
          "  return(if(ok, then() { s.count() - s.count() / 200i32 * 200i32 }, else() { 99i32 }))",
          (3000 + 13) % 200);
}

TEST_CASE("the string heap reuses blocks and reports the same figures") {
  matches("string_heap",
          "  [i32 mut] checksum{0i32}\n"
          "  for([i32 mut] i{0i32}, i < 2000i32, i++) {\n"
          "    [String] s{/std/text/stringFrom(\"a name long enough to take a block\")}\n"
          "    checksum = checksum + s.count()\n"
          "  }\n"
          "  [bool] steady{/std/text/stringHeapChunks() <= 2 && /std/text/stringHeapBytesInUse() == 0u64}\n"
          "  return(if(steady, then() { checksum - checksum / 251i32 * 251i32 }, else() { 99i32 }))",
          (2000 * 34) % 251);
}

TEST_CASE("slices over vectors agree with the VM") {
  matches("slices",
          "  [vector<i32> mut] values{vector<i32>{}}\n"
          "  for([i32 mut] i{0i32}, i < 10i32, i++) {\n"
          "    values.push(i * i)\n"
          "  }\n"
          "  [Slice<i32, Read>] all{/std/collections/vector/view<i32>(values)}\n"
          "  [Slice<i32, Read>] view{all.slice(2i32, 6i32)}\n"
          "  [i32 mut] total{0i32}\n"
          "  for([i32 mut] i{0i32}, i < view.count(), i++) {\n"
          "    total = total + view.at(i)\n"
          "  }\n"
          "  return(total)",
          4 + 9 + 16 + 25);
}

TEST_CASE("an out-of-range vector<u8> read stops the program the same way") {
  const std::string source = programOf(
      "  [vector<u8> mut] bytes{vector<u8>{}}\n  bytes.push(1u8)\n  bytes.push(2u8)\n  return(convert<i32>(bytes.at(5i32)))", "");
  ScriptEngine engine;
  const auto script = engine.compileSource("/cpp_strings_fault.prime", source);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto vm = script.run();
  REQUIRE_MESSAGE(vm.ok, vm.diagnostics);
  CHECK((vm.exitCode & 0xFF) != 0);
  const Compiled compiled = compileWith("optexe", "byte_fault", source);
  REQUIRE_MESSAGE(compiled.status == 0, compiled.diagnostics);
  const std::string errors = compiled.program + ".stderr";
  const int exitCode = run(shellQuote(compiled.program) + " > /dev/null 2> " + shellQuote(errors));
  CHECK(exitCode == (vm.exitCode & 0xFF));
  CHECK(readFile(errors).find("array index out of bounds") != std::string::npos);
}

TEST_CASE("the simple C++ emitters name the optimized ones for byte programs") {
  const std::string source = programOf("  [String] s{/std/text/stringFrom(\"abc\")}\n  return(s.count())", "");
  for (const char *kind : {"exe", "cpp"}) {
    const Compiled compiled = compileWith(kind, "u8_simple", source);
    CHECK_FALSE(compiled.status == 0);
    CHECK_MESSAGE(compiled.diagnostics.find("optexe") != std::string::npos, kind << ": " << compiled.diagnostics);
  }
}
