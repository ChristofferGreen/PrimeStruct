// collection-surface-audit: exempt
#include "SemanticsValidator.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "SemanticsValidatorMethodTargetResolutionDetail.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/ReceiverElementFamilyClassifier.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/support/CollectionHelperNames.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace primec::semantics {
using namespace method_target_detail;

// Dispatch order (TODO-4724/TODO-5275): this function tries progressively
// more general receiver-typing strategies until one resolves the method
// call's target definition path, in this order:
//   1. explicit rooted/removed-compat-helper spellings computed up front
//      (explicitRemovedMethodPath/explicitVectorHelperPath/
//      explicitKeyValueHelperPath) - an explicit spelling wins outright so
//      it can't be silently reinterpreted by a later, more general rule;
//   2. collection-vector-metadata shortcuts (count/capacity-shaped builtins
//      that don't need a full type resolution);
//   3. explicit rooted key-value / vector-family receiver special cases
//      (args-pack element access, explicit canonical vector helper
//      receivers, indexed args-pack key-value targets, direct key-value
//      constructor receivers) - each narrower than a full type inference;
//   4. resolveMethodTargetGenericFallback - the last-resort path that does
//      a full receiver type inference (inferMethodTargetReceiverType) and
//      walks File / collection / struct / sum-type candidates in turn.
// Earlier steps are checked first because they're cheaper and more
// specific; the generic fallback is only reached once every explicit or
// shape-specific shortcut has been ruled out.
bool SemanticsValidator::resolveMethodTarget(const std::vector<ParameterInfo> &params,
                                             const std::unordered_map<std::string, BindingInfo> &locals,
                                             const std::string &callNamespacePrefix,
                                             const Expr &receiver,
                                             const std::string &methodName,
                                             std::string &resolvedOut,
                                             bool &isBuiltinOut) {
  isBuiltinOut = false;
  auto hasDefinitionFamilyPath = [&](std::string_view path) {
    const std::string pathText(path);
    if (defMap_.count(pathText) > 0 || definitionFamilyPathIndex().count(pathText) > 0) {
      return true;
    }
    return anyDefinitionFamilyPathStartsWith(pathText + "<") ||
           anyDefinitionFamilyPathStartsWith(pathText + "__t") ||
           anyDefinitionFamilyPathStartsWith(pathText + "__ov");
  };
  auto startsWithRootVectorMethodPrefix = [&](std::string_view path) {
    return isUnrootedVectorHelperPath(path);
  };
  auto startsWithRootedVectorMethodPrefix = [&](std::string_view path) {
    return isRootedVectorHelperPath(path);
  };
  auto stripRootVectorMethodPrefix = [&](std::string_view path) {
    return stripUnrootedVectorHelperPrefix(path);
  };
  auto stripRootedVectorMethodPrefix = [&](std::string_view path) {
    return stripRootedVectorHelperPrefix(path);
  };
  const std::string explicitRemovedMethodPath =
      explicitRemovedCollectionMethodPathForCallNamespace(methodName, callNamespacePrefix);
  const std::string explicitVectorHelperPath =
      explicitVectorMethodPath(methodName, callNamespacePrefix);
  const std::string explicitKeyValueHelperPath =
      explicitKeyValueMethodPath(methodName, callNamespacePrefix);
  std::string normalizedMethodName = methodName;
  if (!normalizedMethodName.empty() && normalizedMethodName.front() == '/') {
    normalizedMethodName.erase(normalizedMethodName.begin());
  }
  if (startsWithRootVectorMethodPrefix(normalizedMethodName)) {
    normalizedMethodName = std::string(stripRootVectorMethodPrefix(normalizedMethodName));
  } else if (normalizedMethodName.rfind("array/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("array/").size());
  } else if (std::string soaHelperName;
             splitSoaSurfaceHelperPath(normalizedMethodName,
                                       &soaHelperName,
                                       nullptr)) {
    normalizedMethodName = soaHelperName;
  } else if (isUnrootedCanonicalVectorCompatibilityPath(normalizedMethodName)) {
    normalizedMethodName = std::string(
        stripUnrootedCanonicalVectorCompatibilityPrefix(normalizedMethodName));
  } else if (std::string canonicalKeyValueHelperName;
             resolveCanonicalKeyValueHelperNameFromSpelling(
                 normalizedMethodName, canonicalKeyValueHelperName)) {
    normalizedMethodName = canonicalKeyValueHelperName;
  }
  if (resolveCollectionVectorMetadataMethodTarget(normalizedMethodName, receiver, params, locals,
                                                  resolvedOut, isBuiltinOut)) {
    return true;
  }
  std::string canonicalCollectionHelperName = normalizedMethodName;
  if (const size_t specializationSuffix =
          canonicalCollectionHelperName.find("__t");
      specializationSuffix != std::string::npos) {
    canonicalCollectionHelperName.erase(specializationSuffix);
  }
  auto exprKindName = [](Expr::Kind kind) -> const char * {
    switch (kind) {
    case Expr::Kind::Literal:
      return "Literal";
    case Expr::Kind::BoolLiteral:
      return "BoolLiteral";
    case Expr::Kind::FloatLiteral:
      return "FloatLiteral";
    case Expr::Kind::StringLiteral:
      return "StringLiteral";
    case Expr::Kind::Call:
      return "Call";
    case Expr::Kind::Name:
      return "Name";
    }
    return "Unknown";
  };
  const bool traceFileErrorResult =
      normalizedMethodName == "result" &&
      (receiver.name == "FileError" ||
       receiver.name.find("FileError") != std::string::npos ||
       receiver.namespacePrefix.find("FileError") != std::string::npos ||
       callNamespacePrefix.find("FileError") != std::string::npos);
  std::optional<std::string> rememberedMethodTargetTraceFailure;
  auto failMethodTargetResolutionDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(receiver, std::move(message));
  };
  auto rememberMethodTargetTraceFailure = [&](std::string message) {
    if (!error_.empty() || rememberedMethodTargetTraceFailure.has_value()) {
      return;
    }
    rememberedMethodTargetTraceFailure = std::move(message);
  };
  auto stampFileErrorResultFailure = [&](std::string_view site,
                                         std::string_view typeName = {},
                                         std::string_view resolvedType = {}) {
    if (!traceFileErrorResult || !error_.empty() ||
        rememberedMethodTargetTraceFailure.has_value()) {
      return;
    }
    rememberMethodTargetTraceFailure(
        "resolveMethodTarget " + std::string(site) +
        " receiver.kind=" + exprKindName(receiver.kind) +
        " receiver.name=" + receiver.name +
        " receiver.namespace=" + receiver.namespacePrefix +
        " call.namespace=" + callNamespacePrefix +
        " typeName=" + std::string(typeName) +
        " resolvedType=" + std::string(resolvedType));
  };
  if (receiver.kind == Expr::Kind::Name && receiver.name == "FileError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "is_eof" ||
       normalizedMethodName == "eof" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    resolvedOut = preferredFileErrorHelperTarget(normalizedMethodName);
    isBuiltinOut = resolvedOut == "/file_error/why";
    if (resolvedOut.empty() && error_.empty()) {
      const std::string overload1 = "/std/file/FileError/result__ov1";
      std::string programMatch = "none";
      for (const auto &def : program_.definitions) {
        if (def.fullPath.find("FileError/result") != std::string::npos) {
          programMatch = def.fullPath;
          break;
        }
      }
      std::string paramsMatch = "none";
      for (const auto &[path, paramList] : paramsByDef_) {
        (void)paramList;
        if (path.find("FileError/result") != std::string::npos) {
          paramsMatch = path;
          break;
        }
      }
      return failMethodTargetResolutionDiagnostic(
          "preferredFileErrorHelperTarget empty for " + normalizedMethodName +
          " receiver=" + receiver.name +
          " has:/std/file/FileError/result=" +
          (hasDefinitionFamilyPath("/std/file/FileError/result") ? "yes" : "no") +
          " def:/std/file/FileError/result__ov1=" +
          (defMap_.count(overload1) > 0 ? "yes" : "no") +
          " params:/std/file/FileError/result__ov1=" +
          (paramsByDef_.count(overload1) > 0 ? "yes" : "no") +
          " programMatch=" + programMatch + " paramsMatch=" + paramsMatch +
          " has:/std/file/FileError/status=" +
          (hasDefinitionFamilyPath("/std/file/FileError/status") ? "yes"
                                                                 : "no"));
    }
    return !resolvedOut.empty();
  }

  auto isStaticBinding = [&](const Expr &bindingExpr) -> bool {
    for (const auto &transform : bindingExpr.transforms) {
      if (transform.name == "static") {
        return true;
      }
    }
    return false;
  };
  auto resolveStructTypePath = [&](const std::string &typeName,
                                   const std::string &namespacePrefix) -> std::string {
    return this->resolveMethodTargetStructTypePath(typeName, namespacePrefix);
  };
  if (normalizedMethodName == "ok" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
    resolvedOut = "/result/ok";
    isBuiltinOut = true;
    return true;
  }
  if (normalizedMethodName == "error" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
    resolvedOut = "/result/error";
    isBuiltinOut = true;
    return true;
  }
  if (normalizedMethodName == "why" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
    resolvedOut = "/result/why";
    isBuiltinOut = true;
    return true;
  }
  if ((normalizedMethodName == "map" || normalizedMethodName == "and_then" || normalizedMethodName == "map2") &&
      receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
    resolvedOut = "/result/" + normalizedMethodName;
    isBuiltinOut = true;
    return true;
  }
  if (receiver.kind == Expr::Kind::Name &&
      findParamBinding(params, receiver.name) == nullptr &&
      locals.find(receiver.name) == locals.end()) {
    std::string resolvedReceiverPath;
    const std::string rootReceiverPath = "/" + receiver.name;
    if (defMap_.find(rootReceiverPath) != defMap_.end()) {
      resolvedReceiverPath = rootReceiverPath;
    } else {
      auto importIt = importAliases_.find(receiver.name);
      if (importIt != importAliases_.end()) {
        resolvedReceiverPath = importIt->second;
      }
    }
    if (!resolvedReceiverPath.empty() &&
        (structNames_.count(resolvedReceiverPath) > 0 ||
         defMap_.find(resolvedReceiverPath + "/" + normalizedMethodName) != defMap_.end())) {
      resolvedOut = resolvedReceiverPath + "/" + normalizedMethodName;
      return true;
    }
    const std::string resolvedType = resolveStructTypePath(receiver.name, receiver.namespacePrefix);
    if (!resolvedType.empty()) {
      const bool isConcreteExperimentalSoaReceiver =
          isExperimentalSoaVectorSpecializedTypePath(resolvedType);
      const bool isCanonicalSoaWrapperMethod =
          isSupportedCompatibilitySoaHelperName(canonicalCollectionHelperName);
      if (isConcreteExperimentalSoaReceiver && isCanonicalSoaWrapperMethod) {
        resolvedOut = preferredSoaHelperTargetForCollectionType(
            canonicalCollectionHelperName,
            internalSoaCollectionTypePath(true));
        isBuiltinOut = defMap_.count(resolvedOut) == 0 &&
                       !hasImportedDefinitionPath(resolvedOut);
        return true;
      }
      resolvedOut = resolvedType + "/" + normalizedMethodName;
      return true;
    }
  }

  auto resolvesBorrowedExperimentalSoaReceiver = [&](const Expr &candidate) {
    const std::string previousError = error_;
    error_.clear();
    std::string inferredTypeText;
    const bool inferred =
        inferQueryExprTypeText(candidate, params, locals, inferredTypeText);
    error_.clear();
    error_ = previousError;
    if (!inferred) {
      return false;
    }
    std::string ignoredElemType;
    return resolveExperimentalBorrowedSoaTypeText(inferredTypeText,
                                                 ignoredElemType);
  };
  auto preferredSoaToAosHelperTargetForReceiver = [&](const Expr &receiverExpr) {
    if (normalizedMethodName == "to_aos" &&
        resolvesBorrowedExperimentalSoaReceiver(receiverExpr)) {
      return preferredBorrowedSoaAccessHelperTarget(normalizedMethodName);
    }
    return preferredSoaHelperTargetForCollectionType(
        normalizedMethodName, internalSoaCollectionTypePath(true));
  };
  std::function<bool(const Expr &, std::string &)> resolveArgsPackAccessTarget =
      [&](const Expr &target, std::string &elemType) -> bool {
    return this->resolveArgsPackAccessTarget(target, elemType, params, locals);
  };
  auto resolveKeyValueValueType = [&](const Expr &target, std::string &valueTypeOut) -> bool {
    return this->resolveMethodTargetKeyValueValueType(target, valueTypeOut, params, locals,
                                                       resolveArgsPackAccessTarget);
  };
  std::string elemType;
  auto setCollectionMethodTarget = [&](const std::string &path) -> bool {
    return resolveExplicitOrCanonicalCollectionMethodTarget(
        path, explicitRemovedMethodPath, normalizedMethodName, receiver, params, locals,
        resolvedOut, isBuiltinOut);
  };
  auto canonicalVectorHelperTarget = [](std::string_view helperName) {
    return canonicalVectorCompatibilityHelperPathOrFallback(helperName);
  };
  auto setPreferredKeyValueMethodTarget = [&](const Expr &receiverExpr, const std::string &helperName) {
    return this->setPreferredKeyValueMethodTarget(
        receiverExpr, helperName, explicitKeyValueHelperPath, receiver, explicitRemovedMethodPath,
        normalizedMethodName, params, locals, resolvedOut, isBuiltinOut);
  };
  auto resolveExplicitRootKeyValueMethodPath = [&]() -> bool {
    if (!isRootedKeyValueHelperAliasPathForMethodTargets(explicitKeyValueHelperPath)) {
      return false;
    }
    if (hasDeclaredDefinitionPath(explicitKeyValueHelperPath)) {
      resolvedOut = explicitKeyValueHelperPath;
      isBuiltinOut = false;
      return true;
    }
    return failMethodTargetResolutionDiagnostic("unknown method: " +
                                                explicitKeyValueHelperPath);
  };
  if (!explicitKeyValueHelperPath.empty()) {
    const bool resolvedExplicitRootKeyValueMethod =
        resolveExplicitRootKeyValueMethodPath();
    if (resolvedExplicitRootKeyValueMethod || !error_.empty()) {
      return resolvedExplicitRootKeyValueMethod;
    }
  }
  if (normalizedMethodName == "count" &&
      this->resolveArgsPackCountTarget(receiver, elemType, params, locals)) {
    return setCollectionMethodTarget(collection_helpers::kRootedArrayCount);
  }
  if (isValueSurfaceAccessMethodName(normalizedMethodName) &&
      resolveArgsPackAccessTarget(receiver, elemType)) {
    return setCollectionMethodTarget(collection_helpers::kRootedArrayPrefix + normalizedMethodName);
  }
  const std::function<bool(const Expr &, std::string &)> resolveSoaVectorTargetFn =
      [&](const Expr &target, std::string &elemTypeOut) -> bool {
    return this->resolveSoaVectorTarget(target, elemTypeOut, params, locals,
                                        resolveArgsPackAccessTarget);
  };
  auto resolveDirectReceiver = [&](const Expr &directCandidate,
                                   std::string &directElemTypeOut) -> bool {
    return this->resolveDirectSoaVectorOrExperimentalBorrowedReceiver(
        directCandidate, params, locals, resolveSoaVectorTargetFn,
        directElemTypeOut);
  };
  const std::string explicitRemovedVectorReceiverFamily =
      classifyExplicitVectorHelperReceiver(receiver, params, locals);
  if (startsWithRootedVectorMethodPrefix(explicitVectorHelperPath) &&
      (explicitRemovedVectorReceiverFamily == "string" ||
       explicitRemovedVectorReceiverFamily == "array" ||
       explicitRemovedVectorReceiverFamily == "map")) {
    const std::string helperName =
        std::string(stripRootedVectorMethodPrefix(explicitVectorHelperPath));
    if ((explicitRemovedVectorReceiverFamily == "string" ||
         explicitRemovedVectorReceiverFamily == "array") &&
        hasDeclaredDefinitionPath(explicitVectorHelperPath) &&
        explicitVectorCompatHelperFamilyHasCompatibleReceiver(
            explicitVectorHelperPath, explicitRemovedVectorReceiverFamily)) {
      resolvedOut = explicitVectorHelperPath;
      isBuiltinOut = false;
      return true;
    }
    return failMethodTargetResolutionDiagnostic(
        "unknown method: /" + explicitRemovedVectorReceiverFamily + "/" +
        helperName);
  }
  auto isDirectKeyValueConstructorReceiverCall = [&](const Expr &receiverExpr) {
    if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding || receiverExpr.isMethodCall) {
      return false;
    }
    return isResolvedPublishedKeyValueConstructorPath(resolveCalleePath(receiverExpr));
  };
  if ((collection_helpers::isCountHelperName(normalizedMethodName) ||
       normalizedMethodName == "size" ||
       collection_helpers::isContainsHelperName(normalizedMethodName) ||
       collection_helpers::isTryAtHelperName(normalizedMethodName) ||
       isCanonicalKeyValueAccessMethodName(normalizedMethodName) ||
       collection_helpers::isInsertHelperName(normalizedMethodName)) &&
      setIndexedArgsPackKeyValueMethodTarget(
          receiver, normalizedMethodName, explicitKeyValueHelperPath, receiver, explicitRemovedMethodPath,
            normalizedMethodName, params, locals,
            resolvedOut, isBuiltinOut)) {
    return true;
  }
  auto setMethodTargetFromTypeText =
      [&](const std::string &typeText, const std::string &typeNamespace) -> bool {
    const std::string normalizedType =
        normalizeBindingTypeName(unwrapReferencePointerTypeText(typeText));
    if (normalizedType.empty()) {
      return false;
    }
    std::string normalizedBaseType = normalizedType;
    if (!normalizedBaseType.empty() && normalizedBaseType.front() == '/') {
      normalizedBaseType.erase(normalizedBaseType.begin());
    }
    if (normalizedType == "string" &&
        (normalizedMethodName == "count" || normalizedMethodName == "at" ||
         normalizedMethodName == "at_unsafe")) {
      return setCollectionMethodTarget(collection_helpers::kRootedStringPrefix + normalizedMethodName);
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText)) {
      base = normalizeBindingTypeName(base);
      if (base == "vector" &&
          (normalizedMethodName == "count" || normalizedMethodName == "capacity" ||
           normalizedMethodName == "at" || normalizedMethodName == "at_unsafe")) {
        return setCollectionMethodTarget(canonicalVectorHelperTarget(normalizedMethodName));
      }
      if (base == "array" &&
          (normalizedMethodName == "count" || normalizedMethodName == "at" ||
           normalizedMethodName == "at_unsafe")) {
        return setCollectionMethodTarget(collection_helpers::kRootedArrayPrefix + normalizedMethodName);
      }
      const bool isCanonicalSoaWrapperMethod =
          isSupportedCompatibilitySoaHelperName(canonicalCollectionHelperName);
      if ((isInternalSoaCollectionTypeName(base) ||
           (base == "vector" &&
            usesSamePathSoaHelperTargetForCollectionType(canonicalCollectionHelperName, collection_helpers::kRootedVector))) &&
          isCanonicalSoaWrapperMethod) {
        return setCollectionMethodTarget(
            preferredSoaHelperTargetForCollectionType(
                canonicalCollectionHelperName,
                isInternalSoaCollectionTypeName(base)
                    ? internalSoaCollectionTypePath(true)
                    : collection_helpers::kRootedVector));
      }
      if (base == "Buffer" &&
          (normalizedMethodName == "count" || normalizedMethodName == "empty" ||
           normalizedMethodName == "is_valid" || normalizedMethodName == "readback" ||
           normalizedMethodName == "load" || normalizedMethodName == "store")) {
        return setCollectionMethodTarget(preferredBufferMethodTarget(normalizedMethodName));
      }
      if (isKeyValueSurfaceTypeName(base) &&
          (collection_helpers::isCountHelperName(normalizedMethodName) ||
           collection_helpers::isContainsHelperName(normalizedMethodName) ||
           collection_helpers::isTryAtHelperName(normalizedMethodName) ||
           isCanonicalKeyValueAccessMethodName(normalizedMethodName) ||
           collection_helpers::isInsertHelperName(normalizedMethodName))) {
        return setPreferredKeyValueMethodTarget(receiver, normalizedMethodName);
      }
    }
    if (isPrimitiveBindingTypeName(normalizedBaseType)) {
      resolvedOut = "/" + normalizedBaseType + "/" + normalizedMethodName;
      return true;
    }
    std::string resolvedType = resolveStructTypePath(normalizedType, typeNamespace);
    if (resolvedType.empty()) {
      resolvedType = resolveTypePath(normalizedType, typeNamespace);
    }
    if (resolvedType.empty()) {
      return false;
    }
    const bool isConcreteExperimentalSoaReceiver =
        isExperimentalSoaVectorSpecializedTypePath(resolvedType);
    const bool isCanonicalSoaWrapperMethod =
        isSupportedCompatibilitySoaHelperName(canonicalCollectionHelperName);
    if (isConcreteExperimentalSoaReceiver && isCanonicalSoaWrapperMethod) {
      return setCollectionMethodTarget(
          preferredSoaHelperTargetForCollectionType(canonicalCollectionHelperName,
                                                    internalSoaCollectionTypePath(true)));
    }
    resolvedOut = resolvedType + "/" + normalizedMethodName;
    return true;
  };

  if ((collection_helpers::isCountHelperName(normalizedMethodName) ||
       normalizedMethodName == "size" ||
       collection_helpers::isContainsHelperName(normalizedMethodName) ||
       collection_helpers::isTryAtHelperName(normalizedMethodName) ||
       isCanonicalKeyValueAccessMethodName(normalizedMethodName) ||
       collection_helpers::isInsertHelperName(normalizedMethodName)) &&
      isDirectKeyValueConstructorReceiverCall(receiver)) {
    std::string keyType;
    std::string valueType;
    if (resolveExperimentalKeyValueTarget(receiver, keyType, valueType, params, locals)) {
      return failMethodTargetResolutionDiagnostic(
          "unknown call target: " +
          this->preferredCanonicalExperimentalKeyValueHelperTarget(
              normalizedMethodName));
    }
    return setPreferredKeyValueMethodTarget(receiver, normalizedMethodName);
  }
  auto explicitVectorReceiverFamily =
      classifyExplicitVectorHelperReceiver(receiver, params, locals);
  const bool isExplicitVectorFamilyReceiver =
      explicitVectorReceiverFamily == "vector" ||
      explicitVectorReceiverFamily ==
          legacyExperimentalVectorCompatibilityFamilyName() ||
      isInternalSoaCollectionTypeName(explicitVectorReceiverFamily);
  const std::string_view explicitRootedVectorHelperName =
      startsWithRootedVectorMethodPrefix(explicitVectorHelperPath)
          ? stripRootedVectorMethodPrefix(explicitVectorHelperPath)
          : std::string_view{};
  const bool isExplicitRootedVectorMethod =
      !explicitRootedVectorHelperName.empty() &&
      isRemovedVectorCompatibilityHelper(explicitRootedVectorHelperName);
  if (isExplicitRootedVectorMethod && isExplicitVectorFamilyReceiver) {
    if (hasReceiverCompatibleExplicitVectorHelperPath(
            explicitVectorHelperPath, receiver, params, locals)) {
      resolvedOut = explicitVectorHelperPath;
      isBuiltinOut = false;
      return true;
    }
    return failMethodTargetResolutionDiagnostic(
        "unknown method: " + explicitVectorHelperPath);
  }
  // receiver.isMethodCall here means "was this call written with dot-call
  // syntax" (see Ast.h), not "does this resolve to a method" - it does not
  // rule out a bare-call-syntax vector-compatibility-helper call below.
  if (receiver.isMethodCall && !explicitVectorHelperPath.empty() && !isExplicitVectorFamilyReceiver &&
      isVectorCompatibilityHelperName(normalizedMethodName)) {
    if (hasDeclaredDefinitionPath(explicitVectorHelperPath) ||
        hasImportedDefinitionPath(explicitVectorHelperPath)) {
      resolvedOut = explicitVectorHelperPath;
      isBuiltinOut = false;
      return true;
    }
    const std::string preferredExplicitVectorHelperPath =
        preferVectorStdlibHelperPath(explicitVectorHelperPath);
    if (normalizedMethodName == "capacity") {
      return failMethodTargetResolutionDiagnostic("capacity requires vector target");
    }
    return failMethodTargetResolutionDiagnostic(receiver.isMethodCall
                                                   ? "unknown method: " +
                                                         preferredExplicitVectorHelperPath
                                                   : "unknown call target: " +
                                                         preferredExplicitVectorHelperPath);
  }
  const bool usesBuiltinVectorMethodSemantics =
      normalizedMethodName == "count" || normalizedMethodName == "capacity" ||
      normalizedMethodName == "at" || normalizedMethodName == "at_unsafe";
  if (!usesBuiltinVectorMethodSemantics &&
      preferExplicitCanonicalVectorHelperForReceiver(
          receiver, explicitVectorHelperPath, params, locals)) {
    resolvedOut = explicitVectorHelperPath;
    isBuiltinOut = false;
    return true;
  }
  if (collection_helpers::isCountHelperName(normalizedMethodName) ||
      normalizedMethodName == "size") {
    if (normalizedMethodName == "count" &&
        this->resolveArgsPackCountTarget(receiver, elemType, params, locals)) {
      return setCollectionMethodTarget(collection_helpers::kRootedArrayCount);
    }
    if (this->resolveVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget) &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector)) {
      return setCollectionMethodTarget(
          preferredSoaHelperTargetForCollectionType(normalizedMethodName,
                                                    collection_helpers::kRootedVector));
    }
    if (normalizedMethodName == "count" &&
        this->resolveVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget)) {
      return setCollectionMethodTarget(canonicalVectorHelperTarget("count"));
    }
    if (normalizedMethodName == "count" &&
        this->resolveCollectionVectorValueTarget(receiver, elemType, params, locals)) {
      return setCollectionMethodTarget(canonicalVectorHelperTarget("count"));
    }
    if (this->resolveSoaVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget)) {
      return setCollectionMethodTarget(
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedSoa));
    }
    if ((collection_helpers::isCountHelperName(normalizedMethodName)) &&
        this->resolveSoaVectorOrExperimentalBorrowedReceiver(
            receiver, params, locals, resolveDirectReceiver, elemType)) {
      return setCollectionMethodTarget(
          preferredBorrowedSoaAccessHelperTarget(normalizedMethodName));
    }
    if (normalizedMethodName == "count" &&
        this->resolveArrayTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget)) {
      if (auto explicitTarget = tryResolveExplicitCanonicalVectorCountMethodTarget(
              receiver, explicitVectorHelperPath, normalizedMethodName, params, locals,
              resolvedOut, isBuiltinOut);
          explicitTarget.has_value()) {
        return *explicitTarget;
      }
      return setCollectionMethodTarget(collection_helpers::kRootedArrayCount);
    }
    if (normalizedMethodName == "count" &&
        this->resolveStringTarget(receiver, params, locals, resolveArgsPackAccessTarget)) {
      if (auto explicitTarget = tryResolveExplicitCanonicalVectorCountMethodTarget(
              receiver, explicitVectorHelperPath, normalizedMethodName, params, locals,
              resolvedOut, isBuiltinOut);
          explicitTarget.has_value()) {
        return *explicitTarget;
      }
      return setCollectionMethodTarget(collection_helpers::kRootedStringCount);
    }
    if (normalizedMethodName == "count" &&
        setIndexedArgsPackKeyValueMethodTarget(
            receiver, "count", explicitKeyValueHelperPath, receiver, explicitRemovedMethodPath,
            normalizedMethodName, params, locals,
            resolvedOut, isBuiltinOut)) {
      return true;
    }
    if (this->resolveKeyValueTarget(receiver, params, locals, resolveArgsPackAccessTarget)) {
      if (normalizedMethodName == "count") {
        if (auto explicitTarget = tryResolveExplicitCanonicalVectorCountMethodTarget(
              receiver, explicitVectorHelperPath, normalizedMethodName, params, locals,
              resolvedOut, isBuiltinOut);
            explicitTarget.has_value()) {
          return *explicitTarget;
        }
      }
      return setPreferredKeyValueMethodTarget(receiver, normalizedMethodName);
    }
  }
  if (normalizedMethodName == "contains" || normalizedMethodName == "tryAt" ||
      normalizedMethodName == "insert") {
    if (setIndexedArgsPackKeyValueMethodTarget(
            receiver, normalizedMethodName, explicitKeyValueHelperPath, receiver, explicitRemovedMethodPath,
            normalizedMethodName, params, locals,
            resolvedOut, isBuiltinOut)) {
      return true;
    }
    if (normalizedMethodName != "insert" &&
        this->resolveKeyValueTarget(receiver, params, locals, resolveArgsPackAccessTarget)) {
      return setPreferredKeyValueMethodTarget(receiver, normalizedMethodName);
    }
  }
  if (normalizedMethodName == "insert") {
    if (this->resolveKeyValueTarget(receiver, params, locals, resolveArgsPackAccessTarget)) {
      return setPreferredKeyValueMethodTarget(receiver, "insert");
    }
  }
  if (normalizedMethodName == "capacity") {
    if (this->resolveArrayTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget) &&
        (hasDeclaredDefinitionPath(collection_helpers::kRootedArrayCapacity) ||
         hasImportedDefinitionPath(collection_helpers::kRootedArrayCapacity))) {
      return setCollectionMethodTarget(collection_helpers::kRootedArrayCapacity);
    }
    if (this->resolveVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget)) {
      return setCollectionMethodTarget(canonicalVectorHelperTarget("capacity"));
    }
    if (this->resolveCollectionVectorValueTarget(receiver, elemType, params, locals)) {
      return setCollectionMethodTarget(canonicalVectorHelperTarget("capacity"));
    }
  }
  if (isValueSurfaceAccessMethodName(normalizedMethodName)) {
    if (resolveArgsPackAccessTarget(receiver, elemType)) {
      return setCollectionMethodTarget(collection_helpers::kRootedArrayPrefix + normalizedMethodName);
    }
    if (this->resolveVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget)) {
      return setCollectionMethodTarget(canonicalVectorHelperTarget(normalizedMethodName));
    }
    if (this->resolveCollectionVectorValueTarget(receiver, elemType, params, locals)) {
      return setCollectionMethodTarget(canonicalVectorHelperTarget(normalizedMethodName));
    }
    if (this->resolveArrayTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget)) {
      return setCollectionMethodTarget(collection_helpers::kRootedArrayPrefix + normalizedMethodName);
    }
    if (this->resolveStringTarget(receiver, params, locals, resolveArgsPackAccessTarget)) {
      return setCollectionMethodTarget(collection_helpers::kRootedStringPrefix + normalizedMethodName);
    }
  }
  if (isCanonicalKeyValueAccessMethodName(normalizedMethodName) &&
      setIndexedArgsPackKeyValueMethodTarget(
          receiver, normalizedMethodName, explicitKeyValueHelperPath, receiver, explicitRemovedMethodPath,
            normalizedMethodName, params, locals,
            resolvedOut, isBuiltinOut)) {
    return true;
  }
  if (isCanonicalKeyValueAccessMethodName(normalizedMethodName) &&
      this->resolveKeyValueTarget(receiver, params, locals, resolveArgsPackAccessTarget)) {
    return setPreferredKeyValueMethodTarget(receiver, normalizedMethodName);
  }
  if (collection_helpers::isGetHelperName(normalizedMethodName)) {
    if (this->resolveVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget) &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName,
                                                     collection_helpers::kRootedVector)) {
      return setCollectionMethodTarget(
          preferredSoaHelperTargetForCollectionType(normalizedMethodName,
                                                    collection_helpers::kRootedVector));
    }
    if ((collection_helpers::isGetHelperName(normalizedMethodName)) &&
        resolveBorrowedVectorReceiver(receiver, elemType, params, locals) &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName,
                                                     collection_helpers::kRootedVector)) {
      return setCollectionMethodTarget(
          preferredSoaHelperTargetForCollectionType(normalizedMethodName,
                                                    collection_helpers::kRootedVector));
    }
    if (this->resolveSoaVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget)) {
      return setCollectionMethodTarget(
          preferredSoaHelperTargetForCollectionType(normalizedMethodName,
                                                    internalSoaCollectionTypePath(true)));
    }
    if ((collection_helpers::isGetHelperName(normalizedMethodName)) &&
        this->resolveSoaVectorOrExperimentalBorrowedReceiver(
            receiver, params, locals, resolveDirectReceiver, elemType)) {
      return setCollectionMethodTarget(
          preferredBorrowedSoaAccessHelperTarget(normalizedMethodName));
    }
  }
  if (collection_helpers::isRefHelperName(normalizedMethodName)) {
    if (this->resolveVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget) &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector)) {
      return setCollectionMethodTarget(
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector));
    }
    if (this->resolveSoaVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget)) {
      return setCollectionMethodTarget(
          preferredSoaHelperTargetForCollectionType(
              normalizedMethodName, internalSoaCollectionTypePath(true)));
    }
    if (this->resolveSoaVectorOrExperimentalBorrowedReceiver(
            receiver, params, locals, resolveDirectReceiver, elemType)) {
      return setCollectionMethodTarget(
          preferredSoaToAosHelperTargetForReceiver(receiver));
    }
  }
  if (collection_helpers::isToAosHelperName(normalizedMethodName)) {
    if (this->resolveVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget)) {
      return setCollectionMethodTarget(
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector));
    }
    // Direct (owned) soa receivers must be tried before the OR-combined
    // resolveSoaVectorOrExperimentalBorrowedReceiver check below: that
    // check's "direct" branch also matches an owned receiver (it is an OR
    // of "direct" and "borrowed"), but its result unconditionally builds
    // the borrowed *_ref target name, so checking it first would route an
    // owned receiver to the borrowed helper. get/ref above already use
    // this same ordering (direct soa check before the OR-combined check).
    if (this->resolveSoaVectorTarget(receiver, elemType, params, locals, resolveArgsPackAccessTarget)) {
      return setCollectionMethodTarget(
          preferredSoaHelperTargetForCollectionType(
              normalizedMethodName, internalSoaCollectionTypePath(true)));
    }
    if (this->resolveSoaVectorOrExperimentalBorrowedReceiver(
            receiver, params, locals, resolveDirectReceiver, elemType)) {
      return setCollectionMethodTarget(
          preferredBorrowedSoaAccessHelperTarget(normalizedMethodName));
    }
  }
  if (this->resolveSoaVectorOrExperimentalBorrowedReceiver(
          receiver, params, locals, resolveDirectReceiver, elemType)) {
    const std::string normalizedElemType = normalizeBindingTypeName(elemType);
    std::string currentNamespace;
    if (!currentValidationState_.context.definitionPath.empty()) {
      const size_t slash = currentValidationState_.context.definitionPath.find_last_of('/');
      if (slash != std::string::npos && slash > 0) {
        currentNamespace = currentValidationState_.context.definitionPath.substr(0, slash);
      }
    }
    const std::string lookupNamespace =
        !receiver.namespacePrefix.empty() ? receiver.namespacePrefix : currentNamespace;
    const std::string elementStructPath = resolveStructTypePath(normalizedElemType, lookupNamespace);
    if (!elementStructPath.empty()) {
      auto structIt = defMap_.find(elementStructPath);
      if (structIt != defMap_.end() && structIt->second != nullptr) {
        for (const auto &stmt : structIt->second->statements) {
          if (!stmt.isBinding || isStaticBinding(stmt) || stmt.name != normalizedMethodName) {
            continue;
          }
          if (hasVisibleSoaHelperTargetForCurrentImports(normalizedMethodName)) {
            resolvedOut =
                preferredSoaHelperTargetForCurrentImports(normalizedMethodName);
            isBuiltinOut = false;
          } else {
            resolvedOut = soaFieldViewHelperPath(normalizedMethodName);
            isBuiltinOut = true;
          }
          return true;
        }
      }
    }
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    const std::string removedVectorMethodCompatibilityPath =
        receiver.isMethodCall
            ? this->explicitRemovedCollectionMethodPath(receiver.name, receiver.namespacePrefix)
            : std::string();
    std::string accessHelperName;
    if (getBuiltinArrayAccessName(receiver, accessHelperName) && !receiver.args.empty()) {
      const std::string removedKeyValueCompatibilityPath =
          getDirectKeyValueHelperCompatibilityPath(receiver, params, locals,
                                                   resolveArgsPackAccessTarget);
      if (!removedKeyValueCompatibilityPath.empty()) {
        return failMethodTargetResolutionDiagnostic("unknown call target: " +
                                                    removedKeyValueCompatibilityPath);
      }
      size_t accessReceiverIndex = 0;
      if (!receiver.isMethodCall && hasNamedArguments(receiver.argNames)) {
        bool foundValues = false;
        for (size_t i = 0; i < receiver.args.size(); ++i) {
          if (i < receiver.argNames.size() && receiver.argNames[i].has_value() &&
              *receiver.argNames[i] == "values") {
            accessReceiverIndex = i;
            foundValues = true;
            break;
          }
        }
        if (!foundValues) {
          accessReceiverIndex = 0;
        }
      }
      if (accessReceiverIndex < receiver.args.size()) {
        const Expr &accessReceiver = receiver.args[accessReceiverIndex];
        const std::string removedVectorAccessCompatibilityPath =
            receiver.isMethodCall
                ? this->explicitRemovedCollectionMethodPath(receiver.name, receiver.namespacePrefix)
                : std::string();
        const bool hasSamePathRemovedVectorAccessHelper =
            !removedVectorAccessCompatibilityPath.empty() &&
            (hasDefinitionPath(removedVectorAccessCompatibilityPath) ||
             hasImportedDefinitionPath(removedVectorAccessCompatibilityPath));
        if ((removedVectorAccessCompatibilityPath == collection_helpers::kRootedArrayAt ||
             removedVectorAccessCompatibilityPath == collection_helpers::kRootedArrayAtUnsafe) &&
            !hasSamePathRemovedVectorAccessHelper) {
          std::string vectorElemType;
          if (this->resolveVectorTarget(accessReceiver, vectorElemType, params, locals,
                                        resolveArgsPackAccessTarget)) {
            return failMethodTargetResolutionDiagnostic("unknown method: " +
                                                        removedVectorAccessCompatibilityPath);
          }
        }
        std::string indexedArgsPackElemType;
        if (!resolveArgsPackAccessTarget(accessReceiver, indexedArgsPackElemType)) {
          auto accessDefIt = defMap_.find(resolveCalleePath(receiver));
          if (accessDefIt != defMap_.end() && accessDefIt->second != nullptr) {
            BindingInfo inferredReturn;
            if (inferDefinitionReturnBinding(*accessDefIt->second, inferredReturn)) {
              const std::string inferredTypeText =
                  inferredReturn.typeTemplateArg.empty()
                      ? inferredReturn.typeName
                      : inferredReturn.typeName + "<" + inferredReturn.typeTemplateArg + ">";
              if (setMethodTargetFromTypeText(
                      inferredTypeText,
                      accessDefIt->second->namespacePrefix)) {
                return true;
              }
            }
          }
        }
        std::string accessElemType;
        std::string accessValueType;
        if (resolveArgsPackAccessTarget(accessReceiver, accessElemType) ||
            this->resolveVectorTarget(accessReceiver, accessElemType, params, locals,
                                      resolveArgsPackAccessTarget) ||
            this->resolveArrayTarget(accessReceiver, accessElemType, params, locals,
                                     resolveArgsPackAccessTarget)) {
          const std::string normalizedElemType =
              normalizeBindingTypeName(unwrapReferencePointerTypeText(accessElemType));
          std::string normalizedElemBaseType = normalizedElemType;
          if (!normalizedElemBaseType.empty() && normalizedElemBaseType.front() == '/') {
            normalizedElemBaseType.erase(normalizedElemBaseType.begin());
          }

          // Step 2 of docs/ReceiverTargetResolutionConsolidation.md
          // (second migration): this block's own inline classification
          // cascade (a near-duplicate of resolveArgsPackElementMethodTarget's
          // R1-R9, Step 1b slice 2's own harnessed call site) has been
          // replaced by a single call into the shared classifier, proven
          // byte-faithful by that slice's diff-audit harness. Two
          // site-specific input conventions (documented on
          // classifyReceiverElementFamilyJoint's own header, "slice 2"):
          // this call site's element text is already Reference/Pointer-
          // unwrapped before either classifier input is computed, so both
          // `unwrappedElementType` and `rawElementBaseType` below use the
          // exact same normalizedElemType/normalizedElemBaseType text (no
          // wrapped/unwrapped asymmetry, unlike
          // resolveArgsPackElementMethodTarget's R7); and this block's own
          // FileError check historically sits after the template-shape
          // block, which is provably equivalent to the classifier's fixed
          // FileError-before-template ordering since a template-shaped
          // base name can never equal the bare literal "FileError". Only
          // the *downstream* action per family below is unchanged from the
          // pre-migration inline cascade.
          std::string elemBase;
          std::string elemArgText;
          const bool isTemplateShaped2 =
              splitTemplateTypeName(normalizedElemType, elemBase, elemArgText);
          if (isTemplateShaped2) {
            elemBase = normalizeBindingTypeName(elemBase);
          }
          primec::ReceiverElementFamilyJointInput jointInput2;
          jointInput2.unwrappedElementType = normalizedElemType;
          jointInput2.rawElementBaseType = normalizedElemBaseType;
          jointInput2.isTemplateShaped = isTemplateShaped2;
          jointInput2.templateShapedBaseName = elemBase;
          jointInput2.normalizedMethodName = normalizedMethodName;
          primec::ReceiverElementFamilyPredicates predicates2{
              [](std::string_view name) {
                return isInternalSoaCollectionTypeName(name);
              },
              [](std::string_view name) {
                return isKeyValueSurfaceTypeName(std::string(name));
              },
          };
          const primec::ReceiverElementFamilyResult classified2 =
              primec::classifyReceiverElementFamilyJoint(jointInput2, predicates2);

          switch (classified2.family) {
            case primec::ReceiverElementFamily::String:
              return setCollectionMethodTarget(collection_helpers::kRootedStringPrefix + normalizedMethodName);
            case primec::ReceiverElementFamily::VectorLike:
            case primec::ReceiverElementFamily::Soa:
              // classified2.collectionBaseName is the already-normalized
              // elemBase the pre-migration cascade used here.
              return setCollectionMethodTarget("/" + classified2.collectionBaseName + "/" +
                                                normalizedMethodName);
            case primec::ReceiverElementFamily::Buffer:
              return setCollectionMethodTarget(preferredBufferMethodTarget(normalizedMethodName));
            case primec::ReceiverElementFamily::KeyValue:
              if (setIndexedArgsPackKeyValueMethodTarget(
                      receiver, normalizedMethodName, explicitKeyValueHelperPath, receiver, explicitRemovedMethodPath,
                      normalizedMethodName, params, locals,
                      resolvedOut, isBuiltinOut)) {
                return true;
              }
              return setPreferredKeyValueMethodTarget(receiver, normalizedMethodName);
            case primec::ReceiverElementFamily::File:
              resolvedOut = preferredFileHelperTarget(normalizedMethodName,
                                                     currentValidationState_.context.definitionPath);
              isBuiltinOut = (resolvedOut.rfind("/file/", 0) == 0);
              return true;
            case primec::ReceiverElementFamily::FileError:
              resolvedOut = preferredFileErrorHelperTarget(normalizedMethodName);
              isBuiltinOut = resolvedOut == "/file_error/why";
              return !resolvedOut.empty();
            case primec::ReceiverElementFamily::Primitive:
              // No wrapped/unwrapped asymmetry at this call site (see the
              // header comment above) - normalizedElemBaseType is safe to
              // use directly here, unlike resolveArgsPackElementMethodTarget's
              // R7, which must keep its own separately-tracked raw text.
              resolvedOut = "/" + normalizedElemBaseType + "/" + normalizedMethodName;
              return true;
            case primec::ReceiverElementFamily::StructOrUnknown:
            default:
              break;
          }
          std::string resolvedElemType = resolveStructTypePath(normalizedElemType, receiver.namespacePrefix);
          if (resolvedElemType.empty()) {
            resolvedElemType = resolveTypePath(normalizedElemType, receiver.namespacePrefix);
          }
          if (!resolvedElemType.empty()) {
            resolvedOut = resolvedElemType + "/" + normalizedMethodName;
            return true;
          }
          // No return here: production falls through to further,
          // unrelated fallback resolution below (resolveStringTarget,
          // resolveKeyValueValueType, ...) when the struct-type-path
          // fallback itself fails - this cascade does not "commit" to
          // StructOrUnknown as ITS OWN final answer in that case (unlike
          // resolveArgsPackElementMethodTarget's R9, which does return
          // false as this function's own terminal verdict).
        }
        if (this->resolveStringTarget(accessReceiver, params, locals, resolveArgsPackAccessTarget)) {
          resolvedOut = "/i32/" + normalizedMethodName;
          return true;
        }
        if (resolveKeyValueValueType(accessReceiver, accessValueType)) {
          std::string normalizedAccessName = receiver.name;
          if (!normalizedAccessName.empty() &&
              normalizedAccessName.front() == '/') {
            normalizedAccessName.erase(normalizedAccessName.begin());
          }
          const size_t accessTemplateSuffix = normalizedAccessName.find("__t");
          if (accessTemplateSuffix != std::string::npos) {
            normalizedAccessName.erase(accessTemplateSuffix);
          }
          const bool isExplicitAccessAlias =
              normalizedAccessName.find('/') != std::string::npos;
          const std::string preferredAccessPath =
              preferredKeyValueMethodTarget(receiver, accessHelperName, explicitKeyValueHelperPath,
                                            params, locals, resolveArgsPackAccessTarget);
          auto defIt = defMap_.find(preferredAccessPath);
          if (defIt != defMap_.end() && defIt->second != nullptr) {
            BindingInfo inferredReturn;
            if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
              const std::string inferredReturnTypeText =
                  inferredReturn.typeTemplateArg.empty()
                      ? inferredReturn.typeName
                      : inferredReturn.typeName + "<" + inferredReturn.typeTemplateArg + ">";
              const std::string normalizedReturnType =
                  normalizeBindingTypeName(inferredReturnTypeText);
              std::string normalizedReturnBaseType = normalizedReturnType;
              if (!normalizedReturnBaseType.empty() &&
                  normalizedReturnBaseType.front() == '/') {
                normalizedReturnBaseType.erase(normalizedReturnBaseType.begin());
              }
              if (isPrimitiveBindingTypeName(normalizedReturnBaseType)) {
                resolvedOut = "/" + normalizedReturnBaseType + "/" +
                              normalizedMethodName;
                return true;
              }
              std::string resolvedReturnType = resolveStructTypePath(
                  normalizedReturnType, defIt->second->namespacePrefix);
              if (resolvedReturnType.empty()) {
                resolvedReturnType =
                    resolveTypePath(normalizedReturnType,
                                    defIt->second->namespacePrefix);
              }
              if (!resolvedReturnType.empty()) {
                if (tryRedirectConcreteExperimentalSoaMethodTarget(
                        resolvedReturnType, canonicalCollectionHelperName, receiver,
                        explicitRemovedMethodPath, normalizedMethodName, params, locals,
                        resolvedOut, isBuiltinOut)) {
                  return true;
                }
                resolvedOut = resolvedReturnType + "/" + normalizedMethodName;
                return true;
              }
            }
          }

          if (!isExplicitAccessAlias ||
              ((defIt != defMap_.end() && defIt->second != nullptr) ||
               hasImportedDefinitionPath(preferredAccessPath))) {
            const std::string normalizedValueType =
                normalizeBindingTypeName(accessValueType);
            std::string normalizedValueBaseType = normalizedValueType;
            if (!normalizedValueBaseType.empty() &&
                normalizedValueBaseType.front() == '/') {
              normalizedValueBaseType.erase(normalizedValueBaseType.begin());
            }
            if (isPrimitiveBindingTypeName(normalizedValueBaseType)) {
              resolvedOut = "/" + normalizedValueBaseType + "/" +
                            normalizedMethodName;
              return true;
            }
            std::string resolvedValueType =
                resolveStructTypePath(normalizedValueType, receiver.namespacePrefix);
            if (resolvedValueType.empty()) {
              resolvedValueType =
                  resolveTypePath(normalizedValueType, receiver.namespacePrefix);
            }
            if (!resolvedValueType.empty()) {
              resolvedOut = resolvedValueType + "/" + normalizedMethodName;
              return true;
            }
          }
        }
      }
    }
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    std::string dereferencedElemType;
    if (resolveDereferencedIndexedArgsPackElementType(receiver, dereferencedElemType,
                                                       resolveArgsPackAccessTarget) &&
        resolveArgsPackElementMethodTarget(
            dereferencedElemType, receiver, normalizedMethodName, setCollectionMethodTarget,
            setPreferredKeyValueMethodTarget, resolvedOut, isBuiltinOut)) {
      return true;
    }
  }
  {
    bool handledRetiredMaybeMutableHelper = false;
    if (bool ok = resolveRetiredMaybeMutableHelperMethodTarget(
            receiver, normalizedMethodName, params, locals,
            handledRetiredMaybeMutableHelper);
        handledRetiredMaybeMutableHelper) {
      return ok;
    }
  }
  if (isStdlibSurfaceMemberName(StdlibSurfaceId::CollectionsManifestSurface0, normalizedMethodName)) {
    std::string vectorMethodTarget;
    if (resolveVectorHelperMethodTarget(params, locals, receiver, normalizedMethodName,
                                        vectorMethodTarget)) {
      const bool isVectorFamilyTarget =
          splitSoaSurfaceHelperPath(vectorMethodTarget, nullptr, nullptr) ||
          startsWithRootedVectorMethodPrefix(vectorMethodTarget) ||
          isCanonicalVectorCompatibilityPath(vectorMethodTarget) ||
          isLegacyExperimentalVectorCompatibilityPath(vectorMethodTarget);
      if (!isVectorFamilyTarget) {
        vectorMethodTarget.clear();
      }
    }
    if (!vectorMethodTarget.empty()) {
      if (isLegacyExperimentalVectorCompatibilityPath(vectorMethodTarget) &&
          !hasDeclaredDefinitionPath(vectorMethodTarget) &&
          !hasImportedDefinitionPath(vectorMethodTarget)) {
        const std::string canonicalVectorMethodTarget =
            canonicalVectorHelperTarget(normalizedMethodName);
        if (isPublishedVectorMutatorHelperName(normalizedMethodName) ||
            hasDeclaredDefinitionPath(canonicalVectorMethodTarget) ||
            hasImportedDefinitionPath(canonicalVectorMethodTarget)) {
          vectorMethodTarget = canonicalVectorMethodTarget;
        }
      }
      return setCollectionMethodTarget(vectorMethodTarget);
    }
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    const std::string resolvedType = resolveCalleePath(receiver);
    if (!resolvedType.empty() && structNames_.count(resolvedType) > 0) {
      std::string experimentalElemType;
      BindingInfo receiverBinding;
      receiverBinding.typeName = resolvedType;
      const bool isConcreteExperimentalSoaReceiver =
          isExperimentalSoaVectorSpecializedTypePath(resolvedType);
      const bool isCanonicalSoaWrapperMethod =
          isSupportedCompatibilitySoaHelperName(canonicalCollectionHelperName);
      if (isConcreteExperimentalSoaReceiver && isCanonicalSoaWrapperMethod) {
        return setCollectionMethodTarget(
            preferredSoaHelperTargetForCollectionType(canonicalCollectionHelperName,
                                                      internalSoaCollectionTypePath(true)));
      }
      if ((((collection_helpers::isCountHelperName(normalizedMethodName)) &&
            usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector)) ||
           normalizedMethodName == "capacity" ||
           normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") &&
          extractCollectionVectorElementType(receiverBinding, experimentalElemType)) {
        if ((collection_helpers::isCountHelperName(normalizedMethodName)) &&
            usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector)) {
          return setCollectionMethodTarget(
              preferredSoaHelperTargetForCollectionType(normalizedMethodName,
                                                        collection_helpers::kRootedVector));
        }
        if (normalizedMethodName == "count") {
          return setCollectionMethodTarget(canonicalVectorHelperTarget("count"));
        }
        if (normalizedMethodName == "capacity") {
          return setCollectionMethodTarget(canonicalVectorHelperTarget("capacity"));
        }
        return setCollectionMethodTarget(canonicalVectorHelperTarget(normalizedMethodName));
      }
      if (extractExperimentalSoaVectorElementType(receiverBinding, experimentalElemType) &&
          resolveCollectionMethodFromTypePath(
              internalSoaCollectionTypePath(true), normalizedMethodName, receiver,
              explicitVectorHelperPath, explicitKeyValueHelperPath, explicitRemovedMethodPath,
              params, locals,
              resolvedOut, isBuiltinOut)) {
        return true;
      }
      resolvedOut = resolvedType + "/" + normalizedMethodName;
      return true;
    }
    if (resolveExplicitDirectCallReturnMethodTarget(
            receiver, canonicalCollectionHelperName, normalizedMethodName, receiver,
            explicitRemovedMethodPath, params, locals, resolvedOut, isBuiltinOut)) {
      return true;
    }
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding &&
      (normalizedMethodName == "count" || normalizedMethodName == "capacity" ||
       normalizedMethodName == "at" || normalizedMethodName == "at_unsafe")) {
    std::string receiverCollectionTypePath;
    if (resolveCallCollectionTypePath(receiver, params, locals, receiverCollectionTypePath) &&
        receiverCollectionTypePath == collection_helpers::kRootedVector) {
      if (normalizedMethodName == "count") {
        return setCollectionMethodTarget(canonicalVectorHelperTarget("count"));
      }
      if (normalizedMethodName == "capacity") {
        return setCollectionMethodTarget(canonicalVectorHelperTarget("capacity"));
      }
      return setCollectionMethodTarget(canonicalVectorHelperTarget(normalizedMethodName));
    }
  }
  if (receiver.kind == Expr::Kind::Call) {
    std::string receiverCollectionTypePath;
    if (resolveCallCollectionTypePath(receiver, params, locals,
                                      receiverCollectionTypePath)) {
      if (resolveCollectionMethodFromTypePath(
              receiverCollectionTypePath, normalizedMethodName, receiver,
              explicitVectorHelperPath, explicitKeyValueHelperPath, explicitRemovedMethodPath,
              params, locals,
              resolvedOut, isBuiltinOut)) {
        return true;
      }
    }
  }

  return resolveMethodTargetGenericFallback(
      params, locals, callNamespacePrefix, receiver, normalizedMethodName,
      canonicalCollectionHelperName, explicitVectorHelperPath, explicitKeyValueHelperPath,
      explicitRemovedMethodPath, traceFileErrorResult,
      failMethodTargetResolutionDiagnostic, stampFileErrorResultFailure, resolvedOut,
      isBuiltinOut);
}


} // namespace primec::semantics
