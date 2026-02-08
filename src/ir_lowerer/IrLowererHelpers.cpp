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

bool getBuiltinClampName(const Expr &expr) {
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
  return name == "clamp";
}

bool getBuiltinMinMaxName(const Expr &expr, std::string &out) {
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
  if (name == "min" || name == "max") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinAbsSignName(const Expr &expr, std::string &out) {
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
  if (name == "abs" || name == "sign") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinSaturateName(const Expr &expr, std::string &out) {
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
  if (name == "saturate") {
    out = name;
    return true;
  }
  return false;
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
  if (name == "array" || name == "map") {
    out = name;
    return true;
  }
  return false;
}

} // namespace primec::ir_lowerer
