std::string Emitter::emitExpr(const Expr &expr,
                              const std::unordered_map<std::string, std::string> &nameMap,
                              const std::unordered_map<std::string, std::vector<Expr>> &paramMap,
                              const std::unordered_map<std::string, std::string> &structTypeMap,
                              const std::unordered_map<std::string, std::string> &importAliases,
                              const std::unordered_map<std::string, BindingInfo> &localTypes,
                              const std::unordered_map<std::string, ReturnKind> &returnKinds,
                              bool allowMathBare) const {
  std::function<bool(const Expr &)> isPointerExpr;
  isPointerExpr = [&](const Expr &candidate) -> bool {
    if (candidate.kind == Expr::Kind::Name) {
      auto it = localTypes.find(candidate.name);
      return it != localTypes.end() && it->second.typeName == "Pointer";
    }
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    if (isSimpleCallName(candidate, "location")) {
      return true;
    }
    char op = '\0';
    if (getBuiltinOperator(candidate, op) && candidate.args.size() == 2 && (op == '+' || op == '-')) {
      return isPointerExpr(candidate.args[0]) && !isPointerExpr(candidate.args[1]);
    }
    return false;
  };
  if (expr.isLambda) {
    struct LambdaCapture {
      std::string name;
      bool byRef = false;
    };
    auto splitCaptureTokens = [](const std::string &capture) {
      std::vector<std::string> tokens;
      std::istringstream stream(capture);
      std::string token;
      while (stream >> token) {
        tokens.push_back(token);
      }
      return tokens;
    };

    std::string captureAllToken;
    std::vector<LambdaCapture> explicitCaptures;
    explicitCaptures.reserve(expr.lambdaCaptures.size());
    for (const auto &capture : expr.lambdaCaptures) {
      std::vector<std::string> tokens = splitCaptureTokens(capture);
      if (tokens.empty()) {
        continue;
      }
      const std::string &last = tokens.back();
      if (last == "=" || last == "&") {
        if (captureAllToken.empty()) {
          captureAllToken = last;
        }
        continue;
      }
      bool byRef = false;
      for (const auto &token : tokens) {
        if (token == "ref") {
          byRef = true;
          break;
        }
      }
      explicitCaptures.push_back({last, byRef});
    }

    std::unordered_map<std::string, BindingInfo> lambdaTypes;
    lambdaTypes.reserve(localTypes.size() + expr.args.size());
    if (!captureAllToken.empty()) {
      for (const auto &entry : localTypes) {
        lambdaTypes.emplace(entry.first, entry.second);
      }
    } else {
      for (const auto &capture : explicitCaptures) {
        auto it = localTypes.find(capture.name);
        if (it != localTypes.end()) {
          lambdaTypes.emplace(it->first, it->second);
        }
      }
    }

    std::ostringstream out;
    out << "[";
    bool firstCapture = true;
    if (!captureAllToken.empty()) {
      out << captureAllToken;
      firstCapture = false;
    }
    for (const auto &capture : explicitCaptures) {
      if (!firstCapture) {
        out << ", ";
      }
      if (capture.byRef) {
        out << "&";
      }
      out << capture.name;
      firstCapture = false;
    }
    out << "](";
    for (size_t i = 0; i < expr.args.size(); ++i) {
      const Expr &param = expr.args[i];
      BindingInfo paramInfo = getBindingInfo(param);
      if (!hasExplicitBindingTypeTransform(param) && param.args.size() == 1) {
        ReturnKind initKind = inferPrimitiveReturnKind(param.args.front(), lambdaTypes, returnKinds, allowMathBare);
        std::string inferred = typeNameForReturnKind(initKind);
        if (!inferred.empty()) {
          paramInfo.typeName = inferred;
          paramInfo.typeTemplateArg.clear();
        }
      }
      const std::string typeNamespace = param.namespacePrefix.empty() ? expr.namespacePrefix : param.namespacePrefix;
      std::string paramType = bindingTypeToCpp(paramInfo, typeNamespace, importAliases, structTypeMap);
      const bool refCandidate = isReferenceCandidate(paramInfo);
      const bool passByRef = refCandidate && !paramInfo.isCopy;
      if (passByRef) {
        if (!paramInfo.isMutable && paramType.rfind("const ", 0) != 0) {
          out << "const ";
        }
        out << paramType << " & " << param.name;
      } else {
        bool needsConst = !paramInfo.isMutable;
        if (needsConst && paramType.rfind("const ", 0) == 0) {
          needsConst = false;
        }
        out << (needsConst ? "const " : "") << paramType << " " << param.name;
      }
      if (param.args.size() == 1) {
        out << " = "
            << emitExpr(param.args.front(),
                        nameMap,
                        paramMap,
                        structTypeMap,
                        importAliases,
                        lambdaTypes,
                        returnKinds,
                        allowMathBare);
      }
      if (i + 1 < expr.args.size()) {
        out << ", ";
      }
      lambdaTypes[param.name] = paramInfo;
    }
    out << ") { ";

    std::unordered_map<std::string, BindingInfo> bodyTypes = lambdaTypes;
    int repeatCounter = 0;
    std::function<void(const Expr &, std::unordered_map<std::string, BindingInfo> &)> emitLambdaStatement;
    emitLambdaStatement = [&](const Expr &stmt, std::unordered_map<std::string, BindingInfo> &activeTypes) {
      if (isReturnCall(stmt)) {
        out << "return";
        if (!stmt.args.empty()) {
          out << " "
              << emitExpr(stmt.args.front(),
                          nameMap,
                          paramMap,
                          structTypeMap,
                          importAliases,
                          activeTypes,
                          returnKinds,
                          allowMathBare);
        }
        out << "; ";
        return;
      }
      if (!stmt.isBinding && stmt.kind == Expr::Kind::Call) {
        PrintBuiltin printBuiltin;
        if (getPrintBuiltin(stmt, printBuiltin)) {
          const char *stream = (printBuiltin.target == PrintTarget::Err) ? "stderr" : "stdout";
          std::string argText = "\"\"";
          if (!stmt.args.empty()) {
            argText = emitExpr(stmt.args.front(),
                               nameMap,
                               paramMap,
                               structTypeMap,
                               importAliases,
                               activeTypes,
                               returnKinds,
                               allowMathBare);
          }
          out << "ps_print_value(" << stream << ", " << argText << ", "
              << (printBuiltin.newline ? "true" : "false") << "); ";
          return;
        }
      }
      if (stmt.isBinding) {
        BindingInfo binding = getBindingInfo(stmt);
        const bool hasExplicitType = hasExplicitBindingTypeTransform(stmt);
        const bool lambdaInit = stmt.args.size() == 1 && stmt.args.front().isLambda;
        if (!hasExplicitType && stmt.args.size() == 1 && !lambdaInit) {
          ReturnKind initKind = inferPrimitiveReturnKind(stmt.args.front(), activeTypes, returnKinds, allowMathBare);
          std::string inferred = typeNameForReturnKind(initKind);
          if (!inferred.empty()) {
            binding.typeName = inferred;
            binding.typeTemplateArg.clear();
          }
        }
        const bool useAuto = !hasExplicitType && lambdaInit;
        activeTypes[stmt.name] = binding;
        if (useAuto) {
          out << (binding.isMutable ? "" : "const ") << "auto " << stmt.name;
          if (!stmt.args.empty()) {
            out << " = "
                << emitExpr(stmt.args.front(),
                            nameMap,
                            paramMap,
                            structTypeMap,
                            importAliases,
                            activeTypes,
                            returnKinds,
                            allowMathBare);
          }
          out << "; ";
          return;
        }
        bool needsConst = !binding.isMutable;
        const bool useRef = !binding.isMutable && !binding.isCopy && !stmt.args.empty() && isReferenceCandidate(binding);
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
                                         activeTypes,
                                         returnKinds,
                                         allowMathBare)
                  << ")";
            } else {
              out << " = " << emitExpr(stmt.args.front(),
                                      nameMap,
                                      paramMap,
                                      structTypeMap,
                                      importAliases,
                                      activeTypes,
                                      returnKinds,
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
                                    activeTypes,
                                    returnKinds,
                                    allowMathBare);
          }
          out << "; ";
        }
        return;
      }
      if (isBuiltinIf(stmt, nameMap) && stmt.args.size() == 3) {
        const Expr &cond = stmt.args[0];
        const Expr &thenArg = stmt.args[1];
        const Expr &elseArg = stmt.args[2];
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
        out << "if ("
            << emitExpr(cond,
                        nameMap,
                        paramMap,
                        structTypeMap,
                        importAliases,
                        activeTypes,
                        returnKinds,
                        allowMathBare)
            << ") { ";
        {
          auto blockTypes = activeTypes;
          if (isIfBlockEnvelope(thenArg)) {
            for (const auto &bodyStmt : thenArg.bodyArguments) {
              emitLambdaStatement(bodyStmt, blockTypes);
            }
          } else {
            emitLambdaStatement(thenArg, blockTypes);
          }
        }
        out << "} else { ";
        {
          auto blockTypes = activeTypes;
          if (isIfBlockEnvelope(elseArg)) {
            for (const auto &bodyStmt : elseArg.bodyArguments) {
              emitLambdaStatement(bodyStmt, blockTypes);
            }
          } else {
            emitLambdaStatement(elseArg, blockTypes);
          }
        }
        out << "} ";
        return;
      }
      auto isLoopBlockEnvelope = [&](const Expr &candidate) -> bool {
        if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
          return false;
        }
        if (!candidate.args.empty() || !candidate.templateArgs.empty()) {
          return false;
        }
        if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
          return false;
        }
        return true;
      };
      if (stmt.kind == Expr::Kind::Call && isLoopCall(stmt) && stmt.args.size() == 2) {
        const int repeatIndex = repeatCounter++;
        const std::string endVar = "ps_loop_end_" + std::to_string(repeatIndex);
        const std::string indexVar = "ps_loop_i_" + std::to_string(repeatIndex);
        out << "{ ";
        std::string countExpr =
            emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare);
        ReturnKind countKind = inferPrimitiveReturnKind(stmt.args[0], activeTypes, returnKinds, allowMathBare);
        out << "auto " << endVar << " = " << countExpr << "; ";
        const std::string indexType = (countKind == ReturnKind::Bool) ? "int" : ("decltype(" + endVar + ")");
        out << "for (" << indexType << " " << indexVar << " = 0; " << indexVar << " < " << endVar << "; ++"
            << indexVar << ") { ";
        if (isLoopBlockEnvelope(stmt.args[1])) {
          auto blockTypes = activeTypes;
          for (const auto &bodyStmt : stmt.args[1].bodyArguments) {
            emitLambdaStatement(bodyStmt, blockTypes);
          }
        }
        out << "} ";
        out << "} ";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isWhileCall(stmt) && stmt.args.size() == 2 && isLoopBlockEnvelope(stmt.args[1])) {
        out << "while ("
            << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
            << ") { ";
        auto blockTypes = activeTypes;
        for (const auto &bodyStmt : stmt.args[1].bodyArguments) {
          emitLambdaStatement(bodyStmt, blockTypes);
        }
        out << "} ";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isForCall(stmt) && stmt.args.size() == 4 && isLoopBlockEnvelope(stmt.args[3])) {
        out << "{ ";
        auto loopTypes = activeTypes;
        emitLambdaStatement(stmt.args[0], loopTypes);
        out << "while ("
            << emitExpr(stmt.args[1], nameMap, paramMap, structTypeMap, importAliases, loopTypes, returnKinds, allowMathBare)
            << ") { ";
        auto blockTypes = loopTypes;
        for (const auto &bodyStmt : stmt.args[3].bodyArguments) {
          emitLambdaStatement(bodyStmt, blockTypes);
        }
        emitLambdaStatement(stmt.args[2], loopTypes);
        out << "} ";
        out << "} ";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isRepeatCall(stmt)) {
        const int repeatIndex = repeatCounter++;
        const std::string endVar = "ps_repeat_end_" + std::to_string(repeatIndex);
        const std::string indexVar = "ps_repeat_i_" + std::to_string(repeatIndex);
        out << "{ ";
        std::string countExpr = "\"\"";
        ReturnKind countKind = ReturnKind::Unknown;
        if (!stmt.args.empty()) {
          countExpr =
              emitExpr(stmt.args.front(), nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare);
          countKind = inferPrimitiveReturnKind(stmt.args.front(), activeTypes, returnKinds, allowMathBare);
        }
        out << "auto " << endVar << " = " << countExpr << "; ";
        const std::string indexType = (countKind == ReturnKind::Bool) ? "int" : ("decltype(" + endVar + ")");
        out << "for (" << indexType << " " << indexVar << " = 0; " << indexVar << " < " << endVar << "; ++"
            << indexVar << ") { ";
        auto blockTypes = activeTypes;
        for (const auto &bodyStmt : stmt.bodyArguments) {
          emitLambdaStatement(bodyStmt, blockTypes);
        }
        out << "} ";
        out << "} ";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isBuiltinBlock(stmt, nameMap) && (stmt.hasBodyArguments || !stmt.bodyArguments.empty())) {
        out << "{ ";
        auto blockTypes = activeTypes;
        for (const auto &bodyStmt : stmt.bodyArguments) {
          emitLambdaStatement(bodyStmt, blockTypes);
        }
        out << "} ";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && (stmt.hasBodyArguments || !stmt.bodyArguments.empty())) {
        Expr callExpr = stmt;
        callExpr.bodyArguments.clear();
        callExpr.hasBodyArguments = false;
        out << emitExpr(callExpr, nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
            << "; ";
        out << "{ ";
        auto blockTypes = activeTypes;
        for (const auto &bodyStmt : stmt.bodyArguments) {
          emitLambdaStatement(bodyStmt, blockTypes);
        }
        out << "} ";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isBuiltinAssign(stmt, nameMap) && stmt.args.size() == 2 &&
          stmt.args.front().kind == Expr::Kind::Name) {
        out << stmt.args.front().name << " = "
            << emitExpr(stmt.args[1], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
            << "; ";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isPathSpaceBuiltinName(stmt) && nameMap.find(resolveExprPath(stmt)) == nameMap.end()) {
        for (const auto &arg : stmt.args) {
          out << "(void)("
              << emitExpr(arg, nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
              << "); ";
        }
        return;
      }
      if (stmt.kind == Expr::Kind::Call) {
        std::string vectorHelper;
        if (getVectorMutatorName(stmt, nameMap, vectorHelper)) {
          if (vectorHelper == "push") {
            out << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
                << ".push_back("
                << emitExpr(stmt.args[1], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
                << "); ";
            return;
          }
          if (vectorHelper == "pop") {
            out << "ps_vector_pop("
                << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
                << "); ";
            return;
          }
          if (vectorHelper == "reserve") {
            out << "ps_vector_reserve("
                << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
                << ", "
                << emitExpr(stmt.args[1], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
                << "); ";
            return;
          }
          if (vectorHelper == "clear") {
            out << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
                << ".clear(); ";
            return;
          }
          if (vectorHelper == "remove_at") {
            out << "ps_vector_remove_at("
                << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
                << ", "
                << emitExpr(stmt.args[1], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
                << "); ";
            return;
          }
          if (vectorHelper == "remove_swap") {
            out << "ps_vector_remove_swap("
                << emitExpr(stmt.args[0], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
                << ", "
                << emitExpr(stmt.args[1], nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
                << "); ";
            return;
          }
        }
      }
      out << emitExpr(stmt, nameMap, paramMap, structTypeMap, importAliases, activeTypes, returnKinds, allowMathBare)
          << "; ";
    };
    for (const auto &stmt : expr.bodyArguments) {
      emitLambdaStatement(stmt, bodyTypes);
    }
    out << "}";
    return out.str();
  }
