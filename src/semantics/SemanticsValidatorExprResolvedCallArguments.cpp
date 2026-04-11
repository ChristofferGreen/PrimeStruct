#include "SemanticsValidator.h"
#include "MapConstructorHelpers.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace primec::semantics {

namespace {

bool isCanonicalMapConstructorResolvedPath(const std::string &resolvedPath) {
  const std::string normalizedPath = stripMapConstructorSuffixes(resolvedPath);
  return normalizedPath == "/std/collections/map/map" ||
         normalizedPath == "/std/collections/mapNew" ||
         normalizedPath == "/std/collections/mapSingle" ||
         normalizedPath == "/std/collections/mapDouble" ||
         normalizedPath == "/std/collections/mapPair" ||
         normalizedPath == "/std/collections/mapTriple" ||
         normalizedPath == "/std/collections/mapQuad" ||
         normalizedPath == "/std/collections/mapQuint" ||
         normalizedPath == "/std/collections/mapSext" ||
         normalizedPath == "/std/collections/mapSept" ||
         normalizedPath == "/std/collections/mapOct";
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
        this->extractExperimentalVectorElementType(
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
        actualVectorSurface = "soa_vector";
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
                  normalizedActualBase ==
                      "std/collections/experimental_vector/Vector") {
                actualVectorSurface = "vector";
                actualElemType = actualTypeArgs.front();
              } else if (normalizedActualBase == "soa_vector") {
                actualVectorSurface = "soa_vector";
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
      if (arg.name != "ref" && arg.name != "ref_ref") {
        return false;
      }
      receiverOut = &arg.args.front();
      return true;
    }
    const std::string resolvedPath = resolveCalleePath(arg);
    const std::string resolvedPathCanonical =
        canonicalizeLegacySoaRefHelperPath(resolvedPath);
    if (!isSimpleCallName(arg, "ref") &&
        !isSimpleCallName(arg, "ref_ref") &&
        resolvedPathCanonical.rfind("/std/collections/soa_vector/ref", 0) != 0 &&
        resolvedPathCanonical.rfind("/std/collections/soa_vector/ref_ref", 0) != 0 &&
        resolvedPathCanonical.rfind("/std/collections/experimental_soa_vector/soaVectorRef", 0) !=
            0 &&
        resolvedPathCanonical.rfind("/std/collections/experimental_soa_vector/soaVectorRefRef",
                                    0) != 0 &&
        resolvedPathCanonical.rfind("/std/collections/experimental_soa_storage/soaColumnRef", 0) !=
            0) {
      return false;
    }
    receiverOut = &arg.args.front();
    return true;
  };
  auto isSoaFieldViewTypeText = [&](const std::string &typeText) -> bool {
    std::string base;
    std::string argText;
    const std::string normalized = normalizeBindingTypeName(typeText);
    if (splitTemplateTypeName(normalized, base, argText)) {
      const std::string normalizedBase = normalizeBindingTypeName(base);
      return normalizedBase == "SoaFieldView" ||
             normalizedBase == "std/collections/experimental_soa_storage/SoaFieldView";
    }
    return normalized == "SoaFieldView" ||
           normalized == "std/collections/experimental_soa_storage/SoaFieldView";
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
    if (!isReferenceEscapeCandidate(arg, param)) {
      return true;
    }
    return failResolvedCallArgumentDiagnostic(
        "reference escapes via argument to " + resolved);
  };
  auto checkStandaloneSoaFieldViewEscapes = [&](const Expr &arg,
                                                const ParameterInfo &param) -> bool {
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
    if (receiverExpr == nullptr || receiverExpr->kind == Expr::Kind::Name) {
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

  bool calleeIsUnsafe = false;
  if (context.resolvedDefinition != nullptr) {
    for (const auto &transform : context.resolvedDefinition->transforms) {
      if (transform.name == "unsafe") {
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

  if (context.resolvedDefinition != nullptr &&
      isCanonicalMapConstructorResolvedPath(resolved) &&
      !orderedArgs.empty()) {
    auto resolvedContainerType = [&](size_t templateIndex,
                                     const ParameterInfo &param,
                                     std::string &typeOut) {
      typeOut.clear();
      if (expr.templateArgs.size() == 2 && templateIndex < expr.templateArgs.size() &&
          !expr.templateArgs[templateIndex].empty()) {
        typeOut = expr.templateArgs[templateIndex];
        return;
      }
      typeOut = expectedBindingTypeText(param.binding);
    };

    std::string keyType;
    std::string valueType;
    resolvedContainerType(0, calleeParams.front(), keyType);
    if (calleeParams.size() > 1) {
      resolvedContainerType(1, calleeParams[1], valueType);
    }

    const std::vector<std::string> *definitionTemplateArgs =
        context.resolvedDefinition->templateArgs.empty()
            ? nullptr
            : &context.resolvedDefinition->templateArgs;
    std::unordered_set<std::string> visitingStructs;
    if (!keyType.empty() &&
        !isRelocationTrivialContainerElementType(keyType,
                                                 context.resolvedDefinition->namespacePrefix,
                                                 definitionTemplateArgs,
                                                 visitingStructs)) {
      return failResolvedCallArgumentDiagnostic(
          "map literal requires relocation-trivial map key type until container move/reallocation semantics are "
          "implemented: " +
          keyType);
    }
    visitingStructs.clear();
    if (!valueType.empty() &&
        !isRelocationTrivialContainerElementType(valueType,
                                                 context.resolvedDefinition->namespacePrefix,
                                                 definitionTemplateArgs,
                                                 visitingStructs)) {
      return failResolvedCallArgumentDiagnostic(
          "map literal requires relocation-trivial map value type until container move/reallocation semantics are "
          "implemented: " +
          valueType);
    }
  }

  handledOut = true;
  return true;
}

} // namespace primec::semantics
