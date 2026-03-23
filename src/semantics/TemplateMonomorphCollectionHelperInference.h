#pragma once

bool inferStdlibCollectionHelperTemplateArgs(const Definition &def,
                                             const Expr &callExpr,
                                             const LocalTypeMap &locals,
                                             const std::vector<ParameterInfo> &params,
                                             const SubstMap &mapping,
                                             const std::unordered_set<std::string> &allowedParams,
                                             const std::string &namespacePrefix,
                                             Context &ctx,
                                             bool allowMathBare,
                                             std::vector<std::string> &outArgs) {
  enum class HelperFamily { None, Vector, Map };
  HelperFamily family = HelperFamily::None;
  if (def.fullPath.rfind("/std/collections/vector/", 0) == 0) {
    family = HelperFamily::Vector;
  } else if (def.fullPath.rfind("/std/collections/experimental_vector/", 0) == 0) {
    family = HelperFamily::Vector;
  } else if (def.fullPath.rfind("/std/collections/map/", 0) == 0) {
    family = HelperFamily::Map;
  } else if (def.fullPath.rfind("/std/collections/experimental_map/", 0) == 0) {
    family = HelperFamily::Map;
  } else if (def.fullPath.rfind("/map/", 0) == 0) {
    family = HelperFamily::Map;
  } else {
    return false;
  }
  if (def.parameters.empty()) {
    return false;
  }
  const size_t helperSlash = def.fullPath.find_last_of('/');
  const std::string helperName = helperSlash == std::string::npos ? def.fullPath : def.fullPath.substr(helperSlash + 1);
  auto splitInlineTemplateType = [](BindingInfo &info) {
    if (!info.typeTemplateArg.empty()) {
      return;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(info.typeName, base, argText) && !base.empty()) {
      info.typeName = base;
      info.typeTemplateArg = argText;
    }
  };
  BindingInfo receiverParamInfo;
  if (!extractExplicitBindingType(def.parameters.front(), receiverParamInfo)) {
    return false;
  }
  splitInlineTemplateType(receiverParamInfo);
  const std::string normalizedReceiverParamType = normalizeCollectionReceiverTypeName(receiverParamInfo.typeName);
  size_t expectedTemplateArgCount = 0;
  if (family == HelperFamily::Vector) {
    if (def.templateArgs.size() != 1) {
      return false;
    }
    if (normalizedReceiverParamType != "vector" && normalizedReceiverParamType != "soa_vector") {
      return false;
    }
    expectedTemplateArgCount = 1;
  } else if (family == HelperFamily::Map) {
    if (def.templateArgs.size() != 2) {
      return false;
    }
    if (normalizedReceiverParamType != "map") {
      return false;
    }
    expectedTemplateArgCount = 2;
  } else {
    return false;
  }
  std::vector<std::string> receiverParamTemplateArgs;
  if (!splitTopLevelTemplateArgs(receiverParamInfo.typeTemplateArg, receiverParamTemplateArgs) ||
      receiverParamTemplateArgs.size() != expectedTemplateArgCount) {
    return false;
  }
  for (size_t i = 0; i < expectedTemplateArgCount; ++i) {
    if (trimWhitespace(receiverParamTemplateArgs[i]) != def.templateArgs[i]) {
      return false;
    }
  }

  std::vector<ParameterInfo> callParams;
  callParams.reserve(def.parameters.size());
  for (const auto &paramExpr : def.parameters) {
    ParameterInfo paramInfo;
    paramInfo.name = paramExpr.name;
    if (paramExpr.args.size() == 1) {
      paramInfo.defaultExpr = &paramExpr.args.front();
    }
    callParams.push_back(std::move(paramInfo));
  }
  std::vector<const Expr *> orderedArgs;
  std::string orderError;
  if (!buildOrderedArguments(callParams, callExpr.args, callExpr.argNames, orderedArgs, orderError)) {
    return false;
  }
  if (orderedArgs.empty() || orderedArgs.front() == nullptr) {
    return false;
  }
  auto tryInferReceiverTemplateArgs = [&](const Expr &receiverExpr) -> bool {
    auto inferFromTypeText = [&](std::string receiverTypeText) -> bool {
      if (receiverTypeText.empty()) {
        return false;
      }
      while (true) {
        std::string base;
        std::string argText;
        if (!splitTemplateTypeName(receiverTypeText, base, argText) || base.empty()) {
          const std::string normalizedType = normalizeCollectionReceiverTypeName(receiverTypeText);
          if (family == HelperFamily::Vector && normalizedType == "string" &&
              (helperName == "at" || helperName == "at_unsafe")) {
            outArgs = {"i32"};
            return true;
          }
          return false;
        }
        const std::string normalizedBase = normalizeCollectionReceiverTypeName(base);
        std::vector<std::string> receiverArgs;
        if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
          if (!splitTopLevelTemplateArgs(argText, receiverArgs) || receiverArgs.size() != 1) {
            return false;
          }
          receiverTypeText = receiverArgs.front();
          continue;
        }
        if (!splitTopLevelTemplateArgs(argText, receiverArgs)) {
          return false;
        }
        if (family == HelperFamily::Vector &&
            (normalizedBase == "vector" || normalizedBase == "soa_vector") &&
            receiverArgs.size() == 1) {
          outArgs = {receiverArgs.front()};
          return true;
        }
        if (family == HelperFamily::Map && normalizedBase == "map" && receiverArgs.size() == 2) {
          outArgs = receiverArgs;
          return true;
        }
        return false;
      }
    };

    BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
        inferFromTypeText(bindingTypeToString(receiverInfo))) {
      return true;
    }
    return inferFromTypeText(
        inferExprTypeTextForTemplatedVectorFallback(receiverExpr, locals, namespacePrefix, ctx, allowMathBare));
  };
  if (tryInferReceiverTemplateArgs(*orderedArgs.front())) {
    return true;
  }

  std::string receiverArgType;
  std::string receiverArgTemplateArg;
  BindingInfo receiverArgInfo;
  const bool inferredReceiverBinding =
      inferBindingTypeForMonomorph(*orderedArgs.front(), params, locals, allowMathBare, ctx, receiverArgInfo);
  if (inferredReceiverBinding) {
    splitInlineTemplateType(receiverArgInfo);
    receiverArgType = normalizeCollectionReceiverTypeName(receiverArgInfo.typeName);
    receiverArgTemplateArg = receiverArgInfo.typeTemplateArg;
  }
  if (!inferredReceiverBinding ||
      ((receiverArgType == "vector" || receiverArgType == "soa_vector" || receiverArgType == "map") &&
       receiverArgTemplateArg.empty())) {
    const std::string inferredReceiverTypeText =
        inferExprTypeTextForTemplatedVectorFallback(
            *orderedArgs.front(), locals, namespacePrefix, ctx, allowMathBare);
    if (inferredReceiverTypeText.empty()) {
      if (!inferredReceiverBinding) {
        return false;
      }
    } else {
      const std::string normalizedInferredReceiverType =
          normalizeCollectionReceiverTypeName(inferredReceiverTypeText);
      if (family == HelperFamily::Vector && normalizedInferredReceiverType == "string" &&
          (helperName == "at" || helperName == "at_unsafe")) {
        outArgs = {"i32"};
        return true;
      }
      std::string receiverBase;
      std::string receiverArgText;
      if (!splitTemplateTypeName(normalizedInferredReceiverType, receiverBase, receiverArgText) ||
          receiverBase.empty()) {
        return false;
      }
      receiverArgType = normalizeCollectionReceiverTypeName(receiverBase);
      receiverArgTemplateArg = receiverArgText;
    }
  }
  if ((receiverArgType == "Reference" || receiverArgType == "Pointer") && !receiverArgTemplateArg.empty()) {
    std::string pointeeBase;
    std::string pointeeArgText;
    if (splitTemplateTypeName(receiverArgTemplateArg, pointeeBase, pointeeArgText)) {
      receiverArgType = normalizeCollectionReceiverTypeName(pointeeBase);
      receiverArgTemplateArg = pointeeArgText;
    }
  }
  if (family == HelperFamily::Vector && receiverArgType == "string" &&
      (helperName == "at" || helperName == "at_unsafe")) {
    outArgs = {"i32"};
    return true;
  }
  if (receiverArgType != normalizedReceiverParamType) {
    return false;
  }

  std::vector<std::string> receiverArgTemplateArgs;
  if (!splitTopLevelTemplateArgs(receiverArgTemplateArg, receiverArgTemplateArgs) ||
      receiverArgTemplateArgs.size() != expectedTemplateArgCount) {
    return false;
  }
  outArgs.clear();
  outArgs.reserve(expectedTemplateArgCount);
  for (const auto &receiverArgTemplate : receiverArgTemplateArgs) {
    std::string resolvedError;
    ResolvedType resolvedArg =
        resolveTypeString(receiverArgTemplate, mapping, allowedParams, namespacePrefix, ctx, resolvedError);
    if (!resolvedError.empty() || !resolvedArg.concrete || resolvedArg.text.empty()) {
      return false;
    }
    outArgs.push_back(resolvedArg.text);
  }
  return true;
}
