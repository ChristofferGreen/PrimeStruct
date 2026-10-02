#include "SemanticsValidator.h"

#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <unordered_set>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"
#include "SemanticsValidatorStatementBindingsHelpers.h"
#include "SemanticsValidatorStatementBindingsState.h"

namespace primec::semantics {
using namespace statementBindingsHelpers;

PhaseStatus SemanticsValidator::validateBindingPhase3([[maybe_unused]] const std::vector<ParameterInfo> &params, [[maybe_unused]] std::unordered_map<std::string, BindingInfo> &locals, [[maybe_unused]] const Expr &stmt, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] bool &handled, ValidateBindingState &st) {
  [[maybe_unused]] auto &allowBindings = st.allowBindings;
  [[maybe_unused]] auto &allowCompileTimeTypeBindings = st.allowCompileTimeTypeBindings;
  [[maybe_unused]] auto &failBindingDiagnostic = st.failBindingDiagnostic;
  [[maybe_unused]] auto &definitionTemplateArgs = st.definitionTemplateArgs;
  [[maybe_unused]] auto &info = st.info;
  [[maybe_unused]] auto &restrictType = st.restrictType;
  [[maybe_unused]] auto &hasExplicitType = st.hasExplicitType;
  [[maybe_unused]] auto &explicitAutoType = st.explicitAutoType;
  [[maybe_unused]] const Expr &initializer = *st.initializer;
  [[maybe_unused]] auto &isStandaloneSoaFieldViewInitializer = st.isStandaloneSoaFieldViewInitializer;
  [[maybe_unused]] auto &validateAndRecordTargetTypedSumInitializer = st.validateAndRecordTargetTypedSumInitializer;
  [[maybe_unused]] auto &isTargetTypedSumInitializerSyntax = st.isTargetTypedSumInitializerSyntax;
  if (isTargetTypedSumInitializerSyntax()) {
    if (std::optional<bool> handled = validateAndRecordTargetTypedSumInitializer()) {
      return st.done(*handled);
    }
  }
  const bool isMoveInitializer =
      initializer.kind == Expr::Kind::Call && !initializer.isMethodCall &&
      !initializer.isFieldAccess && isSimpleCallName(initializer, "move");
  if (initializer.kind == Expr::Kind::Call && isIfCall(initializer) &&
      !validateIfExpr(params, locals, initializer)) {
    return st.done(false);
  }
  BindingInfo prevalidatedComparableInfo = info;
  if (!hasExplicitType || explicitAutoType) {
    (void)inferBindingTypeFromInitializer(
        initializer, params, locals, prevalidatedComparableInfo, &stmt);
  }
  if (!validateBuiltinComparableKeyType(
          prevalidatedComparableInfo, definitionTemplateArgs, error_)) {
    return st.done(false);
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
      std::string resolvedInitializerPath =
          preferredCollectionHelperResolvedPath(initializer);
      if (resolvedInitializerPath.empty()) {
        resolvedInitializerPath = resolveCalleePath(initializer);
      }
      isMapConstructorInitializer =
          isResolvedKeyValueConstructorPath(resolvedInitializerPath);
    }
    if (!isMapConstructorInitializer) {
      return true;
    }
    std::unordered_set<std::string> visitingStructs;
    if (isRelocationTrivialContainerElementType(
            valueType, namespacePrefix, definitionTemplateArgs,
            visitingStructs)) {
      return true;
    }
    return failBindingDiagnostic(
        std::string("map ") +
        "literal requires relocation-trivial map value type until container "
        "move/reallocation semantics are implemented: " +
        valueType);
  };
  if (!validateMapConstructorInitializerRelocation()) {
    return st.done(false);
  }
  if (!validateExpr(params, locals, initializer)) {
    if (isStandaloneSoaFieldViewInitializer() && !initializer.args.empty()) {
      error_.clear();
      if (!validateExpr(params, locals, initializer.args.front())) {
        return st.done(false);
      }
    } else {
      if (const auto pendingPath =
              builtinSoaDirectPendingHelperPath(initializer, params, locals)) {
        return st.done(failBindingDiagnostic(
            soaUnavailableMethodDiagnostic(*pendingPath)));
      }
      if (error_.empty()) {
        return st.done(failBindingDiagnostic("binding initializer validateExpr failed"));
      }
      return st.done(false);
    }
  }
  if (const auto pendingPath =
          builtinSoaDirectPendingHelperPath(initializer, params, locals)) {
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
      return st.done(failBindingDiagnostic(
          soaUnavailableMethodDiagnostic(*pendingPath)));
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
    if (movedBinding == nullptr ||
        !errorTypesMatch(expectedBindingTypeText(info),
                         bindingTypeText(*movedBinding),
                         namespacePrefix)) {
      return st.done(failBindingDiagnostic("binding initializer type mismatch"));
    }
    if (restrictType.has_value()) {
      const bool hasTemplate = !info.typeTemplateArg.empty();
      if (!restrictMatchesBinding(*restrictType, info.typeName,
                                  info.typeTemplateArg, hasTemplate,
                                  namespacePrefix)) {
        return st.done(failBindingDiagnostic("restrict type does not match binding type"));
      }
    }
    if (!validateBuiltinComparableKeyType(info, definitionTemplateArgs, error_)) {
      return st.done(false);
    }
    insertLocalBinding(locals, stmt.name, std::move(info));
    return st.done(true);
  }
  if (!isMoveInitializer) {
    if (std::optional<bool> handled = validateAndRecordTargetTypedSumInitializer()) {
      return st.done(*handled);
    }
  }
  ReturnKind initKind = inferExprReturnKind(initializer, params, locals);
  if (initKind == ReturnKind::Void && !isStructConstructorValueExpr(initializer)) {
    BindingInfo recoveredInitializerBinding;
    const bool recoveredInitializerValueBinding =
        inferBindingTypeFromInitializer(initializer,
                                        params,
                                        locals,
                                        recoveredInitializerBinding,
                                        &stmt) &&
        !(recoveredInitializerBinding.typeName.empty() ||
          (recoveredInitializerBinding.typeName == "array" &&
           recoveredInitializerBinding.typeTemplateArg.empty()));
    if (!recoveredInitializerValueBinding) {
      return st.done(failBindingDiagnostic("binding initializer requires a value"));
    }
    initKind = returnKindForTypeName(
        normalizeBindingTypeName(recoveredInitializerBinding.typeName));
  }
  auto isSoftwareNumericBindingCompatible = [](ReturnKind expectedKind, ReturnKind actualKind) -> bool {
    switch (expectedKind) {
      case ReturnKind::Integer:
        return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 || actualKind == ReturnKind::UInt64 ||
               actualKind == ReturnKind::Bool || actualKind == ReturnKind::Integer;
      case ReturnKind::Decimal:
        return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 || actualKind == ReturnKind::UInt64 ||
               actualKind == ReturnKind::Bool || actualKind == ReturnKind::Float32 ||
               actualKind == ReturnKind::Float64 || actualKind == ReturnKind::Integer ||
               actualKind == ReturnKind::Decimal;
      case ReturnKind::Complex:
        return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 || actualKind == ReturnKind::UInt64 ||
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
    if (base == "Vector" ||
        isLegacyExperimentalVectorCompatibilityTypePath(base) ||
        isLegacyExperimentalVectorCompatibilityTypePath("/" + base)) {
      return legacyExperimentalVectorCompatibilityFamilyName();
    }
    return {};
  };
  auto collectionRepresentationsCompatible =
      [](const std::string &expectedRepresentation,
         const std::string &actualRepresentation) {
        if (expectedRepresentation == actualRepresentation) {
          return true;
        }
        const bool isVectorRepresentationPair =
            (expectedRepresentation == "builtin_vector" &&
             actualRepresentation ==
                 legacyExperimentalVectorCompatibilityFamilyName()) ||
            (expectedRepresentation ==
                 legacyExperimentalVectorCompatibilityFamilyName() &&
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
        resolveResultTypeForExpr(initializer, params, locals, resultInfo) &&
        resultInfo.isResult) {
      return st.done(failBindingDiagnostic("binding initializer type mismatch"));
    }
    if (expectedType == "Task") {
      BindingInfo initializerBindingInfo;
      if (!inferTaskSpawnBinding(initializer, params, locals,
                                 initializerBindingInfo) ||
          !errorTypesMatch(info.typeTemplateArg,
                           initializerBindingInfo.typeTemplateArg,
                           namespacePrefix)) {
        return st.done(failBindingDiagnostic("binding initializer type mismatch"));
      }
    } else if (expectedType == "string") {
      if (!isStringExpr(initializer, params, locals)) {
        return st.done(failBindingDiagnostic("binding initializer type mismatch"));
      }
    } else {
      BindingInfo initializerBindingInfo;
      const bool hasInitializerBindingInfo =
          inferBindingTypeFromInitializer(initializer, params, locals, initializerBindingInfo, &stmt);
      std::string initializerTypeText;
      const bool hasInitializerTypeText =
          inferQueryExprTypeText(initializer, params, locals, initializerTypeText);
      if (hasInitializerTypeText) {
        std::string actualRepresentation =
            collectionRepresentation(initializerTypeText, {});
        const std::string initializerBindingRepresentation =
            hasInitializerBindingInfo
                ? collectionRepresentation(initializerBindingInfo.typeName, initializerBindingInfo.typeTemplateArg)
                : std::string{};
        if (actualRepresentation.empty()) {
          actualRepresentation = initializerBindingRepresentation;
        } else if (!expectedRepresentation.empty() &&
                   !initializerBindingRepresentation.empty() &&
                   initializerBindingRepresentation == expectedRepresentation) {
          actualRepresentation = initializerBindingRepresentation;
        }
        if (!expectedRepresentation.empty() &&
            !actualRepresentation.empty() &&
            !collectionRepresentationsCompatible(expectedRepresentation,
                                                 actualRepresentation)) {
          return st.done(failBindingDiagnostic("binding initializer type mismatch"));
        }
      } else if (hasInitializerBindingInfo) {
        const std::string actualRepresentation =
            collectionRepresentation(initializerBindingInfo.typeName, initializerBindingInfo.typeTemplateArg);
        if (!expectedRepresentation.empty() &&
            !actualRepresentation.empty() &&
            !collectionRepresentationsCompatible(expectedRepresentation,
                                                 actualRepresentation)) {
          return st.done(failBindingDiagnostic("binding initializer type mismatch"));
        }
      }
      const ReturnKind expectedKind = returnKindForTypeName(expectedType);
      if (expectedKind != ReturnKind::Unknown && initKind != ReturnKind::Unknown) {
        if (!isSoftwareNumericBindingCompatible(expectedKind, initKind) &&
            !isFloatBindingCompatible(expectedKind, initKind) &&
            initKind != expectedKind) {
          return st.done(failBindingDiagnostic("binding initializer type mismatch"));
        }
      }
      const std::string expectedStruct =
          resolveStructTypePath(expectedType, namespacePrefix, structNames_);
      // The `map<K, V>(...)` literal spelling (builtin or its rewritten stdlib
      // constructor) intentionally initializes the public `Map` wrapper.
      std::string builtinCollectionName;
      std::string initializerBase = initializer.name.substr(
          initializer.name.find_last_of('/') == std::string::npos
              ? 0
              : initializer.name.find_last_of('/') + 1);
      initializerBase = initializerBase.substr(0, initializerBase.find("__"));
      if (!expectedStruct.empty() && initializerBase != "map" &&
          !getBuiltinCollectionName(initializer, builtinCollectionName)) {
        const std::string actualStruct =
            inferStructReturnPath(initializer, params, locals);
        if (structNames_.count(actualStruct) > 0 &&
            actualStruct.substr(0, actualStruct.find("__t")) !=
                expectedStruct.substr(0, expectedStruct.find("__t"))) {
          return st.done(failBindingDiagnostic("binding initializer type mismatch"));
        }
      }
    }
  }
  return PhaseStatus::Continue;
}

PhaseStatus SemanticsValidator::validateBindingPhase4([[maybe_unused]] const std::vector<ParameterInfo> &params, [[maybe_unused]] std::unordered_map<std::string, BindingInfo> &locals, [[maybe_unused]] const Expr &stmt, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] bool &handled, ValidateBindingState &st) {
  [[maybe_unused]] auto &allowBindings = st.allowBindings;
  [[maybe_unused]] auto &allowCompileTimeTypeBindings = st.allowCompileTimeTypeBindings;
  [[maybe_unused]] auto &failBindingDiagnostic = st.failBindingDiagnostic;
  [[maybe_unused]] auto &definitionTemplateArgs = st.definitionTemplateArgs;
  [[maybe_unused]] auto &info = st.info;
  [[maybe_unused]] auto &restrictType = st.restrictType;
  [[maybe_unused]] const Expr &initializer = *st.initializer;
  [[maybe_unused]] auto &entryArgInit = st.entryArgInit;
  [[maybe_unused]] auto &entryArgStringInit = st.entryArgStringInit;
  if (info.typeName == "uninitialized") {
    if (info.typeTemplateArg.empty()) {
      return st.done(failBindingDiagnostic("uninitialized requires exactly one template argument"));
    }
    if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall || initializer.isBinding) {
      return st.done(failBindingDiagnostic("uninitialized bindings require uninitialized<T>() initializer"));
    }
    if (initializer.name != "uninitialized" && initializer.name != "/uninitialized") {
      return st.done(failBindingDiagnostic("uninitialized bindings require uninitialized<T>() initializer"));
    }
    if (initializer.hasBodyArguments || !initializer.bodyArguments.empty() || !initializer.args.empty()) {
      return st.done(failBindingDiagnostic("uninitialized does not accept arguments"));
    }
    if (initializer.templateArgs.size() != 1 ||
        !errorTypesMatch(info.typeTemplateArg, initializer.templateArgs.front(), namespacePrefix)) {
      return st.done(failBindingDiagnostic("uninitialized initializer type mismatch"));
    }
  }
  if (restrictType.has_value()) {
    const bool hasTemplate = !info.typeTemplateArg.empty();
    if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
      return st.done(failBindingDiagnostic("restrict type does not match binding type"));
    }
  }
  if (entryArgInit || entryArgStringInit) {
    if (normalizeBindingTypeName(info.typeName) != "string") {
      return st.done(failBindingDiagnostic("entry argument strings require string bindings"));
    }
    info.isEntryArgString = true;
  }
  auto pointerAliasRootForBinding = [&](const std::string &bindingName, const BindingInfo &binding) -> std::string {
    std::string referenceRoot = referenceRootForBorrowBinding(bindingName, binding);
    if (!referenceRoot.empty()) {
      return referenceRoot;
    }
    if (binding.typeName == "Pointer" && !binding.referenceRoot.empty()) {
      return binding.referenceRoot;
    }
    return "";
  };
  st.resolveNamedBinding = [&](const std::string &name) -> const BindingInfo * {
    if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
      return paramBinding;
    }
    auto it = locals.find(name);
    if (it == locals.end()) {
      return nullptr;
    }
    return &it->second;
  };
  [[maybe_unused]] auto &resolveNamedBinding = st.resolveNamedBinding;
  std::function<bool(const Expr &, std::string &)> resolveStorageRootExpr;
  [[maybe_unused]] auto &resolvePointerRoot = st.resolvePointerRoot;
  resolveStorageRootExpr = [&](const Expr &expr, std::string &rootOut) -> bool {
    if (expr.kind == Expr::Kind::Name) {
      const BindingInfo *binding = resolveNamedBinding(expr.name);
      if (binding == nullptr) {
        return false;
      }
      std::string aliasRoot = pointerAliasRootForBinding(expr.name, *binding);
      if (!aliasRoot.empty()) {
        rootOut = std::move(aliasRoot);
      } else {
        rootOut = expr.name;
      }
      return true;
    }
    if (expr.kind != Expr::Kind::Call) {
      return false;
    }
    std::string builtinName;
    if (getBuiltinPointerName(expr, builtinName) && builtinName == "dereference" && expr.args.size() == 1) {
      return resolvePointerRoot(expr.args.front(), rootOut);
    }
    if (expr.isFieldAccess && expr.args.size() == 1) {
      std::string receiverRoot;
      if (!resolveStorageRootExpr(expr.args.front(), receiverRoot) || receiverRoot.empty()) {
        return false;
      }
      rootOut = receiverRoot + "." + expr.name;
      return true;
    }
    return false;
  };
  resolvePointerRoot = [&](const Expr &expr, std::string &rootOut) -> bool {
    if (expr.kind == Expr::Kind::Name) {
      const BindingInfo *binding = resolveNamedBinding(expr.name);
      if (binding == nullptr) {
        return false;
      }
      rootOut = pointerAliasRootForBinding(expr.name, *binding);
      if (rootOut.empty() && binding->typeName == "Pointer") {
        rootOut = expr.name;
      }
      return !rootOut.empty();
    }
    if (expr.kind != Expr::Kind::Call) {
      return false;
    }
    std::string builtinName;
    if (getBuiltinPointerName(expr, builtinName) && builtinName == "location" && expr.args.size() == 1) {
      const Expr &target = expr.args.front();
      if (target.kind == Expr::Kind::Name) {
        const BindingInfo *binding = resolveNamedBinding(target.name);
        if (binding != nullptr) {
          std::string root = pointerAliasRootForBinding(target.name, *binding);
          if (!root.empty()) {
            rootOut = std::move(root);
          } else {
            rootOut = target.name;
          }
          return true;
        }
        return false;
      }
      return resolvePointerRoot(target, rootOut);
    }
    if (expr.isFieldAccess && expr.args.size() == 1) {
      std::string receiverRoot;
      if (!resolvePointerRoot(expr.args.front(), receiverRoot) || receiverRoot.empty()) {
        return false;
      }
      rootOut = receiverRoot + "." + expr.name;
      return true;
    }
    std::string opName;
    if (getBuiltinOperatorName(expr, opName) && (opName == "plus" || opName == "minus") && expr.args.size() == 2) {
      if (isPointerLikeExpr(expr.args[1], params, locals)) {
        return false;
      }
      return resolvePointerRoot(expr.args[0], rootOut);
    }
    std::string resolvedCallPath = preferredCollectionHelperResolvedPath(expr);
    if (resolvedCallPath.empty()) {
      resolvedCallPath = resolveCalleePath(expr);
    }
    if (const std::string concreteResolvedCallPath =
            resolveExprConcreteCallPath(params, locals, expr, resolvedCallPath);
        !concreteResolvedCallPath.empty()) {
      resolvedCallPath = concreteResolvedCallPath;
    }
    const bool isSoaColumnSlotUnsafe =
        isExperimentalSoaColumnSlotHelperPath(resolvedCallPath);
    const bool isVectorSlotUnsafe =
        resolvedCallPath.rfind(
            legacyExperimentalVectorCompatibilityPrefix() +
                std::string("vector") + "SlotUnsafe",
            0) == 0;
    auto isCanonicalVectorSlotUnsafe = [](std::string path) {
      const size_t specializationSuffix = path.find("__");
      if (specializationSuffix != std::string::npos) {
        path.erase(specializationSuffix);
      }
      return path.rfind(collection_paths::memberPath(collection_paths::kVectorFolder,
                                                    "vectorSlotUnsafe"),
                        0) == 0;
    };
    if ((isSoaColumnSlotUnsafe || isVectorSlotUnsafe ||
         isCanonicalVectorSlotUnsafe(resolvedCallPath)) &&
        !expr.args.empty()) {
      std::string storageRoot;
      if (!resolveStorageRootExpr(expr.args.front(), storageRoot) || storageRoot.empty()) {
        return false;
      }
      rootOut = storageRoot + ".data";
      return true;
    }
    auto isPointerRootPreservingCall = [](const std::string &name) {
      std::string normalizedName = name;
      if (const auto slash = normalizedName.find_last_of('/'); slash != std::string::npos) {
        normalizedName = normalizedName.substr(slash + 1);
      }
      if (const auto generatedSuffix = normalizedName.find("__");
          generatedSuffix != std::string::npos) {
        normalizedName = normalizedName.substr(0, generatedSuffix);
      }
      return normalizedName == "at" || normalizedName == "at_unsafe" ||
             normalizedName == "reinterpret" ||
             normalizedName == "bufferOffsetUnsafe" ||
             normalizedName == "bufferOffsetChecked" ||
             normalizedName == "bufferReinterpret" ||
             normalizedName == "bufferReinterpretBytes" ||
             normalizedName == "bufferOffsetBytesUnsafe" ||
             normalizedName == "bufferOffsetBytesChecked" ||
             normalizedName == "bufferReinterpretFromBytes";
    };
    if (isPointerRootPreservingCall(resolvedCallPath) && !expr.args.empty()) {
      return resolvePointerRoot(expr.args.front(), rootOut);
    }
    auto defIt = defMap_.find(resolvedCallPath);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    bool returnsPointer = false;
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      std::string base;
      std::string arg;
      if (!splitTemplateTypeName(normalizeBindingTypeName(transform.templateArgs.front()), base, arg)) {
        continue;
      }
      if (normalizeBindingTypeName(base) == "Pointer" && !arg.empty()) {
        returnsPointer = true;
        break;
      }
    }
    if (!returnsPointer) {
      return false;
    }
    const auto paramsIt = paramsByDef_.find(resolvedCallPath);
    if (paramsIt == paramsByDef_.end()) {
      return false;
    }
    const auto &nestedParams = paramsIt->second;
    std::string nestedArgError;
    std::vector<const Expr *> nestedOrderedArgs;
    if (!buildOrderedArguments(nestedParams, expr.args, expr.argNames,
                               nestedOrderedArgs, nestedArgError)) {
      return false;
    }
    const Expr *returnedValueExpr = nullptr;
    const Definition &nestedDef = *defIt->second;
    for (const auto &stmtExpr : nestedDef.statements) {
      if (isReturnCall(stmtExpr) && stmtExpr.args.size() == 1) {
        returnedValueExpr = &stmtExpr.args.front();
      }
    }
    if (nestedDef.returnExpr.has_value()) {
      returnedValueExpr = &*nestedDef.returnExpr;
    }
    if (returnedValueExpr == nullptr) {
      return false;
    }
    auto resolveNestedArgPointerRoot = [&](const Expr &nestedArg,
                                           std::string &nestedRootOut) {
      if (nestedArg.kind == Expr::Kind::Name) {
        for (size_t index = 0;
             index < nestedParams.size() && index < nestedOrderedArgs.size();
             ++index) {
          if (nestedParams[index].name == nestedArg.name &&
              nestedOrderedArgs[index] != nullptr) {
            return resolvePointerRoot(*nestedOrderedArgs[index], nestedRootOut);
          }
        }
      }
      return resolvePointerRoot(nestedArg, nestedRootOut);
    };
    if (returnedValueExpr->kind == Expr::Kind::Name) {
      return resolveNestedArgPointerRoot(*returnedValueExpr, rootOut);
    }
    if (returnedValueExpr->kind != Expr::Kind::Call) {
      return false;
    }
    std::string returnedOpName;
    if (getBuiltinOperatorName(*returnedValueExpr, returnedOpName) &&
        (returnedOpName == "plus" || returnedOpName == "minus") &&
        returnedValueExpr->args.size() == 2) {
      return resolveNestedArgPointerRoot(returnedValueExpr->args.front(), rootOut);
    }
    std::string returnedCallPath = preferredCollectionHelperResolvedPath(*returnedValueExpr);
    if (returnedCallPath.empty()) {
      returnedCallPath = resolveCalleePath(*returnedValueExpr);
    }
    if (isPointerRootPreservingCall(returnedCallPath) && !returnedValueExpr->args.empty()) {
      return resolveNestedArgPointerRoot(returnedValueExpr->args.front(), rootOut);
    }
    return false;
  };
  if (isExperimentalSoaColumnBindingType(info) && info.referenceRoot.empty()) {
    std::string storageRoot;
    if (resolveStorageRootExpr(initializer, storageRoot) && !storageRoot.empty()) {
      info.referenceRoot = std::move(storageRoot);
    }
  }
  if (info.typeName == "Pointer") {
    std::string pointerRoot;
    if (resolvePointerRoot(initializer, pointerRoot)) {
      info.referenceRoot = std::move(pointerRoot);
    }
    if (!validateBuiltinComparableKeyType(info, definitionTemplateArgs, error_)) {
      return st.done(false);
    }
    insertLocalBinding(locals, stmt.name, std::move(info));
    return st.done(true);
  }
  return PhaseStatus::Continue;
}

} // namespace primec::semantics
