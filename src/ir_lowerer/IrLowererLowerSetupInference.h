
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
  auto &inferCallExprBaseKind = inferenceSetupBootstrap.inferCallExprBaseKind;
  auto &inferCallExprDirectReturnKind = inferenceSetupBootstrap.inferCallExprDirectReturnKind;
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
  if (!ir_lowerer::runLowerInferenceExprKindCallBaseSetup(
          {
              .inferStructExprPath = inferStructExprPath,
              .resolveStructFieldSlot = resolveStructFieldSlot,
              .resolveUninitializedStorage = resolveUninitializedStorage,
          },
          inferenceSetupBootstrap,
          error)) {
    return false;
  }
  if (!ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
          {
              .defMap = &defMap,
              .resolveExprPath = resolveExprPath,
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
        LocalInfo::ValueKind callBaseKind = LocalInfo::ValueKind::Unknown;
        if (inferCallExprBaseKind(expr, localsIn, callBaseKind)) {
          return callBaseKind;
        }
        LocalInfo::ValueKind callReturnKind = LocalInfo::ValueKind::Unknown;
        const auto callReturnResolution = inferCallExprDirectReturnKind(expr, localsIn, callReturnKind);
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
