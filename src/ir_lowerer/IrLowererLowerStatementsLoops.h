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
    if (isLoopCall(stmt)) {
      if (stmt.args.size() != 2) {
        error = "loop requires count and body";
        return false;
      }
      const Expr &countExpr = stmt.args[0];
      if (!emitExpr(countExpr, localsIn)) {
        return false;
      }
      LocalInfo::ValueKind countKind = LocalInfo::ValueKind::Unknown;
      if (!ir_lowerer::resolveCountedLoopKind(
              inferExprKind(countExpr, localsIn),
              false,
              "loop count requires integer",
              countKind,
              error)) {
        return false;
      }

      ir_lowerer::CountedLoopControl loopControl;
      if (!ir_lowerer::emitCountedLoopPrologue(
              countKind,
              [&]() { return allocTempLocal(); },
              [&]() { return function.instructions.size(); },
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
              [&](size_t instructionIndex, int32_t imm) { function.instructions[instructionIndex].imm = imm; },
              [&]() { emitLoopCountNegative(); },
              loopControl,
              error)) {
        return false;
      }

      const Expr &body = stmt.args[1];
      if (!isLoopBlockEnvelope(body)) {
        error = "loop body requires a block envelope";
        return false;
      }
      OnErrorScope onErrorScope(currentOnError, std::nullopt);
      pushFileScope();
      LocalMap bodyLocals = localsIn;
      for (const auto &bodyStmt : body.bodyArguments) {
        if (!emitStatement(bodyStmt, bodyLocals)) {
          return false;
        }
      }
      emitFileScopeCleanup(fileScopeStack.back());
      popFileScope();

      ir_lowerer::emitCountedLoopIterationStep(
          loopControl,
          [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); });
      ir_lowerer::patchCountedLoopEnd(
          loopControl,
          [&]() { return function.instructions.size(); },
          [&](size_t instructionIndex, int32_t imm) { function.instructions[instructionIndex].imm = imm; });
      return true;
    }
    if (isWhileCall(stmt)) {
      if (stmt.args.size() != 2) {
        error = "while requires condition and body";
        return false;
      }
      const Expr &cond = stmt.args[0];
      const Expr &body = stmt.args[1];
      if (!isLoopBlockEnvelope(body)) {
        error = "while body requires a block envelope";
        return false;
      }
      const size_t checkIndex = function.instructions.size();
      if (!emitExpr(cond, localsIn)) {
        return false;
      }
      LocalInfo::ValueKind condKind = inferExprKind(cond, localsIn);
      if (condKind == LocalInfo::ValueKind::Unknown) {
        std::string builtinComparison;
        if (getBuiltinComparisonName(cond, builtinComparison)) {
          condKind = LocalInfo::ValueKind::Bool;
        }
      }
      if (condKind != LocalInfo::ValueKind::Bool) {
        error = "while condition requires bool";
        return false;
      }
      const size_t jumpEndIndex = function.instructions.size();
      function.instructions.push_back({IrOpcode::JumpIfZero, 0});

      if (!ir_lowerer::emitBodyStatements(
              body.bodyArguments,
              localsIn,
              [&](const Expr &bodyStmt, LocalMap &bodyLocals) { return emitStatement(bodyStmt, bodyLocals); })) {
        return false;
      }

      function.instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(checkIndex)});
      const size_t endIndex = function.instructions.size();
      function.instructions[jumpEndIndex].imm = static_cast<int32_t>(endIndex);
      return true;
    }
    if (isForCall(stmt)) {
      if (stmt.args.size() != 4) {
        error = "for requires init, condition, step, and body";
        return false;
      }
      LocalMap loopLocals = localsIn;
      if (!emitStatement(stmt.args[0], loopLocals)) {
        return false;
      }
      const Expr &cond = stmt.args[1];
      const Expr &step = stmt.args[2];
      const Expr &body = stmt.args[3];
      if (!isLoopBlockEnvelope(body)) {
        error = "for body requires a block envelope";
        return false;
      }
      if (cond.isBinding) {
        auto isSemanticForConditionBindingCandidate = [&](const Expr &bindingExpr) {
          std::string explicitTypeName;
          std::vector<std::string> explicitTemplateArgs;
          if (!extractFirstBindingTypeTransform(
                  bindingExpr, explicitTypeName, explicitTemplateArgs)) {
            return true;
          }
          return trimTemplateTypeText(explicitTypeName) == "auto";
        };
        auto makeSemanticBindingTypeTransform = [&](const std::string &bindingTypeText) {
          Transform semanticTypeTransform;
          std::string semanticTypeBase;
          std::string semanticTypeArgList;
          if (splitTemplateTypeName(bindingTypeText, semanticTypeBase, semanticTypeArgList)) {
            semanticTypeTransform.name = trimTemplateTypeText(semanticTypeBase);
            if (!semanticTypeArgList.empty()) {
              if (!splitTemplateArgs(semanticTypeArgList, semanticTypeTransform.templateArgs)) {
                semanticTypeTransform.templateArgs.push_back(trimTemplateTypeText(semanticTypeArgList));
              }
            }
          } else {
            semanticTypeTransform.name = trimTemplateTypeText(bindingTypeText);
          }
          return semanticTypeTransform;
        };
        Expr semanticForConditionBindingExpr;
        const Expr *conditionBindingForDeclaration = &cond;
        if (callResolutionAdapters.semanticProgram != nullptr &&
            cond.semanticNodeId != 0 &&
            isSemanticForConditionBindingCandidate(cond)) {
          const SemanticProgramBindingFact *bindingFact =
              findSemanticProductBindingFact(
                  callResolutionAdapters.semanticProductTargets, cond);
          std::string bindingTypeText;
          if (bindingFact != nullptr) {
            bindingTypeText = trimTemplateTypeText(std::string(
                semanticProgramResolvePublishedText(
                    *callResolutionAdapters.semanticProgram,
                    bindingFact->bindingTypeTextId,
                    bindingFact->bindingTypeText)));
          }
          if (bindingTypeText.empty()) {
            const std::string scopePath =
                activeInlineContext != nullptr ? activeInlineContext->defPath : function.name;
            error = "missing semantic-product for-condition binding fact: " + scopePath +
                    " -> local " + (cond.name.empty() ? std::string("<unnamed>") : cond.name);
            return false;
          }
          semanticForConditionBindingExpr = cond;
          semanticForConditionBindingExpr.semanticNodeId = 0;
          semanticForConditionBindingExpr.transforms.clear();
          semanticForConditionBindingExpr.transforms.push_back(
              makeSemanticBindingTypeTransform(bindingTypeText));
          conditionBindingForDeclaration = &semanticForConditionBindingExpr;
        }
        if (!ir_lowerer::declareForConditionBinding(
                *conditionBindingForDeclaration,
                loopLocals,
                nextLocal,
                [&](const Expr &bindingExpr) { return isBindingMutable(bindingExpr); },
                [&](const Expr &bindingExpr) { return bindingKind(bindingExpr); },
                [&](const Expr &bindingExpr) { return hasExplicitBindingTypeTransform(bindingExpr); },
                [&](const Expr &bindingExpr, LocalInfo::Kind kind) { return bindingValueKind(bindingExpr, kind); },
                [&](const Expr &valueExpr, const LocalMap &valueLocals) {
                  return inferExprKind(valueExpr, valueLocals);
                },
                [&](const Expr &valueExpr, const LocalMap &valueLocals) {
                  return inferStructExprPath(valueExpr, valueLocals);
                },
                [&](const Expr &bindingExpr, LocalInfo &info) { applyStructArrayInfo(bindingExpr, info); },
                [&](const Expr &bindingExpr, LocalInfo &info) { applyStructValueInfo(bindingExpr, info); },
                error)) {
          return false;
        }
      }

      const size_t checkIndex = function.instructions.size();
      LocalInfo::ValueKind condKind = LocalInfo::ValueKind::Unknown;
      if (cond.isBinding) {
        if (!ir_lowerer::emitForConditionBindingInit(
                cond,
                loopLocals,
                [&](const Expr &valueExpr, const LocalMap &valueLocals) {
                  return emitExpr(valueExpr, valueLocals);
                },
                [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
                error)) {
          return false;
        }
        Expr condName;
        condName.kind = Expr::Kind::Name;
        condName.name = cond.name;
        condName.namespacePrefix = cond.namespacePrefix;
        if (!emitExpr(condName, loopLocals)) {
          return false;
        }
        condKind = inferExprKind(condName, loopLocals);
      } else {
        if (!emitExpr(cond, loopLocals)) {
          return false;
        }
        condKind = inferExprKind(cond, loopLocals);
      }
      if (condKind == LocalInfo::ValueKind::Unknown) {
        std::string builtinComparison;
        if (getBuiltinComparisonName(cond, builtinComparison)) {
          condKind = LocalInfo::ValueKind::Bool;
        }
      }
      if (condKind != LocalInfo::ValueKind::Bool) {
        error = "for condition requires bool";
        return false;
      }
      const size_t jumpEndIndex = function.instructions.size();
      function.instructions.push_back({IrOpcode::JumpIfZero, 0});

      OnErrorScope onErrorScope(currentOnError, std::nullopt);
      if (!ir_lowerer::emitBodyStatementsWithFileScope(
              body.bodyArguments,
              loopLocals,
              [&](const Expr &bodyStmt, LocalMap &bodyLocals) {
                return emitStatement(bodyStmt, bodyLocals);
              },
              [&]() { return emitStatement(step, loopLocals); },
              [&]() { pushFileScope(); },
              [&]() { emitFileScopeCleanup(fileScopeStack.back()); },
              [&]() { popFileScope(); })) {
        return false;
      }

      function.instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(checkIndex)});
      const size_t endIndex = function.instructions.size();
      function.instructions[jumpEndIndex].imm = static_cast<int32_t>(endIndex);
      return true;
    }
    if (isRepeatCall(stmt)) {
      if (stmt.args.size() != 1) {
        error = "repeat requires exactly one argument";
        return false;
      }
      if (!emitExpr(stmt.args.front(), localsIn)) {
        return false;
      }
      LocalInfo::ValueKind countKind = LocalInfo::ValueKind::Unknown;
      if (!ir_lowerer::resolveCountedLoopKind(
              inferExprKind(stmt.args.front(), localsIn),
              true,
              "repeat count requires integer or bool",
              countKind,
              error)) {
        return false;
      }

      ir_lowerer::CountedLoopControl loopControl;
      if (!ir_lowerer::emitCountedLoopPrologue(
              countKind,
              [&]() { return allocTempLocal(); },
              [&]() { return function.instructions.size(); },
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
              [&](size_t instructionIndex, int32_t imm) { function.instructions[instructionIndex].imm = imm; },
              std::function<void()>{},
              loopControl,
              error)) {
        return false;
      }

      OnErrorScope onErrorScope(currentOnError, std::nullopt);
      pushFileScope();
      LocalMap bodyLocals = localsIn;
      for (const auto &bodyStmt : stmt.bodyArguments) {
        if (!emitStatement(bodyStmt, bodyLocals)) {
          return false;
        }
      }
      emitFileScopeCleanup(fileScopeStack.back());
      popFileScope();

      ir_lowerer::emitCountedLoopIterationStep(
          loopControl,
          [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); });
      ir_lowerer::patchCountedLoopEnd(
          loopControl,
          [&]() { return function.instructions.size(); },
          [&](size_t instructionIndex, int32_t imm) { function.instructions[instructionIndex].imm = imm; });
      return true;
    }
    if (stmt.kind == Expr::Kind::Call && isBlockCall(stmt) && stmt.hasBodyArguments && resolveDefinitionCall(stmt) == nullptr) {
      if (!stmt.args.empty() || !stmt.templateArgs.empty() || hasNamedArguments(stmt.argNames)) {
        error = "block does not accept arguments";
        return false;
      }
      std::optional<OnErrorHandler> blockOnError;
      auto definitionExists = [&](const std::string &path) {
        return defMap.find(path) != defMap.end();
      };
      if (!ir_lowerer::parseOnErrorTransform(
              stmt.transforms, stmt.namespacePrefix, "block", resolveExprPath, definitionExists, blockOnError, error)) {
        return false;
      }
      OnErrorScope onErrorScope(currentOnError, blockOnError);
      pushFileScope();
      LocalMap blockLocals = localsIn;
      for (const auto &bodyStmt : stmt.bodyArguments) {
        if (!emitStatement(bodyStmt, blockLocals)) {
          return false;
        }
      }
      emitFileScopeCleanup(fileScopeStack.back());
      popFileScope();
      return true;
    }
    if (stmt.kind == Expr::Kind::Call) {
      if ((stmt.hasBodyArguments || !stmt.bodyArguments.empty()) && !isBlockCall(stmt)) {
        Expr callExpr = stmt;
        callExpr.bodyArguments.clear();
        callExpr.hasBodyArguments = false;
        const Definition *callee = nullptr;
        if (callExpr.isMethodCall && !isArrayCountCall(callExpr, localsIn) && !isStringCountCall(callExpr, localsIn) &&
            !isVectorCapacityCall(callExpr, localsIn)) {
          callee = resolveMethodCallDefinition(callExpr, localsIn);
          if (!callee) {
            return false;
          }
        } else {
          callee = resolveDefinitionCall(callExpr);
          if (!callee) {
            error = "block arguments require a definition target: " + resolveExprPath(callExpr);
            return false;
          }
        }
        ReturnInfo info;
        if (!getReturnInfo(callee->fullPath, info)) {
          return false;
        }
        if (!emitInlineDefinitionCall(callExpr, *callee, localsIn, false)) {
          return false;
        }
        if (!info.returnsVoid) {
          function.instructions.push_back({IrOpcode::Pop, 0});
        }
        OnErrorScope onErrorScope(currentOnError, std::nullopt);
        pushFileScope();
        LocalMap blockLocals = localsIn;
        for (const auto &bodyExpr : stmt.bodyArguments) {
          if (!emitStatement(bodyExpr, blockLocals)) {
            return false;
          }
        }
        emitFileScopeCleanup(fileScopeStack.back());
        popFileScope();
        return true;
      }
      if (!stmt.isMethodCall && stmt.name.rfind("/std/collections/experimental_vector/", 0) == 0) {
        if (const Definition *callee = resolveDefinitionCall(stmt)) {
          ReturnInfo info;
          if (!getReturnInfo(callee->fullPath, info)) {
            return false;
          }
          if (!emitInlineDefinitionCall(stmt, *callee, localsIn, false)) {
            return false;
          }
          if (!info.returnsVoid) {
            function.instructions.push_back({IrOpcode::Pop, 0});
          }
          return true;
        }
      }
      const auto vectorHelperResult = ir_lowerer::tryEmitVectorStatementHelper(
          stmt,
          localsIn,
          function.instructions,
          [&]() { return allocTempLocal(); },
          [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferExprKind(valueExpr, valueLocals); },
          [&](const Expr &valueExpr, const LocalMap &valueLocals) {
            return inferStructExprPath(valueExpr, valueLocals);
          },
          [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
          [&](const std::string &structPath) -> const Definition * {
            auto destroyIt = defMap.find(structPath + "/DestroyStack");
            if (destroyIt != defMap.end()) {
              return destroyIt->second;
            }
            destroyIt = defMap.find(structPath + "/Destroy");
            if (destroyIt != defMap.end()) {
              return destroyIt->second;
            }
            return nullptr;
          },
          [&](const std::string &structPath) -> const Definition * {
            auto moveIt = defMap.find(structPath + "/Move");
            if (moveIt != defMap.end()) {
              return moveIt->second;
            }
            moveIt = defMap.find(structPath + "/Copy");
            if (moveIt != defMap.end()) {
              return moveIt->second;
            }
            return nullptr;
          },
          [&](const Expr &callExpr, const Definition &callee, const LocalMap &callLocals, bool requireValue) {
            return emitInlineDefinitionCall(callExpr, callee, callLocals, requireValue);
          },
          [&](const Expr &candidate) {
            if (candidate.isMethodCall && candidate.namespacePrefix.empty() &&
                !candidate.args.empty() &&
                (candidate.name == "push" || candidate.name == "pop" ||
                 candidate.name == "reserve" || candidate.name == "clear" ||
                 candidate.name == "remove_at" ||
                 candidate.name == "remove_swap") &&
                candidate.args.front().kind == Expr::Kind::Name) {
              auto localIt = localsIn.find(candidate.args.front().name);
              if (localIt != localsIn.end() && !localIt->second.isSoaVector &&
                  (localIt->second.kind == LocalInfo::Kind::Vector ||
                   localIt->second.structTypeName ==
                       "/std/collections/experimental_vector/Vector" ||
                   localIt->second.structTypeName.rfind(
                       "/std/collections/experimental_vector/Vector__", 0) ==
                       0)) {
                return false;
              }
            }
            if (candidate.isMethodCall && !isArrayCountCall(candidate, localsIn) &&
                !isStringCountCall(candidate, localsIn) && !isVectorCapacityCall(candidate, localsIn)) {
              return resolveMethodCallDefinition(candidate, localsIn) != nullptr;
            }
            return resolveDefinitionCall(candidate) != nullptr;
          },
          [&]() { emitVectorCapacityExceeded(); },
          [&]() { emitVectorPopOnEmpty(); },
          [&]() { emitVectorIndexOutOfBounds(); },
          [&]() { emitArrayIndexOutOfBounds(); },
          [&]() { emitVectorReserveNegative(); },
          [&]() { emitVectorReserveExceeded(); },
          error);
      if (vectorHelperResult == ir_lowerer::VectorStatementHelperEmitResult::Error) {
        return false;
      }
      if (vectorHelperResult == ir_lowerer::VectorStatementHelperEmitResult::Emitted) {
        return true;
      }
    }
