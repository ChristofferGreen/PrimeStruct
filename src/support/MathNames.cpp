#include "primec/support/MathNames.h"

#include <cstddef>
#include <iterator>

namespace primec {

namespace {

struct NameList {
  const std::string_view *data;
  size_t size;
};

constexpr std::string_view ClampNames[] = {"clamp"};
constexpr std::string_view MinMaxNames[] = {"min", "max"};
constexpr std::string_view LerpNames[] = {"lerp"};
constexpr std::string_view FmaNames[] = {"fma"};
constexpr std::string_view HypotNames[] = {"hypot"};
constexpr std::string_view CopysignNames[] = {"copysign"};
constexpr std::string_view AngleNames[] = {"radians", "degrees"};
constexpr std::string_view TrigNames[] = {"sin", "cos", "tan"};
constexpr std::string_view Trig2Names[] = {"atan2"};
constexpr std::string_view ArcTrigNames[] = {"asin", "acos", "atan"};
constexpr std::string_view HyperbolicNames[] = {"sinh", "cosh", "tanh"};
constexpr std::string_view ArcHyperbolicNames[] = {"asinh", "acosh", "atanh"};
constexpr std::string_view ExpNames[] = {"exp", "exp2"};
constexpr std::string_view LogNames[] = {"log", "log2", "log10"};
constexpr std::string_view AbsSignNames[] = {"abs", "sign"};
constexpr std::string_view SaturateNames[] = {"saturate"};
constexpr std::string_view PowNames[] = {"pow"};
constexpr std::string_view PredicateNames[] = {"is_nan", "is_inf", "is_finite"};
constexpr std::string_view RoundingNames[] = {"floor", "ceil", "round", "trunc", "fract"};
constexpr std::string_view RootNames[] = {"sqrt", "cbrt"};
constexpr std::string_view ConstantNames[] = {"pi", "tau", "e"};

NameList familyNames(MathFamily family) {
  switch (family) {
  case MathFamily::Clamp:
    return {ClampNames, std::size(ClampNames)};
  case MathFamily::MinMax:
    return {MinMaxNames, std::size(MinMaxNames)};
  case MathFamily::Lerp:
    return {LerpNames, std::size(LerpNames)};
  case MathFamily::Fma:
    return {FmaNames, std::size(FmaNames)};
  case MathFamily::Hypot:
    return {HypotNames, std::size(HypotNames)};
  case MathFamily::Copysign:
    return {CopysignNames, std::size(CopysignNames)};
  case MathFamily::Angle:
    return {AngleNames, std::size(AngleNames)};
  case MathFamily::Trig:
    return {TrigNames, std::size(TrigNames)};
  case MathFamily::Trig2:
    return {Trig2Names, std::size(Trig2Names)};
  case MathFamily::ArcTrig:
    return {ArcTrigNames, std::size(ArcTrigNames)};
  case MathFamily::Hyperbolic:
    return {HyperbolicNames, std::size(HyperbolicNames)};
  case MathFamily::ArcHyperbolic:
    return {ArcHyperbolicNames, std::size(ArcHyperbolicNames)};
  case MathFamily::Exp:
    return {ExpNames, std::size(ExpNames)};
  case MathFamily::Log:
    return {LogNames, std::size(LogNames)};
  case MathFamily::AbsSign:
    return {AbsSignNames, std::size(AbsSignNames)};
  case MathFamily::Saturate:
    return {SaturateNames, std::size(SaturateNames)};
  case MathFamily::Pow:
    return {PowNames, std::size(PowNames)};
  case MathFamily::Predicate:
    return {PredicateNames, std::size(PredicateNames)};
  case MathFamily::Rounding:
    return {RoundingNames, std::size(RoundingNames)};
  case MathFamily::Root:
    return {RootNames, std::size(RootNames)};
  case MathFamily::Constant:
    return {ConstantNames, std::size(ConstantNames)};
  }
  return {nullptr, 0};
}

constexpr MathFamily AllFamilies[] = {
    MathFamily::Clamp,    MathFamily::MinMax,    MathFamily::Lerp,       MathFamily::Fma,
    MathFamily::Hypot,    MathFamily::Copysign,  MathFamily::Angle,      MathFamily::Trig,
    MathFamily::Trig2,    MathFamily::ArcTrig,   MathFamily::Hyperbolic, MathFamily::ArcHyperbolic,
    MathFamily::Exp,      MathFamily::Log,       MathFamily::AbsSign,    MathFamily::Saturate,
    MathFamily::Pow,      MathFamily::Predicate, MathFamily::Rounding,   MathFamily::Root,
    MathFamily::Constant,
};

} // namespace

bool parseMathName(std::string_view spelling, std::string &out, MathNamePolicy policy) {
  if (spelling.empty()) {
    return false;
  }
  const bool hasLeadingSlash = spelling.front() == '/';
  std::string_view rest = hasLeadingSlash ? spelling.substr(1) : spelling;
  constexpr std::string_view StdMath = "std/math/";
  if (rest.substr(0, StdMath.size()) == StdMath) {
    out.assign(rest.substr(StdMath.size()));
    return true;
  }
  if (rest.find('/') != std::string_view::npos) {
    return false;
  }
  if (!hasLeadingSlash && !policy.allowBare) {
    return false;
  }
  out.assign(rest);
  return true;
}

bool isMathFamilyName(MathFamily family, std::string_view name) {
  const NameList list = familyNames(family);
  for (size_t i = 0; i < list.size; ++i) {
    if (list.data[i] == name) {
      return true;
    }
  }
  return false;
}

bool isElementaryMathName(std::string_view name) {
  for (const MathFamily family : AllFamilies) {
    if (family == MathFamily::Clamp || family == MathFamily::MinMax ||
        family == MathFamily::AbsSign || family == MathFamily::Saturate ||
        family == MathFamily::Constant) {
      continue;
    }
    if (isMathFamilyName(family, name)) {
      return true;
    }
  }
  return false;
}

bool isMathFunctionName(std::string_view name) {
  for (const MathFamily family : AllFamilies) {
    if (family != MathFamily::Constant && isMathFamilyName(family, name)) {
      return true;
    }
  }
  return false;
}

bool classifyMathName(std::string_view spelling,
                      std::string &out,
                      MathNamePolicy policy,
                      MathFamily family) {
  if (!parseMathName(spelling, out, policy)) {
    return false;
  }
  return isMathFamilyName(family, out);
}

} // namespace primec
