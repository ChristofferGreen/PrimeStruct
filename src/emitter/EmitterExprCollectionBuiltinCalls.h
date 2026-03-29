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
