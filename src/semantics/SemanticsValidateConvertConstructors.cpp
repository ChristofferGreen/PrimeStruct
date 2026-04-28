#include "SemanticsValidateConvertConstructors.h"

#include "SemanticsHelpers.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace primec::semantics {

bool rewriteConvertConstructors(Program &program, std::string &error) {
  std::unordered_set<std::string> structNames;
  std::unordered_set<std::string> publicDefinitions;
  auto isDefinitionPublic = [](const Definition &def) -> bool {
    bool sawVisibility = false;
    bool isPublic = false;
    for (const auto &transform : def.transforms) {
      if (transform.name != "public" && transform.name != "private") {
        continue;
      }
      if (sawVisibility) {
        return false;
      }
      sawVisibility = true;
      isPublic = (transform.name == "public");
    }
    return isPublic;
  };
  for (const auto &def : program.definitions) {
    bool hasStructTransform = false;
    bool hasReturnTransform = false;
    bool hasSumTransform = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "sum") {
        hasSumTransform = true;
      }
      if (isStructTransformName(transform.name)) {
        hasStructTransform = true;
      }
      if (transform.name == "return") {
        hasReturnTransform = true;
      }
    }
    bool fieldOnlyStruct = false;
    if (!hasSumTransform && !hasStructTransform && !hasReturnTransform && def.parameters.empty() &&
        !def.hasReturnStatement && !def.returnExpr.has_value()) {
      fieldOnlyStruct = true;
      for (const auto &stmt : def.statements) {
        if (!stmt.isBinding) {
          fieldOnlyStruct = false;
          break;
        }
      }
    }
    if (hasStructTransform || fieldOnlyStruct) {
      structNames.insert(def.fullPath);
    }
    if (isDefinitionPublic(def)) {
      publicDefinitions.insert(def.fullPath);
    }
  }

  std::unordered_map<std::string, std::string> importAliases;
  for (const auto &importPath : program.imports) {
    if (importPath.empty() || importPath[0] != '/') {
      continue;
    }
    bool isWildcard = false;
    std::string prefix;
    if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
      isWildcard = true;
      prefix = importPath.substr(0, importPath.size() - 2);
    } else if (importPath.find('/', 1) == std::string::npos) {
      isWildcard = true;
      prefix = importPath;
    }
    if (isWildcard) {
      const std::string scopedPrefix = prefix + "/";
      for (const auto &def : program.definitions) {
        if (def.fullPath.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        const std::string remainder = def.fullPath.substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string::npos) {
          continue;
        }
        if (publicDefinitions.count(def.fullPath) == 0) {
          continue;
        }
        importAliases.emplace(remainder, def.fullPath);
      }
      continue;
    }
    const std::string remainder = importPath.substr(importPath.find_last_of('/') + 1);
    if (remainder.empty()) {
      continue;
    }
    if (publicDefinitions.count(importPath) == 0) {
      continue;
    }
    importAliases.emplace(remainder, importPath);
  }

  struct ResolvedStructTarget {
    std::string path;
    bool fromImport = false;
  };
  auto resolveStructTarget = [&](const std::string &typeName,
                                 const std::string &namespacePrefix) -> ResolvedStructTarget {
    if (typeName == "Self") {
      return ResolvedStructTarget{structNames.count(namespacePrefix) > 0 ? namespacePrefix : std::string{}, false};
    }
    if (typeName.find('<') != std::string::npos) {
      return {};
    }
    std::string resolved = resolveStructTypePath(typeName, namespacePrefix, structNames);
    if (!resolved.empty()) {
      return ResolvedStructTarget{std::move(resolved), false};
    }
    auto importIt = importAliases.find(typeName);
    if (importIt != importAliases.end() && structNames.count(importIt->second) > 0) {
      return ResolvedStructTarget{importIt->second, true};
    }
    return {};
  };

  auto resolveReturnStructTarget =
      [&](const std::string &typeName, const std::string &namespacePrefix) -> std::string {
    if (typeName == "Self") {
      return structNames.count(namespacePrefix) > 0 ? namespacePrefix : std::string{};
    }
    if (typeName.find('<') != std::string::npos) {
      return {};
    }
    std::string resolved = resolveStructTypePath(typeName, namespacePrefix, structNames);
    if (!resolved.empty()) {
      return resolved;
    }
    auto importIt = importAliases.find(typeName);
    if (importIt != importAliases.end() && structNames.count(importIt->second) > 0) {
      return importIt->second;
    }
    return {};
  };

  auto isConvertHelperPath = [](const std::string &helperBase, const std::string &path) -> bool {
    if (path == helperBase) {
      return true;
    }
    const std::string mangledPrefix = helperBase + "__t";
    return path.rfind(mangledPrefix, 0) == 0;
  };

  auto convertHelperMatches = [&](const Definition &def, const std::string &targetStruct) -> bool {
    if (def.parameters.size() != 1) {
      return false;
    }
    bool sawStatic = false;
    bool sawReturn = false;
    std::string returnType;
    for (const auto &transform : def.transforms) {
      if (transform.name == "static") {
        sawStatic = true;
      }
      if (transform.name != "return") {
        continue;
      }
      sawReturn = true;
      if (transform.templateArgs.size() != 1) {
        return false;
      }
      returnType = transform.templateArgs.front();
      break;
    }
    if (!sawStatic) {
      return false;
    }
    if (!sawReturn || returnType.empty() || returnType == "auto") {
      return false;
    }
    std::string resolvedReturn = resolveReturnStructTarget(returnType, def.namespacePrefix);
    return !resolvedReturn.empty() && resolvedReturn == targetStruct;
  };

  auto collectConvertHelpers = [&](const std::string &structPath,
                                   bool requirePublic) -> std::vector<std::string> {
    std::vector<std::string> matches;
    const std::string helperBase = structPath + "/Convert";
    for (const auto &def : program.definitions) {
      if (!isConvertHelperPath(helperBase, def.fullPath)) {
        continue;
      }
      if (requirePublic && publicDefinitions.count(def.fullPath) == 0) {
        continue;
      }
      if (!convertHelperMatches(def, structPath)) {
        continue;
      }
      matches.push_back(def.fullPath);
    }
    std::sort(matches.begin(), matches.end());
    return matches;
  };

  auto rewriteExpr = [&](auto &self, Expr &expr) -> bool {
    for (auto &arg : expr.args) {
      if (!self(self, arg)) {
        return false;
      }
    }
    for (auto &arg : expr.bodyArguments) {
      if (!self(self, arg)) {
        return false;
      }
    }
    std::string builtinName;
    if (expr.kind != Expr::Kind::Call || expr.isMethodCall || !getBuiltinConvertName(expr, builtinName) ||
        expr.templateArgs.size() != 1) {
      return true;
    }
    const std::string targetName = expr.templateArgs.front();
    const std::string normalizedTarget = normalizeBindingTypeName(targetName);
    if (normalizedTarget == "i32" || normalizedTarget == "i64" || normalizedTarget == "u64" ||
        normalizedTarget == "bool" || normalizedTarget == "f32" || normalizedTarget == "f64") {
      return true;
    }
    if (isSoftwareNumericTypeName(normalizedTarget)) {
      return true;
    }
    ResolvedStructTarget target = resolveStructTarget(targetName, expr.namespacePrefix);
    if (target.path.empty()) {
      return true;
    }
    std::vector<std::string> helpers = collectConvertHelpers(target.path, target.fromImport);
    if (helpers.empty()) {
      error = "no conversion found for convert<" + targetName + ">";
      return false;
    }
    if (helpers.size() > 1) {
      std::ostringstream message;
      message << "ambiguous conversion for convert<" << targetName << ">: ";
      for (size_t i = 0; i < helpers.size(); ++i) {
        if (i > 0) {
          message << ", ";
        }
        message << helpers[i];
      }
      error = message.str();
      return false;
    }
    expr.name = helpers.front();
    expr.templateArgs.clear();
    return true;
  };

  for (auto &def : program.definitions) {
    for (auto &param : def.parameters) {
      if (!rewriteExpr(rewriteExpr, param)) {
        return false;
      }
    }
    for (auto &stmt : def.statements) {
      if (!rewriteExpr(rewriteExpr, stmt)) {
        return false;
      }
    }
    if (def.returnExpr.has_value()) {
      if (!rewriteExpr(rewriteExpr, *def.returnExpr)) {
        return false;
      }
    }
  }
  for (auto &exec : program.executions) {
    for (auto &arg : exec.arguments) {
      if (!rewriteExpr(rewriteExpr, arg)) {
        return false;
      }
    }
    for (auto &arg : exec.bodyArguments) {
      if (!rewriteExpr(rewriteExpr, arg)) {
        return false;
      }
    }
  }
  return true;
}

} // namespace primec::semantics
