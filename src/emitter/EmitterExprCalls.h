  if (expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
      expr.args.front().name == "Result" && expr.name == "ok") {
    if (expr.args.size() == 1) {
      return "static_cast<uint32_t>(0)";
    }
    if (expr.args.size() == 2) {
      std::ostringstream out;
      out << "ps_result_pack(0u, static_cast<uint32_t>("
          << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds,
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
        emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds,
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
        emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds,
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
        emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds,
                 resultInfos, returnStructs, allowMathBare);
    std::string lambdaExpr =
        emitExpr(expr.args[2], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds,
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
        emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds,
                 resultInfos, returnStructs, allowMathBare);
    std::string lambdaExpr =
        emitExpr(expr.args[2], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds,
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
        emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds,
                 resultInfos, returnStructs, allowMathBare);
    std::string rightExpr =
        emitExpr(expr.args[2], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds,
                 resultInfos, returnStructs, allowMathBare);
    std::string lambdaExpr =
        emitExpr(expr.args[3], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds,
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
          emitExpr(expr.args.front(), nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds,
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
  auto resolvedTypePathForResolvedCall = [&](const std::string &resolvedPath) -> std::string {
    auto structIt = returnStructs.find(resolvedPath);
    if (structIt != returnStructs.end()) {
      std::string normalized = normalizedTypePath(structIt->second);
      if (!normalized.empty()) {
        return normalized;
      }
    }
    auto kindIt = returnKinds.find(resolvedPath);
    if (kindIt == returnKinds.end()) {
      return "";
    }
    if (kindIt->second == ReturnKind::String) {
      return "/string";
    }
    if (kindIt->second == ReturnKind::Array) {
      return "/array";
    }
    return "";
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
      return resolvedTypePathForResolvedCall(resolveExprPath(targetExpr));
    }
    std::string methodPath;
    if (!resolveMethodCallPath(
            targetExpr, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, methodPath)) {
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
    return normalized == std::string("vector/") + helper ||
           normalized == std::string("std/collections/vector/") + helper;
  };
  const std::string resolvedFull = preferVectorStdlibHelperPath(full, nameMap);
  auto it = nameMap.find(resolvedFull);
  if (it == nameMap.end()) {
    if (isVectorBuiltinName(expr, "count") && expr.args.size() == 1 && isResolvedMapTarget(expr.args.front())) {
      std::ostringstream out;
      out << "ps_map_count("
          << emitExpr(expr.args.front(),
                      nameMap,
                      paramMap,
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
          emitExpr(expr.args.front(), nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds,
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
          emitExpr(expr.args.front(), nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds,
                   resultInfos, returnStructs, allowMathBare);
      if (expr.name == "write" || expr.name == "write_line") {
        out << "ps_file_" << expr.name << "(" << receiver;
        for (size_t i = 1; i < expr.args.size(); ++i) {
          out << ", "
              << emitExpr(expr.args[i], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds,
                          resultInfos, returnStructs, allowMathBare);
        }
        out << ")";
        return out.str();
      }
      if (expr.name == "write_byte" && expr.args.size() == 2) {
        out << "ps_file_write_byte(" << receiver << ", "
            << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds,
                        resultInfos, returnStructs, allowMathBare)
            << ")";
        return out.str();
      }
      if (expr.name == "write_bytes" && expr.args.size() == 2) {
        out << "ps_file_write_bytes(" << receiver << ", "
            << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds,
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
          << emitExpr(expr.args.front(), nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds,
                      resultInfos, returnStructs, allowMathBare)
          << ")";
      return out.str();
    }
    if (isSimpleCallName(expr, "at") && expr.args.size() == 2) {
      std::ostringstream out;
      const size_t receiverIndex = pickAccessReceiverIndex();
      const size_t indexIndex = receiverIndex == 0 ? 1 : 0;
      const Expr &target = expr.args[receiverIndex];
      if (isResolvedStringTarget(target)) {
        out << "ps_string_at("
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[indexIndex],
                        nameMap,
                        paramMap,
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
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[indexIndex],
                        nameMap,
                        paramMap,
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
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[indexIndex],
                        nameMap,
                        paramMap,
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
    if (isSimpleCallName(expr, "at_unsafe") && expr.args.size() == 2) {
      std::ostringstream out;
      const size_t receiverIndex = pickAccessReceiverIndex();
      const size_t indexIndex = receiverIndex == 0 ? 1 : 0;
      const Expr &target = expr.args[receiverIndex];
      if (isResolvedStringTarget(target)) {
        out << "ps_string_at_unsafe("
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[indexIndex],
                        nameMap,
                        paramMap,
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
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[indexIndex],
                        nameMap,
                        paramMap,
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
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[indexIndex],
                        nameMap,
                        paramMap,
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
    char op = '\0';
    if (getBuiltinOperator(expr, op) && expr.args.size() == 2) {
      std::ostringstream out;
      if ((op == '+' || op == '-') && isPointerExpr(expr.args[0])) {
        out << "ps_pointer_add("
            << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ", ";
        if (op == '-') {
          out << "-("
              << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
              << ")";
        } else {
          out << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare);
        }
        out << ")";
        return out.str();
      }
      out << "("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << " "
          << op << " "
          << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
      return out.str();
    }
    if (isBuiltinNegate(expr) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "(-"
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
      return out.str();
    }
    if (!expr.isMethodCall && isSimpleCallName(expr, "move") && expr.args.size() == 1) {
      std::ostringstream out;
      out << "std::move("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
          << ")";
      return out.str();
    }
    std::string mutateName;
    if (getBuiltinMutationName(expr, mutateName) && expr.args.size() == 1) {
      std::ostringstream out;
      const char *op = mutateName == "increment" ? "++" : "--";
      out << "(" << op
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
      return out.str();
    }
    const char *cmp = nullptr;
    if (getBuiltinComparison(expr, cmp)) {
      std::ostringstream out;
      if (expr.args.size() == 1) {
        out << "("
            << cmp
            << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ")";
        return out.str();
      }
      if (expr.args.size() == 2) {
        out << "("
            << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << " "
            << cmp << " "
            << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ")";
        return out.str();
      }
    }
    if (isBuiltinClamp(expr, allowMathBare) && expr.args.size() == 3) {
      std::ostringstream out;
      out << "ps_builtin_clamp("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ", "
          << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ", "
          << emitExpr(expr.args[2], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
          << ")";
      return out.str();
    }
    std::string minMaxName;
    if (getBuiltinMinMaxName(expr, minMaxName, allowMathBare) && expr.args.size() == 2) {
      std::ostringstream out;
      out << "ps_builtin_" << minMaxName << "("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ", "
          << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
      return out.str();
    }
    std::string absSignName;
    if (getBuiltinAbsSignName(expr, absSignName, allowMathBare) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "ps_builtin_" << absSignName << "("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
      return out.str();
    }
    std::string saturateName;
    if (getBuiltinSaturateName(expr, saturateName, allowMathBare) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "ps_builtin_" << saturateName << "("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
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
        out << emitExpr(expr.args[i], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare);
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
            << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds,
                        resultInfos, returnStructs, allowMathBare)
            << ")";
      } else {
        out << "static_cast<" << targetType << ">("
            << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds,
                        resultInfos, returnStructs, allowMathBare)
            << ")";
      }
      return out.str();
    }
    char pointerOp = '\0';
    if (getBuiltinPointerOperator(expr, pointerOp) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "(" << pointerOp
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
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
          out << emitExpr(expr.args[i], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare);
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
              << emitExpr(expr.args[i], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
              << ", "
              << emitExpr(expr.args[i + 1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
              << "}";
        }
        out << "}";
        return out.str();
      }
    }
    if (isBuiltinAssign(expr, nameMap) && expr.args.size() == 2) {
      std::ostringstream out;
      out << "("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << " = "
          << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
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
    orderedArgs = orderCallArguments(expr, paramIt->second);
  } else {
    for (const auto &arg : expr.args) {
      orderedArgs.push_back(&arg);
    }
  }
  for (size_t i = 0; i < orderedArgs.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << emitExpr(*orderedArgs[i], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare);
  }
  out << ")";
  return out.str();
}
