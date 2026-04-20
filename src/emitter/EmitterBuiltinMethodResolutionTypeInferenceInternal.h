#pragma once

#include "EmitterHelpers.h"
#include "primec/StdlibSurfaceRegistry.h"

namespace primec::emitter {

struct MethodResolutionMetadataView {
  const std::unordered_map<std::string, const Definition *> &defMap;
  const std::unordered_map<std::string, std::string> &importAliases;
  const std::unordered_map<std::string, std::string> &structTypeMap;
  const std::unordered_map<std::string, ReturnKind> &returnKinds;
  const std::unordered_map<std::string, std::string> &returnStructs;
};

bool resolvePublishedCollectionSurfaceMemberToken(std::string_view memberToken,
                                                  StdlibSurfaceId surfaceId,
                                                  std::string &memberNameOut);
bool resolvePublishedCollectionSurfaceExprMemberName(const Expr &expr,
                                                     StdlibSurfaceId surfaceId,
                                                     std::string &memberNameOut);
bool isRemovedCollectionMethodAliasPath(std::string_view rawMethodName);
bool removedCollectionAliasNeedsDefinitionPath(std::string_view rawMethodName);
void appendUniqueCandidate(std::vector<std::string> &candidates, const std::string &candidate);
std::string typeNameFromResolvedCandidates(const MethodResolutionMetadataView &view,
                                           const std::vector<std::string> &resolvedCandidates);
bool extractCollectionElementTypeFromReturnType(const std::string &typeName, std::string &typeOut);
std::string normalizeCollectionReceiverType(const std::string &typePath);
std::vector<std::string> collectionHelperPathCandidates(const std::string &path);
void pruneMapAccessStructReturnCompatibilityCandidates(
    const std::string &path,
    std::vector<std::string> &candidates);
std::string normalizeMapImportAliasPath(const std::string &path);
const std::string *findStructTypeMetadata(const MethodResolutionMetadataView &view,
                                          const std::string &path);
const std::string *findReturnStructMetadata(const MethodResolutionMetadataView &view,
                                            const std::string &path);
const ReturnKind *findReturnKindMetadata(const MethodResolutionMetadataView &view,
                                         const std::string &path);
std::string preferCanonicalMapMethodHelperPath(const MethodResolutionMetadataView &view,
                                               const std::string &path);
bool hasDefinitionOrMetadata(const MethodResolutionMetadataView &view, const std::string &path);
std::string inferMethodResolutionPrimitiveTypeName(
    const Expr &expr,
    const MethodResolutionMetadataView &view,
    const std::unordered_map<std::string, BindingInfo> &localTypes);

} // namespace primec::emitter
