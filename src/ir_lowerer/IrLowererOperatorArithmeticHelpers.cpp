#include "IrLowererOperatorArithmeticHelpers.h"

#include "IrLowererHelpers.h"

#include <functional>

namespace primec::ir_lowerer {

namespace {

std::string matrixHelperBaseForPath(const std::string &typePath) {
  if (typePath == "/std/math/Mat2") {
    return "/std/math/mat2";
  }
  if (typePath == "/std/math/Mat3") {
    return "/std/math/mat3";
  }
  if (typePath == "/std/math/Mat4") {
    return "/std/math/mat4";
  }
  return "";
}

std::string quaternionHelperBaseForPath(const std::string &typePath) {
  if (typePath == "/std/math/Quat") {
    return "/std/math/quat";
  }
  return "";
}

bool isNumericScalarKind(LocalInfo::ValueKind kind) {
  return kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Int64 ||
         kind == LocalInfo::ValueKind::UInt64 || kind == LocalInfo::ValueKind::Float32 ||
         kind == LocalInfo::ValueKind::Float64;
}

void rewriteHelperCall(const Expr &expr, const std::string &helperPath, Expr &rewrittenExpr) {
  rewrittenExpr = expr;
  rewrittenExpr.name = helperPath;
  rewrittenExpr.namespacePrefix.clear();
  rewrittenExpr.isMethodCall = false;
  rewrittenExpr.isFieldAccess = false;
}

bool rewriteMathArithmeticCall(const Expr &expr,
                               const LocalMap &localsIn,
                               const InferExprKindWithLocalsFn &inferExprKind,
                               const InferStructExprPathWithLocalsFn &inferStructExprPath,
                               Expr &rewrittenExpr) {
  std::string builtin;
  if (!getBuiltinOperatorName(expr, builtin) || expr.args.size() != 2) {
    return false;
  }
  const std::string leftType = inferStructExprPath(expr.args[0], localsIn);
  const std::string rightType = inferStructExprPath(expr.args[1], localsIn);
  const std::string resultType = inferStructExprPath(expr, localsIn);
  const std::string leftMatrixBase = matrixHelperBaseForPath(leftType);
  const std::string rightMatrixBase = matrixHelperBaseForPath(rightType);
  const std::string leftQuaternionBase = quaternionHelperBaseForPath(leftType);
  const std::string rightQuaternionBase = quaternionHelperBaseForPath(rightType);
  const LocalInfo::ValueKind leftKind = inferExprKind(expr.args[0], localsIn);
  const LocalInfo::ValueKind rightKind = inferExprKind(expr.args[1], localsIn);
  if (builtin == "plus" && !leftMatrixBase.empty() && leftType == rightType) {
    rewriteHelperCall(expr, leftMatrixBase + "_add_internal", rewrittenExpr);
    return true;
  }
  if (builtin == "plus" && !leftQuaternionBase.empty() && leftType == rightType) {
    rewriteHelperCall(expr, leftQuaternionBase + "_add_internal", rewrittenExpr);
    return true;
  }
  if (builtin == "minus" && !leftMatrixBase.empty() && leftType == rightType) {
    rewriteHelperCall(expr, leftMatrixBase + "_sub_internal", rewrittenExpr);
    return true;
  }
  if (builtin == "minus" && !leftQuaternionBase.empty() && leftType == rightType) {
    rewriteHelperCall(expr, leftQuaternionBase + "_sub_internal", rewrittenExpr);
    return true;
  }
  if (builtin == "multiply" && !leftMatrixBase.empty() && isNumericScalarKind(rightKind)) {
    rewriteHelperCall(expr, leftMatrixBase + "_scale_internal", rewrittenExpr);
    return true;
  }
  if (builtin == "multiply" && !leftQuaternionBase.empty() && isNumericScalarKind(rightKind)) {
    rewriteHelperCall(expr, leftQuaternionBase + "_scale_internal", rewrittenExpr);
    return true;
  }
  if (builtin == "multiply" && isNumericScalarKind(leftKind) && !rightMatrixBase.empty()) {
    rewriteHelperCall(expr, rightMatrixBase + "_scale_left_internal", rewrittenExpr);
    return true;
  }
  if (builtin == "multiply" && isNumericScalarKind(leftKind) && !rightQuaternionBase.empty()) {
    rewriteHelperCall(expr, rightQuaternionBase + "_scale_left_internal", rewrittenExpr);
    return true;
  }
  if (builtin == "divide" && !leftMatrixBase.empty() && isNumericScalarKind(rightKind)) {
    rewriteHelperCall(expr, leftMatrixBase + "_div_scalar_internal", rewrittenExpr);
    return true;
  }
  if (builtin == "divide" && !leftQuaternionBase.empty() && isNumericScalarKind(rightKind)) {
    rewriteHelperCall(expr, leftQuaternionBase + "_div_scalar_internal", rewrittenExpr);
    return true;
  }
  if (leftType == "/std/math/Mat2" && rightType == "/std/math/Vec2") {
    rewriteHelperCall(expr, "/std/math/mat2_mul_vec2_internal", rewrittenExpr);
    return true;
  }
  if (leftType == "/std/math/Mat3" && rightType == "/std/math/Vec3") {
    rewriteHelperCall(expr, "/std/math/mat3_mul_vec3_internal", rewrittenExpr);
    return true;
  }
  if (leftType == "/std/math/Mat4" && rightType == "/std/math/Vec4") {
    rewriteHelperCall(expr, "/std/math/mat4_mul_vec4_internal", rewrittenExpr);
    return true;
  }
  if (leftType == "/std/math/Mat2" && rightType == "/std/math/Mat2") {
    rewriteHelperCall(expr, "/std/math/mat2_mul_mat2_internal", rewrittenExpr);
    return true;
  }
  if (leftType == "/std/math/Mat3" && rightType == "/std/math/Mat3") {
    rewriteHelperCall(expr, "/std/math/mat3_mul_mat3_internal", rewrittenExpr);
    return true;
  }
  if (leftType == "/std/math/Mat4" && rightType == "/std/math/Mat4") {
    rewriteHelperCall(expr, "/std/math/mat4_mul_mat4_internal", rewrittenExpr);
    return true;
  }
  if (leftType == "/std/math/Quat" && rightType == "/std/math/Quat") {
    rewriteHelperCall(expr, "/std/math/quat_multiply_internal", rewrittenExpr);
    return true;
  }
  if (leftType == "/std/math/Quat" && rightType == "/std/math/Vec3") {
    rewriteHelperCall(expr, "/std/math/quat_rotate_vec3_internal", rewrittenExpr);
    return true;
  }
  // Semantic-product call targets can preserve builtin names (for example
  // "/multiply") while still publishing a concrete struct return type. Use
  // that return type to recover struct math rewrites when operand struct
  // inference is incomplete.
  if (builtin == "multiply" && leftType == "/std/math/Mat2" && resultType == "/std/math/Vec2") {
    rewriteHelperCall(expr, "/std/math/mat2_mul_vec2_internal", rewrittenExpr);
    return true;
  }
  if (builtin == "multiply" && leftType == "/std/math/Mat3" && resultType == "/std/math/Vec3") {
    rewriteHelperCall(expr, "/std/math/mat3_mul_vec3_internal", rewrittenExpr);
    return true;
  }
  if (builtin == "multiply" && leftType == "/std/math/Mat4" && resultType == "/std/math/Vec4") {
    rewriteHelperCall(expr, "/std/math/mat4_mul_vec4_internal", rewrittenExpr);
    return true;
  }
  if (builtin == "multiply" && leftType == "/std/math/Mat2" && resultType == "/std/math/Mat2") {
    rewriteHelperCall(expr, "/std/math/mat2_mul_mat2_internal", rewrittenExpr);
    return true;
  }
  if (builtin == "multiply" && leftType == "/std/math/Mat3" && resultType == "/std/math/Mat3") {
    rewriteHelperCall(expr, "/std/math/mat3_mul_mat3_internal", rewrittenExpr);
    return true;
  }
  if (builtin == "multiply" && leftType == "/std/math/Mat4" && resultType == "/std/math/Mat4") {
    rewriteHelperCall(expr, "/std/math/mat4_mul_mat4_internal", rewrittenExpr);
    return true;
  }
  if (builtin == "multiply" && leftType == "/std/math/Quat" && resultType == "/std/math/Quat") {
    rewriteHelperCall(expr, "/std/math/quat_multiply_internal", rewrittenExpr);
    return true;
  }
  if (builtin == "multiply" && leftType == "/std/math/Quat" && resultType == "/std/math/Vec3") {
    rewriteHelperCall(expr, "/std/math/quat_rotate_vec3_internal", rewrittenExpr);
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
  Expr rewrittenExpr;
  if (rewriteMathArithmeticCall(expr, localsIn, inferExprKind, inferStructExprPath, rewrittenExpr)) {
    return emitExpr(rewrittenExpr, localsIn) ? OperatorArithmeticEmitResult::Handled
                                             : OperatorArithmeticEmitResult::Error;
  }
  std::function<bool(const Expr &, const LocalMap &)> isPointerOperand;
  isPointerOperand = [&](const Expr &candidate, const LocalMap &localsRef) -> bool {
    if (candidate.kind == Expr::Kind::Name) {
      auto it = localsRef.find(candidate.name);
      return it != localsRef.end() &&
             (it->second.kind == LocalInfo::Kind::Pointer ||
              it->second.kind == LocalInfo::Kind::Reference);
    }
    if (candidate.kind == Expr::Kind::Call && isSimpleCallName(candidate, "location")) {
      return true;
    }
    if (candidate.kind == Expr::Kind::Call) {
      std::string memoryBuiltinName;
      if (getBuiltinMemoryName(candidate, memoryBuiltinName)) {
        if (memoryBuiltinName == "alloc" && candidate.templateArgs.size() == 1) {
          return true;
        }
        if (memoryBuiltinName == "realloc" && candidate.args.size() == 2) {
          return isPointerOperand(candidate.args.front(), localsRef);
        }
        if (memoryBuiltinName == "at" && candidate.args.size() == 3) {
          return isPointerOperand(candidate.args.front(), localsRef);
        }
        if (memoryBuiltinName == "at_unsafe" && candidate.args.size() == 2) {
          return isPointerOperand(candidate.args.front(), localsRef);
        }
      }
      std::string opName;
      if (getBuiltinOperatorName(candidate, opName) && (opName == "plus" || opName == "minus") &&
          candidate.args.size() == 2) {
        return isPointerOperand(candidate.args[0], localsRef) && !isPointerOperand(candidate.args[1], localsRef);
      }
    }
    return false;
  };
  auto isScalarReferenceOffsetOperand = [&](const Expr &candidate, const LocalMap &localsRef) -> bool {
    if (candidate.kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsRef.find(candidate.name);
    if (it == localsRef.end()) {
      return false;
    }
    const LocalInfo &info = it->second;
    if (info.kind != LocalInfo::Kind::Reference) {
      return false;
    }
    if (info.referenceToArray || info.referenceToVector || info.referenceToBuffer || info.referenceToMap ||
        info.isFileHandle || info.isResult || info.isUninitializedStorage || info.targetsUninitializedStorage ||
        info.isSoaVector || !info.structTypeName.empty()) {
      return false;
    }
    return info.valueKind == LocalInfo::ValueKind::Int32 || info.valueKind == LocalInfo::ValueKind::Int64 ||
           info.valueKind == LocalInfo::ValueKind::UInt64;
  };
  auto emitPointerOperand = [&](const Expr &candidate, const LocalMap &localsRef) -> bool {
    if (candidate.kind == Expr::Kind::Name) {
      auto it = localsRef.find(candidate.name);
      if (it != localsRef.end() &&
          (it->second.kind == LocalInfo::Kind::Pointer ||
           it->second.kind == LocalInfo::Kind::Reference)) {
        emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
        return true;
      }
    }
    return emitExpr(candidate, localsRef);
  };
  bool leftPointer = false;
  bool rightPointer = false;
  if (builtin == "plus" || builtin == "minus") {
    leftPointer = isPointerOperand(expr.args[0], localsIn);
    rightPointer = isPointerOperand(expr.args[1], localsIn);
    if (rightPointer && isScalarReferenceOffsetOperand(expr.args[1], localsIn)) {
      rightPointer = false;
    }
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
  if ((leftPointer || rightPointer) ? !emitPointerOperand(expr.args[0], localsIn)
                                    : !emitExpr(expr.args[0], localsIn)) {
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
