        Expr inlineDispatchExpr = expr;
        const auto inlineDispatchResult = ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            inlineDispatchExpr,
            localsIn,
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return isArrayCountCall(callExpr, localMap);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return isStringCountCall(callExpr, localMap);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return isVectorCapacityCall(callExpr, localMap);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return resolveMethodCallDefinition(callExpr, localMap);
            },
            [&](const Expr &callExpr) { return resolveDefinitionCall(callExpr); },
            [&](const Expr &callExpr, const Definition &callee, const ir_lowerer::LocalMap &localMap) {
              return emitInlineDefinitionCall(callExpr, callee, localMap, true);
            },
            error);
        if (inlineDispatchResult == ir_lowerer::InlineCallDispatchResult::Emitted) {
          return true;
        }
        if (inlineDispatchResult == ir_lowerer::InlineCallDispatchResult::Error) {
          return false;
        }
        auto rewriteExplicitMapHelperBuiltinExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall || callExpr.args.empty()) {
            return false;
          }
          std::string helperName;
          if (callExpr.name == "/map/count" || callExpr.name == "/std/collections/map/count") {
            helperName = "count";
          } else if (callExpr.name == "/map/at" || callExpr.name == "/std/collections/map/at") {
            helperName = "at";
          } else if (callExpr.name == "/map/at_unsafe" || callExpr.name == "/std/collections/map/at_unsafe") {
            helperName = "at_unsafe";
          } else {
            return false;
          }
          if (!ir_lowerer::resolveMapAccessTargetInfo(callExpr.args.front(), localsIn).isMapTarget) {
            return false;
          }
          rewrittenExpr = callExpr;
          rewrittenExpr.name = helperName;
          return true;
        };
        auto rewriteImplicitBorrowedMapReceiverExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.args.empty()) {
            return false;
          }

          auto isBorrowedOrPointerMapReceiver = [&](const Expr &receiverExpr) {
            if (receiverExpr.kind == Expr::Kind::Call && isSimpleCallName(receiverExpr, "dereference") &&
                receiverExpr.args.size() == 1) {
              return false;
            }
            if (receiverExpr.kind == Expr::Kind::Name) {
              auto it = localsIn.find(receiverExpr.name);
              return it != localsIn.end() &&
                     ((it->second.kind == ir_lowerer::LocalInfo::Kind::Reference && it->second.referenceToMap) ||
                      (it->second.kind == ir_lowerer::LocalInfo::Kind::Pointer && it->second.pointerToMap));
            }
            std::string accessName;
            if (receiverExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(receiverExpr, accessName) &&
                receiverExpr.args.size() == 2 && receiverExpr.args.front().kind == Expr::Kind::Name) {
              auto it = localsIn.find(receiverExpr.args.front().name);
              return it != localsIn.end() && it->second.isArgsPack &&
                     (((it->second.argsPackElementKind == ir_lowerer::LocalInfo::Kind::Reference) &&
                       it->second.referenceToMap) ||
                      ((it->second.argsPackElementKind == ir_lowerer::LocalInfo::Kind::Pointer) &&
                       it->second.pointerToMap));
            }
            return false;
          };

          auto shouldRewriteReceiver = [&](const Expr &candidate) {
            if (candidate.isMethodCall) {
              return candidate.name == "contains" || candidate.name == "tryAt" ||
                     candidate.name == "at" || candidate.name == "at_unsafe";
            }
            if (candidate.name == "contains" || candidate.name == "tryAt" ||
                candidate.name == "/map/contains" || candidate.name == "/std/collections/map/contains" ||
                candidate.name == "/map/tryAt" || candidate.name == "/std/collections/map/tryAt" ||
                candidate.name == "at" || candidate.name == "at_unsafe") {
              return true;
            }
            return false;
          };

          if (!shouldRewriteReceiver(callExpr) || !isBorrowedOrPointerMapReceiver(callExpr.args.front())) {
            return false;
          }

          Expr derefExpr;
          derefExpr.kind = Expr::Kind::Call;
          derefExpr.name = "dereference";
          derefExpr.args.push_back(callExpr.args.front());

          rewrittenExpr = callExpr;
          rewrittenExpr.args.front() = derefExpr;
          return true;
        };
        Expr nativeTailExpr = inlineDispatchExpr;
        Expr rewrittenExplicitMapHelperExpr;
        if (rewriteExplicitMapHelperBuiltinExpr(expr, rewrittenExplicitMapHelperExpr)) {
          nativeTailExpr = rewrittenExplicitMapHelperExpr;
        }
        Expr rewrittenBorrowedMapReceiverExpr;
        if (rewriteImplicitBorrowedMapReceiverExpr(nativeTailExpr, rewrittenBorrowedMapReceiverExpr)) {
          nativeTailExpr = rewrittenBorrowedMapReceiverExpr;
        }
        const auto nativeTailResult = ir_lowerer::tryEmitNativeCallTailDispatchWithLocals(
            nativeTailExpr,
            localsIn,
            [&](const Expr &callExpr, std::string &mathBuiltinName) {
              return getMathBuiltinName(callExpr, mathBuiltinName);
            },
            [&](const std::string &mathBuiltinName) {
              return ir_lowerer::isSupportedMathBuiltinName(mathBuiltinName);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return isArrayCountCall(callExpr, localMap);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return isVectorCapacityCall(callExpr, localMap);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return isStringCountCall(callExpr, localMap);
            },
            [&](const Expr &targetExpr, const ir_lowerer::LocalMap &localMap) {
              return isEntryArgsName(targetExpr, localMap);
            },
            [&](const Expr &targetExpr,
                const ir_lowerer::LocalMap &localMap,
                int32_t &stringIndexOut,
                size_t &lengthOut) {
              return resolveStringTableTarget(targetExpr, localMap, stringIndexOut, lengthOut);
            },
            stringTable.size(),
            [&](const Expr &valueExpr, const ir_lowerer::LocalMap &localMap) {
              return emitExpr(valueExpr, localMap);
            },
            [&](const Expr &targetCallExpr, ir_lowerer::MapAccessTargetInfo &targetInfoOut) {
              targetInfoOut = {};
              const Definition *callee = resolveDefinitionCall(targetCallExpr);
              if (callee == nullptr) {
                return false;
              }
              std::string collectionName;
              std::vector<std::string> collectionArgs;
              if (!ir_lowerer::inferDeclaredReturnCollection(*callee, collectionName, collectionArgs)) {
                return false;
              }
              if (collectionName != "map" || collectionArgs.size() != 2) {
                return false;
              }
              targetInfoOut.isMapTarget = true;
              targetInfoOut.mapKeyKind = ir_lowerer::valueKindFromTypeName(collectionArgs[0]);
              targetInfoOut.mapValueKind = ir_lowerer::valueKindFromTypeName(collectionArgs[1]);
              return true;
            },
            [&](const Expr &targetCallExpr, ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
              targetInfoOut = {};
              if (targetCallExpr.isFieldAccess && targetCallExpr.args.size() == 1) {
                const std::string receiverStruct = inferStructExprPath(targetCallExpr.args.front(), localsIn);
                if (!receiverStruct.empty()) {
                  StructSlotFieldInfo fieldInfo;
                  if (resolveStructFieldSlot(receiverStruct, targetCallExpr.name, fieldInfo) &&
                      fieldInfo.structPath == "/vector") {
                    targetInfoOut.isArrayOrVectorTarget = true;
                    targetInfoOut.isVectorTarget = true;
                    targetInfoOut.elemKind = fieldInfo.valueKind;
                    return true;
                  }
                }
              }
              const Definition *callee = resolveDefinitionCall(targetCallExpr);
              if (callee == nullptr) {
                return false;
              }
              std::string collectionName;
              std::vector<std::string> collectionArgs;
              if (!ir_lowerer::inferDeclaredReturnCollection(*callee, collectionName, collectionArgs)) {
                return false;
              }
              if ((collectionName != "array" && collectionName != "vector") || collectionArgs.size() != 1) {
                return false;
              }
              targetInfoOut.isArrayOrVectorTarget = true;
              targetInfoOut.isVectorTarget = (collectionName == "vector");
              targetInfoOut.elemKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front());
              return true;
            },
            [&](const Expr &callExpr, std::string &builtinName) {
              PrintBuiltin printBuiltin;
              if (!getPrintBuiltin(callExpr, printBuiltin)) {
                return false;
              }
              builtinName = printBuiltin.name;
              return true;
            },
            [&](const Expr &lookupKeyExpr, const ir_lowerer::LocalMap &localMap) {
              return inferExprKind(lookupKeyExpr, localMap);
            },
            [&]() { return allocTempLocal(); },
            [&]() { emitStringIndexOutOfBounds(); },
            [&]() { emitMapKeyNotFound(); },
            [&]() { emitArrayIndexOutOfBounds(); },
            [&]() { return function.instructions.size(); },
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            [&](size_t instructionIndex, uint64_t imm) { function.instructions[instructionIndex].imm = imm; },
            error);
        if (nativeTailResult == ir_lowerer::NativeCallTailDispatchResult::Emitted) {
          return true;
        }
        if (nativeTailResult == ir_lowerer::NativeCallTailDispatchResult::Error) {
          return false;
        }
