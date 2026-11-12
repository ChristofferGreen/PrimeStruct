#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/ast/Ast.h"
#include "primec/support/StdlibSurfaceRegistry.h"

namespace primec {
struct SemanticProgram;
}

namespace primec::ir_lowerer {


std::string preferredFileErrorHelperTarget(
    std::string_view helperName,
    const std::unordered_map<std::string, const Definition *> &defMap);
std::string preferredImageErrorHelperTarget(
    std::string_view helperName,
    const std::unordered_map<std::string, const Definition *> &defMap);
std::string preferredContainerErrorHelperTarget(
    std::string_view helperName,
    const std::unordered_map<std::string, const Definition *> &defMap);
std::string preferredGfxErrorHelperTarget(
    std::string_view helperName,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::string &resolvedTypePath = "");

const StdlibSurfaceMetadata *vectorHelperSurfaceMetadata();
const StdlibSurfaceMetadata *keyValueHelperSurfaceMetadata();
const StdlibSurfaceMetadata *keyValueConstructorSurfaceMetadata();
bool resolveVectorHelperAliasName(const Expr &expr, std::string &helperNameOut);
bool resolveKeyValueHelperAliasName(const Expr &expr, std::string &helperNameOut);
bool resolveBorrowedKeyValueHelperAliasName(const Expr &expr, std::string &helperNameOut);

// The rooted path a call expression names: its own name when that is already a path
// or has no namespace, else the namespace prefix joined to the name.
std::string resolveScopedCallPath(const Expr &expr);
std::string stdCollectionsRoot(bool leadingSlash = true);
std::string collectionTypePath(std::string_view collectionName,
                               bool leadingSlash = true);
std::string collectionMemberRoot(std::string_view collectionName,
                                 bool leadingSlash = true);
std::string collectionMemberPath(std::string_view collectionName,
                                 std::string_view memberName,
                                 bool leadingSlash = true);
std::string canonicalKeyValueHelperPath(std::string_view memberName,
                                        bool leadingSlash = true);
std::string canonicalKeyValueConstructorPath(bool leadingSlash = true);
std::string experimentalCollectionMemberRoot(std::string_view collectionName,
                                             bool leadingSlash = true);
std::string experimentalCollectionTypePath(std::string_view collectionName,
                                           std::string_view typeName,
                                           bool leadingSlash = true);
std::string collectionWrapperAlias(std::string_view collectionName,
                                   std::string_view suffix);
std::string keyValueCollectionAliasRoot(bool leadingSlash = true);
bool isBuiltinCollectionTypeName(std::string_view typeName,
                                 std::string_view collectionName);
bool isExperimentalCollectionTypeName(std::string_view typeName,
                                      std::string_view collectionName,
                                      std::string_view experimentalTypeName);
std::string keyValueStorageStructRootPath(bool leadingSlash = true);
bool isKeyValueStorageStructPath(std::string_view path);
std::string normalizeBuiltinCollectionStructPath(std::string_view collectionName);
std::string normalizeExperimentalCollectionTypePath(std::string_view typeName,
                                                    std::string_view collectionName,
                                                    std::string_view experimentalTypeName);

std::string normalizeCollectionHelperPath(const std::string &path);
bool isExplicitRemovedVectorMethodAliasPath(const std::string &methodName);
bool isExplicitKeyValueMethodAliasPath(const std::string &methodName);
bool isExplicitKeyValueContainsOrTryAtMethodPath(const std::string &methodName);
bool isUnqualifiedCollectionBuiltinName(const Expr &expr, const char *name);
bool isExplicitKeyValueHelperFallbackPath(const Expr &expr);
bool isExplicitKeyValueReceiverProbeHelperExpr(const Expr &expr);
bool isExplicitVectorAccessHelperPath(const std::string &path);
bool isExplicitVectorAccessHelperExpr(const Expr &expr);
bool isExplicitVectorReceiverProbeHelperExpr(const Expr &expr);
bool blocksExplicitVectorReceiverProbeKindFallbackExpr(const Expr &expr);

bool isAllowedResolvedMapDirectCallPath(const std::string &callPath, const std::string &resolvedPath);
bool isAllowedResolvedVectorDirectCallPath(const std::string &callPath, const std::string &resolvedPath);
bool resolvePublishedStdlibSurfaceMemberToken(std::string_view memberToken,
                                              StdlibSurfaceId surfaceId,
                                              std::string &memberNameOut);
bool resolvePublishedStdlibSurfaceExprMemberName(const Expr &expr,
                                                 StdlibSurfaceId surfaceId,
                                                 std::string &memberNameOut);
bool resolvePublishedStdlibSurfaceConstructorMemberName(std::string_view path,
                                                        StdlibSurfaceId surfaceId,
                                                        std::string &memberNameOut);
bool resolvePublishedStdlibSurfaceConstructorExprMemberName(const Expr &expr,
                                                            StdlibSurfaceId surfaceId,
                                                            std::string &memberNameOut);
bool isResolvedCanonicalPublishedStdlibSurfaceConstructorPath(std::string_view path,
                                                              StdlibSurfaceId surfaceId);
bool isPublishedStdlibSurfaceConstructorExpr(const Expr &expr,
                                             StdlibSurfaceId surfaceId);
std::string inferPublishedKeyValueStorageStructPathFromConstructorPath(std::string_view path);
bool resolvePublishedSemanticStdlibSurfaceMemberName(const SemanticProgram *semanticProgram,
                                                     const Expr &expr,
                                                     StdlibSurfaceId surfaceId,
                                                     std::string &memberNameOut);
bool resolvePublishedStdlibSurfaceMemberName(std::string_view path,
                                             StdlibSurfaceId surfaceId,
                                             std::string &memberNameOut);
bool isPublishedStdlibSurfaceLoweringPath(std::string_view path,
                                          StdlibSurfaceId surfaceId);
bool isCanonicalPublishedStdlibSurfaceHelperPath(std::string_view path,
                                                 StdlibSurfaceId surfaceId);

std::string normalizeMapImportAliasPath(const std::string &path);
std::vector<std::string> collectionHelperPathCandidates(const std::string &path);

inline std::string vectorBackingTypePath(bool leadingSlash = true) {
  constexpr const char kVec[] = "vector";
  return experimentalCollectionTypePath(kVec, "Vector", leadingSlash);
}
inline std::string vectorBackingMemberRoot(bool leadingSlash = true) {
  constexpr const char kVec[] = "vector";
  return experimentalCollectionMemberRoot(kVec, leadingSlash);
}
// A resolved struct record path of the collection vector: the backing type path or one of
// its generated `__` specializations. Bare or `<...>` type text does not match, so a user
// struct that happens to be named `Vector` is not taken for the builtin.
inline bool isCollectionVectorRecordPath(std::string_view structPath) {
  return collection_paths::isTypePathOrSpecialization(structPath, vectorBackingTypePath());
}
// The same, and also type text spelled bare or with template arguments, or without the leading
// slash: for callers that may be looking at an unresolved type name.
inline bool isCollectionVectorTypeText(std::string_view typeText) {
  return isCollectionVectorRecordPath(typeText) ||
         isExperimentalCollectionTypeName(typeText, "vector", "Vector");
}
inline std::string vectorBuiltinStructNormalizedPath() {
  constexpr const char kVec[] = "vector";
  return normalizeBuiltinCollectionStructPath(kVec);
}
inline std::string mapBackingTypePath(bool leadingSlash = true) {
  constexpr const char kMap[] = "map";
  return experimentalCollectionTypePath(kMap, "Map", leadingSlash);
}

} // namespace primec::ir_lowerer
