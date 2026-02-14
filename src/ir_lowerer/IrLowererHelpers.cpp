#include "IrLowererHelpers.h"

namespace primec::ir_lowerer {

bool isSimpleCallName(const Expr &expr, const char *nameToMatch) {
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
  return name == nameToMatch;
}

bool isReturnCall(const Expr &expr) {
  return isSimpleCallName(expr, "return");
}

bool isIfCall(const Expr &expr) {
  return isSimpleCallName(expr, "if");
}

bool isLoopCall(const Expr &expr) {
  return isSimpleCallName(expr, "loop");
}

bool isWhileCall(const Expr &expr) {
  return isSimpleCallName(expr, "while");
}

bool isForCall(const Expr &expr) {
  return isSimpleCallName(expr, "for");
}

bool isRepeatCall(const Expr &expr) {
  return isSimpleCallName(expr, "repeat");
}

bool isBlockCall(const Expr &expr) {
  return isSimpleCallName(expr, "block");
}

bool hasNamedArguments(const std::vector<std::optional<std::string>> &argNames) {
  for (const auto &name : argNames) {
    if (name.has_value()) {
      return true;
    }
  }
  return false;
}

bool getPrintBuiltin(const Expr &expr, PrintBuiltin &out) {
  if (isSimpleCallName(expr, "print")) {
    out.target = PrintTarget::Out;
    out.newline = false;
    out.name = "print";
    return true;
  }
  if (isSimpleCallName(expr, "print_line")) {
    out.target = PrintTarget::Out;
    out.newline = true;
    out.name = "print_line";
    return true;
  }
  if (isSimpleCallName(expr, "print_error")) {
    out.target = PrintTarget::Err;
    out.newline = false;
    out.name = "print_error";
    return true;
  }
  if (isSimpleCallName(expr, "print_line_error")) {
    out.target = PrintTarget::Err;
    out.newline = true;
    out.name = "print_line_error";
    return true;
  }
  return false;
}

bool getPathSpaceBuiltin(const Expr &expr, PathSpaceBuiltin &out) {
  if (isSimpleCallName(expr, "notify")) {
    out.name = "notify";
    out.argumentCount = 2;
    return true;
  }
  if (isSimpleCallName(expr, "insert")) {
    out.name = "insert";
    out.argumentCount = 2;
    return true;
  }
  if (isSimpleCallName(expr, "take")) {
    out.name = "take";
    out.argumentCount = 1;
    return true;
  }
  return false;
}

bool getBuiltinOperatorName(const Expr &expr, std::string &out) {
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
  if (name == "plus" || name == "minus" || name == "multiply" || name == "divide" || name == "negate") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinComparisonName(const Expr &expr, std::string &out) {
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
  if (name == "greater_than" || name == "less_than" || name == "equal" || name == "not_equal" ||
      name == "greater_equal" || name == "less_equal" || name == "and" || name == "or" || name == "not") {
    out = name;
    return true;
  }
  return false;
}

namespace {

bool parseMathName(const std::string &name, std::string &out, bool allowBare) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("math/", 0) == 0) {
    out = normalized.substr(5);
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

bool getBuiltinArrayAccessName(const Expr &expr, std::string &out) {
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
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "dereference" || name == "location") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinCollectionName(const Expr &expr, std::string &out) {
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
  if (name == "array" || name == "vector" || name == "map") {
    out = name;
    return true;
  }
  return false;
}

} // namespace primec::ir_lowerer
