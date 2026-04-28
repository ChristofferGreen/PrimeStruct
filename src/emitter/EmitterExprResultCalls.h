  const std::string legacyPackedResultStatusCppType = legacyPackedResultCppType(false);
  const std::string legacyPackedResultValueCppType = legacyPackedResultCppType(true);
  if (expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
      expr.args.front().name == "Result" && expr.name == "ok") {
    if (expr.args.size() == 1) {
      return "static_cast<" + legacyPackedResultStatusCppType + ">(0)";
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
        resultInfo.hasValue ? "ps_result_error(" + argText + ")"
                            : "static_cast<" + legacyPackedResultStatusCppType + ">(" + argText + ")";
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
      return "static_cast<" + legacyPackedResultValueCppType + ">(0)";
    }
    ResultInfo resultInfo;
    if (!resolveResultExprInfo(expr.args[1], resultInfo) || !resultInfo.isResult || !resultInfo.hasValue) {
      return "static_cast<" + legacyPackedResultValueCppType + ">(0)";
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
    out << "([&]() -> " << legacyPackedResultValueCppType << " {";
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
      return "static_cast<" + legacyPackedResultValueCppType + ">(0)";
    }
    ResultInfo resultInfo;
    if (!resolveResultExprInfo(expr.args[1], resultInfo) || !resultInfo.isResult || !resultInfo.hasValue) {
      return "static_cast<" + legacyPackedResultValueCppType + ">(0)";
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
    out << "([&]() -> " << legacyPackedResultValueCppType << " {";
    out << " auto ps_result = " << resultExpr << ";";
    out << " uint32_t ps_err = ps_result_error(ps_result);";
    out << " if (ps_err != 0u) { return ps_result_pack(ps_err, 0u); }";
    out << " auto ps_value = static_cast<" << valueType << ">(ps_result_value(ps_result));";
    out << " auto ps_next = (" << lambdaExpr << ")(ps_value);";
    out << " return static_cast<" << legacyPackedResultValueCppType << ">(ps_next);";
    out << " }())";
    return out.str();
  }
  if (expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
      expr.args.front().name == "Result" && expr.name == "map2") {
    if (expr.args.size() != 4) {
      return "static_cast<" + legacyPackedResultValueCppType + ">(0)";
    }
    ResultInfo leftInfo;
    ResultInfo rightInfo;
    if (!resolveResultExprInfo(expr.args[1], leftInfo) || !leftInfo.isResult || !leftInfo.hasValue ||
        !resolveResultExprInfo(expr.args[2], rightInfo) || !rightInfo.isResult || !rightInfo.hasValue) {
      return "static_cast<" + legacyPackedResultValueCppType + ">(0)";
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
    out << "([&]() -> " << legacyPackedResultValueCppType << " {";
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
