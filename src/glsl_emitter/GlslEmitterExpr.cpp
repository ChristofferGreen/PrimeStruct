#include "GlslEmitterInternal.h"
#include "GlslEmitterExprInternal.h"

namespace primec::glsl_emitter {

ExprResult emitExpr(const Expr &expr, EmitState &state, std::string &error);

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
  if (expr.isFieldAccess) {
    if (expr.args.size() != 1) {
      error = "glsl backend requires field access receiver";
      return {};
    }
    ExprResult receiver = emitExpr(expr.args.front(), state, error);
    if (!error.empty()) {
      return {};
    }
    if (isQuaternionType(receiver.type)) {
      int component = 0;
      if (!vectorFieldComponentIndex(expr.name, 4, component)) {
        error = "glsl backend does not support quaternion field: " + expr.name;
        return {};
      }
      ExprResult out;
      out.type = GlslType::Float;
      out.code = "(" + receiver.code + ").";
      out.code.push_back("xyzw"[component]);
      out.prelude = receiver.prelude;
      return out;
    }
    const int vectorSize = vectorDimension(receiver.type);
    if (vectorSize != 0) {
      int component = 0;
      if (!vectorFieldComponentIndex(expr.name, vectorSize, component)) {
        error = "glsl backend does not support vector field: " + expr.name;
        return {};
      }
      ExprResult out;
      out.type = GlslType::Float;
      out.code = "(" + receiver.code + ").";
      out.code.push_back("xyzw"[component]);
      out.prelude = receiver.prelude;
      return out;
    }
    const int dimension = matrixDimension(receiver.type);
    if (dimension == 0) {
      error = "glsl backend only supports vector, quaternion, or matrix field access";
      return {};
    }
    int row = 0;
    int column = 0;
    if (!matrixFieldRowColumn(expr.name, dimension, row, column)) {
      error = "glsl backend does not support matrix field: " + expr.name;
      return {};
    }
    ExprResult out;
    out.type = GlslType::Float;
    out.code = "(" + receiver.code + ")[" + std::to_string(column) + "][" + std::to_string(row) + "]";
    out.prelude = receiver.prelude;
    return out;
  }
  const std::string name = normalizeName(expr);
  const std::string leafName = callLeafName(name);
  ExprResult quaternionHelperOut;
  if (emitQuaternionHelperCall(expr, leafName, state, error, quaternionHelperOut)) {
    return quaternionHelperOut;
  }
  if (!error.empty()) {
    return {};
  }
  if (name == "Vec2") {
    ExprResult out;
    if (!emitVectorConstructor(expr, name, GlslType::Vec2, state, error, out)) {
      return {};
    }
    return out;
  }
  if (name == "Vec3") {
    ExprResult out;
    if (!emitVectorConstructor(expr, name, GlslType::Vec3, state, error, out)) {
      return {};
    }
    return out;
  }
  if (name == "Vec4") {
    ExprResult out;
    if (!emitVectorConstructor(expr, name, GlslType::Vec4, state, error, out)) {
      return {};
    }
    return out;
  }
  if (name == "Mat2") {
    ExprResult out;
    if (!emitMatrixConstructor(expr, name, GlslType::Mat2, state, error, out)) {
      return {};
    }
    return out;
  }
  if (name == "Mat3") {
    ExprResult out;
    if (!emitMatrixConstructor(expr, name, GlslType::Mat3, state, error, out)) {
      return {};
    }
    return out;
  }
  if (name == "Mat4") {
    ExprResult out;
    if (!emitMatrixConstructor(expr, name, GlslType::Mat4, state, error, out)) {
      return {};
    }
    return out;
  }
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
      error = "glsl backend requires GLSL-supported block values";
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
    ExprResult quaternionOut;
    if (emitQuaternionArithmetic(expr, name, state, error, quaternionOut)) {
      return quaternionOut;
    }
    if (!error.empty()) {
      return {};
    }
    ExprResult vectorOut;
    if (emitVectorArithmetic(expr, name, state, error, vectorOut)) {
      return vectorOut;
    }
    if (!error.empty()) {
      return {};
    }
    ExprResult matrixOut;
    if (emitMatrixArithmetic(expr, name, state, error, matrixOut)) {
      return matrixOut;
    }
    if (!error.empty()) {
      return {};
    }
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
    ExprResult out;
    if (tryEmitMathBuiltinCall(expr, mathName, state, error, out)) {
      return out;
    }
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
