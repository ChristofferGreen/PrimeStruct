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
      if (const auto branchValueStep = emitter::runEmitterExprControlIfBranchValueStep(
              candidate,
              [&](const Expr &candidateExpr) {
                return runEmitterExprControlIfBlockEnvelopeStep(candidateExpr);
              },
              [&](const Expr &candidateExpr) {
                return emitExpr(candidateExpr,
                                nameMap,
                                paramMap,
                                structTypeMap,
                                importAliases,
                                branchTypes,
                                returnKinds,
                                resultInfos,
                                returnStructs,
                                allowMathBare);
              },
              [&](const Expr &stmt, bool isLast) {
                if (const auto handlersStep = emitter::runEmitterExprControlIfBranchBodyHandlersStep(
                        stmt,
                        isLast,
                        branchTypes,
                        returnKinds,
                        allowMathBare,
                        importAliases,
                        structTypeMap,
                        [&](const Expr &candidate) { return isReturnCall(candidate); },
                        [&](const Expr &candidate) { return getBindingInfo(candidate); },
                        [&](const Expr &candidate) { return hasExplicitBindingTypeTransform(candidate); },
                        [&](const Expr &candidate,
                            const std::unordered_map<std::string, BindingInfo> &candidateBranchTypes,
                            const std::unordered_map<std::string, ReturnKind> &candidateReturnKinds,
                            bool candidateAllowMathBare) {
                          return inferPrimitiveReturnKind(
                              candidate, candidateBranchTypes, candidateReturnKinds, candidateAllowMathBare);
                        },
                        [&](ReturnKind candidateKind) { return typeNameForReturnKind(candidateKind); },
                        [&](const BindingInfo &candidateBinding) { return isReferenceCandidate(candidateBinding); },
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
                    handlersStep.handled) {
                  return handlersStep.emitted;
                }
                return emitter::EmitterExprControlIfBranchBodyEmitResult{};
              });
          branchValueStep.handled) {
        return branchValueStep.emittedExpr;
      }
      return std::string{};
    };
  if (const auto ternaryStep = emitter::runEmitterExprControlIfTernaryStep(
          [&]() {
            return emitExpr(expr.args[0],
                            nameMap,
                            paramMap,
                            structTypeMap,
                            importAliases,
                            localTypes,
                            returnKinds,
                            resultInfos,
                            returnStructs,
                            allowMathBare);
          },
          [&]() { return emitBranchValueExpr(expr.args[1], localTypes); },
          [&]() { return emitBranchValueExpr(expr.args[2], localTypes); });
      ternaryStep.handled) {
    return ternaryStep.emittedExpr;
  }

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
