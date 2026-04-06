  struct InlineContext {
    std::string defPath;
    bool returnsVoid = false;
    bool returnsArray = false;
    LocalInfo::ValueKind returnKind = LocalInfo::ValueKind::Unknown;
    int32_t returnLocal = -1;
    std::vector<size_t> returnJumps;
  };

  InlineContext *activeInlineContext = nullptr;
  std::unordered_set<std::string> inlineStack;

  std::function<bool(const Expr &, const LocalMap &)> emitExpr;
  std::function<bool(const Expr &, LocalMap &)> emitStatement;
  auto allocTempLocal = [&]() -> int32_t {
    return nextLocal++;
  };
  auto emitStructCopyFromPtrs = [&](int32_t destPtrLocal, int32_t srcPtrLocal, int32_t slotCount) -> bool {
    return ir_lowerer::emitStructCopyFromPtrs(function.instructions, destPtrLocal, srcPtrLocal, slotCount);
  };
  auto emitStructCopySlots = [&](int32_t destBaseLocal, int32_t srcPtrLocal, int32_t slotCount) -> bool {
    return ir_lowerer::emitStructCopySlots(
        function.instructions, destBaseLocal, srcPtrLocal, slotCount, [&]() { return allocTempLocal(); });
  };
  auto emitFileScopeCleanup = [&](const std::vector<int32_t> &scope) {
    ir_lowerer::emitFileScopeCleanup(function.instructions, scope);
  };
  auto emitFileScopeCleanupAll = [&]() {
    ir_lowerer::emitAllFileScopeCleanup(function.instructions, fileScopeStack);
  };
  auto pushFileScope = [&]() { fileScopeStack.emplace_back(); };
  auto popFileScope = [&]() { fileScopeStack.pop_back(); };

  auto emitBlock = [&](const Expr &blockExpr, LocalMap &blockLocals) -> bool {
    if (blockExpr.kind != Expr::Kind::Call) {
      error = "native backend expects if branch blocks to be calls";
      return false;
    }
    if (!blockExpr.args.empty()) {
      error = "native backend does not support arguments on if branch blocks";
      return false;
    }
    OnErrorScope onErrorScope(currentOnError, std::nullopt);
    pushFileScope();
    for (const auto &stmt : blockExpr.bodyArguments) {
      if (!emitStatement(stmt, blockLocals)) {
        return false;
      }
    }
    emitFileScopeCleanup(fileScopeStack.back());
    popFileScope();
    return true;
  };

  auto emitFloatLiteral = [&](const Expr &expr) -> bool {
    return ir_lowerer::emitFloatLiteral(function.instructions, expr, error);
  };

  auto emitCompareToZero = [&](LocalInfo::ValueKind kind, bool equals) -> bool {
    return ir_lowerer::emitCompareToZero(function.instructions, kind, equals, error);
  };

  auto resolveDefinitionCall = ir_lowerer::makeResolveDefinitionCall(defMap, resolveExprPath);
  auto resolveResultExprInfo = ir_lowerer::makeResolveResultExprInfoFromLocals(
      [&](const Expr &callExpr, const LocalMap &localsForCall) -> const Definition * {
        return resolveMethodCallDefinition(callExpr, localsForCall);
      },
      resolveDefinitionCall,
      [&](const std::string &path, ReturnInfo &info) -> bool {
        return getReturnInfo && getReturnInfo(path, info);
      },
      [&](const Expr &expr, const LocalMap &localsForKind) -> LocalInfo::ValueKind {
        return inferExprKind(expr, localsForKind);
      },
      &callResolutionAdapters.semanticProductTargets,
      &error);

  auto emitStringValueForCall = [&](const Expr &arg,
                                    const LocalMap &callerLocals,
                                    LocalInfo::StringSource &sourceOut,
                                    int32_t &stringIndexOut,
                                    bool &argvCheckedOut) -> bool {
    return ir_lowerer::emitStringValueForCallFromLocals(
        arg,
        callerLocals,
        internString,
        [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
        [&](const Expr &callExpr, std::string &accessName) { return getBuiltinArrayAccessName(callExpr, accessName); },
        [&](const Expr &targetExpr) { return isEntryArgsName(targetExpr, callerLocals); },
        [&](const Expr &indexExpr,
            const std::string &accessName,
            ir_lowerer::StringIndexOps &ops,
            std::string &helperError) -> bool {
          LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(indexExpr, callerLocals));
          if (!isSupportedIndexKind(indexKind)) {
            helperError = "native backend requires integer indices for " + accessName;
            return false;
          }
          ops.pushZero = pushZeroForIndex(indexKind);
          ops.cmpLt = cmpLtForIndex(indexKind);
          ops.cmpGe = cmpGeForIndex(indexKind);
          ops.skipNegativeCheck = (indexKind == LocalInfo::ValueKind::UInt64);
          return true;
        },
        [&](const Expr &exprToEmit) { return emitExpr(exprToEmit, callerLocals); },
        [&](const Expr &callExpr) { return inferExprKind(callExpr, callerLocals) == LocalInfo::ValueKind::String; },
        [&]() { return allocTempLocal(); },
        [&]() { return function.instructions.size(); },
        [&](size_t index, int32_t imm) { function.instructions.at(index).imm = imm; },
        [&]() { emitArrayIndexOutOfBounds(); },
        sourceOut,
        stringIndexOut,
        argvCheckedOut,
        error);
  };
