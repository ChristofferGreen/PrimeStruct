#include "SemanticsValidator.h"

#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "SemanticsValidatorInferMethodResolutionHelpers.h"

#include <algorithm>
#include <functional>
#include <string_view>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec::semantics {

using namespace inferMethodResolutionHelpers;

bool SemanticsValidator::resolveInferMethodCallPath(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::string &methodName,
    std::string &resolvedOut) {
  const Definition *activeDefinition = currentDefinitionContext_;
  const Execution *activeExecution = currentExecutionContext_;
  const bool hasScopedOwner = activeDefinition != nullptr || activeExecution != nullptr;
  if (hasScopedOwner &&
      (callTargetResolutionScratch_.definitionOwner != activeDefinition ||
       callTargetResolutionScratch_.executionOwner != activeExecution)) {
    callTargetResolutionScratch_.resetArena();
    callTargetResolutionScratch_.definitionOwner = activeDefinition;
    callTargetResolutionScratch_.executionOwner = activeExecution;
  } else if (!hasScopedOwner &&
             (!callTargetResolutionScratch_.definitionFamilyPathCache.empty() ||
              !callTargetResolutionScratch_.overloadFamilyPathCache.empty() ||
              !callTargetResolutionScratch_.overloadFamilyPrefixCache.empty() ||
              !callTargetResolutionScratch_.specializationPrefixCache.empty() ||
              !callTargetResolutionScratch_.overloadCandidatePathCache.empty() ||
              !callTargetResolutionScratch_.normalizedMethodNameCache.empty() ||
              !callTargetResolutionScratch_.explicitRemovedMethodPathCache.empty() ||
              !callTargetResolutionScratch_.concreteCallBaseCandidates.empty() ||
              !callTargetResolutionScratch_.methodReceiverResolutionCandidates.empty() ||
              !callTargetResolutionScratch_.canonicalReceiverAliasPathCache.empty() ||
              callTargetResolutionScratch_.definitionOwner != nullptr ||
              callTargetResolutionScratch_.executionOwner != nullptr)) {
    callTargetResolutionScratch_.resetArena();
  }

  auto resolveArgsPackCountTarget = [&](const Expr &target, std::string &elemType) -> bool {
    return resolveInferArgsPackCountTarget(params, locals, target, elemType);
  };
  const BuiltinCollectionDispatchResolverAdapters builtinCollectionDispatchResolverAdapters;
  const BuiltinCollectionDispatchResolvers builtinCollectionDispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals, builtinCollectionDispatchResolverAdapters);
  const auto &resolveArgsPackAccessTarget = builtinCollectionDispatchResolvers.resolveArgsPackAccessTarget;
  const auto &resolveArrayTarget = builtinCollectionDispatchResolvers.resolveArrayTarget;
  const auto &resolveVectorTarget = builtinCollectionDispatchResolvers.resolveVectorTarget;
  const auto &resolveSoaVectorTarget = builtinCollectionDispatchResolvers.resolveSoaVectorTarget;
  const auto &resolveBufferTarget = builtinCollectionDispatchResolvers.resolveBufferTarget;
  const auto &resolveStringTarget = builtinCollectionDispatchResolvers.resolveStringTarget;
  const auto &resolveKeyValueTarget =
      builtinCollectionDispatchResolvers.resolveMapTarget;

  if (expr.args.empty()) {
    return false;
  }
  const Expr &receiver = expr.args.front();
  std::string typeName;
  std::string typeTemplateArg;
  auto normalizeMethodName = [this](const std::string &name) {
    std::string normalizedMethodName = name;
    if (!normalizedMethodName.empty() && normalizedMethodName.front() == '/') {
      normalizedMethodName.erase(normalizedMethodName.begin());
    }
    if (isUnrootedVectorHelperPath(normalizedMethodName)) {
      normalizedMethodName =
          normalizedMethodName.substr(unrootedVectorHelperPrefix().size());
    } else if (normalizedMethodName.rfind("array/", 0) == 0) {
      normalizedMethodName = normalizedMethodName.substr(std::string("array/").size());
    } else if (normalizedMethodName.rfind("soa/", 0) == 0) {
      normalizedMethodName = normalizedMethodName.substr(std::string("soa/").size());
    } else if (normalizedMethodName.rfind("std/collections/soa/", 0) == 0) {
      normalizedMethodName =
          normalizedMethodName.substr(std::string("std/collections/soa/").size());
    } else if (isUnrootedCanonicalVectorCompatibilityPath(
                   normalizedMethodName)) {
      normalizedMethodName = std::string(
          stripUnrootedCanonicalVectorCompatibilityPrefix(
              normalizedMethodName));
    } else if (const std::string keyValueHelperName =
                   metadataBackedKeyValueHelperMethodName(normalizedMethodName);
               keyValueHelperName != normalizedMethodName) {
      normalizedMethodName = keyValueHelperName;
    }
    return normalizedMethodName;
  };
  auto typeMatches = [&](std::string_view candidate, std::string_view expected) {
    return candidate == expected || normalizedTypeLeafName(std::string(candidate)) == expected;
  };
  auto lookupNormalizedMethodName = [&](const std::string &rawMethodName,
                                        std::string &normalizedMethodNameOut) {
    if (!hasScopedOwner) {
      return false;
    }
    const SymbolId rawMethodNameKey = callTargetResolutionScratch_.keyInterner.intern(rawMethodName);
    if (rawMethodNameKey == InvalidSymbolId) {
      return false;
    }
    if (const auto cacheIt = callTargetResolutionScratch_.normalizedMethodNameCache.find(rawMethodNameKey);
        cacheIt != callTargetResolutionScratch_.normalizedMethodNameCache.end()) {
      normalizedMethodNameOut = cacheIt->second;
      return true;
    }
    return false;
  };
  std::string normalizedMethodName;
  if (hasScopedOwner) {
    if (!lookupNormalizedMethodName(methodName, normalizedMethodName)) {
      normalizedMethodName = normalizeMethodName(methodName);
      const SymbolId rawMethodNameKey = callTargetResolutionScratch_.keyInterner.intern(methodName);
      if (rawMethodNameKey != InvalidSymbolId) {
        callTargetResolutionScratch_.normalizedMethodNameCache.emplace(rawMethodNameKey,
                                                                       normalizedMethodName);
      }
    }
  } else {
    normalizedMethodName = normalizeMethodName(methodName);
  }
  const SymbolId normalizedMethodNameKey =
      hasScopedOwner ? callTargetResolutionScratch_.keyInterner.intern(normalizedMethodName)
                     : InvalidSymbolId;
  auto rootedPathFragment = [&](std::string_view token) -> std::string {
    if (!hasScopedOwner) {
      return "/" + std::string(token);
    }
    const SymbolId key = callTargetResolutionScratch_.keyInterner.intern(token);
    if (key == InvalidSymbolId) {
      return "/" + std::string(token);
    }
    if (const auto cacheIt = callTargetResolutionScratch_.rootedCallNamePathCache.find(key);
        cacheIt != callTargetResolutionScratch_.rootedCallNamePathCache.end()) {
      return cacheIt->second;
    }
    auto [insertIt, inserted] =
        callTargetResolutionScratch_.rootedCallNamePathCache.emplace(key, "/" + std::string(token));
    (void)inserted;
    return insertIt->second;
  };
  auto joinMethodTarget = [&](std::string_view pathPrefix, std::string_view methodFragment) -> std::string {
    if (!hasScopedOwner) {
      return std::string(pathPrefix) + "/" + std::string(methodFragment);
    }
    const SymbolId prefixKey = callTargetResolutionScratch_.keyInterner.intern(pathPrefix);
    const SymbolId methodKey = callTargetResolutionScratch_.keyInterner.intern(methodFragment);
    if (prefixKey == InvalidSymbolId || methodKey == InvalidSymbolId) {
      return std::string(pathPrefix) + "/" + std::string(methodFragment);
    }
    const CallTargetResolutionScratch::SymbolPairKey joinKey{prefixKey, methodKey};
    if (const auto cacheIt = callTargetResolutionScratch_.joinedCallPathCache.find(joinKey);
        cacheIt != callTargetResolutionScratch_.joinedCallPathCache.end()) {
      return cacheIt->second;
    }
    std::string joined = std::string(pathPrefix) + "/" + std::string(methodFragment);
    auto [insertIt, inserted] =
        callTargetResolutionScratch_.joinedCallPathCache.emplace(joinKey, std::move(joined));
    (void)inserted;
    return insertIt->second;
  };
  auto canonicalReceiverAliasPath = [&](const std::string &resolvedReceiverPath) -> std::string {
    auto buildAliasPath = [&]() -> std::string {
      return {};
    };

    if (!hasScopedOwner) {
      return buildAliasPath();
    }

    const SymbolId resolvedReceiverPathKey =
        callTargetResolutionScratch_.keyInterner.intern(resolvedReceiverPath);
    if (resolvedReceiverPathKey == InvalidSymbolId) {
      return buildAliasPath();
    }
    if (const auto cacheIt =
            callTargetResolutionScratch_.canonicalReceiverAliasPathCache.find(resolvedReceiverPathKey);
        cacheIt != callTargetResolutionScratch_.canonicalReceiverAliasPathCache.end()) {
      return cacheIt->second;
    }
    auto [insertIt, inserted] = callTargetResolutionScratch_.canonicalReceiverAliasPathCache.emplace(
        resolvedReceiverPathKey,
        buildAliasPath());
    (void)inserted;
    return insertIt->second;
  };
  auto appendCanonicalReceiverResolutionCandidates = [&](const std::string &resolvedReceiverPath,
                                                         const auto &appendCandidate) {
    appendCandidate(resolvedReceiverPath);
    const std::string aliasPath = canonicalReceiverAliasPath(resolvedReceiverPath);
    if (!aliasPath.empty()) {
      appendCandidate(aliasPath);
    }
  };
  const auto isValueSurfaceAccessMethodName = [](std::string_view helperName) {
    return helperName == "at" || helperName == "at_unsafe";
  };
  const auto isCanonicalKeyValueAccessMethodName = [&](std::string_view helperName) {
    return isValueSurfaceAccessMethodName(helperName) ||
           helperName == "size" ||
           helperName == collection_helpers::kAtRef || helperName == collection_helpers::kAtUnsafeRef;
  };
  std::string explicitRemovedMethodPath;
  if (hasScopedOwner) {
    const SymbolId methodNameKey = callTargetResolutionScratch_.keyInterner.intern(methodName);
    const SymbolId namespaceKey = callTargetResolutionScratch_.keyInterner.intern(expr.namespacePrefix);
    if (methodNameKey != InvalidSymbolId && namespaceKey != InvalidSymbolId) {
      const CallTargetResolutionScratch::SymbolPairKey cacheKey{methodNameKey, namespaceKey};
      if (const auto cacheIt = callTargetResolutionScratch_.explicitRemovedMethodPathCache.find(cacheKey);
          cacheIt != callTargetResolutionScratch_.explicitRemovedMethodPathCache.end()) {
        explicitRemovedMethodPath = cacheIt->second;
      } else {
        explicitRemovedMethodPath = explicitRemovedCollectionMethodPath(methodName, expr.namespacePrefix);
        callTargetResolutionScratch_.explicitRemovedMethodPathCache.emplace(cacheKey, explicitRemovedMethodPath);
      }
    } else {
      explicitRemovedMethodPath = explicitRemovedCollectionMethodPath(methodName, expr.namespacePrefix);
    }
  } else {
    explicitRemovedMethodPath = explicitRemovedCollectionMethodPath(methodName, expr.namespacePrefix);
  }
  if (!removedRootMapMethodDiagnostic(expr).empty() &&
      !hasDeclaredDefinitionPath(explicitRemovedMethodPath)) {
    return false;
  }
  auto methodTargetMemoKey = [&](std::string_view receiverTypeText)
      -> std::optional<CallTargetResolutionScratch::MethodTargetMemoKey> {
    if (!hasScopedOwner || expr.semanticNodeId == 0 || receiverTypeText.empty() ||
        normalizedMethodNameKey == InvalidSymbolId) {
      return std::nullopt;
    }
    const SymbolId receiverTypeTextKey =
        callTargetResolutionScratch_.keyInterner.intern(receiverTypeText);
    if (receiverTypeTextKey == InvalidSymbolId) {
      return std::nullopt;
    }
    const uint64_t localsRevision = currentLocalBindingMemoRevision(&locals);
    return CallTargetResolutionScratch::MethodTargetMemoKey{
        expr.semanticNodeId,
        expr.sourceLine,
        expr.sourceColumn,
        localsRevision,
        receiverTypeTextKey,
        normalizedMethodNameKey};
  };
  auto lookupMethodTargetMemo = [&](std::string_view receiverTypeText,
                                    std::string &resolvedPathOut,
                                    bool &successOut) -> bool {
    if (!methodTargetMemoizationEnabled_) {
      return false;
    }
    const auto key = methodTargetMemoKey(receiverTypeText);
    if (!key.has_value()) {
      return false;
    }
    const auto memoIt = callTargetResolutionScratch_.methodTargetMemoCache.find(*key);
    if (memoIt == callTargetResolutionScratch_.methodTargetMemoCache.end()) {
      return false;
    }
    resolvedPathOut = memoIt->second;
    successOut = !memoIt->second.empty();
    return true;
  };
  auto storeMethodTargetMemo = [&](std::string_view receiverTypeText, bool success) {
    if (!methodTargetMemoizationEnabled_) {
      return;
    }
    const auto key = methodTargetMemoKey(receiverTypeText);
    if (!key.has_value()) {
      return;
    }
    if (!success) {
      callTargetResolutionScratch_.methodTargetMemoCache.insert_or_assign(*key, std::string());
      return;
    }
    callTargetResolutionScratch_.methodTargetMemoCache.insert_or_assign(*key, resolvedOut);
  };
  auto resolveCollectionMethodFromTypePath = [&](const std::string &collectionTypePath) -> bool {
    const collection_helpers::CollectionFamily collectionFamily =
        collection_helpers::parseCollectionFamily(collectionTypePath);
    if (!explicitRemovedMethodPath.empty() && hasDefinitionPath(explicitRemovedMethodPath)) {
      if (((normalizedMethodName == "count" &&
            (collectionFamily == collection_helpers::CollectionFamily::Vector || collectionFamily == collection_helpers::CollectionFamily::Array ||
             collectionFamily == collection_helpers::CollectionFamily::Soa || collectionFamily == collection_helpers::CollectionFamily::String)) ||
           (normalizedMethodName == collection_helpers::kCountRef &&
            (collectionFamily == collection_helpers::CollectionFamily::Soa || collectionFamily == collection_helpers::CollectionFamily::Map)))) {
        resolvedOut = explicitRemovedMethodPath;
        return true;
      }
      if (normalizedMethodName == "capacity" &&
          (collectionFamily == collection_helpers::CollectionFamily::Vector || collectionFamily == collection_helpers::CollectionFamily::Soa)) {
        resolvedOut = explicitRemovedMethodPath;
        return true;
      }
      if (isValueSurfaceAccessMethodName(normalizedMethodName) &&
          (collectionFamily == collection_helpers::CollectionFamily::Vector || collectionFamily == collection_helpers::CollectionFamily::Array ||
           collectionFamily == collection_helpers::CollectionFamily::String)) {
        resolvedOut = explicitRemovedMethodPath;
        return true;
      }
    }
    if (collection_helpers::isCountHelperName(normalizedMethodName)) {
      if (collectionFamily == collection_helpers::CollectionFamily::Array) {
        if (normalizedMethodName == collection_helpers::kCountRef) {
          return false;
        }
        resolvedOut = preferVectorStdlibHelperPath(collection_helpers::kRootedArrayCount);
        return true;
      }
      if (collectionFamily == collection_helpers::CollectionFamily::Vector &&
          usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector)) {
        resolvedOut =
            preferredSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector);
        return true;
      }
      if (collectionFamily == collection_helpers::CollectionFamily::Vector && normalizedMethodName == "count") {
        resolvedOut = canonicalVectorCompatibilityHelperPathOrFallback("count");
        return true;
      }
      if (collectionFamily == collection_helpers::CollectionFamily::Soa) {
        resolvedOut = preferredSoaHelperTargetForCollectionType(normalizedMethodName,
                                                                collection_helpers::kRootedSoa);
        return true;
      }
      if (collectionFamily == collection_helpers::CollectionFamily::String && normalizedMethodName == "count") {
        resolvedOut = collection_helpers::kRootedStringCount;
        return true;
      }
      if (collectionFamily == collection_helpers::CollectionFamily::Map &&
          (collection_helpers::isCountHelperName(normalizedMethodName))) {
        resolvedOut = preferredKeyValueMethodTargetForCall(params, locals, receiver,
                                                      normalizedMethodName);
        return true;
      }
      if (collectionTypePath == "/Buffer" && normalizedMethodName == "count") {
        resolvedOut = preferredBufferMethodTargetForCall(params, locals, receiver, "count");
        return !resolvedOut.empty();
      }
    }
    if (normalizedMethodName == "capacity" && collectionFamily == collection_helpers::CollectionFamily::Vector) {
      resolvedOut =
          canonicalVectorCompatibilityHelperPathOrFallback("capacity");
      return true;
    }
    if ((normalizedMethodName == "empty" || normalizedMethodName == "is_valid" ||
         normalizedMethodName == "readback" || normalizedMethodName == "load" ||
         normalizedMethodName == "store") &&
        collectionTypePath == "/Buffer") {
      resolvedOut = preferredBufferMethodTargetForCall(params, locals, receiver, normalizedMethodName);
      return !resolvedOut.empty();
    }
    if (normalizedMethodName == "contains" && collectionFamily == collection_helpers::CollectionFamily::Map) {
      resolvedOut = preferredKeyValueMethodTargetForCall(params, locals, receiver, "contains");
      return true;
    }
    if (normalizedMethodName == "tryAt" && collectionFamily == collection_helpers::CollectionFamily::Map) {
      resolvedOut = preferredKeyValueMethodTargetForCall(params, locals, receiver, "tryAt");
      return true;
    }
    if (normalizedMethodName == "insert" && collectionFamily == collection_helpers::CollectionFamily::Map) {
      resolvedOut = preferredKeyValueMethodTargetForCall(params, locals, receiver, "insert");
      return true;
    }
    if (normalizedMethodName == "size" && collectionFamily == collection_helpers::CollectionFamily::Map) {
      resolvedOut = preferredKeyValueMethodTargetForCall(params, locals, receiver, "size");
      return true;
    }
    if (isValueSurfaceAccessMethodName(normalizedMethodName)) {
      if (collectionFamily == collection_helpers::CollectionFamily::Array) {
        resolvedOut = preferVectorStdlibHelperPath(collection_helpers::kRootedArrayPrefix + normalizedMethodName);
        return true;
      }
      if (collectionFamily == collection_helpers::CollectionFamily::Vector) {
        resolvedOut =
            preferVectorStdlibHelperPath(
                canonicalVectorCompatibilityHelperPathOrFallback(
                    normalizedMethodName));
        return true;
      }
      if (collectionFamily == collection_helpers::CollectionFamily::String) {
        resolvedOut = collection_helpers::kRootedStringPrefix + normalizedMethodName;
        return true;
      }
    }
    if (isCanonicalKeyValueAccessMethodName(normalizedMethodName) &&
        collectionFamily == collection_helpers::CollectionFamily::Map) {
      resolvedOut = preferredKeyValueMethodTargetForCall(params, locals, receiver, normalizedMethodName);
      return true;
    }
    if (normalizedMethodName == "empty" && collectionTypePath == "/Buffer") {
      resolvedOut = preferredBufferMethodTargetForCall(params, locals, receiver, "empty");
      return !resolvedOut.empty();
    }
    if (normalizedMethodName == "is_valid" && collectionTypePath == "/Buffer") {
      resolvedOut = preferredBufferMethodTargetForCall(params, locals, receiver, "is_valid");
      return !resolvedOut.empty();
    }
    if (normalizedMethodName == "readback" && collectionTypePath == "/Buffer") {
      resolvedOut = preferredBufferMethodTargetForCall(params, locals, receiver, "readback");
      return !resolvedOut.empty();
    }
    if (normalizedMethodName == "load" && collectionTypePath == "/Buffer") {
      resolvedOut = preferredBufferMethodTargetForCall(params, locals, receiver, "load");
      return !resolvedOut.empty();
    }
    if (normalizedMethodName == "store" && collectionTypePath == "/Buffer") {
      resolvedOut = preferredBufferMethodTargetForCall(params, locals, receiver, "store");
      return !resolvedOut.empty();
    }
    if ((collection_helpers::isGetHelperName(normalizedMethodName)) &&
        (collectionFamily == collection_helpers::CollectionFamily::Soa ||
         (collectionFamily == collection_helpers::CollectionFamily::Vector &&
          usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector)))) {
      resolvedOut = preferredSoaHelperTargetForCollectionType(
          normalizedMethodName,
          collectionFamily == collection_helpers::CollectionFamily::Soa ? collection_helpers::kRootedSoa : collection_helpers::kRootedVector);
      return true;
    }
    if ((collection_helpers::isRefHelperName(normalizedMethodName)) &&
        (collectionFamily == collection_helpers::CollectionFamily::Soa ||
         (collectionFamily == collection_helpers::CollectionFamily::Vector &&
          usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector)))) {
      resolvedOut = preferredSoaHelperTargetForCollectionType(
          normalizedMethodName,
          collectionFamily == collection_helpers::CollectionFamily::Soa ? collection_helpers::kRootedSoa : collection_helpers::kRootedVector);
      return true;
    }
    if ((normalizedMethodName == "push" || normalizedMethodName == "reserve") &&
        (collectionFamily == collection_helpers::CollectionFamily::Soa ||
         (collectionFamily == collection_helpers::CollectionFamily::Vector &&
          usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName,
                                                       collection_helpers::kRootedVector)))) {
      resolvedOut = preferredSoaHelperTargetForCollectionType(
          normalizedMethodName,
          collectionFamily == collection_helpers::CollectionFamily::Soa ? collection_helpers::kRootedSoa : collection_helpers::kRootedVector);
      return true;
    }
    if (normalizedMethodName == "to_soa" && collectionFamily == collection_helpers::CollectionFamily::Vector) {
      resolvedOut = "/to_soa";
      return true;
    }
    if ((collection_helpers::isToAosHelperName(normalizedMethodName)) &&
        (collectionFamily == collection_helpers::CollectionFamily::Soa || collectionFamily == collection_helpers::CollectionFamily::Vector)) {
      resolvedOut = preferredSoaHelperTargetForCollectionType(
          normalizedMethodName,
          collectionFamily == collection_helpers::CollectionFamily::Soa ? collection_helpers::kRootedSoa : collection_helpers::kRootedVector);
      return true;
    }
    return false;
  };
  auto resolveSoaFieldViewMethodTarget = [&](const Expr &soaReceiver) -> bool {
    std::string elemType;
    auto resolveDirectReceiver = [&](const Expr &directCandidate,
                                     std::string &directElemTypeOut) -> bool {
      return this->resolveDirectSoaVectorOrExperimentalBorrowedReceiver(
          directCandidate, params, locals, resolveSoaVectorTarget,
          directElemTypeOut);
    };
    if (!this->resolveSoaVectorOrExperimentalBorrowedReceiver(
            soaReceiver, params, locals, resolveDirectReceiver, elemType)) {
      return false;
    }
    const std::string normalizedElemType = normalizeBindingTypeName(elemType);
    std::string currentNamespace;
    if (!currentValidationState_.context.definitionPath.empty()) {
      const size_t slash = currentValidationState_.context.definitionPath.find_last_of('/');
      if (slash != std::string::npos && slash > 0) {
        currentNamespace = currentValidationState_.context.definitionPath.substr(0, slash);
      }
    }
    const std::string lookupNamespace =
        !soaReceiver.namespacePrefix.empty() ? soaReceiver.namespacePrefix : currentNamespace;
    const std::string elementStructPath =
        primec::semantics::resolveStructTypePath(normalizedElemType, lookupNamespace, structNames_);
    if (elementStructPath.empty()) {
      return false;
    }
    auto structIt = defMap_.find(elementStructPath);
    if (structIt == defMap_.end() || structIt->second == nullptr) {
      return false;
    }
    for (const auto &stmt : structIt->second->statements) {
      const bool isStaticBinding = [&]() {
        for (const auto &transform : stmt.transforms) {
          if (transform.name == "static") {
            return true;
          }
        }
        return false;
      }();
      if (!stmt.isBinding || isStaticBinding || stmt.name != normalizedMethodName) {
        continue;
      }
      if (hasVisibleSoaHelperTargetForCurrentImports(normalizedMethodName)) {
        resolvedOut =
            preferredSoaHelperTargetForCurrentImports(normalizedMethodName);
      } else {
        resolvedOut = soaFieldViewHelperPath(normalizedMethodName);
      }
      return true;
    }
    return false;
  };
  std::function<bool(const Expr &, std::string &)> resolveBorrowedVectorReceiver =
      [&](const Expr &candidate, std::string &elemTypeOut) -> bool {
    return this->resolveBorrowedVectorReceiver(candidate, elemTypeOut, params, locals);
  };
  auto preferredBorrowedSoaAccessHelperTarget = [&](std::string_view helperName) {
    if (helperName == "count") {
      helperName = collection_helpers::kCountRef;
    } else if (helperName == "get") {
      helperName = collection_helpers::kGetRef;
    } else if (helperName == "ref") {
      helperName = collection_helpers::kRefRef;
    } else if (helperName == "to_aos") {
      helperName = collection_helpers::kToAosRef;
    }
    return preferredSoaHelperTargetForCollectionType(helperName, collection_helpers::kRootedSoa);
  };
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
    return preferredSoaHelperTargetForCollectionType(normalizedMethodName,
                                                    collection_helpers::kRootedSoa);
  };
  auto isCanonicalSoaWrapperMethod = [&](std::string_view helperName) {
    return collection_helpers::isCountHelperName(helperName) ||
           collection_helpers::isGetHelperName(helperName) ||
           collection_helpers::isRefHelperName(helperName) ||
           collection_helpers::isToAosHelperName(helperName) ||
           helperName == "push" || helperName == "reserve";
  };
  auto redirectConcreteExperimentalSoaMethodTarget = [&](const std::string &resolvedType) -> bool {
    const bool isConcreteExperimentalSoaReceiver =
        resolvedType.rfind(collection_paths::specializedTypePrefix(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName), 0) == 0;
    if (!isConcreteExperimentalSoaReceiver ||
        !isCanonicalSoaWrapperMethod(normalizedMethodName)) {
      return false;
    }
    resolvedOut = preferredSoaHelperTargetForCollectionType(normalizedMethodName,
                                                            collection_helpers::kRootedSoa);
    return true;
  };
  auto setIndexedArgsPackKeyValueMethodTarget = [&](const Expr &receiverExpr, const std::string &helperName) -> bool {
    if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding || receiverExpr.args.size() != 2) {
      return false;
    }
    std::string accessName;
    if (!getBuiltinArrayAccessName(receiverExpr, accessName)) {
      return false;
    }
    const Expr *accessReceiver = this->resolveBuiltinAccessReceiverExpr(receiverExpr);
    if (accessReceiver == nullptr) {
      return false;
    }
    std::string indexedElemType;
    std::string keyType;
    std::string valueType;
    if (!resolveArgsPackAccessTarget(*accessReceiver, indexedElemType)) {
      return false;
    }
    const std::string unwrappedIndexedElemType =
        normalizeBindingTypeName(unwrapReferencePointerTypeText(indexedElemType));
    const std::string indexedKeyValueTypeText =
        unwrappedIndexedElemType.empty() ? indexedElemType : unwrappedIndexedElemType;
    if (!extractKeyValueCollectionTypesFromTypeText(indexedKeyValueTypeText, keyType, valueType)) {
      return false;
    }
    resolvedOut = preferredKeyValueMethodTargetForCall(params, locals, receiverExpr, helperName);
    return true;
  };
  auto shouldPreferStructReturnMethodTargetForCall = [&](const Expr &receiverExpr) {
    return receiverExpr.kind == Expr::Kind::Call &&
           !receiverExpr.isBinding &&
           receiverExpr.isFieldAccess;
  };

  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    std::string resolvedType = resolveCalleePath(receiver);
    if (!resolvedType.empty() && structNames_.count(resolvedType) > 0) {
      resolvedOut = joinMethodTarget(resolvedType, normalizedMethodName);
      return true;
    }
    if (receiver.isMethodCall) {
      resolvedType = inferStructReturnPath(receiver, params, locals);
      if (!resolvedType.empty() && structNames_.count(resolvedType) > 0) {
        if (redirectConcreteExperimentalSoaMethodTarget(resolvedType)) {
          return true;
        }
        resolvedOut = joinMethodTarget(resolvedType, normalizedMethodName);
        return true;
      }
    }
    std::string receiverCollectionTypePath;
    if (resolveCallCollectionTypePath(receiver, params, locals, receiverCollectionTypePath) &&
        resolveCollectionMethodFromTypePath(receiverCollectionTypePath)) {
      return true;
    }
    std::string receiverTypeText;
    if (inferQueryExprTypeText(receiver, params, locals, receiverTypeText) &&
        !receiverTypeText.empty()) {
      const std::string normalizedReceiverType = normalizeBindingTypeName(receiverTypeText);
      const std::string receiverCollectionTypePath =
          inferMethodCollectionTypePathFromTypeText(normalizedReceiverType);
      if (!receiverCollectionTypePath.empty() &&
          resolveCollectionMethodFromTypePath(receiverCollectionTypePath)) {
        return true;
      }
      const std::string resolvedReceiverType =
          resolveMethodStructTypePath(normalizedReceiverType, receiver.namespacePrefix);
      if (!resolvedReceiverType.empty()) {
        if (redirectConcreteExperimentalSoaMethodTarget(resolvedReceiverType)) {
          return true;
        }
        resolvedOut = joinMethodTarget(resolvedReceiverType, normalizedMethodName);
        return true;
      }
      if (isPrimitiveBindingTypeName(normalizedReceiverType)) {
        const std::string inferredStructPath =
            inferStructReturnPath(receiver, params, locals);
        if (!inferredStructPath.empty() &&
            structNames_.count(inferredStructPath) > 0) {
          resolvedOut = joinMethodTarget(inferredStructPath, normalizedMethodName);
          return true;
        }
        resolvedOut = joinMethodTarget(rootedPathFragment(normalizedReceiverType), normalizedMethodName);
        return true;
      }
    }
    std::string bufferElemType;
    if (resolveBufferTarget != nullptr && resolveBufferTarget(receiver, bufferElemType) &&
        !bufferElemType.empty()) {
      resolvedOut = preferredBufferMethodTargetForCall(params, locals, receiver, normalizedMethodName);
      return !resolvedOut.empty();
    }
    resolvedType = inferStructReturnPath(receiver, params, locals);
    if (!resolvedType.empty()) {
      if (redirectConcreteExperimentalSoaMethodTarget(resolvedType)) {
        return true;
      }
      resolvedOut = joinMethodTarget(resolvedType, normalizedMethodName);
      return true;
    }
    const ReturnKind receiverKind = inferExprReturnKind(receiver, params, locals);
    if (receiverKind == ReturnKind::Unknown || receiverKind == ReturnKind::Void) {
      return false;
    }
    if (receiverKind != ReturnKind::Array) {
      const std::string receiverType = typeNameForReturnKind(receiverKind);
      if (receiverType.empty()) {
        return false;
      }
      resolvedOut = joinMethodTarget(rootedPathFragment(receiverType), normalizedMethodName);
      return true;
    }
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding &&
      shouldPreferStructReturnMethodTargetForCall(receiver)) {
    std::string inferredStructPath = inferStructReturnPath(receiver, params, locals);
    if (!inferredStructPath.empty() && structNames_.count(inferredStructPath) > 0) {
      resolvedOut = joinMethodTarget(inferredStructPath, normalizedMethodName);
      return true;
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding &&
             receiver.isMethodCall && !receiver.args.empty()) {
    std::string receiverHelperName = receiver.name;
    if (!receiverHelperName.empty() && receiverHelperName.front() == '/') {
      receiverHelperName.erase(receiverHelperName.begin());
    }
    if (const std::string keyValueHelperName =
            metadataBackedKeyValueHelperMethodName(receiverHelperName);
        keyValueHelperName != receiverHelperName) {
      receiverHelperName = keyValueHelperName;
    }
    std::string keyType;
    std::string valueType;
    if (isCanonicalKeyValueAccessMethodName(receiverHelperName) &&
        resolveKeyValueTarget(receiver.args.front(), keyType, valueType)) {
      const std::string resolvedReceiver =
          preferredKeyValueMethodTargetForCall(params, locals, receiver, receiverHelperName);
      auto defIt = defMap_.find(resolvedReceiver);
      if (defIt != defMap_.end() && defIt->second != nullptr) {
        BindingInfo inferredReceiverBinding;
        if (inferDefinitionReturnBinding(*defIt->second, inferredReceiverBinding)) {
          const std::string receiverTypeText =
              inferredReceiverBinding.typeTemplateArg.empty()
                  ? inferredReceiverBinding.typeName
                  : inferredReceiverBinding.typeName + "<" + inferredReceiverBinding.typeTemplateArg + ">";
          const std::string resolvedReceiverType =
              resolveMethodStructTypePath(normalizeBindingTypeName(receiverTypeText),
                                          defIt->second->namespacePrefix);
          if (!resolvedReceiverType.empty()) {
            resolvedOut = joinMethodTarget(resolvedReceiverType, normalizedMethodName);
            return true;
          }
          const std::string normalizedReceiverType =
              normalizeBindingTypeName(receiverTypeText);
          if (isPrimitiveBindingTypeName(normalizedReceiverType)) {
            resolvedOut = joinMethodTarget(rootedPathFragment(normalizedReceiverType), normalizedMethodName);
            return true;
          }
        }
      }
    }
    std::string bufferElemType;
    if (resolveBufferTarget != nullptr && resolveBufferTarget(receiver, bufferElemType) &&
        !bufferElemType.empty()) {
      resolvedOut = preferredBufferMethodTargetForCall(params, locals, receiver, normalizedMethodName);
      return !resolvedOut.empty();
    }
  }
  if (receiver.kind == Expr::Kind::Call) {
    std::string receiverCollectionTypePath;
    if (resolveCallCollectionTypePath(receiver, params, locals, receiverCollectionTypePath) &&
        resolveCollectionMethodFromTypePath(receiverCollectionTypePath)) {
      return true;
    }
  }
  if (receiver.kind == Expr::Kind::Name &&
      findParamBinding(params, receiver.name) == nullptr &&
      locals.find(receiver.name) == locals.end()) {
    std::string resolvedReceiverPath;
    const std::string rootReceiverPath = rootedPathFragment(receiver.name);
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
         defMap_.find(joinMethodTarget(resolvedReceiverPath, normalizedMethodName)) != defMap_.end())) {
      resolvedOut = joinMethodTarget(resolvedReceiverPath, normalizedMethodName);
      return true;
    }
    const std::string resolvedType = resolveMethodStructTypePath(receiver.name, receiver.namespacePrefix);
    if (!resolvedType.empty()) {
      resolvedOut = joinMethodTarget(resolvedType, normalizedMethodName);
      return true;
    }
  }
  auto resolveDirectReceiver = [&](const Expr &directCandidate,
                                   std::string &directElemTypeOut) -> bool {
    return this->resolveDirectSoaVectorOrExperimentalBorrowedReceiver(
        directCandidate, params, locals, resolveSoaVectorTarget,
        directElemTypeOut);
  };
  {
    std::string elemType;
    std::string keyType;
    std::string valueType;
    if (collection_helpers::isCountHelperName(normalizedMethodName)) {
      if (normalizedMethodName == "count" &&
          resolveArgsPackCountTarget(receiver, elemType)) {
        resolvedOut = preferVectorStdlibHelperPath(collection_helpers::kRootedArrayCount);
        return true;
      }
      if (resolveVectorTarget(receiver, elemType) &&
          usesSamePathSoaHelperTargetForCurrentImports(normalizedMethodName)) {
        resolvedOut = preferredSoaHelperTargetForCurrentImports(normalizedMethodName);
        return true;
      }
      if (normalizedMethodName == "count" &&
          resolveVectorTarget(receiver, elemType)) {
        resolvedOut = canonicalVectorCompatibilityHelperPathOrFallback("count");
        return true;
      }
      if (resolveSoaVectorTarget(receiver, elemType)) {
        resolvedOut = preferredSoaHelperTargetForCurrentImports(normalizedMethodName);
        return true;
      }
      if (normalizedMethodName == "count" &&
          resolveArrayTarget(receiver, elemType)) {
        resolvedOut = preferVectorStdlibHelperPath(collection_helpers::kRootedArrayCount);
        return true;
      }
      if (normalizedMethodName == "count" && resolveStringTarget(receiver)) {
        resolvedOut = collection_helpers::kRootedStringCount;
        return true;
      }
      if (normalizedMethodName == "count" &&
          setIndexedArgsPackKeyValueMethodTarget(receiver, "count")) {
        return true;
      }
    }
    if (normalizedMethodName == "capacity" && resolveVectorTarget(receiver, elemType)) {
      resolvedOut =
          canonicalVectorCompatibilityHelperPathOrFallback("capacity");
      return true;
    }
    if ((collection_helpers::isCountHelperName(normalizedMethodName) ||
         normalizedMethodName == "size") &&
        resolveKeyValueTarget(receiver, keyType, valueType)) {
      resolvedOut = preferredKeyValueMethodTargetForCall(params, locals, receiver,
                                                         normalizedMethodName);
      return true;
    }
    if ((normalizedMethodName == "contains" || normalizedMethodName == "tryAt" ||
         normalizedMethodName == "insert") &&
        resolveKeyValueTarget(receiver, keyType, valueType)) {
      resolvedOut = preferredKeyValueMethodTargetForCall(params, locals, receiver,
                                                         normalizedMethodName);
      return true;
    }
    if ((normalizedMethodName == "contains" || normalizedMethodName == "tryAt" ||
         normalizedMethodName == "insert") &&
        setIndexedArgsPackKeyValueMethodTarget(receiver, normalizedMethodName)) {
      return true;
    }
    if (isValueSurfaceAccessMethodName(normalizedMethodName)) {
      if (resolveArgsPackAccessTarget(receiver, elemType)) {
        resolvedOut = preferVectorStdlibHelperPath(collection_helpers::kRootedArrayPrefix + normalizedMethodName);
        return true;
      }
      if (resolveVectorTarget(receiver, elemType)) {
        resolvedOut =
            preferVectorStdlibHelperPath(
                canonicalVectorCompatibilityHelperPathOrFallback(
                    normalizedMethodName));
        return true;
      }
      if (resolveArrayTarget(receiver, elemType)) {
        resolvedOut = preferVectorStdlibHelperPath(collection_helpers::kRootedArrayPrefix + normalizedMethodName);
        return true;
      }
      if (resolveStringTarget(receiver)) {
        resolvedOut = collection_helpers::kRootedStringPrefix + normalizedMethodName;
        return true;
      }
      if (setIndexedArgsPackKeyValueMethodTarget(receiver, normalizedMethodName)) {
        return true;
      }
    }
    if (isCanonicalKeyValueAccessMethodName(normalizedMethodName) &&
        setIndexedArgsPackKeyValueMethodTarget(receiver, normalizedMethodName)) {
      return true;
    }
    if (isCanonicalKeyValueAccessMethodName(normalizedMethodName) &&
        resolveKeyValueTarget(receiver, keyType, valueType)) {
      resolvedOut = preferredKeyValueMethodTargetForCall(params, locals, receiver,
                                                         normalizedMethodName);
      return true;
    }
    if ((collection_helpers::isCountHelperName(normalizedMethodName)) &&
        resolveVectorTarget(receiver, elemType) &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector)) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector);
      return true;
    }
    if ((collection_helpers::isCountHelperName(normalizedMethodName)) &&
        resolveSoaVectorTarget(receiver, elemType)) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedSoa);
      return true;
    }
    if ((collection_helpers::isCountHelperName(normalizedMethodName)) &&
        this->resolveSoaVectorOrExperimentalBorrowedReceiver(
            receiver, params, locals, resolveDirectReceiver, elemType)) {
      resolvedOut = preferredBorrowedSoaAccessHelperTarget(normalizedMethodName);
      return true;
    }
    if ((collection_helpers::isGetHelperName(normalizedMethodName)) &&
        resolveVectorTarget(receiver, elemType) &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector)) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector);
      return true;
    }
    if ((collection_helpers::isGetHelperName(normalizedMethodName)) &&
        resolveBorrowedVectorReceiver(receiver, elemType) &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector)) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector);
      return true;
    }
    if ((collection_helpers::isGetHelperName(normalizedMethodName)) &&
        resolveSoaVectorTarget(receiver, elemType)) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedSoa);
      return true;
    }
    if ((collection_helpers::isGetHelperName(normalizedMethodName)) &&
        this->resolveSoaVectorOrExperimentalBorrowedReceiver(
            receiver, params, locals, resolveDirectReceiver, elemType)) {
      resolvedOut = preferredBorrowedSoaAccessHelperTarget(normalizedMethodName);
      return true;
    }
    if ((collection_helpers::isRefHelperName(normalizedMethodName)) &&
        resolveVectorTarget(receiver, elemType) &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector)) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector);
      return true;
    }
    if ((collection_helpers::isRefHelperName(normalizedMethodName)) &&
        resolveSoaVectorTarget(receiver, elemType)) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedSoa);
      return true;
    }
    if ((collection_helpers::isRefHelperName(normalizedMethodName)) &&
        this->resolveSoaVectorOrExperimentalBorrowedReceiver(
            receiver, params, locals, resolveDirectReceiver, elemType)) {
      resolvedOut = preferredBorrowedSoaAccessHelperTarget(normalizedMethodName);
      return true;
    }
    if ((collection_helpers::isToAosHelperName(normalizedMethodName)) &&
        resolveVectorTarget(receiver, elemType)) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedVector);
      return true;
    }
    // Direct (owned) soa receivers must be tried before the OR-combined
    // resolveSoaVectorOrExperimentalBorrowedReceiver check below: that
    // check's "direct" branch also matches an owned receiver (it is an OR
    // of "direct" and "borrowed"), but its result unconditionally builds
    // the borrowed to_aos target, so checking it first would route an
    // owned receiver to the borrowed helper. get/ref above already use
    // this same ordering (direct soa check before the OR-combined check).
    if ((collection_helpers::isToAosHelperName(normalizedMethodName)) &&
        resolveSoaVectorTarget(receiver, elemType)) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, collection_helpers::kRootedSoa);
      return true;
    }
    if ((collection_helpers::isToAosHelperName(normalizedMethodName)) &&
        this->resolveSoaVectorOrExperimentalBorrowedReceiver(
            receiver, params, locals, resolveDirectReceiver, elemType)) {
      resolvedOut = preferredSoaToAosHelperTargetForReceiver(receiver);
      return true;
    }
    if (resolveSoaFieldViewMethodTarget(receiver)) {
      return true;
    }
  }
  if (receiver.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
      typeName = paramBinding->typeName;
      typeTemplateArg = paramBinding->typeTemplateArg;
    } else {
      auto it = locals.find(receiver.name);
      if (it != locals.end()) {
        typeName = it->second.typeName;
        typeTemplateArg = it->second.typeTemplateArg;
      }
    }
  }
  if (typeName.empty()) {
    std::string receiverTypeText;
    if (receiver.kind == Expr::Kind::Call &&
        inferQueryExprTypeText(receiver, params, locals, receiverTypeText) &&
        !receiverTypeText.empty()) {
      const std::string normalizedReceiverType = normalizeBindingTypeName(receiverTypeText);
      const std::string receiverCollectionTypePath =
          inferMethodCollectionTypePathFromTypeText(normalizedReceiverType);
      if (!receiverCollectionTypePath.empty() &&
          resolveCollectionMethodFromTypePath(receiverCollectionTypePath)) {
        return true;
      }
      typeName = normalizedReceiverType;
    }
  }
  if (typeName.empty()) {
    if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
      typeName = inferPointerLikeCallReturnType(receiver, params, locals);
    }
  }
  if (typeName.empty()) {
    ReturnKind inferredKind = inferExprReturnKind(receiver, params, locals);
    std::string inferred;
    if (inferredKind == ReturnKind::Array) {
      inferred = inferStructReturnPath(receiver, params, locals);
      if (inferred.empty()) {
        inferred = typeNameForReturnKind(inferredKind);
      }
    } else {
      inferred = typeNameForReturnKind(inferredKind);
    }
    if (!inferred.empty()) {
      typeName = inferred;
    }
  }
  if (typeName.empty() && receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    BindingInfo callBinding;
    const std::string resolvedReceiverPath = resolveCalleePath(receiver);
    auto resolveReceiverCandidates = [&](auto &resolvedCandidates) {
      resolvedCandidates.clear();
      auto appendResolvedCandidate = [&](const std::string &candidate) {
        if (candidate.empty()) {
          return;
        }
        for (const auto &existing : resolvedCandidates) {
          if (existing == candidate) {
            return;
          }
        }
        resolvedCandidates.push_back(candidate);
      };
      appendCanonicalReceiverResolutionCandidates(resolvedReceiverPath, appendResolvedCandidate);
      for (const auto &candidate : resolvedCandidates) {
        auto defIt = defMap_.find(candidate);
        if (defIt == defMap_.end() || defIt->second == nullptr) {
          continue;
        }
        if (!inferDefinitionReturnBinding(*defIt->second, callBinding)) {
          continue;
        }
        typeName = callBinding.typeName;
        typeTemplateArg = callBinding.typeTemplateArg;
        return;
      }
    };

    if (hasScopedOwner) {
      auto &resolvedCandidates = callTargetResolutionScratch_.methodReceiverResolutionCandidates;
      resolveReceiverCandidates(resolvedCandidates);
    } else {
      std::vector<std::string> resolvedCandidates;
      resolveReceiverCandidates(resolvedCandidates);
    }
  }
  if (receiver.kind == Expr::Kind::Name && receiver.name == "FileError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "is_eof" ||
       normalizedMethodName == "eof" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    resolvedOut = preferredFileErrorHelperTarget(normalizedMethodName);
    return !resolvedOut.empty();
  }
  if (receiver.kind == Expr::Kind::Name && receiver.name == "ImageError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    resolvedOut = preferredImageErrorHelperTarget(normalizedMethodName);
    return !resolvedOut.empty();
  }
  if (receiver.kind == Expr::Kind::Name && receiver.name == "ContainerError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    resolvedOut = preferredContainerErrorHelperTarget(normalizedMethodName);
    return !resolvedOut.empty();
  }
  if (receiver.kind == Expr::Kind::Name && receiver.name == "GfxError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    resolvedOut =
        preferredGfxErrorHelperTarget(normalizedMethodName,
                                      resolveMethodStructTypePath(receiver.name, receiver.namespacePrefix));
    if (!resolvedOut.empty()) {
      return true;
    }
  }
  if (typeName.empty()) {
    return false;
  }
  const std::string memoReceiverTypeText = normalizeBindingTypeName(
      typeTemplateArg.empty() ? typeName : typeName + "<" + typeTemplateArg + ">");
  std::string memoResolvedPath;
  bool memoSuccess = false;
  if (lookupMethodTargetMemo(memoReceiverTypeText, memoResolvedPath, memoSuccess)) {
    if (memoSuccess) {
      resolvedOut = std::move(memoResolvedPath);
    }
    return memoSuccess;
  }
  auto returnWithMethodTargetMemo = [&](bool success) {
    storeMethodTargetMemo(memoReceiverTypeText, success);
    return success;
  };
  {
    const std::string receiverTypeText =
        typeTemplateArg.empty() ? typeName : typeName + "<" + typeTemplateArg + ">";
    const std::string receiverCollectionTypePath =
        inferMethodCollectionTypePathFromTypeText(normalizeBindingTypeName(receiverTypeText));
    if (!receiverCollectionTypePath.empty() &&
        resolveCollectionMethodFromTypePath(receiverCollectionTypePath)) {
      return returnWithMethodTargetMemo(true);
    }
  }
  if (typeMatches(typeName, "File") &&
      (normalizedMethodName == "write" || normalizedMethodName == "write_line") &&
      expr.args.size() > 10) {
    resolvedOut = "/file/" + normalizedMethodName;
    return returnWithMethodTargetMemo(true);
  }
  if (typeMatches(typeName, "FileError") &&
      (normalizedMethodName == "why" || normalizedMethodName == "is_eof" ||
       normalizedMethodName == "status" || normalizedMethodName == "result")) {
    resolvedOut = preferredFileErrorHelperTarget(normalizedMethodName);
    return returnWithMethodTargetMemo(!resolvedOut.empty());
  }
  if (typeMatches(typeName, "ImageError") &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    resolvedOut = preferredImageErrorHelperTarget(normalizedMethodName);
    return returnWithMethodTargetMemo(!resolvedOut.empty());
  }
  if (typeMatches(typeName, "ContainerError") &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    resolvedOut = preferredContainerErrorHelperTarget(normalizedMethodName);
    return returnWithMethodTargetMemo(!resolvedOut.empty());
  }
  if (typeMatches(typeName, "GfxError") &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    resolvedOut =
        preferredGfxErrorHelperTarget(normalizedMethodName,
                                      resolveMethodStructTypePath(typeName, expr.namespacePrefix));
    if (!resolvedOut.empty()) {
      return returnWithMethodTargetMemo(true);
    }
  }
  if (typeName == "Reference" &&
      (normalizedMethodName == "count" || normalizedMethodName == "contains" ||
       normalizedMethodName == "tryAt" || normalizedMethodName == "at" ||
       normalizedMethodName == "at_unsafe" || normalizedMethodName == "insert")) {
    std::string keyType;
    std::string valueType;
    if (resolveInferExperimentalKeyValueTarget(params, locals, receiver, keyType, valueType)) {
      resolvedOut = preferredKeyValueMethodTargetForCall(params, locals, receiver, normalizedMethodName);
      return returnWithMethodTargetMemo(!resolvedOut.empty());
    }
  }
  if (typeName == "Pointer" || typeName == "Reference") {
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      resolvedOut = joinMethodTarget(rootedPathFragment(typeName), normalizedMethodName);
      return returnWithMethodTargetMemo(true);
    }
    return returnWithMethodTargetMemo(false);
  }
  if (isPrimitiveBindingTypeName(typeName)) {
    resolvedOut = joinMethodTarget(rootedPathFragment(normalizeBindingTypeName(typeName)), normalizedMethodName);
    return returnWithMethodTargetMemo(true);
  }
  std::string resolvedType = resolveMethodStructTypePath(typeName, expr.namespacePrefix);
  if (resolvedType.empty()) {
    resolvedType = resolveTypePath(typeName, expr.namespacePrefix);
  }
  if (redirectConcreteExperimentalSoaMethodTarget(resolvedType)) {
    return returnWithMethodTargetMemo(true);
  }
  const std::string receiverHelperLeaf = receiverHelperFamilyLeaf(resolvedType);
  if (!receiverHelperLeaf.empty()) {
    const std::string samePathHelper = joinMethodTarget(resolvedType, normalizedMethodName);
    const std::string rootedHelper = "/" + receiverHelperLeaf + "/" + normalizedMethodName;
    if (samePathHelper != rootedHelper && !hasDefinitionFamilyPath(samePathHelper) &&
        hasDefinitionFamilyPath(rootedHelper)) {
      resolvedOut = rootedHelper;
      return returnWithMethodTargetMemo(true);
    }
  }
  resolvedOut = joinMethodTarget(resolvedType, normalizedMethodName);
  return returnWithMethodTargetMemo(true);
}

} // namespace primec::semantics
