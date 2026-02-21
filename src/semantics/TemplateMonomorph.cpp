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

using LocalTypeMap = std::unordered_map<std::string, BindingInfo>;

bool isNonTypeTransformName(const std::string &name) {
  return name == "return" || name == "effects" || name == "capabilities" || name == "mut" || name == "copy" ||
         name == "restrict" || name == "align_bytes" || name == "align_kbytes" || name == "on_error" ||
         name == "struct" ||
         name == "pod" || name == "handle" || name == "gpu_lane" || name == "no_padding" ||
         name == "platform_independent_padding" || name == "public" || name == "private" || name == "package" ||
         name == "static" || name == "single_type_to_return" || name == "stack" || name == "heap" || name == "buffer";
}

bool isBuiltinTemplateContainer(const std::string &name) {
  return name == "array" || name == "vector" || name == "map" || name == "Result" || name == "File" ||
         isBuiltinTemplateTypeName(name);
}

bool isStructDefinition(const Definition &def) {
  bool isStruct = false;
  bool hasReturnTransform = false;
  for (const auto &transform : def.transforms) {
    if (transform.name == "return") {
      hasReturnTransform = true;
    }
    if (isStructTransformName(transform.name)) {
      isStruct = true;
    }
  }
  if (isStruct) {
    return true;
  }
  if (hasReturnTransform || !def.parameters.empty() || def.hasReturnStatement || def.returnExpr.has_value()) {
    return false;
  }
  for (const auto &stmt : def.statements) {
    if (!stmt.isBinding) {
      return false;
    }
  }
  return true;
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

bool hasMathImport(const Context &ctx) {
  for (const auto &importPath : ctx.program.imports) {
    if (importPath.rfind("/math/", 0) == 0 && importPath.size() > 6) {
      return true;
    }
  }
  return false;
}

bool extractExplicitBindingType(const Expr &expr, BindingInfo &infoOut) {
  if (!expr.isBinding) {
    return false;
  }
  if (!hasExplicitBindingTypeTransform(expr)) {
    return false;
  }
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities" || transform.name == "return") {
      continue;
    }
    if (isBindingAuxTransformName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    infoOut.typeName = transform.name;
    infoOut.typeTemplateArg.clear();
    if (!transform.templateArgs.empty()) {
      infoOut.typeTemplateArg = joinTemplateArgs(transform.templateArgs);
    }
    return true;
  }
  return false;
}

std::string resolveCalleePath(const Expr &expr, const std::string &namespacePrefix, const Context &ctx);

bool inferBindingTypeForMonomorph(const Expr &initializer,
                                  const std::vector<ParameterInfo> &params,
                                  const LocalTypeMap &locals,
                                  bool allowMathBare,
                                  Context &ctx,
                                  BindingInfo &infoOut);

bool resolveMethodCallTemplateTarget(const Expr &expr,
                                     const LocalTypeMap &locals,
                                     const Context &ctx,
                                     std::string &pathOut) {
  pathOut.clear();
  if (!expr.isMethodCall || expr.args.empty() || expr.name.empty()) {
    return false;
  }
  const Expr &receiver = expr.args.front();
  std::string typeName;
  if (receiver.kind == Expr::Kind::Name) {
    auto it = locals.find(receiver.name);
    if (it != locals.end()) {
      typeName = it->second.typeName;
    }
  } else if (receiver.kind == Expr::Kind::Literal) {
    typeName = receiver.isUnsigned ? "u64" : (receiver.intWidth == 64 ? "i64" : "i32");
  } else if (receiver.kind == Expr::Kind::BoolLiteral) {
    typeName = "bool";
  } else if (receiver.kind == Expr::Kind::FloatLiteral) {
    typeName = receiver.floatWidth == 64 ? "f64" : "f32";
  } else if (receiver.kind == Expr::Kind::StringLiteral) {
    typeName = "string";
  } else if (receiver.kind == Expr::Kind::Call) {
    std::string collection;
    if (getBuiltinCollectionName(receiver, collection)) {
      typeName = collection;
    }
    if (typeName.empty() && !receiver.isMethodCall && !receiver.isBinding) {
      std::string resolved = resolveCalleePath(receiver, receiver.namespacePrefix, ctx);
      auto defIt = ctx.sourceDefs.find(resolved);
      if (defIt != ctx.sourceDefs.end() && isStructDefinition(defIt->second)) {
        pathOut = resolved + "/" + expr.name;
        return true;
      }
    }
  }
  if (typeName.empty()) {
    return false;
  }
  if (isPrimitiveBindingTypeName(typeName)) {
    pathOut = "/" + normalizeBindingTypeName(typeName) + "/" + expr.name;
    return true;
  }
  std::string resolvedType = resolveTypePath(typeName, receiver.namespacePrefix);
  if (ctx.sourceDefs.count(resolvedType) == 0) {
    auto aliasIt = ctx.importAliases.find(typeName);
    if (aliasIt != ctx.importAliases.end()) {
      resolvedType = aliasIt->second;
    }
  }
  if (ctx.sourceDefs.count(resolvedType) == 0) {
    return false;
  }
  pathOut = resolvedType + "/" + expr.name;
  return true;
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

bool inferBindingTypeForMonomorph(const Expr &initializer,
                                  const std::vector<ParameterInfo> &params,
                                  const LocalTypeMap &locals,
                                  bool allowMathBare,
                                  Context &ctx,
                                  BindingInfo &infoOut) {
  if (tryInferBindingTypeFromInitializer(initializer, params, locals, infoOut, allowMathBare)) {
    return true;
  }
  if (initializer.kind != Expr::Kind::Call || !isBlockCall(initializer) || !initializer.hasBodyArguments) {
    return false;
  }
  const std::string resolved = resolveCalleePath(initializer, initializer.namespacePrefix, ctx);
  if (ctx.sourceDefs.count(resolved) > 0) {
    return false;
  }
  if (!initializer.args.empty() || !initializer.templateArgs.empty() || hasNamedArguments(initializer.argNames)) {
    return false;
  }
  if (initializer.bodyArguments.empty()) {
    return false;
  }
  LocalTypeMap blockLocals = locals;
  const Expr *valueExpr = nullptr;
  bool sawReturn = false;
  for (const auto &bodyExpr : initializer.bodyArguments) {
    if (bodyExpr.isBinding) {
      BindingInfo binding;
      if (extractExplicitBindingType(bodyExpr, binding)) {
        blockLocals[bodyExpr.name] = binding;
      } else if (bodyExpr.args.size() == 1) {
        if (inferBindingTypeForMonomorph(bodyExpr.args.front(), params, blockLocals, allowMathBare, ctx, binding)) {
          blockLocals[bodyExpr.name] = binding;
        }
      }
      continue;
    }
    if (isReturnCall(bodyExpr)) {
      if (bodyExpr.args.size() != 1) {
        return false;
      }
      valueExpr = &bodyExpr.args.front();
      sawReturn = true;
      continue;
    }
    if (!sawReturn) {
      valueExpr = &bodyExpr;
    }
  }
  if (!valueExpr) {
    return false;
  }
  return inferBindingTypeForMonomorph(*valueExpr, params, blockLocals, allowMathBare, ctx, infoOut);
}

bool rewriteExpr(Expr &expr,
                 const SubstMap &mapping,
                 const std::unordered_set<std::string> &allowedParams,
                 const std::string &namespacePrefix,
                 Context &ctx,
                 std::string &error,
                 const LocalTypeMap &locals,
                 const std::vector<ParameterInfo> &params,
                 bool allowMathBare);

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
    std::string current = namespacePrefix;
    while (true) {
      if (!current.empty()) {
        std::string scoped = current + "/" + expr.name;
        if (ctx.sourceDefs.count(scoped) > 0) {
          return scoped;
        }
        if (current.size() > expr.name.size()) {
          const size_t start = current.size() - expr.name.size();
          if (start > 0 && current[start - 1] == '/' &&
              current.compare(start, expr.name.size(), expr.name) == 0 &&
              ctx.sourceDefs.count(current) > 0) {
            return current;
          }
        }
      } else {
        std::string root = "/" + expr.name;
        if (ctx.sourceDefs.count(root) > 0) {
          return root;
        }
      }
      if (current.empty()) {
        break;
      }
      const size_t slash = current.find_last_of('/');
      if (slash == std::string::npos || slash == 0) {
        current.clear();
      } else {
        current.erase(slash);
      }
    }
    auto aliasIt = ctx.importAliases.find(expr.name);
    if (aliasIt != ctx.importAliases.end()) {
      return aliasIt->second;
    }
    return "/" + expr.name;
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
                 std::string &error,
                 const LocalTypeMap &locals,
                 const std::vector<ParameterInfo> &params,
                 bool allowMathBare) {
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
    LocalTypeMap lambdaLocals = locals;
    lambdaLocals.reserve(lambdaLocals.size() + expr.args.size() + expr.bodyArguments.size());
    for (auto &param : expr.args) {
      if (!rewriteExpr(param, lambdaMapping, lambdaAllowed, namespacePrefix, ctx, error, lambdaLocals, params,
                       allowMathBare)) {
        return false;
      }
      BindingInfo info;
      if (extractExplicitBindingType(param, info)) {
        lambdaLocals[param.name] = info;
      } else if (param.isBinding && param.args.size() == 1) {
        if (inferBindingTypeForMonomorph(param.args.front(), {}, {}, allowMathBare, ctx, info)) {
          lambdaLocals[param.name] = info;
        }
      }
    }
    for (auto &bodyArg : expr.bodyArguments) {
      if (!rewriteExpr(bodyArg, lambdaMapping, lambdaAllowed, namespacePrefix, ctx, error, lambdaLocals, params,
                       allowMathBare)) {
        return false;
      }
      BindingInfo info;
      if (extractExplicitBindingType(bodyArg, info)) {
        lambdaLocals[bodyArg.name] = info;
      } else if (bodyArg.isBinding && bodyArg.args.size() == 1) {
        if (inferBindingTypeForMonomorph(bodyArg.args.front(), params, lambdaLocals, allowMathBare, ctx, info)) {
          lambdaLocals[bodyArg.name] = info;
        }
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
  if (expr.isMethodCall) {
    std::string methodPath;
    if (resolveMethodCallTemplateTarget(expr, locals, ctx, methodPath)) {
      const bool isTemplateDef = ctx.templateDefs.count(methodPath) > 0;
      const bool isKnownDef = ctx.sourceDefs.count(methodPath) > 0;
      if (isTemplateDef) {
        if (expr.templateArgs.empty()) {
          error = "template arguments required for " + methodPath;
          return false;
        }
        if (allConcrete) {
          std::string specializedPath;
          if (!instantiateTemplate(methodPath, expr.templateArgs, ctx, error, specializedPath)) {
            return false;
          }
          expr.name = specializedPath;
          expr.templateArgs.clear();
          expr.isMethodCall = false;
        }
      } else if (isKnownDef && !expr.templateArgs.empty()) {
        error = "template arguments are only supported on templated definitions: " + methodPath;
        return false;
      }
    }
  }

  for (auto &arg : expr.args) {
    if (!rewriteExpr(arg, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, allowMathBare)) {
      return false;
    }
  }
  LocalTypeMap bodyLocals = locals;
  for (auto &arg : expr.bodyArguments) {
    if (!rewriteExpr(arg, mapping, allowedParams, namespacePrefix, ctx, error, bodyLocals, params, allowMathBare)) {
      return false;
    }
    BindingInfo info;
    if (extractExplicitBindingType(arg, info)) {
      bodyLocals[arg.name] = info;
    } else if (arg.isBinding && arg.args.size() == 1) {
      if (inferBindingTypeForMonomorph(arg.args.front(), params, bodyLocals, allowMathBare, ctx, info)) {
        bodyLocals[arg.name] = info;
      }
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
  const bool allowMathBare = hasMathImport(ctx);
  std::vector<ParameterInfo> params;
  params.reserve(def.parameters.size());
  LocalTypeMap locals;
  locals.reserve(def.parameters.size() + def.statements.size());
  for (auto &param : def.parameters) {
    if (!rewriteExpr(param, mapping, allowedParams, def.namespacePrefix, ctx, error, locals, params, allowMathBare)) {
      return false;
    }
    BindingInfo info;
    if (extractExplicitBindingType(param, info)) {
      locals[param.name] = info;
      ParameterInfo paramInfo;
      paramInfo.name = param.name;
      paramInfo.binding = info;
      if (param.args.size() == 1) {
        paramInfo.defaultExpr = &param.args.front();
      }
      params.push_back(std::move(paramInfo));
    } else if (param.isBinding && param.args.size() == 1) {
      if (inferBindingTypeForMonomorph(param.args.front(), {}, {}, allowMathBare, ctx, info)) {
        locals[param.name] = info;
        ParameterInfo paramInfo;
        paramInfo.name = param.name;
        paramInfo.binding = info;
        paramInfo.defaultExpr = &param.args.front();
        params.push_back(std::move(paramInfo));
      }
    }
  }
  for (auto &stmt : def.statements) {
    if (!rewriteExpr(stmt, mapping, allowedParams, def.namespacePrefix, ctx, error, locals, params, allowMathBare)) {
      return false;
    }
    BindingInfo info;
    if (extractExplicitBindingType(stmt, info)) {
      locals[stmt.name] = info;
    } else if (stmt.isBinding && stmt.args.size() == 1) {
      if (inferBindingTypeForMonomorph(stmt.args.front(), params, locals, allowMathBare, ctx, info)) {
        locals[stmt.name] = info;
      }
    }
  }
  if (def.returnExpr.has_value()) {
    if (!rewriteExpr(*def.returnExpr, mapping, allowedParams, def.namespacePrefix, ctx, error, locals, params,
                     allowMathBare)) {
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
  LocalTypeMap emptyLocals;
  const bool allowMathBare = hasMathImport(ctx);
  if (!rewriteExpr(execExpr, SubstMap{}, {}, exec.namespacePrefix, ctx, error, emptyLocals, {}, allowMathBare)) {
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
