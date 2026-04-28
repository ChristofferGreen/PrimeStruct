#include "SemanticsValidator.h"

#include <string_view>

namespace primec::semantics {

namespace {

bool isBuiltinCollectionHelperName(std::string_view helperName) {
  return helperName == "count" || helperName == "capacity" || helperName == "contains" ||
         helperName == "count_ref" || helperName == "contains_ref" ||
         helperName == "tryAt" || helperName == "tryAt_ref" || helperName == "at" ||
         helperName == "at_ref" || helperName == "at_unsafe" ||
         helperName == "at_unsafe_ref" || helperName == "insert" ||
         helperName == "get" || helperName == "get_ref" ||
         helperName == "ref" || helperName == "ref_ref" ||
         helperName == "insert_ref" || helperName == "push" || helperName == "pop" ||
         helperName == "reserve" || helperName == "clear" || helperName == "remove_at" ||
         helperName == "remove_swap" || helperName == "to_soa" ||
         helperName == "to_aos" || helperName == "to_aos_ref";
}

bool isFlowEffectDiagnosticMessage(const std::string &message) {
  return message.rfind("execution effects must be a subset of enclosing effects on ", 0) == 0 ||
         message.rfind("capability requires matching effect on ", 0) == 0 ||
         message.rfind("duplicate effects transform on ", 0) == 0 ||
         message.rfind("duplicate capabilities transform on ", 0) == 0;
}

bool isLoopBlockEnvelope(const Expr &candidate) {
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
}

} // namespace

void SemanticsValidator::collectDefinitionIntraBodyCallDiagnostics(
    const Definition &def,
    std::vector<SemanticDiagnosticRecord> &out) {
  const auto &definitionParams = paramsByDef_[def.fullPath];
  std::unordered_map<std::string, BindingInfo> definitionLocals;
  definitionLocals.reserve(definitionParams.size());
  for (const auto &param : definitionParams) {
    definitionLocals.emplace(param.name, param.binding);
  }

  auto appendDefinitionRecord = [&](const Expr &expr, std::string message) {
    SemanticDiagnosticRecord record;
    record.message = std::move(message);
    if (expr.sourceLine > 0 && expr.sourceColumn > 0) {
      record.primarySpan.line = expr.sourceLine;
      record.primarySpan.column = expr.sourceColumn;
      record.primarySpan.endLine = expr.sourceLine;
      record.primarySpan.endColumn = expr.sourceColumn;
      record.hasPrimarySpan = true;
    }
    if (def.sourceLine > 0 && def.sourceColumn > 0) {
      SemanticDiagnosticRelatedSpan related;
      related.span.line = def.sourceLine;
      related.span.column = def.sourceColumn;
      related.span.endLine = def.sourceLine;
      related.span.endColumn = def.sourceColumn;
      related.label = "definition: " + def.fullPath;
      record.relatedSpans.push_back(std::move(related));
    }
    out.push_back(std::move(record));
  };

  auto isBuiltinCall = [&](const Expr &expr) -> bool {
    if (expr.isMethodCall) {
      return false;
    }
    if (isRootBuiltinName(expr.name)) {
      return true;
    }
    if (isIfCall(expr) || isMatchCall(expr) || isPickCall(expr) || isLoopCall(expr) || isWhileCall(expr) || isForCall(expr) ||
        isRepeatCall(expr) || isReturnCall(expr) || isBlockCall(expr) ||
        isLoopBlockEnvelope(expr)) {
      return true;
    }
    const std::string resolved = resolveCalleePath(expr);
    std::string builtinName;
    std::string namespacedCollection;
    std::string namespacedHelper;
    const bool isCollectionHelperBuiltin =
        isSimpleCallName(expr, "count") || isSimpleCallName(expr, "capacity") ||
        isSimpleCallName(expr, "count_ref") ||
        (!expr.isMethodCall && isSimpleCallName(expr, "contains")) ||
        (!expr.isMethodCall && isSimpleCallName(expr, "contains_ref")) ||
        isSimpleCallName(expr, "tryAt") || isSimpleCallName(expr, "tryAt_ref") ||
        isSimpleCallName(expr, "at") || isSimpleCallName(expr, "at_ref") ||
        isSimpleCallName(expr, "at_unsafe") || isSimpleCallName(expr, "at_unsafe_ref") ||
        isSimpleCallName(expr, "get") || isSimpleCallName(expr, "get_ref") ||
        isSimpleCallName(expr, "ref") || isSimpleCallName(expr, "ref_ref") ||
        isSimpleCallName(expr, "insert") || isSimpleCallName(expr, "insert_ref") ||
        isSimpleCallName(expr, "push") || isSimpleCallName(expr, "pop") ||
        isSimpleCallName(expr, "reserve") || isSimpleCallName(expr, "clear") ||
        isSimpleCallName(expr, "remove_at") || isSimpleCallName(expr, "remove_swap") ||
        isSimpleCallName(expr, "to_soa") || isSimpleCallName(expr, "to_aos") ||
        isSimpleCallName(expr, "to_aos_ref") ||
        (getNamespacedCollectionHelperName(expr, namespacedCollection, namespacedHelper) &&
         isBuiltinCollectionHelperName(namespacedHelper));
    const bool isCollectionBuiltin = defMap_.count(resolved) == 0 && getBuiltinCollectionName(expr, builtinName);
    const bool isDirectFileErrorBuiltin = resolved == "/file_error/why";
    return getBuiltinOperatorName(expr, builtinName) || getBuiltinComparisonName(expr, builtinName) ||
           getBuiltinMutationName(expr, builtinName) ||
           getBuiltinClampName(expr, builtinName, allowMathBareName(expr.name)) ||
           getBuiltinMinMaxName(expr, builtinName, allowMathBareName(expr.name)) ||
           getBuiltinAbsSignName(expr, builtinName, allowMathBareName(expr.name)) ||
           getBuiltinSaturateName(expr, builtinName, allowMathBareName(expr.name)) ||
           getBuiltinMathName(expr, builtinName, allowMathBareName(expr.name)) ||
           getBuiltinGpuName(expr, builtinName) || getBuiltinConvertName(expr, builtinName) ||
           getBuiltinArrayAccessName(expr, builtinName) || getBuiltinPointerName(expr, builtinName) ||
           (!expr.isMethodCall && isSimpleCallName(expr, "contains")) ||
           getBuiltinMemoryName(expr, builtinName) ||
           isCollectionBuiltin || isCollectionHelperBuiltin || isDirectFileErrorBuiltin;
  };

  auto describeReflectionCallDiagnostic = [&](const Expr &expr, std::string &messageOut) -> bool {
    messageOut.clear();
    auto isUnboundMetaReceiver = [&](const Expr &receiver) {
      if (receiver.kind != Expr::Kind::Name || receiver.name != "meta") {
        return false;
      }
      if (findParamBinding(definitionParams, receiver.name) != nullptr) {
        return false;
      }
      return definitionLocals.find(receiver.name) == definitionLocals.end();
    };
    if (expr.isMethodCall && !expr.args.empty()) {
      const Expr &receiver = expr.args.front();
      if (isUnboundMetaReceiver(receiver)) {
        if (isReflectionMetadataQueryName(expr.name)) {
          messageOut = "reflection metadata queries are compile-time only and not yet implemented: meta." + expr.name;
          return true;
        }
        if (expr.name == "object" || expr.name == "table") {
          messageOut = "runtime reflection objects/tables are unsupported: meta." + expr.name;
          return true;
        }
        messageOut = "unsupported reflection metadata query: meta." + expr.name;
        return true;
      }
    }
    const std::string resolved = resolveCalleePath(expr);
    if (defMap_.count(resolved) > 0) {
      return false;
    }
    if (isReflectionMetadataQueryPath(resolved)) {
      messageOut = "reflection metadata queries are compile-time only and not yet implemented: " + resolved;
      return true;
    }
    if (isRuntimeReflectionPath(resolved)) {
      messageOut = "runtime reflection objects/tables are unsupported: " + resolved;
      return true;
    }
    if (resolved.rfind("/meta/", 0) == 0) {
      const std::string queryName = resolved.substr(6);
      if (!queryName.empty() && queryName.find('/') == std::string::npos) {
        messageOut = "unsupported reflection metadata query: " + resolved;
        return true;
      }
    }
    return false;
  };

  auto collectResolvedCallArgumentDiagnostic = [&](const Expr &expr, const std::string &resolved) -> bool {
    const std::string diagnosticResolved = diagnosticCallTargetPath(resolved);
    const BuiltinCollectionDispatchResolverAdapters dispatchResolverAdapters;
    const BuiltinCollectionDispatchResolvers dispatchResolvers =
        makeBuiltinCollectionDispatchResolvers(definitionParams, definitionLocals, dispatchResolverAdapters);
    auto appendArgumentTypeMismatch = [&](const std::string &paramName,
                                          const std::string &expectedType,
                                          const std::string &actualType) {
      appendDefinitionRecord(expr,
                             "argument type mismatch for " + diagnosticResolved + " parameter " + paramName +
                                 ": expected " + expectedType + " got " + actualType);
    };
    auto isCompatibleExperimentalVectorReceiver = [&](const Expr &arg,
                                                      const ParameterInfo &param,
                                                      const std::string &expectedStructPath) {
      std::string expectedElemType;
      std::string actualElemType;
      if (!(extractExperimentalVectorElementType(param.binding, expectedElemType) ||
            extractExperimentalVectorElementTypeFromStructPath(expectedStructPath, expectedElemType))) {
        return false;
      }
      if (dispatchResolvers.resolveVectorTarget != nullptr &&
          dispatchResolvers.resolveVectorTarget(arg, actualElemType) &&
          normalizeBindingTypeName(expectedElemType) == normalizeBindingTypeName(actualElemType)) {
        return true;
      }
      std::vector<std::string> resolvedVectorArgs;
      if (resolveCallCollectionTemplateArgs(arg,
                                            "vector",
                                            definitionParams,
                                            definitionLocals,
                                            resolvedVectorArgs) &&
          resolvedVectorArgs.size() == 1 &&
          normalizeBindingTypeName(expectedElemType) ==
              normalizeBindingTypeName(resolvedVectorArgs.front())) {
        return true;
      }
      std::string inferredTypeText;
      if (!inferQueryExprTypeText(arg, definitionParams, definitionLocals, inferredTypeText) ||
          inferredTypeText.empty()) {
        return false;
      }
      std::string inferredBase;
      std::string inferredArgText;
      if (!splitTemplateTypeName(inferredTypeText, inferredBase, inferredArgText)) {
        return false;
      }
      std::vector<std::string> inferredArgs;
      if (!splitTopLevelTemplateArgs(inferredArgText, inferredArgs) || inferredArgs.size() != 1) {
        return false;
      }
      const std::string normalizedInferredBase = normalizeBindingTypeName(inferredBase);
      return (normalizedInferredBase == "vector" ||
              normalizedInferredBase == "Vector" ||
              normalizedInferredBase == "std/collections/experimental_vector/Vector") &&
             normalizeBindingTypeName(expectedElemType) == normalizeBindingTypeName(inferredArgs.front());
    };
    std::string message;
    if (!validateNamedArguments(expr.args, expr.argNames, diagnosticResolved, message)) {
      appendDefinitionRecord(expr, std::move(message));
      return true;
    }
    if (structNames_.count(resolved) > 0) {
      return false;
    }
    auto paramsIt = paramsByDef_.find(resolved);
    if (paramsIt == paramsByDef_.end()) {
      return false;
    }
    const auto &calleeParams = paramsIt->second;
    if (!validateNamedArgumentsAgainstParams(calleeParams, expr.argNames, message)) {
      appendDefinitionRecord(expr, std::move(message));
      return true;
    }
    std::vector<const Expr *> orderedArgs;
    std::vector<const Expr *> packedArgs;
    size_t packedParamIndex = calleeParams.size();
    std::string orderError;
    if (!buildOrderedArguments(calleeParams, expr.args, expr.argNames, orderedArgs, packedArgs, packedParamIndex,
                               orderError)) {
      if (orderError.find("argument count mismatch") != std::string::npos) {
        message = "argument count mismatch for " + diagnosticResolved;
      } else {
        message = std::move(orderError);
      }
      appendDefinitionRecord(expr, std::move(message));
      return true;
    }
    std::unordered_set<const Expr *> explicitArgs;
    explicitArgs.reserve(expr.args.size());
    for (const auto &arg : expr.args) {
      explicitArgs.insert(&arg);
    }

    auto validateArgumentTypeMismatch = [&](const Expr &arg,
                                           const ParameterInfo &param,
                                           const std::string &expectedTypeName) {
      if (expectedTypeName.empty() || expectedTypeName == "auto") {
        return;
      }
      ReturnKind expectedKind = returnKindForTypeName(expectedTypeName);
      if (expectedKind != ReturnKind::Unknown) {
        ReturnKind actualKind = inferExprReturnKind(arg, definitionParams, definitionLocals);
        if (actualKind == ReturnKind::Unknown || actualKind == ReturnKind::Array || actualKind == expectedKind) {
          return;
        }
        const std::string expectedName = typeNameForReturnKind(expectedKind);
        const std::string actualName = typeNameForReturnKind(actualKind);
        if (expectedName.empty() || actualName.empty()) {
          return;
        }
        appendArgumentTypeMismatch(param.name, expectedName, actualName);
        return;
      }
      const std::string expectedStructPath =
          resolveStructTypePath(expectedTypeName, expr.namespacePrefix, structNames_);
      if (expectedStructPath.empty()) {
        return;
      }
      const std::string actualStructPath = inferStructReturnPath(arg, definitionParams, definitionLocals);
      if (!actualStructPath.empty()) {
        if (actualStructPath != expectedStructPath) {
          if (actualStructPath == "/vector" &&
              expectedStructPath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0) {
            return;
          }
          if (isCompatibleExperimentalVectorReceiver(arg, param, expectedStructPath)) {
            return;
          }
          appendArgumentTypeMismatch(param.name, expectedStructPath, actualStructPath);
        }
        return;
      }
      ReturnKind actualKind = inferExprReturnKind(arg, definitionParams, definitionLocals);
      if (actualKind == ReturnKind::Unknown || actualKind == ReturnKind::Array) {
        return;
      }
      const std::string actualName = typeNameForReturnKind(actualKind);
      if (actualName.empty()) {
        return;
      }
      appendArgumentTypeMismatch(param.name, expectedStructPath, actualName);
    };

    auto validateSpreadArgumentTypeMismatch = [&](const Expr &arg,
                                                  const ParameterInfo &param,
                                                  const std::string &expectedTypeName) {
      std::string actualElementType;
      if (!resolveArgsPackElementTypeForExpr(arg, definitionParams, definitionLocals, actualElementType)) {
        appendDefinitionRecord(arg, "spread argument requires args<T> value");
        return;
      }
      ReturnKind expectedKind = returnKindForTypeName(expectedTypeName);
      if (expectedKind != ReturnKind::Unknown) {
        ReturnKind actualKind = returnKindForTypeName(actualElementType);
        const bool softwareNumericCompatible =
            (expectedKind == ReturnKind::Integer &&
             (actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 ||
              actualKind == ReturnKind::UInt64 || actualKind == ReturnKind::Bool ||
              actualKind == ReturnKind::Integer)) ||
            (expectedKind == ReturnKind::Decimal &&
             (actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 ||
              actualKind == ReturnKind::UInt64 || actualKind == ReturnKind::Bool ||
              actualKind == ReturnKind::Float32 || actualKind == ReturnKind::Float64 ||
              actualKind == ReturnKind::Integer || actualKind == ReturnKind::Decimal)) ||
            (expectedKind == ReturnKind::Complex &&
             (actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 ||
              actualKind == ReturnKind::UInt64 || actualKind == ReturnKind::Bool ||
              actualKind == ReturnKind::Float32 || actualKind == ReturnKind::Float64 ||
              actualKind == ReturnKind::Integer || actualKind == ReturnKind::Decimal ||
              actualKind == ReturnKind::Complex));
        if (actualKind == ReturnKind::Unknown || actualKind == ReturnKind::Array || actualKind == expectedKind ||
            softwareNumericCompatible) {
          return;
        }
        const std::string expectedName = typeNameForReturnKind(expectedKind);
        const std::string actualName = typeNameForReturnKind(actualKind);
        if (expectedName.empty() || actualName.empty()) {
          return;
        }
        appendArgumentTypeMismatch(param.name, expectedName, actualName);
        return;
      }
      const std::string expectedStructPath =
          resolveStructTypePath(expectedTypeName, expr.namespacePrefix, structNames_);
      if (expectedStructPath.empty()) {
        if (normalizeBindingTypeName(actualElementType) != normalizeBindingTypeName(expectedTypeName)) {
          appendArgumentTypeMismatch(param.name, expectedTypeName, normalizeBindingTypeName(actualElementType));
        }
        return;
      }
      const std::string actualStructPath =
          resolveStructTypePath(actualElementType, expr.namespacePrefix, structNames_);
      if (!actualStructPath.empty()) {
        if (actualStructPath != expectedStructPath) {
          appendArgumentTypeMismatch(param.name, expectedStructPath, actualStructPath);
        }
        return;
      }
      appendArgumentTypeMismatch(param.name, expectedStructPath, normalizeBindingTypeName(actualElementType));
    };

    for (size_t paramIndex = 0; paramIndex < calleeParams.size(); ++paramIndex) {
      const ParameterInfo &param = calleeParams[paramIndex];
      if (paramIndex == packedParamIndex) {
        std::string packElementType;
        if (!getArgsPackElementType(param.binding, packElementType)) {
          continue;
        }
        std::string packElementBase = packElementType;
        std::string packElementArgs;
        if (splitTemplateTypeName(packElementType, packElementBase, packElementArgs)) {
          packElementType = packElementBase;
        }
        for (const Expr *arg : packedArgs) {
          if (arg == nullptr || explicitArgs.count(arg) == 0) {
            continue;
          }
          if (arg->isSpread) {
            validateSpreadArgumentTypeMismatch(*arg, param, packElementType);
            continue;
          }
          validateArgumentTypeMismatch(*arg, param, packElementType);
        }
        continue;
      }
      const Expr *arg = paramIndex < orderedArgs.size() ? orderedArgs[paramIndex] : nullptr;
      if (arg == nullptr || explicitArgs.count(arg) == 0) {
        continue;
      }
      validateArgumentTypeMismatch(*arg, param, param.binding.typeName);
    }
    return false;
  };

  std::function<void(const Expr &)> scanExpr;
  scanExpr = [&](const Expr &expr) {
    if (expr.kind == Expr::Kind::Call) {
      std::optional<EffectScope> effectScope;
      if (!expr.isBinding && !expr.transforms.empty()) {
        std::unordered_set<std::string> executionEffects;
        if (!resolveExecutionEffects(expr, executionEffects)) {
          const std::string effectError = error_;
          error_.clear();
          if (isFlowEffectDiagnosticMessage(effectError)) {
            appendDefinitionRecord(expr, effectError);
          }
          return;
        }
        effectScope.emplace(*this, std::move(executionEffects));
      }
      if (!expr.isBinding && !expr.isLambda && !expr.name.empty() && !expr.isMethodCall && !expr.isFieldAccess &&
          !isBuiltinCall(expr)) {
        std::string reflectionDiagnostic;
        if (describeReflectionCallDiagnostic(expr, reflectionDiagnostic)) {
          appendDefinitionRecord(expr, std::move(reflectionDiagnostic));
          return;
        }
        const std::string resolved = resolveCalleePath(expr);
        if (!hasDefinitionPath(resolved)) {
          if (resolved.rfind("/std/collections/vector/count", 0) == 0 &&
              expr.args.size() != 1) {
            appendDefinitionRecord(
                expr,
                hasNamedArguments(expr.argNames)
                    ? "named arguments not supported for builtin calls"
                    : "argument count mismatch for builtin count");
            return;
          }
          if (resolved.rfind("/std/collections/vector/capacity", 0) == 0 &&
              expr.args.size() != 1) {
            appendDefinitionRecord(
                expr,
                hasNamedArguments(expr.argNames)
                    ? "named arguments not supported for builtin calls"
                    : "argument count mismatch for builtin capacity");
            return;
          }
          appendDefinitionRecord(expr, "unknown call target: " + formatUnknownCallTarget(expr));
        } else {
          collectResolvedCallArgumentDiagnostic(expr, resolved);
        }
      }
    }
    for (const auto &arg : expr.args) {
      scanExpr(arg);
    }
    for (const auto &arg : expr.bodyArguments) {
      scanExpr(arg);
    }
  };

  for (const auto &stmt : def.statements) {
    scanExpr(stmt);
  }
  if (def.returnExpr.has_value()) {
    scanExpr(*def.returnExpr);
  }
}

} // namespace primec::semantics
