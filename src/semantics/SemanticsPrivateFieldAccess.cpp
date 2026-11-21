#include "SemanticsPrivateFieldAccess.h"

namespace primec::semantics {

bool isPrivateFieldBinding(const Expr &fieldBinding) {
  for (const auto &transform : fieldBinding.transforms) {
    if (transform.name == "private") {
      return true;
    }
  }
  return false;
}

bool isPrivateFieldAccessible(const std::string &structPath, const std::string &accessNamespace) {
  const size_t slash = structPath.find_last_of('/');
  if (slash == std::string::npos || slash == 0) {
    // A struct at the root has the whole program as its namespace.
    return true;
  }
  const std::string structNamespace = structPath.substr(0, slash);
  if (accessNamespace == structNamespace) {
    return true;
  }
  return accessNamespace.size() > structNamespace.size() &&
         accessNamespace.compare(0, structNamespace.size(), structNamespace) == 0 &&
         accessNamespace[structNamespace.size()] == '/';
}

std::string privateFieldViolation(const std::unordered_map<std::string, const Definition *> &defMap,
                                  const std::string &structPath,
                                  const std::string &fieldName,
                                  const std::string &accessNamespace) {
  if (isPrivateFieldAccessible(structPath, accessNamespace)) {
    return {};
  }
  auto defIt = defMap.find(structPath);
  if (defIt == defMap.end() || defIt->second == nullptr) {
    return {};
  }
  for (const Expr &stmt : defIt->second->statements) {
    if (stmt.isBinding && stmt.name == fieldName && isPrivateFieldBinding(stmt)) {
      return privateFieldDiagnostic(structPath, fieldName);
    }
  }
  return {};
}

std::string privateFieldDiagnostic(const std::string &structPath, const std::string &fieldName) {
  return "private field is not accessible: " + structPath + "/" + fieldName;
}

} // namespace primec::semantics
