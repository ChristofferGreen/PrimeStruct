#pragma once

#include <string>
#include <string_view>

namespace primec {

// The builtin math functions and constants of `/std/math`, classified once for every layer (semantics, the
// lowerer and the IR printer). Each layer decides which text of an expression to classify (its name, or the name
// with its namespace prefix); the rule that turns a spelling into a math name is here.

enum class MathFamily {
  Clamp,
  MinMax,
  Lerp,
  Fma,
  Hypot,
  Copysign,
  Angle,
  Trig,
  Trig2,
  ArcTrig,
  Hyperbolic,
  ArcHyperbolic,
  Exp,
  Log,
  AbsSign,
  Saturate,
  Pow,
  Predicate,
  Rounding,
  Root,
  Constant,
};

struct MathNamePolicy {
  // A bare name (`min`) is a math name only when `/std/math` is imported.
  bool allowBare = false;
};

// Reads a math name from a spelling: `std/math/min` and `/std/math/min` give `min`; `/min` gives `min`; a bare `min`
// gives `min` only with `policy.allowBare`; any other path (`/foo/min`, `foo/min`) is not a math name. The result is
// not checked against the families.
bool parseMathName(std::string_view spelling, std::string &out, MathNamePolicy policy);

// True when `name` is one of the names of `family`.
bool isMathFamilyName(MathFamily family, std::string_view name);

// True for the names of every family except Clamp, MinMax, AbsSign, Saturate and Constant (the elementary math
// functions).
bool isElementaryMathName(std::string_view name);

// True for every function name of every family but Constant.
bool isMathFunctionName(std::string_view name);

// `parseMathName` followed by `isMathFamilyName`; `out` is the name on success.
bool classifyMathName(std::string_view spelling,
                      std::string &out,
                      MathNamePolicy policy,
                      MathFamily family);

} // namespace primec
