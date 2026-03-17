#include "IrLowererOperatorArithmeticHelpers.h"

#include "IrLowererHelpers.h"

#include <functional>

namespace primec::ir_lowerer {

namespace {

bool rewriteMathMultiplyCall(const Expr &expr,
                             const LocalMap &localsIn,
                             const InferStructExprPathWithLocalsFn &inferStructExprPath,
                             Expr &rewrittenExpr) {
  if (!isSimpleCallName(expr, "multiply") || expr.args.size() != 2) {
    return false;
  }
  const std::string leftType = inferStructExprPath(expr.args[0], localsIn);
  const std::string rightType = inferStructExprPath(expr.args[1], localsIn);
  if (leftType == "/std/math/Mat2" && rightType == "/std/math/Vec2") {
    rewrittenExpr = expr;
    rewrittenExpr.name = "/std/math/mat2_mul_vec2_internal";
    rewrittenExpr.namespacePrefix.clear();
    rewrittenExpr.isMethodCall = false;
    rewrittenExpr.isFieldAccess = false;
    return true;
  }
  if (leftType == "/std/math/Mat3" && rightType == "/std/math/Vec3") {
    rewrittenExpr = expr;
    rewrittenExpr.name = "/std/math/mat3_mul_vec3_internal";
    rewrittenExpr.namespacePrefix.clear();
    rewrittenExpr.isMethodCall = false;
    rewrittenExpr.isFieldAccess = false;
    return true;
  }
  if (leftType == "/std/math/Mat4" && rightType == "/std/math/Vec4") {
    rewrittenExpr = expr;
    rewrittenExpr.name = "/std/math/mat4_mul_vec4_internal";
    rewrittenExpr.namespacePrefix.clear();
    rewrittenExpr.isMethodCall = false;
    rewrittenExpr.isFieldAccess = false;
    return true;
  }
  if (leftType == "/std/math/Mat2" && rightType == "/std/math/Mat2") {
    rewrittenExpr = expr;
    rewrittenExpr.name = "/std/math/mat2_mul_mat2_internal";
    rewrittenExpr.namespacePrefix.clear();
    rewrittenExpr.isMethodCall = false;
    rewrittenExpr.isFieldAccess = false;
    return true;
  }
  if (leftType == "/std/math/Mat3" && rightType == "/std/math/Mat3") {
    rewrittenExpr = expr;
    rewrittenExpr.name = "/std/math/mat3_mul_mat3_internal";
    rewrittenExpr.namespacePrefix.clear();
    rewrittenExpr.isMethodCall = false;
    rewrittenExpr.isFieldAccess = false;
    return true;
  }
  if (leftType == "/std/math/Mat4" && rightType == "/std/math/Mat4") {
    rewrittenExpr = expr;
    rewrittenExpr.name = "/std/math/mat4_mul_mat4_internal";
    rewrittenExpr.namespacePrefix.clear();
    rewrittenExpr.isMethodCall = false;
    rewrittenExpr.isFieldAccess = false;
    return true;
  }
  if (leftType == "/std/math/Quat" && rightType == "/std/math/Quat") {
    rewrittenExpr = expr;
    rewrittenExpr.name = "/std/math/quat_multiply_internal";
    rewrittenExpr.namespacePrefix.clear();
    rewrittenExpr.isMethodCall = false;
    rewrittenExpr.isFieldAccess = false;
    return true;
  }
  if (leftType == "/std/math/Quat" && rightType == "/std/math/Vec3") {
    rewrittenExpr = expr;
    rewrittenExpr.name = "/std/math/quat_rotate_vec3_internal";
    rewrittenExpr.namespacePrefix.clear();
    rewrittenExpr.isMethodCall = false;
    rewrittenExpr.isFieldAccess = false;
    return true;
  }
  return false;
}

} // namespace

OperatorArithmeticEmitResult emitArithmeticOperatorExpr(const Expr &expr,
                                                        const LocalMap &localsIn,
                                                        const EmitExprWithLocalsFn &emitExpr,
                                                        const InferExprKindWithLocalsFn &inferExprKind,
                                                        const InferStructExprPathWithLocalsFn &inferStructExprPath,
                                                        const CombineNumericKindsFn &combineNumericKinds,
                                                        const EmitInstructionFn &emitInstruction,
                                                        std::string &error) {
  std::string builtin;
  if (!getBuiltinOperatorName(expr, builtin)) {
    return OperatorArithmeticEmitResult::NotHandled;
  }

  if (builtin == "negate") {
    if (expr.args.size() != 1) {
      error = "negate requires exactly one argument";
      return OperatorArithmeticEmitResult::Error;
    }
    if (!emitExpr(expr.args.front(), localsIn)) {
      return OperatorArithmeticEmitResult::Error;
    }
    LocalInfo::ValueKind kind = inferExprKind(expr.args.front(), localsIn);
    if (kind == LocalInfo::ValueKind::Bool || kind == LocalInfo::ValueKind::Unknown) {
      error = "negate requires numeric operand";
      return OperatorArithmeticEmitResult::Error;
    }
    if (kind == LocalInfo::ValueKind::UInt64) {
      error = "negate does not support unsigned operands";
      return OperatorArithmeticEmitResult::Error;
    }
    IrOpcode negOp = IrOpcode::NegI32;
    if (kind == LocalInfo::ValueKind::Int64) {
      negOp = IrOpcode::NegI64;
    } else if (kind == LocalInfo::ValueKind::Float64) {
      negOp = IrOpcode::NegF64;
    } else if (kind == LocalInfo::ValueKind::Float32) {
      negOp = IrOpcode::NegF32;
    }
    emitInstruction(negOp, 0);
    return OperatorArithmeticEmitResult::Handled;
  }

  if (expr.args.size() != 2) {
    error = builtin + " requires exactly two arguments";
    return OperatorArithmeticEmitResult::Error;
  }
  if (builtin == "multiply") {
    Expr rewrittenExpr;
    if (rewriteMathMultiplyCall(expr, localsIn, inferStructExprPath, rewrittenExpr)) {
      return emitExpr(rewrittenExpr, localsIn) ? OperatorArithmeticEmitResult::Handled
                                               : OperatorArithmeticEmitResult::Error;
    }
  }
  std::function<bool(const Expr &, const LocalMap &)> isPointerOperand;
  isPointerOperand = [&](const Expr &candidate, const LocalMap &localsRef) -> bool {
    if (candidate.kind == Expr::Kind::Name) {
      auto it = localsRef.find(candidate.name);
      return it != localsRef.end() && it->second.kind == LocalInfo::Kind::Pointer;
    }
    if (candidate.kind == Expr::Kind::Call && isSimpleCallName(candidate, "location")) {
      return true;
    }
    if (candidate.kind == Expr::Kind::Call) {
      std::string opName;
      if (getBuiltinOperatorName(candidate, opName) && (opName == "plus" || opName == "minus") &&
          candidate.args.size() == 2) {
        return isPointerOperand(candidate.args[0], localsRef) && !isPointerOperand(candidate.args[1], localsRef);
      }
    }
    return false;
  };
  bool leftPointer = false;
  bool rightPointer = false;
  if (builtin == "plus" || builtin == "minus") {
    leftPointer = isPointerOperand(expr.args[0], localsIn);
    rightPointer = isPointerOperand(expr.args[1], localsIn);
    if (leftPointer && rightPointer) {
      error = "pointer arithmetic does not support pointer + pointer";
      return OperatorArithmeticEmitResult::Error;
    }
    if (rightPointer) {
      error = "pointer arithmetic requires pointer on the left";
      return OperatorArithmeticEmitResult::Error;
    }
    if (leftPointer || rightPointer) {
      const Expr &offsetExpr = leftPointer ? expr.args[1] : expr.args[0];
      LocalInfo::ValueKind offsetKind = inferExprKind(offsetExpr, localsIn);
      if (offsetKind != LocalInfo::ValueKind::Int32 && offsetKind != LocalInfo::ValueKind::Int64 &&
          offsetKind != LocalInfo::ValueKind::UInt64) {
        error = "pointer arithmetic requires an integer offset";
        return OperatorArithmeticEmitResult::Error;
      }
    }
  }
  if (!emitExpr(expr.args[0], localsIn)) {
    return OperatorArithmeticEmitResult::Error;
  }
  if (!emitExpr(expr.args[1], localsIn)) {
    return OperatorArithmeticEmitResult::Error;
  }
  LocalInfo::ValueKind numericKind =
      combineNumericKinds(inferExprKind(expr.args[0], localsIn), inferExprKind(expr.args[1], localsIn));
  if (numericKind == LocalInfo::ValueKind::Unknown && !(leftPointer || rightPointer)) {
    error = "unsupported operand types for " + builtin;
    return OperatorArithmeticEmitResult::Error;
  }
  IrOpcode op = IrOpcode::AddI32;
  if (builtin == "plus") {
    if (leftPointer || rightPointer) {
      op = IrOpcode::AddI64;
    } else if (numericKind == LocalInfo::ValueKind::Float64) {
      op = IrOpcode::AddF64;
    } else if (numericKind == LocalInfo::ValueKind::Float32) {
      op = IrOpcode::AddF32;
    } else if (numericKind == LocalInfo::ValueKind::Int64 || numericKind == LocalInfo::ValueKind::UInt64) {
      op = IrOpcode::AddI64;
    } else {
      op = IrOpcode::AddI32;
    }
  } else if (builtin == "minus") {
    if (leftPointer || rightPointer) {
      op = IrOpcode::SubI64;
    } else if (numericKind == LocalInfo::ValueKind::Float64) {
      op = IrOpcode::SubF64;
    } else if (numericKind == LocalInfo::ValueKind::Float32) {
      op = IrOpcode::SubF32;
    } else if (numericKind == LocalInfo::ValueKind::Int64 || numericKind == LocalInfo::ValueKind::UInt64) {
      op = IrOpcode::SubI64;
    } else {
      op = IrOpcode::SubI32;
    }
  } else if (builtin == "multiply") {
    if (numericKind == LocalInfo::ValueKind::Float64) {
      op = IrOpcode::MulF64;
    } else if (numericKind == LocalInfo::ValueKind::Float32) {
      op = IrOpcode::MulF32;
    } else {
      op = (numericKind == LocalInfo::ValueKind::Int64 || numericKind == LocalInfo::ValueKind::UInt64)
               ? IrOpcode::MulI64
               : IrOpcode::MulI32;
    }
  } else if (builtin == "divide") {
    if (numericKind == LocalInfo::ValueKind::UInt64) {
      op = IrOpcode::DivU64;
    } else if (numericKind == LocalInfo::ValueKind::Float64) {
      op = IrOpcode::DivF64;
    } else if (numericKind == LocalInfo::ValueKind::Float32) {
      op = IrOpcode::DivF32;
    } else if (numericKind == LocalInfo::ValueKind::Int64) {
      op = IrOpcode::DivI64;
    } else {
      op = IrOpcode::DivI32;
    }
  }
  emitInstruction(op, 0);
  return OperatorArithmeticEmitResult::Handled;
}

} // namespace primec::ir_lowerer
