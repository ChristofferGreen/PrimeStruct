        auto resolveDirectHelperPath = [&](const Expr &callExpr) {
          if (!callExpr.name.empty() && callExpr.name.front() == '/') {
            return callExpr.name;
          }
          if (!callExpr.namespacePrefix.empty()) {
            std::string scoped = callExpr.namespacePrefix;
            if (!scoped.empty() && scoped.front() != '/') {
              scoped.insert(scoped.begin(), '/');
            }
            return scoped + "/" + callExpr.name;
          }
          return callExpr.name;
        };
        auto isDirectHelperDefinitionFamily = [&](const std::string &rawPath,
                                                  const Definition &callee) {
          return callee.fullPath == rawPath ||
                 callee.fullPath.rfind(rawPath + "__", 0) == 0 ||
                 normalizeCollectionHelperPath(rawPath) ==
                     normalizeCollectionHelperPath(callee.fullPath);
        };
        auto findDirectHelperDefinition = [&](const std::string &rawPath) -> const Definition * {
          auto defIt = defMap.find(rawPath);
          if (defIt != defMap.end()) {
            return defIt->second;
          }
          auto matchesGeneratedLeafDefinition = [&](const std::string &path,
                                                    const char *marker,
                                                    size_t markerSize) {
            return path.rfind(rawPath, 0) == 0 &&
                   path.compare(rawPath.size(), markerSize, marker) == 0 &&
                   path.find('/', rawPath.size() + markerSize) ==
                       std::string::npos;
          };
          for (const auto &[path, def] : defMap) {
            if (def == nullptr) {
              continue;
            }
            if (matchesGeneratedLeafDefinition(path, "__t", 3) ||
                matchesGeneratedLeafDefinition(path, "__ov", 4)) {
              return def;
            }
          }
          return nullptr;
        };
        auto stripGeneratedHelperSuffix = [](std::string helperPath) {
          const size_t leafStart = helperPath.find_last_of('/');
          const size_t generatedSuffix =
              helperPath.find("__", leafStart == std::string::npos ? 0 : leafStart + 1);
          if (generatedSuffix != std::string::npos) {
            helperPath.erase(generatedSuffix);
          }
          return helperPath;
        };
        auto extractHelperTail = [&](std::string helperPath) {
          helperPath = stripGeneratedHelperSuffix(std::move(helperPath));
          const size_t slash = helperPath.find_last_of('/');
          if (slash != std::string::npos) {
            helperPath = helperPath.substr(slash + 1);
          }
          return helperPath;
        };
        auto isInternalSoaHelperFamilyName = [&](const std::string &helperName) {
          return helperName.rfind("soaColumn", 0) == 0 ||
                 helperName.rfind("soaColumns", 0) == 0 ||
                 helperName.rfind("SoaColumn", 0) == 0 ||
                 helperName.rfind("SoaColumns", 0) == 0;
        };
        auto isInternalSoaHelperFamilyPath = [&](const std::string &path) {
          return isInternalSoaHelperFamilyName(
              extractHelperTail(normalizeCollectionHelperPath(path)));
        };
        auto isSoaWrapperHelperFamilyPath = [&](const std::string &path) {
          const std::string normalizedPath = stripGeneratedHelperSuffix(path);
          return normalizedPath.rfind("/soa_vector/", 0) == 0 ||
                 normalizedPath == "/to_aos" ||
                 normalizedPath == "/to_aos_ref" ||
                 normalizedPath.rfind("/std/collections/soa_vector/", 0) == 0 ||
                 normalizedPath.rfind("/std/collections/experimental_soa_vector/soaVector", 0) == 0 ||
                 normalizedPath.rfind("/std/collections/experimental_soa_vector_conversions/soaVector", 0) == 0;
        };
        auto findDirectInternalSoaDefinition = [&](const std::string &rawPath)
            -> const Definition * {
          const std::string helperName = extractHelperTail(rawPath);
          if (!isInternalSoaHelperFamilyName(helperName)) {
            return nullptr;
          }
          for (const auto &[path, def] : defMap) {
            if (def == nullptr ||
                path.rfind("/std/collections/internal_soa_storage/", 0) != 0) {
              continue;
            }
            if (extractHelperTail(path) == helperName) {
              return def;
            }
          }
          return nullptr;
        };
        auto findDirectStructDefinition = [&](const Expr &callExpr) -> const Definition * {
          const std::string rawPath = resolveDirectHelperPath(callExpr);
          if (const Definition *rawDef = findDirectHelperDefinition(rawPath);
              rawDef != nullptr && ir_lowerer::isStructDefinition(*rawDef)) {
            return rawDef;
          }
          std::string directStructPath;
          if (!resolveStructTypeName(callExpr.name, callExpr.namespacePrefix, directStructPath)) {
            return nullptr;
          }
          if (const Definition *structDef = findDirectHelperDefinition(directStructPath);
              structDef != nullptr && ir_lowerer::isStructDefinition(*structDef)) {
            return structDef;
          }
          return nullptr;
        };
        auto resolveHelperReturnedArrayVectorAccessTargetInfo =
            [&](const Expr &targetCallExpr,
                ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
              targetInfoOut = {};
              const std::string inferredReceiverStruct =
                  inferStructExprPath(targetCallExpr, localsIn);
              if (inferredReceiverStruct.rfind(
                      "/std/collections/experimental_soa_vector/SoaVector__", 0) == 0 ||
                  normalizeCollectionBindingTypeName(inferredReceiverStruct) ==
                      "soa_vector") {
                targetInfoOut.isArrayOrVectorTarget = true;
                targetInfoOut.isVectorTarget = false;
                targetInfoOut.isSoaVector = true;
                targetInfoOut.structTypeName = inferredReceiverStruct;
                return true;
              }
              const Definition *callee = resolveDefinitionCall(targetCallExpr);
              if (callee == nullptr) {
                return false;
              }
              std::string collectionName;
              std::vector<std::string> collectionArgs;
              if (!ir_lowerer::inferDeclaredReturnCollection(*callee,
                                                             collectionName,
                                                             collectionArgs)) {
                return false;
              }
              if ((collectionName != "array" && collectionName != "vector" &&
                   collectionName != "soa_vector") ||
                  collectionArgs.size() != 1) {
                return false;
              }
              targetInfoOut.isArrayOrVectorTarget = true;
              targetInfoOut.isVectorTarget = (collectionName == "vector");
              targetInfoOut.isSoaVector = (collectionName == "soa_vector");
              targetInfoOut.elemKind =
                  ir_lowerer::valueKindFromTypeName(collectionArgs.front());
              if (targetInfoOut.isSoaVector) {
                std::string elementTypeName =
                    trimTemplateTypeText(collectionArgs.front());
                if (!elementTypeName.empty() &&
                    elementTypeName.front() == '/') {
                  elementTypeName.erase(elementTypeName.begin());
                }
                targetInfoOut.structTypeName =
                    "/std/collections/experimental_soa_vector/SoaVector__" +
                    elementTypeName;
              }
              return true;
            };
        if (!expr.isMethodCall) {
          const std::string rawPath = resolveDirectHelperPath(expr);
          const Definition *directCallee = resolveDefinitionCall(expr);
          if (directCallee == nullptr &&
              isInternalSoaHelperFamilyPath(rawPath)) {
            directCallee = findDirectInternalSoaDefinition(rawPath);
          }
          if (directCallee == nullptr) {
            directCallee = findDirectStructDefinition(expr);
          }
          if (directCallee == nullptr &&
              isSoaWrapperHelperFamilyPath(rawPath)) {
            directCallee = findDirectHelperDefinition(rawPath);
          }
          if (directCallee == nullptr &&
              (rawPath.rfind("/map/", 0) == 0 ||
               rawPath.rfind("/std/collections/map/", 0) == 0)) {
            directCallee = findDirectHelperDefinition(rawPath);
          }
          if (directCallee != nullptr) {
            if (ir_lowerer::isStructDefinition(*directCallee)) {
              if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                return false;
              }
              return true;
            }
            if (directCallee->fullPath.rfind("/std/collections/internal_soa_storage/", 0) == 0 &&
                isInternalSoaHelperFamilyPath(directCallee->fullPath)) {
              if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                return false;
              }
              return true;
            }
            if (isSoaWrapperHelperFamilyPath(rawPath) ||
                isSoaWrapperHelperFamilyPath(directCallee->fullPath)) {
              if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                return false;
              }
              return true;
            }
            std::string helperName;
            if (resolveMapHelperAliasName(expr, helperName) &&
                (helperName == "count" || helperName == "contains" ||
                 helperName == "tryAt") &&
                (rawPath.rfind("/map/", 0) == 0 ||
                 rawPath.rfind("/std/collections/map/", 0) == 0) &&
                isDirectHelperDefinitionFamily(rawPath, *directCallee)) {
              if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                return false;
              }
              return true;
            }
            std::string accessName;
            if (getBuiltinArrayAccessName(expr, accessName) &&
                expr.args.size() == 2 &&
                rawPath.rfind("/std/collections/map/", 0) == 0 &&
                isDirectHelperDefinitionFamily(rawPath, *directCallee)) {
              error =
                  "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions (call=" +
                  resolveExprPath(expr) + ", name=" + expr.name +
                  ", args=" + std::to_string(expr.args.size()) +
                  ", method=" + std::string(expr.isMethodCall ? "true" : "false") + ")";
              return false;
            }
          }
        }

        std::string accessName;
        if (getBuiltinArrayAccessName(expr, accessName)) {
          if (expr.args.size() != 2) {
            error = accessName + " requires exactly two arguments";
            return false;
          }
          if (!emitBuiltinArrayAccess(
                  accessName,
                  expr.args[0],
                  expr.args[1],
                  localsIn,
                  resolveStringTableTarget,
                  {},
                  resolveHelperReturnedArrayVectorAccessTargetInfo,
                  inferExprKind,
                  isEntryArgsName,
                  allocTempLocal,
                  [&](const Expr &valueExpr, const LocalMap &valueLocals) {
                    return emitExpr(valueExpr, valueLocals);
                  },
                  emitStringIndexOutOfBounds,
                  emitMapKeyNotFound,
                  emitArrayIndexOutOfBounds,
                  [&]() { return function.instructions.size(); },
                  [&](IrOpcode opcode, uint64_t imm) {
                    function.instructions.push_back({opcode, imm});
                  },
                  [&](size_t instructionIndex, uint64_t imm) {
                    function.instructions[instructionIndex].imm = imm;
                  },
                  error)) {
            return false;
          }
          return true;
        }

        const auto countAccessResult = tryEmitCountAccessCall(
            expr,
            localsIn,
            isArrayCountCall,
            isVectorCapacityCall,
            isStringCountCall,
            isEntryArgsName,
            [&](const Expr &targetExpr, const LocalMap &targetLocals) {
              const auto targetInfo =
                  ir_lowerer::resolveArrayVectorAccessTargetInfo(
                      targetExpr,
                      targetLocals,
                      resolveHelperReturnedArrayVectorAccessTargetInfo);
              const std::string structPath = inferStructExprPath(targetExpr, targetLocals);
              const bool isExperimentalVectorTarget =
                  structPath == "/std/collections/experimental_vector/Vector" ||
                  structPath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
              const bool isExperimentalMapTarget =
                  structPath == "/std/collections/experimental_map/Map" ||
                  structPath.rfind("/std/collections/experimental_map/Map__", 0) == 0;
              return targetInfo.isArrayOrVectorTarget || structPath == "/array" ||
                     structPath == "/vector" || structPath == "/Buffer" || structPath == "/map" ||
                     structPath == "/soa_vector" || isExperimentalVectorTarget ||
                     isExperimentalMapTarget;
            },
            [&](const Expr &targetExpr, const LocalMap &targetLocals) {
              const auto targetInfo =
                  ir_lowerer::resolveArrayVectorAccessTargetInfo(
                      targetExpr,
                      targetLocals,
                      resolveHelperReturnedArrayVectorAccessTargetInfo);
              const std::string structPath = inferStructExprPath(targetExpr, targetLocals);
              return (targetInfo.isArrayOrVectorTarget && targetInfo.isVectorTarget) ||
                     structPath == "/std/collections/experimental_vector/Vector" ||
                     structPath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
            },
            [&](const Expr &targetExpr, const LocalMap &targetLocals) {
              const auto targetInfo =
                  ir_lowerer::resolveArrayVectorAccessTargetInfo(
                      targetExpr,
                      targetLocals,
                      resolveHelperReturnedArrayVectorAccessTargetInfo);
              const std::string structPath = inferStructExprPath(targetExpr, targetLocals);
              return (targetInfo.isArrayOrVectorTarget && targetInfo.isVectorTarget) ||
                     structPath == "/std/collections/experimental_vector/Vector" ||
                     structPath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
            },
            inferExprKind,
            resolveStringTableTarget,
            [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
            [&](IrOpcode opcode, uint64_t imm) { function.instructions.push_back({opcode, imm}); },
            error);
        if (countAccessResult == CountAccessCallEmitResult::Emitted) {
          return true;
        }
        if (countAccessResult == CountAccessCallEmitResult::Error) {
          return false;
        }
        error =
            "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions (call=" +
            resolveExprPath(expr) + ", name=" + expr.name +
            ", args=" + std::to_string(expr.args.size()) +
            ", method=" + std::string(expr.isMethodCall ? "true" : "false") + ")";
        return false;
      }
      default:
        error = "native backend only supports literals, names, and calls";
        return false;
    }
  };

  emitPrintArg = [&](const Expr &arg, const LocalMap &localsIn, const PrintBuiltin &builtin) -> bool {
    uint64_t flags = encodePrintFlags(builtin.newline, builtin.target == PrintTarget::Err);
    if (arg.kind == Expr::Kind::Call) {
      std::string accessName;
      if (getBuiltinArrayAccessName(arg, accessName)) {
        if (arg.args.size() != 2) {
          error = accessName + " requires exactly two arguments";
          return false;
        }
        if (isEntryArgsName(arg.args.front(), localsIn)) {
          LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(arg.args[1], localsIn));
          if (!isSupportedIndexKind(indexKind)) {
            error = "native backend requires integer indices for " + accessName;
            return false;
          }

          const int32_t indexLocal = allocTempLocal();
          if (!emitExpr(arg.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

          if (accessName == "at") {
            if (indexKind != LocalInfo::ValueKind::UInt64) {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
              function.instructions.push_back({pushZeroForIndex(indexKind), 0});
              function.instructions.push_back({cmpLtForIndex(indexKind), 0});
              size_t jumpNonNegative = function.instructions.size();
              function.instructions.push_back({IrOpcode::JumpIfZero, 0});
              emitArrayIndexOutOfBounds();
              size_t nonNegativeIndex = function.instructions.size();
              function.instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
            }

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({IrOpcode::PushArgc, 0});
            function.instructions.push_back({cmpGeForIndex(indexKind), 0});
            size_t jumpInRange = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitArrayIndexOutOfBounds();
            size_t inRangeIndex = function.instructions.size();
            function.instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);
          }

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
          IrOpcode printOp = (accessName == "at_unsafe") ? IrOpcode::PrintArgvUnsafe : IrOpcode::PrintArgv;
          function.instructions.push_back({printOp, flags});
          return true;
        }
      }
    }
    if (arg.kind == Expr::Kind::StringLiteral) {
      std::string decoded;
      if (!ir_lowerer::parseLowererStringLiteral(arg.stringValue, decoded, error)) {
        return false;
      }
      int32_t index = internString(decoded);
      function.instructions.push_back(
          {IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(index), flags)});
      return true;
    }
    if (arg.kind == Expr::Kind::Name) {
      auto it = localsIn.find(arg.name);
      if (it == localsIn.end()) {
        error = "native backend does not know identifier: " + arg.name;
        return false;
      }
      if (it->second.valueKind == LocalInfo::ValueKind::String) {
        if (it->second.stringSource == LocalInfo::StringSource::TableIndex) {
          if (it->second.stringIndex < 0) {
            error = "native backend missing string data for: " + arg.name;
            return false;
          }
          function.instructions.push_back(
              {IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(it->second.stringIndex), flags)});
          return true;
        }
        if (it->second.stringSource == LocalInfo::StringSource::ArgvIndex) {
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          IrOpcode printOp = it->second.argvChecked ? IrOpcode::PrintArgv : IrOpcode::PrintArgvUnsafe;
          function.instructions.push_back({printOp, flags});
          return true;
        }
        if (it->second.stringSource == LocalInfo::StringSource::RuntimeIndex) {
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          function.instructions.push_back({IrOpcode::PrintStringDynamic, flags});
          return true;
        }
      }
    }
    if (!emitExpr(arg, localsIn)) {
      return false;
    }
    LocalInfo::ValueKind kind = inferExprKind(arg, localsIn);
    if (kind == LocalInfo::ValueKind::String) {
      function.instructions.push_back({IrOpcode::PrintStringDynamic, flags});
      return true;
    }
    if (kind == LocalInfo::ValueKind::Int64) {
      function.instructions.push_back({IrOpcode::PrintI64, flags});
      return true;
    }
    if (kind == LocalInfo::ValueKind::UInt64) {
      function.instructions.push_back({IrOpcode::PrintU64, flags});
      return true;
    }
    if (kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Bool) {
      function.instructions.push_back({IrOpcode::PrintI32, flags});
      return true;
    }
    error = builtin.name + " requires an integer/bool or string literal/binding argument";
    return false;
  };
