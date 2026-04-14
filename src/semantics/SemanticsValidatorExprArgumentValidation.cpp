#include "SemanticsValidator.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace primec::semantics {

namespace {

bool isCollectionLikeTemplateBase(std::string_view baseName) {
  const std::string normalizedBase = normalizeBindingTypeName(std::string(baseName));
  return normalizedBase == "array" || normalizedBase == "vector" ||
         normalizedBase == "soa_vector" || normalizedBase == "map" ||
         normalizedBase == "std/collections/map";
}

} // namespace

std::string SemanticsValidator::expectedBindingTypeText(const BindingInfo &binding) const {
  if (binding.typeName.empty()) {
    return std::string();
  }
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

std::string SemanticsValidator::argumentStructMismatchDiagnostic(
    std::string_view diagnosticResolved,
    std::string_view paramName,
    const std::string &expectedTypePath,
    const std::string &actualTypePath) const {
  const std::string prefix = "argument type mismatch for " + std::string(diagnosticResolved) +
                             " parameter " + std::string(paramName) + ": ";
  if (isImplicitMatrixQuaternionConversion(expectedTypePath, actualTypePath)) {
    return prefix + implicitMatrixQuaternionConversionDiagnostic(expectedTypePath, actualTypePath);
  }
  return prefix + "expected " + expectedTypePath + " got " + actualTypePath;
}

bool SemanticsValidator::isSoftwareNumericParamCompatible(ReturnKind expectedKind, ReturnKind actualKind) {
  auto isSoftwareIntegerKind = [](ReturnKind kind) {
    return kind == ReturnKind::Int || kind == ReturnKind::Int64 ||
           kind == ReturnKind::UInt64 || kind == ReturnKind::Bool ||
           kind == ReturnKind::Integer;
  };
  auto isSoftwareDecimalKind = [&](ReturnKind kind) {
    return isSoftwareIntegerKind(kind) || kind == ReturnKind::Float32 ||
           kind == ReturnKind::Float64 || kind == ReturnKind::Decimal;
  };
  switch (expectedKind) {
    case ReturnKind::Int:
    case ReturnKind::Int64:
    case ReturnKind::UInt64:
    case ReturnKind::Integer:
      return isSoftwareIntegerKind(actualKind);
    case ReturnKind::Float32:
    case ReturnKind::Float64:
    case ReturnKind::Decimal:
      return isSoftwareDecimalKind(actualKind);
    case ReturnKind::Complex:
      return isSoftwareDecimalKind(actualKind) ||
             actualKind == ReturnKind::Complex;
    default:
      return false;
  }
}

bool SemanticsValidator::validateArgumentTypeAgainstParam(
    const Expr &arg,
    const ParameterInfo &param,
    const std::string &expectedTypeName,
    const std::string &expectedTypeText,
    const ExprArgumentValidationContext &context) {
  const Expr &callExpr = *context.callExpr;
  const std::string &resolved = *context.resolved;
  const std::string &diagnosticResolved = *context.diagnosticResolved;
  const auto &params = *context.params;
  const auto &locals = *context.locals;
  const auto &dispatchResolvers = *context.dispatchResolvers;
  auto failArgumentValidation = [&](const Expr &diagnosticExpr, std::string message) -> bool {
    return failExprDiagnostic(diagnosticExpr, std::move(message));
  };
  auto inferCollectionBindingType = [&](const Expr &candidate,
                                        std::string &baseOut,
                                        std::vector<std::string> &argsOut) -> bool {
    auto inferFromCallCollection = [&](const Expr &callCandidate) -> bool {
      if (callCandidate.kind != Expr::Kind::Call) {
        return false;
      }
      std::string collectionTypePath;
      if (!resolveCallCollectionTypePath(callCandidate, params, locals, collectionTypePath)) {
        return false;
      }
      baseOut = normalizeBindingTypeName(collectionTypePath);
      if (!baseOut.empty() && baseOut.front() == '/') {
        baseOut.erase(baseOut.begin());
      }
      if (baseOut.empty()) {
        return false;
      }
      std::vector<std::string> resolvedArgs;
      if (resolveCallCollectionTemplateArgs(callCandidate, baseOut, params, locals, resolvedArgs)) {
        argsOut = std::move(resolvedArgs);
      }
      return true;
    };
    baseOut.clear();
    argsOut.clear();
    BindingInfo inferredBinding;
    if (!inferBindingTypeFromInitializer(candidate, params, locals, inferredBinding)) {
      return inferFromCallCollection(candidate);
    }
    const std::string inferredTypeText = expectedBindingTypeText(inferredBinding);
    if (inferredTypeText.empty()) {
      return inferFromCallCollection(candidate);
    }
    std::string argText;
    if (!splitTemplateTypeName(inferredTypeText, baseOut, argText)) {
      if (inferFromCallCollection(candidate)) {
        return true;
      }
      baseOut = normalizeBindingTypeName(inferredTypeText);
      return !baseOut.empty();
    }
    baseOut = normalizeBindingTypeName(baseOut);
    return splitTopLevelTemplateArgs(argText, argsOut);
  };
  auto maybePreferExplicitCanonicalMapKeyDiagnostic =
      [&](const std::vector<std::string> &expectedTemplateArgs) -> bool {
    if (param.name != "values" || callExpr.isMethodCall ||
        hasNamedArguments(callExpr.argNames) || callExpr.args.size() != 2 ||
        (diagnosticResolved != "/std/collections/map/at" &&
         diagnosticResolved != "/std/collections/map/at_ref" &&
         diagnosticResolved != "/std/collections/map/at_unsafe" &&
         diagnosticResolved != "/std/collections/map/at_unsafe_ref")) {
      return false;
    }

    std::string receiverKeyType;
    std::string receiverValueType;
    if (!resolveMapKeyType(callExpr.args[1], dispatchResolvers, receiverKeyType) ||
        !resolveMapValueType(callExpr.args[1], dispatchResolvers,
                             receiverValueType) ||
        normalizeBindingTypeName(receiverKeyType) !=
            normalizeBindingTypeName(expectedTemplateArgs[0]) ||
        normalizeBindingTypeName(receiverValueType) !=
            normalizeBindingTypeName(expectedTemplateArgs[1])) {
      return false;
    }

    std::string misplacedReceiverKeyType;
    std::string misplacedReceiverValueType;
    if (resolveMapKeyType(callExpr.args[0], dispatchResolvers,
                          misplacedReceiverKeyType) &&
        resolveMapValueType(callExpr.args[0], dispatchResolvers,
                            misplacedReceiverValueType)) {
      return false;
    }

    const std::string normalizedExpectedKeyType =
        normalizeBindingTypeName(expectedTemplateArgs[0]);
    if (normalizedExpectedKeyType == "string") {
      if (!isStringExprForArgumentValidation(callExpr.args[0],
                                             dispatchResolvers)) {
        (void)failArgumentValidation(
            callExpr.args[0],
            "argument type mismatch for " + diagnosticResolved + " parameter key");
        return true;
      }
      return false;
    }

    const ReturnKind expectedKeyKind =
        returnKindForTypeName(normalizedExpectedKeyType);
    if (expectedKeyKind == ReturnKind::Unknown) {
      return false;
    }

    if (isStringExprForArgumentValidation(callExpr.args[0], dispatchResolvers)) {
      (void)failArgumentValidation(
          callExpr.args[0],
          "argument type mismatch for " + diagnosticResolved + " parameter key");
      return true;
    }

    const ReturnKind actualKeyKind =
        inferExprReturnKind(callExpr.args[0], params, locals);
    if (actualKeyKind != ReturnKind::Unknown &&
        actualKeyKind != expectedKeyKind) {
      (void)failArgumentValidation(
          callExpr.args[0],
          "argument type mismatch for " + diagnosticResolved + " parameter key");
      return true;
    }

    return false;
  };

  if (expectedTypeName.empty() || expectedTypeName == "auto") {
    return true;
  }
  if (normalizeBindingTypeName(expectedTypeName) == "string") {
    if (!isStringExprForArgumentValidation(arg, dispatchResolvers)) {
      return failArgumentValidation(
          arg,
          "argument type mismatch for " + diagnosticResolved +
              " parameter " + param.name + ": expected string");
    }
    return true;
  }
  if (isStringExprForArgumentValidation(arg, dispatchResolvers)) {
    return failArgumentValidation(
        arg,
        "argument type mismatch for " + diagnosticResolved +
            " parameter " + param.name + ": expected " + expectedTypeText);
  }

  std::string expectedBase;
  std::string expectedArgText;
  if (splitTemplateTypeName(expectedTypeText, expectedBase, expectedArgText)) {
    const std::string normalizedExpectedBase = normalizeBindingTypeName(expectedBase);
    std::vector<std::string> expectedTemplateArgs;
    if (splitTopLevelTemplateArgs(expectedArgText, expectedTemplateArgs)) {
      if (normalizedExpectedBase == "array" && expectedTemplateArgs.size() == 1) {
        std::string actualElemType;
        if (dispatchResolvers.resolveArrayTarget != nullptr &&
            dispatchResolvers.resolveArrayTarget(arg, actualElemType)) {
          if (normalizeBindingTypeName(expectedTemplateArgs.front()) ==
              normalizeBindingTypeName(actualElemType)) {
            return true;
          }
          return failArgumentValidation(
              arg,
              "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                  ": expected " + expectedTypeText + " got array<" + actualElemType + ">");
        }
        if (dispatchResolvers.resolveVectorTarget != nullptr &&
            dispatchResolvers.resolveVectorTarget(arg, actualElemType)) {
          return failArgumentValidation(
              arg,
              "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                  ": expected " + expectedTypeText + " got vector<" + actualElemType + ">");
        }
        if (dispatchResolvers.resolveSoaVectorTarget != nullptr &&
            dispatchResolvers.resolveSoaVectorTarget(arg, actualElemType)) {
          return failArgumentValidation(
              arg,
              "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                  ": expected " + expectedTypeText + " got soa_vector<" + actualElemType + ">");
        }
      } else if (normalizedExpectedBase == "vector" && expectedTemplateArgs.size() == 1) {
        std::string actualElemType;
        if (dispatchResolvers.resolveVectorTarget != nullptr &&
            dispatchResolvers.resolveVectorTarget(arg, actualElemType)) {
          if (normalizeBindingTypeName(expectedTemplateArgs.front()) ==
              normalizeBindingTypeName(actualElemType)) {
            return true;
          }
          return failArgumentValidation(
              arg,
              "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                  ": expected " + expectedTypeText + " got vector<" + actualElemType + ">");
        }
        if (dispatchResolvers.resolveSoaVectorTarget != nullptr &&
            dispatchResolvers.resolveSoaVectorTarget(arg, actualElemType)) {
          return failArgumentValidation(
              arg,
              "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                  ": expected " + expectedTypeText + " got soa_vector<" + actualElemType + ">");
        }
        if (dispatchResolvers.resolveArrayTarget != nullptr &&
            dispatchResolvers.resolveArrayTarget(arg, actualElemType)) {
          return failArgumentValidation(
              arg,
              "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                  ": expected " + expectedTypeText + " got array<" + actualElemType + ">");
        }
        std::string inferredBase;
        std::vector<std::string> inferredArgs;
        if (inferCollectionBindingType(arg, inferredBase, inferredArgs) &&
            inferredArgs.size() == 1) {
          const std::string normalizedInferredBase = normalizeBindingTypeName(inferredBase);
          const std::string normalizedExpectedArg =
              normalizeBindingTypeName(expectedTemplateArgs.front());
          const std::string normalizedInferredArg =
              normalizeBindingTypeName(inferredArgs.front());
          if (normalizedInferredBase == "vector") {
            if (normalizedExpectedArg == normalizedInferredArg) {
              return true;
            }
            return failArgumentValidation(
                arg,
                "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                    ": expected " + expectedTypeText + " got vector<" + inferredArgs.front() + ">");
          }
          if (normalizedInferredBase == "soa_vector") {
            return failArgumentValidation(
                arg,
                "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                    ": expected " + expectedTypeText + " got soa_vector<" + inferredArgs.front() + ">");
          }
          if (normalizedInferredBase == "array") {
            return failArgumentValidation(
                arg,
                "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                    ": expected " + expectedTypeText + " got array<" + inferredArgs.front() + ">");
          }
        } else if (inferCollectionBindingType(arg, inferredBase, inferredArgs)) {
          std::string inferredStructPath = inferredBase;
          if (!inferredStructPath.empty() && inferredStructPath.front() != '/') {
            inferredStructPath.insert(inferredStructPath.begin(), '/');
          }
          std::string actualElemType;
          if (extractExperimentalVectorElementTypeFromStructPath(inferredStructPath,
                                                                actualElemType)) {
            if (normalizeBindingTypeName(expectedTemplateArgs.front()) ==
                normalizeBindingTypeName(actualElemType)) {
              return true;
            }
            return failArgumentValidation(
                arg,
                "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                    ": expected " + expectedTypeText + " got vector<" + actualElemType + ">");
          }
        }
      } else if ((normalizedExpectedBase == "Vector" ||
                  normalizedExpectedBase == "std/collections/experimental_vector/Vector") &&
                 expectedTemplateArgs.size() == 1) {
        std::string actualElemType;
        if (dispatchResolvers.resolveVectorTarget != nullptr &&
            dispatchResolvers.resolveVectorTarget(arg, actualElemType)) {
          if (normalizeBindingTypeName(expectedTemplateArgs.front()) ==
                  normalizeBindingTypeName(actualElemType)) {
            return true;
          }
          return failArgumentValidation(
              arg,
              "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                  ": expected " + expectedTypeText + " got vector<" + actualElemType + ">");
        }
        if (dispatchResolvers.resolveSoaVectorTarget != nullptr &&
            dispatchResolvers.resolveSoaVectorTarget(arg, actualElemType)) {
          return failArgumentValidation(
              arg,
              "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                  ": expected " + expectedTypeText + " got soa_vector<" + actualElemType + ">");
        }
        if (dispatchResolvers.resolveArrayTarget != nullptr &&
            dispatchResolvers.resolveArrayTarget(arg, actualElemType)) {
          return failArgumentValidation(
              arg,
              "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                  ": expected " + expectedTypeText + " got array<" + actualElemType + ">");
        }
      } else if (normalizedExpectedBase == "soa_vector" &&
                 expectedTemplateArgs.size() == 1) {
        std::string actualElemType;
        if (dispatchResolvers.resolveSoaVectorTarget != nullptr &&
            dispatchResolvers.resolveSoaVectorTarget(arg, actualElemType)) {
          if (normalizeBindingTypeName(expectedTemplateArgs.front()) ==
              normalizeBindingTypeName(actualElemType)) {
            return true;
          }
          return failArgumentValidation(
              arg,
              "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                  ": expected " + expectedTypeText + " got soa_vector<" + actualElemType + ">");
        }
        if (dispatchResolvers.resolveVectorTarget != nullptr &&
            dispatchResolvers.resolveVectorTarget(arg, actualElemType)) {
          return failArgumentValidation(
              arg,
              "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                  ": expected " + expectedTypeText + " got vector<" + actualElemType + ">");
        }
        if (dispatchResolvers.resolveArrayTarget != nullptr &&
            dispatchResolvers.resolveArrayTarget(arg, actualElemType)) {
          return failArgumentValidation(
              arg,
              "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                  ": expected " + expectedTypeText + " got array<" + actualElemType + ">");
        }
      } else if ((normalizedExpectedBase == "map" ||
                  normalizedExpectedBase == "std/collections/map") &&
                 expectedTemplateArgs.size() == 2) {
        std::string actualKeyType;
        std::string actualValueType;
        if (resolveMapKeyType(arg, dispatchResolvers, actualKeyType) &&
            resolveMapValueType(arg, dispatchResolvers, actualValueType) &&
            (normalizeBindingTypeName(expectedTemplateArgs[0]) !=
                 normalizeBindingTypeName(actualKeyType) ||
             normalizeBindingTypeName(expectedTemplateArgs[1]) !=
                 normalizeBindingTypeName(actualValueType))) {
          return failArgumentValidation(
              arg,
              "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                  ": expected " + expectedTypeText + " got map<" + actualKeyType + ", " +
                  actualValueType + ">");
        }
        if (maybePreferExplicitCanonicalMapKeyDiagnostic(expectedTemplateArgs)) {
          return false;
        }
      } else if ((normalizedExpectedBase == "Map" ||
                  normalizedExpectedBase == "std/collections/experimental_map/Map") &&
                 expectedTemplateArgs.size() == 2) {
        std::string actualKeyType;
        std::string actualValueType;
        if (resolveMapKeyType(arg, dispatchResolvers, actualKeyType) &&
            resolveMapValueType(arg, dispatchResolvers, actualValueType)) {
          if (normalizeBindingTypeName(expectedTemplateArgs[0]) ==
                  normalizeBindingTypeName(actualKeyType) &&
              normalizeBindingTypeName(expectedTemplateArgs[1]) ==
                  normalizeBindingTypeName(actualValueType)) {
            return true;
          }
          return failArgumentValidation(
              arg,
              "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                  ": expected " + expectedTypeText + " got map<" + actualKeyType + ", " +
                  actualValueType + ">");
        }

        std::string inferredBase;
        std::vector<std::string> inferredArgs;
        if (inferCollectionBindingType(arg, inferredBase, inferredArgs) &&
            inferredArgs.size() == 2) {
          const std::string normalizedInferredBase = normalizeBindingTypeName(inferredBase);
          if (isMapCollectionTypeName(normalizedInferredBase) ||
              normalizedInferredBase == "Map" ||
              normalizedInferredBase == "std/collections/experimental_map/Map") {
            if (normalizeBindingTypeName(expectedTemplateArgs[0]) ==
                    normalizeBindingTypeName(inferredArgs[0]) &&
                normalizeBindingTypeName(expectedTemplateArgs[1]) ==
                    normalizeBindingTypeName(inferredArgs[1])) {
              return true;
            }
            return failArgumentValidation(
                arg,
                "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                    ": expected " + expectedTypeText + " got map<" + inferredArgs[0] + ", " +
                    inferredArgs[1] + ">");
          }
        }
      }
    }
    if (isCollectionLikeTemplateBase(normalizedExpectedBase)) {
      std::string actualTypeText;
      if (inferQueryExprTypeText(arg, params, locals, actualTypeText) &&
          !actualTypeText.empty()) {
        std::string actualBase;
        std::string actualArgText;
        if (!splitTemplateTypeName(actualTypeText, actualBase, actualArgText)) {
          if (normalizedExpectedBase == "vector" &&
              expectedTemplateArgs.size() == 1) {
            std::string actualStructPath = actualTypeText;
            if (!actualStructPath.empty() && actualStructPath.front() != '/') {
              actualStructPath.insert(actualStructPath.begin(), '/');
            }
            std::string actualElemType;
            if (extractExperimentalVectorElementTypeFromStructPath(actualStructPath,
                                                                  actualElemType)) {
              if (normalizeBindingTypeName(actualElemType) ==
                  normalizeBindingTypeName(expectedTemplateArgs.front())) {
                return true;
              }
              return failArgumentValidation(
                  arg,
                  "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                      ": expected " + expectedTypeText + " got vector<" + actualElemType + ">");
            }
          }
          return failArgumentValidation(
              arg,
              "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                  ": expected " + expectedTypeText + " got " +
                  normalizeBindingTypeName(actualTypeText));
        }
        actualBase = normalizeBindingTypeName(actualBase);
        if (isCollectionLikeTemplateBase(actualBase) &&
            normalizeBindingTypeName(actualTypeText) !=
                normalizeBindingTypeName(expectedTypeText)) {
          return failArgumentValidation(
              arg,
              "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                  ": expected " + expectedTypeText + " got " +
                  normalizeBindingTypeName(actualTypeText));
        }
      }
    }
  }

  std::string expectedExplicitVectorElemType;
  if (extractExperimentalVectorElementType(param.binding, expectedExplicitVectorElemType)) {
    std::string actualElemType;
    if (dispatchResolvers.resolveVectorTarget != nullptr &&
        dispatchResolvers.resolveVectorTarget(arg, actualElemType)) {
      if (normalizeBindingTypeName(expectedExplicitVectorElemType) ==
          normalizeBindingTypeName(actualElemType)) {
        return true;
      }
      return failArgumentValidation(
          arg,
          "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
              ": expected " + expectedTypeText + " got vector<" + actualElemType + ">");
    }
    if (dispatchResolvers.resolveSoaVectorTarget != nullptr &&
        dispatchResolvers.resolveSoaVectorTarget(arg, actualElemType)) {
      return failArgumentValidation(
          arg,
          "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
              ": expected " + expectedTypeText + " got soa_vector<" + actualElemType + ">");
    }
    if (dispatchResolvers.resolveArrayTarget != nullptr &&
        dispatchResolvers.resolveArrayTarget(arg, actualElemType)) {
      return failArgumentValidation(
          arg,
          "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
              ": expected " + expectedTypeText + " got array<" + actualElemType + ">");
    }
  }

  const ReturnKind expectedKind =
      returnKindForTypeName(normalizeBindingTypeName(expectedTypeName));
  if (expectedKind != ReturnKind::Unknown) {
    const ReturnKind actualKind = inferExprReturnKind(arg, params, locals);
    if (actualKind == ReturnKind::Array && isBuiltinCollectionLiteralExpr(arg)) {
      return true;
    }
    if (isSoftwareNumericParamCompatible(expectedKind, actualKind)) {
      return true;
    }
    if (actualKind != ReturnKind::Unknown && actualKind != expectedKind) {
      return failArgumentValidation(
          arg,
          "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
              ": expected " + typeNameForReturnKind(expectedKind) + " got " +
              typeNameForReturnKind(actualKind));
    }
    return true;
  }

  std::string expectedStructPath =
      resolveStructTypePath(expectedTypeName, callExpr.namespacePrefix, structNames_);
  if (expectedStructPath.empty()) {
    const size_t calleeSlash = resolved.find_last_of('/');
    if (calleeSlash != std::string::npos && calleeSlash > 0) {
      expectedStructPath =
          resolveStructTypePath(expectedTypeName, resolved.substr(0, calleeSlash), structNames_);
    }
  }
  if (expectedStructPath.empty()) {
    return true;
  }

  std::string expectedMapKeyType;
  std::string expectedMapValueType;
  std::string actualMapKeyType;
  std::string actualMapValueType;
  std::string actualMapBase;
  std::vector<std::string> actualMapTemplateArgs;
  std::string expectedExperimentalVectorElemType;
  std::string expectedExperimentalSoaVectorElemType;
  std::string actualVectorElemType;
  std::string actualSoaVectorElemType;
  const bool isCompatibleExperimentalMapReceiver =
      extractExperimentalMapFieldTypesFromStructPath(
          expectedStructPath, expectedMapKeyType, expectedMapValueType) &&
      ((resolveMapKeyType(arg, dispatchResolvers, actualMapKeyType) &&
        resolveMapValueType(arg, dispatchResolvers, actualMapValueType) &&
        normalizeBindingTypeName(expectedMapKeyType) ==
            normalizeBindingTypeName(actualMapKeyType) &&
        normalizeBindingTypeName(expectedMapValueType) ==
            normalizeBindingTypeName(actualMapValueType)) ||
       (inferCollectionBindingType(arg, actualMapBase, actualMapTemplateArgs) &&
        actualMapTemplateArgs.size() == 2 &&
        (isMapCollectionTypeName(normalizeBindingTypeName(actualMapBase)) ||
         normalizeBindingTypeName(actualMapBase) == "Map" ||
         normalizeBindingTypeName(actualMapBase) == "std/collections/experimental_map/Map") &&
        normalizeBindingTypeName(expectedMapKeyType) ==
            normalizeBindingTypeName(actualMapTemplateArgs[0]) &&
        normalizeBindingTypeName(expectedMapValueType) ==
            normalizeBindingTypeName(actualMapTemplateArgs[1])));
  const bool isCompatibleCanonicalVectorReceiver = [&] {
    if (!(extractExperimentalVectorElementType(param.binding, expectedExperimentalVectorElemType) ||
          extractExperimentalVectorElementTypeFromStructPath(
              expectedStructPath, expectedExperimentalVectorElemType))) {
      return false;
    }
    if (dispatchResolvers.resolveVectorTarget != nullptr &&
        dispatchResolvers.resolveVectorTarget(arg, actualVectorElemType) &&
        normalizeBindingTypeName(expectedExperimentalVectorElemType) ==
            normalizeBindingTypeName(actualVectorElemType)) {
      return true;
    }
    std::string inferredBase;
    std::vector<std::string> inferredArgs;
    if (inferCollectionBindingType(arg, inferredBase, inferredArgs) &&
        inferredArgs.size() == 1) {
      std::string normalizedInferredBase = normalizeBindingTypeName(inferredBase);
      if (!normalizedInferredBase.empty() && normalizedInferredBase.front() == '/') {
        normalizedInferredBase.erase(normalizedInferredBase.begin());
      }
      if ((normalizedInferredBase == "vector" ||
           normalizedInferredBase == "Vector" ||
           normalizedInferredBase == "std/collections/experimental_vector/Vector") &&
          normalizeBindingTypeName(expectedExperimentalVectorElemType) ==
              normalizeBindingTypeName(inferredArgs.front())) {
        return true;
      }
    }
    std::vector<std::string> resolvedVectorArgs;
    if (resolveCallCollectionTemplateArgs(arg, "vector", params, locals, resolvedVectorArgs) &&
        resolvedVectorArgs.size() == 1 &&
        normalizeBindingTypeName(expectedExperimentalVectorElemType) ==
            normalizeBindingTypeName(resolvedVectorArgs.front())) {
      return true;
    }
    std::string inferredTypeText;
    if (!inferQueryExprTypeText(arg, params, locals, inferredTypeText) || inferredTypeText.empty()) {
      return false;
    }
    std::string inferredBaseText;
    std::string inferredArgText;
    if (!splitTemplateTypeName(inferredTypeText, inferredBaseText, inferredArgText)) {
      return false;
    }
    std::vector<std::string> inferredTypeArgs;
    if (!splitTopLevelTemplateArgs(inferredArgText, inferredTypeArgs) || inferredTypeArgs.size() != 1) {
      return false;
    }
    std::string normalizedInferredBase = normalizeBindingTypeName(inferredBaseText);
    if (!normalizedInferredBase.empty() && normalizedInferredBase.front() == '/') {
      normalizedInferredBase.erase(normalizedInferredBase.begin());
    }
    return (normalizedInferredBase == "vector" ||
            normalizedInferredBase == "Vector" ||
            normalizedInferredBase == "std/collections/experimental_vector/Vector") &&
           normalizeBindingTypeName(expectedExperimentalVectorElemType) ==
               normalizeBindingTypeName(inferredTypeArgs.front());
  }();
  const bool isCompatibleExperimentalSoaVectorReceiver = [&] {
    if (!extractExperimentalSoaVectorElementTypeFromStructPath(
            expectedStructPath, expectedExperimentalSoaVectorElemType)) {
      return false;
    }
    if (dispatchResolvers.resolveSoaVectorTarget != nullptr &&
        dispatchResolvers.resolveSoaVectorTarget(arg, actualSoaVectorElemType) &&
        normalizeBindingTypeName(expectedExperimentalSoaVectorElemType) ==
            normalizeBindingTypeName(actualSoaVectorElemType)) {
      return true;
    }
    std::string inferredBase;
    std::vector<std::string> inferredArgs;
    if (inferCollectionBindingType(arg, inferredBase, inferredArgs) &&
        inferredArgs.size() == 1) {
      std::string normalizedInferredBase = normalizeBindingTypeName(inferredBase);
      if (!normalizedInferredBase.empty() && normalizedInferredBase.front() == '/') {
        normalizedInferredBase.erase(normalizedInferredBase.begin());
      }
      return normalizedInferredBase == "soa_vector" &&
             normalizeBindingTypeName(expectedExperimentalSoaVectorElemType) ==
                 normalizeBindingTypeName(inferredArgs.front());
    }
    return false;
  }();
  const std::string actualStructPath = inferStructReturnPath(arg, params, locals);
  if (!actualStructPath.empty() && actualStructPath != expectedStructPath) {
    if (isCompatibleExperimentalMapReceiver || isCompatibleCanonicalVectorReceiver ||
        isCompatibleExperimentalSoaVectorReceiver) {
      return true;
    }
    return failArgumentValidation(
        arg,
        argumentStructMismatchDiagnostic(
            diagnosticResolved, param.name, expectedStructPath, actualStructPath));
  }
  if (actualStructPath.empty()) {
    if (isCompatibleExperimentalMapReceiver || isCompatibleCanonicalVectorReceiver ||
        isCompatibleExperimentalSoaVectorReceiver) {
      return true;
    }
    const BindingInfo *actualBinding = nullptr;
    if (arg.kind == Expr::Kind::Name) {
      actualBinding = findParamBinding(params, arg.name);
      if (actualBinding == nullptr) {
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          actualBinding = &it->second;
        }
      }
    }
    if (actualBinding != nullptr &&
        (normalizeBindingTypeName(actualBinding->typeName) == "Reference" ||
         normalizeBindingTypeName(actualBinding->typeName) == "Pointer") &&
        !actualBinding->typeTemplateArg.empty()) {
      std::string actualTargetStructPath =
          resolveStructTypePath(actualBinding->typeTemplateArg, arg.namespacePrefix, structNames_);
      if (actualTargetStructPath.empty()) {
        const size_t calleeSlash = resolved.find_last_of('/');
        if (calleeSlash != std::string::npos && calleeSlash > 0) {
          actualTargetStructPath = resolveStructTypePath(
              actualBinding->typeTemplateArg, resolved.substr(0, calleeSlash), structNames_);
        }
      }
      if (actualTargetStructPath == expectedStructPath) {
        return true;
      }
    }
    const ReturnKind actualKind = inferExprReturnKind(arg, params, locals);
    if (actualKind != ReturnKind::Unknown) {
      return failArgumentValidation(
          arg,
          "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
              ": expected " + expectedStructPath + " got " +
              typeNameForReturnKind(actualKind));
    }
  }
  return true;
}

bool SemanticsValidator::validateSpreadArgumentTypeAgainstParam(
    const Expr &arg,
    const ParameterInfo &param,
    const std::string &expectedTypeName,
    const std::string &expectedTypeText,
    const ExprArgumentValidationContext &context) {
  const Expr &callExpr = *context.callExpr;
  const std::string &diagnosticResolved = *context.diagnosticResolved;
  const auto &params = *context.params;
  const auto &locals = *context.locals;
  auto failSpreadArgumentValidation = [&](const Expr &diagnosticExpr, std::string message) -> bool {
    return failExprDiagnostic(diagnosticExpr, std::move(message));
  };

  std::string actualElementTypeText;
  if (!resolveArgsPackElementTypeForExpr(arg, params, locals, actualElementTypeText)) {
    return failSpreadArgumentValidation(arg, "spread argument requires args<T> value");
  }
  const ReturnKind expectedKind = returnKindForTypeName(expectedTypeName);
  if (expectedKind != ReturnKind::Unknown) {
    const ReturnKind actualKind = returnKindForTypeName(actualElementTypeText);
    if (actualKind == ReturnKind::Unknown || actualKind == expectedKind ||
        isSoftwareNumericParamCompatible(expectedKind, actualKind)) {
      return true;
    }
    return failSpreadArgumentValidation(
        arg,
        "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
            ": expected " + typeNameForReturnKind(expectedKind) + " got " +
            typeNameForReturnKind(actualKind));
  }

  const std::string expectedStructPath =
      resolveStructTypePath(expectedTypeName, callExpr.namespacePrefix, structNames_);
  if (!expectedStructPath.empty()) {
    const std::string actualStructPath =
        resolveStructTypePath(actualElementTypeText, arg.namespacePrefix, structNames_);
    if (!actualStructPath.empty() && actualStructPath != expectedStructPath) {
      return failSpreadArgumentValidation(
          arg,
          argumentStructMismatchDiagnostic(
              diagnosticResolved, param.name, expectedStructPath, actualStructPath));
    }
    if (actualStructPath.empty() &&
        normalizeBindingTypeName(actualElementTypeText) != expectedStructPath) {
      return failSpreadArgumentValidation(
          arg,
          "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
              ": expected " + expectedStructPath + " got " +
              normalizeBindingTypeName(actualElementTypeText));
    }
    return true;
  }
  if (normalizeBindingTypeName(actualElementTypeText) !=
      normalizeBindingTypeName(expectedTypeText)) {
    return failSpreadArgumentValidation(
        arg,
        "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
            ": expected " + expectedTypeText + " got " +
            normalizeBindingTypeName(actualElementTypeText));
  }
  return true;
}

bool SemanticsValidator::validateArgumentsForParameter(
    const ParameterInfo &param,
    const std::string &expectedTypeName,
    const std::string &expectedTypeText,
    const std::vector<const Expr *> &argsToValidate,
    const ExprArgumentValidationContext &context) {
  for (const Expr *arg : argsToValidate) {
    if (arg == nullptr) {
      continue;
    }
    if (arg->isSpread) {
      if (!validateSpreadArgumentTypeAgainstParam(
              *arg, param, expectedTypeName, expectedTypeText, context)) {
        return false;
      }
      continue;
    }
    if (!validateArgumentTypeAgainstParam(
            *arg, param, expectedTypeName, expectedTypeText, context)) {
      return false;
    }
  }
  return true;
}

} // namespace primec::semantics
