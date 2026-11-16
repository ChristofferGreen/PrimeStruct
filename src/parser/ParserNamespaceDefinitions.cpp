#include "ParserNamespaceDefinitions.h"

#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace primec::parser {

bool isNamespaceDefinition(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name == "namespace") {
      return true;
    }
  }
  return false;
}

std::string namespaceDefinitionShapeError(const Definition &def) {
  if (!isNamespaceDefinition(def)) {
    return {};
  }
  for (const auto &transform : def.transforms) {
    if (transform.name == "return") {
      return "namespace definition cannot have a return transform: " + def.fullPath;
    }
  }
  if (!def.templateArgs.empty()) {
    return "namespace definition cannot have a template list: " + def.fullPath;
  }
  if (!def.parameters.empty()) {
    return "namespace definition cannot have parameters: " + def.fullPath;
  }
  if (!def.statements.empty()) {
    return "namespace definition body can only hold definitions: " + def.fullPath;
  }
  return {};
}

bool foldNamespaceDefinitions(std::vector<Definition> &definitions, NamespaceDefinitionError &error) {
  std::unordered_set<std::string> otherPaths;
  bool hasNamespace = false;
  for (const auto &def : definitions) {
    if (isNamespaceDefinition(def)) {
      hasNamespace = true;
    } else {
      otherPaths.insert(def.fullPath);
    }
  }
  if (!hasNamespace) {
    return true;
  }
  // Members of a namespace are plain definitions of the path, never helpers of an enclosing type.
  std::unordered_set<std::string> namespacePaths;
  for (const auto &def : definitions) {
    if (isNamespaceDefinition(def)) {
      namespacePaths.insert(def.fullPath);
    }
  }
  for (auto &def : definitions) {
    if (!def.isNested) {
      continue;
    }
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && namespacePaths.count(def.fullPath.substr(0, slash)) != 0) {
      def.isNested = false;
    }
  }
  std::unordered_map<std::string, size_t> firstByPath;
  std::vector<Definition> out;
  out.reserve(definitions.size());
  for (auto &def : definitions) {
    if (!isNamespaceDefinition(def)) {
      out.push_back(std::move(def));
      continue;
    }
    if (otherPaths.count(def.fullPath) != 0) {
      if (def.transforms.size() != 1) {
        error = {"namespace definition shares its path with another definition, so its transform list must be "
                 "'namespace' alone: " + def.fullPath,
                 def.sourceLine, def.sourceColumn};
        return false;
      }
      continue;
    }
    auto found = firstByPath.find(def.fullPath);
    if (found == firstByPath.end()) {
      firstByPath.emplace(def.fullPath, out.size());
      out.push_back(std::move(def));
      continue;
    }
    Definition &first = out[found->second];
    for (auto &transform : def.transforms) {
      if (transform.name == "namespace") {
        continue;
      }
      first.transforms.push_back(std::move(transform));
    }
  }
  definitions = std::move(out);
  return true;
}

} // namespace primec::parser
