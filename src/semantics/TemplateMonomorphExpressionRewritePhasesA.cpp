// soa-surface-audit: exempt
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

PhaseStatus rewriteExprPhase2([[maybe_unused]] Expr &expr, [[maybe_unused]] const SubstMap &mapping, [[maybe_unused]] const std::unordered_set<std::string> &allowedParams, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] Context &ctx, [[maybe_unused]] std::string &error, [[maybe_unused]] const LocalTypeMap &locals, [[maybe_unused]] const std::vector<ParameterInfo> &params, RewriteExprState &st) {
  [[maybe_unused]] auto &allowMathBare = st.allowMathBare;
  [[maybe_unused]] auto &resolvePickSumDefinition = st.resolvePickSumDefinition;
  [[maybe_unused]] auto &appendPickPayloadLocal = st.appendPickPayloadLocal;
  [[maybe_unused]] auto &recordBodyBindingLocal = st.recordBodyBindingLocal;
  auto expandCurrentTypePackSpreadArguments =
      [&](std::vector<Expr> &arguments,
          std::vector<std::optional<std::string>> &argumentNames) {
        if (ctx.currentRewriteDefinition == nullptr ||
            ctx.currentRewriteDefinition->templatePackBindings.empty() ||
            arguments.empty()) {
          return;
        }
        const TemplatePackBinding &packBinding =
            ctx.currentRewriteDefinition->templatePackBindings.front();
        std::vector<Expr> expanded;
        std::vector<std::optional<std::string>> expandedNames;
        expanded.reserve(arguments.size());
        if (!argumentNames.empty()) {
          expandedNames.reserve(argumentNames.size() + packBinding.arguments.size());
        }
        auto spreadSourceName = [](const Expr &arg) -> std::string {
          if (arg.kind == Expr::Kind::Name) {
            return arg.name;
          }
          if (arg.kind == Expr::Kind::Call && !arg.isBinding &&
              !arg.isMethodCall && !arg.isFieldAccess && arg.args.empty() &&
              arg.argNames.empty() && arg.bodyArguments.empty() &&
              !arg.hasBodyArguments && arg.templateArgs.empty()) {
            return arg.name;
          }
          return {};
        };
        bool changed = false;
        for (size_t argIndex = 0; argIndex < arguments.size(); ++argIndex) {
          Expr &arg = arguments[argIndex];
          const bool hasArgName =
              !argumentNames.empty() && argIndex < argumentNames.size();
          const std::string spreadName = arg.isSpread ? spreadSourceName(arg)
                                                      : std::string{};
          if (!arg.isSpread || spreadName.empty()) {
            expanded.push_back(std::move(arg));
            if (!argumentNames.empty()) {
              expandedNames.push_back(hasArgName ? argumentNames[argIndex]
                                                 : std::nullopt);
            }
            continue;
          }

          bool allGeneratedBindingsVisible = true;
          for (size_t packIndex = 0; packIndex < packBinding.arguments.size();
               ++packIndex) {
            if (findBinding(params,
                            locals,
                            generatedPackFieldName(spreadName, packIndex)) ==
                nullptr) {
              allGeneratedBindingsVisible = false;
              break;
            }
          }
          if (!allGeneratedBindingsVisible) {
            expanded.push_back(std::move(arg));
            if (!argumentNames.empty()) {
              expandedNames.push_back(hasArgName ? argumentNames[argIndex]
                                                 : std::nullopt);
            }
            continue;
          }

          changed = true;
          for (size_t packIndex = 0; packIndex < packBinding.arguments.size();
               ++packIndex) {
            Expr expandedArg;
            expandedArg.kind = Expr::Kind::Name;
            expandedArg.name = generatedPackFieldName(spreadName, packIndex);
            expandedArg.sourceName = expandedArg.name;
            expandedArg.namespacePrefix = namespacePrefix;
            expandedArg.sourceLine = arg.sourceLine;
            expandedArg.sourceColumn = arg.sourceColumn;
            expanded.push_back(std::move(expandedArg));
            if (!argumentNames.empty()) {
              expandedNames.push_back(std::nullopt);
            }
          }
        }
        (void)changed;
        arguments = std::move(expanded);
        if (!argumentNames.empty()) {
          argumentNames = std::move(expandedNames);
        }
      };
  expandCurrentTypePackSpreadArguments(expr.args, expr.argNames);
  std::vector<std::optional<std::string>> bodyArgumentNames;
  expandCurrentTypePackSpreadArguments(expr.bodyArguments, bodyArgumentNames);
  (void)rewriteKeyValueWrapperHelperCallToMethod(
      expr, namespacePrefix, ctx, locals, params, allowMathBare);
  if (!expr.isMethodCall && !expr.isBinding && !expr.isFieldAccess &&
      expr.kind == Expr::Kind::Call && expr.transforms.empty() &&
      isSimpleCallName(expr, "wait") && expr.args.size() > 1 &&
      !expr.hasBodyArguments && expr.bodyArguments.empty() &&
      expr.templateArgs.empty()) {
    if (hasNamedCallArguments(expr)) {
      error = "wait does not accept named task handles";
      return st.done(false);
    }
    std::vector<std::string> tupleArgs;
    tupleArgs.reserve(expr.args.size());
    for (Expr &arg : expr.args) {
      if (!rewriteExpr(arg,
                       mapping,
                       allowedParams,
                       namespacePrefix,
                       ctx,
                       error,
                       locals,
                       params,
                       allowMathBare)) {
        return st.done(false);
      }
      if (arg.kind != Expr::Kind::Name) {
        error = "wait requires a task handle binding";
        return st.done(false);
      }
      BindingInfo taskBinding;
      if (!inferBindingTypeForMonomorph(arg,
                                        params,
                                        locals,
                                        allowMathBare,
                                        ctx,
                                        taskBinding)) {
        error = "wait requires a task handle binding";
        return st.done(false);
      }
      std::string taskBase = normalizeBindingTypeName(taskBinding.typeName);
      std::string taskResultType = taskBinding.typeTemplateArg;
      if (taskResultType.empty()) {
        std::string splitBase;
        std::string splitArg;
        if (splitTemplateTypeName(taskBase, splitBase, splitArg)) {
          taskBase = normalizeBindingTypeName(splitBase);
          taskResultType = splitArg;
        }
      }
      if (taskBase != "Task" || taskResultType.empty()) {
        error = "wait requires a task handle binding";
        return st.done(false);
      }
      ResolvedType resolvedArg =
          resolveTypeString(taskResultType, mapping, allowedParams, namespacePrefix, ctx, error);
      if (!error.empty()) {
        return st.done(false);
      }
      if (!resolvedArg.concrete) {
        error = "multi-wait task result types must be concrete";
        return st.done(false);
      }
      tupleArgs.push_back(std::move(resolvedArg.text));
    }
    std::string tuplePath;
    if (!instantiateTemplate("/std/tuple/tuple",
                             tupleArgs,
                             nullptr,
                             ctx,
                             error,
                             tuplePath)) {
      return st.done(false);
    }
    const std::string sourceName = expr.sourceName.empty() ? expr.name : expr.sourceName;
    expr.name = std::move(tuplePath);
    expr.sourceName = sourceName;
    expr.sourceIsMethodCall = false;
    expr.namespacePrefix.clear();
    expr.templateArgs.clear();
    expr.templateArgDetails.clear();
    expr.isMethodCall = false;
    expr.isFieldAccess = false;
    expr.isBraceConstructor = true;
    return st.done(true);
  }
  if (!expr.isMethodCall && !expr.isBinding && expr.kind == Expr::Kind::Call &&
      !expr.hasBodyArguments && expr.bodyArguments.empty() &&
      resolveCalleePath(expr, namespacePrefix, ctx) == "/std/tuple/make_tuple") {
    if (hasNamedCallArguments(expr)) {
      error = "named arguments cannot bind heterogeneous value-pack parameter: values";
      return st.done(false);
    }
    for (Expr &arg : expr.args) {
      if (arg.isSpread) {
        error = "heterogeneous value-pack inference does not support spread forwarding on /std/tuple/make_tuple";
        return st.done(false);
      }
      if (!rewriteExpr(arg,
                       mapping,
                       allowedParams,
                       namespacePrefix,
                       ctx,
                       error,
                       locals,
                       params,
                       allowMathBare)) {
        return st.done(false);
      }
    }
    std::vector<std::string> tupleArgs = expr.templateArgs;
    if (tupleArgs.empty()) {
      tupleArgs.reserve(expr.args.size());
      for (const Expr &arg : expr.args) {
        BindingInfo argInfo;
        if (!inferBindingTypeForMonomorph(arg,
                                          params,
                                          locals,
                                          allowMathBare,
                                          ctx,
                                          argInfo)) {
          error = "unable to infer implicit template arguments for /std/tuple/make_tuple";
          return st.done(false);
        }
        const std::string argType = bindingTypeToString(argInfo);
        if (argType.empty()) {
          error = "unable to infer implicit template arguments for /std/tuple/make_tuple";
          return st.done(false);
        }
        ResolvedType resolvedArg =
            resolveTypeString(argType, mapping, allowedParams, namespacePrefix, ctx, error);
        if (!error.empty()) {
          return st.done(false);
        }
        if (!resolvedArg.concrete) {
          error = "implicit template arguments must be concrete on /std/tuple/make_tuple";
          return st.done(false);
        }
        tupleArgs.push_back(resolvedArg.text);
      }
    } else {
      bool tupleArgsConcrete = true;
      if (!resolveTemplateArgumentList(tupleArgs,
                                       mapping,
                                       allowedParams,
                                       namespacePrefix,
                                       ctx,
                                       error,
                                       tupleArgsConcrete)) {
        return st.done(false);
      }
      if (!tupleArgsConcrete) {
        error = "implicit template arguments must be concrete on /std/tuple/make_tuple";
        return st.done(false);
      }
    }
    std::string tuplePath;
    if (!instantiateTemplate("/std/tuple/tuple",
                             tupleArgs,
                             nullptr,
                             ctx,
                             error,
                             tuplePath)) {
      return st.done(false);
    }
    expr.name = std::move(tuplePath);
    expr.sourceName = expr.name;
    expr.namespacePrefix.clear();
    expr.templateArgs.clear();
    expr.templateArgDetails.clear();
    expr.isBraceConstructor = true;
    return st.done(true);
  }
  if (isPickCall(expr)) {
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
        return st.done(false);
      }
    }

    Definition *sumDef =
        expr.args.empty() ? nullptr : resolvePickSumDefinition(expr.args.front());
    for (auto &arm : expr.bodyArguments) {
      arm.namespacePrefix = namespacePrefix;
      if (!rewriteTransforms(arm.transforms,
                             mapping,
                             allowedParams,
                             namespacePrefix,
                             ctx,
                             error)) {
        return st.done(false);
      }
      for (auto &armArg : arm.args) {
        armArg.namespacePrefix = namespacePrefix;
        if (!rewriteTransforms(armArg.transforms,
                               mapping,
                               allowedParams,
                               namespacePrefix,
                               ctx,
                               error)) {
          return st.done(false);
        }
      }
      LocalTypeMap armLocals = locals;
      if (sumDef != nullptr) {
        for (const auto &variant : sumDef->sumVariants) {
          if (variant.name == arm.name) {
            appendPickPayloadLocal(variant, arm, armLocals);
            break;
          }
        }
      }
      for (auto &bodyExpr : arm.bodyArguments) {
        if (!rewriteExpr(bodyExpr,
                         mapping,
                         allowedParams,
                         namespacePrefix,
                         ctx,
                         error,
                         armLocals,
                         params,
                         allowMathBare)) {
          return st.done(false);
        }
        recordBodyBindingLocal(bodyExpr, armLocals);
      }
    }
    return st.done(true);
  }
  return PhaseStatus::Continue;
}

PhaseStatus rewriteExprPhase3([[maybe_unused]] Expr &expr, [[maybe_unused]] const SubstMap &mapping, [[maybe_unused]] const std::unordered_set<std::string> &allowedParams, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] Context &ctx, [[maybe_unused]] std::string &error, [[maybe_unused]] const LocalTypeMap &locals, [[maybe_unused]] const std::vector<ParameterInfo> &params, RewriteExprState &st) {
  [[maybe_unused]] auto &allowMathBare = st.allowMathBare;
  st.isCanonicalBuiltinKeyValueHelperPath = [](const std::string &path) {
    return isTemplateMonomorphCanonicalKeyValueHelperPath(path);
  };
  [[maybe_unused]] auto &isCanonicalBuiltinKeyValueHelperPath = st.isCanonicalBuiltinKeyValueHelperPath;
  st.isCanonicalStdlibCollectionHelperPath = [&](const std::string &path) {
    if (isCanonicalBuiltinKeyValueHelperPath(path)) {
      return true;
    }
    auto canonicalizeSoaHelperPath = [](std::string canonicalPath) {
      const size_t specializationSuffix = canonicalPath.find("__");
      if (specializationSuffix != std::string::npos) {
        canonicalPath.erase(specializationSuffix);
      }
      return canonicalPath;
    };
    auto isCanonicalSoaHelperPath = [](const std::string &candidate,
                                       std::string_view helperName) {
      return (candidate.rfind(templateMonomorphCompatibilitySoaHelperPrefix(),
                              0) == 0 ||
              candidate.rfind(templateMonomorphPublicSoaHelperPrefix(), 0) == 0) &&
             isLegacyOrCanonicalSoaHelperPath(candidate, helperName);
    };
    const std::string canonicalSoaCountPath = canonicalizeSoaHelperPath(path);
    const std::string canonicalSoaGetPath =
        canonicalizeLegacySoaGetHelperPath(path);
    const std::string canonicalSoaToAosPath =
        canonicalizeLegacySoaToAosHelperPath(path);
    std::string vectorHelperName;
    return (resolveCanonicalVectorHelperNameFromResolvedPath(path, vectorHelperName) &&
            isVectorCompatibilityHelperName(vectorHelperName)) ||
           isCanonicalSoaHelperPath(canonicalSoaCountPath, "count") ||
           isCanonicalSoaHelperPath(canonicalSoaCountPath, collection_helpers::kCountRef) ||
           isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, "get") ||
           isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, collection_helpers::kGetRef) ||
           isCanonicalSoaRefLikeHelperPath(path) ||
           isCanonicalSoaHelperPath(canonicalSoaCountPath, "reserve") ||
           isCanonicalSoaHelperPath(canonicalSoaCountPath, "push") ||
           isLegacyOrCanonicalSoaHelperPath(
               canonicalSoaToAosPath, templateMonomorphSoaToAosHelperName()) ||
           isLegacyOrCanonicalSoaHelperPath(
               canonicalSoaToAosPath, templateMonomorphSoaToAosHelperName(true));
  };
  [[maybe_unused]] auto &isCanonicalStdlibCollectionHelperPath = st.isCanonicalStdlibCollectionHelperPath;
  st.isTemplatedAutoCompatVectorHelperPath = [](std::string_view path) {
    const std::string compatibilityPrefix =
        "/std/collections/" + std::string("vector");
    return path == compatibilityPrefix + "At" ||
           path == compatibilityPrefix + "AtUnsafe";
  };
  [[maybe_unused]] auto &isTemplatedAutoCompatVectorHelperPath = st.isTemplatedAutoCompatVectorHelperPath;
  st.isSyntheticSamePathSoaHelperTemplateCarryPath = [&](const std::string &path) {
    auto isSyntheticSamePathSoaCarryNonRefHelperPath = [](const std::string &candidate) {
      if (isLegacyOrCanonicalSoaHelperPath(candidate, "count") ||
          isLegacyOrCanonicalSoaHelperPath(candidate, collection_helpers::kCountRef) ||
          isLegacyOrCanonicalSoaHelperPath(candidate, "push") ||
          isLegacyOrCanonicalSoaHelperPath(candidate, "reserve")) {
        return true;
      }
      const std::string getCanonicalPath =
          canonicalizeLegacySoaGetHelperPath(candidate);
      return isLegacyOrCanonicalSoaHelperPath(getCanonicalPath, "get") ||
             isLegacyOrCanonicalSoaHelperPath(getCanonicalPath, collection_helpers::kGetRef);
    };
    const std::string canonicalPath = canonicalizeLegacySoaRefHelperPath(path);
    return isSyntheticSamePathSoaCarryNonRefHelperPath(path) ||
           isCanonicalSoaRefLikeHelperPath(canonicalPath) ||
           isExperimentalSoaGetLikeHelperPath(path) ||
           isExperimentalSoaRefLikeHelperPath(path);
  };
  [[maybe_unused]] auto &isSyntheticSamePathSoaHelperTemplateCarryPath = st.isSyntheticSamePathSoaHelperTemplateCarryPath;
  st.collectionHelperReceiverExpr = [&](const Expr &candidate) -> const Expr * {
    if (candidate.isMethodCall) {
      return candidate.args.empty() ? nullptr : &candidate.args.front();
    }
    if (candidate.args.empty()) {
      return nullptr;
    }
    if (hasNamedArguments(candidate.argNames)) {
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          return &candidate.args[i];
        }
      }
    }
    return &candidate.args.front();
  };
  [[maybe_unused]] auto &collectionHelperReceiverExpr = st.collectionHelperReceiverExpr;
  st.mutableCollectionHelperReceiverExpr = [&](Expr &candidate) -> Expr * {
    if (candidate.isMethodCall) {
      return candidate.args.empty() ? nullptr : &candidate.args.front();
    }
    if (candidate.args.empty()) {
      return nullptr;
    }
    if (hasNamedArguments(candidate.argNames)) {
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          return &candidate.args[i];
        }
      }
    }
    return &candidate.args.front();
  };
  [[maybe_unused]] auto &mutableCollectionHelperReceiverExpr = st.mutableCollectionHelperReceiverExpr;
  st.resolvesBuiltinKeyValueReceiver = [&](const Expr *receiverExpr) {
    if (receiverExpr == nullptr) {
      return false;
    }
    BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(*receiverExpr, params, locals, allowMathBare, ctx, receiverInfo)) {
      std::string receiverType = normalizeCollectionReceiverTypeName(receiverInfo.typeName);
      if ((receiverType == "Reference" || receiverType == "Pointer") && !receiverInfo.typeTemplateArg.empty()) {
        std::string innerBase;
        std::string innerArgText;
        if (splitTemplateTypeName(receiverInfo.typeTemplateArg, innerBase, innerArgText)) {
          receiverType = normalizeCollectionReceiverTypeName(innerBase);
        }
      }
      if (receiverType == "map") {
        return true;
      }
    }
    const std::string inferredReceiverType =
        inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare);
    if (inferredReceiverType.empty()) {
      return false;
    }
    std::string receiverBase;
    std::string receiverArgText;
    if (splitTemplateTypeName(inferredReceiverType, receiverBase, receiverArgText)) {
      return normalizeCollectionReceiverTypeName(receiverBase) == "map";
    }
    return normalizeCollectionReceiverTypeName(inferredReceiverType) == "map";
  };
  [[maybe_unused]] auto &resolvesBuiltinKeyValueReceiver = st.resolvesBuiltinKeyValueReceiver;
  st.resolvesBuiltinVectorReceiver = [&](const Expr *receiverExpr) {
    if (receiverExpr == nullptr) {
      return false;
    }
    BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(*receiverExpr, params, locals, allowMathBare, ctx, receiverInfo)) {
      std::string receiverType = normalizeCollectionReceiverTypeName(receiverInfo.typeName);
      if ((receiverType == "Reference" || receiverType == "Pointer") && !receiverInfo.typeTemplateArg.empty()) {
        std::string innerBase;
        std::string innerArgText;
        if (splitTemplateTypeName(receiverInfo.typeTemplateArg, innerBase, innerArgText)) {
          receiverType = normalizeCollectionReceiverTypeName(innerBase);
        }
      }
      if (receiverType == "vector") {
        return true;
      }
    }
    const std::string inferredReceiverType =
        inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare);
    if (inferredReceiverType.empty()) {
      return false;
    }
    std::string receiverBase;
    std::string receiverArgText;
    if (splitTemplateTypeName(inferredReceiverType, receiverBase, receiverArgText)) {
      return normalizeCollectionReceiverTypeName(receiverBase) == "vector";
    }
    return normalizeCollectionReceiverTypeName(inferredReceiverType) == "vector";
  };
  [[maybe_unused]] auto &resolvesBuiltinVectorReceiver = st.resolvesBuiltinVectorReceiver;
  st.inferCollectionReceiverFamilyForRewrite = [&](const Expr *receiverExpr) {
    if (receiverExpr == nullptr) {
      return std::string{};
    }
    auto familyFromBinding = [](const BindingInfo &binding) {
      std::string typeText = binding.typeName;
      if (!binding.typeTemplateArg.empty()) {
        typeText += "<" + binding.typeTemplateArg + ">";
      }
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(typeText, base, argText) &&
          (normalizeBindingTypeName(base) == "Reference" ||
           normalizeBindingTypeName(base) == "Pointer")) {
        std::vector<std::string> args;
        if (splitTopLevelTemplateArgs(argText, args) && args.size() == 1) {
          typeText = trimWhitespace(args.front());
        }
      }
      return normalizeCollectionReceiverTypeName(typeText);
    };
    BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(*receiverExpr,
                                     params,
                                     locals,
                                     allowMathBare,
                                     ctx,
                                     receiverInfo)) {
      return familyFromBinding(receiverInfo);
    }
    const std::string inferredReceiverType =
        inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare);
    if (!inferredReceiverType.empty()) {
      return normalizeCollectionReceiverTypeName(inferredReceiverType);
    }
    return std::string{};
  };
  [[maybe_unused]] auto &inferCollectionReceiverFamilyForRewrite = st.inferCollectionReceiverFamilyForRewrite;
  auto resolvesExperimentalSoaReceiverForRewrite = [&](const Expr &receiverExpr) {
    auto matchesExperimentalSoaType = [](std::string typeText) {
      typeText = normalizeBindingTypeName(typeText);
      while (!typeText.empty()) {
        std::string base;
        std::string argText;
        if (!splitTemplateTypeName(typeText, base, argText) || base.empty()) {
          return isExperimentalSoaVectorTypePath(typeText);
        }
        const std::string normalizedBase = normalizeBindingTypeName(base);
        if (isExperimentalSoaVectorTypePath(normalizedBase)) {
          return true;
        }
        if (normalizedBase != "Reference" && normalizedBase != "Pointer") {
          return false;
        }
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
          return false;
        }
        typeText = normalizeBindingTypeName(args.front());
      }
      return false;
    };
    BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(receiverExpr,
                                     params,
                                     locals,
                                     allowMathBare,
                                     ctx,
                                     receiverInfo) &&
        matchesExperimentalSoaType(bindingTypeToString(receiverInfo))) {
      return true;
    }
    return matchesExperimentalSoaType(
        inferExprTypeTextForTemplatedVectorFallback(receiverExpr,
                                                    locals,
                                                    namespacePrefix,
                                                    ctx,
                                                    allowMathBare));
  };
  st.resolvesSoaReceiverForRewrite = [&](const Expr &receiverExpr) {
    // inferCollectionReceiverFamilyForRewrite normalizes a builtin `soa<T>`
    // receiver's family to the plain "soa" spelling (the public/current
    // type name - see normalizeCollectionReceiverTypeName), not the
    // "soa_vector" legacy internal name isTemplateMonomorphSoaReceiverType
    // checks against. Accept both here rather than widening
    // normalizeCollectionReceiverTypeName itself, which many other,
    // unrelated same-path-shadow/precedence call sites also depend on.
    const std::string receiverFamily = inferCollectionReceiverFamilyForRewrite(&receiverExpr);
    return isTemplateMonomorphSoaReceiverType(receiverFamily) || receiverFamily == "soa" ||
           resolvesExperimentalSoaReceiverForRewrite(receiverExpr);
  };
  [[maybe_unused]] auto &resolvesSoaReceiverForRewrite = st.resolvesSoaReceiverForRewrite;
  st.resolveExperimentalSoaVectorReceiverTemplateArgs =
      [&](const Expr *receiverExpr, std::vector<std::string> &templateArgsOut) {
        templateArgsOut.clear();
        if (receiverExpr == nullptr) {
          return false;
        }
        auto inferFromTypeText = [&](std::string receiverTypeText) {
          if (receiverTypeText.empty()) {
            return false;
          }
          receiverTypeText = normalizeBindingTypeName(receiverTypeText);
          while (true) {
            std::string base;
            std::string argText;
            if (!splitTemplateTypeName(receiverTypeText, base, argText) || base.empty()) {
              break;
            }
            const std::string normalizedBase = normalizeBindingTypeName(base);
            if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
              std::vector<std::string> wrappedArgs;
              if (!splitTopLevelTemplateArgs(argText, wrappedArgs) || wrappedArgs.size() != 1) {
                return false;
              }
              receiverTypeText = normalizeBindingTypeName(wrappedArgs.front());
              continue;
            }
            if ((isTemplateMonomorphSoaReceiverType(
                     normalizeCollectionReceiverTypeName(normalizedBase)) ||
                 isExperimentalSoaVectorSpecializedTypePath(normalizedBase)) &&
                !argText.empty()) {
              return splitTopLevelTemplateArgs(argText, templateArgsOut) && templateArgsOut.size() == 1;
            }
            return false;
          }
          std::string resolvedPath = receiverTypeText;
          if (!resolvedPath.empty() && resolvedPath.front() != '/') {
            resolvedPath.insert(resolvedPath.begin(), '/');
          }
          const std::string normalizedResolvedPath = normalizeBindingTypeName(resolvedPath);
          if (!isExperimentalSoaVectorSpecializedTypePath(normalizedResolvedPath)) {
            return false;
          }
          for (const auto &[cacheKey, specializedPath] : ctx.specializationCache) {
            if (normalizeBindingTypeName(specializedPath) != normalizeBindingTypeName(resolvedPath)) {
              continue;
            }
            std::string base;
            std::string argText;
            if (!splitTemplateTypeName(cacheKey, base, argText) || base.empty()) {
              continue;
            }
            const std::string normalizedBase = normalizeCollectionReceiverTypeName(base);
            if (!isTemplateMonomorphSoaReceiverType(normalizedBase)) {
              continue;
            }
            if (!splitTopLevelTemplateArgs(argText, templateArgsOut) ||
                templateArgsOut.size() != 1) {
              return false;
            }
            for (std::string &arg : templateArgsOut) {
              arg = stripMangledTemplateArgKindPrefix(std::move(arg));
            }
            return true;
          }
          return false;
        };
        BindingInfo receiverInfo;
        if (inferBindingTypeForMonomorph(*receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
            inferFromTypeText(bindingTypeToString(receiverInfo))) {
          return true;
        }
        if (receiverExpr->kind == Expr::Kind::Call && !receiverExpr->isBinding) {
          std::vector<std::string> receiverCandidatePaths;
          auto appendReceiverCandidatePath = [&](const std::string &candidatePath) {
            if (!candidatePath.empty() &&
                std::find(receiverCandidatePaths.begin(),
                          receiverCandidatePaths.end(),
                          candidatePath) == receiverCandidatePaths.end()) {
              receiverCandidatePaths.push_back(candidatePath);
            }
          };
          std::string resolvedReceiverPath;
          if (receiverExpr->isMethodCall) {
            if (resolveMethodCallTemplateTarget(*receiverExpr, locals, ctx, resolvedReceiverPath)) {
              appendReceiverCandidatePath(resolvedReceiverPath);
            }
          } else {
            appendReceiverCandidatePath(resolveCalleePath(*receiverExpr, namespacePrefix, ctx));
            if (!receiverExpr->name.empty() && receiverExpr->name.front() == '/') {
              appendReceiverCandidatePath(receiverExpr->name);
            } else {
              if (!receiverExpr->namespacePrefix.empty()) {
                appendReceiverCandidatePath(receiverExpr->namespacePrefix + "/" + receiverExpr->name);
              }
              appendReceiverCandidatePath("/" + receiverExpr->name);
              appendReceiverCandidatePath(receiverExpr->name);
            }
          }
          for (const std::string &receiverCandidatePath : receiverCandidatePaths) {
            auto defIt = ctx.sourceDefs.find(receiverCandidatePath);
            if (defIt == ctx.sourceDefs.end()) {
              continue;
            }
            BindingInfo inferredReturn;
            if (inferDefinitionReturnBindingForTemplatedFallback(
                    defIt->second, allowMathBare, ctx, inferredReturn) &&
                inferFromTypeText(bindingTypeToString(inferredReturn))) {
              return true;
            }
          }
        }
        return inferFromTypeText(
            inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare));
      };
  [[maybe_unused]] auto &resolveExperimentalSoaVectorReceiverTemplateArgs = st.resolveExperimentalSoaVectorReceiverTemplateArgs;
  return PhaseStatus::Continue;
}

} // namespace primec
