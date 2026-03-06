  if (const auto integerExpr = emitter::runEmitterExprControlIntegerLiteralStep(expr); integerExpr.has_value()) {
    return *integerExpr;
  }
  if (const auto boolExpr = emitter::runEmitterExprControlBoolLiteralStep(expr); boolExpr.has_value()) {
    return *boolExpr;
  }
  if (const auto floatExpr = emitter::runEmitterExprControlFloatLiteralStep(expr); floatExpr.has_value()) {
    return *floatExpr;
  }
  if (const auto stringExpr = emitter::runEmitterExprControlStringLiteralStep(expr); stringExpr.has_value()) {
    return *stringExpr;
  }
  if (const auto nameExpr = emitter::runEmitterExprControlNameStep(expr, localTypes, allowMathBare);
      nameExpr.has_value()) {
    return *nameExpr;
  }
  if (const auto fieldAccessExpr = emitter::runEmitterExprControlFieldAccessStep(
          expr,
          [&](const Expr &receiverExpr) {
            return emitExpr(receiverExpr,
                            nameMap,
                            paramMap,
                            structTypeMap,
                            importAliases,
                            localTypes,
                            returnKinds,
                            resultInfos,
                            returnStructs,
                            allowMathBare);
          });
      fieldAccessExpr.has_value()) {
    return *fieldAccessExpr;
  }
  std::string full = resolveExprPath(expr);
  if (const auto methodPath = emitter::runEmitterExprControlMethodPathStep(
          expr,
          localTypes,
          [&](const Expr &candidate, const std::unordered_map<std::string, BindingInfo> &candidateLocalTypes) {
            return isArrayCountCall(candidate, candidateLocalTypes);
          },
          [&](const Expr &candidate, const std::unordered_map<std::string, BindingInfo> &candidateLocalTypes) {
            return isMapCountCall(candidate, candidateLocalTypes);
          },
          [&](const Expr &candidate, const std::unordered_map<std::string, BindingInfo> &candidateLocalTypes) {
            return isStringCountCall(candidate, candidateLocalTypes);
          },
          [&](std::string &methodPathOut) {
            return resolveMethodCallPath(expr,
                                         localTypes,
                                         importAliases,
                                         structTypeMap,
                                         returnKinds,
                                         returnStructs,
                                         methodPathOut);
          });
      methodPath.has_value()) {
    full = *methodPath;
  }
  if (const auto callPath = emitter::runEmitterExprControlCallPathStep(expr, full, nameMap, importAliases);
      callPath.has_value()) {
    full = *callPath;
  }
  if (const auto countRewritePath = emitter::runEmitterExprControlCountRewriteStep(
          expr,
          full,
          nameMap,
          localTypes,
          [&](const Expr &candidate, const std::unordered_map<std::string, BindingInfo> &candidateLocalTypes) {
            return isArrayCountCall(candidate, candidateLocalTypes);
          },
          [&](const Expr &candidate, const std::unordered_map<std::string, BindingInfo> &candidateLocalTypes) {
            return isMapCountCall(candidate, candidateLocalTypes);
          },
          [&](const Expr &candidate, const std::unordered_map<std::string, BindingInfo> &candidateLocalTypes) {
            return isStringCountCall(candidate, candidateLocalTypes);
          },
          [&](const Expr &methodExpr, std::string &methodPathOut) {
            return resolveMethodCallPath(methodExpr,
                                         localTypes,
                                         importAliases,
                                         structTypeMap,
                                         returnKinds,
                                         returnStructs,
                                         methodPathOut);
          });
      countRewritePath.has_value()) {
    full = *countRewritePath;
  }
  if (const auto builtinBlockPrelude = emitter::runEmitterExprControlBuiltinBlockPreludeStep(
          expr,
          nameMap,
          [&](const Expr &candidate, const std::unordered_map<std::string, std::string> &candidateNameMap) {
            return isBuiltinBlock(candidate, candidateNameMap);
          },
          [&](const std::vector<std::optional<std::string>> &argNames) { return hasNamedArguments(argNames); });
      builtinBlockPrelude.matched) {
    if (builtinBlockPrelude.earlyReturnExpr.has_value()) {
      return *builtinBlockPrelude.earlyReturnExpr;
    }
    std::unordered_map<std::string, BindingInfo> blockTypes = localTypes;
    std::ostringstream out;
    out << "([&]() { ";
    for (size_t i = 0; i < expr.bodyArguments.size(); ++i) {
      const Expr &stmt = expr.bodyArguments[i];
      const bool isLast = (i + 1 == expr.bodyArguments.size());
      if (const auto earlyReturnStep = emitter::runEmitterExprControlBuiltinBlockEarlyReturnStep(
              stmt,
              isLast,
              [&](const Expr &candidate) { return isReturnCall(candidate); },
              [&](const Expr &candidate) {
                return emitExpr(candidate,
                                nameMap,
                                paramMap,
                                structTypeMap,
                                importAliases,
                                blockTypes,
                                returnKinds,
                                resultInfos,
                                returnStructs,
                                allowMathBare);
              });
          earlyReturnStep.handled) {
        out << earlyReturnStep.emittedStatement;
        break;
      }
      if (const auto finalValueStep = emitter::runEmitterExprControlBuiltinBlockFinalValueStep(
              stmt,
              isLast,
              [&](const Expr &candidate) { return isReturnCall(candidate); },
              [&](const Expr &candidate) {
                return emitExpr(candidate,
                                nameMap,
                                paramMap,
                                structTypeMap,
                                importAliases,
                                blockTypes,
                                returnKinds,
                                resultInfos,
                                returnStructs,
                                allowMathBare);
              });
          finalValueStep.handled) {
        out << finalValueStep.emittedStatement;
        break;
      }
      if (const auto bindingPrelude = emitter::runEmitterExprControlBuiltinBlockBindingPreludeStep(
              stmt,
              blockTypes,
              returnKinds,
              allowMathBare,
              [&](const Expr &candidate) { return getBindingInfo(candidate); },
              [&](const Expr &candidate) { return hasExplicitBindingTypeTransform(candidate); },
              [&](const Expr &candidate,
                  const std::unordered_map<std::string, BindingInfo> &candidateBlockTypes,
                  const std::unordered_map<std::string, ReturnKind> &candidateReturnKinds,
                  bool candidateAllowMathBare) {
                return inferPrimitiveReturnKind(candidate, candidateBlockTypes, candidateReturnKinds, candidateAllowMathBare);
              },
              [&](ReturnKind candidateKind) { return typeNameForReturnKind(candidateKind); });
          bindingPrelude.handled) {
        BindingInfo binding = bindingPrelude.binding;
        const bool hasExplicitType = bindingPrelude.hasExplicitType;
        const bool useAuto = bindingPrelude.useAuto;
        if (const auto autoBindingStep = emitter::runEmitterExprControlBuiltinBlockBindingAutoStep(
                stmt,
                binding,
                useAuto,
                [&](const Expr &candidate) {
                  return emitExpr(candidate,
                                  nameMap,
                                  paramMap,
                                  structTypeMap,
                                  importAliases,
                                  blockTypes,
                                  returnKinds,
                                  resultInfos,
                                  returnStructs,
                                  allowMathBare);
                });
            autoBindingStep.handled) {
          out << autoBindingStep.emittedStatement;
          continue;
        }
        const auto bindingQualifiers = emitter::runEmitterExprControlBuiltinBlockBindingQualifiersStep(
            binding,
            !stmt.args.empty(),
            [&](const BindingInfo &candidateBinding) { return isReferenceCandidate(candidateBinding); });
        const bool needsConst = bindingQualifiers.needsConst;
        const bool useRef = bindingQualifiers.useRef;
        if (const auto explicitBindingStep = emitter::runEmitterExprControlBuiltinBlockBindingExplicitStep(
                stmt,
                binding,
                hasExplicitType,
                needsConst,
                useRef,
                stmt.namespacePrefix,
                importAliases,
                structTypeMap,
                [&](const Expr &candidate) {
                  return emitExpr(candidate,
                                  nameMap,
                                  paramMap,
                                  structTypeMap,
                                  importAliases,
                                  blockTypes,
                                  returnKinds,
                                  resultInfos,
                                  returnStructs,
                                  allowMathBare);
                });
            explicitBindingStep.handled) {
          out << explicitBindingStep.emittedStatement;
        } else if (const auto fallbackBindingStep = emitter::runEmitterExprControlBuiltinBlockBindingFallbackStep(
                       stmt,
                       hasExplicitType,
                       needsConst,
                       useRef,
                       [&](const Expr &candidate) {
                         return emitExpr(candidate,
                                         nameMap,
                                         paramMap,
                                         structTypeMap,
                                         importAliases,
                                         blockTypes,
                                         returnKinds,
                                         resultInfos,
                                         returnStructs,
                                         allowMathBare);
                       });
                   fallbackBindingStep.handled) {
          out << fallbackBindingStep.emittedStatement;
        }
        continue;
      }
      if (const auto statementStep = emitter::runEmitterExprControlBuiltinBlockStatementStep(
              stmt,
              [&](const Expr &candidate) {
                return emitExpr(candidate,
                                nameMap,
                                paramMap,
                                structTypeMap,
                                importAliases,
                                blockTypes,
                                returnKinds,
                                resultInfos,
                                returnStructs,
                                allowMathBare);
              });
          statementStep.handled) {
        out << statementStep.emittedStatement;
      }
    }
    out << "}())";
    return out.str();
  }
  if (const auto wrappedBodyCall = emitter::runEmitterExprControlBodyWrapperStep(
          expr,
          nameMap,
          [&](const Expr &candidate, const std::unordered_map<std::string, std::string> &candidateNameMap) {
            return isBuiltinBlock(candidate, candidateNameMap);
          },
          [&](const Expr &candidate) {
            return emitExpr(candidate,
                            nameMap,
                            paramMap,
                            structTypeMap,
                            importAliases,
                            localTypes,
                            returnKinds,
                            resultInfos,
                            returnStructs,
                            allowMathBare);
          });
      wrappedBodyCall.has_value()) {
    return *wrappedBodyCall;
  }
  if (isBuiltinIf(expr, nameMap) && expr.args.size() == 3) {
    auto emitBranchValueExpr =
        [&](const Expr &candidate, std::unordered_map<std::string, BindingInfo> branchTypes) -> std::string {
      if (!runEmitterExprControlIfBlockEnvelopeStep(candidate)) {
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
        if (const auto earlyReturnStep = emitter::runEmitterExprControlIfBlockEarlyReturnStep(
                stmt,
                isLast,
                [&](const Expr &candidate) { return isReturnCall(candidate); },
                [&](const Expr &candidate) {
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
                });
            earlyReturnStep.handled) {
          out << earlyReturnStep.emittedStatement;
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
