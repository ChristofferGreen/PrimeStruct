#include "SemanticsValidator.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "primec/support/CollectionHelperNames.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace primec::semantics {

namespace {

bool isCanonicalMapConstructorResolvedPath(const std::string &resolvedPath) {
  return isResolvedCanonicalKeyValueConstructorPath(resolvedPath);
}

std::vector<std::string> explicitMapConstructorTemplateArgs(const Expr &expr) {
  if (!expr.templateArgs.empty()) {
    return expr.templateArgs;
  }
  auto parseSpelledTemplateArgs = [](const std::string &name) {
    std::vector<std::string> parsed;
    const size_t anglePos = name.find('<');
    if (anglePos == std::string::npos) {
      return parsed;
    }
    const size_t endAnglePos = name.rfind('>');
    if (endAnglePos == std::string::npos || endAnglePos <= anglePos) {
      return parsed;
    }
    const std::string templateArgText =
        name.substr(anglePos + 1, endAnglePos - anglePos - 1);
    (void)splitTopLevelTemplateArgs(templateArgText, parsed);
    return parsed;
  };
  std::vector<std::string> parsed = parseSpelledTemplateArgs(expr.name);
  if (!parsed.empty()) {
    return parsed;
  }
  return parseSpelledTemplateArgs(expr.sourceName);
}

} // namespace

bool SemanticsValidator::validateExprResolvedCallArguments(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    const ExprResolvedCallArgumentContext &context,
    bool &handledOut) {
  handledOut = false;
  if (context.calleeParams == nullptr ||
      context.argumentValidationContext == nullptr ||
      context.diagnosticResolved == nullptr) {
    return true;
  }
  auto failResolvedCallArgumentDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };

  Expr reorderedCallExpr;
  Expr trimmedTypeNamespaceCallExpr;
  const std::vector<Expr> *orderedCallArgs = &expr.args;
  const std::vector<std::optional<std::string>> *orderedCallArgNames =
      &expr.argNames;
  if (this->isTypeNamespaceMethodCall(params, locals, expr, resolved)) {
    trimmedTypeNamespaceCallExpr = expr;
    trimmedTypeNamespaceCallExpr.args.erase(
        trimmedTypeNamespaceCallExpr.args.begin());
    if (!trimmedTypeNamespaceCallExpr.argNames.empty()) {
      trimmedTypeNamespaceCallExpr.argNames.erase(
          trimmedTypeNamespaceCallExpr.argNames.begin());
    }
    orderedCallArgs = &trimmedTypeNamespaceCallExpr.args;
    orderedCallArgNames = &trimmedTypeNamespaceCallExpr.argNames;
  } else if (context.hasMethodReceiverIndex &&
             context.methodReceiverIndex > 0 &&
             context.methodReceiverIndex < expr.args.size()) {
    std::string calleeKeyType;
    std::string calleeValueType;
    const bool calleeFirstParamIsMap =
        !context.calleeParams->empty() &&
        extractKeyValueCollectionTypes(context.calleeParams->front().binding,
                                       calleeKeyType, calleeValueType);
    const std::string canonicalAtUnsafe =
        metadataBackedCanonicalKeyValueHelperPath("at_unsafe");
    if (!expr.isMethodCall &&
        expr.name == "at_unsafe" &&
        calleeFirstParamIsMap &&
        !canonicalAtUnsafe.empty()) {
      return failResolvedCallArgumentDiagnostic(
          "argument type mismatch for " + canonicalAtUnsafe);
    }
    reorderedCallExpr = expr;
    std::swap(reorderedCallExpr.args[0],
              reorderedCallExpr.args[context.methodReceiverIndex]);
    if (reorderedCallExpr.argNames.size() < reorderedCallExpr.args.size()) {
      reorderedCallExpr.argNames.resize(reorderedCallExpr.args.size());
    }
    std::swap(reorderedCallExpr.argNames[0],
              reorderedCallExpr.argNames[context.methodReceiverIndex]);
    orderedCallArgs = &reorderedCallExpr.args;
    orderedCallArgNames = &reorderedCallExpr.argNames;
  }

  const auto &calleeParams = *context.calleeParams;
  if (!validateNamedArgumentsAgainstParams(calleeParams, *orderedCallArgNames,
                                           error_)) {
    if (error_.find("argument count mismatch") != std::string::npos) {
      return failResolvedCallArgumentDiagnostic("argument count mismatch for " +
                                                *context.diagnosticResolved);
    }
    return false;
  }

  std::vector<const Expr *> orderedArgs;
  std::vector<const Expr *> packedArgs;
  size_t packedParamIndex = calleeParams.size();
  std::string orderError;
  if (!buildOrderedArguments(calleeParams, *orderedCallArgs, *orderedCallArgNames,
                             orderedArgs, packedArgs, packedParamIndex,
                             orderError)) {
    if (orderError.find("argument count mismatch") != std::string::npos) {
      return failResolvedCallArgumentDiagnostic("argument count mismatch for " +
                                                *context.diagnosticResolved);
    } else {
      return failResolvedCallArgumentDiagnostic(orderError);
    }
  }

  for (const auto *arg : orderedArgs) {
    if (!arg) {
      continue;
    }
    if (!validateExpr(params, locals, *arg)) {
      return false;
    }
  }
  for (const auto *arg : packedArgs) {
    if (!arg) {
      continue;
    }
    if (!validateExpr(params, locals, *arg)) {
      return false;
    }
  }

  for (size_t paramIndex = 0; paramIndex < calleeParams.size(); ++paramIndex) {
    const ParameterInfo &param = calleeParams[paramIndex];
    if (paramIndex == packedParamIndex) {
      std::string packElementTypeText;
      if (!getArgsPackElementType(param.binding, packElementTypeText)) {
        continue;
      }
      std::string packElementTypeName = packElementTypeText;
      std::string packBase;
      std::string packArgs;
      if (splitTemplateTypeName(packElementTypeText, packBase, packArgs)) {
        packElementTypeName = packBase;
      }
      if (!this->validateArgumentsForParameter(
              param, packElementTypeName, packElementTypeText, packedArgs,
              *context.argumentValidationContext)) {
        return false;
      }
      continue;
    }
    const Expr *arg =
        paramIndex < orderedArgs.size() ? orderedArgs[paramIndex] : nullptr;
    if (arg == nullptr) {
      continue;
    }
    if (arg == param.defaultExpr) {
      continue;
    }
    const std::string &expectedTypeName = param.binding.typeName;
    const std::string expectedTypeText =
        this->expectedBindingTypeText(param.binding);
    std::string expectedExperimentalVectorElemType;
    if (context.argumentValidationContext->dispatchResolvers != nullptr &&
        this->extractCollectionVectorElementType(
            param.binding, expectedExperimentalVectorElemType)) {
      std::string actualElemType;
      std::string actualVectorSurface;
      const auto &dispatchResolvers =
          *context.argumentValidationContext->dispatchResolvers;
      if (dispatchResolvers.resolveVectorTarget != nullptr &&
          dispatchResolvers.resolveVectorTarget(*arg, actualElemType)) {
        actualVectorSurface = "vector";
      } else if (dispatchResolvers.resolveSoaVectorTarget != nullptr &&
                 dispatchResolvers.resolveSoaVectorTarget(*arg, actualElemType)) {
        actualVectorSurface = "soa";
      } else if (dispatchResolvers.resolveArrayTarget != nullptr &&
                 dispatchResolvers.resolveArrayTarget(*arg, actualElemType)) {
        actualVectorSurface = "array";
      } else {
        std::string actualTypeText;
        if (this->inferQueryExprTypeText(*arg, params, locals, actualTypeText)) {
          std::string actualBase;
          std::string actualArgText;
          if (splitTemplateTypeName(actualTypeText, actualBase, actualArgText)) {
            std::vector<std::string> actualTypeArgs;
            if (splitTopLevelTemplateArgs(actualArgText, actualTypeArgs) &&
                actualTypeArgs.size() == 1) {
              const std::string normalizedActualBase =
                  normalizeBindingTypeName(actualBase);
              if (normalizedActualBase == "vector" ||
                  normalizedActualBase == "Vector" ||
                  isLegacyExperimentalVectorCompatibilityPath(
                      "/" + normalizedActualBase)) {
                actualVectorSurface = "vector";
                actualElemType = actualTypeArgs.front();
              } else if (normalizedActualBase == "soa") {
                actualVectorSurface = "soa";
                actualElemType = actualTypeArgs.front();
              } else if (normalizedActualBase == "array") {
                actualVectorSurface = "array";
                actualElemType = actualTypeArgs.front();
              }
            }
          }
        }
      }
      if (!actualVectorSurface.empty() &&
          (actualVectorSurface != "vector" ||
           normalizeBindingTypeName(expectedExperimentalVectorElemType) !=
               normalizeBindingTypeName(actualElemType))) {
        return failResolvedCallArgumentDiagnostic(
            "argument type mismatch for " + *context.diagnosticResolved +
            " parameter " + param.name + ": expected " + expectedTypeText +
            " got " + actualVectorSurface + "<" + actualElemType + ">");
      }
    }
    if (!this->validateArgumentTypeAgainstParam(
            *arg, param, expectedTypeName, expectedTypeText,
            *context.argumentValidationContext)) {
      return false;
    }
  }

  auto isReferenceTypeText = [](const std::string &typeName,
                                const std::string &typeText) {
    if (normalizeBindingTypeName(typeName) == "Reference") {
      return true;
    }
    std::string base;
    std::string argText;
    return splitTemplateTypeName(typeText, base, argText) &&
           normalizeBindingTypeName(base) == "Reference";
  };
  auto isStandaloneSoaRefCall = [&](const Expr &arg,
                                    const Expr *&receiverOut) -> bool {
    receiverOut = nullptr;
    if (arg.kind != Expr::Kind::Call || arg.args.size() != 2) {
      return false;
    }
    if (arg.isMethodCall) {
      if (!collection_helpers::isRefHelperName(arg.name)) {
        return false;
      }
      receiverOut = &arg.args.front();
      return true;
    }
    const std::string resolvedPath = resolveCalleePath(arg);
    const std::string resolvedPathCanonical =
        canonicalizeLegacySoaRefHelperPath(resolvedPath);
    const bool matchesCanonicalSoaRefHelperPath =
        isCanonicalSoaRefLikeHelperPath(resolvedPathCanonical);
    const bool matchesExperimentalSoaRefHelperPath =
        isExperimentalSoaRefLikeHelperPath(resolvedPathCanonical);
    if (!isSimpleCallName(arg, "ref") &&
        !isSimpleCallName(arg, collection_helpers::kRefRef) &&
        !matchesCanonicalSoaRefHelperPath &&
        !matchesExperimentalSoaRefHelperPath) {
      return false;
    }
    receiverOut = &arg.args.front();
    return true;
  };
  auto isSoaFieldViewTypeText = [&](const std::string &typeText) -> bool {
    return isSoaFieldViewTypePath(typeText);
  };
  auto isStandaloneSoaFieldViewCall = [&](const Expr &arg,
                                          const Expr *&receiverOut) -> bool {
    receiverOut = nullptr;
    std::string fieldName;
    if (!isBuiltinSoaFieldViewExpr(arg, params, locals, &fieldName)) {
      return false;
    }
    if (arg.kind != Expr::Kind::Call || arg.args.empty()) {
      return false;
    }
    receiverOut = &arg.args.front();
    return true;
  };
  auto isReferenceEscapeCandidate = [&](const Expr &arg,
                                        const ParameterInfo &param) -> bool {
    const std::string expectedTypeText =
        param.binding.typeTemplateArg.empty()
            ? param.binding.typeName
            : param.binding.typeName + "<" + param.binding.typeTemplateArg + ">";
    if (!isReferenceTypeText(param.binding.typeName, expectedTypeText)) {
      return false;
    }
    const Expr *receiverExpr = nullptr;
    if (!isStandaloneSoaRefCall(arg, receiverExpr)) {
      return false;
    }
    if (receiverExpr == nullptr) {
      return false;
    }
    return receiverExpr->kind != Expr::Kind::Name;
  };
  auto checkStandaloneSoaRefEscapes = [&](const Expr &arg,
                                          const ParameterInfo &param) -> bool {
    if (const auto pendingPath =
            builtinSoaDirectPendingHelperPath(arg, params, locals)) {
      if (pendingPath->find(collection_helpers::kCanonicalSoaRef) == 0 ||
          pendingPath->find(collection_helpers::kCanonicalSoaRef) == 0) {
        return failResolvedCallArgumentDiagnostic(
            soaUnavailableMethodDiagnostic(*pendingPath));
      }
    }
    if (!isReferenceEscapeCandidate(arg, param)) {
      return true;
    }
    return failResolvedCallArgumentDiagnostic(
        "reference escapes via argument to " + resolved);
  };
  auto checkStandaloneSoaFieldViewEscapes = [&](const Expr &arg,
                                                const ParameterInfo &param) -> bool {
    if (const auto pendingPath =
            builtinSoaDirectPendingHelperPath(arg, params, locals)) {
      std::string pendingFieldName;
      if (splitSoaFieldViewHelperPath(*pendingPath, &pendingFieldName)) {
        return failResolvedCallArgumentDiagnostic(
            "field-view escapes via argument to " + resolved);
      }
    }
    const std::string expectedTypeText =
        param.binding.typeTemplateArg.empty()
            ? param.binding.typeName
            : param.binding.typeName + "<" + param.binding.typeTemplateArg + ">";
    if (!isSoaFieldViewTypeText(expectedTypeText)) {
      return true;
    }
    const Expr *receiverExpr = nullptr;
    if (!isStandaloneSoaFieldViewCall(arg, receiverExpr)) {
      return true;
    }
    if (receiverExpr == nullptr) {
      return true;
    }
    return failResolvedCallArgumentDiagnostic(
        "field-view escapes via argument to " + resolved);
  };

  for (size_t paramIndex = 0; paramIndex < calleeParams.size(); ++paramIndex) {
    if (paramIndex == packedParamIndex) {
      for (const Expr *arg : packedArgs) {
        if (arg != nullptr &&
            !checkStandaloneSoaRefEscapes(*arg, calleeParams[paramIndex])) {
          return false;
        }
        if (arg != nullptr &&
            !checkStandaloneSoaFieldViewEscapes(*arg, calleeParams[paramIndex])) {
          return false;
        }
      }
      continue;
    }
    const Expr *arg =
        paramIndex < orderedArgs.size() ? orderedArgs[paramIndex] : nullptr;
    if (arg == nullptr) {
      continue;
    }
    if (!checkStandaloneSoaRefEscapes(*arg, calleeParams[paramIndex])) {
      return false;
    }
    if (!checkStandaloneSoaFieldViewEscapes(*arg, calleeParams[paramIndex])) {
      return false;
    }
  }

  // Parameter modes (docs/spec/value-lifecycle.md, Parameter Passing): a `mut` parameter borrows its
  // argument mutably, so the argument must be a mutable place, and that place cannot also be passed
  // to another parameter of the same call.
  auto isMutableBorrowParam = [&](const ParameterInfo &param) {
    if (!param.binding.isMutable || param.binding.isCopy || param.binding.isMove) {
      return false;
    }
    // References, pointers and capability views (`Slice<T, ReadWrite>`) carry their own write access.
    const std::string typeName = normalizeBindingTypeName(param.binding.typeName);
    return typeName != "Reference" && typeName != "Pointer" &&
           param.binding.typeCapabilityArg.empty();
  };
  auto bindingForName = [&](const std::string &name) -> const BindingInfo * {
    if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
      return paramBinding;
    }
    const auto it = locals.find(name);
    return it == locals.end() ? nullptr : &it->second;
  };
  // A `move` parameter takes ownership: a named argument of an owning type is moved-from after the
  // call (values of other types are copied), and a borrowed parameter cannot be handed over.
  for (size_t paramIndex = 0; paramIndex < calleeParams.size(); ++paramIndex) {
    const ParameterInfo &param = calleeParams[paramIndex];
    const Expr *arg = paramIndex < orderedArgs.size() ? orderedArgs[paramIndex] : nullptr;
    if (paramIndex == packedParamIndex || arg == nullptr || arg == param.defaultExpr ||
        !param.binding.isMove || arg->kind != Expr::Kind::Name ||
        currentValidationState_.context.definitionIsUnsafe) {
      continue;
    }
    if (isOwningBorrowedParameter(params, *arg, expr.namespacePrefix)) {
      return failResolvedCallArgumentDiagnostic("borrowed parameter cannot be moved: " + arg->name);
    }
    const BindingInfo *binding = bindingForName(arg->name);
    if (binding == nullptr || !bindingOwnsResources(*binding, expr.namespacePrefix)) {
      continue;
    }
    for (size_t otherIndex = 0; otherIndex < orderedArgs.size(); ++otherIndex) {
      const Expr *other = orderedArgs[otherIndex];
      if (otherIndex != paramIndex && other != nullptr && other->kind == Expr::Kind::Name &&
          other->name == arg->name) {
        return failResolvedCallArgumentDiagnostic("borrow conflict: " + arg->name + " (root: " +
                                                  arg->name + ", sink: " + param.name + ")");
      }
    }
    auto &moveSites = currentValidationState_.moveArgumentSites;
    if (currentValidationState_.movedBindings.count(arg->name) > 0) {
      const auto site = moveSites.find(arg->name);
      if (site != moveSites.end() && site->second.matches(*arg)) {
        continue;
      }
      return failResolvedCallArgumentDiagnostic("use-after-move: " + arg->name);
    }
    currentValidationState_.movedBindings.insert(arg->name);
    moveSites[arg->name] = {arg, arg->sourceLine, arg->sourceColumn};
  }

  for (size_t paramIndex = 0; paramIndex < calleeParams.size(); ++paramIndex) {
    const ParameterInfo &param = calleeParams[paramIndex];
    const Expr *arg = paramIndex < orderedArgs.size() ? orderedArgs[paramIndex] : nullptr;
    if (paramIndex == packedParamIndex || arg == nullptr || arg == param.defaultExpr ||
        !isMutableBorrowParam(param)) {
      continue;
    }
    if (arg->kind != Expr::Kind::Name) {
      continue; // literals and other temporaries, fields and elements
    }
    const BindingInfo *binding = bindingForName(arg->name);
    if (binding == nullptr) {
      continue;
    }
    const std::string typeName = normalizeBindingTypeName(binding->typeName);
    if (!binding->isMutable && typeName != "Reference" && typeName != "Pointer") {
      return failResolvedCallArgumentDiagnostic("mut parameter requires a mutable place: " +
                                                arg->name);
    }
    for (size_t otherIndex = 0; otherIndex < orderedArgs.size(); ++otherIndex) {
      const Expr *other = orderedArgs[otherIndex];
      if (otherIndex == paramIndex || other == nullptr || other->kind != Expr::Kind::Name ||
          other->name != arg->name) {
        continue;
      }
      return failResolvedCallArgumentDiagnostic(
          "borrow conflict: " + arg->name + " (root: " + arg->name + ", sink: " + param.name + ")");
    }
  }

  bool calleeIsUnsafe = false;
  if (context.resolvedDefinition != nullptr) {
    for (const auto &transform : context.resolvedDefinition->transforms) {
      if (transform.name == "unsafe_api" && !currentValidationState_.context.definitionIsUnsafe) {
        // An `[unsafe_api]` definition can break memory safety when misused; only unsafe code may
        // call it (docs/spec/type-system.md, Ownership and Mutability).
        return failResolvedCallArgumentDiagnostic("calling " + resolved +
                                                  " requires an unsafe definition");
      }
      if (transform.name == "unsafe" || transform.name == "unsafe_api") {
        calleeIsUnsafe = true;
        break;
      }
    }
  }

  if (currentValidationState_.context.definitionIsUnsafe && !calleeIsUnsafe) {
    for (size_t i = 0; i < calleeParams.size(); ++i) {
      const ParameterInfo &param = calleeParams[i];
      if (i == packedParamIndex) {
        std::string packElementTypeText;
        if (!getArgsPackElementType(param.binding, packElementTypeText)) {
          continue;
        }
        std::string packElementTypeName = packElementTypeText;
        std::string packBase;
        std::string packArgs;
        if (splitTemplateTypeName(packElementTypeText, packBase, packArgs)) {
          packElementTypeName = packBase;
        }
        if (!isReferenceTypeText(packElementTypeName, packElementTypeText)) {
          continue;
        }
        for (const Expr *arg : packedArgs) {
          if (!arg || !isUnsafeReferenceExpr(params, locals, *arg)) {
            continue;
          }
          return failResolvedCallArgumentDiagnostic(
              "unsafe reference escapes across safe boundary to " + resolved);
        }
        continue;
      }

      const Expr *arg = i < orderedArgs.size() ? orderedArgs[i] : nullptr;
      if (arg == nullptr) {
        continue;
      }
      const std::string expectedTypeText =
          param.binding.typeTemplateArg.empty()
              ? param.binding.typeName
              : param.binding.typeName + "<" + param.binding.typeTemplateArg +
                    ">";
      if (!isReferenceTypeText(param.binding.typeName, expectedTypeText)) {
        continue;
      }
      if (!isUnsafeReferenceExpr(params, locals, *arg)) {
        continue;
      }
      return failResolvedCallArgumentDiagnostic(
          "unsafe reference escapes across safe boundary to " + resolved);
    }
  }

  auto keyValueConstructorArgumentMatchesExactType =
      [&](const Expr &arg, const std::string &expectedTypeText,
          std::string &actualTypeTextOut) -> bool {
    actualTypeTextOut.clear();
    const std::string normalizedExpected =
        normalizeBindingTypeName(expectedTypeText);
    if (normalizedExpected.empty()) {
      return true;
    }

    if (context.argumentValidationContext->dispatchResolvers != nullptr &&
        isStringExprForArgumentValidation(
            arg, *context.argumentValidationContext->dispatchResolvers)) {
      actualTypeTextOut = "string";
      return normalizedExpected == "string";
    }

    const ReturnKind expectedKind = returnKindForTypeName(normalizedExpected);
    if (expectedKind != ReturnKind::Unknown) {
      const ReturnKind actualKind = inferExprReturnKind(arg, params, locals);
      if (actualKind != ReturnKind::Unknown) {
        actualTypeTextOut = typeNameForReturnKind(actualKind);
        return actualKind == expectedKind;
      }
    }

    std::string inferredTypeText;
    if (inferQueryExprTypeText(arg, params, locals, inferredTypeText) &&
        !inferredTypeText.empty()) {
      actualTypeTextOut = inferredTypeText;
      return normalizeBindingTypeName(inferredTypeText) == normalizedExpected;
    }

    const std::string actualStructPath =
        inferStructReturnPath(arg, params, locals);
    if (!actualStructPath.empty()) {
      actualTypeTextOut = actualStructPath;
      const std::string expectedStructPath = resolveStructTypePath(
          expectedTypeText, expr.namespacePrefix, structNames_);
      if (!expectedStructPath.empty()) {
        return actualStructPath == expectedStructPath;
      }
      return normalizeBindingTypeName(actualStructPath) == normalizedExpected;
    }

    return true;
  };

  auto validateExplicitCanonicalKeyValueConstructorArguments = [&]() -> bool {
    if (context.resolvedDefinition == nullptr ||
        !isCanonicalMapConstructorResolvedPath(resolved) ||
        orderedArgs.empty() ||
        !packedArgs.empty() || orderedArgs.size() != expr.args.size() ||
        orderedArgs.size() % 2 != 0) {
      return true;
    }

    std::vector<std::string> effectiveTemplateArgs =
        explicitMapConstructorTemplateArgs(expr);
    if (effectiveTemplateArgs.size() != 2) {
      return true;
    }

    for (size_t argIndex = 0; argIndex < orderedArgs.size(); ++argIndex) {
      const Expr *arg = orderedArgs[argIndex];
      if (arg == nullptr || arg->isSpread) {
        continue;
      }
      const std::string &expectedTypeText = effectiveTemplateArgs[argIndex % 2];
      std::string actualTypeText;
      if (keyValueConstructorArgumentMatchesExactType(*arg, expectedTypeText,
                                                      actualTypeText)) {
        continue;
      }

      const std::string paramName =
          argIndex < calleeParams.size() && !calleeParams[argIndex].name.empty()
              ? calleeParams[argIndex].name
              : (argIndex % 2 == 0 ? "key" : "value");
      std::string message = "argument type mismatch for " +
                            *context.diagnosticResolved + " parameter " +
                            paramName + ": expected " + expectedTypeText;
      if (!actualTypeText.empty()) {
        message += " got " + actualTypeText;
      }
      return failResolvedCallArgumentDiagnostic(std::move(message));
    }

    return true;
  };

  if (!validateExplicitCanonicalKeyValueConstructorArguments()) {
    return false;
  }

  handledOut = true;
  return true;
}

} // namespace primec::semantics
