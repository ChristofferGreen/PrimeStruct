#include "TemplateMonomorphAssignmentTargetResolution.h"
#include "TemplateMonomorphBindingBlockInference.h"
#include "TemplateMonomorphBindingCallInference.h"
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
#include "TemplateMonomorphExpressionRewriteState.h"

namespace primec {

PhaseStatus rewriteExprPhase7([[maybe_unused]] Expr &expr, [[maybe_unused]] const SubstMap &mapping, [[maybe_unused]] const std::unordered_set<std::string> &allowedParams, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] Context &ctx, [[maybe_unused]] std::string &error, [[maybe_unused]] const LocalTypeMap &locals, [[maybe_unused]] const std::vector<ParameterInfo> &params, RewriteExprState &st) {
  [[maybe_unused]] auto &allowMathBare = st.allowMathBare;
  [[maybe_unused]] auto &isSyntheticSamePathSoaHelperTemplateCarryPath = st.isSyntheticSamePathSoaHelperTemplateCarryPath;
  [[maybe_unused]] auto &collectionHelperReceiverExpr = st.collectionHelperReceiverExpr;
  [[maybe_unused]] auto &resolveExperimentalSoaVectorReceiverTemplateArgs = st.resolveExperimentalSoaVectorReceiverTemplateArgs;
  [[maybe_unused]] auto &resolvesExperimentalSoaVectorReceiver = st.resolvesExperimentalSoaVectorReceiver;
  [[maybe_unused]] auto &resolvesConcreteExperimentalSoaVectorReceiver = st.resolvesConcreteExperimentalSoaVectorReceiver;
  [[maybe_unused]] auto &inferCollectionReceiverFamily = st.inferCollectionReceiverFamily;
  [[maybe_unused]] auto &isCanonicalSoaBorrowedWrapperHelper = st.isCanonicalSoaBorrowedWrapperHelper;
  [[maybe_unused]] auto &preferCanonicalStdlibCollectionHelperPath = st.preferCanonicalStdlibCollectionHelperPath;
  [[maybe_unused]] auto &shouldDeferStdlibCollectionHelperTemplateRewrite = st.shouldDeferStdlibCollectionHelperTemplateRewrite;
  [[maybe_unused]] auto &rewriteNestedExperimentalKeyValueConstructorValue = st.rewriteNestedExperimentalKeyValueConstructorValue;
  [[maybe_unused]] auto &rewriteNestedExperimentalVectorConstructorValue = st.rewriteNestedExperimentalVectorConstructorValue;
  [[maybe_unused]] auto &rewriteKeyValueTargetValueForResolvedType = st.rewriteKeyValueTargetValueForResolvedType;
  [[maybe_unused]] auto &rewriteVectorTargetValueForResolvedType = st.rewriteVectorTargetValueForResolvedType;
  [[maybe_unused]] auto &allConcrete = st.allConcrete;
  if (expr.isMethodCall) {
    if (!expr.args.empty()) {
      if (!rewriteNestedExperimentalKeyValueConstructorValue(expr.args.front())) {
        return st.done(false);
      }
      if (!rewriteNestedExperimentalVectorConstructorValue(expr.args.front())) {
        return st.done(false);
      }
    }
    unwrapDereferencedBorrowedVectorReceiver(expr, params, locals, allowMathBare, ctx);
    const bool methodCallSyntax = expr.isMethodCall;
    std::string methodPath;
    if (resolveMethodCallTemplateTarget(expr, locals, ctx, methodPath)) {
      if (!ctx.requirementOverloadSelectionError.empty()) {
        error = ctx.requirementOverloadSelectionError;
        ctx.requirementOverloadSelectionError.clear();
        return st.done(false);
      }
      const bool preserveExplicitCompatibilityTemplateMethodPath =
          !expr.templateArgs.empty() &&
          shouldPreserveCompatibilityTemplatePath(methodPath, ctx);
      if (preserveExplicitCompatibilityTemplateMethodPath) {
        error = "template arguments are only supported on templated definitions: " +
                helperOverloadDisplayPath(methodPath, ctx);
        return st.done(false);
      }
      const std::string preferredCollectionHelperMethodPath =
          preserveExplicitCompatibilityTemplateMethodPath
              ? methodPath
              : preferCanonicalStdlibCollectionHelperPath(methodPath);
      if (!error.empty()) {
        return st.done(false);
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
          resolvesCollectionVectorValueReceiver(
              collectionHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
        methodPath = experimentalVectorMethodPath;
        if (expr.templateArgs.empty()) {
          std::vector<std::string> receiverTemplateArgs;
          if (resolveCollectionVectorValueReceiverTemplateArgs(
                  collectionHelperReceiverExpr(expr),
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
        BindingInfo borrowedReceiver;
        std::string vectorBase;
        std::string elementType;
        const Expr *borrowedReceiverExpr = collectionHelperReceiverExpr(expr);
        if (borrowedReceiverExpr != nullptr &&
            inferBindingTypeForMonomorph(*borrowedReceiverExpr, params, locals,
                                         allowMathBare, ctx, borrowedReceiver) &&
            normalizeBindingTypeName(borrowedReceiver.typeName) == "Reference" &&
            splitTemplateTypeName(normalizeBindingTypeName(borrowedReceiver.typeTemplateArg), vectorBase,
                                  elementType) &&
            normalizeBindingTypeName(vectorBase) == "vector" && !elementType.empty()) {
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
          methodPath =
            preferVectorStdlibImplicitTemplatePath(expr, methodPath, locals, params, allowMathBare, ctx, namespacePrefix);
      }
      const bool shouldRewriteCanonicalSoaMethodToExperimental =
          ctx.sourceDefs.count(methodPath) == 0 &&
          ctx.helperOverloads.count(methodPath) == 0 &&
          !isCanonicalSoaBorrowedWrapperHelper(methodPath) &&
          hasVisibleStdCollectionsImportForPath(ctx, methodPath) &&
          resolvesConcreteExperimentalSoaVectorReceiver(
              collectionHelperReceiverExpr(expr));
      if (shouldRewriteCanonicalSoaMethodToExperimental &&
          !experimentalSoaVectorMethodPath.empty()) {
        methodPath = experimentalSoaVectorMethodPath;
        if (expr.templateArgs.empty()) {
          std::vector<std::string> receiverTemplateArgs;
          if (resolveExperimentalSoaVectorReceiverTemplateArgs(collectionHelperReceiverExpr(expr), receiverTemplateArgs)) {
            expr.templateArgs = std::move(receiverTemplateArgs);
            allConcrete = true;
          }
        }
      }
      if (expr.templateArgs.empty() &&
          isCollectionVectorPublicHelperPath(methodPath) &&
          resolvesCollectionVectorValueReceiver(
              collectionHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveCollectionVectorValueReceiverTemplateArgs(
                collectionHelperReceiverExpr(expr),
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
      if (expr.templateArgs.empty() &&
          isExperimentalSoaVectorPublicHelperPath(methodPath) &&
          resolvesExperimentalSoaVectorReceiver(collectionHelperReceiverExpr(expr))) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalSoaVectorReceiverTemplateArgs(collectionHelperReceiverExpr(expr), receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
          allConcrete = true;
        }
      }
      if (expr.templateArgs.empty() &&
          isTemplateMonomorphCanonicalKeyValueHelperPath(methodPath) &&
          hasVisibleStdCollectionsImportForPath(ctx, methodPath)) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalKeyValueReceiverTemplateArgs(
                collectionHelperReceiverExpr(expr),
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
      if (expr.templateArgs.empty() &&
          isCanonicalSoaBorrowedWrapperHelper(methodPath) &&
          resolvesExperimentalSoaVectorReceiver(collectionHelperReceiverExpr(expr))) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalSoaVectorReceiverTemplateArgs(collectionHelperReceiverExpr(expr), receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
          allConcrete = true;
        }
      }
      if (methodPath.rfind(experimentalCollectionConstructorRootLocal("map"), 0) == 0 &&
          methodPath.find("__t") != std::string::npos) {
        expr.templateArgs.clear();
      }
      if (methodPath.rfind(legacyExperimentalVectorCompatibilityPrefix(), 0) == 0 &&
          methodPath.find("__t") != std::string::npos) {
        expr.templateArgs.clear();
      }
      if (isExperimentalSoaVectorPublicHelperPath(methodPath) &&
          methodPath.find("__t") != std::string::npos) {
        expr.templateArgs.clear();
      }
      if (isCanonicalSoaBorrowedWrapperHelper(methodPath) &&
          methodPath.find("__t") != std::string::npos) {
        expr.templateArgs.clear();
      }
      const bool isStaticFileErrorHelperCall =
          expr.isMethodCall && !expr.args.empty() &&
          expr.args.front().kind == Expr::Kind::Name &&
          normalizeBindingTypeName(expr.args.front().name) == "FileError" &&
          methodPath.rfind("/std/file/FileError/", 0) == 0;
      if (ctx.helperOverloadInternalToPublic.count(methodPath) > 0) {
        expr.name = methodPath;
        expr.namespacePrefix.clear();
      }
      const Expr *resolvedReceiverExpr = collectionHelperReceiverExpr(expr);
      const std::string resolvedReceiverFamily =
          inferCollectionReceiverFamily(resolvedReceiverExpr);
      const bool isSyntheticSamePathSoaHelperTemplateCarry =
          isSyntheticSamePathSoaHelperTemplateCarryPath(methodPath) &&
          ctx.templateDefs.count(methodPath) == 0 &&
          !expr.templateArgs.empty() &&
          resolvedReceiverExpr != nullptr &&
          ((resolvedReceiverExpr->kind == Expr::Kind::Call &&
            !resolvedReceiverExpr->isBinding) ||
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
        const Expr *receiverExpr = collectionHelperReceiverExpr(expr);
        if (defIt != ctx.sourceDefs.end() && receiverExpr != nullptr &&
            !defIt->second.parameters.empty()) {
          BindingInfo receiverInfo;
          BindingInfo receiverParamInfo;
          if (inferBindingTypeForMonomorph(*receiverExpr,
                                           params,
                                           locals,
                                           allowMathBare,
                                           ctx,
                                           receiverInfo) &&
              extractExplicitBindingType(defIt->second.parameters.front(),
                                         receiverParamInfo) &&
              !receiverInfo.typeTemplateArg.empty()) {
            std::string receiverBase = normalizeBindingTypeName(receiverInfo.typeName);
            std::string paramBase = normalizeBindingTypeName(receiverParamInfo.typeName);
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
            if (receiverBase == paramBase ||
                (!receiverBase.empty() && !paramBase.empty() &&
                 leafName(receiverBase) == leafName(paramBase))) {
              std::vector<std::string> receiverTemplateArgs;
              if (splitTopLevelTemplateArgs(receiverInfo.typeTemplateArg,
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
            defIt != ctx.sourceDefs.end() &&
            !expr.templateArgs.empty() &&
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
              return st.done(false);
            }
          }
        }
        if (expr.templateArgs.empty()) {
          if (defIt != ctx.sourceDefs.end() &&
              definitionAllowsEmptyTypePackSpecialization(defIt->second)) {
            allConcrete = true;
          } else if (shouldDeferStdlibCollectionHelperTemplateRewrite(methodPath)) {
            return st.done(true);
          } else {
            error = "template arguments required for " + helperOverloadDisplayPath(methodPath, ctx);
            return st.done(false);
          }
        }
        if (allConcrete) {
          std::string specializedPath;
          if (!instantiateTemplate(methodPath,
                                   expr.templateArgs,
                                   matchingTemplateArgumentDetails(expr.templateArgs,
                                                                   expr.templateArgDetails),
                                   ctx,
                                   error,
                                   specializedPath)) {
            return st.done(false);
          }
          expr.name = specializedPath;
          expr.templateArgs.clear();
          expr.templateArgDetails.clear();
          expr.isMethodCall = false;
        }
      } else if (isKnownDef && !expr.templateArgs.empty()) {
        const Expr *templateCarryReceiverExpr = collectionHelperReceiverExpr(expr);
        const std::string templateCarryReceiverFamily =
            inferCollectionReceiverFamily(templateCarryReceiverExpr);
        if (isSyntheticSamePathSoaHelperTemplateCarryPath(methodPath) &&
            ctx.templateDefs.count(methodPath) == 0 &&
            templateCarryReceiverExpr != nullptr &&
            ((templateCarryReceiverExpr->kind == Expr::Kind::Call &&
              !templateCarryReceiverExpr->isBinding) ||
             templateCarryReceiverFamily == "vector" ||
             isTemplateMonomorphSoaReceiverType(templateCarryReceiverFamily))) {
          expr.templateArgs.clear();
        } else {
        error = "template arguments are only supported on templated definitions: " +
                helperOverloadDisplayPath(methodPath, ctx);
        return st.done(false);
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
                  return rewriteVectorTargetValueForResolvedType(typeText, argExpr);
                })) {
          return st.done(false);
        }
        if (!rewriteExperimentalConstructorArgsForTarget(
                expr,
                methodDefIt->second,
                methodCallSyntax,
                allowMathBare,
                ctx,
                [&](const std::string &typeText, Expr &argExpr) {
                  return rewriteKeyValueTargetValueForResolvedType(typeText, argExpr);
                })) {
          return st.done(false);
        }
      }
    }
  }
  return PhaseStatus::Continue;
}

PhaseStatus rewriteExprPhase8([[maybe_unused]] Expr &expr, [[maybe_unused]] const SubstMap &mapping, [[maybe_unused]] const std::unordered_set<std::string> &allowedParams, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] Context &ctx, [[maybe_unused]] std::string &error, [[maybe_unused]] const LocalTypeMap &locals, [[maybe_unused]] const std::vector<ParameterInfo> &params, RewriteExprState &st) {
  [[maybe_unused]] auto &allowMathBare = st.allowMathBare;
  for (auto &arg : expr.args) {
    if (!rewriteExpr(arg, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, allowMathBare)) {
      return st.done(false);
    }
  }
  LocalTypeMap bodyLocals = locals;
  for (auto &arg : expr.bodyArguments) {
    if (!rewriteExpr(arg, mapping, allowedParams, namespacePrefix, ctx, error, bodyLocals, params, allowMathBare)) {
      return st.done(false);
    }
    BindingInfo info;
    if (extractExplicitBindingType(arg, info)) {
      if (info.typeName == "auto" && arg.args.size() == 1 &&
          inferBindingTypeForMonomorph(arg.args.front(), params, bodyLocals, allowMathBare, ctx, info)) {
        bodyLocals[arg.name] = info;
      } else {
        bodyLocals[arg.name] = info;
      }
    } else if (arg.isBinding && arg.args.size() == 1) {
      if (inferBindingTypeForMonomorph(arg.args.front(), params, bodyLocals, allowMathBare, ctx, info)) {
        bodyLocals[arg.name] = info;
      }
    }
  }
  std::string builtinAccessName;
  if (!expr.isMethodCall && getBuiltinArrayAccessName(expr, builtinAccessName)) {
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
    if (receiverIndex < expr.args.size() &&
        expr.args[receiverIndex].kind == Expr::Kind::Name) {
      expr.args[receiverIndex].namespacePrefix.clear();
    }
  }
  return st.done(true);
  return PhaseStatus::Continue;
}

} // namespace primec
