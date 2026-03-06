    return ir_lowerer::runLowerStatementsCallsStep(
        {
            .inferExprKind =
                [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferExprKind(valueExpr, valueLocals); },
            .emitExpr = [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
            .allocTempLocal = [&]() { return allocTempLocal(); },
            .resolveExprPath = [&](const Expr &valueExpr) { return resolveExprPath(valueExpr); },
            .findDefinitionByPath = [&](const std::string &path) -> const Definition * {
              auto it = defMap.find(path);
              return it == defMap.end() ? nullptr : it->second;
            },
            .isArrayCountCall = [&](const Expr &callExpr, const LocalMap &callLocals) {
              return isArrayCountCall(callExpr, callLocals);
            },
            .isStringCountCall = [&](const Expr &callExpr, const LocalMap &callLocals) {
              return isStringCountCall(callExpr, callLocals);
            },
            .isVectorCapacityCall = [&](const Expr &callExpr, const LocalMap &callLocals) {
              return isVectorCapacityCall(callExpr, callLocals);
            },
            .resolveMethodCallDefinition = [&](const Expr &callExpr, const LocalMap &callLocals) {
              return resolveMethodCallDefinition(callExpr, callLocals);
            },
            .resolveDefinitionCall = [&](const Expr &callExpr) { return resolveDefinitionCall(callExpr); },
            .getReturnInfo =
                [&](const std::string &definitionPath, ReturnInfo &returnInfo) {
                  return getReturnInfo(definitionPath, returnInfo);
                },
            .emitInlineDefinitionCall =
                [&](const Expr &callExpr, const Definition &callee, const LocalMap &callLocals, bool expectValue) {
                  return emitInlineDefinitionCall(callExpr, callee, callLocals, expectValue);
                },
            .instructions = &function.instructions,
        },
        stmt,
        localsIn,
        error);
  };

  auto emitEntryStatement = [&](const Expr &stmt) -> bool {
    const size_t startInstructionIndex = function.instructions.size();
    if (!emitStatement(stmt, locals)) {
      return false;
    }
    appendInstructionSourceRange(function.name, stmt, startInstructionIndex, function.instructions.size());
    return true;
  };
  if (!ir_lowerer::runLowerStatementsEntryExecutionStep(
          {
              .entryDef = entryDef,
              .returnsVoid = returnsVoid,
              .sawReturn = &sawReturn,
              .onErrorByDef = &onErrorByDef,
              .currentOnError = &currentOnError,
              .currentReturnResult = &currentReturnResult,
              .entryHasResultInfo = entryHasResultInfo,
              .entryResultInfo = &entryResultInfo,
              .emitEntryStatement = emitEntryStatement,
              .pushFileScope = [&]() { pushFileScope(); },
              .emitCurrentFileScopeCleanup = [&]() { emitFileScopeCleanup(fileScopeStack.back()); },
              .popFileScope = [&]() { popFileScope(); },
              .instructions = &function.instructions,
          },
          error)) {
    return false;
  }

  if (!ir_lowerer::runLowerStatementsFunctionTableStep(
          {
              .program = &program,
              .entryDef = entryDef,
              .function = &function,
              .loweredCallTargets = &loweredCallTargets,
              .isStructDefinition = [&](const Definition &def) { return isStructDefinition(def); },
              .getReturnInfo =
                  [&](const std::string &definitionPath, ReturnInfo &returnInfo) {
                    return getReturnInfo(definitionPath, returnInfo);
                  },
              .defaultEffects = &defaultEffects,
              .entryDefaultEffects = &entryDefaultEffects,
              .isTailCallCandidate = [&](const Expr &expr) { return isTailCallCandidate(expr); },
              .resetDefinitionLoweringState = [&]() {
                onErrorTempCounter = 0;
                sawReturn = false;
                activeInlineContext = nullptr;
                inlineStack.clear();
                fileScopeStack.clear();
                currentOnError.reset();
                currentReturnResult.reset();
              },
              .buildDefinitionCallContext =
                  [&](const Definition &def,
                      int32_t &definitionNextLocal,
                      LocalMap &definitionLocals,
                      Expr &callExpr,
                      std::string &callError) {
                    return ir_lowerer::buildCallableDefinitionCallContext(
                        def,
                        definitionNextLocal,
                        definitionLocals,
                        callExpr,
                        [&](const Expr &param, const LocalMap &localsForKindInference, LocalInfo &info, std::string &inferError) {
                          return ir_lowerer::inferCallParameterLocalInfo(param,
                                                                         localsForKindInference,
                                                                         isBindingMutable,
                                                                         hasExplicitBindingTypeTransform,
                                                                         bindingKind,
                                                                         bindingValueKind,
                                                                         inferExprKind,
                                                                         isFileErrorBinding,
                                                                         setReferenceArrayInfo,
                                                                         applyStructArrayInfo,
                                                                         applyStructValueInfo,
                                                                         isStringBinding,
                                                                         info,
                                                                         inferError);
                        },
                        callError);
                  },
              .emitInlineDefinitionCall =
                  [&](const Expr &callExpr, const Definition &def, const LocalMap &definitionLocals, bool expectValue) {
                    return emitInlineDefinitionCall(callExpr, def, definitionLocals, expectValue);
                  },
              .nextLocal = &nextLocal,
              .outFunctions = &out.functions,
              .entryIndex = &out.entryIndex,
          },
          error)) {
    return false;
  }

  for (auto &rangeEntry : instructionSourceRangesByFunction) {
    auto &ranges = rangeEntry.second;
    std::sort(ranges.begin(), ranges.end(), [](const InstructionSourceRange &left, const InstructionSourceRange &right) {
      if (left.beginIndex != right.beginIndex) {
        return left.beginIndex < right.beginIndex;
      }
      if (left.endIndex != right.endIndex) {
        return left.endIndex < right.endIndex;
      }
      if (left.line != right.line) {
        return left.line < right.line;
      }
      return left.column < right.column;
    });
  }

  size_t totalInstructionCount = 0;
  for (const auto &loweredFunction : out.functions) {
    totalInstructionCount += loweredFunction.instructions.size();
  }
  out.instructionSourceMap.clear();
  out.instructionSourceMap.reserve(totalInstructionCount);

  uint64_t nextInstructionDebugId = 1;
  for (auto &loweredFunction : out.functions) {
    uint32_t fallbackSourceLine = 0;
    uint32_t fallbackSourceColumn = 0;
    auto defIt = defMap.find(loweredFunction.name);
    if (defIt != defMap.end() && defIt->second != nullptr) {
      if (defIt->second->sourceLine > 0) {
        fallbackSourceLine = static_cast<uint32_t>(defIt->second->sourceLine);
      }
      if (defIt->second->sourceColumn > 0) {
        fallbackSourceColumn = static_cast<uint32_t>(defIt->second->sourceColumn);
      }
    }
    const auto sourceRangesIt = instructionSourceRangesByFunction.find(loweredFunction.name);
    const std::vector<InstructionSourceRange> *sourceRanges =
        sourceRangesIt == instructionSourceRangesByFunction.end() ? nullptr : &sourceRangesIt->second;
    for (size_t instructionIndex = 0; instructionIndex < loweredFunction.instructions.size(); ++instructionIndex) {
      auto &instruction = loweredFunction.instructions[instructionIndex];
      if (nextInstructionDebugId > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
        error = "too many IR instructions for debug id metadata";
        return false;
      }
      instruction.debugId = static_cast<uint32_t>(nextInstructionDebugId);
      uint32_t sourceLine = fallbackSourceLine;
      uint32_t sourceColumn = fallbackSourceColumn;
      IrSourceMapProvenance sourceProvenance = IrSourceMapProvenance::SyntheticIr;
      if (sourceRanges != nullptr) {
        const InstructionSourceRange *bestRange = nullptr;
        for (const auto &range : *sourceRanges) {
          if (range.beginIndex > instructionIndex) {
            break;
          }
          if (instructionIndex < range.endIndex) {
            if (bestRange == nullptr) {
              bestRange = &range;
            } else {
              const size_t bestWidth = bestRange->endIndex - bestRange->beginIndex;
              const size_t width = range.endIndex - range.beginIndex;
              if (width < bestWidth || (width == bestWidth && range.beginIndex >= bestRange->beginIndex)) {
                bestRange = &range;
              }
            }
          }
        }
        if (bestRange != nullptr) {
          sourceLine = bestRange->line;
          sourceColumn = bestRange->column;
          sourceProvenance = bestRange->provenance;
        }
      }
      out.instructionSourceMap.push_back(
          {instruction.debugId, sourceLine, sourceColumn, sourceProvenance});
      ++nextInstructionDebugId;
    }
  }

  out.stringTable = std::move(stringTable);
  return true;
}
