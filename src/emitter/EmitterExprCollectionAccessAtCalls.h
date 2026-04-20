    std::string builtinAccessName;
    if (getBuiltinArrayAccessNameLocal(expr, builtinAccessName) && builtinAccessName == "at" &&
        expr.args.size() == 2) {
      std::ostringstream out;
      const size_t receiverIndex = pickAccessReceiverIndex();
      const size_t indexIndex = receiverIndex == 0 ? 1 : 0;
      const Expr &target = expr.args[receiverIndex];
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
    if (getBuiltinArrayAccessNameLocal(expr, builtinAccessName) && builtinAccessName == "at_unsafe" &&
        expr.args.size() == 2) {
      std::ostringstream out;
      const size_t receiverIndex = pickAccessReceiverIndex();
      const size_t indexIndex = receiverIndex == 0 ? 1 : 0;
      const Expr &target = expr.args[receiverIndex];
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
