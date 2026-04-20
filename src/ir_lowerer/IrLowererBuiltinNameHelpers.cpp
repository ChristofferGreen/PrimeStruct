#include "IrLowererHelpers.h"

namespace primec::ir_lowerer {
namespace {
std::string resolveScopedExprName(const Expr &expr) {
  if (expr.name.find('/') != std::string::npos || expr.namespacePrefix.empty()) {
    return expr.name;
  }
  if (expr.namespacePrefix == "/") {
    return "/" + expr.name;
  }
  return expr.namespacePrefix + "/" + expr.name;
}

std::string resolveMathExprName(const Expr &expr) {
  return resolveScopedExprName(expr);
}

bool parseMathName(const std::string &name, std::string &out, bool allowBare) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  const bool hasLeadingSlash = !normalized.empty() && normalized[0] == '/';
  if (hasLeadingSlash) {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/math/", 0) == 0) {
    out = normalized.substr(9);
    return true;
  }
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  if (hasLeadingSlash) {
    out = normalized;
    return true;
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
  if (!parseMathName(resolveMathExprName(expr), name, allowBare)) {
    return false;
  }
  return name == "clamp";
}

bool getBuiltinMinMaxName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "min" || out == "max";
}

bool getBuiltinLerpName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "lerp";
}

bool getBuiltinFmaName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "fma";
}

bool getBuiltinHypotName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "hypot";
}

bool getBuiltinCopysignName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "copysign";
}

bool getBuiltinAngleName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "radians" || out == "degrees";
}

bool getBuiltinTrigName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "sin" || out == "cos" || out == "tan";
}

bool getBuiltinTrig2Name(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "atan2";
}

bool getBuiltinArcTrigName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "asin" || out == "acos" || out == "atan";
}

bool getBuiltinHyperbolicName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "sinh" || out == "cosh" || out == "tanh";
}

bool getBuiltinArcHyperbolicName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "asinh" || out == "acosh" || out == "atanh";
}

bool getBuiltinExpName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "exp" || out == "exp2";
}

bool getBuiltinLogName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "log" || out == "log2" || out == "log10";
}

bool getBuiltinAbsSignName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "abs" || out == "sign";
}

bool getBuiltinSaturateName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "saturate";
}

bool getBuiltinPowName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "pow";
}

bool getBuiltinMathPredicateName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "is_nan" || out == "is_inf" || out == "is_finite";
}

bool getBuiltinRoundingName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "floor" || out == "ceil" || out == "round" || out == "trunc" || out == "fract";
}

bool getBuiltinRootName(const Expr &expr, std::string &out, bool allowBare) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (!parseMathName(resolveMathExprName(expr), out, allowBare)) {
    return false;
  }
  return out == "sqrt" || out == "cbrt";
}

bool getBuiltinConvertName(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = resolveScopedExprName(expr);
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
  std::string normalized = resolveScopedExprName(expr);
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
  std::string normalized = resolveScopedExprName(expr);
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
  auto matchAccessAlias = [&](const std::string &normalizedName, const char *prefix,
                              const char *receiverBase) {
    const std::string prefixText(prefix);
    if (normalizedName.rfind(prefixText, 0) != 0) {
      return false;
    }
    std::string alias = normalizedName.substr(prefixText.size());
    const size_t slash = alias.find('/');
    if (slash != std::string::npos) {
      const std::string receiverPath = alias.substr(0, slash);
      const std::string receiverBaseText(receiverBase);
      if (receiverPath != receiverBaseText &&
          receiverPath.rfind(receiverBaseText + "__", 0) != 0) {
        return false;
      }
      alias = alias.substr(slash + 1);
    }
    alias = stripGeneratedSuffix(std::move(alias));
    if (alias == "at" || alias == "at_unsafe") {
      out = alias;
      return true;
    }
    return false;
  };
  std::string name = resolveScopedExprName(expr);
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (matchAccessAlias(name, "std/collections/vector/", "Vector")) {
    return true;
  }
  if (name.rfind("std/collections/vector/", 0) == 0) {
    return false;
  }
  if (matchAccessAlias(name, "std/collections/experimental_vector/", "Vector")) {
    return true;
  }
  if (name.rfind("std/collections/experimental_vector/", 0) == 0) {
    return false;
  }
  if (matchAccessAlias(name, "std/collections/internal_soa_storage/", "SoaColumn")) {
    return true;
  }
  if (name.rfind("std/collections/internal_soa_storage/", 0) == 0) {
    std::string alias = stripGeneratedSuffix(
        name.substr(std::string("std/collections/internal_soa_storage/").size()));
    if (alias == "at" || alias == "at_unsafe") {
      out = alias;
      return true;
    }
    return false;
  }
  if (name.rfind("array/", 0) == 0) {
    return false;
  }
  if (matchAccessAlias(name, "map/", "Map")) {
    return true;
  }
  if (name.rfind("map/", 0) == 0) {
    return false;
  }
  if (matchAccessAlias(name, "std/collections/map/", "Map")) {
    return true;
  }
  if (name.rfind("std/collections/map/", 0) == 0) {
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
  std::string scopedName = resolveScopedExprName(expr);
  if (!scopedName.empty() && scopedName[0] == '/') {
    scopedName.erase(0, 1);
  }
  std::string rawName = expr.name;
  if (!rawName.empty() && rawName[0] == '/') {
    rawName.erase(0, 1);
  }
  if (scopedName == "soa_vector/dereference" || scopedName == "soa_vector/location") {
    if (scopedName == "soa_vector/dereference") {
      out = "dereference";
      return true;
    }
    if (scopedName == "soa_vector/location") {
      out = "location";
      return true;
    }
  }
  if (rawName.find('/') != std::string::npos) {
    return false;
  }
  if (rawName == "dereference" || rawName == "location") {
    out = rawName;
    return true;
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
    std::string normalizedName = resolveScopedExprName(candidate);
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
  std::string scopedName = resolveScopedExprName(expr);
  if (!scopedName.empty() && scopedName[0] == '/') {
    scopedName.erase(scopedName.begin());
  }
  std::string rawName = expr.name;
  if (!rawName.empty() && rawName[0] == '/') {
    rawName.erase(rawName.begin());
  }
  if (scopedName.rfind("std/collections/vector/", 0) == 0) {
    std::string alias = scopedName.substr(std::string("std/collections/vector/").size());
    if (alias == "vector") {
      out = "vector";
      return true;
    }
    return false;
  }
  if (scopedName.rfind("map/", 0) == 0) {
    std::string alias = scopedName.substr(std::string("map/").size());
    if (alias == "map") {
      if (hasEntryCtorArgs) {
        return false;
      }
      out = "map";
      return true;
    }
    return false;
  }
  if (scopedName.rfind("std/collections/map/", 0) == 0) {
    std::string alias = scopedName.substr(std::string("std/collections/map/").size());
    if (alias == "map") {
      if (hasEntryCtorArgs) {
        return false;
      }
      out = "map";
      return true;
    }
    return false;
  }
  if (rawName.find('/') != std::string::npos) {
    return false;
  }
  if (rawName == "array" || rawName == "vector" || rawName == "map" || rawName == "soa_vector") {
    out = rawName;
    return true;
  }
  return false;
}

} // namespace primec::ir_lowerer
