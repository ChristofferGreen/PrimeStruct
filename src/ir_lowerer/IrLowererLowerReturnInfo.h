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

    if (hasReturnTransformLocal) {
      if (hasReturnAuto) {
        LocalMap localsForInference;
        for (const auto &param : def.parameters) {
          LocalInfo paramInfo;
          paramInfo.index = 0;
          paramInfo.isMutable = isBindingMutable(param);
          paramInfo.kind = bindingKind(param);
          if (hasExplicitBindingTypeTransform(param)) {
            paramInfo.valueKind = bindingValueKind(param, paramInfo.kind);
          } else if (param.args.size() == 1 && paramInfo.kind == LocalInfo::Kind::Value) {
            paramInfo.valueKind = inferExprKind(param.args.front(), localsForInference);
            if (paramInfo.valueKind == LocalInfo::ValueKind::Unknown) {
              paramInfo.valueKind = LocalInfo::ValueKind::Int32;
            }
          } else {
            paramInfo.valueKind = bindingValueKind(param, paramInfo.kind);
          }
          paramInfo.isFileError = isFileErrorBinding(param);
          applyStructArrayInfo(param, paramInfo);
          applyStructValueInfo(param, paramInfo);
          if (isStringBinding(param)) {
            if (paramInfo.kind != LocalInfo::Kind::Value) {
              error = "native backend does not support string pointers or references";
              returnInferenceStack.erase(path);
              return false;
            }
          }
          if (paramInfo.valueKind == LocalInfo::ValueKind::Unknown && paramInfo.structTypeName.empty()) {
            error = "native backend requires typed parameters on " + def.fullPath;
            returnInferenceStack.erase(path);
            return false;
          }
          localsForInference.emplace(param.name, paramInfo);
        }

        LocalInfo::ValueKind inferred = LocalInfo::ValueKind::Unknown;
        bool inferredArray = false;
        LocalInfo::ValueKind inferredArrayKind = LocalInfo::ValueKind::Unknown;
        bool sawReturnLocal = false;
        bool inferredVoid = false;
        auto recordInferredReturn = [&](const Expr &valueExpr, const LocalMap &activeLocals) -> bool {
          LocalInfo::ValueKind arrayKind = inferArrayElementKind(valueExpr, activeLocals);
          if (arrayKind != LocalInfo::ValueKind::Unknown) {
            if (arrayKind == LocalInfo::ValueKind::String) {
              error = "native backend does not support string array return types on " + def.fullPath;
              return false;
            }
            if (!inferredArray && inferred == LocalInfo::ValueKind::Unknown) {
              inferredArray = true;
              inferredArrayKind = arrayKind;
              return true;
            }
            if (inferredArray && inferredArrayKind == arrayKind) {
              return true;
            }
            error = "conflicting return types on " + def.fullPath;
            return false;
          }
          LocalInfo::ValueKind kind = inferExprKind(valueExpr, activeLocals);
          if (kind == LocalInfo::ValueKind::Unknown) {
            error = "unable to infer return type on " + def.fullPath;
            return false;
          }
          if (inferredArray) {
            error = "conflicting return types on " + def.fullPath;
            return false;
          }
          if (inferred == LocalInfo::ValueKind::Unknown) {
            inferred = kind;
            return true;
          }
          if (inferred != kind) {
            error = "conflicting return types on " + def.fullPath;
            return false;
          }
          return true;
        };
        std::function<bool(const Expr &, LocalMap &)> inferStatement;
        inferStatement = [&](const Expr &stmt, LocalMap &activeLocals) -> bool {
          if (stmt.isBinding) {
            LocalInfo bindingInfo;
            bindingInfo.index = 0;
            bindingInfo.isMutable = isBindingMutable(stmt);
            bindingInfo.kind = bindingKind(stmt);
            if (hasExplicitBindingTypeTransform(stmt)) {
              bindingInfo.valueKind = bindingValueKind(stmt, bindingInfo.kind);
            } else if (stmt.args.size() == 1 && bindingInfo.kind == LocalInfo::Kind::Value) {
              bindingInfo.valueKind = inferExprKind(stmt.args.front(), activeLocals);
              if (bindingInfo.valueKind == LocalInfo::ValueKind::Unknown) {
                bindingInfo.valueKind = LocalInfo::ValueKind::Int32;
              }
            } else {
              bindingInfo.valueKind = LocalInfo::ValueKind::Unknown;
            }
            bindingInfo.isFileError = isFileErrorBinding(stmt);
            applyStructArrayInfo(stmt, bindingInfo);
            applyStructValueInfo(stmt, bindingInfo);
            if (bindingInfo.structTypeName.empty() && bindingInfo.kind == LocalInfo::Kind::Value &&
                bindingInfo.valueKind == LocalInfo::ValueKind::Unknown && stmt.args.size() == 1) {
              std::string inferredStruct = inferStructExprPath(stmt.args.front(), activeLocals);
              if (!inferredStruct.empty()) {
                bindingInfo.structTypeName = inferredStruct;
              }
            }
            if (bindingInfo.valueKind == LocalInfo::ValueKind::String && bindingInfo.kind != LocalInfo::Kind::Value) {
              error = "native backend does not support string pointers or references";
              return false;
            }
            if (bindingInfo.valueKind == LocalInfo::ValueKind::Unknown && bindingInfo.structTypeName.empty()) {
              error = "native backend requires typed bindings on " + def.fullPath;
              return false;
            }
            activeLocals.emplace(stmt.name, bindingInfo);
            return true;
          }
          if (isReturnCall(stmt)) {
            sawReturnLocal = true;
            if (stmt.args.empty()) {
              inferredVoid = true;
              return true;
            }
            return recordInferredReturn(stmt.args.front(), activeLocals);
          }
          if (isMatchCall(stmt)) {
            Expr expanded;
            if (!lowerMatchToIf(stmt, expanded, error)) {
              return false;
            }
            return inferStatement(expanded, activeLocals);
          }
          if (isIfCall(stmt) && stmt.args.size() == 3) {
            const Expr &thenBlock = stmt.args[1];
            const Expr &elseBlock = stmt.args[2];
            auto walkBlock = [&](const Expr &block) -> bool {
              LocalMap blockLocals = activeLocals;
              for (const auto &bodyStmt : block.bodyArguments) {
                if (!inferStatement(bodyStmt, blockLocals)) {
                  return false;
                }
              }
              return true;
            };
            if (!walkBlock(thenBlock)) {
              return false;
            }
            if (!walkBlock(elseBlock)) {
              return false;
            }
          }
          if (isRepeatCall(stmt)) {
            LocalMap blockLocals = activeLocals;
            for (const auto &bodyStmt : stmt.bodyArguments) {
              if (!inferStatement(bodyStmt, blockLocals)) {
                return false;
              }
            }
          }
          return true;
        };

        LocalMap locals = localsForInference;
        for (const auto &stmt : def.statements) {
          if (!inferStatement(stmt, locals)) {
            returnInferenceStack.erase(path);
            return false;
          }
        }
        if (def.returnExpr.has_value()) {
          sawReturnLocal = true;
          if (!recordInferredReturn(*def.returnExpr, locals)) {
            returnInferenceStack.erase(path);
            return false;
          }
        }
        if (!sawReturnLocal) {
          error = "unable to infer return type on " + def.fullPath;
          returnInferenceStack.erase(path);
          return false;
        }
        if (inferredVoid) {
          if (inferred != LocalInfo::ValueKind::Unknown || inferredArray) {
            error = "conflicting return types on " + def.fullPath;
            returnInferenceStack.erase(path);
            return false;
          }
          info.returnsVoid = true;
        } else {
          info.returnsVoid = false;
          if (inferredArray) {
            info.returnsArray = true;
            info.kind = inferredArrayKind;
          } else {
            info.returnsArray = false;
            info.kind = inferred;
          }
          if (info.kind == LocalInfo::ValueKind::Unknown) {
            error = "unable to infer return type on " + def.fullPath;
            returnInferenceStack.erase(path);
            return false;
          }
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
        LocalMap localsForInference;
        for (const auto &param : def.parameters) {
          LocalInfo paramInfo;
          paramInfo.index = 0;
          paramInfo.isMutable = isBindingMutable(param);
          paramInfo.kind = bindingKind(param);
          if (hasExplicitBindingTypeTransform(param)) {
            paramInfo.valueKind = bindingValueKind(param, paramInfo.kind);
          } else if (param.args.size() == 1 && paramInfo.kind == LocalInfo::Kind::Value) {
            paramInfo.valueKind = inferExprKind(param.args.front(), localsForInference);
            if (paramInfo.valueKind == LocalInfo::ValueKind::Unknown) {
              paramInfo.valueKind = LocalInfo::ValueKind::Int32;
            }
          } else {
            paramInfo.valueKind = bindingValueKind(param, paramInfo.kind);
          }
          paramInfo.isFileError = isFileErrorBinding(param);
          applyStructArrayInfo(param, paramInfo);
          applyStructValueInfo(param, paramInfo);
          if (isStringBinding(param)) {
            if (paramInfo.kind != LocalInfo::Kind::Value) {
              error = "native backend does not support string pointers or references";
              returnInferenceStack.erase(path);
              return false;
            }
          }
          if (paramInfo.valueKind == LocalInfo::ValueKind::Unknown && paramInfo.structTypeName.empty()) {
            error = "native backend requires typed parameters on " + def.fullPath;
            returnInferenceStack.erase(path);
            return false;
          }
          localsForInference.emplace(param.name, paramInfo);
        }

        LocalInfo::ValueKind inferred = LocalInfo::ValueKind::Unknown;
        bool inferredArray = false;
        LocalInfo::ValueKind inferredArrayKind = LocalInfo::ValueKind::Unknown;
        bool sawReturnLocal = false;
        bool inferredVoid = false;
        std::function<bool(const Expr &, LocalMap &)> inferStatement;
        inferStatement = [&](const Expr &stmt, LocalMap &activeLocals) -> bool {
          if (stmt.isBinding) {
            LocalInfo bindingInfo;
            bindingInfo.index = 0;
            bindingInfo.isMutable = isBindingMutable(stmt);
            bindingInfo.kind = bindingKind(stmt);
            if (hasExplicitBindingTypeTransform(stmt)) {
              bindingInfo.valueKind = bindingValueKind(stmt, bindingInfo.kind);
            } else if (stmt.args.size() == 1 && bindingInfo.kind == LocalInfo::Kind::Value) {
              bindingInfo.valueKind = inferExprKind(stmt.args.front(), activeLocals);
              if (bindingInfo.valueKind == LocalInfo::ValueKind::Unknown) {
                bindingInfo.valueKind = LocalInfo::ValueKind::Int32;
              }
            } else {
              bindingInfo.valueKind = LocalInfo::ValueKind::Unknown;
            }
            bindingInfo.isFileError = isFileErrorBinding(stmt);
            applyStructArrayInfo(stmt, bindingInfo);
            applyStructValueInfo(stmt, bindingInfo);
            if (bindingInfo.structTypeName.empty() && bindingInfo.kind == LocalInfo::Kind::Value &&
                bindingInfo.valueKind == LocalInfo::ValueKind::Unknown && stmt.args.size() == 1) {
              std::string inferredStruct = inferStructExprPath(stmt.args.front(), activeLocals);
              if (!inferredStruct.empty()) {
                bindingInfo.structTypeName = inferredStruct;
              }
            }
            if (bindingInfo.valueKind == LocalInfo::ValueKind::String && bindingInfo.kind != LocalInfo::Kind::Value) {
              error = "native backend does not support string pointers or references";
              return false;
            }
            if (bindingInfo.valueKind == LocalInfo::ValueKind::Unknown && bindingInfo.structTypeName.empty()) {
              error = "native backend requires typed bindings on " + def.fullPath;
              return false;
            }
            activeLocals.emplace(stmt.name, bindingInfo);
            return true;
          }
          if (isReturnCall(stmt)) {
            sawReturnLocal = true;
            if (stmt.args.empty()) {
              inferredVoid = true;
              return true;
            }
            LocalInfo::ValueKind arrayKind = inferArrayElementKind(stmt.args.front(), activeLocals);
            if (arrayKind != LocalInfo::ValueKind::Unknown) {
              if (arrayKind == LocalInfo::ValueKind::String) {
                error = "native backend does not support string array return types on " + def.fullPath;
                return false;
              }
              if (!inferredArray && inferred == LocalInfo::ValueKind::Unknown) {
                inferredArray = true;
                inferredArrayKind = arrayKind;
                return true;
              }
              if (inferredArray && inferredArrayKind == arrayKind) {
                return true;
              }
              error = "conflicting return types on " + def.fullPath;
              return false;
            }
            LocalInfo::ValueKind kind = inferExprKind(stmt.args.front(), activeLocals);
            if (kind == LocalInfo::ValueKind::Unknown) {
              error = "unable to infer return type on " + def.fullPath;
              return false;
            }
            if (inferredArray) {
              error = "conflicting return types on " + def.fullPath;
              return false;
            }
            if (inferred == LocalInfo::ValueKind::Unknown) {
              inferred = kind;
              return true;
            }
            if (inferred != kind) {
              error = "conflicting return types on " + def.fullPath;
              return false;
            }
            return true;
          }
          if (isMatchCall(stmt)) {
            Expr expanded;
            if (!lowerMatchToIf(stmt, expanded, error)) {
              return false;
            }
            return inferStatement(expanded, activeLocals);
          }
          if (isIfCall(stmt) && stmt.args.size() == 3) {
            const Expr &thenBlock = stmt.args[1];
            const Expr &elseBlock = stmt.args[2];
            auto walkBlock = [&](const Expr &block) -> bool {
              LocalMap blockLocals = activeLocals;
              for (const auto &bodyStmt : block.bodyArguments) {
                if (!inferStatement(bodyStmt, blockLocals)) {
                  return false;
                }
              }
              return true;
            };
            if (!walkBlock(thenBlock)) {
              return false;
            }
            if (!walkBlock(elseBlock)) {
              return false;
            }
          }
          if (isRepeatCall(stmt)) {
            LocalMap blockLocals = activeLocals;
            for (const auto &bodyStmt : stmt.bodyArguments) {
              if (!inferStatement(bodyStmt, blockLocals)) {
                return false;
              }
            }
          }
          return true;
        };

        LocalMap locals = localsForInference;
        for (const auto &stmt : def.statements) {
          if (!inferStatement(stmt, locals)) {
            returnInferenceStack.erase(path);
            return false;
          }
        }
        if (!sawReturnLocal || inferredVoid) {
          if (sawReturnLocal && (inferred != LocalInfo::ValueKind::Unknown || inferredArray)) {
            error = "conflicting return types on " + def.fullPath;
            returnInferenceStack.erase(path);
            return false;
          }
          info.returnsVoid = true;
        } else {
          info.returnsVoid = false;
          if (inferredArray) {
            info.returnsArray = true;
            info.kind = inferredArrayKind;
          } else {
            info.returnsArray = false;
            info.kind = inferred;
          }
          if (info.kind == LocalInfo::ValueKind::Unknown) {
            error = "unable to infer return type on " + def.fullPath;
            returnInferenceStack.erase(path);
            return false;
          }
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
    if (slotCount <= 0) {
      return true;
    }
    for (int32_t i = 0; i < slotCount; ++i) {
      const uint64_t offsetBytes = static_cast<uint64_t>(i * 16);
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
      if (offsetBytes != 0) {
        function.instructions.push_back({IrOpcode::PushI64, offsetBytes});
        function.instructions.push_back({IrOpcode::AddI64, 0});
      }
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(srcPtrLocal)});
      if (offsetBytes != 0) {
        function.instructions.push_back({IrOpcode::PushI64, offsetBytes});
        function.instructions.push_back({IrOpcode::AddI64, 0});
      }
      function.instructions.push_back({IrOpcode::LoadIndirect, 0});
      function.instructions.push_back({IrOpcode::StoreIndirect, 0});
      function.instructions.push_back({IrOpcode::Pop, 0});
    }
    return true;
  };
  auto emitStructCopySlots = [&](int32_t destBaseLocal, int32_t srcPtrLocal, int32_t slotCount) -> bool {
    const int32_t destPtrLocal = allocTempLocal();
    function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(destBaseLocal)});
    function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
    return emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, slotCount);
  };
  auto emitFileCloseIfValid = [&](int32_t localIndex) {
    function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(localIndex)});
    function.instructions.push_back({IrOpcode::PushI64, 0});
    function.instructions.push_back({IrOpcode::CmpGeI64, 0});
    size_t jumpSkip = function.instructions.size();
    function.instructions.push_back({IrOpcode::JumpIfZero, 0});
    function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(localIndex)});
    function.instructions.push_back({IrOpcode::FileClose, 0});
    function.instructions.push_back({IrOpcode::Pop, 0});
    size_t skipIndex = function.instructions.size();
    function.instructions[jumpSkip].imm = static_cast<int32_t>(skipIndex);
  };
  auto emitFileScopeCleanup = [&](const std::vector<int32_t> &scope) {
    for (auto it = scope.rbegin(); it != scope.rend(); ++it) {
      emitFileCloseIfValid(*it);
    }
  };
  auto emitFileScopeCleanupAll = [&]() {
    for (auto it = fileScopeStack.rbegin(); it != fileScopeStack.rend(); ++it) {
      emitFileScopeCleanup(*it);
    }
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
    char *end = nullptr;
    const char *text = expr.floatValue.c_str();
    double value = std::strtod(text, &end);
    if (end == text || (end && *end != '\0')) {
      error = "invalid float literal";
      return false;
    }
    if (expr.floatWidth == 64) {
      uint64_t bits = 0;
      std::memcpy(&bits, &value, sizeof(bits));
      function.instructions.push_back({IrOpcode::PushF64, bits});
      return true;
    }
    float f32 = static_cast<float>(value);
    uint32_t bits = 0;
    std::memcpy(&bits, &f32, sizeof(bits));
    function.instructions.push_back({IrOpcode::PushF32, static_cast<uint64_t>(bits)});
    return true;
  };

  auto emitCompareToZero = [&](LocalInfo::ValueKind kind, bool equals) -> bool {
    if (kind == LocalInfo::ValueKind::Int64 || kind == LocalInfo::ValueKind::UInt64) {
      function.instructions.push_back({IrOpcode::PushI64, 0});
      function.instructions.push_back({equals ? IrOpcode::CmpEqI64 : IrOpcode::CmpNeI64, 0});
      return true;
    }
    if (kind == LocalInfo::ValueKind::Float64) {
      function.instructions.push_back({IrOpcode::PushF64, 0});
      function.instructions.push_back({equals ? IrOpcode::CmpEqF64 : IrOpcode::CmpNeF64, 0});
      return true;
    }
    if (kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Bool) {
      function.instructions.push_back({IrOpcode::PushI32, 0});
      function.instructions.push_back({equals ? IrOpcode::CmpEqI32 : IrOpcode::CmpNeI32, 0});
      return true;
    }
    if (kind == LocalInfo::ValueKind::Float32) {
      function.instructions.push_back({IrOpcode::PushF32, 0});
      function.instructions.push_back({equals ? IrOpcode::CmpEqF32 : IrOpcode::CmpNeF32, 0});
      return true;
    }
    error = "boolean conversion requires numeric operand";
    return false;
  };

  auto resolveDefinitionCall = [&](const Expr &callExpr) -> const Definition * {
    return ir_lowerer::resolveDefinitionCall(callExpr, defMap, resolveExprPath);
  };
  auto resolveResultExprInfo = [&](const Expr &expr, const LocalMap &localsIn, ResultExprInfo &out) -> bool {
    auto lookupLocal = [&](const std::string &name) -> ir_lowerer::LocalResultInfo {
      ir_lowerer::LocalResultInfo local;
      auto it = localsIn.find(name);
      if (it == localsIn.end()) {
        return local;
      }
      local.found = true;
      local.isResult = it->second.isResult;
      local.resultHasValue = it->second.resultHasValue;
      local.resultErrorType = it->second.resultErrorType;
      local.isFileHandle = it->second.isFileHandle;
      return local;
    };
    auto resolveMethodCall = [&](const Expr &callExpr) -> const Definition * {
      return resolveMethodCallDefinition(callExpr, localsIn);
    };
    auto lookupDefinitionResult = [&](const std::string &path, ResultExprInfo &resultOut) -> bool {
      ReturnInfo info;
      if (!getReturnInfo || !getReturnInfo(path, info) || !info.isResult) {
        return false;
      }
      resultOut.isResult = true;
      resultOut.hasValue = info.resultHasValue;
      resultOut.errorType = info.resultErrorType;
      return true;
    };
    return ir_lowerer::resolveResultExprInfo(
        expr, lookupLocal, resolveMethodCall, resolveDefinitionCall, lookupDefinitionResult, out);
  };
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
    sourceOut = LocalInfo::StringSource::None;
    stringIndexOut = -1;
    argvCheckedOut = true;
    ir_lowerer::StringCallSource helperSource = ir_lowerer::StringCallSource::None;
    ir_lowerer::StringCallEmitResult helperResult = ir_lowerer::emitLiteralOrBindingStringCallValue(
        arg,
        internString,
        [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
        [&](const std::string &name) -> ir_lowerer::StringBindingInfo {
          ir_lowerer::StringBindingInfo binding;
          auto it = callerLocals.find(name);
          if (it == callerLocals.end()) {
            return binding;
          }
          binding.found = true;
          binding.isString = (it->second.valueKind == LocalInfo::ValueKind::String);
          binding.localIndex = it->second.index;
          if (it->second.stringSource == LocalInfo::StringSource::TableIndex) {
            binding.source = ir_lowerer::StringCallSource::TableIndex;
          } else if (it->second.stringSource == LocalInfo::StringSource::ArgvIndex) {
            binding.source = ir_lowerer::StringCallSource::ArgvIndex;
          } else if (it->second.stringSource == LocalInfo::StringSource::RuntimeIndex) {
            binding.source = ir_lowerer::StringCallSource::RuntimeIndex;
          } else {
            binding.source = ir_lowerer::StringCallSource::None;
          }
          binding.stringIndex = it->second.stringIndex;
          binding.argvChecked = it->second.argvChecked;
          return binding;
        },
        helperSource,
        stringIndexOut,
        argvCheckedOut,
        error);
    if (helperResult == ir_lowerer::StringCallEmitResult::Error) {
      return false;
    }
    if (helperResult == ir_lowerer::StringCallEmitResult::Handled) {
      if (helperSource == ir_lowerer::StringCallSource::TableIndex) {
        sourceOut = LocalInfo::StringSource::TableIndex;
      } else if (helperSource == ir_lowerer::StringCallSource::ArgvIndex) {
        sourceOut = LocalInfo::StringSource::ArgvIndex;
      } else if (helperSource == ir_lowerer::StringCallSource::RuntimeIndex) {
        sourceOut = LocalInfo::StringSource::RuntimeIndex;
      } else {
        sourceOut = LocalInfo::StringSource::None;
      }
      return true;
    }
    helperSource = ir_lowerer::StringCallSource::None;
    helperResult = ir_lowerer::emitCallStringCallValue(
        arg,
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
        [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
        [&]() { return function.instructions.size(); },
        [&](size_t index, int32_t imm) { function.instructions.at(index).imm = imm; },
        [&]() { emitArrayIndexOutOfBounds(); },
        helperSource,
        argvCheckedOut,
        error);
    if (helperResult == ir_lowerer::StringCallEmitResult::Error) {
      return false;
    }
    if (helperResult == ir_lowerer::StringCallEmitResult::Handled) {
      if (helperSource == ir_lowerer::StringCallSource::TableIndex) {
        sourceOut = LocalInfo::StringSource::TableIndex;
      } else if (helperSource == ir_lowerer::StringCallSource::ArgvIndex) {
        sourceOut = LocalInfo::StringSource::ArgvIndex;
      } else if (helperSource == ir_lowerer::StringCallSource::RuntimeIndex) {
        sourceOut = LocalInfo::StringSource::RuntimeIndex;
      } else {
        sourceOut = LocalInfo::StringSource::None;
      }
      return true;
    }
    error = "native backend requires string arguments to use string literals, bindings, or entry args";
    return false;
  };
