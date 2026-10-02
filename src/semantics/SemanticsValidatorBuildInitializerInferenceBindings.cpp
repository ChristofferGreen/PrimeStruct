#include "SemanticsValidator.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/CollectionHelperNames.h"

#include <algorithm>
#include <limits>
#include "SemanticsValidatorBuildInitializerInferenceHelpers.h"

namespace primec::semantics {
using namespace buildInitializerHelpers;

bool SemanticsValidator::canonicalizeInferredCollectionBinding(
    const Expr *sourceExpr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    BindingInfo &bindingOut) {
  auto canonicalizeResolvedPath = [](std::string path) {
    const size_t suffix = path.find("__t");
    if (suffix != std::string::npos) {
      path.erase(suffix);
    }
    return path;
  };
  auto preferResolvedCollectionBinding = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    std::string builtinCollectionName;
    if (getBuiltinCollectionName(candidate, builtinCollectionName)) {
      return false;
    }
    std::string resolvedCandidate = preferredCollectionHelperResolvedPath(candidate);
    if (resolvedCandidate.empty()) {
      resolvedCandidate = resolveCalleePath(candidate);
    }
    resolvedCandidate = canonicalizeResolvedPath(std::move(resolvedCandidate));
    if (!resolvedCandidate.empty()) {
      const std::string concreteResolvedCandidate =
          resolveExprConcreteCallPath(params, locals, candidate, resolvedCandidate);
      if (!concreteResolvedCandidate.empty()) {
        resolvedCandidate = canonicalizeResolvedPath(concreteResolvedCandidate);
      }
    }
    if (resolvedCandidate.empty()) {
      return false;
    }
    BindingInfo resolvedBinding;
    if (!inferResolvedDirectCallBindingType(resolvedCandidate, resolvedBinding)) {
      return false;
    }
    if (normalizeCollectionTypePath(bindingTypeText(resolvedBinding)).empty()) {
      return false;
    }
    bindingOut = std::move(resolvedBinding);
    return true;
  };
  auto inferDirectMapConstructorBinding = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    std::string resolvedCandidate = preferredCollectionHelperResolvedPath(candidate);
    if (resolvedCandidate.empty()) {
      resolvedCandidate = resolveCalleePath(candidate);
    }
    if (!resolvedCandidate.empty()) {
      const std::string concreteResolvedCandidate =
          resolveExprConcreteCallPath(params, locals, candidate, resolvedCandidate);
      if (!concreteResolvedCandidate.empty()) {
        resolvedCandidate = concreteResolvedCandidate;
      }
    }
    if (!isResolvedKeyValueConstructorPath(resolvedCandidate)) {
      return false;
    }
    if (candidate.templateArgs.size() == 2) {
      bindingOut.typeName = "map";
      bindingOut.isInferredKeyValueConstructorResult = true;
      bindingOut.typeTemplateArg = joinTemplateArgs(candidate.templateArgs);
      return true;
    }
    if (candidate.args.size() < 2 || candidate.args.size() % 2 != 0) {
      return false;
    }
    {
      // Entry-pack constructor arguments are not key/value pairs; bail so
      // the declared binding type or callee return type drives inference.
      const StdlibSurfaceMetadata *entryMetadata =
          keyValueHelperSurfaceMetadataLocal();
      const auto isEntryConstructorArg = [&](const Expr &argExpr) {
        return entryMetadata != nullptr && argExpr.kind == Expr::Kind::Call &&
               !argExpr.isMethodCall && !argExpr.name.empty() &&
               resolveStdlibSurfaceMemberName(*entryMetadata, argExpr.name) ==
                   "entry";
      };
      if (std::all_of(candidate.args.begin(), candidate.args.end(),
                      isEntryConstructorArg)) {
        const Expr &firstEntry = candidate.args.front();
        if (firstEntry.templateArgs.size() == 2) {
          bindingOut.typeName = "map";
          bindingOut.isInferredKeyValueConstructorResult = true;
          bindingOut.typeTemplateArg = joinTemplateArgs(firstEntry.templateArgs);
          return true;
        }
        std::string entryKeyType;
        std::string entryValueType;
        if (deriveKeyValueTypesFromEntryPackCall(candidate, entryKeyType,
                                                 entryValueType)) {
          bindingOut.typeName = "map";
          bindingOut.isInferredKeyValueConstructorResult = true;
          bindingOut.typeTemplateArg = entryKeyType + ", " + entryValueType;
          return true;
        }
        return false;
      }
    }
    auto inferArgumentTypeText = [&](const Expr &argument, std::string &typeTextOut) -> bool {
      typeTextOut.clear();
      if (inferQueryExprTypeText(argument, params, locals, typeTextOut) && !typeTextOut.empty()) {
        typeTextOut = normalizeBindingTypeName(typeTextOut);
        return true;
      }
      const ReturnKind argumentKind = inferExprReturnKind(argument, params, locals);
      if (argumentKind == ReturnKind::Unknown || argumentKind == ReturnKind::Void) {
        return false;
      }
      typeTextOut = typeNameForReturnKind(argumentKind);
      return !typeTextOut.empty();
    };
    std::string keyTypeText;
    std::string valueTypeText;
    if (!inferArgumentTypeText(candidate.args[0], keyTypeText) ||
        !inferArgumentTypeText(candidate.args[1], valueTypeText)) {
      return false;
    }
    bindingOut.typeName = "map";
    bindingOut.isInferredKeyValueConstructorResult = true;
    bindingOut.typeTemplateArg = keyTypeText + ", " + valueTypeText;
    return true;
  };
  auto applySourceExprCollectionTemplateArgs = [&](const Expr &candidate) -> bool {
    std::vector<std::string> collectionArgs;
    if (resolveCallCollectionTemplateArgs(candidate,
                                         bindingOut.typeName,
                                         params,
                                         locals,
                                         collectionArgs) &&
        !collectionArgs.empty()) {
      bindingOut.typeTemplateArg = joinTemplateArgs(collectionArgs);
      return true;
    }
    std::string collectionName;
    if (!getBuiltinCollectionName(candidate, collectionName)) {
      return false;
    }
    if ((collectionName == "array" || collectionName == "vector" || collectionName == "soa") &&
        candidate.templateArgs.size() == 1) {
      bindingOut.typeName = collectionName;
      bindingOut.typeTemplateArg = candidate.templateArgs.front();
      return true;
    }
    if (collectionName == "map" && candidate.templateArgs.size() == 2) {
      bindingOut.typeName = "map";
      bindingOut.typeTemplateArg = joinTemplateArgs(candidate.templateArgs);
      return true;
    }
    return false;
  };
  auto assignBindingTypeFromText = [&](const std::string &typeText) -> bool {
    const std::string normalizedType = normalizeBindingTypeName(typeText);
    if (normalizedType.empty()) {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText)) {
      bindingOut.typeName = normalizeBindingTypeName(base);
      bindingOut.typeTemplateArg = argText;
      return true;
    }
    bindingOut.typeName = normalizedType;
    bindingOut.typeTemplateArg.clear();
    return true;
  };

  if (sourceExpr != nullptr && sourceExpr->kind == Expr::Kind::Call &&
      inferDirectMapConstructorBinding(*sourceExpr)) {
    return true;
  }
  if (sourceExpr != nullptr && preferResolvedCollectionBinding(*sourceExpr)) {
    return true;
  }
  const std::string normalizedBindingType = normalizeBindingTypeName(bindingOut.typeName);
  if ((normalizedBindingType == "Vector" && !bindingOut.typeTemplateArg.empty()) ||
      isLegacyExperimentalVectorCompatibilityTypePath(normalizedBindingType) ||
      isLegacyExperimentalVectorCompatibilityTypePath("/" + normalizedBindingType)) {
    return true;
  }
  if ((normalizedBindingType == "map" &&
       bindingOut.isInferredKeyValueConstructorResult &&
       !bindingOut.typeTemplateArg.empty()) ||
      isSpecializedExperimentalKeyValueBackingPath(normalizedBindingType)) {
    return true;
  }
  const std::string normalizedCollectionType = normalizeCollectionTypePath(bindingTypeText(bindingOut));
  if (normalizedCollectionType.empty()) {
    return false;
  }
  if (sourceExpr != nullptr) {
    std::string inferredTypeText;
    if (inferQueryExprTypeText(*sourceExpr, params, locals, inferredTypeText) &&
        assignBindingTypeFromText(inferredTypeText)) {
      (void)applySourceExprCollectionTemplateArgs(*sourceExpr);
      return true;
    }
  }
  bindingOut.typeName = normalizedCollectionType.substr(1);
  bindingOut.typeTemplateArg.clear();
  if (sourceExpr != nullptr && sourceExpr->kind == Expr::Kind::Call) {
    if (applySourceExprCollectionTemplateArgs(*sourceExpr)) {
      return true;
    }
  }
  return true;
}

bool SemanticsValidator::inferBindingTypeFromInitializer(
    const Expr &initializer,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    BindingInfo &bindingOut,
    const Expr *bindingExpr) {
  const bool preservedIsMutable = bindingOut.isMutable;
  const bool preservedIsEntryArgString = bindingOut.isEntryArgString;
  auto preserveBindingQualifiers = [&]() -> bool {
    bindingOut.isMutable = bindingOut.isMutable || preservedIsMutable;
    bindingOut.isEntryArgString = bindingOut.isEntryArgString || preservedIsEntryArgString;
    return true;
  };
  // TODO-5210 (resolved): an if(cond, then(){...}, else(){...}) expression
  // used as a binding initializer (including a function's implicit or
  // explicit `return(if(...))` value, since inferDefinitionReturnBinding
  // falls back to this same function) was never recognized here - every
  // other branch below only matches specific call shapes (lambdas, sum
  // constructors, task spawn/wait, field access, ...), so an if-expression
  // fell through all of them and silently failed to infer, leaving
  // auto-typed bindings/returns built from an if/else with (e.g.) a
  // map<K,V> constructor in each branch untyped. Recurse into each
  // branch's block, find its value expression (an explicit return()'s
  // argument if present, matching real early-return semantics; otherwise
  // the last non-binding statement in the block, matching how
  // inferDefinitionReturnBinding treats a whole function body), infer
  // each branch's type via this same function, and require both branches
  // to agree before accepting the result - if either branch can't be
  // inferred, or they disagree, defer to the checks below exactly as
  // before (harmless no-op for every previously-passing case, since none
  // of them start with an if-call).
  if (isIfCall(initializer) && initializer.args.size() == 3) {
    auto blockValueExpr = [](const Expr &block) -> const Expr * {
      const Expr *lastValueExpr = nullptr;
      for (const auto &bodyStmt : block.bodyArguments) {
        if (bodyStmt.isBinding) {
          continue;
        }
        if (isReturnCall(bodyStmt)) {
          return bodyStmt.args.empty() ? nullptr : &bodyStmt.args.front();
        }
        lastValueExpr = &bodyStmt;
      }
      return lastValueExpr;
    };
    const Expr *thenValueExpr = blockValueExpr(initializer.args[1]);
    const Expr *elseValueExpr = blockValueExpr(initializer.args[2]);
    if (thenValueExpr != nullptr && elseValueExpr != nullptr) {
      BindingInfo thenBinding;
      BindingInfo elseBinding;
      if (inferBindingTypeFromInitializer(*thenValueExpr, params, locals, thenBinding) &&
          inferBindingTypeFromInitializer(*elseValueExpr, params, locals, elseBinding) &&
          !thenBinding.typeName.empty() &&
          thenBinding.typeName == elseBinding.typeName &&
          thenBinding.typeTemplateArg == elseBinding.typeTemplateArg &&
          thenBinding.isInferredKeyValueConstructorResult ==
              elseBinding.isInferredKeyValueConstructorResult) {
        bindingOut.typeName = thenBinding.typeName;
        bindingOut.typeTemplateArg = thenBinding.typeTemplateArg;
        bindingOut.isInferredKeyValueConstructorResult =
            thenBinding.isInferredKeyValueConstructorResult;
        return preserveBindingQualifiers();
      }
    }
  }
  auto assignBindingTypeFromText = [&](const std::string &typeText) -> bool {
    const std::string normalizedType = normalizeBindingTypeName(typeText);
    if (normalizedType.empty()) {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText)) {
      bindingOut.typeName = normalizeBindingTypeName(base);
      bindingOut.typeTemplateArg = argText;
      return true;
    }
    bindingOut.typeName = normalizedType;
    bindingOut.typeTemplateArg.clear();
    return true;
  };
  auto assignBindingTypeFromResultInfo = [&](const ResultTypeInfo &resultInfo) -> bool {
    if (!resultInfo.isResult || resultInfo.errorType.empty()) {
      return false;
    }
    bindingOut.typeName = "Result";
    if (!resultInfo.hasValue) {
      bindingOut.typeTemplateArg = resultInfo.errorType;
      return true;
    }
    if (resultInfo.valueType.empty()) {
      return false;
    }
    bindingOut.typeTemplateArg = resultInfo.valueType + ", " + resultInfo.errorType;
    return true;
  };
  if (initializer.isLambda) {
    bindingOut.typeName = "lambda";
    bindingOut.typeTemplateArg.clear();
    return preserveBindingQualifiers();
  }
  if (inferExplicitSumConstructorBinding(initializer, bindingOut)) {
    return preserveBindingQualifiers();
  }
  if (inferTaskSpawnBinding(initializer, params, locals, bindingOut) ||
      inferTaskWaitBinding(initializer, params, locals, bindingOut)) {
    return preserveBindingQualifiers();
  }
  if (initializer.kind == Expr::Kind::Call && initializer.isFieldAccess &&
      initializer.args.size() == 1) {
    BindingInfo fieldBinding;
    if (resolveStructFieldBinding(
            params, locals, initializer.args.front(), initializer.name, fieldBinding)) {
      bindingOut.typeName = fieldBinding.typeName;
      bindingOut.typeTemplateArg = fieldBinding.typeTemplateArg;
      return preserveBindingQualifiers();
    }
  }
  auto inferUninitializedTakeBorrowBinding = [&]() -> bool {
    if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall ||
        initializer.args.size() != 1 ||
        (!isSimpleCallName(initializer, "take") &&
         !isSimpleCallName(initializer, "borrow"))) {
      return false;
    }
    BindingInfo storageBinding;
    bool resolvedStorage = false;
    if (!resolveUninitializedStorageBinding(params,
                                           locals,
                                           initializer.args.front(),
                                           storageBinding,
                                           resolvedStorage) ||
        !resolvedStorage || storageBinding.typeName != "uninitialized" ||
        storageBinding.typeTemplateArg.empty()) {
      return false;
    }
    return assignBindingTypeFromText(storageBinding.typeTemplateArg);
  };
  if (bindingExpr != nullptr && !shouldBypassGraphBindingLookup(*bindingExpr) &&
      lookupGraphLocalAutoBinding(*bindingExpr, bindingOut)) {
    if (graphBindingIsUsable(bindingOut)) {
      return preserveBindingQualifiers();
    }
    bindingOut = {};
  }
  std::string resolvedInitializerPath =
      preferredCollectionHelperResolvedPath(initializer);
  if (resolvedInitializerPath.empty()) {
    resolvedInitializerPath = resolveCalleePath(initializer);
  }
  if (!resolvedInitializerPath.empty()) {
    const std::string concreteResolvedInitializerPath =
        resolveExprConcreteCallPath(params, locals, initializer, resolvedInitializerPath);
    if (!concreteResolvedInitializerPath.empty()) {
      resolvedInitializerPath = concreteResolvedInitializerPath;
    }
  }
  auto canonicalizeResolvedPath = [](std::string path) {
    const size_t suffix = path.find("__t");
    if (suffix != std::string::npos) {
      path.erase(suffix);
    }
    return path;
  };
  const std::string canonicalResolvedInitializerPath =
      canonicalizeResolvedPath(resolvedInitializerPath);
  const bool isBareImportedExperimentalVectorConstructor =
      initializer.name == "vector" &&
      initializer.namespacePrefix.empty() &&
      hasDirectExperimentalVectorImport();
  if (initializer.kind == Expr::Kind::Call &&
      !initializer.isMethodCall &&
      initializer.templateArgs.size() == 1 &&
      (isResolvedExperimentalVectorConstructorPath(canonicalResolvedInitializerPath) ||
       canonicalResolvedInitializerPath == collection_helpers::kRootedVector ||
       isBareImportedExperimentalVectorConstructor)) {
    if (canonicalResolvedInitializerPath == collection_helpers::kRootedVector &&
        !hasDirectExperimentalVectorImport()) {
      bindingOut.typeName = "vector";
      bindingOut.typeTemplateArg = initializer.templateArgs.front();
      return preserveBindingQualifiers();
    }
    bindingOut.typeName = "Vector";
    bindingOut.typeTemplateArg = initializer.templateArgs.front();
    return preserveBindingQualifiers();
  }
  auto inferTryInitializerBinding = [&]() -> bool {
    if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall || !isSimpleCallName(initializer, "try") ||
        initializer.args.size() != 1 || !initializer.templateArgs.empty() || initializer.hasBodyArguments ||
        !initializer.bodyArguments.empty()) {
      return false;
    }
    ResultTypeInfo resultInfo;
    if (!resolveResultTypeForExpr(initializer.args.front(), params, locals, resultInfo) || !resultInfo.isResult ||
        !resultInfo.hasValue || resultInfo.valueType.empty()) {
      return false;
    }
    return assignBindingTypeFromText(resultInfo.valueType);
  };
  if (inferUninitializedTakeBorrowBinding()) {
    return preserveBindingQualifiers();
  }
  auto inferDirectResultOkBinding = [&]() -> bool {
    if (initializer.kind != Expr::Kind::Call || !initializer.isMethodCall || initializer.name != "ok" ||
        initializer.templateArgs.size() != 0 || initializer.hasBodyArguments || !initializer.bodyArguments.empty()) {
      return false;
    }
    if (initializer.args.empty()) {
      return false;
    }
    const Expr &receiver = initializer.args.front();
    if (receiver.kind != Expr::Kind::Name || normalizeBindingTypeName(receiver.name) != "Result") {
      return false;
    }
    auto inferCurrentErrorType = [&]() -> std::string {
      if (currentValidationState_.context.resultType.has_value() &&
          currentValidationState_.context.resultType->isResult &&
          !currentValidationState_.context.resultType->errorType.empty()) {
        return currentValidationState_.context.resultType->errorType;
      }
      if (currentValidationState_.context.onError.has_value() &&
          !currentValidationState_.context.onError->errorType.empty()) {
        return currentValidationState_.context.onError->errorType;
      }
      return "_";
    };
    if (initializer.args.size() == 1) {
      bindingOut.typeName = "Result";
      bindingOut.typeTemplateArg = inferCurrentErrorType();
      return true;
    }
    if (initializer.args.size() != 2) {
      return false;
    }
    BindingInfo payloadBinding;
    if (!inferBindingTypeFromInitializer(initializer.args.back(), params, locals, payloadBinding)) {
      return false;
    }
    const std::string payloadTypeText = bindingTypeText(payloadBinding);
    if (payloadTypeText.empty()) {
      return false;
    }
    bindingOut.typeName = "Result";
    bindingOut.typeTemplateArg = payloadTypeText + ", " + inferCurrentErrorType();
    return true;
  };
  if (initializer.kind == Expr::Kind::Call) {
    if (inferBuiltinPointerBinding(initializer, params, locals, bindingOut)) {
      return preserveBindingQualifiers();
    }
    const BindingInfo previousBinding = bindingOut;
    bindingOut = {};
    if (inferCallInitializerBinding(initializer, params, locals, bindingOut, bindingExpr)) {
      (void)canonicalizeInferredCollectionBinding(&initializer, params, locals, bindingOut);
      if (!(bindingOut.typeName == "array" && bindingOut.typeTemplateArg.empty())) {
        return preserveBindingQualifiers();
      }
    }
    bindingOut = previousBinding;
  }
  if (tryInferBindingTypeFromInitializer(initializer, params, locals, bindingOut, hasAnyMathImport())) {
    (void)canonicalizeInferredCollectionBinding(&initializer, params, locals, bindingOut);
    if (!(bindingOut.typeName == "array" && bindingOut.typeTemplateArg.empty())) {
      return preserveBindingQualifiers();
    }
    if (inferCallInitializerBinding(initializer, params, locals, bindingOut, bindingExpr)) {
      (void)canonicalizeInferredCollectionBinding(&initializer, params, locals, bindingOut);
      return preserveBindingQualifiers();
    }
    if (inferBuiltinCollectionValueBinding(initializer, params, locals, bindingOut)) {
      return preserveBindingQualifiers();
    }
    if (inferBuiltinPointerBinding(initializer, params, locals, bindingOut)) {
      return preserveBindingQualifiers();
    }
    return preserveBindingQualifiers();
  }
  if (inferTryInitializerBinding()) {
    (void)canonicalizeInferredCollectionBinding(&initializer, params, locals, bindingOut);
    return preserveBindingQualifiers();
  }
  if (inferDirectResultOkBinding()) {
    return preserveBindingQualifiers();
  }
  ResultTypeInfo resultInfo;
  if (resolveResultTypeForExpr(initializer, params, locals, resultInfo) &&
      assignBindingTypeFromResultInfo(resultInfo)) {
    return preserveBindingQualifiers();
  }
  if (inferCallInitializerBinding(initializer, params, locals, bindingOut, bindingExpr)) {
    (void)canonicalizeInferredCollectionBinding(&initializer, params, locals, bindingOut);
    return preserveBindingQualifiers();
  }
  if (inferBuiltinCollectionValueBinding(initializer, params, locals, bindingOut)) {
    return preserveBindingQualifiers();
  }
  if (inferBuiltinPointerBinding(initializer, params, locals, bindingOut)) {
    return preserveBindingQualifiers();
  }
  // Fix (c): the primary defMap_-backed inferCallInitializerBinding chain
  // above fails for a rewritten entries-pack call whose specialized
  // definition (e.g. .../map__ov1__ta<hash>) was minted by TemplateMonomorph
  // *after* defMap_ was built, so it never finds an entry for it and
  // suffix-stripping canonicalization falls back to the generic,
  // unspecialized entries constructor (no concrete K/V to report). The
  // rewritten call's *shape* alone - its args are each `entry(...)`-shaped,
  // one per key/value pair - is enough to answer the K/V question without
  // any defMap_ lookup, so fall back to deriving it directly from that
  // shape here.
  {
    std::string entryPackKeyType;
    std::string entryPackValueType;
    if (deriveKeyValueTypesFromEntryPackCall(initializer, entryPackKeyType, entryPackValueType)) {
      bindingOut.typeName = "map";
      bindingOut.isInferredKeyValueConstructorResult = true;
      bindingOut.typeTemplateArg = entryPackKeyType + ", " + entryPackValueType;
      return preserveBindingQualifiers();
    }
  }
  ReturnKind kind = inferExprReturnKind(initializer, params, locals);
  if (kind == ReturnKind::Unknown || kind == ReturnKind::Void) {
    return false;
  }
  std::string inferred = typeNameForReturnKind(kind);
  if (inferred.empty()) {
    return false;
  }
  bindingOut.typeName = inferred;
  bindingOut.typeTemplateArg.clear();
  return preserveBindingQualifiers();
}

}  // namespace primec::semantics
