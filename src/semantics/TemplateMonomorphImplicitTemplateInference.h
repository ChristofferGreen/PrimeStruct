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
        if (def.fullPath.rfind("/std/collections/vector/", 0) == 0 ||
            def.fullPath.rfind("/std/collections/soa_vector/", 0) == 0 ||
            def.fullPath.rfind("/std/collections/map/", 0) == 0 ||
            def.fullPath.rfind("/map/", 0) == 0) {
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
        return normalizedReceiverType == "vector" || normalizedReceiverType == "soa_vector" ||
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
  std::vector<const Expr *> orderedArgs;
  std::vector<const Expr *> packedArgs;
  size_t packedParamIndex = callParams.size();
  size_t callArgStart = 0;
  size_t paramIndexOffset = 0;
  auto assignBindingFromTypeText = [](const std::string &typeText, BindingInfo &bindingOut) -> bool {
    const std::string normalizedType = normalizeBindingTypeName(typeText);
    if (normalizedType.empty()) {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
      bindingOut.typeName = base;
      bindingOut.typeTemplateArg = argText;
      return true;
    }
    bindingOut.typeName = normalizedType;
    bindingOut.typeTemplateArg.clear();
    return true;
  };
  auto unsupportedBuiltinSoaPendingDiagnostic = [&](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.name.empty()) {
      return {};
    }
    const auto normalizeCallName = [](std::string name) {
      if (!name.empty() && name.front() == '/') {
        name.erase(name.begin());
      }
      if (const size_t suffix = name.find("__t"); suffix != std::string::npos) {
        name.erase(suffix);
      }
      return name;
    };
    std::string resolvedPath;
    if (candidate.isMethodCall) {
      if (!resolveMethodCallTemplateTarget(candidate, locals, ctx, resolvedPath)) {
        resolvedPath.clear();
      }
    }
    if (candidate.args.empty()) {
      return {};
    }
    std::string normalizedPrefix = candidate.namespacePrefix;
    if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
      normalizedPrefix.erase(normalizedPrefix.begin());
    }
    const std::string normalizedName = normalizeCallName(candidate.name);
    if (normalizedName.empty()) {
      return {};
    }
    auto resolvesBuiltinSoaReceiver = [&](const Expr &receiverExpr) {
      auto matchesTypeText = [&](std::string typeText) {
        typeText = normalizeBindingTypeName(typeText);
        if (typeText.empty()) {
          return false;
        }
        while (true) {
          std::string base;
          std::string argText;
          if (!splitTemplateTypeName(typeText, base, argText) || base.empty()) {
            return normalizeCollectionReceiverTypeName(typeText) == "soa_vector";
          }
          const std::string normalizedBase = normalizeCollectionReceiverTypeName(base);
          if ((normalizedBase == "Reference" || normalizedBase == "Pointer") &&
              !argText.empty()) {
            std::vector<std::string> wrappedArgs;
            if (!splitTopLevelTemplateArgs(argText, wrappedArgs) || wrappedArgs.size() != 1) {
              return false;
            }
            typeText = normalizeBindingTypeName(wrappedArgs.front());
            continue;
          }
          return normalizedBase == "soa_vector";
        }
      };
      BindingInfo receiverInfo;
      if (inferBindingTypeForMonomorph(receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
          matchesTypeText(bindingTypeToString(receiverInfo))) {
        return true;
      }
      return matchesTypeText(
          inferExprTypeTextForTemplatedVectorFallback(
              receiverExpr, locals, namespacePrefix, ctx, allowMathBare));
    };
    if (!resolvesBuiltinSoaReceiver(candidate.args.front())) {
      return {};
    }
    auto hasVisibleSoaBorrowedHelper = [&](std::string_view helperName) {
      const std::string samePath = "/soa_vector/" + std::string(helperName);
      const std::string canonicalPath =
          "/std/collections/soa_vector/" + std::string(helperName);
      return ctx.sourceDefs.count(samePath) > 0 ||
             ctx.helperOverloads.count(samePath) > 0 ||
             ctx.sourceDefs.count(canonicalPath) > 0 ||
             ctx.helperOverloads.count(canonicalPath) > 0;
    };
    const bool hasVisibleSoaRefHelper =
        hasVisibleSoaBorrowedHelper("ref");
    const bool hasVisibleSoaRefRefHelper =
        hasVisibleSoaBorrowedHelper("ref_ref");
    const std::string resolvedSoaCanonical =
        canonicalizeLegacySoaRefHelperPath(resolvedPath);
    const std::string normalizedNameSoaCanonical =
        canonicalizeLegacySoaRefHelperPath("/" + normalizedName);
    const std::string normalizedNameSoaPath = "/" + normalizedName;
    const bool normalizedNameUsesCanonicalSoaNamespace =
        normalizedName.rfind("std/collections/soa_vector/", 0) == 0;
    const bool normalizedNameUsesLegacySoaNamespace =
        normalizedName.rfind("soa_vector/", 0) == 0;
    const std::string normalizedPrefixedSoaPath =
        normalizedPrefix.empty()
            ? std::string{}
            : "/" + normalizedPrefix + "/" + normalizedName;
    const bool normalizedPrefixedUsesLegacySoaNamespace =
        normalizedPrefixedSoaPath.rfind("/soa_vector/", 0) == 0;
    const bool normalizedPrefixedNameMatchesSoaRef =
        isLegacyOrCanonicalSoaHelperPath(normalizedPrefixedSoaPath, "ref");
    const bool normalizedPrefixedNameMatchesSoaRefRef =
        isLegacyOrCanonicalSoaHelperPath(
            normalizedPrefixedSoaPath, "ref_ref");
    const bool normalizedCanonicalNameMatchesSoaRef =
        isLegacyOrCanonicalSoaHelperPath(normalizedNameSoaCanonical, "ref");
    const bool normalizedCanonicalNameMatchesSoaRefRef =
        isLegacyOrCanonicalSoaHelperPath(
            normalizedNameSoaCanonical, "ref_ref");
    const bool resolvedCanonicalNameMatchesSoaRef =
        isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, "ref");
    const bool resolvedCanonicalNameMatchesSoaRefRef =
        isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, "ref_ref");
    const bool normalizedNameMatchesSoaRef =
        isLegacyOrCanonicalSoaHelperPath(normalizedNameSoaPath, "ref");
    const bool normalizedNameMatchesSoaRefRef =
        isLegacyOrCanonicalSoaHelperPath(normalizedNameSoaPath, "ref_ref");
    const bool hasAnyNormalizedNameSoaRefMatch =
        normalizedNameMatchesSoaRef || normalizedNameMatchesSoaRefRef;
    const bool canonicalNamespaceNameMatchesSoaRef =
        normalizedNameUsesCanonicalSoaNamespace &&
        normalizedCanonicalNameMatchesSoaRef;
    const bool canonicalNamespaceNameMatchesSoaRefRef =
        normalizedNameUsesCanonicalSoaNamespace &&
        normalizedCanonicalNameMatchesSoaRefRef;
    const bool isCanonicalBuiltinSoaRefCall =
        canonicalNamespaceNameMatchesSoaRef ||
        resolvedCanonicalNameMatchesSoaRef;
    const bool isCanonicalBuiltinSoaRefRefCall =
        canonicalNamespaceNameMatchesSoaRefRef ||
        resolvedCanonicalNameMatchesSoaRefRef;
    const bool isOldSurfaceBuiltinSoaRefRefCall =
        normalizedNameUsesLegacySoaNamespace &&
        normalizedNameMatchesSoaRefRef;
    const bool isAnyCanonicalOrOldSurfaceBuiltinSoaRefRefCall =
        isCanonicalBuiltinSoaRefRefCall || isOldSurfaceBuiltinSoaRefRefCall;
    const bool hasAnyCanonicalNamespaceNameSoaRefMatch =
        canonicalNamespaceNameMatchesSoaRef ||
        canonicalNamespaceNameMatchesSoaRefRef;
    const bool hasAnyResolvedCanonicalNameSoaRefMatch =
        resolvedCanonicalNameMatchesSoaRef ||
        resolvedCanonicalNameMatchesSoaRefRef;
    const bool isAnyCanonicalBuiltinSoaRefCall =
        hasAnyCanonicalNamespaceNameSoaRefMatch ||
        hasAnyResolvedCanonicalNameSoaRefMatch;
    (void)isCanonicalBuiltinSoaRefCall;
    const bool isAnyOldSurfaceBuiltinSoaRefCall =
        normalizedNameUsesLegacySoaNamespace &&
        hasAnyNormalizedNameSoaRefMatch;
    const bool isAnyCanonicalOrOldSurfaceBuiltinSoaRefCall =
        isAnyCanonicalBuiltinSoaRefCall || isAnyOldSurfaceBuiltinSoaRefCall;
    const std::string normalizedMethodSoaPath =
        "/soa_vector/" + normalizedName;
    const bool normalizedMethodNameMatchesSoaRef =
        isLegacyOrCanonicalSoaHelperPath(normalizedMethodSoaPath, "ref");
    const bool normalizedMethodNameMatchesSoaRefRef =
        isLegacyOrCanonicalSoaHelperPath(normalizedMethodSoaPath, "ref_ref");
    const bool isAnyNormalizedMethodNameSoaRefCall =
        normalizedMethodNameMatchesSoaRef || normalizedMethodNameMatchesSoaRefRef;
    const bool hasAnyBuiltinSoaRefNameMatch =
        isAnyNormalizedMethodNameSoaRefCall ||
        isAnyCanonicalOrOldSurfaceBuiltinSoaRefCall;
    const bool isAnyBuiltinSoaRefCall = hasAnyBuiltinSoaRefNameMatch;
    if (isAnyBuiltinSoaRefCall) {
      const bool hasAnyBuiltinSoaRefRefNameMatch =
          normalizedMethodNameMatchesSoaRefRef ||
          isAnyCanonicalOrOldSurfaceBuiltinSoaRefRefCall;
      const bool isAnyBuiltinSoaRefRefCall =
          hasAnyBuiltinSoaRefRefNameMatch;
      const char *missingSoaRefHelperPath =
          isAnyBuiltinSoaRefRefCall ? "/std/collections/soa_vector/ref_ref"
                                    : "/std/collections/soa_vector/ref";
      const bool hasVisibleMissingSoaRefHelper =
          isAnyBuiltinSoaRefRefCall ? hasVisibleSoaRefRefHelper
                                    : hasVisibleSoaRefHelper;
      if (hasVisibleMissingSoaRefHelper) {
        return {};
      }
      const bool hasLeadingCallArg =
          !candidate.args.empty() &&
          candidate.args.front().kind == Expr::Kind::Call;
      const bool hasCanonicalBuiltinSoaRefCallArg =
          isAnyCanonicalBuiltinSoaRefCall && hasLeadingCallArg;
      if (hasCanonicalBuiltinSoaRefCallArg) {
        return soaUnavailableMethodDiagnostic(missingSoaRefHelperPath);
      }
      const bool isMethodCall = candidate.isMethodCall;
      const bool isNonMethodCall = !isMethodCall;
      const bool isExplicitLegacySoaNonMethodCall =
          isNonMethodCall && normalizedPrefixedUsesLegacySoaNamespace;
      const bool hasAnyNormalizedPrefixedNameSoaRefMatch =
          normalizedPrefixedNameMatchesSoaRef ||
          normalizedPrefixedNameMatchesSoaRefRef;
      const bool isAnyExplicitLegacySoaNonMethodRefCall =
          isExplicitLegacySoaNonMethodCall &&
          hasAnyNormalizedPrefixedNameSoaRefMatch;
      const bool hasAnyBuiltinSoaRefCanonicalNameMatch =
          normalizedCanonicalNameMatchesSoaRef ||
          normalizedCanonicalNameMatchesSoaRefRef;
      const bool hasAnyBuiltinSoaRefMethodNameMatch =
          isAnyNormalizedMethodNameSoaRefCall ||
          hasAnyBuiltinSoaRefCanonicalNameMatch;
      const bool isAnyBuiltinSoaRefMethodCall =
          isMethodCall && hasAnyBuiltinSoaRefMethodNameMatch;
      const bool isAnyBuiltinSoaRefNonMethodCall =
          isNonMethodCall && isAnyNormalizedMethodNameSoaRefCall;
      const bool isAnyBuiltinSoaRefMethodOrNonMethodCall =
          isAnyBuiltinSoaRefMethodCall || isAnyBuiltinSoaRefNonMethodCall;
      const bool isAnyExplicitSoaRefCall =
          isAnyExplicitLegacySoaNonMethodRefCall ||
          isAnyOldSurfaceBuiltinSoaRefCall;
      const bool isAnyExplicitOrBuiltinSoaRefCall =
          isAnyExplicitSoaRefCall || isAnyBuiltinSoaRefMethodOrNonMethodCall;
      const bool hasCanonicalResolvedSoaRefHelper =
          isCanonicalSoaRefLikeHelperPath(resolvedSoaCanonical);
      const bool hasAnyUnavailableSoaRefHelperCall =
          hasCanonicalResolvedSoaRefHelper || isAnyExplicitOrBuiltinSoaRefCall;
      if (hasAnyUnavailableSoaRefHelperCall) {
        return soaUnavailableMethodDiagnostic(missingSoaRefHelperPath);
      }
      return {};
    }
    if (normalizedName == "count" || normalizedName == "get" ||
        normalizedName == "to_soa" || normalizedName == "to_aos" ||
        normalizedName == "to_aos_ref" ||
        normalizedName == "contains") {
      return {};
    }
    const std::string ownedPath = "/soa_vector/" + normalizedName;
    const std::string canonicalPath =
        "/std/collections/soa_vector/" + normalizedName;
    if (ctx.sourceDefs.count(ownedPath) > 0 ||
        ctx.helperOverloads.count(ownedPath) > 0 ||
        ctx.sourceDefs.count(canonicalPath) > 0 ||
        ctx.helperOverloads.count(canonicalPath) > 0) {
      return {};
    }
    return soaUnavailableMethodDiagnostic(
        soaFieldViewHelperPath(normalizedName));
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
  if (!buildOrderedArguments(callParams, *orderedCallArgs, *orderedCallArgNames, orderedArgs, packedArgs,
                             packedParamIndex, error)) {
    if (error.find("argument count mismatch") != std::string::npos) {
      error = "argument count mismatch for " + def.fullPath;
    }
    return false;
  }
  std::string implicitInferenceFactKey;
  bool canConsumeImplicitInferenceFact = true;
  {
    std::ostringstream key;
    key << (namespacePrefix.empty() ? std::string("/") : namespacePrefix)
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
        factIt->second.inferredArgs.size() == def.templateArgs.size()) {
      outArgs = factIt->second.inferredArgs;
      ++ctx.implicitTemplateArgInferenceFactHitsForTesting;
      if (ctx.collectImplicitTemplateArgFactsForTesting) {
        ctx.implicitTemplateArgFactsForTesting.push_back(
            ImplicitTemplateArgResolutionFactForTesting{
                namespacePrefix.empty() ? std::string("/") : namespacePrefix,
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
    const bool inferFromPackedArgs = callParamIndex == packedParamIndex && isArgsPackBinding(paramInfo) &&
                                     implicitSet.count(paramInfo.typeTemplateArg) > 0;
    bool inferFromWrappedTemplateArgs = false;
    std::vector<std::string> inferredParamNames;
    if (inferFromPackedArgs) {
      inferredParamNames.push_back(paramInfo.typeTemplateArg);
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
    if (inferFromPackedArgs) {
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
    if (argsToInfer.empty()) {
      error = "implicit template arguments require values on " + def.fullPath;
      return false;
    }
    for (const Expr *argExpr : argsToInfer) {
      BindingInfo argInfo;
      if (!argExpr) {
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
        if (normalizeCollectionReceiverTypeName(argBaseType) !=
            normalizeCollectionReceiverTypeName(paramInfo.typeName)) {
          if (isStdlibCollectionHelper) {
            return false;
          }
          error = "unable to infer implicit template arguments for " + def.fullPath;
          return false;
        }
        std::vector<std::string> argTemplateArgs;
        if (argTemplateArgText.empty()) {
          if (!extractExperimentalVectorValueReceiverTemplateArgsFromTypeText(argBaseType, ctx, argTemplateArgs) &&
              !extractExperimentalSoaVectorValueReceiverTemplateArgsFromTypeText(
                  argBaseType, ctx, argTemplateArgs) &&
              !extractExperimentalMapValueReceiverTemplateArgsFromTypeText(argBaseType, ctx, argTemplateArgs)) {
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
        error = "implicit template arguments must be concrete on " + def.fullPath;
        return false;
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
  for (const auto &paramName : def.templateArgs) {
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
            namespacePrefix.empty() ? std::string("/") : namespacePrefix,
            callExpr.name,
            def.fullPath,
            joinTemplateArgs(outArgs),
        });
  }
  return true;
}
