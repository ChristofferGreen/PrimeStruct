#include "TemplateMonomorphAssignmentTargetResolution.h"
#include "primec/frontend/StringLiteral.h"
#include "TemplateMonomorphBindingBlockInference.h"
#include "TemplateMonomorphBindingCallInference.h"
#include "TemplateMonomorphCallableRewrites.h"
#include "TemplateMonomorphTextViewConversions.h"
#include "TemplateMonomorphDefinitionBindingSetup.h"
#include "TemplateMonomorphDefinitionExperimentalCollectionRewrites.h"
#include "TemplateMonomorphDefinitionReturnOrchestration.h"
#include "TemplateMonomorphDefinitionRewrites.h"
#include "TemplateMonomorphExecutionRewrites.h"
#include "TemplateMonomorphExperimentalCollectionArgumentRewrites.h"
#include "TemplateMonomorphExperimentalCollectionConstructorRewrites.h"
#include "TemplateMonomorphExperimentalCollectionReceiverResolution.h"
#include "TemplateMonomorphExperimentalCollectionReturnRewrites.h"
#include "TemplateMonomorphExperimentalCollectionReturnSetup.h"
#include "TemplateMonomorphExperimentalCollectionTargetValueRewrites.h"
#include "TemplateMonomorphExperimentalCollectionValueRewrites.h"
#include "TemplateMonomorphExpressionRewrite.h"
#include "TemplateMonomorphFallbackTypeInference.h"
#include "TemplateMonomorphFinalOrchestration.h"
#include "TemplateMonomorphImplicitTemplateInference.h"
#include "TemplateMonomorphMethodTargets.h"
#include "TemplateMonomorphTemplateSpecialization.h"
#include "TemplateMonomorphTypeResolution.h"
#include "SemanticsHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "TemplateMonomorphCoreUtilities.h"
#include "TemplateMonomorphSetupUtilities.h"
#include "TemplateMonomorphCollectionCompatibilityPaths.h"
#include "TemplateMonomorphExperimentalCollectionTypeHelpers.h"
#include "TemplateMonomorphSourceDefinitionSetup.h"
#include "TemplateMonomorphExperimentalCollectionConstructorPaths.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include <sstream>
#include "primec/support/CompileArena.h"
#include "primec/support/CollectionHelperNames.h"
#include "TemplateMonomorphExpressionReferenceRules.h"
#include "TemplateMonomorphExpressionRewriteContext.h"

namespace primec {

// Rewrites of constructor values by target type, and the pick and body binding locals of an expression.

bool rewriteKeyValueTargetValueForResolvedType(ExprRewriteContext &rc,
                                               const std::string &typeText,
                                               Expr &valueExpr) {
  const SubstMap &mapping = rc.mapping;
  const std::unordered_set<std::string> &allowedParams = rc.allowedParams;
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;

  return rewriteExperimentalKeyValueTargetValueForType(
      typeText,
      valueExpr,
      mapping,
      allowedParams,
      namespacePrefix,
      ctx,
      [&](auto &&...args) {
        return rewriteNestedExperimentalKeyValueConstructorValue(
            rc, std::forward<decltype(args)>(args)...);
      },
      [&](auto &&...args) {
        return rewriteNestedExperimentalKeyValueResultOkPayloadValue(
            rc, std::forward<decltype(args)>(args)...);
      });
}

bool rewriteVectorTargetValueForResolvedType(ExprRewriteContext &rc,
                                             const std::string &typeText,
                                             Expr &valueExpr) {

  return rewriteExperimentalVectorTargetValueForType(typeText, valueExpr, [&](auto &&...args) {
    return rewriteNestedExperimentalVectorConstructorValue(rc,
                                                           std::forward<decltype(args)>(args)...);
  });
}

bool rewriteNestedExperimentalKeyValueConstructorValue(ExprRewriteContext &rc, Expr &candidate) {
  const SubstMap &mapping = rc.mapping;
  const std::unordered_set<std::string> &allowedParams = rc.allowedParams;
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  std::string &error = rc.error;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &allowMathBare = rc.allowMathBare;

  return rewriteExperimentalConstructorValueTree(candidate, [&](Expr &current) {
    return rewriteCanonicalExperimentalKeyValueConstructorExpr(current,
                                                               locals,
                                                               params,
                                                               mapping,
                                                               allowedParams,
                                                               namespacePrefix,
                                                               ctx,
                                                               allowMathBare,
                                                               error);
  });
}

bool rewriteNestedExperimentalKeyValueResultOkPayloadValue(ExprRewriteContext &rc,
                                                           Expr &candidate) {

  return rewriteExperimentalKeyValueResultOkPayloadTree(candidate, [&](auto &&...args) {
    return rewriteNestedExperimentalKeyValueConstructorValue(rc,
                                                             std::forward<decltype(args)>(args)...);
  });
}

bool rewriteNestedExperimentalVectorConstructorValue(ExprRewriteContext &rc, Expr &candidate) {
  const SubstMap &mapping = rc.mapping;
  const std::unordered_set<std::string> &allowedParams = rc.allowedParams;
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  std::string &error = rc.error;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &allowMathBare = rc.allowMathBare;

  return rewriteExperimentalConstructorValueTree(candidate, [&](Expr &current) {
    return rewriteCanonicalExperimentalVectorConstructorExpr(current,
                                                             locals,
                                                             params,
                                                             mapping,
                                                             allowedParams,
                                                             namespacePrefix,
                                                             ctx,
                                                             allowMathBare,
                                                             error);
  });
}

Definition *resolvePickSumDefinition(ExprRewriteContext &rc, const Expr &target) {
  const SubstMap &mapping = rc.mapping;
  const std::unordered_set<std::string> &allowedParams = rc.allowedParams;
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &allowMathBare = rc.allowMathBare;

  semantics::BindingInfo targetInfo;
  if (!inferBindingTypeForMonomorph(target, params, locals, allowMathBare, ctx, targetInfo)) {
    return nullptr;
  }
  std::string targetTypeText = bindingTypeToString(targetInfo);
  if (targetTypeText.empty()) {
    return nullptr;
  }
  std::string resolveError;
  ResolvedType resolvedType =
      resolveTypeString(targetTypeText, mapping, allowedParams, namespacePrefix, ctx, resolveError);
  if (!resolveError.empty() || resolvedType.text.empty()) {
    return nullptr;
  }
  targetTypeText = semantics::normalizeBindingTypeName(resolvedType.text);
  std::string base;
  std::string argText;
  if (semantics::splitTemplateTypeName(targetTypeText, base, argText)) {
    targetTypeText = semantics::normalizeBindingTypeName(base);
  }
  std::string sumPath = targetTypeText;
  if (sumPath.empty()) {
    return nullptr;
  }
  if (sumPath.front() != '/') {
    sumPath = resolveNameToPath(sumPath,
                                namespacePrefix,
                                scopedImportAliasesForNamespace(namespacePrefix, ctx),
                                ctx.sourceDefs);
  }
  auto sumIt = ctx.sourceDefs.find(sumPath);
  if (sumIt == ctx.sourceDefs.end() || !isSumDefinitionForMonomorphRefresh(sumIt->second)) {
    return nullptr;
  }
  return &sumIt->second;
}

void appendPickPayloadLocal([[maybe_unused]] ExprRewriteContext &rc,
                            const SumVariant &variant,
                            const Expr &arm,
                            LocalTypeMap &armLocals) {

  if (!variant.hasPayload || arm.args.size() != 1) {
    return;
  }
  const Expr &payloadBinder = arm.args.front();
  if (payloadBinder.kind != Expr::Kind::Name || payloadBinder.name.empty()) {
    return;
  }
  semantics::BindingInfo payloadInfo;
  payloadInfo.typeName = variant.payloadType;
  if (!variant.payloadTemplateArgs.empty()) {
    payloadInfo.typeTemplateArg = semantics::joinTemplateArgs(variant.payloadTemplateArgs);
  }
  armLocals[payloadBinder.name] = std::move(payloadInfo);
}

void recordBodyBindingLocal(ExprRewriteContext &rc, Expr &bodyExpr, LocalTypeMap &bodyLocals) {
  Context &ctx = rc.ctx;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &allowMathBare = rc.allowMathBare;

  semantics::BindingInfo info;
  if (extractExplicitBindingType(bodyExpr, info)) {
    if (info.typeName == "auto" && bodyExpr.args.size() == 1 &&
        inferBindingTypeForMonomorph(
            bodyExpr.args.front(), params, bodyLocals, allowMathBare, ctx, info)) {
      bodyLocals[bodyExpr.name] = info;
    } else {
      bodyLocals[bodyExpr.name] = info;
    }
    return;
  }
  if (bodyExpr.isBinding && bodyExpr.args.size() == 1 &&
      inferBindingTypeForMonomorph(
          bodyExpr.args.front(), params, bodyLocals, allowMathBare, ctx, info)) {
    bodyLocals[bodyExpr.name] = info;
  }
}

} // namespace primec
