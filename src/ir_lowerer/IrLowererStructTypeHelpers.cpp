#include "IrLowererStructTypeHelpers.h"

namespace primec::ir_lowerer {

std::string joinTemplateArgsText(const std::vector<std::string> &args) {
  std::string out;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      out += ", ";
    }
    out += args[i];
  }
  return out;
}

bool resolveStructTypePathFromScope(
    const std::string &typeName,
    const std::string &namespacePrefix,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases,
    std::string &resolvedOut) {
  if (typeName.empty()) {
    return false;
  }
  if (typeName[0] == '/') {
    if (structNames.count(typeName) > 0) {
      resolvedOut = typeName;
      return true;
    }
    return false;
  }
  if (!namespacePrefix.empty()) {
    if (namespacePrefix.size() > typeName.size() &&
        namespacePrefix.compare(namespacePrefix.size() - typeName.size() - 1,
                                typeName.size() + 1,
                                "/" + typeName) == 0) {
      if (structNames.count(namespacePrefix) > 0) {
        resolvedOut = namespacePrefix;
        return true;
      }
    }
    std::string candidate = namespacePrefix + "/" + typeName;
    if (structNames.count(candidate) > 0) {
      resolvedOut = candidate;
      return true;
    }
  }
  auto importIt = importAliases.find(typeName);
  if (importIt != importAliases.end() && structNames.count(importIt->second) > 0) {
    resolvedOut = importIt->second;
    return true;
  }
  std::string root = "/" + typeName;
  if (structNames.count(root) > 0) {
    resolvedOut = root;
    return true;
  }
  return false;
}

} // namespace primec::ir_lowerer
