#include "TemplateMonomorphAssignmentTargetResolution.h"
#include "primec/support/Ascii.h"
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

// The later rules of rewriteExpr: helper templates, plain call references, method and builtin targets and the
// child expressions.

PhaseStatus rewriteCollectionHelperTemplate(ExprRewriteContext &rc) {
  Expr &expr = rc.expr;
  const SubstMap &mapping = rc.mapping;
  const std::unordered_set<std::string> &allowedParams = rc.allowedParams;
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  std::string &error = rc.error;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &allowMathBare = rc.allowMathBare;
  auto &allConcrete = rc.allConcrete;
  auto rewriteCanonicalKeyValueConstructorBinding = [&](Expr &bindingExpr) -> bool {
    return rewriteExperimentalConstructorBinding(
        bindingExpr,
        params,
        locals,
        allowMathBare,
        ctx,
        [&](const std::string &bindingTypeText) {
          return resolvesExperimentalKeyValueTypeText(
              bindingTypeText, mapping, allowedParams, namespacePrefix, ctx);
        },
        "map",
        [&](auto &&...args) {
          return rewriteKeyValueTargetValueForResolvedType(rc,
                                                           std::forward<decltype(args)>(args)...);
        });
  };
  auto rewriteCanonicalExperimentalVectorConstructorBinding = [&](Expr &bindingExpr) -> bool {
    return rewriteExperimentalConstructorBinding(
        bindingExpr,
        params,
        locals,
        allowMathBare,
        ctx,
        [&](const std::string &bindingTypeText) {
          return resolvesCollectionVectorValueTypeText(bindingTypeText);
        },
        "vector",
        [&](auto &&...args) {
          return rewriteVectorTargetValueForResolvedType(rc, std::forward<decltype(args)>(args)...);
        });
  };
  if (expr.isBinding) {
    if (!rewriteCanonicalExperimentalVectorConstructorBinding(expr)) {
      return rc.done(false);
    }
    if (!rewriteCanonicalKeyValueConstructorBinding(expr)) {
      return rc.done(false);
    }
  }
  if (!expr.isBinding) {
    if (!expr.isMethodCall && expr.namespacePrefix.empty() &&
        expr.name.find('/') == std::string::npos && expr.args.size() == 1 &&
        !hasNamedCallArguments(expr) && expr.templateArgs.empty() && !expr.hasBodyArguments &&
        expr.bodyArguments.empty()) {
      semantics::BindingInfo receiverInfo;
      if (inferBindingTypeForMonomorph(
              expr.args.front(), params, locals, allowMathBare, ctx, receiverInfo) &&
          receiverInfo.typeTemplateArg.empty()) {
        const std::string receiverType = semantics::normalizeBindingTypeName(receiverInfo.typeName);
        if (receiverType == "i32" || receiverType == "i64" || receiverType == "u64" ||
            receiverType == "u8" || receiverType == "bool" || receiverType == "f32" ||
            receiverType == "f64" || receiverType == "integer" || receiverType == "decimal" ||
            receiverType == "complex") {
          const std::string methodStylePath = "/" + receiverType + "/" + expr.name;
          if (ctx.sourceDefs.count(methodStylePath) > 0 ||
              ctx.helperOverloads.count(methodStylePath) > 0) {
            expr.name = methodStylePath;
          }
        }
      }
    }
    if (expr.isMethodCall && (expr.name == "at" || expr.name == "at_unsafe") &&
        expr.namespacePrefix.empty() && !expr.args.empty() &&
        inferCollectionReceiverFamilyForRewrite(rc, &expr.args.front()) == "string") {
      expr.isMethodCall = false;
      expr.namespacePrefix.clear();
    }
    auto helperReturnSoaRefHelper = [&]() -> std::string {
      if (expr.args.empty() || expr.args.front().kind != Expr::Kind::Call) {
        return std::string{};
      }
      std::string normalizedName = expr.name;
      if (!normalizedName.empty() && normalizedName.front() == '/') {
        normalizedName.erase(normalizedName.begin());
      }
      std::string normalizedPrefix = expr.namespacePrefix;
      if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
        normalizedPrefix.erase(normalizedPrefix.begin());
      }
      const bool usesCanonicalSoaSurface =
          normalizedName.rfind(templateMonomorphCompatibilitySoaHelperPrefix(false), 0) == 0 ||
          normalizedName.rfind(templateMonomorphPublicSoaHelperPrefix(false), 0) == 0 ||
          normalizedPrefix ==
              semantics::trimLeadingSlash(semantics::compatibilitySoaHelperTargetPath("")) ||
          normalizedPrefix == semantics::trimLeadingSlash(semantics::publicSoaHelperTargetPath(""));
      if (!usesCanonicalSoaSurface) {
        return std::string{};
      }
      if (collection_helpers::isGetHelperName(normalizedName) ||
          collection_helpers::isRefHelperName(normalizedName)) {
        return normalizedName;
      }
      if (normalizedName == templateMonomorphSamePathSoaHelperPrefix(false) + "get" ||
          normalizedName == templateMonomorphCompatibilitySoaHelperPrefix(false) + "get" ||
          normalizedName == "soa/get" ||
          normalizedName == templateMonomorphPublicSoaHelperPrefix(false) + "get") {
        return std::string("get");
      }
      if (normalizedName ==
              templateMonomorphSamePathSoaHelperPrefix(false) + collection_helpers::kGetRef ||
          normalizedName ==
              templateMonomorphCompatibilitySoaHelperPrefix(false) + collection_helpers::kGetRef ||
          normalizedName == "soa/get_ref" ||
          normalizedName ==
              templateMonomorphPublicSoaHelperPrefix(false) + collection_helpers::kGetRef) {
        return std::string(collection_helpers::kGetRef);
      }
      if (normalizedName == templateMonomorphSamePathSoaHelperPrefix(false) + "ref" ||
          normalizedName == templateMonomorphCompatibilitySoaHelperPrefix(false) + "ref" ||
          normalizedName == "soa/ref" ||
          normalizedName == templateMonomorphPublicSoaHelperPrefix(false) + "ref") {
        return std::string("ref");
      }
      if (normalizedName ==
              templateMonomorphSamePathSoaHelperPrefix(false) + collection_helpers::kRefRef ||
          normalizedName ==
              templateMonomorphCompatibilitySoaHelperPrefix(false) + collection_helpers::kRefRef ||
          normalizedName == "soa/ref_ref" ||
          normalizedName ==
              templateMonomorphPublicSoaHelperPrefix(false) + collection_helpers::kRefRef) {
        return std::string(collection_helpers::kRefRef);
      }
      if ((normalizedPrefix == templateMonomorphSoaReceiverTypeName() ||
           normalizedPrefix ==
               semantics::trimLeadingSlash(semantics::compatibilitySoaHelperTargetPath("")) ||
           normalizedPrefix == "soa" ||
           normalizedPrefix ==
               semantics::trimLeadingSlash(semantics::publicSoaHelperTargetPath(""))) &&
          (collection_helpers::isGetHelperName(normalizedName) ||
           collection_helpers::isRefHelperName(normalizedName))) {
        return normalizedName;
      }
      return std::string{};
    }();
    if (!helperReturnSoaRefHelper.empty() &&
        ctx.sourceDefs.count(templateMonomorphSamePathSoaHelperPrefix() +
                             helperReturnSoaRefHelper) == 0 &&
        ctx.helperOverloads.count(templateMonomorphSamePathSoaHelperPrefix() +
                                  helperReturnSoaRefHelper) == 0 &&
        !resolvesSoaReceiverForRewrite(rc, expr.args.front())) {
      if (expr.args.size() != 2) {
        error = "argument count mismatch for builtin " + helperReturnSoaRefHelper;
        return rc.done(false);
      }
      const Expr &indexExpr = expr.args[1];
      if (indexExpr.kind == Expr::Kind::BoolLiteral || indexExpr.kind == Expr::Kind::FloatLiteral ||
          indexExpr.kind == Expr::Kind::StringLiteral) {
        error = helperReturnSoaRefHelper + " requires integer index";
        return rc.done(false);
      }
      const std::string unavailablePath =
          (collection_helpers::isGetHelperName(helperReturnSoaRefHelper))
              ? semantics::compatibilitySoaHelperTargetPath(helperReturnSoaRefHelper)
              : templateMonomorphSamePathSoaHelperPrefix() + helperReturnSoaRefHelper;
      error = semantics::soaUnavailableMethodDiagnostic(unavailablePath);
      return rc.done(false);
    }
  }
  const bool isPackAtIntrinsic = !expr.isMethodCall && !expr.isBinding && expr.name == "pack_at";
  const bool isTypeofSymbolIntrinsic =
      !expr.isMethodCall && !expr.isBinding && expr.name == "typeof";
  rc.allConcrete = true;
  if (!isPackAtIntrinsic && !isTypeofSymbolIntrinsic &&
      !resolveTemplateArgumentList(
          expr.templateArgs, mapping, allowedParams, namespacePrefix, ctx, error, allConcrete)) {
    return rc.done(false);
  }
  if (isPackAtIntrinsic) {
    if (expr.templateArgs.size() != 2) {
      error = "pack_at requires integer index and field-stem template arguments";
      return rc.done(false);
    }
    if (expr.args.size() != 1 || hasNamedCallArguments(expr) || expr.hasBodyArguments ||
        !expr.bodyArguments.empty()) {
      error = "pack_at requires exactly one receiver argument";
      return rc.done(false);
    }
    uint64_t packIndex = 0;
    if (!resolveTemplateIndexText(expr.templateArgs[0], mapping, packIndex)) {
      error = "pack_at index must resolve to an integer template argument: " + expr.templateArgs[0];
      return rc.done(false);
    }
    auto isPackAtFieldStem = [](const std::string &text) {
      if (text.empty() ||
          !(std::isalpha(static_cast<unsigned char>(text.front())) || text.front() == '_')) {
        return false;
      }
      for (const char ch : text) {
        if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_')) {
          return false;
        }
      }
      return true;
    };
    std::string fieldStem = trimAscii(expr.templateArgs[1]);
    if (!isPackAtFieldStem(fieldStem)) {
      error = "pack_at field stem must be an identifier: " + fieldStem;
      return rc.done(false);
    }
    if (ctx.currentRewriteDefinition != nullptr &&
        ctx.currentRewriteDefinition->templatePackBindings.size() == 1 &&
        packIndex >= ctx.currentRewriteDefinition->templatePackBindings.front().arguments.size()) {
      error = "pack_at index out of range: " + fieldStem + "[" + expr.templateArgs[0] + "] has " +
              std::to_string(
                  ctx.currentRewriteDefinition->templatePackBindings.front().arguments.size()) +
              " elements";
      return rc.done(false);
    }
    if (!rewriteExpr(expr.args.front(),
                     mapping,
                     allowedParams,
                     namespacePrefix,
                     ctx,
                     error,
                     locals,
                     params,
                     allowMathBare)) {
      return rc.done(false);
    }
    expr.name = generatedPackFieldName(fieldStem, static_cast<size_t>(packIndex));
    expr.sourceName = expr.name;
    expr.templateArgs.clear();
    expr.templateArgDetails.clear();
    expr.isMethodCall = true;
    expr.sourceIsMethodCall = true;
    expr.isFieldAccess = true;
    return rc.done(true);
  }
  return PhaseStatus::Continue;
}

PhaseStatus rewritePlainCallReference(ExprRewriteContext &rc) {
  Expr &expr = rc.expr;
  const SubstMap &mapping = rc.mapping;
  const std::unordered_set<std::string> &allowedParams = rc.allowedParams;
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  std::string &error = rc.error;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  if (!expr.isMethodCall && !expr.isBinding) {
    const PhaseStatus status = rewriteExprReference(
        expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, rc);
    if (status == PhaseStatus::Done) {
      return status;
    }
  }
  return PhaseStatus::Continue;
}

PhaseStatus rewriteMethodAndBuiltinTargets(ExprRewriteContext &rc) {
  Expr &expr = rc.expr;
  const SubstMap &mapping = rc.mapping;
  const std::unordered_set<std::string> &allowedParams = rc.allowedParams;
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  std::string &error = rc.error;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &allowMathBare = rc.allowMathBare;
  auto &allConcrete = rc.allConcrete;
  if (expr.isMethodCall) {
    if (!expr.args.empty()) {
      if (!rewriteNestedExperimentalKeyValueConstructorValue(rc, expr.args.front())) {
        return rc.done(false);
      }
      if (!rewriteNestedExperimentalVectorConstructorValue(rc, expr.args.front())) {
        return rc.done(false);
      }
    }
    unwrapDereferencedBorrowedVectorReceiver(expr, params, locals, allowMathBare, ctx);
    const bool methodCallSyntax = expr.isMethodCall;
    std::string methodPath;
    if (resolveMethodCallTemplateTarget(expr, locals, ctx, methodPath, &params)) {
      if (!ctx.requirementOverloadSelectionError.empty()) {
        error = ctx.requirementOverloadSelectionError;
        ctx.requirementOverloadSelectionError.clear();
        return rc.done(false);
      }
      const bool preserveExplicitCompatibilityTemplateMethodPath =
          !expr.templateArgs.empty() && shouldPreserveCompatibilityTemplatePath(methodPath, ctx);
      if (preserveExplicitCompatibilityTemplateMethodPath) {
        error = "template arguments are only supported on templated definitions: " +
                helperOverloadDisplayPath(methodPath, ctx);
        return rc.done(false);
      }
      const std::string preferredCollectionHelperMethodPath =
          preserveExplicitCompatibilityTemplateMethodPath
              ? methodPath
              : preferCanonicalStdlibCollectionHelperPath(rc, methodPath);
      if (!error.empty()) {
        return rc.done(false);
      }
      if (!preserveExplicitCompatibilityTemplateMethodPath &&
          preferredCollectionHelperMethodPath != methodPath) {
        methodPath = preferredCollectionHelperMethodPath;
        expr.name = preferredCollectionHelperMethodPath;
        expr.namespacePrefix.clear();
      }
      const std::string experimentalVectorMethodPath =
          experimentalVectorHelperPathForCanonicalHelper(methodPath);
      const std::string experimentalSoaVectorMethodPath =
          experimentalSoaVectorHelperPathForCanonicalHelper(methodPath);
      const bool shouldRewriteCanonicalVectorMethodToExperimental =
          !experimentalVectorMethodPath.empty() &&
          ctx.sourceDefs.count(experimentalVectorMethodPath) > 0 &&
          hasVisibleStdCollectionsImportForPath(ctx, methodPath);
      if (shouldRewriteCanonicalVectorMethodToExperimental &&
          !experimentalVectorMethodPath.empty() &&
          ctx.sourceDefs.count(experimentalVectorMethodPath) > 0 &&
          resolvesCollectionVectorValueReceiver(collectionHelperReceiverExpr(rc, expr),
                                                params,
                                                locals,
                                                allowMathBare,
                                                namespacePrefix,
                                                ctx)) {
        methodPath = experimentalVectorMethodPath;
        if (expr.templateArgs.empty()) {
          std::vector<std::string> receiverTemplateArgs;
          if (resolveCollectionVectorValueReceiverTemplateArgs(
                  collectionHelperReceiverExpr(rc, expr),
                  params,
                  locals,
                  allowMathBare,
                  namespacePrefix,
                  ctx,
                  receiverTemplateArgs)) {
            expr.templateArgs = std::move(receiverTemplateArgs);
            allConcrete = true;
          }
        }
      }
      if (expr.templateArgs.empty() && collection_helpers::isBorrowedVectorHelperPath(methodPath)) {
        // TODO-5375: element type of the borrowed `Reference<vector<T>>` receiver.
        semantics::BindingInfo borrowedReceiver;
        std::string vectorBase;
        std::string elementType;
        const Expr *borrowedReceiverExpr = collectionHelperReceiverExpr(rc, expr);
        if (borrowedReceiverExpr != nullptr &&
            inferBindingTypeForMonomorph(
                *borrowedReceiverExpr, params, locals, allowMathBare, ctx, borrowedReceiver) &&
            semantics::normalizeBindingTypeName(borrowedReceiver.typeName) == "Reference" &&
            semantics::splitTemplateTypeName(
                semantics::normalizeBindingTypeName(borrowedReceiver.typeTemplateArg),
                vectorBase,
                elementType) &&
            semantics::normalizeBindingTypeName(vectorBase) == "vector" && !elementType.empty()) {
          expr.templateArgs = {elementType};
          allConcrete = true;
        }
      }
      const bool methodWasTemplate = ctx.templateDefs.count(methodPath) > 0;
      if (!expr.templateArgs.empty() && !methodWasTemplate) {
        if (!shouldPreserveCompatibilityTemplatePath(methodPath, ctx)) {
          methodPath = preferVectorStdlibTemplatePath(methodPath, ctx);
        }
      }
      if (expr.templateArgs.empty()) {
        methodPath = preferVectorStdlibImplicitTemplatePath(
            expr, methodPath, locals, params, allowMathBare, ctx, namespacePrefix);
      }
      const bool shouldRewriteCanonicalSoaMethodToExperimental =
          ctx.sourceDefs.count(methodPath) == 0 && ctx.helperOverloads.count(methodPath) == 0 &&
          !isCanonicalSoaBorrowedWrapperHelper(rc, methodPath) &&
          hasVisibleStdCollectionsImportForPath(ctx, methodPath) &&
          resolvesConcreteExperimentalSoaVectorReceiver(rc, collectionHelperReceiverExpr(rc, expr));
      if (shouldRewriteCanonicalSoaMethodToExperimental &&
          !experimentalSoaVectorMethodPath.empty()) {
        methodPath = experimentalSoaVectorMethodPath;
        if (expr.templateArgs.empty()) {
          std::vector<std::string> receiverTemplateArgs;
          if (resolveExperimentalSoaVectorReceiverTemplateArgs(
                  rc, collectionHelperReceiverExpr(rc, expr), receiverTemplateArgs)) {
            expr.templateArgs = std::move(receiverTemplateArgs);
            allConcrete = true;
          }
        }
      }
      if (expr.templateArgs.empty() && isCollectionVectorPublicHelperPath(methodPath) &&
          resolvesCollectionVectorValueReceiver(collectionHelperReceiverExpr(rc, expr),
                                                params,
                                                locals,
                                                allowMathBare,
                                                namespacePrefix,
                                                ctx)) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveCollectionVectorValueReceiverTemplateArgs(collectionHelperReceiverExpr(rc, expr),
                                                             params,
                                                             locals,
                                                             allowMathBare,
                                                             namespacePrefix,
                                                             ctx,
                                                             receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
          allConcrete = true;
        }
      }
      if (expr.templateArgs.empty() && isExperimentalSoaVectorPublicHelperPath(methodPath) &&
          resolvesExperimentalSoaVectorReceiver(rc, collectionHelperReceiverExpr(rc, expr))) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalSoaVectorReceiverTemplateArgs(
                rc, collectionHelperReceiverExpr(rc, expr), receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
          allConcrete = true;
        }
      }
      if (expr.templateArgs.empty() && isTemplateMonomorphCanonicalKeyValueHelperPath(methodPath) &&
          hasVisibleStdCollectionsImportForPath(ctx, methodPath)) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalKeyValueReceiverTemplateArgs(collectionHelperReceiverExpr(rc, expr),
                                                            params,
                                                            locals,
                                                            allowMathBare,
                                                            namespacePrefix,
                                                            ctx,
                                                            receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
          allConcrete = true;
        }
      }
      if (expr.templateArgs.empty() && isCanonicalSoaBorrowedWrapperHelper(rc, methodPath) &&
          resolvesExperimentalSoaVectorReceiver(rc, collectionHelperReceiverExpr(rc, expr))) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalSoaVectorReceiverTemplateArgs(
                rc, collectionHelperReceiverExpr(rc, expr), receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
          allConcrete = true;
        }
      }
      if (methodPath.rfind(experimentalCollectionConstructorRootLocal("map"), 0) == 0 &&
          methodPath.find("__t") != std::string::npos) {
        expr.templateArgs.clear();
      }
      if (methodPath.rfind(semantics::legacyExperimentalVectorCompatibilityPrefix(), 0) == 0 &&
          methodPath.find("__t") != std::string::npos) {
        expr.templateArgs.clear();
      }
      if (isExperimentalSoaVectorPublicHelperPath(methodPath) &&
          methodPath.find("__t") != std::string::npos) {
        expr.templateArgs.clear();
      }
      if (isCanonicalSoaBorrowedWrapperHelper(rc, methodPath) &&
          methodPath.find("__t") != std::string::npos) {
        expr.templateArgs.clear();
      }
      const bool isStaticFileErrorHelperCall =
          expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
          semantics::normalizeBindingTypeName(expr.args.front().name) == "FileError" &&
          methodPath.rfind("/std/file/FileError/", 0) == 0;
      if (ctx.helperOverloadInternalToPublic.count(methodPath) > 0) {
        expr.name = methodPath;
        expr.namespacePrefix.clear();
      }
      const Expr *resolvedReceiverExpr = collectionHelperReceiverExpr(rc, expr);
      const std::string resolvedReceiverFamily =
          inferCollectionReceiverFamily(rc, resolvedReceiverExpr);
      const bool isSyntheticSamePathSoaHelperTemplateCarry =
          isSyntheticSamePathSoaHelperTemplateCarryPath(rc, methodPath) &&
          ctx.templateDefs.count(methodPath) == 0 && !expr.templateArgs.empty() &&
          resolvedReceiverExpr != nullptr &&
          ((resolvedReceiverExpr->kind == Expr::Kind::Call && !resolvedReceiverExpr->isBinding) ||
           resolvedReceiverFamily == "vector");
      if (isSyntheticSamePathSoaHelperTemplateCarry) {
        expr.templateArgs.clear();
      }
      if (isStaticFileErrorHelperCall) {
        expr.name = methodPath;
        expr.namespacePrefix.clear();
        expr.isMethodCall = false;
        if (!expr.args.empty()) {
          expr.args.erase(expr.args.begin());
        }
        if (!expr.argNames.empty()) {
          expr.argNames.erase(expr.argNames.begin());
        }
      }
      if (expr.templateArgs.empty() && ctx.templateDefs.count(methodPath) > 0) {
        auto defIt = ctx.sourceDefs.find(methodPath);
        const Expr *receiverExpr = collectionHelperReceiverExpr(rc, expr);
        if (defIt != ctx.sourceDefs.end() && receiverExpr != nullptr &&
            !defIt->second.parameters.empty()) {
          semantics::BindingInfo receiverInfo;
          semantics::BindingInfo receiverParamInfo;
          if (inferBindingTypeForMonomorph(
                  *receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
              extractExplicitBindingType(defIt->second.parameters.front(), receiverParamInfo) &&
              !receiverInfo.typeTemplateArg.empty()) {
            std::string receiverBase = semantics::normalizeBindingTypeName(receiverInfo.typeName);
            std::string paramBase = semantics::normalizeBindingTypeName(receiverParamInfo.typeName);
            if (!receiverBase.empty() && receiverBase.front() == '/') {
              receiverBase.erase(receiverBase.begin());
            }
            if (!paramBase.empty() && paramBase.front() == '/') {
              paramBase.erase(paramBase.begin());
            }
            auto leafName = [](const std::string &path) {
              const size_t slash = path.find_last_of('/');
              return slash == std::string::npos ? path : path.substr(slash + 1);
            };
            if (receiverBase == paramBase || (!receiverBase.empty() && !paramBase.empty() &&
                                              leafName(receiverBase) == leafName(paramBase))) {
              std::vector<std::string> receiverTemplateArgs;
              if (semantics::splitTopLevelTemplateArgs(receiverInfo.typeTemplateArg,
                                                       receiverTemplateArgs) &&
                  receiverTemplateArgs.size() == defIt->second.templateArgs.size()) {
                expr.templateArgs = std::move(receiverTemplateArgs);
                allConcrete = true;
              }
            }
          }
        }
      }
      const bool isTemplateDef = ctx.templateDefs.count(methodPath) > 0;
      const bool isKnownDef = ctx.sourceDefs.count(methodPath) > 0;
      if (isTemplateDef) {
        auto defIt = ctx.sourceDefs.find(methodPath);
        const bool shouldInferImplicitTemplateTail =
            defIt != ctx.sourceDefs.end() && !expr.templateArgs.empty() &&
            ctx.implicitTemplateDefs.count(methodPath) > 0 &&
            expr.templateArgs.size() < defIt->second.templateArgs.size();
        if (expr.templateArgs.empty() || shouldInferImplicitTemplateTail) {
          if (defIt != ctx.sourceDefs.end()) {
            std::vector<std::string> inferredArgs;
            if (inferImplicitTemplateArgs(defIt->second,
                                          expr,
                                          locals,
                                          params,
                                          mapping,
                                          allowedParams,
                                          namespacePrefix,
                                          ctx,
                                          allowMathBare,
                                          inferredArgs,
                                          error)) {
              expr.templateArgs = std::move(inferredArgs);
              allConcrete = true;
            } else if (!error.empty()) {
              return rc.done(false);
            }
          }
        }
        if (expr.templateArgs.empty()) {
          if (defIt != ctx.sourceDefs.end() &&
              definitionAllowsEmptyTypePackSpecialization(defIt->second)) {
            allConcrete = true;
          } else if (shouldDeferStdlibCollectionHelperTemplateRewrite(rc, methodPath)) {
            return rc.done(true);
          } else {
            error = "template arguments required for " + helperOverloadDisplayPath(methodPath, ctx);
            return rc.done(false);
          }
        }
        if (allConcrete) {
          std::string specializedPath;
          if (!instantiateTemplate(
                  methodPath,
                  expr.templateArgs,
                  matchingTemplateArgumentDetails(expr.templateArgs, expr.templateArgDetails),
                  ctx,
                  error,
                  specializedPath)) {
            return rc.done(false);
          }
          expr.name = specializedPath;
          expr.templateArgs.clear();
          expr.templateArgDetails.clear();
          expr.isMethodCall = false;
        }
      } else if (isKnownDef && !expr.templateArgs.empty()) {
        const Expr *templateCarryReceiverExpr = collectionHelperReceiverExpr(rc, expr);
        const std::string templateCarryReceiverFamily =
            inferCollectionReceiverFamily(rc, templateCarryReceiverExpr);
        if (isSyntheticSamePathSoaHelperTemplateCarryPath(rc, methodPath) &&
            ctx.templateDefs.count(methodPath) == 0 && templateCarryReceiverExpr != nullptr &&
            ((templateCarryReceiverExpr->kind == Expr::Kind::Call &&
              !templateCarryReceiverExpr->isBinding) ||
             templateCarryReceiverFamily == "vector" ||
             isTemplateMonomorphSoaReceiverType(templateCarryReceiverFamily))) {
          expr.templateArgs.clear();
        } else {
          error = "template arguments are only supported on templated definitions: " +
                  helperOverloadDisplayPath(methodPath, ctx);
          return rc.done(false);
        }
      }
      std::string rewrittenMethodPath =
          expr.isMethodCall ? methodPath : resolveCalleePath(expr, namespacePrefix, ctx);
      expr.resolvedCallPath = rewrittenMethodPath;
      auto methodDefIt = ctx.sourceDefs.find(rewrittenMethodPath);
      if (methodDefIt == ctx.sourceDefs.end()) {
        const std::string fallbackMethodResolvedCallPath =
            resolveCalleePath(expr, namespacePrefix, ctx);
        expr.resolvedCallPath = fallbackMethodResolvedCallPath;
        methodDefIt = ctx.sourceDefs.find(fallbackMethodResolvedCallPath);
      }
      if (methodDefIt != ctx.sourceDefs.end()) {
        if (!rewriteExperimentalConstructorArgsForTarget(
                expr,
                methodDefIt->second,
                methodCallSyntax,
                allowMathBare,
                ctx,
                [&](const std::string &typeText, Expr &argExpr) {
                  return rewriteVectorTargetValueForResolvedType(rc, typeText, argExpr);
                })) {
          return rc.done(false);
        }
        if (!rewriteExperimentalConstructorArgsForTarget(
                expr,
                methodDefIt->second,
                methodCallSyntax,
                allowMathBare,
                ctx,
                [&](const std::string &typeText, Expr &argExpr) {
                  return rewriteKeyValueTargetValueForResolvedType(rc, typeText, argExpr);
                })) {
          return rc.done(false);
        }
      }
    }
  }
  return PhaseStatus::Continue;
}

PhaseStatus rewriteChildExpressions(ExprRewriteContext &rc) {
  Expr &expr = rc.expr;
  const SubstMap &mapping = rc.mapping;
  const std::unordered_set<std::string> &allowedParams = rc.allowedParams;
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  std::string &error = rc.error;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &allowMathBare = rc.allowMathBare;
  for (auto &arg : expr.args) {
    if (!rewriteExpr(arg,
                     mapping,
                     allowedParams,
                     namespacePrefix,
                     ctx,
                     error,
                     locals,
                     params,
                     allowMathBare)) {
      return rc.done(false);
    }
  }
  LocalTypeMap bodyLocals = locals;
  for (size_t bodyIndex = 0; bodyIndex < expr.bodyArguments.size(); ++bodyIndex) {
    bool expandedDestructuring = false;
    if (!expandDestructuringStatementAt(expr.bodyArguments,
                                        bodyIndex,
                                        params,
                                        bodyLocals,
                                        namespacePrefix,
                                        ctx,
                                        error,
                                        expandedDestructuring)) {
      return rc.done(false);
    }
    if (expandedDestructuring) {
      --bodyIndex;
      continue;
    }
    auto &arg = expr.bodyArguments[bodyIndex];
    if (!rewriteExpr(arg,
                     mapping,
                     allowedParams,
                     namespacePrefix,
                     ctx,
                     error,
                     bodyLocals,
                     params,
                     allowMathBare)) {
      return rc.done(false);
    }
    semantics::BindingInfo info;
    if (extractExplicitBindingType(arg, info)) {
      if (info.typeName == "auto" && arg.args.size() == 1 &&
          inferBindingTypeForMonomorph(
              arg.args.front(), params, bodyLocals, allowMathBare, ctx, info)) {
        bodyLocals[arg.name] = info;
      } else {
        bodyLocals[arg.name] = info;
      }
    } else if (arg.isBinding && arg.args.size() == 1) {
      if (inferBindingTypeForMonomorph(
              arg.args.front(), params, bodyLocals, allowMathBare, ctx, info)) {
        bodyLocals[arg.name] = info;
      }
    }
  }
  std::string builtinAccessName;
  if (!expr.isMethodCall && semantics::getBuiltinArrayAccessName(expr, builtinAccessName)) {
    expr.namespacePrefix.clear();
    size_t receiverIndex = 0;
    if (hasNamedCallArguments(expr)) {
      for (size_t i = 0; i < expr.argNames.size() && i < expr.args.size(); ++i) {
        if (expr.argNames[i].has_value() && *expr.argNames[i] == "values") {
          receiverIndex = i;
          break;
        }
      }
    }
    if (receiverIndex < expr.args.size() && expr.args[receiverIndex].kind == Expr::Kind::Name) {
      expr.args[receiverIndex].namespacePrefix.clear();
    }
  }
  return rc.done(true);
  return PhaseStatus::Continue;
}

} // namespace primec
