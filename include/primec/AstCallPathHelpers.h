#pragma once

#include <string>
#include <string_view>

#include "primec/Ast.h"

namespace primec {

inline std::string normalizedCallName(const Expr &expr) {
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized;
}

inline bool isCanonicalCollectionHelperCall(const Expr &expr,
                                            std::string_view collectionPath,
                                            std::string_view helperName,
                                            size_t argumentCount) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty() || expr.args.size() != argumentCount) {
    return false;
  }

  const std::string normalized = normalizedCallName(expr);
  const std::string fullPath = std::string(collectionPath) + "/" + std::string(helperName);
  if (normalized.rfind(fullPath, 0) == 0) {
    return true;
  }

  if (normalized != helperName) {
    return false;
  }

  const std::string collectionPathWithSlash = "/" + std::string(collectionPath);
  return expr.namespacePrefix == collectionPathWithSlash || expr.namespacePrefix == collectionPath;
}

} // namespace primec
