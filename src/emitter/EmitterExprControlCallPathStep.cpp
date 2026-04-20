#include "EmitterExprControlCallPathStep.h"
#include "EmitterBuiltinMethodResolutionTypeInferenceInternal.h"

namespace primec::emitter {

std::optional<std::string> runEmitterExprControlCallPathStep(
    const Expr &expr,
    const std::string &resolvedPath,
    const std::unordered_map<std::string, std::string> &nameMap,
    const std::unordered_map<std::string, std::string> &importAliases) {
  if (expr.isMethodCall || expr.name.empty() || expr.name[0] == '/' || expr.name.find('/') != std::string::npos) {
    return std::nullopt;
  }
  if (nameMap.count(resolvedPath) != 0) {
    return std::nullopt;
  }
  if (!expr.namespacePrefix.empty()) {
    const auto importIt = importAliases.find(expr.name);
    if (importIt != importAliases.end()) {
      return normalizeMapImportAliasPath(importIt->second);
    }
    return std::nullopt;
  }
  const std::string root = "/" + expr.name;
  if (nameMap.count(root) != 0) {
    return root;
  }
  const auto importIt = importAliases.find(expr.name);
  if (importIt != importAliases.end()) {
    return normalizeMapImportAliasPath(importIt->second);
  }
  return std::nullopt;
}

} // namespace primec::emitter
