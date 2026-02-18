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
                      allowMathBare)
          << ")";
      return out.str();
    }
    if (expr.name == "at" && expr.args.size() == 2) {
      std::ostringstream out;
      const Expr &target = expr.args[0];
      if (isStringValue(target, localTypes)) {
        out << "ps_string_at("
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
            << ", "
            << emitExpr(expr.args[1],
                        nameMap,
                        paramMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        allowMathBare)
            << ")";
      } else if (isMapValue(target, localTypes)) {
        out << "ps_map_at("
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
            << ", "
            << emitExpr(expr.args[1],
                        nameMap,
                        paramMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        allowMathBare)
            << ")";
      } else {
        out << "ps_array_at("
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
            << ", "
            << emitExpr(expr.args[1],
                        nameMap,
                        paramMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
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
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
            << ", "
            << emitExpr(expr.args[1],
                        nameMap,
                        paramMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        allowMathBare)
            << ")";
      } else if (isMapValue(target, localTypes)) {
        out << "ps_map_at_unsafe("
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
            << ", "
            << emitExpr(expr.args[1],
                        nameMap,
                        paramMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
                        allowMathBare)
            << ")";
      } else {
        out << "ps_array_at_unsafe("
            << emitExpr(target, nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
            << ", "
            << emitExpr(expr.args[1],
                        nameMap,
                        paramMap,
                        structTypeMap,
                        importAliases,
                        localTypes,
                        returnKinds,
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
            << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ", ";
        if (op == '-') {
          out << "-("
              << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
              << ")";
        } else {
          out << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare);
        }
        out << ")";
        return out.str();
      }
      out << "("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << " "
          << op << " "
          << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ")";
      return out.str();
    }
    if (isBuiltinNegate(expr) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "(-"
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ")";
      return out.str();
    }
    std::string mutateName;
    if (getBuiltinMutationName(expr, mutateName) && expr.args.size() == 1) {
      std::ostringstream out;
      const char *op = mutateName == "increment" ? "++" : "--";
      out << "(" << op
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ")";
      return out.str();
    }
    const char *cmp = nullptr;
    if (getBuiltinComparison(expr, cmp)) {
      std::ostringstream out;
      if (expr.args.size() == 1) {
        out << "("
            << cmp
            << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
            << ")";
        return out.str();
      }
      if (expr.args.size() == 2) {
        out << "("
            << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << " "
            << cmp << " "
            << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
            << ")";
        return out.str();
      }
    }
    if (isBuiltinClamp(expr, allowMathBare) && expr.args.size() == 3) {
      std::ostringstream out;
      out << "ps_builtin_clamp("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ", "
          << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ", "
          << emitExpr(expr.args[2], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
          << ")";
      return out.str();
    }
    std::string minMaxName;
    if (getBuiltinMinMaxName(expr, minMaxName, allowMathBare) && expr.args.size() == 2) {
      std::ostringstream out;
      out << "ps_builtin_" << minMaxName << "("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ", "
          << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ")";
      return out.str();
    }
    std::string absSignName;
    if (getBuiltinAbsSignName(expr, absSignName, allowMathBare) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "ps_builtin_" << absSignName << "("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ")";
      return out.str();
    }
    std::string saturateName;
    if (getBuiltinSaturateName(expr, saturateName, allowMathBare) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "ps_builtin_" << saturateName << "("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
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
        out << emitExpr(expr.args[i], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare);
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
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ")";
      return out.str();
    }
    char pointerOp = '\0';
    if (getBuiltinPointerOperator(expr, pointerOp) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "(" << pointerOp
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ")";
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
          out << emitExpr(expr.args[i], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare);
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
              << emitExpr(expr.args[i], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
              << ", "
              << emitExpr(expr.args[i + 1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare)
              << "}";
        }
        out << "}";
        return out.str();
      }
    }
    if (isBuiltinAssign(expr, nameMap) && expr.args.size() == 2) {
      std::ostringstream out;
      out << "("
          << emitExpr(expr.args[0], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << " = "
          << emitExpr(expr.args[1], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare) << ")";
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
    out << emitExpr(*orderedArgs[i], nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, allowMathBare);
  }
  out << ")";
  return out.str();
}
