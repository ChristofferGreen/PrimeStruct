#include "embed_test_support.h"
#include "primec/embed/ScriptEngine.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using primec::embed::Script;
using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.embed.host_language");

namespace {
const char *AddSource =
    "[host return<int>]\nhost_add([i32] a, [i32] b) {\n}\n\n[return<int>]\nmain() {\n  return(host_add(40i32, 2i32))\n}\n";

const char *AllTypesSource = R"(
[host return<void>]
host_note([i32] value) {
}

[host return<f64>]
host_scale([f64] value, [f32] factor) {
}

[host return<bool>]
host_is_even([i64] value) {
}

[host return<u64>]
host_big([u64] value) {
}

[return<int>]
main() {
  host_note(7i32)
  [mut] i{0i32}
  while(i < 3i32) {
    host_note(i)
    i = i + 1i32
  }
  [f64] scaled{host_scale(2.0f64, 1.5f32)}
  [bool] even{host_is_even(10i64)}
  [u64] big{host_big(5u64)}
  if(even) {
    return(convert<i32>(scaled) + convert<i32>(big))
  } else {
    return(0i32)
  }
}
)";

std::string compileError(const std::string &source) {
  ScriptEngine engine;
  const auto script = engine.compileSource("/host_bad.prime", source);
  REQUIRE_FALSE(script.valid());
  return script.diagnostics();
}
} // namespace

TEST_CASE("host declaration compiles and a bound C++ function answers it") {
  ScriptEngine engine;
  auto script = engine.compileSource("/host_add.prime", AddSource);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  CHECK(script.requiredHostFunctions() == std::vector<std::string>{"host_add(i32, i32) -> i32"});
  script.bind("host_add", [](int32_t a, int32_t b) { return a + b; });
  const auto result = script.run();
  CHECK(result.ok);
  CHECK(result.exitCode == 42);
}

TEST_CASE("host function can be bound on the engine before compiling") {
  ScriptEngine engine;
  engine.bind("host_add", [](int32_t a, int32_t b) { return a * b; });
  const auto script = engine.compileSource("/host_add_engine.prime", AddSource);
  REQUIRE(script.valid());
  CHECK(script.run().exitCode == 80);
}

TEST_CASE("unbound host function is reported before the script runs") {
  ScriptEngine engine;
  const auto script = engine.compileSource("/host_unbound.prime", AddSource);
  REQUIRE(script.valid());
  std::string error;
  CHECK_FALSE(script.checkHostBindings(error));
  const auto result = script.run();
  CHECK_FALSE(result.ok);
  CHECK(result.diagnostics.find("unbound host function: host_add (i32, i32) -> i32") != std::string::npos);
}

TEST_CASE("host signature mismatch names both sides") {
  ScriptEngine engine;
  auto script = engine.compileSource("/host_mismatch.prime", AddSource);
  REQUIRE(script.valid());
  script.bind("host_add", [](int64_t a, int64_t b) { return a + b; });
  const auto result = script.run();
  CHECK_FALSE(result.ok);
  CHECK(result.diagnostics.find("script declares (i32, i32) -> i32") != std::string::npos);
  CHECK(result.diagnostics.find("host bound (i64, i64) -> i64") != std::string::npos);
}

TEST_CASE("host calls cover void callbacks, loops and every primitive type") {
  ScriptEngine engine;
  auto script = engine.compileSource("/host_types.prime", AllTypesSource);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  std::vector<int32_t> notes;
  script.bind("host_note", [&notes](int32_t v) { notes.push_back(v); });
  script.bind("host_scale", [](double value, float factor) { return value * factor; });
  script.bind("host_is_even", [](int64_t value) { return value % 2 == 0; });
  script.bind("host_big", [](uint64_t value) { return value + 1; });
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK(notes == std::vector<int32_t>{7, 0, 1, 2});
  CHECK(result.exitCode == 3 + 6);  // scaled = 3.0, big = 6
}

TEST_CASE("host calls survive a bytecode round trip into the runtime library") {
  ScriptEngine engine;
  const auto script = engine.compileSource("/host_bytecode.prime", AddSource);
  REQUIRE(script.valid());
  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(script.saveBytecode(bytes, error));
  auto loaded = Script::loadBytecode(bytes);
  REQUIRE_MESSAGE(loaded.valid(), loaded.diagnostics());
  CHECK(loaded.requiredHostFunctions() == std::vector<std::string>{"host_add(i32, i32) -> i32"});
  CHECK_FALSE(loaded.run().ok);
  loaded.bind("host_add", [](int32_t a, int32_t b) { return a + b + 1; });
  CHECK(loaded.run().exitCode == 43);
}

TEST_CASE("host function result feeds ordinary script expressions") {
  ScriptEngine engine;
  auto script = engine.compileSource("/host_expr.prime", R"(
[host return<int>]
host_double([i32] value) {
}

[return<int>]
main() {
  return(host_double(host_double(3i32)) + 1i32)
}
)");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  int calls = 0;
  script.bind("host_double", [&calls](int32_t v) {
    ++calls;
    return v * 2;
  });
  CHECK(script.run().exitCode == 13);
  CHECK(calls == 2);
}

TEST_CASE("host call with unused result is discarded cleanly") {
  ScriptEngine engine;
  auto script = engine.compileSource("/host_discard.prime", R"(
[host return<int>]
host_tick([i32] value) {
}

[return<int>]
main() {
  host_tick(1i32)
  host_tick(2i32)
  return(9i32)
}
)");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  int sum = 0;
  script.bind("host_tick", [&sum](int32_t v) {
    sum += v;
    return v;
  });
  CHECK(script.run().exitCode == 9);
  CHECK(sum == 3);
}

TEST_CASE("host declaration rejects a body") {
  const std::string error = compileError("[host return<int>]\nhost_f([i32] a) {\n  return(a)\n}\n\n[return<int>]\nmain() {\n  return(1i32)\n}\n");
  CHECK(error.find("host definition must have an empty body") != std::string::npos);
}

TEST_CASE("host declaration rejects non primitive parameters") {
  for (const char *type : {"array<i32>", "vector<i32>"}) {
    CAPTURE(type);
    const std::string error = compileError(std::string("[host return<int>]\nhost_f([") + type +
                                           "] a) {\n}\n\n[return<int>]\nmain() {\n  return(1i32)\n}\n");
    CHECK(error.find("host definition parameter must be") != std::string::npos);
  }
}

TEST_CASE("host declaration rejects mut and default parameters") {
  const std::string mutError =
      compileError("[host return<int>]\nhost_f([i32 mut] a) {\n}\n\n[return<int>]\nmain() {\n  return(1i32)\n}\n");
  CHECK(mutError.find("host definition parameter must be") != std::string::npos);
  const std::string defaultError =
      compileError("[host return<int>]\nhost_f([i32] a{5i32}) {\n}\n\n[return<int>]\nmain() {\n  return(1i32)\n}\n");
  CHECK(defaultError.find("host definition parameter cannot have a default") != std::string::npos);
}

TEST_CASE("host declaration rejects non primitive return types and generics") {
  const std::string stringError =
      compileError("[host return<array<i32>>]\nhost_f([i32] a) {\n}\n\n[return<int>]\nmain() {\n  return(1i32)\n}\n");
  CHECK(stringError.find("host definition return type must be") != std::string::npos);
  const std::string genericError =
      compileError("[host return<int>]\nhost_f<T>([i32] a) {\n}\n\n[return<int>]\nmain() {\n  return(1i32)\n}\n");
  CHECK(genericError.find("host definition cannot be generic") != std::string::npos);
}

TEST_CASE("host declaration requires an explicit return type") {
  const std::string error = compileError("[host]\nhost_f([i32] a) {\n}\n\n[return<int>]\nmain() {\n  return(1i32)\n}\n");
  CHECK_FALSE(error.empty());
}

TEST_CASE("host declaration rejects wrong call arity and argument types") {
  const std::string arity = compileError("[host return<int>]\nhost_f([i32] a) {\n}\n\n[return<int>]\nmain() {\n  return(host_f(1i32, 2i32))\n}\n");
  CHECK_FALSE(arity.empty());
  const std::string type = compileError("[host return<int>]\nhost_f([i32] a) {\n}\n\n[return<int>]\nmain() {\n  return(host_f(\"x\"))\n}\n");
  CHECK_FALSE(type.empty());
}

TEST_CASE("host declaration on a struct is rejected") {
  const std::string error = compileError("[struct host]\nBox {\n  [i32] value{1i32}\n}\n\n[return<int>]\nmain() {\n  return(1i32)\n}\n");
  CHECK_FALSE(error.empty());
}

TEST_CASE("script without host declarations needs no bindings") {
  ScriptEngine engine;
  const auto script = engine.compileSource("/host_none.prime", "[return<int>]\nmain() {\n  return(4i32)\n}\n");
  REQUIRE(script.valid());
  CHECK(script.requiredHostFunctions().empty());
  std::string error;
  CHECK(script.checkHostBindings(error));
  CHECK(script.run().exitCode == 4);
}

TEST_CASE("declared but never called host function adds no requirement") {
  ScriptEngine engine;
  const auto script = engine.compileSource(
      "/host_uncalled.prime",
      "[host return<int>]\nhost_unused([i32] a) {\n}\n\n[return<int>]\nmain() {\n  return(5i32)\n}\n");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  CHECK(script.requiredHostFunctions().empty());
  CHECK(script.run().exitCode == 5);
}

namespace {
const char *StringSource = R"(
[host return<void>]
host_say([string] text) {
}

[host return<int>]
host_len([string] text) {
}

[return<int>]
main() {
  [string] greeting{"hello"}
  host_say("literal")
  host_say(greeting)
  host_say("")
  host_say("with spaces and é")
  return(host_len("four"))
}
)";
} // namespace

TEST_CASE("script passes literal and local strings to a bound host function") {
  ScriptEngine engine;
  auto script = engine.compileSource("/host_strings.prime", StringSource);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  CHECK(script.requiredHostFunctions() ==
        std::vector<std::string>{"host_say(string) -> void", "host_len(string) -> i32"});
  std::vector<std::string> said;
  script.bind("host_say", [&said](std::string_view text) { said.emplace_back(text); });
  script.bind("host_len", [](const std::string &text) { return static_cast<int32_t>(text.size()); });
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.exitCode == 4);
  REQUIRE(said.size() == 4);
  CHECK(said[0] == "literal");
  CHECK(said[1] == "hello");
  CHECK(said[2].empty());
  CHECK(said[3].rfind("with spaces and ", 0) == 0);
}

TEST_CASE("host string parameter accepts std::string by value too") {
  ScriptEngine engine;
  auto script = engine.compileSource("/host_strings_value.prime", StringSource);
  REQUIRE(script.valid());
  std::string last;
  script.bind("host_say", [&last](std::string text) { last = text; });
  script.bind("host_len", [](std::string_view text) { return static_cast<int32_t>(text.size()); });
  CHECK(script.run().exitCode == 4);
  CHECK(last.rfind("with spaces", 0) == 0);
}

TEST_CASE("host string versus integer signature mismatch is diagnosed") {
  ScriptEngine engine;
  auto script = engine.compileSource("/host_strings_mismatch.prime", StringSource);
  REQUIRE(script.valid());
  script.bind("host_say", [](int32_t) {});
  script.bind("host_len", [](std::string_view text) { return static_cast<int32_t>(text.size()); });
  const auto result = script.run();
  CHECK_FALSE(result.ok);
  CHECK(result.diagnostics.find("script declares (string) -> void") != std::string::npos);
  CHECK(result.diagnostics.find("host bound (i32) -> void") != std::string::npos);
}

#ifdef PRIMESTRUCT_TEST_PRIMEC_PATH
TEST_CASE("non vm backends reject host definitions with a clear diagnostic") {
  const auto dir = embedTestDir("host_backends");
  const auto source = embedWriteFile(dir / "host.prime", AddSource);
  for (const char *kind : {"native", "cpp", "wasm", "glsl"}) {
    CAPTURE(kind);
    const auto result = embedRunPrimec(std::string("--emit=") + kind + " \"" + source.string() + "\" -o \"" +
                                       (dir / (std::string("out_") + kind)).string() + "\"");
    CHECK(result.exitCode != 0);
    CHECK(result.output.find("host calls are only supported by the vm target") != std::string::npos);
  }
}

TEST_CASE("primec emit ir produces bytecode an embedder can bind and run") {
  const auto dir = embedTestDir("host_offline");
  const auto source = embedWriteFile(dir / "host.prime", AddSource);
  const auto psir = dir / "host.psir";
  const auto result = embedRunPrimec("--emit=ir \"" + source.string() + "\" -o \"" + psir.string() + "\"");
  REQUIRE_MESSAGE(result.exitCode == 0, result.output);
  std::ifstream file(psir, std::ios::binary);
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  REQUIRE_FALSE(bytes.empty());
  auto script = Script::loadBytecode(bytes);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  CHECK(script.requiredHostFunctions() == std::vector<std::string>{"host_add(i32, i32) -> i32"});
  CHECK_FALSE(script.run().ok);
  script.bind("host_add", [](int32_t a, int32_t b) { return a + b; });
  CHECK(script.run().exitCode == 42);
}

TEST_CASE("primevm reports unbound host functions instead of crashing") {
  const auto dir = embedTestDir("host_vm_cli");
  const auto source = embedWriteFile(dir / "host.prime", AddSource);
  // primevm lives next to primec in the build tree.
  const std::string primevm =
      (std::filesystem::path(PRIMESTRUCT_TEST_PRIMEC_PATH).parent_path() / "primevm").string();
  const auto out = dir / "out.txt";
  const int status = std::system(("\"" + primevm + "\" \"" + source.string() + "\" > \"" + out.string() + "\" 2>&1").c_str());
  CHECK(WIFEXITED(status));
  std::ifstream in(out);
  const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  CHECK(text.find("unbound host function: host_add") != std::string::npos);
}
#endif

TEST_SUITE_END();
