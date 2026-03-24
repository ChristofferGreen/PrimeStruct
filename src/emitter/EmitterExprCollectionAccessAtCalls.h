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
      if (expr.isMethodCall && isNoHelperExplicitMapAccessMethodFallback(expr)) {
        return emitMissingExplicitMapAccessMethod(expr);
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
      if (expr.isMethodCall && isNoHelperExplicitMapAccessMethodFallback(expr)) {
        return emitMissingExplicitMapAccessMethod(expr);
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
