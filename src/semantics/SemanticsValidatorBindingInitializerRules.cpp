#include "SemanticsValidator.h"

#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <unordered_set>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"
#include "SemanticsValidatorStatementBindingsHelpers.h"
#include "SemanticsValidatorBindingContext.h"

namespace primec::semantics {
using namespace statementBindingsHelpers;

// The rule that checks a binding's initializer against its declared type (TODO-5982).

namespace {

bool isPrimitiveScalarReturnKind(ReturnKind kind) {
  switch (kind) {
  case ReturnKind::Int:
  case ReturnKind::Int64:
  case ReturnKind::UInt64:
  case ReturnKind::UInt8:
  case ReturnKind::Float32:
  case ReturnKind::Float64:
  case ReturnKind::Bool:
  case ReturnKind::String:
    return true;
  default:
    return false;
  }
}

} // namespace

PhaseStatus SemanticsValidator::validateBindingInitializer(BindingContext &ctx) {
  const std::vector<ParameterInfo> &params = ctx.params;
  std::unordered_map<std::string, BindingInfo> &locals = ctx.locals;
  const Expr &stmt = ctx.stmt;
  const std::string &namespacePrefix = ctx.namespacePrefix;
  auto &definitionTemplateArgs = ctx.definitionTemplateArgs;
  auto &info = ctx.info;
  auto &restrictType = ctx.restrictType;
  auto &hasExplicitType = ctx.hasExplicitType;
  auto &explicitAutoType = ctx.explicitAutoType;
  const Expr &initializer = *ctx.initializer;
  if (isTargetTypedSumInitializerSyntax(ctx)) {
    if (std::optional<bool> handled = validateAndRecordTargetTypedSumInitializer(ctx)) {
      return ctx.done(*handled);
    }
  }
  const bool isMoveInitializer = initializer.kind == Expr::Kind::Call &&
                                 !initializer.isMethodCall && !initializer.isFieldAccess &&
                                 isSimpleCallName(initializer, "move");
  if (initializer.kind == Expr::Kind::Call && isIfCall(initializer) &&
      !validateIfExpr(params, locals, initializer)) {
    return ctx.done(false);
  }
  BindingInfo prevalidatedComparableInfo = info;
  if (!hasExplicitType || explicitAutoType) {
    (void)inferBindingTypeFromInitializer(
        initializer, params, locals, prevalidatedComparableInfo, &stmt);
  }
  if (!validateBuiltinComparableKeyType(
          prevalidatedComparableInfo, definitionTemplateArgs, error_)) {
    return ctx.done(false);
  }
  auto validateMapConstructorInitializerRelocation = [&]() -> bool {
    std::string keyType;
    std::string valueType;
    if (!extractKeyValueCollectionTypesFromTypeText(
            expectedBindingTypeText(info), keyType, valueType) ||
        initializer.kind != Expr::Kind::Call || initializer.args.empty()) {
      return true;
    }
    std::string builtinCollectionName;
    bool isMapConstructorInitializer =
        getBuiltinCollectionName(initializer, builtinCollectionName) &&
        builtinCollectionName == "map";
    if (!isMapConstructorInitializer) {
      std::string resolvedInitializerPath = preferredCollectionHelperResolvedPath(initializer);
      if (resolvedInitializerPath.empty()) {
        resolvedInitializerPath = resolveCalleePath(initializer);
      }
      isMapConstructorInitializer = isResolvedKeyValueConstructorPath(resolvedInitializerPath);
    }
    if (!isMapConstructorInitializer) {
      return true;
    }
    std::unordered_set<std::string> visitingStructs;
    if (isRelocationTrivialContainerElementType(
            valueType, namespacePrefix, definitionTemplateArgs, visitingStructs)) {
      return true;
    }
    return failBindingDiagnostic(
        ctx,
        std::string("map ") +
            "literal requires relocation-trivial map value type until container "
            "move/reallocation semantics are implemented: " +
            valueType);
  };
  if (!validateMapConstructorInitializerRelocation()) {
    return ctx.done(false);
  }
  if (!validateExpr(params, locals, initializer)) {
    if (isStandaloneSoaFieldViewInitializer(ctx) && !initializer.args.empty()) {
      error_.clear();
      if (!validateExpr(params, locals, initializer.args.front())) {
        return ctx.done(false);
      }
    } else {
      if (const auto pendingPath = builtinSoaDirectPendingHelperPath(initializer, params, locals)) {
        return ctx.done(failBindingDiagnostic(ctx, soaUnavailableMethodDiagnostic(*pendingPath)));
      }
      if (error_.empty()) {
        return ctx.done(failBindingDiagnostic(ctx, "binding initializer validateExpr failed"));
      }
      return ctx.done(false);
    }
  }
  if (const auto pendingPath = builtinSoaDirectPendingHelperPath(initializer, params, locals)) {
    std::string pendingFieldName;
    std::string resolvedInitializerPath = preferredCollectionHelperResolvedPath(initializer);
    if (resolvedInitializerPath.empty()) {
      resolvedInitializerPath = resolveCalleePath(initializer);
    }
    if (splitSoaFieldViewHelperPath(*pendingPath, &pendingFieldName) &&
        (isBuiltinSoaFieldViewExpr(initializer, params, locals, nullptr) ||
         isExperimentalSoaFieldViewHelperPath(resolvedInitializerPath))) {
      // Field-view bindings are handled below so borrow roots and invalidation
      // diagnostics remain tied to the binding lifetime.
    } else {
      return ctx.done(failBindingDiagnostic(ctx, soaUnavailableMethodDiagnostic(*pendingPath)));
    }
  }
  if (isMoveInitializer && hasExplicitType && !explicitAutoType) {
    const Expr &moveTarget = initializer.args.front();
    const BindingInfo *movedBinding = nullptr;
    if (moveTarget.kind == Expr::Kind::Name) {
      movedBinding = findParamBinding(params, moveTarget.name);
      if (movedBinding == nullptr) {
        auto localIt = locals.find(moveTarget.name);
        if (localIt != locals.end()) {
          movedBinding = &localIt->second;
        }
      }
    }
    if (movedBinding == nullptr || !errorTypesMatch(expectedBindingTypeText(info),
                                                    bindingTypeText(*movedBinding),
                                                    namespacePrefix)) {
      return ctx.done(failBindingDiagnostic(ctx, "binding initializer type mismatch"));
    }
    if (restrictType.has_value()) {
      const bool hasTemplate = !info.typeTemplateArg.empty();
      if (!restrictMatchesBinding(
              *restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
        return ctx.done(failBindingDiagnostic(ctx, "restrict type does not match binding type"));
      }
    }
    if (!validateBuiltinComparableKeyType(info, definitionTemplateArgs, error_)) {
      return ctx.done(false);
    }
    insertLocalBinding(locals, stmt.name, std::move(info));
    return ctx.done(true);
  }
  if (!isMoveInitializer) {
    if (std::optional<bool> handled = validateAndRecordTargetTypedSumInitializer(ctx)) {
      return ctx.done(*handled);
    }
  }
  ReturnKind initKind = inferExprReturnKind(initializer, params, locals);
  if (initKind == ReturnKind::Void && !isStructConstructorValueExpr(initializer)) {
    BindingInfo recoveredInitializerBinding;
    const bool recoveredInitializerValueBinding =
        inferBindingTypeFromInitializer(
            initializer, params, locals, recoveredInitializerBinding, &stmt) &&
        !(recoveredInitializerBinding.typeName.empty() ||
          (recoveredInitializerBinding.typeName == "array" &&
           recoveredInitializerBinding.typeTemplateArg.empty()));
    if (!recoveredInitializerValueBinding) {
      return ctx.done(failBindingDiagnostic(ctx, "binding initializer requires a value"));
    }
    initKind =
        returnKindForTypeName(normalizeBindingTypeName(recoveredInitializerBinding.typeName));
  }
  auto isSoftwareNumericBindingCompatible = [](ReturnKind expectedKind,
                                               ReturnKind actualKind) -> bool {
    switch (expectedKind) {
    case ReturnKind::Integer:
      return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 ||
             (actualKind == ReturnKind::UInt64 || actualKind == ReturnKind::UInt8) ||
             actualKind == ReturnKind::Bool || actualKind == ReturnKind::Integer;
    case ReturnKind::Decimal:
      return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 ||
             (actualKind == ReturnKind::UInt64 || actualKind == ReturnKind::UInt8) ||
             actualKind == ReturnKind::Bool || actualKind == ReturnKind::Float32 ||
             actualKind == ReturnKind::Float64 || actualKind == ReturnKind::Integer ||
             actualKind == ReturnKind::Decimal;
    case ReturnKind::Complex:
      return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 ||
             (actualKind == ReturnKind::UInt64 || actualKind == ReturnKind::UInt8) ||
             actualKind == ReturnKind::Bool || actualKind == ReturnKind::Float32 ||
             actualKind == ReturnKind::Float64 || actualKind == ReturnKind::Integer ||
             actualKind == ReturnKind::Decimal || actualKind == ReturnKind::Complex;
    default:
      return false;
    }
  };
  auto isFloatBindingCompatible = [](ReturnKind expectedKind, ReturnKind actualKind) -> bool {
    if (expectedKind != ReturnKind::Float32 && expectedKind != ReturnKind::Float64) {
      return false;
    }
    return actualKind == ReturnKind::Float32 || actualKind == ReturnKind::Float64;
  };
  auto isStringExpr = [&](const Expr &candidate,
                          const std::vector<ParameterInfo> &paramsIn,
                          const std::unordered_map<std::string, BindingInfo> &localsIn) -> bool {
    if (candidate.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (candidate.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(paramsIn, candidate.name)) {
        return paramBinding->typeName == "string";
      }
      auto it = localsIn.find(candidate.name);
      return it != localsIn.end() && it->second.typeName == "string";
    }
    return inferExprReturnKind(candidate, paramsIn, localsIn) == ReturnKind::String;
  };
  auto collectionRepresentation = [&](const std::string &typeName,
                                      const std::string &typeTemplateArg) -> std::string {
    std::string normalizedType = normalizeBindingTypeName(typeName);
    std::string base = normalizedType;
    std::string argText = typeTemplateArg;
    if (argText.empty()) {
      std::string splitBase;
      std::string splitArgText;
      if (splitTemplateTypeName(normalizedType, splitBase, splitArgText)) {
        base = normalizeBindingTypeName(splitBase);
        argText = splitArgText;
      }
    }
    if (base == "vector") {
      return "builtin_vector";
    }
    if (base == "Vector" || isLegacyExperimentalVectorCompatibilityTypePath(base) ||
        isLegacyExperimentalVectorCompatibilityTypePath("/" + base)) {
      return legacyExperimentalVectorCompatibilityFamilyName();
    }
    return {};
  };
  auto collectionRepresentationsCompatible = [](const std::string &expectedRepresentation,
                                                const std::string &actualRepresentation) {
    if (expectedRepresentation == actualRepresentation) {
      return true;
    }
    const bool isVectorRepresentationPair =
        (expectedRepresentation == "builtin_vector" &&
         actualRepresentation == legacyExperimentalVectorCompatibilityFamilyName()) ||
        (expectedRepresentation == legacyExperimentalVectorCompatibilityFamilyName() &&
         actualRepresentation == "builtin_vector");
    return isVectorRepresentationPair;
  };
  if (!hasExplicitType || explicitAutoType) {
    (void)inferBindingTypeFromInitializer(initializer, params, locals, info, &stmt);
  } else {
    const std::string expectedType = normalizeBindingTypeName(info.typeName);
    const std::string expectedRepresentation =
        collectionRepresentation(info.typeName, info.typeTemplateArg);
    ResultTypeInfo resultInfo;
    if (expectedType != "Result" &&
        resolveResultTypeForExpr(initializer, params, locals, resultInfo) && resultInfo.isResult) {
      return ctx.done(failBindingDiagnostic(ctx, "binding initializer type mismatch"));
    }
    if (expectedType == "Task") {
      BindingInfo initializerBindingInfo;
      if (!inferTaskSpawnBinding(initializer, params, locals, initializerBindingInfo) ||
          !errorTypesMatch(
              info.typeTemplateArg, initializerBindingInfo.typeTemplateArg, namespacePrefix)) {
        return ctx.done(failBindingDiagnostic(ctx, "binding initializer type mismatch"));
      }
    } else if (expectedType == "string") {
      if (!isStringExpr(initializer, params, locals)) {
        return ctx.done(failBindingDiagnostic(ctx, "binding initializer type mismatch"));
      }
    } else {
      BindingInfo initializerBindingInfo;
      const bool hasInitializerBindingInfo = inferBindingTypeFromInitializer(
          initializer, params, locals, initializerBindingInfo, &stmt);
      std::string initializerTypeText;
      const bool hasInitializerTypeText =
          inferQueryExprTypeText(initializer, params, locals, initializerTypeText);
      if (hasInitializerTypeText) {
        std::string actualRepresentation = collectionRepresentation(initializerTypeText, {});
        const std::string initializerBindingRepresentation =
            hasInitializerBindingInfo
                ? collectionRepresentation(initializerBindingInfo.typeName,
                                           initializerBindingInfo.typeTemplateArg)
                : std::string{};
        if (actualRepresentation.empty()) {
          actualRepresentation = initializerBindingRepresentation;
        } else if (!expectedRepresentation.empty() && !initializerBindingRepresentation.empty() &&
                   initializerBindingRepresentation == expectedRepresentation) {
          actualRepresentation = initializerBindingRepresentation;
        }
        if (!expectedRepresentation.empty() && !actualRepresentation.empty() &&
            !collectionRepresentationsCompatible(expectedRepresentation, actualRepresentation)) {
          return ctx.done(failBindingDiagnostic(ctx, "binding initializer type mismatch"));
        }
      } else if (hasInitializerBindingInfo) {
        const std::string actualRepresentation = collectionRepresentation(
            initializerBindingInfo.typeName, initializerBindingInfo.typeTemplateArg);
        if (!expectedRepresentation.empty() && !actualRepresentation.empty() &&
            !collectionRepresentationsCompatible(expectedRepresentation, actualRepresentation)) {
          return ctx.done(failBindingDiagnostic(ctx, "binding initializer type mismatch"));
        }
      }
      const ReturnKind expectedKind = returnKindForTypeName(expectedType);
      if (expectedKind != ReturnKind::Unknown && initKind != ReturnKind::Unknown) {
        if (!isSoftwareNumericBindingCompatible(expectedKind, initKind) &&
            !isFloatBindingCompatible(expectedKind, initKind) && initKind != expectedKind) {
          return ctx.done(failBindingDiagnostic(ctx, "binding initializer type mismatch"));
        }
      }
      const std::string expectedStruct =
          resolveStructTypePath(expectedType, namespacePrefix, structNames_);
      // The `map<K, V>(...)` literal spelling (builtin or its rewritten stdlib
      // constructor) intentionally initializes the public `Map` wrapper.
      std::string builtinCollectionName;
      std::string initializerBase =
          initializer.name.substr(initializer.name.find_last_of('/') == std::string::npos
                                      ? 0
                                      : initializer.name.find_last_of('/') + 1);
      initializerBase = initializerBase.substr(0, initializerBase.find("__"));
      if (!expectedStruct.empty() && initializerBase != "map" &&
          !getBuiltinCollectionName(initializer, builtinCollectionName)) {
        const std::string actualStruct = inferStructReturnPath(initializer, params, locals);
        if (structNames_.count(actualStruct) > 0 &&
            actualStruct.substr(0, actualStruct.find("__t")) !=
                expectedStruct.substr(0, expectedStruct.find("__t"))) {
          return ctx.done(failBindingDiagnostic(ctx, "binding initializer type mismatch"));
        }
        // A single unlabeled entry in a struct binding's braces is the value itself, not a
        // field entry (`[Lib] x{5i32}` is not `Lib{5i32}`). A scalar there would be bound as
        // the struct's address, so field reads go through a number (a null or stray reference).
        if (expectedKind == ReturnKind::Unknown && structNames_.count(expectedStruct) > 0 &&
            isPrimitiveScalarReturnKind(initKind)) {
          return ctx.done(failBindingDiagnostic(
              ctx,
              "binding initializer type mismatch: " + expectedStruct +
                  " needs a struct value; construct it with labeled entries or " + expectedType +
                  "{...}"));
        }
      }
    }
  }
  return PhaseStatus::Continue;
}

} // namespace primec::semantics
