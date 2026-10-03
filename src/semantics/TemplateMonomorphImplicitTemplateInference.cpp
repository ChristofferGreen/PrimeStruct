// collection-surface-audit: exempt
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
#include "TemplateMonomorphImplicitTemplateInferencePacks.h"
#include "TemplateMonomorphImplicitTemplateInferenceLambdas.h"
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
#include "TemplateMonomorphUsings.h"

namespace primec {

bool inferBindingTypeForMonomorph(const Expr &initializer,
                                  const std::vector<ParameterInfo> &params,
                                  const LocalTypeMap &locals,
                                  bool allowMathBare,
                                  Context &ctx,
                                  BindingInfo &infoOut) {
  if (tryInferBindingTypeFromInitializer(initializer, params, locals, infoOut, allowMathBare)) {
    return true;
  }
  if (resolveFieldBindingTarget(initializer,
                                params,
                                locals,
                                allowMathBare,
                                initializer.namespacePrefix,
                                ctx,
                                infoOut)) {
    return true;
  }
  bool handledCallInference = false;
  if (inferCallBindingTypeForMonomorph(initializer, params, locals, allowMathBare, ctx, infoOut,
                                       handledCallInference)) {
    return true;
  }
  if (handledCallInference) {
    return false;
  }
  return inferBlockBodyBindingTypeForMonomorph(initializer, params, locals, allowMathBare, ctx, infoOut);
}

bool isStdlibMapHelperDefinitionPath(std::string_view path) {
  const StdlibSurfaceMetadata *metadata =
      keyValueHelperSurfaceMetadataLocal();
  const StdlibSurfaceMetadata *resolvedMetadata =
      findStdlibSurfaceMetadataByResolvedPath(path);
  return metadata != nullptr && resolvedMetadata != nullptr &&
         resolvedMetadata->id == metadata->id &&
         !resolveStdlibSurfaceMemberName(*metadata, path).empty();
}

bool inferImplicitTemplateArgs(const Definition &def,
                               const Expr &callExpr,
                               const LocalTypeMap &locals,
                               const std::vector<ParameterInfo> &params,
                               const SubstMap &mapping,
                               const std::unordered_set<std::string> &allowedParams,
                               const std::string &namespacePrefix,
                               Context &ctx,
                               bool allowMathBare,
                               std::vector<std::string> &outArgs,
                               std::string &error) {
  if (def.templateArgs.empty()) {
    return false;
  }
  const bool isStdlibCollectionHelper =
      [&]() {
        if (isCanonicalVectorCompatibilityPath(def.fullPath) ||
            def.fullPath.rfind(templateMonomorphCompatibilitySoaHelperPrefix(),
                               0) == 0 ||
            isStdlibMapHelperDefinitionPath(def.fullPath)) {
          return true;
        }
        if (def.fullPath.rfind("/std/collections/", 0) != 0 || def.parameters.empty()) {
          return false;
        }
        BindingInfo receiverBinding;
        if (!extractExplicitBindingType(def.parameters.front(), receiverBinding)) {
          return false;
        }
        const std::string normalizedReceiverType =
            normalizeCollectionReceiverTypeName(receiverBinding.typeName);
        return normalizedReceiverType == "vector" ||
               isTemplateMonomorphSoaReceiverType(normalizedReceiverType) ||
               normalizedReceiverType == "map";
      }();
  std::unordered_set<std::string> implicitSet;
  auto implicitIt = ctx.implicitTemplateParams.find(def.fullPath);
  if (implicitIt != ctx.implicitTemplateParams.end()) {
    const auto &implicitParams = implicitIt->second;
    implicitSet.insert(implicitParams.begin(), implicitParams.end());
  } else {
    implicitSet.insert(def.templateArgs.begin(), def.templateArgs.end());
  }
  std::unordered_map<std::string, std::string> inferred;
  inferred.reserve(def.templateArgs.size());
  std::unordered_map<std::string, std::vector<std::string>> inferredTypePacks;
  std::unordered_set<std::string> wrappedTemplateInferenceParams;
  wrappedTemplateInferenceParams.reserve(def.templateArgs.size());
  if (!callExpr.templateArgs.empty()) {
    if (callExpr.templateArgs.size() > def.templateArgs.size()) {
      error = "template argument count mismatch for " + def.fullPath;
      return false;
    }
    for (size_t i = 0; i < callExpr.templateArgs.size(); ++i) {
      ResolvedType resolvedArg =
          resolveTypeString(callExpr.templateArgs[i], mapping, allowedParams, namespacePrefix, ctx, error);
      if (!error.empty()) {
        return false;
      }
      if (!resolvedArg.concrete) {
        error = "implicit template arguments must be concrete on " + def.fullPath;
        return false;
      }
      inferred.emplace(def.templateArgs[i], resolvedArg.text);
    }
  }
  auto extractBuiltinSoaElementTypeText = [&](std::string typeText,
                                              std::string &elemTypeOut) -> bool {
    return ::primec::extractBuiltinSoaElementTypeText(typeText, elemTypeOut);
  };
  auto extractBuiltinVectorElementTypeText = [&](std::string typeText,
                                                 std::string &elemTypeOut) -> bool {
    return ::primec::extractBuiltinVectorElementTypeText(typeText, elemTypeOut);
  };
  auto inferTemplatedFallbackTypeText = [&](const Expr &candidate) {
    return inferExprTypeTextForTemplatedVectorFallback(
        candidate, locals, namespacePrefix, ctx, allowMathBare);
  };
  auto inferBindingTypeTextForExpr = [&](const Expr &candidate,
                                         std::string &typeTextOut) {
    BindingInfo inferredInfo;
    if (inferBindingTypeForMonomorph(candidate, params, locals, allowMathBare,
                                     ctx, inferredInfo)) {
      typeTextOut = bindingTypeToString(inferredInfo);
      return !typeTextOut.empty();
    }
    typeTextOut = inferTemplatedFallbackTypeText(candidate);
    return !typeTextOut.empty();
  };
  auto extractSpecializedSumTemplateArgsFromTypeText = [&](std::string typeText,
          std::string paramBaseType,
          const std::vector<std::string> &paramNames,
          std::vector<std::string> &templateArgsOut) -> bool {
    return ::primec::extractSpecializedSumTemplateArgsFromTypeText(typeText, paramBaseType, paramNames, templateArgsOut, ctx);
  };
  auto inferMethodReceiverTemplateArgs =
      [&](std::vector<std::string> &templateArgsOut) {
    templateArgsOut.clear();
    if (!callExpr.isMethodCall || callExpr.args.empty() ||
        def.parameters.empty() || def.templateArgs.empty()) {
      return false;
    }
    BindingInfo receiverInfo;
    BindingInfo receiverParamInfo;
    if (!inferBindingTypeForMonomorph(callExpr.args.front(),
                                      params,
                                      locals,
                                      allowMathBare,
                                      ctx,
                                      receiverInfo) ||
        !extractExplicitBindingType(def.parameters.front(), receiverParamInfo)) {
      return false;
    }
    auto leafName = [](const std::string &path) {
      const size_t slash = path.find_last_of('/');
      return slash == std::string::npos ? path : path.substr(slash + 1);
    };
    std::string receiverBase = normalizeBindingTypeName(receiverInfo.typeName);
    std::string paramBase = normalizeBindingTypeName(receiverParamInfo.typeName);
    if (!receiverBase.empty() && receiverBase.front() == '/') {
      receiverBase.erase(receiverBase.begin());
    }
    if (!paramBase.empty() && paramBase.front() == '/') {
      paramBase.erase(paramBase.begin());
    }
    const bool baseMatches =
        receiverBase == paramBase ||
        (!receiverBase.empty() && !paramBase.empty() &&
         leafName(receiverBase).rfind(leafName(paramBase), 0) == 0);
    if (!baseMatches) {
      return false;
    }
    if (!receiverInfo.typeTemplateArg.empty()) {
      std::vector<std::string> receiverTemplateArgs;
      if (splitTopLevelTemplateArgs(receiverInfo.typeTemplateArg,
                                    receiverTemplateArgs) &&
          receiverTemplateArgs.size() == def.templateArgs.size()) {
        templateArgsOut = std::move(receiverTemplateArgs);
        return true;
      }
    }
    return extractSpecializedSumTemplateArgsFromTypeText(
        bindingTypeToString(receiverInfo),
        receiverParamInfo.typeName,
        def.templateArgs,
        templateArgsOut);
  };
  if (callExpr.templateArgs.empty()) {
    std::vector<std::string> receiverTemplateArgs;
    if (inferMethodReceiverTemplateArgs(receiverTemplateArgs)) {
      outArgs = std::move(receiverTemplateArgs);
      return true;
    }
  }
  auto inferSingleSumOwnerTemplateArgs =
      [&](std::vector<std::string> &templateArgsOut) {
    templateArgsOut.clear();
    if (def.templateArgs.size() != 1) {
      return false;
    }
    const size_t methodSlash = def.fullPath.find_last_of('/');
    if (methodSlash == std::string::npos || methodSlash == 0) {
      return false;
    }
    std::string ownerPath = def.fullPath.substr(0, methodSlash);
    std::string baseOwnerPath = ownerPath;
    const size_t ownerLeafStart = baseOwnerPath.find_last_of('/');
    const size_t ownerSearchStart =
        ownerLeafStart == std::string::npos ? 0 : ownerLeafStart + 1;
    if (const size_t generatedSuffix = baseOwnerPath.find("__", ownerSearchStart);
        generatedSuffix != std::string::npos) {
      baseOwnerPath.erase(generatedSuffix);
    }
    if (ownerPath != baseOwnerPath) {
      return extractSpecializedSumTemplateArgsFromTypeText(ownerPath,
                                                          baseOwnerPath,
                                                          def.templateArgs,
                                                          templateArgsOut);
    }
    bool found = false;
    for (const auto &[path, definition] : ctx.sourceDefs) {
      (void)definition;
      if (path.rfind(baseOwnerPath + "__t", 0) != 0) {
        continue;
      }
      std::vector<std::string> candidateArgs;
      if (!extractSpecializedSumTemplateArgsFromTypeText(path,
                                                         baseOwnerPath,
                                                         def.templateArgs,
                                                         candidateArgs)) {
        continue;
      }
      if (found && templateArgsOut != candidateArgs) {
        templateArgsOut.clear();
        return false;
      }
      templateArgsOut = std::move(candidateArgs);
      found = true;
    }
    return found;
  };
  if (callExpr.templateArgs.empty()) {
    std::vector<std::string> sumOwnerTemplateArgs;
    if (inferSingleSumOwnerTemplateArgs(sumOwnerTemplateArgs)) {
      outArgs = std::move(sumOwnerTemplateArgs);
      return true;
    }
  }
  auto inferIndexedArgsPackElementTypeText = [&](const Expr &candidate,
                                                 std::string &elemTypeOut) {
    elemTypeOut.clear();
    std::string accessName;
    if (candidate.kind != Expr::Kind::Call ||
        !getBuiltinArrayAccessName(candidate, accessName) ||
        candidate.args.size() != 2 ||
        candidate.args.front().kind != Expr::Kind::Name) {
      return false;
    }
    const std::string &receiverName = candidate.args.front().name;
    const BindingInfo *binding = nullptr;
    for (const auto &param : params) {
      if (param.name == receiverName) {
        binding = &param.binding;
        break;
      }
    }
    if (binding == nullptr) {
      const auto localIt = locals.find(receiverName);
      if (localIt != locals.end()) {
        binding = &localIt->second;
      }
    }
    return binding != nullptr && getArgsPackElementType(*binding, elemTypeOut);
  };
  auto inferBuiltinVectorTemplateArgFromExpr = [&](const Expr &receiverExpr,
                                                  std::string &elemTypeOut) {
    std::string receiverTypeText;
    if (inferBindingTypeTextForExpr(receiverExpr, receiverTypeText) &&
        extractBuiltinVectorElementTypeText(receiverTypeText, elemTypeOut)) {
      return true;
    }
    std::string indexedElemType;
    if (inferIndexedArgsPackElementTypeText(receiverExpr, indexedElemType) &&
        extractBuiltinVectorElementTypeText(indexedElemType, elemTypeOut)) {
      return true;
    }
    return false;
  };
  auto inferBuiltinSoaTemplateArgFromReceiverExpr = [&](const Expr *receiverExpr,
                                                        std::string &elemTypeOut) {
    if (receiverExpr == nullptr) {
      return false;
    }
    if (receiverExpr->kind == Expr::Kind::Call) {
      const std::string resolvedReceiverPath =
          receiverExpr->isMethodCall ? std::string{}
                                     : resolveCalleePath(*receiverExpr, namespacePrefix, ctx);
      if (((!receiverExpr->isMethodCall &&
            isSimpleCallName(*receiverExpr, "to_soa")) ||
           resolvedReceiverPath == "/to_soa") &&
          receiverExpr->args.size() == 1 &&
          inferBuiltinVectorTemplateArgFromExpr(receiverExpr->args.front(),
                                                elemTypeOut)) {
        return true;
      }
      if (isSimpleCallName(*receiverExpr, "dereference") &&
          receiverExpr->args.size() == 1) {
        std::string indexedElemType;
        if (inferIndexedArgsPackElementTypeText(receiverExpr->args.front(),
                                                indexedElemType) &&
            extractBuiltinSoaElementTypeText(indexedElemType, elemTypeOut)) {
          return true;
        }
      }
    }
    std::string receiverTypeText;
    if (inferBindingTypeTextForExpr(*receiverExpr, receiverTypeText) &&
        extractBuiltinSoaElementTypeText(receiverTypeText, elemTypeOut)) {
      return true;
    }
    return false;
  };
  if (def.fullPath.rfind(templateMonomorphCompatibilitySoaHelperPrefix(), 0) == 0 &&
      !def.templateArgs.empty() &&
      inferred.count(def.templateArgs.front()) == 0 &&
      !callExpr.args.empty()) {
    std::string inferredSoaElemType;
    if (inferBuiltinSoaTemplateArgFromReceiverExpr(&callExpr.args.front(),
                                                   inferredSoaElemType)) {
      inferred.emplace(def.templateArgs.front(), std::move(inferredSoaElemType));
    }
  }
  std::vector<ParameterInfo> callParams;
  callParams.reserve(def.parameters.size());
  for (const auto &paramExpr : def.parameters) {
    ParameterInfo param;
    param.name = paramExpr.name;
    extractExplicitBindingType(paramExpr, param.binding);
    if (paramExpr.args.size() == 1) {
      param.defaultExpr = &paramExpr.args.front();
    }
    callParams.push_back(std::move(param));
  }
  auto isTypePackBindingParameter = [&](const Expr &param,
                                        std::string &packNameOut) {
    packNameOut.clear();
    if (!param.isBinding || param.transforms.size() != 1) {
      return false;
    }
    const Transform &transform = param.transforms.front();
    if (!transform.isPackExpansion || !transform.templateArgs.empty() ||
        !transform.templateArgDetails.empty() || !transform.arguments.empty()) {
      return false;
    }
    for (size_t i = 0; i < def.templateArgs.size(); ++i) {
      if (def.templateArgs[i] == transform.name &&
          i < def.templateArgIsPack.size() && def.templateArgIsPack[i]) {
        packNameOut = transform.name;
        return true;
      }
    }
    return false;
  };
  size_t typePackParamIndex = callParams.size();
  std::string typePackParamName;
  for (size_t i = 0; i < def.parameters.size(); ++i) {
    std::string candidatePackName;
    if (isTypePackBindingParameter(def.parameters[i], candidatePackName)) {
      typePackParamIndex = i;
      typePackParamName = std::move(candidatePackName);
      break;
    }
  }
  std::vector<const Expr *> orderedArgs;
  std::vector<const Expr *> packedArgs;
  size_t packedParamIndex = callParams.size();
  size_t callArgStart = 0;
  size_t paramIndexOffset = 0;
  auto assignBindingFromTypeText = [&](const std::string &typeText, BindingInfo &bindingOut) -> bool {
    return ::primec::assignBindingFromTypeText(typeText, bindingOut);
  };
  auto unsupportedBuiltinSoaPendingDiagnostic = [&](const Expr &candidate) -> std::string {
    return ::primec::unsupportedBuiltinSoaPendingDiagnostic(candidate, locals, params, namespacePrefix, ctx, allowMathBare);
  };
  const bool hasLeadingReceiverParam = [&]() {
    if (callParams.empty()) {
      return false;
    }
    const size_t lastSlash = def.fullPath.find_last_of('/');
    if (lastSlash == std::string::npos || lastSlash == 0) {
      return false;
    }
    const size_t ownerSlash = def.fullPath.find_last_of('/', lastSlash - 1);
    if (ownerSlash == std::string::npos) {
      return false;
    }
    return normalizeBindingTypeName(callParams.front().binding.typeName) ==
           def.fullPath.substr(ownerSlash + 1, lastSlash - ownerSlash - 1);
  }();
  auto receiverArgMatchesLeadingParam = [&]() -> bool {
    if (!hasLeadingReceiverParam || callExpr.args.empty() || callParams.empty()) {
      return false;
    }
    BindingInfo receiverArgInfo;
    if (!inferBindingTypeForMonomorph(callExpr.args.front(), params, locals, allowMathBare, ctx, receiverArgInfo)) {
      if (!assignBindingFromTypeText(
              inferExprTypeTextForTemplatedVectorFallback(
                  callExpr.args.front(), locals, namespacePrefix, ctx, allowMathBare),
              receiverArgInfo)) {
        return false;
      }
    }
    return normalizeBindingTypeName(bindingTypeToString(receiverArgInfo)) ==
           normalizeBindingTypeName(bindingTypeToString(callParams.front().binding));
  };
  if (hasLeadingReceiverParam && callExpr.args.size() + 1 == callParams.size() &&
      !receiverArgMatchesLeadingParam()) {
    // Some member-helper monomorphization paths have already stripped the
    // receiver value from the call arguments, so align the parameter view to
    // the post-receiver shape before argument ordering.
    callParams.erase(callParams.begin());
    paramIndexOffset = 1;
  } else if (callExpr.isMethodCall && !callParams.empty()) {
    if (callExpr.args.size() == callParams.size() + 1) {
      // Method-call sugar prepends the receiver expression.
      callArgStart = 1;
    }
  }
  std::vector<Expr> reorderedArgs;
  std::vector<std::optional<std::string>> reorderedArgNames;
  const std::vector<Expr> *orderedCallArgs = &callExpr.args;
  const std::vector<std::optional<std::string>> *orderedCallArgNames = &callExpr.argNames;
  if (callArgStart > 0) {
    reorderedArgs.assign(callExpr.args.begin() + static_cast<std::ptrdiff_t>(callArgStart), callExpr.args.end());
    if (!callExpr.argNames.empty()) {
      reorderedArgNames.assign(callExpr.argNames.begin() + static_cast<std::ptrdiff_t>(callArgStart),
                               callExpr.argNames.end());
    }
    orderedCallArgs = &reorderedArgs;
    orderedCallArgNames = &reorderedArgNames;
  }
  auto buildTypePackOrderedArguments = [&]() -> bool {
    return ::primec::buildTypePackOrderedArguments(def, callParams, orderedCallArgs, orderedCallArgNames,
                                                   typePackParamIndex, orderedArgs, packedArgs,
                                                   packedParamIndex, error);
  };
  const bool hasTypePackValueParameter =
      typePackParamIndex < callParams.size() &&
      typePackParamIndex + 1 == callParams.size();
  const bool orderedOk = hasTypePackValueParameter
                             ? buildTypePackOrderedArguments()
                             : buildOrderedArguments(callParams,
                                                     *orderedCallArgs,
                                                     *orderedCallArgNames,
                                                     orderedArgs,
                                                     packedArgs,
                                                     packedParamIndex,
                                                     error);
  if (!orderedOk) {
    if (error.find("argument count mismatch") != std::string::npos &&
        error.find(def.fullPath) == std::string::npos) {
      error = "argument count mismatch for " + def.fullPath;
    }
    return false;
  }
  const std::string implicitInferenceFactScopePath =
      !ctx.currentDefinitionPath.empty()
          ? ctx.currentDefinitionPath
          : (namespacePrefix.empty() ? std::string("/") : namespacePrefix);
  std::string implicitInferenceFactKey;
  bool canConsumeImplicitInferenceFact = true;
  {
    std::ostringstream key;
    key << implicitInferenceFactScopePath
        << "|" << def.fullPath
        << "|template:"
        << joinTemplateArgs(callExpr.templateArgs)
        << "|ordered:";
    for (size_t argIndex = 0; argIndex < orderedArgs.size(); ++argIndex) {
      if (argIndex > 0) {
        key << ";";
      }
      const bool hasArgName =
          orderedCallArgNames != nullptr &&
          !orderedCallArgNames->empty() &&
          argIndex < orderedCallArgNames->size() &&
          (*orderedCallArgNames)[argIndex].has_value();
      if (hasArgName) {
        key << *(*orderedCallArgNames)[argIndex] << "=";
      }
      const Expr *argExpr = orderedArgs[argIndex];
      if (!argExpr) {
        key << "<default>";
        continue;
      }
      BindingInfo argInfo;
      if (!inferBindingTypeForMonomorph(*argExpr, params, locals, allowMathBare, ctx, argInfo)) {
        if (!assignBindingFromTypeText(
                inferExprTypeTextForTemplatedVectorFallback(
                    *argExpr, locals, namespacePrefix, ctx, allowMathBare),
                argInfo)) {
          canConsumeImplicitInferenceFact = false;
          break;
        }
      }
      const std::string normalizedArgType = normalizeBindingTypeName(bindingTypeToString(argInfo));
      if (normalizedArgType.empty()) {
        canConsumeImplicitInferenceFact = false;
        break;
      }
      key << normalizedArgType;
    }
    if (canConsumeImplicitInferenceFact) {
      key << "|packed:";
      for (size_t argIndex = 0; argIndex < packedArgs.size(); ++argIndex) {
        if (argIndex > 0) {
          key << ";";
        }
        const Expr *argExpr = packedArgs[argIndex];
        if (!argExpr) {
          key << "<empty>";
          continue;
        }
        BindingInfo argInfo;
        if (!inferBindingTypeForMonomorph(*argExpr, params, locals, allowMathBare, ctx, argInfo)) {
          if (!assignBindingFromTypeText(
                  inferExprTypeTextForTemplatedVectorFallback(
                      *argExpr, locals, namespacePrefix, ctx, allowMathBare),
                  argInfo)) {
            canConsumeImplicitInferenceFact = false;
            break;
          }
        }
        const std::string normalizedArgType = normalizeBindingTypeName(bindingTypeToString(argInfo));
        if (normalizedArgType.empty()) {
          canConsumeImplicitInferenceFact = false;
          break;
        }
        key << normalizedArgType;
      }
    }
    if (canConsumeImplicitInferenceFact) {
      key << "|packed_index:" << packedParamIndex;
      implicitInferenceFactKey = key.str();
    }
  }
  if (canConsumeImplicitInferenceFact) {
    const auto factIt = ctx.implicitTemplateArgInferenceFacts.find(implicitInferenceFactKey);
    if (factIt != ctx.implicitTemplateArgInferenceFacts.end() &&
        (hasTypePackValueParameter ||
         factIt->second.inferredArgs.size() == def.templateArgs.size())) {
      outArgs = factIt->second.inferredArgs;
      ++ctx.implicitTemplateArgInferenceFactHitsForTesting;
      if (ctx.collectImplicitTemplateArgFactsForTesting) {
        ctx.implicitTemplateArgFactsForTesting.push_back(
            ImplicitTemplateArgResolutionFactForTesting{
                implicitInferenceFactScopePath,
                callExpr.name,
                def.fullPath,
                joinTemplateArgs(outArgs),
            });
      }
      return true;
    }
  }

  for (size_t i = 0; i < def.parameters.size(); ++i) {
    const Expr &param = def.parameters[i];
    BindingInfo paramInfo;
    if (!extractExplicitBindingType(param, paramInfo)) {
      continue;
    }
    if (paramInfo.typeName == "auto" && param.args.size() == 1 &&
        inferBindingTypeForMonomorph(param.args.front(), {}, {}, allowMathBare, ctx, paramInfo)) {
      // Auto parameters participate in implicit-template inference through
      // their default initializer shape.
    }
    if (i < paramIndexOffset) {
      continue;
    }
    const size_t callParamIndex = i - paramIndexOffset;
    // Entry-pair packed-arg inference: an `[args<Entry<K, V>>] entries`-shaped
    // parameter whose call-site packed arguments are raw key/value values
    // (not `entry(...)`-shaped calls) never matches the bare-implicit-name
    // "packed args" branch below (its wrapper element type is the compound
    // `Entry<K, V>`, not a bare implicit name) nor the "wrapped template
    // args" branch (the parameter's own template-arg text is a single
    // compound element type, not a comma list of bare implicit names). Infer
    // K/V positionally from the packed arguments here instead, bailing out
    // whenever any packed argument is itself `entry(...)`-shaped so this
    // cannot hijack the already-working entries-pack-call surface owned by
    // `deriveKeyValueTypesFromEntryPackCall` et al.
    if (callParamIndex == packedParamIndex && isArgsPackBinding(paramInfo)) {
      std::string wrapperBase;
      std::string wrapperArgs;
      if (splitTemplateTypeName(paramInfo.typeTemplateArg, wrapperBase, wrapperArgs) &&
          normalizeBindingTypeName(wrapperBase) == "Entry") {
        std::vector<std::string> entryTemplateArgNames;
        if (splitTopLevelTemplateArgs(wrapperArgs, entryTemplateArgNames) &&
            entryTemplateArgNames.size() == 2) {
          std::string entryKeyParamName = trimWhitespace(entryTemplateArgNames[0]);
          std::string entryValueParamName = trimWhitespace(entryTemplateArgNames[1]);
          if (!entryKeyParamName.empty() && !entryValueParamName.empty() &&
              implicitSet.count(entryKeyParamName) > 0 &&
              implicitSet.count(entryValueParamName) > 0 &&
              !packedArgs.empty() && packedArgs.size() % 2 == 0) {
            bool anyPackedArgIsEntryShaped = false;
            for (const Expr *packedArgExpr : packedArgs) {
              if (packedArgExpr != nullptr && packedArgExpr->kind == Expr::Kind::Call &&
                  !packedArgExpr->isMethodCall &&
                  (packedArgExpr->name == "entry" ||
                   (packedArgExpr->name.size() > 6 &&
                    packedArgExpr->name.compare(packedArgExpr->name.size() - 6, 6, "/entry") == 0))) {
                anyPackedArgIsEntryShaped = true;
                break;
              }
            }
            if (!anyPackedArgIsEntryShaped) {
              bool derivedEntryPairTypes = true;
              std::string derivedKeyType;
              std::string derivedValueType;
              for (size_t packedIndex = 0; packedIndex < packedArgs.size(); ++packedIndex) {
                const Expr *packedArgExpr = packedArgs[packedIndex];
                if (packedArgExpr == nullptr) {
                  derivedEntryPairTypes = false;
                  break;
                }
                BindingInfo packedArgInfo;
                if (!inferBindingTypeForMonomorph(*packedArgExpr, params, locals, allowMathBare, ctx,
                                                   packedArgInfo)) {
                  derivedEntryPairTypes = false;
                  break;
                }
                const std::string packedArgTypeText = bindingTypeToString(packedArgInfo);
                if (packedArgTypeText.empty()) {
                  derivedEntryPairTypes = false;
                  break;
                }
                if ((packedIndex % 2) == 0) {
                  if (derivedKeyType.empty()) {
                    derivedKeyType = packedArgTypeText;
                  } else if (derivedKeyType != packedArgTypeText) {
                    // A genuine key-type conflict across pairs (not merely
                    // "couldn't infer a type at all") - report it the same
                    // way the rest of this function reports any other
                    // implicit-template-argument conflict, rather than
                    // silently falling through to a generic "argument
                    // count mismatch"/"require values" diagnostic below.
                    error = "implicit template arguments conflict on " + def.fullPath;
                    return false;
                  }
                } else {
                  if (derivedValueType.empty()) {
                    derivedValueType = packedArgTypeText;
                  } else if (derivedValueType != packedArgTypeText) {
                    error = "implicit template arguments conflict on " + def.fullPath;
                    return false;
                  }
                }
              }
              if (derivedEntryPairTypes && !derivedKeyType.empty() && !derivedValueType.empty()) {
                inferred[entryKeyParamName] = derivedKeyType;
                inferred[entryValueParamName] = derivedValueType;
                continue;
              }
            }
          }
        }
      }
    }
    const bool inferFromPackedArgs = callParamIndex == packedParamIndex && isArgsPackBinding(paramInfo) &&
                                     implicitSet.count(paramInfo.typeTemplateArg) > 0;
    const bool inferFromTypePackArgs =
        hasTypePackValueParameter && callParamIndex == packedParamIndex &&
        !typePackParamName.empty() && implicitSet.count(typePackParamName) > 0;
    bool inferFromWrappedTemplateArgs = false;
    std::vector<std::string> inferredParamNames;
    if (inferFromPackedArgs) {
      inferredParamNames.push_back(paramInfo.typeTemplateArg);
    } else if (inferFromTypePackArgs) {
      inferredParamNames.push_back(typePackParamName);
    } else {
      if (implicitSet.count(paramInfo.typeName) == 0) {
        std::vector<std::string> wrappedTemplateArgs;
        if (paramInfo.typeTemplateArg.empty() ||
            !splitTopLevelTemplateArgs(paramInfo.typeTemplateArg, wrappedTemplateArgs) ||
            wrappedTemplateArgs.empty()) {
          continue;
        }
        bool allImplicit = true;
        for (std::string &wrappedArg : wrappedTemplateArgs) {
          wrappedArg = trimWhitespace(wrappedArg);
          if (wrappedArg.empty() || wrappedArg.find('<') != std::string::npos ||
              implicitSet.count(wrappedArg) == 0) {
            allImplicit = false;
            break;
          }
        }
        if (!allImplicit) {
          continue;
        }
        inferFromWrappedTemplateArgs = true;
        inferredParamNames = std::move(wrappedTemplateArgs);
      } else {
        inferredParamNames.push_back(paramInfo.typeName);
      }
    }
    std::vector<const Expr *> argsToInfer;
    if (inferFromPackedArgs || inferFromTypePackArgs) {
      argsToInfer = packedArgs;
    } else {
      const Expr *argExpr = callParamIndex < orderedArgs.size() ? orderedArgs[callParamIndex] : nullptr;
      if (!argExpr && param.args.size() == 1) {
        argExpr = &param.args.front();
      }
      if (argExpr) {
        argsToInfer.push_back(argExpr);
      }
    }
    auto allInferredParamNamesKnown = [&](const std::vector<std::string> &paramNames) {
      if (paramNames.empty()) {
        return false;
      }
      for (const auto &name : paramNames) {
        if (inferred.find(name) == inferred.end()) {
          return false;
        }
      }
      return true;
    };
    if (argsToInfer.empty()) {
      if (inferFromTypePackArgs) {
        inferredTypePacks[typePackParamName] = {};
        continue;
      }
      if (inferFromWrappedTemplateArgs) {
        std::string paramBaseType = normalizeBindingTypeName(paramInfo.typeName);
        if (!paramBaseType.empty() && paramBaseType.front() != '/') {
          paramBaseType.insert(paramBaseType.begin(), '/');
        }
        std::vector<std::string> inferredSpecializedArgs;
        bool foundSpecializedSum = false;
        bool ambiguousSpecializedSum = false;
        for (const auto &[path, definition] : ctx.sourceDefs) {
          (void)definition;
          if (path.rfind(paramBaseType + "__t", 0) != 0) {
            continue;
          }
          std::vector<std::string> candidateArgs;
          if (!extractSpecializedSumTemplateArgsFromTypeText(path,
                                                             paramInfo.typeName,
                                                             inferredParamNames,
                                                             candidateArgs)) {
            continue;
          }
          if (foundSpecializedSum && inferredSpecializedArgs != candidateArgs) {
            ambiguousSpecializedSum = true;
            break;
          }
          inferredSpecializedArgs = std::move(candidateArgs);
          foundSpecializedSum = true;
        }
        if (foundSpecializedSum && !ambiguousSpecializedSum &&
            inferredSpecializedArgs.size() == inferredParamNames.size()) {
          for (size_t templateIndex = 0; templateIndex < inferredParamNames.size(); ++templateIndex) {
            inferred[inferredParamNames[templateIndex]] =
                inferredSpecializedArgs[templateIndex];
          }
          continue;
        }
      }
      if (isStdlibCollectionHelper && allInferredParamNamesKnown(inferredParamNames)) {
        continue;
      }
      error = "implicit template arguments require values on " + def.fullPath;
      return false;
    }
    for (const Expr *argExpr : argsToInfer) {
      BindingInfo argInfo;
      if (!argExpr) {
        if (isStdlibCollectionHelper && allInferredParamNamesKnown(inferredParamNames)) {
          continue;
        }
        if (isStdlibCollectionHelper) {
          return false;
        }
        error = "unable to infer implicit template arguments for " + def.fullPath;
        return false;
      }
      const bool usesDefaultArgBinding =
          !inferFromPackedArgs && callParamIndex >= orderedArgs.size() && param.args.size() == 1 &&
          argExpr == &param.args.front();
      if (usesDefaultArgBinding) {
        argInfo = paramInfo;
      } else if (argExpr->isSpread) {
        std::string spreadElementType;
        if (!resolveArgsPackElementTypeForExpr(*argExpr, params, locals, spreadElementType)) {
          error = "spread argument requires args<T> value";
          return false;
        }
        argInfo.typeName = normalizeBindingTypeName(spreadElementType);
        argInfo.typeTemplateArg.clear();
        std::string spreadBase;
        std::string spreadArgs;
        if (splitTemplateTypeName(spreadElementType, spreadBase, spreadArgs)) {
          argInfo.typeName = normalizeBindingTypeName(spreadBase);
          argInfo.typeTemplateArg = spreadArgs;
        }
      } else if (!inferBindingTypeForMonomorph(*argExpr, params, locals, allowMathBare, ctx, argInfo)) {
        Expr rewrittenArg = *argExpr;
        std::string rewriteError;
        const bool inferredFromRewrittenArg =
            rewriteExpr(rewrittenArg,
                        mapping,
                        allowedParams,
                        namespacePrefix,
                        ctx,
                        rewriteError,
                        locals,
                        params,
                        allowMathBare) &&
            inferBindingTypeForMonomorph(rewrittenArg, params, locals, allowMathBare, ctx, argInfo);
        TemplatedFallbackQueryStateAdapterData fallbackQueryState;
        const bool inferredFromFallbackQueryState =
            !inferredFromRewrittenArg &&
            inferTemplatedFallbackQueryStateAdapter(
                *argExpr, locals, params, namespacePrefix, ctx, allowMathBare, fallbackQueryState);
        if (inferredFromFallbackQueryState && !fallbackQueryState.mismatchDiagnostic.empty()) {
          error = "template fallback adapter mismatch on " + def.fullPath + ": " +
                  fallbackQueryState.mismatchDiagnostic;
          return false;
        }
        const std::string fallbackTypeText =
            inferredFromFallbackQueryState ? fallbackQueryState.queryTypeText : std::string{};
        if (!inferredFromRewrittenArg && !assignBindingFromTypeText(fallbackTypeText, argInfo)) {
          if (inferredFromFallbackQueryState && !fallbackTypeText.empty()) {
            error = "template fallback adapter mismatch on " + def.fullPath +
                    ": unable to materialize binding type from query type `" + fallbackTypeText + "`";
            return false;
          }
          if (const std::string diagnostic = unsupportedBuiltinSoaPendingDiagnostic(*argExpr);
              !diagnostic.empty()) {
            error = diagnostic;
            return false;
          }
          if (isStdlibCollectionHelper && allInferredParamNamesKnown(inferredParamNames)) {
            continue;
          }
          if (isStdlibCollectionHelper) {
            return false;
          }
          error = "unable to infer implicit template arguments for " + def.fullPath;
          return false;
        }
      }
      if (inferFromWrappedTemplateArgs) {
        std::string argBaseType = argInfo.typeName;
        std::string argTemplateArgText = argInfo.typeTemplateArg;
        if ((normalizeBindingTypeName(argBaseType) == "Reference" ||
             normalizeBindingTypeName(argBaseType) == "Pointer") &&
            !argTemplateArgText.empty()) {
          std::string innerBase;
          std::string innerArgs;
          if (splitTemplateTypeName(argTemplateArgText, innerBase, innerArgs) && !innerBase.empty()) {
            argBaseType = innerBase;
            argTemplateArgText = innerArgs;
          }
        }
        std::string paramBaseType = paramInfo.typeName;
        if ((normalizeBindingTypeName(paramBaseType) == "Reference" ||
             normalizeBindingTypeName(paramBaseType) == "Pointer") &&
            !paramInfo.typeTemplateArg.empty()) {
          std::string innerBase;
          std::string innerArgs;
          if (splitTemplateTypeName(paramInfo.typeTemplateArg, innerBase, innerArgs) &&
              !innerBase.empty()) {
            paramBaseType = innerBase;
          }
        }
        // Inference-local equivalence only: the public soa<T> spelling
        // names the same receiver family as the SoaVector<T> backing type,
        // so a soa<T>-typed argument must unify against a helper's
        // [SoaVector<T>] parameter (otherwise bare get(soaValues, i) dies
        // with "template arguments required for .../soaVectorGet"). Kept
        // out of normalizeCollectionReceiverTypeName itself - widening the
        // shared family classifier flips unrelated monomorph rewrite
        // decisions and breaks user same-path /soa/<helper> shadows.
        auto inferenceReceiverFamilyName = [](std::string value) {
          value = normalizeCollectionReceiverTypeName(std::move(value));
          if (value == "soa" ||
              trimLeadingSlash(value) == "std/collections/soa") {
            return templateMonomorphSoaReceiverTypeName();
          }
          return value;
        };
        if (inferenceReceiverFamilyName(argBaseType) !=
            inferenceReceiverFamilyName(paramBaseType)) {
          if (isStdlibCollectionHelper) {
            return false;
          }
          error = "unable to infer implicit template arguments for " + def.fullPath;
          return false;
        }
        std::vector<std::string> argTemplateArgs;
        if (argTemplateArgText.empty()) {
          if (!extractCollectionVectorValueReceiverTemplateArgsFromTypeText(argBaseType, ctx, argTemplateArgs) &&
              !extractExperimentalSoaVectorValueReceiverTemplateArgsFromTypeText(
                  argBaseType, ctx, argTemplateArgs) &&
              !extractExperimentalKeyValueReceiverTemplateArgsFromTypeText(argBaseType, ctx, argTemplateArgs) &&
              !extractSpecializedSumTemplateArgsFromTypeText(argBaseType,
                                                             paramBaseType,
                                                             inferredParamNames,
                                                             argTemplateArgs)) {
            if (isStdlibCollectionHelper) {
              return false;
            }
            error = "unable to infer implicit template arguments for " + def.fullPath;
            return false;
          }
        } else if (!splitTopLevelTemplateArgs(argTemplateArgText, argTemplateArgs)) {
          if (isStdlibCollectionHelper) {
            return false;
          }
          error = "unable to infer implicit template arguments for " + def.fullPath;
          return false;
        }
        if (argTemplateArgs.size() != inferredParamNames.size()) {
          if (isStdlibCollectionHelper) {
            return false;
          }
          error = "unable to infer implicit template arguments for " + def.fullPath;
          return false;
        }
        for (size_t templateIndex = 0; templateIndex < inferredParamNames.size(); ++templateIndex) {
          ResolvedType resolvedArg =
              resolveTypeString(argTemplateArgs[templateIndex], mapping, allowedParams, namespacePrefix, ctx, error);
          if (!error.empty()) {
            return false;
          }
          if (!resolvedArg.concrete) {
            error = "implicit template arguments must be concrete on " + def.fullPath;
            return false;
          }
          auto it = inferred.find(inferredParamNames[templateIndex]);
          if (it != inferred.end() && it->second != resolvedArg.text) {
            if (wrappedTemplateInferenceParams.count(inferredParamNames[templateIndex]) == 0) {
              error = "implicit template arguments conflict on " + def.fullPath;
              return false;
            }
            continue;
          }
          inferred[inferredParamNames[templateIndex]] = resolvedArg.text;
          wrappedTemplateInferenceParams.insert(inferredParamNames[templateIndex]);
        }
        continue;
      }
      std::string argType = bindingTypeToString(argInfo);
      if (argType.empty() || inferredParamNames.empty()) {
        if (isStdlibCollectionHelper && allInferredParamNamesKnown(inferredParamNames)) {
          continue;
        }
        if (isStdlibCollectionHelper) {
          return false;
        }
        error = "unable to infer implicit template arguments for " + def.fullPath;
        return false;
      }
      ResolvedType resolvedArg = resolveTypeString(argType, mapping, allowedParams, namespacePrefix, ctx, error);
      if (!error.empty()) {
        return false;
      }
      if (!resolvedArg.concrete) {
        if (isStdlibCollectionHelper && allInferredParamNamesKnown(inferredParamNames)) {
          continue;
        }
        error = "implicit template arguments must be concrete on " + def.fullPath;
        return false;
      }
      if (inferFromTypePackArgs) {
        inferredTypePacks[inferredParamNames.front()].push_back(resolvedArg.text);
        continue;
      }
      auto it = inferred.find(inferredParamNames.front());
      if (it != inferred.end() && it->second != resolvedArg.text) {
        if (wrappedTemplateInferenceParams.count(inferredParamNames.front()) == 0) {
          error = "implicit template arguments conflict on " + def.fullPath;
          return false;
        }
        continue;
      }
      inferred[inferredParamNames.front()] = resolvedArg.text;
    }
  }

  outArgs.clear();
  outArgs.reserve(def.templateArgs.size());
  for (size_t paramIndex = 0; paramIndex < def.templateArgs.size(); ++paramIndex) {
    const auto &paramName = def.templateArgs[paramIndex];
    if (paramIndex < def.templateArgIsPack.size() &&
        def.templateArgIsPack[paramIndex]) {
      auto packIt = inferredTypePacks.find(paramName);
      if (packIt == inferredTypePacks.end()) {
        return false;
      }
      outArgs.insert(outArgs.end(), packIt->second.begin(), packIt->second.end());
      continue;
    }
    auto it = inferred.find(paramName);
    if (it == inferred.end()) {
      // Leave error empty so deferred helper-template rewrite gates can decide
      // whether this callsite must remain unresolved in this pass.
      return false;
    }
    outArgs.push_back(it->second);
  }
  if (canConsumeImplicitInferenceFact) {
    ctx.implicitTemplateArgInferenceFacts[implicitInferenceFactKey] =
        ImplicitTemplateArgInferenceFact{outArgs};
  }
  if (ctx.collectImplicitTemplateArgFactsForTesting) {
    ctx.implicitTemplateArgFactsForTesting.push_back(
        ImplicitTemplateArgResolutionFactForTesting{
            implicitInferenceFactScopePath,
            callExpr.name,
            def.fullPath,
            joinTemplateArgs(outArgs),
        });
  }
  return true;
}

} // namespace primec
