#include "third_party/doctest.h"

#include "primec/support/MathNames.h"

#include <string>
#include <vector>

// One rule turns a spelling into a builtin math name for semantics, the lowerer and the IR printer. The table runs
// the same spellings through it with each policy.

TEST_SUITE_BEGIN("primestruct.semantics.math_names");

namespace {

struct Row {
  const char *spelling;
  const char *withImport;    // the math name with allowBare, "" when none
  const char *withoutImport; // the math name without allowBare, "" when none
};

std::string parsed(const char *spelling, bool allowBare) {
  std::string out;
  return primec::parseMathName(spelling, out, {allowBare}) ? out : std::string();
}

} // namespace

TEST_CASE("spellings read as math names by policy") {
  const std::vector<Row> rows = {
      {"min", "min", ""},
      {"/min", "min", "min"},
      {"std/math/min", "min", "min"},
      {"/std/math/min", "min", "min"},
      {"/foo/min", "", ""},
      {"foo/min", "", ""},
      {"/foo/bar/min", "", ""},
      {"/std/other/min", "", ""},
      {"/std/math/", "", ""},
      {"", "", ""},
  };
  for (const Row &row : rows) {
    CAPTURE(row.spelling);
    // `/std/math/` parses to an empty name, which reads as "no name" in this table.
    CHECK(parsed(row.spelling, true) == std::string(row.withImport));
    CHECK(parsed(row.spelling, false) == std::string(row.withoutImport));
  }
}

TEST_CASE("math families own their names") {
  CHECK(primec::isMathFamilyName(primec::MathFamily::MinMax, "min"));
  CHECK(primec::isMathFamilyName(primec::MathFamily::MinMax, "max"));
  CHECK_FALSE(primec::isMathFamilyName(primec::MathFamily::MinMax, "abs"));
  CHECK(primec::isMathFamilyName(primec::MathFamily::AbsSign, "sign"));
  CHECK(primec::isMathFamilyName(primec::MathFamily::Constant, "tau"));
  CHECK(primec::isMathFamilyName(primec::MathFamily::Rounding, "fract"));
  CHECK_FALSE(primec::isMathFamilyName(primec::MathFamily::Trig, "atan2"));
  CHECK(primec::isMathFamilyName(primec::MathFamily::Trig2, "atan2"));
}

TEST_CASE("elementary math names exclude clamp, min, max, abs, sign, saturate and constants") {
  for (const char *name : {"sqrt", "pow", "lerp", "sin", "atanh", "is_nan", "hypot", "radians"}) {
    CAPTURE(name);
    CHECK(primec::isElementaryMathName(name));
    CHECK(primec::isMathFunctionName(name));
  }
  for (const char *name : {"clamp", "min", "max", "abs", "sign", "saturate", "pi"}) {
    CAPTURE(name);
    CHECK_FALSE(primec::isElementaryMathName(name));
  }
  CHECK(primec::isMathFunctionName("clamp"));
  CHECK_FALSE(primec::isMathFunctionName("pi"));
  CHECK_FALSE(primec::isMathFunctionName("print"));
}

TEST_CASE("classifying a spelling checks the family after reading the name") {
  std::string out;
  CHECK(primec::classifyMathName("/std/math/max", out, {false}, primec::MathFamily::MinMax));
  CHECK(out == "max");
  CHECK_FALSE(primec::classifyMathName("/foo/max", out, {true}, primec::MathFamily::MinMax));
  CHECK_FALSE(primec::classifyMathName("max", out, {false}, primec::MathFamily::MinMax));
  CHECK_FALSE(primec::classifyMathName("/abs", out, {true}, primec::MathFamily::MinMax));
}

TEST_SUITE_END();
