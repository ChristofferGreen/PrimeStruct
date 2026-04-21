#include "EmitterExprResultCalls.h"
#include "EmitterExprCollectionTypeHelpers.h"
#include "EmitterExprCollectionFallbackHelpers.h"
  std::string resolvedFull = full;
  if (!isExplicitVectorCountCapacityDirectCall(expr) &&
      !isExplicitVectorAccessAliasDirectCall(expr)) {
    resolvedFull = preferStructReturningCollectionHelperPath(full);
  }
  auto it = nameMap.find(resolvedFull);
  if (it == nameMap.end()) {
    #include "EmitterExprCollectionBuiltinCalls.h"
    #include "EmitterExprFileAccessCalls.h"
    #include "EmitterExprCollectionAccessAtCalls.h"
    auto rewriteMathArithmeticExpr = [&](const Expr &candidate, Expr &rewrittenExpr) {
      char candidateOp = '\0';
      if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || !getBuiltinOperator(candidate, candidateOp) ||
          candidate.args.size() != 2) {
        return false;
      }
      auto resolvedMathTypeForTarget = [&](const Expr &targetExpr) -> std::string {
        if (targetExpr.kind == Expr::Kind::Name) {
          auto localIt = localTypes.find(targetExpr.name);
          if (localIt != localTypes.end()) {
            std::string localType = normalizeBindingTypeName(localIt->second.typeName);
            if (!localType.empty() && localType.front() == '/') {
              return normalizedTypePath(localType);
            }
            auto importIt = importAliases.find(localType);
            if (importIt != importAliases.end()) {
              return normalizedTypePath(importIt->second);
            }
          }
        }
        return resolvedTypePathForTarget(targetExpr);
      };
      auto matrixHelperBaseForType = [&](const std::string &typePath) -> std::string {
        if (typePath == "/std/math/Mat2") {
          return "/std/math/mat2";
        }
        if (typePath == "/std/math/Mat3") {
          return "/std/math/mat3";
        }
        if (typePath == "/std/math/Mat4") {
          return "/std/math/mat4";
        }
        return "";
      };
      auto quaternionHelperBaseForType = [&](const std::string &typePath) -> std::string {
        if (typePath == "/std/math/Quat") {
          return "/std/math/quat";
        }
        return "";
      };
      auto isNumericScalarTarget = [&](const Expr &targetExpr) {
        ReturnKind kind = inferPrimitiveReturnKind(targetExpr, localTypes, returnKinds, allowMathBare);
        return kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
               kind == ReturnKind::Float32 || kind == ReturnKind::Float64;
      };
      auto rewriteHelperCall = [&](const std::string &helperPath) {
        rewrittenExpr = candidate;
        rewrittenExpr.name = helperPath;
        rewrittenExpr.namespacePrefix.clear();
        rewrittenExpr.isMethodCall = false;
        rewrittenExpr.isFieldAccess = false;
        return true;
      };
      const std::string leftType = resolvedMathTypeForTarget(candidate.args[0]);
      const std::string rightType = resolvedMathTypeForTarget(candidate.args[1]);
      const std::string leftMatrixBase = matrixHelperBaseForType(leftType);
      const std::string rightMatrixBase = matrixHelperBaseForType(rightType);
      const std::string leftQuaternionBase = quaternionHelperBaseForType(leftType);
      const std::string rightQuaternionBase = quaternionHelperBaseForType(rightType);
      if (candidateOp == '+' && !leftMatrixBase.empty() && leftType == rightType) {
        return rewriteHelperCall(leftMatrixBase + "_add_internal");
      }
      if (candidateOp == '+' && !leftQuaternionBase.empty() && leftType == rightType) {
        return rewriteHelperCall(leftQuaternionBase + "_add_internal");
      }
      if (candidateOp == '-' && !leftMatrixBase.empty() && leftType == rightType) {
        return rewriteHelperCall(leftMatrixBase + "_sub_internal");
      }
      if (candidateOp == '-' && !leftQuaternionBase.empty() && leftType == rightType) {
        return rewriteHelperCall(leftQuaternionBase + "_sub_internal");
      }
      if (candidateOp == '*' && !leftMatrixBase.empty() && isNumericScalarTarget(candidate.args[1])) {
        return rewriteHelperCall(leftMatrixBase + "_scale_internal");
      }
      if (candidateOp == '*' && !leftQuaternionBase.empty() && isNumericScalarTarget(candidate.args[1])) {
        return rewriteHelperCall(leftQuaternionBase + "_scale_internal");
      }
      if (candidateOp == '*' && isNumericScalarTarget(candidate.args[0]) && !rightMatrixBase.empty()) {
        return rewriteHelperCall(rightMatrixBase + "_scale_left_internal");
      }
      if (candidateOp == '*' && isNumericScalarTarget(candidate.args[0]) && !rightQuaternionBase.empty()) {
        return rewriteHelperCall(rightQuaternionBase + "_scale_left_internal");
      }
      if (candidateOp == '/' && !leftMatrixBase.empty() && isNumericScalarTarget(candidate.args[1])) {
        return rewriteHelperCall(leftMatrixBase + "_div_scalar_internal");
      }
      if (candidateOp == '/' && !leftQuaternionBase.empty() && isNumericScalarTarget(candidate.args[1])) {
        return rewriteHelperCall(leftQuaternionBase + "_div_scalar_internal");
      }
      if (leftType == "/std/math/Mat2" && rightType == "/std/math/Vec2") {
        return rewriteHelperCall("/std/math/mat2_mul_vec2_internal");
      }
      if (leftType == "/std/math/Mat3" && rightType == "/std/math/Vec3") {
        return rewriteHelperCall("/std/math/mat3_mul_vec3_internal");
      }
      if (leftType == "/std/math/Mat4" && rightType == "/std/math/Vec4") {
        return rewriteHelperCall("/std/math/mat4_mul_vec4_internal");
      }
      if (leftType == "/std/math/Mat2" && rightType == "/std/math/Mat2") {
        return rewriteHelperCall("/std/math/mat2_mul_mat2_internal");
      }
      if (leftType == "/std/math/Mat3" && rightType == "/std/math/Mat3") {
        return rewriteHelperCall("/std/math/mat3_mul_mat3_internal");
      }
      if (leftType == "/std/math/Mat4" && rightType == "/std/math/Mat4") {
        return rewriteHelperCall("/std/math/mat4_mul_mat4_internal");
      }
      if (leftType == "/std/math/Quat" && rightType == "/std/math/Quat") {
        return rewriteHelperCall("/std/math/quat_multiply_internal");
      }
      if (leftType == "/std/math/Quat" && rightType == "/std/math/Vec3") {
        return rewriteHelperCall("/std/math/quat_rotate_vec3_internal");
      }
      return false;
    };
    Expr rewrittenMathArithmeticExpr;
    if (rewriteMathArithmeticExpr(expr, rewrittenMathArithmeticExpr)) {
      return emitExpr(rewrittenMathArithmeticExpr,
                      nameMap,
                      paramMap,
                      defMap,
                      structTypeMap,
                      importAliases,
                      localTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare);
    }
    char op = '\0';
    if (getBuiltinOperator(expr, op) && expr.args.size() == 2) {
      std::ostringstream out;
      if ((op == '+' || op == '-') && isPointerExpr(expr.args[0])) {
        out << "ps_pointer_add("
            << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ", ";
        if (op == '-') {
          out << "-("
              << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
              << ")";
        } else {
          out << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare);
        }
        out << ")";
        return out.str();
      }
      out << "("
          << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << " "
          << op << " "
          << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
      return out.str();
    }
    if (isBuiltinNegate(expr) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "(-"
          << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
      return out.str();
    }
    if (!expr.isMethodCall && isSimpleCallName(expr, "move") && expr.args.size() == 1) {
      std::ostringstream out;
      out << "std::move("
          << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
          << ")";
      return out.str();
    }
    std::string mutateName;
    if (getBuiltinMutationName(expr, mutateName) && expr.args.size() == 1) {
      std::ostringstream out;
      const char *op = mutateName == "increment" ? "++" : "--";
      out << "(" << op
          << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
      return out.str();
    }
    const char *cmp = nullptr;
    if (getBuiltinComparison(expr, cmp)) {
      std::ostringstream out;
      if (expr.args.size() == 1) {
        out << "("
            << cmp
            << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ")";
        return out.str();
      }
      if (expr.args.size() == 2) {
        out << "("
            << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << " "
            << cmp << " "
            << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << ")";
        return out.str();
      }
    }
    if (isBuiltinClamp(expr, allowMathBare) && expr.args.size() == 3) {
      std::ostringstream out;
      out << "ps_builtin_clamp("
          << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ", "
          << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ", "
          << emitExpr(expr.args[2], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
          << ")";
      return out.str();
    }
    std::string minMaxName;
    if (getBuiltinMinMaxName(expr, minMaxName, allowMathBare) && expr.args.size() == 2) {
      std::ostringstream out;
      out << "ps_builtin_" << minMaxName << "("
          << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ", "
          << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
      return out.str();
    }
    std::string absSignName;
    if (getBuiltinAbsSignName(expr, absSignName, allowMathBare) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "ps_builtin_" << absSignName << "("
          << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
      return out.str();
    }
    std::string saturateName;
    if (getBuiltinSaturateName(expr, saturateName, allowMathBare) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "ps_builtin_" << saturateName << "("
          << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
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
        out << emitExpr(expr.args[i], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare);
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
            << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                        resultInfos, returnStructs, allowMathBare)
            << ")";
      } else {
        out << "static_cast<" << targetType << ">("
            << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds,
                        resultInfos, returnStructs, allowMathBare)
            << ")";
      }
      return out.str();
    }
    std::string memoryName;
    if (getBuiltinMemoryName(expr, memoryName)) {
      std::ostringstream out;
      if (memoryName == "alloc" && expr.templateArgs.size() == 1 && expr.args.size() == 1) {
        const std::string targetType =
            bindingTypeToCpp(expr.templateArgs[0], expr.namespacePrefix, importAliases, structTypeMap);
        out << "ps_heap_alloc<" << targetType << ">("
            << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes,
                        returnKinds, resultInfos, returnStructs, allowMathBare)
            << ")";
        return out.str();
      }
      if (memoryName == "free" && expr.args.size() == 1) {
        out << "ps_heap_free("
            << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes,
                        returnKinds, resultInfos, returnStructs, allowMathBare)
            << ")";
        return out.str();
      }
      if (memoryName == "realloc" && expr.args.size() == 2) {
        out << "ps_heap_realloc("
            << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes,
                        returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes,
                        returnKinds, resultInfos, returnStructs, allowMathBare)
            << ")";
        return out.str();
      }
      if (memoryName == "at" && expr.args.size() == 3) {
        out << "ps_heap_at("
            << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes,
                        returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes,
                        returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[2], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes,
                        returnKinds, resultInfos, returnStructs, allowMathBare)
            << ")";
        return out.str();
      }
      if (memoryName == "at_unsafe" && expr.args.size() == 2) {
        out << "ps_heap_at_unsafe("
            << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes,
                        returnKinds, resultInfos, returnStructs, allowMathBare)
            << ", "
            << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes,
                        returnKinds, resultInfos, returnStructs, allowMathBare)
            << ")";
        return out.str();
      }
      if (memoryName == "reinterpret" && expr.templateArgs.size() == 1 && expr.args.size() == 1) {
        const std::string targetType =
            bindingTypeToCpp(expr.templateArgs[0], expr.namespacePrefix, importAliases, structTypeMap);
        out << "ps_heap_reinterpret<" << targetType << ">("
            << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes,
                        returnKinds, resultInfos, returnStructs, allowMathBare)
            << ")";
        return out.str();
      }
    }
    char pointerOp = '\0';
    if (getBuiltinPointerOperator(expr, pointerOp) && expr.args.size() == 1) {
      std::ostringstream out;
      const std::string valueExpr =
          emitExpr(expr.args[0],
                   nameMap,
                   paramMap,
                   defMap,
                   structTypeMap,
                   importAliases,
                   localTypes,
                   returnKinds,
                   resultInfos,
                   returnStructs,
                   allowMathBare);
      if (pointerOp == '*') {
        out << "ps_deref(" << valueExpr << ")";
      } else {
        out << "(" << pointerOp << valueExpr << ")";
      }
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
          out << emitExpr(expr.args[i], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare);
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
              << emitExpr(expr.args[i], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
              << ", "
              << emitExpr(expr.args[i + 1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
              << "}";
        }
        out << "}";
        return out.str();
      }
    }
    if (isBuiltinAssign(expr, nameMap) && expr.args.size() == 2) {
      std::ostringstream out;
      out << "("
          << emitExpr(expr.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << " = "
          << emitExpr(expr.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) << ")";
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
                          defMap,
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
    const auto &params = paramIt->second;
    size_t packedParamIndex = params.size();
    BindingInfo packedParamInfo;
    if (!params.empty()) {
      BindingInfo lastParamInfo = getBindingInfo(params.back());
      if (normalizeBindingTypeName(lastParamInfo.typeName) == "args") {
        packedParamIndex = params.size() - 1;
        packedParamInfo = lastParamInfo;
      }
    }
    if (packedParamIndex < params.size()) {
      std::vector<const Expr *> fixedArgs(packedParamIndex, nullptr);
      std::vector<const Expr *> packedArgs;
      bool canPackArgs = true;
      size_t positionalIndex = 0;
      for (size_t i = 0; i < expr.args.size(); ++i) {
        const Expr &arg = expr.args[i];
        if (i < expr.argNames.size() && expr.argNames[i].has_value()) {
          const std::string &name = *expr.argNames[i];
          size_t matchIndex = packedParamIndex;
          for (size_t p = 0; p < packedParamIndex; ++p) {
            if (params[p].name == name) {
              matchIndex = p;
              break;
            }
          }
          if (matchIndex >= packedParamIndex) {
            canPackArgs = false;
            break;
          }
          fixedArgs[matchIndex] = &arg;
          continue;
        }
        while (positionalIndex < fixedArgs.size() && fixedArgs[positionalIndex] != nullptr) {
          ++positionalIndex;
        }
        if (positionalIndex < fixedArgs.size()) {
          fixedArgs[positionalIndex] = &arg;
          ++positionalIndex;
          continue;
        }
        packedArgs.push_back(&arg);
      }
      if (canPackArgs) {
        for (size_t i = 0; i < fixedArgs.size(); ++i) {
          if (fixedArgs[i] != nullptr) {
            continue;
          }
          if (!params[i].args.empty()) {
            fixedArgs[i] = &params[i].args.front();
            continue;
          }
          canPackArgs = false;
          break;
        }
      }
      if (canPackArgs) {
        auto emitRenderedArg = [&](const std::string &argText, bool &needComma) {
          if (needComma) {
            out << ", ";
          }
          out << argText;
          needComma = true;
        };
        bool needComma = false;
        for (const Expr *fixedArg : fixedArgs) {
          emitRenderedArg(emitExpr(*fixedArg,
                                   nameMap,
                                   paramMap,
                                   defMap,
                                   structTypeMap,
                                   importAliases,
                                   localTypes,
                                   returnKinds,
                                   resultInfos,
                                   returnStructs,
                                   allowMathBare),
                          needComma);
        }

        std::string packElementType =
            bindingTypeToCpp(packedParamInfo.typeTemplateArg,
                             params.back().namespacePrefix,
                             importAliases,
                             structTypeMap);
        if (packElementType.empty()) {
          packElementType = "int";
        }
        #include "EmitterExprPackedArgs.h"
        out << ")";
        return out.str();
      }
    }
    orderedArgs = orderCallArguments(expr, resolvedFull, params, localTypes);
  } else {
    for (const auto &arg : expr.args) {
      orderedArgs.push_back(&arg);
    }
  }
  for (size_t i = 0; i < orderedArgs.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << emitExpr(*orderedArgs[i], nameMap, paramMap, defMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare);
  }
  out << ")";
  return out.str();
}
