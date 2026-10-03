#include "TemplateMonomorphAssignmentTargetResolution.h"
#include "TemplateMonomorphBindingBlockInference.h"
#include "TemplateMonomorphBindingCallInference.h"
#include "TemplateMonomorphDefinitionBindingSetup.h"
#include "TemplateMonomorphDefinitionExperimentalCollectionRewrites.h"
#include "TemplateMonomorphDefinitionReturnOrchestration.h"
#include "TemplateMonomorphDefinitionRewrites.h"
#include "TemplateMonomorphExecutionRewrites.h"
#include "TemplateMonomorphExperimentalCollectionArgumentRewrites.h"
#include "TemplateMonomorphExperimentalCollectionConstructorRewrites.h"
#include "TemplateMonomorphExperimentalCollectionReceiverResolution.h"
#include "TemplateMonomorphExperimentalCollectionReturnRewrites.h"
#include "TemplateMonomorphExperimentalCollectionReturnSetup.h"
#include "TemplateMonomorphExperimentalCollectionTargetValueRewrites.h"
#include "TemplateMonomorphExperimentalCollectionValueRewrites.h"
#include "TemplateMonomorphExpressionRewrite.h"
#include "TemplateMonomorphFallbackTypeInference.h"
#include "TemplateMonomorphFinalOrchestration.h"
#include "TemplateMonomorphImplicitTemplateInference.h"
#include "TemplateMonomorphMethodTargets.h"
#include "TemplateMonomorphTemplateSpecialization.h"
#include "TemplateMonomorphTypeResolution.h"
#include "SemanticsHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "TemplateMonomorphCoreUtilities.h"
#include "TemplateMonomorphSetupUtilities.h"
#include "TemplateMonomorphCollectionCompatibilityPaths.h"
#include "TemplateMonomorphExperimentalCollectionTypeHelpers.h"
#include "TemplateMonomorphSourceDefinitionSetup.h"
#include "TemplateMonomorphExperimentalCollectionConstructorPaths.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/StdlibSurfaceRegistry.h"

#include <sstream>

#include "primec/support/CompileArena.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec {

bool isSoftwareNumericParamCompatible(semantics::ReturnKind expectedKind, semantics::ReturnKind actualKind) {
  auto isSoftwareIntegerKind = [](semantics::ReturnKind kind) {
    return kind == semantics::ReturnKind::Int || kind == semantics::ReturnKind::Int64 || kind == semantics::ReturnKind::UInt64 ||
           kind == semantics::ReturnKind::Bool || kind == semantics::ReturnKind::Integer;
  };
  auto isSoftwareDecimalKind = [&](semantics::ReturnKind kind) {
    return isSoftwareIntegerKind(kind) || kind == semantics::ReturnKind::Float32 || kind == semantics::ReturnKind::Float64 ||
           kind == semantics::ReturnKind::Decimal;
  };
  switch (expectedKind) {
    case semantics::ReturnKind::Int:
    case semantics::ReturnKind::Int64:
    case semantics::ReturnKind::UInt64:
    case semantics::ReturnKind::Integer:
      return isSoftwareIntegerKind(actualKind);
    case semantics::ReturnKind::Float32:
    case semantics::ReturnKind::Float64:
    case semantics::ReturnKind::Decimal:
      return isSoftwareDecimalKind(actualKind);
    case semantics::ReturnKind::Complex:
      return isSoftwareDecimalKind(actualKind) ||
             actualKind == semantics::ReturnKind::Complex;
    default:
      return false;
  }
}

std::string resolveStructLikeTypePathForTemplatedVectorFallback(const std::string &typeName,
                                                                const std::string &namespacePrefix,
                                                                const Context &ctx) {
  std::string normalized = semantics::normalizeBindingTypeName(typeName);
  if (normalized.empty()) {
    return {};
  }
  std::string base;
  std::string argText;
  if (semantics::splitTemplateTypeName(normalized, base, argText) && !base.empty()) {
    normalized = base;
  }
  if (semantics::isPrimitiveBindingTypeName(normalized) || semantics::isSoftwareNumericTypeName(normalized) || normalized == "string" ||
      isBuiltinTemplateContainer(normalized)) {
    return {};
  }
  if (!normalized.empty() && normalized[0] == '/') {
    return ctx.sourceDefs.count(normalized) > 0 ? normalized : std::string{};
  }
  if (const std::string *importAlias =
          lookupScopedImportAliasForNamespace(normalized, namespacePrefix, ctx);
      importAlias != nullptr && ctx.sourceDefs.count(*importAlias) > 0) {
    return *importAlias;
  }
  std::string resolved = semantics::resolveTypePath(normalized, namespacePrefix);
  if (ctx.sourceDefs.count(resolved) > 0) {
    return resolved;
  }
  return {};
}

std::string resolveStructLikeExprPathForTemplatedVectorFallback(const Expr &expr,
                                                                const LocalTypeMap &locals,
                                                                const std::string &namespacePrefix,
                                                                const Context &ctx,
                                                                bool allowMathBare) {
  if (expr.kind != Expr::Kind::Call || expr.isBinding) {
    return {};
  }
  std::string resolved;
  if (expr.isMethodCall) {
    if (!resolveMethodCallTemplateTarget(expr, locals, ctx, resolved)) {
      return {};
    }
  } else {
    resolved = resolveCalleePath(expr, namespacePrefix, ctx);
  }
  if (!expr.isMethodCall && expr.templateArgs.size() == 1) {
    const std::string experimentalPath = experimentalVectorConstructorInferencePath(resolved);
    if (!experimentalPath.empty() && ctx.sourceDefs.count(experimentalPath) > 0) {
      return semantics::legacyExperimentalVectorCompatibilityTypeText(semantics::joinTemplateArgs(expr.templateArgs));
    }
    if (isCollectionVectorConstructorHelperPath(resolved)) {
      return semantics::legacyExperimentalVectorCompatibilityTypeText(semantics::joinTemplateArgs(expr.templateArgs));
    }
  }
  if (!expr.isMethodCall && expr.templateArgs.empty()) {
    const std::string experimentalVectorPath = experimentalVectorConstructorInferencePath(resolved);
    if (!experimentalVectorPath.empty() && ctx.sourceDefs.count(experimentalVectorPath) > 0) {
      const auto defIt = ctx.sourceDefs.find(resolved);
      if (defIt != ctx.sourceDefs.end()) {
        std::vector<std::string> inferredArgs;
        std::string inferError;
        if (inferImplicitTemplateArgs(defIt->second,
                                      expr,
                                      locals,
                                      {},
                                      SubstMap{},
                                      {},
                                      namespacePrefix,
                                      const_cast<Context &>(ctx),
                                      allowMathBare,
                                      inferredArgs,
                                      inferError) &&
            inferredArgs.size() == 1) {
          return semantics::legacyExperimentalVectorCompatibilityTypeText(semantics::joinTemplateArgs(inferredArgs));
        }
      }
    }
    if (isCollectionVectorConstructorHelperPath(resolved)) {
      const auto defIt = ctx.sourceDefs.find(resolved);
      if (defIt != ctx.sourceDefs.end()) {
        std::vector<std::string> inferredArgs;
        std::string inferError;
        if (inferImplicitTemplateArgs(defIt->second,
                                      expr,
                                      locals,
                                      {},
                                      SubstMap{},
                                      {},
                                      namespacePrefix,
                                      const_cast<Context &>(ctx),
                                      allowMathBare,
                                      inferredArgs,
                                      inferError) &&
            inferredArgs.size() == 1) {
          return semantics::legacyExperimentalVectorCompatibilityTypeText(semantics::joinTemplateArgs(inferredArgs));
        }
      }
    }
  }
  const auto defIt = ctx.sourceDefs.find(resolved);
  if (defIt == ctx.sourceDefs.end()) {
    return {};
  }
  if (!expr.isMethodCall) {
    const std::string experimentalVectorPath = experimentalVectorConstructorInferencePath(resolved);
    if (!experimentalVectorPath.empty() && ctx.sourceDefs.count(experimentalVectorPath) > 0) {
      for (const auto &transform : defIt->second.transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        std::string valueType;
        if (extractCollectionVectorValueTypeFromTypeText(transform.templateArgs.front(), valueType)) {
          return semantics::legacyExperimentalVectorCompatibilityTypeText(valueType);
        }
      }
    }
  }
  if (isStructDefinition(defIt->second)) {
    return resolved;
  }
  for (const auto &transform : defIt->second.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &returnType = transform.templateArgs.front();
    if (returnType == "auto") {
      continue;
    }
    return resolveStructLikeTypePathForTemplatedVectorFallback(returnType, defIt->second.namespacePrefix, ctx);
  }
  return {};
}

bool isUnspecializedExperimentalKeyValueBackingTypeForFallbackInference(
    std::string typeName) {
  typeName = semantics::normalizeBindingTypeName(std::move(typeName));
  return isUnspecializedExperimentalKeyValueBackingTypeName(typeName);
}

bool isSpecializedExperimentalKeyValueBackingTypeForFallbackInference(
    std::string typeName) {
  typeName = semantics::normalizeBindingTypeName(std::move(typeName));
  return isQualifiedExperimentalKeyValueBackingTypeName(typeName);
}

bool resolvesExperimentalKeyValueTypeText(const std::string &typeText,
                                          const SubstMap &mapping,
                                          const std::unordered_set<std::string> &allowedParams,
                                          const std::string &namespacePrefix,
                                          Context &ctx) {
  if (typeText.empty()) {
    return false;
  }
  std::string normalizedInput = semantics::normalizeBindingTypeName(typeText);
  std::string inputBase;
  std::string inputArgText;
  if (semantics::splitTemplateTypeName(normalizedInput, inputBase, inputArgText)) {
    std::string normalizedInputBase = semantics::normalizeBindingTypeName(inputBase);
    if (!normalizedInputBase.empty() && normalizedInputBase.front() == '/') {
      normalizedInputBase.erase(normalizedInputBase.begin());
    }
    if (!isUnspecializedExperimentalKeyValueBackingTypeForFallbackInference(
            normalizedInputBase)) {
      return false;
    }
  } else {
    if (!normalizedInput.empty() && normalizedInput.front() == '/') {
      normalizedInput.erase(normalizedInput.begin());
    }
    if (!isSpecializedExperimentalKeyValueBackingTypeForFallbackInference(
            normalizedInput)) {
      return false;
    }
  }
  std::string localError;
  ResolvedType resolvedType = resolveTypeString(typeText, mapping, allowedParams, namespacePrefix, ctx, localError);
  if (!localError.empty()) {
    return false;
  }
  std::string normalized = semantics::normalizeBindingTypeName(resolvedType.text);
  std::string base;
  std::string argText;
  if (semantics::splitTemplateTypeName(normalized, base, argText)) {
    std::string normalizedBase = semantics::normalizeBindingTypeName(base);
    if (!normalizedBase.empty() && normalizedBase.front() == '/') {
      normalizedBase.erase(normalizedBase.begin());
    }
    if (isUnspecializedExperimentalKeyValueBackingTypeForFallbackInference(
            normalizedBase)) {
      std::vector<std::string> args;
      return semantics::splitTopLevelTemplateArgs(argText, args) && args.size() == 2;
    }
  }
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return isSpecializedExperimentalKeyValueBackingTypeForFallbackInference(normalized);
}

void populateTemplatedFallbackQueryStateAdapterFromQueryTypeText(
    const std::string &queryTypeText,
    TemplatedFallbackQueryStateAdapterData &out) {
  out.hasResultType = false;
  out.resultTypeHasValue = false;
  out.resultValueType.clear();
  out.resultErrorType.clear();
  out.mismatchDiagnostic.clear();

  const auto isResultQueryTypeBase = [](std::string typeText) {
    typeText = semantics::normalizeBindingTypeName(typeText);
    if (!typeText.empty() && typeText.front() == '/') {
      typeText.erase(typeText.begin());
    }
    return typeText == "Result" || typeText == "std/result/Result";
  };
  std::string normalizedQueryType = semantics::normalizeBindingTypeName(queryTypeText);
  std::string resultBase;
  std::string resultArgText;
  if (!semantics::splitTemplateTypeName(normalizedQueryType, resultBase, resultArgText)) {
    if (isResultQueryTypeBase(normalizedQueryType)) {
      out.mismatchDiagnostic = "result query type missing template arguments: " + queryTypeText;
    }
    return;
  }

  if (!isResultQueryTypeBase(resultBase)) {
    return;
  }

  std::vector<std::string> resultArgs;
  if (!semantics::splitTopLevelTemplateArgs(resultArgText, resultArgs) || resultArgs.empty() || resultArgs.size() > 2) {
    out.mismatchDiagnostic = "invalid Result query type envelope: " + queryTypeText;
    return;
  }

  out.hasResultType = true;
  if (resultArgs.size() == 2) {
    out.resultTypeHasValue = true;
    out.resultValueType = semantics::normalizeBindingTypeName(resultArgs.front());
    out.resultErrorType = semantics::normalizeBindingTypeName(resultArgs.back());
  } else {
    out.resultTypeHasValue = false;
    out.resultErrorType = semantics::normalizeBindingTypeName(resultArgs.front());
  }
}

bool inferDefinitionReturnBindingForTemplatedFallback(const Definition &def,
                                                      bool allowMathBare,
                                                      Context &ctx,
                                                      semantics::BindingInfo &infoOut) {
  if (!def.templateArgs.empty()) {
    return false;
  }
  if (!ctx.returnInferenceStack.insert(def.fullPath).second) {
    return false;
  }
  struct InferenceScopeGuard {
    std::unordered_set<std::string> &stack;
    std::string fullPath;
    ~InferenceScopeGuard() { stack.erase(fullPath); }
  } inferenceScopeGuard{ctx.returnInferenceStack, def.fullPath};

  std::vector<semantics::ParameterInfo> defParams;
  defParams.reserve(def.parameters.size());
  for (const auto &paramExpr : def.parameters) {
    semantics::ParameterInfo paramInfo;
    paramInfo.name = paramExpr.name;
    extractExplicitBindingType(paramExpr, paramInfo.binding);
    if (paramExpr.args.size() == 1) {
      paramInfo.defaultExpr = &paramExpr.args.front();
    }
    defParams.push_back(std::move(paramInfo));
  }

  LocalTypeMap locals;
  const Expr *valueExpr = nullptr;
  bool sawReturn = false;
  for (const auto &stmt : def.statements) {
    if (stmt.isBinding) {
      semantics::BindingInfo binding;
      if (extractExplicitBindingType(stmt, binding)) {
        if (binding.typeName == "auto" && stmt.args.size() == 1 &&
            inferBindingTypeForMonomorph(stmt.args.front(), defParams, locals, allowMathBare, ctx, binding)) {
          locals[stmt.name] = binding;
        } else {
          locals[stmt.name] = binding;
        }
      } else if (stmt.args.size() == 1 &&
                 inferBindingTypeForMonomorph(stmt.args.front(), defParams, locals, allowMathBare, ctx, binding)) {
        locals[stmt.name] = binding;
      }
      continue;
    }
    if (semantics::isReturnCall(stmt)) {
      if (stmt.args.size() != 1) {
        return false;
      }
      valueExpr = &stmt.args.front();
      sawReturn = true;
      continue;
    }
    if (!sawReturn) {
      valueExpr = &stmt;
    }
  }
  if (def.returnExpr.has_value()) {
    valueExpr = &*def.returnExpr;
  }
  if (valueExpr == nullptr) {
    return false;
  }
  return inferBindingTypeForMonomorph(*valueExpr, defParams, locals, allowMathBare, ctx, infoOut);
}

std::string inferExprTypeTextForTemplatedVectorFallback(const Expr &expr,
                                                        const LocalTypeMap &locals,
                                                        const std::string &namespacePrefix,
                                                        const Context &ctx,
                                                        bool allowMathBare) {
  if (expr.kind != Expr::Kind::Call || expr.isBinding) {
    return {};
  }
  std::string builtinCollection;
  if (semantics::getBuiltinCollectionName(expr, builtinCollection)) {
    const std::string keyValueCollectionAlias = semantics::mapCollectionAliasToken();
    if ((builtinCollection == "array" || builtinCollection == "vector" ||
         isTemplateMonomorphSoaReceiverType(builtinCollection)) &&
        expr.templateArgs.size() == 1) {
      return builtinCollection + "<" + expr.templateArgs.front() + ">";
    }
    if (!keyValueCollectionAlias.empty() &&
        builtinCollection == keyValueCollectionAlias &&
        expr.templateArgs.size() == 2) {
      return keyValueCollectionAlias + "<" + expr.templateArgs.front() + ", " +
             expr.templateArgs[1] + ">";
    }
  }
  if (!expr.isBinding && expr.args.size() == 1 &&
      (semantics::isSimpleCallName(expr, "count") || semantics::isSimpleCallName(expr, "capacity"))) {
    return "i32";
  }
  std::string resolved;
  if (expr.isMethodCall) {
    if (!resolveMethodCallTemplateTarget(expr, locals, ctx, resolved)) {
      return {};
    }
  } else {
    resolved = resolveCalleePath(expr, namespacePrefix, ctx);
  }
  const auto defIt = ctx.sourceDefs.find(resolved);
  if (defIt == ctx.sourceDefs.end()) {
    return {};
  }
  if (isStructDefinition(defIt->second)) {
    return resolved;
  }
  const Definition &resolvedDef = defIt->second;
  std::unordered_set<std::string> allowedParams(resolvedDef.templateArgs.begin(),
                                                resolvedDef.templateArgs.end());
  SubstMap returnTypeMapping;
  if (expr.templateArgs.size() == resolvedDef.templateArgs.size()) {
    returnTypeMapping.reserve(expr.templateArgs.size());
    for (size_t i = 0; i < expr.templateArgs.size(); ++i) {
      returnTypeMapping.emplace(resolvedDef.templateArgs[i], expr.templateArgs[i]);
    }
  }
  for (const auto &transform : defIt->second.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &returnType = transform.templateArgs.front();
    if (returnType == "auto") {
      continue;
    }
    std::string resolvedError;
    ResolvedType resolvedReturnType =
        resolveTypeString(returnType, returnTypeMapping, allowedParams, resolvedDef.namespacePrefix,
                          const_cast<Context &>(ctx), resolvedError);
    if (!resolvedError.empty() || !resolvedReturnType.concrete || resolvedReturnType.text.empty()) {
      continue;
    }
    return resolvedReturnType.text;
  }
  semantics::BindingInfo inferredReturn;
  Context &mutableCtx = const_cast<Context &>(ctx);
  if (inferDefinitionReturnBindingForTemplatedFallback(defIt->second, allowMathBare, mutableCtx, inferredReturn)) {
    return bindingTypeToString(inferredReturn);
  }
  return {};
}

bool inferTemplatedFallbackQueryStateAdapter(const Expr &expr,
                                             const LocalTypeMap &locals,
                                             const std::vector<semantics::ParameterInfo> &params,
                                             const std::string &namespacePrefix,
                                             Context &ctx,
                                             bool allowMathBare,
                                             TemplatedFallbackQueryStateAdapterData &out) {
  out = {};
  out.queryTypeText =
      inferExprTypeTextForTemplatedVectorFallback(expr, locals, namespacePrefix, ctx, allowMathBare);
  if (out.queryTypeText.empty()) {
    return false;
  }

  if (expr.kind == Expr::Kind::Call && expr.isMethodCall && !expr.args.empty()) {
    semantics::BindingInfo receiverBinding;
    if (inferBindingTypeForMonomorph(expr.args.front(), params, locals, allowMathBare, ctx, receiverBinding) &&
        !receiverBinding.typeName.empty()) {
      out.receiverBinding = std::move(receiverBinding);
    }
  }
  populateTemplatedFallbackQueryStateAdapterFromQueryTypeText(out.queryTypeText, out);
  return true;
}

bool shouldPreferTemplatedVectorFallbackForTypeMismatch(const Definition &def,
                                                        const Expr &expr,
                                                        const LocalTypeMap &locals,
                                                        const std::vector<semantics::ParameterInfo> &params,
                                                        bool allowMathBare,
                                                        Context &ctx,
                                                        const std::string &namespacePrefix) {
  auto isTemplateParamName = [&](const std::string &name) {
    for (const auto &templateArg : def.templateArgs) {
      if (templateArg == name) {
        return true;
      }
    }
    return false;
  };
  const std::string keyValueCollectionAlias = semantics::mapCollectionAliasToken();
  auto isCollectionEnvelopeBase = [&](const std::string &base) {
    return base == "array" || base == "vector" ||
           (!keyValueCollectionAlias.empty() &&
            base == keyValueCollectionAlias) ||
           isTemplateMonomorphSoaReceiverType(base);
  };
  auto hasUnknownEnvelopeMismatch = [&](const std::string &normalizedExpected,
                                        const std::string &normalizedActual) {
    if (normalizedExpected == normalizedActual) {
      return false;
    }
    std::string expectedBase;
    std::string expectedArgText;
    std::string actualBase;
    std::string actualArgText;
    const bool expectedIsTemplate = semantics::splitTemplateTypeName(normalizedExpected, expectedBase, expectedArgText);
    const bool actualIsTemplate = semantics::splitTemplateTypeName(normalizedActual, actualBase, actualArgText);
    if (expectedIsTemplate && actualIsTemplate) {
      const std::string normalizedExpectedBase = semantics::normalizeBindingTypeName(expectedBase);
      const std::string normalizedActualBase = semantics::normalizeBindingTypeName(actualBase);
      if (normalizedExpectedBase == normalizedActualBase) {
        return true;
      }
      return isCollectionEnvelopeBase(normalizedExpectedBase) || isCollectionEnvelopeBase(normalizedActualBase);
    }
    if (expectedIsTemplate == actualIsTemplate) {
      return false;
    }
    const std::string &nonTemplateText = expectedIsTemplate ? normalizedActual : normalizedExpected;
    if (isTemplateParamName(nonTemplateText)) {
      return false;
    }
    const std::string templateBase = semantics::normalizeBindingTypeName(expectedIsTemplate ? expectedBase : actualBase);
    return isCollectionEnvelopeBase(templateBase);
  };
  std::vector<semantics::ParameterInfo> callParams;
  callParams.reserve(def.parameters.size());
  for (const auto &paramExpr : def.parameters) {
    semantics::ParameterInfo param;
    param.name = paramExpr.name;
    extractExplicitBindingType(paramExpr, param.binding);
    if (paramExpr.args.size() == 1) {
      param.defaultExpr = &paramExpr.args.front();
    }
    callParams.push_back(std::move(param));
  }
  std::vector<const Expr *> ordered;
  std::string orderError;
  if (!semantics::buildOrderedArguments(callParams, expr.args, expr.argNames, ordered, orderError)) {
    return false;
  }
  std::unordered_set<const Expr *> explicitArgs;
  explicitArgs.reserve(expr.args.size());
  for (const auto &arg : expr.args) {
    explicitArgs.insert(&arg);
  }
  for (size_t i = 0; i < callParams.size(); ++i) {
    const auto &param = callParams[i];
    if (param.binding.typeName.empty() || !ordered[i]) {
      continue;
    }
    if (explicitArgs.count(ordered[i]) == 0) {
      continue;
    }
    semantics::BindingInfo actual;
    if (!inferBindingTypeForMonomorph(*ordered[i], params, locals, allowMathBare, ctx, actual)) {
      const std::string expectedTypeText = bindingTypeToString(param.binding);
      const std::string inferredActualTypeText =
          inferExprTypeTextForTemplatedVectorFallback(*ordered[i], locals, namespacePrefix, ctx, allowMathBare);
      if (!expectedTypeText.empty() && !inferredActualTypeText.empty()) {
        const std::string normalizedExpected = semantics::normalizeBindingTypeName(expectedTypeText);
        const std::string normalizedActual = semantics::normalizeBindingTypeName(inferredActualTypeText);
        if (normalizedExpected == "string" && normalizedActual != "string") {
          return true;
        }
        if (normalizedExpected != "string" && normalizedActual == "string") {
          return true;
        }
        const semantics::ReturnKind expectedKind = semantics::returnKindForTypeName(normalizedExpected);
        const semantics::ReturnKind actualKind = semantics::returnKindForTypeName(normalizedActual);
        if (expectedKind != semantics::ReturnKind::Unknown && actualKind != semantics::ReturnKind::Unknown) {
          if (!isSoftwareNumericParamCompatible(expectedKind, actualKind)) {
            if (expectedKind == actualKind && expectedKind == semantics::ReturnKind::Array &&
                normalizedExpected != normalizedActual) {
              return true;
            }
            if (expectedKind != actualKind) {
              return true;
            }
          }
        } else if (expectedKind != actualKind) {
          if (normalizedExpected != normalizedActual) {
            return true;
          }
        } else if (hasUnknownEnvelopeMismatch(normalizedExpected, normalizedActual)) {
          return true;
        }
      }
      const std::string expectedStructPath =
          resolveStructLikeTypePathForTemplatedVectorFallback(param.binding.typeName, def.namespacePrefix, ctx);
      if (expectedStructPath.empty()) {
        continue;
      }
      const std::string actualStructPath =
          resolveStructLikeExprPathForTemplatedVectorFallback(*ordered[i], locals, namespacePrefix, ctx, allowMathBare);
      if (!actualStructPath.empty() && actualStructPath != expectedStructPath) {
        return true;
      }
      continue;
    }
    const std::string expectedTypeText = bindingTypeToString(param.binding);
    const std::string actualTypeText = bindingTypeToString(actual);
    const std::string normalizedExpected = semantics::normalizeBindingTypeName(expectedTypeText);
    const std::string normalizedActual = semantics::normalizeBindingTypeName(actualTypeText);
    if (normalizedExpected == "string" && normalizedActual != "string") {
      return true;
    }
    if (normalizedExpected != "string" && normalizedActual == "string") {
      return true;
    }
    const semantics::ReturnKind expectedKind = semantics::returnKindForTypeName(normalizedExpected);
    const semantics::ReturnKind actualKind = semantics::returnKindForTypeName(normalizedActual);
    if (expectedKind == semantics::ReturnKind::Unknown || actualKind == semantics::ReturnKind::Unknown) {
      if (expectedKind == semantics::ReturnKind::Unknown && actualKind == semantics::ReturnKind::Unknown) {
        const std::string expectedStructPath =
            resolveStructLikeTypePathForTemplatedVectorFallback(param.binding.typeName, def.namespacePrefix, ctx);
        const std::string actualStructPath =
            resolveStructLikeTypePathForTemplatedVectorFallback(actual.typeName, namespacePrefix, ctx);
        if (!expectedStructPath.empty() && !actualStructPath.empty() && expectedStructPath != actualStructPath) {
          return true;
        }
        if (hasUnknownEnvelopeMismatch(normalizedExpected, normalizedActual)) {
          return true;
        }
      } else if (expectedKind != actualKind) {
        if (normalizedExpected != normalizedActual) {
          return true;
        }
      }
      continue;
    }
    if (isSoftwareNumericParamCompatible(expectedKind, actualKind)) {
      continue;
    }
    if (expectedKind == actualKind && expectedKind == semantics::ReturnKind::Array && normalizedExpected != normalizedActual) {
      return true;
    }
    if (actualKind != expectedKind) {
      return true;
    }
  }
  return false;
}

std::string preferVectorStdlibImplicitTemplatePath(const Expr &expr,
                                                   const std::string &path,
                                                   const LocalTypeMap &locals,
                                                   const std::vector<semantics::ParameterInfo> &params,
                                                   bool allowMathBare,
                                                   Context &ctx,
                                                   const std::string &namespacePrefix) {
  if (!expr.templateArgs.empty()) {
    return path;
  }
  const auto defIt = ctx.sourceDefs.find(path);
  if (defIt == ctx.sourceDefs.end() || ctx.templateDefs.count(path) > 0) {
    return path;
  }
  const std::string pathCanonical = semantics::canonicalizeLegacySoaGetHelperPath(path);
  if (semantics::isLegacyOrCanonicalSoaHelperPath(pathCanonical, collection_helpers::kCountRef) ||
      semantics::isLegacyOrCanonicalSoaHelperPath(pathCanonical, collection_helpers::kGetRef) ||
      semantics::isCanonicalSoaRefLikeHelperPath(pathCanonical)) {
    return path;
  }
  const bool preserveCompatibilityTemplatePath = isCollectionCompatibilityTemplateFallbackPath(path);
  const bool acceptsCallShape = definitionAcceptsCallShape(defIt->second, expr);
  if (!acceptsCallShape && preserveCompatibilityTemplatePath &&
      (hasNamedCallArguments(expr) || definitionHasArgumentCountMismatch(defIt->second, expr))) {
    // Keep diagnostics on explicit compatibility helpers when named arguments
    // or argument counts do not match.
    return path;
  }
  const bool prefersTypeMismatchFallback = shouldPreferTemplatedVectorFallbackForTypeMismatch(
      defIt->second, expr, locals, params, allowMathBare, ctx, namespacePrefix);
  if (preserveCompatibilityTemplatePath && prefersTypeMismatchFallback) {
    // Keep diagnostics on explicit compatibility helpers when argument types
    // mismatch the declared helper shape.
    return path;
  }
  std::string pathBase = path;
  if (const size_t specializationSuffix = pathBase.find("__");
      specializationSuffix != std::string::npos) {
    pathBase.erase(specializationSuffix);
  }
  const std::string publicSoaPrefix = templateMonomorphPublicSoaHelperPrefix();
  if (pathBase.rfind(publicSoaPrefix, 0) == 0 &&
      (pathBase == semantics::publicSoaHelperTargetPath("push") ||
       pathBase == semantics::publicSoaHelperTargetPath("reserve")) &&
      !expr.args.empty()) {
    auto inferFirstArgFamily = [&]() -> std::string {
      semantics::BindingInfo receiverBinding;
      std::string receiverTypeText;
      if (inferBindingTypeForMonomorph(expr.args.front(), params, locals,
                                       allowMathBare, ctx, receiverBinding)) {
        receiverTypeText = bindingTypeToString(receiverBinding);
      }
      if (receiverTypeText.empty()) {
        receiverTypeText = inferExprTypeTextForTemplatedVectorFallback(
            expr.args.front(), locals, namespacePrefix, ctx, allowMathBare);
      }
      receiverTypeText = semantics::normalizeBindingTypeName(receiverTypeText);
      std::string base;
      std::string argText;
      if (semantics::splitTemplateTypeName(receiverTypeText, base, argText) &&
          !base.empty()) {
        receiverTypeText = semantics::normalizeBindingTypeName(base);
      }
      return normalizeCollectionReceiverTypeName(receiverTypeText);
    };
    if (inferFirstArgFamily() == "vector") {
      const std::string helperName =
          pathBase.substr(publicSoaPrefix.size());
      const std::string vectorPath =
          semantics::canonicalVectorCompatibilityHelperPathOrFallback(helperName);
      if (ctx.sourceDefs.count(vectorPath) > 0 &&
          ctx.templateDefs.count(vectorPath) > 0) {
        return vectorPath;
      }
    }
  }
  const std::string preferred = preferVectorStdlibTemplatePath(path, ctx);
  if (acceptsCallShape && !prefersTypeMismatchFallback) {
    return path;
  }
  if (preferred != path && ctx.sourceDefs.count(preferred) > 0 && ctx.templateDefs.count(preferred) > 0) {
    return preferred;
  }
  return path;
}

} // namespace primec
