#include "IrLowererInlineCallContextHelpers.h"

#include <string_view>

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
  return stdCollectionsRoot() + "/experimental_" + std::string(collectionName) + "/";
}

std::string experimentalCollectionTypePath(std::string_view collectionName,
                                           std::string_view typeName) {
  return experimentalCollectionMemberRoot(collectionName) + std::string(typeName);
}

std::string collectionWrapperAlias(std::string_view collectionName,
                                   std::string_view suffix) {
  return std::string(collectionName) + std::string(suffix);
}

bool isGeneratedStdlibCollectionStructPath(std::string_view path) {
  return isSinglePathSegmentWithPrefix(path, experimentalCollectionTypePath("vector", "Vector") + "__") ||
         isSinglePathSegmentWithPrefix(path, "/std/collections/experimental_map/Map__") ||
         isSinglePathSegmentWithPrefix(path, "/std/collections/experimental_soa_vector/SoaVector__") ||
         isSinglePathSegmentWithPrefix(path, "/std/collections/internal_soa_storage/SoaColumn__") ||
         isSinglePathSegmentWithPrefix(path, "/std/collections/internal_soa_storage/SoaFieldView__") ||
         isGeneratedSinglePathSegmentWithPrefix(path, "/std/collections/internal_soa_storage/SoaColumns");
}

bool isGeneratedStdlibCollectionConstructorHelperPath(std::string_view path) {
  return isSinglePathSegmentWithPrefix(path, collectionMemberRoot("vector") + "vector__") ||
         isSinglePathSegmentWithPrefix(path, experimentalCollectionMemberRoot("vector") + "vector__") ||
         isSinglePathSegmentWithPrefix(path, "/std/collections/map/map__") ||
         isSinglePathSegmentWithPrefix(path, "/std/collections/experimental_map/map__") ||
         isSinglePathSegmentWithPrefix(path, "/std/collections/soa_vector/soa_vector__") ||
         isSinglePathSegmentWithPrefix(path, "/std/collections/experimental_soa_vector/soa_vector__");
}

bool isGeneratedStdlibVectorImplementationHelperPath(std::string_view path) {
  const std::string prefix = experimentalCollectionMemberRoot("vector");
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
      callee.fullPath == "/std/collections/mapInsert" ||
      callee.fullPath.rfind("/std/collections/mapInsert__", 0) == 0 ||
      callee.fullPath == "/std/collections/map/insert" ||
      callee.fullPath.rfind("/std/collections/map/insert__", 0) == 0 ||
      callee.fullPath == "/std/collections/map/insert_ref" ||
      callee.fullPath.rfind("/std/collections/map/insert_ref__", 0) == 0 ||
      callee.fullPath == "/std/collections/internal_map/insertImpl" ||
      callee.fullPath.rfind("/std/collections/internal_map/insertImpl__", 0) == 0 ||
      callee.fullPath == "/std/collections/internal_map/insertRefImpl" ||
      callee.fullPath.rfind("/std/collections/internal_map/insertRefImpl__", 0) == 0 ||
      callee.fullPath == "/std/collections/experimental_map/mapInsert" ||
      callee.fullPath.rfind("/std/collections/experimental_map/mapInsert__", 0) == 0 ||
      callee.fullPath == "/std/collections/experimental_map/mapInsertRef" ||
      callee.fullPath.rfind("/std/collections/experimental_map/mapInsertRef__", 0) == 0;
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
