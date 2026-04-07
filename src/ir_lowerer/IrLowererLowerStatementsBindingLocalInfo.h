      if (!semanticLocalAutoBinding && !hasExplicitBindingTypeTransform(stmt) && info.kind == LocalInfo::Kind::Value) {
        ResultExprInfo inferredResultInfo;
        if (resolveResultExprInfoFromLocals(
                init,
                localsIn,
                [&](const Expr &callExpr, const LocalMap &callLocals) {
                  return resolveMethodCallDefinition(callExpr, callLocals);
                },
                [&](const Expr &callExpr) { return resolveDefinitionCall(callExpr); },
                [&](const std::string &path, ReturnInfo &infoOut) { return getReturnInfo(path, infoOut); },
                [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferExprKind(valueExpr, valueLocals); },
                inferredResultInfo,
                &callResolutionAdapters.semanticProductTargets,
                &error) &&
            inferredResultInfo.isResult) {
          info.isResult = true;
          info.resultHasValue = inferredResultInfo.hasValue;
          info.resultValueKind = inferredResultInfo.valueKind;
          info.resultValueCollectionKind = inferredResultInfo.valueCollectionKind;
          info.resultValueMapKeyKind = inferredResultInfo.valueMapKeyKind;
          info.resultValueIsFileHandle = inferredResultInfo.valueIsFileHandle;
          info.resultValueStructType = inferredResultInfo.valueStructType;
          info.resultErrorType = inferredResultInfo.errorType;
          info.valueKind = info.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
        } else if (!error.empty()) {
          return false;
        }
      }
      auto assignInferredFileHandleBinding = [&]() -> bool {
        if (semanticLocalAutoBinding || hasExplicitBindingTypeTransform(stmt) ||
            info.kind != LocalInfo::Kind::Value || info.isResult ||
            info.isFileHandle) {
          return true;
        }
        if (init.kind == Expr::Kind::Name) {
          auto existing = localsIn.find(init.name);
          if (existing != localsIn.end() && existing->second.isFileHandle) {
            info.isFileHandle = true;
            info.valueKind = LocalInfo::ValueKind::Int64;
          }
          return true;
        }
        if (init.kind == Expr::Kind::Call && !init.isMethodCall && isSimpleCallName(init, "File") &&
            init.templateArgs.size() == 1) {
          info.isFileHandle = true;
          info.valueKind = LocalInfo::ValueKind::Int64;
          return true;
        }
        if (!(init.kind == Expr::Kind::Call && !init.isMethodCall && isSimpleCallName(init, "try") &&
              init.args.size() == 1)) {
          return true;
        }
        ResultExprInfo inferredTryResultInfo;
        if (!resolveResultExprInfoFromLocals(
                init.args.front(),
                localsIn,
                [&](const Expr &callExpr, const LocalMap &callLocals) {
                  return resolveMethodCallDefinition(callExpr, callLocals);
                },
                [&](const Expr &callExpr) { return resolveDefinitionCall(callExpr); },
                [&](const std::string &path, ReturnInfo &infoOut) { return getReturnInfo(path, infoOut); },
                [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferExprKind(valueExpr, valueLocals); },
                inferredTryResultInfo,
                &callResolutionAdapters.semanticProductTargets,
                &error) ||
            !inferredTryResultInfo.isResult || !inferredTryResultInfo.hasValue || !inferredTryResultInfo.valueIsFileHandle) {
          if (!error.empty()) {
            return false;
          }
          return true;
        }
        info.isFileHandle = true;
        info.valueKind = LocalInfo::ValueKind::Int64;
        return true;
      };
      if (!assignInferredFileHandleBinding()) {
        return false;
      }
      for (const auto &transform : bindingTypeExprRef.transforms) {
        if (transform.name == "soa_vector") {
          info.isSoaVector = true;
          break;
        }
      }
      if (!info.isSoaVector && init.kind == Expr::Kind::Name) {
        auto existing = localsIn.find(init.name);
        if (existing != localsIn.end() && existing->second.isSoaVector) {
          info.isSoaVector = true;
        }
      }
      if (!info.isSoaVector && init.kind == Expr::Kind::Call) {
        std::string collection;
        if (getBuiltinCollectionName(init, collection) && collection == "soa_vector") {
          info.isSoaVector = true;
        }
      }
      auto applySpecializedWrappedMapBindingInfo = [&](const Expr &bindingExpr, LocalInfo &bindingInfo) {
        if ((bindingInfo.kind != LocalInfo::Kind::Reference &&
             bindingInfo.kind != LocalInfo::Kind::Pointer) ||
            bindingInfo.referenceToMap || bindingInfo.pointerToMap) {
          return;
        }
        for (const auto &transform : bindingExpr.transforms) {
          if ((bindingInfo.kind == LocalInfo::Kind::Reference && transform.name != "Reference") ||
              (bindingInfo.kind == LocalInfo::Kind::Pointer && transform.name != "Pointer") ||
              transform.templateArgs.size() != 1) {
            continue;
          }
          const std::string targetType =
              ir_lowerer::unwrapTopLevelUninitializedTypeText(transform.templateArgs.front());
          LocalInfo::ValueKind keyKind = LocalInfo::ValueKind::Unknown;
          LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
          if (!ir_lowerer::resolveSpecializedExperimentalMapTypeKindsForBindingType(
                  targetType,
                  [&](const Expr &callExpr) { return resolveDefinitionCall(callExpr); },
                  keyKind,
                  valueKind)) {
            continue;
          }
          if (bindingInfo.kind == LocalInfo::Kind::Reference) {
            bindingInfo.referenceToMap = true;
          } else {
            bindingInfo.pointerToMap = true;
          }
          bindingInfo.mapKeyKind = keyKind;
          bindingInfo.mapValueKind = valueKind;
          bindingInfo.valueKind = valueKind;
          if (bindingInfo.structTypeName.empty()) {
            ir_lowerer::resolveSpecializedExperimentalMapStructPathForBindingType(
                targetType, bindingInfo.structTypeName);
          }
          return;
        }
      };
      setReferenceArrayInfo(bindingTypeExprRef, info);
      applySpecializedWrappedMapBindingInfo(bindingTypeExprRef, info);
      applyStructArrayInfo(bindingTypeExprRef, info);
      applyStructValueInfo(bindingTypeExprRef, info);
      for (const auto &transform : bindingTypeExprRef.transforms) {
        if ((transform.name == "Reference" || transform.name == "Pointer") && transform.templateArgs.size() == 1) {
          std::string targetType;
          if (ir_lowerer::extractTopLevelUninitializedTypeText(transform.templateArgs.front(), targetType)) {
            info.targetsUninitializedStorage = true;
            break;
          }
        }
      }
      if (!info.targetsUninitializedStorage && init.kind == Expr::Kind::Name) {
        auto existing = localsIn.find(init.name);
        if (existing != localsIn.end() &&
            (existing->second.kind == LocalInfo::Kind::Pointer ||
             existing->second.kind == LocalInfo::Kind::Reference)) {
          info.targetsUninitializedStorage = existing->second.targetsUninitializedStorage;
        }
      }
      if (!info.targetsUninitializedStorage && isPointerMemoryIntrinsicCall(init)) {
        info.targetsUninitializedStorage =
            inferPointerMemoryIntrinsicTargetsUninitializedStorage(init, localsIn);
      }
      if ((info.kind == LocalInfo::Kind::Pointer || info.kind == LocalInfo::Kind::Reference) &&
          info.structTypeName.empty()) {
        std::function<std::string(const Expr &)> inferPointerStructType = [&](const Expr &exprIn) -> std::string {
          if (exprIn.kind == Expr::Kind::Name) {
            auto localIt = localsIn.find(exprIn.name);
            if (localIt == localsIn.end()) {
              return "";
            }
            if (localIt->second.kind == LocalInfo::Kind::Pointer || localIt->second.kind == LocalInfo::Kind::Reference) {
              return localIt->second.structTypeName;
            }
            return "";
          }
          if (exprIn.kind != Expr::Kind::Call) {
            return "";
          }
          std::string memoryBuiltin;
          if (getBuiltinMemoryName(exprIn, memoryBuiltin)) {
            if (memoryBuiltin == "alloc" && exprIn.templateArgs.size() == 1) {
              const std::string targetType =
                  ir_lowerer::unwrapTopLevelUninitializedTypeText(exprIn.templateArgs.front());
              std::string resolvedStruct;
              if (resolveStructTypeName(targetType, exprIn.namespacePrefix, resolvedStruct)) {
                return resolvedStruct;
              }
              return "";
            }
            if (memoryBuiltin == "realloc" && exprIn.args.size() == 2) {
              return inferPointerStructType(exprIn.args.front());
            }
          }
          std::string builtinName;
          if (getBuiltinOperatorName(exprIn, builtinName) &&
              (builtinName == "plus" || builtinName == "minus") &&
              exprIn.args.size() == 2) {
            return inferPointerStructType(exprIn.args.front());
          }
          if (isSimpleCallName(exprIn, "location") && exprIn.args.size() == 1) {
            if (exprIn.args.front().kind != Expr::Kind::Name) {
              return "";
            }
            auto localIt = localsIn.find(exprIn.args.front().name);
            return localIt == localsIn.end() ? "" : localIt->second.structTypeName;
          }
          return "";
        };
        info.structTypeName = inferPointerStructType(init);
      }
      if (info.kind == LocalInfo::Kind::Value && info.structTypeName.empty()) {
        std::string inferredStruct = inferStructExprPath(init, localsIn);
        if (!inferredStruct.empty()) {
          info.structTypeName = inferredStruct;
        } else if (info.valueKind == LocalInfo::ValueKind::Unknown) {
          info.valueKind = LocalInfo::ValueKind::Int32;
        }
      }
      if (info.kind == LocalInfo::Kind::Value && !info.structTypeName.empty()) {
        info.valueKind = LocalInfo::ValueKind::Int64;
      }
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "File") {
          info.isFileHandle = true;
          info.valueKind = LocalInfo::ValueKind::Int64;
        } else if (transform.name == "Result") {
          info.isResult = true;
          info.resultHasValue = (transform.templateArgs.size() == 2);
          info.resultValueKind = LocalInfo::ValueKind::Unknown;
          info.resultValueCollectionKind = LocalInfo::Kind::Value;
          info.resultValueMapKeyKind = LocalInfo::ValueKind::Unknown;
          if (info.resultHasValue && !transform.templateArgs.empty()) {
            assignDeclaredResultCollection(transform.templateArgs.front());
            if (info.resultValueCollectionKind == LocalInfo::Kind::Value) {
              info.resultValueKind = valueKindFromTypeName(transform.templateArgs.front());
            }
            assignDeclaredResultFileHandle(transform.templateArgs.front());
            if (info.resultValueIsFileHandle) {
              info.resultValueKind = LocalInfo::ValueKind::Int64;
            }
            assignDeclaredResultStructType(transform.templateArgs.front());
          }
          info.valueKind = info.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
          if (!transform.templateArgs.empty()) {
            info.resultErrorType = transform.templateArgs.back();
          }
        } else if ((transform.name == "Reference" || transform.name == "Pointer") &&
                   transform.templateArgs.size() == 1) {
          const std::string originalTargetType = trimTemplateTypeText(transform.templateArgs.front());
          std::string targetType = originalTargetType;
          if (ir_lowerer::extractTopLevelUninitializedTypeText(originalTargetType, targetType)) {
            info.targetsUninitializedStorage = true;
          }
          std::string wrappedBase;
          std::string wrappedArg;
          if (splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
              normalizeCollectionBindingTypeName(wrappedBase) == "File") {
            info.isFileHandle = true;
            info.valueKind = LocalInfo::ValueKind::Int64;
          }
          bool resultHasValue = false;
          LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
          std::string resultErrorType;
          if (parseResultTypeName(targetType, resultHasValue, resultValueKind, resultErrorType)) {
            info.isResult = true;
            info.resultHasValue = resultHasValue;
            info.resultValueKind = resultValueKind;
            info.resultValueCollectionKind = LocalInfo::Kind::Value;
            info.resultValueMapKeyKind = LocalInfo::ValueKind::Unknown;
            if (info.resultHasValue) {
              std::string resultValueType;
              if (extractDeclaredResultValueType(targetType, resultValueType)) {
                assignDeclaredResultCollection(resultValueType);
                if (info.resultValueCollectionKind == LocalInfo::Kind::Value) {
                  info.resultValueKind = valueKindFromTypeName(resultValueType);
                }
                assignDeclaredResultFileHandle(resultValueType);
                if (info.resultValueIsFileHandle) {
                  info.resultValueKind = LocalInfo::ValueKind::Int64;
                }
                assignDeclaredResultStructType(resultValueType);
              }
            }
            info.valueKind = info.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
            info.resultErrorType = resultErrorType;
          }
        }
      }
      info.isFileError = isFileErrorBinding(bindingTypeExprRef);
      valueKind = info.valueKind;
