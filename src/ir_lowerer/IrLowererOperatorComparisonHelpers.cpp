#include "IrLowererOperatorComparisonHelpers.h"

#include "IrLowererHelpers.h"

namespace primec::ir_lowerer {

namespace {

// Lowers equal(strA, strB)/not_equal(strA, strB) to the same byte-by-byte
// comparison the stdlib /string/equal helper (stdlib/std/collections/
// equality.prime) implements, so a plain equal()/not_equal() call on two
// strings works everywhere a real .equal() method call already does,
// without requiring an import (this must stay a core, always-available
// capability, matching every other builtin comparison). Each operand is
// evaluated exactly once into a fresh local (mirroring how /string/equal's
// own self/other parameters are each read multiple times without
// re-evaluating the caller's argument expression), then count()/at() are
// re-emitted through the normal `emitExpr` path against synthetic Name
// exprs bound to those locals - this reuses count()/at()'s existing,
// already-correct handling of every string representation (literal,
// parameter, runtime-computed) instead of re-deriving it here.
bool emitStringEqualityComparison(const Expr &expr,
                                  const LocalMap &localsIn,
                                  const EmitComparisonExprWithLocalsFn &emitExpr,
                                  const ComparisonAllocTempLocalFn &allocTempLocal,
                                  const EmitComparisonToZeroFn &emitCompareToZero,
                                  bool negate,
                                  std::vector<IrInstruction> &instructions) {
  if (!emitExpr(expr.args[0], localsIn)) {
    return false;
  }
  const int32_t selfLocal = allocTempLocal();
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(selfLocal)});

  if (!emitExpr(expr.args[1], localsIn)) {
    return false;
  }
  const int32_t otherLocal = allocTempLocal();
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(otherLocal)});

  LocalMap locals = localsIn;
  LocalInfo stringLocalInfo;
  stringLocalInfo.valueKind = LocalInfo::ValueKind::String;
  stringLocalInfo.stringSource = LocalInfo::StringSource::RuntimeIndex;
  stringLocalInfo.stringIndex = -1;

  const std::string selfName = "__ir_string_eq_self";
  LocalInfo selfInfo = stringLocalInfo;
  selfInfo.index = selfLocal;
  locals[selfName] = selfInfo;

  const std::string otherName = "__ir_string_eq_other";
  LocalInfo otherInfo = stringLocalInfo;
  otherInfo.index = otherLocal;
  locals[otherName] = otherInfo;

  Expr selfNameExpr;
  selfNameExpr.kind = Expr::Kind::Name;
  selfNameExpr.name = selfName;
  Expr otherNameExpr;
  otherNameExpr.kind = Expr::Kind::Name;
  otherNameExpr.name = otherName;

  auto makeCountExpr = [](const Expr &receiver) {
    Expr countExpr;
    countExpr.kind = Expr::Kind::Call;
    countExpr.name = "count";
    countExpr.args.push_back(receiver);
    countExpr.argNames.resize(1);
    return countExpr;
  };

  const int32_t limitLocal = allocTempLocal();
  if (!emitExpr(makeCountExpr(selfNameExpr), locals)) {
    return false;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(limitLocal)});

  const int32_t resultLocal = allocTempLocal();
  instructions.push_back({IrOpcode::PushI32, 1});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(resultLocal)});

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(limitLocal)});
  if (!emitExpr(makeCountExpr(otherNameExpr), locals)) {
    return false;
  }
  instructions.push_back({IrOpcode::CmpEqI32, 0});
  const size_t jumpIfLengthMismatch = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});

  const int32_t indexLocal = allocTempLocal();
  instructions.push_back({IrOpcode::PushI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

  Expr indexNameExpr;
  indexNameExpr.kind = Expr::Kind::Name;
  indexNameExpr.name = "__ir_string_eq_index";
  LocalInfo indexInfo;
  indexInfo.index = indexLocal;
  indexInfo.valueKind = LocalInfo::ValueKind::Int32;
  locals[indexNameExpr.name] = indexInfo;

  const size_t loopStart = instructions.size();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(limitLocal)});
  instructions.push_back({IrOpcode::CmpLtI32, 0});
  const size_t jumpToEndNormal = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});

  auto makeAtExpr = [](const Expr &receiver, const Expr &index) {
    Expr atExpr;
    atExpr.kind = Expr::Kind::Call;
    atExpr.name = "at";
    atExpr.args.push_back(receiver);
    atExpr.args.push_back(index);
    atExpr.argNames.resize(2);
    return atExpr;
  };

  Expr byteNotEqualExpr;
  byteNotEqualExpr.kind = Expr::Kind::Call;
  byteNotEqualExpr.name = "not_equal";
  byteNotEqualExpr.args.push_back(makeAtExpr(selfNameExpr, indexNameExpr));
  byteNotEqualExpr.args.push_back(makeAtExpr(otherNameExpr, indexNameExpr));
  byteNotEqualExpr.argNames.resize(2);
  if (!emitExpr(byteNotEqualExpr, locals)) {
    return false;
  }
  const size_t jumpToContinue = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});

  instructions.push_back({IrOpcode::PushI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(resultLocal)});
  const size_t jumpToEndFromMismatch = instructions.size();
  instructions.push_back({IrOpcode::Jump, 0});

  const size_t continueIndex = instructions.size();
  instructions[jumpToContinue].imm = static_cast<uint64_t>(continueIndex);
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  instructions.push_back({IrOpcode::PushI32, 1});
  instructions.push_back({IrOpcode::AddI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});
  instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(loopStart)});

  const size_t lengthMismatchIndex = instructions.size();
  instructions[jumpIfLengthMismatch].imm = static_cast<uint64_t>(lengthMismatchIndex);
  instructions.push_back({IrOpcode::PushI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(resultLocal)});

  const size_t endIndex = instructions.size();
  instructions[jumpToEndNormal].imm = static_cast<uint64_t>(endIndex);
  instructions[jumpToEndFromMismatch].imm = static_cast<uint64_t>(endIndex);
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal)});

  if (negate) {
    return emitCompareToZero(LocalInfo::ValueKind::Bool, true);
  }
  return true;
}

} // namespace

OperatorComparisonEmitResult emitComparisonOperatorExpr(const Expr &expr,
                                                        const LocalMap &localsIn,
                                                        const EmitComparisonExprWithLocalsFn &emitExpr,
                                                        const InferComparisonExprKindWithLocalsFn &inferExprKind,
                                                        const ComparisonKindFn &comparisonKind,
                                                        const EmitComparisonToZeroFn &emitCompareToZero,
                                                        const ComparisonAllocTempLocalFn &allocTempLocal,
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
    if ((builtin == "equal" || builtin == "not_equal") && leftKind == LocalInfo::ValueKind::String &&
        rightKind == LocalInfo::ValueKind::String) {
      if (!emitStringEqualityComparison(expr, localsIn, emitExpr, allocTempLocal, emitCompareToZero,
                                        builtin == "not_equal", instructions)) {
        return OperatorComparisonEmitResult::Error;
      }
      return OperatorComparisonEmitResult::Handled;
    }
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
