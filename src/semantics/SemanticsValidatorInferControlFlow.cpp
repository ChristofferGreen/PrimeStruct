#include "SemanticsValidator.h"

#include <optional>

namespace primec::semantics {
namespace {

ReturnKind combineNumericReturnKinds(ReturnKind left, ReturnKind right) {
  if (left == ReturnKind::Unknown || right == ReturnKind::Unknown) {
    return ReturnKind::Unknown;
  }
  if (left == ReturnKind::Bool || right == ReturnKind::Bool || left == ReturnKind::String ||
      right == ReturnKind::String || left == ReturnKind::Void || right == ReturnKind::Void ||
      left == ReturnKind::Array || right == ReturnKind::Array) {
    return ReturnKind::Unknown;
  }
  if (left == ReturnKind::Float64 || right == ReturnKind::Float64) {
    return ReturnKind::Float64;
  }
  if (left == ReturnKind::Float32 || right == ReturnKind::Float32) {
    return ReturnKind::Float32;
  }
  if (left == ReturnKind::UInt64 || right == ReturnKind::UInt64) {
    return (left == ReturnKind::UInt64 && right == ReturnKind::UInt64) ? ReturnKind::UInt64 : ReturnKind::Unknown;
  }
  if (left == ReturnKind::Int64 || right == ReturnKind::Int64) {
    if ((left == ReturnKind::Int64 || left == ReturnKind::Int) &&
        (right == ReturnKind::Int64 || right == ReturnKind::Int)) {
      return ReturnKind::Int64;
    }
    return ReturnKind::Unknown;
  }
  if (left == ReturnKind::Int && right == ReturnKind::Int) {
    return ReturnKind::Int;
  }
  return ReturnKind::Unknown;
}

} // namespace

ReturnKind SemanticsValidator::inferControlFlowExprReturnKind(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    bool &handled) {
  handled = false;

  if (isMatchCall(expr)) {
    handled = true;
    Expr expanded;
    if (!lowerMatchToIf(expr, expanded, error_)) {
      return ReturnKind::Unknown;
    }
    return inferExprReturnKind(expanded, params, locals);
  }

  if (isPickCall(expr)) {
    handled = true;
    return inferPickExprReturnKind(expr, params, locals);
  }

  if (isIfCall(expr) && expr.args.size() == 3) {
    handled = true;
    std::unordered_map<std::string, BindingInfo> branchLocals = locals;
    auto inferBlockEnvelopeValue = [&](const Expr &candidate,
                                       ReturnKind &kindOut) -> bool {
      kindOut = ReturnKind::Unknown;
      if (!isEnvelopeValueExpr(candidate, true)) {
        return false;
      }
      LocalBindingScope branchScope(*this, branchLocals);
      const Expr *valueExpr = nullptr;
      bool sawReturn = false;
      for (const auto &bodyExpr : candidate.bodyArguments) {
        if (isSyntheticBlockValueBinding(bodyExpr)) {
          if (!sawReturn) {
            valueExpr = &bodyExpr.args.front();
          }
          continue;
        }
        if (bodyExpr.isBinding) {
          BindingInfo binding;
          std::optional<std::string> restrictType;
          if (!parseBindingInfo(bodyExpr, candidate.namespacePrefix, structNames_, importAliases_, binding, restrictType,
                                error_, &sumNames_)) {
            return false;
          }
          const bool hasExplicitType = hasExplicitBindingTypeTransform(bodyExpr);
          const bool explicitAutoType = hasExplicitType && normalizeBindingTypeName(binding.typeName) == "auto";
          if (bodyExpr.args.size() == 1 && (!hasExplicitType || explicitAutoType)) {
            (void)inferBindingTypeFromInitializer(bodyExpr.args.front(), params, branchLocals, binding, &bodyExpr);
          }
          insertLocalBinding(branchLocals, bodyExpr.name, std::move(binding));
          continue;
        }
        if (isReturnCall(bodyExpr)) {
          if (bodyExpr.args.size() != 1) {
            return false;
          }
          valueExpr = &bodyExpr.args.front();
          sawReturn = true;
          continue;
        }
        if (!sawReturn) {
          valueExpr = &bodyExpr;
        }
      }
      if (valueExpr == nullptr) {
        return false;
      }
      kindOut = inferExprReturnKind(*valueExpr, params, branchLocals);
      return true;
    };

    ReturnKind thenKind = ReturnKind::Unknown;
    ReturnKind elseKind = ReturnKind::Unknown;
    if (!inferBlockEnvelopeValue(expr.args[1], thenKind) ||
        !inferBlockEnvelopeValue(expr.args[2], elseKind)) {
      return ReturnKind::Unknown;
    }

    if (thenKind == elseKind) {
      return thenKind;
    }
    return combineNumericReturnKinds(thenKind, elseKind);
  }

  if (isBlockCall(expr) && expr.hasBodyArguments) {
    const std::string resolved = resolveCalleePath(expr);
    if (defMap_.find(resolved) == defMap_.end() && expr.args.empty() && expr.templateArgs.empty() &&
        !hasNamedArguments(expr.argNames)) {
      handled = true;
      if (expr.bodyArguments.empty()) {
        return ReturnKind::Unknown;
      }
      std::unordered_map<std::string, BindingInfo> blockLocals = locals;
      ReturnKind result = ReturnKind::Unknown;
      bool sawReturn = false;
      for (const auto &bodyExpr : expr.bodyArguments) {
        if (bodyExpr.isBinding) {
          BindingInfo info;
          std::optional<std::string> restrictType;
          if (!parseBindingInfo(bodyExpr, bodyExpr.namespacePrefix, structNames_, importAliases_, info, restrictType,
                                error_, &sumNames_)) {
            return ReturnKind::Unknown;
          }
          if (!hasExplicitBindingTypeTransform(bodyExpr) && bodyExpr.args.size() == 1) {
            (void)inferBindingTypeFromInitializer(bodyExpr.args.front(), params, blockLocals, info, &bodyExpr);
          }
          if (restrictType.has_value()) {
            const bool hasTemplate = !info.typeTemplateArg.empty();
            if (!restrictMatchesBinding(*restrictType,
                                        info.typeName,
                                        info.typeTemplateArg,
                                        hasTemplate,
                                        bodyExpr.namespacePrefix)) {
              return ReturnKind::Unknown;
            }
          }
          insertLocalBinding(blockLocals, bodyExpr.name, std::move(info));
          continue;
        }
        if (isReturnCall(bodyExpr)) {
          if (bodyExpr.args.size() == 1) {
            result = inferExprReturnKind(bodyExpr.args.front(), params, blockLocals);
            sawReturn = true;
          }
          continue;
        }
        if (!sawReturn) {
          result = inferExprReturnKind(bodyExpr, params, blockLocals);
        }
      }
      return result;
    }
  }

  return ReturnKind::Unknown;
}

} // namespace primec::semantics
