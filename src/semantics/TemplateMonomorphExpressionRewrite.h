bool rewriteExpr(Expr &expr,
                 const SubstMap &mapping,
                 const std::unordered_set<std::string> &allowedParams,
                 const std::string &namespacePrefix,
                 Context &ctx,
                 std::string &error,
                 const LocalTypeMap &locals,
                 const std::vector<ParameterInfo> &params,
                 bool allowMathBare) {
  expr.namespacePrefix = namespacePrefix;
  if (!rewriteTransforms(expr.transforms, mapping, allowedParams, namespacePrefix, ctx, error)) {
    return false;
  }
  if (expr.kind != Expr::Kind::Call) {
    return true;
  }
  if (expr.isLambda) {
    std::unordered_set<std::string> lambdaAllowed = allowedParams;
    for (const auto &param : expr.templateArgs) {
      lambdaAllowed.insert(param);
    }
    SubstMap lambdaMapping = mapping;
    for (const auto &param : expr.templateArgs) {
      lambdaMapping.erase(param);
    }
    LocalTypeMap lambdaLocals = locals;
    lambdaLocals.reserve(lambdaLocals.size() + expr.args.size() + expr.bodyArguments.size());
    for (auto &param : expr.args) {
      if (!rewriteExpr(param, lambdaMapping, lambdaAllowed, namespacePrefix, ctx, error, lambdaLocals, params,
                       allowMathBare)) {
        return false;
      }
      BindingInfo info;
      if (extractExplicitBindingType(param, info)) {
        if (info.typeName == "auto" && param.args.size() == 1 &&
            inferBindingTypeForMonomorph(param.args.front(), {}, {}, allowMathBare, ctx, info)) {
          lambdaLocals[param.name] = info;
        } else {
          lambdaLocals[param.name] = info;
        }
      } else if (param.isBinding && param.args.size() == 1) {
        if (inferBindingTypeForMonomorph(param.args.front(), {}, {}, allowMathBare, ctx, info)) {
          lambdaLocals[param.name] = info;
        }
      }
    }
    for (auto &bodyArg : expr.bodyArguments) {
      if (!rewriteExpr(bodyArg, lambdaMapping, lambdaAllowed, namespacePrefix, ctx, error, lambdaLocals, params,
                       allowMathBare)) {
        return false;
      }
      BindingInfo info;
      if (extractExplicitBindingType(bodyArg, info)) {
        if (info.typeName == "auto" && bodyArg.args.size() == 1 &&
            inferBindingTypeForMonomorph(bodyArg.args.front(), params, lambdaLocals, allowMathBare, ctx, info)) {
          lambdaLocals[bodyArg.name] = info;
        } else {
          lambdaLocals[bodyArg.name] = info;
        }
      } else if (bodyArg.isBinding && bodyArg.args.size() == 1) {
        if (inferBindingTypeForMonomorph(bodyArg.args.front(), params, lambdaLocals, allowMathBare, ctx, info)) {
          lambdaLocals[bodyArg.name] = info;
        }
      }
    }
    return true;
  }

  if (expr.templateArgs.empty()) {
    std::string explicitBase;
    std::string explicitArgText;
    if (splitTemplateTypeName(expr.name, explicitBase, explicitArgText)) {
      std::vector<std::string> explicitArgs;
      if (!splitTopLevelTemplateArgs(explicitArgText, explicitArgs)) {
        error = "invalid template arguments for " + expr.name;
        return false;
      }
      expr.name = explicitBase;
      expr.templateArgs = std::move(explicitArgs);
    }
  }

  auto isCanonicalBuiltinMapHelperPath = [](const std::string &path) {
    return path == "/std/collections/map/count" || path == "/std/collections/map/contains" ||
           path == "/std/collections/map/tryAt" || path == "/std/collections/map/at" ||
           path == "/std/collections/map/at_unsafe" ||
           path == "/std/collections/map/insert" ||
           path == "/std/collections/map/count_ref" ||
           path == "/std/collections/map/contains_ref" ||
           path == "/std/collections/map/tryAt_ref" ||
           path == "/std/collections/map/at_ref" ||
           path == "/std/collections/map/at_unsafe_ref" ||
           path == "/std/collections/map/insert_ref";
  };
  auto isCanonicalStdlibCollectionHelperPath = [&](const std::string &path) {
    if (isCanonicalBuiltinMapHelperPath(path)) {
      return true;
    }
    const std::string canonicalSoaToAosPath =
        canonicalizeLegacySoaToAosHelperPath(path);
    return path == "/std/collections/vector/count" || path == "/std/collections/vector/capacity" ||
           path == "/std/collections/vector/push" || path == "/std/collections/vector/pop" ||
           path == "/std/collections/vector/reserve" || path == "/std/collections/vector/clear" ||
           path == "/std/collections/vector/remove_at" || path == "/std/collections/vector/remove_swap" ||
           path == "/std/collections/vector/at" || path == "/std/collections/vector/at_unsafe" ||
           path == "/std/collections/soa_vector/count" || path == "/std/collections/soa_vector/count_ref" ||
           path == "/std/collections/soa_vector/get" || path == "/std/collections/soa_vector/get_ref" ||
           path == "/std/collections/soa_vector/ref" || path == "/std/collections/soa_vector/ref_ref" ||
           path == "/std/collections/soa_vector/reserve" || path == "/std/collections/soa_vector/push" ||
           isLegacyOrCanonicalSoaHelperPath(canonicalSoaToAosPath, "to_aos") ||
           isLegacyOrCanonicalSoaHelperPath(canonicalSoaToAosPath, "to_aos_ref");
  };
  auto isSyntheticSamePathSoaHelperTemplateCarryPath = [&](const std::string &path) {
    auto isSyntheticSamePathSoaCarryNonRefHelperPath = [](const std::string &candidate) {
      if (isLegacyOrCanonicalSoaHelperPath(candidate, "count") ||
          isLegacyOrCanonicalSoaHelperPath(candidate, "push") ||
          isLegacyOrCanonicalSoaHelperPath(candidate, "reserve")) {
        return true;
      }
      const std::string getCanonicalPath =
          canonicalizeLegacySoaGetHelperPath(candidate);
      return isLegacyOrCanonicalSoaHelperPath(getCanonicalPath, "get") ||
             isLegacyOrCanonicalSoaHelperPath(getCanonicalPath, "get_ref");
    };
    const std::string canonicalPath = canonicalizeLegacySoaRefHelperPath(path);
    return isSyntheticSamePathSoaCarryNonRefHelperPath(path) ||
           isCanonicalSoaRefLikeHelperPath(canonicalPath);
  };
  auto mapHelperReceiverExpr = [&](const Expr &candidate) -> const Expr * {
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
  auto mutableMapHelperReceiverExpr = [&](Expr &candidate) -> Expr * {
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
  auto resolvesBuiltinMapReceiver = [&](const Expr *receiverExpr) {
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
  auto resolvesBuiltinVectorReceiver = [&](const Expr *receiverExpr) {
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
  auto resolveExperimentalSoaVectorReceiverTemplateArgs =
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
            if ((normalizedBase == "SoaVector" || normalizedBase.rfind("SoaVector__", 0) == 0 ||
                 normalizedBase == "std/collections/experimental_soa_vector/SoaVector" ||
                 normalizedBase.rfind("std/collections/experimental_soa_vector/SoaVector__", 0) == 0) &&
                !argText.empty()) {
              return splitTopLevelTemplateArgs(argText, templateArgsOut) && templateArgsOut.size() == 1;
            }
            return false;
          }
          std::string resolvedPath = receiverTypeText;
          if (!resolvedPath.empty() && resolvedPath.front() != '/') {
            resolvedPath.insert(resolvedPath.begin(), '/');
          }
          std::string normalizedResolvedPath = normalizeBindingTypeName(resolvedPath);
          if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
            normalizedResolvedPath.erase(normalizedResolvedPath.begin());
          }
          if (normalizedResolvedPath.rfind("std/collections/experimental_soa_vector/SoaVector__", 0) != 0) {
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
            const std::string normalizedBase = normalizeBindingTypeName(base);
            if (normalizedBase != "SoaVector" &&
                normalizedBase != "std/collections/experimental_soa_vector/SoaVector") {
              continue;
            }
            return splitTopLevelTemplateArgs(argText, templateArgsOut) && templateArgsOut.size() == 1;
          }
          return false;
        };
        BindingInfo receiverInfo;
        if (inferBindingTypeForMonomorph(*receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
            inferFromTypeText(bindingTypeToString(receiverInfo))) {
          return true;
        }
        return inferFromTypeText(
            inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare));
      };
  auto resolvesExperimentalSoaVectorReceiver = [&](const Expr *receiverExpr) {
    std::vector<std::string> receiverTemplateArgs;
    return resolveExperimentalSoaVectorReceiverTemplateArgs(receiverExpr, receiverTemplateArgs);
  };
  auto inferCollectionReceiverFamily = [&](const Expr *receiverExpr) -> std::string {
    auto inferFromTypeText = [&](std::string receiverTypeText) -> std::string {
      if (receiverTypeText.empty()) {
        return {};
      }
      while (true) {
        std::string base;
        std::string argText;
        if (!splitTemplateTypeName(receiverTypeText, base, argText) || base.empty()) {
          return normalizeCollectionReceiverTypeName(receiverTypeText);
        }
        const std::string normalizedBase = normalizeCollectionReceiverTypeName(base);
        if (normalizedBase != "Reference" && normalizedBase != "Pointer") {
          return normalizedBase;
        }
        std::vector<std::string> receiverArgs;
        if (!splitTopLevelTemplateArgs(argText, receiverArgs) || receiverArgs.size() != 1) {
          return {};
        }
        receiverTypeText = receiverArgs.front();
      }
    };
    if (receiverExpr == nullptr) {
      return {};
    }
    BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(*receiverExpr, params, locals, allowMathBare, ctx, receiverInfo)) {
      const std::string family = inferFromTypeText(bindingTypeToString(receiverInfo));
      if (!family.empty()) {
        return family;
      }
    }
    return inferFromTypeText(
        inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare));
  };
  auto preferCanonicalStdlibVectorFamilyHelperPath = [&](const std::string &path) {
    const Expr *receiverExpr = mapHelperReceiverExpr(expr);
    if (receiverExpr == nullptr) {
      return path;
    }
    auto hasDefinitionFamilyPath = [&](std::string_view candidatePath) {
      const std::string ownedPath(candidatePath);
      if (ctx.sourceDefs.count(ownedPath) > 0 || ctx.helperOverloads.count(ownedPath) > 0) {
        return true;
      }
      const std::string templatedPrefix = ownedPath + "<";
      const std::string specializedPrefix = ownedPath + "__t";
      for (const auto &[defPath, _] : ctx.sourceDefs) {
        if (defPath == ownedPath || defPath.rfind(templatedPrefix, 0) == 0 ||
            defPath.rfind(specializedPrefix, 0) == 0) {
          return true;
        }
      }
      return false;
    };
    auto hasVisibleRootBuiltinSoaConversionHelper = [&](std::string_view helperPath) {
      auto matchesBuiltinSoaHelper = [&](const std::string &helperPath) {
        auto defIt = ctx.sourceDefs.find(helperPath);
        if (defIt == ctx.sourceDefs.end() || defIt->second.parameters.empty()) {
          return false;
        }
        BindingInfo paramBinding;
        if (!extractExplicitBindingType(defIt->second.parameters.front(), paramBinding)) {
          return false;
        }
        return normalizeBindingTypeName(paramBinding.typeName) == "soa_vector" &&
               !paramBinding.typeTemplateArg.empty();
      };
      const std::string ownedHelperPath(helperPath);
      if (matchesBuiltinSoaHelper(ownedHelperPath)) {
        return true;
      }
      auto familyIt = ctx.helperOverloads.find(ownedHelperPath);
      if (familyIt == ctx.helperOverloads.end()) {
        return false;
      }
      for (const auto &entry : familyIt->second) {
        if (matchesBuiltinSoaHelper(entry.internalPath)) {
          return true;
        }
      }
      return false;
    };
    auto resolvesBuiltinSoaToAosShadowReceiver = [&](const Expr *candidate) {
      if (candidate == nullptr) {
        return false;
      }
      if (inferCollectionReceiverFamily(candidate) == "soa_vector") {
        return true;
      }
      if (candidate->kind != Expr::Kind::Call || candidate->isBinding) {
        return false;
      }
      std::string resolvedReceiverPath;
      if (candidate->isMethodCall) {
        if (!resolveMethodCallTemplateTarget(*candidate, locals, ctx, resolvedReceiverPath)) {
          resolvedReceiverPath.clear();
        }
      } else {
        resolvedReceiverPath = resolveCalleePath(*candidate, namespacePrefix, ctx);
      }
      auto defIt = ctx.sourceDefs.find(resolvedReceiverPath);
      if (defIt == ctx.sourceDefs.end()) {
        return false;
      }
      BindingInfo inferredReturn;
      if (!inferDefinitionReturnBindingForTemplatedFallback(
              defIt->second, hasMathImport(ctx), const_cast<Context &>(ctx), inferredReturn)) {
        return false;
      }
      return normalizeBindingTypeName(inferredReturn.typeName) == "soa_vector" &&
             !inferredReturn.typeTemplateArg.empty();
    };
    std::string helperName;
    bool resolvesVectorFamilyPath = false;
    if (path.rfind("/std/collections/vector/", 0) == 0) {
      helperName = path.substr(std::string("/std/collections/vector/").size());
      resolvesVectorFamilyPath = true;
    } else if (path.rfind("/std/collections/soa_vector/", 0) == 0) {
      helperName = path.substr(std::string("/std/collections/soa_vector/").size());
    } else if (!expr.isMethodCall &&
               !path.empty() &&
               path.front() == '/' &&
               path.find('/', 1) == std::string::npos &&
               ctx.sourceDefs.count(path) == 0 &&
               ctx.helperOverloads.count(path) == 0) {
      helperName = path.substr(1);
    } else {
      return path;
    }
    if (helperName != "count" && helperName != "count_ref" &&
        helperName != "capacity" && helperName != "push" &&
        helperName != "reserve" && helperName != "get" &&
        helperName != "get_ref" && helperName != "ref" &&
        helperName != "ref_ref" && helperName != "to_aos" &&
        helperName != "to_aos_ref") {
      return path;
    }
    const std::string receiverFamily = inferCollectionReceiverFamily(receiverExpr);
    if (helperName == "count" || helperName == "push" || helperName == "reserve") {
      const std::string samePathSoaNonRefHelper = "/soa_vector/" + helperName;
      const bool receiverEligibleForSamePathSoaHelper =
          receiverFamily == "soa_vector" ||
          (helperName == "count" && receiverFamily == "vector");
      if (receiverEligibleForSamePathSoaHelper &&
          hasDefinitionFamilyPath(samePathSoaNonRefHelper) &&
          isLegacyOrCanonicalSoaHelperPath(samePathSoaNonRefHelper,
                                           helperName)) {
        return samePathSoaNonRefHelper;
      }
    }
    if (helperName == "get" || helperName == "get_ref") {
      const std::string samePathGetHelper = "/soa_vector/" + helperName;
      const std::string canonicalGetHelper =
          canonicalizeLegacySoaGetHelperPath(samePathGetHelper);
      if (hasDefinitionFamilyPath(samePathGetHelper) &&
          isLegacyOrCanonicalSoaHelperPath(canonicalGetHelper, helperName) &&
          (receiverFamily == "soa_vector" || receiverFamily == "vector")) {
        return samePathGetHelper;
      }
    }
    if (helperName == "ref" || helperName == "ref_ref") {
      const std::string samePathRefHelper = "/soa_vector/" + helperName;
      const std::string canonicalRefHelper =
          canonicalizeLegacySoaRefHelperPath(samePathRefHelper);
      if (hasDefinitionFamilyPath(samePathRefHelper) &&
          isCanonicalSoaRefLikeHelperPath(canonicalRefHelper) &&
          (receiverFamily == "soa_vector" || receiverFamily == "vector")) {
        return samePathRefHelper;
      }
    }
    if (helperName == "to_aos" || helperName == "to_aos_ref") {
      const std::string samePathToAosHelper = "/" + helperName;
      if (hasVisibleRootBuiltinSoaConversionHelper(samePathToAosHelper) &&
          resolvesBuiltinSoaToAosShadowReceiver(receiverExpr)) {
        return samePathToAosHelper;
      }
    }
    if (receiverFamily == "soa_vector") {
      const std::string preferred = "/std/collections/soa_vector/" + helperName;
      if (ctx.sourceDefs.count(preferred) > 0 &&
          (resolvesVectorFamilyPath ||
           hasVisibleStdCollectionsImportForPath(ctx, preferred))) {
        return preferred;
      }
    }
    if (!resolvesVectorFamilyPath &&
        (receiverFamily == "vector" || receiverFamily == "array" ||
         (helperName == "count" && receiverFamily == "string"))) {
      const std::string preferred = "/std/collections/vector/" + helperName;
      if (hasVisibleStdCollectionsImportForPath(ctx, preferred) &&
          ctx.sourceDefs.count(preferred) > 0) {
        return preferred;
      }
    }
    return path;
  };
  auto shouldDeferStdlibCollectionHelperTemplateRewrite = [&](const std::string &path) {
    if (!expr.templateArgs.empty() || !isCanonicalStdlibCollectionHelperPath(path)) {
      return false;
    }
    if (hasVisibleStdCollectionsImportForPath(ctx, path) && ctx.templateDefs.count(path) > 0) {
      if (path.rfind("/std/collections/vector/", 0) == 0 &&
          !resolvesBuiltinVectorReceiver(mapHelperReceiverExpr(expr)) &&
          !resolvesExperimentalVectorValueReceiver(
              mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
        return true;
      }
      return false;
    }
    if (isCanonicalBuiltinMapHelperPath(path)) {
      return resolvesBuiltinMapReceiver(mapHelperReceiverExpr(expr)) && ctx.templateDefs.count(path) == 0;
    }
    return true;
  };
  std::function<bool(Expr &)> rewriteNestedExperimentalMapConstructorValue;
  std::function<bool(Expr &)> rewriteNestedExperimentalMapResultOkPayloadValue;
  std::function<bool(Expr &)> rewriteNestedExperimentalVectorConstructorValue;
  std::function<bool(const std::string &, Expr &)> rewriteMapTargetValueForResolvedType;
  std::function<bool(const std::string &, Expr &)> rewriteVectorTargetValueForResolvedType;

  rewriteMapTargetValueForResolvedType = [&](const std::string &typeText, Expr &valueExpr) -> bool {
    return rewriteExperimentalMapTargetValueForType(typeText,
                                                    valueExpr,
                                                    mapping,
                                                    allowedParams,
                                                    namespacePrefix,
                                                    ctx,
                                                    rewriteNestedExperimentalMapConstructorValue,
                                                    rewriteNestedExperimentalMapResultOkPayloadValue);
  };
  rewriteVectorTargetValueForResolvedType = [&](const std::string &typeText, Expr &valueExpr) -> bool {
    return rewriteExperimentalVectorTargetValueForType(typeText,
                                                       valueExpr,
                                                       rewriteNestedExperimentalVectorConstructorValue);
  };

  auto rewriteCanonicalExperimentalMapConstructorBinding = [&](Expr &bindingExpr) -> bool {
    return rewriteExperimentalConstructorBinding(
        bindingExpr,
        params,
        locals,
        allowMathBare,
        ctx,
        [&](const std::string &bindingTypeText) {
          return resolvesExperimentalMapValueTypeText(bindingTypeText,
                                                      mapping,
                                                      allowedParams,
                                                      namespacePrefix,
                                                      ctx);
        },
        "map",
        rewriteMapTargetValueForResolvedType);
  };
  auto rewriteCanonicalExperimentalVectorConstructorBinding = [&](Expr &bindingExpr) -> bool {
    return rewriteExperimentalConstructorBinding(
        bindingExpr,
        params,
        locals,
        allowMathBare,
        ctx,
        [&](const std::string &bindingTypeText) {
          return resolvesExperimentalVectorValueTypeText(bindingTypeText);
        },
        "vector",
        rewriteVectorTargetValueForResolvedType);
  };
  rewriteNestedExperimentalMapConstructorValue = [&](Expr &candidate) -> bool {
    return rewriteExperimentalConstructorValueTree(candidate, [&](Expr &current) {
      return rewriteCanonicalExperimentalMapConstructorExpr(current,
                                                            locals,
                                                            params,
                                                            mapping,
                                                            allowedParams,
                                                            namespacePrefix,
                                                            ctx,
                                                            allowMathBare,
                                                            error);
    });
  };

  rewriteNestedExperimentalMapResultOkPayloadValue = [&](Expr &candidate) -> bool {
    return rewriteExperimentalMapResultOkPayloadTree(candidate, rewriteNestedExperimentalMapConstructorValue);
  };
  rewriteNestedExperimentalVectorConstructorValue = [&](Expr &candidate) -> bool {
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
  };

  if (expr.isBinding) {
    if (!rewriteCanonicalExperimentalVectorConstructorBinding(expr)) {
      return false;
    }
    if (!rewriteCanonicalExperimentalMapConstructorBinding(expr)) {
      return false;
    }
  }

  bool allConcrete = true;
  for (auto &templArg : expr.templateArgs) {
    ResolvedType resolvedArg = resolveTypeString(templArg, mapping, allowedParams, namespacePrefix, ctx, error);
    if (!error.empty()) {
      return false;
    }
    if (!resolvedArg.concrete) {
      allConcrete = false;
    }
    templArg = resolvedArg.text;
  }
  if (!expr.isMethodCall && !expr.isBinding) {
    if (Expr *receiverExpr = mutableMapHelperReceiverExpr(expr)) {
      if (!rewriteNestedExperimentalMapConstructorValue(*receiverExpr)) {
        return false;
      }
      if (!rewriteNestedExperimentalVectorConstructorValue(*receiverExpr)) {
        return false;
      }
      if (!rewriteExpr(*receiverExpr,
                       mapping,
                       allowedParams,
                       namespacePrefix,
                       ctx,
                       error,
                       locals,
                       params,
                       allowMathBare)) {
        return false;
      }
    }
    std::string resolvedPath = resolveCalleePath(expr, namespacePrefix, ctx);
    const std::string preferredVectorFamilyPath = preferCanonicalStdlibVectorFamilyHelperPath(resolvedPath);
    if (preferredVectorFamilyPath != resolvedPath) {
      resolvedPath = preferredVectorFamilyPath;
      expr.name = preferredVectorFamilyPath;
      expr.namespacePrefix.clear();
    }
    const bool resolvesBorrowedExperimentalMapReceiver =
        resolvesExperimentalMapBorrowedReceiver(
            mapHelperReceiverExpr(expr), params, locals, allowMathBare, mapping, allowedParams, namespacePrefix, ctx);
    const std::string borrowedCanonicalMapUnknownTarget = canonicalMapHelperUnknownTargetPath(resolvedPath);
    if (!borrowedCanonicalMapUnknownTarget.empty() &&
        resolvesBorrowedExperimentalMapReceiver) {
      error = "unknown call target: " + borrowedCanonicalMapUnknownTarget;
      return false;
    }
    const std::string experimentalMapPath = experimentalMapHelperPathForCanonicalHelper(resolvedPath);
    if (!experimentalMapPath.empty() && ctx.sourceDefs.count(experimentalMapPath) > 0 &&
        resolvesExperimentalMapValueReceiver(
            mapHelperReceiverExpr(expr), params, locals, allowMathBare, mapping, allowedParams, namespacePrefix, ctx)) {
      resolvedPath = experimentalMapPath;
      expr.name = experimentalMapPath;
      expr.namespacePrefix.clear();
      if (expr.templateArgs.empty()) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalMapValueReceiverTemplateArgs(
                mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
        }
      }
      if (Expr *receiverExpr = mutableMapHelperReceiverExpr(expr)) {
        if (!rewriteNestedExperimentalMapConstructorValue(*receiverExpr)) {
          return false;
        }
      }
    }
    const std::string experimentalWrapperMapPath = experimentalMapHelperPathForWrapperHelper(resolvedPath);
    if (!experimentalWrapperMapPath.empty() && ctx.sourceDefs.count(experimentalWrapperMapPath) > 0 &&
        resolvesExperimentalMapValueReceiver(
            mapHelperReceiverExpr(expr), params, locals, allowMathBare, mapping, allowedParams, namespacePrefix, ctx)) {
      resolvedPath = experimentalWrapperMapPath;
      expr.name = experimentalWrapperMapPath;
      expr.namespacePrefix.clear();
      if (expr.templateArgs.empty()) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalMapValueReceiverTemplateArgs(
                mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
        }
      }
      if (Expr *receiverExpr = mutableMapHelperReceiverExpr(expr)) {
        if (!rewriteNestedExperimentalMapConstructorValue(*receiverExpr)) {
          return false;
        }
      }
    }
    const std::string experimentalVectorPath = experimentalVectorHelperPathForCanonicalHelper(resolvedPath);
    if (!experimentalVectorPath.empty() && ctx.sourceDefs.count(experimentalVectorPath) > 0 &&
        hasVisibleStdCollectionsImportForPath(ctx, resolvedPath) &&
        resolvesExperimentalVectorValueReceiver(
            mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
      resolvedPath = experimentalVectorPath;
      expr.name = experimentalVectorPath;
      expr.namespacePrefix.clear();
      if (expr.templateArgs.empty()) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalVectorValueReceiverTemplateArgs(
                mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
        }
      }
      if (Expr *receiverExpr = mutableMapHelperReceiverExpr(expr)) {
        if (!rewriteNestedExperimentalVectorConstructorValue(*receiverExpr)) {
          return false;
        }
      }
    }
    const std::string experimentalSoaVectorPath = experimentalSoaVectorHelperPathForCanonicalHelper(resolvedPath);
    if (!experimentalSoaVectorPath.empty() &&
        ctx.sourceDefs.count(experimentalSoaVectorPath) > 0 &&
        resolvesExperimentalSoaVectorReceiver(mapHelperReceiverExpr(expr))) {
      resolvedPath = experimentalSoaVectorPath;
      expr.name = experimentalSoaVectorPath;
      expr.namespacePrefix.clear();
      if (expr.templateArgs.empty()) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalSoaVectorReceiverTemplateArgs(mapHelperReceiverExpr(expr), receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
        }
      }
    }
    if (expr.templateArgs.empty() &&
        resolvedPath.rfind("/std/collections/experimental_map/", 0) == 0 &&
        resolvesExperimentalMapValueReceiver(
            mapHelperReceiverExpr(expr), params, locals, allowMathBare, mapping, allowedParams, namespacePrefix, ctx)) {
      std::vector<std::string> receiverTemplateArgs;
      if (resolveExperimentalMapValueReceiverTemplateArgs(
              mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
        expr.templateArgs = std::move(receiverTemplateArgs);
      }
    }
    if (expr.templateArgs.empty() &&
        resolvedPath.rfind("/std/collections/experimental_vector/", 0) == 0 &&
        resolvesExperimentalVectorValueReceiver(
            mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
      std::vector<std::string> receiverTemplateArgs;
      if (resolveExperimentalVectorValueReceiverTemplateArgs(
              mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
        expr.templateArgs = std::move(receiverTemplateArgs);
      }
    }
    if (expr.templateArgs.empty() &&
        (resolvedPath.rfind("/std/collections/experimental_soa_vector/", 0) == 0 ||
         resolvedPath.rfind("/std/collections/experimental_soa_vector_conversions/", 0) == 0) &&
        resolvesExperimentalSoaVectorReceiver(mapHelperReceiverExpr(expr))) {
      std::vector<std::string> receiverTemplateArgs;
      if (resolveExperimentalSoaVectorReceiverTemplateArgs(mapHelperReceiverExpr(expr), receiverTemplateArgs)) {
        expr.templateArgs = std::move(receiverTemplateArgs);
      }
    }
    if (resolvedPath.rfind("/std/collections/experimental_map/", 0) == 0 &&
        resolvedPath.find("__t") != std::string::npos) {
      expr.templateArgs.clear();
    }
    if (resolvedPath.rfind("/std/collections/experimental_vector/", 0) == 0 &&
        resolvedPath.find("__t") != std::string::npos) {
      expr.templateArgs.clear();
    }
    if ((resolvedPath.rfind("/std/collections/experimental_soa_vector/", 0) == 0 ||
         resolvedPath.rfind("/std/collections/experimental_soa_vector_conversions/", 0) == 0) &&
        resolvedPath.find("__t") != std::string::npos) {
      expr.templateArgs.clear();
    }
    const std::string originalResolvedPath = resolvedPath;
    const std::string preferredPath = preferVectorStdlibHelperPath(resolvedPath, ctx.sourceDefs);
    if (preferredPath != resolvedPath && ctx.sourceDefs.count(preferredPath) > 0) {
      resolvedPath = preferredPath;
      expr.name = preferredPath;
    }
    const bool explicitCompatibilityAliasToCanonicalTemplate =
        expr.templateArgs.empty() && isExplicitCollectionCompatibilityAliasPath(originalResolvedPath) &&
        preferredPath != originalResolvedPath && ctx.templateDefs.count(preferredPath) > 0;
    const bool resolvedWasTemplate = ctx.templateDefs.count(resolvedPath) > 0;
    const bool isBuiltinMapCountPath =
        resolvedPath == "/std/collections/map/count" || resolvedPath == "/map/count";
    const bool isKnownDef = ctx.sourceDefs.count(resolvedPath) > 0;
    if (!expr.templateArgs.empty() && !resolvedWasTemplate && !isKnownDef && isBuiltinMapCountPath) {
      error = "count does not accept template arguments";
      return false;
    }
    if (!expr.templateArgs.empty() && !resolvedWasTemplate) {
      if (!shouldPreserveCompatibilityTemplatePath(resolvedPath, ctx) &&
          !shouldPreserveCanonicalMapTemplatePath(resolvedPath, ctx)) {
        const std::string templatePreferredPath = preferVectorStdlibTemplatePath(resolvedPath, ctx);
        if (templatePreferredPath != resolvedPath) {
          resolvedPath = templatePreferredPath;
          expr.name = templatePreferredPath;
        }
      }
    }
    if (expr.templateArgs.empty() && !explicitCompatibilityAliasToCanonicalTemplate) {
      const std::string implicitTemplatePreferredPath =
          preferVectorStdlibImplicitTemplatePath(expr, resolvedPath, locals, params, allowMathBare, ctx, namespacePrefix);
      if (implicitTemplatePreferredPath != resolvedPath) {
        resolvedPath = implicitTemplatePreferredPath;
        expr.name = implicitTemplatePreferredPath;
      }
    }
    if (ctx.helperOverloadInternalToPublic.count(resolvedPath) > 0) {
      expr.name = resolvedPath;
      expr.namespacePrefix.clear();
    }
    const Expr *resolvedReceiverExpr = mapHelperReceiverExpr(expr);
    const std::string resolvedReceiverFamily =
        inferCollectionReceiverFamily(resolvedReceiverExpr);
    const bool isSyntheticSamePathSoaHelperTemplateCarry =
        isSyntheticSamePathSoaHelperTemplateCarryPath(resolvedPath) &&
        ctx.templateDefs.count(resolvedPath) == 0 &&
        !expr.templateArgs.empty() &&
        resolvedReceiverExpr != nullptr &&
        ((resolvedReceiverExpr->kind == Expr::Kind::Call &&
          !resolvedReceiverExpr->isBinding) ||
         resolvedReceiverFamily == "vector");
    if (isSyntheticSamePathSoaHelperTemplateCarry) {
      expr.templateArgs.clear();
    }
    const bool isTemplateDef = ctx.templateDefs.count(resolvedPath) > 0;
    if (isTemplateDef) {
      auto defIt = ctx.sourceDefs.find(resolvedPath);
      const bool shouldInferImplicitTemplateTail =
          defIt != ctx.sourceDefs.end() &&
          !expr.templateArgs.empty() &&
          ctx.implicitTemplateDefs.count(resolvedPath) > 0 &&
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
            return false;
          }
        }
      }
      if (expr.templateArgs.empty()) {
        if (shouldDeferStdlibCollectionHelperTemplateRewrite(resolvedPath)) {
          return true;
        }
        error = "template arguments required for " + helperOverloadDisplayPath(resolvedPath, ctx);
        return false;
      }
      if (allConcrete) {
        std::string specializedPath;
        if (!instantiateTemplate(resolvedPath, expr.templateArgs, ctx, error, specializedPath)) {
          return false;
        }
        expr.name = specializedPath;
        expr.templateArgs.clear();
      }
    } else if (isKnownDef && !expr.templateArgs.empty()) {
      error = "template arguments are only supported on templated definitions: " +
              helperOverloadDisplayPath(resolvedPath, ctx);
      return false;
    }
    auto defIt = ctx.sourceDefs.find(resolveCalleePath(expr, namespacePrefix, ctx));
    if (defIt != ctx.sourceDefs.end()) {
      if (!rewriteExperimentalConstructorArgsForTarget(
              expr,
              defIt->second,
              false,
              allowMathBare,
              ctx,
              [&](const std::string &typeText, Expr &argExpr) {
                return rewriteVectorTargetValueForResolvedType(typeText, argExpr);
              })) {
        return false;
      }
      if (!rewriteExperimentalConstructorArgsForTarget(
              expr,
              defIt->second,
              false,
              allowMathBare,
              ctx,
              [&](const std::string &typeText, Expr &argExpr) {
                return rewriteMapTargetValueForResolvedType(typeText, argExpr);
              })) {
        return false;
      }
    }
    rewriteExperimentalAssignTargetValue(
        expr,
        params,
        locals,
        allowMathBare,
        namespacePrefix,
        ctx,
        [&](const std::string &targetTypeText, Expr &valueExpr) {
          return rewriteVectorTargetValueForResolvedType(targetTypeText, valueExpr);
        });
    rewriteExperimentalInitTargetValue(
        expr,
        params,
        locals,
        allowMathBare,
        namespacePrefix,
        ctx,
        [&](const std::string &targetTypeText, Expr &valueExpr) {
          return rewriteVectorTargetValueForResolvedType(targetTypeText, valueExpr);
        });
    rewriteExperimentalAssignTargetValue(
        expr,
        params,
        locals,
        allowMathBare,
        namespacePrefix,
        ctx,
        [&](const std::string &targetTypeText, Expr &valueExpr) {
          return rewriteMapTargetValueForResolvedType(targetTypeText, valueExpr);
        });
    rewriteExperimentalInitTargetValue(
        expr,
        params,
        locals,
        allowMathBare,
        namespacePrefix,
        ctx,
        [&](const std::string &targetTypeText, Expr &valueExpr) {
          return rewriteMapTargetValueForResolvedType(targetTypeText, valueExpr);
        });
  }
  if (expr.isMethodCall) {
    if (!expr.args.empty()) {
      if (!rewriteNestedExperimentalMapConstructorValue(expr.args.front())) {
        return false;
      }
      if (!rewriteNestedExperimentalVectorConstructorValue(expr.args.front())) {
        return false;
      }
    }
    const bool methodCallSyntax = expr.isMethodCall;
    std::string methodPath;
    if (resolveMethodCallTemplateTarget(expr, locals, ctx, methodPath)) {
      const std::string experimentalVectorMethodPath =
          experimentalVectorHelperPathForCanonicalHelper(methodPath);
      const std::string experimentalSoaVectorMethodPath =
          experimentalSoaVectorHelperPathForCanonicalHelper(methodPath);
      const bool shouldRewriteCanonicalVectorMethodToExperimental =
          ctx.sourceDefs.count(methodPath) == 0 && ctx.helperOverloads.count(methodPath) == 0 &&
          hasVisibleStdCollectionsImportForPath(ctx, methodPath);
      if (shouldRewriteCanonicalVectorMethodToExperimental &&
          !experimentalVectorMethodPath.empty() &&
          ctx.sourceDefs.count(experimentalVectorMethodPath) > 0 &&
          resolvesExperimentalVectorValueReceiver(
              mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
        methodPath = experimentalVectorMethodPath;
        if (expr.templateArgs.empty()) {
          std::vector<std::string> receiverTemplateArgs;
          if (resolveExperimentalVectorValueReceiverTemplateArgs(
                  mapHelperReceiverExpr(expr),
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
      const bool methodWasTemplate = ctx.templateDefs.count(methodPath) > 0;
      if (!expr.templateArgs.empty() && !methodWasTemplate) {
        if (!shouldPreserveCompatibilityTemplatePath(methodPath, ctx) &&
            !shouldPreserveCanonicalMapTemplatePath(methodPath, ctx)) {
          methodPath = preferVectorStdlibTemplatePath(methodPath, ctx);
        }
      }
      if (expr.templateArgs.empty()) {
          methodPath =
            preferVectorStdlibImplicitTemplatePath(expr, methodPath, locals, params, allowMathBare, ctx, namespacePrefix);
      }
      if (!experimentalSoaVectorMethodPath.empty() &&
          resolvesExperimentalSoaVectorReceiver(mapHelperReceiverExpr(expr))) {
        methodPath = experimentalSoaVectorMethodPath;
        if (expr.templateArgs.empty()) {
          std::vector<std::string> receiverTemplateArgs;
          if (resolveExperimentalSoaVectorReceiverTemplateArgs(mapHelperReceiverExpr(expr), receiverTemplateArgs)) {
            expr.templateArgs = std::move(receiverTemplateArgs);
            allConcrete = true;
          }
        }
      }
      if (expr.templateArgs.empty() &&
          isExperimentalVectorPublicHelperPath(methodPath) &&
          resolvesExperimentalVectorValueReceiver(
              mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalVectorValueReceiverTemplateArgs(
                mapHelperReceiverExpr(expr),
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
          (methodPath.rfind("/std/collections/experimental_soa_vector/", 0) == 0 ||
           methodPath.rfind("/std/collections/experimental_soa_vector_conversions/", 0) == 0) &&
          resolvesExperimentalSoaVectorReceiver(mapHelperReceiverExpr(expr))) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalSoaVectorReceiverTemplateArgs(mapHelperReceiverExpr(expr), receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
          allConcrete = true;
        }
      }
      if (methodPath.rfind("/std/collections/experimental_map/", 0) == 0 &&
          methodPath.find("__t") != std::string::npos) {
        expr.templateArgs.clear();
      }
      if (methodPath.rfind("/std/collections/experimental_vector/", 0) == 0 &&
          methodPath.find("__t") != std::string::npos) {
        expr.templateArgs.clear();
      }
      if ((methodPath.rfind("/std/collections/experimental_soa_vector/", 0) == 0 ||
           methodPath.rfind("/std/collections/experimental_soa_vector_conversions/", 0) == 0) &&
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
      const Expr *resolvedReceiverExpr = mapHelperReceiverExpr(expr);
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
              return false;
            }
          }
        }
        if (expr.templateArgs.empty()) {
          if (shouldDeferStdlibCollectionHelperTemplateRewrite(methodPath)) {
            return true;
          }
          error = "template arguments required for " + helperOverloadDisplayPath(methodPath, ctx);
          return false;
        }
        if (allConcrete) {
          std::string specializedPath;
          if (!instantiateTemplate(methodPath, expr.templateArgs, ctx, error, specializedPath)) {
            return false;
          }
          expr.name = specializedPath;
          expr.templateArgs.clear();
          expr.isMethodCall = false;
        }
      } else if (isKnownDef && !expr.templateArgs.empty()) {
        error = "template arguments are only supported on templated definitions: " +
                helperOverloadDisplayPath(methodPath, ctx);
        return false;
      }
      std::string rewrittenMethodPath =
          expr.isMethodCall ? methodPath : resolveCalleePath(expr, namespacePrefix, ctx);
      auto methodDefIt = ctx.sourceDefs.find(rewrittenMethodPath);
      if (methodDefIt == ctx.sourceDefs.end()) {
        methodDefIt = ctx.sourceDefs.find(resolveCalleePath(expr, namespacePrefix, ctx));
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
          return false;
        }
        if (!rewriteExperimentalConstructorArgsForTarget(
                expr,
                methodDefIt->second,
                methodCallSyntax,
                allowMathBare,
                ctx,
                [&](const std::string &typeText, Expr &argExpr) {
                  return rewriteMapTargetValueForResolvedType(typeText, argExpr);
                })) {
          return false;
        }
      }
    }
  }

  for (auto &arg : expr.args) {
    if (!rewriteExpr(arg, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, allowMathBare)) {
      return false;
    }
  }
  LocalTypeMap bodyLocals = locals;
  for (auto &arg : expr.bodyArguments) {
    if (!rewriteExpr(arg, mapping, allowedParams, namespacePrefix, ctx, error, bodyLocals, params, allowMathBare)) {
      return false;
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
  return true;
}
