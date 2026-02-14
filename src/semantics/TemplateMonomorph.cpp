#include "SemanticsHelpers.h"
#include "primec/Ast.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace primec::semantics {
namespace {

struct ResolvedType {
  std::string text;
  bool concrete = true;
};

using SubstMap = std::unordered_map<std::string, std::string>;

struct TemplateRootInfo {
  std::string fullPath;
  std::vector<std::string> params;
};

struct Context {
  Program &program;
  std::unordered_map<std::string, Definition> sourceDefs;
  std::unordered_set<std::string> templateDefs;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> specializationCache;
  std::unordered_set<std::string> outputPaths;
  std::vector<Definition> outputDefs;
  std::vector<Execution> outputExecs;
};

bool isNonTypeTransformName(const std::string &name) {
  return name == "return" || name == "effects" || name == "capabilities" || name == "mut" || name == "copy" ||
         name == "restrict" || name == "align_bytes" || name == "align_kbytes" || name == "struct" ||
         name == "pod" || name == "handle" || name == "gpu_lane" || name == "public" || name == "private" ||
         name == "package" || name == "static" || name == "single_type_to_return" || name == "stack" ||
         name == "heap" || name == "buffer";
}

bool isBuiltinTemplateContainer(const std::string &name) {
  return name == "array" || name == "vector" || name == "map" || isBuiltinTemplateTypeName(name);
}

std::string trimWhitespace(const std::string &text) {
  size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }
  size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(start, end - start);
}

std::string stripWhitespace(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    if (!std::isspace(static_cast<unsigned char>(c))) {
      out.push_back(c);
    }
  }
  return out;
}

uint64_t fnv1a64(const std::string &text) {
  uint64_t hash = 1469598103934665603ULL;
  for (unsigned char c : text) {
    hash ^= static_cast<uint64_t>(c);
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::string mangleTemplateArgs(const std::vector<std::string> &args) {
  const std::string canonical = stripWhitespace(joinTemplateArgs(args));
  const uint64_t hash = fnv1a64(canonical);
  std::ostringstream out;
  out << "__t" << std::hex << hash;
  return out.str();
}

bool isPathPrefix(const std::string &prefix, const std::string &path) {
  if (prefix.empty()) {
    return false;
  }
  if (path == prefix) {
    return true;
  }
  if (path.size() <= prefix.size()) {
    return false;
  }
  if (path.rfind(prefix, 0) != 0) {
    return false;
  }
  return path[prefix.size()] == '/';
}

std::string replacePathPrefix(const std::string &path, const std::string &prefix, const std::string &replacement) {
  if (!isPathPrefix(prefix, path)) {
    return path;
  }
  if (path == prefix) {
    return replacement;
  }
  return replacement + path.substr(prefix.size());
}

std::string resolveNameToPath(const std::string &name,
                              const std::string &namespacePrefix,
                              const std::unordered_map<std::string, std::string> &importAliases,
                              const std::unordered_map<std::string, Definition> &defs) {
  if (name.empty()) {
    return "";
  }
  if (!name.empty() && name[0] == '/') {
    return name;
  }
  if (name.find('/') != std::string::npos) {
    return "/" + name;
  }
  if (!namespacePrefix.empty()) {
    std::string scoped = namespacePrefix + "/" + name;
    if (defs.count(scoped) > 0) {
      return scoped;
    }
    auto aliasIt = importAliases.find(name);
    if (aliasIt != importAliases.end()) {
      return aliasIt->second;
    }
    return scoped;
  }
  std::string root = "/" + name;
  if (defs.count(root) > 0) {
    return root;
  }
  auto aliasIt = importAliases.find(name);
  if (aliasIt != importAliases.end()) {
    return aliasIt->second;
  }
  return root;
}

bool rewriteExpr(Expr &expr,
                 const SubstMap &mapping,
                 const std::unordered_set<std::string> &allowedParams,
                 const std::string &namespacePrefix,
                 Context &ctx,
                 std::string &error);

bool instantiateTemplate(const std::string &basePath,
                         const std::vector<std::string> &resolvedArgs,
                         Context &ctx,
                         std::string &error,
                         std::string &specializedPathOut);

ResolvedType resolveTypeString(const std::string &input,
                               const SubstMap &mapping,
                               const std::unordered_set<std::string> &allowedParams,
                               const std::string &namespacePrefix,
                               Context &ctx,
                               std::string &error) {
  ResolvedType result;
  std::string trimmed = trimWhitespace(input);
  if (trimmed.empty()) {
    result.text = input;
    result.concrete = true;
    return result;
  }
  if (allowedParams.count(trimmed) > 0) {
    result.text = trimmed;
    result.concrete = false;
    return result;
  }
  auto mapIt = mapping.find(trimmed);
  if (mapIt != mapping.end()) {
    return resolveTypeString(mapIt->second, mapping, allowedParams, namespacePrefix, ctx, error);
  }
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(trimmed, base, argText)) {
    std::string resolvedPath = resolveNameToPath(trimmed, namespacePrefix, ctx.importAliases, ctx.sourceDefs);
    if (ctx.templateDefs.count(resolvedPath) > 0) {
      error = "template arguments required for " + resolvedPath;
      result.text.clear();
      result.concrete = false;
      return result;
    }
    result.text = trimmed;
    result.concrete = true;
    return result;
  }
  if (allowedParams.count(base) > 0) {
    error = "template parameters cannot be templated: " + base;
    result.text.clear();
    result.concrete = false;
    return result;
  }
  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(argText, args)) {
    error = "invalid template arguments for " + base;
    result.text.clear();
    result.concrete = false;
    return result;
  }
  std::vector<std::string> resolvedArgs;
  resolvedArgs.reserve(args.size());
  bool allConcrete = true;
  for (const auto &arg : args) {
    ResolvedType resolved = resolveTypeString(arg, mapping, allowedParams, namespacePrefix, ctx, error);
    if (!error.empty()) {
      result.text.clear();
      result.concrete = false;
      return result;
    }
    if (!resolved.concrete) {
      allConcrete = false;
    }
    resolvedArgs.push_back(resolved.text);
  }
  if (isBuiltinTemplateContainer(base)) {
    result.text = base + "<" + joinTemplateArgs(resolvedArgs) + ">";
    result.concrete = allConcrete;
    return result;
  }
  std::string resolvedBasePath = resolveNameToPath(base, namespacePrefix, ctx.importAliases, ctx.sourceDefs);
  if (ctx.templateDefs.count(resolvedBasePath) == 0) {
    error = "template arguments are only supported on templated definitions: " + resolvedBasePath;
    result.text.clear();
    result.concrete = false;
    return result;
  }
  if (!allConcrete) {
    result.text = base + "<" + joinTemplateArgs(resolvedArgs) + ">";
    result.concrete = false;
    return result;
  }
  std::string specializedPath;
  if (!instantiateTemplate(resolvedBasePath, resolvedArgs, ctx, error, specializedPath)) {
    result.text.clear();
    result.concrete = false;
    return result;
  }
  result.text = specializedPath;
  result.concrete = true;
  return result;
}

bool rewriteTransforms(std::vector<Transform> &transforms,
                       const SubstMap &mapping,
                       const std::unordered_set<std::string> &allowedParams,
                       const std::string &namespacePrefix,
                       Context &ctx,
                       std::string &error) {
  for (auto &transform : transforms) {
    if (!isNonTypeTransformName(transform.name)) {
      ResolvedType resolvedName = resolveTypeString(transform.name, mapping, allowedParams, namespacePrefix, ctx, error);
      if (!error.empty()) {
        return false;
      }
      if (resolvedName.text != transform.name) {
        std::string base;
        std::string argText;
        if (splitTemplateTypeName(resolvedName.text, base, argText)) {
          if (!transform.templateArgs.empty()) {
            error = "template arguments cannot be combined on " + transform.name;
            return false;
          }
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(argText, args)) {
            error = "invalid template arguments for " + resolvedName.text;
            return false;
          }
          transform.name = base;
          transform.templateArgs = std::move(args);
        } else {
          transform.name = resolvedName.text;
        }
      }
    }
    for (auto &arg : transform.templateArgs) {
      ResolvedType resolvedArg = resolveTypeString(arg, mapping, allowedParams, namespacePrefix, ctx, error);
      if (!error.empty()) {
        return false;
      }
      arg = resolvedArg.text;
    }
  }
  return true;
}

std::string resolveCalleePath(const Expr &expr, const std::string &namespacePrefix, const Context &ctx) {
  if (expr.name.empty()) {
    return "";
  }
  if (!expr.name.empty() && expr.name[0] == '/') {
    return expr.name;
  }
  if (expr.name.find('/') != std::string::npos) {
    return "/" + expr.name;
  }
  if (!namespacePrefix.empty()) {
    std::string scoped = namespacePrefix + "/" + expr.name;
    if (ctx.sourceDefs.count(scoped) > 0) {
      return scoped;
    }
    auto aliasIt = ctx.importAliases.find(expr.name);
    if (aliasIt != ctx.importAliases.end()) {
      return aliasIt->second;
    }
    return scoped;
  }
  std::string root = "/" + expr.name;
  if (ctx.sourceDefs.count(root) > 0) {
    return root;
  }
  auto aliasIt = ctx.importAliases.find(expr.name);
  if (aliasIt != ctx.importAliases.end()) {
    return aliasIt->second;
  }
  return root;
}

bool rewriteExpr(Expr &expr,
                 const SubstMap &mapping,
                 const std::unordered_set<std::string> &allowedParams,
                 const std::string &namespacePrefix,
                 Context &ctx,
                 std::string &error) {
  expr.namespacePrefix = namespacePrefix;
  if (!rewriteTransforms(expr.transforms, mapping, allowedParams, namespacePrefix, ctx, error)) {
    return false;
  }
  if (expr.kind != Expr::Kind::Call) {
    return true;
  }
  if (expr.isLambda) {
    std::unordered_set<std::string> lambdaAllowed = allowedParams;
    for (const auto &param : expr.templateArgs) {
      lambdaAllowed.insert(param);
    }
    SubstMap lambdaMapping = mapping;
    for (const auto &param : expr.templateArgs) {
      lambdaMapping.erase(param);
    }
    for (auto &param : expr.args) {
      if (!rewriteExpr(param, lambdaMapping, lambdaAllowed, namespacePrefix, ctx, error)) {
        return false;
      }
    }
    for (auto &bodyArg : expr.bodyArguments) {
      if (!rewriteExpr(bodyArg, lambdaMapping, lambdaAllowed, namespacePrefix, ctx, error)) {
        return false;
      }
    }
    return true;
  }

  bool allConcrete = true;
  for (auto &templArg : expr.templateArgs) {
    ResolvedType resolvedArg = resolveTypeString(templArg, mapping, allowedParams, namespacePrefix, ctx, error);
    if (!error.empty()) {
      return false;
    }
    if (!resolvedArg.concrete) {
      allConcrete = false;
    }
    templArg = resolvedArg.text;
  }

  if (!expr.isMethodCall) {
    std::string resolvedPath = resolveCalleePath(expr, namespacePrefix, ctx);
    const bool isTemplateDef = ctx.templateDefs.count(resolvedPath) > 0;
    const bool isKnownDef = ctx.sourceDefs.count(resolvedPath) > 0;
    if (isTemplateDef) {
      if (expr.templateArgs.empty()) {
        error = "template arguments required for " + resolvedPath;
        return false;
      }
      if (allConcrete) {
        std::string specializedPath;
        if (!instantiateTemplate(resolvedPath, expr.templateArgs, ctx, error, specializedPath)) {
          return false;
        }
        expr.name = specializedPath;
        expr.templateArgs.clear();
      }
    } else if (isKnownDef && !expr.templateArgs.empty()) {
      error = "template arguments are only supported on templated definitions: " + resolvedPath;
      return false;
    }
  }

  for (auto &arg : expr.args) {
    if (!rewriteExpr(arg, mapping, allowedParams, namespacePrefix, ctx, error)) {
      return false;
    }
  }
  for (auto &arg : expr.bodyArguments) {
    if (!rewriteExpr(arg, mapping, allowedParams, namespacePrefix, ctx, error)) {
      return false;
    }
  }
  return true;
}

bool rewriteDefinition(Definition &def,
                       const SubstMap &mapping,
                       const std::unordered_set<std::string> &allowedParams,
                       Context &ctx,
                       std::string &error) {
  if (!rewriteTransforms(def.transforms, mapping, allowedParams, def.namespacePrefix, ctx, error)) {
    return false;
  }
  for (auto &param : def.parameters) {
    if (!rewriteExpr(param, mapping, allowedParams, def.namespacePrefix, ctx, error)) {
      return false;
    }
  }
  for (auto &stmt : def.statements) {
    if (!rewriteExpr(stmt, mapping, allowedParams, def.namespacePrefix, ctx, error)) {
      return false;
    }
  }
  if (def.returnExpr.has_value()) {
    if (!rewriteExpr(*def.returnExpr, mapping, allowedParams, def.namespacePrefix, ctx, error)) {
      return false;
    }
  }
  return true;
}

bool rewriteExecution(Execution &exec, Context &ctx, std::string &error) {
  if (!rewriteTransforms(exec.transforms, SubstMap{}, {}, exec.namespacePrefix, ctx, error)) {
    return false;
  }
  bool allConcrete = true;
  for (auto &templArg : exec.templateArgs) {
    ResolvedType resolved = resolveTypeString(templArg, SubstMap{}, {}, exec.namespacePrefix, ctx, error);
    if (!error.empty()) {
      return false;
    }
    if (!resolved.concrete) {
      allConcrete = false;
    }
    templArg = resolved.text;
  }
  Expr execExpr;
  execExpr.kind = Expr::Kind::Call;
  execExpr.name = exec.name;
  execExpr.namespacePrefix = exec.namespacePrefix;
  execExpr.templateArgs = exec.templateArgs;
  execExpr.args = exec.arguments;
  execExpr.argNames = exec.argumentNames;
  execExpr.bodyArguments = exec.bodyArguments;
  execExpr.hasBodyArguments = exec.hasBodyArguments;
  if (!rewriteExpr(execExpr, SubstMap{}, {}, exec.namespacePrefix, ctx, error)) {
    return false;
  }
  exec.name = execExpr.name;
  exec.namespacePrefix = execExpr.namespacePrefix;
  exec.templateArgs = execExpr.templateArgs;
  exec.arguments = execExpr.args;
  exec.argumentNames = execExpr.argNames;
  exec.bodyArguments = execExpr.bodyArguments;
  exec.hasBodyArguments = execExpr.hasBodyArguments;
  if (ctx.templateDefs.count(resolveCalleePath(execExpr, exec.namespacePrefix, ctx)) > 0 && exec.templateArgs.empty()) {
    error = "template arguments required for " + resolveCalleePath(execExpr, exec.namespacePrefix, ctx);
    return false;
  }
  if (ctx.templateDefs.count(resolveCalleePath(execExpr, exec.namespacePrefix, ctx)) > 0 && !allConcrete) {
    error = "template arguments must be concrete on executions";
    return false;
  }
  return true;
}

bool instantiateTemplate(const std::string &basePath,
                         const std::vector<std::string> &resolvedArgs,
                         Context &ctx,
                         std::string &error,
                         std::string &specializedPathOut) {
  auto defIt = ctx.sourceDefs.find(basePath);
  if (defIt == ctx.sourceDefs.end()) {
    error = "unknown template definition: " + basePath;
    return false;
  }
  const Definition &baseDef = defIt->second;
  if (baseDef.templateArgs.empty()) {
    error = "template arguments are only supported on templated definitions: " + basePath;
    return false;
  }
  if (baseDef.templateArgs.size() != resolvedArgs.size()) {
    std::ostringstream out;
    out << "template argument count mismatch for " << basePath << ": expected " << baseDef.templateArgs.size()
        << ", got " << resolvedArgs.size();
    error = out.str();
    return false;
  }
  const std::string key = basePath + "<" + stripWhitespace(joinTemplateArgs(resolvedArgs)) + ">";
  auto cacheIt = ctx.specializationCache.find(key);
  if (cacheIt != ctx.specializationCache.end()) {
    specializedPathOut = cacheIt->second;
    return true;
  }

  const size_t lastSlash = basePath.find_last_of('/');
  const std::string baseName = lastSlash == std::string::npos ? basePath : basePath.substr(lastSlash + 1);
  const std::string suffix = mangleTemplateArgs(resolvedArgs);
  const std::string specializedName = baseName + suffix;
  const std::string specializedBasePath = (lastSlash == std::string::npos)
                                              ? specializedName
                                              : basePath.substr(0, lastSlash + 1) + specializedName;
  if (ctx.sourceDefs.count(specializedBasePath) > 0) {
    error = "template specialization conflicts with existing definition: " + specializedBasePath;
    return false;
  }
  ctx.specializationCache.emplace(key, specializedBasePath);

  std::vector<Definition> family;
  family.reserve(ctx.sourceDefs.size());
  for (const auto &entry : ctx.sourceDefs) {
    if (isPathPrefix(basePath, entry.first)) {
      family.push_back(entry.second);
    }
  }

  std::vector<TemplateRootInfo> nestedTemplates;
  nestedTemplates.reserve(family.size());
  for (const auto &def : family) {
    if (def.fullPath == basePath) {
      continue;
    }
    if (!def.templateArgs.empty()) {
      nestedTemplates.push_back({def.fullPath, def.templateArgs});
    }
  }

  SubstMap baseMapping;
  baseMapping.reserve(baseDef.templateArgs.size());
  for (size_t i = 0; i < baseDef.templateArgs.size(); ++i) {
    baseMapping.emplace(baseDef.templateArgs[i], resolvedArgs[i]);
  }

  for (const auto &def : family) {
    Definition clone = def;
    clone.fullPath = replacePathPrefix(def.fullPath, basePath, specializedBasePath);
    clone.namespacePrefix = replacePathPrefix(def.namespacePrefix, basePath, specializedBasePath);
    if (def.fullPath == basePath) {
      clone.name = specializedName;
      clone.templateArgs.clear();
    }
    if (ctx.sourceDefs.count(clone.fullPath) > 0) {
      error = "template specialization conflicts with existing definition: " + clone.fullPath;
      ctx.specializationCache.erase(key);
      return false;
    }

    std::unordered_set<std::string> shadowedParams;
    for (const auto &nested : nestedTemplates) {
      if (isPathPrefix(nested.fullPath, def.fullPath)) {
        for (const auto &param : nested.params) {
          shadowedParams.insert(param);
        }
      }
    }
    SubstMap mapping = baseMapping;
    for (const auto &param : shadowedParams) {
      mapping.erase(param);
    }
    if (!rewriteDefinition(clone, mapping, shadowedParams, ctx, error)) {
      ctx.specializationCache.erase(key);
      return false;
    }

    ctx.sourceDefs.emplace(clone.fullPath, clone);
    if (!clone.templateArgs.empty()) {
      ctx.templateDefs.insert(clone.fullPath);
    }

    bool underNestedTemplate = false;
    if (def.fullPath != basePath) {
      for (const auto &nested : nestedTemplates) {
        if (isPathPrefix(nested.fullPath, def.fullPath)) {
          underNestedTemplate = true;
          break;
        }
      }
    }
    if (!underNestedTemplate && clone.templateArgs.empty()) {
      if (ctx.outputPaths.insert(clone.fullPath).second) {
        ctx.outputDefs.push_back(clone);
      }
    }
  }

  specializedPathOut = specializedBasePath;
  return true;
}

void buildImportAliases(Context &ctx) {
  ctx.importAliases.clear();
  for (const auto &importPath : ctx.program.imports) {
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
      for (const auto &entry : ctx.sourceDefs) {
        const std::string &path = entry.first;
        if (path.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        const std::string remainder = path.substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string::npos) {
          continue;
        }
        ctx.importAliases.emplace(remainder, path);
      }
      continue;
    }
    auto defIt = ctx.sourceDefs.find(importPath);
    if (defIt == ctx.sourceDefs.end()) {
      continue;
    }
    const std::string remainder = importPath.substr(importPath.find_last_of('/') + 1);
    if (remainder.empty()) {
      continue;
    }
    ctx.importAliases.emplace(remainder, importPath);
  }
}

} // namespace

bool monomorphizeTemplates(Program &program, const std::string &entryPath, std::string &error) {
  Context ctx{program, {}, {}, {}, {}, {}, {}, {}};
  ctx.sourceDefs.clear();
  ctx.templateDefs.clear();
  ctx.outputDefs.clear();
  ctx.outputExecs.clear();
  ctx.outputPaths.clear();

  std::unordered_set<std::string> templateRoots;
  templateRoots.clear();
  for (const auto &def : program.definitions) {
    auto [it, inserted] = ctx.sourceDefs.emplace(def.fullPath, def);
    if (!inserted) {
      error = "duplicate definition: " + def.fullPath;
      return false;
    }
    if (!def.templateArgs.empty()) {
      ctx.templateDefs.insert(def.fullPath);
      templateRoots.insert(def.fullPath);
    }
  }
  if (templateRoots.count(entryPath) > 0) {
    error = "entry definition cannot be templated: " + entryPath;
    return false;
  }

  buildImportAliases(ctx);

  auto isUnderTemplateRoot = [&](const std::string &path) -> bool {
    for (const auto &root : templateRoots) {
      if (isPathPrefix(root, path)) {
        return true;
      }
    }
    return false;
  };

  for (const auto &def : program.definitions) {
    if (isUnderTemplateRoot(def.fullPath)) {
      continue;
    }
    Definition clone = def;
    if (!rewriteDefinition(clone, SubstMap{}, {}, ctx, error)) {
      return false;
    }
    if (ctx.outputPaths.insert(clone.fullPath).second) {
      ctx.outputDefs.push_back(std::move(clone));
    }
  }

  ctx.outputExecs.reserve(program.executions.size());
  for (const auto &exec : program.executions) {
    Execution clone = exec;
    if (!rewriteExecution(clone, ctx, error)) {
      return false;
    }
    ctx.outputExecs.push_back(std::move(clone));
  }

  program.definitions = std::move(ctx.outputDefs);
  program.executions = std::move(ctx.outputExecs);
  return true;
}

} // namespace primec::semantics
