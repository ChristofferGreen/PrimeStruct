#pragma once

bool inferCallBindingTypeForMonomorph(const Expr &initializer,
                                      const std::vector<ParameterInfo> &params,
                                      const LocalTypeMap &locals,
                                      bool allowMathBare,
                                      Context &ctx,
                                      BindingInfo &infoOut,
                                      bool &handledOut) {
  handledOut = false;
  if (initializer.kind != Expr::Kind::Call) {
    return false;
  }

  if (initializer.isMethodCall && initializer.name == "ok" && initializer.args.size() == 2 &&
      initializer.templateArgs.empty() && !initializer.hasBodyArguments && initializer.bodyArguments.empty()) {
    const Expr &receiver = initializer.args.front();
    if (receiver.kind == Expr::Kind::Name && normalizeBindingTypeName(receiver.name) == "Result") {
      BindingInfo payloadInfo;
      if (!inferBindingTypeForMonomorph(initializer.args.back(), params, locals, allowMathBare, ctx, payloadInfo)) {
        handledOut = true;
        return false;
      }
      const std::string payloadTypeText = bindingTypeToString(payloadInfo);
      if (payloadTypeText.empty()) {
        handledOut = true;
        return false;
      }
      infoOut.typeName = "Result";
      infoOut.typeTemplateArg = payloadTypeText + ", _";
      return true;
    }
  }
  if (isIfCall(initializer) && initializer.args.size() == 3) {
    BindingInfo thenInfo;
    BindingInfo elseInfo;
    if (!inferBindingTypeForMonomorph(initializer.args[1], params, locals, allowMathBare, ctx, thenInfo) ||
        !inferBindingTypeForMonomorph(initializer.args[2], params, locals, allowMathBare, ctx, elseInfo)) {
      handledOut = true;
      return false;
    }
    if (bindingTypeToString(thenInfo) != bindingTypeToString(elseInfo)) {
      handledOut = true;
      return false;
    }
    infoOut = thenInfo;
    return true;
  }

  std::string resolved;
  if (initializer.isMethodCall) {
    if (!resolveMethodCallTemplateTarget(initializer, locals, ctx, resolved)) {
      resolved.clear();
    }
  } else {
    resolved = resolveCalleePath(initializer, initializer.namespacePrefix, ctx);
  }

  if (!initializer.isMethodCall && initializer.templateArgs.size() == 2) {
    const std::string experimentalPath = experimentalMapConstructorInferencePath(resolved);
    if (!experimentalPath.empty() && ctx.sourceDefs.count(experimentalPath) > 0) {
      handledOut = true;
      infoOut.typeName = "/std/collections/experimental_map/Map";
      infoOut.typeTemplateArg = joinTemplateArgs(initializer.templateArgs);
      return true;
    }
    if (isExperimentalMapConstructorHelperPath(resolved)) {
      handledOut = true;
      infoOut.typeName = "/std/collections/experimental_map/Map";
      infoOut.typeTemplateArg = joinTemplateArgs(initializer.templateArgs);
      return true;
    }
  }
  if (!initializer.isMethodCall && initializer.templateArgs.empty()) {
    const std::string experimentalPath = experimentalMapConstructorInferencePath(resolved);
    if (!experimentalPath.empty() && ctx.sourceDefs.count(experimentalPath) > 0) {
      auto defIt = ctx.sourceDefs.find(resolved);
      if (defIt != ctx.sourceDefs.end()) {
        std::vector<std::string> inferredArgs;
        std::string inferError;
        if (inferImplicitTemplateArgs(defIt->second,
                                      initializer,
                                      locals,
                                      params,
                                      SubstMap{},
                                      {},
                                      initializer.namespacePrefix,
                                      ctx,
                                      allowMathBare,
                                      inferredArgs,
                                      inferError) &&
            inferredArgs.size() == 2) {
          infoOut.typeName = "/std/collections/experimental_map/Map";
          infoOut.typeTemplateArg = joinTemplateArgs(inferredArgs);
          return true;
        }
        if (!inferError.empty()) {
          handledOut = true;
          return false;
        }
        defIt = ctx.sourceDefs.find(resolved);
        if (defIt == ctx.sourceDefs.end()) {
          handledOut = true;
          return false;
        }
        for (const auto &transform : defIt->second.transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          std::string keyType;
          std::string valueType;
          if (extractMapKeyValueTypesFromTypeText(transform.templateArgs.front(), keyType, valueType)) {
            infoOut.typeName = "/std/collections/experimental_map/Map";
            infoOut.typeTemplateArg = joinTemplateArgs({keyType, valueType});
            return true;
          }
        }
      }
    }
    if (isExperimentalMapConstructorHelperPath(resolved)) {
      auto defIt = ctx.sourceDefs.find(resolved);
      if (defIt != ctx.sourceDefs.end()) {
        std::vector<std::string> inferredArgs;
        std::string inferError;
        if (inferImplicitTemplateArgs(defIt->second,
                                      initializer,
                                      locals,
                                      params,
                                      SubstMap{},
                                      {},
                                      initializer.namespacePrefix,
                                      ctx,
                                      allowMathBare,
                                      inferredArgs,
                                      inferError) &&
            inferredArgs.size() == 2) {
          infoOut.typeName = "/std/collections/experimental_map/Map";
          infoOut.typeTemplateArg = joinTemplateArgs(inferredArgs);
          return true;
        }
        if (!inferError.empty()) {
          handledOut = true;
          return false;
        }
        defIt = ctx.sourceDefs.find(resolved);
        if (defIt == ctx.sourceDefs.end()) {
          handledOut = true;
          return false;
        }
        for (const auto &transform : defIt->second.transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          std::string keyType;
          std::string valueType;
          if (resolvesExperimentalMapValueTypeText(transform.templateArgs.front(),
                                                   SubstMap{},
                                                   {},
                                                   defIt->second.namespacePrefix,
                                                   ctx) &&
              extractMapKeyValueTypesFromTypeText(transform.templateArgs.front(), keyType, valueType)) {
            infoOut.typeName = "/std/collections/experimental_map/Map";
            infoOut.typeTemplateArg = joinTemplateArgs({keyType, valueType});
            return true;
          }
        }
      }
    }
  }
  if (resolved.empty()) {
    return false;
  }

  auto defIt = ctx.sourceDefs.find(resolved);
  if (defIt == ctx.sourceDefs.end()) {
    return false;
  }

  std::vector<std::string> effectiveTemplateArgs = initializer.templateArgs;
  if (effectiveTemplateArgs.empty() && !defIt->second.templateArgs.empty()) {
    std::string inferError;
    if (!inferImplicitTemplateArgs(defIt->second,
                                   initializer,
                                   locals,
                                   params,
                                   SubstMap{},
                                   {},
                                   initializer.namespacePrefix,
                                   ctx,
                                   allowMathBare,
                                   effectiveTemplateArgs,
                                   inferError)) {
      if (!inferError.empty()) {
        handledOut = true;
        return false;
      }
      effectiveTemplateArgs.clear();
    }
  }

  std::unordered_set<std::string> allowedParams(defIt->second.templateArgs.begin(), defIt->second.templateArgs.end());
  SubstMap returnTypeMapping;
  if (effectiveTemplateArgs.size() == defIt->second.templateArgs.size()) {
    returnTypeMapping.reserve(effectiveTemplateArgs.size());
    for (size_t i = 0; i < effectiveTemplateArgs.size(); ++i) {
      returnTypeMapping.emplace(defIt->second.templateArgs[i], effectiveTemplateArgs[i]);
    }
  }
  for (const auto &transform : defIt->second.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &returnType = transform.templateArgs.front();
    if (returnType == "auto") {
      break;
    }
    std::string resolvedError;
    ResolvedType resolvedReturnType =
        resolveTypeString(returnType, returnTypeMapping, allowedParams, defIt->second.namespacePrefix, ctx,
                          resolvedError);
    if (!resolvedError.empty() || !resolvedReturnType.concrete || resolvedReturnType.text.empty()) {
      break;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(resolvedReturnType.text, base, argText) && !base.empty()) {
      infoOut.typeName = base;
      infoOut.typeTemplateArg = argText;
    } else {
      infoOut.typeName = resolvedReturnType.text;
      infoOut.typeTemplateArg.clear();
    }
    return true;
  }
  return inferDefinitionReturnBindingForTemplatedFallback(defIt->second, allowMathBare, ctx, infoOut);
}
