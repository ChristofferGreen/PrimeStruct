#include "IrLowererOperatorComparisonHelpers.h"

#include "IrLowererHelpers.h"

namespace primec::ir_lowerer {

OperatorComparisonEmitResult emitComparisonOperatorExpr(const Expr &expr,
                                                        const LocalMap &localsIn,
                                                        const EmitComparisonExprWithLocalsFn &emitExpr,
                                                        const InferComparisonExprKindWithLocalsFn &inferExprKind,
                                                        const ComparisonKindFn &comparisonKind,
                                                        const EmitComparisonToZeroFn &emitCompareToZero,
                                                        std::vector<IrInstruction> &instructions,
                                                        std::string &error) {
  auto inferConditionKind = [&](const Expr &candidate) {
    LocalInfo::ValueKind kind = inferExprKind(candidate, localsIn);
    if (kind != LocalInfo::ValueKind::Unknown) {
      return kind;
    }
    std::string comparisonBuiltin;
    if (getBuiltinComparisonName(candidate, comparisonBuiltin)) {
      return LocalInfo::ValueKind::Bool;
    }
    return kind;
  };

  std::string builtin;
  if (!getBuiltinComparisonName(expr, builtin)) {
    return OperatorComparisonEmitResult::NotHandled;
  }

  if (builtin == "not") {
    if (expr.args.size() != 1) {
      error = "not requires exactly one argument";
      return OperatorComparisonEmitResult::Error;
    }
    if (!emitExpr(expr.args.front(), localsIn)) {
      return OperatorComparisonEmitResult::Error;
    }
    LocalInfo::ValueKind kind = inferConditionKind(expr.args.front());
    if (!emitCompareToZero(kind, true)) {
      return OperatorComparisonEmitResult::Error;
    }
    return OperatorComparisonEmitResult::Handled;
  }

  if (builtin == "and") {
    if (expr.args.size() != 2) {
      error = "and requires exactly two arguments";
      return OperatorComparisonEmitResult::Error;
    }
    if (!emitExpr(expr.args[0], localsIn)) {
      return OperatorComparisonEmitResult::Error;
    }
    LocalInfo::ValueKind leftKind = inferConditionKind(expr.args[0]);
    if (!emitCompareToZero(leftKind, false)) {
      return OperatorComparisonEmitResult::Error;
    }
    size_t jumpFalse = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    if (!emitExpr(expr.args[1], localsIn)) {
      return OperatorComparisonEmitResult::Error;
    }
    LocalInfo::ValueKind rightKind = inferConditionKind(expr.args[1]);
    if (!emitCompareToZero(rightKind, false)) {
      return OperatorComparisonEmitResult::Error;
    }
    size_t jumpEnd = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});
    size_t falseIndex = instructions.size();
    instructions[jumpFalse].imm = static_cast<int32_t>(falseIndex);
    instructions.push_back({IrOpcode::PushI32, 0});
    size_t endIndex = instructions.size();
    instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
    return OperatorComparisonEmitResult::Handled;
  }

  if (builtin == "or") {
    if (expr.args.size() != 2) {
      error = "or requires exactly two arguments";
      return OperatorComparisonEmitResult::Error;
    }
    if (!emitExpr(expr.args[0], localsIn)) {
      return OperatorComparisonEmitResult::Error;
    }
    LocalInfo::ValueKind leftKind = inferConditionKind(expr.args[0]);
    if (!emitCompareToZero(leftKind, false)) {
      return OperatorComparisonEmitResult::Error;
    }
    size_t jumpEval = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    instructions.push_back({IrOpcode::PushI32, 1});
    size_t jumpEnd = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});
    size_t evalIndex = instructions.size();
    instructions[jumpEval].imm = static_cast<int32_t>(evalIndex);
    if (!emitExpr(expr.args[1], localsIn)) {
      return OperatorComparisonEmitResult::Error;
    }
    LocalInfo::ValueKind rightKind = inferConditionKind(expr.args[1]);
    if (!emitCompareToZero(rightKind, false)) {
      return OperatorComparisonEmitResult::Error;
    }
    size_t endIndex = instructions.size();
    instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
    return OperatorComparisonEmitResult::Handled;
  }

  if (expr.args.size() != 2) {
    error = builtin + " requires exactly two arguments";
    return OperatorComparisonEmitResult::Error;
  }
  LocalInfo::ValueKind leftKind = inferExprKind(expr.args[0], localsIn);
  LocalInfo::ValueKind rightKind = inferExprKind(expr.args[1], localsIn);
  if (leftKind == LocalInfo::ValueKind::String || rightKind == LocalInfo::ValueKind::String) {
    error = "native backend does not support string comparisons";
    return OperatorComparisonEmitResult::Error;
  }
  if (!emitExpr(expr.args[0], localsIn)) {
    return OperatorComparisonEmitResult::Error;
  }
  if (!emitExpr(expr.args[1], localsIn)) {
    return OperatorComparisonEmitResult::Error;
  }
  LocalInfo::ValueKind numericKind = comparisonKind(leftKind, rightKind);
  if (numericKind == LocalInfo::ValueKind::Unknown) {
    error = "unsupported operand types for " + builtin;
    return OperatorComparisonEmitResult::Error;
  }
  IrOpcode op = IrOpcode::CmpEqI32;
  if (builtin == "equal") {
    if (numericKind == LocalInfo::ValueKind::Float64) {
      op = IrOpcode::CmpEqF64;
    } else if (numericKind == LocalInfo::ValueKind::Float32) {
      op = IrOpcode::CmpEqF32;
    } else {
      op = (numericKind == LocalInfo::ValueKind::UInt64 || numericKind == LocalInfo::ValueKind::Int64)
               ? IrOpcode::CmpEqI64
               : IrOpcode::CmpEqI32;
    }
  } else if (builtin == "not_equal") {
    if (numericKind == LocalInfo::ValueKind::Float64) {
      op = IrOpcode::CmpNeF64;
    } else if (numericKind == LocalInfo::ValueKind::Float32) {
      op = IrOpcode::CmpNeF32;
    } else {
      op = (numericKind == LocalInfo::ValueKind::UInt64 || numericKind == LocalInfo::ValueKind::Int64)
               ? IrOpcode::CmpNeI64
               : IrOpcode::CmpNeI32;
    }
  } else if (builtin == "less_than") {
    if (numericKind == LocalInfo::ValueKind::Float64) {
      op = IrOpcode::CmpLtF64;
    } else if (numericKind == LocalInfo::ValueKind::Float32) {
      op = IrOpcode::CmpLtF32;
    } else if (numericKind == LocalInfo::ValueKind::UInt64) {
      op = IrOpcode::CmpLtU64;
    } else if (numericKind == LocalInfo::ValueKind::Int64) {
      op = IrOpcode::CmpLtI64;
    } else {
      op = IrOpcode::CmpLtI32;
    }
  } else if (builtin == "less_equal") {
    if (numericKind == LocalInfo::ValueKind::Float64) {
      op = IrOpcode::CmpLeF64;
    } else if (numericKind == LocalInfo::ValueKind::Float32) {
      op = IrOpcode::CmpLeF32;
    } else if (numericKind == LocalInfo::ValueKind::UInt64) {
      op = IrOpcode::CmpLeU64;
    } else if (numericKind == LocalInfo::ValueKind::Int64) {
      op = IrOpcode::CmpLeI64;
    } else {
      op = IrOpcode::CmpLeI32;
    }
  } else if (builtin == "greater_than") {
    if (numericKind == LocalInfo::ValueKind::Float64) {
      op = IrOpcode::CmpGtF64;
    } else if (numericKind == LocalInfo::ValueKind::Float32) {
      op = IrOpcode::CmpGtF32;
    } else if (numericKind == LocalInfo::ValueKind::UInt64) {
      op = IrOpcode::CmpGtU64;
    } else if (numericKind == LocalInfo::ValueKind::Int64) {
      op = IrOpcode::CmpGtI64;
    } else {
      op = IrOpcode::CmpGtI32;
    }
  } else if (builtin == "greater_equal") {
    if (numericKind == LocalInfo::ValueKind::Float64) {
      op = IrOpcode::CmpGeF64;
    } else if (numericKind == LocalInfo::ValueKind::Float32) {
      op = IrOpcode::CmpGeF32;
    } else if (numericKind == LocalInfo::ValueKind::UInt64) {
      op = IrOpcode::CmpGeU64;
    } else if (numericKind == LocalInfo::ValueKind::Int64) {
      op = IrOpcode::CmpGeI64;
    } else {
      op = IrOpcode::CmpGeI32;
    }
  }
  instructions.push_back({op, 0});
  return OperatorComparisonEmitResult::Handled;
}

} // namespace primec::ir_lowerer
