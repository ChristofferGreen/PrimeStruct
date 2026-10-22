#include "primec/embed/Script.h"
#include "primec/embed/ScriptEngine.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <string>

using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.language.u8_scalar");

namespace {

// Runs `body` as the body of `main` and returns its exit code, or -1 when the
// program does not compile (the diagnostics land in `diagnosticsOut`).
int runMain(const std::string &body, std::string *diagnosticsOut = nullptr, const std::string &prelude = "") {
  ScriptEngine engine;
  const std::string source = prelude + "\n[return<int>]\nmain() {\n" + body + "\n}\n";
  const auto script = engine.compileSource("/u8_scalar.prime", source);
  if (!script.valid()) {
    if (diagnosticsOut != nullptr) {
      *diagnosticsOut = script.diagnostics();
    }
    return -1;
  }
  const auto result = script.run();
  REQUIRE(result.ok);
  return result.exitCode;
}

std::string diagnosticsFor(const std::string &body, const std::string &prelude = "") {
  std::string diagnostics;
  CHECK(runMain(body, &diagnostics, prelude) == -1);
  return diagnostics;
}

} // namespace

TEST_CASE("u8 literals hold 0 through 255") {
  CHECK(runMain("  return(convert<i32>(0u8))") == 0);
  CHECK(runMain("  return(convert<i32>(255u8))") == 255);
  CHECK(runMain("  return(convert<i32>(0xFFu8))") == 255);
}

TEST_CASE("u8 literal beyond 255 is rejected") {
  CHECK(diagnosticsFor("  [u8] a{256u8}\n  return(0i32)").find("u8 literal out of range") != std::string::npos);
}

TEST_CASE("u8 addition wraps modulo 256") {
  CHECK(runMain("  [u8] a{200u8}\n  [u8] b{100u8}\n  return(convert<i32>(a + b))") == 44);
}

TEST_CASE("u8 subtraction wraps modulo 256") {
  CHECK(runMain("  [u8] a{200u8}\n  [u8] b{100u8}\n  return(convert<i32>(b - a))") == 156);
}

TEST_CASE("u8 multiplication wraps modulo 256") {
  CHECK(runMain("  [u8] a{200u8}\n  [u8] b{100u8}\n  return(convert<i32>(a * b))") == 32);
}

TEST_CASE("u8 division is unsigned") {
  CHECK(runMain("  [u8] a{200u8}\n  return(convert<i32>(a / 7u8))") == 28);
}

TEST_CASE("u8 comparisons are unsigned") {
  CHECK(runMain("  [u8] a{200u8}\n  [u8] b{100u8}\n  return(if(a > b, then() { 1i32 }, else() { 0i32 }))") == 1);
  CHECK(runMain("  [u8] a{200u8}\n  return(if(a == 200u8, then() { 1i32 }, else() { 0i32 }))") == 1);
  CHECK(runMain("  [u8] a{255u8}\n  return(if(a < 255u8, then() { 1i32 }, else() { 0i32 }))") == 0);
}

TEST_CASE("convert to u8 keeps the low eight bits") {
  CHECK(runMain("  return(convert<i32>(convert<u8>(300i32)))") == 44);
  CHECK(runMain("  return(convert<i32>(convert<u8>(-1i32)))") == 255);
  CHECK(runMain("  return(convert<i32>(convert<u8>(1000u64)))") == 232);
  CHECK(runMain("  return(convert<i32>(convert<u8>(3.9f32)))") == 3);
}

TEST_CASE("convert from u8 widens without sign extension") {
  CHECK(runMain("  [u8] a{200u8}\n  return(convert<i32>(a) + 1i32)") == 201);
  CHECK(runMain("  [u8] a{200u8}\n  return(convert<i32>(convert<u64>(a)))") == 200);
  CHECK(runMain("  [u8] a{200u8}\n  return(convert<i32>(convert<i64>(a)))") == 200);
  CHECK(runMain("  [u8] a{100u8}\n  return(convert<i32>(convert<f32>(a)))") == 100);
  CHECK(runMain("  [u8] a{100u8}\n  return(convert<i32>(convert<f64>(a)))") == 100);
}

TEST_CASE("u8 increment and decrement wrap") {
  CHECK(runMain("  [u8 mut] c{254u8}\n  increment(c)\n  increment(c)\n  increment(c)\n  return(convert<i32>(c))") == 1);
  CHECK(runMain("  [u8 mut] c{1u8}\n  decrement(c)\n  decrement(c)\n  return(convert<i32>(c))") == 255);
}

TEST_CASE("u8 parameters and returns") {
  const std::string prelude = R"(
[return<u8>]
brighten([u8] value, [u8] amount) {
  return(value + amount)
}
)";
  CHECK(runMain("  return(convert<i32>(brighten(250u8, 10u8)))", nullptr, prelude) == 4);
}

TEST_CASE("u8 struct fields") {
  const std::string prelude = R"(
[struct]
Pixel() {
  [u8] red{0u8}
  [u8] green{0u8}
  [bool] on{false}
  [u8] blue{0u8}
}
)";
  CHECK(runMain("  [Pixel mut] p{Pixel{250u8, 7u8, true, 9u8}}\n"
                "  p.red = p.red + 10u8\n"
                "  return(convert<i32>(p.red) * 10000i32 + convert<i32>(p.green) * 100i32 + convert<i32>(p.blue))",
                nullptr,
                prelude) == 40709);
}

TEST_CASE("u8 cannot be mixed with other integers") {
  const std::string wide = diagnosticsFor("  [u8] a{1u8}\n  [u64] b{2u64}\n  return(convert<i32>(a + b))");
  CHECK(wide.find("u8 operands cannot be mixed") != std::string::npos);
  const std::string compare = diagnosticsFor("  [u8] a{1u8}\n  [u64] b{2u64}\n  return(if(a < b, then() { 1i32 }, else() { 0i32 }))");
  CHECK(compare.find("u8 operands cannot be mixed") != std::string::npos);
  const std::string narrow = diagnosticsFor("  [u8] a{1u8}\n  [i32] b{2i32}\n  return(convert<i32>(a + b))");
  CHECK(narrow.find("mixed") != std::string::npos);
}

TEST_CASE("u8 binding rejects a wider initializer") {
  CHECK_FALSE(diagnosticsFor("  [u8] c{5i32}\n  return(0i32)").empty());
}

TEST_CASE("u8 values cross the host boundary") {
  ScriptEngine engine;
  const std::string source = R"(
[host return<u8>]
host_twice([u8] value) {
}

[return<int>]
main() {
  [u8] answer{host_twice(200u8)}
  return(convert<i32>(answer))
}
)";
  auto script = engine.compileSource("/u8_host.prime", source);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  script.bind("host_twice", [](uint8_t value) { return static_cast<uint8_t>(value * 2); });
  const auto result = script.run();
  CHECK(result.ok);
  CHECK(result.exitCode == 144);
}
