#include "primec/Semantics.h"

#include "SemanticsValidateTransforms.h"
#include "SemanticsValidator.h"
#include "primec/StringLiteral.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace primec {

namespace semantics {
bool monomorphizeTemplates(Program &program, const std::string &entryPath, std::string &error);
}

namespace {
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
    for (const auto &transform : def.transforms) {
      if (semantics::isStructTransformName(transform.name)) {
        hasStructTransform = true;
      }
      if (transform.name == "return") {
        hasReturnTransform = true;
      }
    }
    bool fieldOnlyStruct = false;
    if (!hasStructTransform && !hasReturnTransform && def.parameters.empty() && !def.hasReturnStatement &&
        !def.returnExpr.has_value()) {
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
    std::string resolved = semantics::resolveStructTypePath(typeName, namespacePrefix, structNames);
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
    std::string resolved = semantics::resolveStructTypePath(typeName, namespacePrefix, structNames);
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
    if (expr.kind != Expr::Kind::Call || expr.isMethodCall ||
        !semantics::getBuiltinConvertName(expr, builtinName) ||
        expr.templateArgs.size() != 1) {
      return true;
    }
    const std::string targetName = expr.templateArgs.front();
    const std::string normalizedTarget = semantics::normalizeBindingTypeName(targetName);
    if (normalizedTarget == "i32" || normalizedTarget == "i64" || normalizedTarget == "u64" ||
        normalizedTarget == "bool" || normalizedTarget == "f32" || normalizedTarget == "f64") {
      return true;
    }
    if (semantics::isSoftwareNumericTypeName(normalizedTarget)) {
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

bool rewriteMaybeConstructors(Program &program, std::string &error) {
  (void)error;
  std::unordered_set<std::string> structNames;
  structNames.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    bool hasStructTransform = false;
    bool hasReturnTransform = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "return") {
        hasReturnTransform = true;
      }
      if (semantics::isStructTransformName(transform.name)) {
        hasStructTransform = true;
      }
    }
    bool fieldOnlyStruct = false;
    if (!hasStructTransform && !hasReturnTransform && def.parameters.empty() && !def.hasReturnStatement &&
        !def.returnExpr.has_value()) {
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
  }

  std::unordered_set<std::string> publicDefinitions;
  publicDefinitions.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    bool sawPublic = false;
    bool sawPrivate = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "public") {
        sawPublic = true;
      } else if (transform.name == "private") {
        sawPrivate = true;
      }
    }
    if (sawPublic && !sawPrivate) {
      publicDefinitions.insert(def.fullPath);
    }
  }

  std::unordered_map<std::string, std::string> importAliases;
  auto isWildcardImport = [](const std::string &path, std::string &prefixOut) -> bool {
    if (path.size() >= 2 && path.compare(path.size() - 2, 2, "/*") == 0) {
      prefixOut = path.substr(0, path.size() - 2);
      return true;
    }
    if (path.find('/', 1) == std::string::npos) {
      prefixOut = path;
      return true;
    }
    return false;
  };
  for (const auto &importPath : program.imports) {
    std::string prefix;
    if (isWildcardImport(importPath, prefix)) {
      std::string scopedPrefix = prefix;
      if (!scopedPrefix.empty() && scopedPrefix.back() != '/') {
        scopedPrefix += "/";
      }
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
    if (expr.kind != Expr::Kind::Call || expr.isBinding || expr.isMethodCall) {
      return true;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return true;
    }
    if (expr.args.size() != 1) {
      return true;
    }
    std::string resolved = semantics::resolveStructTypePath(expr.name, expr.namespacePrefix, structNames);
    if (resolved.empty()) {
      auto it = importAliases.find(expr.name);
      if (it != importAliases.end() && structNames.count(it->second) > 0) {
        resolved = it->second;
      }
    }
    if (resolved.empty()) {
      return true;
    }
    const size_t lastSlash = resolved.find_last_of('/');
    const std::string_view base =
        lastSlash == std::string::npos ? std::string_view(resolved) : std::string_view(resolved).substr(lastSlash + 1);
    if (base != "Maybe") {
      return true;
    }
    Expr call;
    call.kind = Expr::Kind::Call;
    call.templateArgs = expr.templateArgs;
    call.args = std::move(expr.args);
    call.argNames = std::move(expr.argNames);
    call.transforms = std::move(expr.transforms);
    if (!resolved.empty() && resolved[0] == '/') {
      std::string prefix = resolved.substr(0, resolved.size() - 6);
      call.name = prefix.empty() ? "/some" : (prefix + "/some");
      call.namespacePrefix.clear();
    } else {
      call.name = "some";
      call.namespacePrefix = expr.namespacePrefix;
    }
    expr = std::move(call);
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

bool rewriteExperimentalGfxConstructors(Program &program, std::string &error) {
  (void)error;
  std::unordered_set<std::string> structNames;
  structNames.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    bool hasStructTransform = false;
    bool hasReturnTransform = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "return") {
        hasReturnTransform = true;
      }
      if (semantics::isStructTransformName(transform.name)) {
        hasStructTransform = true;
      }
    }
    bool fieldOnlyStruct = false;
    if (!hasStructTransform && !hasReturnTransform && def.parameters.empty() && !def.hasReturnStatement &&
        !def.returnExpr.has_value()) {
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
  }

  std::unordered_set<std::string> publicDefinitions;
  publicDefinitions.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    bool sawPublic = false;
    bool sawPrivate = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "public") {
        sawPublic = true;
      } else if (transform.name == "private") {
        sawPrivate = true;
      }
    }
    if (sawPublic && !sawPrivate) {
      publicDefinitions.insert(def.fullPath);
    }
  }

  std::unordered_map<std::string, std::string> importAliases;
  auto isWildcardImport = [](const std::string &path, std::string &prefixOut) -> bool {
    if (path.size() >= 2 && path.compare(path.size() - 2, 2, "/*") == 0) {
      prefixOut = path.substr(0, path.size() - 2);
      return true;
    }
    if (path.find('/', 1) == std::string::npos) {
      prefixOut = path;
      return true;
    }
    return false;
  };
  for (const auto &importPath : program.imports) {
    std::string prefix;
    if (isWildcardImport(importPath, prefix)) {
      std::string scopedPrefix = prefix;
      if (!scopedPrefix.empty() && scopedPrefix.back() != '/') {
        scopedPrefix += "/";
      }
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

  auto resolveImportedStructPath = [&](const std::string &name, const std::string &namespacePrefix) {
    std::string resolved = semantics::resolveStructTypePath(name, namespacePrefix, structNames);
    if (resolved.empty()) {
      auto it = importAliases.find(name);
      if (it != importAliases.end() && structNames.count(it->second) > 0) {
        resolved = it->second;
      }
    }
    return resolved;
  };

  using LocalBindings = std::unordered_map<std::string, semantics::BindingInfo>;

  auto resolveBindingStructPath = [&](const semantics::BindingInfo &binding, const std::string &namespacePrefix) {
    if (!binding.typeName.empty() && binding.typeName.front() == '/' && structNames.count(binding.typeName) > 0) {
      return binding.typeName;
    }
    return resolveImportedStructPath(binding.typeName, namespacePrefix);
  };

  auto rememberBinding = [&](const Expr &expr, const std::string &namespacePrefix, LocalBindings &locals) {
    if (!expr.isBinding) {
      return;
    }
    semantics::BindingInfo binding;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!semantics::parseBindingInfo(expr, namespacePrefix, structNames, importAliases, binding, restrictType, parseError)) {
      return;
    }
    locals[expr.name] = std::move(binding);
  };

  auto rewriteExpr = [&](auto &self, Expr &expr, const std::string &namespacePrefix,
                         const LocalBindings &locals) -> bool {
    for (auto &arg : expr.args) {
      if (!self(self, arg, namespacePrefix, locals)) {
        return false;
      }
    }
    LocalBindings bodyLocals = locals;
    for (auto &arg : expr.bodyArguments) {
      if (!self(self, arg, namespacePrefix, bodyLocals)) {
        return false;
      }
      rememberBinding(arg, namespacePrefix, bodyLocals);
    }
    if (expr.kind != Expr::Kind::Call || expr.isBinding) {
      return true;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return true;
    }
    if (!expr.templateArgs.empty()) {
      return true;
    }
    if (expr.isMethodCall) {
      if (expr.args.empty() || expr.name != "create_pipeline") {
        return true;
      }
      const Expr &receiverExpr = expr.args.front();
      std::string receiverPath;
      if (receiverExpr.kind == Expr::Kind::Name) {
        auto localIt = locals.find(receiverExpr.name);
        if (localIt != locals.end()) {
          receiverPath = resolveBindingStructPath(localIt->second, namespacePrefix);
        } else {
          receiverPath = resolveImportedStructPath(receiverExpr.name, receiverExpr.namespacePrefix);
        }
      }
      const bool isExperimentalDevice = receiverPath == "/std/gfx/experimental/Device";
      const bool isCanonicalDevice = receiverPath == "/std/gfx/Device";
      if (!isExperimentalDevice && !isCanonicalDevice) {
        return true;
      }
      size_t vertexTypeIndex = expr.args.size();
      for (size_t i = 0; i < expr.argNames.size() && i < expr.args.size(); ++i) {
        if (expr.argNames[i].has_value() && *expr.argNames[i] == "vertex_type") {
          vertexTypeIndex = i;
          break;
        }
      }
      if (vertexTypeIndex >= expr.args.size()) {
        return true;
      }
      const Expr &vertexTypeExpr = expr.args[vertexTypeIndex];
      if (vertexTypeExpr.kind != Expr::Kind::Name) {
        error = "experimental gfx create_pipeline requires bare struct type on [vertex_type]";
        return false;
      }
      const std::string vertexTypePath = resolveImportedStructPath(vertexTypeExpr.name, vertexTypeExpr.namespacePrefix);
      const bool isExperimentalVertexType = vertexTypePath == "/std/gfx/experimental/VertexColored";
      const bool isCanonicalVertexType = vertexTypePath == "/std/gfx/VertexColored";
      if (!isExperimentalVertexType && !isCanonicalVertexType) {
        error = "experimental gfx create_pipeline currently supports only VertexColored";
        return false;
      }
      expr.name = "create_pipeline_VertexColored";
      expr.args.erase(expr.args.begin() + static_cast<std::ptrdiff_t>(vertexTypeIndex));
      expr.argNames.erase(expr.argNames.begin() + static_cast<std::ptrdiff_t>(vertexTypeIndex));
      return true;
    }
    std::string resolved = resolveImportedStructPath(expr.name, expr.namespacePrefix);
    if (resolved.empty()) {
      return true;
    }
    if ((resolved == "/std/gfx/experimental/Window" || resolved == "/std/gfx/Window") && expr.args.size() == 3) {
      bool sawTitleArg = false;
      for (const auto &argName : expr.argNames) {
        if (argName.has_value() && *argName == "title") {
          sawTitleArg = true;
          break;
        }
      }
      if (sawTitleArg) {
        expr.name = resolved == "/std/gfx/Window" ? "/std/gfx/windowCreate" : "/std/gfx/experimental/windowCreate";
        expr.namespacePrefix.clear();
      }
      return true;
    }
    if ((resolved == "/std/gfx/experimental/Device" || resolved == "/std/gfx/Device") && expr.args.empty() &&
        !expr.hasBodyArguments &&
        expr.bodyArguments.empty() && !semantics::hasNamedArguments(expr.argNames)) {
      expr.name = resolved == "/std/gfx/Device" ? "/std/gfx/deviceCreate" : "/std/gfx/experimental/deviceCreate";
      expr.namespacePrefix.clear();
      return true;
    }
    return true;
  };

  for (auto &def : program.definitions) {
    LocalBindings locals;
    for (auto &param : def.parameters) {
      if (!rewriteExpr(rewriteExpr, param, def.namespacePrefix, locals)) {
        return false;
      }
      rememberBinding(param, def.namespacePrefix, locals);
    }
    for (auto &stmt : def.statements) {
      if (!rewriteExpr(rewriteExpr, stmt, def.namespacePrefix, locals)) {
        return false;
      }
      rememberBinding(stmt, def.namespacePrefix, locals);
    }
    if (def.returnExpr.has_value()) {
      if (!rewriteExpr(rewriteExpr, *def.returnExpr, def.namespacePrefix, locals)) {
        return false;
      }
    }
  }
  for (auto &exec : program.executions) {
    LocalBindings locals;
    for (auto &arg : exec.arguments) {
      if (!rewriteExpr(rewriteExpr, arg, exec.namespacePrefix, locals)) {
        return false;
      }
      rememberBinding(arg, exec.namespacePrefix, locals);
    }
    for (auto &arg : exec.bodyArguments) {
      if (!rewriteExpr(rewriteExpr, arg, exec.namespacePrefix, locals)) {
        return false;
      }
      rememberBinding(arg, exec.namespacePrefix, locals);
    }
  }
  return true;
}

bool rewriteReflectionMetadataQueries(Program &program, std::string &error) {
  struct FieldMetadata {
    std::string name;
    std::string typeName;
    std::string visibility;
  };

  std::unordered_set<std::string> structNames;
  std::unordered_set<std::string> reflectedStructNames;
  std::unordered_map<std::string, std::vector<FieldMetadata>> structFieldMetadata;
  std::unordered_map<std::string, const Definition *> definitionByPath;
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
    definitionByPath.emplace(def.fullPath, &def);
    bool hasStructTransform = false;
    bool hasReturnTransform = false;
    for (const auto &transform : def.transforms) {
      if (semantics::isStructTransformName(transform.name)) {
        hasStructTransform = true;
      }
      if (transform.name == "return") {
        hasReturnTransform = true;
      }
    }
    bool fieldOnlyStruct = false;
    if (!hasStructTransform && !hasReturnTransform && def.parameters.empty() && !def.hasReturnStatement &&
        !def.returnExpr.has_value()) {
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
      if (std::any_of(def.transforms.begin(), def.transforms.end(), [](const Transform &transform) {
            return transform.name == "reflect";
          })) {
        reflectedStructNames.insert(def.fullPath);
      }
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

  auto trim = [](const std::string &text) -> std::string {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
      return {};
    }
    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
  };
  auto escapeStringLiteral = [](const std::string &text) {
    std::string escaped;
    escaped.reserve(text.size() + 8);
    escaped.push_back('"');
    for (char c : text) {
      if (c == '"' || c == '\\') {
        escaped.push_back('\\');
      }
      escaped.push_back(c);
    }
    escaped += "\"utf8";
    return escaped;
  };

  std::function<bool(const std::string &, const std::string &, std::string &, std::string &)> canonicalizeTypeName;
  canonicalizeTypeName = [&](const std::string &typeName,
                             const std::string &namespacePrefix,
                             std::string &canonicalOut,
                             std::string &structPathOut) {
    canonicalOut.clear();
    structPathOut.clear();
    const std::string trimmed = trim(typeName);
    if (trimmed.empty()) {
      error = "reflection query requires a type argument";
      return false;
    }
    if (trimmed == "Self" && structNames.count(namespacePrefix) > 0) {
      canonicalOut = namespacePrefix;
      structPathOut = namespacePrefix;
      return true;
    }

    const std::string normalized = semantics::normalizeBindingTypeName(trimmed);
    std::string resolvedStruct = semantics::resolveStructTypePath(normalized, namespacePrefix, structNames);
    if (resolvedStruct.empty()) {
      auto importIt = importAliases.find(normalized);
      if (importIt != importAliases.end() && structNames.count(importIt->second) > 0) {
        resolvedStruct = importIt->second;
      }
    }
    if (!resolvedStruct.empty()) {
      canonicalOut = resolvedStruct;
      structPathOut = resolvedStruct;
      return true;
    }

    std::string base;
    std::string arg;
    if (!semantics::splitTemplateTypeName(normalized, base, arg)) {
      canonicalOut = normalized;
      return true;
    }

    std::vector<std::string> args;
    if (!semantics::splitTopLevelTemplateArgs(arg, args) || args.empty()) {
      canonicalOut = normalized;
      return true;
    }

    std::string canonicalBase;
    std::string baseStructPath;
    if (!canonicalizeTypeName(base, namespacePrefix, canonicalBase, baseStructPath)) {
      return false;
    }
    std::vector<std::string> canonicalArgs;
    canonicalArgs.reserve(args.size());
    for (const auto &nestedArg : args) {
      std::string canonicalArg;
      std::string nestedStructPath;
      if (!canonicalizeTypeName(nestedArg, namespacePrefix, canonicalArg, nestedStructPath)) {
        return false;
      }
      canonicalArgs.push_back(std::move(canonicalArg));
    }
    canonicalOut = canonicalBase + "<" + semantics::joinTemplateArgs(canonicalArgs) + ">";
    return true;
  };

  auto resolveFieldTypeName = [&](const Definition &def, const Expr &stmt) {
    std::string typeCandidate;
    bool hasExplicitType = false;
    for (const auto &transform : stmt.transforms) {
      if (transform.name == "effects" || transform.name == "capabilities") {
        continue;
      }
      if (transform.name == "restrict" && transform.templateArgs.size() == 1) {
        typeCandidate = transform.templateArgs.front();
        hasExplicitType = true;
        continue;
      }
      if (semantics::isBindingAuxTransformName(transform.name)) {
        continue;
      }
      if (!transform.arguments.empty()) {
        continue;
      }
      if (!transform.templateArgs.empty()) {
        typeCandidate = transform.name + "<" + semantics::joinTemplateArgs(transform.templateArgs) + ">";
      } else {
        typeCandidate = transform.name;
      }
      hasExplicitType = true;
      break;
    }

    if (!hasExplicitType && stmt.args.size() == 1) {
      semantics::BindingInfo inferred;
      if (semantics::tryInferBindingTypeFromInitializer(stmt.args.front(), {}, {}, inferred, true)) {
        typeCandidate = inferred.typeName;
        if (!inferred.typeTemplateArg.empty()) {
          typeCandidate += "<" + inferred.typeTemplateArg + ">";
        }
      }
    }
    if (typeCandidate.empty()) {
      typeCandidate = "int";
    }

    std::string canonicalType;
    std::string ignoredStructPath;
    if (canonicalizeTypeName(typeCandidate, def.namespacePrefix, canonicalType, ignoredStructPath)) {
      return canonicalType;
    }
    return semantics::normalizeBindingTypeName(typeCandidate);
  };

  for (const auto &def : program.definitions) {
    if (structNames.count(def.fullPath) == 0) {
      continue;
    }
    std::vector<FieldMetadata> fields;
    fields.reserve(def.statements.size());
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        continue;
      }
      FieldMetadata field;
      field.name = stmt.name;
      field.typeName = resolveFieldTypeName(def, stmt);
      field.visibility = "public";
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "private") {
          field.visibility = "private";
          break;
        }
        if (transform.name == "public") {
          field.visibility = "public";
          break;
        }
      }
      fields.push_back(std::move(field));
    }
    structFieldMetadata.emplace(def.fullPath, std::move(fields));
  }

  auto typeKindForCanonical = [](const std::string &canonicalType, const std::string &structPath) {
    if (!structPath.empty()) {
      return std::string("struct");
    }
    std::string base;
    std::string arg;
    if (semantics::splitTemplateTypeName(canonicalType, base, arg)) {
      const std::string normalizedBase = semantics::normalizeBindingTypeName(base);
      if (normalizedBase == "array" || normalizedBase == "vector" || normalizedBase == "map" ||
          normalizedBase == "soa_vector" || normalizedBase == "Result") {
        return normalizedBase;
      }
      if (normalizedBase == "Pointer") {
        return std::string("pointer");
      }
      if (normalizedBase == "Reference") {
        return std::string("reference");
      }
      if (normalizedBase == "Buffer") {
        return std::string("buffer");
      }
      if (normalizedBase == "uninitialized") {
        return std::string("uninitialized");
      }
      return std::string("template");
    }
    const std::string normalized = semantics::normalizeBindingTypeName(canonicalType);
    if (normalized == "i32" || normalized == "i64" || normalized == "u64" || normalized == "integer") {
      return std::string("integer");
    }
    if (normalized == "f32" || normalized == "f64") {
      return std::string("float");
    }
    if (normalized == "decimal") {
      return std::string("decimal");
    }
    if (normalized == "complex") {
      return std::string("complex");
    }
    if (normalized == "bool") {
      return std::string("bool");
    }
    if (normalized == "string") {
      return std::string("string");
    }
    return std::string("type");
  };
  auto parseFieldIndex = [&](const Expr &indexExpr, const std::string &queryName, size_t &indexOut) {
    if (indexExpr.kind != Expr::Kind::Literal) {
      error = "meta." + queryName + " requires constant integer index argument";
      return false;
    }
    if (indexExpr.isUnsigned) {
      indexOut = static_cast<size_t>(indexExpr.literalValue);
      return true;
    }
    if (indexExpr.intWidth == 64) {
      const int64_t value = static_cast<int64_t>(indexExpr.literalValue);
      if (value < 0) {
        error = "meta." + queryName + " requires non-negative field index";
        return false;
      }
      indexOut = static_cast<size_t>(value);
      return true;
    }
    const int32_t value = static_cast<int32_t>(static_cast<uint32_t>(indexExpr.literalValue));
    if (value < 0) {
      error = "meta." + queryName + " requires non-negative field index";
      return false;
    }
    indexOut = static_cast<size_t>(value);
    return true;
  };
  auto resolveDefinitionTarget = [&](const std::string &canonicalType,
                                     const std::string &structPath,
                                     const std::string &namespacePrefix) -> const Definition * {
    auto findByPath = [&](const std::string &path) -> const Definition * {
      auto it = definitionByPath.find(path);
      return it == definitionByPath.end() ? nullptr : it->second;
    };
    if (!structPath.empty()) {
      return findByPath(structPath);
    }
    if (canonicalType.empty() || canonicalType.find('<') != std::string::npos) {
      return nullptr;
    }
    if (canonicalType[0] == '/') {
      return findByPath(canonicalType);
    }
    if (!namespacePrefix.empty()) {
      std::string prefix = namespacePrefix;
      while (!prefix.empty()) {
        const std::string candidate = prefix + "/" + canonicalType;
        if (const Definition *resolved = findByPath(candidate)) {
          return resolved;
        }
        const size_t slash = prefix.find_last_of('/');
        if (slash == std::string::npos) {
          break;
        }
        prefix = prefix.substr(0, slash);
      }
    }
    auto importIt = importAliases.find(canonicalType);
    if (importIt != importAliases.end()) {
      if (const Definition *resolved = findByPath(importIt->second)) {
        return resolved;
      }
    }
    return findByPath("/" + canonicalType);
  };
  auto parseNamedQueryName = [&](const Expr &argExpr,
                                 const std::string &queryName,
                                 const std::string &nameLabel,
                                 std::string &nameOut) {
    if (argExpr.kind == Expr::Kind::Name) {
      nameOut = argExpr.name;
      return true;
    }
    if (argExpr.kind != Expr::Kind::StringLiteral) {
      error = "meta." + queryName + " requires constant string or identifier argument";
      return false;
    }
    ParsedStringLiteral parsed;
    std::string parseError;
    if (!parseStringLiteralToken(argExpr.stringValue, parsed, parseError)) {
      error = "meta." + queryName + " requires utf8/ascii string literal argument";
      return false;
    }
    nameOut = parsed.decoded;
    if (nameOut.empty()) {
      error = "meta." + queryName + " requires non-empty " + nameLabel + " name";
      return false;
    }
    return true;
  };
  auto typePathForCanonical = [&](const std::string &canonicalType, const std::string &structPath) {
    if (!structPath.empty()) {
      return structPath;
    }
    if (!canonicalType.empty() && canonicalType.front() == '/') {
      return canonicalType;
    }
    return std::string("/") + canonicalType;
  };
  auto isNumericTraitType = [&](const std::string &canonicalType) {
    if (canonicalType.empty() || canonicalType.find('<') != std::string::npos) {
      return false;
    }
    const std::string normalized = semantics::normalizeBindingTypeName(canonicalType);
    return normalized == "i32" || normalized == "i64" || normalized == "u64" || normalized == "f32" ||
           normalized == "f64" || normalized == "integer" || normalized == "decimal" ||
           normalized == "complex";
  };
  auto isComparableBuiltinTraitType = [&](const std::string &canonicalType) {
    if (canonicalType.empty() || canonicalType.find('<') != std::string::npos) {
      return false;
    }
    const std::string normalized = semantics::normalizeBindingTypeName(canonicalType);
    if (normalized == "complex") {
      return false;
    }
    if (isNumericTraitType(canonicalType)) {
      return true;
    }
    return normalized == "bool" || normalized == "string";
  };
  auto isIndexableBuiltinTraitType = [&](const std::string &canonicalType, const std::string &elemCanonicalType) {
    const std::string normalized = semantics::normalizeBindingTypeName(canonicalType);
    if (normalized == "string") {
      return elemCanonicalType == "i32";
    }
    std::string base;
    std::string arg;
    if (!semantics::splitTemplateTypeName(canonicalType, base, arg)) {
      return false;
    }
    const std::string normalizedBase = semantics::normalizeBindingTypeName(base);
    if (normalizedBase != "array" && normalizedBase != "vector") {
      return false;
    }
    std::vector<std::string> args;
    if (!semantics::splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
      return false;
    }
    return args.front() == elemCanonicalType;
  };
  auto resolveBindingCanonicalType = [&](const Definition &ownerDef,
                                         const Expr &bindingExpr,
                                         std::string &canonicalOut) -> bool {
    semantics::BindingInfo binding;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!semantics::parseBindingInfo(bindingExpr, ownerDef.namespacePrefix, structNames, importAliases, binding,
                                     restrictType, parseError)) {
      return false;
    }
    std::string typeName = binding.typeName;
    if (!binding.typeTemplateArg.empty()) {
      typeName += "<" + binding.typeTemplateArg + ">";
    }
    if (typeName.empty()) {
      return false;
    }
    std::string ignoredStructPath;
    return canonicalizeTypeName(typeName, ownerDef.namespacePrefix, canonicalOut, ignoredStructPath);
  };
  auto resolveDefinitionReturnCanonicalType = [&](const Definition &definition, std::string &canonicalOut) -> bool {
    for (const auto &transform : definition.transforms) {
      if (transform.name != "return") {
        continue;
      }
      if (transform.templateArgs.size() != 1 || transform.templateArgs.front() == "auto") {
        return false;
      }
      std::string ignoredStructPath;
      return canonicalizeTypeName(transform.templateArgs.front(), definition.namespacePrefix, canonicalOut,
                                  ignoredStructPath);
    }
    return false;
  };
  auto definitionMatchesSignature = [&](const std::string &functionPath,
                                        const std::vector<std::string> &expectedParamTypes,
                                        const std::string &expectedReturnType) -> bool {
    auto defIt = definitionByPath.find(functionPath);
    if (defIt == definitionByPath.end()) {
      return false;
    }
    const Definition &definition = *defIt->second;
    if (definition.parameters.size() != expectedParamTypes.size()) {
      return false;
    }
    for (size_t index = 0; index < expectedParamTypes.size(); ++index) {
      std::string canonicalParamType;
      if (!resolveBindingCanonicalType(definition, definition.parameters[index], canonicalParamType)) {
        return false;
      }
      if (canonicalParamType != expectedParamTypes[index]) {
        return false;
      }
    }
    std::string canonicalReturnType;
    if (!resolveDefinitionReturnCanonicalType(definition, canonicalReturnType)) {
      return false;
    }
    return canonicalReturnType == expectedReturnType;
  };
  auto hasTraitConformance = [&](const std::string &traitName,
                                 const std::string &canonicalType,
                                 const std::string &structPath,
                                 const std::optional<std::string> &elemCanonicalType) {
    if (traitName == "Additive") {
      if (isNumericTraitType(canonicalType)) {
        return true;
      }
      const std::string basePath = typePathForCanonical(canonicalType, structPath);
      std::vector<std::string> params = {canonicalType, canonicalType};
      return definitionMatchesSignature(basePath + "/plus", params, canonicalType);
    }
    if (traitName == "Multiplicative") {
      if (isNumericTraitType(canonicalType)) {
        return true;
      }
      const std::string basePath = typePathForCanonical(canonicalType, structPath);
      std::vector<std::string> params = {canonicalType, canonicalType};
      return definitionMatchesSignature(basePath + "/multiply", params, canonicalType);
    }
    if (traitName == "Comparable") {
      if (isComparableBuiltinTraitType(canonicalType)) {
        return true;
      }
      const std::string basePath = typePathForCanonical(canonicalType, structPath);
      std::vector<std::string> params = {canonicalType, canonicalType};
      if (!definitionMatchesSignature(basePath + "/equal", params, "bool")) {
        return false;
      }
      return definitionMatchesSignature(basePath + "/less_than", params, "bool");
    }
    if (traitName == "Indexable") {
      if (!elemCanonicalType.has_value()) {
        return false;
      }
      if (isIndexableBuiltinTraitType(canonicalType, *elemCanonicalType)) {
        return true;
      }
      const std::string basePath = typePathForCanonical(canonicalType, structPath);
      std::vector<std::string> countParams = {canonicalType};
      if (!definitionMatchesSignature(basePath + "/count", countParams, "i32")) {
        return false;
      }
      std::vector<std::string> atParams = {canonicalType, "i32"};
      return definitionMatchesSignature(basePath + "/at", atParams, *elemCanonicalType);
    }
    return false;
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
    if (expr.kind != Expr::Kind::Call || expr.isBinding) {
      return true;
    }

    std::string queryName;
    if (expr.isMethodCall) {
      if (expr.args.empty() || expr.args.front().kind != Expr::Kind::Name || expr.args.front().name != "meta") {
        return true;
      }
      queryName = expr.name;
    } else if (expr.name.rfind("/meta/", 0) == 0) {
      queryName = expr.name.substr(6);
    } else {
      return true;
    }

    if (queryName != "type_name" && queryName != "type_kind" && queryName != "is_struct" &&
        queryName != "field_count" && queryName != "field_name" && queryName != "field_type" &&
        queryName != "field_visibility" && queryName != "has_transform" && queryName != "has_trait") {
      return true;
    }

    const bool needsTransformName = queryName == "has_transform";
    const bool needsTraitName = queryName == "has_trait";
    const bool needsFieldIndex =
        queryName == "field_name" || queryName == "field_type" || queryName == "field_visibility";
    const size_t expectedArgs =
        expr.isMethodCall ? (needsFieldIndex || needsTransformName || needsTraitName ? 2u : 1u)
                          : (needsFieldIndex || needsTransformName || needsTraitName ? 1u : 0u);
    if (expr.args.size() != expectedArgs) {
      if (needsFieldIndex) {
        error = "meta." + queryName + " requires exactly one index argument";
      } else if (needsTransformName) {
        error = "meta.has_transform requires exactly one transform-name argument";
      } else if (needsTraitName) {
        error = "meta.has_trait requires exactly one trait-name argument";
      } else {
        error = "meta." + queryName + " does not accept call arguments";
      }
      return false;
    }
    if (semantics::hasNamedArguments(expr.argNames)) {
      error = "meta." + queryName + " does not accept named arguments";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error = "meta." + queryName + " does not accept block arguments";
      return false;
    }
    if (queryName == "has_trait") {
      if (expr.templateArgs.empty() || expr.templateArgs.size() > 2) {
        error = "meta.has_trait requires one or two template arguments";
        return false;
      }
    } else if (expr.templateArgs.size() != 1) {
      error = "meta." + queryName + " requires exactly one template argument";
      return false;
    }

    std::string canonicalType;
    std::string structPath;
    if (!canonicalizeTypeName(expr.templateArgs.front(), expr.namespacePrefix, canonicalType, structPath)) {
      return false;
    }

    Expr rewritten;
    rewritten.sourceLine = expr.sourceLine;
    rewritten.sourceColumn = expr.sourceColumn;
    if (queryName == "type_name") {
      rewritten.kind = Expr::Kind::StringLiteral;
      rewritten.stringValue = escapeStringLiteral(canonicalType);
    } else if (queryName == "type_kind") {
      rewritten.kind = Expr::Kind::StringLiteral;
      rewritten.stringValue = escapeStringLiteral(typeKindForCanonical(canonicalType, structPath));
    } else if (queryName == "is_struct") {
      rewritten.kind = Expr::Kind::BoolLiteral;
      rewritten.boolValue = !structPath.empty();
    } else if (queryName == "field_count") {
      if (structPath.empty()) {
        error = "meta.field_count requires struct type argument: " + canonicalType;
        return false;
      }
      if (reflectedStructNames.count(structPath) == 0) {
        error = "meta.field_count requires reflect-enabled struct type argument: " + canonicalType;
        return false;
      }
      size_t fieldCount = 0;
      auto fieldMetaIt = structFieldMetadata.find(structPath);
      if (fieldMetaIt != structFieldMetadata.end()) {
        fieldCount = fieldMetaIt->second.size();
      }
      rewritten.kind = Expr::Kind::Literal;
      rewritten.intWidth = 32;
      rewritten.isUnsigned = false;
      rewritten.literalValue = static_cast<uint64_t>(fieldCount);
    } else if (queryName == "has_transform") {
      const size_t nameArg = expr.isMethodCall ? 1u : 0u;
      std::string transformName;
      if (!parseNamedQueryName(expr.args[nameArg], "has_transform", "transform", transformName)) {
        return false;
      }
      bool hasTransform = false;
      if (const Definition *targetDef = resolveDefinitionTarget(canonicalType, structPath, expr.namespacePrefix)) {
        for (const auto &transform : targetDef->transforms) {
          if (transform.name == transformName) {
            hasTransform = true;
            break;
          }
        }
      }
      rewritten.kind = Expr::Kind::BoolLiteral;
      rewritten.boolValue = hasTransform;
    } else if (queryName == "has_trait") {
      const size_t nameArg = expr.isMethodCall ? 1u : 0u;
      std::string traitName;
      if (!parseNamedQueryName(expr.args[nameArg], "has_trait", "trait", traitName)) {
        return false;
      }
      traitName = trim(traitName);
      if (traitName.empty()) {
        error = "meta.has_trait requires non-empty trait name";
        return false;
      }
      const bool isAdditive = traitName == "Additive";
      const bool isMultiplicative = traitName == "Multiplicative";
      const bool isComparable = traitName == "Comparable";
      const bool isIndexable = traitName == "Indexable";
      if (!isAdditive && !isMultiplicative && !isComparable && !isIndexable) {
        error = "meta.has_trait does not support trait: " + traitName;
        return false;
      }
      if (isIndexable) {
        if (expr.templateArgs.size() != 2) {
          error = "meta.has_trait Indexable requires type and element template arguments";
          return false;
        }
      } else if (expr.templateArgs.size() != 1) {
        error = "meta.has_trait " + traitName + " requires exactly one type template argument";
        return false;
      }
      std::optional<std::string> elemCanonicalType;
      if (isIndexable) {
        std::string elemStructPath;
        std::string elemCanonical;
        if (!canonicalizeTypeName(expr.templateArgs[1], expr.namespacePrefix, elemCanonical, elemStructPath)) {
          return false;
        }
        elemCanonicalType = std::move(elemCanonical);
      }
      rewritten.kind = Expr::Kind::BoolLiteral;
      rewritten.boolValue = hasTraitConformance(traitName, canonicalType, structPath, elemCanonicalType);
    } else {
      if (structPath.empty()) {
        error = "meta." + queryName + " requires struct type argument: " + canonicalType;
        return false;
      }
      if (reflectedStructNames.count(structPath) == 0) {
        error = "meta." + queryName + " requires reflect-enabled struct type argument: " + canonicalType;
        return false;
      }
      const size_t indexArg = expr.isMethodCall ? 1u : 0u;
      size_t fieldIndex = 0;
      if (!parseFieldIndex(expr.args[indexArg], queryName, fieldIndex)) {
        return false;
      }
      auto fieldMetaIt = structFieldMetadata.find(structPath);
      if (fieldMetaIt == structFieldMetadata.end() || fieldIndex >= fieldMetaIt->second.size()) {
        error = "meta." + queryName + " field index out of range for " + structPath + ": " +
                std::to_string(fieldIndex);
        return false;
      }
      const FieldMetadata &field = fieldMetaIt->second[fieldIndex];
      rewritten.kind = Expr::Kind::StringLiteral;
      if (queryName == "field_name") {
        rewritten.stringValue = escapeStringLiteral(field.name);
      } else if (queryName == "field_type") {
        rewritten.stringValue = escapeStringLiteral(field.typeName);
      } else {
        rewritten.stringValue = escapeStringLiteral(field.visibility);
      }
    }
    expr = std::move(rewritten);
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

bool rewriteReflectionGeneratedHelpers(Program &program, std::string &error) {
  auto isStructDefinition = [](const Definition &def, bool &isExplicitOut) {
    bool hasStructTransform = false;
    bool hasReturnTransform = false;
    for (const auto &transform : def.transforms) {
      if (semantics::isStructTransformName(transform.name)) {
        hasStructTransform = true;
      }
      if (transform.name == "return") {
        hasReturnTransform = true;
      }
    }
    if (hasStructTransform) {
      isExplicitOut = true;
      return true;
    }
    if (hasReturnTransform || !def.parameters.empty() || def.hasReturnStatement || def.returnExpr.has_value()) {
      isExplicitOut = false;
      return false;
    }
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        isExplicitOut = false;
        return false;
      }
    }
    isExplicitOut = false;
    return true;
  };
  auto hasTransformNamed = [](const std::vector<Transform> &transforms, const std::string &name) {
    return std::any_of(transforms.begin(), transforms.end(), [&](const Transform &transform) {
      return transform.name == name;
    });
  };
  auto transformHasArgument = [](const Transform &transform, const std::string &value) {
    return std::any_of(transform.arguments.begin(), transform.arguments.end(), [&](const std::string &arg) {
      return arg == value;
    });
  };

  std::unordered_set<std::string> structNames;
  std::unordered_set<std::string> definitionPaths;
  structNames.reserve(program.definitions.size());
  definitionPaths.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    bool isExplicitStruct = false;
    if (isStructDefinition(def, isExplicitStruct)) {
      structNames.insert(def.fullPath);
    }
    definitionPaths.insert(def.fullPath);
  }

  auto makeTypeBinding = [](const std::string &bindingName,
                            const std::string &typeName,
                            const std::string &namespacePrefix,
                            bool isMutable = false) {
    Expr binding;
    binding.isBinding = true;
    binding.name = bindingName;
    binding.namespacePrefix = namespacePrefix;
    Transform typeTransform;
    std::string typeBase;
    std::string typeArgText;
    if (semantics::splitTemplateTypeName(typeName, typeBase, typeArgText) && !typeBase.empty() &&
        !typeArgText.empty()) {
      typeTransform.name = typeBase;
      std::vector<std::string> typeArgs;
      if (semantics::splitTopLevelTemplateArgs(typeArgText, typeArgs)) {
        typeTransform.templateArgs = std::move(typeArgs);
      } else {
        typeTransform.templateArgs.push_back(typeArgText);
      }
    } else {
      typeTransform.name = typeName;
    }
    binding.transforms.push_back(std::move(typeTransform));
    if (isMutable) {
      Transform mutTransform;
      mutTransform.name = "mut";
      binding.transforms.push_back(std::move(mutTransform));
    }
    return binding;
  };
  auto makeNameExpr = [](const std::string &name) {
    Expr expr;
    expr.kind = Expr::Kind::Name;
    expr.name = name;
    return expr;
  };
  auto makeStringLiteralExpr = [](const std::string &value) {
    Expr expr;
    expr.kind = Expr::Kind::StringLiteral;
    std::string encoded;
    encoded.reserve(value.size() + 8);
    encoded.push_back('"');
    for (const char ch : value) {
      if (ch == '"' || ch == '\\') {
        encoded.push_back('\\');
      }
      encoded.push_back(ch);
    }
    encoded += "\"utf8";
    expr.stringValue = std::move(encoded);
    return expr;
  };
  auto makeCallExpr = [](const std::string &name, Expr arg) {
    Expr call;
    call.kind = Expr::Kind::Call;
    call.name = name;
    call.args.push_back(std::move(arg));
    call.argNames.push_back(std::nullopt);
    return call;
  };
  auto makeI32LiteralExpr = [](uint64_t value) {
    Expr expr;
    expr.kind = Expr::Kind::Literal;
    expr.literalValue = value;
    expr.intWidth = 32;
    expr.isUnsigned = false;
    return expr;
  };
  auto makeU64LiteralExpr = [](uint64_t value) {
    Expr expr;
    expr.kind = Expr::Kind::Literal;
    expr.literalValue = value;
    expr.intWidth = 64;
    expr.isUnsigned = true;
    return expr;
  };
  auto makeBoolLiteralExpr = [](bool value) {
    Expr expr;
    expr.kind = Expr::Kind::BoolLiteral;
    expr.boolValue = value;
    return expr;
  };
  auto makeBinaryCallExpr = [](const std::string &name, Expr left, Expr right) {
    Expr call;
    call.kind = Expr::Kind::Call;
    call.name = name;
    call.args.push_back(std::move(left));
    call.argNames.push_back(std::nullopt);
    call.args.push_back(std::move(right));
    call.argNames.push_back(std::nullopt);
    return call;
  };
  auto makeReturnStatementExpr = [](Expr valueExpr) {
    Expr returnCall;
    returnCall.kind = Expr::Kind::Call;
    returnCall.name = "return";
    returnCall.args.push_back(std::move(valueExpr));
    returnCall.argNames.push_back(std::nullopt);
    return returnCall;
  };
  auto makeEnvelopeExpr = [](const std::string &name, std::vector<Expr> bodyArguments) {
    Expr envelope;
    envelope.kind = Expr::Kind::Call;
    envelope.name = name;
    envelope.hasBodyArguments = true;
    envelope.bodyArguments = std::move(bodyArguments);
    return envelope;
  };
  auto makeIfStatementExpr = [](Expr conditionExpr, Expr thenEnvelopeExpr, Expr elseEnvelopeExpr) {
    Expr ifCall;
    ifCall.kind = Expr::Kind::Call;
    ifCall.name = "if";
    ifCall.args.push_back(std::move(conditionExpr));
    ifCall.argNames.push_back(std::nullopt);
    ifCall.args.push_back(std::move(thenEnvelopeExpr));
    ifCall.argNames.push_back(std::nullopt);
    ifCall.args.push_back(std::move(elseEnvelopeExpr));
    ifCall.argNames.push_back(std::nullopt);
    return ifCall;
  };
  auto appendPublicVisibility = [](Definition &helper) {
    Transform visibilityTransform;
    visibilityTransform.name = "public";
    helper.transforms.push_back(std::move(visibilityTransform));
  };
  auto makeFieldAccessExpr = [&](const std::string &receiverName, const std::string &fieldName) {
    Expr fieldAccess;
    fieldAccess.kind = Expr::Kind::Call;
    fieldAccess.isFieldAccess = true;
    fieldAccess.name = fieldName;
    fieldAccess.args.push_back(makeNameExpr(receiverName));
    fieldAccess.argNames.push_back(std::nullopt);
    return fieldAccess;
  };
  auto makeFieldComparisonExpr = [&](const std::string &comparisonName,
                                     const std::string &leftReceiverName,
                                     const std::string &rightReceiverName,
                                     const std::string &fieldName) {
    Expr compare;
    compare.kind = Expr::Kind::Call;
    compare.name = comparisonName;
    compare.args.push_back(makeFieldAccessExpr(leftReceiverName, fieldName));
    compare.argNames.push_back(std::nullopt);
    compare.args.push_back(makeFieldAccessExpr(rightReceiverName, fieldName));
    compare.argNames.push_back(std::nullopt);
    return compare;
  };
  auto makeBinaryBoolExpr = [](const std::string &operatorName, Expr left, Expr right) {
    Expr call;
    call.kind = Expr::Kind::Call;
    call.name = operatorName;
    call.args.push_back(std::move(left));
    call.argNames.push_back(std::nullopt);
    call.args.push_back(std::move(right));
    call.argNames.push_back(std::nullopt);
    return call;
  };
  auto bindingTypeFromField = [](const Expr &fieldBindingExpr, bool &ambiguousOut) -> std::string {
    ambiguousOut = false;
    std::optional<std::string> typeName;
    for (const auto &transform : fieldBindingExpr.transforms) {
      if (isNonTypeTransformName(transform.name)) {
        continue;
      }
      const std::string candidateType = formatTransformType(transform);
      if (typeName.has_value()) {
        ambiguousOut = true;
        return {};
      }
      typeName = candidateType;
    }
    if (!typeName.has_value()) {
      return "int";
    }
    return *typeName;
  };
  auto isCompareEligibleFieldType = [](const std::string &fieldTypeName) {
    const std::string normalized = semantics::normalizeBindingTypeName(fieldTypeName);
    return normalized == "int" || normalized == "i32" || normalized == "i64" || normalized == "u64" ||
           normalized == "bool" || normalized == "float" || normalized == "f32" || normalized == "f64" ||
           normalized == "string" || normalized == "integer" || normalized == "decimal";
  };
  auto isHash64EligibleFieldType = [](const std::string &fieldTypeName) {
    const std::string normalized = semantics::normalizeBindingTypeName(fieldTypeName);
    return normalized == "int" || normalized == "i32" || normalized == "i64" || normalized == "u64" ||
           normalized == "bool" || normalized == "float" || normalized == "f32" || normalized == "f64";
  };
  constexpr uint64_t SerializationFormatVersionTag = 1ULL;
  constexpr uint64_t DeserializePayloadSizeErrorCode = 1ULL;
  constexpr uint64_t DeserializeFormatVersionErrorCode = 2ULL;

  std::vector<Definition> rewrittenDefinitions;
  rewrittenDefinitions.reserve(program.definitions.size());
  for (auto &def : program.definitions) {
    const bool isStruct = structNames.count(def.fullPath) > 0;
    bool shouldGenerateEqual = false;
    bool shouldGenerateNotEqual = false;
    bool shouldGenerateDefault = false;
    bool shouldGenerateIsDefault = false;
    bool shouldGenerateClone = false;
    bool shouldGenerateDebugPrint = false;
    bool shouldGenerateCompare = false;
    bool shouldGenerateHash64 = false;
    bool shouldGenerateClear = false;
    bool shouldGenerateCopyFrom = false;
    bool shouldGenerateValidate = false;
    bool shouldGenerateSerialize = false;
    bool shouldGenerateDeserialize = false;
    if (isStruct && hasTransformNamed(def.transforms, "reflect")) {
      for (const auto &transform : def.transforms) {
        if (transform.name != "generate") {
          continue;
        }
        shouldGenerateEqual = shouldGenerateEqual || transformHasArgument(transform, "Equal");
        shouldGenerateNotEqual = shouldGenerateNotEqual || transformHasArgument(transform, "NotEqual");
        shouldGenerateDefault = shouldGenerateDefault || transformHasArgument(transform, "Default");
        shouldGenerateIsDefault = shouldGenerateIsDefault || transformHasArgument(transform, "IsDefault");
        shouldGenerateClone = shouldGenerateClone || transformHasArgument(transform, "Clone");
        shouldGenerateDebugPrint = shouldGenerateDebugPrint || transformHasArgument(transform, "DebugPrint");
        shouldGenerateCompare = shouldGenerateCompare || transformHasArgument(transform, "Compare");
        shouldGenerateHash64 = shouldGenerateHash64 || transformHasArgument(transform, "Hash64");
        shouldGenerateClear = shouldGenerateClear || transformHasArgument(transform, "Clear");
        shouldGenerateCopyFrom = shouldGenerateCopyFrom || transformHasArgument(transform, "CopyFrom");
        shouldGenerateValidate = shouldGenerateValidate || transformHasArgument(transform, "Validate");
        shouldGenerateSerialize = shouldGenerateSerialize || transformHasArgument(transform, "Serialize");
        shouldGenerateDeserialize = shouldGenerateDeserialize || transformHasArgument(transform, "Deserialize");
      }
    }

    std::vector<std::string> fieldNames;
    std::unordered_map<std::string, std::string> fieldTypeNames;
    if (shouldGenerateEqual || shouldGenerateNotEqual || shouldGenerateIsDefault || shouldGenerateClone ||
        shouldGenerateDebugPrint || shouldGenerateCompare || shouldGenerateHash64 || shouldGenerateClear ||
        shouldGenerateCopyFrom || shouldGenerateValidate || shouldGenerateSerialize || shouldGenerateDeserialize) {
      fieldNames.reserve(def.statements.size());
      fieldTypeNames.reserve(def.statements.size());
      for (const auto &stmt : def.statements) {
        if (!stmt.isBinding) {
          continue;
        }
        const bool isStaticField =
            std::any_of(stmt.transforms.begin(), stmt.transforms.end(), [](const Transform &transform) {
              return transform.name == "static";
            });
        if (!isStaticField) {
          fieldNames.push_back(stmt.name);
          bool ambiguousFieldType = false;
          const std::string fieldTypeName = bindingTypeFromField(stmt, ambiguousFieldType);
          if (!ambiguousFieldType && !fieldTypeName.empty()) {
            fieldTypeNames.emplace(stmt.name, fieldTypeName);
          }
        }
      }
    }
    auto ensureGeneratedHelperFieldEligibility = [&](const std::string &helperName,
                                                     const std::function<bool(const std::string &)> &isEligible) -> bool {
      const std::string helperPath = def.fullPath + "/" + helperName;
      for (const auto &fieldName : fieldNames) {
        const auto typeIt = fieldTypeNames.find(fieldName);
        if (typeIt == fieldTypeNames.end()) {
          continue;
        }
        if (isEligible(typeIt->second)) {
          continue;
        }
        error = "generated reflection helper " + helperPath + " does not support field envelope: " +
                fieldName + " (" + typeIt->second + ")";
        return false;
      }
      return true;
    };
    if (shouldGenerateCompare && !ensureGeneratedHelperFieldEligibility("Compare", isCompareEligibleFieldType)) {
      return false;
    }
    if (shouldGenerateHash64 && !ensureGeneratedHelperFieldEligibility("Hash64", isHash64EligibleFieldType)) {
      return false;
    }
    if (shouldGenerateSerialize && !ensureGeneratedHelperFieldEligibility("Serialize", isHash64EligibleFieldType)) {
      return false;
    }
    if (shouldGenerateDeserialize && !ensureGeneratedHelperFieldEligibility("Deserialize", isHash64EligibleFieldType)) {
      return false;
    }

    auto emitComparisonHelper = [&](const std::string &helperName,
                                    const std::string &comparisonName,
                                    const std::string &foldOperatorName,
                                    bool emptyValue) -> bool {
      const std::string helperPath = def.fullPath + "/" + helperName;
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      Definition helper;
      helper.name = helperName;
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back("bool");
      helper.transforms.push_back(std::move(returnTransform));

      helper.parameters.push_back(makeTypeBinding("left", def.fullPath, helper.namespacePrefix));
      helper.parameters.push_back(makeTypeBinding("right", def.fullPath, helper.namespacePrefix));

      if (fieldNames.empty()) {
        Expr emptyLiteral;
        emptyLiteral.kind = Expr::Kind::BoolLiteral;
        emptyLiteral.boolValue = emptyValue;
        helper.returnExpr = std::move(emptyLiteral);
      } else {
        Expr combined = makeFieldComparisonExpr(comparisonName, "left", "right", fieldNames.front());
        for (size_t index = 1; index < fieldNames.size(); ++index) {
          combined = makeBinaryBoolExpr(foldOperatorName,
                                        std::move(combined),
                                        makeFieldComparisonExpr(comparisonName, "left", "right", fieldNames[index]));
        }
        helper.returnExpr = std::move(combined);
      }
      helper.hasReturnStatement = true;

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };
    auto emitDebugPrintHelper = [&]() -> bool {
      const std::string helperPath = def.fullPath + "/DebugPrint";
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      Definition helper;
      helper.name = "DebugPrint";
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back("void");
      helper.transforms.push_back(std::move(returnTransform));
      helper.parameters.push_back(makeTypeBinding("value", def.fullPath, helper.namespacePrefix));

      if (fieldNames.empty()) {
        helper.statements.push_back(makeCallExpr("print_line", makeStringLiteralExpr(def.fullPath + " {}")));
      } else {
        helper.statements.push_back(makeCallExpr("print_line", makeStringLiteralExpr(def.fullPath + " {")));
        for (const auto &fieldName : fieldNames) {
          helper.statements.push_back(makeCallExpr("print_line", makeStringLiteralExpr("  " + fieldName)));
        }
        helper.statements.push_back(makeCallExpr("print_line", makeStringLiteralExpr("}")));
      }

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };
    auto emitCompareHelper = [&]() -> bool {
      const std::string helperPath = def.fullPath + "/Compare";
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      Definition helper;
      helper.name = "Compare";
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back("i32");
      helper.transforms.push_back(std::move(returnTransform));

      helper.parameters.push_back(makeTypeBinding("left", def.fullPath, helper.namespacePrefix));
      helper.parameters.push_back(makeTypeBinding("right", def.fullPath, helper.namespacePrefix));

      for (const auto &fieldName : fieldNames) {
        Expr lessThanExpr = makeFieldComparisonExpr("less_than", "left", "right", fieldName);
        Expr lessThanResultExpr =
            makeBinaryCallExpr("minus", makeI32LiteralExpr(0), makeI32LiteralExpr(1));
        std::vector<Expr> lessThenBody;
        lessThenBody.push_back(makeReturnStatementExpr(std::move(lessThanResultExpr)));
        helper.statements.push_back(makeIfStatementExpr(
            std::move(lessThanExpr),
            makeEnvelopeExpr("then", std::move(lessThenBody)),
            makeEnvelopeExpr("else", {})));

        Expr greaterThanExpr = makeFieldComparisonExpr("greater_than", "left", "right", fieldName);
        std::vector<Expr> greaterThenBody;
        greaterThenBody.push_back(makeReturnStatementExpr(makeI32LiteralExpr(1)));
        helper.statements.push_back(makeIfStatementExpr(
            std::move(greaterThanExpr),
            makeEnvelopeExpr("then", std::move(greaterThenBody)),
            makeEnvelopeExpr("else", {})));
      }

      helper.returnExpr = makeI32LiteralExpr(0);
      helper.hasReturnStatement = true;

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };
    auto emitHash64Helper = [&]() -> bool {
      const std::string helperPath = def.fullPath + "/Hash64";
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      Definition helper;
      helper.name = "Hash64";
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back("u64");
      helper.transforms.push_back(std::move(returnTransform));
      helper.parameters.push_back(makeTypeBinding("value", def.fullPath, helper.namespacePrefix));

      Expr combined = makeU64LiteralExpr(1469598103934665603ULL);
      for (const auto &fieldName : fieldNames) {
        Expr convertFieldExpr;
        convertFieldExpr.kind = Expr::Kind::Call;
        convertFieldExpr.name = "convert";
        convertFieldExpr.templateArgs.push_back("u64");
        convertFieldExpr.args.push_back(makeFieldAccessExpr("value", fieldName));
        convertFieldExpr.argNames.push_back(std::nullopt);

        combined = makeBinaryCallExpr(
            "plus",
            makeBinaryCallExpr("multiply", std::move(combined), makeU64LiteralExpr(1099511628211ULL)),
            std::move(convertFieldExpr));
      }
      helper.returnExpr = std::move(combined);
      helper.hasReturnStatement = true;

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };
    auto emitClearHelper = [&]() -> bool {
      const std::string helperPath = def.fullPath + "/Clear";
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      Definition helper;
      helper.name = "Clear";
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back("void");
      helper.transforms.push_back(std::move(returnTransform));
      helper.parameters.push_back(makeTypeBinding("value", def.fullPath, helper.namespacePrefix, true));

      if (!fieldNames.empty()) {
        Expr defaultCall;
        defaultCall.kind = Expr::Kind::Call;
        defaultCall.name = def.fullPath;

        Expr defaultValueBinding = makeTypeBinding("defaultValue", def.fullPath, helper.namespacePrefix);
        defaultValueBinding.args.push_back(std::move(defaultCall));
        defaultValueBinding.argNames.push_back(std::nullopt);
        helper.statements.push_back(std::move(defaultValueBinding));

        for (const auto &fieldName : fieldNames) {
          Expr assignExpr;
          assignExpr.kind = Expr::Kind::Call;
          assignExpr.name = "assign";
          assignExpr.args.push_back(makeFieldAccessExpr("value", fieldName));
          assignExpr.argNames.push_back(std::nullopt);
          assignExpr.args.push_back(makeFieldAccessExpr("defaultValue", fieldName));
          assignExpr.argNames.push_back(std::nullopt);
          helper.statements.push_back(std::move(assignExpr));
        }
      }

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };
    auto emitCopyFromHelper = [&]() -> bool {
      const std::string helperPath = def.fullPath + "/CopyFrom";
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      Definition helper;
      helper.name = "CopyFrom";
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back("void");
      helper.transforms.push_back(std::move(returnTransform));
      helper.parameters.push_back(makeTypeBinding("value", def.fullPath, helper.namespacePrefix, true));
      helper.parameters.push_back(makeTypeBinding("other", def.fullPath, helper.namespacePrefix));

      for (const auto &fieldName : fieldNames) {
        Expr assignExpr;
        assignExpr.kind = Expr::Kind::Call;
        assignExpr.name = "assign";
        assignExpr.args.push_back(makeFieldAccessExpr("value", fieldName));
        assignExpr.argNames.push_back(std::nullopt);
        assignExpr.args.push_back(makeFieldAccessExpr("other", fieldName));
        assignExpr.argNames.push_back(std::nullopt);
        helper.statements.push_back(std::move(assignExpr));
      }

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };
    auto emitValidateHelper = [&]() -> bool {
      const std::string helperPath = def.fullPath + "/Validate";
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      std::vector<std::string> fieldHookPaths;
      fieldHookPaths.reserve(fieldNames.size());
      for (const auto &fieldName : fieldNames) {
        const std::string hookPath = def.fullPath + "/ValidateField_" + fieldName;
        if (definitionPaths.count(hookPath) > 0) {
          error = "generated reflection helper already exists: " + hookPath;
          return false;
        }
        fieldHookPaths.push_back(hookPath);
      }

      for (size_t fieldIndex = 0; fieldIndex < fieldNames.size(); ++fieldIndex) {
        Definition hook;
        hook.name = "ValidateField_" + fieldNames[fieldIndex];
        hook.fullPath = fieldHookPaths[fieldIndex];
        hook.namespacePrefix = def.fullPath;
        hook.sourceLine = def.sourceLine;
        hook.sourceColumn = def.sourceColumn;

        appendPublicVisibility(hook);
        Transform returnTransform;
        returnTransform.name = "return";
        returnTransform.templateArgs.push_back("bool");
        hook.transforms.push_back(std::move(returnTransform));
        hook.parameters.push_back(makeTypeBinding("value", def.fullPath, hook.namespacePrefix));
        hook.returnExpr = makeBoolLiteralExpr(true);
        hook.hasReturnStatement = true;

        rewrittenDefinitions.push_back(std::move(hook));
        definitionPaths.insert(fieldHookPaths[fieldIndex]);
      }

      Definition helper;
      helper.name = "Validate";
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back("Result<FileError>");
      helper.transforms.push_back(std::move(returnTransform));
      helper.parameters.push_back(makeTypeBinding("value", def.fullPath, helper.namespacePrefix));

      for (size_t fieldIndex = 0; fieldIndex < fieldHookPaths.size(); ++fieldIndex) {
        Expr checkCall;
        checkCall.kind = Expr::Kind::Call;
        checkCall.name = fieldHookPaths[fieldIndex];
        checkCall.args.push_back(makeNameExpr("value"));
        checkCall.argNames.push_back(std::nullopt);

        Expr conditionExpr;
        conditionExpr.kind = Expr::Kind::Call;
        conditionExpr.name = "not";
        conditionExpr.args.push_back(std::move(checkCall));
        conditionExpr.argNames.push_back(std::nullopt);

        std::vector<Expr> thenBody;
        thenBody.push_back(makeReturnStatementExpr(makeI32LiteralExpr(fieldIndex + 1)));
        helper.statements.push_back(makeIfStatementExpr(
            std::move(conditionExpr),
            makeEnvelopeExpr("then", std::move(thenBody)),
            makeEnvelopeExpr("else", {})));
      }

      Expr okResultTypeExpr;
      okResultTypeExpr.kind = Expr::Kind::Name;
      okResultTypeExpr.name = "Result";

      Expr okResultCall;
      okResultCall.kind = Expr::Kind::Call;
      okResultCall.isMethodCall = true;
      okResultCall.name = "ok";
      okResultCall.args.push_back(std::move(okResultTypeExpr));
      okResultCall.argNames.push_back(std::nullopt);
      helper.returnExpr = std::move(okResultCall);
      helper.hasReturnStatement = true;

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };
    auto emitSerializeHelper = [&]() -> bool {
      const std::string helperPath = def.fullPath + "/Serialize";
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      Definition helper;
      helper.name = "Serialize";
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back("array<u64>");
      helper.transforms.push_back(std::move(returnTransform));
      helper.parameters.push_back(makeTypeBinding("value", def.fullPath, helper.namespacePrefix));

      Expr payloadCall;
      payloadCall.kind = Expr::Kind::Call;
      payloadCall.name = "array";
      payloadCall.templateArgs.push_back("u64");
      payloadCall.args.push_back(makeU64LiteralExpr(SerializationFormatVersionTag));
      payloadCall.argNames.push_back(std::nullopt);
      for (const auto &fieldName : fieldNames) {
        Expr convertFieldExpr;
        convertFieldExpr.kind = Expr::Kind::Call;
        convertFieldExpr.name = "convert";
        convertFieldExpr.templateArgs.push_back("u64");
        convertFieldExpr.args.push_back(makeFieldAccessExpr("value", fieldName));
        convertFieldExpr.argNames.push_back(std::nullopt);
        payloadCall.args.push_back(std::move(convertFieldExpr));
        payloadCall.argNames.push_back(std::nullopt);
      }
      helper.returnExpr = std::move(payloadCall);
      helper.hasReturnStatement = true;

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };
    auto emitDeserializeHelper = [&]() -> bool {
      const std::string helperPath = def.fullPath + "/Deserialize";
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      Definition helper;
      helper.name = "Deserialize";
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back("Result<FileError>");
      helper.transforms.push_back(std::move(returnTransform));
      helper.parameters.push_back(makeTypeBinding("value", def.fullPath, helper.namespacePrefix, true));
      helper.parameters.push_back(makeTypeBinding("payload", "array<u64>", helper.namespacePrefix));

      Expr payloadCountExpr;
      payloadCountExpr.kind = Expr::Kind::Call;
      payloadCountExpr.name = "count";
      payloadCountExpr.args.push_back(makeNameExpr("payload"));
      payloadCountExpr.argNames.push_back(std::nullopt);
      Expr sizeCheckExpr = makeBinaryCallExpr(
          "not_equal",
          std::move(payloadCountExpr),
          makeI32LiteralExpr(static_cast<uint64_t>(fieldNames.size() + 1)));
      std::vector<Expr> sizeCheckBody;
      sizeCheckBody.push_back(makeReturnStatementExpr(makeI32LiteralExpr(DeserializePayloadSizeErrorCode)));
      helper.statements.push_back(makeIfStatementExpr(
          std::move(sizeCheckExpr),
          makeEnvelopeExpr("then", std::move(sizeCheckBody)),
          makeEnvelopeExpr("else", {})));

      Expr payloadVersionExpr;
      payloadVersionExpr.kind = Expr::Kind::Call;
      payloadVersionExpr.name = "at";
      payloadVersionExpr.args.push_back(makeNameExpr("payload"));
      payloadVersionExpr.argNames.push_back(std::nullopt);
      payloadVersionExpr.args.push_back(makeI32LiteralExpr(0));
      payloadVersionExpr.argNames.push_back(std::nullopt);
      Expr formatCheckExpr = makeBinaryCallExpr(
          "not_equal",
          std::move(payloadVersionExpr),
          makeU64LiteralExpr(SerializationFormatVersionTag));
      std::vector<Expr> formatCheckBody;
      formatCheckBody.push_back(makeReturnStatementExpr(makeI32LiteralExpr(DeserializeFormatVersionErrorCode)));
      helper.statements.push_back(makeIfStatementExpr(
          std::move(formatCheckExpr),
          makeEnvelopeExpr("then", std::move(formatCheckBody)),
          makeEnvelopeExpr("else", {})));

      for (size_t fieldIndex = 0; fieldIndex < fieldNames.size(); ++fieldIndex) {
        const std::string &fieldName = fieldNames[fieldIndex];
        std::string fieldTypeName = "int";
        const auto fieldTypeIt = fieldTypeNames.find(fieldName);
        if (fieldTypeIt != fieldTypeNames.end()) {
          fieldTypeName = fieldTypeIt->second;
        }

        Expr payloadFieldExpr;
        payloadFieldExpr.kind = Expr::Kind::Call;
        payloadFieldExpr.name = "at";
        payloadFieldExpr.args.push_back(makeNameExpr("payload"));
        payloadFieldExpr.argNames.push_back(std::nullopt);
        payloadFieldExpr.args.push_back(makeI32LiteralExpr(static_cast<uint64_t>(fieldIndex + 1)));
        payloadFieldExpr.argNames.push_back(std::nullopt);

        Expr convertFieldExpr;
        convertFieldExpr.kind = Expr::Kind::Call;
        convertFieldExpr.name = "convert";
        convertFieldExpr.templateArgs.push_back(fieldTypeName);
        convertFieldExpr.args.push_back(std::move(payloadFieldExpr));
        convertFieldExpr.argNames.push_back(std::nullopt);

        Expr assignExpr;
        assignExpr.kind = Expr::Kind::Call;
        assignExpr.name = "assign";
        assignExpr.args.push_back(makeFieldAccessExpr("value", fieldName));
        assignExpr.argNames.push_back(std::nullopt);
        assignExpr.args.push_back(std::move(convertFieldExpr));
        assignExpr.argNames.push_back(std::nullopt);
        helper.statements.push_back(std::move(assignExpr));
      }

      Expr okResultTypeExpr;
      okResultTypeExpr.kind = Expr::Kind::Name;
      okResultTypeExpr.name = "Result";

      Expr okResultCall;
      okResultCall.kind = Expr::Kind::Call;
      okResultCall.isMethodCall = true;
      okResultCall.name = "ok";
      okResultCall.args.push_back(std::move(okResultTypeExpr));
      okResultCall.argNames.push_back(std::nullopt);
      helper.returnExpr = std::move(okResultCall);
      helper.hasReturnStatement = true;

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };
    auto emitCloneHelper = [&]() -> bool {
      const std::string helperPath = def.fullPath + "/Clone";
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      Definition helper;
      helper.name = "Clone";
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back(def.fullPath);
      helper.transforms.push_back(std::move(returnTransform));
      helper.parameters.push_back(makeTypeBinding("value", def.fullPath, helper.namespacePrefix));

      Expr cloneCall;
      cloneCall.kind = Expr::Kind::Call;
      cloneCall.name = def.fullPath;
      for (const auto &fieldName : fieldNames) {
        cloneCall.args.push_back(makeFieldAccessExpr("value", fieldName));
        cloneCall.argNames.push_back(std::nullopt);
      }
      helper.returnExpr = std::move(cloneCall);
      helper.hasReturnStatement = true;

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };
    auto emitDefaultHelper = [&]() -> bool {
      const std::string helperPath = def.fullPath + "/Default";
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      Definition helper;
      helper.name = "Default";
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back(def.fullPath);
      helper.transforms.push_back(std::move(returnTransform));

      Expr defaultCall;
      defaultCall.kind = Expr::Kind::Call;
      defaultCall.name = def.fullPath;
      helper.returnExpr = std::move(defaultCall);
      helper.hasReturnStatement = true;

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };
    auto emitIsDefaultHelper = [&]() -> bool {
      const std::string helperPath = def.fullPath + "/IsDefault";
      if (definitionPaths.count(helperPath) > 0) {
        error = "generated reflection helper already exists: " + helperPath;
        return false;
      }

      Definition helper;
      helper.name = "IsDefault";
      helper.fullPath = helperPath;
      helper.namespacePrefix = def.fullPath;
      helper.sourceLine = def.sourceLine;
      helper.sourceColumn = def.sourceColumn;

      appendPublicVisibility(helper);
      Transform returnTransform;
      returnTransform.name = "return";
      returnTransform.templateArgs.push_back("bool");
      helper.transforms.push_back(std::move(returnTransform));
      helper.parameters.push_back(makeTypeBinding("value", def.fullPath, helper.namespacePrefix));

      if (fieldNames.empty()) {
        Expr alwaysTrue;
        alwaysTrue.kind = Expr::Kind::BoolLiteral;
        alwaysTrue.boolValue = true;
        helper.returnExpr = std::move(alwaysTrue);
      } else {
        Expr defaultCall;
        defaultCall.kind = Expr::Kind::Call;
        defaultCall.name = def.fullPath;

        Expr defaultValueBinding = makeTypeBinding("defaultValue", def.fullPath, helper.namespacePrefix);
        defaultValueBinding.args.push_back(std::move(defaultCall));
        defaultValueBinding.argNames.push_back(std::nullopt);
        helper.statements.push_back(std::move(defaultValueBinding));

        Expr combined = makeFieldComparisonExpr("equal", "value", "defaultValue", fieldNames.front());
        for (size_t index = 1; index < fieldNames.size(); ++index) {
          Expr compare = makeFieldComparisonExpr("equal", "value", "defaultValue", fieldNames[index]);
          combined = makeBinaryBoolExpr("and", std::move(combined), std::move(compare));
        }
        helper.returnExpr = std::move(combined);
      }
      helper.hasReturnStatement = true;

      rewrittenDefinitions.push_back(std::move(helper));
      definitionPaths.insert(helperPath);
      return true;
    };

    if (shouldGenerateEqual) {
      if (!emitComparisonHelper("Equal", "equal", "and", true)) {
        return false;
      }
    }
    if (shouldGenerateNotEqual) {
      if (!emitComparisonHelper("NotEqual", "not_equal", "or", false)) {
        return false;
      }
    }
    if (shouldGenerateCompare) {
      if (!emitCompareHelper()) {
        return false;
      }
    }
    if (shouldGenerateHash64) {
      if (!emitHash64Helper()) {
        return false;
      }
    }
    if (shouldGenerateClear) {
      if (!emitClearHelper()) {
        return false;
      }
    }
    if (shouldGenerateCopyFrom) {
      if (!emitCopyFromHelper()) {
        return false;
      }
    }
    if (shouldGenerateValidate) {
      if (!emitValidateHelper()) {
        return false;
      }
    }
    if (shouldGenerateSerialize) {
      if (!emitSerializeHelper()) {
        return false;
      }
    }
    if (shouldGenerateDeserialize) {
      if (!emitDeserializeHelper()) {
        return false;
      }
    }
    if (shouldGenerateDefault) {
      if (!emitDefaultHelper()) {
        return false;
      }
    }
    if (shouldGenerateIsDefault) {
      if (!emitIsDefaultHelper()) {
        return false;
      }
    }
    if (shouldGenerateClone) {
      if (!emitCloneHelper()) {
        return false;
      }
    }
    if (shouldGenerateDebugPrint) {
      if (!emitDebugPrintHelper()) {
        return false;
      }
    }

    rewrittenDefinitions.push_back(std::move(def));
  }
  program.definitions = std::move(rewrittenDefinitions);
  return true;
}

bool rewriteOmittedStructInitializers(Program &program, std::string &error) {
  std::unordered_set<std::string> structNames;
  structNames.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    bool hasStructTransform = false;
    bool hasReturnTransform = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "return") {
        hasReturnTransform = true;
      }
      if (semantics::isStructTransformName(transform.name)) {
        hasStructTransform = true;
      }
    }
    bool fieldOnlyStruct = false;
    if (!hasStructTransform && !hasReturnTransform && def.parameters.empty() && !def.hasReturnStatement &&
        !def.returnExpr.has_value()) {
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
  }

  std::unordered_set<std::string> publicDefinitions;
  publicDefinitions.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    bool sawPublic = false;
    bool sawPrivate = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "public") {
        sawPublic = true;
      } else if (transform.name == "private") {
        sawPrivate = true;
      }
    }
    if (sawPublic && !sawPrivate) {
      publicDefinitions.insert(def.fullPath);
    }
  }

  std::unordered_map<std::string, std::string> importAliases;
  auto isWildcardImport = [](const std::string &path, std::string &prefixOut) -> bool {
    if (path.size() >= 2 && path.compare(path.size() - 2, 2, "/*") == 0) {
      prefixOut = path.substr(0, path.size() - 2);
      return true;
    }
    if (path.find('/', 1) == std::string::npos) {
      prefixOut = path;
      return true;
    }
    return false;
  };
  for (const auto &importPath : program.imports) {
    std::string prefix;
    if (isWildcardImport(importPath, prefix)) {
      std::string scopedPrefix = prefix;
      if (!scopedPrefix.empty() && scopedPrefix.back() != '/') {
        scopedPrefix += "/";
      }
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
    auto isEmptyBuiltinBlockInitializer = [&](const Expr &initializer) -> bool {
      if (!initializer.hasBodyArguments || !initializer.bodyArguments.empty()) {
        return false;
      }
      if (!initializer.args.empty() || !initializer.templateArgs.empty() ||
          semantics::hasNamedArguments(initializer.argNames)) {
        return false;
      }
      return initializer.kind == Expr::Kind::Call && !initializer.isBinding &&
             initializer.name == "block" && initializer.namespacePrefix.empty();
    };
    const bool omittedInitializer = expr.args.empty() ||
                                    (expr.args.size() == 1 && isEmptyBuiltinBlockInitializer(expr.args.front()));
    if (!expr.isBinding || !omittedInitializer) {
      return true;
    }
    semantics::BindingInfo info;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!semantics::parseBindingInfo(expr, expr.namespacePrefix, structNames, importAliases, info, restrictType, parseError)) {
      error = parseError;
      return false;
    }
    const std::string normalizedType = semantics::normalizeBindingTypeName(info.typeName);
    if (!info.typeTemplateArg.empty()) {
      if (normalizedType != "vector") {
        error = "omitted initializer requires struct type: " + info.typeName;
        return false;
      }
      std::vector<std::string> templateArgs;
      if (!semantics::splitTopLevelTemplateArgs(info.typeTemplateArg, templateArgs) || templateArgs.size() != 1) {
        error = "vector requires exactly one template argument";
        return false;
      }
      Expr call;
      call.kind = Expr::Kind::Call;
      call.name = info.typeName;
      call.namespacePrefix = expr.namespacePrefix;
      call.templateArgs = std::move(templateArgs);
      expr.args.clear();
      expr.argNames.clear();
      expr.args.push_back(std::move(call));
      expr.argNames.push_back(std::nullopt);
      return true;
    }
    Expr call;
    call.kind = Expr::Kind::Call;
    call.name = info.typeName;
    call.namespacePrefix = expr.namespacePrefix;
    expr.args.clear();
    expr.argNames.clear();
    expr.args.push_back(std::move(call));
    expr.argNames.push_back(std::nullopt);
    return true;
  };

  for (auto &def : program.definitions) {
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
} // namespace

bool Semantics::validate(Program &program,
                         const std::string &entryPath,
                         std::string &error,
                         const std::vector<std::string> &defaultEffects,
                         const std::vector<std::string> &entryDefaultEffects,
                         const std::vector<std::string> &semanticTransforms,
                         SemanticDiagnosticInfo *diagnosticInfo,
                         bool collectDiagnostics) const {
  error.clear();
  if (!semantics::applySemanticTransforms(program, semanticTransforms, error)) {
    return false;
  }
  if (!rewriteMaybeConstructors(program, error)) {
    return false;
  }
  if (!rewriteExperimentalGfxConstructors(program, error)) {
    return false;
  }
  if (!rewriteReflectionGeneratedHelpers(program, error)) {
    return false;
  }
  if (!semantics::monomorphizeTemplates(program, entryPath, error)) {
    return false;
  }
  if (!rewriteReflectionMetadataQueries(program, error)) {
    return false;
  }
  if (!rewriteConvertConstructors(program, error)) {
    return false;
  }
  semantics::SemanticsValidator validator(
      program, entryPath, error, defaultEffects, entryDefaultEffects, diagnosticInfo, collectDiagnostics);
  if (!validator.run()) {
    return false;
  }
  if (!rewriteOmittedStructInitializers(program, error)) {
    return false;
  }
  error.clear();
  return true;
}

} // namespace primec
