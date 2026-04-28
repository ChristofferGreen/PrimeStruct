#include "SemanticsValidator.h"

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace primec::semantics {
namespace {

struct PickArm {
  const Expr *expr = nullptr;
  const SumVariant *variant = nullptr;
  std::string binderName;
  bool hasPayloadBinder = false;
};

BindingInfo payloadBindingForVariant(const SumVariant &variant) {
  BindingInfo binding;
  binding.typeName = variant.payloadType;
  if (!variant.payloadTemplateArgs.empty()) {
    binding.typeTemplateArg = joinTemplateArgs(variant.payloadTemplateArgs);
  }
  return binding;
}

std::string joinVariantNames(const std::vector<std::string> &names) {
  std::string out;
  for (const std::string &name : names) {
    if (!out.empty()) {
      out += ", ";
    }
    out += name;
  }
  return out;
}

const SumVariant *findVariantByName(const Definition &sumDef,
                                    const std::string &name) {
  for (const auto &variant : sumDef.sumVariants) {
    if (variant.name == name) {
      return &variant;
    }
  }
  return nullptr;
}

bool isPickArmEnvelopeBase(const Expr &candidate) {
  return candidate.kind == Expr::Kind::Call && !candidate.isBinding &&
         !candidate.isMethodCall && !candidate.isFieldAccess &&
         candidate.hasBodyArguments && !candidate.name.empty() &&
         candidate.templateArgs.empty() && !hasNamedArguments(candidate.argNames);
}

bool isPayloadPickArmEnvelope(const Expr &candidate) {
  return isPickArmEnvelopeBase(candidate) && candidate.args.size() == 1 &&
         candidate.args.front().kind == Expr::Kind::Name;
}

bool isUnitCallPickArmEnvelope(const Expr &candidate) {
  return isPickArmEnvelopeBase(candidate) && candidate.args.empty();
}

bool isUnitBindingPickArmEnvelope(const Expr &candidate) {
  return candidate.kind == Expr::Kind::Call && candidate.isBinding &&
         !candidate.isMethodCall && !candidate.isFieldAccess &&
         !candidate.name.empty() && candidate.transforms.empty() &&
         candidate.templateArgs.empty() && !candidate.hasBodyArguments &&
         candidate.bodyArguments.empty() && candidate.args.size() == 1 &&
         candidate.argNames.size() == 1 && !candidate.argNames.front().has_value();
}

std::vector<const Expr *> pickArmBodyExprs(const Expr &arm) {
  std::vector<const Expr *> out;
  if (isPayloadPickArmEnvelope(arm) || isUnitCallPickArmEnvelope(arm)) {
    out.reserve(arm.bodyArguments.size());
    for (const Expr &bodyExpr : arm.bodyArguments) {
      out.push_back(&bodyExpr);
    }
    return out;
  }
  if (!isUnitBindingPickArmEnvelope(arm)) {
    return out;
  }
  const Expr &initializer = arm.args.front();
  if (initializer.kind == Expr::Kind::Call && initializer.name == "block" &&
      initializer.hasBodyArguments && initializer.args.empty()) {
    out.reserve(initializer.bodyArguments.size());
    for (const Expr &bodyExpr : initializer.bodyArguments) {
      out.push_back(&bodyExpr);
    }
    return out;
  }
  out.push_back(&initializer);
  return out;
}

const std::vector<Expr> *pickArmBodyRange(const Expr &arm) {
  if (isPayloadPickArmEnvelope(arm) || isUnitCallPickArmEnvelope(arm)) {
    return &arm.bodyArguments;
  }
  if (!isUnitBindingPickArmEnvelope(arm)) {
    return nullptr;
  }
  const Expr &initializer = arm.args.front();
  if (initializer.kind == Expr::Kind::Call && initializer.name == "block" &&
      initializer.hasBodyArguments && initializer.args.empty()) {
    return &initializer.bodyArguments;
  }
  return nullptr;
}

ReturnKind combinePickNumericReturnKinds(ReturnKind left, ReturnKind right) {
  if (left == ReturnKind::Unknown || right == ReturnKind::Unknown) {
    return ReturnKind::Unknown;
  }
  if (left == ReturnKind::Bool || right == ReturnKind::Bool ||
      left == ReturnKind::String || right == ReturnKind::String ||
      left == ReturnKind::Void || right == ReturnKind::Void ||
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
    return (left == ReturnKind::UInt64 && right == ReturnKind::UInt64)
               ? ReturnKind::UInt64
               : ReturnKind::Unknown;
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

bool SemanticsValidator::validatePickExpr(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr) {
  auto failPickDiagnostic = [&](const Expr &diagnosticExpr,
                                std::string message) -> bool {
    return failExprDiagnostic(diagnosticExpr, std::move(message));
  };
  if (hasNamedArguments(expr.argNames)) {
    return failPickDiagnostic(expr, "named arguments not supported for builtin calls");
  }
  if (!expr.templateArgs.empty()) {
    return failPickDiagnostic(expr, "pick does not accept template arguments");
  }
  if (expr.args.size() != 1) {
    return failPickDiagnostic(expr, "pick requires exactly one sum value");
  }
  if (!expr.hasBodyArguments || expr.bodyArguments.empty()) {
    return failPickDiagnostic(expr, "pick requires variant arms");
  }

  const Expr &target = expr.args.front();
  if (!validateExpr(params, locals, target)) {
    return false;
  }
  const Definition *sumDef =
      resolvePickSumDefinitionForExpr(target, params, locals, expr.namespacePrefix);
  if (sumDef == nullptr) {
    return failPickDiagnostic(target, "pick target requires sum value");
  }

  std::vector<PickArm> arms;
  arms.reserve(expr.bodyArguments.size());
  std::unordered_set<std::string> seenVariants;
  for (const Expr &arm : expr.bodyArguments) {
    const bool payloadArm = isPayloadPickArmEnvelope(arm);
    const bool unitArm = isUnitCallPickArmEnvelope(arm) ||
                         isUnitBindingPickArmEnvelope(arm);
    if (!payloadArm && !unitArm) {
      return failPickDiagnostic(
          arm,
          "pick arms require variant(payload) block for " + sumDef->fullPath);
    }
    const SumVariant *variant = findVariantByName(*sumDef, arm.name);
    if (variant == nullptr) {
      return failPickDiagnostic(
          arm,
          "unknown pick variant on " + sumDef->fullPath + ": " + arm.name);
    }
    if (!seenVariants.insert(arm.name).second) {
      return failPickDiagnostic(
          arm,
          "duplicate pick variant on " + sumDef->fullPath + ": " + arm.name);
    }
    if (variant->hasPayload && !payloadArm) {
      return failPickDiagnostic(
          arm,
          "pick arms require variant(payload) block for " + sumDef->fullPath);
    }
    if (!variant->hasPayload && payloadArm) {
      return failPickDiagnostic(
          arm,
          "pick unit variant arm does not accept payload binder: " +
              sumDef->fullPath + "/" + variant->name);
    }
    arms.push_back(PickArm{&arm,
                           variant,
                           payloadArm ? arm.args.front().name : std::string{},
                           payloadArm});
  }

  std::vector<std::string> missingVariants;
  for (const auto &variant : sumDef->sumVariants) {
    if (seenVariants.count(variant.name) == 0) {
      missingVariants.push_back(variant.name);
    }
  }
  if (!missingVariants.empty()) {
    return failPickDiagnostic(
        expr,
        "missing pick variants for " + sumDef->fullPath + ": " +
            joinVariantNames(missingVariants));
  }

  std::optional<ReturnKind> combinedKind;
  bool sawString = false;
  bool sawNonString = false;
  for (const PickArm &arm : arms) {
    std::unordered_map<std::string, BindingInfo> branchLocals = locals;
    if (arm.hasPayloadBinder &&
        (isParam(params, arm.binderName) ||
         branchLocals.count(arm.binderName) > 0)) {
      return failPickDiagnostic(
          *arm.expr,
          "pick payload binder conflicts with existing binding: " +
              arm.binderName);
    }
    if (arm.hasPayloadBinder) {
      insertLocalBinding(branchLocals,
                         arm.binderName,
                         payloadBindingForVariant(*arm.variant));
    }

    const Expr *valueExpr = nullptr;
    bool sawReturn = false;
    const std::vector<const Expr *> bodyExprs = pickArmBodyExprs(*arm.expr);
    for (const Expr *bodyExprPtr : bodyExprs) {
      const Expr &bodyExpr = *bodyExprPtr;
      if (isSyntheticBlockValueBinding(bodyExpr)) {
        if (!validateExpr(params, branchLocals, bodyExpr.args.front())) {
          return false;
        }
        if (!sawReturn) {
          valueExpr = &bodyExpr.args.front();
        }
        continue;
      }
      if (bodyExpr.isBinding) {
        bool handledBinding = false;
        if (!validateBindingStatement(params,
                                      branchLocals,
                                      bodyExpr,
                                      true,
                                      bodyExpr.namespacePrefix,
                                      handledBinding)) {
          return false;
        }
        if (!handledBinding) {
          return failPickDiagnostic(bodyExpr, "pick arm binding is invalid");
        }
        continue;
      }
      if (isReturnCall(bodyExpr)) {
        if (bodyExpr.args.size() != 1) {
          return failPickDiagnostic(bodyExpr,
                                    "return requires a value in pick arm");
        }
        if (!validateExpr(params, branchLocals, bodyExpr.args.front())) {
          return false;
        }
        valueExpr = &bodyExpr.args.front();
        sawReturn = true;
        continue;
      }
      if (!validateExpr(params, branchLocals, bodyExpr)) {
        return false;
      }
      if (!sawReturn) {
        valueExpr = &bodyExpr;
      }
    }
    if (valueExpr == nullptr) {
      return failPickDiagnostic(*arm.expr,
                                "pick arm must produce a value: " +
                                    arm.variant->name);
    }
    ReturnKind armKind = inferExprReturnKind(*valueExpr, params, branchLocals);
    if (armKind == ReturnKind::Void) {
      if (isStructConstructorValueExpr(*valueExpr)) {
        armKind = ReturnKind::Unknown;
      } else {
        return failPickDiagnostic(*valueExpr,
                                  "pick arms must produce a value");
      }
    }
    sawString = sawString || armKind == ReturnKind::String;
    sawNonString = sawNonString || (armKind != ReturnKind::String &&
                                   armKind != ReturnKind::Unknown);
    if (!combinedKind.has_value()) {
      combinedKind = armKind;
    } else if (*combinedKind != armKind) {
      combinedKind = combinePickNumericReturnKinds(*combinedKind, armKind);
    }
  }
  if (sawString && sawNonString) {
    return failPickDiagnostic(expr, "pick branches must return compatible types");
  }
  if (combinedKind.has_value() && *combinedKind == ReturnKind::Unknown &&
      (sawString || sawNonString)) {
    return failPickDiagnostic(expr, "pick branches must return compatible types");
  }
  return true;
}

ReturnKind SemanticsValidator::inferPickExprReturnKind(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) {
  if (!isPickCall(expr) || expr.args.size() != 1 || expr.bodyArguments.empty()) {
    return ReturnKind::Unknown;
  }
  const Definition *sumDef = resolvePickSumDefinitionForExpr(
      expr.args.front(), params, locals, expr.namespacePrefix);
  if (sumDef == nullptr) {
    return ReturnKind::Unknown;
  }

  std::optional<ReturnKind> combinedKind;
  for (const Expr &arm : expr.bodyArguments) {
    const bool payloadArm = isPayloadPickArmEnvelope(arm);
    const bool unitArm = isUnitCallPickArmEnvelope(arm) ||
                         isUnitBindingPickArmEnvelope(arm);
    if (!payloadArm && !unitArm) {
      return ReturnKind::Unknown;
    }
    const SumVariant *variant = findVariantByName(*sumDef, arm.name);
    if (variant == nullptr) {
      return ReturnKind::Unknown;
    }
    if (variant->hasPayload != payloadArm) {
      return ReturnKind::Unknown;
    }
    std::unordered_map<std::string, BindingInfo> branchLocals = locals;
    if (payloadArm) {
      insertLocalBinding(branchLocals,
                         arm.args.front().name,
                         payloadBindingForVariant(*variant));
    }

    const Expr *valueExpr = nullptr;
    bool sawReturn = false;
    const std::vector<const Expr *> bodyExprs = pickArmBodyExprs(arm);
    for (const Expr *bodyExprPtr : bodyExprs) {
      const Expr &bodyExpr = *bodyExprPtr;
      if (isSyntheticBlockValueBinding(bodyExpr)) {
        if (!sawReturn) {
          valueExpr = &bodyExpr.args.front();
        }
        continue;
      }
      if (bodyExpr.isBinding) {
        BindingInfo binding;
        std::optional<std::string> restrictType;
        if (!parseBindingInfo(bodyExpr,
                              bodyExpr.namespacePrefix,
                              structNames_,
                              importAliases_,
                              binding,
                              restrictType,
                              error_,
                              &sumNames_)) {
          return ReturnKind::Unknown;
        }
        const bool hasExplicitType = hasExplicitBindingTypeTransform(bodyExpr);
        const bool explicitAutoType =
            hasExplicitType && normalizeBindingTypeName(binding.typeName) == "auto";
        if (bodyExpr.args.size() == 1 && (!hasExplicitType || explicitAutoType)) {
          (void)inferBindingTypeFromInitializer(bodyExpr.args.front(),
                                                params,
                                                branchLocals,
                                                binding,
                                                &bodyExpr);
        }
        insertLocalBinding(branchLocals, bodyExpr.name, std::move(binding));
        continue;
      }
      if (isReturnCall(bodyExpr)) {
        if (bodyExpr.args.size() != 1) {
          return ReturnKind::Unknown;
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
      return ReturnKind::Unknown;
    }
    const ReturnKind armKind = inferExprReturnKind(*valueExpr, params, branchLocals);
    if (!combinedKind.has_value()) {
      combinedKind = armKind;
    } else if (*combinedKind != armKind) {
      combinedKind = combinePickNumericReturnKinds(*combinedKind, armKind);
    }
  }
  return combinedKind.value_or(ReturnKind::Unknown);
}

bool SemanticsValidator::validatePickStatement(
    const std::vector<ParameterInfo> &params,
    std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &stmt,
    ReturnKind returnKind,
    bool allowReturn,
    bool allowBindings,
    bool *sawReturn,
    const std::string &namespacePrefix,
    const std::vector<Expr> *enclosingStatements,
    size_t statementIndex,
    bool &handled) {
  handled = false;
  if (!isPickCall(stmt)) {
    return true;
  }
  handled = true;

  auto failPickStatementDiagnostic = [&](const Expr &diagnosticExpr,
                                         std::string message) -> bool {
    return failExprDiagnostic(diagnosticExpr, std::move(message));
  };
  if (hasNamedArguments(stmt.argNames)) {
    return failPickStatementDiagnostic(stmt, "named arguments not supported for builtin calls");
  }
  if (!stmt.templateArgs.empty()) {
    return failPickStatementDiagnostic(stmt, "pick does not accept template arguments");
  }
  if (stmt.args.size() != 1) {
    return failPickStatementDiagnostic(stmt, "pick requires exactly one sum value");
  }
  if (!stmt.hasBodyArguments || stmt.bodyArguments.empty()) {
    return failPickStatementDiagnostic(stmt, "pick requires variant arms");
  }
  if (!validateExpr(params, locals, stmt.args.front())) {
    return false;
  }
  const Definition *sumDef = resolvePickSumDefinitionForExpr(
      stmt.args.front(), params, locals, namespacePrefix);
  if (sumDef == nullptr) {
    return failPickStatementDiagnostic(stmt.args.front(),
                                       "pick target requires sum value");
  }

  std::unordered_set<std::string> seenVariants;
  for (const Expr &arm : stmt.bodyArguments) {
    const bool payloadArm = isPayloadPickArmEnvelope(arm);
    const bool unitArm = isUnitCallPickArmEnvelope(arm) ||
                         isUnitBindingPickArmEnvelope(arm);
    if (!payloadArm && !unitArm) {
      return failPickStatementDiagnostic(
          arm,
          "pick arms require variant(payload) block for " + sumDef->fullPath);
    }
    const SumVariant *variant = findVariantByName(*sumDef, arm.name);
    if (variant == nullptr) {
      return failPickStatementDiagnostic(
          arm,
          "unknown pick variant on " + sumDef->fullPath + ": " + arm.name);
    }
    if (!seenVariants.insert(arm.name).second) {
      return failPickStatementDiagnostic(
          arm,
          "duplicate pick variant on " + sumDef->fullPath + ": " + arm.name);
    }
    if (variant->hasPayload && !payloadArm) {
      return failPickStatementDiagnostic(
          arm,
          "pick arms require variant(payload) block for " + sumDef->fullPath);
    }
    if (!variant->hasPayload && payloadArm) {
      return failPickStatementDiagnostic(
          arm,
          "pick unit variant arm does not accept payload binder: " +
              sumDef->fullPath + "/" + variant->name);
    }
    if (payloadArm && (isParam(params, arm.args.front().name) ||
                       locals.count(arm.args.front().name) > 0)) {
      return failPickStatementDiagnostic(
          arm,
          "pick payload binder conflicts with existing binding: " +
              arm.args.front().name);
    }
  }
  std::vector<std::string> missingVariants;
  for (const auto &variant : sumDef->sumVariants) {
    if (seenVariants.count(variant.name) == 0) {
      missingVariants.push_back(variant.name);
    }
  }
  if (!missingVariants.empty()) {
    return failPickStatementDiagnostic(
        stmt,
        "missing pick variants for " + sumDef->fullPath + ": " +
            joinVariantNames(missingVariants));
  }

  const std::vector<Expr> *postMergeStatements = enclosingStatements;
  const size_t postMergeStartIndex =
      enclosingStatements == nullptr
          ? 0
          : std::min(statementIndex + 1, enclosingStatements->size());
  for (const Expr &arm : stmt.bodyArguments) {
    const SumVariant *variant = findVariantByName(*sumDef, arm.name);
    const bool payloadArm = isPayloadPickArmEnvelope(arm);
    if (variant == nullptr || (variant->hasPayload && !payloadArm)) {
      continue;
    }
    std::unordered_map<std::string, BindingInfo> branchLocals = locals;
    if (payloadArm) {
      insertLocalBinding(branchLocals,
                         arm.args.front().name,
                         payloadBindingForVariant(*variant));
    }

    std::vector<BorrowLivenessRange> livenessRanges;
    livenessRanges.reserve(2);
    OnErrorScope onErrorScope(*this, std::nullopt);
    BorrowEndScope borrowScope(*this, currentValidationState_.endedReferenceBorrows);
    const std::vector<const Expr *> bodyExprs = pickArmBodyExprs(arm);
    for (size_t bodyIndex = 0; bodyIndex < bodyExprs.size(); ++bodyIndex) {
      const Expr &bodyExpr = *bodyExprs[bodyIndex];
      if (!validateStatement(params,
                             branchLocals,
                             bodyExpr,
                             returnKind,
                             allowReturn,
                             allowBindings,
                             sawReturn,
                             namespacePrefix,
                             nullptr,
                             bodyIndex)) {
        return false;
      }
      livenessRanges.clear();
      if (const std::vector<Expr> *bodyRange = pickArmBodyRange(arm)) {
        livenessRanges.push_back(BorrowLivenessRange{bodyRange,
                                                     bodyIndex + 1});
      }
      if (postMergeStatements != nullptr &&
          postMergeStartIndex < postMergeStatements->size()) {
        livenessRanges.push_back(BorrowLivenessRange{postMergeStatements,
                                                     postMergeStartIndex});
      }
      expireReferenceBorrowsForRanges(params, branchLocals, livenessRanges);
    }
  }
  return true;
}

} // namespace primec::semantics
