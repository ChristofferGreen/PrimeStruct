#include "primec/GlslEmitter.h"

#include <string>
#include <unordered_map>

namespace primec {
namespace {

enum class GlslType {
  Bool,
  Int,
  UInt,
  Int64,
  UInt64,
  Float,
  Double,
  Unknown,
};

struct LocalInfo {
  GlslType type = GlslType::Unknown;
  bool isMutable = false;
};

struct EmitState {
  std::unordered_map<std::string, LocalInfo> locals;
  bool needsInt64Ext = false;
  bool needsFp64Ext = false;
  uint32_t tempIndex = 0;
};

struct ExprResult {
  std::string code;
  GlslType type = GlslType::Unknown;
};

std::string normalizeName(const Expr &expr) {
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  return name;
}

bool isSimpleCallName(const Expr &expr, const char *name) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string normalized = normalizeName(expr);
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  return normalized == name;
}

bool getMathBuiltinName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string normalized = normalizeName(expr);
  if (normalized.rfind("math/", 0) == 0) {
    out = normalized.substr(5);
  } else {
    if (normalized.find('/') != std::string::npos) {
      return false;
    }
    out = normalized;
  }
  return out == "abs" || out == "sign" || out == "min" || out == "max" || out == "clamp" || out == "saturate" ||
         out == "lerp" || out == "pow" || out == "floor" || out == "ceil" || out == "round" || out == "trunc" ||
         out == "fract" || out == "sqrt" || out == "cbrt" || out == "exp" || out == "exp2" || out == "log" ||
         out == "log2" || out == "log10" || out == "sin" || out == "cos" || out == "tan" || out == "asin" ||
         out == "acos" || out == "atan" || out == "atan2" || out == "radians" || out == "degrees" ||
         out == "sinh" || out == "cosh" || out == "tanh" || out == "asinh" || out == "acosh" || out == "atanh" ||
         out == "fma" || out == "hypot" || out == "copysign" || out == "is_nan" || out == "is_inf" ||
         out == "is_finite";
}

bool isBindingAuxTransformName(const std::string &name) {
  return name == "mut" || name == "copy" || name == "restrict" || name == "align_bytes" || name == "align_kbytes" ||
         name == "pod" || name == "handle" || name == "gpu_lane" || name == "public" || name == "private" ||
         name == "package" || name == "static";
}

bool getExplicitBindingTypeName(const Expr &expr, std::string &outTypeName) {
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      continue;
    }
    if (isBindingAuxTransformName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    if (transform.templateArgs.empty()) {
      outTypeName = transform.name;
      return true;
    }
  }
  return false;
}

GlslType glslTypeFromName(const std::string &name, EmitState &state, std::string &error) {
  if (name == "bool") {
    return GlslType::Bool;
  }
  if (name == "i32" || name == "int") {
    return GlslType::Int;
  }
  if (name == "u32") {
    return GlslType::UInt;
  }
  if (name == "i64") {
    state.needsInt64Ext = true;
    return GlslType::Int64;
  }
  if (name == "u64") {
    state.needsInt64Ext = true;
    return GlslType::UInt64;
  }
  if (name == "f32" || name == "float") {
    return GlslType::Float;
  }
  if (name == "f64") {
    state.needsFp64Ext = true;
    return GlslType::Double;
  }
  error = "glsl backend does not support type: " + name;
  return GlslType::Unknown;
}

std::string glslTypeName(GlslType type) {
  switch (type) {
  case GlslType::Bool:
    return "bool";
  case GlslType::Int:
    return "int";
  case GlslType::UInt:
    return "uint";
  case GlslType::Int64:
    return "int64_t";
  case GlslType::UInt64:
    return "uint64_t";
  case GlslType::Float:
    return "float";
  case GlslType::Double:
    return "double";
  case GlslType::Unknown:
    return "";
  }
  return "";
}

bool isNumericType(GlslType type) {
  return type == GlslType::Int || type == GlslType::UInt || type == GlslType::Int64 || type == GlslType::UInt64 ||
         type == GlslType::Float || type == GlslType::Double;
}

bool isIntegerType(GlslType type) {
  return type == GlslType::Int || type == GlslType::UInt || type == GlslType::Int64 || type == GlslType::UInt64;
}

bool isSignedInteger(GlslType type) {
  return type == GlslType::Int || type == GlslType::Int64;
}

bool isUnsignedInteger(GlslType type) {
  return type == GlslType::UInt || type == GlslType::UInt64;
}

void noteTypeExtensions(GlslType type, EmitState &state) {
  if (type == GlslType::Int64 || type == GlslType::UInt64) {
    state.needsInt64Ext = true;
  }
  if (type == GlslType::Double) {
    state.needsFp64Ext = true;
  }
}

std::string ensureFloatLiteral(const std::string &literal);

std::string literalForType(GlslType type, int value) {
  switch (type) {
  case GlslType::Int:
    return std::to_string(value);
  case GlslType::UInt:
    return std::to_string(value) + "u";
  case GlslType::Int64:
    return "int64_t(" + std::to_string(value) + ")";
  case GlslType::UInt64:
    return "uint64_t(" + std::to_string(value) + ")";
  case GlslType::Float: {
    std::string literal = std::to_string(value);
    return ensureFloatLiteral(literal);
  }
  case GlslType::Double: {
    std::string literal = std::to_string(value);
    return "double(" + ensureFloatLiteral(literal) + ")";
  }
  case GlslType::Bool:
  case GlslType::Unknown:
    return std::to_string(value);
  }
  return std::to_string(value);
}

std::string allocTempName(EmitState &state, const std::string &prefix) {
  for (;;) {
    std::string candidate = prefix + std::to_string(state.tempIndex++);
    if (state.locals.find(candidate) == state.locals.end()) {
      return candidate;
    }
  }
}

GlslType combineNumericTypes(GlslType left, GlslType right, std::string &error) {
  if (left == right) {
    return left;
  }
  if (!isNumericType(left) || !isNumericType(right)) {
    error = "glsl backend requires numeric operands";
    return GlslType::Unknown;
  }
  if (left == GlslType::Double || right == GlslType::Double) {
    return GlslType::Double;
  }
  if (left == GlslType::Float || right == GlslType::Float) {
    return GlslType::Float;
  }
  if (left == GlslType::Int64 || right == GlslType::Int64) {
    if (isSignedInteger(left) && isSignedInteger(right)) {
      return GlslType::Int64;
    }
    error = "glsl backend does not allow mixed signed/unsigned integer operands";
    return GlslType::Unknown;
  }
  if (left == GlslType::UInt64 || right == GlslType::UInt64) {
    if (isUnsignedInteger(left) && isUnsignedInteger(right)) {
      return GlslType::UInt64;
    }
    error = "glsl backend does not allow mixed signed/unsigned integer operands";
    return GlslType::Unknown;
  }
  if (left == GlslType::Int && right == GlslType::Int) {
    return GlslType::Int;
  }
  if (left == GlslType::UInt && right == GlslType::UInt) {
    return GlslType::UInt;
  }
  error = "glsl backend does not allow mixed signed/unsigned integer operands";
  return GlslType::Unknown;
}

std::string ensureFloatLiteral(const std::string &literal) {
  if (literal.find('.') == std::string::npos && literal.find('e') == std::string::npos &&
      literal.find('E') == std::string::npos) {
    return literal + ".0";
  }
  return literal;
}

ExprResult emitExpr(const Expr &expr, EmitState &state, std::string &error);

ExprResult castExpr(const ExprResult &expr, GlslType target) {
  if (expr.type == target) {
    return expr;
  }
  ExprResult out;
  out.type = target;
  out.code = glslTypeName(target) + "(" + expr.code + ")";
  return out;
}

ExprResult emitBinaryNumeric(const Expr &leftExpr, const Expr &rightExpr, const char *op, EmitState &state, std::string &error) {
  ExprResult left = emitExpr(leftExpr, state, error);
  if (error.empty() == false) {
    return {};
  }
  ExprResult right = emitExpr(rightExpr, state, error);
  if (error.empty() == false) {
    return {};
  }
  GlslType resultType = combineNumericTypes(left.type, right.type, error);
  if (resultType == GlslType::Unknown) {
    return {};
  }
  left = castExpr(left, resultType);
  right = castExpr(right, resultType);
  ExprResult out;
  out.type = resultType;
  out.code = "(" + left.code + " " + op + " " + right.code + ")";
  return out;
}

ExprResult emitExpr(const Expr &expr, EmitState &state, std::string &error) {
  if (expr.kind == Expr::Kind::Literal) {
    ExprResult out;
    if (expr.isUnsigned) {
      if (expr.intWidth == 64) {
        out.type = GlslType::UInt64;
        out.code = "uint64_t(" + std::to_string(static_cast<uint64_t>(expr.literalValue)) + ")";
      } else {
        out.type = GlslType::UInt;
        out.code = std::to_string(static_cast<uint64_t>(expr.literalValue)) + "u";
      }
    } else if (expr.intWidth == 64) {
      out.type = GlslType::Int64;
      out.code = "int64_t(" + std::to_string(static_cast<int64_t>(expr.literalValue)) + ")";
    } else {
      out.type = GlslType::Int;
      out.code = std::to_string(static_cast<int64_t>(expr.literalValue));
    }
    if (out.type == GlslType::Int64 || out.type == GlslType::UInt64) {
      state.needsInt64Ext = true;
    }
    return out;
  }
  if (expr.kind == Expr::Kind::BoolLiteral) {
    return {expr.boolValue ? "true" : "false", GlslType::Bool};
  }
  if (expr.kind == Expr::Kind::FloatLiteral) {
    ExprResult out;
    if (expr.floatWidth == 64) {
      state.needsFp64Ext = true;
      out.type = GlslType::Double;
      out.code = "double(" + ensureFloatLiteral(expr.floatValue) + ")";
    } else {
      out.type = GlslType::Float;
      out.code = ensureFloatLiteral(expr.floatValue);
    }
    return out;
  }
  if (expr.kind == Expr::Kind::StringLiteral) {
    error = "glsl backend does not support string literals";
    return {};
  }
  if (expr.kind == Expr::Kind::Name) {
    auto it = state.locals.find(expr.name);
    if (it == state.locals.end()) {
      error = "glsl backend requires local binding for name: " + expr.name;
      return {};
    }
    return {expr.name, it->second.type};
  }
  if (expr.kind != Expr::Kind::Call) {
    error = "glsl backend encountered unsupported expression";
    return {};
  }
  const std::string name = normalizeName(expr);
  if (name == "plus" || name == "minus" || name == "multiply" || name == "divide") {
    if (expr.args.size() != 2) {
      error = "glsl backend requires binary numeric operator arguments";
      return {};
    }
    const char *op = name == "plus" ? "+" : (name == "minus" ? "-" : (name == "multiply" ? "*" : "/"));
    return emitBinaryNumeric(expr.args[0], expr.args[1], op, state, error);
  }
  if (name == "negate") {
    if (expr.args.size() != 1) {
      error = "glsl backend requires unary negate argument";
      return {};
    }
    ExprResult arg = emitExpr(expr.args[0], state, error);
    if (!error.empty()) {
      return {};
    }
    if (!isNumericType(arg.type)) {
      error = "glsl backend requires numeric operand for negate";
      return {};
    }
    ExprResult out;
    out.type = arg.type;
    out.code = "(-" + arg.code + ")";
    return out;
  }
  if (name == "assign") {
    if (expr.args.size() != 2) {
      error = "glsl backend requires assign(target, value)";
      return {};
    }
    const Expr &target = expr.args.front();
    if (target.kind != Expr::Kind::Name) {
      error = "glsl backend requires assign target to be a local name";
      return {};
    }
    auto it = state.locals.find(target.name);
    if (it == state.locals.end()) {
      error = "glsl backend requires local binding for assign target";
      return {};
    }
    if (!it->second.isMutable) {
      error = "glsl backend requires assign target to be mutable";
      return {};
    }
    ExprResult value = emitExpr(expr.args[1], state, error);
    if (!error.empty()) {
      return {};
    }
    value = castExpr(value, it->second.type);
    return {"(" + target.name + " = " + value.code + ")", it->second.type};
  }
  if (name == "increment" || name == "decrement") {
    if (expr.args.size() != 1) {
      error = "glsl backend requires increment/decrement target to be a local name";
      return {};
    }
    const Expr &target = expr.args.front();
    if (target.kind != Expr::Kind::Name) {
      error = "glsl backend requires increment/decrement target to be a local name";
      return {};
    }
    auto it = state.locals.find(target.name);
    if (it == state.locals.end()) {
      error = "glsl backend requires local binding for increment/decrement target";
      return {};
    }
    if (!it->second.isMutable) {
      error = "glsl backend requires increment/decrement target to be mutable";
      return {};
    }
    if (!isNumericType(it->second.type)) {
      error = "glsl backend requires increment/decrement target to be numeric";
      return {};
    }
    const char *op = (name == "increment") ? "++" : "--";
    return {"(" + std::string(op) + target.name + ")", it->second.type};
  }
  if (name == "equal" || name == "not_equal" || name == "less_than" || name == "less_equal" ||
      name == "greater_than" || name == "greater_equal") {
    if (expr.args.size() != 2) {
      error = "glsl backend requires comparison arguments";
      return {};
    }
    ExprResult left = emitExpr(expr.args[0], state, error);
    if (!error.empty()) {
      return {};
    }
    ExprResult right = emitExpr(expr.args[1], state, error);
    if (!error.empty()) {
      return {};
    }
    if (left.type == GlslType::Bool || right.type == GlslType::Bool) {
      if (left.type != right.type) {
        error = "glsl backend does not allow mixed boolean/numeric comparisons";
        return {};
      }
    } else {
      GlslType common = combineNumericTypes(left.type, right.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      left = castExpr(left, common);
      right = castExpr(right, common);
    }
    const char *op = "==";
    if (name == "not_equal") {
      op = "!=";
    } else if (name == "less_than") {
      op = "<";
    } else if (name == "less_equal") {
      op = "<=";
    } else if (name == "greater_than") {
      op = ">";
    } else if (name == "greater_equal") {
      op = ">=";
    }
    ExprResult out;
    out.type = GlslType::Bool;
    out.code = "(" + left.code + " " + op + " " + right.code + ")";
    return out;
  }
  if (name == "and" || name == "or") {
    if (expr.args.size() != 2) {
      error = "glsl backend requires boolean arguments";
      return {};
    }
    ExprResult left = emitExpr(expr.args[0], state, error);
    if (!error.empty()) {
      return {};
    }
    ExprResult right = emitExpr(expr.args[1], state, error);
    if (!error.empty()) {
      return {};
    }
    if (left.type != GlslType::Bool || right.type != GlslType::Bool) {
      error = "glsl backend requires boolean operands";
      return {};
    }
    ExprResult out;
    out.type = GlslType::Bool;
    out.code = "(" + left.code + (name == "and" ? " && " : " || ") + right.code + ")";
    return out;
  }
  if (name == "not") {
    if (expr.args.size() != 1) {
      error = "glsl backend requires boolean operand";
      return {};
    }
    ExprResult arg = emitExpr(expr.args[0], state, error);
    if (!error.empty()) {
      return {};
    }
    if (arg.type != GlslType::Bool) {
      error = "glsl backend requires boolean operand";
      return {};
    }
    return {"(!" + arg.code + ")", GlslType::Bool};
  }
  std::string mathName;
  if (getMathBuiltinName(expr, mathName)) {
    auto emitUnaryMath = [&](const char *func, bool requireFloat) -> ExprResult {
      if (expr.args.size() != 1) {
        error = mathName + " requires exactly one argument";
        return {};
      }
      ExprResult arg = emitExpr(expr.args.front(), state, error);
      if (!error.empty()) {
        return {};
      }
      if (requireFloat && arg.type != GlslType::Float && arg.type != GlslType::Double) {
        error = mathName + " requires float argument";
        return {};
      }
      if (mathName == "abs" && (arg.type == GlslType::UInt || arg.type == GlslType::UInt64)) {
        return arg;
      }
      if (mathName == "sign" && (arg.type == GlslType::UInt || arg.type == GlslType::UInt64)) {
        std::string zeroLiteral = literalForType(arg.type, 0);
        std::string oneLiteral = literalForType(arg.type, 1);
        return {"(" + arg.code + " == " + zeroLiteral + " ? " + zeroLiteral + " : " + oneLiteral + ")",
                arg.type};
      }
      return {std::string(func) + "(" + arg.code + ")", arg.type};
    };
    auto emitBinaryMath = [&](const char *func, bool requireFloat) -> ExprResult {
      if (expr.args.size() != 2) {
        error = mathName + " requires exactly two arguments";
        return {};
      }
      ExprResult left = emitExpr(expr.args[0], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult right = emitExpr(expr.args[1], state, error);
      if (!error.empty()) {
        return {};
      }
      GlslType common = combineNumericTypes(left.type, right.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      if (requireFloat && common != GlslType::Float && common != GlslType::Double) {
        error = mathName + " requires float arguments";
        return {};
      }
      left = castExpr(left, common);
      right = castExpr(right, common);
      return {std::string(func) + "(" + left.code + ", " + right.code + ")", common};
    };

    if (mathName == "abs") {
      return emitUnaryMath("abs", false);
    }
    if (mathName == "sign") {
      return emitUnaryMath("sign", false);
    }
    if (mathName == "min") {
      return emitBinaryMath("min", false);
    }
    if (mathName == "max") {
      return emitBinaryMath("max", false);
    }
    if (mathName == "clamp") {
      if (expr.args.size() != 3) {
        error = "clamp requires exactly three arguments";
        return {};
      }
      ExprResult value = emitExpr(expr.args[0], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult minValue = emitExpr(expr.args[1], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult maxValue = emitExpr(expr.args[2], state, error);
      if (!error.empty()) {
        return {};
      }
      GlslType common = combineNumericTypes(value.type, minValue.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      common = combineNumericTypes(common, maxValue.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      value = castExpr(value, common);
      minValue = castExpr(minValue, common);
      maxValue = castExpr(maxValue, common);
      return {"clamp(" + value.code + ", " + minValue.code + ", " + maxValue.code + ")", common};
    }
    if (mathName == "saturate") {
      if (expr.args.size() != 1) {
        error = "saturate requires exactly one argument";
        return {};
      }
      ExprResult value = emitExpr(expr.args[0], state, error);
      if (!error.empty()) {
        return {};
      }
      if (!isNumericType(value.type)) {
        error = "saturate requires numeric argument";
        return {};
      }
      std::string zeroLiteral = literalForType(value.type, 0);
      std::string oneLiteral = literalForType(value.type, 1);
      return {"clamp(" + value.code + ", " + zeroLiteral + ", " + oneLiteral + ")", value.type};
    }
    if (mathName == "lerp") {
      if (expr.args.size() != 3) {
        error = "lerp requires exactly three arguments";
        return {};
      }
      ExprResult start = emitExpr(expr.args[0], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult end = emitExpr(expr.args[1], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult t = emitExpr(expr.args[2], state, error);
      if (!error.empty()) {
        return {};
      }
      GlslType common = combineNumericTypes(start.type, end.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      common = combineNumericTypes(common, t.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      start = castExpr(start, common);
      end = castExpr(end, common);
      t = castExpr(t, common);
      return {"(" + start.code + " + (" + end.code + " - " + start.code + ") * " + t.code + ")", common};
    }
    if (mathName == "pow") {
      return emitBinaryMath("pow", true);
    }
    if (mathName == "floor") {
      return emitUnaryMath("floor", true);
    }
    if (mathName == "ceil") {
      return emitUnaryMath("ceil", true);
    }
    if (mathName == "round") {
      return emitUnaryMath("round", true);
    }
    if (mathName == "trunc") {
      return emitUnaryMath("trunc", true);
    }
    if (mathName == "fract") {
      return emitUnaryMath("fract", true);
    }
    if (mathName == "sqrt") {
      return emitUnaryMath("sqrt", true);
    }
    if (mathName == "cbrt") {
      if (expr.args.size() != 1) {
        error = "cbrt requires exactly one argument";
        return {};
      }
      ExprResult arg = emitExpr(expr.args.front(), state, error);
      if (!error.empty()) {
        return {};
      }
      if (arg.type != GlslType::Float && arg.type != GlslType::Double) {
        error = "cbrt requires float argument";
        return {};
      }
      std::string oneThird = glslTypeName(arg.type) + "(1.0/3.0)";
      return {"pow(" + arg.code + ", " + oneThird + ")", arg.type};
    }
    if (mathName == "exp") {
      return emitUnaryMath("exp", true);
    }
    if (mathName == "exp2") {
      return emitUnaryMath("exp2", true);
    }
    if (mathName == "log") {
      return emitUnaryMath("log", true);
    }
    if (mathName == "log2") {
      return emitUnaryMath("log2", true);
    }
    if (mathName == "log10") {
      if (expr.args.size() != 1) {
        error = "log10 requires exactly one argument";
        return {};
      }
      ExprResult arg = emitExpr(expr.args.front(), state, error);
      if (!error.empty()) {
        return {};
      }
      if (arg.type != GlslType::Float && arg.type != GlslType::Double) {
        error = "log10 requires float argument";
        return {};
      }
      std::string tenLiteral = literalForType(arg.type, 10);
      return {"(log(" + arg.code + ") / log(" + tenLiteral + "))", arg.type};
    }
    if (mathName == "sin" || mathName == "cos" || mathName == "tan" || mathName == "asin" || mathName == "acos" ||
        mathName == "atan" || mathName == "radians" || mathName == "degrees" || mathName == "sinh" ||
        mathName == "cosh" || mathName == "tanh" || mathName == "asinh" || mathName == "acosh" ||
        mathName == "atanh") {
      return emitUnaryMath(mathName.c_str(), true);
    }
    if (mathName == "atan2") {
      if (expr.args.size() != 2) {
        error = "atan2 requires exactly two arguments";
        return {};
      }
      ExprResult y = emitExpr(expr.args[0], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult x = emitExpr(expr.args[1], state, error);
      if (!error.empty()) {
        return {};
      }
      GlslType common = combineNumericTypes(y.type, x.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      if (common != GlslType::Float && common != GlslType::Double) {
        error = "atan2 requires float arguments";
        return {};
      }
      y = castExpr(y, common);
      x = castExpr(x, common);
      return {"atan(" + y.code + ", " + x.code + ")", common};
    }
    if (mathName == "fma") {
      if (expr.args.size() != 3) {
        error = "fma requires exactly three arguments";
        return {};
      }
      ExprResult a = emitExpr(expr.args[0], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult b = emitExpr(expr.args[1], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult c = emitExpr(expr.args[2], state, error);
      if (!error.empty()) {
        return {};
      }
      GlslType common = combineNumericTypes(a.type, b.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      common = combineNumericTypes(common, c.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      if (common != GlslType::Float && common != GlslType::Double) {
        error = "fma requires float arguments";
        return {};
      }
      a = castExpr(a, common);
      b = castExpr(b, common);
      c = castExpr(c, common);
      return {"fma(" + a.code + ", " + b.code + ", " + c.code + ")", common};
    }
    if (mathName == "hypot") {
      if (expr.args.size() != 2) {
        error = "hypot requires exactly two arguments";
        return {};
      }
      ExprResult a = emitExpr(expr.args[0], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult b = emitExpr(expr.args[1], state, error);
      if (!error.empty()) {
        return {};
      }
      GlslType common = combineNumericTypes(a.type, b.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      if (common != GlslType::Float && common != GlslType::Double) {
        error = "hypot requires float arguments";
        return {};
      }
      a = castExpr(a, common);
      b = castExpr(b, common);
      return {"sqrt(" + a.code + " * " + a.code + " + " + b.code + " * " + b.code + ")", common};
    }
    if (mathName == "copysign") {
      if (expr.args.size() != 2) {
        error = "copysign requires exactly two arguments";
        return {};
      }
      ExprResult mag = emitExpr(expr.args[0], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult sign = emitExpr(expr.args[1], state, error);
      if (!error.empty()) {
        return {};
      }
      GlslType common = combineNumericTypes(mag.type, sign.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      if (common != GlslType::Float && common != GlslType::Double) {
        error = "copysign requires float arguments";
        return {};
      }
      mag = castExpr(mag, common);
      sign = castExpr(sign, common);
      return {"(abs(" + mag.code + ") * sign(" + sign.code + "))", common};
    }
    if (mathName == "is_nan" || mathName == "is_inf" || mathName == "is_finite") {
      if (expr.args.size() != 1) {
        error = mathName + " requires exactly one argument";
        return {};
      }
      ExprResult arg = emitExpr(expr.args.front(), state, error);
      if (!error.empty()) {
        return {};
      }
      if (arg.type != GlslType::Float && arg.type != GlslType::Double) {
        error = mathName + " requires float argument";
        return {};
      }
      std::string func = mathName == "is_nan" ? "isnan" : (mathName == "is_inf" ? "isinf" : "isfinite");
      return {func + "(" + arg.code + ")", GlslType::Bool};
    }
    error = "glsl backend does not support math builtin: " + mathName;
    return {};
  }
  if (name == "convert") {
    if (expr.templateArgs.size() != 1 || expr.args.size() != 1) {
      error = "glsl backend requires convert<T>(value)";
      return {};
    }
    ExprResult arg = emitExpr(expr.args[0], state, error);
    if (!error.empty()) {
      return {};
    }
    GlslType target = glslTypeFromName(expr.templateArgs.front(), state, error);
    if (!error.empty()) {
      return {};
    }
    return castExpr(arg, target);
  }
  error = "glsl backend does not support builtin: " + name;
  return {};
}

bool emitStatement(const Expr &stmt, EmitState &state, std::string &out, std::string &error, const std::string &indent);

bool emitBlock(const std::vector<Expr> &stmts,
               EmitState &state,
               std::string &out,
               std::string &error,
               const std::string &indent) {
  auto savedLocals = state.locals;
  for (const auto &stmt : stmts) {
    if (!emitStatement(stmt, state, out, error, indent)) {
      state.locals = savedLocals;
      return false;
    }
  }
  state.locals = savedLocals;
  return true;
}

bool emitStatement(const Expr &stmt, EmitState &state, std::string &out, std::string &error, const std::string &indent) {
  if (stmt.isBinding) {
    bool isMutable = false;
    bool isStatic = false;
    for (const auto &transform : stmt.transforms) {
      if (transform.name == "mut" && transform.templateArgs.empty() && transform.arguments.empty()) {
        isMutable = true;
      }
      if (transform.name == "static" && transform.templateArgs.empty() && transform.arguments.empty()) {
        isStatic = true;
      }
    }
    if (isStatic) {
      error = "glsl backend does not support static bindings";
      return false;
    }
    if (stmt.args.empty()) {
      error = "glsl backend requires binding initializers";
      return false;
    }
    std::string explicitType;
    bool hasExplicitType = getExplicitBindingTypeName(stmt, explicitType);
    ExprResult init = emitExpr(stmt.args.front(), state, error);
    if (!error.empty()) {
      return false;
    }
    GlslType bindingType = init.type;
    if (hasExplicitType) {
      bindingType = glslTypeFromName(explicitType, state, error);
      if (!error.empty()) {
        return false;
      }
      init = castExpr(init, bindingType);
    }
    std::string typeName = glslTypeName(bindingType);
    if (typeName.empty()) {
      error = "glsl backend requires numeric or boolean binding types";
      return false;
    }
    state.locals[stmt.name] = {bindingType, isMutable};
    out += indent;
    if (!isMutable) {
      out += "const ";
    }
    out += typeName + " " + stmt.name + " = " + init.code + ";\n";
    return true;
  }
  if (stmt.kind == Expr::Kind::Call && !stmt.isBinding) {
    if (isSimpleCallName(stmt, "return")) {
      if (!stmt.args.empty()) {
        error = "glsl backend requires entry definition to return void";
        return false;
      }
      out += indent + "return;\n";
      return true;
    }
    if (isSimpleCallName(stmt, "assign")) {
      if (stmt.args.size() != 2) {
        error = "glsl backend requires assign(target, value)";
        return false;
      }
      const Expr &target = stmt.args.front();
      if (target.kind != Expr::Kind::Name) {
        error = "glsl backend requires assign target to be a local name";
        return false;
      }
      auto it = state.locals.find(target.name);
      if (it == state.locals.end()) {
        error = "glsl backend requires local binding for assign target";
        return false;
      }
      if (!it->second.isMutable) {
        error = "glsl backend requires assign target to be mutable";
        return false;
      }
      ExprResult value = emitExpr(stmt.args[1], state, error);
      if (!error.empty()) {
        return false;
      }
      value = castExpr(value, it->second.type);
      out += indent + target.name + " = " + value.code + ";\n";
      return true;
    }
    if (isSimpleCallName(stmt, "increment") || isSimpleCallName(stmt, "decrement")) {
      if (stmt.args.size() != 1 || stmt.args.front().kind != Expr::Kind::Name) {
        error = "glsl backend requires increment/decrement target to be a local name";
        return false;
      }
      const std::string &targetName = stmt.args.front().name;
      auto it = state.locals.find(targetName);
      if (it == state.locals.end()) {
        error = "glsl backend requires local binding for increment/decrement target";
        return false;
      }
      if (!it->second.isMutable) {
        error = "glsl backend requires increment/decrement target to be mutable";
        return false;
      }
      out += indent + targetName + (isSimpleCallName(stmt, "increment") ? "++" : "--") + ";\n";
      return true;
    }
    if (isSimpleCallName(stmt, "if")) {
      if (stmt.args.size() < 2 || stmt.args.size() > 3) {
        error = "glsl backend requires if(cond, then() { ... }, else() { ... })";
        return false;
      }
      ExprResult cond = emitExpr(stmt.args[0], state, error);
      if (!error.empty()) {
        return false;
      }
      if (cond.type != GlslType::Bool) {
        error = "glsl backend requires if condition to be bool";
        return false;
      }
      const Expr &thenExpr = stmt.args[1];
      if (!isSimpleCallName(thenExpr, "then") || !thenExpr.hasBodyArguments) {
        error = "glsl backend requires then() { ... } block";
        return false;
      }
      const Expr *elseExpr = nullptr;
      if (stmt.args.size() == 3) {
        elseExpr = &stmt.args[2];
        if (!isSimpleCallName(*elseExpr, "else") || !elseExpr->hasBodyArguments) {
          error = "glsl backend requires else() { ... } block";
          return false;
        }
      }
      out += indent + "if (" + cond.code + ") {\n";
      if (!emitBlock(thenExpr.bodyArguments, state, out, error, indent + "  ")) {
        return false;
      }
      out += indent + "}";
      if (elseExpr) {
        out += " else {\n";
        if (!emitBlock(elseExpr->bodyArguments, state, out, error, indent + "  ")) {
          return false;
        }
        out += indent + "}";
      }
      out += "\n";
      return true;
    }
    if (isSimpleCallName(stmt, "loop")) {
      if (stmt.args.size() != 2) {
        error = "glsl backend requires loop(count, do() { ... })";
        return false;
      }
      const Expr &countExpr = stmt.args[0];
      const Expr &bodyExpr = stmt.args[1];
      if (!isSimpleCallName(bodyExpr, "do") || !bodyExpr.hasBodyArguments) {
        error = "glsl backend requires loop body in do() { ... }";
        return false;
      }
      ExprResult count = emitExpr(countExpr, state, error);
      if (!error.empty()) {
        return false;
      }
      if (!isIntegerType(count.type)) {
        error = "glsl backend requires loop count to be integer";
        return false;
      }
      noteTypeExtensions(count.type, state);
      const std::string counterName = allocTempName(state, "_ps_loop_");
      const std::string counterType = glslTypeName(count.type);
      const std::string zeroLiteral = literalForType(count.type, 0);
      const std::string oneLiteral = literalForType(count.type, 1);
      const std::string cond =
          isUnsignedInteger(count.type) ? (counterName + " != " + zeroLiteral)
                                         : (counterName + " > " + zeroLiteral);

      auto savedLocals = state.locals;
      state.locals[counterName] = {count.type, true};
      out += indent + "{\n";
      out += indent + "  " + counterType + " " + counterName + " = " + count.code + ";\n";
      out += indent + "  while (" + cond + ") {\n";
      if (!emitBlock(bodyExpr.bodyArguments, state, out, error, indent + "    ")) {
        state.locals = savedLocals;
        return false;
      }
      out += indent + "    " + counterName + " = " + counterName + " - " + oneLiteral + ";\n";
      out += indent + "  }\n";
      out += indent + "}\n";
      state.locals = savedLocals;
      return true;
    }
    if (isSimpleCallName(stmt, "repeat")) {
      if (stmt.args.size() != 1 || !stmt.hasBodyArguments) {
        error = "glsl backend requires repeat(count) { ... }";
        return false;
      }
      ExprResult count = emitExpr(stmt.args[0], state, error);
      if (!error.empty()) {
        return false;
      }
      GlslType counterType = count.type;
      if (counterType == GlslType::Bool) {
        counterType = GlslType::Int;
      }
      if (!isIntegerType(counterType)) {
        error = "glsl backend requires repeat count to be integer or bool";
        return false;
      }
      noteTypeExtensions(counterType, state);
      ExprResult countCast = castExpr(count, counterType);
      const std::string counterName = allocTempName(state, "_ps_repeat_");
      const std::string counterTypeName = glslTypeName(counterType);
      const std::string zeroLiteral = literalForType(counterType, 0);
      const std::string oneLiteral = literalForType(counterType, 1);
      const std::string cond =
          isUnsignedInteger(counterType) ? (counterName + " != " + zeroLiteral)
                                          : (counterName + " > " + zeroLiteral);

      auto savedLocals = state.locals;
      state.locals[counterName] = {counterType, true};
      out += indent + "{\n";
      out += indent + "  " + counterTypeName + " " + counterName + " = " + countCast.code + ";\n";
      out += indent + "  while (" + cond + ") {\n";
      if (!emitBlock(stmt.bodyArguments, state, out, error, indent + "    ")) {
        state.locals = savedLocals;
        return false;
      }
      out += indent + "    " + counterName + " = " + counterName + " - " + oneLiteral + ";\n";
      out += indent + "  }\n";
      out += indent + "}\n";
      state.locals = savedLocals;
      return true;
    }
    if (isSimpleCallName(stmt, "while")) {
      if (stmt.args.size() != 2) {
        error = "glsl backend requires while(cond, do() { ... })";
        return false;
      }
      ExprResult cond = emitExpr(stmt.args[0], state, error);
      if (!error.empty()) {
        return false;
      }
      if (cond.type != GlslType::Bool) {
        error = "glsl backend requires while condition to be bool";
        return false;
      }
      const Expr &bodyExpr = stmt.args[1];
      if (!isSimpleCallName(bodyExpr, "do") || !bodyExpr.hasBodyArguments) {
        error = "glsl backend requires while body in do() { ... }";
        return false;
      }
      out += indent + "while (" + cond.code + ") {\n";
      if (!emitBlock(bodyExpr.bodyArguments, state, out, error, indent + "  ")) {
        return false;
      }
      out += indent + "}\n";
      return true;
    }
    if (isSimpleCallName(stmt, "for")) {
      if (stmt.args.size() != 4) {
        error = "glsl backend requires for(init, cond, step, do() { ... })";
        return false;
      }
      auto savedLocals = state.locals;
      out += indent + "{\n";
      if (!emitStatement(stmt.args[0], state, out, error, indent + "  ")) {
        state.locals = savedLocals;
        return false;
      }
      ExprResult cond = emitExpr(stmt.args[1], state, error);
      if (!error.empty()) {
        state.locals = savedLocals;
        return false;
      }
      if (cond.type != GlslType::Bool) {
        error = "glsl backend requires for condition to be bool";
        state.locals = savedLocals;
        return false;
      }
      const Expr &bodyExpr = stmt.args[3];
      if (!isSimpleCallName(bodyExpr, "do") || !bodyExpr.hasBodyArguments) {
        error = "glsl backend requires for body in do() { ... }";
        state.locals = savedLocals;
        return false;
      }
      out += indent + "  while (" + cond.code + ") {\n";
      if (!emitBlock(bodyExpr.bodyArguments, state, out, error, indent + "    ")) {
        state.locals = savedLocals;
        return false;
      }
      if (!emitStatement(stmt.args[2], state, out, error, indent + "    ")) {
        state.locals = savedLocals;
        return false;
      }
      out += indent + "  }\n";
      out += indent + "}\n";
      state.locals = savedLocals;
      return true;
    }
    ExprResult exprResult = emitExpr(stmt, state, error);
    if (!error.empty()) {
      return false;
    }
    out += indent + exprResult.code + ";\n";
    return true;
  }
  error = "glsl backend encountered unsupported statement";
  return false;
}

} // namespace

bool GlslEmitter::emitSource(const Program &program,
                             const std::string &entryPath,
                             std::string &out,
                             std::string &error) const {
  const Definition *entryDef = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == entryPath) {
      entryDef = &def;
      break;
    }
  }
  if (!entryDef) {
    error = "glsl backend requires entry definition " + entryPath;
    return false;
  }
  if (!entryDef->parameters.empty()) {
    error = "glsl backend requires entry definition to have no parameters";
    return false;
  }
  bool hasReturnTransform = false;
  bool returnsVoid = false;
  for (const auto &transform : entryDef->transforms) {
    if (transform.name != "return") {
      continue;
    }
    hasReturnTransform = true;
    returnsVoid = transform.templateArgs.size() == 1 && transform.templateArgs.front() == "void";
  }
  if (hasReturnTransform && !returnsVoid) {
    error = "glsl backend requires entry definition to return void";
    return false;
  }
  if (!hasReturnTransform && entryDef->returnExpr.has_value()) {
    error = "glsl backend requires entry definition to return void";
    return false;
  }
  EmitState state;
  std::string body;
  bool sawReturn = false;
  for (const auto &stmt : entryDef->statements) {
    if (stmt.kind == Expr::Kind::Call && !stmt.isBinding && stmt.name == "return") {
      if (sawReturn) {
        error = "glsl backend only supports a single return statement";
        return false;
      }
      sawReturn = true;
    }
    if (!emitStatement(stmt, state, body, error, "  ")) {
      return false;
    }
  }
  out.clear();
  out += "#version 450\n";
  if (state.needsInt64Ext) {
    out += "#extension GL_ARB_gpu_shader_int64 : require\n";
  }
  if (state.needsFp64Ext) {
    out += "#extension GL_ARB_gpu_shader_fp64 : require\n";
  }
  out += "void main() {\n";
  out += body;
  out += "}\n";
  return true;
}

} // namespace primec
