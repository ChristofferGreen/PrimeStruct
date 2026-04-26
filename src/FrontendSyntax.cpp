#include "primec/FrontendSyntax.h"

namespace primec {

bool isSyntaxWildcardImportPath(const std::string &path, std::string &prefixOut) {
  if (path.size() >= 2 && path.compare(path.size() - 2, 2, "/*") == 0) {
    prefixOut = path.substr(0, path.size() - 2);
    return true;
  }
  if (path.find('/', 1) == std::string::npos) {
    prefixOut = path;
    return true;
  }
  return false;
}

std::string normalizeSyntaxImportAliasTargetPath(const std::string &path) {
  if (path.empty() || path.front() == '/') {
    return path;
  }
  if (path.rfind("map/", 0) == 0 || path.rfind("std/collections/map/", 0) == 0) {
    return "/" + path;
  }
  return path;
}

std::unordered_map<std::string, std::string> buildSyntaxImportAliases(
    const std::vector<std::string> &imports,
    const std::vector<Definition> &definitions,
    const std::unordered_map<std::string, const Definition *> &definitionByPath) {
  std::unordered_map<std::string, std::string> importAliases;
  for (const auto &importPath : imports) {
    if (importPath.empty() || importPath[0] != '/') {
      continue;
    }
    std::string wildcardPrefix;
    if (isSyntaxWildcardImportPath(importPath, wildcardPrefix) && wildcardPrefix != importPath) {
      const std::string scopedPrefix = wildcardPrefix + "/";
      for (const auto &def : definitions) {
        if (def.fullPath.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        const std::string remainder = def.fullPath.substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string::npos) {
          continue;
        }
        importAliases.emplace(remainder, def.fullPath);
      }
      continue;
    }
    auto defIt = definitionByPath.find(importPath);
    if (defIt == definitionByPath.end()) {
      continue;
    }
    const std::string remainder = importPath.substr(importPath.find_last_of('/') + 1);
    if (remainder.empty()) {
      continue;
    }
    importAliases.emplace(remainder, importPath);
  }
  return importAliases;
}

} // namespace primec
