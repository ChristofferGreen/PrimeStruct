#include "SemanticsValidator.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace primec::semantics {

namespace {

bool isCanonicalVectorCompatibilitySurface(const std::string &resolvedPath) {
  return resolvedPath.rfind("/std/collections/vector/", 0) == 0 ||
         resolvedPath.rfind("/vector/", 0) == 0 ||
         resolvedPath.rfind("/std/collections/vectorCount", 0) == 0 ||
         resolvedPath.rfind("/std/collections/vectorCapacity", 0) == 0 ||
         resolvedPath.rfind("/std/collections/vectorAt", 0) == 0 ||
         resolvedPath.rfind("/std/collections/vectorAtUnsafe", 0) == 0 ||
         resolvedPath.rfind("/std/collections/vectorPush", 0) == 0 ||
         resolvedPath.rfind("/std/collections/vectorPop", 0) == 0 ||
         resolvedPath.rfind("/std/collections/vectorReserve", 0) == 0 ||
         resolvedPath.rfind("/std/collections/vectorClear", 0) == 0 ||
         resolvedPath.rfind("/std/collections/vectorRemoveAt", 0) == 0 ||
         resolvedPath.rfind("/std/collections/vectorRemoveSwap", 0) == 0;
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
  switch (expectedKind) {
    case ReturnKind::Integer:
      return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 ||
             actualKind == ReturnKind::UInt64 || actualKind == ReturnKind::Bool ||
             actualKind == ReturnKind::Integer;
    case ReturnKind::Decimal:
      return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 ||
             actualKind == ReturnKind::UInt64 || actualKind == ReturnKind::Bool ||
             actualKind == ReturnKind::Float32 || actualKind == ReturnKind::Float64 ||
             actualKind == ReturnKind::Integer || actualKind == ReturnKind::Decimal;
    case ReturnKind::Complex:
      return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 ||
             actualKind == ReturnKind::UInt64 || actualKind == ReturnKind::Bool ||
             actualKind == ReturnKind::Float32 || actualKind == ReturnKind::Float64 ||
             actualKind == ReturnKind::Integer || actualKind == ReturnKind::Decimal ||
             actualKind == ReturnKind::Complex;
    default:
      return false;
  }
}

bool SemanticsValidator::isBuiltinCollectionLiteralExpr(const Expr &candidate) const {
  if (candidate.kind != Expr::Kind::Call) {
    return false;
  }
  if (defMap_.find(resolveCalleePath(candidate)) != defMap_.end()) {
    return false;
  }
  std::string collectionName;
  return getBuiltinCollectionName(candidate, collectionName);
}

bool SemanticsValidator::isStringExprForArgumentValidation(
    const Expr &arg,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers) const {
  if (dispatchResolvers.resolveStringTarget != nullptr &&
      dispatchResolvers.resolveStringTarget(arg)) {
    return true;
  }
  if (arg.kind == Expr::Kind::Call) {
    std::string accessName;
    if (defMap_.find(resolveCalleePath(arg)) == defMap_.end() &&
        getBuiltinArrayAccessName(arg, accessName) &&
        arg.args.size() == 2) {
      std::string mapValueType;
      if (resolveMapValueType(arg.args.front(), dispatchResolvers, mapValueType) &&
          normalizeBindingTypeName(mapValueType) == "string") {
        return true;
      }
    }
  }
  return false;
}

bool SemanticsValidator::extractExperimentalMapFieldTypesFromStructPath(
    const std::string &structPath,
    std::string &keyTypeOut,
    std::string &valueTypeOut) const {
  keyTypeOut.clear();
  valueTypeOut.clear();
  if (structPath.rfind("/std/collections/experimental_map/Map__", 0) != 0) {
    return false;
  }
  auto defIt = defMap_.find(structPath);
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    return false;
  }
  for (const auto &fieldExpr : defIt->second->statements) {
    if (!fieldExpr.isBinding) {
      continue;
    }
    BindingInfo fieldBinding;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!parseBindingInfo(fieldExpr,
                          defIt->second->namespacePrefix,
                          structNames_,
                          importAliases_,
                          fieldBinding,
                          restrictType,
                          parseError)) {
      continue;
    }
    std::string elemType;
    if (normalizeBindingTypeName(fieldBinding.typeName) == "vector" &&
        !fieldBinding.typeTemplateArg.empty()) {
      elemType = fieldBinding.typeTemplateArg;
    } else if (!extractExperimentalVectorElementType(fieldBinding, elemType)) {
      continue;
    }
    if (fieldExpr.name == "keys") {
      keyTypeOut = elemType;
    } else if (fieldExpr.name == "payloads") {
      valueTypeOut = elemType;
    }
  }
  return !keyTypeOut.empty() && !valueTypeOut.empty();
}

bool SemanticsValidator::extractExperimentalVectorElementTypeFromStructPath(
    const std::string &structPath,
    std::string &elemTypeOut) const {
  elemTypeOut.clear();
  if (structPath.rfind("/std/collections/experimental_vector/Vector__", 0) != 0) {
    return false;
  }
  auto defIt = defMap_.find(structPath);
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    return false;
  }
  for (const auto &fieldExpr : defIt->second->statements) {
    if (!fieldExpr.isBinding || fieldExpr.name != "data") {
      continue;
    }
    BindingInfo fieldBinding;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!parseBindingInfo(fieldExpr,
                          defIt->second->namespacePrefix,
                          structNames_,
                          importAliases_,
                          fieldBinding,
                          restrictType,
                          parseError)) {
      continue;
    }
    if (normalizeBindingTypeName(fieldBinding.typeName) != "Pointer" ||
        fieldBinding.typeTemplateArg.empty()) {
      continue;
    }
    std::string pointeeBase;
    std::string pointeeArgText;
    if (!splitTemplateTypeName(normalizeBindingTypeName(fieldBinding.typeTemplateArg),
                               pointeeBase,
                               pointeeArgText) ||
        normalizeBindingTypeName(pointeeBase) != "uninitialized") {
      continue;
    }
    std::vector<std::string> pointeeArgs;
    if (!splitTopLevelTemplateArgs(pointeeArgText, pointeeArgs) ||
        pointeeArgs.size() != 1) {
      continue;
    }
    elemTypeOut = pointeeArgs.front();
    return true;
  }
  return false;
}

bool SemanticsValidator::extractExperimentalVectorElementType(const BindingInfo &binding,
                                                              std::string &elemTypeOut) const {
  auto extractFromTypeText = [&](std::string normalizedType) {
    while (true) {
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
        base = normalizeBindingTypeName(base);
        if (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          normalizedType = normalizeBindingTypeName(args.front());
          continue;
        }
        std::string normalizedBase = base;
        if (!normalizedBase.empty() && normalizedBase.front() == '/') {
          normalizedBase.erase(normalizedBase.begin());
        }
        if ((normalizedBase == "Vector" ||
             normalizedBase == "std/collections/experimental_vector/Vector") &&
            !argText.empty()) {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          elemTypeOut = args.front();
          return true;
        }
      }

      std::string resolvedPath = normalizedType;
      if (!resolvedPath.empty() && resolvedPath.front() != '/') {
        resolvedPath.insert(resolvedPath.begin(), '/');
      }
      std::string normalizedResolvedPath = normalizeBindingTypeName(resolvedPath);
      if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
        normalizedResolvedPath.erase(normalizedResolvedPath.begin());
      }
      if (normalizedResolvedPath.rfind("std/collections/experimental_vector/Vector__", 0) != 0) {
        return false;
      }
      return extractExperimentalVectorElementTypeFromStructPath(resolvedPath, elemTypeOut);
    }
  };

  elemTypeOut.clear();
  if (binding.typeTemplateArg.empty()) {
    return extractFromTypeText(normalizeBindingTypeName(binding.typeName));
  }
  return extractFromTypeText(
      normalizeBindingTypeName(binding.typeName + "<" + binding.typeTemplateArg + ">"));
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
  auto inferCollectionBindingType = [&](const Expr &candidate,
                                        std::string &baseOut,
                                        std::vector<std::string> &argsOut) -> bool {
    baseOut.clear();
    argsOut.clear();
    BindingInfo inferredBinding;
    if (!inferBindingTypeFromInitializer(candidate, params, locals, inferredBinding)) {
      return false;
    }
    const std::string inferredTypeText = expectedBindingTypeText(inferredBinding);
    if (inferredTypeText.empty()) {
      return false;
    }
    std::string argText;
    if (!splitTemplateTypeName(inferredTypeText, baseOut, argText)) {
      baseOut = normalizeBindingTypeName(inferredTypeText);
      return !baseOut.empty();
    }
    baseOut = normalizeBindingTypeName(baseOut);
    return splitTopLevelTemplateArgs(argText, argsOut);
  };

  if (expectedTypeName.empty() || expectedTypeName == "auto") {
    return true;
  }
  if (normalizeBindingTypeName(expectedTypeName) == "string") {
    if (!isStringExprForArgumentValidation(arg, dispatchResolvers)) {
      error_ = "argument type mismatch for " + diagnosticResolved +
               " parameter " + param.name + ": expected string";
      return false;
    }
    return true;
  }
  if (isStringExprForArgumentValidation(arg, dispatchResolvers)) {
    error_ = "argument type mismatch for " + diagnosticResolved +
             " parameter " + param.name + ": expected " + expectedTypeText;
    return false;
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
          error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                   ": expected " + expectedTypeText + " got array<" + actualElemType + ">";
          return false;
        }
        if (dispatchResolvers.resolveVectorTarget != nullptr &&
            dispatchResolvers.resolveVectorTarget(arg, actualElemType)) {
          error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                   ": expected " + expectedTypeText + " got vector<" + actualElemType + ">";
          return false;
        }
        if (dispatchResolvers.resolveSoaVectorTarget != nullptr &&
            dispatchResolvers.resolveSoaVectorTarget(arg, actualElemType)) {
          error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                   ": expected " + expectedTypeText + " got soa_vector<" + actualElemType + ">";
          return false;
        }
      } else if (normalizedExpectedBase == "vector" && expectedTemplateArgs.size() == 1) {
        std::string actualElemType;
        if (dispatchResolvers.resolveVectorTarget != nullptr &&
            dispatchResolvers.resolveVectorTarget(arg, actualElemType)) {
          if (normalizeBindingTypeName(expectedTemplateArgs.front()) ==
              normalizeBindingTypeName(actualElemType)) {
            return true;
          }
          error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                   ": expected " + expectedTypeText + " got vector<" + actualElemType + ">";
          return false;
        }
        if (dispatchResolvers.resolveSoaVectorTarget != nullptr &&
            dispatchResolvers.resolveSoaVectorTarget(arg, actualElemType)) {
          error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                   ": expected " + expectedTypeText + " got soa_vector<" + actualElemType + ">";
          return false;
        }
        if (dispatchResolvers.resolveArrayTarget != nullptr &&
            dispatchResolvers.resolveArrayTarget(arg, actualElemType)) {
          error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                   ": expected " + expectedTypeText + " got array<" + actualElemType + ">";
          return false;
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
            error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                     ": expected " + expectedTypeText + " got vector<" + inferredArgs.front() + ">";
            return false;
          }
          if (normalizedInferredBase == "soa_vector") {
            error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                     ": expected " + expectedTypeText + " got soa_vector<" + inferredArgs.front() + ">";
            return false;
          }
          if (normalizedInferredBase == "array") {
            error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                     ": expected " + expectedTypeText + " got array<" + inferredArgs.front() + ">";
            return false;
          }
        }
      } else if ((normalizedExpectedBase == "Vector" ||
                  normalizedExpectedBase == "std/collections/experimental_vector/Vector") &&
                 expectedTemplateArgs.size() == 1) {
        std::string actualElemType;
        if (dispatchResolvers.resolveVectorTarget != nullptr &&
            dispatchResolvers.resolveVectorTarget(arg, actualElemType)) {
          if (isCanonicalVectorCompatibilitySurface(resolved) &&
              normalizeBindingTypeName(expectedTemplateArgs.front()) ==
                  normalizeBindingTypeName(actualElemType)) {
            return true;
          }
          error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                   ": expected " + expectedTypeText + " got vector<" + actualElemType + ">";
          return false;
        }
        if (dispatchResolvers.resolveSoaVectorTarget != nullptr &&
            dispatchResolvers.resolveSoaVectorTarget(arg, actualElemType)) {
          error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                   ": expected " + expectedTypeText + " got soa_vector<" + actualElemType + ">";
          return false;
        }
        if (dispatchResolvers.resolveArrayTarget != nullptr &&
            dispatchResolvers.resolveArrayTarget(arg, actualElemType)) {
          error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                   ": expected " + expectedTypeText + " got array<" + actualElemType + ">";
          return false;
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
          error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                   ": expected " + expectedTypeText + " got soa_vector<" + actualElemType + ">";
          return false;
        }
        if (dispatchResolvers.resolveVectorTarget != nullptr &&
            dispatchResolvers.resolveVectorTarget(arg, actualElemType)) {
          error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                   ": expected " + expectedTypeText + " got vector<" + actualElemType + ">";
          return false;
        }
        if (dispatchResolvers.resolveArrayTarget != nullptr &&
            dispatchResolvers.resolveArrayTarget(arg, actualElemType)) {
          error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                   ": expected " + expectedTypeText + " got array<" + actualElemType + ">";
          return false;
        }
      } else if (normalizedExpectedBase == "map" &&
                 expectedTemplateArgs.size() == 2) {
        std::string actualKeyType;
        std::string actualValueType;
        if (resolveMapKeyType(arg, dispatchResolvers, actualKeyType) &&
            resolveMapValueType(arg, dispatchResolvers, actualValueType) &&
            (normalizeBindingTypeName(expectedTemplateArgs[0]) !=
                 normalizeBindingTypeName(actualKeyType) ||
             normalizeBindingTypeName(expectedTemplateArgs[1]) !=
                 normalizeBindingTypeName(actualValueType))) {
          error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                   ": expected " + expectedTypeText + " got map<" + actualKeyType + ", " +
                   actualValueType + ">";
          return false;
        }
      }
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
      error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
               ": expected " + typeNameForReturnKind(expectedKind) + " got " +
               typeNameForReturnKind(actualKind);
      return false;
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
  std::string expectedExperimentalVectorElemType;
  std::string actualVectorElemType;
  const bool isCompatibleExperimentalMapReceiver =
      extractExperimentalMapFieldTypesFromStructPath(
          expectedStructPath, expectedMapKeyType, expectedMapValueType) &&
      resolveMapKeyType(arg, dispatchResolvers, actualMapKeyType) &&
      resolveMapValueType(arg, dispatchResolvers, actualMapValueType) &&
      normalizeBindingTypeName(expectedMapKeyType) ==
          normalizeBindingTypeName(actualMapKeyType) &&
      normalizeBindingTypeName(expectedMapValueType) ==
          normalizeBindingTypeName(actualMapValueType);
  const bool isCompatibleCanonicalVectorReceiver =
      isCanonicalVectorCompatibilitySurface(resolved) &&
      extractExperimentalVectorElementTypeFromStructPath(
          expectedStructPath, expectedExperimentalVectorElemType) &&
      dispatchResolvers.resolveVectorTarget != nullptr &&
      dispatchResolvers.resolveVectorTarget(arg, actualVectorElemType) &&
      normalizeBindingTypeName(expectedExperimentalVectorElemType) ==
          normalizeBindingTypeName(actualVectorElemType);
  const std::string actualStructPath = inferStructReturnPath(arg, params, locals);
  if (!actualStructPath.empty() && actualStructPath != expectedStructPath) {
    if (isCompatibleExperimentalMapReceiver || isCompatibleCanonicalVectorReceiver) {
      return true;
    }
    error_ = argumentStructMismatchDiagnostic(
        diagnosticResolved, param.name, expectedStructPath, actualStructPath);
    return false;
  }
  if (actualStructPath.empty()) {
    if (isCompatibleExperimentalMapReceiver || isCompatibleCanonicalVectorReceiver) {
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
      error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
               ": expected " + expectedStructPath + " got " +
               typeNameForReturnKind(actualKind);
      return false;
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

  std::string actualElementTypeText;
  if (!resolveArgsPackElementTypeForExpr(arg, params, locals, actualElementTypeText)) {
    error_ = "spread argument requires args<T> value";
    return false;
  }
  const ReturnKind expectedKind = returnKindForTypeName(expectedTypeName);
  if (expectedKind != ReturnKind::Unknown) {
    const ReturnKind actualKind = returnKindForTypeName(actualElementTypeText);
    if (actualKind == ReturnKind::Unknown || actualKind == expectedKind ||
        isSoftwareNumericParamCompatible(expectedKind, actualKind)) {
      return true;
    }
    error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
             ": expected " + typeNameForReturnKind(expectedKind) + " got " +
             typeNameForReturnKind(actualKind);
    return false;
  }

  const std::string expectedStructPath =
      resolveStructTypePath(expectedTypeName, callExpr.namespacePrefix, structNames_);
  if (!expectedStructPath.empty()) {
    const std::string actualStructPath =
        resolveStructTypePath(actualElementTypeText, arg.namespacePrefix, structNames_);
    if (!actualStructPath.empty() && actualStructPath != expectedStructPath) {
      error_ = argumentStructMismatchDiagnostic(
          diagnosticResolved, param.name, expectedStructPath, actualStructPath);
      return false;
    }
    if (actualStructPath.empty() &&
        normalizeBindingTypeName(actualElementTypeText) != expectedStructPath) {
      error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
               ": expected " + expectedStructPath + " got " +
               normalizeBindingTypeName(actualElementTypeText);
      return false;
    }
    return true;
  }
  if (normalizeBindingTypeName(actualElementTypeText) !=
      normalizeBindingTypeName(expectedTypeText)) {
    error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
             ": expected " + expectedTypeText + " got " +
             normalizeBindingTypeName(actualElementTypeText);
    return false;
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
