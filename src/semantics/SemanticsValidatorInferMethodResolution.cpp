#include "SemanticsValidator.h"

#include <functional>
#include <string_view>

namespace primec::semantics {

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
  const auto &resolveMapTarget = builtinCollectionDispatchResolvers.resolveMapTarget;

  if (expr.args.empty()) {
    return false;
  }
  const Expr &receiver = expr.args.front();
  std::string typeName;
  std::string typeTemplateArg;
  auto normalizeMethodName = [](const std::string &name) {
    std::string normalizedMethodName = name;
    if (!normalizedMethodName.empty() && normalizedMethodName.front() == '/') {
      normalizedMethodName.erase(normalizedMethodName.begin());
    }
    if (normalizedMethodName.rfind("vector/", 0) == 0) {
      normalizedMethodName = normalizedMethodName.substr(std::string("vector/").size());
    } else if (normalizedMethodName.rfind("array/", 0) == 0) {
      normalizedMethodName = normalizedMethodName.substr(std::string("array/").size());
    } else if (normalizedMethodName.rfind("soa_vector/", 0) == 0) {
      normalizedMethodName = normalizedMethodName.substr(std::string("soa_vector/").size());
    } else if (normalizedMethodName.rfind("std/collections/soa_vector/", 0) == 0) {
      normalizedMethodName =
          normalizedMethodName.substr(std::string("std/collections/soa_vector/").size());
    } else if (normalizedMethodName.rfind("std/collections/vector/", 0) == 0) {
      normalizedMethodName = normalizedMethodName.substr(std::string("std/collections/vector/").size());
    } else if (normalizedMethodName.rfind("map/", 0) == 0) {
      normalizedMethodName = normalizedMethodName.substr(std::string("map/").size());
    } else if (normalizedMethodName.rfind("std/collections/map/", 0) == 0) {
      normalizedMethodName = normalizedMethodName.substr(std::string("std/collections/map/").size());
    }
    return normalizedMethodName;
  };
  auto normalizedTypeLeafName = [](std::string value) {
    value = normalizeBindingTypeName(value);
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(value, base, argText) && !base.empty()) {
      value = base;
    }
    if (!value.empty() && value.front() == '/') {
      value.erase(value.begin());
    }
    const size_t slash = value.find_last_of('/');
    return slash == std::string::npos ? value : value.substr(slash + 1);
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
      if (resolvedReceiverPath.rfind("/map/", 0) == 0) {
        const std::string_view helperSuffix =
            std::string_view(resolvedReceiverPath).substr(std::string_view("/map/").size());
        return joinMethodTarget("/std/collections/map", helperSuffix);
      }
      if (resolvedReceiverPath.rfind("/std/collections/map/", 0) == 0) {
        const std::string_view helperSuffix = std::string_view(resolvedReceiverPath).substr(
            std::string_view("/std/collections/map/").size());
        return joinMethodTarget("/map", helperSuffix);
      }
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
  const auto isCanonicalMapAccessMethodName = [&](std::string_view helperName) {
    return isValueSurfaceAccessMethodName(helperName) ||
           helperName == "at_ref" || helperName == "at_unsafe_ref";
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
    if (!explicitRemovedMethodPath.empty() && hasDefinitionPath(explicitRemovedMethodPath)) {
      if (((normalizedMethodName == "count" &&
            (collectionTypePath == "/vector" || collectionTypePath == "/array" ||
             collectionTypePath == "/soa_vector" || collectionTypePath == "/string")) ||
           (normalizedMethodName == "count_ref" &&
            (collectionTypePath == "/soa_vector" || collectionTypePath == "/map")))) {
        resolvedOut = explicitRemovedMethodPath;
        return true;
      }
      if (normalizedMethodName == "capacity" &&
          (collectionTypePath == "/vector" || collectionTypePath == "/soa_vector")) {
        resolvedOut = explicitRemovedMethodPath;
        return true;
      }
      if (isValueSurfaceAccessMethodName(normalizedMethodName) &&
          (collectionTypePath == "/vector" || collectionTypePath == "/array" ||
           collectionTypePath == "/string")) {
        resolvedOut = explicitRemovedMethodPath;
        return true;
      }
    }
    if (normalizedMethodName == "count" || normalizedMethodName == "count_ref") {
      if (collectionTypePath == "/array") {
        if (normalizedMethodName == "count_ref") {
          return false;
        }
        resolvedOut = preferVectorStdlibHelperPath("/array/count");
        return true;
      }
      if (collectionTypePath == "/vector" &&
          usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, "/vector")) {
        resolvedOut =
            preferredSoaHelperTargetForCollectionType(normalizedMethodName, "/vector");
        return true;
      }
      if (collectionTypePath == "/vector" && normalizedMethodName == "count") {
        resolvedOut = preferredBareVectorHelperTarget("count");
        return true;
      }
      if (collectionTypePath == "/soa_vector") {
        resolvedOut = preferredSoaHelperTargetForCollectionType(normalizedMethodName,
                                                                "/soa_vector");
        return true;
      }
      if (collectionTypePath == "/string" && normalizedMethodName == "count") {
        resolvedOut = "/string/count";
        return true;
      }
      if (collectionTypePath == "/map" &&
          (normalizedMethodName == "count" || normalizedMethodName == "count_ref")) {
        resolvedOut = preferredMapMethodTargetForCall(params, locals, receiver,
                                                      normalizedMethodName);
        return true;
      }
      if (collectionTypePath == "/Buffer" && normalizedMethodName == "count") {
        resolvedOut = preferredBufferMethodTargetForCall(params, locals, receiver, "count");
        return !resolvedOut.empty();
      }
    }
    if (normalizedMethodName == "capacity" && collectionTypePath == "/vector") {
      resolvedOut = preferredBareVectorHelperTarget("capacity");
      return true;
    }
    if ((normalizedMethodName == "empty" || normalizedMethodName == "is_valid" ||
         normalizedMethodName == "readback" || normalizedMethodName == "load" ||
         normalizedMethodName == "store") &&
        collectionTypePath == "/Buffer") {
      resolvedOut = preferredBufferMethodTargetForCall(params, locals, receiver, normalizedMethodName);
      return !resolvedOut.empty();
    }
    if (normalizedMethodName == "contains" && collectionTypePath == "/map") {
      resolvedOut = preferredMapMethodTargetForCall(params, locals, receiver, "contains");
      return true;
    }
    if (normalizedMethodName == "tryAt" && collectionTypePath == "/map") {
      resolvedOut = preferredMapMethodTargetForCall(params, locals, receiver, "tryAt");
      return true;
    }
    if (normalizedMethodName == "insert" && collectionTypePath == "/map") {
      resolvedOut = preferredMapMethodTargetForCall(params, locals, receiver, "insert");
      return true;
    }
    if (isValueSurfaceAccessMethodName(normalizedMethodName)) {
      if (collectionTypePath == "/array") {
        resolvedOut = preferVectorStdlibHelperPath("/array/" + normalizedMethodName);
        return true;
      }
      if (collectionTypePath == "/vector") {
        resolvedOut =
            preferVectorStdlibHelperPath("/std/collections/vector/" + normalizedMethodName);
        return true;
      }
      if (collectionTypePath == "/string") {
        resolvedOut = "/string/" + normalizedMethodName;
        return true;
      }
    }
    if (isCanonicalMapAccessMethodName(normalizedMethodName) &&
        collectionTypePath == "/map") {
      resolvedOut = preferredMapMethodTargetForCall(params, locals, receiver, normalizedMethodName);
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
    if ((normalizedMethodName == "get" || normalizedMethodName == "get_ref") &&
        (collectionTypePath == "/soa_vector" ||
         (collectionTypePath == "/vector" &&
          usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, "/vector")))) {
      resolvedOut = preferredSoaHelperTargetForCollectionType(
          normalizedMethodName,
          collectionTypePath == "/soa_vector" ? "/soa_vector" : "/vector");
      return true;
    }
    if ((normalizedMethodName == "ref" || normalizedMethodName == "ref_ref") &&
        (collectionTypePath == "/soa_vector" ||
         (collectionTypePath == "/vector" &&
          usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, "/vector")))) {
      resolvedOut = preferredSoaHelperTargetForCollectionType(
          normalizedMethodName,
          collectionTypePath == "/soa_vector" ? "/soa_vector" : "/vector");
      return true;
    }
    if ((normalizedMethodName == "push" || normalizedMethodName == "reserve") &&
        (collectionTypePath == "/soa_vector" ||
         (collectionTypePath == "/vector" &&
          usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName,
                                                       "/vector")))) {
      resolvedOut = preferredSoaHelperTargetForCollectionType(
          normalizedMethodName,
          collectionTypePath == "/soa_vector" ? "/soa_vector" : "/vector");
      return true;
    }
    if (normalizedMethodName == "to_soa" && collectionTypePath == "/vector") {
      resolvedOut = "/to_soa";
      return true;
    }
    if ((normalizedMethodName == "to_aos" || normalizedMethodName == "to_aos_ref") &&
        (collectionTypePath == "/soa_vector" || collectionTypePath == "/vector")) {
      resolvedOut = preferredSoaHelperTargetForCollectionType(
          normalizedMethodName,
          collectionTypePath == "/soa_vector" ? "/soa_vector" : "/vector");
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
  auto preferredExperimentalMapReferenceMethodTarget = [&](const std::string &helperName) {
    if (helperName == "count") {
      return std::string("/std/collections/experimental_map/mapCountRef");
    }
    if (helperName == "contains") {
      return std::string("/std/collections/experimental_map/mapContainsRef");
    }
    if (helperName == "tryAt") {
      return std::string("/std/collections/experimental_map/mapTryAtRef");
    }
    if (helperName == "at") {
      return std::string("/std/collections/experimental_map/mapAtRef");
    }
    if (helperName == "at_unsafe") {
      return std::string("/std/collections/experimental_map/mapAtUnsafeRef");
    }
    if (helperName == "insert") {
      return std::string("/std/collections/experimental_map/mapInsertRef");
    }
    return std::string();
  };
  std::function<bool(const Expr &, std::string &)> resolveBorrowedVectorReceiver =
      [&](const Expr &candidate, std::string &elemTypeOut) {
    auto extractBorrowedVectorType = [&](const BindingInfo &binding) {
      const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
      if (normalizedType == "vector" && !binding.typeTemplateArg.empty()) {
        elemTypeOut = binding.typeTemplateArg;
        return true;
      }
      if ((normalizedType != "Reference" && normalizedType != "Pointer") ||
          binding.typeTemplateArg.empty()) {
        return false;
      }
      std::string pointeeBase;
      std::string pointeeArgText;
      const std::string normalizedPointee =
          normalizeBindingTypeName(binding.typeTemplateArg);
      if (!splitTemplateTypeName(normalizedPointee, pointeeBase,
                                 pointeeArgText)) {
        return false;
      }
      if (normalizeBindingTypeName(pointeeBase) != "vector" ||
          pointeeArgText.empty()) {
        return false;
      }
      elemTypeOut = pointeeArgText;
      return true;
    };
    if (candidate.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
        return extractBorrowedVectorType(*paramBinding);
      }
      if (auto localIt = locals.find(candidate.name); localIt != locals.end()) {
        return extractBorrowedVectorType(localIt->second);
      }
    }
    if (candidate.kind == Expr::Kind::Call && !candidate.isBinding &&
        isSimpleCallName(candidate, "location") && candidate.args.size() == 1) {
      return resolveBorrowedVectorReceiver(candidate.args.front(), elemTypeOut);
    }
    if (candidate.kind == Expr::Kind::Call && !candidate.isBinding &&
        isSimpleCallName(candidate, "dereference") && candidate.args.size() == 1) {
      return resolveBorrowedVectorReceiver(candidate.args.front(), elemTypeOut);
    }
    std::string inferredTypeText;
    if (!inferQueryExprTypeText(candidate, params, locals, inferredTypeText) ||
        inferredTypeText.empty()) {
      return false;
    }
    const std::string normalizedType = normalizeBindingTypeName(inferredTypeText);
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalizedType, base, argText)) {
      return false;
    }
    const std::string normalizedBase = normalizeBindingTypeName(base);
    if (normalizedBase != "Reference" && normalizedBase != "Pointer") {
      return false;
    }
    std::string pointeeBase;
    std::string pointeeArgText;
    const std::string normalizedPointee = normalizeBindingTypeName(argText);
    if (!splitTemplateTypeName(normalizedPointee, pointeeBase, pointeeArgText)) {
      return false;
    }
    if (normalizeBindingTypeName(pointeeBase) != "vector" || pointeeArgText.empty()) {
      return false;
    }
    elemTypeOut = pointeeArgText;
    return true;
  };
  auto preferredBorrowedSoaAccessHelperTarget = [&](std::string_view helperName) {
    if (helperName == "count") {
      helperName = "count_ref";
    } else if (helperName == "get") {
      helperName = "get_ref";
    } else if (helperName == "ref") {
      helperName = "ref_ref";
    } else if (helperName == "to_aos") {
      helperName = "to_aos_ref";
    }
    return preferredSoaHelperTargetForCollectionType(helperName, "/soa_vector");
  };
  auto isCanonicalSoaWrapperMethod = [&](std::string_view helperName) {
    return helperName == "count" || helperName == "count_ref" ||
           helperName == "get" || helperName == "get_ref" ||
           helperName == "ref" || helperName == "ref_ref" ||
           helperName == "to_aos" || helperName == "to_aos_ref" ||
           helperName == "push" || helperName == "reserve";
  };
  auto redirectConcreteExperimentalSoaMethodTarget = [&](const std::string &resolvedType) -> bool {
    const bool isConcreteExperimentalSoaReceiver =
        resolvedType.rfind("/std/collections/experimental_soa_vector/SoaVector__", 0) == 0;
    if (!isConcreteExperimentalSoaReceiver ||
        !isCanonicalSoaWrapperMethod(normalizedMethodName)) {
      return false;
    }
    resolvedOut = preferredSoaHelperTargetForCollectionType(normalizedMethodName,
                                                            "/soa_vector");
    return true;
  };
  auto setIndexedArgsPackMapMethodTarget = [&](const Expr &receiverExpr, const std::string &helperName) -> bool {
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
    if (!resolveArgsPackAccessTarget(*accessReceiver, indexedElemType) ||
        !extractMapKeyValueTypesFromTypeText(indexedElemType, keyType, valueType)) {
      return false;
    }
    resolvedOut = preferredMapMethodTargetForCall(params, locals, receiverExpr, helperName);
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
    if (receiverHelperName.rfind("map/", 0) == 0) {
      receiverHelperName.erase(0, std::string("map/").size());
    } else if (receiverHelperName.rfind("std/collections/map/", 0) == 0) {
      receiverHelperName.erase(0, std::string("std/collections/map/").size());
    }
    std::string keyType;
    std::string valueType;
    if (isCanonicalMapAccessMethodName(receiverHelperName) &&
        resolveMapTarget(receiver.args.front(), keyType, valueType)) {
      const std::string resolvedReceiver =
          preferredMapMethodTargetForCall(params, locals, receiver, receiverHelperName);
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
    if (normalizedMethodName == "count" || normalizedMethodName == "count_ref") {
      if (normalizedMethodName == "count" &&
          resolveArgsPackCountTarget(receiver, elemType)) {
        resolvedOut = preferVectorStdlibHelperPath("/array/count");
        return true;
      }
      if (resolveVectorTarget(receiver, elemType) &&
          usesSamePathSoaHelperTargetForCurrentImports(normalizedMethodName)) {
        resolvedOut = preferredSoaHelperTargetForCurrentImports(normalizedMethodName);
        return true;
      }
      if (normalizedMethodName == "count" &&
          resolveVectorTarget(receiver, elemType)) {
        resolvedOut = preferredBareVectorHelperTarget("count");
        return true;
      }
      if (resolveSoaVectorTarget(receiver, elemType)) {
        resolvedOut = preferredSoaHelperTargetForCurrentImports(normalizedMethodName);
        return true;
      }
      if (normalizedMethodName == "count" &&
          resolveArrayTarget(receiver, elemType)) {
        resolvedOut = preferVectorStdlibHelperPath("/array/count");
        return true;
      }
      if (normalizedMethodName == "count" && resolveStringTarget(receiver)) {
        resolvedOut = "/string/count";
        return true;
      }
      if (normalizedMethodName == "count" &&
          setIndexedArgsPackMapMethodTarget(receiver, "count")) {
        return true;
      }
    }
    if (normalizedMethodName == "capacity" && resolveVectorTarget(receiver, elemType)) {
      resolvedOut = preferredBareVectorHelperTarget("capacity");
      return true;
    }
    if ((normalizedMethodName == "count" || normalizedMethodName == "count_ref") &&
        resolveMapTarget(receiver, keyType, valueType)) {
      resolvedOut = preferredMapMethodTargetForCall(params, locals, receiver,
                                                    normalizedMethodName);
      return true;
    }
    if ((normalizedMethodName == "contains" || normalizedMethodName == "tryAt" ||
         normalizedMethodName == "insert") &&
        resolveMapTarget(receiver, keyType, valueType)) {
      resolvedOut = preferredMapMethodTargetForCall(params, locals, receiver, normalizedMethodName);
      return true;
    }
    if ((normalizedMethodName == "contains" || normalizedMethodName == "tryAt" ||
         normalizedMethodName == "insert") &&
        setIndexedArgsPackMapMethodTarget(receiver, normalizedMethodName)) {
      return true;
    }
    if (isValueSurfaceAccessMethodName(normalizedMethodName)) {
      if (resolveArgsPackAccessTarget(receiver, elemType)) {
        resolvedOut = preferVectorStdlibHelperPath("/array/" + normalizedMethodName);
        return true;
      }
      if (resolveVectorTarget(receiver, elemType)) {
        resolvedOut =
            preferVectorStdlibHelperPath("/std/collections/vector/" + normalizedMethodName);
        return true;
      }
      if (resolveArrayTarget(receiver, elemType)) {
        resolvedOut = preferVectorStdlibHelperPath("/array/" + normalizedMethodName);
        return true;
      }
      if (resolveStringTarget(receiver)) {
        resolvedOut = "/string/" + normalizedMethodName;
        return true;
      }
      if (setIndexedArgsPackMapMethodTarget(receiver, normalizedMethodName)) {
        return true;
      }
    }
    if (isCanonicalMapAccessMethodName(normalizedMethodName) &&
        setIndexedArgsPackMapMethodTarget(receiver, normalizedMethodName)) {
      return true;
    }
    if (isCanonicalMapAccessMethodName(normalizedMethodName) &&
        resolveMapTarget(receiver, keyType, valueType)) {
      resolvedOut = preferredMapMethodTargetForCall(params, locals, receiver, normalizedMethodName);
      return true;
    }
    if ((normalizedMethodName == "count" || normalizedMethodName == "count_ref") &&
        resolveVectorTarget(receiver, elemType) &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, "/vector")) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, "/vector");
      return true;
    }
    if ((normalizedMethodName == "count" || normalizedMethodName == "count_ref") &&
        this->resolveSoaVectorOrExperimentalBorrowedReceiver(
            receiver, params, locals, resolveDirectReceiver, elemType)) {
      resolvedOut = preferredBorrowedSoaAccessHelperTarget(normalizedMethodName);
      return true;
    }
    if ((normalizedMethodName == "count" || normalizedMethodName == "count_ref") &&
        resolveSoaVectorTarget(receiver, elemType)) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, "/soa_vector");
      return true;
    }
    if ((normalizedMethodName == "get" || normalizedMethodName == "get_ref") &&
        resolveVectorTarget(receiver, elemType) &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, "/vector")) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, "/vector");
      return true;
    }
    if ((normalizedMethodName == "get" || normalizedMethodName == "get_ref") &&
        resolveBorrowedVectorReceiver(receiver, elemType) &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, "/vector")) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, "/vector");
      return true;
    }
    if ((normalizedMethodName == "get" || normalizedMethodName == "get_ref") &&
        resolveSoaVectorTarget(receiver, elemType)) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, "/soa_vector");
      return true;
    }
    if ((normalizedMethodName == "get" || normalizedMethodName == "get_ref") &&
        this->resolveSoaVectorOrExperimentalBorrowedReceiver(
            receiver, params, locals, resolveDirectReceiver, elemType)) {
      resolvedOut = preferredBorrowedSoaAccessHelperTarget(normalizedMethodName);
      return true;
    }
    if ((normalizedMethodName == "ref" || normalizedMethodName == "ref_ref") &&
        resolveVectorTarget(receiver, elemType) &&
        usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, "/vector")) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, "/vector");
      return true;
    }
    if ((normalizedMethodName == "ref" || normalizedMethodName == "ref_ref") &&
        this->resolveSoaVectorOrExperimentalBorrowedReceiver(
            receiver, params, locals, resolveDirectReceiver, elemType)) {
      resolvedOut = preferredBorrowedSoaAccessHelperTarget(normalizedMethodName);
      return true;
    }
    if ((normalizedMethodName == "ref" || normalizedMethodName == "ref_ref") &&
        resolveSoaVectorTarget(receiver, elemType)) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, "/soa_vector");
      return true;
    }
    if ((normalizedMethodName == "to_aos" || normalizedMethodName == "to_aos_ref") &&
        resolveVectorTarget(receiver, elemType)) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, "/vector");
      return true;
    }
    if ((normalizedMethodName == "to_aos" || normalizedMethodName == "to_aos_ref") &&
        this->resolveSoaVectorOrExperimentalBorrowedReceiver(
            receiver, params, locals, resolveDirectReceiver, elemType)) {
      resolvedOut = preferredBorrowedSoaAccessHelperTarget(normalizedMethodName);
      return true;
    }
    if ((normalizedMethodName == "to_aos" || normalizedMethodName == "to_aos_ref") &&
        resolveSoaVectorTarget(receiver, elemType)) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, "/soa_vector");
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
    if (resolveInferExperimentalMapTarget(params, locals, receiver, keyType, valueType)) {
      resolvedOut = preferredExperimentalMapReferenceMethodTarget(normalizedMethodName);
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
  resolvedOut = joinMethodTarget(resolvedType, normalizedMethodName);
  return returnWithMethodTargetMemo(true);
}

} // namespace primec::semantics
