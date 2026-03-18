#include "SemanticsValidateExperimentalGfxConstructors.h"

#include "SemanticsHelpers.h"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace primec::semantics {

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
      if (isStructTransformName(transform.name)) {
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
    std::string resolved = resolveStructTypePath(name, namespacePrefix, structNames);
    if (resolved.empty()) {
      auto it = importAliases.find(name);
      if (it != importAliases.end() && structNames.count(it->second) > 0) {
        resolved = it->second;
      }
    }
    return resolved;
  };

  using LocalBindings = std::unordered_map<std::string, BindingInfo>;

  auto resolveBindingStructPath = [&](const BindingInfo &binding, const std::string &namespacePrefix) {
    if (!binding.typeName.empty() && binding.typeName.front() == '/' && structNames.count(binding.typeName) > 0) {
      return binding.typeName;
    }
    return resolveImportedStructPath(binding.typeName, namespacePrefix);
  };

  auto rememberBinding = [&](const Expr &expr, const std::string &namespacePrefix, LocalBindings &locals) {
    if (!expr.isBinding) {
      return;
    }
    BindingInfo binding;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!parseBindingInfo(expr, namespacePrefix, structNames, importAliases, binding, restrictType, parseError)) {
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
        !expr.hasBodyArguments && expr.bodyArguments.empty() && !hasNamedArguments(expr.argNames)) {
      expr.name = resolved == "/std/gfx/Device" ? "/std/gfx/deviceCreate" : "/std/gfx/experimental/deviceCreate";
      expr.namespacePrefix.clear();
      return true;
    }
    if ((resolved == "/std/gfx/experimental/Buffer" || resolved == "/std/gfx/Buffer") && expr.args.size() == 1 &&
        !expr.hasBodyArguments && expr.bodyArguments.empty() && !hasNamedArguments(expr.argNames)) {
      expr.name = resolved == "/std/gfx/Buffer" ? "/std/gfx/Buffer/allocate" : "/std/gfx/experimental/Buffer/allocate";
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

} // namespace primec::semantics
