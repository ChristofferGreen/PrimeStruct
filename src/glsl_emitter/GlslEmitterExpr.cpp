#include "GlslEmitterInternal.h"

namespace primec::glsl_emitter {

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
  out.prelude = left.prelude;
  out.prelude += right.prelude;
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
    ExprResult out;
    out.type = GlslType::Bool;
    out.code = expr.boolValue ? "true" : "false";
    return out;
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
    if (it != state.locals.end()) {
      ExprResult out;
      out.type = it->second.type;
      out.code = expr.name;
      return out;
    }
    std::string constantName;
    if (getMathConstantName(expr, constantName)) {
      state.needsFp64Ext = true;
      if (constantName == "pi") {
        ExprResult out;
        out.type = GlslType::Double;
        out.code = "double(3.14159265358979323846)";
        return out;
      }
      if (constantName == "tau") {
        ExprResult out;
        out.type = GlslType::Double;
        out.code = "double(6.28318530717958647692)";
        return out;
      }
      ExprResult out;
      out.type = GlslType::Double;
      out.code = "double(2.71828182845904523536)";
      return out;
    }
    error = "glsl backend requires local binding for name: " + expr.name;
    return {};
  }
  if (expr.kind != Expr::Kind::Call) {
    error = "glsl backend encountered unsupported expression";
    return {};
  }
  const std::string name = normalizeName(expr);
  if (name == "block") {
    if (!expr.hasBodyArguments) {
      error = "glsl backend requires block() { ... }";
      return {};
    }
    if (!expr.args.empty() || !expr.templateArgs.empty() || hasNamedArguments(expr.argNames)) {
      error = "glsl backend requires block() { ... }";
      return {};
    }
    EmitState blockState = state;
    std::string blockBody;
    ExprResult blockResult;
    if (!emitValueBlock(expr, blockState, blockBody, error, "  ", blockResult)) {
      return {};
    }
    std::string typeName = glslTypeName(blockResult.type);
    if (typeName.empty()) {
      error = "glsl backend requires numeric or boolean block values";
      return {};
    }
    std::string tempName = allocTempName(state, "_ps_block_");
    ExprResult out;
    out.type = blockResult.type;
    out.code = tempName;
    out.prelude = typeName + " " + tempName + ";\n";
    out.prelude += "{\n";
    out.prelude += blockBody;
    out.prelude += "  " + tempName + " = " + blockResult.code + ";\n";
    out.prelude += "}\n";
    state.needsInt64Ext = state.needsInt64Ext || blockState.needsInt64Ext;
    state.needsFp64Ext = state.needsFp64Ext || blockState.needsFp64Ext;
    state.needsIntPow = state.needsIntPow || blockState.needsIntPow;
    state.needsUIntPow = state.needsUIntPow || blockState.needsUIntPow;
    state.needsInt64Pow = state.needsInt64Pow || blockState.needsInt64Pow;
    state.needsUInt64Pow = state.needsUInt64Pow || blockState.needsUInt64Pow;
    state.tempIndex = blockState.tempIndex;
    return out;
  }
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
    out.prelude = arg.prelude;
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
    ExprResult out;
    out.type = it->second.type;
    out.code = "(" + target.name + " = " + value.code + ")";
    out.prelude = value.prelude;
    return out;
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
    ExprResult out;
    out.type = it->second.type;
    out.code = "(" + std::string(op) + target.name + ")";
    return out;
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
    out.prelude = left.prelude;
    out.prelude += right.prelude;
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
    out.prelude = left.prelude;
    out.prelude += right.prelude;
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
    ExprResult out;
    out.type = GlslType::Bool;
    out.code = "(!" + arg.code + ")";
    out.prelude = arg.prelude;
    return out;
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
        ExprResult out;
        out.type = arg.type;
        out.code = "(" + arg.code + " == " + zeroLiteral + " ? " + zeroLiteral + " : " + oneLiteral + ")";
        out.prelude = arg.prelude;
        return out;
      }
      ExprResult out;
      out.type = arg.type;
      out.code = std::string(func) + "(" + arg.code + ")";
      out.prelude = arg.prelude;
      return out;
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
      ExprResult out;
      out.type = common;
      out.code = std::string(func) + "(" + left.code + ", " + right.code + ")";
      out.prelude = left.prelude;
      out.prelude += right.prelude;
      return out;
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
      ExprResult out;
      out.type = common;
      out.code = "clamp(" + value.code + ", " + minValue.code + ", " + maxValue.code + ")";
      out.prelude = value.prelude;
      out.prelude += minValue.prelude;
      out.prelude += maxValue.prelude;
      return out;
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
      ExprResult out;
      out.type = value.type;
      out.code = "clamp(" + value.code + ", " + zeroLiteral + ", " + oneLiteral + ")";
      out.prelude = value.prelude;
      return out;
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
      ExprResult out;
      out.type = common;
      out.code = "(" + start.code + " + (" + end.code + " - " + start.code + ") * " + t.code + ")";
      out.prelude = start.prelude;
      out.prelude += end.prelude;
      out.prelude += t.prelude;
      return out;
    }
    if (mathName == "pow") {
      if (expr.args.size() != 2) {
        error = "pow requires exactly two arguments";
        return {};
      }
      ExprResult base = emitExpr(expr.args[0], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult exponent = emitExpr(expr.args[1], state, error);
      if (!error.empty()) {
        return {};
      }
      GlslType common = combineNumericTypes(base.type, exponent.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      base = castExpr(base, common);
      exponent = castExpr(exponent, common);
      if (common == GlslType::Float || common == GlslType::Double) {
        ExprResult out;
        out.type = common;
        out.code = "pow(" + base.code + ", " + exponent.code + ")";
        out.prelude = base.prelude;
        out.prelude += exponent.prelude;
        return out;
      }
      if (!isIntegerType(common)) {
        error = "pow requires numeric arguments";
        return {};
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
      ExprResult out;
      out.type = common;
      out.code = funcName + "(" + base.code + ", " + exponent.code + ")";
      out.prelude = base.prelude;
      out.prelude += exponent.prelude;
      return out;
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
      ExprResult out;
      out.type = arg.type;
      out.code = "pow(" + arg.code + ", " + oneThird + ")";
      out.prelude = arg.prelude;
      return out;
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
      ExprResult out;
      out.type = arg.type;
      out.code = "(log(" + arg.code + ") / log(" + tenLiteral + "))";
      out.prelude = arg.prelude;
      return out;
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
      ExprResult out;
      out.type = common;
      out.code = "atan(" + y.code + ", " + x.code + ")";
      out.prelude = y.prelude;
      out.prelude += x.prelude;
      return out;
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
      ExprResult out;
      out.type = common;
      out.code = "fma(" + a.code + ", " + b.code + ", " + c.code + ")";
      out.prelude = a.prelude;
      out.prelude += b.prelude;
      out.prelude += c.prelude;
      return out;
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
      ExprResult out;
      out.type = common;
      out.code = "sqrt(" + a.code + " * " + a.code + " + " + b.code + " * " + b.code + ")";
      out.prelude = a.prelude;
      out.prelude += b.prelude;
      return out;
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
      ExprResult out;
      out.type = common;
      out.code = "(abs(" + mag.code + ") * sign(" + sign.code + "))";
      out.prelude = mag.prelude;
      out.prelude += sign.prelude;
      return out;
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
      ExprResult out;
      out.type = GlslType::Bool;
      out.code = func + "(" + arg.code + ")";
      out.prelude = arg.prelude;
      return out;
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

} // namespace primec::glsl_emitter
