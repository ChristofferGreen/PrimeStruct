#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "primec/Ast.h"
#include "primec/StdlibSurfaceRegistry.h"

namespace primec::ir_lowerer {

bool allowsArrayVectorCompatibilitySuffix(const std::string &suffix);

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

bool isRemovedVectorCompatibilityHelper(const std::string &helperName);
bool resolveVectorHelperAliasName(const Expr &expr, std::string &helperNameOut);
bool resolveMapHelperAliasName(const Expr &expr, std::string &helperNameOut);
bool resolveBorrowedMapHelperAliasName(const Expr &expr, std::string &helperNameOut);

std::string normalizeCollectionHelperPath(const std::string &path);
bool isExplicitRemovedVectorMethodAliasPath(const std::string &methodName);
bool isExplicitMapMethodAliasPath(const std::string &methodName);
bool isExplicitMapContainsOrTryAtMethodPath(const std::string &methodName);
bool isVectorBuiltinName(const Expr &expr, const char *name);
bool isMapBuiltinName(const Expr &expr, const char *name);
bool isExplicitMapHelperFallbackPath(const Expr &expr);
bool isExplicitMapReceiverProbeHelperExpr(const Expr &expr);
bool isExplicitVectorAccessHelperPath(const std::string &path);
bool isExplicitVectorAccessHelperExpr(const Expr &expr);
bool isExplicitVectorReceiverProbeHelperExpr(const Expr &expr);

bool isAllowedResolvedMapDirectCallPath(const std::string &callPath, const std::string &resolvedPath);
bool isAllowedResolvedVectorDirectCallPath(const std::string &callPath, const std::string &resolvedPath);
bool resolvePublishedStdlibSurfaceMemberName(std::string_view path,
                                             StdlibSurfaceId surfaceId,
                                             std::string &memberNameOut);
bool isCanonicalPublishedStdlibSurfaceHelperPath(std::string_view path,
                                                 StdlibSurfaceId surfaceId);

std::string normalizeMapImportAliasPath(const std::string &path);
std::vector<std::string> collectionHelperPathCandidates(const std::string &path);

} // namespace primec::ir_lowerer
