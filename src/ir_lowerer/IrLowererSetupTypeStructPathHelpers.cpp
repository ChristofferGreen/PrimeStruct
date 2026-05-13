#include "IrLowererSetupTypeHelpers.h"

#include "IrLowererSetupTypeCollectionHelpers.h"

namespace primec::ir_lowerer {

namespace {

std::string resolveStructPathCandidates(const std::string &path,
                                        const std::unordered_set<std::string> &structNames) {
  const std::string normalizedPath = normalizeMapImportAliasPath(path);
  auto candidates = collectionHelperPathCandidates(normalizedPath);
  for (const auto &candidate : candidates) {
    if (structNames.count(candidate) > 0) {
      return candidate;
    }
  }
  return "";
}

} // namespace

std::string resolveMethodReceiverStructTypePathFromCallExpr(
    const Expr &receiverCallExpr,
    const std::string &resolvedReceiverPath,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames) {
  if (receiverCallExpr.isBinding || receiverCallExpr.isMethodCall) {
    return "";
  }

  std::string resolved = normalizeMapImportAliasPath(resolvedReceiverPath);
  auto importIt = importAliases.find(receiverCallExpr.name);
  std::string matched = resolveStructPathCandidates(resolved, structNames);
  if (!matched.empty()) {
    return matched;
  }
  if (importIt != importAliases.end()) {
    matched = resolveStructPathCandidates(importIt->second, structNames);
    if (!matched.empty()) {
      return matched;
    }
  }
  return "";
}

} // namespace primec::ir_lowerer
