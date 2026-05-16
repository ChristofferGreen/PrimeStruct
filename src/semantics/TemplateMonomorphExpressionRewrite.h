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

  auto resolvePickSumDefinition = [&](const Expr &target) -> Definition * {
    BindingInfo targetInfo;
    if (!inferBindingTypeForMonomorph(target, params, locals, allowMathBare, ctx,
                                      targetInfo)) {
      return nullptr;
    }
    std::string targetTypeText = bindingTypeToString(targetInfo);
    if (targetTypeText.empty()) {
      return nullptr;
    }
    std::string resolveError;
    ResolvedType resolvedType =
        resolveTypeString(targetTypeText, mapping, allowedParams, namespacePrefix,
                          ctx, resolveError);
    if (!resolveError.empty() || resolvedType.text.empty()) {
      return nullptr;
    }
    targetTypeText = normalizeBindingTypeName(resolvedType.text);
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(targetTypeText, base, argText)) {
      targetTypeText = normalizeBindingTypeName(base);
    }
    std::string sumPath = targetTypeText;
    if (sumPath.empty()) {
      return nullptr;
    }
    if (sumPath.front() != '/') {
      sumPath = resolveNameToPath(sumPath,
                                  namespacePrefix,
                                  scopedImportAliasesForNamespace(namespacePrefix,
                                                                  ctx),
                                  ctx.sourceDefs);
    }
    auto sumIt = ctx.sourceDefs.find(sumPath);
    if (sumIt == ctx.sourceDefs.end() ||
        !isSumDefinitionForMonomorphRefresh(sumIt->second)) {
      return nullptr;
    }
    return &sumIt->second;
  };

  auto appendPickPayloadLocal = [](const SumVariant &variant,
                                   const Expr &arm,
                                   LocalTypeMap &armLocals) {
    if (!variant.hasPayload || arm.args.size() != 1) {
      return;
    }
    const Expr &payloadBinder = arm.args.front();
    if (payloadBinder.kind != Expr::Kind::Name || payloadBinder.name.empty()) {
      return;
    }
    BindingInfo payloadInfo;
    payloadInfo.typeName = variant.payloadType;
    if (!variant.payloadTemplateArgs.empty()) {
      payloadInfo.typeTemplateArg = joinTemplateArgs(variant.payloadTemplateArgs);
    }
    armLocals[payloadBinder.name] = std::move(payloadInfo);
  };

  auto recordBodyBindingLocal = [&](Expr &bodyExpr, LocalTypeMap &bodyLocals) {
    BindingInfo info;
    if (extractExplicitBindingType(bodyExpr, info)) {
      if (info.typeName == "auto" && bodyExpr.args.size() == 1 &&
          inferBindingTypeForMonomorph(bodyExpr.args.front(),
                                       params,
                                       bodyLocals,
                                       allowMathBare,
                                       ctx,
                                       info)) {
        bodyLocals[bodyExpr.name] = info;
      } else {
        bodyLocals[bodyExpr.name] = info;
      }
      return;
    }
    if (bodyExpr.isBinding && bodyExpr.args.size() == 1 &&
        inferBindingTypeForMonomorph(bodyExpr.args.front(),
                                     params,
                                     bodyLocals,
                                     allowMathBare,
                                     ctx,
                                     info)) {
      bodyLocals[bodyExpr.name] = info;
    }
  };

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
        return false;
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
        return false;
      }
      for (auto &armArg : arm.args) {
        armArg.namespacePrefix = namespacePrefix;
        if (!rewriteTransforms(armArg.transforms,
                               mapping,
                               allowedParams,
                               namespacePrefix,
                               ctx,
                               error)) {
          return false;
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
          return false;
        }
        recordBodyBindingLocal(bodyExpr, armLocals);
      }
    }
    return true;
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
           isCanonicalSoaHelperPath(canonicalSoaCountPath, "count_ref") ||
           isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, "get") ||
           isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, "get_ref") ||
           isCanonicalSoaRefLikeHelperPath(path) ||
           isCanonicalSoaHelperPath(canonicalSoaCountPath, "reserve") ||
           isCanonicalSoaHelperPath(canonicalSoaCountPath, "push") ||
           isLegacyOrCanonicalSoaHelperPath(
               canonicalSoaToAosPath, templateMonomorphSoaToAosHelperName()) ||
           isLegacyOrCanonicalSoaHelperPath(
               canonicalSoaToAosPath, templateMonomorphSoaToAosHelperName(true));
  };
  auto isTemplatedAutoCompatVectorHelperPath = [](std::string_view path) {
    const std::string compatibilityPrefix =
        "/std/collections/" + std::string("vector");
    return path == compatibilityPrefix + "At" ||
           path == compatibilityPrefix + "AtUnsafe";
  };
  auto isSyntheticSamePathSoaHelperTemplateCarryPath = [&](const std::string &path) {
    auto isSyntheticSamePathSoaCarryNonRefHelperPath = [](const std::string &candidate) {
      if (isLegacyOrCanonicalSoaHelperPath(candidate, "count") ||
          isLegacyOrCanonicalSoaHelperPath(candidate, "count_ref") ||
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
           isCanonicalSoaRefLikeHelperPath(canonicalPath) ||
           isExperimentalSoaGetLikeHelperPath(path) ||
           isExperimentalSoaRefLikeHelperPath(path);
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
  auto inferCollectionReceiverFamilyForRewrite = [&](const Expr *receiverExpr) {
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
  auto resolvesSoaReceiverForRewrite = [&](const Expr &receiverExpr) {
    return isTemplateMonomorphSoaReceiverType(
               inferCollectionReceiverFamilyForRewrite(&receiverExpr)) ||
           resolvesExperimentalSoaReceiverForRewrite(receiverExpr);
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
            return splitTopLevelTemplateArgs(argText, templateArgsOut) && templateArgsOut.size() == 1;
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
  auto resolvesExperimentalSoaVectorReceiver = [&](const Expr *receiverExpr) {
    std::vector<std::string> receiverTemplateArgs;
    return resolveExperimentalSoaVectorReceiverTemplateArgs(receiverExpr, receiverTemplateArgs);
  };
  auto resolvesBorrowedExperimentalSoaVectorReceiver = [&](const Expr *receiverExpr) {
    if (receiverExpr == nullptr) {
      return false;
    }
    auto inferFromTypeText = [&](std::string receiverTypeText) {
      if (receiverTypeText.empty()) {
        return false;
      }
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(normalizeBindingTypeName(receiverTypeText), base, argText)) {
        return false;
      }
      const std::string normalizedBase = normalizeBindingTypeName(base);
      if ((normalizedBase != "Reference" && normalizedBase != "Pointer") ||
          argText.empty()) {
        return false;
      }
      std::vector<std::string> wrappedArgs;
      if (!splitTopLevelTemplateArgs(argText, wrappedArgs) || wrappedArgs.size() != 1) {
        return false;
      }
      std::vector<std::string> receiverTemplateArgs;
      return extractExperimentalSoaVectorValueReceiverTemplateArgsFromTypeText(
          wrappedArgs.front(), ctx, receiverTemplateArgs);
    };
    auto definitionReturnsBorrowedExperimentalSoaVector =
        [&](const Definition &definition) {
          BindingInfo inferredReturn;
          if (inferDefinitionReturnBindingForTemplatedFallback(
                  definition, allowMathBare, ctx, inferredReturn) &&
              inferFromTypeText(bindingTypeToString(inferredReturn))) {
            return true;
          }
          for (const auto &transform : definition.transforms) {
            if (transform.name == "return" &&
                transform.templateArgs.size() == 1 &&
                inferFromTypeText(transform.templateArgs.front())) {
              return true;
            }
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
        if (definitionReturnsBorrowedExperimentalSoaVector(defIt->second)) {
          return true;
        }
      }
      if (!receiverExpr->name.empty()) {
        for (const auto &[defPath, definition] : ctx.sourceDefs) {
          const size_t slash = defPath.find_last_of('/');
          const std::string leaf =
              slash == std::string::npos ? defPath : defPath.substr(slash + 1);
          if (leaf == receiverExpr->name &&
              definitionReturnsBorrowedExperimentalSoaVector(definition)) {
            return true;
          }
        }
      }
    }
    return inferFromTypeText(
        inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare));
  };
  auto resolvesConcreteExperimentalSoaVectorReceiver = [&](const Expr *receiverExpr) {
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
        return isExperimentalSoaVectorSpecializedTypePath(normalizedBase);
      }
      if (!receiverTypeText.empty() && receiverTypeText.front() != '/') {
        receiverTypeText.insert(receiverTypeText.begin(), '/');
      }
      return isExperimentalSoaVectorSpecializedTypePath(
          normalizeBindingTypeName(receiverTypeText));
    };
    BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(*receiverExpr,
                                     params,
                                     locals,
                                     allowMathBare,
                                     ctx,
                                     receiverInfo) &&
        inferFromTypeText(bindingTypeToString(receiverInfo))) {
      return true;
    }
    return inferFromTypeText(inferExprTypeTextForTemplatedVectorFallback(
        *receiverExpr, locals, namespacePrefix, ctx, allowMathBare));
  };
  auto inferCollectionReceiverFamily = [&](const Expr *receiverExpr) -> std::string {
    auto inferFromTypeText = [&](std::string receiverTypeText) -> std::string {
      if (receiverTypeText.empty()) {
        return std::string{};
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
  auto preferredBorrowedPathForPublicOrCompatibility =
      [](const std::string &canonicalPath, std::string_view borrowedHelper) {
        if (canonicalPath.rfind(templateMonomorphPublicSoaHelperPrefix(), 0) == 0) {
          return publicSoaHelperTargetPath(borrowedHelper);
        }
        return compatibilitySoaHelperTargetPath(borrowedHelper);
      };
  auto isCanonicalSoaBorrowedWrapperHelper = [&](const std::string &path) {
    auto canonicalizeSoaHelperPath = [](std::string canonicalPath) {
      const size_t specializationSuffix = canonicalPath.find("__");
      if (specializationSuffix != std::string::npos) {
        canonicalPath.erase(specializationSuffix);
      }
      return canonicalPath;
    };
    const std::string canonicalSoaCountPath = canonicalizeSoaHelperPath(path);
    const std::string canonicalSoaGetPath =
        canonicalizeSoaHelperPath(canonicalizeLegacySoaGetHelperPath(path));
    const std::string canonicalSoaRefPath =
        canonicalizeSoaHelperPath(canonicalizeLegacySoaRefHelperPath(path));
    const std::string canonicalSoaToAosPath =
        canonicalizeLegacySoaToAosHelperPath(path);
    return isLegacyOrCanonicalSoaHelperPath(canonicalSoaCountPath, "count_ref") ||
           isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, "get_ref") ||
           isLegacyOrCanonicalSoaHelperPath(canonicalSoaRefPath, "ref_ref") ||
           isLegacyOrCanonicalSoaHelperPath(
               canonicalSoaToAosPath, templateMonomorphSoaToAosHelperName(true));
  };
  auto preferredBorrowedSoaWrapperPath = [&](const std::string &path) {
    auto canonicalizeSoaHelperPath = [](std::string canonicalPath) {
      const size_t specializationSuffix = canonicalPath.find("__");
      if (specializationSuffix != std::string::npos) {
        canonicalPath.erase(specializationSuffix);
      }
      return canonicalPath;
    };
    const std::string canonicalSoaCountPath = canonicalizeSoaHelperPath(path);
    const std::string canonicalSoaGetPath =
        canonicalizeSoaHelperPath(canonicalizeLegacySoaGetHelperPath(path));
    const std::string canonicalSoaRefPath =
        canonicalizeSoaHelperPath(canonicalizeLegacySoaRefHelperPath(path));
    const std::string canonicalSoaToAosPath =
        canonicalizeLegacySoaToAosHelperPath(path);
    if (path == "/count" || path == "count") {
      return compatibilitySoaHelperTargetPath("count_ref");
    }
    if (path == "/get" || path == "get") {
      return compatibilitySoaHelperTargetPath("get_ref");
    }
    if (path == "/ref" || path == "ref") {
      return compatibilitySoaHelperTargetPath("ref_ref");
    }
    const std::string samePathSoaToAos =
        "/" + templateMonomorphSoaToAosHelperName();
    if (path == samePathSoaToAos ||
        path == templateMonomorphSoaToAosHelperName()) {
      return compatibilitySoaHelperTargetPath(
          templateMonomorphSoaToAosHelperName(true));
    }
    if (isLegacyOrCanonicalSoaHelperPath(canonicalSoaCountPath, "count")) {
      return preferredBorrowedPathForPublicOrCompatibility(canonicalSoaCountPath,
                                                           "count_ref");
    }
    if (isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, "get")) {
      return preferredBorrowedPathForPublicOrCompatibility(canonicalSoaGetPath,
                                                           "get_ref");
    }
    if (isLegacyOrCanonicalSoaHelperPath(canonicalSoaRefPath, "ref")) {
      return preferredBorrowedPathForPublicOrCompatibility(canonicalSoaRefPath,
                                                           "ref_ref");
    }
    if (isLegacyOrCanonicalSoaHelperPath(
            canonicalSoaToAosPath, templateMonomorphSoaToAosHelperName())) {
      return compatibilitySoaHelperTargetPath(
          templateMonomorphSoaToAosHelperName(true));
    }
    return std::string{};
  };
  auto preferCanonicalStdlibCollectionHelperPath = [&](const std::string &path) {
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
        return isTemplateMonomorphSoaReceiverType(
                   normalizeBindingTypeName(paramBinding.typeName)) &&
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
      if (isTemplateMonomorphSoaReceiverType(
              inferCollectionReceiverFamily(candidate))) {
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
      return isTemplateMonomorphSoaReceiverType(
                 normalizeBindingTypeName(inferredReturn.typeName)) &&
             !inferredReturn.typeTemplateArg.empty();
    };
    auto canonicalizeSoaHelperPath = [](std::string canonicalPath) {
      const size_t specializationSuffix = canonicalPath.find("__");
      if (specializationSuffix != std::string::npos) {
        canonicalPath.erase(specializationSuffix);
      }
      return canonicalPath;
    };
    std::string helperName;
    bool resolvesVectorFamilyPath = false;
    if (isCanonicalVectorCompatibilityPath(path)) {
      helperName =
          std::string(stripUnrootedCanonicalVectorCompatibilityPrefix(path));
      resolvesVectorFamilyPath = true;
    } else if (path.rfind(templateMonomorphCompatibilitySoaHelperPrefix(), 0) == 0) {
      helperName = path.substr(templateMonomorphCompatibilitySoaHelperPrefix().size());
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
    helperName = canonicalizeSoaHelperPath(helperName);
    if (helperName != "count" && helperName != "count_ref" &&
        helperName != "capacity" && helperName != "push" &&
        helperName != "reserve" && helperName != "get" &&
        helperName != "get_ref" && helperName != "ref" &&
        helperName != "ref_ref" &&
        helperName != templateMonomorphSoaToAosHelperName() &&
        helperName != templateMonomorphSoaToAosHelperName(true)) {
      return path;
    }
    const std::string receiverFamily = inferCollectionReceiverFamily(receiverExpr);
    const bool receiverResolvesExperimentalSoaVector =
        resolvesExperimentalSoaVectorReceiver(receiverExpr);
    const bool receiverResolvesBorrowedExperimentalSoaVector =
        resolvesBorrowedExperimentalSoaVectorReceiver(receiverExpr);
    if (receiverResolvesBorrowedExperimentalSoaVector) {
      if (helperName == "count") {
        helperName = "count_ref";
      } else if (helperName == "get") {
        helperName = "get_ref";
      } else if (helperName == "ref") {
        helperName = "ref_ref";
      } else if (helperName == templateMonomorphSoaToAosHelperName()) {
        helperName = templateMonomorphSoaToAosHelperName(true);
      }
    }
    if (receiverFamily == "vector" &&
        (helperName == "count" || helperName == "count_ref" ||
         helperName == "capacity")) {
      const std::string samePathVectorHelper =
          "/" + std::string("vector") + "/" + helperName;
      if (hasDefinitionFamilyPath(samePathVectorHelper)) {
        return samePathVectorHelper;
      }
    }
    const auto vectorReceiverHasVisibleCanonicalHelper =
        [&](std::string_view candidateHelperName) {
          const std::string preferred =
              canonicalVectorCompatibilityHelperPathOrFallback(candidateHelperName);
          return receiverFamily == "vector" &&
                 hasVisibleStdCollectionsImportForPath(ctx, preferred) &&
                 ctx.sourceDefs.count(preferred) > 0;
        };
    if (helperName == "count" || helperName == "count_ref" ||
        helperName == "push" || helperName == "reserve") {
      const std::string samePathSoaNonRefHelper =
          templateMonomorphSamePathSoaHelperPrefix() + helperName;
      const bool receiverEligibleForSamePathSoaHelper =
          isTemplateMonomorphSoaReceiverType(receiverFamily) ||
          receiverResolvesBorrowedExperimentalSoaVector ||
          receiverResolvesExperimentalSoaVector ||
          ((helperName == "count" || helperName == "count_ref") &&
           receiverFamily == "vector" &&
           !vectorReceiverHasVisibleCanonicalHelper(helperName));
      if (receiverEligibleForSamePathSoaHelper &&
          hasDefinitionFamilyPath(samePathSoaNonRefHelper)) {
        return samePathSoaNonRefHelper;
      }
    }
    if (helperName == "get" || helperName == "get_ref") {
      const std::string samePathGetHelper =
          templateMonomorphSamePathSoaHelperPrefix() + helperName;
      if (hasDefinitionFamilyPath(samePathGetHelper) &&
          (isTemplateMonomorphSoaReceiverType(receiverFamily) ||
           receiverResolvesBorrowedExperimentalSoaVector ||
           receiverResolvesExperimentalSoaVector ||
           receiverFamily == "vector")) {
        return samePathGetHelper;
      }
    }
    if (helperName == "ref" || helperName == "ref_ref") {
      const std::string samePathRefHelper =
          templateMonomorphSamePathSoaHelperPrefix() + helperName;
      if (hasDefinitionFamilyPath(samePathRefHelper) &&
          (isTemplateMonomorphSoaReceiverType(receiverFamily) ||
           receiverResolvesBorrowedExperimentalSoaVector ||
           receiverResolvesExperimentalSoaVector ||
           receiverFamily == "vector")) {
        return samePathRefHelper;
      }
    }
    if (helperName == templateMonomorphSoaToAosHelperName() ||
        helperName == templateMonomorphSoaToAosHelperName(true)) {
      const std::string samePathToAosHelper = "/" + helperName;
      if (hasVisibleRootBuiltinSoaConversionHelper(samePathToAosHelper) &&
          resolvesBuiltinSoaToAosShadowReceiver(receiverExpr)) {
        return samePathToAosHelper;
      }
    }
    if (isTemplateMonomorphSoaReceiverType(receiverFamily) ||
        receiverResolvesBorrowedExperimentalSoaVector) {
      const std::string preferred =
          templateMonomorphCompatibilitySoaHelperPrefix() + helperName;
      if (ctx.sourceDefs.count(preferred) > 0 &&
          (resolvesVectorFamilyPath ||
           hasVisibleStdCollectionsImportForPath(ctx, preferred))) {
        return preferred;
      }
    }
    if (!resolvesVectorFamilyPath && receiverFamily == "map" &&
        (helperName == "count" || helperName == "count_ref")) {
      const std::string preferred = "/std/collections/map/" + helperName;
      if (hasVisibleStdCollectionsImportForPath(ctx, preferred) &&
          ctx.sourceDefs.count(preferred) > 0) {
        return preferred;
      }
    }
    if (!resolvesVectorFamilyPath &&
        (receiverFamily == "vector" || receiverFamily == "array")) {
      const std::string preferred =
          canonicalVectorCompatibilityHelperPathOrFallback(helperName);
      if (hasVisibleStdCollectionsImportForPath(ctx, preferred) &&
          ctx.sourceDefs.count(preferred) > 0) {
        return preferred;
      }
    }
    return path;
  };
  auto shouldDeferStdlibCollectionHelperTemplateRewrite = [&](const std::string &path) {
    if (!expr.templateArgs.empty()) {
      return false;
    }
    if (path == "/std/collections/experimental_map/mapNew" &&
        ctx.templateDefs.count(path) > 0) {
      return true;
    }
    if (isTemplatedAutoCompatVectorHelperPath(path)) {
      return true;
    }
    if (!isCanonicalStdlibCollectionHelperPath(path)) {
      return false;
    }
    const Expr *helperReceiverExpr = mapHelperReceiverExpr(expr);
    const bool borrowedExperimentalSoaReceiver =
        resolvesBorrowedExperimentalSoaVectorReceiver(helperReceiverExpr);
    const bool isCanonicalNonBorrowedSoaHelperPath =
        path == compatibilitySoaHelperTargetPath("count") ||
        path == publicSoaHelperTargetPath("count") ||
        path == compatibilitySoaHelperTargetPath("get") ||
        path == publicSoaHelperTargetPath("get") ||
        path == compatibilitySoaHelperTargetPath("ref") ||
        path == publicSoaHelperTargetPath("ref") ||
        path == compatibilitySoaHelperTargetPath(
                    templateMonomorphSoaToAosHelperName()) ||
        path == publicSoaHelperTargetPath(
                    templateMonomorphSoaToAosHelperName());
    if (borrowedExperimentalSoaReceiver &&
        isCanonicalNonBorrowedSoaHelperPath) {
      return true;
    }
    if ((path.rfind(templateMonomorphCompatibilitySoaHelperPrefix(), 0) == 0 ||
         path.rfind(templateMonomorphPublicSoaHelperPrefix(), 0) == 0) &&
        helperReceiverExpr != nullptr) {
      if (path == publicSoaHelperTargetPath(
                      templateMonomorphSoaToAosHelperName()) ||
          path == publicSoaHelperTargetPath(
                      templateMonomorphSoaToAosHelperName(true))) {
        return true;
      }
      const bool directArgsPackReceiver =
          inferCollectionReceiverFamilyForRewrite(helperReceiverExpr) == "args";
      const bool directBuiltinSoaReceiver =
          isTemplateMonomorphSoaReceiverType(
              inferCollectionReceiverFamily(helperReceiverExpr));
      const bool directExperimentalSoaReceiver =
          resolvesExperimentalSoaVectorReceiver(helperReceiverExpr);
      if (directArgsPackReceiver || directBuiltinSoaReceiver ||
          directExperimentalSoaReceiver ||
          borrowedExperimentalSoaReceiver) {
        return true;
      }
    }
    if (hasVisibleStdCollectionsImportForPath(ctx, path) && ctx.templateDefs.count(path) > 0) {
      if (isCanonicalVectorCompatibilityPath(path) &&
          !resolvesBuiltinVectorReceiver(mapHelperReceiverExpr(expr)) &&
          !resolvesExperimentalVectorValueReceiver(
              mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
        return true;
      }
      if (path.rfind("/std/collections/map/", 0) == 0) {
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
  if (!expr.isBinding) {
    if (!expr.isMethodCall && expr.namespacePrefix.empty() &&
        expr.name.find('/') == std::string::npos && expr.args.size() == 1 &&
        !hasNamedCallArguments(expr) && expr.templateArgs.empty() &&
        !expr.hasBodyArguments && expr.bodyArguments.empty()) {
      BindingInfo receiverInfo;
      if (inferBindingTypeForMonomorph(expr.args.front(),
                                       params,
                                       locals,
                                       allowMathBare,
                                       ctx,
                                       receiverInfo) &&
          receiverInfo.typeTemplateArg.empty()) {
        const std::string receiverType =
            normalizeBindingTypeName(receiverInfo.typeName);
        if (receiverType == "i32" || receiverType == "i64" ||
            receiverType == "u64" || receiverType == "bool" ||
            receiverType == "f32" || receiverType == "f64" ||
            receiverType == "integer" || receiverType == "decimal" ||
            receiverType == "complex") {
          const std::string methodStylePath =
              "/" + receiverType + "/" + expr.name;
          if (ctx.sourceDefs.count(methodStylePath) > 0 ||
              ctx.helperOverloads.count(methodStylePath) > 0) {
            expr.name = methodStylePath;
          }
        }
      }
    }
    if (expr.isMethodCall &&
        (expr.name == "at" || expr.name == "at_unsafe") &&
        expr.namespacePrefix.empty() &&
        !expr.args.empty() &&
        inferCollectionReceiverFamilyForRewrite(&expr.args.front()) == "string") {
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
          normalizedName.rfind(
              templateMonomorphCompatibilitySoaHelperPrefix(false), 0) == 0 ||
          normalizedName.rfind(
              templateMonomorphPublicSoaHelperPrefix(false), 0) == 0 ||
          normalizedPrefix ==
              trimLeadingSlash(compatibilitySoaHelperTargetPath("")) ||
          normalizedPrefix == trimLeadingSlash(publicSoaHelperTargetPath(""));
      if (!usesCanonicalSoaSurface) {
        return std::string{};
      }
      if (normalizedName == "get" || normalizedName == "get_ref" ||
          normalizedName == "ref" || normalizedName == "ref_ref") {
        return normalizedName;
      }
      if (normalizedName == templateMonomorphSamePathSoaHelperPrefix(false) + "get" ||
          normalizedName == templateMonomorphCompatibilitySoaHelperPrefix(false) + "get" ||
          normalizedName == "soa/get" ||
          normalizedName == templateMonomorphPublicSoaHelperPrefix(false) + "get") {
        return std::string("get");
      }
      if (normalizedName == templateMonomorphSamePathSoaHelperPrefix(false) + "get_ref" ||
          normalizedName == templateMonomorphCompatibilitySoaHelperPrefix(false) + "get_ref" ||
          normalizedName == "soa/get_ref" ||
          normalizedName == templateMonomorphPublicSoaHelperPrefix(false) + "get_ref") {
        return std::string("get_ref");
      }
      if (normalizedName == templateMonomorphSamePathSoaHelperPrefix(false) + "ref" ||
          normalizedName == templateMonomorphCompatibilitySoaHelperPrefix(false) + "ref" ||
          normalizedName == "soa/ref" ||
          normalizedName == templateMonomorphPublicSoaHelperPrefix(false) + "ref") {
        return std::string("ref");
      }
      if (normalizedName == templateMonomorphSamePathSoaHelperPrefix(false) + "ref_ref" ||
          normalizedName == templateMonomorphCompatibilitySoaHelperPrefix(false) + "ref_ref" ||
          normalizedName == "soa/ref_ref" ||
          normalizedName == templateMonomorphPublicSoaHelperPrefix(false) + "ref_ref") {
        return std::string("ref_ref");
      }
      if ((normalizedPrefix == templateMonomorphSoaReceiverTypeName() ||
           normalizedPrefix == trimLeadingSlash(compatibilitySoaHelperTargetPath("")) ||
           normalizedPrefix == "soa" ||
           normalizedPrefix == trimLeadingSlash(publicSoaHelperTargetPath(""))) &&
          (normalizedName == "get" || normalizedName == "get_ref" ||
           normalizedName == "ref" || normalizedName == "ref_ref")) {
        return normalizedName;
      }
      return std::string{};
    }();
    if (!helperReturnSoaRefHelper.empty() &&
        ctx.sourceDefs.count(templateMonomorphSamePathSoaHelperPrefix() +
                             helperReturnSoaRefHelper) == 0 &&
        ctx.helperOverloads.count(templateMonomorphSamePathSoaHelperPrefix() +
                                  helperReturnSoaRefHelper) == 0 &&
        !resolvesSoaReceiverForRewrite(expr.args.front())) {
      if (expr.args.size() != 2) {
        error = "argument count mismatch for builtin " + helperReturnSoaRefHelper;
        return false;
      }
      const Expr &indexExpr = expr.args[1];
      if (indexExpr.kind == Expr::Kind::BoolLiteral ||
          indexExpr.kind == Expr::Kind::FloatLiteral ||
          indexExpr.kind == Expr::Kind::StringLiteral) {
        error = helperReturnSoaRefHelper + " requires integer index";
        return false;
      }
      const std::string unavailablePath =
          (helperReturnSoaRefHelper == "get" ||
           helperReturnSoaRefHelper == "get_ref")
              ? compatibilitySoaHelperTargetPath(helperReturnSoaRefHelper)
              : templateMonomorphSamePathSoaHelperPrefix() +
                    helperReturnSoaRefHelper;
      error = soaUnavailableMethodDiagnostic(unavailablePath);
      return false;
    }
  }
  bool allConcrete = true;
  if (!resolveTemplateArgumentList(expr.templateArgs,
                                   mapping,
                                   allowedParams,
                                   namespacePrefix,
                                   ctx,
                                   error,
                                   allConcrete)) {
    return false;
  }
  if (!expr.isMethodCall && !expr.isBinding) {
    if (Expr *receiverExpr = mutableMapHelperReceiverExpr(expr)) {
      if (!isRootMapConstructorReceiverExpr(receiverExpr) &&
          !rewriteNestedExperimentalMapConstructorValue(*receiverExpr)) {
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
    const std::string preferredCollectionHelperPath =
        preferCanonicalStdlibCollectionHelperPath(resolvedPath);
    if (preferredCollectionHelperPath != resolvedPath) {
      resolvedPath = preferredCollectionHelperPath;
      expr.name = preferredCollectionHelperPath;
      expr.namespacePrefix.clear();
    }
    if (!expr.templateArgs.empty() &&
        expr.name.find('/') == std::string::npos &&
        !expr.args.empty()) {
      BindingInfo receiverInfo;
      if (inferBindingTypeForMonomorph(expr.args.front(),
                                       params,
                                       locals,
                                       allowMathBare,
                                       ctx,
                                       receiverInfo)) {
        const std::string wrapperBase =
            normalizeCollectionReceiverTypeName(receiverInfo.typeName);
        if ((wrapperBase == "Reference" || wrapperBase == "Pointer") &&
            !receiverInfo.typeTemplateArg.empty()) {
          const std::string rootedWrapperMethodPath =
              "/" + wrapperBase + "/" + expr.name;
          if (ctx.templateDefs.count(rootedWrapperMethodPath) > 0 &&
              ctx.sourceDefs.count(rootedWrapperMethodPath) > 0) {
            resolvedPath = rootedWrapperMethodPath;
            expr.name = rootedWrapperMethodPath;
            expr.namespacePrefix.clear();
          }
        }
      }
    }
    if (!expr.templateArgs.empty() &&
        expr.name.find('/') == std::string::npos &&
        inferCollectionReceiverFamily(mapHelperReceiverExpr(expr)) == "map" &&
        isRemovedMapCompatibilityHelper(mapCompatibilityHelperBase(expr.name))) {
      const std::string compatibilityPath = "/map/" + std::string(mapCompatibilityHelperBase(expr.name));
      if (ctx.sourceDefs.count(compatibilityPath) > 0 &&
          ctx.templateDefs.count(compatibilityPath) == 0) {
        error = "template arguments are only supported on templated definitions: " +
                helperOverloadDisplayPath(compatibilityPath, ctx);
        return false;
      }
    }
    if (!expr.isMethodCall && expr.templateArgs.empty() &&
        expr.name.find('/') == std::string::npos &&
        (resolvedPath == "/count" || resolvedPath == "/capacity")) {
      const std::string samePathVectorHelper =
          "/" + std::string("vector") + "/" + expr.name;
      auto samePathVectorHelperIt = ctx.sourceDefs.find(samePathVectorHelper);
      if (samePathVectorHelperIt != ctx.sourceDefs.end() &&
          ctx.templateDefs.count(samePathVectorHelper) > 0 &&
          ctx.helperOverloads.count(samePathVectorHelper) == 0) {
        std::vector<std::string> inferredArgs;
        std::string inferenceError;
        if (inferImplicitTemplateArgs(samePathVectorHelperIt->second,
                                      expr,
                                      locals,
                                      params,
                                      mapping,
                                      allowedParams,
                                      namespacePrefix,
                                      ctx,
                                      allowMathBare,
                                      inferredArgs,
                                      inferenceError)) {
          resolvedPath = samePathVectorHelper;
          expr.name = samePathVectorHelper;
          expr.namespacePrefix.clear();
          expr.templateArgs = std::move(inferredArgs);
          allConcrete = true;
        }
      }
    }
    if (!expr.isMethodCall &&
        expr.name.find('/') == std::string::npos &&
        isLegacyOrCanonicalSoaHelperPath(resolvedPath, "count") &&
        !isTemplateMonomorphSoaReceiverType(
            inferCollectionReceiverFamily(mapHelperReceiverExpr(expr)))) {
      return true;
    }
    const std::string preferredBorrowedSoaPath =
        preferredBorrowedSoaWrapperPath(resolvedPath);
    if (!preferredBorrowedSoaPath.empty() &&
        resolvesBorrowedExperimentalSoaVectorReceiver(
            mapHelperReceiverExpr(expr)) &&
        (ctx.sourceDefs.count(preferredBorrowedSoaPath) > 0 ||
         ctx.helperOverloads.count(preferredBorrowedSoaPath) > 0 ||
         ctx.templateDefs.count(preferredBorrowedSoaPath) > 0)) {
      resolvedPath = preferredBorrowedSoaPath;
      expr.name = preferredBorrowedSoaPath;
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
    auto stripGeneratedSuffix = [](std::string path) {
      const size_t suffix = path.find("__");
      if (suffix != std::string::npos) {
        path.erase(suffix);
      }
      return path;
    };
    const std::string removedMapCompatibilityPath =
        stripGeneratedSuffix(resolvedPath);
    const std::string removedMapCompatibilityHelper =
        removedMapCompatibilityPath.rfind("/map/", 0) == 0
            ? removedMapCompatibilityPath.substr(std::string("/map/").size())
            : std::string{};
    const std::string_view removedMapCompatibilityHelperBase =
        mapCompatibilityHelperBase(removedMapCompatibilityHelper);
    if (removedMapCompatibilityPath.rfind("/map/", 0) == 0 &&
        isRemovedMapCompatibilityHelper(removedMapCompatibilityHelperBase) &&
        (removedMapCompatibilityHelperBase == "count" ||
         removedMapCompatibilityHelperBase == "count_ref" ||
         removedMapCompatibilityHelperBase == "size") &&
        ctx.sourceDefs.count(removedMapCompatibilityPath) == 0 &&
        ctx.templateDefs.count(removedMapCompatibilityPath) == 0 &&
        ctx.helperOverloads.count(removedMapCompatibilityPath) == 0) {
      const std::string helperName(removedMapCompatibilityHelperBase);
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error = "block arguments require a definition target: " +
                removedMapCompatibilityPath;
        return false;
      }
      const size_t expectedArgCount =
          (helperName == "count" || helperName == "count_ref" ||
           helperName == "size")
              ? 1
              : ((helperName == "at" || helperName == "at_ref" ||
                  helperName == "at_unsafe" ||
                  helperName == "at_unsafe_ref" ||
                  helperName == "contains" ||
                  helperName == "contains_ref" ||
                  helperName == "tryAt" ||
                  helperName == "tryAt_ref")
                     ? 2
                     : 3);
      if (expr.args.size() != expectedArgCount) {
        error = "argument count mismatch for " + removedMapCompatibilityPath;
        return false;
      }
      error = "unknown call target: " + removedMapCompatibilityPath;
      return false;
    }
    if (removedMapCompatibilityPath.rfind("/map/", 0) == 0 &&
        removedMapCompatibilityPath != resolvedPath &&
        ctx.templateDefs.count(removedMapCompatibilityPath) > 0) {
      resolvedPath = removedMapCompatibilityPath;
      expr.name = removedMapCompatibilityPath;
      expr.namespacePrefix.clear();
    }
    const std::string experimentalMapPath = experimentalMapHelperPathForCanonicalHelper(resolvedPath);
    const Expr *experimentalMapReceiverExpr = mapHelperReceiverExpr(expr);
    const bool receiverIsPublishedMapConstructor =
        isPublishedMapConstructorReceiverExpr(experimentalMapReceiverExpr,
                                              namespacePrefix,
                                              ctx);
    const bool rejectsWrapperReturnedExperimentalMapAccess =
        experimentalMapReceiverExpr != nullptr &&
        experimentalMapReceiverExpr->kind == Expr::Kind::Call &&
        !experimentalMapReceiverExpr->isFieldAccess &&
        !receiverIsPublishedMapConstructor &&
        (borrowedCanonicalMapUnknownTarget == "/std/collections/map/at" ||
         borrowedCanonicalMapUnknownTarget == "/std/collections/map/at_unsafe");
    if (!experimentalMapPath.empty() && ctx.sourceDefs.count(experimentalMapPath) > 0 &&
        resolvesExperimentalMapValueReceiver(
            experimentalMapReceiverExpr, params, locals, allowMathBare, mapping, allowedParams, namespacePrefix, ctx)) {
      if (rejectsWrapperReturnedExperimentalMapAccess) {
        error = "unknown call target: " + borrowedCanonicalMapUnknownTarget;
        return false;
      }
      resolvedPath = experimentalMapPath;
      expr.name = experimentalMapPath;
      expr.namespacePrefix.clear();
      if (expr.templateArgs.empty()) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalMapValueReceiverTemplateArgs(
                experimentalMapReceiverExpr, params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
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
    const Expr *experimentalVectorReceiverExpr = mapHelperReceiverExpr(expr);
    const bool canRewriteNamedExperimentalVectorTemporary =
        experimentalVectorReceiverExpr == nullptr ||
        experimentalVectorReceiverExpr->kind != Expr::Kind::Call ||
        !hasNamedArguments(experimentalVectorReceiverExpr->argNames);
    const std::string experimentalVectorPath = experimentalVectorHelperPathForCanonicalHelper(resolvedPath);
    if (!experimentalVectorPath.empty() && ctx.sourceDefs.count(experimentalVectorPath) > 0 &&
        hasVisibleStdCollectionsImportForPath(ctx, resolvedPath) &&
        canRewriteNamedExperimentalVectorTemporary &&
        resolvesExperimentalVectorValueReceiver(
            experimentalVectorReceiverExpr, params, locals, allowMathBare, namespacePrefix, ctx)) {
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
    const bool shouldRewriteCanonicalSoaHelperToExperimental =
        ctx.sourceDefs.count(resolvedPath) == 0 &&
        ctx.helperOverloads.count(resolvedPath) == 0 &&
        !isCanonicalSoaBorrowedWrapperHelper(resolvedPath) &&
        hasVisibleStdCollectionsImportForPath(ctx, resolvedPath) &&
        resolvesConcreteExperimentalSoaVectorReceiver(
            mapHelperReceiverExpr(expr));
    if (shouldRewriteCanonicalSoaHelperToExperimental &&
        !experimentalSoaVectorPath.empty() &&
        ctx.sourceDefs.count(experimentalSoaVectorPath) > 0) {
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
    bool inferredCanonicalMapReceiverTemplateArgs = false;
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
        resolvedPath.rfind("/std/collections/map/", 0) == 0 &&
        hasVisibleStdCollectionsImportForPath(
            ctx,
            resolvedPath)) {
      std::vector<std::string> receiverTemplateArgs;
      if (resolveExperimentalMapValueReceiverTemplateArgs(
              mapHelperReceiverExpr(expr),
              params,
              locals,
              allowMathBare,
              namespacePrefix,
              ctx,
              receiverTemplateArgs)) {
        expr.templateArgs = std::move(receiverTemplateArgs);
        allConcrete = true;
        inferredCanonicalMapReceiverTemplateArgs = true;
      }
    }
    if (expr.templateArgs.empty() &&
        resolvedPath.rfind(legacyExperimentalVectorCompatibilityPrefix(), 0) == 0 &&
        resolvesExperimentalVectorValueReceiver(
            mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
      std::vector<std::string> receiverTemplateArgs;
      if (resolveExperimentalVectorValueReceiverTemplateArgs(
              mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
        expr.templateArgs = std::move(receiverTemplateArgs);
      }
    }
    if (expr.templateArgs.empty() &&
        isExperimentalSoaVectorPublicHelperPath(resolvedPath) &&
        resolvesExperimentalSoaVectorReceiver(mapHelperReceiverExpr(expr))) {
      std::vector<std::string> receiverTemplateArgs;
      if (resolveExperimentalSoaVectorReceiverTemplateArgs(mapHelperReceiverExpr(expr), receiverTemplateArgs)) {
        expr.templateArgs = std::move(receiverTemplateArgs);
      }
    }
    if (expr.templateArgs.empty() &&
        isCanonicalSoaBorrowedWrapperHelper(resolvedPath) &&
        resolvesBorrowedExperimentalSoaVectorReceiver(mapHelperReceiverExpr(expr))) {
      std::vector<std::string> receiverTemplateArgs;
      if (resolveExperimentalSoaVectorReceiverTemplateArgs(mapHelperReceiverExpr(expr), receiverTemplateArgs)) {
        expr.templateArgs = std::move(receiverTemplateArgs);
        allConcrete = true;
      }
    }
    if (resolvedPath.rfind("/std/collections/experimental_map/", 0) == 0 &&
        resolvedPath.find("__t") != std::string::npos) {
      expr.templateArgs.clear();
    }
    if (resolvedPath.rfind(legacyExperimentalVectorCompatibilityPrefix(), 0) == 0 &&
        resolvedPath.find("__t") != std::string::npos) {
      expr.templateArgs.clear();
    }
    if (isExperimentalSoaVectorPublicHelperPath(resolvedPath) &&
        resolvedPath.find("__t") != std::string::npos) {
      expr.templateArgs.clear();
    }
    if (isCanonicalSoaBorrowedWrapperHelper(resolvedPath) &&
        resolvedPath.find("__t") != std::string::npos) {
      expr.templateArgs.clear();
    }
    if (inferredCanonicalMapReceiverTemplateArgs &&
        (resolvedPath.rfind("/std/collections/map/", 0) == 0 ||
         resolvedPath.rfind("/map/", 0) == 0) &&
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
      if (!shouldPreserveCompatibilityTemplatePath(resolvedPath, ctx)) {
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
    auto preferredConcreteSamePathSoaHelperPath = [&](const std::string &path) {
      auto canonicalizeSoaHelperPath = [](std::string canonicalPath) {
        const size_t specializationSuffix = canonicalPath.find("__");
        if (specializationSuffix != std::string::npos) {
          canonicalPath.erase(specializationSuffix);
        }
        return canonicalPath;
      };
      const std::string canonicalPath = canonicalizeSoaHelperPath(path);
      auto extractHelperName = [&](std::string_view prefix) -> std::string {
        if (canonicalPath.rfind(std::string(prefix), 0) != 0) {
          return {};
        }
        return canonicalPath.substr(prefix.size());
      };
      std::string helperName =
          extractHelperName(templateMonomorphCompatibilitySoaHelperPrefix());
      if (helperName.empty()) {
        return std::string{};
      }
      if (helperName != "count" && helperName != "count_ref" &&
          helperName != "get" && helperName != "get_ref" &&
          helperName != "ref" && helperName != "ref_ref" &&
          helperName != templateMonomorphSoaToAosHelperName() &&
          helperName != templateMonomorphSoaToAosHelperName(true) &&
          helperName != "push" && helperName != "reserve") {
        return std::string{};
      }
      const std::string samePath =
          (helperName == templateMonomorphSoaToAosHelperName() ||
           helperName == templateMonomorphSoaToAosHelperName(true))
              ? "/" + helperName
              : templateMonomorphSamePathSoaHelperPrefix() + helperName;
      if ((ctx.sourceDefs.count(samePath) == 0 &&
           ctx.helperOverloads.count(samePath) == 0) ||
          ctx.templateDefs.count(samePath) > 0) {
        return std::string{};
      }
      if (!resolvesExperimentalSoaVectorReceiver(mapHelperReceiverExpr(expr))) {
        return std::string{};
      }
      return samePath;
    };
    if (const std::string samePathHelper =
            preferredConcreteSamePathSoaHelperPath(resolvedPath);
        !samePathHelper.empty()) {
      resolvedPath = samePathHelper;
      expr.name = samePathHelper;
      expr.namespacePrefix.clear();
      expr.templateArgs.clear();
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
    const std::string latePreferredBorrowedSoaPath =
        preferredBorrowedSoaWrapperPath(resolvedPath);
    if (expr.templateArgs.empty() &&
        !latePreferredBorrowedSoaPath.empty() &&
        resolvesBorrowedExperimentalSoaVectorReceiver(resolvedReceiverExpr) &&
        (ctx.sourceDefs.count(latePreferredBorrowedSoaPath) > 0 ||
         ctx.helperOverloads.count(latePreferredBorrowedSoaPath) > 0 ||
         ctx.templateDefs.count(latePreferredBorrowedSoaPath) > 0)) {
      resolvedPath = latePreferredBorrowedSoaPath;
      expr.name = latePreferredBorrowedSoaPath;
      expr.namespacePrefix.clear();
    }
    auto publicSoaMutatorBasePath = [](std::string path) {
      if (const size_t specializationSuffix = path.find("__");
          specializationSuffix != std::string::npos) {
        path.erase(specializationSuffix);
      }
      if (path == publicSoaHelperTargetPath("push") ||
          path == publicSoaHelperTargetPath("reserve")) {
        return path;
      }
      return std::string{};
    };
    if (const std::string publicSoaMutatorPath =
            publicSoaMutatorBasePath(resolvedPath);
        !publicSoaMutatorPath.empty() &&
        resolvedReceiverFamily == "vector") {
      const std::string helperName = publicSoaMutatorPath.substr(
          templateMonomorphPublicSoaHelperPrefix().size());
      const std::string vectorPath =
          canonicalVectorCompatibilityHelperPathOrFallback(helperName);
      if (ctx.sourceDefs.count(vectorPath) > 0) {
        resolvedPath = vectorPath;
        expr.name = vectorPath;
        expr.namespacePrefix.clear();
      }
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
          if (defIt != ctx.sourceDefs.end() &&
              definitionAllowsEmptyTypePackSpecialization(defIt->second)) {
            allConcrete = true;
          } else if (shouldDeferStdlibCollectionHelperTemplateRewrite(resolvedPath)) {
            return true;
          } else {
            error = "template arguments required for " + helperOverloadDisplayPath(resolvedPath, ctx);
            return false;
          }
        }
      if (allConcrete) {
        std::string specializedPath;
        if (!instantiateTemplate(resolvedPath,
                                 expr.templateArgs,
                                 matchingTemplateArgumentDetails(expr.templateArgs,
                                                                 expr.templateArgDetails),
                                 ctx,
                                 error,
                                 specializedPath)) {
          return false;
        }
        expr.name = specializedPath;
        expr.templateArgs.clear();
        expr.templateArgDetails.clear();
      }
    } else if (isKnownDef && !expr.templateArgs.empty()) {
      const Expr *templateCarryReceiverExpr = mapHelperReceiverExpr(expr);
      const std::string templateCarryReceiverFamily =
          inferCollectionReceiverFamily(templateCarryReceiverExpr);
      if (isSyntheticSamePathSoaHelperTemplateCarryPath(resolvedPath) &&
          ctx.templateDefs.count(resolvedPath) == 0 &&
          templateCarryReceiverExpr != nullptr &&
          ((templateCarryReceiverExpr->kind == Expr::Kind::Call &&
            !templateCarryReceiverExpr->isBinding) ||
           templateCarryReceiverFamily == "vector")) {
        expr.templateArgs.clear();
      } else {
      const std::string resolvedGetPath =
          canonicalizeLegacySoaGetHelperPath(resolvedPath);
      const std::string resolvedRefPath =
          canonicalizeLegacySoaRefHelperPath(resolvedPath);
      std::string soaAccessHelper;
      if (isLegacyOrCanonicalSoaHelperPath(resolvedGetPath, "get")) {
        soaAccessHelper = "get";
      } else if (isLegacyOrCanonicalSoaHelperPath(resolvedGetPath, "get_ref")) {
        soaAccessHelper = "get_ref";
      } else if (isLegacyOrCanonicalSoaHelperPath(resolvedRefPath, "ref")) {
        soaAccessHelper = "ref";
      } else if (isLegacyOrCanonicalSoaHelperPath(resolvedRefPath, "ref_ref")) {
        soaAccessHelper = "ref_ref";
      }
      if (!soaAccessHelper.empty()) {
        if (expr.args.size() != 2) {
          error = "argument count mismatch for builtin " + soaAccessHelper;
          return false;
        }
        const Expr &indexExpr = expr.args[1];
        if (indexExpr.kind == Expr::Kind::BoolLiteral ||
            indexExpr.kind == Expr::Kind::FloatLiteral ||
            indexExpr.kind == Expr::Kind::StringLiteral) {
          error = soaAccessHelper + " requires integer index";
          return false;
        }
      }
      if (soaAccessHelper.empty() &&
          (isExperimentalSoaGetLikeHelperPath(resolvedPath) ||
           isExperimentalSoaRefLikeHelperPath(resolvedPath))) {
        if (resolvedPath.find("get_ref") != std::string::npos) {
          soaAccessHelper = "get_ref";
        } else if (resolvedPath.find("get") != std::string::npos) {
          soaAccessHelper = "get";
        } else if (resolvedPath.find("ref_ref") != std::string::npos) {
          soaAccessHelper = "ref_ref";
        } else if (resolvedPath.find("ref") != std::string::npos) {
          soaAccessHelper = "ref";
        }
        if (!soaAccessHelper.empty()) {
          if (expr.args.size() != 2) {
            error = "argument count mismatch for builtin " + soaAccessHelper;
            return false;
          }
          const Expr &indexExpr = expr.args[1];
          if (indexExpr.kind == Expr::Kind::BoolLiteral ||
              indexExpr.kind == Expr::Kind::FloatLiteral ||
              indexExpr.kind == Expr::Kind::StringLiteral) {
            error = soaAccessHelper + " requires integer index";
            return false;
          }
        }
      }
      error = "template arguments are only supported on templated definitions: " +
              helperOverloadDisplayPath(resolvedPath, ctx);
      return false;
      }
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
      const bool preserveExplicitCompatibilityTemplateMethodPath =
          !expr.templateArgs.empty() &&
          shouldPreserveCompatibilityTemplatePath(methodPath, ctx);
      if (preserveExplicitCompatibilityTemplateMethodPath) {
        error = "template arguments are only supported on templated definitions: " +
                helperOverloadDisplayPath(methodPath, ctx);
        return false;
      }
      const std::string preferredCollectionHelperMethodPath =
          preserveExplicitCompatibilityTemplateMethodPath
              ? methodPath
              : preferCanonicalStdlibCollectionHelperPath(methodPath);
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
              mapHelperReceiverExpr(expr));
      if (shouldRewriteCanonicalSoaMethodToExperimental &&
          !experimentalSoaVectorMethodPath.empty()) {
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
          isExperimentalSoaVectorPublicHelperPath(methodPath) &&
          resolvesExperimentalSoaVectorReceiver(mapHelperReceiverExpr(expr))) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalSoaVectorReceiverTemplateArgs(mapHelperReceiverExpr(expr), receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
          allConcrete = true;
        }
      }
      if (expr.templateArgs.empty() &&
          methodPath.rfind("/std/collections/map/", 0) == 0 &&
          hasVisibleStdCollectionsImportForPath(ctx, methodPath)) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalMapValueReceiverTemplateArgs(
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
          isCanonicalSoaBorrowedWrapperHelper(methodPath) &&
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
      if (expr.templateArgs.empty() && ctx.templateDefs.count(methodPath) > 0) {
        auto defIt = ctx.sourceDefs.find(methodPath);
        const Expr *receiverExpr = mapHelperReceiverExpr(expr);
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
              return false;
            }
          }
        }
        if (expr.templateArgs.empty()) {
          if (defIt != ctx.sourceDefs.end() &&
              definitionAllowsEmptyTypePackSpecialization(defIt->second)) {
            allConcrete = true;
          } else if (shouldDeferStdlibCollectionHelperTemplateRewrite(methodPath)) {
            return true;
          } else {
            error = "template arguments required for " + helperOverloadDisplayPath(methodPath, ctx);
            return false;
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
            return false;
          }
          expr.name = specializedPath;
          expr.templateArgs.clear();
          expr.templateArgDetails.clear();
          expr.isMethodCall = false;
        }
      } else if (isKnownDef && !expr.templateArgs.empty()) {
        const Expr *templateCarryReceiverExpr = mapHelperReceiverExpr(expr);
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
        return false;
        }
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
  return true;
}
