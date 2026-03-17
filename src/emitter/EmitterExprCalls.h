  if (expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
      expr.args.front().name == "Result" && expr.name == "ok") {
    if (expr.args.size() == 1) {
      return "static_cast<uint32_t>(0)";
    }
    if (expr.args.size() == 2) {
      std::ostringstream out;
      out << "ps_result_pack(0u, static_cast<uint32_t>("
          << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                      resultInfos, returnStructs, allowMathBare)
          << "))";
      return out.str();
    }
    return "0";
  }
  if (expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
      expr.args.front().name == "Result" && expr.name == "error") {
    if (expr.args.size() != 2) {
      return "false";
    }
    ResultInfo resultInfo;
    if (!resolveResultExprInfo(expr.args[1], resultInfo) || !resultInfo.isResult) {
      return "false";
    }
    std::string argText =
        emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                 resultInfos, returnStructs, allowMathBare);
    if (resultInfo.hasValue) {
      return "(ps_result_error(" + argText + ") != 0u)";
    }
    return "(" + argText + " != 0u)";
  }
  if (expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
      expr.args.front().name == "Result" && expr.name == "why") {
    if (expr.args.size() != 2) {
      return "std::string_view()";
    }
    ResultInfo resultInfo;
    if (!resolveResultExprInfo(expr.args[1], resultInfo) || !resultInfo.isResult) {
      return "std::string_view()";
    }
    std::string argText =
        emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                 resultInfos, returnStructs, allowMathBare);
    const std::string errorExpr =
        resultInfo.hasValue ? "ps_result_error(" + argText + ")" : "static_cast<uint32_t>(" + argText + ")";
    if (resultInfo.errorType == "FileError") {
      return "ps_file_error_why(" + errorExpr + ")";
    }
    auto isSupportedErrorCodeType = [&](const BindingInfo &info) -> bool {
      if (!info.typeTemplateArg.empty()) {
        return false;
      }
      const std::string normalized = normalizeBindingTypeName(info.typeName);
      return normalized == "i32" || normalized == "i64" || normalized == "u64" || normalized == "bool" ||
             normalized == "FileError";
    };
    auto resolveStructTypePath =
        [&](const std::string &typeName, const std::string &namespacePrefix) -> std::string {
      if (typeName.empty()) {
        return "";
      }
      if (!typeName.empty() && typeName[0] == '/') {
        return structTypeMap.count(typeName) > 0 ? typeName : "";
      }
      std::string current = namespacePrefix;
      while (true) {
        if (!current.empty()) {
          std::string scoped = current + "/" + typeName;
          if (structTypeMap.count(scoped) > 0) {
            return scoped;
          }
          if (current.size() > typeName.size()) {
            const size_t start = current.size() - typeName.size();
            if (start > 0 && current[start - 1] == '/' &&
                current.compare(start, typeName.size(), typeName) == 0 &&
                structTypeMap.count(current) > 0) {
              return current;
            }
          }
        } else {
          std::string root = "/" + typeName;
          if (structTypeMap.count(root) > 0) {
            return root;
          }
        }
        if (current.empty()) {
          break;
        }
        const size_t slash = current.find_last_of('/');
        if (slash == std::string::npos || slash == 0) {
          current.clear();
        } else {
          current.erase(slash);
        }
      }
      auto importIt = importAliases.find(typeName);
      if (importIt != importAliases.end() && structTypeMap.count(importIt->second) > 0) {
        return importIt->second;
      }
      return "";
    };
    auto emitWhyCall = [&](const std::string &whyPath, const BindingInfo &paramInfo) -> std::string {
      std::string paramType =
          bindingTypeToCpp(paramInfo, expr.namespacePrefix, importAliases, structTypeMap);
      if (paramType.empty()) {
        paramType = "uint32_t";
      }
      return nameMap.at(whyPath) + "(static_cast<" + paramType + ">(" + errorExpr + "))";
    };
    const std::string errorStructPath = resolveStructTypePath(resultInfo.errorType, expr.namespacePrefix);
    if (!errorStructPath.empty()) {
      const std::string whyPath = errorStructPath + "/why";
      auto whyIt = nameMap.find(whyPath);
      auto whyParamsIt = paramMap.find(whyPath);
      if (whyIt != nameMap.end() && whyParamsIt != paramMap.end() && whyParamsIt->second.size() == 1) {
        const Expr &paramExpr = whyParamsIt->second.front();
        BindingInfo paramInfo = getBindingInfo(paramExpr);
        const std::string paramStructPath = resolveStructTypePath(paramInfo.typeName, paramExpr.namespacePrefix);
        if (!paramStructPath.empty() && paramStructPath == errorStructPath && paramInfo.typeTemplateArg.empty()) {
          auto ctorIt = nameMap.find(errorStructPath);
          auto ctorParamsIt = paramMap.find(errorStructPath);
          if (ctorIt != nameMap.end() && ctorParamsIt != paramMap.end() && ctorParamsIt->second.size() == 1) {
            const Expr &fieldExpr = ctorParamsIt->second.front();
            BindingInfo fieldInfo = getBindingInfo(fieldExpr);
            if (isSupportedErrorCodeType(fieldInfo)) {
              std::string fieldType =
                  bindingTypeToCpp(fieldInfo, fieldExpr.namespacePrefix, importAliases, structTypeMap);
              if (fieldType.empty()) {
                fieldType = "uint32_t";
              }
              return whyIt->second + "(" + ctorIt->second + "(static_cast<" + fieldType + ">(" + errorExpr + ")))";
            }
          }
        }
        if (isSupportedErrorCodeType(paramInfo)) {
          return emitWhyCall(whyPath, paramInfo);
        }
      }
    }
    const std::string normalizedError = normalizeBindingTypeName(resultInfo.errorType);
    if (isPrimitiveBindingTypeName(normalizedError)) {
      const std::string whyPath = "/" + normalizedError + "/why";
      auto whyIt = nameMap.find(whyPath);
      auto whyParamsIt = paramMap.find(whyPath);
      if (whyIt != nameMap.end() && whyParamsIt != paramMap.end() && whyParamsIt->second.size() == 1) {
        BindingInfo paramInfo = getBindingInfo(whyParamsIt->second.front());
        if (isSupportedErrorCodeType(paramInfo)) {
          return emitWhyCall(whyPath, paramInfo);
        }
      }
    }
    return "std::string_view()";
  }
  if (expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
      expr.args.front().name == "Result" && expr.name == "map") {
    if (expr.args.size() != 3) {
      return "static_cast<uint64_t>(0)";
    }
    ResultInfo resultInfo;
    if (!resolveResultExprInfo(expr.args[1], resultInfo) || !resultInfo.isResult || !resultInfo.hasValue) {
      return "static_cast<uint64_t>(0)";
    }
    std::string valueType =
        bindingTypeToCpp(resultInfo.valueType, expr.namespacePrefix, importAliases, structTypeMap);
    if (valueType.empty()) {
      valueType = "uint32_t";
    }
    std::string resultExpr =
        emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                 resultInfos, returnStructs, allowMathBare);
    std::string lambdaExpr =
        emitExpr(expr.args[2], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                 resultInfos, returnStructs, allowMathBare);
    std::ostringstream out;
    out << "([&]() -> uint64_t {";
    out << " auto ps_result = " << resultExpr << ";";
    out << " uint32_t ps_err = ps_result_error(ps_result);";
    out << " if (ps_err != 0u) { return ps_result_pack(ps_err, 0u); }";
    out << " auto ps_value = static_cast<" << valueType << ">(ps_result_value(ps_result));";
    out << " auto ps_mapped = (" << lambdaExpr << ")(ps_value);";
    out << " return ps_result_pack(0u, static_cast<uint32_t>(ps_mapped));";
    out << " }())";
    return out.str();
  }
  if (expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
      expr.args.front().name == "Result" && expr.name == "and_then") {
    if (expr.args.size() != 3) {
      return "static_cast<uint64_t>(0)";
    }
    ResultInfo resultInfo;
    if (!resolveResultExprInfo(expr.args[1], resultInfo) || !resultInfo.isResult || !resultInfo.hasValue) {
      return "static_cast<uint64_t>(0)";
    }
    std::string valueType =
        bindingTypeToCpp(resultInfo.valueType, expr.namespacePrefix, importAliases, structTypeMap);
    if (valueType.empty()) {
      valueType = "uint32_t";
    }
    std::string resultExpr =
        emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                 resultInfos, returnStructs, allowMathBare);
    std::string lambdaExpr =
        emitExpr(expr.args[2], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                 resultInfos, returnStructs, allowMathBare);
    std::ostringstream out;
    out << "([&]() -> uint64_t {";
    out << " auto ps_result = " << resultExpr << ";";
    out << " uint32_t ps_err = ps_result_error(ps_result);";
    out << " if (ps_err != 0u) { return ps_result_pack(ps_err, 0u); }";
    out << " auto ps_value = static_cast<" << valueType << ">(ps_result_value(ps_result));";
    out << " auto ps_next = (" << lambdaExpr << ")(ps_value);";
    out << " return static_cast<uint64_t>(ps_next);";
    out << " }())";
    return out.str();
  }
  if (expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
      expr.args.front().name == "Result" && expr.name == "map2") {
    if (expr.args.size() != 4) {
      return "static_cast<uint64_t>(0)";
    }
    ResultInfo leftInfo;
    ResultInfo rightInfo;
    if (!resolveResultExprInfo(expr.args[1], leftInfo) || !leftInfo.isResult || !leftInfo.hasValue ||
        !resolveResultExprInfo(expr.args[2], rightInfo) || !rightInfo.isResult || !rightInfo.hasValue) {
      return "static_cast<uint64_t>(0)";
    }
    std::string leftType =
        bindingTypeToCpp(leftInfo.valueType, expr.namespacePrefix, importAliases, structTypeMap);
    std::string rightType =
        bindingTypeToCpp(rightInfo.valueType, expr.namespacePrefix, importAliases, structTypeMap);
    if (leftType.empty()) {
      leftType = "uint32_t";
    }
    if (rightType.empty()) {
      rightType = "uint32_t";
    }
    std::string leftExpr =
        emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                 resultInfos, returnStructs, allowMathBare);
    std::string rightExpr =
        emitExpr(expr.args[2], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                 resultInfos, returnStructs, allowMathBare);
    std::string lambdaExpr =
        emitExpr(expr.args[3], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                 resultInfos, returnStructs, allowMathBare);
    std::ostringstream out;
    out << "([&]() -> uint64_t {";
    out << " auto ps_left = " << leftExpr << ";";
    out << " uint32_t ps_left_err = ps_result_error(ps_left);";
    out << " if (ps_left_err != 0u) { return ps_result_pack(ps_left_err, 0u); }";
    out << " auto ps_right = " << rightExpr << ";";
    out << " uint32_t ps_right_err = ps_result_error(ps_right);";
    out << " if (ps_right_err != 0u) { return ps_result_pack(ps_right_err, 0u); }";
    out << " auto ps_left_value = static_cast<" << leftType << ">(ps_result_value(ps_left));";
    out << " auto ps_right_value = static_cast<" << rightType << ">(ps_result_value(ps_right));";
    out << " auto ps_mapped = (" << lambdaExpr << ")(ps_left_value, ps_right_value);";
    out << " return ps_result_pack(0u, static_cast<uint32_t>(ps_mapped));";
    out << " }())";
    return out.str();
  }
  if (!expr.isMethodCall && isSimpleCallName(expr, "try") && expr.args.size() == 1) {
    ResultInfo resultInfo;
    if (resolveResultExprInfo(expr.args.front(), resultInfo) && resultInfo.isResult) {
      std::string argText =
          emitExpr(expr.args.front(), nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                   resultInfos, returnStructs, allowMathBare);
      if (resultInfo.hasValue) {
        std::string base;
        std::string arg;
        if (resultInfo.valueType == "File" ||
            (splitTemplateTypeName(resultInfo.valueType, base, arg) && base == "File")) {
          return "ps_try_file(" + argText + ", ps_on_error)";
        }
        std::string cppType =
            bindingTypeToCpp(resultInfo.valueType, expr.namespacePrefix, importAliases, structTypeMap);
        if (cppType.empty()) {
          cppType = "uint32_t";
        }
        return "ps_try_value<" + cppType + ">(" + argText + ", ps_on_error)";
      }
      return "ps_try_status(" + argText + ", ps_on_error)";
    }
    return "0";
  }
  auto normalizedTypePath = [](const std::string &typePath) -> std::string {
    if (typePath == "/array" || typePath == "array") {
      return "/array";
    }
    if (typePath == "/vector" || typePath == "vector") {
      return "/vector";
    }
    if (typePath == "/map" || typePath == "map") {
      return "/map";
    }
    if (typePath == "/string" || typePath == "string") {
      return "/string";
    }
    return "";
  };
  auto collectionHelperPathCandidates = [](const std::string &path) {
    auto allowsArrayVectorCompatibilitySuffix = [](const std::string &suffix) {
      return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
             suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
             suffix != "remove_at" && suffix != "remove_swap";
    };
    auto allowsVectorStdlibCompatibilitySuffix = [](const std::string &suffix) {
      return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
             suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
             suffix != "remove_at" && suffix != "remove_swap";
    };
    std::vector<std::string> candidates;
    auto appendUnique = [&](const std::string &candidate) {
      if (candidate.empty()) {
        return;
      }
      for (const auto &existing : candidates) {
        if (existing == candidate) {
          return;
        }
      }
      candidates.push_back(candidate);
    };

    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("array/", 0) == 0 || normalizedPath.rfind("vector/", 0) == 0 ||
          normalizedPath.rfind("std/collections/vector/", 0) == 0 || normalizedPath.rfind("map/", 0) == 0 ||
          normalizedPath.rfind("std/collections/map/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }

    appendUnique(path);
    appendUnique(normalizedPath);
    if (normalizedPath.rfind("/array/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/array/").size());
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        appendUnique("/vector/" + suffix);
        appendUnique("/std/collections/vector/" + suffix);
      }
    } else if (normalizedPath.rfind("/vector/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/vector/").size());
      if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
        appendUnique("/std/collections/vector/" + suffix);
      }
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        appendUnique("/array/" + suffix);
      }
    } else if (normalizedPath.rfind("/std/collections/vector/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/vector/").size());
      if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
        appendUnique("/vector/" + suffix);
      }
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        appendUnique("/array/" + suffix);
      }
    } else if (normalizedPath.rfind("/map/", 0) == 0) {
      appendUnique("/std/collections/map/" + normalizedPath.substr(std::string("/map/").size()));
    } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
      if (suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
          suffix != "at" && suffix != "at_unsafe") {
        appendUnique("/map/" + suffix);
      }
    }
    return candidates;
  };
  auto pruneMapAccessStructReturnCompatibilityCandidates = [](const std::string &path,
                                                              std::vector<std::string> &candidates) {
    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("map/", 0) == 0 || normalizedPath.rfind("std/collections/map/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }
    auto eraseCandidate = [&](const std::string &candidate) {
      for (auto it = candidates.begin(); it != candidates.end();) {
        if (*it == candidate) {
          it = candidates.erase(it);
        } else {
          ++it;
        }
      }
    };
    if (normalizedPath.rfind("/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/map/").size());
      if (suffix == "at" || suffix == "at_unsafe") {
        eraseCandidate("/std/collections/map/" + suffix);
      }
    } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
      if (suffix == "at" || suffix == "at_unsafe") {
        eraseCandidate("/map/" + suffix);
      }
    }
  };
  auto pruneMapTryAtStructReturnCompatibilityCandidates = [](const std::string &path,
                                                             std::vector<std::string> &candidates) {
    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("map/", 0) == 0 || normalizedPath.rfind("std/collections/map/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }
    auto eraseCandidate = [&](const std::string &candidate) {
      for (auto it = candidates.begin(); it != candidates.end();) {
        if (*it == candidate) {
          it = candidates.erase(it);
        } else {
          ++it;
        }
      }
    };
    if (normalizedPath == "/map/tryAt") {
      eraseCandidate("/std/collections/map/tryAt");
    } else if (normalizedPath == "/std/collections/map/tryAt") {
      eraseCandidate("/map/tryAt");
    }
  };
  auto isExplicitVectorAccessCompatibilityCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "vector/at" && normalized != "vector/at_unsafe") {
      return false;
    }
    return defMap.count("/" + normalized) == 0;
  };
  auto isExplicitVectorAccessDirectCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    return normalized == "vector/at" || normalized == "vector/at_unsafe" ||
           normalized == "std/collections/vector/at" ||
           normalized == "std/collections/vector/at_unsafe";
  };
  auto explicitVectorAccessResolvedTypePath = [&](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return "";
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "vector/at" && normalized != "vector/at_unsafe" &&
        normalized != "std/collections/vector/at" &&
        normalized != "std/collections/vector/at_unsafe") {
      return "";
    }
    for (const auto &resolvedCandidate : collectionHelperPathCandidates(resolveExprPath(candidate))) {
      auto structIt = returnStructs.find(resolvedCandidate);
      if (structIt != returnStructs.end()) {
        const std::string normalizedStruct = normalizedTypePath(structIt->second);
        if (!normalizedStruct.empty()) {
          return normalizedStruct;
        }
      }
      auto kindIt = returnKinds.find(resolvedCandidate);
      if (kindIt != returnKinds.end()) {
        const std::string normalizedKindType = normalizedTypePath(typeNameForReturnKind(kindIt->second));
        if (!normalizedKindType.empty()) {
          return normalizedKindType;
        }
        return "";
      }
    }
    return "";
  };
  auto probedTypePathForTarget = [&](const Expr &targetExpr) -> std::string {
    Expr probeCall;
    probeCall.kind = Expr::Kind::Call;
    probeCall.isMethodCall = true;
    probeCall.name = "__primec_type_probe";
    probeCall.args.push_back(targetExpr);
    probeCall.argNames.push_back(std::nullopt);

    std::string methodPath;
    if (!resolveMethodCallPath(
            probeCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, methodPath)) {
      return "";
    }
    const size_t suffixPos = methodPath.rfind('/');
    if (suffixPos == std::string::npos || suffixPos == 0) {
      return "";
    }
    return methodPath.substr(0, suffixPos);
  };
  auto resolvedTypePathForResolvedCall = [&](const std::string &resolvedPath) -> std::string {
    auto resolvedCandidates = collectionHelperPathCandidates(resolvedPath);
    pruneMapAccessStructReturnCompatibilityCandidates(resolvedPath, resolvedCandidates);
    pruneMapTryAtStructReturnCompatibilityCandidates(resolvedPath, resolvedCandidates);
    for (const auto &candidate : resolvedCandidates) {
      auto structIt = returnStructs.find(candidate);
      if (structIt != returnStructs.end()) {
        std::string normalized = normalizedTypePath(structIt->second);
        if (!normalized.empty()) {
          return normalized;
        }
      }
    }
    for (const auto &candidate : resolvedCandidates) {
      auto kindIt = returnKinds.find(candidate);
      if (kindIt == returnKinds.end()) {
        continue;
      }
      if (kindIt->second == ReturnKind::String) {
        return "/string";
      }
      if (kindIt->second == ReturnKind::Array) {
        return "/array";
      }
    }
    return "";
  };
  auto builtinCanonicalVectorAccessReceiverTypePath = [&](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return "";
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "std/collections/vector/at" && normalized != "std/collections/vector/at_unsafe") {
      return "";
    }
    if (candidate.args.empty()) {
      return "";
    }
    size_t receiverIndex = 0;
    if (hasNamedArguments(candidate.argNames)) {
      bool foundValues = false;
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          receiverIndex = i;
          foundValues = true;
          break;
        }
      }
      if (!foundValues) {
        receiverIndex = 0;
      }
    }
    if (receiverIndex >= candidate.args.size()) {
      return "";
    }
    const Expr &receiver = candidate.args[receiverIndex];
    if (receiver.kind == Expr::Kind::StringLiteral || isStringValue(receiver, localTypes)) {
      return "/string";
    }
    if (isVectorValue(receiver, localTypes)) {
      return "/vector";
    }
    if (isArrayValue(receiver, localTypes)) {
      return "/array";
    }
    if (receiver.kind == Expr::Kind::Call) {
      std::string collectionName;
      if (getBuiltinCollectionName(receiver, collectionName)) {
        if (collectionName == "vector" || collectionName == "array") {
          return "/" + collectionName;
        }
      }
      const std::string receiverTypePath = resolvedTypePathForResolvedCall(resolveExprPath(receiver));
      if (receiverTypePath == "/vector" || receiverTypePath == "/array" || receiverTypePath == "/string") {
        return receiverTypePath;
      }
    }
    return "";
  };
  auto builtinCanonicalMapAccessReceiverTypePath = [&](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return "";
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "std/collections/map/at" && normalized != "std/collections/map/at_unsafe") {
      return "";
    }
    if (candidate.args.empty()) {
      return "";
    }
    size_t receiverIndex = 0;
    if (hasNamedArguments(candidate.argNames)) {
      bool foundValues = false;
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          receiverIndex = i;
          foundValues = true;
          break;
        }
      }
      if (!foundValues) {
        receiverIndex = 0;
      }
    }
    if (receiverIndex >= candidate.args.size()) {
      return "";
    }
    const Expr &receiver = candidate.args[receiverIndex];
    if (isMapValue(receiver, localTypes)) {
      return "/map";
    }
    if (receiver.kind == Expr::Kind::Call) {
      std::string collectionName;
      if (getBuiltinCollectionName(receiver, collectionName) && collectionName == "map") {
        return "/map";
      }
      const std::string receiverTypePath = resolvedTypePathForResolvedCall(resolveExprPath(receiver));
      if (receiverTypePath == "/map") {
        return receiverTypePath;
      }
    }
    return "";
  };
  auto builtinVectorAccessMethodReceiverTypePath = [&](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name.empty() || candidate.args.empty()) {
      return "";
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "at" && normalized != "at_unsafe" && normalized != "vector/at" &&
        normalized != "vector/at_unsafe" && normalized != "std/collections/vector/at" &&
        normalized != "std/collections/vector/at_unsafe") {
      return "";
    }
    const Expr &receiver = candidate.args.front();
    if (receiver.kind == Expr::Kind::StringLiteral || isStringValue(receiver, localTypes)) {
      return "/string";
    }
    if (isVectorValue(receiver, localTypes)) {
      return "/vector";
    }
    if (isArrayValue(receiver, localTypes)) {
      return "/array";
    }
    if (receiver.kind == Expr::Kind::Call) {
      std::string collectionName;
      if (getBuiltinCollectionName(receiver, collectionName)) {
        if (collectionName == "vector" || collectionName == "array") {
          return "/" + collectionName;
        }
      }
      const std::string receiverTypePath = resolvedTypePathForResolvedCall(resolveExprPath(receiver));
      if (receiverTypePath == "/vector" || receiverTypePath == "/array" || receiverTypePath == "/string") {
        return receiverTypePath;
      }
    }
    return "";
  };
  auto builtinMapAccessMethodReceiverTypePath = [&](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name.empty() ||
        candidate.args.empty()) {
      return "";
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "at" && normalized != "at_unsafe") {
      return "";
    }
    const Expr &receiver = candidate.args.front();
    if (isMapValue(receiver, localTypes)) {
      return "/map";
    }
    if (receiver.kind == Expr::Kind::Call) {
      std::string collectionName;
      if (getBuiltinCollectionName(receiver, collectionName) && collectionName == "map") {
        return "/map";
      }
      const std::string receiverTypePath = resolvedTypePathForResolvedCall(resolveExprPath(receiver));
      if (receiverTypePath == "/map") {
        return receiverTypePath;
      }
    }
    return "";
  };
  auto isExplicitMapAccessMethod = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    return normalized == "map/at" || normalized == "map/at_unsafe" ||
           normalized == "std/collections/map/at" ||
           normalized == "std/collections/map/at_unsafe";
  };
  auto resolvedTypePathForTarget = [&](const Expr &targetExpr) -> std::string {
    if (isStringValue(targetExpr, localTypes)) {
      return "/string";
    }
    if (isMapValue(targetExpr, localTypes)) {
      return "/map";
    }
    if (isVectorValue(targetExpr, localTypes)) {
      return "/vector";
    }
    if (isArrayValue(targetExpr, localTypes)) {
      return "/array";
    }
    if (targetExpr.kind != Expr::Kind::Call) {
      return "";
    }
    std::string collectionName;
    if (getBuiltinCollectionName(targetExpr, collectionName)) {
      if (collectionName == "array" || collectionName == "vector" || collectionName == "map") {
        return "/" + collectionName;
      }
    }
    if (!targetExpr.isMethodCall) {
      const std::string resolvedExplicitVectorAccessType =
          explicitVectorAccessResolvedTypePath(targetExpr);
      if (!resolvedExplicitVectorAccessType.empty()) {
        return resolvedExplicitVectorAccessType;
      }
      const bool shouldProbeBuiltinVectorAccessType =
          !isExplicitVectorAccessDirectCall(targetExpr) &&
          !builtinCanonicalVectorAccessReceiverTypePath(targetExpr).empty();
      const bool shouldProbeBuiltinMapAccessType =
          !builtinCanonicalMapAccessReceiverTypePath(targetExpr).empty();
      if (shouldProbeBuiltinVectorAccessType || shouldProbeBuiltinMapAccessType) {
        const std::string probedTypePath = probedTypePathForTarget(targetExpr);
        if (probedTypePath == "/string" || probedTypePath == "/array" || probedTypePath == "/vector" ||
            probedTypePath == "/map") {
          return probedTypePath;
        }
        if (!probedTypePath.empty()) {
          return "";
        }
      }
      return resolvedTypePathForResolvedCall(resolveExprPath(targetExpr));
    }
    if (isExplicitMapAccessMethod(targetExpr)) {
      const std::string probedTypePath = probedTypePathForTarget(targetExpr);
      if (!probedTypePath.empty()) {
        return probedTypePath;
      }
    }
    if (!builtinMapAccessMethodReceiverTypePath(targetExpr).empty()) {
      const std::string probedTypePath = probedTypePathForTarget(targetExpr);
      if (probedTypePath == "/map" || probedTypePath == "/string") {
        return probedTypePath;
      }
      if (!probedTypePath.empty()) {
        return "";
      }
    }
    if (!builtinVectorAccessMethodReceiverTypePath(targetExpr).empty()) {
      const std::string probedTypePath = probedTypePathForTarget(targetExpr);
      if (probedTypePath == "/string" || probedTypePath == "/array" || probedTypePath == "/vector" ||
          probedTypePath == "/map") {
        return probedTypePath;
      }
      if (!probedTypePath.empty()) {
        return "";
      }
    }
    std::string methodPath;
    if (!resolveMethodCallPath(
            targetExpr, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, methodPath)) {
      return "";
    }
    return resolvedTypePathForResolvedCall(methodPath);
  };
  auto isResolvedMapTarget = [&](const Expr &targetExpr) -> bool {
    return resolvedTypePathForTarget(targetExpr) == "/map";
  };
  auto isResolvedStringTarget = [&](const Expr &targetExpr) -> bool {
    return resolvedTypePathForTarget(targetExpr) == "/string";
  };
  auto isResolvedArrayLikeTarget = [&](const Expr &targetExpr) -> bool {
    const std::string typePath = resolvedTypePathForTarget(targetExpr);
    return typePath == "/array" || typePath == "/vector";
  };
  auto isResolvedVectorTarget = [&](const Expr &targetExpr) -> bool {
    return resolvedTypePathForTarget(targetExpr) == "/vector";
  };
  auto pickExplicitVectorAccessReceiverIndex = [&](const Expr &candidate) -> size_t {
    if (candidate.args.size() != 2) {
      return 0;
    }
    if (hasNamedArguments(candidate.argNames)) {
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          return i;
        }
      }
    }
    return 0;
  };
  auto isNoHelperExplicitVectorAccessCallFallback = [&](const Expr &candidate) {
    if (!isExplicitVectorAccessDirectCall(candidate) || !explicitVectorAccessResolvedTypePath(candidate).empty() ||
        candidate.args.size() != 2) {
      return false;
    }
    const size_t receiverIndex = pickExplicitVectorAccessReceiverIndex(candidate);
    return receiverIndex < candidate.args.size() && isResolvedVectorTarget(candidate.args[receiverIndex]);
  };
  auto isExplicitVectorCountCapacityDirectCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    return normalized == "vector/count" || normalized == "std/collections/vector/count" ||
           normalized == "vector/capacity" || normalized == "std/collections/vector/capacity";
  };
  auto isExplicitVectorCountCapacitySlashMethod = [&](const Expr &candidate, const char *helper) {
    if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    return normalized == std::string("vector/") + helper ||
           normalized == std::string("std/collections/vector/") + helper;
  };
  auto isExplicitVectorAccessSlashMethod = [&](const Expr &candidate, const char *helper) {
    if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    return normalized == std::string("vector/") + helper ||
           normalized == std::string("std/collections/vector/") + helper;
  };
  auto pickExplicitVectorCountCapacityReceiverIndex = [&](const Expr &candidate) -> size_t {
    if (candidate.args.size() != 1) {
      return 0;
    }
    if (hasNamedArguments(candidate.argNames) && !candidate.argNames.empty() && candidate.argNames.front().has_value() &&
        *candidate.argNames.front() == "values") {
      return 0;
    }
    return 0;
  };
  auto isNoHelperExplicitVectorCountCapacityCallFallback = [&](const Expr &candidate) {
    if (!isExplicitVectorCountCapacityDirectCall(candidate) || candidate.args.size() != 1) {
      return false;
    }
    const size_t receiverIndex = pickExplicitVectorCountCapacityReceiverIndex(candidate);
    return receiverIndex < candidate.args.size() && isResolvedVectorTarget(candidate.args[receiverIndex]);
  };
  auto emitMissingExplicitVectorCountCapacityCall = [&](const Expr &candidate) {
    const size_t receiverIndex = pickExplicitVectorCountCapacityReceiverIndex(candidate);
    const std::string resolvedPath = resolveExprPath(candidate);
    const bool isCapacity = resolvedPath == "/vector/capacity" ||
                            resolvedPath == "/std/collections/vector/capacity";
    std::ostringstream out;
    out << "ps_missing_vector_" << (isCapacity ? "capacity" : "count") << "_call_helper("
        << emitExpr(candidate.args[receiverIndex],
                    nameMap,
                    paramMap,
                    defMap,
                    structTypeMap,
                    importAliases,
                    localTypes,
                    returnKinds,
                    resultInfos,
                    returnStructs,
                    allowMathBare)
        << ")";
    return out.str();
  };
  auto emitMissingExplicitVectorAccessCall = [&](const Expr &candidate) {
    const size_t receiverIndex = pickExplicitVectorAccessReceiverIndex(candidate);
    const size_t indexIndex = receiverIndex == 0 ? 1 : 0;
    const std::string resolvedPath = resolveExprPath(candidate);
    const bool isUnsafe = resolvedPath == "/vector/at_unsafe" ||
                          resolvedPath == "/std/collections/vector/at_unsafe";
    std::ostringstream out;
    out << "ps_missing_vector_" << (isUnsafe ? "at_unsafe" : "at") << "_call_helper("
        << emitExpr(candidate.args[receiverIndex],
                    nameMap,
                    paramMap,
                    defMap,
                    structTypeMap,
                    importAliases,
                    localTypes,
                    returnKinds,
                    resultInfos,
                    returnStructs,
                    allowMathBare)
        << ", "
        << emitExpr(candidate.args[indexIndex],
                    nameMap,
                    paramMap,
                    defMap,
                    structTypeMap,
                    importAliases,
                    localTypes,
                    returnKinds,
                    resultInfos,
                    returnStructs,
                    allowMathBare)
        << ")";
    return out.str();
  };
  auto isNoHelperExplicitVectorAccessCountReceiver = [&](const Expr &candidate) {
    if (isExplicitVectorAccessDirectCall(candidate)) {
      return explicitVectorAccessResolvedTypePath(candidate).empty();
    }
    if ((isExplicitVectorAccessSlashMethod(candidate, "at") ||
         isExplicitVectorAccessSlashMethod(candidate, "at_unsafe")) &&
        candidate.isMethodCall) {
      return probedTypePathForTarget(candidate).empty();
    }
    return false;
  };
  auto pickAccessReceiverIndex = [&]() -> size_t {
    if (expr.args.size() != 2) {
      return 0;
    }
    const Expr &leading = expr.args.front();
    const bool leadingCouldBeIndex =
        leading.kind == Expr::Kind::Literal || leading.kind == Expr::Kind::BoolLiteral ||
        leading.kind == Expr::Kind::FloatLiteral || leading.kind == Expr::Kind::StringLiteral;
    const bool leadingIsKnownReceiver = isResolvedArrayLikeTarget(leading) || isResolvedMapTarget(leading) ||
                                        isResolvedStringTarget(leading);
    const bool trailingIsKnownReceiver = isResolvedArrayLikeTarget(expr.args[1]) || isResolvedMapTarget(expr.args[1]) ||
                                         isResolvedStringTarget(expr.args[1]);
    if (!leadingIsKnownReceiver && trailingIsKnownReceiver && leadingCouldBeIndex) {
      return 1;
    }
    return 0;
  };
  auto isVectorBuiltinName = [&](const Expr &candidate, const char *helper) -> bool {
    if (isSimpleCallName(candidate, helper)) {
      return true;
    }
    if (candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    return normalized == std::string("std/collections/vector/") + helper;
  };
  auto preferStructReturningCollectionHelperPath = [&](const std::string &path) {
    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/' &&
        normalizedPath.rfind("std/collections/map/", 0) == 0) {
      normalizedPath.insert(normalizedPath.begin(), '/');
    }
    std::string firstExisting;
    for (const auto &candidate : collectionHelperPathCandidates(path)) {
      if (nameMap.count(candidate) == 0) {
        continue;
      }
      if (firstExisting.empty()) {
        firstExisting = candidate;
      }
      auto kindIt = returnKinds.find(candidate);
      auto structIt = returnStructs.find(candidate);
      if (kindIt != returnKinds.end() && kindIt->second == ReturnKind::Array &&
          structIt != returnStructs.end() && !structIt->second.empty()) {
        return candidate;
      }
    }
    if (!firstExisting.empty()) {
      return firstExisting;
    }
    return preferVectorStdlibHelperPath(path, nameMap);
  };
  auto preferBareMapHelperPath = [&](const Expr &candidate, const char *helperName) {
    if (candidate.isMethodCall || !isSimpleCallName(candidate, helperName) || candidate.args.size() != 2) {
      return std::string{};
    }
    if (!isResolvedMapTarget(candidate.args.front())) {
      return std::string{};
    }
    const std::string canonicalPath = std::string("/std/collections/map/") + helperName;
    if (nameMap.count(canonicalPath) > 0) {
      return canonicalPath;
    }
    const std::string compatibilityPath = std::string("/map/") + helperName;
    if (nameMap.count(compatibilityPath) > 0) {
      return compatibilityPath;
    }
    return std::string{};
  };
  auto preferExplicitVectorCountCapacityHelperPath = [&](const Expr &candidate, const std::string &path) {
    if (!isExplicitVectorCountCapacityDirectCall(candidate)) {
      return preferStructReturningCollectionHelperPath(path);
    }
    if (path == "/vector/count" || path == "/vector/capacity") {
      return path;
    }
    return preferStructReturningCollectionHelperPath(path);
  };
  std::string resolvedFull = preferExplicitVectorCountCapacityHelperPath(expr, full);
  auto it = nameMap.find(resolvedFull);
  if (it == nameMap.end()) {
    const std::string preferredBareMapContainsPath = preferBareMapHelperPath(expr, "contains");
    if (!preferredBareMapContainsPath.empty()) {
      resolvedFull = preferredBareMapContainsPath;
      it = nameMap.find(resolvedFull);
    }
  }
  if (it == nameMap.end()) {
    const std::string preferredBareMapTryAtPath = preferBareMapHelperPath(expr, "tryAt");
    if (!preferredBareMapTryAtPath.empty()) {
      resolvedFull = preferredBareMapTryAtPath;
      it = nameMap.find(resolvedFull);
    }
  }
  if (it == nameMap.end()) {
    if (isNoHelperExplicitVectorCountCapacityCallFallback(expr)) {
      return emitMissingExplicitVectorCountCapacityCall(expr);
    }
    if (isNoHelperExplicitVectorAccessCallFallback(expr)) {
      return emitMissingExplicitVectorAccessCall(expr);
    }
    if (isVectorBuiltinName(expr, "count") && expr.args.size() == 1 &&
        isNoHelperExplicitVectorAccessCountReceiver(expr.args.front())) {
      std::ostringstream out;
      out << "ps_missing_vector_access_count_receiver_helper("
          << (isNoHelperExplicitVectorAccessCallFallback(expr.args.front())
                  ? emitMissingExplicitVectorAccessCall(expr.args.front())
                  : emitExpr(expr.args.front(),
                             nameMap,
                             paramMap,
                             defMap,
                             structTypeMap,
                             importAliases,
                             localTypes,
                             returnKinds,
                             resultInfos,
                             returnStructs,
                             allowMathBare))
          << ")";
      return out.str();
    }
    if (expr.isMethodCall &&
        (isSimpleCallName(expr, "count") ||
         isExplicitVectorCountCapacitySlashMethod(expr, "count")) &&
        expr.args.size() == 1 &&
        isResolvedVectorTarget(expr.args.front())) {
      std::ostringstream out;
      out << "ps_missing_vector_count_method_helper("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (expr.isMethodCall && isExplicitStdlibVectorHelper(expr.name, "count") && expr.args.size() == 1) {
      std::ostringstream out;
      out << "ps_missing_vector_count_method_helper("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (expr.isMethodCall && resolveExprPath(expr) == "/vector/count" && expr.args.size() == 1 &&
        isResolvedStringTarget(expr.args.front())) {
      std::ostringstream out;
      out << "ps_missing_vector_count_method_helper("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (expr.isMethodCall && resolveExprPath(expr) == "/vector/count" && expr.args.size() == 1 &&
        isResolvedMapTarget(expr.args.front())) {
      std::ostringstream out;
      out << "ps_missing_vector_count_method_helper("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (expr.isMethodCall && resolveExprPath(expr) == "/vector/count" && expr.args.size() == 1 &&
        isResolvedArrayLikeTarget(expr.args.front())) {
      std::ostringstream out;
      out << "ps_missing_vector_count_method_helper("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (!expr.isMethodCall && isVectorBuiltinName(expr, "count") && expr.args.size() == 1 &&
        isResolvedVectorTarget(expr.args.front())) {
      std::ostringstream out;
      out << "ps_missing_vector_count_call_helper("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (!expr.isMethodCall && isExplicitStdlibVectorHelper(expr.name, "count") && expr.args.size() == 1) {
      std::ostringstream out;
      out << "ps_missing_vector_count_call_helper("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (!expr.isMethodCall && resolveExprPath(expr) == "/vector/count" && expr.args.size() == 1 &&
        isResolvedStringTarget(expr.args.front())) {
      std::ostringstream out;
      out << "ps_missing_vector_count_call_helper("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (!expr.isMethodCall && resolveExprPath(expr) == "/vector/count" && expr.args.size() == 1 &&
        isResolvedArrayLikeTarget(expr.args.front())) {
      std::ostringstream out;
      out << "ps_missing_vector_count_call_helper("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (isVectorBuiltinName(expr, "count") && expr.args.size() == 1 && isResolvedMapTarget(expr.args.front())) {
      std::ostringstream out;
      out << "ps_map_count("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (isVectorBuiltinName(expr, "count") && expr.args.size() == 1 && isResolvedArrayLikeTarget(expr.args.front())) {
      std::ostringstream out;
      out << "ps_array_count("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (isVectorBuiltinName(expr, "count") && expr.args.size() == 1 && isResolvedStringTarget(expr.args.front())) {
      std::ostringstream out;
      out << "ps_string_count("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (expr.isMethodCall &&
        (isSimpleCallName(expr, "capacity") ||
         isExplicitVectorCountCapacitySlashMethod(expr, "capacity")) &&
        expr.args.size() == 1 &&
        isResolvedVectorTarget(expr.args.front())) {
      std::ostringstream out;
      out << "ps_missing_vector_capacity_method_helper("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (expr.isMethodCall && isExplicitStdlibVectorHelper(expr.name, "capacity") && expr.args.size() == 1) {
      std::ostringstream out;
      out << "ps_missing_vector_capacity_method_helper("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (expr.isMethodCall && resolveExprPath(expr) == "/vector/capacity" && expr.args.size() == 1 &&
        isResolvedMapTarget(expr.args.front())) {
      std::ostringstream out;
      out << "ps_missing_vector_capacity_method_helper("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (!expr.isMethodCall && isVectorBuiltinName(expr, "capacity") && expr.args.size() == 1 &&
        isResolvedVectorTarget(expr.args.front())) {
      std::ostringstream out;
      out << "ps_missing_vector_capacity_call_helper("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (!expr.isMethodCall && isExplicitStdlibVectorHelper(expr.name, "capacity") && expr.args.size() == 1) {
      std::ostringstream out;
      out << "ps_missing_vector_capacity_call_helper("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (isVectorBuiltinName(expr, "capacity") && expr.args.size() == 1 && isResolvedVectorTarget(expr.args.front())) {
      std::ostringstream out;
      out << "ps_vector_capacity("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (!expr.isMethodCall && isSimpleCallName(expr, "contains") && expr.args.size() == 2 &&
        isResolvedMapTarget(expr.args.front())) {
      std::ostringstream out;
      out << "ps_map_contains("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << ", "
          << emitExpr(expr.args[1],
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (!expr.isMethodCall && isSimpleCallName(expr, "File") && expr.templateArgs.size() == 1 && expr.args.size() == 1) {
      std::ostringstream out;
      const std::string mode = expr.templateArgs.front();
      std::string pathText =
          emitExpr(expr.args.front(), nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                   resultInfos, returnStructs, allowMathBare);
      if (mode == "Read") {
        out << "ps_file_open_read(" << pathText << ")";
        return out.str();
      }
      if (mode == "Write") {
        out << "ps_file_open_write(" << pathText << ")";
        return out.str();
      }
      if (mode == "Append") {
        out << "ps_file_open_append(" << pathText << ")";
        return out.str();
      }
    }
    if (full.rfind("/file/", 0) == 0) {
      std::ostringstream out;
      const std::string receiver =
          emitExpr(expr.args.front(), nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                   resultInfos, returnStructs, allowMathBare);
      if (expr.name == "write" || expr.name == "write_line") {
        out << "ps_file_" << expr.name << "(" << receiver;
        for (size_t i = 1; i < expr.args.size(); ++i) {
          out << ", "
              << emitExpr(expr.args[i], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                          resultInfos, returnStructs, allowMathBare);
        }
        out << ")";
        return out.str();
      }
      if (expr.name == "write_byte" && expr.args.size() == 2) {
        out << "ps_file_write_byte(" << receiver << ", "
            << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                        resultInfos, returnStructs, allowMathBare)
            << ")";
        return out.str();
      }
      if (expr.name == "read_byte" && expr.args.size() == 2) {
        out << "ps_file_read_byte(" << receiver << ", "
            << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                        resultInfos, returnStructs, allowMathBare)
            << ")";
        return out.str();
      }
      if (expr.name == "write_bytes" && expr.args.size() == 2) {
        out << "ps_file_write_bytes(" << receiver << ", "
            << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                        resultInfos, returnStructs, allowMathBare)
            << ")";
        return out.str();
      }
      if (expr.name == "flush") {
        out << "ps_file_flush(" << receiver << ")";
        return out.str();
      }
      if (expr.name == "close") {
        out << "ps_file_close(" << receiver << ")";
        return out.str();
      }
    }
    if (full == "/file_error/why" && expr.args.size() == 1) {
      std::ostringstream out;
      out << "ps_file_error_why("
          << emitExpr(expr.args.front(), nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                      resultInfos, returnStructs, allowMathBare)
          << ")";
      return out.str();
    }
    if ((isSimpleCallName(expr, "at") ||
         isExplicitVectorAccessSlashMethod(expr, "at")) &&
        expr.args.size() == 2) {
      std::ostringstream out;
      const size_t receiverIndex = pickAccessReceiverIndex();
      const size_t indexIndex = receiverIndex == 0 ? 1 : 0;
      const Expr &target = expr.args[receiverIndex];
      if (!expr.isMethodCall && isResolvedVectorTarget(target)) {
        out << "ps_missing_vector_at_call_helper("
            << emitExpr(target,
                        nameMap,
                        paramMap,
                        defMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare)
            << ", "
            << emitExpr(expr.args[indexIndex],
                        nameMap,
                        paramMap,
                        defMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare)
            << ")";
        return out.str();
      }
      if (!expr.isMethodCall && isExplicitVectorAccessDirectCall(expr)) {
        return emitMissingExplicitVectorAccessCall(expr);
      }
      if (expr.isMethodCall && isResolvedVectorTarget(target)) {
        out << "ps_missing_vector_at_method_helper("
            << emitExpr(target,
                        nameMap,
                        paramMap,
                        defMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare)
            << ", "
            << emitExpr(expr.args[indexIndex],
                        nameMap,
                        paramMap,
                        defMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare)
            << ")";
        return out.str();
      }
      if (expr.isMethodCall && isExplicitVectorAccessSlashMethod(expr, "at")) {
        out << "ps_missing_vector_at_method_helper("
            << emitExpr(target,
                        nameMap,
                        paramMap,
                        defMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare)
            << ", "
            << emitExpr(expr.args[indexIndex],
                        nameMap,
                        paramMap,
                        defMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare)
            << ")";
        return out.str();
      }
      if (isResolvedStringTarget(target)) {
        out << "ps_string_at("
            << emitExpr(target, nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[indexIndex],
                        nameMap,
                        paramMap,
                        defMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare)
            << ")";
      } else if (isResolvedMapTarget(target)) {
        out << "ps_map_at("
            << emitExpr(target, nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[indexIndex],
                        nameMap,
                        paramMap,
                        defMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare)
            << ")";
      } else if (isResolvedVectorTarget(target)) {
        out << "ps_vector_at("
            << emitExpr(target, nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[indexIndex],
                        nameMap,
                        paramMap,
                        defMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare)
            << ")";
      } else {
        out << "ps_array_at("
            << emitExpr(target, nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[indexIndex],
                        nameMap,
                        paramMap,
                        defMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare)
            << ")";
      }
      return out.str();
    }
    if ((isSimpleCallName(expr, "at_unsafe") ||
         isExplicitVectorAccessSlashMethod(expr, "at_unsafe")) &&
        expr.args.size() == 2) {
      std::ostringstream out;
      const size_t receiverIndex = pickAccessReceiverIndex();
      const size_t indexIndex = receiverIndex == 0 ? 1 : 0;
      const Expr &target = expr.args[receiverIndex];
      if (!expr.isMethodCall && isResolvedVectorTarget(target)) {
        out << "ps_missing_vector_at_unsafe_call_helper("
            << emitExpr(target,
                        nameMap,
                        paramMap,
                        defMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare)
            << ", "
            << emitExpr(expr.args[indexIndex],
                        nameMap,
                        paramMap,
                        defMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare)
            << ")";
        return out.str();
      }
      if (!expr.isMethodCall && isExplicitVectorAccessDirectCall(expr)) {
        return emitMissingExplicitVectorAccessCall(expr);
      }
      if (expr.isMethodCall && isResolvedVectorTarget(target)) {
        out << "ps_missing_vector_at_unsafe_method_helper("
            << emitExpr(target,
                        nameMap,
                        paramMap,
                        defMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare)
            << ", "
            << emitExpr(expr.args[indexIndex],
                        nameMap,
                        paramMap,
                        defMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare)
            << ")";
        return out.str();
      }
      if (expr.isMethodCall && isExplicitVectorAccessSlashMethod(expr, "at_unsafe")) {
        out << "ps_missing_vector_at_unsafe_method_helper("
            << emitExpr(target,
                        nameMap,
                        paramMap,
                        defMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare)
            << ", "
            << emitExpr(expr.args[indexIndex],
                        nameMap,
                        paramMap,
                        defMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare)
            << ")";
        return out.str();
      }
      if (isResolvedStringTarget(target)) {
        out << "ps_string_at_unsafe("
            << emitExpr(target, nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[indexIndex],
                        nameMap,
                        paramMap,
                        defMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare)
            << ")";
      } else if (isResolvedMapTarget(target)) {
        out << "ps_map_at_unsafe("
            << emitExpr(target, nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[indexIndex],
                        nameMap,
                        paramMap,
                        defMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare)
            << ")";
      } else {
        out << "ps_array_at_unsafe("
            << emitExpr(target, nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[indexIndex],
                        nameMap,
                        paramMap,
                        defMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare)
            << ")";
      }
      return out.str();
    }
    auto rewriteMathArithmeticExpr = [&](const Expr &candidate, Expr &rewrittenExpr) {
      char candidateOp = '\0';
      if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || !getBuiltinOperator(candidate, candidateOp) ||
          candidate.args.size() != 2) {
        return false;
      }
      auto resolvedMathTypeForTarget = [&](const Expr &targetExpr) -> std::string {
        if (targetExpr.kind == Expr::Kind::Name) {
          auto localIt = localTypes.find(targetExpr.name);
          if (localIt != localTypes.end()) {
            std::string localType = normalizeBindingTypeName(localIt->second.typeName);
            if (!localType.empty() && localType.front() == '/') {
              return normalizedTypePath(localType);
            }
            auto importIt = importAliases.find(localType);
            if (importIt != importAliases.end()) {
              return normalizedTypePath(importIt->second);
            }
          }
        }
        return resolvedTypePathForTarget(targetExpr);
      };
      auto matrixHelperBaseForType = [&](const std::string &typePath) -> std::string {
        if (typePath == "/std/math/Mat2") {
          return "/std/math/mat2";
        }
        if (typePath == "/std/math/Mat3") {
          return "/std/math/mat3";
        }
        if (typePath == "/std/math/Mat4") {
          return "/std/math/mat4";
        }
        return "";
      };
      auto quaternionHelperBaseForType = [&](const std::string &typePath) -> std::string {
        if (typePath == "/std/math/Quat") {
          return "/std/math/quat";
        }
        return "";
      };
      auto isNumericScalarTarget = [&](const Expr &targetExpr) {
        ReturnKind kind = inferPrimitiveReturnKind(targetExpr, localTypes, returnKinds, allowMathBare);
        return kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
               kind == ReturnKind::Float32 || kind == ReturnKind::Float64;
      };
      auto rewriteHelperCall = [&](const std::string &helperPath) {
        rewrittenExpr = candidate;
        rewrittenExpr.name = helperPath;
        rewrittenExpr.namespacePrefix.clear();
        rewrittenExpr.isMethodCall = false;
        rewrittenExpr.isFieldAccess = false;
        return true;
      };
      const std::string leftType = resolvedMathTypeForTarget(candidate.args[0]);
      const std::string rightType = resolvedMathTypeForTarget(candidate.args[1]);
      const std::string leftMatrixBase = matrixHelperBaseForType(leftType);
      const std::string rightMatrixBase = matrixHelperBaseForType(rightType);
      const std::string leftQuaternionBase = quaternionHelperBaseForType(leftType);
      const std::string rightQuaternionBase = quaternionHelperBaseForType(rightType);
      if (candidateOp == '+' && !leftMatrixBase.empty() && leftType == rightType) {
        return rewriteHelperCall(leftMatrixBase + "_add_internal");
      }
      if (candidateOp == '+' && !leftQuaternionBase.empty() && leftType == rightType) {
        return rewriteHelperCall(leftQuaternionBase + "_add_internal");
      }
      if (candidateOp == '-' && !leftMatrixBase.empty() && leftType == rightType) {
        return rewriteHelperCall(leftMatrixBase + "_sub_internal");
      }
      if (candidateOp == '-' && !leftQuaternionBase.empty() && leftType == rightType) {
        return rewriteHelperCall(leftQuaternionBase + "_sub_internal");
      }
      if (candidateOp == '*' && !leftMatrixBase.empty() && isNumericScalarTarget(candidate.args[1])) {
        return rewriteHelperCall(leftMatrixBase + "_scale_internal");
      }
      if (candidateOp == '*' && !leftQuaternionBase.empty() && isNumericScalarTarget(candidate.args[1])) {
        return rewriteHelperCall(leftQuaternionBase + "_scale_internal");
      }
      if (candidateOp == '*' && isNumericScalarTarget(candidate.args[0]) && !rightMatrixBase.empty()) {
        return rewriteHelperCall(rightMatrixBase + "_scale_left_internal");
      }
      if (candidateOp == '*' && isNumericScalarTarget(candidate.args[0]) && !rightQuaternionBase.empty()) {
        return rewriteHelperCall(rightQuaternionBase + "_scale_left_internal");
      }
      if (candidateOp == '/' && !leftMatrixBase.empty() && isNumericScalarTarget(candidate.args[1])) {
        return rewriteHelperCall(leftMatrixBase + "_div_scalar_internal");
      }
      if (candidateOp == '/' && !leftQuaternionBase.empty() && isNumericScalarTarget(candidate.args[1])) {
        return rewriteHelperCall(leftQuaternionBase + "_div_scalar_internal");
      }
      if (leftType == "/std/math/Mat2" && rightType == "/std/math/Vec2") {
        return rewriteHelperCall("/std/math/mat2_mul_vec2_internal");
      }
      if (leftType == "/std/math/Mat3" && rightType == "/std/math/Vec3") {
        return rewriteHelperCall("/std/math/mat3_mul_vec3_internal");
      }
      if (leftType == "/std/math/Mat4" && rightType == "/std/math/Vec4") {
        return rewriteHelperCall("/std/math/mat4_mul_vec4_internal");
      }
      if (leftType == "/std/math/Mat2" && rightType == "/std/math/Mat2") {
        return rewriteHelperCall("/std/math/mat2_mul_mat2_internal");
      }
      if (leftType == "/std/math/Mat3" && rightType == "/std/math/Mat3") {
        return rewriteHelperCall("/std/math/mat3_mul_mat3_internal");
      }
      if (leftType == "/std/math/Mat4" && rightType == "/std/math/Mat4") {
        return rewriteHelperCall("/std/math/mat4_mul_mat4_internal");
      }
      if (leftType == "/std/math/Quat" && rightType == "/std/math/Quat") {
        return rewriteHelperCall("/std/math/quat_multiply_internal");
      }
      if (leftType == "/std/math/Quat" && rightType == "/std/math/Vec3") {
        return rewriteHelperCall("/std/math/quat_rotate_vec3_internal");
      }
      return false;
    };
    Expr rewrittenMathArithmeticExpr;
    if (rewriteMathArithmeticExpr(expr, rewrittenMathArithmeticExpr)) {
      return emitExpr(rewrittenMathArithmeticExpr,
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare);
    }
    char op = '\0';
    if (getBuiltinOperator(expr, op) && expr.args.size() == 2) {
      std::ostringstream out;
      if ((op == '+' || op == '-') && isPointerExpr(expr.args[0])) {
        out << "ps_pointer_add("
            << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ", ";
        if (op == '-') {
          out << "-("
              << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
              << ")";
        } else {
          out << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare);
        }
        out << ")";
        return out.str();
      }
      out << "("
          << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << " "
          << op << " "
          << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
      return out.str();
    }
    if (isBuiltinNegate(expr) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "(-"
          << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
      return out.str();
    }
    if (!expr.isMethodCall && isSimpleCallName(expr, "move") && expr.args.size() == 1) {
      std::ostringstream out;
      out << "std::move("
          << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
          << ")";
      return out.str();
    }
    std::string mutateName;
    if (getBuiltinMutationName(expr, mutateName) && expr.args.size() == 1) {
      std::ostringstream out;
      const char *op = mutateName == "increment" ? "++" : "--";
      out << "(" << op
          << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
      return out.str();
    }
    const char *cmp = nullptr;
    if (getBuiltinComparison(expr, cmp)) {
      std::ostringstream out;
      if (expr.args.size() == 1) {
        out << "("
            << cmp
            << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ")";
        return out.str();
      }
      if (expr.args.size() == 2) {
        out << "("
            << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << " "
            << cmp << " "
            << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ")";
        return out.str();
      }
    }
    if (isBuiltinClamp(expr, allowMathBare) && expr.args.size() == 3) {
      std::ostringstream out;
      out << "ps_builtin_clamp("
          << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ", "
          << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ", "
          << emitExpr(expr.args[2], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
          << ")";
      return out.str();
    }
    std::string minMaxName;
    if (getBuiltinMinMaxName(expr, minMaxName, allowMathBare) && expr.args.size() == 2) {
      std::ostringstream out;
      out << "ps_builtin_" << minMaxName << "("
          << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ", "
          << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
      return out.str();
    }
    std::string absSignName;
    if (getBuiltinAbsSignName(expr, absSignName, allowMathBare) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "ps_builtin_" << absSignName << "("
          << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
      return out.str();
    }
    std::string saturateName;
    if (getBuiltinSaturateName(expr, saturateName, allowMathBare) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "ps_builtin_" << saturateName << "("
          << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
          << ")";
      return out.str();
    }
    std::string mathName;
    if (getBuiltinMathName(expr, mathName, allowMathBare)) {
      std::ostringstream out;
      out << "ps_builtin_" << mathName << "(";
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i > 0) {
          out << ", ";
        }
        out << emitExpr(expr.args[i], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare);
      }
      out << ")";
      return out.str();
    }
    std::string convertName;
    if (getBuiltinConvertName(expr, convertName) && expr.templateArgs.size() == 1 && expr.args.size() == 1) {
      std::string targetType =
          bindingTypeToCpp(expr.templateArgs[0], expr.namespacePrefix, importAliases, structTypeMap);
      std::ostringstream out;
      ReturnKind argKind = inferPrimitiveReturnKind(expr.args.front(), localTypes, returnKinds, allowMathBare);
      ReturnKind targetKind = returnKindForTypeName(normalizeBindingTypeName(expr.templateArgs.front()));
      const bool argIsFloat = argKind == ReturnKind::Float32 || argKind == ReturnKind::Float64;
      const bool targetIsInt = targetKind == ReturnKind::Int || targetKind == ReturnKind::Int64 ||
                               targetKind == ReturnKind::UInt64;
      if (argIsFloat && targetIsInt) {
        out << "ps_convert_float_to_int<" << targetType << ">("
            << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                        resultInfos, returnStructs, allowMathBare)
            << ")";
      } else {
        out << "static_cast<" << targetType << ">("
            << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                        resultInfos, returnStructs, allowMathBare)
            << ")";
      }
      return out.str();
    }
    char pointerOp = '\0';
    if (getBuiltinPointerOperator(expr, pointerOp) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "(" << pointerOp
          << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
      return out.str();
    }
    std::string collection;
    if (getBuiltinCollectionName(expr, collection)) {
      if ((collection == "array" || collection == "vector") && expr.templateArgs.size() == 1) {
        std::ostringstream out;
        const std::string elemType =
            bindingTypeToCpp(expr.templateArgs[0], expr.namespacePrefix, importAliases, structTypeMap);
        out << "std::vector<" << elemType << ">{";
        for (size_t i = 0; i < expr.args.size(); ++i) {
          if (i > 0) {
            out << ", ";
          }
          out << emitExpr(expr.args[i], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare);
        }
        out << "}";
        return out.str();
      }
      if (collection == "map" && expr.templateArgs.size() == 2) {
        std::ostringstream out;
        const std::string keyType =
            bindingTypeToCpp(expr.templateArgs[0], expr.namespacePrefix, importAliases, structTypeMap);
        const std::string valueType =
            bindingTypeToCpp(expr.templateArgs[1], expr.namespacePrefix, importAliases, structTypeMap);
        out << "std::unordered_map<" << keyType << ", " << valueType << ">{";
        for (size_t i = 0; i + 1 < expr.args.size(); i += 2) {
          if (i > 0) {
            out << ", ";
          }
          out << "{"
              << emitExpr(expr.args[i], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
              << ", "
              << emitExpr(expr.args[i + 1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
              << "}";
        }
        out << "}";
        return out.str();
      }
    }
    if (isBuiltinAssign(expr, nameMap) && expr.args.size() == 2) {
      std::ostringstream out;
      out << "("
          << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << " = "
          << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
      return out.str();
    }
    if (!expr.isMethodCall && expr.name.find('/') == std::string::npos) {
      auto localIt = localTypes.find(expr.name);
      if (localIt != localTypes.end() && localIt->second.typeName == "lambda") {
        std::ostringstream out;
        out << expr.name << "(";
        for (size_t i = 0; i < expr.args.size(); ++i) {
          if (i > 0) {
            out << ", ";
          }
          out << emitExpr(expr.args[i],
                          nameMap,
                          paramMap,
                          defMap,
                          structTypeMap,
                          importAliases,
                          localTypes,
                          returnKinds,
                          resultInfos,
                          returnStructs,
                          allowMathBare);
        }
        out << ")";
        return out.str();
      }
    }
    return "0";
  }
  std::ostringstream out;
  out << it->second << "(";
  std::vector<const Expr *> orderedArgs;
  auto paramIt = paramMap.find(resolvedFull);
  if (paramIt != paramMap.end()) {
    const auto &params = paramIt->second;
    size_t packedParamIndex = params.size();
    BindingInfo packedParamInfo;
    if (!params.empty()) {
      BindingInfo lastParamInfo = getBindingInfo(params.back());
      if (normalizeBindingTypeName(lastParamInfo.typeName) == "args") {
        packedParamIndex = params.size() - 1;
        packedParamInfo = lastParamInfo;
      }
    }
    if (packedParamIndex < params.size()) {
      std::vector<const Expr *> fixedArgs(packedParamIndex, nullptr);
      std::vector<const Expr *> packedArgs;
      bool canPackArgs = true;
      size_t positionalIndex = 0;
      for (size_t i = 0; i < expr.args.size(); ++i) {
        const Expr &arg = expr.args[i];
        if (i < expr.argNames.size() && expr.argNames[i].has_value()) {
          const std::string &name = *expr.argNames[i];
          size_t matchIndex = packedParamIndex;
          for (size_t p = 0; p < packedParamIndex; ++p) {
            if (params[p].name == name) {
              matchIndex = p;
              break;
            }
          }
          if (matchIndex >= packedParamIndex) {
            canPackArgs = false;
            break;
          }
          fixedArgs[matchIndex] = &arg;
          continue;
        }
        while (positionalIndex < fixedArgs.size() && fixedArgs[positionalIndex] != nullptr) {
          ++positionalIndex;
        }
        if (positionalIndex < fixedArgs.size()) {
          fixedArgs[positionalIndex] = &arg;
          ++positionalIndex;
          continue;
        }
        packedArgs.push_back(&arg);
      }
      if (canPackArgs) {
        for (size_t i = 0; i < fixedArgs.size(); ++i) {
          if (fixedArgs[i] != nullptr) {
            continue;
          }
          if (!params[i].args.empty()) {
            fixedArgs[i] = &params[i].args.front();
            continue;
          }
          canPackArgs = false;
          break;
        }
      }
      if (canPackArgs) {
        auto emitRenderedArg = [&](const std::string &argText, bool &needComma) {
          if (needComma) {
            out << ", ";
          }
          out << argText;
          needComma = true;
        };
        bool needComma = false;
        for (const Expr *fixedArg : fixedArgs) {
          emitRenderedArg(emitExpr(*fixedArg,
                                   nameMap,
                                   paramMap,
                                   defMap,
                                   structTypeMap,
                                   importAliases,
                                   localTypes,
                                   returnKinds,
                                   resultInfos,
                                   returnStructs,
                                   allowMathBare),
                          needComma);
        }

        std::string packElementType =
            bindingTypeToCpp(packedParamInfo.typeTemplateArg,
                             params.back().namespacePrefix,
                             importAliases,
                             structTypeMap);
        if (packElementType.empty()) {
          packElementType = "int";
        }

        std::ostringstream packOut;
        if (packedArgs.empty()) {
          packOut << "std::vector<" << packElementType << ">{}";
        } else {
          const Expr *spreadArg = packedArgs.back()->isSpread ? packedArgs.back() : nullptr;
          const size_t explicitCount = spreadArg ? packedArgs.size() - 1 : packedArgs.size();
          if (spreadArg != nullptr && explicitCount == 0) {
            packOut << emitExpr(*spreadArg,
                                nameMap,
                                paramMap,
                                defMap,
                                structTypeMap,
                                importAliases,
                                localTypes,
                                returnKinds,
                                resultInfos,
                                returnStructs,
                                allowMathBare);
          } else {
            if (spreadArg != nullptr) {
              packOut << "ps_args_concat(";
            }
            packOut << "std::vector<" << packElementType << ">{";
            for (size_t i = 0; i < explicitCount; ++i) {
              if (i > 0) {
                packOut << ", ";
              }
              packOut << emitExpr(*packedArgs[i],
                                  nameMap,
                                  paramMap,
                                  defMap,
                                  structTypeMap,
                                  importAliases,
                                  localTypes,
                                  returnKinds,
                                  resultInfos,
                                  returnStructs,
                                  allowMathBare);
            }
            packOut << "}";
            if (spreadArg != nullptr) {
              packOut << ", "
                      << emitExpr(*spreadArg,
                                  nameMap,
                                  paramMap,
                                  defMap,
                                  structTypeMap,
                                  importAliases,
                                  localTypes,
                                  returnKinds,
                                  resultInfos,
                                  returnStructs,
                                  allowMathBare)
                      << ")";
            }
          }
        }
        emitRenderedArg(packOut.str(), needComma);
        out << ")";
        return out.str();
      }
    }
    orderedArgs = orderCallArguments(expr, resolvedFull, params, localTypes);
  } else {
    for (const auto &arg : expr.args) {
      orderedArgs.push_back(&arg);
    }
  }
  for (size_t i = 0; i < orderedArgs.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << emitExpr(*orderedArgs[i], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare);
  }
  out << ")";
  return out.str();
}
