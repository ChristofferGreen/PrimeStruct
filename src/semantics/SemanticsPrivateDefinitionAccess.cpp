#include "SemanticsPrivateDefinitionAccess.h"

#include "SemanticsHelpers.h"
#include "SemanticsPrivateFieldAccess.h"

namespace primec::semantics {

namespace {

bool hasTransform(const Definition &def, const char *name) {
  for (const auto &transform : def.transforms) {
    if (transform.name == name) {
      return true;
    }
  }
  return false;
}

std::string parentPath(const std::string &path) {
  const size_t slash = path.find_last_of('/');
  if (slash == std::string::npos || slash == 0) {
    return {};
  }
  return path.substr(0, slash);
}

// A monomorphized instance segment ends in `__t<hex>`; the diagnostic names the template instead.
std::string withoutSpecializationSuffix(const std::string &segment) {
  const size_t suffix = segment.rfind("__t");
  if (suffix == std::string::npos || suffix + 3 >= segment.size()) {
    return segment;
  }
  for (size_t i = suffix + 3; i < segment.size(); ++i) {
    const char c = segment[i];
    const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    if (!hex) {
      return segment;
    }
  }
  return segment.substr(0, suffix);
}

std::string displayPath(const std::string &path) {
  std::string display;
  size_t start = 0;
  while (start < path.size()) {
    size_t end = path.find('/', start + 1);
    if (end == std::string::npos) {
      end = path.size();
    }
    display += withoutSpecializationSuffix(path.substr(start, end - start));
    start = end;
  }
  return display;
}

// The definition a nested definition sits in: a struct for a method, a function for a definition in its body.
// Empty for a member of a namespace.
const Definition *
enclosingDefinition(const std::unordered_map<std::string, const Definition *> &defMap,
                    const std::string &definitionPath) {
  const auto ownerIt = defMap.find(parentPath(definitionPath));
  if (ownerIt == defMap.end() || ownerIt->second == nullptr ||
      hasTransform(*ownerIt->second, "namespace")) {
    return nullptr;
  }
  return ownerIt->second;
}

} // namespace

bool isPrivateStdDefinition(const std::unordered_map<std::string, const Definition *> &defMap,
                            const Definition &def) {
  if (def.fullPath.rfind("/std/", 0) != 0 || hasTransform(def, "public")) {
    return false;
  }
  if (hasTransform(def, "private")) {
    return true;
  }
  return enclosingDefinition(defMap, def.fullPath) == nullptr;
}

std::string definitionNamespace(const std::unordered_map<std::string, const Definition *> &defMap,
                                const std::string &definitionPath) {
  std::string path = definitionPath;
  while (const Definition *owner = enclosingDefinition(defMap, path)) {
    path = owner->fullPath;
  }
  return parentPath(path);
}

std::string
privateDefinitionCallViolation(const std::unordered_map<std::string, const Definition *> &defMap,
                               const Definition &callee,
                               const std::string &accessPath) {
  // The rule is about calls: the brace construction of a type is not one.
  for (const auto &transform : callee.transforms) {
    if (isStructTransformName(transform.name) || transform.name == "sum") {
      return {};
    }
  }
  if (!isPrivateStdDefinition(defMap, callee)) {
    return {};
  }
  const std::string calleeNamespace = definitionNamespace(defMap, callee.fullPath);
  if (calleeNamespace.empty()) {
    return {};
  }
  // The field rule takes the path of a struct and allows its namespace: hand it a member of the namespace.
  if (isPrivateFieldAccessible(calleeNamespace + "/_", accessPath)) {
    return {};
  }
  return privateDefinitionDiagnostic(callee.fullPath, calleeNamespace);
}

std::string privateDefinitionDiagnostic(const std::string &definitionPath,
                                        const std::string &definitionNamespace) {
  return displayPath(definitionPath) + " is private: only definitions in " + definitionNamespace +
         " may call it";
}

} // namespace primec::semantics
