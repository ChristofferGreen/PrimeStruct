  if (expr.kind == Expr::Kind::Literal) {
    if (!expr.isUnsigned && expr.intWidth == 32) {
      return std::to_string(static_cast<int64_t>(expr.literalValue));
    }
    if (expr.isUnsigned) {
      std::ostringstream out;
      out << "static_cast<uint64_t>(" << static_cast<uint64_t>(expr.literalValue) << ")";
      return out.str();
    }
    std::ostringstream out;
    out << "static_cast<int64_t>(" << static_cast<int64_t>(expr.literalValue) << ")";
    return out.str();
  }
  if (expr.kind == Expr::Kind::BoolLiteral) {
    return expr.boolValue ? "true" : "false";
  }
  if (expr.kind == Expr::Kind::FloatLiteral) {
    if (expr.floatWidth == 64) {
      return expr.floatValue;
    }
    std::string literal = expr.floatValue;
    if (literal.find('.') == std::string::npos && literal.find('e') == std::string::npos &&
        literal.find('E') == std::string::npos) {
      literal += ".0";
    }
    return literal + "f";
  }
  if (expr.kind == Expr::Kind::StringLiteral) {
    return "std::string_view(" + stripStringLiteralSuffix(expr.stringValue) + ")";
  }
  if (expr.kind == Expr::Kind::Name) {
    if (localTypes.count(expr.name) == 0 && isBuiltinMathConstantName(expr.name, allowMathBare)) {
      std::string constantName = expr.name;
      if (!constantName.empty() && constantName[0] == '/') {
        constantName.erase(0, 1);
      }
      if (constantName.rfind("math/", 0) == 0) {
        constantName.erase(0, 5);
      }
      return "ps_const_" + constantName;
    }
    return expr.name;
  }
  if (expr.kind == Expr::Kind::Call && expr.isFieldAccess && !expr.args.empty()) {
    return emitExpr(expr.args.front(), nameMap, paramMap, structTypeMap, importAliases, localTypes, returnKinds, resultInfos, returnStructs, allowMathBare) +
           "." + expr.name;
  }
  std::string full = resolveExprPath(expr);
  if (expr.isMethodCall && !isArrayCountCall(expr, localTypes) && !isMapCountCall(expr, localTypes) &&
      !isStringCountCall(expr, localTypes)) {
    std::string methodPath;
    if (resolveMethodCallPath(expr, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, methodPath)) {
      full = methodPath;
    }
  }
  if (!expr.isMethodCall && !expr.name.empty() && expr.name[0] != '/' && expr.name.find('/') == std::string::npos) {
    if (nameMap.count(full) == 0) {
      if (!expr.namespacePrefix.empty()) {
        auto importIt = importAliases.find(expr.name);
        if (importIt != importAliases.end()) {
          full = importIt->second;
        }
      } else {
        const std::string root = "/" + expr.name;
        if (nameMap.count(root) > 0) {
          full = root;
        } else {
          auto importIt = importAliases.find(expr.name);
          if (importIt != importAliases.end()) {
            full = importIt->second;
          }
        }
      }
    }
  }
  if (!expr.isMethodCall && isSimpleCallName(expr, "count") && expr.args.size() == 1 && nameMap.count(full) == 0 &&
      !isArrayCountCall(expr, localTypes) && !isMapCountCall(expr, localTypes) && !isStringCountCall(expr, localTypes)) {
    Expr methodExpr = expr;
    methodExpr.isMethodCall = true;
    std::string methodPath;
    if (resolveMethodCallPath(methodExpr, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, methodPath)) {
      full = methodPath;
    }
  }
  if (isBuiltinBlock(expr, nameMap) && expr.hasBodyArguments) {
    if (!expr.args.empty() || !expr.templateArgs.empty() || hasNamedArguments(expr.argNames)) {
      return "0";
    }
    if (expr.bodyArguments.empty()) {
      return "0";
    }
    std::unordered_map<std::string, BindingInfo> blockTypes = localTypes;
    std::ostringstream out;
    out << "([&]() { ";
    for (size_t i = 0; i < expr.bodyArguments.size(); ++i) {
      const Expr &stmt = expr.bodyArguments[i];
      const bool isLast = (i + 1 == expr.bodyArguments.size());
      if (!isLast && isReturnCall(stmt)) {
        if (stmt.args.size() == 1) {
          out << "return "
              << emitExpr(stmt.args.front(),
                          nameMap,
                          paramMap,
                          structTypeMap,
                          importAliases,
                          blockTypes,
                          returnKinds,
                          resultInfos,
                          returnStructs,
                          allowMathBare)
              << "; ";
        } else {
          out << "return 0; ";
        }
        break;
      }
      if (isLast) {
        if (stmt.isBinding) {
          out << "return 0; ";
          break;
        }
        const Expr *valueExpr = &stmt;
        if (isReturnCall(stmt)) {
          if (stmt.args.size() == 1) {
            valueExpr = &stmt.args.front();
          } else {
            out << "return 0; ";
            break;
          }
        }
        out << "return "
            << emitExpr(*valueExpr,
                        nameMap,
                        paramMap,
                        structTypeMap,
                        importAliases,
                        blockTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare)
            << "; ";
        break;
      }
      if (stmt.isBinding) {
        BindingInfo binding = getBindingInfo(stmt);
        const bool hasExplicitType = hasExplicitBindingTypeTransform(stmt);
        const bool lambdaInit = stmt.args.size() == 1 && stmt.args.front().isLambda;
        if (!hasExplicitType && stmt.args.size() == 1 && !lambdaInit) {
          ReturnKind initKind = inferPrimitiveReturnKind(stmt.args.front(), blockTypes, returnKinds, allowMathBare);
          std::string inferred = typeNameForReturnKind(initKind);
          if (!inferred.empty()) {
            binding.typeName = inferred;
            binding.typeTemplateArg.clear();
          }
        }
        blockTypes[stmt.name] = binding;
        const bool useAuto = !hasExplicitType && lambdaInit;
        if (useAuto) {
          out << (binding.isMutable ? "" : "const ") << "auto " << stmt.name;
          if (!stmt.args.empty()) {
            out << " = " << emitExpr(stmt.args.front(),
                                    nameMap,
                                    paramMap,
                                    structTypeMap,
                                    importAliases,
                                    blockTypes,
                                    returnKinds,
                                    resultInfos,
                                    returnStructs,
                                    allowMathBare);
          }
          out << "; ";
          continue;
        }
        bool needsConst = !binding.isMutable;
        const bool useRef =
            !binding.isMutable && !binding.isCopy && !stmt.args.empty() && isReferenceCandidate(binding);
        if (hasExplicitType) {
          std::string type = bindingTypeToCpp(binding, stmt.namespacePrefix, importAliases, structTypeMap);
          bool isReference = binding.typeName == "Reference";
          if (useRef) {
            if (type.rfind("const ", 0) != 0) {
              out << "const " << type << " & " << stmt.name;
            } else {
              out << type << " & " << stmt.name;
            }
          } else {
            out << (needsConst ? "const " : "") << type << " " << stmt.name;
          }
          if (!stmt.args.empty()) {
            if (isReference) {
              out << " = *(" << emitExpr(stmt.args.front(),
                                         nameMap,
                                         paramMap,
                                         structTypeMap,
                                         importAliases,
                                         blockTypes,
                                         returnKinds,
                                         resultInfos,
                                         returnStructs,
                                         allowMathBare)
                  << ")";
            } else {
              out << " = " << emitExpr(stmt.args.front(),
                                      nameMap,
                                      paramMap,
                                      structTypeMap,
                                      importAliases,
                                      blockTypes,
                                      returnKinds,
                                      resultInfos,
                                      returnStructs,
                                      allowMathBare);
            }
          }
          out << "; ";
        } else {
          if (useRef) {
            out << "const auto & " << stmt.name;
          } else {
            out << (needsConst ? "const " : "") << "auto " << stmt.name;
          }
          if (!stmt.args.empty()) {
            out << " = " << emitExpr(stmt.args.front(),
                                    nameMap,
                                    paramMap,
                                    structTypeMap,
                                    importAliases,
                                    blockTypes,
                                    returnKinds,
                                    resultInfos,
                                    returnStructs,
                                    allowMathBare);
          }
          out << "; ";
        }
        continue;
      }
      out << "(void)"
          << emitExpr(stmt, nameMap, paramMap, structTypeMap, importAliases, blockTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
          << "; ";
    }
    out << "}())";
    return out.str();
  }
  if ((expr.hasBodyArguments || !expr.bodyArguments.empty()) && !isBuiltinBlock(expr, nameMap)) {
    Expr callExpr = expr;
    callExpr.bodyArguments.clear();
    callExpr.hasBodyArguments = false;
    Expr blockExpr = expr;
    blockExpr.name = "block";
    blockExpr.args.clear();
    blockExpr.templateArgs.clear();
    blockExpr.argNames.clear();
    blockExpr.isMethodCall = false;
    blockExpr.isBinding = false;
    blockExpr.isLambda = false;
    blockExpr.hasBodyArguments = true;
    std::ostringstream out;
    out << "([&]() { ";
    out << "auto ps_call_value = "
        << emitExpr(callExpr,
                    nameMap,
                    paramMap,
                    structTypeMap,
                    importAliases,
                    localTypes,
                    returnKinds,
                    resultInfos,
                    returnStructs,
                    allowMathBare)
        << "; ";
    out << "(void)"
        << emitExpr(blockExpr,
                    nameMap,
                    paramMap,
                    structTypeMap,
                    importAliases,
                    localTypes,
                    returnKinds,
                    resultInfos,
                    returnStructs,
                    allowMathBare)
        << "; ";
    out << "return ps_call_value; }())";
    return out.str();
  }
  if (isBuiltinIf(expr, nameMap) && expr.args.size() == 3) {
    auto isIfBlockEnvelope = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
        return false;
      }
      if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
        return false;
      }
      if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
        return false;
      }
      return true;
    };
    auto emitBranchValueExpr =
        [&](const Expr &candidate, std::unordered_map<std::string, BindingInfo> branchTypes) -> std::string {
      if (!isIfBlockEnvelope(candidate)) {
        return emitExpr(candidate,
                        nameMap,
                        paramMap,
                        structTypeMap,
                        importAliases,
                        branchTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
                        allowMathBare);
      }
      if (candidate.bodyArguments.empty()) {
        return "0";
      }
      std::ostringstream out;
      out << "([&]() { ";
      for (size_t i = 0; i < candidate.bodyArguments.size(); ++i) {
        const Expr &stmt = candidate.bodyArguments[i];
        const bool isLast = (i + 1 == candidate.bodyArguments.size());
        if (!isLast && isReturnCall(stmt)) {
          if (stmt.args.size() == 1) {
            out << "return "
                << emitExpr(stmt.args.front(),
                            nameMap,
                            paramMap,
                            structTypeMap,
                            importAliases,
                            branchTypes,
                            returnKinds,
                            resultInfos,
                            returnStructs,
                            allowMathBare)
                << "; ";
          } else {
            out << "return 0; ";
          }
          break;
        }
        if (isLast) {
          if (stmt.isBinding) {
            out << "return 0; ";
            break;
          }
          const Expr *valueExpr = &stmt;
          if (isReturnCall(stmt)) {
            if (stmt.args.size() == 1) {
              valueExpr = &stmt.args.front();
            } else {
              out << "return 0; ";
              break;
            }
          }
          out << "return "
              << emitExpr(*valueExpr,
                          nameMap,
                          paramMap,
                          structTypeMap,
                          importAliases,
                          branchTypes,
                          returnKinds,
                          resultInfos,
                          returnStructs,
                          allowMathBare)
              << "; ";
          break;
        }
        if (stmt.isBinding) {
          BindingInfo binding = getBindingInfo(stmt);
          const bool hasExplicitType = hasExplicitBindingTypeTransform(stmt);
          const bool lambdaInit = stmt.args.size() == 1 && stmt.args.front().isLambda;
          if (!hasExplicitType && stmt.args.size() == 1 && !lambdaInit) {
            ReturnKind initKind = inferPrimitiveReturnKind(stmt.args.front(), branchTypes, returnKinds, allowMathBare);
            std::string inferred = typeNameForReturnKind(initKind);
            if (!inferred.empty()) {
              binding.typeName = inferred;
              binding.typeTemplateArg.clear();
            }
          }
          branchTypes[stmt.name] = binding;
          const bool useAuto = !hasExplicitType && lambdaInit;
          if (useAuto) {
            out << (binding.isMutable ? "" : "const ") << "auto " << stmt.name;
            if (!stmt.args.empty()) {
              out << " = " << emitExpr(stmt.args.front(),
                                      nameMap,
                                      paramMap,
                                      structTypeMap,
                                      importAliases,
                                      branchTypes,
                                      returnKinds,
                                      resultInfos,
                                      returnStructs,
                                      allowMathBare);
            }
            out << "; ";
            continue;
          }
          bool needsConst = !binding.isMutable;
          const bool useRef =
              !binding.isMutable && !binding.isCopy && !stmt.args.empty() && isReferenceCandidate(binding);
          if (hasExplicitType) {
            std::string type = bindingTypeToCpp(binding, stmt.namespacePrefix, importAliases, structTypeMap);
            bool isReference = binding.typeName == "Reference";
            if (useRef) {
              if (type.rfind("const ", 0) != 0) {
                out << "const " << type << " & " << stmt.name;
              } else {
                out << type << " & " << stmt.name;
              }
            } else {
              out << (needsConst ? "const " : "") << type << " " << stmt.name;
            }
            if (!stmt.args.empty()) {
            if (isReference) {
              out << " = *(" << emitExpr(stmt.args.front(),
                                         nameMap,
                                         paramMap,
                                         structTypeMap,
                                         importAliases,
                                         branchTypes,
                                         returnKinds,
                                         resultInfos,
                                         returnStructs,
                                         allowMathBare)
                  << ")";
            } else {
              out << " = " << emitExpr(stmt.args.front(),
                                      nameMap,
                                      paramMap,
                                      structTypeMap,
                                      importAliases,
                                      branchTypes,
                                      returnKinds,
                                      resultInfos,
                                      returnStructs,
                                      allowMathBare);
            }
          }
          out << "; ";
        } else {
          if (useRef) {
            out << "const auto & " << stmt.name;
          } else {
            out << (needsConst ? "const " : "") << "auto " << stmt.name;
          }
          if (!stmt.args.empty()) {
            out << " = " << emitExpr(stmt.args.front(),
                                    nameMap,
                                    paramMap,
                                    structTypeMap,
                                    importAliases,
                                    branchTypes,
                                    returnKinds,
                                    resultInfos,
                                    returnStructs,
                                    allowMathBare);
          }
          out << "; ";
        }
        continue;
      }
      out << "(void)"
          << emitExpr(stmt,
                      nameMap,
                      paramMap,
                      structTypeMap,
                      importAliases,
                      branchTypes,
                      returnKinds,
                      resultInfos,
                      returnStructs,
                      allowMathBare)
          << "; ";
    }
    out << "}())";
    return out.str();
  };
  std::ostringstream out;
    out << "("
        << emitExpr(expr.args[0],
                    nameMap,
                    paramMap,
                    structTypeMap,
                    importAliases,
                    localTypes,
                    returnKinds,
                    resultInfos,
                    returnStructs,
                    allowMathBare)
        << " ? "
        << emitBranchValueExpr(expr.args[1], localTypes) << " : " << emitBranchValueExpr(expr.args[2], localTypes)
        << ")";
    return out.str();
  }
