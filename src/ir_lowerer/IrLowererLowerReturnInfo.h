    bool hasReturnTransformLocal = false;
    bool hasReturnAuto = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "struct" || transform.name == "pod" || transform.name == "handle" ||
          transform.name == "gpu_lane" || transform.name == "no_padding" ||
          transform.name == "platform_independent_padding") {
        info.returnsVoid = true;
        hasReturnTransformLocal = true;
        break;
      }
      if (transform.name != "return") {
        continue;
      }
      hasReturnTransformLocal = true;
      if (transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string &typeName = transform.templateArgs.front();
      if (typeName == "auto") {
        hasReturnAuto = true;
        continue;
      }
      bool resultHasValue = false;
      LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
      std::string resultErrorType;
      if (parseResultTypeName(typeName, resultHasValue, resultValueKind, resultErrorType)) {
        info.returnsArray = false;
        info.returnsVoid = false;
        info.isResult = true;
        info.resultHasValue = resultHasValue;
        info.resultErrorType = resultErrorType;
        info.kind = resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
        break;
      }
      if (typeName == "void") {
        info.returnsVoid = true;
        break;
      }
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(typeName, base, arg) && base == "array") {
        info.returnsArray = true;
        info.kind = valueKindFromTypeName(arg);
        info.returnsVoid = false;
        break;
      }
      std::string structPath;
      StructArrayInfo structInfo;
      if (resolveStructTypeName(typeName, def.namespacePrefix, structPath) &&
          resolveStructArrayInfoFromPath(structPath, structInfo)) {
        info.returnsArray = true;
        info.kind = structInfo.elementKind;
        info.returnsVoid = false;
        break;
      }
      info.returnsArray = false;
      info.kind = valueKindFromTypeName(typeName);
      info.returnsVoid = false;
      break;
    }

    auto inferBindingIntoLocals = [&](const Expr &bindingExpr,
                                      bool isParameter,
                                      LocalMap &activeLocals,
                                      std::string &inferError) -> bool {
      return ir_lowerer::inferReturnInferenceBindingIntoLocals(bindingExpr,
                                                               isParameter,
                                                               def.fullPath,
                                                               activeLocals,
                                                               isBindingMutable,
                                                               bindingKind,
                                                               hasExplicitBindingTypeTransform,
                                                               bindingValueKind,
                                                               inferExprKind,
                                                               isFileErrorBinding,
                                                               applyStructArrayInfo,
                                                               applyStructValueInfo,
                                                               inferStructExprPath,
                                                               isStringBinding,
                                                               inferError);
    };

    if (hasReturnTransformLocal) {
      if (hasReturnAuto) {
        ReturnInferenceOptions options;
        options.missingReturnBehavior = MissingReturnBehavior::Error;
        options.includeDefinitionReturnExpr = true;
        if (!ir_lowerer::inferDefinitionReturnType(
                def,
                LocalMap{},
                inferBindingIntoLocals,
                [&](const Expr &expr, const LocalMap &localsIn) { return inferExprKind(expr, localsIn); },
                [&](const Expr &expr, const LocalMap &localsIn) { return inferArrayElementKind(expr, localsIn); },
                [&](const Expr &expr, Expr &expanded, std::string &inferError) {
                  return lowerMatchToIf(expr, expanded, inferError);
                },
                options,
                info,
                error)) {
          returnInferenceStack.erase(path);
          return false;
        }
      } else if (!info.returnsVoid) {
        if (info.kind == LocalInfo::ValueKind::Unknown) {
          error = "native backend does not support return type on " + def.fullPath;
          returnInferenceStack.erase(path);
          return false;
        }
        if (info.returnsArray && info.kind == LocalInfo::ValueKind::String) {
          error = "native backend does not support string array return types on " + def.fullPath;
          returnInferenceStack.erase(path);
          return false;
        }
      }
    } else {
      if (!def.hasReturnStatement) {
        info.returnsVoid = true;
      } else {
        ReturnInferenceOptions options;
        options.missingReturnBehavior = MissingReturnBehavior::Void;
        options.includeDefinitionReturnExpr = false;
        if (!ir_lowerer::inferDefinitionReturnType(
                def,
                LocalMap{},
                inferBindingIntoLocals,
                [&](const Expr &expr, const LocalMap &localsIn) { return inferExprKind(expr, localsIn); },
                [&](const Expr &expr, const LocalMap &localsIn) { return inferArrayElementKind(expr, localsIn); },
                [&](const Expr &expr, Expr &expanded, std::string &inferError) {
                  return lowerMatchToIf(expr, expanded, inferError);
                },
                options,
                info,
                error)) {
          returnInferenceStack.erase(path);
          return false;
        }
      }
    }

    returnInferenceStack.erase(path);
    returnInfoCache.emplace(path, info);
    outInfo = info;
    return true;
  };

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
      });
  auto buildOrderedCallArguments = [&](const Expr &callExpr,
                                       const std::vector<Expr> &params,
                                       std::vector<const Expr *> &ordered) -> bool {
    return ir_lowerer::buildOrderedCallArguments(callExpr, params, ordered, error);
  };

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
