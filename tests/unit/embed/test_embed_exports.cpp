#include "embed_test_support.h"
#include "primec/embed/ScriptEngine.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <algorithm>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using primec::embed::HostType;
using primec::embed::Script;
using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.embed.exports");

namespace {
const char *LibrarySource = R"(
[return<f64>]
scale([i32] count, [f64] factor) {
  return(convert<f64>(count) * factor)
}

[return<int>]
add([i32] a, [i32] b) {
  return(a + b)
}

[return<i64>]
widen([i64] value) {
  return(value * 2i64)
}

[return<bool>]
is_positive([i32] value) {
  return(value > 0i32)
}

[return<f32>]
halve([f32] value) {
  return(value / 2.0f32)
}

[return<u64>]
next_big([u64] value) {
  return(value + 1u64)
}

[return<void>]
noop([i32] value) {
}

[return<int>]
main() {
  return(7i32)
}
)";

ScriptEngine makeEngine() {
  ScriptEngine engine;
  engine.exportFunction<double(int32_t, double)>("scale");
  engine.exportFunction<int32_t(int32_t, int32_t)>("add");
  engine.exportFunction<int64_t(int64_t)>("widen");
  engine.exportFunction<bool(int32_t)>("is_positive");
  engine.exportFunction<float(float)>("halve");
  engine.exportFunction<uint64_t(uint64_t)>("next_big");
  engine.exportFunction<void(int32_t)>("noop");
  return engine;
}

Script compileLibrary() {
  const auto script = makeEngine().compileSource("/exports_library.prime", LibrarySource);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  return script;
}
} // namespace

TEST_CASE("exports call functions with every primitive type") {
  const auto script = compileLibrary();
  const auto scaled = script.call<double>("scale", 3, 2.5);
  REQUIRE_MESSAGE(scaled.ok, scaled.diagnostics);
  CHECK(scaled.value == 7.5);
  const auto sum = script.call<int32_t>("add", 40, 2);
  REQUIRE_MESSAGE(sum.ok, sum.diagnostics);
  CHECK(sum.value == 42);
  const auto wide = script.call<int64_t>("widen", int64_t{3000000000});
  REQUIRE_MESSAGE(wide.ok, wide.diagnostics);
  CHECK(wide.value == 6000000000);
  const auto positive = script.call<bool>("is_positive", -4);
  REQUIRE_MESSAGE(positive.ok, positive.diagnostics);
  CHECK_FALSE(positive.value);
  CHECK(script.call<bool>("is_positive", 9).value);
  const auto half = script.call<float>("halve", 5.0f);
  REQUIRE_MESSAGE(half.ok, half.diagnostics);
  CHECK(half.value == 2.5f);
  const auto big = script.call<uint64_t>("next_big", uint64_t{0xFFFFFFFFFFFFFFF0ull});
  REQUIRE_MESSAGE(big.ok, big.diagnostics);
  CHECK(big.value == 0xFFFFFFFFFFFFFFF1ull);
}

TEST_CASE("exports support void results and keep main callable") {
  const auto script = compileLibrary();
  const auto result = script.call<void>("noop", 5);
  CHECK_MESSAGE(result.ok, result.diagnostics);
  CHECK(script.run().exitCode == 7);
}

TEST_CASE("exports return negative and boundary integers intact") {
  const auto script = compileLibrary();
  CHECK(script.call<int32_t>("add", -50, 8).value == -42);
  CHECK(script.call<int32_t>("add", 2147483647, 0).value == 2147483647);
  CHECK(script.call<int32_t>("add", -2147483647 - 1, 0).value == -2147483647 - 1);
  CHECK(script.call<double>("scale", 0, 1e300).value == 0.0);
}

TEST_CASE("exports can be called repeatedly with changing arguments") {
  const auto script = compileLibrary();
  for (int i = 0; i < 200; ++i) {
    const auto result = script.call<int32_t>("add", i, i * 2);
    REQUIRE(result.ok);
    CHECK(result.value == i * 3);
  }
}

TEST_CASE("export errors are values: unknown name, arity and type mismatch") {
  const auto script = compileLibrary();
  const auto unknown = script.call<int32_t>("nope", 1);
  CHECK_FALSE(unknown.ok);
  CHECK(unknown.diagnostics.find("unknown exported function: nope") != std::string::npos);
  const auto arity = script.call<int32_t>("add", 1);
  CHECK_FALSE(arity.ok);
  CHECK(arity.diagnostics.find("exported function add(i32, i32) -> i32 called as (i32) -> i32") != std::string::npos);
  const auto argType = script.call<int32_t>("add", 1.5, 2);
  CHECK_FALSE(argType.ok);
  CHECK(argType.diagnostics.find("called as (f64, i32) -> i32") != std::string::npos);
  const auto resultType = script.call<double>("add", 1, 2);
  CHECK_FALSE(resultType.ok);
  CHECK(resultType.diagnostics.find("-> f64") != std::string::npos);
}

TEST_CASE("exports list their signatures") {
  const auto script = compileLibrary();
  const auto exported = script.exportedFunctions();
  REQUIRE(exported.size() == 7);
  CHECK(exported[0] == "scale(i32, f64) -> f64");
  CHECK(exported[1] == "add(i32, i32) -> i32");
  CHECK(exported[6] == "noop(i32) -> void");
}

TEST_CASE("compiling an export that does not match the script fails with a clear message") {
  ScriptEngine engine;
  engine.exportFunction<int32_t(int32_t)>("add");  // script's add takes two arguments
  const auto script = engine.compileSource("/exports_mismatch.prime", LibrarySource);
  CHECK_FALSE(script.valid());
  CHECK(script.diagnostics().find("export 'add':") != std::string::npos);
  ScriptEngine missing;
  missing.exportFunction<int32_t(int32_t)>("does_not_exist");
  const auto missingScript = missing.compileSource("/exports_missing.prime", LibrarySource);
  CHECK_FALSE(missingScript.valid());
  CHECK(missingScript.diagnostics().find("export 'does_not_exist':") != std::string::npos);
}

TEST_CASE("a script without main still works when it only exports functions") {
  ScriptEngine engine;
  engine.exportFunction<int32_t(int32_t, int32_t)>("add");
  const auto script = engine.compileSource(
      "/exports_library_only.prime", "[return<int>]\nadd([i32] a, [i32] b) {\n  return(a + b)\n}\n");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  CHECK(script.call<int32_t>("add", 1, 2).value == 3);
  const auto run = script.run();
  CHECK_FALSE(run.ok);
  CHECK(run.diagnostics.find("no main entry") != std::string::npos);
}

TEST_CASE("exports can call host functions and see script state per call") {
  ScriptEngine engine;
  engine.exportFunction<int32_t(int32_t)>("apply");
  const char *source = R"(
[host return<int>]
host_double([i32] value) {
}

[return<int>]
apply([i32] value) {
  return(host_double(value) + 1i32)
}

[return<int>]
main() {
  return(0i32)
}
)";
  auto script = engine.compileSource("/exports_host.prime", source);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto unbound = script.call<int32_t>("apply", 5);
  CHECK_FALSE(unbound.ok);
  CHECK(unbound.diagnostics.find("unbound host function: host_double") != std::string::npos);
  script.bind("host_double", [](int32_t v) { return v * 2; });
  CHECK(script.call<int32_t>("apply", 5).value == 11);
  CHECK(script.requiredHostFunctions() == std::vector<std::string>{"host_double(i32) -> i32"});
  std::string error;
  CHECK(script.checkHostBindings(error));
}

TEST_CASE("exports compile from a file and resolve relative imports") {
  const auto dir = embedTestDir("exports_file");
  embedWriteFile(dir / "helper.prime", "[public i32]\nhelp([i32] value) {\n  return(value + 100i32)\n}\n");
  const auto path = embedWriteFile(dir / "lib.prime",
                                   "import<\"helper.prime\">\n\n[return<int>]\nboosted([i32] v) {\n  return(help(v))\n}\n\n"
                                   "[return<int>]\nmain() {\n  return(0i32)\n}\n");
  ScriptEngine engine;
  engine.exportFunction<int32_t(int32_t)>("boosted");
  const auto script = engine.compileFile(path.string());
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  CHECK(script.call<int32_t>("boosted", 5).value == 105);
}

TEST_CASE("exports survive a bytecode bundle round trip") {
  const auto script = compileLibrary();
  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE_MESSAGE(script.saveBytecode(bytes, error), error);
  CHECK(bytes[0] == 'P');
  CHECK(bytes[1] == 'S');
  CHECK(bytes[2] == 'B');
  CHECK(bytes[3] == 'N');
  auto loaded = Script::loadBytecode(bytes);
  REQUIRE_MESSAGE(loaded.valid(), loaded.diagnostics());
  CHECK(loaded.exportedFunctions() == script.exportedFunctions());
  CHECK(loaded.call<int32_t>("add", 20, 22).value == 42);
  CHECK(loaded.call<double>("scale", 4, 0.5).value == 2.0);
  CHECK(loaded.run().exitCode == 7);
  std::vector<uint8_t> again;
  REQUIRE(loaded.saveBytecode(again, error));
  CHECK(again == bytes);
}

TEST_CASE("exports compare bundle loading to hostile bytes") {
  const auto script = compileLibrary();
  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(script.saveBytecode(bytes, error));
  for (size_t length = 0; length < bytes.size(); length += 7) {
    const std::vector<uint8_t> prefix(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(length));
    CHECK_FALSE(Script::loadBytecode(prefix).valid());
  }
  auto trailing = bytes;
  trailing.push_back(0x42);
  CHECK_FALSE(Script::loadBytecode(trailing).valid());
  auto badVersion = bytes;
  badVersion[4] = 0x7f;
  CHECK_FALSE(Script::loadBytecode(badVersion).valid());
  for (size_t at = 4; at + 4 <= std::min<size_t>(bytes.size(), 200); ++at) {
    auto hostile = bytes;
    for (size_t i = 0; i < 4; ++i) {
      hostile[at + i] = 0xff;
    }
    const auto loaded = Script::loadBytecode(hostile);
    if (!loaded.valid()) {
      CHECK_FALSE(loaded.diagnostics().empty());
    }
  }
}

TEST_CASE("exports re-exporting replaces the earlier declaration") {
  ScriptEngine engine;
  engine.exportFunction<int32_t(int32_t)>("add");
  engine.exportFunction<int32_t(int32_t, int32_t)>("add");  // replaces
  const auto script = engine.compileSource(
      "/exports_replace.prime", "[return<int>]\nadd([i32] a, [i32] b) {\n  return(a + b)\n}\n\n[return<int>]\nmain() {\n  return(0i32)\n}\n");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  CHECK(script.exportedFunctions() == std::vector<std::string>{"add(i32, i32) -> i32"});
  CHECK(script.call<int32_t>("add", 4, 5).value == 9);
}

TEST_CASE("exports work from several threads on one script") {
  const auto script = compileLibrary();
  std::vector<int> results(8, -1);
  std::vector<std::thread> threads;
  for (size_t t = 0; t < results.size(); ++t) {
    threads.emplace_back([&, t] {
      int value = 0;
      for (int i = 0; i < 50; ++i) {
        value += script.call<int32_t>("add", static_cast<int>(t), i).value;
      }
      results[t] = value;
    });
  }
  for (auto &thread : threads) {
    thread.join();
  }
  for (size_t t = 0; t < results.size(); ++t) {
    CHECK(results[t] == static_cast<int>(t) * 50 + 49 * 50 / 2);
  }
}

namespace {
const char *StringLibrary = R"(
[return<int>]
count_chars([string] text) {
  return(text.count())
}

[host return<void>]
host_say([string] text) {
}

[return<void>]
speak([string] text, [i32] times) {
  [mut] i{0i32}
  while(i < times) {
    host_say(text)
    i = i + 1i32
  }
}

[return<int>]
mixed([i32] a, [string] left, [f64] scale, [string] right) {
  return(a + left.count() * 10i32 + right.count() * 100i32 + convert<i32>(scale))
}

[return<int>]
main() {
  return(0i32)
}
)";

ScriptEngine makeStringEngine() {
  ScriptEngine engine;
  engine.exportFunction<int32_t(std::string_view)>("count_chars");
  engine.exportFunction<void(std::string_view, int32_t)>("speak");
  engine.exportFunction<int32_t(int32_t, std::string_view, double, std::string_view)>("mixed");
  return engine;
}

Script compileStringLibrary() {
  const auto script = makeStringEngine().compileSource("/exports_strings.prime", StringLibrary);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  return script;
}
} // namespace

TEST_CASE("exports accept string arguments of every C++ string type") {
  const auto script = compileStringLibrary();
  CHECK(script.call<int32_t>("count_chars", "hello").value == 5);
  CHECK(script.call<int32_t>("count_chars", std::string("four")).value == 4);
  CHECK(script.call<int32_t>("count_chars", std::string_view("sixsix")).value == 6);
  const std::string owned = "heap string";
  CHECK(script.call<int32_t>("count_chars", owned).value == 11);
}

TEST_CASE("an exported string argument can be forwarded to a host function") {
  auto script = compileStringLibrary();
  std::vector<std::string> said;
  script.bind("host_say", [&said](std::string_view text) { said.emplace_back(text); });
  const auto result = script.call<void>("speak", "hi there", 3);
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK(said == std::vector<std::string>{"hi there", "hi there", "hi there"});
  said.clear();
  CHECK(script.call<void>("speak", std::string("again"), 1).ok);
  CHECK(said == std::vector<std::string>{"again"});
  const auto unbound = compileStringLibrary().call<void>("speak", "x", 1);
  CHECK_FALSE(unbound.ok);
  CHECK(unbound.diagnostics.find("unbound host function: host_say") != std::string::npos);
}

TEST_CASE("exports handle empty, binary, unicode and very long strings") {
  const auto script = compileStringLibrary();
  CHECK(script.call<int32_t>("count_chars", "").value == 0);
  const std::string withNul("a\0b\0", 4);
  CHECK(script.call<int32_t>("count_chars", withNul).value == 4);
  CHECK(script.call<int32_t>("count_chars", "héllo").value == 6);  // UTF-8 bytes
  const std::string big(200000, 'x');
  const auto result = script.call<int32_t>("count_chars", big);
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.value == 200000);
}

TEST_CASE("exports mix string and numeric arguments") {
  const auto script = compileStringLibrary();
  const auto result = script.call<int32_t>("mixed", 1, "ab", 3.9, "xyz");
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.value == 1 + 2 * 10 + 3 * 100 + 3);
}

TEST_CASE("exports with strings are repeatable and do not retain earlier arguments") {
  const auto script = compileStringLibrary();
  for (int i = 0; i < 100; ++i) {
    const std::string text(static_cast<size_t>(i), 'q');
    const auto result = script.call<int32_t>("count_chars", text);
    REQUIRE(result.ok);
    CHECK(result.value == i);
  }
}

TEST_CASE("exports reject a string where a number is declared and the reverse") {
  const auto script = compileStringLibrary();
  const auto wrong = script.call<int32_t>("count_chars", 5);
  CHECK_FALSE(wrong.ok);
  CHECK(wrong.diagnostics.find("count_chars(string) -> i32 called as (i32) -> i32") != std::string::npos);
  const auto add = compileLibrary().call<int32_t>("add", "1", "2");
  CHECK_FALSE(add.ok);
  CHECK(add.diagnostics.find("called as (string, string) -> i32") != std::string::npos);
}

TEST_CASE("string exports are listed with their signature") {
  const auto script = compileStringLibrary();
  CHECK(script.exportedFunctions()[0] == "count_chars(string) -> i32");
  CHECK(script.exportedFunctions()[2] == "mixed(i32, string, f64, string) -> i32");
  CHECK(script.exportedFunctions()[1] == "speak(string, i32) -> void");
}

TEST_CASE("string exports survive a bundle round trip and run on several threads") {
  const auto script = compileStringLibrary();
  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE_MESSAGE(script.saveBytecode(bytes, error), error);
  const auto loaded = Script::loadBytecode(bytes);
  REQUIRE_MESSAGE(loaded.valid(), loaded.diagnostics());
  std::vector<int> results(6, -1);
  std::vector<std::thread> threads;
  for (size_t t = 0; t < results.size(); ++t) {
    threads.emplace_back([&, t] {
      int total = 0;
      for (int i = 0; i < 40; ++i) {
        total += loaded.call<int32_t>("count_chars", std::string(t + static_cast<size_t>(i), 'z')).value;
      }
      results[t] = total;
    });
  }
  for (auto &thread : threads) {
    thread.join();
  }
  for (size_t t = 0; t < results.size(); ++t) {
    CHECK(results[t] == static_cast<int>(t) * 40 + 39 * 40 / 2);
  }
}

TEST_CASE("the original module is untouched by string calls") {
  const auto script = compileStringLibrary();
  std::vector<uint8_t> before;
  std::vector<uint8_t> after;
  std::string error;
  REQUIRE(script.saveBytecode(before, error));
  CHECK(script.call<int32_t>("count_chars", "some text").ok);
  REQUIRE(script.saveBytecode(after, error));
  CHECK(before == after);
}

TEST_SUITE_END();
