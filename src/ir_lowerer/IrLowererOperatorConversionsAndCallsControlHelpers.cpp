#include "IrLowererOperatorConversionsAndCallsHelpers.h"
#include "IrLowererStatementBindingHelpers.h"

namespace primec::ir_lowerer {

bool emitConversionsAndCallsControlExprTail(
    const Expr &expr,
    const LocalMap &localsIn,
    const EmitConversionsAndCallsExprWithLocalsFn &emitExpr,
    const EmitConversionsAndCallsStatementWithLocalsFn &emitStatement,
    const InferConversionsAndCallsExprKindWithLocalsFn &inferExprKind,
    const CombineConversionsAndCallsNumericKindsFn &combineNumericKinds,
    const HasConversionsAndCallsNamedArgumentsFn &hasNamedArguments,
    const ResolveConversionsAndCallsDefinitionCallFn &resolveDefinitionCall,
    const ResolveConversionsAndCallsExprPathFn &resolveExprPath,
    const LowerConversionsAndCallsMatchToIfFn &lowerMatchToIf,
    const IsConversionsAndCallsBindingMutableFn &isBindingMutable,
    const ConversionsAndCallsBindingKindFn &bindingKind,
    const HasConversionsAndCallsExplicitBindingTypeTransformFn &hasExplicitBindingTypeTransform,
    const ConversionsAndCallsBindingValueKindFn &bindingValueKind,
    const InferConversionsAndCallsStructExprPathFn &inferStructExprPath,
    const ApplyConversionsAndCallsStructArrayInfoFn &applyStructArrayInfo,
    const ApplyConversionsAndCallsStructValueInfoFn &applyStructValueInfo,
    const EnterConversionsAndCallsScopedBlockFn &enterScopedBlock,
    const ExitConversionsAndCallsScopedBlockFn &exitScopedBlock,
    const IsConversionsAndCallsReturnCallFn &isReturnCall,
    const IsConversionsAndCallsBlockCallFn &isBlockCall,
    const IsConversionsAndCallsMatchCallFn &isMatchCall,
    const IsConversionsAndCallsIfCallFn &isIfCall,
    std::vector<IrInstruction> &instructions,
    bool &handled,
    std::string &error) {
  handled = true;
  if (isBlockCall(expr) && expr.hasBodyArguments) {
    if (!expr.args.empty() || !expr.templateArgs.empty() || hasNamedArguments(expr.argNames)) {
      error = "block expression does not accept arguments";
      return false;
    }
    if (resolveDefinitionCall(expr) != nullptr) {
      error = "block arguments require a definition target: " + resolveExprPath(expr);
      return false;
    }
    if (expr.bodyArguments.empty()) {
      error = "block expression requires a value";
      return false;
    }
    LocalMap blockLocals = localsIn;
    for (size_t i = 0; i < expr.bodyArguments.size(); ++i) {
      const Expr &bodyStmt = expr.bodyArguments[i];
      const bool isLast = (i + 1 == expr.bodyArguments.size());
      if (bodyStmt.isBinding) {
        if (isLast) {
          error = "block expression must end with an expression";
          return false;
        }
        if (!emitStatement(bodyStmt, blockLocals)) {
          return false;
        }
        continue;
      }
      if (isReturnCall(bodyStmt)) {
        if (bodyStmt.args.size() != 1) {
          error = "return requires a value in block expression";
          return false;
        }
        return emitExpr(bodyStmt.args.front(), blockLocals);
      }
      if (isLast) {
        return emitExpr(bodyStmt, blockLocals);
      }
      if (!emitStatement(bodyStmt, blockLocals)) {
        return false;
      }
    }
    error = "block expression requires a value";
    return false;
  }

  if (isMatchCall(expr)) {
    Expr expanded;
    if (!lowerMatchToIf(expr, expanded, error)) {
      return false;
    }
    return emitExpr(expanded, localsIn);
  }

  if (isIfCall(expr)) {
    if (expr.args.size() != 3) {
      error = "if requires condition, then, else";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error = "if does not accept trailing block arguments";
      return false;
    }

    const Expr &cond = expr.args[0];
    const Expr &thenArg = expr.args[1];
    const Expr &elseArg = expr.args[2];
    auto isIfBlockEnvelope = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
        return false;
      }
      if (!candidate.args.empty() || !candidate.templateArgs.empty() ||
          hasNamedArguments(candidate.argNames)) {
        return false;
      }
      if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
        return false;
      }
      return true;
    };

    if (!isIfBlockEnvelope(thenArg) || !isIfBlockEnvelope(elseArg)) {
      error = "if branches require block envelopes";
      return false;
    }

    auto inferBranchValueKind =
        [&](const Expr &candidate, const LocalMap &localsBase) -> LocalInfo::ValueKind {
      LocalMap branchLocals = localsBase;
      bool sawValue = false;
      LocalInfo::ValueKind lastKind = LocalInfo::ValueKind::Unknown;
      for (const auto &bodyExpr : candidate.bodyArguments) {
        if (bodyExpr.isBinding) {
          if (bodyExpr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          LocalInfo info;
          info.index = 0;
          info.isMutable = isBindingMutable(bodyExpr);
          info.kind = bindingKind(bodyExpr);
          LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
          if (hasExplicitBindingTypeTransform(bodyExpr)) {
            valueKind = bindingValueKind(bodyExpr, info.kind);
          } else if (bodyExpr.args.size() == 1 && info.kind == LocalInfo::Kind::Value) {
            valueKind = inferExprKind(bodyExpr.args.front(), branchLocals);
            if (valueKind == LocalInfo::ValueKind::Unknown) {
              std::string inferredStruct =
                  inferStructExprPath(bodyExpr.args.front(), branchLocals);
              if (!inferredStruct.empty()) {
                info.structTypeName = inferredStruct;
              } else {
                valueKind = LocalInfo::ValueKind::Int32;
              }
            }
          }
          info.valueKind = valueKind;
          applyStructArrayInfo(bodyExpr, info);
          applyStructValueInfo(bodyExpr, info);
          if (info.structTypeName.empty() && info.kind == LocalInfo::Kind::Value &&
              info.valueKind == LocalInfo::ValueKind::Unknown &&
              bodyExpr.args.size() == 1) {
            std::string inferredStruct =
                inferStructExprPath(bodyExpr.args.front(), branchLocals);
            if (!inferredStruct.empty()) {
              info.structTypeName = inferredStruct;
            } else {
              info.valueKind = LocalInfo::ValueKind::Int32;
            }
          }
          branchLocals.emplace(bodyExpr.name, info);
          continue;
        }
        if (isReturnCall(bodyExpr)) {
          if (bodyExpr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          return inferExprKind(bodyExpr.args.front(), branchLocals);
        }
        sawValue = true;
        lastKind = inferExprKind(bodyExpr, branchLocals);
      }
      return sawValue ? lastKind : LocalInfo::ValueKind::Unknown;
    };
    auto inferBranchValueTypeInfo =
        [&](const Expr &candidate, const LocalMap &localsBase) -> StatementBindingTypeInfo {
      StatementBindingTypeInfo info;
      const Expr *valueExpr = &candidate;
      if (isIfBlockEnvelope(candidate)) {
        valueExpr = nullptr;
        for (const auto &bodyExpr : candidate.bodyArguments) {
          if (bodyExpr.isBinding) {
            continue;
          }
          if (isReturnCall(bodyExpr)) {
            if (bodyExpr.args.size() != 1) {
              return info;
            }
            valueExpr = &bodyExpr.args.front();
            break;
          }
          valueExpr = &bodyExpr;
        }
      }
      if (valueExpr == nullptr) {
        return info;
      }
      Expr syntheticBinding;
      syntheticBinding.isBinding = true;
      syntheticBinding.args.push_back(*valueExpr);
      return inferStatementBindingTypeInfo(syntheticBinding,
                                           *valueExpr,
                                           localsBase,
                                           hasExplicitBindingTypeTransform,
                                           bindingKind,
                                           bindingValueKind,
                                           inferExprKind,
                                           resolveDefinitionCall);
    };
    auto branchTypeInfoCompatible = [&](const StatementBindingTypeInfo &left,
                                        const StatementBindingTypeInfo &right) {
      if (left.kind != right.kind) {
        return false;
      }
      if (left.kind == LocalInfo::Kind::Map) {
        return left.mapKeyKind == right.mapKeyKind &&
               left.mapValueKind == right.mapValueKind;
      }
      if (left.kind == LocalInfo::Kind::Value) {
        if (!left.structTypeName.empty() || !right.structTypeName.empty()) {
          return !left.structTypeName.empty() &&
                 left.structTypeName == right.structTypeName;
        }
        return left.valueKind != LocalInfo::ValueKind::Unknown &&
               left.valueKind == right.valueKind;
      }
      return left.valueKind == right.valueKind &&
             left.structTypeName == right.structTypeName;
    };

    auto emitBranchValue = [&](const Expr &candidate, const LocalMap &localsBase) -> bool {
      if (!isIfBlockEnvelope(candidate)) {
        return emitExpr(candidate, localsBase);
      }
      if (candidate.bodyArguments.empty()) {
        error = "if branch blocks must produce a value";
        return false;
      }

      enterScopedBlock();
      auto leaveScope = [&]() { exitScopedBlock(); };

      LocalMap branchLocals = localsBase;
      for (size_t i = 0; i < candidate.bodyArguments.size(); ++i) {
        const Expr &bodyStmt = candidate.bodyArguments[i];
        const bool isLast = (i + 1 == candidate.bodyArguments.size());
        if (bodyStmt.isBinding) {
          if (isLast) {
            error = "if branch blocks must end with an expression";
            leaveScope();
            return false;
          }
          if (!emitStatement(bodyStmt, branchLocals)) {
            leaveScope();
            return false;
          }
          continue;
        }
        if (isReturnCall(bodyStmt)) {
          if (bodyStmt.args.size() != 1) {
            error = "return requires a value in if branch";
            leaveScope();
            return false;
          }
          bool ok = emitExpr(bodyStmt.args.front(), branchLocals);
          leaveScope();
          return ok;
        }
        if (isLast) {
          bool ok = emitExpr(bodyStmt, branchLocals);
          leaveScope();
          return ok;
        }
        if (!emitStatement(bodyStmt, branchLocals)) {
          leaveScope();
          return false;
        }
      }

      leaveScope();
      error = "if branch blocks must produce a value";
      return false;
    };

    if (!emitExpr(cond, localsIn)) {
      return false;
    }
    LocalInfo::ValueKind condKind = inferExprKind(cond, localsIn);
    const bool isIntegralCondition =
        condKind == LocalInfo::ValueKind::Int32 || condKind == LocalInfo::ValueKind::Int64 ||
        condKind == LocalInfo::ValueKind::UInt64;
    if (condKind != LocalInfo::ValueKind::Bool && !isIntegralCondition) {
      error = "if condition requires bool";
      return false;
    }

    LocalInfo::ValueKind thenKind = inferBranchValueKind(thenArg, localsIn);
    LocalInfo::ValueKind elseKind = inferBranchValueKind(elseArg, localsIn);
    const StatementBindingTypeInfo thenInfo = inferBranchValueTypeInfo(thenArg, localsIn);
    const StatementBindingTypeInfo elseInfo = inferBranchValueTypeInfo(elseArg, localsIn);
    LocalInfo::ValueKind resultKind = LocalInfo::ValueKind::Unknown;
    if (branchTypeInfoCompatible(thenInfo, elseInfo)) {
      resultKind = thenInfo.valueKind;
    } else if (thenKind == elseKind) {
      resultKind = thenKind;
    } else {
      resultKind = combineNumericKinds(thenKind, elseKind);
    }
    if (resultKind == LocalInfo::ValueKind::Unknown) {
      error = "if branches must return compatible types";
      return false;
    }

    size_t jumpIfZeroIndex = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    if (!emitBranchValue(thenArg, localsIn)) {
      return false;
    }
    size_t jumpEndIndex = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});
    size_t elseIndex = instructions.size();
    instructions[jumpIfZeroIndex].imm = static_cast<int32_t>(elseIndex);
    if (!emitBranchValue(elseArg, localsIn)) {
      return false;
    }
    size_t endIndex = instructions.size();
    instructions[jumpEndIndex].imm = static_cast<int32_t>(endIndex);
    return true;
  }

  handled = false;
  return true;
}

} // namespace primec::ir_lowerer
