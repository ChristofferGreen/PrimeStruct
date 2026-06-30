// soa-surface-audit: exempt
#include "IrLowererInlineCallContextHelpers.h"

#include <string_view>

#include "IrLowererSetupTypeCollectionHelpers.h"
#include "primec/StdlibCollectionPaths.h"

namespace primec::ir_lowerer {

namespace {

bool isSinglePathSegmentWithPrefix(std::string_view path, std::string_view prefix) {
  return path.rfind(prefix, 0) == 0 && path.size() > prefix.size() &&
         path.find('/', prefix.size()) == std::string_view::npos;
}

bool isGeneratedSinglePathSegmentWithPrefix(std::string_view path, std::string_view prefix) {
  return isSinglePathSegmentWithPrefix(path, prefix) &&
         path.find("__", prefix.size()) != std::string_view::npos;
}

std::string stdCollectionsRoot() {
  return "/std/collections";
}

std::string collectionMemberRoot(std::string_view collectionName) {
  return stdCollectionsRoot() + "/" + std::string(collectionName) + "/";
}

std::string experimentalCollectionMemberRoot(std::string_view collectionName) {
  return stdCollectionsRoot() + "/" + collection_paths::experimentalFolder(collectionName) + "/";
}

std::string collectionWrapperAlias(std::string_view collectionName,
                                   std::string_view suffix) {
  return std::string(collectionName) + std::string(suffix);
}

bool isGeneratedStdlibCollectionStructPath(std::string_view path) {
  return isSinglePathSegmentWithPrefix(path, vectorBackingTypePath() + "__") ||
         isSinglePathSegmentWithPrefix(path, keyValueStorageStructRootPath() + "__") ||
         isSinglePathSegmentWithPrefix(path, collection_paths::specializedTypePrefix(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName)) ||
         isSinglePathSegmentWithPrefix(path, collection_paths::specializedTypePrefix(collection_paths::kInternalSoaStorageFolder, collection_paths::kSoaColumnTypeName)) ||
         isSinglePathSegmentWithPrefix(path, collection_paths::specializedTypePrefix(collection_paths::kInternalSoaStorageFolder, "SoaFieldView")) ||
         isGeneratedSinglePathSegmentWithPrefix(path, collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "SoaColumns"));
}

bool isGeneratedStdlibCollectionConstructorHelperPath(std::string_view path) {
  return isSinglePathSegmentWithPrefix(path, collectionMemberRoot("vector") + "vector__") ||
         isSinglePathSegmentWithPrefix(path, vectorBackingMemberRoot() + "vector__") ||
         isSinglePathSegmentWithPrefix(path, collectionMemberRoot("map") + "map__") ||
         isSinglePathSegmentWithPrefix(path, experimentalCollectionMemberRoot("map") + "map__") ||
         isSinglePathSegmentWithPrefix(path, "/std/collections/soa/soa__") ||
         isSinglePathSegmentWithPrefix(path, "/std/collections/soa/soa__") ||
         isSinglePathSegmentWithPrefix(path, collection_paths::specializedTypePrefix(collection_paths::kExperimentalSoaVectorFolder, "soa"));
}

bool isGeneratedStdlibVectorImplementationHelperPath(std::string_view path) {
  const std::string prefix = vectorBackingMemberRoot();
  const std::string_view Prefix(prefix.data(), prefix.size());
  if (!isSinglePathSegmentWithPrefix(path, Prefix)) {
    return false;
  }
  std::string_view leaf = path.substr(Prefix.size());
  const size_t generatedSuffix = leaf.find("__");
  if (generatedSuffix == std::string_view::npos) {
    return false;
  }
  leaf = leaf.substr(0, generatedSuffix);
  return leaf == collectionWrapperAlias("vector", "SlotUnsafe") ||
         leaf == collectionWrapperAlias("vector", "DataPtr") ||
         leaf == collectionWrapperAlias("vector", "InitSlot") ||
         leaf == collectionWrapperAlias("vector", "DropSlot") ||
         leaf == collectionWrapperAlias("vector", "TakeSlot") ||
         leaf == collectionWrapperAlias("vector", "BorrowSlot") ||
         leaf == collectionWrapperAlias("vector", "DropRange") ||
         leaf == collectionWrapperAlias("vector", "MovePrefixToBuffer") ||
         leaf == collectionWrapperAlias("vector", "CheckShape") ||
         leaf == collectionWrapperAlias("vector", "CheckIndex") ||
         leaf == collectionWrapperAlias("vector", "ReserveInternal");
}

} // namespace

bool prepareInlineDefinitionCallContext(
    const Definition &callee,
    bool requireValue,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<bool(const Definition &)> &isStructDefinition,
    std::unordered_set<std::string> &inlineStack,
    std::unordered_set<std::string> &loweredCallTargets,
    const OnErrorByDefinition &onErrorByDef,
    InlineDefinitionCallContextSetup &out,
    std::string &error) {
  out = InlineDefinitionCallContextSetup{};
  if (!getReturnInfo(callee.fullPath, out.returnInfo)) {
    return false;
  }

  out.structDefinition = isStructDefinition(callee);
  const bool isGeneratedMapInsertHelper =
      callee.fullPath == collection_paths::memberPath(collection_paths::kMapFolder, "insertImpl") ||
      callee.fullPath.rfind(collection_paths::specializedTypePrefix(collection_paths::kMapFolder, "insertImpl"), 0) == 0 ||
      callee.fullPath == collection_paths::memberPath(collection_paths::kMapFolder, "insertRefImpl") ||
      callee.fullPath.rfind(collection_paths::specializedTypePrefix(collection_paths::kMapFolder, "insertRefImpl"), 0) == 0;
  if (out.returnInfo.returnsVoid && requireValue && !out.structDefinition &&
      !isGeneratedMapInsertHelper) {
    error = "void call not allowed in expression context: " + callee.fullPath;
    return false;
  }

  const bool alreadyInInlineStack = inlineStack.count(callee.fullPath) != 0;
  const bool allowsGeneratedCollectionReentry =
      (out.structDefinition && isGeneratedStdlibCollectionStructPath(callee.fullPath)) ||
      isGeneratedStdlibCollectionConstructorHelperPath(callee.fullPath) ||
      isGeneratedStdlibVectorImplementationHelperPath(callee.fullPath);
  if (alreadyInInlineStack && !allowsGeneratedCollectionReentry) {
    error = "native backend does not support recursive calls: " + callee.fullPath;
    return false;
  }
  if (!alreadyInInlineStack) {
    inlineStack.insert(callee.fullPath);
    out.insertedInlineStackEntry = true;
  }
  loweredCallTargets.insert(callee.fullPath);

  auto onErrorIt = onErrorByDef.find(callee.fullPath);
  if (onErrorIt != onErrorByDef.end() && onErrorIt->second.has_value()) {
    out.scopedOnError = onErrorIt->second;
  }
  if (out.returnInfo.isResult) {
    out.scopedResult = ResultReturnInfo{true, out.returnInfo.resultHasValue};
  }
  return true;
}

} // namespace primec::ir_lowerer
