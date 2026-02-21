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
  auto it = nameMap.find(full);
  if (it == nameMap.end()) {
    if (isMapCountCall(expr, localTypes)) {
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
    if (isArrayCountCall(expr, localTypes)) {
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
    if (isStringCountCall(expr, localTypes)) {
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
    if (isVectorCapacityCall(expr, localTypes)) {
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
    if (expr.name == "at" && expr.args.size() == 2) {
      std::ostringstream out;
      const Expr &target = expr.args[0];
      if (isStringValue(target, localTypes)) {
        out << "ps_string_at("
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[1],
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
      } else if (isMapValue(target, localTypes)) {
        out << "ps_map_at("
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[1],
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
            << emitExpr(expr.args[1],
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
    if (expr.name == "at_unsafe" && expr.args.size() == 2) {
      std::ostringstream out;
      const Expr &target = expr.args[0];
      if (isStringValue(target, localTypes)) {
        out << "ps_string_at_unsafe("
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[1],
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
      } else if (isMapValue(target, localTypes)) {
        out << "ps_map_at_unsafe("
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[1],
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
            << emitExpr(expr.args[1],
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
      out << "static_cast<" << targetType << ">("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
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
    return "0";
  }
  std::ostringstream out;
  out << it->second << "(";
  std::vector<const Expr *> orderedArgs;
  auto paramIt = paramMap.find(full);
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
