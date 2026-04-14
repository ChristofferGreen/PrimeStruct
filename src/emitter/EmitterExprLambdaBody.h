  if (expr.isLambda) {
    struct LambdaCapture {
      std::string name;
      bool byRef = false;
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
        bool needsConst = !paramInfo.isMutable && !isAliasingBinding(paramInfo);
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
                        defMap,
                        structTypeMap,
                        importAliases,
                        lambdaTypes,
                        returnKinds,
                        resultInfos,
                        returnStructs,
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
      auto emitConditionText = [&](const Expr &condExpr,
                                   const std::unordered_map<std::string, BindingInfo> &condTypes) -> std::string {
        std::string text = emitExpr(condExpr,
                                    nameMap,
                                    paramMap,
                                    defMap,
                                    structTypeMap,
                                    importAliases,
                                    condTypes,
                                    returnKinds,
                                    resultInfos,
                                    returnStructs,
                                    allowMathBare);
        if (text.size() >= 2 && text.front() == '(' && text.back() == ')') {
          return text;
        }
        return "(" + text + ")";
      };
      if (isReturnCall(stmt)) {
        out << "return";
        if (!stmt.args.empty()) {
          out << " "
              << emitExpr(stmt.args.front(),
                          nameMap,
                          paramMap,
                          defMap,
                          structTypeMap,
                          importAliases,
                          activeTypes,
                          returnKinds,
                          resultInfos,
                          returnStructs,
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
                               defMap,
                               structTypeMap,
                               importAliases,
                               activeTypes,
                               returnKinds,
                               resultInfos,
                               returnStructs,
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
        if (!hasExplicitType && lambdaInit) {
          binding.typeName = "lambda";
          binding.typeTemplateArg.clear();
        }
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
                            defMap,
                            structTypeMap,
                            importAliases,
                            activeTypes,
                            returnKinds,
                            resultInfos,
                            returnStructs,
                            allowMathBare);
          }
          out << "; ";
          return;
        }
        bool needsConst = !binding.isMutable && !isAliasingBinding(binding);
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
                                         defMap,
                                         structTypeMap,
                                         importAliases,
                                         activeTypes,
                                         returnKinds,
                                         resultInfos,
                                         returnStructs,
                                         allowMathBare)
                  << ")";
            } else {
              out << " = " << emitExpr(stmt.args.front(),
                                      nameMap,
                                      paramMap,
                                      defMap,
                                      structTypeMap,
                                      importAliases,
                                      activeTypes,
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
                                    defMap,
                                    structTypeMap,
                                    importAliases,
                                    activeTypes,
                                    returnKinds,
                                    resultInfos,
                                    returnStructs,
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
        out << "if " << emitConditionText(cond, activeTypes) << " { ";
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
            emitExpr(stmt.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, activeTypes, returnKinds, resultInfos, returnStructs, allowMathBare);
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
        out << "while " << emitConditionText(stmt.args[0], activeTypes) << " { ";
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
        out << "while " << emitConditionText(stmt.args[1], loopTypes) << " { ";
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
              emitExpr(stmt.args.front(), nameMap, paramMap, defMap, structTypeMap, importAliases, activeTypes, returnKinds, resultInfos, returnStructs, allowMathBare);
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
        out << emitExpr(callExpr, nameMap, paramMap, defMap, structTypeMap, importAliases, activeTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
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
            << emitExpr(stmt.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, activeTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
            << "; ";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isPathSpaceBuiltinName(stmt) && nameMap.find(resolveExprPath(stmt)) == nameMap.end()) {
        for (const auto &arg : stmt.args) {
          out << "(void)("
              << emitExpr(arg, nameMap, paramMap, defMap, structTypeMap, importAliases, activeTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
              << "); ";
        }
        return;
      }
      if (stmt.kind == Expr::Kind::Call) {
        std::string vectorHelper;
        if (getVectorMutatorName(stmt, nameMap, vectorHelper)) {
          const std::string explicitRequestedVectorHelperPath =
              (!stmt.isMethodCall &&
               (resolveExprPath(stmt).rfind("/std/collections/vector/", 0) == 0))
                  ? resolveExprPath(stmt)
                  : std::string{};
          std::string helperPath;
          bool hasUserVectorHelper = false;
          Expr helperCall = stmt;
          if (stmt.isMethodCall) {
            if (resolveMethodCallPath(
                    stmt, defMap, activeTypes, importAliases, structTypeMap, returnKinds, returnStructs, helperPath)) {
              if (!stmt.name.empty() && stmt.name.front() == '/' && !explicitRequestedVectorHelperPath.empty() &&
                  helperPath != explicitRequestedVectorHelperPath) {
                helperPath.clear();
              } else {
                hasUserVectorHelper = nameMap.find(helperPath) != nameMap.end();
              }
            }
            if (hasUserVectorHelper) {
              helperCall.isMethodCall = true;
            }
          } else {
            std::vector<size_t> receiverIndices;
            auto appendReceiverIndex = [&](size_t index) {
              if (index >= stmt.args.size()) {
                return;
              }
              for (size_t existing : receiverIndices) {
                if (existing == index) {
                  return;
                }
              }
              receiverIndices.push_back(index);
            };
            const bool hasNamedArgs = hasNamedArguments(stmt.argNames);
            if (hasNamedArgs) {
              bool hasValuesNamedReceiver = false;
              for (size_t i = 0; i < stmt.args.size(); ++i) {
                if (i < stmt.argNames.size() && stmt.argNames[i].has_value() &&
                    *stmt.argNames[i] == "values") {
                  appendReceiverIndex(i);
                  hasValuesNamedReceiver = true;
                }
              }
              if (!hasValuesNamedReceiver) {
                appendReceiverIndex(0);
                for (size_t i = 1; i < stmt.args.size(); ++i) {
                  appendReceiverIndex(i);
                }
              }
            } else {
              appendReceiverIndex(0);
            }
            const bool probePositionalReorderedReceiver =
                !hasNamedArgs && stmt.args.size() > 1 &&
                (stmt.args.front().kind == Expr::Kind::Literal || stmt.args.front().kind == Expr::Kind::BoolLiteral ||
                 stmt.args.front().kind == Expr::Kind::FloatLiteral || stmt.args.front().kind == Expr::Kind::StringLiteral ||
                 (stmt.args.front().kind == Expr::Kind::Name && !isVectorValue(stmt.args.front(), activeTypes)));
            if (probePositionalReorderedReceiver) {
              for (size_t i = 1; i < stmt.args.size(); ++i) {
                appendReceiverIndex(i);
              }
            }
            for (size_t receiverIndex : receiverIndices) {
              Expr methodCandidate = stmt;
              methodCandidate.isMethodCall = true;
              methodCandidate.name = vectorHelper;
              if (receiverIndex != 0 && receiverIndex < methodCandidate.args.size()) {
                std::swap(methodCandidate.args[0], methodCandidate.args[receiverIndex]);
                if (methodCandidate.argNames.size() < methodCandidate.args.size()) {
                  methodCandidate.argNames.resize(methodCandidate.args.size());
                }
                std::swap(methodCandidate.argNames[0], methodCandidate.argNames[receiverIndex]);
              }
              const bool resolvedMethodPath = resolveMethodCallPath(
                  methodCandidate,
                  defMap,
                  activeTypes,
                  importAliases,
                  structTypeMap,
                  returnKinds,
                  returnStructs,
                  helperPath);
              if (resolvedMethodPath) {
                if (!explicitRequestedVectorHelperPath.empty() &&
                    helperPath != explicitRequestedVectorHelperPath) {
                  helperPath.clear();
                }
              }
              if (resolvedMethodPath && !helperPath.empty() && nameMap.find(helperPath) != nameMap.end()) {
                hasUserVectorHelper = true;
                helperCall = std::move(methodCandidate);
                break;
              }
            }
          }
          if (hasUserVectorHelper) {
            out << emitExpr(helperCall,
                            nameMap,
                            paramMap,
                            defMap,
                            structTypeMap,
                            importAliases,
                            activeTypes,
                            returnKinds,
                            resultInfos,
                            returnStructs,
                            allowMathBare)
                << "; ";
            return;
          }
          if (vectorHelper == "push") {
            out << "ps_vector_push("
                << emitExpr(stmt.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, activeTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
                << ", "
                << emitExpr(stmt.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, activeTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
                << "); ";
            return;
          }
          if (vectorHelper == "pop") {
            out << "ps_vector_pop("
                << emitExpr(stmt.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, activeTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
                << "); ";
            return;
          }
          if (vectorHelper == "reserve") {
            out << "ps_vector_reserve("
                << emitExpr(stmt.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, activeTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
                << ", "
                << emitExpr(stmt.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, activeTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
                << "); ";
            return;
          }
          if (vectorHelper == "clear") {
            out << emitExpr(stmt.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, activeTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
                << ".clear(); ";
            return;
          }
          if (vectorHelper == "remove_at") {
            out << "ps_vector_remove_at("
                << emitExpr(stmt.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, activeTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
                << ", "
                << emitExpr(stmt.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, activeTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
                << "); ";
            return;
          }
          if (vectorHelper == "remove_swap") {
            out << "ps_vector_remove_swap("
                << emitExpr(stmt.args[0], nameMap, paramMap, defMap, structTypeMap, importAliases, activeTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
                << ", "
                << emitExpr(stmt.args[1], nameMap, paramMap, defMap, structTypeMap, importAliases, activeTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
                << "); ";
            return;
          }
        }
      }
      out << emitExpr(stmt, nameMap, paramMap, defMap, structTypeMap, importAliases, activeTypes, returnKinds, resultInfos, returnStructs, allowMathBare)
          << "; ";
    };
    for (const auto &stmt : expr.bodyArguments) {
      emitLambdaStatement(stmt, bodyTypes);
    }
    out << "}";
    return out.str();
  }
