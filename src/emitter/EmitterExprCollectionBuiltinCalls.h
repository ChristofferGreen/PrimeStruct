    if (isUnqualifiedCollectionBuiltinName(expr, "slice") && expr.args.size() == 3 &&
        resolvedTypePathForTarget(expr.args.front()) == "/array") {
      std::ostringstream out;
      out << "ps_array_slice("
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
          << ", "
          << emitExpr(expr.args[2],
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
    if (isUnqualifiedCollectionBuiltinName(expr, "count") && expr.args.size() == 1 &&
        isResolvedArrayLikeTarget(expr.args.front())) {
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
    if (isUnqualifiedCollectionBuiltinName(expr, "count") && expr.args.size() == 1 &&
        isResolvedStringTarget(expr.args.front())) {
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
