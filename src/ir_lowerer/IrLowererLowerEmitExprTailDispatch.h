        auto matchesGeneratedMapHelperPath = [&](const Expr &callExpr, const char *basePath) {
          const std::string base(basePath == nullptr ? "" : basePath);
          return callExpr.name == base ||
                 callExpr.name.rfind(base + "__t", 0) == 0;
        };
        auto resolveBuiltinMapHelperName =
            [&](const Expr &callExpr, bool allowMethodCall, std::string &helperNameOut) {
              helperNameOut.clear();
              if (callExpr.kind != Expr::Kind::Call || callExpr.args.empty()) {
                return false;
              }
              if (allowMethodCall && callExpr.isMethodCall) {
                if (callExpr.name == "count" || callExpr.name == "contains" ||
                    callExpr.name == "tryAt" || callExpr.name == "at" ||
                    callExpr.name == "at_unsafe") {
                  helperNameOut = callExpr.name;
                  return true;
                }
                return false;
              }
              if (callExpr.isMethodCall) {
                return false;
              }
              if (ir_lowerer::resolveMapHelperAliasName(callExpr, helperNameOut)) {
                return true;
              }
              if (matchesGeneratedMapHelperPath(callExpr, "/std/collections/mapCount")) {
                helperNameOut = "count";
                return true;
              }
              if (matchesGeneratedMapHelperPath(callExpr, "/std/collections/mapContains")) {
                helperNameOut = "contains";
                return true;
              }
              if (matchesGeneratedMapHelperPath(callExpr, "/std/collections/mapTryAt")) {
                helperNameOut = "tryAt";
                return true;
              }
              if (matchesGeneratedMapHelperPath(callExpr, "/std/collections/mapAt")) {
                helperNameOut = "at";
                return true;
              }
              if (matchesGeneratedMapHelperPath(callExpr, "/std/collections/mapAtUnsafe")) {
                helperNameOut = "at_unsafe";
                return true;
              }
              return false;
            };
        auto rewriteBuiltinMapInsertPendingExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.args.size() != 3) {
            return false;
          }

          size_t receiverIndex = 0;
          if (callExpr.isMethodCall) {
            if (callExpr.name != "insert") {
              return false;
            }
          } else {
            std::string helperName;
            if (!ir_lowerer::resolveMapHelperAliasName(callExpr, helperName) || helperName != "insert") {
              return false;
            }
            if (hasNamedArguments(callExpr.argNames)) {
              for (size_t i = 0; i < callExpr.args.size(); ++i) {
                if (i < callExpr.argNames.size() && callExpr.argNames[i].has_value() &&
                    *callExpr.argNames[i] == "values") {
                  receiverIndex = i;
                  break;
                }
              }
            }
          }

          if (receiverIndex >= callExpr.args.size()) {
            return false;
          }
          const auto targetInfo =
              ir_lowerer::resolveMapAccessTargetInfo(callExpr.args[receiverIndex], localsIn);
          if (!targetInfo.isMapTarget) {
            return false;
          }

          rewrittenExpr = callExpr;
          rewrittenExpr.name = "/std/collections/map/insert_builtin_pending";
          rewrittenExpr.namespacePrefix.clear();
          rewrittenExpr.isMethodCall = false;
          rewrittenExpr.isFieldAccess = false;
          // Force rewritten rooted helper calls to resolve from their new path
          // instead of stale semantic-product call-target facts.
          rewrittenExpr.semanticNodeId = 0;
          if (rewrittenExpr.templateArgs.empty() &&
              targetInfo.mapKeyKind != LocalInfo::ValueKind::Unknown &&
              targetInfo.mapValueKind != LocalInfo::ValueKind::Unknown) {
            rewrittenExpr.templateArgs = {
                ir_lowerer::typeNameForValueKind(targetInfo.mapKeyKind),
                ir_lowerer::typeNameForValueKind(targetInfo.mapValueKind),
            };
          }
          return true;
        };
        auto rewriteCanonicalMapHelperForExperimentalReceiverExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          std::string helperName;
          if (!resolveBuiltinMapHelperName(callExpr, true, helperName)) {
            return false;
          }
          const size_t expectedArgCount = helperName == "count" ? 1u : 2u;
          if (callExpr.args.size() != expectedArgCount) {
            return false;
          }

          auto inferExperimentalMapStructPath = [&](const Expr &receiverExpr) {
            if (receiverExpr.kind == Expr::Kind::Name) {
              auto it = localsIn.find(receiverExpr.name);
              if (it != localsIn.end() &&
                  !it->second.isArgsPack &&
                  it->second.kind != LocalInfo::Kind::Reference &&
                  it->second.kind != LocalInfo::Kind::Pointer &&
                  it->second.structTypeName.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
                return it->second.structTypeName;
              }
            }
            const auto accessTargetInfo =
                ir_lowerer::resolveArrayVectorAccessTargetInfo(receiverExpr, localsIn);
            if (accessTargetInfo.isWrappedMapTarget) {
              const auto mapTargetInfo =
                  ir_lowerer::resolveMapAccessTargetInfo(receiverExpr, localsIn);
              if (mapTargetInfo.structTypeName.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
                return mapTargetInfo.structTypeName;
              }
              return std::string{};
            }
            if (accessTargetInfo.structTypeName.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
              return accessTargetInfo.structTypeName;
            }
            const auto mapTargetInfo =
                ir_lowerer::resolveMapAccessTargetInfo(receiverExpr, localsIn);
            if (mapTargetInfo.structTypeName.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
              return mapTargetInfo.structTypeName;
            }
            return std::string{};
          };
          auto buildExperimentalMapHelperPath = [&](const std::string &structPath) {
            if (structPath.rfind("/std/collections/experimental_map/Map__", 0) != 0) {
              return std::string{};
            }
            const size_t suffixStart = structPath.find("__");
            if (suffixStart == std::string::npos) {
              return std::string{};
            }
            std::string helperStem;
            if (helperName == "count") {
              helperStem = "mapCount";
            } else if (helperName == "contains") {
              helperStem = "mapContains";
            } else if (helperName == "tryAt") {
              helperStem = "mapTryAt";
            } else if (helperName == "at") {
              helperStem = "mapAt";
            } else if (helperName == "at_unsafe") {
              helperStem = "mapAtUnsafe";
            } else {
              return std::string{};
            }
            return "/std/collections/experimental_map/" + helperStem + structPath.substr(suffixStart);
          };

          const std::string receiverStructPath = inferExperimentalMapStructPath(callExpr.args.front());
          const std::string directExperimentalHelperPath = buildExperimentalMapHelperPath(receiverStructPath);
          if (!directExperimentalHelperPath.empty() && defMap.find(directExperimentalHelperPath) != defMap.end()) {
            rewrittenExpr = callExpr;
            rewrittenExpr.name = directExperimentalHelperPath;
            rewrittenExpr.namespacePrefix.clear();
            rewrittenExpr.isMethodCall = false;
            rewrittenExpr.templateArgs.clear();
            return true;
          }

          Expr methodExpr = callExpr;
          methodExpr.isMethodCall = true;
          methodExpr.name = helperName;
          methodExpr.namespacePrefix.clear();
          const Definition *callee = resolveMethodCallDefinition(methodExpr, localsIn);
          if (callee == nullptr) {
            return false;
          }
          const bool isExperimentalMapHelper =
              callee->fullPath.rfind("/std/collections/experimental_map/Map__", 0) == 0 ||
              callee->fullPath.rfind("/std/collections/experimental_map/map", 0) == 0;
          if (!isExperimentalMapHelper) {
            return false;
          }

          rewrittenExpr = callExpr;
          rewrittenExpr.name = callee->fullPath;
          rewrittenExpr.namespacePrefix.clear();
          rewrittenExpr.isMethodCall = false;
          rewrittenExpr.templateArgs.clear();
          return true;
        };
        auto isVectorStructPath = [](const std::string &structPath) {
          return structPath == "/vector" ||
                 structPath == "/std/collections/experimental_vector/Vector" ||
                 structPath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
        };
        auto rewriteExplicitMapHelperBuiltinExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          std::string helperName;
          if (!resolveBuiltinMapHelperName(callExpr, false, helperName) ||
              (helperName != "count" && helperName != "at" && helperName != "at_unsafe")) {
            return false;
          }
          const auto receiverTargetInfo =
              ir_lowerer::resolveArrayVectorAccessTargetInfo(callExpr.args.front(), localsIn);
          if (helperName == "at" &&
              receiverTargetInfo.isArgsPackTarget &&
              receiverTargetInfo.argsPackElementKind == LocalInfo::Kind::Map) {
            rewrittenExpr = callExpr;
            rewrittenExpr.name = "at";
            rewrittenExpr.namespacePrefix.clear();
            return true;
          }
          if (receiverTargetInfo.isArgsPackTarget &&
              receiverTargetInfo.argsPackElementKind == LocalInfo::Kind::Map) {
            return false;
          }
          if (receiverTargetInfo.structTypeName.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
            return false;
          }
          const std::string receiverStructPath = inferStructExprPath(callExpr.args.front(), localsIn);
          if (receiverStructPath.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
            return false;
          }
          if (!ir_lowerer::resolveMapAccessTargetInfo(callExpr.args.front(), localsIn).isMapTarget) {
            return false;
          }
          rewrittenExpr = callExpr;
          rewrittenExpr.name = helperName;
          rewrittenExpr.namespacePrefix.clear();
          return true;
        };
        Expr inlineDispatchExpr = expr;
        Expr rewrittenBuiltinMapInsertPendingExpr;
        if (rewriteBuiltinMapInsertPendingExpr(expr, rewrittenBuiltinMapInsertPendingExpr)) {
          inlineDispatchExpr = rewrittenBuiltinMapInsertPendingExpr;
        }
        Expr rewrittenCanonicalExperimentalMapHelperExpr;
        if (rewriteCanonicalMapHelperForExperimentalReceiverExpr(
                inlineDispatchExpr, rewrittenCanonicalExperimentalMapHelperExpr)) {
          inlineDispatchExpr = rewrittenCanonicalExperimentalMapHelperExpr;
        }
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
        Expr nativeTailExpr = inlineDispatchExpr;
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
              return candidate.name == "count" || candidate.name == "contains" ||
                     candidate.name == "tryAt" || candidate.name == "at" ||
                     candidate.name == "at_unsafe";
            }
            std::string helperName;
            return resolveBuiltinMapHelperName(candidate, false, helperName) &&
                   (helperName == "count" || helperName == "contains" ||
                    helperName == "tryAt" || helperName == "at" ||
                    helperName == "at_unsafe");
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
        Expr rewrittenExplicitMapHelperExpr;
        if (rewriteExplicitMapHelperBuiltinExpr(nativeTailExpr, rewrittenExplicitMapHelperExpr)) {
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
              auto inferExperimentalMapStructPathFromTypeTexts =
                  [](const std::string &keyTypeText, const std::string &valueTypeText) {
                    std::string canonicalArgs;
                    auto appendCanonicalType = [&](const std::string &typeText) {
                      const std::string trimmed = trimTemplateTypeText(typeText);
                      for (char ch : trimmed) {
                        if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r' && ch != '\f' &&
                            ch != '\v') {
                          canonicalArgs.push_back(ch);
                        }
                      }
                    };
                    appendCanonicalType(keyTypeText);
                    canonicalArgs.push_back(',');
                    appendCanonicalType(valueTypeText);
                    if (canonicalArgs.empty()) {
                      return std::string{};
                    }

                    uint64_t hash = 1469598103934665603ULL;
                    for (unsigned char ch : canonicalArgs) {
                      hash ^= static_cast<uint64_t>(ch);
                      hash *= 1099511628211ULL;
                    }

                    std::string suffix;
                    do {
                      constexpr char HexDigits[] = "0123456789abcdef";
                      suffix.push_back(HexDigits[hash & 0xfu]);
                      hash >>= 4u;
                    } while (hash != 0);

                    std::string structPath = "/std/collections/experimental_map/Map__t";
                    for (auto it = suffix.rbegin(); it != suffix.rend(); ++it) {
                      structPath.push_back(*it);
                    }
                    return structPath;
                  };
              const Definition *callee = resolveDefinitionCall(targetCallExpr);
              if (callee == nullptr) {
                return false;
              }
              std::string collectionName;
              std::vector<std::string> collectionArgs;
              if (ir_lowerer::inferDeclaredReturnCollection(*callee, collectionName, collectionArgs) &&
                  collectionName == "map" && collectionArgs.size() == 2) {
                targetInfoOut.isMapTarget = true;
                targetInfoOut.mapKeyKind = ir_lowerer::valueKindFromTypeName(collectionArgs[0]);
                targetInfoOut.mapValueKind = ir_lowerer::valueKindFromTypeName(collectionArgs[1]);
                targetInfoOut.structTypeName =
                    inferExperimentalMapStructPathFromTypeTexts(collectionArgs[0], collectionArgs[1]);
                return true;
              }
              auto extractParameterTypeName = [&](const Expr &paramExpr) {
                for (const auto &transform : paramExpr.transforms) {
                  if (transform.name == "mut" || transform.name == "public" || transform.name == "private" ||
                      transform.name == "static" || transform.name == "shared" || transform.name == "placement" ||
                      transform.name == "align" || transform.name == "packed" || transform.name == "reflection" ||
                      transform.name == "effects" || transform.name == "capabilities") {
                    continue;
                  }
                  if (!transform.arguments.empty()) {
                    continue;
                  }
                  std::string typeName = transform.name;
                  if (!transform.templateArgs.empty()) {
                    typeName += "<";
                    for (size_t index = 0; index < transform.templateArgs.size(); ++index) {
                      if (index != 0) {
                        typeName += ", ";
                      }
                      typeName += trimTemplateTypeText(transform.templateArgs[index]);
                    }
                    typeName += ">";
                  }
                  return typeName;
                }
                return std::string{};
              };
              if (callee->parameters.size() < 2) {
                return false;
              }
              const std::string keyTypeName = extractParameterTypeName(callee->parameters[0]);
              const std::string valueTypeName = extractParameterTypeName(callee->parameters[1]);
              if (keyTypeName.empty() || valueTypeName.empty()) {
                return false;
              }
              targetInfoOut.isMapTarget = true;
              targetInfoOut.mapKeyKind = ir_lowerer::valueKindFromTypeName(keyTypeName);
              targetInfoOut.mapValueKind = ir_lowerer::valueKindFromTypeName(valueTypeName);
              if (targetInfoOut.mapKeyKind == ir_lowerer::LocalInfo::ValueKind::Unknown ||
                  targetInfoOut.mapValueKind == ir_lowerer::LocalInfo::ValueKind::Unknown) {
                return false;
              }
              targetInfoOut.structTypeName =
                  inferExperimentalMapStructPathFromTypeTexts(keyTypeName, valueTypeName);
              return true;
            },
            [&](const Expr &targetCallExpr, ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
              targetInfoOut = {};
              if (targetCallExpr.isFieldAccess && targetCallExpr.args.size() == 1) {
                const std::string receiverStruct = inferStructExprPath(targetCallExpr.args.front(), localsIn);
                if (!receiverStruct.empty()) {
                  StructSlotFieldInfo fieldInfo;
                  if (resolveStructFieldSlot(receiverStruct, targetCallExpr.name, fieldInfo) &&
                      isVectorStructPath(fieldInfo.structPath)) {
                    targetInfoOut.isArrayOrVectorTarget = true;
                    targetInfoOut.isVectorTarget = true;
                    targetInfoOut.elemKind = fieldInfo.valueKind;
                    targetInfoOut.structTypeName = fieldInfo.structPath;
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
