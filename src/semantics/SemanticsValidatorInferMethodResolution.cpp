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
  std::string normalizedMethodName = methodName;
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
  const auto isValueSurfaceAccessMethodName = [](std::string_view helperName) {
    return helperName == "at" || helperName == "at_unsafe";
  };
  const auto isCanonicalMapAccessMethodName = [&](std::string_view helperName) {
    return isValueSurfaceAccessMethodName(helperName) ||
           helperName == "at_ref" || helperName == "at_unsafe_ref";
  };
  const std::string explicitRemovedMethodPath =
      explicitRemovedCollectionMethodPath(methodName, expr.namespacePrefix);
  auto resolveCollectionMethodFromTypePath = [&](const std::string &collectionTypePath) -> bool {
    if (!explicitRemovedMethodPath.empty() && hasDefinitionPath(explicitRemovedMethodPath)) {
      if (normalizedMethodName == "count" &&
          (collectionTypePath == "/vector" || collectionTypePath == "/array" ||
           collectionTypePath == "/soa_vector" || collectionTypePath == "/string")) {
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
    if (normalizedMethodName == "count") {
      if (collectionTypePath == "/array") {
        resolvedOut = preferVectorStdlibHelperPath("/array/count");
        return true;
      }
      if (collectionTypePath == "/vector" &&
          usesSamePathSoaHelperTargetForCollectionType("count", "/vector")) {
        resolvedOut =
            preferredSoaHelperTargetForCollectionType("count", "/vector");
        return true;
      }
      if (collectionTypePath == "/vector") {
        resolvedOut = preferredBareVectorHelperTarget("count");
        return true;
      }
      if (collectionTypePath == "/soa_vector") {
        resolvedOut = preferredSoaHelperTargetForCollectionType("count",
                                                                "/soa_vector");
        return true;
      }
      if (collectionTypePath == "/string") {
        resolvedOut = "/string/count";
        return true;
      }
      if (collectionTypePath == "/map") {
        resolvedOut = preferredMapMethodTargetForCall(params, locals, receiver, "count");
        return true;
      }
      if (collectionTypePath == "/Buffer") {
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
        resolvedOut = preferVectorStdlibHelperPath("/vector/" + normalizedMethodName);
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
    if (normalizedMethodName == "get" &&
        (collectionTypePath == "/soa_vector" ||
         (collectionTypePath == "/vector" &&
          usesSamePathSoaHelperTargetForCollectionType("get", "/vector")))) {
      resolvedOut = preferredSoaHelperTargetForCollectionType(
          "get",
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
    if (normalizedMethodName == "to_aos" &&
        (collectionTypePath == "/soa_vector" || collectionTypePath == "/vector")) {
      resolvedOut = preferredSoaHelperTargetForCollectionType(
          "to_aos",
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
  auto resolveBorrowedSoaVectorReceiver = [&](const Expr &candidate,
                                              std::string &elemTypeOut) {
    std::string inferredTypeText;
    return inferQueryExprTypeText(candidate, params, locals, inferredTypeText) &&
           !inferredTypeText.empty() &&
           resolveExperimentalBorrowedSoaTypeText(inferredTypeText, elemTypeOut);
  };
  auto preferredBorrowedSoaRefHelperTarget = [&]() {
    return std::string("/std/collections/experimental_soa_vector/soaVectorRefRef");
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

  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
    std::string resolvedType = resolveCalleePath(receiver);
    if (!resolvedType.empty() && structNames_.count(resolvedType) > 0) {
      resolvedOut = resolvedType + "/" + normalizedMethodName;
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
        resolvedOut = resolvedReceiverType + "/" + normalizedMethodName;
        return true;
      }
      if (isPrimitiveBindingTypeName(normalizedReceiverType)) {
        resolvedOut = "/" + normalizedReceiverType + "/" + normalizedMethodName;
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
      resolvedOut = resolvedType + "/" + normalizedMethodName;
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
      resolvedOut = "/" + receiverType + "/" + normalizedMethodName;
      return true;
    }
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding &&
      shouldPreferStructReturnMethodTargetForCall(receiver)) {
    std::string inferredStructPath = inferStructReturnPath(receiver, params, locals);
    if (!inferredStructPath.empty() && structNames_.count(inferredStructPath) > 0) {
      resolvedOut = inferredStructPath + "/" + normalizedMethodName;
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
            resolvedOut = resolvedReceiverType + "/" + normalizedMethodName;
            return true;
          }
          const std::string normalizedReceiverType =
              normalizeBindingTypeName(receiverTypeText);
          if (isPrimitiveBindingTypeName(normalizedReceiverType)) {
            resolvedOut = "/" + normalizedReceiverType + "/" + normalizedMethodName;
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
    const std::string resolvedType = resolveMethodStructTypePath(receiver.name, receiver.namespacePrefix);
    if (!resolvedType.empty()) {
      resolvedOut = resolvedType + "/" + normalizedMethodName;
      return true;
    }
  }
  {
    std::string elemType;
    std::string keyType;
    std::string valueType;
    if (normalizedMethodName == "count") {
      if (resolveArgsPackCountTarget(receiver, elemType)) {
        resolvedOut = preferVectorStdlibHelperPath("/array/count");
        return true;
      }
      if (resolveVectorTarget(receiver, elemType) &&
          usesSamePathSoaHelperTargetForCurrentImports("count")) {
        resolvedOut = preferredSoaHelperTargetForCurrentImports("count");
        return true;
      }
      if (resolveVectorTarget(receiver, elemType)) {
        resolvedOut = preferredBareVectorHelperTarget("count");
        return true;
      }
      if (resolveSoaVectorTarget(receiver, elemType)) {
        resolvedOut = preferredSoaHelperTargetForCurrentImports("count");
        return true;
      }
      if (resolveArrayTarget(receiver, elemType)) {
        resolvedOut = preferVectorStdlibHelperPath("/array/count");
        return true;
      }
      if (resolveStringTarget(receiver)) {
        resolvedOut = "/string/count";
        return true;
      }
      if (setIndexedArgsPackMapMethodTarget(receiver, "count")) {
        return true;
      }
    }
    if (normalizedMethodName == "capacity" && resolveVectorTarget(receiver, elemType)) {
      resolvedOut = preferredBareVectorHelperTarget("capacity");
      return true;
    }
    if (normalizedMethodName == "count" && resolveMapTarget(receiver, keyType, valueType)) {
      resolvedOut = preferredMapMethodTargetForCall(params, locals, receiver, "count");
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
        resolvedOut = preferVectorStdlibHelperPath("/vector/" + normalizedMethodName);
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
    if (normalizedMethodName == "get" &&
        resolveVectorTarget(receiver, elemType) &&
        usesSamePathSoaHelperTargetForCollectionType("get", "/vector")) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType("get", "/vector");
      return true;
    }
    if (normalizedMethodName == "get" &&
        resolveSoaVectorTarget(receiver, elemType)) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType("get", "/soa_vector");
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
        resolveBorrowedSoaVectorReceiver(receiver, elemType)) {
      resolvedOut = preferredBorrowedSoaRefHelperTarget();
      return true;
    }
    if ((normalizedMethodName == "ref" || normalizedMethodName == "ref_ref") &&
        resolveSoaVectorTarget(receiver, elemType)) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType(normalizedMethodName, "/soa_vector");
      return true;
    }
    if (normalizedMethodName == "to_aos" &&
        resolveVectorTarget(receiver, elemType)) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType("to_aos", "/vector");
      return true;
    }
    if (normalizedMethodName == "to_aos" &&
        resolveSoaVectorTarget(receiver, elemType)) {
      resolvedOut =
          preferredSoaHelperTargetForCollectionType("to_aos", "/soa_vector");
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
    if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
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
  if (typeName.empty() && receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
    BindingInfo callBinding;
    std::vector<std::string> resolvedCandidates;
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
    const std::string resolvedReceiverPath = resolveCalleePath(receiver);
    appendResolvedCandidate(resolvedReceiverPath);
    if (resolvedReceiverPath.rfind("/vector/", 0) == 0) {
      appendResolvedCandidate("/std/collections/vector/" +
                              resolvedReceiverPath.substr(std::string("/vector/").size()));
    } else if (resolvedReceiverPath.rfind("/std/collections/vector/", 0) == 0) {
      appendResolvedCandidate("/vector/" +
                              resolvedReceiverPath.substr(std::string("/std/collections/vector/").size()));
    } else if (resolvedReceiverPath.rfind("/map/", 0) == 0) {
      appendResolvedCandidate("/std/collections/map/" +
                              resolvedReceiverPath.substr(std::string("/map/").size()));
    } else if (resolvedReceiverPath.rfind("/std/collections/map/", 0) == 0) {
      appendResolvedCandidate("/map/" +
                              resolvedReceiverPath.substr(std::string("/std/collections/map/").size()));
    }
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
      break;
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
  {
    const std::string receiverTypeText =
        typeTemplateArg.empty() ? typeName : typeName + "<" + typeTemplateArg + ">";
    const std::string receiverCollectionTypePath =
        inferMethodCollectionTypePathFromTypeText(normalizeBindingTypeName(receiverTypeText));
    if (!receiverCollectionTypePath.empty() &&
        resolveCollectionMethodFromTypePath(receiverCollectionTypePath)) {
      return true;
    }
  }
  if (typeName == "FileError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "is_eof" ||
       normalizedMethodName == "status" || normalizedMethodName == "result")) {
    resolvedOut = preferredFileErrorHelperTarget(normalizedMethodName);
    return !resolvedOut.empty();
  }
  if (typeName == "ImageError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    resolvedOut = preferredImageErrorHelperTarget(normalizedMethodName);
    return !resolvedOut.empty();
  }
  if (typeName == "ContainerError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    resolvedOut = preferredContainerErrorHelperTarget(normalizedMethodName);
    return !resolvedOut.empty();
  }
  if (typeName == "GfxError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    resolvedOut =
        preferredGfxErrorHelperTarget(normalizedMethodName,
                                      resolveMethodStructTypePath(typeName, expr.namespacePrefix));
    if (!resolvedOut.empty()) {
      return true;
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
      return !resolvedOut.empty();
    }
  }
  if (typeName == "Pointer" || typeName == "Reference") {
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      resolvedOut = "/" + typeName + "/" + normalizedMethodName;
      return true;
    }
    return false;
  }
  if (isPrimitiveBindingTypeName(typeName)) {
    resolvedOut = "/" + normalizeBindingTypeName(typeName) + "/" + normalizedMethodName;
    return true;
  }
  std::string resolvedType = resolveMethodStructTypePath(typeName, expr.namespacePrefix);
  if (resolvedType.empty()) {
    resolvedType = resolveTypePath(typeName, expr.namespacePrefix);
  }
  resolvedOut = resolvedType + "/" + normalizedMethodName;
  return true;
}

} // namespace primec::semantics
