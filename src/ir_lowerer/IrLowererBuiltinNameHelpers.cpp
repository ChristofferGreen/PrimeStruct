#include "IrLowererHelpers.h"

namespace primec::ir_lowerer {
namespace {

bool parseMathName(const std::string &name, std::string &out, bool allowBare) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/math/", 0) == 0) {
    out = normalized.substr(9);
    return true;
  }
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  if (!allowBare) {
    return false;
  }
  out = normalized;
  return true;
}

} // namespace

bool getBuiltinClampName(const Expr &expr, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name;
  if (!parseMathName(expr.name, name, allowBare)) {
    return false;
  }
  return name == "clamp";
}

bool getBuiltinMinMaxName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "min" || out == "max";
}

bool getBuiltinLerpName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "lerp";
}

bool getBuiltinFmaName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "fma";
}

bool getBuiltinHypotName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "hypot";
}

bool getBuiltinCopysignName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "copysign";
}

bool getBuiltinAngleName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "radians" || out == "degrees";
}

bool getBuiltinTrigName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "sin" || out == "cos" || out == "tan";
}

bool getBuiltinTrig2Name(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "atan2";
}

bool getBuiltinArcTrigName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "asin" || out == "acos" || out == "atan";
}

bool getBuiltinHyperbolicName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "sinh" || out == "cosh" || out == "tanh";
}

bool getBuiltinArcHyperbolicName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "asinh" || out == "acosh" || out == "atanh";
}

bool getBuiltinExpName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "exp" || out == "exp2";
}

bool getBuiltinLogName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "log" || out == "log2" || out == "log10";
}

bool getBuiltinAbsSignName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "abs" || out == "sign";
}

bool getBuiltinSaturateName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "saturate";
}

bool getBuiltinPowName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "pow";
}

bool getBuiltinMathPredicateName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "is_nan" || out == "is_inf" || out == "is_finite";
}

bool getBuiltinRoundingName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "floor" || out == "ceil" || out == "round" || out == "trunc" || out == "fract";
}

bool getBuiltinRootName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "sqrt" || out == "cbrt";
}

bool getBuiltinConvertName(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == "convert";
}

bool getBuiltinMemoryName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/intrinsics/memory/", 0) != 0) {
    return false;
  }
  normalized = normalized.substr(22);
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  if (normalized == "alloc" || normalized == "free" || normalized == "realloc" || normalized == "at" ||
      normalized == "at_unsafe" || normalized == "reinterpret") {
    out = normalized;
    return true;
  }
  return false;
}

bool getBuiltinGpuName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/gpu/", 0) != 0) {
    return false;
  }
  normalized = normalized.substr(8);
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  if (normalized == "global_id_x" || normalized == "global_id_y" || normalized == "global_id_z") {
    out = normalized;
    return true;
  }
  return false;
}

bool getBuiltinArrayAccessName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  auto stripGeneratedSuffix = [](std::string alias) {
    const size_t suffix = alias.find("__");
    if (suffix != std::string::npos) {
      alias.erase(suffix);
    }
    return alias;
  };
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("std/collections/vector/", 0) == 0) {
    std::string alias = stripGeneratedSuffix(name.substr(std::string("std/collections/vector/").size()));
    if (alias == "at" || alias == "at_unsafe") {
      out = alias;
      return true;
    }
    return false;
  }
  if (name.rfind("array/", 0) == 0) {
    return false;
  }
  if (name.rfind("map/", 0) == 0) {
    std::string alias = stripGeneratedSuffix(name.substr(std::string("map/").size()));
    if (alias == "at" || alias == "at_unsafe") {
      out = alias;
      return true;
    }
    return false;
  }
  if (name.rfind("std/collections/map/", 0) == 0) {
    std::string alias =
        stripGeneratedSuffix(name.substr(std::string("std/collections/map/").size()));
    if (alias == "at" || alias == "at_unsafe") {
      out = alias;
      return true;
    }
    return false;
  }
  name = stripGeneratedSuffix(std::move(name));
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "at" || name == "at_unsafe") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinPointerName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name == "dereference" || name == "location" || name == "soa_vector/dereference" ||
      name == "soa_vector/location") {
    if (name == "soa_vector/dereference") {
      out = "dereference";
      return true;
    }
    if (name == "soa_vector/location") {
      out = "location";
      return true;
    }
    out = name;
    return true;
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return false;
}

bool getBuiltinCollectionName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  auto isMapEntryCallExpr = [](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
      return false;
    }
    std::string normalizedName = candidate.name;
    if (!normalizedName.empty() && normalizedName.front() == '/') {
      normalizedName.erase(normalizedName.begin());
    }
    const auto generatedSuffix = normalizedName.find("__");
    if (generatedSuffix != std::string::npos) {
      normalizedName.erase(generatedSuffix);
    }
    return normalizedName == "map/entry" ||
           normalizedName == "std/collections/map/entry";
  };
  const bool hasEntryCtorArgs = [&]() {
    for (const auto &arg : expr.args) {
      if (isMapEntryCallExpr(arg)) {
        return true;
      }
    }
    return false;
  }();
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("vector/", 0) == 0) {
    std::string alias = name.substr(std::string("vector/").size());
    if (alias == "vector") {
      out = "vector";
      return true;
    }
    return false;
  }
  if (name.rfind("std/collections/vector/", 0) == 0) {
    std::string alias = name.substr(std::string("std/collections/vector/").size());
    if (alias == "vector") {
      out = "vector";
      return true;
    }
    return false;
  }
  if (name.rfind("map/", 0) == 0) {
    std::string alias = name.substr(std::string("map/").size());
    if (alias == "map") {
      if (hasEntryCtorArgs) {
        return false;
      }
      out = "map";
      return true;
    }
    return false;
  }
  if (name.rfind("std/collections/map/", 0) == 0) {
    std::string alias = name.substr(std::string("std/collections/map/").size());
    if (alias == "map") {
      if (hasEntryCtorArgs) {
        return false;
      }
      out = "map";
      return true;
    }
    return false;
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "array" || name == "vector" || name == "map" || name == "soa_vector") {
    out = name;
    return true;
  }
  return false;
}

} // namespace primec::ir_lowerer
