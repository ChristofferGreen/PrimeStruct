    const auto bufferStoreResult = ir_lowerer::tryEmitBufferStoreStatement(
        stmt,
        localsIn,
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferExprKind(valueExpr, valueLocals); },
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
        [&]() { return allocTempLocal(); },
        function.instructions,
        error);
    if (bufferStoreResult == ir_lowerer::BufferStoreStatementEmitResult::Error) {
      return false;
    }
    if (bufferStoreResult == ir_lowerer::BufferStoreStatementEmitResult::Emitted) {
      return true;
    }
    const auto dispatchResult = ir_lowerer::tryEmitDispatchStatement(
        stmt,
        localsIn,
        [&](const Expr &valueExpr) { return resolveExprPath(valueExpr); },
        [&](const std::string &path) -> const Definition * {
          auto it = defMap.find(path);
          return it == defMap.end() ? nullptr : it->second;
        },
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferExprKind(valueExpr, valueLocals); },
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
        [&]() { return allocTempLocal(); },
        [&](const Expr &callExpr, const Definition &callee, const LocalMap &callLocals, bool expectValue) {
          return emitInlineDefinitionCall(callExpr, callee, callLocals, expectValue);
        },
        function.instructions,
        error);
    if (dispatchResult == ir_lowerer::DispatchStatementEmitResult::Error) {
      return false;
    }
    if (dispatchResult == ir_lowerer::DispatchStatementEmitResult::Emitted) {
      return true;
    }
    const auto directCallResult = ir_lowerer::tryEmitDirectCallStatement(
        stmt,
        localsIn,
        [&](const Expr &callExpr, const LocalMap &callLocals) { return isArrayCountCall(callExpr, callLocals); },
        [&](const Expr &callExpr, const LocalMap &callLocals) { return isStringCountCall(callExpr, callLocals); },
        [&](const Expr &callExpr, const LocalMap &callLocals) { return isVectorCapacityCall(callExpr, callLocals); },
        [&](const Expr &callExpr, const LocalMap &callLocals) { return resolveMethodCallDefinition(callExpr, callLocals); },
        [&](const Expr &callExpr) { return resolveDefinitionCall(callExpr); },
        [&](const std::string &definitionPath, ReturnInfo &returnInfo) { return getReturnInfo(definitionPath, returnInfo); },
        [&](const Expr &callExpr, const Definition &callee, const LocalMap &callLocals, bool expectValue) {
          return emitInlineDefinitionCall(callExpr, callee, callLocals, expectValue);
        },
        function.instructions,
        error);
    if (directCallResult == ir_lowerer::DirectCallStatementEmitResult::Error) {
      return false;
    }
    if (directCallResult == ir_lowerer::DirectCallStatementEmitResult::Emitted) {
      return true;
    }
    const auto assignOrExprResult = ir_lowerer::emitAssignOrExprStatementWithPop(
        stmt,
        localsIn,
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
        function.instructions);
    if (assignOrExprResult == ir_lowerer::AssignOrExprStatementEmitResult::Error) {
      return false;
    }
    return true;
  };

  std::optional<OnErrorHandler> entryOnError;
  auto entryOnErrorIt = onErrorByDef.find(entryDef->fullPath);
  if (entryOnErrorIt != onErrorByDef.end()) {
    entryOnError = entryOnErrorIt->second;
  }
  std::optional<ResultReturnInfo> entryResult;
  if (entryHasResultInfo) {
    entryResult = entryResultInfo;
  }
  auto emitEntryStatement = [&](const Expr &stmt) -> bool {
    const size_t startInstructionIndex = function.instructions.size();
    if (!emitStatement(stmt, locals)) {
      return false;
    }
    appendInstructionSourceRange(function.name, stmt, startInstructionIndex, function.instructions.size());
    return true;
  };
  const auto entryExecutionResult = ir_lowerer::emitEntryCallableExecutionWithCleanup(
      *entryDef,
      returnsVoid,
      sawReturn,
      currentOnError,
      entryOnError,
      currentReturnResult,
      entryResult,
      emitEntryStatement,
      [&]() { pushFileScope(); },
      [&]() { emitFileScopeCleanup(fileScopeStack.back()); },
      [&]() { popFileScope(); },
      function.instructions,
      error);
  if (entryExecutionResult == ir_lowerer::EntryCallableExecutionResult::Error) {
    return false;
  }

  const auto functionTableResult = ir_lowerer::finalizeEntryFunctionTableAndLowerCallables(
      program,
      *entryDef,
      function,
      loweredCallTargets,
      [&](const Definition &def) { return isStructDefinition(def); },
      [&](const std::string &definitionPath, ReturnInfo &returnInfo) { return getReturnInfo(definitionPath, returnInfo); },
      defaultEffects,
      entryDefaultEffects,
      [&](const Expr &expr) { return isTailCallCandidate(expr); },
      [&]() {
        onErrorTempCounter = 0;
        sawReturn = false;
        activeInlineContext = nullptr;
        inlineStack.clear();
        fileScopeStack.clear();
        currentOnError.reset();
        currentReturnResult.reset();
      },
      [&](const Definition &def, int32_t &definitionNextLocal, LocalMap &definitionLocals, Expr &callExpr, std::string &callError) {
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
      [&](const Expr &callExpr, const Definition &def, const LocalMap &definitionLocals, bool expectValue) {
        return emitInlineDefinitionCall(callExpr, def, definitionLocals, expectValue);
      },
      nextLocal,
      out.functions,
      out.entryIndex,
      error);
  if (functionTableResult == ir_lowerer::FunctionTableFinalizationResult::Error) {
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
