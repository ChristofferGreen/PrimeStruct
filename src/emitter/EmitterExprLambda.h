std::string Emitter::emitExpr(const Expr &expr,
                              const std::unordered_map<std::string, std::string> &nameMap,
                              const std::unordered_map<std::string, std::vector<Expr>> &paramMap,
                              const std::unordered_map<std::string, const Definition *> &defMap,
                              const std::unordered_map<std::string, std::string> &structTypeMap,
                              const std::unordered_map<std::string, std::string> &importAliases,
                              const std::unordered_map<std::string, BindingInfo> &localTypes,
                              const std::unordered_map<std::string, ReturnKind> &returnKinds,
                              const std::unordered_map<std::string, ResultInfo> &resultInfos,
                              const std::unordered_map<std::string, std::string> &returnStructs,
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
  auto bindingTypeText = [](const BindingInfo &binding) {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  auto resolveDirectResultOkInfo = [&](const Expr &candidate,
                                       const std::unordered_map<std::string, BindingInfo> &currentTypes,
                                       const std::string &preferredErrorType,
                                       ResultInfo &out) -> bool {
    out = ResultInfo{};
    if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name != "ok" ||
        !candidate.templateArgs.empty() || candidate.hasBodyArguments || !candidate.bodyArguments.empty() ||
        candidate.args.empty()) {
      return false;
    }
    const Expr &receiver = candidate.args.front();
    if (receiver.kind != Expr::Kind::Name || normalizeBindingTypeName(receiver.name) != "Result") {
      return false;
    }
    out.isResult = true;
    out.errorType = preferredErrorType.empty() ? "_" : preferredErrorType;
    if (candidate.args.size() == 1) {
      out.hasValue = false;
      return true;
    }
    if (candidate.args.size() != 2) {
      return false;
    }
    const Expr &valueExpr = candidate.args.back();
    if (valueExpr.kind == Expr::Kind::Name) {
      auto it = currentTypes.find(valueExpr.name);
      if (it != currentTypes.end()) {
        out.hasValue = true;
        out.valueType = bindingTypeText(it->second);
        return !out.valueType.empty();
      }
    }
    ReturnKind valueKind = inferPrimitiveReturnKind(valueExpr, currentTypes, returnKinds, allowMathBare);
    out.valueType = typeNameForReturnKind(valueKind);
    out.hasValue = !out.valueType.empty();
    return out.hasValue;
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
  auto seedLambdaTypes = [&](const Expr &lambdaExpr,
                             const std::unordered_map<std::string, BindingInfo> &outerTypes,
                             std::unordered_map<std::string, BindingInfo> &lambdaTypesOut) -> bool {
    lambdaTypesOut.clear();
    bool captureAll = false;
    std::vector<std::string> explicitCaptureNames;
    explicitCaptureNames.reserve(lambdaExpr.lambdaCaptures.size());
    for (const auto &capture : lambdaExpr.lambdaCaptures) {
      std::vector<std::string> tokens = splitCaptureTokens(capture);
      if (tokens.empty()) {
        return false;
      }
      if (tokens.size() == 1 && (tokens[0] == "=" || tokens[0] == "&")) {
        captureAll = true;
        continue;
      }
      explicitCaptureNames.push_back(tokens.back());
    }
    if (captureAll) {
      for (const auto &entry : outerTypes) {
        lambdaTypesOut.emplace(entry.first, entry.second);
      }
    } else {
      for (const auto &captureName : explicitCaptureNames) {
        auto it = outerTypes.find(captureName);
        if (it != outerTypes.end()) {
          lambdaTypesOut.emplace(captureName, it->second);
        }
      }
    }
    for (const auto &param : lambdaExpr.args) {
      if (!param.isBinding) {
        return false;
      }
      BindingInfo binding = getBindingInfo(param);
      if (!hasExplicitBindingTypeTransform(param) && param.args.size() == 1) {
        ReturnKind initKind = inferPrimitiveReturnKind(param.args.front(), lambdaTypesOut, returnKinds, allowMathBare);
        std::string inferred = typeNameForReturnKind(initKind);
        if (!inferred.empty()) {
          binding.typeName = inferred;
          binding.typeTemplateArg.clear();
        }
      }
      lambdaTypesOut[param.name] = std::move(binding);
    }
    return true;
  };
  auto inferLambdaValueTypeText = [&](const Expr &lambdaExpr,
                                      const std::unordered_map<std::string, BindingInfo> &outerTypes,
                                      std::string &typeTextOut) -> bool {
    typeTextOut.clear();
    if (!lambdaExpr.isLambda || (!lambdaExpr.hasBodyArguments && lambdaExpr.bodyArguments.empty())) {
      return false;
    }
    std::unordered_map<std::string, BindingInfo> activeTypes;
    if (!seedLambdaTypes(lambdaExpr, outerTypes, activeTypes)) {
      return false;
    }
    const Expr *valueExpr = nullptr;
    for (const auto &stmt : lambdaExpr.bodyArguments) {
      if (stmt.isBinding) {
        BindingInfo binding = getBindingInfo(stmt);
        if (!hasExplicitBindingTypeTransform(stmt) && stmt.args.size() == 1) {
          ReturnKind initKind = inferPrimitiveReturnKind(stmt.args.front(), activeTypes, returnKinds, allowMathBare);
          std::string inferred = typeNameForReturnKind(initKind);
          if (!inferred.empty()) {
            binding.typeName = inferred;
            binding.typeTemplateArg.clear();
          }
        }
        activeTypes[stmt.name] = std::move(binding);
        continue;
      }
      if (isSimpleCallName(stmt, "return") && stmt.args.size() == 1) {
        valueExpr = &stmt.args.front();
        break;
      }
      valueExpr = &stmt;
    }
    if (valueExpr == nullptr) {
      return false;
    }
    if (valueExpr->kind == Expr::Kind::Name) {
      auto it = activeTypes.find(valueExpr->name);
      if (it != activeTypes.end()) {
        typeTextOut = bindingTypeText(it->second);
        return !typeTextOut.empty();
      }
    }
    ReturnKind kind = inferPrimitiveReturnKind(*valueExpr, activeTypes, returnKinds, allowMathBare);
    typeTextOut = typeNameForReturnKind(kind);
    return !typeTextOut.empty();
  };
  std::function<bool(const Expr &, const std::unordered_map<std::string, BindingInfo> &, ResultInfo &)>
      resolveResultExprInfoWithTypes;
  auto inferLambdaResultInfo = [&](const Expr &lambdaExpr,
                                   const std::unordered_map<std::string, BindingInfo> &outerTypes,
                                   const std::string &preferredErrorType,
                                   ResultInfo &resultOut) -> bool {
    resultOut = ResultInfo{};
    if (!lambdaExpr.isLambda || (!lambdaExpr.hasBodyArguments && lambdaExpr.bodyArguments.empty())) {
      return false;
    }
    std::unordered_map<std::string, BindingInfo> activeTypes;
    if (!seedLambdaTypes(lambdaExpr, outerTypes, activeTypes)) {
      return false;
    }
    const Expr *valueExpr = nullptr;
    for (const auto &stmt : lambdaExpr.bodyArguments) {
      if (stmt.isBinding) {
        BindingInfo binding = getBindingInfo(stmt);
        if (!hasExplicitBindingTypeTransform(stmt) && stmt.args.size() == 1) {
          ReturnKind initKind = inferPrimitiveReturnKind(stmt.args.front(), activeTypes, returnKinds, allowMathBare);
          std::string inferred = typeNameForReturnKind(initKind);
          if (!inferred.empty()) {
            binding.typeName = inferred;
            binding.typeTemplateArg.clear();
          }
        }
        activeTypes[stmt.name] = std::move(binding);
        continue;
      }
      if (isSimpleCallName(stmt, "return") && stmt.args.size() == 1) {
        valueExpr = &stmt.args.front();
        break;
      }
      valueExpr = &stmt;
    }
    if (valueExpr == nullptr) {
      return false;
    }
    if (resolveDirectResultOkInfo(*valueExpr, activeTypes, preferredErrorType, resultOut)) {
      return true;
    }
    if (!resolveResultExprInfoWithTypes(*valueExpr, activeTypes, resultOut) || !resultOut.isResult) {
      return false;
    }
    if (!preferredErrorType.empty() && resultOut.errorType == "_") {
      resultOut.errorType = preferredErrorType;
    }
    return true;
  };
  resolveResultExprInfoWithTypes =
      [&](const Expr &candidate, const std::unordered_map<std::string, BindingInfo> &currentTypes, ResultInfo &out) -> bool {
        out = ResultInfo{};
        if (candidate.kind == Expr::Kind::Name) {
          auto it = currentTypes.find(candidate.name);
          if (it != currentTypes.end() && it->second.typeName == "Result") {
            std::vector<std::string> args;
            if (splitTopLevelTemplateArgs(it->second.typeTemplateArg, args)) {
              out.isResult = true;
              out.hasValue = (args.size() == 2);
              if (out.hasValue) {
                out.valueType = args.front();
                out.errorType = args.back();
              } else if (!args.empty()) {
                out.errorType = args.front();
              }
              return true;
            }
          }
          return false;
        }
        if (candidate.kind != Expr::Kind::Call) {
          return false;
        }
        if (resolveDirectResultOkInfo(candidate, currentTypes, "", out)) {
          return true;
        }
        if (!candidate.isMethodCall && isSimpleCallName(candidate, "File")) {
          out.isResult = true;
          out.hasValue = true;
          out.valueType = "File";
          out.errorType = "FileError";
          return true;
        }
        std::string resolved = resolveExprPath(candidate);
        if (candidate.isMethodCall) {
          std::string methodPath;
          if (resolveMethodCallPath(
                  candidate, defMap, currentTypes, importAliases, structTypeMap, returnKinds, returnStructs, methodPath)) {
            resolved = methodPath;
          }
          if (resolved.rfind("/file/", 0) == 0) {
            out.isResult = true;
            out.hasValue = false;
            out.errorType = "FileError";
            return true;
          }
          if (!candidate.args.empty() &&
              candidate.args.front().kind == Expr::Kind::Name &&
              normalizeBindingTypeName(candidate.args.front().name) == "Result") {
            if (candidate.name == "map" && candidate.args.size() == 3) {
              ResultInfo argInfo;
              if (!resolveResultExprInfoWithTypes(candidate.args[1], currentTypes, argInfo) || !argInfo.isResult ||
                  !argInfo.hasValue) {
                return false;
              }
              std::string mappedType;
              if (!inferLambdaValueTypeText(candidate.args[2], currentTypes, mappedType) || mappedType.empty()) {
                return false;
              }
              out.isResult = true;
              out.hasValue = true;
              out.valueType = std::move(mappedType);
              out.errorType = argInfo.errorType;
              return true;
            }
            if (candidate.name == "and_then" && candidate.args.size() == 3) {
              ResultInfo argInfo;
              ResultInfo nextInfo;
              if (!resolveResultExprInfoWithTypes(candidate.args[1], currentTypes, argInfo) || !argInfo.isResult ||
                  !argInfo.hasValue ||
                  !inferLambdaResultInfo(candidate.args[2], currentTypes, argInfo.errorType, nextInfo) ||
                  !nextInfo.isResult) {
                return false;
              }
              if (!nextInfo.errorType.empty() &&
                  !argInfo.errorType.empty() &&
                  normalizeBindingTypeName(nextInfo.errorType) != normalizeBindingTypeName(argInfo.errorType)) {
                return false;
              }
              out = std::move(nextInfo);
              return true;
            }
            if (candidate.name == "map2" && candidate.args.size() == 4) {
              ResultInfo leftInfo;
              ResultInfo rightInfo;
              if (!resolveResultExprInfoWithTypes(candidate.args[1], currentTypes, leftInfo) || !leftInfo.isResult ||
                  !leftInfo.hasValue ||
                  !resolveResultExprInfoWithTypes(candidate.args[2], currentTypes, rightInfo) || !rightInfo.isResult ||
                  !rightInfo.hasValue ||
                  normalizeBindingTypeName(leftInfo.errorType) != normalizeBindingTypeName(rightInfo.errorType)) {
                return false;
              }
              std::string mappedType;
              if (!inferLambdaValueTypeText(candidate.args[3], currentTypes, mappedType) || mappedType.empty()) {
                return false;
              }
              out.isResult = true;
              out.hasValue = true;
              out.valueType = std::move(mappedType);
              out.errorType = leftInfo.errorType;
              return true;
            }
          }
        }
        auto it = resultInfos.find(resolved);
        if (it != resultInfos.end() && it->second.isResult) {
          out = it->second;
          return true;
        }
        if (!candidate.isMethodCall && !candidate.name.empty() && candidate.name[0] != '/' &&
            candidate.name.find('/') == std::string::npos) {
          auto importIt = importAliases.find(candidate.name);
          if (importIt != importAliases.end()) {
            auto aliasIt = resultInfos.find(importIt->second);
            if (aliasIt != resultInfos.end() && aliasIt->second.isResult) {
              out = aliasIt->second;
              return true;
            }
          }
        }
        return false;
      };
  auto resolveResultExprInfo = [&](const Expr &candidate, ResultInfo &out) -> bool {
    return resolveResultExprInfoWithTypes(candidate, localTypes, out);
  };
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
               (resolveExprPath(stmt).rfind("/vector/", 0) == 0 ||
                resolveExprPath(stmt).rfind("/std/collections/vector/", 0) == 0))
                  ? resolveExprPath(stmt)
                  : std::string{};
          std::string helperPath;
          bool hasUserVectorHelper = false;
          Expr helperCall = stmt;
          Expr missingHelperCall = stmt;
          bool hasMissingVectorHelper = false;
          auto rememberMissingVectorHelperCall = [&](const Expr &candidate, const std::string &resolvedPath) {
            if (resolvedPath != "/vector/" + vectorHelper &&
                resolvedPath != "/std/collections/vector/" + vectorHelper) {
              return;
            }
            missingHelperCall = candidate;
            hasMissingVectorHelper = true;
          };
          if (stmt.isMethodCall) {
            if (resolveMethodCallPath(
                    stmt, defMap, activeTypes, importAliases, structTypeMap, returnKinds, returnStructs, helperPath)) {
              if (!stmt.name.empty() && stmt.name.front() == '/' && !explicitRequestedVectorHelperPath.empty() &&
                  helperPath != explicitRequestedVectorHelperPath) {
                missingHelperCall = stmt;
                hasMissingVectorHelper = true;
                helperPath.clear();
              } else {
                helperPath = preferVectorStdlibHelperPath(helperPath, nameMap);
                rememberMissingVectorHelperCall(stmt, helperPath);
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
                  missingHelperCall = stmt;
                  hasMissingVectorHelper = true;
                  helperPath.clear();
                } else {
                  helperPath = preferVectorStdlibHelperPath(helperPath, nameMap);
                  rememberMissingVectorHelperCall(methodCandidate, helperPath);
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
          if (hasMissingVectorHelper) {
            out << "ps_missing_vector_" << vectorHelper
                << (stmt.isMethodCall ? "_method_helper(" : "_call_helper(");
            for (size_t i = 0; i < missingHelperCall.args.size(); ++i) {
              if (i > 0) {
                out << ", ";
              }
              out << emitExpr(missingHelperCall.args[i],
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
            out << "); ";
            return;
          }
          if (stmt.isMethodCall && !stmt.name.empty() && stmt.name.front() == '/') {
            out << "ps_missing_vector_" << vectorHelper << "_method_helper(";
            for (size_t i = 0; i < stmt.args.size(); ++i) {
              if (i > 0) {
                out << ", ";
              }
              out << emitExpr(stmt.args[i],
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
            out << "); ";
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
