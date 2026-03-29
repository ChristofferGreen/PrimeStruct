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
