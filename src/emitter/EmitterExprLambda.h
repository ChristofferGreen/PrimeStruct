std::string Emitter::emitExpr(const Expr &expr,
                              const std::unordered_map<std::string, std::string> &nameMap,
                              const std::unordered_map<std::string, std::vector<Expr>> &paramMap,
                              const std::unordered_map<std::string, const Definition *> &defMap,
                              const std::unordered_map<std::string, std::string> &structTypeMap,
                              const std::unordered_map<std::string, std::string> &importAliases,
                              const std::unordered_map<std::string, BindingInfo> &localTypes,
                              const std::unordered_map<std::string, ReturnKind> &returnKinds,
                              const std::unordered_map<std::string, ResultInfo> &resultInfos,
                              const std::unordered_map<std::string, std::string> &returnStructs,
                              bool allowMathBare) const {
  std::function<bool(const Expr &)> isPointerExpr;
  isPointerExpr = [&](const Expr &candidate) -> bool {
    if (candidate.kind == Expr::Kind::Name) {
      auto it = localTypes.find(candidate.name);
      return it != localTypes.end() && it->second.typeName == "Pointer";
    }
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    if (isSimpleCallName(candidate, "location")) {
      return true;
    }
    char op = '\0';
    if (getBuiltinOperator(candidate, op) && candidate.args.size() == 2 && (op == '+' || op == '-')) {
      return isPointerExpr(candidate.args[0]) && !isPointerExpr(candidate.args[1]);
    }
    return false;
  };
  auto bindingTypeText = [](const BindingInfo &binding) {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  auto populateBindingFromTypeText = [](const std::string &typeText, BindingInfo &bindingOut) {
    bindingOut = BindingInfo{};
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(typeText, base, arg)) {
      bindingOut.typeName = normalizeBindingTypeName(base);
      bindingOut.typeTemplateArg = arg;
    } else {
      bindingOut.typeName = normalizeBindingTypeName(typeText);
    }
    return !bindingOut.typeName.empty();
  };
  auto populateResultInfoFromBinding = [&](const BindingInfo &binding, ResultInfo &out) {
    out = ResultInfo{};
    if (!isResultBindingTypeName(binding.typeName)) {
      return false;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(binding.typeTemplateArg, args)) {
      return false;
    }
    if (args.size() == 1) {
      out.isResult = true;
      out.hasValue = false;
      out.errorType = args.front();
      return true;
    }
    if (args.size() == 2) {
      out.isResult = true;
      out.hasValue = true;
      out.valueType = args.front();
      out.errorType = args.back();
      return true;
    }
    return false;
  };
  std::function<bool(const Expr &, const std::unordered_map<std::string, BindingInfo> &, BindingInfo &)>
      resolveExprBindingInfoWithTypes;
  resolveExprBindingInfoWithTypes =
      [&](const Expr &candidate, const std::unordered_map<std::string, BindingInfo> &currentTypes,
          BindingInfo &bindingOut) -> bool {
    if (candidate.kind == Expr::Kind::Name) {
      auto it = currentTypes.find(candidate.name);
      if (it != currentTypes.end()) {
        bindingOut = it->second;
        return true;
      }
      return false;
    }
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    if (isSimpleCallName(candidate, "dereference") && candidate.args.size() == 1) {
      BindingInfo sourceBinding;
      if (!resolveExprBindingInfoWithTypes(candidate.args.front(), currentTypes, sourceBinding)) {
        return false;
      }
      const std::string normalizedSourceType = normalizeBindingTypeName(sourceBinding.typeName);
      if ((normalizedSourceType == "Reference" || normalizedSourceType == "Pointer") &&
          !sourceBinding.typeTemplateArg.empty()) {
        return populateBindingFromTypeText(sourceBinding.typeTemplateArg, bindingOut);
      }
      return false;
    }
    std::string accessName;
    if (candidate.args.size() == 2 && getBuiltinArrayAccessNameLocal(candidate, accessName) &&
        (accessName == "at" || accessName == "at_unsafe")) {
      for (const Expr &argExpr : candidate.args) {
        std::string elementType;
        if (inferCollectionElementTypeNameFromExpr(argExpr, currentTypes, {}, elementType)) {
          return populateBindingFromTypeText(elementType, bindingOut);
        }
      }
    }
    return false;
  };
  auto resolveDirectResultOkInfo = [&](const Expr &candidate,
                                       const std::unordered_map<std::string, BindingInfo> &currentTypes,
                                       const std::string &preferredErrorType,
                                       ResultInfo &out) -> bool {
    out = ResultInfo{};
    if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name != "ok" ||
        !candidate.templateArgs.empty() || candidate.hasBodyArguments || !candidate.bodyArguments.empty() ||
        candidate.args.empty()) {
      return false;
    }
    const Expr &receiver = candidate.args.front();
    if (receiver.kind != Expr::Kind::Name || normalizeBindingTypeName(receiver.name) != "Result") {
      return false;
    }
    out.isResult = true;
    out.errorType = preferredErrorType.empty() ? "_" : preferredErrorType;
    if (candidate.args.size() == 1) {
      out.hasValue = false;
      return true;
    }
    if (candidate.args.size() != 2) {
      return false;
    }
    const Expr &valueExpr = candidate.args.back();
    if (valueExpr.kind == Expr::Kind::Name) {
      auto it = currentTypes.find(valueExpr.name);
      if (it != currentTypes.end()) {
        out.hasValue = true;
        out.valueType = bindingTypeText(it->second);
        return !out.valueType.empty();
      }
    }
    ReturnKind valueKind = inferPrimitiveReturnKind(valueExpr, currentTypes, returnKinds, allowMathBare);
    out.valueType = typeNameForReturnKind(valueKind);
    out.hasValue = !out.valueType.empty();
    return out.hasValue;
  };
  auto splitCaptureTokens = [](const std::string &capture) {
    std::vector<std::string> tokens;
    std::istringstream stream(capture);
    std::string token;
    while (stream >> token) {
      tokens.push_back(token);
    }
    return tokens;
  };
  auto seedLambdaTypes = [&](const Expr &lambdaExpr,
                             const std::unordered_map<std::string, BindingInfo> &outerTypes,
                             std::unordered_map<std::string, BindingInfo> &lambdaTypesOut) -> bool {
    lambdaTypesOut.clear();
    bool captureAll = false;
    std::vector<std::string> explicitCaptureNames;
    explicitCaptureNames.reserve(lambdaExpr.lambdaCaptures.size());
    for (const auto &capture : lambdaExpr.lambdaCaptures) {
      std::vector<std::string> tokens = splitCaptureTokens(capture);
      if (tokens.empty()) {
        return false;
      }
      if (tokens.size() == 1 && (tokens[0] == "=" || tokens[0] == "&")) {
        captureAll = true;
        continue;
      }
      explicitCaptureNames.push_back(tokens.back());
    }
    if (captureAll) {
      for (const auto &entry : outerTypes) {
        lambdaTypesOut.emplace(entry.first, entry.second);
      }
    } else {
      for (const auto &captureName : explicitCaptureNames) {
        auto it = outerTypes.find(captureName);
        if (it != outerTypes.end()) {
          lambdaTypesOut.emplace(captureName, it->second);
        }
      }
    }
    for (const auto &param : lambdaExpr.args) {
      if (!param.isBinding) {
        return false;
      }
      BindingInfo binding = getBindingInfo(param);
      if (!hasExplicitBindingTypeTransform(param) && param.args.size() == 1) {
        ReturnKind initKind = inferPrimitiveReturnKind(param.args.front(), lambdaTypesOut, returnKinds, allowMathBare);
        std::string inferred = typeNameForReturnKind(initKind);
        if (!inferred.empty()) {
          binding.typeName = inferred;
          binding.typeTemplateArg.clear();
        }
      }
      lambdaTypesOut[param.name] = std::move(binding);
    }
    return true;
  };
  auto inferLambdaValueTypeText = [&](const Expr &lambdaExpr,
                                      const std::unordered_map<std::string, BindingInfo> &outerTypes,
                                      std::string &typeTextOut) -> bool {
    typeTextOut.clear();
    if (!lambdaExpr.isLambda || (!lambdaExpr.hasBodyArguments && lambdaExpr.bodyArguments.empty())) {
      return false;
    }
    std::unordered_map<std::string, BindingInfo> activeTypes;
    if (!seedLambdaTypes(lambdaExpr, outerTypes, activeTypes)) {
      return false;
    }
    const Expr *valueExpr = nullptr;
    for (const auto &stmt : lambdaExpr.bodyArguments) {
      if (stmt.isBinding) {
        BindingInfo binding = getBindingInfo(stmt);
        if (!hasExplicitBindingTypeTransform(stmt) && stmt.args.size() == 1) {
          ReturnKind initKind = inferPrimitiveReturnKind(stmt.args.front(), activeTypes, returnKinds, allowMathBare);
          std::string inferred = typeNameForReturnKind(initKind);
          if (!inferred.empty()) {
            binding.typeName = inferred;
            binding.typeTemplateArg.clear();
          }
        }
        activeTypes[stmt.name] = std::move(binding);
        continue;
      }
      if (isSimpleCallName(stmt, "return") && stmt.args.size() == 1) {
        valueExpr = &stmt.args.front();
        break;
      }
      valueExpr = &stmt;
    }
    if (valueExpr == nullptr) {
      return false;
    }
    if (valueExpr->kind == Expr::Kind::Name) {
      auto it = activeTypes.find(valueExpr->name);
      if (it != activeTypes.end()) {
        typeTextOut = bindingTypeText(it->second);
        return !typeTextOut.empty();
      }
    }
    ReturnKind kind = inferPrimitiveReturnKind(*valueExpr, activeTypes, returnKinds, allowMathBare);
    typeTextOut = typeNameForReturnKind(kind);
    return !typeTextOut.empty();
  };
  std::function<bool(const Expr &, const std::unordered_map<std::string, BindingInfo> &, ResultInfo &)>
      resolveResultExprInfoWithTypes;
  auto inferLambdaResultInfo = [&](const Expr &lambdaExpr,
                                   const std::unordered_map<std::string, BindingInfo> &outerTypes,
                                   const std::string &preferredErrorType,
                                   ResultInfo &resultOut) -> bool {
    resultOut = ResultInfo{};
    if (!lambdaExpr.isLambda || (!lambdaExpr.hasBodyArguments && lambdaExpr.bodyArguments.empty())) {
      return false;
    }
    std::unordered_map<std::string, BindingInfo> activeTypes;
    if (!seedLambdaTypes(lambdaExpr, outerTypes, activeTypes)) {
      return false;
    }
    const Expr *valueExpr = nullptr;
    for (const auto &stmt : lambdaExpr.bodyArguments) {
      if (stmt.isBinding) {
        BindingInfo binding = getBindingInfo(stmt);
        if (!hasExplicitBindingTypeTransform(stmt) && stmt.args.size() == 1) {
          ReturnKind initKind = inferPrimitiveReturnKind(stmt.args.front(), activeTypes, returnKinds, allowMathBare);
          std::string inferred = typeNameForReturnKind(initKind);
          if (!inferred.empty()) {
            binding.typeName = inferred;
            binding.typeTemplateArg.clear();
          }
        }
        activeTypes[stmt.name] = std::move(binding);
        continue;
      }
      if (isSimpleCallName(stmt, "return") && stmt.args.size() == 1) {
        valueExpr = &stmt.args.front();
        break;
      }
      valueExpr = &stmt;
    }
    if (valueExpr == nullptr) {
      return false;
    }
    if (resolveDirectResultOkInfo(*valueExpr, activeTypes, preferredErrorType, resultOut)) {
      return true;
    }
    if (!resolveResultExprInfoWithTypes(*valueExpr, activeTypes, resultOut) || !resultOut.isResult) {
      return false;
    }
    if (!preferredErrorType.empty() && resultOut.errorType == "_") {
      resultOut.errorType = preferredErrorType;
    }
    return true;
  };
  resolveResultExprInfoWithTypes =
      [&](const Expr &candidate, const std::unordered_map<std::string, BindingInfo> &currentTypes, ResultInfo &out) -> bool {
        out = ResultInfo{};
        BindingInfo binding;
        if (resolveExprBindingInfoWithTypes(candidate, currentTypes, binding) &&
            populateResultInfoFromBinding(binding, out)) {
          return true;
        }
        if (candidate.kind != Expr::Kind::Call) {
          return false;
        }
        if (resolveDirectResultOkInfo(candidate, currentTypes, "", out)) {
          return true;
        }
        if (!candidate.isMethodCall && isSimpleCallName(candidate, "File")) {
          out.isResult = true;
          out.hasValue = true;
          out.valueType = "File";
          out.errorType = "FileError";
          return true;
        }
        std::string resolved = resolveExprPath(candidate);
        if (candidate.isMethodCall) {
          std::string methodPath;
          if (resolveMethodCallPath(
                  candidate, defMap, currentTypes, importAliases, structTypeMap, returnKinds, returnStructs, methodPath)) {
            resolved = methodPath;
          }
          if (resolved.rfind("/file/", 0) == 0) {
            out.isResult = true;
            out.hasValue = false;
            out.errorType = "FileError";
            return true;
          }
          if (!candidate.args.empty() &&
              candidate.args.front().kind == Expr::Kind::Name &&
              normalizeBindingTypeName(candidate.args.front().name) == "Result") {
            if (candidate.name == "map" && candidate.args.size() == 3) {
              ResultInfo argInfo;
              if (!resolveResultExprInfoWithTypes(candidate.args[1], currentTypes, argInfo) || !argInfo.isResult ||
                  !argInfo.hasValue) {
                return false;
              }
              std::string mappedType;
              if (!inferLambdaValueTypeText(candidate.args[2], currentTypes, mappedType) || mappedType.empty()) {
                return false;
              }
              out.isResult = true;
              out.hasValue = true;
              out.valueType = std::move(mappedType);
              out.errorType = argInfo.errorType;
              return true;
            }
            if (candidate.name == "and_then" && candidate.args.size() == 3) {
              ResultInfo argInfo;
              ResultInfo nextInfo;
              if (!resolveResultExprInfoWithTypes(candidate.args[1], currentTypes, argInfo) || !argInfo.isResult ||
                  !argInfo.hasValue ||
                  !inferLambdaResultInfo(candidate.args[2], currentTypes, argInfo.errorType, nextInfo) ||
                  !nextInfo.isResult) {
                return false;
              }
              if (!nextInfo.errorType.empty() &&
                  !argInfo.errorType.empty() &&
                  normalizeBindingTypeName(nextInfo.errorType) != normalizeBindingTypeName(argInfo.errorType)) {
                return false;
              }
              out = std::move(nextInfo);
              return true;
            }
            if (candidate.name == "map2" && candidate.args.size() == 4) {
              ResultInfo leftInfo;
              ResultInfo rightInfo;
              if (!resolveResultExprInfoWithTypes(candidate.args[1], currentTypes, leftInfo) || !leftInfo.isResult ||
                  !leftInfo.hasValue ||
                  !resolveResultExprInfoWithTypes(candidate.args[2], currentTypes, rightInfo) || !rightInfo.isResult ||
                  !rightInfo.hasValue ||
                  normalizeBindingTypeName(leftInfo.errorType) != normalizeBindingTypeName(rightInfo.errorType)) {
                return false;
              }
              std::string mappedType;
              if (!inferLambdaValueTypeText(candidate.args[3], currentTypes, mappedType) || mappedType.empty()) {
                return false;
              }
              out.isResult = true;
              out.hasValue = true;
              out.valueType = std::move(mappedType);
              out.errorType = leftInfo.errorType;
              return true;
            }
          }
        }
        auto it = resultInfos.find(resolved);
        if (it != resultInfos.end() && it->second.isResult) {
          out = it->second;
          return true;
        }
        if (!candidate.isMethodCall && !candidate.name.empty() && candidate.name[0] != '/' &&
            candidate.name.find('/') == std::string::npos) {
          auto importIt = importAliases.find(candidate.name);
          if (importIt != importAliases.end()) {
            auto aliasIt = resultInfos.find(importIt->second);
            if (aliasIt != resultInfos.end() && aliasIt->second.isResult) {
              out = aliasIt->second;
              return true;
            }
          }
        }
        return false;
      };
  auto resolveResultExprInfo = [&](const Expr &candidate, ResultInfo &out) -> bool {
    return resolveResultExprInfoWithTypes(candidate, localTypes, out);
  };
#include "EmitterExprLambdaBody.h"
