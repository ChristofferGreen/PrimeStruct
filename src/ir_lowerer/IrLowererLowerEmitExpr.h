  emitFileErrorWhy = {};
  if (!ir_lowerer::runLowerReturnCallsSetup(
          {
              .function = &function,
              .internString = internString,
          },
          emitFileErrorWhy,
          error)) {
    return false;
  }
  emitMovePassthroughCall = {};
  emitUploadPassthroughCall = {};
  emitReadbackPassthroughCall = {};
  if (!ir_lowerer::runLowerExprEmitSetup(
          {},
          emitMovePassthroughCall,
          emitUploadPassthroughCall,
          emitReadbackPassthroughCall,
          error)) {
    return false;
  }

  emitExpr = [&](const Expr &expr, const LocalMap &localsIn) -> bool {
    if (expr.isLambda) {
      error = "IR backends do not support lambdas";
      return false;
    }
    const auto moveResult = ir_lowerer::runLowerExprEmitMovePassthroughStep(
        expr,
        localsIn,
        emitMovePassthroughCall,
        [&](const Expr &argExpr, const LocalMap &argLocals) { return emitExpr(argExpr, argLocals); },
        error);
    if (moveResult != ir_lowerer::UnaryPassthroughCallResult::NotMatched) {
      return moveResult == ir_lowerer::UnaryPassthroughCallResult::Emitted;
    }
    switch (expr.kind) {
      case Expr::Kind::Literal: {
        if (expr.intWidth == 64 || expr.isUnsigned) {
          IrInstruction inst;
          inst.op = IrOpcode::PushI64;
          inst.imm = expr.literalValue;
          function.instructions.push_back(inst);
          return true;
        }
        int64_t value = static_cast<int64_t>(expr.literalValue);
        if (value < std::numeric_limits<int32_t>::min() || value > std::numeric_limits<int32_t>::max()) {
          error = "i32 literal out of range for native backend";
          return false;
        }
        IrInstruction inst;
        inst.op = IrOpcode::PushI32;
        inst.imm = static_cast<int32_t>(value);
        function.instructions.push_back(inst);
        return true;
      }
      case Expr::Kind::FloatLiteral:
        return emitFloatLiteral(expr);
      case Expr::Kind::StringLiteral:
        {
          std::string decoded;
          if (!ir_lowerer::parseLowererStringLiteral(expr.stringValue, decoded, error)) {
            return false;
          }
          int32_t index = internString(decoded);
          function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(index)});
          return true;
        }
      case Expr::Kind::BoolLiteral: {
        IrInstruction inst;
        inst.op = IrOpcode::PushI32;
        inst.imm = expr.boolValue ? 1 : 0;
        function.instructions.push_back(inst);
        return true;
      }
      case Expr::Kind::Name: {
        auto it = localsIn.find(expr.name);
        if (it != localsIn.end()) {
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          const bool isAggregateReference =
              it->second.kind == LocalInfo::Kind::Reference &&
              (!it->second.structTypeName.empty() || it->second.referenceToArray ||
               it->second.referenceToVector || it->second.referenceToMap || it->second.referenceToBuffer);
          if (it->second.kind == LocalInfo::Kind::Reference && !isAggregateReference) {
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          }
          return true;
        }
        if (hasEntryArgs && expr.name == entryArgsName) {
          error = "native backend only supports count() on entry arguments";
          return false;
        }
        std::string mathConst;
        if (getMathConstantName(expr.name, mathConst)) {
          double value = 0.0;
          if (mathConst == "pi") {
            value = 3.14159265358979323846;
          } else if (mathConst == "tau") {
            value = 6.28318530717958647692;
          } else if (mathConst == "e") {
            value = 2.71828182845904523536;
          }
          uint64_t bits = 0;
          std::memcpy(&bits, &value, sizeof(bits));
          function.instructions.push_back({IrOpcode::PushF64, bits});
          return true;
        }
        error = "native backend does not know identifier: " + expr.name;
        return false;
      }
      case Expr::Kind::Call: {
        if (expr.isFieldAccess) {
          if (expr.args.size() != 1) {
            error = "field access requires a receiver";
            return false;
          }
          const Expr &receiver = expr.args.front();
          if (receiver.kind == Expr::Kind::Name && localsIn.find(receiver.name) == localsIn.end()) {
            std::string receiverStructPath;
            if (resolveStructTypeName(receiver.name, receiver.namespacePrefix, receiverStructPath)) {
              auto defIt = defMap.find(receiverStructPath);
              if (defIt == defMap.end() || defIt->second == nullptr) {
                error = "field access requires struct receiver";
                return false;
              }
              auto isStaticField = [](const Expr &stmt) {
                for (const auto &transform : stmt.transforms) {
                  if (transform.name == "static") {
                    return true;
                  }
                }
                return false;
              };
              for (const auto &stmt : defIt->second->statements) {
                if (!stmt.isBinding || !isStaticField(stmt) || stmt.name != expr.name) {
                  continue;
                }
                if (stmt.args.empty()) {
                  error = "static field missing initializer: " + receiverStructPath + "/" + expr.name;
                  return false;
                }
                return emitExpr(stmt.args.front(), localsIn);
              }
              error = "unknown struct field: " + expr.name;
              return false;
            }
          }
          std::string structPath = inferStructExprPath(receiver, localsIn);
          if (structPath.empty()) {
            error = "field access requires struct receiver";
            return false;
          }
          StructSlotFieldInfo fieldInfo;
          if (!resolveStructFieldSlot(structPath, expr.name, fieldInfo)) {
            error = "unknown struct field: " + expr.name;
            return false;
          }
          const auto receiverLocalIt =
              receiver.kind == Expr::Kind::Name ? localsIn.find(receiver.name) : localsIn.end();
          const bool isRawBuiltinSoaStorageFieldAccess =
              receiver.kind == Expr::Kind::Name && receiverLocalIt != localsIn.end() &&
              receiverLocalIt->second.usesBuiltinCollectionLayout &&
              receiverLocalIt->second.isSoaVector && expr.name == "storage" &&
              fieldInfo.structPath.rfind("/std/collections/internal_soa_storage/SoaColumn__", 0) == 0;
          if (isRawBuiltinSoaStorageFieldAccess) {
            StructSlotLayoutInfo storageLayout;
            if (!resolveStructSlotLayout(fieldInfo.structPath, storageLayout)) {
              return false;
            }
            if (storageLayout.totalSlots < 5) {
              error = "internal error: builtin soa_vector storage bridge requires SoaColumn layout";
              return false;
            }

            const int32_t baseLocal = nextLocal;
            nextLocal += storageLayout.totalSlots;
            const int32_t receiverPtrLocal = allocTempLocal();
            if (!emitExpr(receiver, localsIn)) {
              return false;
            }
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(receiverPtrLocal)});

            function.instructions.push_back(
                {IrOpcode::PushI32,
                 static_cast<uint64_t>(static_cast<int32_t>(storageLayout.totalSlots - 1))});
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});

            function.instructions.push_back(
                {IrOpcode::LoadLocal, static_cast<uint64_t>(receiverPtrLocal)});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1)});

            function.instructions.push_back(
                {IrOpcode::LoadLocal, static_cast<uint64_t>(receiverPtrLocal)});
            function.instructions.push_back(
                {IrOpcode::PushI64, static_cast<uint64_t>(IrSlotBytes)});
            function.instructions.push_back({IrOpcode::AddI64, 0});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 2)});

            function.instructions.push_back(
                {IrOpcode::LoadLocal, static_cast<uint64_t>(receiverPtrLocal)});
            function.instructions.push_back(
                {IrOpcode::PushI64, static_cast<uint64_t>(2 * IrSlotBytes)});
            function.instructions.push_back({IrOpcode::AddI64, 0});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 3)});

            function.instructions.push_back({IrOpcode::PushI32, 0});
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 4)});
            function.instructions.push_back(
                {IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
            return true;
          }
          if (!emitExpr(receiver, localsIn)) {
            return false;
          }
          const int32_t ptrLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
          const uint64_t offsetBytes = static_cast<uint64_t>(fieldInfo.slotOffset) * IrSlotBytes;
          function.instructions.push_back({IrOpcode::PushI64, offsetBytes});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          if (fieldInfo.structPath.empty()) {
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          }
          return true;
        }
        if (!expr.isMethodCall && !expr.args.empty()) {
          std::string canonicalMapHelperName;
          if (resolveMapHelperAliasName(expr, canonicalMapHelperName) &&
              resolveDefinitionCall(expr) == nullptr &&
              (canonicalMapHelperName == "count" || canonicalMapHelperName == "contains" ||
               canonicalMapHelperName == "tryAt" ||
               canonicalMapHelperName == "insert" ||
               canonicalMapHelperName == "insert_ref") &&
              ((expr.name.find('/') != std::string::npos) || !expr.namespacePrefix.empty() ||
               !expr.templateArgs.empty())) {
            Expr rewrittenExpr = expr;
            rewrittenExpr.name = canonicalMapHelperName;
            rewrittenExpr.namespacePrefix.clear();
            rewrittenExpr.templateArgs.clear();
            return emitExpr(rewrittenExpr, localsIn);
          }
        }
        #include "IrLowererLowerEmitExprStorageHelpers.h"
        #include "IrLowererLowerEmitExprTryHelpers.h"
        auto isFileHandleExpr = [&](const Expr &valueExpr, const LocalMap &valueLocals) {
          if (valueExpr.kind != Expr::Kind::Name) {
            return false;
          }
          auto localIt = valueLocals.find(valueExpr.name);
          return localIt != valueLocals.end() && localIt->second.isFileHandle;
        };
        auto resolveResultLambdaValueExpr =
            [&](const Expr &lambdaExpr,
                LocalMap &lambdaLocals,
                const std::string &builtinName,
                const Expr *&mappedValueExprOut) -> bool {
          mappedValueExprOut = nullptr;
          for (size_t i = 0; i < lambdaExpr.bodyArguments.size(); ++i) {
            const Expr &bodyExpr = lambdaExpr.bodyArguments[i];
            const bool isLast = (i + 1 == lambdaExpr.bodyArguments.size());
            if (bodyExpr.isBinding) {
              if (!emitStatement(bodyExpr, lambdaLocals)) {
                return false;
              }
              continue;
            }
            if (isSimpleCallName(bodyExpr, "return")) {
              if (bodyExpr.args.size() != 1) {
                error = "IR backends require " + builtinName + " lambda returns with a value";
                return false;
              }
              if (!isLast) {
                error = "IR backends require " + builtinName + " lambda returns to be final";
                return false;
              }
              mappedValueExprOut = &bodyExpr.args.front();
              break;
            }
            if (!isLast) {
              if (!emitStatement(bodyExpr, lambdaLocals)) {
                return false;
              }
              continue;
            }
            mappedValueExprOut = &bodyExpr;
          }
          if (mappedValueExprOut == nullptr) {
            error = "IR backends require " + builtinName + " lambdas to produce a value";
            return false;
          }
          return true;
        };
        auto resolvePackedResultPayloadInfo = [&](const Expr &valueExpr,
                                                  const LocalMap &valueLocals,
                                                  LocalInfo::ValueKind &packedKindOut,
                                                  std::string &structTypeOut) -> bool {
          packedKindOut = LocalInfo::ValueKind::Unknown;
          structTypeOut.clear();
          auto isBufferHandleCall = [&](const Expr &candidate) {
            if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.isBinding) {
              return false;
            }
            const std::string scopedName = resolveExprPath(candidate);
            return scopedName == "Buffer" || scopedName == "/std/gfx/Buffer" ||
                   scopedName == "/std/gfx/experimental/Buffer" ||
                   scopedName.rfind("/std/gfx/Buffer__t", 0) == 0 ||
                   scopedName.rfind("/std/gfx/experimental/Buffer__t", 0) == 0;
          };
          auto resolveCollectionPayload = [&](LocalInfo::Kind &collectionKindOut,
                                             LocalInfo::ValueKind &collectionValueKindOut) {
            collectionKindOut = LocalInfo::Kind::Value;
            collectionValueKindOut = LocalInfo::ValueKind::Unknown;
            if (valueExpr.kind == Expr::Kind::Name) {
              auto localIt = valueLocals.find(valueExpr.name);
              if (localIt == valueLocals.end() ||
                  !ir_lowerer::isSupportedPackedResultCollectionKind(localIt->second.kind)) {
                return false;
              }
              collectionKindOut = localIt->second.kind;
              collectionValueKindOut = localIt->second.valueKind;
              return collectionValueKindOut != LocalInfo::ValueKind::Unknown;
            }
            if (valueExpr.kind != Expr::Kind::Call) {
              return false;
            }
            std::string collectionName;
            if (!valueExpr.isMethodCall && getBuiltinCollectionName(valueExpr, collectionName) &&
                ((((collectionName == "array" || collectionName == "vector") &&
                   valueExpr.templateArgs.size() == 1) ||
                  (collectionName == "Buffer" && valueExpr.templateArgs.size() == 1) ||
                  (collectionName == "map" && valueExpr.templateArgs.size() == 2)))) {
              if (collectionName == "array") {
                collectionKindOut = LocalInfo::Kind::Array;
              } else if (collectionName == "vector") {
                collectionKindOut = LocalInfo::Kind::Vector;
              } else if (collectionName == "Buffer") {
                collectionKindOut = LocalInfo::Kind::Buffer;
              } else if (collectionName == "map") {
                if (valueKindFromTypeName(trimTemplateTypeText(valueExpr.templateArgs.front())) ==
                    LocalInfo::ValueKind::Unknown) {
                  return false;
                }
                collectionKindOut = LocalInfo::Kind::Map;
              } else {
                return false;
              }
              collectionValueKindOut = valueKindFromTypeName(
                  trimTemplateTypeText(collectionKindOut == LocalInfo::Kind::Map ? valueExpr.templateArgs.back()
                                                                                : valueExpr.templateArgs.front()));
              return collectionValueKindOut != LocalInfo::ValueKind::Unknown;
            }
            if (!valueExpr.isMethodCall && isBufferHandleCall(valueExpr) && valueExpr.templateArgs.size() == 1) {
              collectionKindOut = LocalInfo::Kind::Buffer;
              collectionValueKindOut = valueKindFromTypeName(trimTemplateTypeText(valueExpr.templateArgs.front()));
              return collectionValueKindOut != LocalInfo::ValueKind::Unknown;
            }
            if (!valueExpr.isMethodCall) {
              std::string normalized = resolveExprPath(valueExpr);
              if (!normalized.empty() && normalized.front() == '/') {
                normalized.erase(normalized.begin());
              }
              const bool isBufferAllocateCall =
                  normalized == "allocate" ||
                  normalized.rfind("allocate__t", 0) == 0 ||
                  normalized == "std/gfx/Buffer/allocate" ||
                  normalized == "std/gfx/experimental/Buffer/allocate" ||
                  normalized.rfind("std/gfx/Buffer/allocate__t", 0) == 0 ||
                  normalized.rfind("std/gfx/experimental/Buffer/allocate__t", 0) == 0;
              if (isBufferAllocateCall && valueExpr.templateArgs.size() == 1) {
                collectionKindOut = LocalInfo::Kind::Buffer;
                collectionValueKindOut = valueKindFromTypeName(trimTemplateTypeText(valueExpr.templateArgs.front()));
                return collectionValueKindOut != LocalInfo::ValueKind::Unknown;
              }
              const bool isBufferUploadCall =
                  normalized == "upload" ||
                  normalized.rfind("upload__t", 0) == 0 ||
                  normalized == "std/gfx/Buffer/upload" ||
                  normalized == "std/gfx/experimental/Buffer/upload" ||
                  normalized.rfind("std/gfx/Buffer/upload__t", 0) == 0 ||
                  normalized.rfind("std/gfx/experimental/Buffer/upload__t", 0) == 0;
              if (isBufferUploadCall) {
                if (valueExpr.templateArgs.size() == 1) {
                  collectionKindOut = LocalInfo::Kind::Buffer;
                  collectionValueKindOut = valueKindFromTypeName(trimTemplateTypeText(valueExpr.templateArgs.front()));
                  return collectionValueKindOut != LocalInfo::ValueKind::Unknown;
                }
                if (valueExpr.args.size() == 1) {
                  const Expr &sourceExpr = valueExpr.args.front();
                  if (sourceExpr.kind == Expr::Kind::Name) {
                    auto localIt = valueLocals.find(sourceExpr.name);
                    if (localIt != valueLocals.end() &&
                        ir_lowerer::isSupportedPackedResultCollectionKind(localIt->second.kind)) {
                      collectionKindOut = LocalInfo::Kind::Buffer;
                      collectionValueKindOut = localIt->second.valueKind;
                      return collectionValueKindOut != LocalInfo::ValueKind::Unknown;
                    }
                  }
                  if (sourceExpr.kind == Expr::Kind::Call) {
                    std::string sourceCollectionName;
                    if (!sourceExpr.isMethodCall && getBuiltinCollectionName(sourceExpr, sourceCollectionName) &&
                        ((((sourceCollectionName == "array" || sourceCollectionName == "vector") &&
                           sourceExpr.templateArgs.size() == 1) ||
                          (sourceCollectionName == "Buffer" && sourceExpr.templateArgs.size() == 1) ||
                          (sourceCollectionName == "map" && sourceExpr.templateArgs.size() == 2)))) {
                      collectionKindOut = LocalInfo::Kind::Buffer;
                      collectionValueKindOut = valueKindFromTypeName(
                          trimTemplateTypeText(sourceCollectionName == "map" ? sourceExpr.templateArgs.back()
                                                                             : sourceExpr.templateArgs.front()));
                      return collectionValueKindOut != LocalInfo::ValueKind::Unknown;
                    }
                  }
                }
              }
            }
            const Definition *callee = resolveDefinitionCall(valueExpr);
            if (callee == nullptr) {
              return false;
            }
            std::string declaredCollection;
            std::vector<std::string> declaredCollectionArgs;
            if (!ir_lowerer::inferDeclaredReturnCollection(*callee, declaredCollection, declaredCollectionArgs)) {
              return false;
            }
            if (declaredCollection == "array") {
              if (declaredCollectionArgs.size() != 1) {
                return false;
              }
              collectionKindOut = LocalInfo::Kind::Array;
            } else if (declaredCollection == "vector") {
              if (declaredCollectionArgs.size() != 1) {
                return false;
              }
              collectionKindOut = LocalInfo::Kind::Vector;
            } else if (declaredCollection == "map") {
              if (declaredCollectionArgs.size() != 2) {
                return false;
              }
              if (valueKindFromTypeName(trimTemplateTypeText(declaredCollectionArgs.front())) ==
                  LocalInfo::ValueKind::Unknown) {
                return false;
              }
              collectionKindOut = LocalInfo::Kind::Map;
            } else if (declaredCollection == "Buffer") {
              if (declaredCollectionArgs.size() != 1) {
                return false;
              }
              collectionKindOut = LocalInfo::Kind::Buffer;
            } else {
              return false;
            }
            collectionValueKindOut = valueKindFromTypeName(
                trimTemplateTypeText(collectionKindOut == LocalInfo::Kind::Map ? declaredCollectionArgs.back()
                                                                              : declaredCollectionArgs.front()));
            return collectionValueKindOut != LocalInfo::ValueKind::Unknown;
          };
          LocalInfo::Kind collectionKind = LocalInfo::Kind::Value;
          LocalInfo::ValueKind collectionValueKind = LocalInfo::ValueKind::Unknown;
          if (resolveCollectionPayload(collectionKind, collectionValueKind) &&
              ir_lowerer::isSupportedPackedResultCollectionKind(collectionKind)) {
            packedKindOut = collectionValueKind;
            return true;
          }
          LocalInfo::ValueKind inferredValueKind = inferExprKind(valueExpr, valueLocals);
          if (inferredValueKind == LocalInfo::ValueKind::Unknown) {
            std::string builtinComparison;
            if (getBuiltinComparisonName(valueExpr, builtinComparison)) {
              inferredValueKind = LocalInfo::ValueKind::Bool;
            }
          }
          if (isFileHandleExpr(valueExpr, valueLocals) && inferredValueKind == LocalInfo::ValueKind::Int64) {
            packedKindOut = inferredValueKind;
            return true;
          }
          std::string inferredStructType;
          ir_lowerer::inferPackedResultStructType(
              valueExpr,
              valueLocals,
              [&](const Expr &candidateExpr) { return resolveDefinitionCall(candidateExpr); },
              [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                return inferStructExprPath(candidateExpr, candidateLocals);
              },
              inferredStructType);
          bool isPackedSingleSlot = false;
          LocalInfo::ValueKind packedStructKind = LocalInfo::ValueKind::Unknown;
          int32_t slotCount = 0;
          if (ir_lowerer::resolveSupportedResultStructPayloadInfo(
                  inferredStructType,
                  [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
                    return resolveStructSlotLayout(structPath, layoutOut);
                  },
                  isPackedSingleSlot,
                  packedStructKind,
                  slotCount)) {
            (void)slotCount;
            structTypeOut = inferredStructType;
            packedKindOut = isPackedSingleSlot ? packedStructKind : LocalInfo::ValueKind::Unknown;
            return true;
          }
          packedKindOut = inferredValueKind;
          structTypeOut.clear();
          return ir_lowerer::isSupportedPackedResultValueKind(packedKindOut);
        };
        auto materializePackedResultStructLocal = [&](int32_t payloadLocal,
                                                      const std::string &structType,
                                                      LocalInfo &paramInfo) -> bool {
          ir_lowerer::PackedResultStructPayloadInfo payloadInfo;
          if (!ir_lowerer::resolvePackedResultStructPayloadInfo(
                  structType,
                  [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
                    return resolveStructSlotLayout(structPath, layoutOut);
                  },
                  payloadInfo)) {
            return false;
          }
          const int32_t baseLocal = nextLocal;
          nextLocal += payloadInfo.slotCount;
          const int32_t ptrLocal = nextLocal++;
          function.instructions.push_back(
              {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(payloadInfo.slotCount - 1))});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
          function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
          if (payloadInfo.isPackedSingleSlot) {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(payloadLocal)});
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + payloadInfo.fieldOffset)});
          } else if (!emitStructCopyFromPtrs(ptrLocal, payloadLocal, payloadInfo.slotCount)) {
            return false;
          }
          paramInfo.index = ptrLocal;
          paramInfo.kind = LocalInfo::Kind::Value;
          paramInfo.valueKind = LocalInfo::ValueKind::Int64;
          paramInfo.structTypeName = structType;
          return true;
        };
        #include "IrLowererLowerEmitExprResultHelpers.h"
        const auto resultOkCallResult = ir_lowerer::tryEmitResultOkCall(
            expr,
            localsIn,
            [&](const Expr &valueExpr, const LocalMap &valueLocals) {
              return inferExprKind(valueExpr, valueLocals);
            },
            [&](const Expr &valueExpr, const LocalMap &valueLocals) {
              return inferStructExprPath(valueExpr, valueLocals);
            },
            [&](const Expr &valueExpr) { return resolveDefinitionCall(valueExpr); },
            [&](const Expr &valueExpr, const LocalMap &valueLocals) {
              return isFileHandleExpr(valueExpr, valueLocals);
            },
            [&](const Expr &valueExpr, const LocalMap &valueLocals) {
              return emitExpr(valueExpr, valueLocals);
            },
            [&]() { return allocTempLocal(); },
            [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
              return resolveStructSlotLayout(structPath, layoutOut);
            },
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            error);
        if (resultOkCallResult == ir_lowerer::ResultOkMethodCallEmitResult::Emitted) {
          return true;
        }
        if (resultOkCallResult == ir_lowerer::ResultOkMethodCallEmitResult::Error) {
          return false;
        }
        const auto resultErrorCallResult = ir_lowerer::tryEmitResultErrorCall(
            expr,
            localsIn,
            resolveResultExprInfo,
            [&](const Expr &valueExpr, const LocalMap &valueLocals) {
              return emitExpr(valueExpr, valueLocals);
            },
            [&]() { return allocTempLocal(); },
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            error);
        if (resultErrorCallResult == ir_lowerer::ResultErrorMethodCallEmitResult::Emitted) {
          return true;
        }
        if (resultErrorCallResult == ir_lowerer::ResultErrorMethodCallEmitResult::Error) {
          return false;
        }
        const auto resultWhyDispatchResult = ir_lowerer::tryEmitResultWhyDispatchCall(
            expr,
            localsIn,
            defMap,
            onErrorTempCounter,
            resolveResultExprInfo,
            [&](const Expr &valueExpr, const LocalMap &valueLocals) {
              return emitExpr(valueExpr, valueLocals);
            },
            [&]() { return allocTempLocal(); },
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            [&](const std::string &value) { return internString(value); },
            [&](const std::string &typeName, const std::string &nsPrefix, std::string &structPathOut) {
              return resolveStructTypeName(typeName, nsPrefix, structPathOut);
            },
            [&](const std::string &definitionPath, ReturnInfo &returnInfoOut) {
              return getReturnInfo && getReturnInfo(definitionPath, returnInfoOut);
            },
            [&](const Expr &bindingExpr) { return bindingKind(bindingExpr); },
            [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
              return resolveStructSlotLayout(structPath, layoutOut);
            },
            [&](const std::string &typeName) { return valueKindFromTypeName(typeName); },
            [&](const Expr &callExpr, const Definition &callee, const LocalMap &callLocals) {
              return emitInlineDefinitionCall(callExpr, callee, callLocals, true);
            },
            [&](int32_t emittedErrorLocal) { return emitFileErrorWhy(emittedErrorLocal); },
            &function.instructions,
            error);
        if (resultWhyDispatchResult == ir_lowerer::ResultWhyDispatchEmitResult::Emitted) {
          return true;
        }
        if (resultWhyDispatchResult == ir_lowerer::ResultWhyDispatchEmitResult::Error) {
          return false;
        }
        const auto fileConstructorResult = ir_lowerer::tryEmitFileConstructorCall(
            expr,
            localsIn,
            [&](const Expr &valueExpr,
                const ir_lowerer::LocalMap &localMap,
                int32_t &stringIndexOut,
                size_t &lengthOut) {
              return resolveStringTableTarget(valueExpr, localMap, stringIndexOut, lengthOut);
            },
            [&](const Expr &valueExpr, const ir_lowerer::LocalMap &localMap) {
              return inferExprKind(valueExpr, localMap);
            },
            [&](const Expr &valueExpr, const ir_lowerer::LocalMap &localMap) {
              return emitExpr(valueExpr, localMap);
            },
            [&](const Expr &valueExpr, const ir_lowerer::LocalMap &localMap) {
              return isEntryArgsName(valueExpr, localMap);
            },
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            error);
        if (fileConstructorResult == ir_lowerer::FileConstructorCallEmitResult::Emitted) {
          return true;
        }
        if (fileConstructorResult == ir_lowerer::FileConstructorCallEmitResult::Error) {
          return false;
        }
        const auto fileHandleCallResult = ir_lowerer::tryEmitFileHandleMethodCall(
            expr,
            localsIn,
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              if (!callExpr.args.empty() && callExpr.args.front().kind == Expr::Kind::Name &&
                  callExpr.args.front().name == "self" && localMap.find("self") != localMap.end()) {
                return false;
              }
              const Definition *callee = resolveMethodCallDefinition(callExpr, localMap);
              if (callee == nullptr) {
                return false;
              }
              return callee->fullPath.rfind("/File/write", 0) == 0 ||
                     callee->fullPath.rfind("/File/write_line", 0) == 0 ||
                     callee->fullPath.rfind("/File/close", 0) == 0;
            },
            [&](const Expr &valueExpr,
                const ir_lowerer::LocalMap &localMap,
                int32_t &stringIndexOut,
                size_t &lengthOut) {
              return resolveStringTableTarget(valueExpr, localMap, stringIndexOut, lengthOut);
            },
            [&](const Expr &valueExpr, const ir_lowerer::LocalMap &localMap) {
              return inferExprKind(valueExpr, localMap);
            },
            [&](const Expr &valueExpr, const ir_lowerer::LocalMap &localMap) {
              return emitExpr(valueExpr, localMap);
            },
            [&]() { return allocTempLocal(); },
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            [&]() { return function.instructions.size(); },
            [&](size_t instructionIndex, int32_t imm) { function.instructions[instructionIndex].imm = imm; },
            error);
        if (fileHandleCallResult == ir_lowerer::FileHandleMethodCallEmitResult::Emitted) {
          return true;
        }
        if (fileHandleCallResult == ir_lowerer::FileHandleMethodCallEmitResult::Error) {
          return false;
        }
        std::string gpuBuiltin;
        if (getBuiltinGpuName(expr, gpuBuiltin)) {
          return ir_lowerer::emitGpuBuiltinLoad(
              gpuBuiltin,
              [&](const char *localName) -> std::optional<int32_t> {
                auto it = localsIn.find(localName);
                if (it == localsIn.end()) {
                  return std::nullopt;
                }
                return it->second.index;
              },
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
              error);
        }
        const auto uploadReadbackResult = ir_lowerer::runLowerExprEmitUploadReadbackPassthroughStep(
            expr,
            localsIn,
            emitUploadPassthroughCall,
            emitReadbackPassthroughCall,
            [&](const Expr &argExpr, const LocalMap &argLocals) { return emitExpr(argExpr, argLocals); },
            error);
        if (uploadReadbackResult != ir_lowerer::UnaryPassthroughCallResult::NotMatched) {
          return uploadReadbackResult == ir_lowerer::UnaryPassthroughCallResult::Emitted;
        }
        const auto bufferBuiltinResult = ir_lowerer::tryEmitBufferBuiltinDispatchWithLocals(
            expr,
            localsIn,
            [&](const std::string &typeName) { return valueKindFromTypeName(typeName); },
            [&](const Expr &valueExpr, const ir_lowerer::LocalMap &localMap) {
              return inferExprKind(valueExpr, localMap);
            },
            [&](int32_t localCount) {
              const int32_t baseLocal = nextLocal;
              nextLocal += localCount;
              return baseLocal;
            },
            [&]() { return allocTempLocal(); },
            [&](const Expr &valueExpr, const ir_lowerer::LocalMap &localMap) {
              return emitExpr(valueExpr, localMap);
            },
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            error);
        if (bufferBuiltinResult != ir_lowerer::BufferBuiltinDispatchResult::NotHandled) {
          return bufferBuiltinResult == ir_lowerer::BufferBuiltinDispatchResult::Emitted;
        }
        #include "IrLowererLowerEmitExprCollectionHelpers.h"
        #include "IrLowererLowerEmitExprTailDispatch.h"
