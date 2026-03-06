
  ir_lowerer::LowerInferenceSetupBootstrapState inferenceSetupBootstrap;
  if (!ir_lowerer::runLowerInferenceSetupBootstrap(
          {
              .defMap = &defMap,
              .importAliases = &importAliases,
              .structNames = &structNames,
              .isArrayCountCall = isArrayCountCall,
              .isVectorCapacityCall = isVectorCapacityCall,
              .isEntryArgsName = isEntryArgsName,
              .resolveExprPath = resolveExprPath,
              .getBuiltinOperatorName = getBuiltinOperatorName,
          },
          inferenceSetupBootstrap,
          error)) {
    return false;
  }

  auto &returnInfoCache = inferenceSetupBootstrap.returnInfoCache;
  auto &returnInferenceStack = inferenceSetupBootstrap.returnInferenceStack;
  auto &getReturnInfo = inferenceSetupBootstrap.getReturnInfo;
  auto &inferExprKind = inferenceSetupBootstrap.inferExprKind;
  auto &inferArrayElementKind = inferenceSetupBootstrap.inferArrayElementKind;
  auto &inferBufferElementKind = inferenceSetupBootstrap.inferBufferElementKind;
  auto &inferLiteralOrNameExprKind = inferenceSetupBootstrap.inferLiteralOrNameExprKind;
  auto &resolveMethodCallDefinition = inferenceSetupBootstrap.resolveMethodCallDefinition;
  auto &inferPointerTargetKind = inferenceSetupBootstrap.inferPointerTargetKind;

  if (!ir_lowerer::runLowerInferenceArrayKindSetup(
          {
              .defMap = &defMap,
              .resolveExprPath = resolveExprPath,
              .resolveStructArrayInfoFromPath = resolveStructArrayInfoFromPath,
              .isArrayCountCall = isArrayCountCall,
              .isStringCountCall = isStringCountCall,
          },
          inferenceSetupBootstrap,
          error)) {
    return false;
  }
  if (!ir_lowerer::runLowerInferenceExprKindBaseSetup(
          {
              .getMathConstantName = getMathConstantName,
          },
          inferenceSetupBootstrap,
          error)) {
    return false;
  }

  inferExprKind = [&](const Expr &expr, const LocalMap &localsIn) -> LocalInfo::ValueKind {
    LocalInfo::ValueKind literalOrNameKind = LocalInfo::ValueKind::Unknown;
    if (inferLiteralOrNameExprKind(expr, localsIn, literalOrNameKind)) {
      return literalOrNameKind;
    }
    switch (expr.kind) {
      case Expr::Kind::Call: {
        if (expr.isFieldAccess) {
          if (expr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          const Expr &receiver = expr.args.front();
          std::string structPath = inferStructExprPath(receiver, localsIn);
          if (structPath.empty()) {
            return LocalInfo::ValueKind::Unknown;
          }
          StructSlotFieldInfo fieldInfo;
          if (!resolveStructFieldSlot(structPath, expr.name, fieldInfo)) {
            return LocalInfo::ValueKind::Unknown;
          }
          return fieldInfo.structPath.empty() ? fieldInfo.valueKind : LocalInfo::ValueKind::Unknown;
        }
        if (!expr.isMethodCall &&
            (isSimpleCallName(expr, "take") || isSimpleCallName(expr, "borrow")) &&
            expr.args.size() == 1) {
          UninitializedStorageAccess access;
          bool resolved = false;
          if (!resolveUninitializedStorage(expr.args.front(), localsIn, access, resolved)) {
            return LocalInfo::ValueKind::Unknown;
          }
          if (resolved) {
            if (access.typeInfo.kind == LocalInfo::Kind::Value &&
                access.typeInfo.structPath.empty() &&
                access.typeInfo.valueKind != LocalInfo::ValueKind::Unknown) {
              return access.typeInfo.valueKind;
            }
            return LocalInfo::ValueKind::Unknown;
          }
        }
        if (expr.isMethodCall) {
          if (!expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
              expr.args.front().name == "Result") {
            if (expr.name == "ok") {
              return expr.args.size() > 1 ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
            }
            if (expr.name == "why") {
              return LocalInfo::ValueKind::String;
            }
          }
          if (!expr.args.empty() && expr.name == "why") {
            const Expr &receiver = expr.args.front();
            if (receiver.kind == Expr::Kind::Name) {
              if (receiver.name == "FileError") {
                return LocalInfo::ValueKind::String;
              }
              auto it = localsIn.find(receiver.name);
              if (it != localsIn.end() && it->second.isFileError) {
                return LocalInfo::ValueKind::String;
              }
            }
          }
          if (!expr.args.empty() && expr.args.front().kind == Expr::Kind::Name) {
            auto it = localsIn.find(expr.args.front().name);
            if (it != localsIn.end() && it->second.isFileHandle) {
              if (expr.name == "write" || expr.name == "write_line" || expr.name == "write_byte" ||
                  expr.name == "write_bytes" || expr.name == "flush" || expr.name == "close") {
                return LocalInfo::ValueKind::Int32;
              }
            }
          }
        } else {
          if (isSimpleCallName(expr, "File")) {
            return LocalInfo::ValueKind::Int64;
          }
          if (isSimpleCallName(expr, "try") && expr.args.size() == 1) {
            const Expr &arg = expr.args.front();
            if (arg.kind == Expr::Kind::Name) {
              auto it = localsIn.find(arg.name);
              if (it != localsIn.end() && it->second.isResult) {
                return it->second.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
              }
            }
            if (arg.kind == Expr::Kind::Call) {
              if (!arg.isMethodCall && isSimpleCallName(arg, "File")) {
                return LocalInfo::ValueKind::Int64;
              }
              if (arg.isMethodCall && !arg.args.empty() && arg.args.front().kind == Expr::Kind::Name) {
                auto it = localsIn.find(arg.args.front().name);
                if (it != localsIn.end() && it->second.isFileHandle) {
                  if (arg.name == "write" || arg.name == "write_line" || arg.name == "write_byte" ||
                      arg.name == "write_bytes" || arg.name == "flush" || arg.name == "close") {
                    return LocalInfo::ValueKind::Int32;
                  }
                }
                if (arg.args.front().name == "Result" && arg.name == "ok") {
                  return arg.args.size() > 1 ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
                }
              }
            }
          }
        }
        LocalInfo::ValueKind callReturnKind = LocalInfo::ValueKind::Unknown;
        const auto callReturnResolution = resolveCallExpressionReturnKind(
            expr,
            localsIn,
            [&](const Expr &candidate,
                const LocalMap &,
                LocalInfo::ValueKind &kindOut,
                bool &matchedOut) {
              bool definitionMatched = false;
              const bool resolved = resolveDefinitionCallReturnKind(
                  candidate, defMap, resolveExprPath, getReturnInfo, false, kindOut, &definitionMatched);
              matchedOut = definitionMatched;
              return resolved;
            },
            [&](const Expr &candidate,
                const LocalMap &candidateLocals,
                LocalInfo::ValueKind &kindOut,
                bool &matchedOut) {
              bool countMethodResolved = false;
              const bool resolved = resolveCountMethodCallReturnKind(candidate,
                                                                    candidateLocals,
                                                                    isArrayCountCall,
                                                                    isStringCountCall,
                                                                    resolveMethodCallDefinition,
                                                                    getReturnInfo,
                                                                    false,
                                                                    kindOut,
                                                                    &countMethodResolved);
              matchedOut = countMethodResolved;
              return resolved;
            },
            [&](const Expr &candidate,
                const LocalMap &candidateLocals,
                LocalInfo::ValueKind &kindOut,
                bool &matchedOut) {
              bool methodResolved = false;
              const bool resolved = resolveMethodCallReturnKind(
                  candidate, candidateLocals, resolveMethodCallDefinition, getReturnInfo, false, kindOut, &methodResolved);
              matchedOut = methodResolved;
              return resolved;
            },
            callReturnKind);
        if (callReturnResolution == CallExpressionReturnKindResolution::Resolved) {
          return callReturnKind;
        }
        if (callReturnResolution == CallExpressionReturnKindResolution::MatchedButUnsupported) {
          return LocalInfo::ValueKind::Unknown;
        }
        LocalInfo::ValueKind countCapacityKind = LocalInfo::ValueKind::Unknown;
        if (inferCountCapacityCallReturnKind(
                expr,
                localsIn,
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return isArrayCountCall(candidateExpr, candidateLocals);
                },
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return isStringCountCall(candidateExpr, candidateLocals);
                },
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return isVectorCapacityCall(candidateExpr, candidateLocals);
                },
                countCapacityKind) == CountCapacityCallReturnKindResolution::Resolved) {
          return countCapacityKind;
        }
        LocalInfo::ValueKind accessElementKind = LocalInfo::ValueKind::Unknown;
        if (resolveArrayMapAccessElementKind(
                expr,
                localsIn,
                [&](const Expr &candidate, const LocalMap &candidateLocals) {
                  return isEntryArgsName(candidate, candidateLocals);
                },
                accessElementKind) == ArrayMapAccessElementKindResolution::Resolved) {
          return accessElementKind;
        }
        LocalInfo::ValueKind gpuBufferKind = LocalInfo::ValueKind::Unknown;
        if (inferGpuBufferCallReturnKind(
                expr,
                localsIn,
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return inferBufferElementKind(candidateExpr, candidateLocals);
                },
                gpuBufferKind) == GpuBufferCallReturnKindResolution::Resolved) {
          return gpuBufferKind;
        }
        LocalInfo::ValueKind comparisonOperatorKind = LocalInfo::ValueKind::Unknown;
        if (inferComparisonOperatorCallReturnKind(
                expr,
                localsIn,
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return inferExprKind(candidateExpr, candidateLocals);
                },
                [&](LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
                  return combineNumericKinds(left, right);
                },
                comparisonOperatorKind) == ComparisonOperatorCallReturnKindResolution::Resolved) {
          return comparisonOperatorKind;
        }
        LocalInfo::ValueKind mathBuiltinKind = LocalInfo::ValueKind::Unknown;
        if (inferMathBuiltinReturnKind(
                expr,
                localsIn,
                hasMathImport,
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return inferExprKind(candidateExpr, candidateLocals);
                },
                [&](LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
                  return combineNumericKinds(left, right);
                },
                mathBuiltinKind) == MathBuiltinReturnKindResolution::Resolved) {
          return mathBuiltinKind;
        }
        LocalInfo::ValueKind nonMathScalarKind = LocalInfo::ValueKind::Unknown;
        if (inferNonMathScalarCallReturnKind(
                expr,
                localsIn,
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return inferExprKind(candidateExpr, candidateLocals);
                },
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return inferPointerTargetKind(candidateExpr, candidateLocals);
                },
                nonMathScalarKind) == NonMathScalarCallReturnKindResolution::Resolved) {
          return nonMathScalarKind;
        }
        LocalInfo::ValueKind controlFlowKind = LocalInfo::ValueKind::Unknown;
        if (inferControlFlowCallReturnKind(
                expr,
                localsIn,
                [&](const Expr &candidateExpr) { return resolveExprPath(candidateExpr); },
                [&](const Expr &candidateExpr, Expr &expandedExpr, std::string &errorOut) {
                  return lowerMatchToIf(candidateExpr, expandedExpr, errorOut);
                },
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return inferExprKind(candidateExpr, candidateLocals);
                },
                [&](LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
                  return combineNumericKinds(left, right);
                },
                [&](const std::vector<Expr> &bodyExpressions, const LocalMap &localsBase) {
                  return inferBodyValueKindWithLocalsScaffolding(
                      bodyExpressions,
                      localsBase,
                      [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                        return inferExprKind(candidateExpr, candidateLocals);
                      },
                      [&](const Expr &candidateExpr) { return isBindingMutable(candidateExpr); },
                      [&](const Expr &candidateExpr) { return bindingKind(candidateExpr); },
                      [&](const Expr &candidateExpr) { return hasExplicitBindingTypeTransform(candidateExpr); },
                      [&](const Expr &candidateExpr, LocalInfo::Kind kind) {
                        return bindingValueKind(candidateExpr, kind);
                      },
                      [&](const Expr &candidateExpr, LocalInfo &info) { applyStructArrayInfo(candidateExpr, info); },
                      [&](const Expr &candidateExpr, LocalInfo &info) { applyStructValueInfo(candidateExpr, info); },
                      [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                        return inferStructExprPath(candidateExpr, candidateLocals);
                      });
                },
                [&](const std::string &path) { return defMap.find(path) != defMap.end(); },
                error,
                controlFlowKind) == ControlFlowCallReturnKindResolution::Resolved) {
          return controlFlowKind;
        }
        LocalInfo::ValueKind pointerBuiltinKind = LocalInfo::ValueKind::Unknown;
        if (inferPointerBuiltinCallReturnKind(
                expr,
                localsIn,
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return inferPointerTargetKind(candidateExpr, candidateLocals);
                },
                pointerBuiltinKind) == PointerBuiltinCallReturnKindResolution::Resolved) {
          return pointerBuiltinKind;
        }
        return LocalInfo::ValueKind::Unknown;
      }
      default:
        return LocalInfo::ValueKind::Unknown;
    }
  };

  getReturnInfo = [&](const std::string &path, ReturnInfo &outInfo) -> bool {
    auto cached = returnInfoCache.find(path);
    if (cached != returnInfoCache.end()) {
      outInfo = cached->second;
      return true;
    }
    auto defIt = defMap.find(path);
    if (defIt == defMap.end() || !defIt->second) {
      error = "native backend cannot resolve definition: " + path;
      return false;
    }
    if (!returnInferenceStack.insert(path).second) {
      error = "native backend return type inference requires explicit annotation on " + path;
      return false;
    }

    const Definition &def = *defIt->second;
    ReturnInfo info;
