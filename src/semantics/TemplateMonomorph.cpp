#include "SemanticsHelpers.h"
#include "primec/Ast.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
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
  std::unordered_set<std::string> implicitTemplateDefs;
  std::unordered_map<std::string, std::vector<std::string>> implicitTemplateParams;
};

using LocalTypeMap = std::unordered_map<std::string, BindingInfo>;

ResolvedType resolveTypeString(const std::string &input,
                               const SubstMap &mapping,
                               const std::unordered_set<std::string> &allowedParams,
                               const std::string &namespacePrefix,
                               Context &ctx,
                               std::string &error);

bool isNonTypeTransformName(const std::string &name) {
  return name == "return" || name == "effects" || name == "capabilities" || name == "mut" || name == "copy" ||
         name == "restrict" || name == "align_bytes" || name == "align_kbytes" || name == "on_error" ||
         name == "struct" || name == "enum" || name == "unsafe" ||
         name == "pod" || name == "handle" || name == "gpu_lane" || name == "no_padding" ||
         name == "platform_independent_padding" || name == "public" || name == "private" ||
         name == "static" || name == "single_type_to_return" || name == "stack" || name == "heap" || name == "buffer" ||
         name == "Additive" || name == "Multiplicative" || name == "Comparable" || name == "Indexable";
}

bool isBuiltinTemplateContainer(const std::string &name) {
  return name == "array" || name == "vector" || name == "soa_vector" || name == "map" || name == "Result" ||
         name == "File" || isBuiltinTemplateTypeName(name);
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
    if (importPath.rfind("/std/math/", 0) == 0 && importPath.size() > 10) {
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

std::string bindingTypeToString(const BindingInfo &info) {
  if (info.typeTemplateArg.empty()) {
    return info.typeName;
  }
  return info.typeName + "<" + info.typeTemplateArg + ">";
}

std::string generateTemplateParamName(const Definition &def, size_t index) {
  std::string candidate = "T" + std::to_string(index);
  auto isUsed = [&](const std::string &name) -> bool {
    for (const auto &existing : def.templateArgs) {
      if (existing == name) {
        return true;
      }
    }
    return false;
  };
  while (isUsed(candidate)) {
    ++index;
    candidate = "T" + std::to_string(index);
  }
  return candidate;
}

bool replaceBindingTypeTransform(Expr &binding, const std::string &typeName, std::string &error) {
  bool replaced = false;
  for (auto &transform : binding.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities" || transform.name == "return") {
      continue;
    }
    if (isBindingAuxTransformName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      error = "binding transforms do not take arguments";
      return false;
    }
    if (!transform.templateArgs.empty()) {
      error = "binding transforms do not take template arguments";
      return false;
    }
    if (replaced) {
      error = "binding requires exactly one type";
      return false;
    }
    transform.name = typeName;
    replaced = true;
  }
  if (!replaced) {
    Transform transform;
    transform.name = typeName;
    binding.transforms.push_back(std::move(transform));
    replaced = true;
  }
  return replaced;
}

bool applyImplicitAutoTemplates(Program &program, Context &ctx, std::string &error) {
  for (auto &def : program.definitions) {
    std::vector<std::string> implicitParams;
    if (!def.templateArgs.empty()) {
      for (const auto &param : def.parameters) {
        if (!param.isBinding) {
          continue;
        }
        BindingInfo info;
        if (!hasExplicitBindingTypeTransform(param)) {
          error = "implicit auto parameters are only supported on non-templated definitions: " + def.fullPath;
          return false;
        }
        if (extractExplicitBindingType(param, info) && info.typeName == "auto") {
          error = "implicit auto parameters are only supported on non-templated definitions: " + def.fullPath;
          return false;
        }
      }
      continue;
    }
    size_t autoIndex = 0;
    for (auto &param : def.parameters) {
      if (!param.isBinding) {
        continue;
      }
      BindingInfo info;
      bool hasExplicit = extractExplicitBindingType(param, info);
      if (!hasExplicit && hasExplicitBindingTypeTransform(param)) {
        continue;
      }
      if (hasExplicit && info.typeName != "auto") {
        continue;
      }
      if (hasExplicit && !info.typeTemplateArg.empty()) {
        error = "auto parameters do not accept template arguments: " + def.fullPath;
        return false;
      }
      std::string templateParam = generateTemplateParamName(def, autoIndex++);
      if (!replaceBindingTypeTransform(param, templateParam, error)) {
        if (error.empty()) {
          error = "auto parameter requires an explicit type transform: " + def.fullPath;
        }
        return false;
      }
      def.templateArgs.push_back(templateParam);
      implicitParams.push_back(templateParam);
    }
    if (!implicitParams.empty()) {
      ctx.implicitTemplateDefs.insert(def.fullPath);
      ctx.implicitTemplateParams.emplace(def.fullPath, std::move(implicitParams));
    }
  }
  return true;
}

std::string resolveCalleePath(const Expr &expr, const std::string &namespacePrefix, const Context &ctx);

bool resolveMethodCallTemplateTarget(const Expr &expr,
                                     const LocalTypeMap &locals,
                                     const Context &ctx,
                                     std::string &pathOut);

bool inferBindingTypeForMonomorph(const Expr &initializer,
                                  const std::vector<ParameterInfo> &params,
                                  const LocalTypeMap &locals,
                                  bool allowMathBare,
                                  Context &ctx,
                                  BindingInfo &infoOut);

std::string preferVectorStdlibHelperPath(const std::string &path,
                                         const std::unordered_map<std::string, Definition> &defs) {
  std::string preferred = path;
  if (preferred.rfind("/array/", 0) == 0 && defs.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/array/").size());
    if (suffix != "count") {
      const std::string vectorAlias = "/vector/" + suffix;
      if (defs.count(vectorAlias) > 0) {
        return vectorAlias;
      }
      const std::string stdlibAlias = "/std/collections/vector/" + suffix;
      if (defs.count(stdlibAlias) > 0) {
        return stdlibAlias;
      }
    }
  }
  if (preferred.rfind("/vector/", 0) == 0 && defs.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/vector/").size());
    const std::string stdlibAlias = "/std/collections/vector/" + suffix;
    if (defs.count(stdlibAlias) > 0) {
      preferred = stdlibAlias;
    } else {
      if (suffix != "count") {
        const std::string arrayAlias = "/array/" + suffix;
        if (defs.count(arrayAlias) > 0) {
          preferred = arrayAlias;
        }
      }
    }
  }
  if (preferred.rfind("/std/collections/vector/", 0) == 0 && defs.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/std/collections/vector/").size());
    const std::string vectorAlias = "/vector/" + suffix;
    if (defs.count(vectorAlias) > 0) {
      preferred = vectorAlias;
    } else {
      if (suffix != "count") {
        const std::string arrayAlias = "/array/" + suffix;
        if (defs.count(arrayAlias) > 0) {
          preferred = arrayAlias;
        }
      }
    }
  }
  if (preferred.rfind("/map/", 0) == 0 && defs.count(preferred) == 0) {
    const std::string stdlibAlias =
        "/std/collections/map/" + preferred.substr(std::string("/map/").size());
    if (defs.count(stdlibAlias) > 0) {
      preferred = stdlibAlias;
    }
  }
  return preferred;
}

std::string preferVectorStdlibTemplatePath(const std::string &path, const Context &ctx) {
  if (path.rfind("/array/", 0) == 0) {
    const std::string suffix = path.substr(std::string("/array/").size());
    if (suffix != "count") {
      const std::string stdlibPath = "/std/collections/vector/" + suffix;
      if (ctx.sourceDefs.count(stdlibPath) > 0 && ctx.templateDefs.count(stdlibPath) > 0) {
        return stdlibPath;
      }
      const std::string vectorPath = "/vector/" + suffix;
      if (ctx.sourceDefs.count(vectorPath) > 0 && ctx.templateDefs.count(vectorPath) > 0) {
        return vectorPath;
      }
    }
    return path;
  }
  if (path.rfind("/std/collections/vector/", 0) == 0) {
    const std::string suffix = path.substr(std::string("/std/collections/vector/").size());
    const std::string vectorPath = "/vector/" + suffix;
    if (ctx.sourceDefs.count(vectorPath) > 0 && ctx.templateDefs.count(vectorPath) > 0) {
      return vectorPath;
    }
    if (suffix != "count") {
      const std::string arrayPath = "/array/" + suffix;
      if (ctx.sourceDefs.count(arrayPath) > 0 && ctx.templateDefs.count(arrayPath) > 0) {
        return arrayPath;
      }
    }
    return path;
  }
  if (path.rfind("/vector/", 0) != 0) {
    if (path.rfind("/map/", 0) == 0) {
      const std::string stdlibPath = "/std/collections/map/" + path.substr(std::string("/map/").size());
      if (ctx.sourceDefs.count(stdlibPath) > 0 && ctx.templateDefs.count(stdlibPath) > 0) {
        return stdlibPath;
      }
    }
    return path;
  }
  const std::string stdlibPath = "/std/collections/vector/" + path.substr(std::string("/vector/").size());
  if (ctx.sourceDefs.count(stdlibPath) > 0 && ctx.templateDefs.count(stdlibPath) > 0) {
    return stdlibPath;
  }
  if (ctx.sourceDefs.count(path) == 0 || ctx.templateDefs.count(path) == 0) {
    const std::string suffix = path.substr(std::string("/vector/").size());
    if (suffix != "count") {
      const std::string arrayPath = "/array/" + suffix;
      if (ctx.sourceDefs.count(arrayPath) > 0 && ctx.templateDefs.count(arrayPath) > 0) {
        return arrayPath;
      }
    }
  }
  return path;
}

bool definitionAcceptsCallShape(const Definition &def, const Expr &expr) {
  std::vector<ParameterInfo> params;
  params.reserve(def.parameters.size());
  for (const auto &paramExpr : def.parameters) {
    ParameterInfo param;
    param.name = paramExpr.name;
    if (paramExpr.args.size() == 1) {
      param.defaultExpr = &paramExpr.args.front();
    }
    params.push_back(std::move(param));
  }
  std::vector<const Expr *> ordered;
  std::string error;
  return buildOrderedArguments(params, expr.args, expr.argNames, ordered, error);
}

bool isSoftwareNumericParamCompatible(ReturnKind expectedKind, ReturnKind actualKind) {
  switch (expectedKind) {
    case ReturnKind::Integer:
      return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 || actualKind == ReturnKind::UInt64 ||
             actualKind == ReturnKind::Bool || actualKind == ReturnKind::Integer;
    case ReturnKind::Decimal:
      return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 || actualKind == ReturnKind::UInt64 ||
             actualKind == ReturnKind::Bool || actualKind == ReturnKind::Float32 || actualKind == ReturnKind::Float64 ||
             actualKind == ReturnKind::Integer || actualKind == ReturnKind::Decimal;
    case ReturnKind::Complex:
      return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 || actualKind == ReturnKind::UInt64 ||
             actualKind == ReturnKind::Bool || actualKind == ReturnKind::Float32 || actualKind == ReturnKind::Float64 ||
             actualKind == ReturnKind::Integer || actualKind == ReturnKind::Decimal ||
             actualKind == ReturnKind::Complex;
    default:
      return false;
  }
}

std::string resolveStructLikeTypePathForTemplatedVectorFallback(const std::string &typeName,
                                                                const std::string &namespacePrefix,
                                                                const Context &ctx) {
  std::string normalized = normalizeBindingTypeName(typeName);
  if (normalized.empty()) {
    return {};
  }
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalized, base, argText) && !base.empty()) {
    normalized = base;
  }
  if (isPrimitiveBindingTypeName(normalized) || isSoftwareNumericTypeName(normalized) || normalized == "string" ||
      isBuiltinTemplateContainer(normalized)) {
    return {};
  }
  if (!normalized.empty() && normalized[0] == '/') {
    return ctx.sourceDefs.count(normalized) > 0 ? normalized : std::string{};
  }
  auto aliasIt = ctx.importAliases.find(normalized);
  if (aliasIt != ctx.importAliases.end() && ctx.sourceDefs.count(aliasIt->second) > 0) {
    return aliasIt->second;
  }
  std::string resolved = resolveTypePath(normalized, namespacePrefix);
  if (ctx.sourceDefs.count(resolved) > 0) {
    return resolved;
  }
  return {};
}

std::string resolveStructLikeExprPathForTemplatedVectorFallback(const Expr &expr,
                                                                const LocalTypeMap &locals,
                                                                const std::string &namespacePrefix,
                                                                const Context &ctx) {
  if (expr.kind != Expr::Kind::Call || expr.isBinding) {
    return {};
  }
  std::string resolved;
  if (expr.isMethodCall) {
    if (!resolveMethodCallTemplateTarget(expr, locals, ctx, resolved)) {
      return {};
    }
  } else {
    resolved = resolveCalleePath(expr, namespacePrefix, ctx);
  }
  const auto defIt = ctx.sourceDefs.find(resolved);
  if (defIt == ctx.sourceDefs.end()) {
    return {};
  }
  if (isStructDefinition(defIt->second)) {
    return resolved;
  }
  for (const auto &transform : defIt->second.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &returnType = transform.templateArgs.front();
    if (returnType == "auto") {
      continue;
    }
    return resolveStructLikeTypePathForTemplatedVectorFallback(returnType, defIt->second.namespacePrefix, ctx);
  }
  return {};
}

std::string inferExprTypeTextForTemplatedVectorFallback(const Expr &expr,
                                                        const LocalTypeMap &locals,
                                                        const std::string &namespacePrefix,
                                                        const Context &ctx) {
  if (expr.kind != Expr::Kind::Call || expr.isBinding) {
    return {};
  }
  std::string builtinCollection;
  if (getBuiltinCollectionName(expr, builtinCollection)) {
    if ((builtinCollection == "array" || builtinCollection == "vector" || builtinCollection == "soa_vector") &&
        expr.templateArgs.size() == 1) {
      return builtinCollection + "<" + expr.templateArgs.front() + ">";
    }
    if (builtinCollection == "map" && expr.templateArgs.size() == 2) {
      return "map<" + expr.templateArgs.front() + ", " + expr.templateArgs[1] + ">";
    }
  }
  std::string resolved;
  if (expr.isMethodCall) {
    if (!resolveMethodCallTemplateTarget(expr, locals, ctx, resolved)) {
      return {};
    }
  } else {
    resolved = resolveCalleePath(expr, namespacePrefix, ctx);
  }
  const auto defIt = ctx.sourceDefs.find(resolved);
  if (defIt == ctx.sourceDefs.end()) {
    return {};
  }
  if (isStructDefinition(defIt->second)) {
    return resolved;
  }
  for (const auto &transform : defIt->second.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &returnType = transform.templateArgs.front();
    if (returnType == "auto") {
      continue;
    }
    return returnType;
  }
  return {};
}

bool shouldPreferTemplatedVectorFallbackForTypeMismatch(const Definition &def,
                                                        const Expr &expr,
                                                        const LocalTypeMap &locals,
                                                        const std::vector<ParameterInfo> &params,
                                                        bool allowMathBare,
                                                        Context &ctx,
                                                        const std::string &namespacePrefix) {
  auto isTemplateParamName = [&](const std::string &name) {
    for (const auto &templateArg : def.templateArgs) {
      if (templateArg == name) {
        return true;
      }
    }
    return false;
  };
  auto isCollectionEnvelopeBase = [&](const std::string &base) {
    return base == "array" || base == "vector" || base == "map" || base == "soa_vector";
  };
  auto hasUnknownEnvelopeMismatch = [&](const std::string &normalizedExpected,
                                        const std::string &normalizedActual) {
    if (normalizedExpected == normalizedActual) {
      return false;
    }
    std::string expectedBase;
    std::string expectedArgText;
    std::string actualBase;
    std::string actualArgText;
    const bool expectedIsTemplate = splitTemplateTypeName(normalizedExpected, expectedBase, expectedArgText);
    const bool actualIsTemplate = splitTemplateTypeName(normalizedActual, actualBase, actualArgText);
    if (expectedIsTemplate && actualIsTemplate) {
      const std::string normalizedExpectedBase = normalizeBindingTypeName(expectedBase);
      const std::string normalizedActualBase = normalizeBindingTypeName(actualBase);
      if (normalizedExpectedBase == normalizedActualBase) {
        return true;
      }
      return isCollectionEnvelopeBase(normalizedExpectedBase) || isCollectionEnvelopeBase(normalizedActualBase);
    }
    if (expectedIsTemplate == actualIsTemplate) {
      return false;
    }
    const std::string &nonTemplateText = expectedIsTemplate ? normalizedActual : normalizedExpected;
    if (isTemplateParamName(nonTemplateText)) {
      return false;
    }
    const std::string templateBase = normalizeBindingTypeName(expectedIsTemplate ? expectedBase : actualBase);
    return isCollectionEnvelopeBase(templateBase);
  };
  std::vector<ParameterInfo> callParams;
  callParams.reserve(def.parameters.size());
  for (const auto &paramExpr : def.parameters) {
    ParameterInfo param;
    param.name = paramExpr.name;
    extractExplicitBindingType(paramExpr, param.binding);
    if (paramExpr.args.size() == 1) {
      param.defaultExpr = &paramExpr.args.front();
    }
    callParams.push_back(std::move(param));
  }
  std::vector<const Expr *> ordered;
  std::string orderError;
  if (!buildOrderedArguments(callParams, expr.args, expr.argNames, ordered, orderError)) {
    return false;
  }
  std::unordered_set<const Expr *> explicitArgs;
  explicitArgs.reserve(expr.args.size());
  for (const auto &arg : expr.args) {
    explicitArgs.insert(&arg);
  }
  for (size_t i = 0; i < callParams.size(); ++i) {
    const auto &param = callParams[i];
    if (param.binding.typeName.empty() || !ordered[i]) {
      continue;
    }
    if (explicitArgs.count(ordered[i]) == 0) {
      continue;
    }
    BindingInfo actual;
    if (!inferBindingTypeForMonomorph(*ordered[i], params, locals, allowMathBare, ctx, actual)) {
      const std::string expectedTypeText = bindingTypeToString(param.binding);
      const std::string inferredActualTypeText =
          inferExprTypeTextForTemplatedVectorFallback(*ordered[i], locals, namespacePrefix, ctx);
      if (!expectedTypeText.empty() && !inferredActualTypeText.empty()) {
        const std::string normalizedExpected = normalizeBindingTypeName(expectedTypeText);
        const std::string normalizedActual = normalizeBindingTypeName(inferredActualTypeText);
        if (normalizedExpected == "string" && normalizedActual != "string") {
          return true;
        }
        if (normalizedExpected != "string" && normalizedActual == "string") {
          return true;
        }
        const ReturnKind expectedKind = returnKindForTypeName(normalizedExpected);
        const ReturnKind actualKind = returnKindForTypeName(normalizedActual);
        if (expectedKind != ReturnKind::Unknown && actualKind != ReturnKind::Unknown) {
          if (!isSoftwareNumericParamCompatible(expectedKind, actualKind)) {
            if (expectedKind == actualKind && expectedKind == ReturnKind::Array &&
                normalizedExpected != normalizedActual) {
              return true;
            }
            if (expectedKind != actualKind) {
              return true;
            }
          }
        } else if (expectedKind != actualKind) {
          if (normalizedExpected != normalizedActual) {
            return true;
          }
        } else if (hasUnknownEnvelopeMismatch(normalizedExpected, normalizedActual)) {
          return true;
        }
      }
      const std::string expectedStructPath =
          resolveStructLikeTypePathForTemplatedVectorFallback(param.binding.typeName, def.namespacePrefix, ctx);
      if (expectedStructPath.empty()) {
        continue;
      }
      const std::string actualStructPath =
          resolveStructLikeExprPathForTemplatedVectorFallback(*ordered[i], locals, namespacePrefix, ctx);
      if (!actualStructPath.empty() && actualStructPath != expectedStructPath) {
        return true;
      }
      continue;
    }
    const std::string expectedTypeText = bindingTypeToString(param.binding);
    const std::string actualTypeText = bindingTypeToString(actual);
    const std::string normalizedExpected = normalizeBindingTypeName(expectedTypeText);
    const std::string normalizedActual = normalizeBindingTypeName(actualTypeText);
    if (normalizedExpected == "string" && normalizedActual != "string") {
      return true;
    }
    if (normalizedExpected != "string" && normalizedActual == "string") {
      return true;
    }
    const ReturnKind expectedKind = returnKindForTypeName(normalizedExpected);
    const ReturnKind actualKind = returnKindForTypeName(normalizedActual);
    if (expectedKind == ReturnKind::Unknown || actualKind == ReturnKind::Unknown) {
      if (expectedKind == ReturnKind::Unknown && actualKind == ReturnKind::Unknown) {
        const std::string expectedStructPath =
            resolveStructLikeTypePathForTemplatedVectorFallback(param.binding.typeName, def.namespacePrefix, ctx);
        const std::string actualStructPath =
            resolveStructLikeTypePathForTemplatedVectorFallback(actual.typeName, namespacePrefix, ctx);
        if (!expectedStructPath.empty() && !actualStructPath.empty() && expectedStructPath != actualStructPath) {
          return true;
        }
        if (hasUnknownEnvelopeMismatch(normalizedExpected, normalizedActual)) {
          return true;
        }
      } else if (expectedKind != actualKind) {
        if (normalizedExpected != normalizedActual) {
          return true;
        }
      }
      continue;
    }
    if (isSoftwareNumericParamCompatible(expectedKind, actualKind)) {
      continue;
    }
    if (expectedKind == actualKind && expectedKind == ReturnKind::Array && normalizedExpected != normalizedActual) {
      return true;
    }
    if (actualKind != expectedKind) {
      return true;
    }
  }
  return false;
}

std::string preferVectorStdlibImplicitTemplatePath(const Expr &expr,
                                                   const std::string &path,
                                                   const LocalTypeMap &locals,
                                                   const std::vector<ParameterInfo> &params,
                                                   bool allowMathBare,
                                                   Context &ctx,
                                                   const std::string &namespacePrefix) {
  if (!expr.templateArgs.empty()) {
    return path;
  }
  const auto defIt = ctx.sourceDefs.find(path);
  if (defIt == ctx.sourceDefs.end() || ctx.templateDefs.count(path) > 0) {
    return path;
  }
  const std::string preferred = preferVectorStdlibTemplatePath(path, ctx);
  if (definitionAcceptsCallShape(defIt->second, expr) &&
      !shouldPreferTemplatedVectorFallbackForTypeMismatch(
          defIt->second, expr, locals, params, allowMathBare, ctx, namespacePrefix)) {
    return path;
  }
  if (preferred != path && ctx.sourceDefs.count(preferred) > 0 && ctx.templateDefs.count(preferred) > 0) {
    return preferred;
  }
  return path;
}

bool resolveMethodCallTemplateTarget(const Expr &expr,
                                     const LocalTypeMap &locals,
                                     const Context &ctx,
                                     std::string &pathOut) {
  pathOut.clear();
  if (!expr.isMethodCall || expr.args.empty() || expr.name.empty()) {
    return false;
  }
  std::string methodName = expr.name;
  if (!methodName.empty() && methodName.front() == '/') {
    methodName.erase(methodName.begin());
  }
  auto normalizeCollectionMethodName = [](const std::string &receiverTypeName,
                                          std::string candidate) -> std::string {
    if (receiverTypeName == "array" || receiverTypeName == "vector" || receiverTypeName == "soa_vector") {
      const std::string vectorPrefix = "vector/";
      const std::string arrayPrefix = "array/";
      const std::string stdVectorPrefix = "std/collections/vector/";
      if (candidate.rfind(vectorPrefix, 0) == 0) {
        return candidate.substr(vectorPrefix.size());
      }
      if (candidate.rfind(arrayPrefix, 0) == 0) {
        return candidate.substr(arrayPrefix.size());
      }
      if (candidate.rfind(stdVectorPrefix, 0) == 0) {
        return candidate.substr(stdVectorPrefix.size());
      }
    }
    if (receiverTypeName == "map") {
      const std::string mapPrefix = "map/";
      const std::string stdMapPrefix = "std/collections/map/";
      if (candidate.rfind(mapPrefix, 0) == 0) {
        return candidate.substr(mapPrefix.size());
      }
      if (candidate.rfind(stdMapPrefix, 0) == 0) {
        return candidate.substr(stdMapPrefix.size());
      }
    }
    return candidate;
  };
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
    if (!receiver.isBinding) {
      std::string resolved;
      if (receiver.isMethodCall) {
        if (!resolveMethodCallTemplateTarget(receiver, locals, ctx, resolved)) {
          resolved.clear();
        }
      } else {
        resolved = resolveCalleePath(receiver, receiver.namespacePrefix, ctx);
      }
      auto defIt = ctx.sourceDefs.find(resolved);
      if (defIt != ctx.sourceDefs.end()) {
        if (isStructDefinition(defIt->second)) {
          pathOut = resolved + "/" + methodName;
          return true;
        }
        for (const auto &transform : defIt->second.transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          const std::string &returnType = transform.templateArgs.front();
          if (returnType == "auto") {
            continue;
          }
          typeName = returnType;
          break;
        }
      } else {
        std::string collection;
        if (getBuiltinCollectionName(receiver, collection)) {
          typeName = collection;
        }
      }
    }
  }
  if (typeName.empty()) {
    return false;
  }
  std::string typeBase;
  std::string typeArgText;
  if (splitTemplateTypeName(typeName, typeBase, typeArgText) && !typeBase.empty()) {
    typeName = typeBase;
  }
  const std::string normalizedMethodName = normalizeCollectionMethodName(typeName, methodName);
  if (isPrimitiveBindingTypeName(typeName)) {
    pathOut = "/" + normalizeBindingTypeName(typeName) + "/" + normalizedMethodName;
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
    if (typeName == "array" || typeName == "vector" || typeName == "map" || typeName == "soa_vector") {
      pathOut = "/" + typeName + "/" + normalizedMethodName;
      pathOut = preferVectorStdlibHelperPath(pathOut, ctx.sourceDefs);
      return true;
    }
    if (typeName == "string") {
      pathOut = "/string/" + normalizedMethodName;
      return true;
    }
    return false;
  }
  pathOut = resolvedType + "/" + normalizedMethodName;
  pathOut = preferVectorStdlibHelperPath(pathOut, ctx.sourceDefs);
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

bool inferImplicitTemplateArgs(const Definition &def,
                               const Expr &callExpr,
                               const LocalTypeMap &locals,
                               const std::vector<ParameterInfo> &params,
                               const SubstMap &mapping,
                               const std::unordered_set<std::string> &allowedParams,
                               const std::string &namespacePrefix,
                               Context &ctx,
                               bool allowMathBare,
                               std::vector<std::string> &outArgs,
                               std::string &error) {
  if (def.templateArgs.empty()) {
    return false;
  }
  std::unordered_set<std::string> implicitSet;
  auto implicitIt = ctx.implicitTemplateParams.find(def.fullPath);
  if (implicitIt != ctx.implicitTemplateParams.end()) {
    const auto &implicitParams = implicitIt->second;
    implicitSet.insert(implicitParams.begin(), implicitParams.end());
  } else {
    implicitSet.insert(def.templateArgs.begin(), def.templateArgs.end());
  }
  std::unordered_map<std::string, std::string> inferred;
  inferred.reserve(def.templateArgs.size());
  std::vector<const Expr *> orderedArgs(def.parameters.size(), nullptr);
  size_t callArgStart = 0;
  if (callExpr.isMethodCall && callExpr.args.size() == def.parameters.size() + 1) {
    // Method-call sugar prepends the receiver expression.
    callArgStart = 1;
  }
  size_t positionalIndex = 0;
  for (size_t i = callArgStart; i < callExpr.args.size(); ++i) {
    if (i < callExpr.argNames.size() && callExpr.argNames[i].has_value()) {
      const std::string &name = *callExpr.argNames[i];
      size_t index = def.parameters.size();
      for (size_t p = 0; p < def.parameters.size(); ++p) {
        if (def.parameters[p].name == name) {
          index = p;
          break;
        }
      }
      if (index >= def.parameters.size()) {
        error = "unknown named argument: " + name;
        return false;
      }
      if (orderedArgs[index] != nullptr) {
        error = "named argument duplicates parameter: " + name;
        return false;
      }
      orderedArgs[index] = &callExpr.args[i];
      continue;
    }
    while (positionalIndex < orderedArgs.size() && orderedArgs[positionalIndex] != nullptr) {
      ++positionalIndex;
    }
    if (positionalIndex >= orderedArgs.size()) {
      error = "argument count mismatch";
      return false;
    }
    orderedArgs[positionalIndex] = &callExpr.args[i];
    ++positionalIndex;
  }

  for (size_t i = 0; i < def.parameters.size(); ++i) {
    const Expr &param = def.parameters[i];
    BindingInfo paramInfo;
    if (!extractExplicitBindingType(param, paramInfo)) {
      continue;
    }
    if (implicitSet.count(paramInfo.typeName) == 0) {
      continue;
    }
    const Expr *argExpr = i < orderedArgs.size() ? orderedArgs[i] : nullptr;
    if (!argExpr && param.args.size() == 1) {
      argExpr = &param.args.front();
    }
    if (!argExpr) {
      error = "implicit template arguments require values on " + def.fullPath;
      return false;
    }
    BindingInfo argInfo;
    if (!inferBindingTypeForMonomorph(*argExpr, params, locals, allowMathBare, ctx, argInfo)) {
      error = "unable to infer implicit template arguments for " + def.fullPath;
      return false;
    }
    std::string argType = bindingTypeToString(argInfo);
    if (argType.empty()) {
      error = "unable to infer implicit template arguments for " + def.fullPath;
      return false;
    }
    ResolvedType resolvedArg = resolveTypeString(argType, mapping, allowedParams, namespacePrefix, ctx, error);
    if (!error.empty()) {
      return false;
    }
    if (!resolvedArg.concrete) {
      error = "implicit template arguments must be concrete on " + def.fullPath;
      return false;
    }
    auto it = inferred.find(paramInfo.typeName);
    if (it != inferred.end() && it->second != resolvedArg.text) {
      error = "implicit template arguments conflict on " + def.fullPath;
      return false;
    }
    inferred[paramInfo.typeName] = resolvedArg.text;
  }

  outArgs.clear();
  outArgs.reserve(def.templateArgs.size());
  for (const auto &paramName : def.templateArgs) {
    auto it = inferred.find(paramName);
    if (it == inferred.end()) {
      // Leave error empty so specialized fallback inference can run (for example,
      // stdlib collection helpers that infer T from vector<T> receiver shape).
      return false;
    }
    outArgs.push_back(it->second);
  }
  return true;
}

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
      if (transform.templateArgs.empty()) {
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
      } else {
        for (auto &arg : transform.templateArgs) {
          ResolvedType resolvedArg = resolveTypeString(arg, mapping, allowedParams, namespacePrefix, ctx, error);
          if (!error.empty()) {
            return false;
          }
          arg = resolvedArg.text;
        }

        const std::string resolvedPath =
            resolveNameToPath(transform.name, namespacePrefix, ctx.importAliases, ctx.sourceDefs);
        const bool canResolveTemplatedName =
            isBuiltinTemplateContainer(transform.name) || ctx.templateDefs.count(resolvedPath) > 0;
        if (canResolveTemplatedName) {
          std::string templatedName = transform.name + "<" + joinTemplateArgs(transform.templateArgs) + ">";
          ResolvedType resolvedName = resolveTypeString(templatedName, mapping, allowedParams, namespacePrefix, ctx, error);
          if (!error.empty()) {
            return false;
          }

          std::string base;
          std::string argText;
          if (splitTemplateTypeName(resolvedName.text, base, argText)) {
            std::vector<std::string> args;
            if (!splitTopLevelTemplateArgs(argText, args)) {
              error = "invalid template arguments for " + resolvedName.text;
              return false;
            }
            transform.name = base;
            transform.templateArgs = std::move(args);
          } else {
            transform.name = resolvedName.text;
            transform.templateArgs.clear();
          }
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
    const size_t lastSlash = namespacePrefix.find_last_of('/');
    const std::string_view suffix = lastSlash == std::string::npos
                                        ? std::string_view(namespacePrefix)
                                        : std::string_view(namespacePrefix).substr(lastSlash + 1);
    if (suffix == expr.name && ctx.sourceDefs.count(namespacePrefix) > 0) {
      return namespacePrefix;
    }
    std::string prefix = namespacePrefix;
    while (!prefix.empty()) {
      std::string candidate = prefix + "/" + expr.name;
      if (ctx.sourceDefs.count(candidate) > 0) {
        return candidate;
      }
      const size_t slash = prefix.find_last_of('/');
      if (slash == std::string::npos) {
        break;
      }
      prefix = prefix.substr(0, slash);
    }
    auto aliasIt = ctx.importAliases.find(expr.name);
    if (aliasIt != ctx.importAliases.end()) {
      return aliasIt->second;
    }
    return namespacePrefix + "/" + expr.name;
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

bool inferStdlibVectorHelperTemplateArgs(const Definition &def,
                                         const Expr &callExpr,
                                         const LocalTypeMap &locals,
                                         const std::vector<ParameterInfo> &params,
                                         const SubstMap &mapping,
                                         const std::unordered_set<std::string> &allowedParams,
                                         const std::string &namespacePrefix,
                                         Context &ctx,
                                         bool allowMathBare,
                                         std::vector<std::string> &outArgs) {
  if (def.fullPath.rfind("/std/collections/vector/", 0) != 0) {
    return false;
  }
  const std::string helperSuffix = def.fullPath.substr(std::string("/std/collections/vector/").size());
  const std::string vectorAliasPath = "/vector/" + helperSuffix;
  const std::string arrayAliasPath = "/array/" + helperSuffix;
  if (ctx.sourceDefs.count(vectorAliasPath) == 0 && ctx.sourceDefs.count(arrayAliasPath) == 0) {
    return false;
  }
  if (def.templateArgs.size() != 1 || def.parameters.empty()) {
    return false;
  }
  BindingInfo receiverParamInfo;
  if (!extractExplicitBindingType(def.parameters.front(), receiverParamInfo)) {
    return false;
  }
  const std::string normalizedReceiverParamType = normalizeBindingTypeName(receiverParamInfo.typeName);
  if (normalizedReceiverParamType != "vector" && normalizedReceiverParamType != "soa_vector") {
    return false;
  }
  std::vector<std::string> receiverParamTemplateArgs;
  if (!splitTopLevelTemplateArgs(receiverParamInfo.typeTemplateArg, receiverParamTemplateArgs) ||
      receiverParamTemplateArgs.size() != 1 || trimWhitespace(receiverParamTemplateArgs.front()) != def.templateArgs.front()) {
    return false;
  }

  std::vector<ParameterInfo> callParams;
  callParams.reserve(def.parameters.size());
  for (const auto &paramExpr : def.parameters) {
    ParameterInfo paramInfo;
    paramInfo.name = paramExpr.name;
    if (paramExpr.args.size() == 1) {
      paramInfo.defaultExpr = &paramExpr.args.front();
    }
    callParams.push_back(std::move(paramInfo));
  }
  std::vector<const Expr *> orderedArgs;
  std::string orderError;
  if (!buildOrderedArguments(callParams, callExpr.args, callExpr.argNames, orderedArgs, orderError)) {
    return false;
  }
  if (orderedArgs.empty() || orderedArgs.front() == nullptr) {
    return false;
  }

  BindingInfo receiverArgInfo;
  if (!inferBindingTypeForMonomorph(*orderedArgs.front(), params, locals, allowMathBare, ctx, receiverArgInfo)) {
    return false;
  }
  std::string receiverArgType = normalizeBindingTypeName(receiverArgInfo.typeName);
  std::string receiverArgTemplateArg = receiverArgInfo.typeTemplateArg;
  if ((receiverArgType == "Reference" || receiverArgType == "Pointer") && !receiverArgTemplateArg.empty()) {
    std::string pointeeBase;
    std::string pointeeArgText;
    if (splitTemplateTypeName(receiverArgTemplateArg, pointeeBase, pointeeArgText)) {
      receiverArgType = normalizeBindingTypeName(pointeeBase);
      receiverArgTemplateArg = pointeeArgText;
    }
  }
  if (receiverArgType != normalizedReceiverParamType) {
    return false;
  }

  std::vector<std::string> receiverArgTemplateArgs;
  if (!splitTopLevelTemplateArgs(receiverArgTemplateArg, receiverArgTemplateArgs) ||
      receiverArgTemplateArgs.size() != 1) {
    return false;
  }
  std::string resolvedError;
  ResolvedType resolvedArg = resolveTypeString(
      receiverArgTemplateArgs.front(), mapping, allowedParams, namespacePrefix, ctx, resolvedError);
  if (!resolvedError.empty() || !resolvedArg.concrete || resolvedArg.text.empty()) {
    return false;
  }
  outArgs.clear();
  outArgs.push_back(resolvedArg.text);
  return true;
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

  if (expr.templateArgs.empty()) {
    std::string explicitBase;
    std::string explicitArgText;
    if (splitTemplateTypeName(expr.name, explicitBase, explicitArgText)) {
      std::vector<std::string> explicitArgs;
      if (!splitTopLevelTemplateArgs(explicitArgText, explicitArgs)) {
        error = "invalid template arguments for " + expr.name;
        return false;
      }
      expr.name = explicitBase;
      expr.templateArgs = std::move(explicitArgs);
    }
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
    const std::string preferredPath = preferVectorStdlibHelperPath(resolvedPath, ctx.sourceDefs);
    if (preferredPath != resolvedPath && ctx.sourceDefs.count(preferredPath) > 0) {
      resolvedPath = preferredPath;
      expr.name = preferredPath;
    }
    const bool resolvedWasTemplate = ctx.templateDefs.count(resolvedPath) > 0;
    if (!expr.templateArgs.empty() && !resolvedWasTemplate) {
      const std::string templatePreferredPath = preferVectorStdlibTemplatePath(resolvedPath, ctx);
      if (templatePreferredPath != resolvedPath) {
        resolvedPath = templatePreferredPath;
        expr.name = templatePreferredPath;
      }
    }
    if (expr.templateArgs.empty()) {
      const std::string implicitTemplatePreferredPath =
          preferVectorStdlibImplicitTemplatePath(expr, resolvedPath, locals, params, allowMathBare, ctx, namespacePrefix);
      if (implicitTemplatePreferredPath != resolvedPath) {
        resolvedPath = implicitTemplatePreferredPath;
        expr.name = implicitTemplatePreferredPath;
      }
    }
    std::string builtinCollectionName;
    const bool builtinCollectionCall = getBuiltinCollectionName(expr, builtinCollectionName);
    const bool isTemplateDef = ctx.templateDefs.count(resolvedPath) > 0;
    const bool isKnownDef = ctx.sourceDefs.count(resolvedPath) > 0;
    if (isTemplateDef) {
      if (expr.templateArgs.empty()) {
        auto defIt = ctx.sourceDefs.find(resolvedPath);
        if (defIt != ctx.sourceDefs.end()) {
          std::vector<std::string> inferredArgs;
          if (inferImplicitTemplateArgs(defIt->second,
                                        expr,
                                        locals,
                                        params,
                                        mapping,
                                        allowedParams,
                                        namespacePrefix,
                                        ctx,
                                        allowMathBare,
                                        inferredArgs,
                                        error)) {
            expr.templateArgs = std::move(inferredArgs);
            allConcrete = true;
          } else if (error.empty() &&
                     inferStdlibVectorHelperTemplateArgs(defIt->second,
                                                         expr,
                                                         locals,
                                                         params,
                                                         mapping,
                                                         allowedParams,
                                                         namespacePrefix,
                                                         ctx,
                                                         allowMathBare,
                                                         inferredArgs)) {
            expr.templateArgs = std::move(inferredArgs);
            allConcrete = true;
          } else if (!error.empty()) {
            return false;
          }
        }
      }
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
    } else if (isKnownDef && !expr.templateArgs.empty() && !builtinCollectionCall) {
      error = "template arguments are only supported on templated definitions: " + resolvedPath;
      return false;
    }
  }
  if (expr.isMethodCall) {
    std::string methodPath;
    if (resolveMethodCallTemplateTarget(expr, locals, ctx, methodPath)) {
      const bool methodWasTemplate = ctx.templateDefs.count(methodPath) > 0;
      if (!expr.templateArgs.empty() && !methodWasTemplate) {
        methodPath = preferVectorStdlibTemplatePath(methodPath, ctx);
      }
      if (expr.templateArgs.empty()) {
        methodPath =
            preferVectorStdlibImplicitTemplatePath(expr, methodPath, locals, params, allowMathBare, ctx, namespacePrefix);
      }
      const bool isTemplateDef = ctx.templateDefs.count(methodPath) > 0;
      const bool isKnownDef = ctx.sourceDefs.count(methodPath) > 0;
      if (isTemplateDef) {
        if (expr.templateArgs.empty()) {
          auto defIt = ctx.sourceDefs.find(methodPath);
          if (defIt != ctx.sourceDefs.end()) {
            std::vector<std::string> inferredArgs;
            if (inferImplicitTemplateArgs(defIt->second,
                                          expr,
                                          locals,
                                          params,
                                          mapping,
                                          allowedParams,
                                          namespacePrefix,
                                          ctx,
                                          allowMathBare,
                                          inferredArgs,
                                          error)) {
              expr.templateArgs = std::move(inferredArgs);
              allConcrete = true;
            } else if (error.empty() &&
                       inferStdlibVectorHelperTemplateArgs(defIt->second,
                                                           expr,
                                                           locals,
                                                           params,
                                                           mapping,
                                                           allowedParams,
                                                           namespacePrefix,
                                                           ctx,
                                                           allowMathBare,
                                                           inferredArgs)) {
              expr.templateArgs = std::move(inferredArgs);
              allConcrete = true;
            } else if (!error.empty()) {
              return false;
            }
          }
        }
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
  Context ctx{program, {}, {}, {}, {}, {}, {}, {}, {}, {}};
  ctx.sourceDefs.clear();
  ctx.templateDefs.clear();
  ctx.outputDefs.clear();
  ctx.outputExecs.clear();
  ctx.outputPaths.clear();
  ctx.implicitTemplateDefs.clear();
  ctx.implicitTemplateParams.clear();

  if (!applyImplicitAutoTemplates(program, ctx, error)) {
    return false;
  }

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
