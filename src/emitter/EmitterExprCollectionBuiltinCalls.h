    if (isNoHelperExplicitVectorCountCapacityCallFallback(expr)) {
      return emitMissingExplicitVectorCountCapacityCall(expr);
    }
    if (isNoHelperExplicitVectorAccessCallFallback(expr)) {
      return emitMissingExplicitVectorAccessCall(expr);
    }
    if (isNoHelperExplicitMapAccessCallFallback(expr)) {
      return emitMissingExplicitMapAccessCall(expr);
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
    if (expr.isMethodCall &&
        (isExplicitStdlibVectorHelper(expr.name, "count") ||
         resolveExprPath(expr) == "/std/collections/vector/count") &&
        expr.args.size() == 1) {
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
