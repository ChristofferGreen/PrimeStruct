#include "GlslEmitterExprInternal.h"

namespace primec::glsl_emitter {

bool tryEmitMathBuiltinCall(const Expr &expr,
                            const std::string &mathName,
                            EmitState &state,
                            std::string &error,
                            ExprResult &out) {
  auto emitUnaryMath = [&](const char *func, bool requireFloat) -> bool {
    if (expr.args.size() != 1) {
      error = mathName + " requires exactly one argument";
      return false;
    }
    ExprResult arg = emitExpr(expr.args.front(), state, error);
    if (!error.empty()) {
      return false;
    }
    if (requireFloat && arg.type != GlslType::Float && arg.type != GlslType::Double) {
      error = mathName + " requires float argument";
      return false;
    }
    if (mathName == "abs" && (arg.type == GlslType::UInt || arg.type == GlslType::UInt64)) {
      out = arg;
      return true;
    }
    if (mathName == "sign" && (arg.type == GlslType::UInt || arg.type == GlslType::UInt64)) {
      std::string zeroLiteral = literalForType(arg.type, 0);
      std::string oneLiteral = literalForType(arg.type, 1);
      out.type = arg.type;
      out.code = "(" + arg.code + " == " + zeroLiteral + " ? " + zeroLiteral + " : " + oneLiteral + ")";
      out.prelude = arg.prelude;
      return true;
    }
    out.type = arg.type;
    out.code = std::string(func) + "(" + arg.code + ")";
    out.prelude = arg.prelude;
    return true;
  };

  auto emitBinaryMath = [&](const char *func, bool requireFloat) -> bool {
    if (expr.args.size() != 2) {
      error = mathName + " requires exactly two arguments";
      return false;
    }
    ExprResult left = emitExpr(expr.args[0], state, error);
    if (!error.empty()) {
      return false;
    }
    ExprResult right = emitExpr(expr.args[1], state, error);
    if (!error.empty()) {
      return false;
    }
    GlslType common = combineNumericTypes(left.type, right.type, error);
    if (common == GlslType::Unknown) {
      return false;
    }
    if (requireFloat && common != GlslType::Float && common != GlslType::Double) {
      error = mathName + " requires float arguments";
      return false;
    }
    left = castExpr(left, common);
    right = castExpr(right, common);
    out.type = common;
    out.code = std::string(func) + "(" + left.code + ", " + right.code + ")";
    out.prelude = left.prelude;
    out.prelude += right.prelude;
    return true;
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
      return false;
    }
    ExprResult value = emitExpr(expr.args[0], state, error);
    if (!error.empty()) {
      return false;
    }
    ExprResult minValue = emitExpr(expr.args[1], state, error);
    if (!error.empty()) {
      return false;
    }
    ExprResult maxValue = emitExpr(expr.args[2], state, error);
    if (!error.empty()) {
      return false;
    }
    GlslType common = combineNumericTypes(value.type, minValue.type, error);
    if (common == GlslType::Unknown) {
      return false;
    }
    common = combineNumericTypes(common, maxValue.type, error);
    if (common == GlslType::Unknown) {
      return false;
    }
    value = castExpr(value, common);
    minValue = castExpr(minValue, common);
    maxValue = castExpr(maxValue, common);
    out.type = common;
    out.code = "clamp(" + value.code + ", " + minValue.code + ", " + maxValue.code + ")";
    out.prelude = value.prelude;
    out.prelude += minValue.prelude;
    out.prelude += maxValue.prelude;
    return true;
  }
  if (mathName == "saturate") {
    if (expr.args.size() != 1) {
      error = "saturate requires exactly one argument";
      return false;
    }
    ExprResult value = emitExpr(expr.args[0], state, error);
    if (!error.empty()) {
      return false;
    }
    if (!isNumericType(value.type)) {
      error = "saturate requires numeric argument";
      return false;
    }
    std::string zeroLiteral = literalForType(value.type, 0);
    std::string oneLiteral = literalForType(value.type, 1);
    out.type = value.type;
    out.code = "clamp(" + value.code + ", " + zeroLiteral + ", " + oneLiteral + ")";
    out.prelude = value.prelude;
    return true;
  }
  if (mathName == "lerp") {
    if (expr.args.size() != 3) {
      error = "lerp requires exactly three arguments";
      return false;
    }
    ExprResult start = emitExpr(expr.args[0], state, error);
    if (!error.empty()) {
      return false;
    }
    ExprResult end = emitExpr(expr.args[1], state, error);
    if (!error.empty()) {
      return false;
    }
    ExprResult t = emitExpr(expr.args[2], state, error);
    if (!error.empty()) {
      return false;
    }
    GlslType common = combineNumericTypes(start.type, end.type, error);
    if (common == GlslType::Unknown) {
      return false;
    }
    common = combineNumericTypes(common, t.type, error);
    if (common == GlslType::Unknown) {
      return false;
    }
    start = castExpr(start, common);
    end = castExpr(end, common);
    t = castExpr(t, common);
    out.type = common;
    out.code = "(" + start.code + " + (" + end.code + " - " + start.code + ") * " + t.code + ")";
    out.prelude = start.prelude;
    out.prelude += end.prelude;
    out.prelude += t.prelude;
    return true;
  }
  if (mathName == "pow") {
    if (expr.args.size() != 2) {
      error = "pow requires exactly two arguments";
      return false;
    }
    ExprResult base = emitExpr(expr.args[0], state, error);
    if (!error.empty()) {
      return false;
    }
    ExprResult exponent = emitExpr(expr.args[1], state, error);
    if (!error.empty()) {
      return false;
    }
    GlslType common = combineNumericTypes(base.type, exponent.type, error);
    if (common == GlslType::Unknown) {
      return false;
    }
    base = castExpr(base, common);
    exponent = castExpr(exponent, common);
    if (common == GlslType::Float || common == GlslType::Double) {
      out.type = common;
      out.code = "pow(" + base.code + ", " + exponent.code + ")";
      out.prelude = base.prelude;
      out.prelude += exponent.prelude;
      return true;
    }
    if (!isIntegerType(common)) {
      error = "pow requires numeric arguments";
      return false;
    }
    noteTypeExtensions(common, state);
    std::string funcName = powHelperName(common);
    if (common == GlslType::Int) {
      state.needsIntPow = true;
    } else if (common == GlslType::UInt) {
      state.needsUIntPow = true;
    } else if (common == GlslType::Int64) {
      state.needsInt64Pow = true;
    } else if (common == GlslType::UInt64) {
      state.needsUInt64Pow = true;
    }
    out.type = common;
    out.code = funcName + "(" + base.code + ", " + exponent.code + ")";
    out.prelude = base.prelude;
    out.prelude += exponent.prelude;
    return true;
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
      return false;
    }
    ExprResult arg = emitExpr(expr.args.front(), state, error);
    if (!error.empty()) {
      return false;
    }
    if (arg.type != GlslType::Float && arg.type != GlslType::Double) {
      error = "cbrt requires float argument";
      return false;
    }
    std::string oneThird = glslTypeName(arg.type) + "(1.0/3.0)";
    out.type = arg.type;
    out.code = "pow(" + arg.code + ", " + oneThird + ")";
    out.prelude = arg.prelude;
    return true;
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
      return false;
    }
    ExprResult arg = emitExpr(expr.args.front(), state, error);
    if (!error.empty()) {
      return false;
    }
    if (arg.type != GlslType::Float && arg.type != GlslType::Double) {
      error = "log10 requires float argument";
      return false;
    }
    std::string tenLiteral = literalForType(arg.type, 10);
    out.type = arg.type;
    out.code = "(log(" + arg.code + ") / log(" + tenLiteral + "))";
    out.prelude = arg.prelude;
    return true;
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
      return false;
    }
    ExprResult y = emitExpr(expr.args[0], state, error);
    if (!error.empty()) {
      return false;
    }
    ExprResult x = emitExpr(expr.args[1], state, error);
    if (!error.empty()) {
      return false;
    }
    GlslType common = combineNumericTypes(y.type, x.type, error);
    if (common == GlslType::Unknown) {
      return false;
    }
    if (common != GlslType::Float && common != GlslType::Double) {
      error = "atan2 requires float arguments";
      return false;
    }
    y = castExpr(y, common);
    x = castExpr(x, common);
    out.type = common;
    out.code = "atan(" + y.code + ", " + x.code + ")";
    out.prelude = y.prelude;
    out.prelude += x.prelude;
    return true;
  }
  if (mathName == "fma") {
    if (expr.args.size() != 3) {
      error = "fma requires exactly three arguments";
      return false;
    }
    ExprResult a = emitExpr(expr.args[0], state, error);
    if (!error.empty()) {
      return false;
    }
    ExprResult b = emitExpr(expr.args[1], state, error);
    if (!error.empty()) {
      return false;
    }
    ExprResult c = emitExpr(expr.args[2], state, error);
    if (!error.empty()) {
      return false;
    }
    GlslType common = combineNumericTypes(a.type, b.type, error);
    if (common == GlslType::Unknown) {
      return false;
    }
    common = combineNumericTypes(common, c.type, error);
    if (common == GlslType::Unknown) {
      return false;
    }
    if (common != GlslType::Float && common != GlslType::Double) {
      error = "fma requires float arguments";
      return false;
    }
    a = castExpr(a, common);
    b = castExpr(b, common);
    c = castExpr(c, common);
    out.type = common;
    out.code = "fma(" + a.code + ", " + b.code + ", " + c.code + ")";
    out.prelude = a.prelude;
    out.prelude += b.prelude;
    out.prelude += c.prelude;
    return true;
  }
  if (mathName == "hypot") {
    if (expr.args.size() != 2) {
      error = "hypot requires exactly two arguments";
      return false;
    }
    ExprResult a = emitExpr(expr.args[0], state, error);
    if (!error.empty()) {
      return false;
    }
    ExprResult b = emitExpr(expr.args[1], state, error);
    if (!error.empty()) {
      return false;
    }
    GlslType common = combineNumericTypes(a.type, b.type, error);
    if (common == GlslType::Unknown) {
      return false;
    }
    if (common != GlslType::Float && common != GlslType::Double) {
      error = "hypot requires float arguments";
      return false;
    }
    a = castExpr(a, common);
    b = castExpr(b, common);
    out.type = common;
    out.code = "sqrt(" + a.code + " * " + a.code + " + " + b.code + " * " + b.code + ")";
    out.prelude = a.prelude;
    out.prelude += b.prelude;
    return true;
  }
  if (mathName == "copysign") {
    if (expr.args.size() != 2) {
      error = "copysign requires exactly two arguments";
      return false;
    }
    ExprResult mag = emitExpr(expr.args[0], state, error);
    if (!error.empty()) {
      return false;
    }
    ExprResult sign = emitExpr(expr.args[1], state, error);
    if (!error.empty()) {
      return false;
    }
    GlslType common = combineNumericTypes(mag.type, sign.type, error);
    if (common == GlslType::Unknown) {
      return false;
    }
    if (common != GlslType::Float && common != GlslType::Double) {
      error = "copysign requires float arguments";
      return false;
    }
    mag = castExpr(mag, common);
    sign = castExpr(sign, common);
    out.type = common;
    out.code = "(abs(" + mag.code + ") * sign(" + sign.code + "))";
    out.prelude = mag.prelude;
    out.prelude += sign.prelude;
    return true;
  }
  if (mathName == "is_nan" || mathName == "is_inf" || mathName == "is_finite") {
    if (expr.args.size() != 1) {
      error = mathName + " requires exactly one argument";
      return false;
    }
    ExprResult arg = emitExpr(expr.args.front(), state, error);
    if (!error.empty()) {
      return false;
    }
    if (arg.type != GlslType::Float && arg.type != GlslType::Double) {
      error = mathName + " requires float argument";
      return false;
    }
    std::string func = mathName == "is_nan" ? "isnan" : (mathName == "is_inf" ? "isinf" : "isfinite");
    out.type = GlslType::Bool;
    out.code = func + "(" + arg.code + ")";
    out.prelude = arg.prelude;
    return true;
  }
  error = "glsl backend does not support math builtin: " + mathName;
  return false;
}

} // namespace primec::glsl_emitter
