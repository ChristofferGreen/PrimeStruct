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

// Whether the arm64 native backend runs byte memory and the String heap in this build (no host calls yet).
constexpr bool NativeRunsBytes =
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
    true;
#else
    false;
#endif

// Runs one program on the VM and through the optimized C++ emitter (and the arm64 native backend when
// `native` is set and this build has it) and checks they agree.
void matches(const std::string &name,
             const std::string &body,
             int expected,
             const std::string &definitions = "",
             bool native = false) {
  const std::string source = programOf(body, definitions);
  const int vm = vmExit(source);
  CHECK_MESSAGE(vm == (expected & 0xFF), name << ": the VM returned " << vm);
  std::vector<const char *> kinds = {"optexe", "exe"};
  if (native && NativeRunsBytes) {
    kinds.push_back("native");
  }
  for (const char *kind : kinds) {
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
          44 + 44,
          "",
          true);
}

TEST_CASE("vector<u8> stores packed bytes") {
  matches("vector_u8",
          "  [vector<u8> mut] bytes{vector<u8>{}}\n"
          "  for([i32 mut] i{0i32}, i < 2000i32, i++) {\n"
          "    bytes.push(convert<u8>(i * 7i32))\n"
          "  }\n"
          "  [i32 mut] total{0i32}\n"
          "  for([i32 mut] i{0i32}, i < 2000i32, i++) {\n"
          "    total = total + convert<i32>(bytes.at(i))\n"
          "  }\n"
          "  return(total - total / 251i32 * 251i32)",
          [] {
            int total = 0;
            for (int i = 0; i < 2000; ++i) {
              total += (i * 7) & 0xFF;
            }
            return total % 251;
          }(),
          "",
          true);
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
          (3000 + 13) % 200,
          "",
          true);
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

TEST_CASE("host functions that take and return String agree on the VM and optexe") {
  const std::string program = std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/fixtures/host_calls/text_host.prime";
  const std::string impl = std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/fixtures/host_calls/text_impl.cpp";

  std::string notes;
  ScriptEngine engine;
  engine.bind("host_greet", [](primec::embed::HostTextView name) {
    return primec::embed::HostText("hello " + std::string(name.value));
  });
  engine.bind("host_empty", [] { return primec::embed::HostText(); });
  engine.bind("host_measure", [](primec::embed::HostTextView text) { return static_cast<int32_t>(text.value.size()); });
  engine.bind("host_note", [&notes](primec::embed::HostTextView text) { notes += "note " + std::string(text.value) + "\n"; });
  const auto script = engine.compileFile(program);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto vm = script.run();
  REQUIRE_MESSAGE(vm.ok, vm.diagnostics);
  CHECK(vm.exitCode == 1);
  CHECK(notes == "note hello world\nnote hello héllo\nnote \n");

  const std::string exe = scratch("text_host_optexe");
  const std::string log = exe + ".log";
  const int status = run(shellQuote(Primec) + " --emit=optexe " + shellQuote(program) + " -o " + shellQuote(exe) +
                         " --link-arg " + shellQuote(impl) + " > " + shellQuote(log) + " 2>&1");
  REQUIRE_MESSAGE(status == 0, readFile(log));
  const std::string output = exe + ".out";
  const int exitCode = run(shellQuote(exe) + " > " + shellQuote(output));
  CHECK(exitCode == vm.exitCode);
  CHECK(readFile(output) == notes);
}

TEST_CASE("Pointer<u8> host parameters reach optexe hosts as checked byte pointers") {
  const std::string fixtures = std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/fixtures/host_calls/";
  const std::string impl = fixtures + "bytes_impl.cpp";
  auto build = [&](const std::string &name) {
    const std::string exe = scratch(name);
    const std::string log = exe + ".log";
    const int status = run(shellQuote(Primec) + " --emit=optexe " + shellQuote(fixtures + name + ".prime") + " -o " +
                           shellQuote(exe) + " --link-arg " + shellQuote(impl) + " > " + shellQuote(log) + " 2>&1");
    REQUIRE_MESSAGE(status == 0, readFile(log));
    return exe;
  };
  // The host fills bytes 1..4 with 1 2 3 4 and sums all five: 4 * 10 + 1 + 10.
  CHECK(run(shellQuote(build("bytes_host"))) == 51);

  // A length past the buffer's byte region faults before the host runs.
  const std::string overrun = build("bytes_host_overrun");
  const std::string errors = overrun + ".err";
  CHECK(run(shellQuote(overrun) + " 2> " + shellQuote(errors)) == 3);
  CHECK(readFile(errors).find("invalid byte pointer passed to host function") != std::string::npos);

  // The VM does not pass byte buffers to hosts and says so.
  ScriptEngine engine;
  const auto script = engine.compileFile(fixtures + "bytes_host.prime");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto vm = script.run();
  CHECK_FALSE(vm.ok);
  CHECK(vm.diagnostics.find("host_fill takes a Pointer<u8> byte buffer") != std::string::npos);
}

TEST_CASE("a Pointer<u8> host parameter needs its u64 length next") {
  ScriptEngine engine;
  const auto script = engine.compileSource("/bytes_missing_length.prime", R"(
[host return<void>]
host_fill([Pointer<u8>] buffer, [i32] length) {
}

[return<int>]
main() {
  return(0i32)
}
)");
  CHECK_FALSE(script.valid());
  CHECK(script.diagnostics().find("host definition Pointer<u8> parameter must be followed by its u64 length: buffer") !=
        std::string::npos);
}

TEST_CASE("std/os reads the environment, sees the clock advance and stops on SIGTERM") {
  const std::string program = std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/fixtures/host_calls/os_host.prime";
  const std::string shim = std::string(PRIMESTRUCT_SOURCE_DIR) + "/stdlib/std/os/os_posix.cpp";
  const std::string exe = scratch("os_host_optexe");
  const std::string log = exe + ".log";
  const int status = run(shellQuote(Primec) + " --emit=optexe " + shellQuote(program) + " -o " + shellQuote(exe) +
                         " --link-arg " + shellQuote(shim) + " > " + shellQuote(log) + " 2>&1");
  REQUIRE_MESSAGE(status == 0, readFile(log));
  // The child prints "ready" once the clock has advanced 20 ms; the script then sends SIGTERM and records the exit
  // code (100 stopped + 10 port from the environment + 1 fallback for the unset variable).
  const std::string output = exe + ".out";
  const std::string code = exe + ".code";
  const std::string script = "PS_OS_TEST_PORT=8080 " + shellQuote(exe) + " > " + shellQuote(output) +
                             " & pid=$!; i=0; until grep -q ready " + shellQuote(output) +
                             " 2>/dev/null || [ $i -ge 200 ]; do sleep 0.05; i=$((i+1)); done; kill -TERM $pid; "
                             "wait $pid; echo $? > " + shellQuote(code);
  const std::string scriptFile = exe + ".sh";
  {
    std::ofstream scriptOut(scriptFile);
    scriptOut << script << "\n";
  }
  CHECK(run("sh " + shellQuote(scriptFile)) == 0);
  CHECK(readFile(output) == "ready\n");
  CHECK(readFile(code) == "111\n");
}

TEST_CASE("std/os host functions are unbound on the VM") {
  ScriptEngine engine;
  const auto script = engine.compileSource("/os_vm.prime", R"(
import /std/os/*

[return<int>]
main() {
  return(convert<i32>(monotonicMs()))
}
)");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  CHECK_FALSE(result.ok);
  CHECK(result.diagnostics.find("unbound host function: ps_os_monotonic_ms") != std::string::npos);
}

TEST_CASE("libSystem host functions run on the arm64 native backend with the VM's result") {
  const std::string program = std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/fixtures/host_calls/libsystem_host.prime";
  ScriptEngine engine;
  engine.bind("labs", [](int64_t value) { return static_cast<int64_t>(std::llabs(value)); });
  engine.bind("strlen", [](std::string_view text) { return static_cast<uint64_t>(text.size()); });
  engine.bind("toupper", [](int32_t code) { return static_cast<int32_t>(std::toupper(code)); });
  engine.bind("atoi", [](primec::embed::HostTextView text) { return std::atoi(std::string(text.value).c_str()); });
  engine.bind("strerror", [](int32_t code) { return primec::embed::HostText(std::string(std::strerror(code))); });
  engine.bind("ldexp", [](double value, int32_t exponent) { return std::ldexp(value, exponent); });
  engine.bind("sqrtf", [](float value) { return std::sqrt(value); });
  const auto script = engine.compileFile(program);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto vm = script.run();
  REQUIRE_MESSAGE(vm.ok, vm.diagnostics);
  // 40 + 11 + 0 + 123 + 25 + 24 + 7
  CHECK(vm.exitCode == 230);

  const std::string exe = scratch("libsystem_host_native");
  const std::string log = exe + ".log";
  const int status = run(shellQuote(Primec) + " --emit=native " + shellQuote(program) + " -o " + shellQuote(exe) +
                         " > " + shellQuote(log) + " 2>&1");
  if (NativeRunsBytes) {
    REQUIRE_MESSAGE(status == 0, readFile(log));
    CHECK(run(shellQuote(exe) + " > /dev/null 2>&1") == vm.exitCode);
  } else {
    CHECK_FALSE(status == 0);
  }
}

TEST_CASE("entry arguments read as String agree on the VM, optexe and the arm64 native backend") {
  const std::string source = programOf(
      "  [String] first{argumentText(1i32)}\n"
      "  [String] second{argumentText(2i32)}\n"
      "  [String] missing{argumentText(9i32)}\n"
      "  [String] negative{argumentText(0i32 - 1i32)}\n"
      "  return(first.count() * 16i32 + second.count() + convert<i32>(first.byteAt(0i32)) - 104i32 + missing.count() + negative.count())",
      "");
  ScriptEngine engine;
  const auto script = engine.compileSource("/entry_arguments.prime", source);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto vm = script.run({"hello", "x"});
  REQUIRE_MESSAGE(vm.ok, vm.diagnostics);
  CHECK(vm.exitCode == 5 * 16 + 1);
  std::vector<const char *> kinds = {"optexe"};
  if (NativeRunsBytes) {
    kinds.push_back("native");
  }
  for (const char *kind : kinds) {
    const Compiled compiled = compileWith(kind, "entry_arguments", source);
    REQUIRE_MESSAGE(compiled.status == 0, kind << ": " << compiled.diagnostics);
    CHECK_MESSAGE(run(shellQuote(compiled.program) + " hello x > /dev/null 2>&1") == vm.exitCode, kind);
  }
}

TEST_CASE("the simple C++ emitters hand byte programs to the optimized emitter") {
  const std::string source = programOf("  [String] s{/std/text/stringFrom(\"abc\")}\n  return(s.count())", "");
  const Compiled exe = compileWith("exe", "u8_simple", source);
  REQUIRE_MESSAGE(exe.status == 0, exe.diagnostics);
  CHECK(run(shellQuote(exe.program) + " > /dev/null 2>&1") == 3);
  // --emit=cpp writes C++ with the optimized emitter's text runtime for a program that runs bytes or text.
  const Compiled simple = compileWith("cpp", "u8_simple", source);
  REQUIRE_MESSAGE(simple.status == 0, simple.diagnostics);
  CHECK(readFile(simple.program).find("ps_text_") != std::string::npos);
  const Compiled plain = compileWith("cpp", "u8_plain", programOf("  return(7i32)", ""));
  REQUIRE_MESSAGE(plain.status == 0, plain.diagnostics);
  CHECK(readFile(plain.program).find("ps_text_") == std::string::npos);
}
