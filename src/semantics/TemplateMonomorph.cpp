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

struct HelperOverloadEntry {
  std::string internalPath;
  size_t parameterCount = 0;
};

struct Context {
  Program &program;
  std::unordered_map<std::string, Definition> sourceDefs;
  std::unordered_set<std::string> templateDefs;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::vector<HelperOverloadEntry>> helperOverloads;
  std::unordered_map<std::string, std::string> helperOverloadInternalToPublic;
  std::unordered_map<std::string, std::string> specializationCache;
  std::unordered_set<std::string> outputPaths;
  std::vector<Definition> outputDefs;
  std::vector<Execution> outputExecs;
  std::unordered_set<std::string> implicitTemplateDefs;
  std::unordered_map<std::string, std::vector<std::string>> implicitTemplateParams;
  std::unordered_set<std::string> returnInferenceStack;
};

using LocalTypeMap = std::unordered_map<std::string, BindingInfo>;

ResolvedType resolveTypeString(std::string input,
                               const SubstMap &mapping,
                               const std::unordered_set<std::string> &allowedParams,
                               const std::string &namespacePrefix,
                               Context &ctx,
                               std::string &error);

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
                               std::string &error);

bool resolvesExperimentalMapValueTypeText(const std::string &typeText,
                                          const SubstMap &mapping,
                                          const std::unordered_set<std::string> &allowedParams,
                                          const std::string &namespacePrefix,
                                          Context &ctx);

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

std::string normalizeBuiltinCollectionTemplateBase(const std::string &name) {
  if (name == "array" || name == "/array") {
    return "array";
  }
  if (name == "vector" || name == "/vector" || name == "std/collections/vector" ||
      name == "/std/collections/vector") {
    return "vector";
  }
  if (name == "soa_vector" || name == "/soa_vector") {
    return "soa_vector";
  }
  if (name == "map" || name == "/map" || name == "std/collections/map" || name == "/std/collections/map") {
    return "map";
  }
  return {};
}

bool isBuiltinCollectionTemplateBase(const std::string &name, size_t argumentCount) {
  const std::string normalized = normalizeBuiltinCollectionTemplateBase(name);
  if (normalized.empty()) {
    return false;
  }
  if ((normalized == "array" || normalized == "vector" || normalized == "soa_vector") && argumentCount == 1) {
    return true;
  }
  return normalized == "map" && argumentCount == 2;
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

std::string helperOverloadInternalPath(const std::string &publicPath, size_t parameterCount) {
  return publicPath + "__ov" + std::to_string(parameterCount);
}

std::string helperOverloadDisplayPath(const std::string &path, const Context &ctx) {
  auto directIt = ctx.helperOverloadInternalToPublic.find(path);
  if (directIt != ctx.helperOverloadInternalToPublic.end()) {
    return directIt->second;
  }
  for (const auto &[internalPath, publicPath] : ctx.helperOverloadInternalToPublic) {
    if (path.rfind(internalPath + "__t", 0) == 0) {
      return publicPath;
    }
  }
  return path;
}

std::string selectHelperOverloadPath(const Expr &expr, const std::string &resolvedPath, const Context &ctx) {
  auto familyIt = ctx.helperOverloads.find(resolvedPath);
  if (familyIt == ctx.helperOverloads.end()) {
    return resolvedPath;
  }
  const size_t argumentCount = expr.args.size();
  for (const auto &entry : familyIt->second) {
    if (entry.parameterCount == argumentCount) {
      return entry.internalPath;
    }
  }
  return resolvedPath;
}

bool resolveHelperOverloadDefinitionIdentity(const Definition &def,
                                             const Context &ctx,
                                             std::string &internalPathOut,
                                             std::string &nameOut) {
  internalPathOut = def.fullPath;
  nameOut = def.name;
  auto familyIt = ctx.helperOverloads.find(def.fullPath);
  if (familyIt == ctx.helperOverloads.end()) {
    return false;
  }
  const size_t parameterCount = def.parameters.size();
  for (const auto &entry : familyIt->second) {
    if (entry.parameterCount != parameterCount) {
      continue;
    }
    internalPathOut = entry.internalPath;
    const size_t slash = internalPathOut.find_last_of('/');
    nameOut = slash == std::string::npos ? internalPathOut : internalPathOut.substr(slash + 1);
    return true;
  }
  return false;
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

std::string experimentalMapConstructorInferencePath(const std::string &resolvedPath) {
  auto matchesPath = [&](std::string_view basePath) {
    return resolvedPath == basePath || resolvedPath.rfind(std::string(basePath) + "__t", 0) == 0;
  };
  if (matchesPath("/std/collections/map/map") || matchesPath("/std/collections/mapNew")) {
    return "/std/collections/experimental_map/mapNew";
  }
  if (matchesPath("/std/collections/mapSingle")) {
    return "/std/collections/experimental_map/mapSingle";
  }
  if (matchesPath("/std/collections/mapDouble")) {
    return "/std/collections/experimental_map/mapDouble";
  }
  if (matchesPath("/std/collections/mapPair")) {
    return "/std/collections/experimental_map/mapPair";
  }
  if (matchesPath("/std/collections/mapTriple")) {
    return "/std/collections/experimental_map/mapTriple";
  }
  if (matchesPath("/std/collections/mapQuad")) {
    return "/std/collections/experimental_map/mapQuad";
  }
  if (matchesPath("/std/collections/mapQuint")) {
    return "/std/collections/experimental_map/mapQuint";
  }
  if (matchesPath("/std/collections/mapSext")) {
    return "/std/collections/experimental_map/mapSext";
  }
  if (matchesPath("/std/collections/mapSept")) {
    return "/std/collections/experimental_map/mapSept";
  }
  if (matchesPath("/std/collections/mapOct")) {
    return "/std/collections/experimental_map/mapOct";
  }
  return {};
}

bool isExperimentalMapConstructorHelperPath(const std::string &resolvedPath) {
  auto matchesPath = [&](std::string_view basePath) {
    return resolvedPath == basePath || resolvedPath.rfind(std::string(basePath) + "__t", 0) == 0;
  };
  return matchesPath("/std/collections/experimental_map/mapNew") ||
         matchesPath("/std/collections/experimental_map/mapSingle") ||
         matchesPath("/std/collections/experimental_map/mapDouble") ||
         matchesPath("/std/collections/experimental_map/mapPair") ||
         matchesPath("/std/collections/experimental_map/mapTriple") ||
         matchesPath("/std/collections/experimental_map/mapQuad") ||
         matchesPath("/std/collections/experimental_map/mapQuint") ||
         matchesPath("/std/collections/experimental_map/mapSext") ||
         matchesPath("/std/collections/experimental_map/mapSept") ||
         matchesPath("/std/collections/experimental_map/mapOct");
}

bool resolvesExperimentalVectorValueTypeText(const std::string &typeText) {
  std::string normalizedType = normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalizedType, base, argText)) {
    std::string normalizedBase = normalizeBindingTypeName(base);
    if (!normalizedBase.empty() && normalizedBase.front() == '/') {
      normalizedBase.erase(normalizedBase.begin());
    }
    if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
      return false;
    }
    if ((normalizedBase == "Vector" ||
         normalizedBase == "std/collections/experimental_vector/Vector") &&
        !argText.empty()) {
      std::vector<std::string> args;
      return splitTopLevelTemplateArgs(argText, args) && args.size() == 1;
    }
  }
  std::string resolvedPath = normalizedType;
  if (!resolvedPath.empty() && resolvedPath.front() != '/') {
    resolvedPath.insert(resolvedPath.begin(), '/');
  }
  std::string normalizedResolvedPath = normalizeBindingTypeName(resolvedPath);
  if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
    normalizedResolvedPath.erase(normalizedResolvedPath.begin());
  }
  return normalizedResolvedPath.rfind("std/collections/experimental_vector/Vector__", 0) == 0;
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

bool isRemovedVectorCompatibilityHelper(const std::string &helperName) {
  return helperName == "count" || helperName == "capacity" || helperName == "at" || helperName == "at_unsafe" ||
         helperName == "push" || helperName == "pop" || helperName == "reserve" || helperName == "clear" ||
         helperName == "remove_at" || helperName == "remove_swap";
}

bool isRemovedMapCompatibilityHelper(std::string_view helperName) {
  return helperName == "count" || helperName == "at" || helperName == "at_unsafe";
}

bool isExplicitRemovedCollectionMethodAlias(const std::string &receiverTypeName,
                                            std::string rawMethodName) {
  if (!rawMethodName.empty() && rawMethodName.front() == '/') {
    rawMethodName.erase(rawMethodName.begin());
  }

  std::string_view helperName;
  bool isVectorFamilyReceiver =
      receiverTypeName == "array" || receiverTypeName == "vector" || receiverTypeName == "soa_vector";
  if (isVectorFamilyReceiver) {
    if (rawMethodName.rfind("array/", 0) == 0) {
      helperName = std::string_view(rawMethodName).substr(std::string_view("array/").size());
    } else if (rawMethodName.rfind("vector/", 0) == 0) {
      helperName = std::string_view(rawMethodName).substr(std::string_view("vector/").size());
    } else if (rawMethodName.rfind("std/collections/vector/", 0) == 0) {
      helperName =
          std::string_view(rawMethodName).substr(std::string_view("std/collections/vector/").size());
    }
    return !helperName.empty() && isRemovedVectorCompatibilityHelper(std::string(helperName));
  }

  if (receiverTypeName != "map") {
    return false;
  }
  if (rawMethodName.rfind("map/", 0) == 0) {
    helperName = std::string_view(rawMethodName).substr(std::string_view("map/").size());
  } else if (rawMethodName.rfind("std/collections/map/", 0) == 0) {
    helperName =
        std::string_view(rawMethodName).substr(std::string_view("std/collections/map/").size());
  }
  return !helperName.empty() && isRemovedMapCompatibilityHelper(helperName);
}

std::string preferVectorStdlibHelperPath(const std::string &path,
                                         const std::unordered_map<std::string, Definition> &defs) {
  std::string preferred = path;
  if (preferred.rfind("/array/", 0) == 0 && defs.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/array/").size());
    if (!isRemovedVectorCompatibilityHelper(suffix)) {
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
    if (isRemovedVectorCompatibilityHelper(suffix)) {
      return preferred;
    }
    const std::string stdlibAlias = "/std/collections/vector/" + suffix;
    if (defs.count(stdlibAlias) > 0) {
      preferred = stdlibAlias;
    } else {
      if (!isRemovedVectorCompatibilityHelper(suffix)) {
        const std::string arrayAlias = "/array/" + suffix;
        if (defs.count(arrayAlias) > 0) {
          preferred = arrayAlias;
        }
      }
    }
  }
  if (preferred.rfind("/std/collections/vector/", 0) == 0 && defs.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/std/collections/vector/").size());
    if (isRemovedVectorCompatibilityHelper(suffix)) {
      return preferred;
    }
    const std::string vectorAlias = "/vector/" + suffix;
    if (defs.count(vectorAlias) > 0) {
      preferred = vectorAlias;
    } else {
      if (!isRemovedVectorCompatibilityHelper(suffix)) {
        const std::string arrayAlias = "/array/" + suffix;
        if (defs.count(arrayAlias) > 0) {
          preferred = arrayAlias;
        }
      }
    }
  }
  if (preferred.rfind("/map/", 0) == 0 && defs.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/map/").size());
    if (suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
        suffix != "at" && suffix != "at_unsafe") {
      const std::string stdlibAlias = "/std/collections/map/" + suffix;
      if (defs.count(stdlibAlias) > 0) {
        preferred = stdlibAlias;
      }
    }
  }
  if (preferred.rfind("/std/collections/map/", 0) == 0 && defs.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/std/collections/map/").size());
    if (suffix != "map" && suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
        suffix != "at" && suffix != "at_unsafe") {
      const std::string mapAlias = "/map/" + suffix;
      if (defs.count(mapAlias) > 0) {
        preferred = mapAlias;
      }
    }
  }
  return preferred;
}

std::string preferVectorStdlibTemplatePath(const std::string &path, const Context &ctx) {
  if (path.rfind("/array/", 0) == 0) {
    const std::string suffix = path.substr(std::string("/array/").size());
    if (!isRemovedVectorCompatibilityHelper(suffix)) {
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
    if (isRemovedVectorCompatibilityHelper(suffix)) {
      return path;
    }
    const std::string vectorPath = "/vector/" + suffix;
    if (ctx.sourceDefs.count(vectorPath) > 0 && ctx.templateDefs.count(vectorPath) > 0) {
      return vectorPath;
    }
    if (!isRemovedVectorCompatibilityHelper(suffix)) {
      const std::string arrayPath = "/array/" + suffix;
      if (ctx.sourceDefs.count(arrayPath) > 0 && ctx.templateDefs.count(arrayPath) > 0) {
        return arrayPath;
      }
    }
    return path;
  }
  if (path.rfind("/vector/", 0) != 0) {
    if (path.rfind("/map/", 0) == 0) {
      const std::string suffix = path.substr(std::string("/map/").size());
      if (suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
          suffix != "at" && suffix != "at_unsafe") {
        const std::string stdlibPath = "/std/collections/map/" + suffix;
        if (ctx.sourceDefs.count(stdlibPath) > 0 && ctx.templateDefs.count(stdlibPath) > 0) {
          return stdlibPath;
        }
      }
    }
    if (path.rfind("/std/collections/map/", 0) == 0) {
      const std::string suffix = path.substr(std::string("/std/collections/map/").size());
      if (suffix != "map" && suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
          suffix != "at" && suffix != "at_unsafe") {
        const std::string mapPath = "/map/" + suffix;
        if (ctx.sourceDefs.count(mapPath) > 0 && ctx.templateDefs.count(mapPath) > 0) {
          return mapPath;
        }
      }
    }
    return path;
  }
  const std::string suffix = path.substr(std::string("/vector/").size());
  if (isRemovedVectorCompatibilityHelper(suffix) && ctx.sourceDefs.count(path) == 0) {
    return path;
  }
  const std::string stdlibPath = "/std/collections/vector/" + suffix;
  if (ctx.sourceDefs.count(stdlibPath) > 0 && ctx.templateDefs.count(stdlibPath) > 0) {
    return stdlibPath;
  }
  if (ctx.sourceDefs.count(path) == 0 || ctx.templateDefs.count(path) == 0) {
    if (!isRemovedVectorCompatibilityHelper(suffix)) {
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
    extractExplicitBindingType(paramExpr, param.binding);
    if (paramExpr.args.size() == 1) {
      param.defaultExpr = &paramExpr.args.front();
    }
    params.push_back(std::move(param));
  }
  std::vector<const Expr *> ordered;
  std::string error;
  return buildOrderedArguments(params, expr.args, expr.argNames, ordered, error);
}

bool definitionHasArgumentCountMismatch(const Definition &def, const Expr &expr) {
  std::vector<ParameterInfo> params;
  params.reserve(def.parameters.size());
  for (const auto &paramExpr : def.parameters) {
    ParameterInfo param;
    param.name = paramExpr.name;
    extractExplicitBindingType(paramExpr, param.binding);
    if (paramExpr.args.size() == 1) {
      param.defaultExpr = &paramExpr.args.front();
    }
    params.push_back(std::move(param));
  }
  std::vector<const Expr *> ordered;
  std::string error;
  if (buildOrderedArguments(params, expr.args, expr.argNames, ordered, error)) {
    return false;
  }
  return error.find("argument count mismatch") != std::string::npos;
}

bool hasNamedCallArguments(const Expr &expr) {
  for (const auto &argName : expr.argNames) {
    if (argName.has_value()) {
      return true;
    }
  }
  return false;
}

bool isCollectionCompatibilityTemplateFallbackPath(const std::string &path) {
  return path == "/vector/count" || path == "/vector/capacity" || path == "/vector/at" ||
         path == "/vector/at_unsafe" || path == "/map/map" || path == "/map/count" || path == "/map/at" ||
         path == "/map/at_unsafe";
}

bool isExplicitCollectionCompatibilityAliasPath(std::string path) {
  if (path.empty()) {
    return false;
  }
  if (path.front() != '/' &&
      (path.rfind("array/", 0) == 0 || path.rfind("vector/", 0) == 0 || path.rfind("map/", 0) == 0)) {
    path.insert(path.begin(), '/');
  }
  return isCollectionCompatibilityTemplateFallbackPath(path) || path == "/array/count" ||
         path == "/array/capacity" || path == "/array/at" || path == "/array/at_unsafe";
}

bool shouldPreserveCompatibilityTemplatePath(const std::string &path, const Context &ctx) {
  return isCollectionCompatibilityTemplateFallbackPath(path) && ctx.sourceDefs.count(path) > 0 &&
         ctx.templateDefs.count(path) == 0;
}

bool shouldPreserveCanonicalMapTemplatePath(const std::string &path, const Context &ctx) {
  constexpr std::string_view canonicalMapPrefix = "/std/collections/map/";
  if (path.rfind(canonicalMapPrefix, 0) != 0) {
    return false;
  }
  if (ctx.sourceDefs.count(path) == 0 || ctx.templateDefs.count(path) > 0) {
    return false;
  }
  const std::string helper = path.substr(canonicalMapPrefix.size());
  if (helper != "map" && helper != "count" && helper != "at" && helper != "at_unsafe") {
    return false;
  }
  const std::string compatibilityPath = "/map/" + helper;
  return ctx.sourceDefs.count(compatibilityPath) > 0;
}

std::string normalizeCollectionReceiverTypeName(std::string value) {
  value = normalizeBindingTypeName(value);
  if (!value.empty() && value.front() == '/') {
    value.erase(value.begin());
  }
  if (value == "std/collections/vector") {
    return "vector";
  }
  if (value == "std/collections/experimental_vector/Vector" ||
      value.rfind("std/collections/experimental_vector/Vector__", 0) == 0) {
    return "vector";
  }
  if (value == "std/collections/map") {
    return "map";
  }
  if (value == "std/collections/experimental_map/Map") {
    return "map";
  }
  return value;
}

bool isCollectionReceiverTypeName(const std::string &value) {
  return value == "array" || value == "vector" || value == "soa_vector" || value == "map" || value == "string";
}

std::string unwrapCollectionReceiverEnvelope(std::string typeName, const std::string &typeTemplateArg = {}) {
  std::string normalizedType = normalizeBindingTypeName(typeName);
  if ((normalizedType == "Reference" || normalizedType == "Pointer") && !typeTemplateArg.empty()) {
    const std::string innerType = normalizeBindingTypeName(typeTemplateArg);
    std::string innerBase;
    std::string innerArgText;
    if (splitTemplateTypeName(innerType, innerBase, innerArgText) && !innerBase.empty()) {
      const std::string normalizedInnerBase = normalizeCollectionReceiverTypeName(innerBase);
      if (isCollectionReceiverTypeName(normalizedInnerBase)) {
        normalizedType = innerType;
      }
    } else {
      const std::string normalizedInner = normalizeCollectionReceiverTypeName(innerType);
      if (isCollectionReceiverTypeName(normalizedInner)) {
        normalizedType = innerType;
      }
    }
  }

  while (true) {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalizedType, base, argText) || base.empty()) {
      return normalizeCollectionReceiverTypeName(normalizedType);
    }

    const std::string normalizedBase = normalizeCollectionReceiverTypeName(base);
    if (isCollectionReceiverTypeName(normalizedBase)) {
      return normalizedBase;
    }

    if (base != "Reference" && base != "Pointer") {
      return normalizedBase;
    }

    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
      return base;
    }

    const std::string innerType = normalizeBindingTypeName(args.front());
    std::string innerBase;
    std::string innerArgText;
    if (splitTemplateTypeName(innerType, innerBase, innerArgText) && !innerBase.empty()) {
      const std::string normalizedInnerBase = normalizeCollectionReceiverTypeName(innerBase);
      if (!isCollectionReceiverTypeName(normalizedInnerBase)) {
        return base;
      }
    } else {
      const std::string normalizedInner = normalizeCollectionReceiverTypeName(innerType);
      if (!isCollectionReceiverTypeName(normalizedInner)) {
        return base;
      }
    }

    normalizedType = innerType;
  }
}

#include "TemplateMonomorphFallbackTypeInference.h"
#include "TemplateMonomorphMethodTargets.h"
#include "TemplateMonomorphBindingCallInference.h"
#include "TemplateMonomorphBindingBlockInference.h"

bool inferBindingTypeForMonomorph(const Expr &initializer,
                                  const std::vector<ParameterInfo> &params,
                                  const LocalTypeMap &locals,
                                  bool allowMathBare,
                                  Context &ctx,
                                  BindingInfo &infoOut) {
  if (tryInferBindingTypeFromInitializer(initializer, params, locals, infoOut, allowMathBare)) {
    return true;
  }
  bool handledCallInference = false;
  if (inferCallBindingTypeForMonomorph(initializer, params, locals, allowMathBare, ctx, infoOut,
                                       handledCallInference)) {
    return true;
  }
  if (handledCallInference) {
    return false;
  }
  return inferBlockBodyBindingTypeForMonomorph(initializer, params, locals, allowMathBare, ctx, infoOut);
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
  const bool isStdlibCollectionHelper =
      def.fullPath.rfind("/std/collections/vector/", 0) == 0 ||
      def.fullPath.rfind("/std/collections/map/", 0) == 0 ||
      def.fullPath.rfind("/map/", 0) == 0;
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
  std::vector<const Expr *> orderedArgs;
  std::vector<const Expr *> packedArgs;
  size_t packedParamIndex = callParams.size();
  size_t callArgStart = 0;
  if (callExpr.isMethodCall && callExpr.args.size() == def.parameters.size() + 1) {
    // Method-call sugar prepends the receiver expression.
    callArgStart = 1;
  }
  std::vector<Expr> reorderedArgs;
  std::vector<std::optional<std::string>> reorderedArgNames;
  const std::vector<Expr> *orderedCallArgs = &callExpr.args;
  const std::vector<std::optional<std::string>> *orderedCallArgNames = &callExpr.argNames;
  if (callArgStart > 0) {
    reorderedArgs.assign(callExpr.args.begin() + static_cast<std::ptrdiff_t>(callArgStart), callExpr.args.end());
    if (!callExpr.argNames.empty()) {
      reorderedArgNames.assign(callExpr.argNames.begin() + static_cast<std::ptrdiff_t>(callArgStart), callExpr.argNames.end());
    }
    orderedCallArgs = &reorderedArgs;
    orderedCallArgNames = &reorderedArgNames;
  }
  if (!buildOrderedArguments(callParams, *orderedCallArgs, *orderedCallArgNames, orderedArgs, packedArgs, packedParamIndex, error)) {
    if (error.find("argument count mismatch") != std::string::npos) {
      error = "argument count mismatch for " + def.fullPath;
    }
    return false;
  }

  for (size_t i = 0; i < def.parameters.size(); ++i) {
    const Expr &param = def.parameters[i];
    BindingInfo paramInfo;
    if (!extractExplicitBindingType(param, paramInfo)) {
      continue;
    }
    const bool inferFromPackedArgs = i == packedParamIndex && isArgsPackBinding(paramInfo) &&
                                     implicitSet.count(paramInfo.typeTemplateArg) > 0;
    std::string inferredParamName;
    if (inferFromPackedArgs) {
      inferredParamName = paramInfo.typeTemplateArg;
    } else {
      if (implicitSet.count(paramInfo.typeName) == 0) {
        continue;
      }
      inferredParamName = paramInfo.typeName;
    }
    std::vector<const Expr *> argsToInfer;
    if (inferFromPackedArgs) {
      argsToInfer = packedArgs;
    } else {
      const Expr *argExpr = i < orderedArgs.size() ? orderedArgs[i] : nullptr;
      if (!argExpr && param.args.size() == 1) {
        argExpr = &param.args.front();
      }
      if (argExpr) {
        argsToInfer.push_back(argExpr);
      }
    }
    if (argsToInfer.empty()) {
      error = "implicit template arguments require values on " + def.fullPath;
      return false;
    }
    for (const Expr *argExpr : argsToInfer) {
      BindingInfo argInfo;
      if (!argExpr) {
        if (isStdlibCollectionHelper) {
          return false;
        }
        error = "unable to infer implicit template arguments for " + def.fullPath;
        return false;
      }
      if (argExpr->isSpread) {
        std::string spreadElementType;
        if (!resolveArgsPackElementTypeForExpr(*argExpr, params, locals, spreadElementType)) {
          error = "spread argument requires args<T> value";
          return false;
        }
        argInfo.typeName = normalizeBindingTypeName(spreadElementType);
        argInfo.typeTemplateArg.clear();
        std::string spreadBase;
        std::string spreadArgs;
        if (splitTemplateTypeName(spreadElementType, spreadBase, spreadArgs)) {
          argInfo.typeName = normalizeBindingTypeName(spreadBase);
          argInfo.typeTemplateArg = spreadArgs;
        }
      } else if (!inferBindingTypeForMonomorph(*argExpr, params, locals, allowMathBare, ctx, argInfo)) {
        if (isStdlibCollectionHelper) {
          return false;
        }
        error = "unable to infer implicit template arguments for " + def.fullPath;
        return false;
      }
      std::string argType = bindingTypeToString(argInfo);
      if (argType.empty()) {
        if (isStdlibCollectionHelper) {
          return false;
        }
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
      auto it = inferred.find(inferredParamName);
      if (it != inferred.end() && it->second != resolvedArg.text) {
        error = "implicit template arguments conflict on " + def.fullPath;
        return false;
      }
      inferred[inferredParamName] = resolvedArg.text;
    }
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

ResolvedType resolveTypeString(std::string input,
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
  if (isBuiltinTemplateContainer(base) || isBuiltinCollectionTemplateBase(base, resolvedArgs.size())) {
    const std::string normalizedBase = isBuiltinTemplateContainer(base)
                                           ? base
                                           : normalizeBuiltinCollectionTemplateBase(base);
    result.text = normalizedBase + "<" + joinTemplateArgs(resolvedArgs) + ">";
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
        bool allConcreteTemplateArgs = true;
        for (auto &arg : transform.templateArgs) {
          ResolvedType resolvedArg = resolveTypeString(arg, mapping, allowedParams, namespacePrefix, ctx, error);
          if (!error.empty()) {
            return false;
          }
          allConcreteTemplateArgs = allConcreteTemplateArgs && resolvedArg.concrete;
          arg = resolvedArg.text;
        }

        const std::string resolvedPath =
            resolveNameToPath(transform.name, namespacePrefix, ctx.importAliases, ctx.sourceDefs);
        const bool isImportedGfxBufferTemplate =
            resolvedPath == "/std/gfx/Buffer" || resolvedPath == "/std/gfx/experimental/Buffer";
        if (isImportedGfxBufferTemplate && allConcreteTemplateArgs) {
          std::string specializedPath;
          if (!instantiateTemplate(resolvedPath, transform.templateArgs, ctx, error, specializedPath)) {
            return false;
          }
          transform.name = resolvedPath;
          continue;
        }
        if (!isBuiltinTemplateContainer(transform.name) && ctx.templateDefs.count(resolvedPath) > 0 &&
            allConcreteTemplateArgs) {
          std::string specializedPath;
          if (!instantiateTemplate(resolvedPath, transform.templateArgs, ctx, error, specializedPath)) {
            return false;
          }
          transform.name = specializedPath;
          transform.templateArgs.clear();
          continue;
        }
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
  auto rewriteBuiltinCollectionImportAlias = [&](const std::string &resolvedPath) -> std::string {
    if (expr.isMethodCall) {
      return resolvedPath;
    }
    std::string builtinCollection;
    if (!getBuiltinCollectionName(expr, builtinCollection)) {
      return resolvedPath;
    }
    if (expr.name != builtinCollection) {
      return resolvedPath;
    }
    if (resolvedPath == "/std/collections/vector") {
      return "/std/collections/vector/vector";
    }
    if (resolvedPath == "/std/collections/map") {
      return "/std/collections/map/map";
    }
    return resolvedPath;
  };
  auto rewriteCanonicalCollectionConstructorPath = [&](const std::string &resolvedPath) -> std::string {
    if (expr.isMethodCall) {
      return resolvedPath;
    }
    if (resolvedPath == "/std/collections/vector/vector" && ctx.sourceDefs.count(resolvedPath) > 0) {
      return resolvedPath;
    }
    auto vectorConstructorHelperPath = [&]() -> std::string {
      switch (expr.args.size()) {
      case 0:
        return "/std/collections/vectorNew";
      case 1:
        return "/std/collections/vectorSingle";
      case 2:
        return "/std/collections/vectorPair";
      case 3:
        return "/std/collections/vectorTriple";
      case 4:
        return "/std/collections/vectorQuad";
      case 5:
        return "/std/collections/vectorQuint";
      case 6:
        return "/std/collections/vectorSext";
      case 7:
        return "/std/collections/vectorSept";
      case 8:
        return "/std/collections/vectorOct";
      default:
        return {};
      }
    };
    auto mapConstructorHelperPath = [&]() -> std::string {
      switch (expr.args.size()) {
      case 0:
        return "/std/collections/mapNew";
      case 2:
        return "/std/collections/mapSingle";
      case 4:
        return "/std/collections/mapPair";
      case 6:
        return "/std/collections/mapTriple";
      case 8:
        return "/std/collections/mapQuad";
      case 10:
        return "/std/collections/mapQuint";
      case 12:
        return "/std/collections/mapSext";
      case 14:
        return "/std/collections/mapSept";
      case 16:
        return "/std/collections/mapOct";
      default:
        return {};
      }
    };
    std::string helperPath;
    if (resolvedPath == "/std/collections/vector/vector") {
      helperPath = vectorConstructorHelperPath();
    } else if (resolvedPath == "/std/collections/map/map") {
      helperPath = mapConstructorHelperPath();
    }
    if (!helperPath.empty() && ctx.sourceDefs.count(helperPath) > 0) {
      return helperPath;
    }
    return resolvedPath;
  };
  auto finalizeResolvedPath = [&](const std::string &resolvedPath) -> std::string {
    return selectHelperOverloadPath(expr, rewriteCanonicalCollectionConstructorPath(resolvedPath), ctx);
  };
  if (expr.name.empty()) {
    return "";
  }
  if (!expr.name.empty() && expr.name[0] == '/') {
    return finalizeResolvedPath(expr.name);
  }
  if (expr.name.find('/') != std::string::npos) {
    return finalizeResolvedPath("/" + expr.name);
  }
  if (!namespacePrefix.empty()) {
    const size_t lastSlash = namespacePrefix.find_last_of('/');
    const std::string_view suffix = lastSlash == std::string::npos
                                        ? std::string_view(namespacePrefix)
                                        : std::string_view(namespacePrefix).substr(lastSlash + 1);
    if (suffix == expr.name && ctx.sourceDefs.count(namespacePrefix) > 0) {
      return finalizeResolvedPath(namespacePrefix);
    }
    std::string prefix = namespacePrefix;
    while (!prefix.empty()) {
      std::string candidate = prefix + "/" + expr.name;
      if (ctx.sourceDefs.count(candidate) > 0) {
        return finalizeResolvedPath(candidate);
      }
      const size_t slash = prefix.find_last_of('/');
      if (slash == std::string::npos) {
        break;
      }
      prefix = prefix.substr(0, slash);
    }
    auto aliasIt = ctx.importAliases.find(expr.name);
    if (aliasIt != ctx.importAliases.end()) {
      return finalizeResolvedPath(rewriteBuiltinCollectionImportAlias(aliasIt->second));
    }
    return finalizeResolvedPath(rewriteBuiltinCollectionImportAlias(namespacePrefix + "/" + expr.name));
  }
  std::string root = "/" + expr.name;
  if (ctx.sourceDefs.count(root) > 0) {
    return finalizeResolvedPath(root);
  }
  auto aliasIt = ctx.importAliases.find(expr.name);
  if (aliasIt != ctx.importAliases.end()) {
    return finalizeResolvedPath(rewriteBuiltinCollectionImportAlias(aliasIt->second));
  }
  return finalizeResolvedPath(rewriteBuiltinCollectionImportAlias(root));
}

bool inferStdlibCollectionHelperTemplateArgs(const Definition &def,
                                             const Expr &callExpr,
                                             const LocalTypeMap &locals,
                                             const std::vector<ParameterInfo> &params,
                                             const SubstMap &mapping,
                                             const std::unordered_set<std::string> &allowedParams,
                                             const std::string &namespacePrefix,
                                             Context &ctx,
                                             bool allowMathBare,
                                             std::vector<std::string> &outArgs) {
  enum class HelperFamily { None, Vector, Map };
  HelperFamily family = HelperFamily::None;
  if (def.fullPath.rfind("/std/collections/vector/", 0) == 0) {
    family = HelperFamily::Vector;
  } else if (def.fullPath.rfind("/std/collections/map/", 0) == 0) {
    family = HelperFamily::Map;
  } else if (def.fullPath.rfind("/map/", 0) == 0) {
    family = HelperFamily::Map;
  } else {
    return false;
  }
  if (def.parameters.empty()) {
    return false;
  }
  const size_t helperSlash = def.fullPath.find_last_of('/');
  const std::string helperName = helperSlash == std::string::npos ? def.fullPath : def.fullPath.substr(helperSlash + 1);
  auto splitInlineTemplateType = [](BindingInfo &info) {
    if (!info.typeTemplateArg.empty()) {
      return;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(info.typeName, base, argText) && !base.empty()) {
      info.typeName = base;
      info.typeTemplateArg = argText;
    }
  };
  BindingInfo receiverParamInfo;
  if (!extractExplicitBindingType(def.parameters.front(), receiverParamInfo)) {
    return false;
  }
  splitInlineTemplateType(receiverParamInfo);
  const std::string normalizedReceiverParamType = normalizeCollectionReceiverTypeName(receiverParamInfo.typeName);
  size_t expectedTemplateArgCount = 0;
  if (family == HelperFamily::Vector) {
    if (def.templateArgs.size() != 1) {
      return false;
    }
    if (normalizedReceiverParamType != "vector" && normalizedReceiverParamType != "soa_vector") {
      return false;
    }
    expectedTemplateArgCount = 1;
  } else if (family == HelperFamily::Map) {
    if (def.templateArgs.size() != 2) {
      return false;
    }
    if (normalizedReceiverParamType != "map") {
      return false;
    }
    expectedTemplateArgCount = 2;
  } else {
    return false;
  }
  std::vector<std::string> receiverParamTemplateArgs;
  if (!splitTopLevelTemplateArgs(receiverParamInfo.typeTemplateArg, receiverParamTemplateArgs) ||
      receiverParamTemplateArgs.size() != expectedTemplateArgCount) {
    return false;
  }
  for (size_t i = 0; i < expectedTemplateArgCount; ++i) {
    if (trimWhitespace(receiverParamTemplateArgs[i]) != def.templateArgs[i]) {
      return false;
    }
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

  std::string receiverArgType;
  std::string receiverArgTemplateArg;
  BindingInfo receiverArgInfo;
  const bool inferredReceiverBinding =
      inferBindingTypeForMonomorph(*orderedArgs.front(), params, locals, allowMathBare, ctx, receiverArgInfo);
  if (inferredReceiverBinding) {
    splitInlineTemplateType(receiverArgInfo);
    receiverArgType = normalizeCollectionReceiverTypeName(receiverArgInfo.typeName);
    receiverArgTemplateArg = receiverArgInfo.typeTemplateArg;
  }
  if (!inferredReceiverBinding ||
      ((receiverArgType == "vector" || receiverArgType == "soa_vector" || receiverArgType == "map") &&
       receiverArgTemplateArg.empty())) {
    const std::string inferredReceiverTypeText =
        inferExprTypeTextForTemplatedVectorFallback(
            *orderedArgs.front(), locals, namespacePrefix, ctx, allowMathBare);
    if (inferredReceiverTypeText.empty()) {
      if (!inferredReceiverBinding) {
        return false;
      }
    } else {
      const std::string normalizedInferredReceiverType =
          normalizeCollectionReceiverTypeName(inferredReceiverTypeText);
      if (family == HelperFamily::Vector && normalizedInferredReceiverType == "string" &&
          (helperName == "at" || helperName == "at_unsafe")) {
        outArgs = {"i32"};
        return true;
      }
      std::string receiverBase;
      std::string receiverArgText;
      if (!splitTemplateTypeName(normalizedInferredReceiverType, receiverBase, receiverArgText) ||
          receiverBase.empty()) {
        return false;
      }
      receiverArgType = normalizeCollectionReceiverTypeName(receiverBase);
      receiverArgTemplateArg = receiverArgText;
    }
  }
  if ((receiverArgType == "Reference" || receiverArgType == "Pointer") && !receiverArgTemplateArg.empty()) {
    std::string pointeeBase;
    std::string pointeeArgText;
    if (splitTemplateTypeName(receiverArgTemplateArg, pointeeBase, pointeeArgText)) {
      receiverArgType = normalizeCollectionReceiverTypeName(pointeeBase);
      receiverArgTemplateArg = pointeeArgText;
    }
  }
  if (family == HelperFamily::Vector && receiverArgType == "string" &&
      (helperName == "at" || helperName == "at_unsafe")) {
    outArgs = {"i32"};
    return true;
  }
  if (receiverArgType != normalizedReceiverParamType) {
    return false;
  }

  std::vector<std::string> receiverArgTemplateArgs;
  if (!splitTopLevelTemplateArgs(receiverArgTemplateArg, receiverArgTemplateArgs) ||
      receiverArgTemplateArgs.size() != expectedTemplateArgCount) {
    return false;
  }
  outArgs.clear();
  outArgs.reserve(expectedTemplateArgCount);
  for (const auto &receiverArgTemplate : receiverArgTemplateArgs) {
    std::string resolvedError;
    ResolvedType resolvedArg =
        resolveTypeString(receiverArgTemplate, mapping, allowedParams, namespacePrefix, ctx, resolvedError);
    if (!resolvedError.empty() || !resolvedArg.concrete || resolvedArg.text.empty()) {
      return false;
    }
    outArgs.push_back(resolvedArg.text);
  }
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
        if (info.typeName == "auto" && param.args.size() == 1 &&
            inferBindingTypeForMonomorph(param.args.front(), {}, {}, allowMathBare, ctx, info)) {
          lambdaLocals[param.name] = info;
        } else {
          lambdaLocals[param.name] = info;
        }
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
        if (info.typeName == "auto" && bodyArg.args.size() == 1 &&
            inferBindingTypeForMonomorph(bodyArg.args.front(), params, lambdaLocals, allowMathBare, ctx, info)) {
          lambdaLocals[bodyArg.name] = info;
        } else {
          lambdaLocals[bodyArg.name] = info;
        }
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

  auto isCanonicalBuiltinMapHelperPath = [](const std::string &path) {
    return path == "/std/collections/map/count" || path == "/std/collections/map/contains" ||
           path == "/std/collections/map/tryAt" || path == "/std/collections/map/at" ||
           path == "/std/collections/map/at_unsafe";
  };
  auto isCanonicalStdlibCollectionHelperPath = [&](const std::string &path) {
    if (isCanonicalBuiltinMapHelperPath(path)) {
      return true;
    }
    return path == "/std/collections/vector/count" || path == "/std/collections/vector/capacity" ||
           path == "/std/collections/vector/push" || path == "/std/collections/vector/pop" ||
           path == "/std/collections/vector/reserve" || path == "/std/collections/vector/clear" ||
           path == "/std/collections/vector/remove_at" || path == "/std/collections/vector/remove_swap" ||
           path == "/std/collections/vector/at" || path == "/std/collections/vector/at_unsafe";
  };
  auto mapHelperReceiverExpr = [&](const Expr &candidate) -> const Expr * {
    if (candidate.isMethodCall) {
      return candidate.args.empty() ? nullptr : &candidate.args.front();
    }
    if (candidate.args.empty()) {
      return nullptr;
    }
    if (hasNamedArguments(candidate.argNames)) {
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          return &candidate.args[i];
        }
      }
    }
    return &candidate.args.front();
  };
  auto mutableMapHelperReceiverExpr = [&](Expr &candidate) -> Expr * {
    if (candidate.isMethodCall) {
      return candidate.args.empty() ? nullptr : &candidate.args.front();
    }
    if (candidate.args.empty()) {
      return nullptr;
    }
    if (hasNamedArguments(candidate.argNames)) {
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          return &candidate.args[i];
        }
      }
    }
    return &candidate.args.front();
  };
  auto resolvesBuiltinMapReceiver = [&](const Expr *receiverExpr) {
    if (receiverExpr == nullptr) {
      return false;
    }
    BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(*receiverExpr, params, locals, allowMathBare, ctx, receiverInfo)) {
      std::string receiverType = normalizeCollectionReceiverTypeName(receiverInfo.typeName);
      if ((receiverType == "Reference" || receiverType == "Pointer") && !receiverInfo.typeTemplateArg.empty()) {
        std::string innerBase;
        std::string innerArgText;
        if (splitTemplateTypeName(receiverInfo.typeTemplateArg, innerBase, innerArgText)) {
          receiverType = normalizeCollectionReceiverTypeName(innerBase);
        }
      }
      if (receiverType == "map") {
        return true;
      }
    }
    const std::string inferredReceiverType =
        inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare);
    if (inferredReceiverType.empty()) {
      return false;
    }
    std::string receiverBase;
    std::string receiverArgText;
    if (splitTemplateTypeName(inferredReceiverType, receiverBase, receiverArgText)) {
      return normalizeCollectionReceiverTypeName(receiverBase) == "map";
    }
    return normalizeCollectionReceiverTypeName(inferredReceiverType) == "map";
  };
  auto shouldDeferStdlibCollectionHelperTemplateRewrite = [&](const std::string &path) {
    if (!expr.templateArgs.empty() || !isCanonicalStdlibCollectionHelperPath(path)) {
      return false;
    }
    if (isCanonicalBuiltinMapHelperPath(path)) {
      return resolvesBuiltinMapReceiver(mapHelperReceiverExpr(expr)) && ctx.templateDefs.count(path) == 0;
    }
    return true;
  };
  auto resolvesExperimentalMapValueReceiver = [&](const Expr *receiverExpr) {
    if (receiverExpr == nullptr) {
      return false;
    }
    auto unwrapBindingTypeText = [&](const BindingInfo &binding) -> std::string {
      if (binding.typeName == "Reference" || binding.typeName == "Pointer") {
        return {};
      }
      std::string typeText = binding.typeName;
      if (!binding.typeTemplateArg.empty()) {
        typeText += "<" + binding.typeTemplateArg + ">";
      }
      return typeText;
    };
    BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(*receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
        resolvesExperimentalMapValueTypeText(unwrapBindingTypeText(receiverInfo),
                                             mapping,
                                             allowedParams,
                                             namespacePrefix,
                                             ctx)) {
      return true;
    }
    const std::string inferredReceiverType =
        inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare);
    return resolvesExperimentalMapValueTypeText(inferredReceiverType, mapping, allowedParams, namespacePrefix, ctx);
  };
  auto resolvesExperimentalMapBorrowedReceiver = [&](const Expr *receiverExpr) {
    if (receiverExpr == nullptr) {
      return false;
    }
    auto unwrapBorrowedTypeText = [&](const BindingInfo &binding) -> std::string {
      const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
      if ((normalizedType != "Reference" && normalizedType != "Pointer") || binding.typeTemplateArg.empty()) {
        return {};
      }
      return binding.typeTemplateArg;
    };
    BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(*receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
        resolvesExperimentalMapValueTypeText(unwrapBorrowedTypeText(receiverInfo),
                                             mapping,
                                             allowedParams,
                                             namespacePrefix,
                                             ctx)) {
      return true;
    }
    std::string inferredReceiverType =
        inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare);
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(inferredReceiverType, base, argText)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    if (base != "Reference" && base != "Pointer") {
      return false;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
      return false;
    }
    return resolvesExperimentalMapValueTypeText(args.front(), mapping, allowedParams, namespacePrefix, ctx);
  };
  auto resolvesExperimentalVectorValueReceiver = [&](const Expr *receiverExpr) {
    if (receiverExpr == nullptr) {
      return false;
    }
    auto unwrapBindingTypeText = [&](const BindingInfo &binding) -> std::string {
      if (binding.typeName == "Reference" || binding.typeName == "Pointer") {
        return {};
      }
      std::string typeText = binding.typeName;
      if (!binding.typeTemplateArg.empty()) {
        typeText += "<" + binding.typeTemplateArg + ">";
      }
      return typeText;
    };
    BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(*receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
        resolvesExperimentalVectorValueTypeText(unwrapBindingTypeText(receiverInfo))) {
      return true;
    }
    const std::string inferredReceiverType =
        inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare);
    return resolvesExperimentalVectorValueTypeText(inferredReceiverType);
  };
  auto canonicalMapHelperUnknownTargetPath = [&](const std::string &resolvedPath) -> std::string {
    if (resolvedPath == "/std/collections/map/count" || resolvedPath == "/map/count" ||
        resolvedPath == "/std/collections/mapCount") {
      return "/std/collections/map/count";
    }
    if (resolvedPath == "/std/collections/map/contains" || resolvedPath == "/map/contains" ||
        resolvedPath == "/std/collections/mapContains") {
      return "/std/collections/map/contains";
    }
    if (resolvedPath == "/std/collections/map/tryAt" || resolvedPath == "/map/tryAt" ||
        resolvedPath == "/std/collections/mapTryAt") {
      return "/std/collections/map/tryAt";
    }
    if (resolvedPath == "/std/collections/map/at" || resolvedPath == "/map/at" ||
        resolvedPath == "/std/collections/mapAt") {
      return "/std/collections/map/at";
    }
    if (resolvedPath == "/std/collections/map/at_unsafe" || resolvedPath == "/map/at_unsafe" ||
        resolvedPath == "/std/collections/mapAtUnsafe") {
      return "/std/collections/map/at_unsafe";
    }
    return {};
  };
  auto resolveExperimentalMapValueReceiverTemplateArgs = [&](const Expr *receiverExpr,
                                                             std::vector<std::string> &templateArgsOut) {
    templateArgsOut.clear();
    if (receiverExpr == nullptr) {
      return false;
    }
    auto extractArgsFromTypeText = [&](const std::string &typeText) {
      std::string normalizedType = normalizeBindingTypeName(typeText);
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        std::string normalizedBase = normalizeBindingTypeName(base);
        if (!normalizedBase.empty() && normalizedBase.front() == '/') {
          normalizedBase.erase(normalizedBase.begin());
        }
        if (normalizedBase == "Map" || normalizedBase == "std/collections/experimental_map/Map") {
          return splitTopLevelTemplateArgs(argText, templateArgsOut) && templateArgsOut.size() == 2;
        }
      }
      std::string resolvedPath = normalizedType;
      if (!resolvedPath.empty() && resolvedPath.front() != '/') {
        resolvedPath.insert(resolvedPath.begin(), '/');
      }
      std::string normalizedResolvedPath = normalizeBindingTypeName(resolvedPath);
      if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
        normalizedResolvedPath.erase(normalizedResolvedPath.begin());
      }
      if (normalizedResolvedPath.rfind("std/collections/experimental_map/Map__", 0) != 0) {
        return false;
      }
      auto defIt = ctx.sourceDefs.find(resolvedPath);
      if (defIt == ctx.sourceDefs.end()) {
        return false;
      }
      std::string keyType;
      std::string valueType;
      for (const auto &fieldExpr : defIt->second.statements) {
        if (!fieldExpr.isBinding) {
          continue;
        }
        BindingInfo fieldBinding;
        if (!extractExplicitBindingType(fieldExpr, fieldBinding)) {
          continue;
        }
        std::vector<std::string> fieldArgs;
        const std::string normalizedFieldType = normalizeBindingTypeName(fieldBinding.typeName);
        if (normalizedFieldType == "vector" && !fieldBinding.typeTemplateArg.empty()) {
          if (!splitTopLevelTemplateArgs(fieldBinding.typeTemplateArg, fieldArgs) ||
              fieldArgs.size() != 1) {
            continue;
          }
        } else {
          std::string fieldBase;
          std::string fieldArgText;
          if (!splitTemplateTypeName(normalizedFieldType, fieldBase, fieldArgText)) {
            continue;
          }
          fieldBase = normalizeBindingTypeName(fieldBase);
          if (fieldBase != "Vector" &&
              fieldBase != "std/collections/experimental_vector/Vector") {
            continue;
          }
          if (!splitTopLevelTemplateArgs(fieldArgText, fieldArgs) ||
              fieldArgs.size() != 1) {
            continue;
          }
        }
        if (fieldExpr.name == "keys") {
          keyType = fieldArgs.front();
        } else if (fieldExpr.name == "payloads") {
          valueType = fieldArgs.front();
        }
      }
      if (keyType.empty() || valueType.empty()) {
        return false;
      }
      templateArgsOut = {keyType, valueType};
      return true;
    };
    BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(*receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
        normalizeBindingTypeName(receiverInfo.typeName) != "Reference" &&
        normalizeBindingTypeName(receiverInfo.typeName) != "Pointer") {
      std::string bindingTypeText = receiverInfo.typeName;
      if (!receiverInfo.typeTemplateArg.empty()) {
        bindingTypeText += "<" + receiverInfo.typeTemplateArg + ">";
      }
      if (extractArgsFromTypeText(bindingTypeText)) {
        return true;
      }
    }
    return extractArgsFromTypeText(
        inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare));
  };
  auto experimentalMapHelperPathForCanonicalHelper = [&](const std::string &path) -> std::string {
    if (path == "/std/collections/map/count") {
      return "/std/collections/experimental_map/mapCount";
    }
    if (path == "/std/collections/map/contains") {
      return "/std/collections/experimental_map/mapContains";
    }
    if (path == "/std/collections/map/tryAt") {
      return "/std/collections/experimental_map/mapTryAt";
    }
    if (path == "/std/collections/map/at") {
      return "/std/collections/experimental_map/mapAt";
    }
    if (path == "/std/collections/map/at_unsafe") {
      return "/std/collections/experimental_map/mapAtUnsafe";
    }
    return {};
  };
  auto experimentalVectorHelperPathForCanonicalHelper = [&](const std::string &path) -> std::string {
    if (path == "/std/collections/vector/count" || path == "/vector/count" ||
        path == "/std/collections/vectorCount") {
      return "/std/collections/experimental_vector/vectorCount";
    }
    if (path == "/std/collections/vector/capacity" || path == "/vector/capacity" ||
        path == "/std/collections/vectorCapacity") {
      return "/std/collections/experimental_vector/vectorCapacity";
    }
    if (path == "/std/collections/vector/push" || path == "/vector/push" ||
        path == "/std/collections/vectorPush") {
      return "/std/collections/experimental_vector/vectorPush";
    }
    if (path == "/std/collections/vector/pop" || path == "/vector/pop" ||
        path == "/std/collections/vectorPop") {
      return "/std/collections/experimental_vector/vectorPop";
    }
    if (path == "/std/collections/vector/reserve" || path == "/vector/reserve" ||
        path == "/std/collections/vectorReserve") {
      return "/std/collections/experimental_vector/vectorReserve";
    }
    if (path == "/std/collections/vector/clear" || path == "/vector/clear" ||
        path == "/std/collections/vectorClear") {
      return "/std/collections/experimental_vector/vectorClear";
    }
    if (path == "/std/collections/vector/remove_at" || path == "/vector/remove_at" ||
        path == "/std/collections/vectorRemoveAt") {
      return "/std/collections/experimental_vector/vectorRemoveAt";
    }
    if (path == "/std/collections/vector/remove_swap" || path == "/vector/remove_swap" ||
        path == "/std/collections/vectorRemoveSwap") {
      return "/std/collections/experimental_vector/vectorRemoveSwap";
    }
    if (path == "/std/collections/vector/at" || path == "/vector/at" ||
        path == "/std/collections/vectorAt") {
      return "/std/collections/experimental_vector/vectorAt";
    }
    if (path == "/std/collections/vector/at_unsafe" || path == "/vector/at_unsafe" ||
        path == "/std/collections/vectorAtUnsafe") {
      return "/std/collections/experimental_vector/vectorAtUnsafe";
    }
    return {};
  };
  auto experimentalMapHelperPathForWrapperHelper = [&](const std::string &path) -> std::string {
    if (path == "/std/collections/mapCount") {
      return "/std/collections/experimental_map/mapCount";
    }
    if (path == "/std/collections/mapContains") {
      return "/std/collections/experimental_map/mapContains";
    }
    if (path == "/std/collections/mapTryAt") {
      return "/std/collections/experimental_map/mapTryAt";
    }
    if (path == "/std/collections/mapAt") {
      return "/std/collections/experimental_map/mapAt";
    }
    if (path == "/std/collections/mapAtUnsafe") {
      return "/std/collections/experimental_map/mapAtUnsafe";
    }
    return {};
  };
  auto experimentalMapConstructorHelperPath = [&](size_t argumentCount) -> std::string {
    switch (argumentCount) {
    case 0:
      return "/std/collections/experimental_map/mapNew";
    case 2:
      return "/std/collections/experimental_map/mapSingle";
    case 4:
      return "/std/collections/experimental_map/mapPair";
    case 6:
      return "/std/collections/experimental_map/mapTriple";
    case 8:
      return "/std/collections/experimental_map/mapQuad";
    case 10:
      return "/std/collections/experimental_map/mapQuint";
    case 12:
      return "/std/collections/experimental_map/mapSext";
    case 14:
      return "/std/collections/experimental_map/mapSept";
    case 16:
      return "/std/collections/experimental_map/mapOct";
    default:
      return {};
    }
  };
  auto experimentalMapConstructorRewritePath =
      [&](const std::string &resolvedPath, size_t argumentCount) -> std::string {
    if (resolvedPath == "/std/collections/map/map") {
      return experimentalMapConstructorHelperPath(argumentCount);
    }
    if (resolvedPath == "/std/collections/mapNew") {
      return "/std/collections/experimental_map/mapNew";
    }
    if (resolvedPath == "/std/collections/mapSingle") {
      return "/std/collections/experimental_map/mapSingle";
    }
    if (resolvedPath == "/std/collections/mapDouble") {
      return "/std/collections/experimental_map/mapDouble";
    }
    if (resolvedPath == "/std/collections/mapPair") {
      return "/std/collections/experimental_map/mapPair";
    }
    if (resolvedPath == "/std/collections/mapTriple") {
      return "/std/collections/experimental_map/mapTriple";
    }
    if (resolvedPath == "/std/collections/mapQuad") {
      return "/std/collections/experimental_map/mapQuad";
    }
    if (resolvedPath == "/std/collections/mapQuint") {
      return "/std/collections/experimental_map/mapQuint";
    }
    if (resolvedPath == "/std/collections/mapSext") {
      return "/std/collections/experimental_map/mapSext";
    }
    if (resolvedPath == "/std/collections/mapSept") {
      return "/std/collections/experimental_map/mapSept";
    }
    if (resolvedPath == "/std/collections/mapOct") {
      return "/std/collections/experimental_map/mapOct";
    }
    return {};
  };
  auto rewriteCanonicalExperimentalMapConstructorExpr = [&](Expr &valueExpr) -> bool {
    if (valueExpr.kind != Expr::Kind::Call || valueExpr.isBinding || valueExpr.isMethodCall) {
      return true;
    }
    const std::string originalPath = resolveCalleePath(valueExpr, namespacePrefix, ctx);
    const std::string helperPath =
        experimentalMapConstructorRewritePath(originalPath, valueExpr.args.size());
    if (helperPath.empty() || ctx.sourceDefs.count(helperPath) == 0) {
      return true;
    }
    valueExpr.name = helperPath;
    valueExpr.namespacePrefix.clear();
    if (valueExpr.templateArgs.empty()) {
      auto defIt = ctx.sourceDefs.find(helperPath);
      if (defIt != ctx.sourceDefs.end()) {
        std::vector<std::string> inferredArgs;
        std::string inferError;
        if (inferImplicitTemplateArgs(defIt->second,
                                      valueExpr,
                                      locals,
                                      params,
                                      mapping,
                                      allowedParams,
                                      namespacePrefix,
                                      ctx,
                                      allowMathBare,
                                      inferredArgs,
                                      inferError)) {
          valueExpr.templateArgs = std::move(inferredArgs);
        } else {
          defIt = ctx.sourceDefs.find(helperPath);
          if (defIt == ctx.sourceDefs.end()) {
            return false;
          }
          if (inferError.empty() &&
              inferStdlibCollectionHelperTemplateArgs(defIt->second,
                                                      valueExpr,
                                                      locals,
                                                      params,
                                                      mapping,
                                                      allowedParams,
                                                      namespacePrefix,
                                                      ctx,
                                                      allowMathBare,
                                                      inferredArgs)) {
          valueExpr.templateArgs = std::move(inferredArgs);
          } else if (!inferError.empty()) {
            error = inferError;
            const size_t helperPos = error.find(helperPath);
            if (helperPos != std::string::npos) {
              error.replace(helperPos, helperPath.size(), originalPath);
            }
            return false;
          }
        }
      }
    }
    return true;
  };
  auto experimentalVectorConstructorRewritePath =
      [&](const std::string &resolvedPath, size_t argumentCount) -> std::string {
    if (resolvedPath == "/std/collections/vectorNew") {
      return "/std/collections/experimental_vector/vectorNew";
    }
    if (resolvedPath == "/std/collections/vectorSingle") {
      return "/std/collections/experimental_vector/vectorSingle";
    }
    if (resolvedPath == "/std/collections/vectorPair") {
      return "/std/collections/experimental_vector/vectorPair";
    }
    if (resolvedPath == "/std/collections/vectorTriple") {
      return "/std/collections/experimental_vector/vectorTriple";
    }
    if (resolvedPath == "/std/collections/vectorQuad") {
      return "/std/collections/experimental_vector/vectorQuad";
    }
    if (resolvedPath == "/std/collections/vectorQuint") {
      return "/std/collections/experimental_vector/vectorQuint";
    }
    if (resolvedPath == "/std/collections/vectorSext") {
      return "/std/collections/experimental_vector/vectorSext";
    }
    if (resolvedPath == "/std/collections/vectorSept") {
      return "/std/collections/experimental_vector/vectorSept";
    }
    if (resolvedPath == "/std/collections/vectorOct") {
      return "/std/collections/experimental_vector/vectorOct";
    }
    (void)argumentCount;
    return {};
  };
  auto rewriteCanonicalExperimentalVectorConstructorExpr = [&](Expr &valueExpr) -> bool {
    if (valueExpr.kind != Expr::Kind::Call || valueExpr.isBinding || valueExpr.isMethodCall) {
      return true;
    }
    const std::string originalPath = resolveCalleePath(valueExpr, namespacePrefix, ctx);
    const std::string helperPath =
        experimentalVectorConstructorRewritePath(originalPath, valueExpr.args.size());
    if (helperPath.empty() || ctx.sourceDefs.count(helperPath) == 0) {
      return true;
    }
    valueExpr.name = helperPath;
    valueExpr.namespacePrefix.clear();
    if (valueExpr.templateArgs.empty()) {
      auto defIt = ctx.sourceDefs.find(helperPath);
      if (defIt != ctx.sourceDefs.end()) {
        std::vector<std::string> inferredArgs;
        std::string inferError;
        if (inferImplicitTemplateArgs(defIt->second,
                                      valueExpr,
                                      locals,
                                      params,
                                      mapping,
                                      allowedParams,
                                      namespacePrefix,
                                      ctx,
                                      allowMathBare,
                                      inferredArgs,
                                      inferError)) {
          valueExpr.templateArgs = std::move(inferredArgs);
        } else {
          defIt = ctx.sourceDefs.find(helperPath);
          if (defIt == ctx.sourceDefs.end()) {
            return false;
          }
          if (inferError.empty() &&
              inferStdlibCollectionHelperTemplateArgs(defIt->second,
                                                      valueExpr,
                                                      locals,
                                                      params,
                                                      mapping,
                                                      allowedParams,
                                                      namespacePrefix,
                                                      ctx,
                                                      allowMathBare,
                                                      inferredArgs)) {
            valueExpr.templateArgs = std::move(inferredArgs);
          } else if (!inferError.empty()) {
            error = inferError;
            const size_t helperPos = error.find(helperPath);
            if (helperPos != std::string::npos) {
              error.replace(helperPos, helperPath.size(), originalPath);
            }
            return false;
          }
        }
      }
    }
    return true;
  };
  std::function<bool(Expr &)> rewriteNestedExperimentalMapConstructorValue;
  std::function<bool(Expr &)> rewriteNestedExperimentalMapResultOkPayloadValue;
  std::function<bool(Expr &)> rewriteNestedExperimentalVectorConstructorValue;
  std::function<bool(const std::string &, Expr &)> rewriteExperimentalMapTargetValueForType;
  std::function<bool(const std::string &, Expr &)> rewriteExperimentalVectorTargetValueForType;

  auto isBuiltinResultOkValueCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name != "ok" ||
        candidate.args.size() != 2) {
      return false;
    }
    const Expr &receiver = candidate.args.front();
    return receiver.kind == Expr::Kind::Name && normalizeBindingTypeName(receiver.name) == "Result";
  };

  rewriteExperimentalMapTargetValueForType = [&](const std::string &typeText, Expr &valueExpr) -> bool {
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(typeText, base, argText) && normalizeBindingTypeName(base) == "uninitialized") {
      std::vector<std::string> storageArgs;
      if (!splitTopLevelTemplateArgs(argText, storageArgs) || storageArgs.size() != 1) {
        return true;
      }
      return rewriteExperimentalMapTargetValueForType(trimWhitespace(storageArgs.front()), valueExpr);
    }
    if (resolvesExperimentalMapValueTypeText(typeText, mapping, allowedParams, namespacePrefix, ctx)) {
      return rewriteNestedExperimentalMapConstructorValue(valueExpr);
    }
    if (!splitTemplateTypeName(typeText, base, argText) || normalizeBindingTypeName(base) != "Result") {
      return true;
    }
    std::vector<std::string> resultArgs;
    if (!splitTopLevelTemplateArgs(argText, resultArgs) || resultArgs.size() != 2) {
      return true;
    }
    if (!resolvesExperimentalMapValueTypeText(trimWhitespace(resultArgs.front()),
                                              mapping,
                                              allowedParams,
                                              namespacePrefix,
                                              ctx)) {
      return true;
    }
    return rewriteNestedExperimentalMapResultOkPayloadValue(valueExpr);
  };
  rewriteExperimentalVectorTargetValueForType = [&](const std::string &typeText, Expr &valueExpr) -> bool {
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(typeText, base, argText) && normalizeBindingTypeName(base) == "uninitialized") {
      std::vector<std::string> storageArgs;
      if (!splitTopLevelTemplateArgs(argText, storageArgs) || storageArgs.size() != 1) {
        return true;
      }
      return rewriteExperimentalVectorTargetValueForType(trimWhitespace(storageArgs.front()), valueExpr);
    }
    if (!resolvesExperimentalVectorValueTypeText(typeText)) {
      return true;
    }
    return rewriteNestedExperimentalVectorConstructorValue(valueExpr);
  };

  auto rewriteCanonicalExperimentalMapConstructorBinding = [&](Expr &bindingExpr) -> bool {
    if (!bindingExpr.isBinding || bindingExpr.args.size() != 1) {
      return true;
    }
    BindingInfo bindingInfo;
    const bool hasExplicitBindingType = extractExplicitBindingType(bindingExpr, bindingInfo);
    if (!hasExplicitBindingType || bindingInfo.typeName == "auto") {
      if (!inferBindingTypeForMonomorph(bindingExpr.args.front(), params, locals, allowMathBare, ctx, bindingInfo)) {
        return true;
      }
    }
    std::string bindingTypeText = bindingInfo.typeName;
    if (!bindingInfo.typeTemplateArg.empty()) {
      bindingTypeText += "<" + bindingInfo.typeTemplateArg + ">";
    }
    return rewriteExperimentalMapTargetValueForType(bindingTypeText, bindingExpr.args.front());
  };
  auto rewriteCanonicalExperimentalVectorConstructorBinding = [&](Expr &bindingExpr) -> bool {
    if (!bindingExpr.isBinding || bindingExpr.args.size() != 1) {
      return true;
    }
    BindingInfo bindingInfo;
    const bool hasExplicitBindingType = extractExplicitBindingType(bindingExpr, bindingInfo);
    if (!hasExplicitBindingType || bindingInfo.typeName == "auto") {
      if (!inferBindingTypeForMonomorph(bindingExpr.args.front(), params, locals, allowMathBare, ctx, bindingInfo)) {
        return true;
      }
    }
    std::string bindingTypeText = bindingInfo.typeName;
    if (!bindingInfo.typeTemplateArg.empty()) {
      bindingTypeText += "<" + bindingInfo.typeTemplateArg + ">";
    }
    return rewriteExperimentalVectorTargetValueForType(bindingTypeText, bindingExpr.args.front());
  };
  auto inferCallTargetBinding = [&](const Expr &bindingExpr, BindingInfo &bindingOut) -> bool {
    const bool hasExplicitBinding = extractExplicitBindingType(bindingExpr, bindingOut);
    if (hasExplicitBinding && bindingOut.typeName != "auto") {
      return true;
    }
    if (bindingExpr.args.size() != 1) {
      return hasExplicitBinding;
    }
    BindingInfo inferredBinding;
    if (!inferBindingTypeForMonomorph(bindingExpr.args.front(), {}, {}, allowMathBare, ctx, inferredBinding)) {
      return hasExplicitBinding;
    }
    bindingOut = inferredBinding;
    return true;
  };
  std::function<bool(const Expr &, BindingInfo &)> resolveAssignmentTargetBinding;
  auto resolveFieldBindingTarget = [&](const Expr &target, BindingInfo &bindingOut) -> bool {
    if (!(target.kind == Expr::Kind::Call && target.isFieldAccess && target.args.size() == 1)) {
      return false;
    }
    const Expr &receiver = target.args.front();
    std::string receiverTypeText;
    BindingInfo receiverInfo;
    if (resolveAssignmentTargetBinding && resolveAssignmentTargetBinding(receiver, receiverInfo)) {
      receiverTypeText = bindingTypeToString(receiverInfo);
    } else if (inferBindingTypeForMonomorph(receiver, params, locals, allowMathBare, ctx, receiverInfo)) {
      receiverTypeText = bindingTypeToString(receiverInfo);
    }
    if (receiverTypeText.empty()) {
      receiverTypeText =
          inferExprTypeTextForTemplatedVectorFallback(receiver, locals, namespacePrefix, ctx, allowMathBare);
    }
    if (receiverTypeText.empty()) {
      return false;
    }
    receiverTypeText = normalizeBindingTypeName(receiverTypeText);
    while (true) {
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(receiverTypeText, base, argText) || base.empty()) {
        break;
      }
      base = normalizeBindingTypeName(base);
      if (base != "Reference" && base != "Pointer") {
        receiverTypeText = base;
        break;
      }
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      receiverTypeText = normalizeBindingTypeName(args.front());
    }
    std::string receiverStructPath = receiverTypeText;
    std::string receiverBase;
    std::string receiverArgText;
    if (splitTemplateTypeName(receiverStructPath, receiverBase, receiverArgText) && !receiverBase.empty()) {
      receiverStructPath = normalizeBindingTypeName(receiverBase);
    }
    if (!receiverStructPath.empty() && receiverStructPath.front() != '/') {
      receiverStructPath = resolveTypePath(receiverStructPath, receiver.namespacePrefix);
    }
    auto structIt = ctx.sourceDefs.find(receiverStructPath);
    if (structIt == ctx.sourceDefs.end() || !isStructDefinition(structIt->second)) {
      return false;
    }
    for (const auto &fieldStmt : structIt->second.statements) {
      if (!fieldStmt.isBinding || fieldStmt.name != target.name) {
        continue;
      }
      return inferCallTargetBinding(fieldStmt, bindingOut);
    }
    return false;
  };
  auto resolveExperimentalVectorValueReceiverTemplateArgs = [&](const Expr *receiverExpr,
                                                                std::vector<std::string> &templateArgsOut) {
    templateArgsOut.clear();
    if (receiverExpr == nullptr) {
      return false;
    }
    auto extractArgsFromTypeText = [&](const std::string &typeText) {
      std::string normalizedType = normalizeBindingTypeName(typeText);
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        std::string normalizedBase = normalizeBindingTypeName(base);
        if (!normalizedBase.empty() && normalizedBase.front() == '/') {
          normalizedBase.erase(normalizedBase.begin());
        }
        if ((normalizedBase == "Vector" ||
             normalizedBase == "std/collections/experimental_vector/Vector") &&
            !argText.empty()) {
          return splitTopLevelTemplateArgs(argText, templateArgsOut) &&
                 templateArgsOut.size() == 1;
        }
      }
      std::string resolvedPath = normalizedType;
      if (!resolvedPath.empty() && resolvedPath.front() != '/') {
        resolvedPath.insert(resolvedPath.begin(), '/');
      }
      std::string normalizedResolvedPath = normalizeBindingTypeName(resolvedPath);
      if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
        normalizedResolvedPath.erase(normalizedResolvedPath.begin());
      }
      if (normalizedResolvedPath.rfind("std/collections/experimental_vector/Vector__", 0) != 0) {
        return false;
      }
      auto defIt = ctx.sourceDefs.find(resolvedPath);
      if (defIt == ctx.sourceDefs.end()) {
        return false;
      }
      for (const auto &fieldExpr : defIt->second.statements) {
        if (!fieldExpr.isBinding || fieldExpr.name != "data") {
          continue;
        }
        BindingInfo fieldBinding;
        if (!extractExplicitBindingType(fieldExpr, fieldBinding)) {
          continue;
        }
        if (normalizeBindingTypeName(fieldBinding.typeName) != "Pointer" ||
            fieldBinding.typeTemplateArg.empty()) {
          continue;
        }
        std::string pointeeBase;
        std::string pointeeArgText;
        if (!splitTemplateTypeName(normalizeBindingTypeName(fieldBinding.typeTemplateArg),
                                   pointeeBase,
                                   pointeeArgText) ||
            normalizeBindingTypeName(pointeeBase) != "uninitialized") {
          continue;
        }
        return splitTopLevelTemplateArgs(pointeeArgText, templateArgsOut) &&
               templateArgsOut.size() == 1;
      }
      return false;
    };

    BindingInfo receiverInfo;
    auto unwrapBindingTypeText = [&](const BindingInfo &binding) -> std::string {
      if (binding.typeName == "Reference" || binding.typeName == "Pointer") {
        return {};
      }
      std::string typeText = binding.typeName;
      if (!binding.typeTemplateArg.empty()) {
        typeText += "<" + binding.typeTemplateArg + ">";
      }
      return typeText;
    };
    if (inferBindingTypeForMonomorph(*receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
        extractArgsFromTypeText(unwrapBindingTypeText(receiverInfo))) {
      return true;
    }
    return extractArgsFromTypeText(
        inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare));
  };
  auto resolveDereferenceBindingTarget = [&](const Expr &target, BindingInfo &bindingOut) -> bool {
    if (target.kind != Expr::Kind::Call || target.args.size() != 1) {
      return false;
    }
    std::string pointerBuiltin;
    if (!getBuiltinPointerName(target, pointerBuiltin) || pointerBuiltin != "dereference") {
      return false;
    }
    std::function<bool(const Expr &, BindingInfo &)> inferPointerBinding =
        [&](const Expr &pointerExpr, BindingInfo &pointerOut) -> bool {
      if (inferBindingTypeForMonomorph(pointerExpr, params, locals, allowMathBare, ctx, pointerOut)) {
        return true;
      }
      if (pointerExpr.kind != Expr::Kind::Call || pointerExpr.args.size() != 1) {
        return false;
      }
      std::string nestedPointerBuiltin;
      if (!getBuiltinPointerName(pointerExpr, nestedPointerBuiltin) || nestedPointerBuiltin != "location") {
        return false;
      }
      BindingInfo pointeeInfo;
      if (!resolveAssignmentTargetBinding(pointerExpr.args.front(), pointeeInfo)) {
        return false;
      }
      const std::string pointeeTypeText = bindingTypeToString(pointeeInfo);
      if (pointeeTypeText.empty()) {
        return false;
      }
      pointerOut.typeName = "Reference";
      pointerOut.typeTemplateArg = pointeeTypeText;
      return true;
    };
    BindingInfo pointerInfo;
    if (!inferPointerBinding(target.args.front(), pointerInfo)) {
      return false;
    }
    const std::string normalizedPointerType = normalizeBindingTypeName(pointerInfo.typeName);
    if ((normalizedPointerType != "Reference" && normalizedPointerType != "Pointer") ||
        pointerInfo.typeTemplateArg.empty()) {
      return false;
    }
    std::string pointeeBase;
    std::string pointeeArgText;
    if (splitTemplateTypeName(pointerInfo.typeTemplateArg, pointeeBase, pointeeArgText) && !pointeeBase.empty()) {
      bindingOut.typeName = pointeeBase;
      bindingOut.typeTemplateArg = pointeeArgText;
    } else {
      bindingOut.typeName = pointerInfo.typeTemplateArg;
      bindingOut.typeTemplateArg.clear();
    }
    return true;
  };
  resolveAssignmentTargetBinding = [&](const Expr &target, BindingInfo &bindingOut) -> bool {
    return inferBindingTypeForMonomorph(target, params, locals, allowMathBare, ctx, bindingOut) ||
           resolveFieldBindingTarget(target, bindingOut) ||
           resolveDereferenceBindingTarget(target, bindingOut);
  };

  rewriteNestedExperimentalMapConstructorValue = [&](Expr &candidate) -> bool {
    if (candidate.isBinding && candidate.args.size() == 1) {
      return rewriteNestedExperimentalMapConstructorValue(candidate.args.front());
    }
    if (candidate.kind != Expr::Kind::Call) {
      return true;
    }
    if (!rewriteCanonicalExperimentalMapConstructorExpr(candidate)) {
      return false;
    }
    // Keep walking through wrapper call trees once an outer destination is
    // known to require an experimental Map value.
    for (auto &arg : candidate.args) {
      if (!rewriteNestedExperimentalMapConstructorValue(arg)) {
        return false;
      }
    }
    for (auto &bodyArg : candidate.bodyArguments) {
      if (!rewriteNestedExperimentalMapConstructorValue(bodyArg)) {
        return false;
      }
    }
    return true;
  };

  rewriteNestedExperimentalMapResultOkPayloadValue = [&](Expr &candidate) -> bool {
    if (candidate.isBinding && candidate.args.size() == 1) {
      return rewriteNestedExperimentalMapResultOkPayloadValue(candidate.args.front());
    }
    if (candidate.kind != Expr::Kind::Call) {
      return true;
    }
    if (isBuiltinResultOkValueCall(candidate)) {
      return rewriteNestedExperimentalMapConstructorValue(candidate.args.back());
    }
    for (auto &arg : candidate.args) {
      if (!rewriteNestedExperimentalMapResultOkPayloadValue(arg)) {
        return false;
      }
    }
    for (auto &bodyArg : candidate.bodyArguments) {
      if (!rewriteNestedExperimentalMapResultOkPayloadValue(bodyArg)) {
        return false;
      }
    }
    return true;
  };
  rewriteNestedExperimentalVectorConstructorValue = [&](Expr &candidate) -> bool {
    if (candidate.isBinding && candidate.args.size() == 1) {
      return rewriteNestedExperimentalVectorConstructorValue(candidate.args.front());
    }
    if (candidate.kind != Expr::Kind::Call) {
      return true;
    }
    if (!rewriteCanonicalExperimentalVectorConstructorExpr(candidate)) {
      return false;
    }
    for (auto &arg : candidate.args) {
      if (!rewriteNestedExperimentalVectorConstructorValue(arg)) {
        return false;
      }
    }
    for (auto &bodyArg : candidate.bodyArguments) {
      if (!rewriteNestedExperimentalVectorConstructorValue(bodyArg)) {
        return false;
      }
    }
    return true;
  };

  if (expr.isBinding) {
    if (!rewriteCanonicalExperimentalVectorConstructorBinding(expr)) {
      return false;
    }
    if (!rewriteCanonicalExperimentalMapConstructorBinding(expr)) {
      return false;
    }
  }

  std::function<bool(const Definition &, bool)> rewriteCanonicalExperimentalVectorConstructorArgs =
      [&](const Definition &targetDef, bool methodCallSyntax) -> bool {
    std::vector<ParameterInfo> callParams;
    if (!targetDef.parameters.empty()) {
      callParams.reserve(targetDef.parameters.size());
      for (const auto &paramExpr : targetDef.parameters) {
        ParameterInfo paramInfo;
        paramInfo.name = paramExpr.name;
        inferCallTargetBinding(paramExpr, paramInfo.binding);
        if (paramExpr.args.size() == 1) {
          paramInfo.defaultExpr = &paramExpr.args.front();
        }
        callParams.push_back(std::move(paramInfo));
      }
    } else if (isStructDefinition(targetDef)) {
      for (const auto &fieldExpr : targetDef.statements) {
        if (!fieldExpr.isBinding) {
          continue;
        }
        ParameterInfo fieldInfo;
        fieldInfo.name = fieldExpr.name;
        inferCallTargetBinding(fieldExpr, fieldInfo.binding);
        if (fieldExpr.args.size() == 1) {
          fieldInfo.defaultExpr = &fieldExpr.args.front();
        }
        callParams.push_back(std::move(fieldInfo));
      }
    }
    if (callParams.empty()) {
      return true;
    }
    std::vector<const Expr *> orderedArgs(callParams.size(), nullptr);
    size_t callArgStart = 0;
    if (methodCallSyntax && expr.args.size() == callParams.size() + 1) {
      callArgStart = 1;
    }
    size_t positionalIndex = 0;
    for (size_t i = callArgStart; i < expr.args.size(); ++i) {
      if (i < expr.argNames.size() && expr.argNames[i].has_value()) {
        const std::string &name = *expr.argNames[i];
        size_t index = callParams.size();
        for (size_t p = 0; p < callParams.size(); ++p) {
          if (callParams[p].name == name) {
            index = p;
            break;
          }
        }
        if (index >= callParams.size() || orderedArgs[index] != nullptr) {
          return true;
        }
        orderedArgs[index] = &expr.args[i];
        continue;
      }
      while (positionalIndex < orderedArgs.size() && orderedArgs[positionalIndex] != nullptr) {
        ++positionalIndex;
      }
      if (positionalIndex >= orderedArgs.size()) {
        return true;
      }
      orderedArgs[positionalIndex] = &expr.args[i];
      ++positionalIndex;
    }
    for (size_t paramIndex = 0; paramIndex < callParams.size() && paramIndex < orderedArgs.size(); ++paramIndex) {
      const BindingInfo &paramBinding = callParams[paramIndex].binding;
      std::string typeText = paramBinding.typeName;
      if (!paramBinding.typeTemplateArg.empty()) {
        typeText += "<" + paramBinding.typeTemplateArg + ">";
      }
      const Expr *orderedArg = orderedArgs[paramIndex];
      if (orderedArg == nullptr) {
        continue;
      }
      for (auto &argExpr : expr.args) {
        if (&argExpr != orderedArg) {
          continue;
        }
        if (!rewriteExperimentalVectorTargetValueForType(typeText, argExpr)) {
          return false;
        }
        break;
      }
    }
    return true;
  };
  std::function<bool(const Definition &, bool)> rewriteCanonicalExperimentalMapConstructorArgs =
      [&](const Definition &targetDef, bool methodCallSyntax) -> bool {
    std::vector<ParameterInfo> callParams;
    if (!targetDef.parameters.empty()) {
      callParams.reserve(targetDef.parameters.size());
      for (const auto &paramExpr : targetDef.parameters) {
        ParameterInfo paramInfo;
        paramInfo.name = paramExpr.name;
        inferCallTargetBinding(paramExpr, paramInfo.binding);
        if (paramExpr.args.size() == 1) {
          paramInfo.defaultExpr = &paramExpr.args.front();
        }
        callParams.push_back(std::move(paramInfo));
      }
    } else if (isStructDefinition(targetDef)) {
      for (const auto &fieldExpr : targetDef.statements) {
        if (!fieldExpr.isBinding) {
          continue;
        }
        ParameterInfo fieldInfo;
        fieldInfo.name = fieldExpr.name;
        inferCallTargetBinding(fieldExpr, fieldInfo.binding);
        if (fieldExpr.args.size() == 1) {
          fieldInfo.defaultExpr = &fieldExpr.args.front();
        }
        callParams.push_back(std::move(fieldInfo));
      }
    }
    if (callParams.empty()) {
      return true;
    }
    std::vector<const Expr *> orderedArgs(callParams.size(), nullptr);
    size_t callArgStart = 0;
    if (methodCallSyntax && expr.args.size() == callParams.size() + 1) {
      // Method-call sugar prepends the implicit receiver expression.
      callArgStart = 1;
    }
    size_t positionalIndex = 0;
    for (size_t i = callArgStart; i < expr.args.size(); ++i) {
      if (i < expr.argNames.size() && expr.argNames[i].has_value()) {
        const std::string &name = *expr.argNames[i];
        size_t index = callParams.size();
        for (size_t p = 0; p < callParams.size(); ++p) {
          if (callParams[p].name == name) {
            index = p;
            break;
          }
        }
        if (index >= callParams.size() || orderedArgs[index] != nullptr) {
          return true;
        }
        orderedArgs[index] = &expr.args[i];
        continue;
      }
      while (positionalIndex < orderedArgs.size() && orderedArgs[positionalIndex] != nullptr) {
        ++positionalIndex;
      }
      if (positionalIndex >= orderedArgs.size()) {
        return true;
      }
      orderedArgs[positionalIndex] = &expr.args[i];
      ++positionalIndex;
    }
    for (size_t paramIndex = 0; paramIndex < callParams.size() && paramIndex < orderedArgs.size(); ++paramIndex) {
      const BindingInfo &paramBinding = callParams[paramIndex].binding;
      std::string typeText = paramBinding.typeName;
      if (!paramBinding.typeTemplateArg.empty()) {
        typeText += "<" + paramBinding.typeTemplateArg + ">";
      }
      const Expr *orderedArg = orderedArgs[paramIndex];
      if (orderedArg == nullptr) {
        continue;
      }
      for (auto &argExpr : expr.args) {
        if (&argExpr != orderedArg) {
          continue;
        }
        if (!rewriteExperimentalMapTargetValueForType(typeText, argExpr)) {
          return false;
        }
        break;
      }
    }
    return true;
  };

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
    if (Expr *receiverExpr = mutableMapHelperReceiverExpr(expr)) {
      if (!rewriteNestedExperimentalMapConstructorValue(*receiverExpr)) {
        return false;
      }
    }
    std::string resolvedPath = resolveCalleePath(expr, namespacePrefix, ctx);
    const std::string borrowedCanonicalMapUnknownTarget = canonicalMapHelperUnknownTargetPath(resolvedPath);
    if (!borrowedCanonicalMapUnknownTarget.empty() &&
        resolvesExperimentalMapBorrowedReceiver(mapHelperReceiverExpr(expr))) {
      error = "unknown call target: " + borrowedCanonicalMapUnknownTarget;
      return false;
    }
    const std::string experimentalMapPath = experimentalMapHelperPathForCanonicalHelper(resolvedPath);
    if (!experimentalMapPath.empty() && ctx.sourceDefs.count(experimentalMapPath) > 0 &&
        resolvesExperimentalMapValueReceiver(mapHelperReceiverExpr(expr))) {
      resolvedPath = experimentalMapPath;
      expr.name = experimentalMapPath;
      expr.namespacePrefix.clear();
      if (expr.templateArgs.empty()) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalMapValueReceiverTemplateArgs(mapHelperReceiverExpr(expr), receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
        }
      }
      if (Expr *receiverExpr = mutableMapHelperReceiverExpr(expr)) {
        if (!rewriteNestedExperimentalMapConstructorValue(*receiverExpr)) {
          return false;
        }
      }
    }
    const std::string experimentalWrapperMapPath = experimentalMapHelperPathForWrapperHelper(resolvedPath);
    if (!experimentalWrapperMapPath.empty() && ctx.sourceDefs.count(experimentalWrapperMapPath) > 0 &&
        resolvesExperimentalMapValueReceiver(mapHelperReceiverExpr(expr))) {
      resolvedPath = experimentalWrapperMapPath;
      expr.name = experimentalWrapperMapPath;
      expr.namespacePrefix.clear();
      if (expr.templateArgs.empty()) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalMapValueReceiverTemplateArgs(mapHelperReceiverExpr(expr), receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
        }
      }
      if (Expr *receiverExpr = mutableMapHelperReceiverExpr(expr)) {
        if (!rewriteNestedExperimentalMapConstructorValue(*receiverExpr)) {
          return false;
        }
      }
    }
    const std::string experimentalVectorPath = experimentalVectorHelperPathForCanonicalHelper(resolvedPath);
    if (!experimentalVectorPath.empty() && ctx.sourceDefs.count(experimentalVectorPath) > 0 &&
        resolvesExperimentalVectorValueReceiver(mapHelperReceiverExpr(expr))) {
      resolvedPath = experimentalVectorPath;
      expr.name = experimentalVectorPath;
      expr.namespacePrefix.clear();
      if (expr.templateArgs.empty()) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalVectorValueReceiverTemplateArgs(mapHelperReceiverExpr(expr), receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
        }
      }
    }
    const std::string originalResolvedPath = resolvedPath;
    const std::string preferredPath = preferVectorStdlibHelperPath(resolvedPath, ctx.sourceDefs);
    if (preferredPath != resolvedPath && ctx.sourceDefs.count(preferredPath) > 0) {
      resolvedPath = preferredPath;
      expr.name = preferredPath;
    }
    const bool explicitCompatibilityAliasToCanonicalTemplate =
        expr.templateArgs.empty() && isExplicitCollectionCompatibilityAliasPath(originalResolvedPath) &&
        preferredPath != originalResolvedPath && ctx.templateDefs.count(preferredPath) > 0;
    const bool resolvedWasTemplate = ctx.templateDefs.count(resolvedPath) > 0;
    const bool isBuiltinMapCountPath =
        resolvedPath == "/std/collections/map/count" || resolvedPath == "/map/count";
    const bool isKnownDef = ctx.sourceDefs.count(resolvedPath) > 0;
    if (!expr.templateArgs.empty() && !resolvedWasTemplate && !isKnownDef && isBuiltinMapCountPath) {
      error = "count does not accept template arguments";
      return false;
    }
    if (!expr.templateArgs.empty() && !resolvedWasTemplate) {
      if (!shouldPreserveCompatibilityTemplatePath(resolvedPath, ctx) &&
          !shouldPreserveCanonicalMapTemplatePath(resolvedPath, ctx)) {
        const std::string templatePreferredPath = preferVectorStdlibTemplatePath(resolvedPath, ctx);
        if (templatePreferredPath != resolvedPath) {
          resolvedPath = templatePreferredPath;
          expr.name = templatePreferredPath;
        }
      }
    }
    if (expr.templateArgs.empty() && !explicitCompatibilityAliasToCanonicalTemplate) {
      const std::string implicitTemplatePreferredPath =
          preferVectorStdlibImplicitTemplatePath(expr, resolvedPath, locals, params, allowMathBare, ctx, namespacePrefix);
      if (implicitTemplatePreferredPath != resolvedPath) {
        resolvedPath = implicitTemplatePreferredPath;
        expr.name = implicitTemplatePreferredPath;
      }
    }
    if (ctx.helperOverloadInternalToPublic.count(resolvedPath) > 0) {
      expr.name = resolvedPath;
      expr.namespacePrefix.clear();
    }
    const bool isTemplateDef = ctx.templateDefs.count(resolvedPath) > 0;
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
          } else {
            defIt = ctx.sourceDefs.find(resolvedPath);
            if (defIt == ctx.sourceDefs.end()) {
              return false;
            }
            if (error.empty() &&
                inferStdlibCollectionHelperTemplateArgs(defIt->second,
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
      }
      if (expr.templateArgs.empty()) {
        if (shouldDeferStdlibCollectionHelperTemplateRewrite(resolvedPath)) {
          return true;
        }
        error = "template arguments required for " + helperOverloadDisplayPath(resolvedPath, ctx);
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
      error = "template arguments are only supported on templated definitions: " +
              helperOverloadDisplayPath(resolvedPath, ctx);
      return false;
    }
    auto defIt = ctx.sourceDefs.find(resolveCalleePath(expr, namespacePrefix, ctx));
    if (defIt != ctx.sourceDefs.end()) {
      if (!rewriteCanonicalExperimentalVectorConstructorArgs(defIt->second, false)) {
        return false;
      }
      if (!rewriteCanonicalExperimentalMapConstructorArgs(defIt->second, false)) {
        return false;
      }
    }
    auto rewriteCanonicalExperimentalVectorConstructorAssign = [&]() {
      if (!isAssignCall(expr) || expr.args.size() != 2) {
        return;
      }
      BindingInfo targetInfo;
      if (!resolveAssignmentTargetBinding(expr.args.front(), targetInfo)) {
        return;
      }
      std::string targetTypeText = targetInfo.typeName;
      if (!targetInfo.typeTemplateArg.empty()) {
        targetTypeText += "<" + targetInfo.typeTemplateArg + ">";
      }
      Expr &valueExpr = expr.args[1];
      (void)rewriteExperimentalVectorTargetValueForType(targetTypeText, valueExpr);
    };
    auto rewriteCanonicalExperimentalVectorConstructorInit = [&]() {
      if (!isSimpleCallName(expr, "init") || expr.args.size() != 2) {
        return;
      }
      BindingInfo targetInfo;
      if (!resolveAssignmentTargetBinding(expr.args.front(), targetInfo)) {
        return;
      }
      std::string targetTypeText = targetInfo.typeName;
      if (!targetInfo.typeTemplateArg.empty()) {
        targetTypeText += "<" + targetInfo.typeTemplateArg + ">";
      }
      Expr &valueExpr = expr.args[1];
      (void)rewriteExperimentalVectorTargetValueForType(targetTypeText, valueExpr);
    };
    auto rewriteCanonicalExperimentalMapConstructorAssign = [&]() {
      if (!isAssignCall(expr) || expr.args.size() != 2) {
        return;
      }
      BindingInfo targetInfo;
      if (!resolveAssignmentTargetBinding(expr.args.front(), targetInfo)) {
        return;
      }
      std::string targetTypeText = targetInfo.typeName;
      if (!targetInfo.typeTemplateArg.empty()) {
        targetTypeText += "<" + targetInfo.typeTemplateArg + ">";
      }
      Expr &valueExpr = expr.args[1];
      if (!rewriteExperimentalMapTargetValueForType(targetTypeText, valueExpr)) {
        return;
      }
    };
    auto rewriteCanonicalExperimentalMapConstructorInit = [&]() {
      if (!isSimpleCallName(expr, "init") || expr.args.size() != 2) {
        return;
      }
      BindingInfo targetInfo;
      if (!resolveAssignmentTargetBinding(expr.args.front(), targetInfo)) {
        return;
      }
      std::string targetTypeText = targetInfo.typeName;
      if (!targetInfo.typeTemplateArg.empty()) {
        targetTypeText += "<" + targetInfo.typeTemplateArg + ">";
      }
      Expr &valueExpr = expr.args[1];
      if (!rewriteExperimentalMapTargetValueForType(targetTypeText, valueExpr)) {
        return;
      }
    };
    rewriteCanonicalExperimentalVectorConstructorAssign();
    rewriteCanonicalExperimentalVectorConstructorInit();
    rewriteCanonicalExperimentalMapConstructorAssign();
    rewriteCanonicalExperimentalMapConstructorInit();
  }
  if (expr.isMethodCall) {
    if (!expr.args.empty()) {
      if (!rewriteNestedExperimentalMapConstructorValue(expr.args.front())) {
        return false;
      }
    }
    const bool methodCallSyntax = expr.isMethodCall;
    std::string methodPath;
    if (resolveMethodCallTemplateTarget(expr, locals, ctx, methodPath)) {
      const std::string experimentalVectorMethodPath =
          experimentalVectorHelperPathForCanonicalHelper(methodPath);
      if (!experimentalVectorMethodPath.empty() &&
          ctx.sourceDefs.count(experimentalVectorMethodPath) > 0 &&
          resolvesExperimentalVectorValueReceiver(mapHelperReceiverExpr(expr))) {
        methodPath = experimentalVectorMethodPath;
        if (expr.templateArgs.empty()) {
          std::vector<std::string> receiverTemplateArgs;
          if (resolveExperimentalVectorValueReceiverTemplateArgs(
                  mapHelperReceiverExpr(expr), receiverTemplateArgs)) {
            expr.templateArgs = std::move(receiverTemplateArgs);
            allConcrete = true;
          }
        }
      }
      const bool methodWasTemplate = ctx.templateDefs.count(methodPath) > 0;
      if (!expr.templateArgs.empty() && !methodWasTemplate) {
        if (!shouldPreserveCompatibilityTemplatePath(methodPath, ctx) &&
            !shouldPreserveCanonicalMapTemplatePath(methodPath, ctx)) {
          methodPath = preferVectorStdlibTemplatePath(methodPath, ctx);
        }
      }
      if (expr.templateArgs.empty()) {
        methodPath =
            preferVectorStdlibImplicitTemplatePath(expr, methodPath, locals, params, allowMathBare, ctx, namespacePrefix);
      }
      if (ctx.helperOverloadInternalToPublic.count(methodPath) > 0) {
        expr.name = methodPath;
        expr.namespacePrefix.clear();
        expr.isMethodCall = false;
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
            } else {
              defIt = ctx.sourceDefs.find(methodPath);
              if (defIt == ctx.sourceDefs.end()) {
                return false;
              }
              if (error.empty() &&
                  inferStdlibCollectionHelperTemplateArgs(defIt->second,
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
        }
        if (expr.templateArgs.empty()) {
          if (shouldDeferStdlibCollectionHelperTemplateRewrite(methodPath)) {
            return true;
          }
          error = "template arguments required for " + helperOverloadDisplayPath(methodPath, ctx);
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
        error = "template arguments are only supported on templated definitions: " +
                helperOverloadDisplayPath(methodPath, ctx);
        return false;
      }
      auto methodDefIt = ctx.sourceDefs.find(resolveCalleePath(expr, namespacePrefix, ctx));
      if (methodDefIt != ctx.sourceDefs.end()) {
        if (!rewriteCanonicalExperimentalVectorConstructorArgs(methodDefIt->second, methodCallSyntax)) {
          return false;
        }
        if (!rewriteCanonicalExperimentalMapConstructorArgs(methodDefIt->second, methodCallSyntax)) {
          return false;
        }
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
      if (info.typeName == "auto" && arg.args.size() == 1 &&
          inferBindingTypeForMonomorph(arg.args.front(), params, bodyLocals, allowMathBare, ctx, info)) {
        bodyLocals[arg.name] = info;
      } else {
        bodyLocals[arg.name] = info;
      }
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
  LocalTypeMap locals;
  auto experimentalMapConstructorHelperPath = [&](size_t argumentCount) -> std::string {
    switch (argumentCount) {
    case 0:
      return "/std/collections/experimental_map/mapNew";
    case 2:
      return "/std/collections/experimental_map/mapSingle";
    case 4:
      return "/std/collections/experimental_map/mapPair";
    case 6:
      return "/std/collections/experimental_map/mapTriple";
    case 8:
      return "/std/collections/experimental_map/mapQuad";
    case 10:
      return "/std/collections/experimental_map/mapQuint";
    case 12:
      return "/std/collections/experimental_map/mapSext";
    case 14:
      return "/std/collections/experimental_map/mapSept";
    case 16:
      return "/std/collections/experimental_map/mapOct";
    default:
      return {};
    }
  };
  auto experimentalMapConstructorRewritePath =
      [&](const std::string &resolvedPath, size_t argumentCount) -> std::string {
    if (resolvedPath == "/std/collections/map/map") {
      return experimentalMapConstructorHelperPath(argumentCount);
    }
    if (resolvedPath == "/std/collections/mapNew") {
      return "/std/collections/experimental_map/mapNew";
    }
    if (resolvedPath == "/std/collections/mapSingle") {
      return "/std/collections/experimental_map/mapSingle";
    }
    if (resolvedPath == "/std/collections/mapDouble") {
      return "/std/collections/experimental_map/mapDouble";
    }
    if (resolvedPath == "/std/collections/mapPair") {
      return "/std/collections/experimental_map/mapPair";
    }
    if (resolvedPath == "/std/collections/mapTriple") {
      return "/std/collections/experimental_map/mapTriple";
    }
    if (resolvedPath == "/std/collections/mapQuad") {
      return "/std/collections/experimental_map/mapQuad";
    }
    if (resolvedPath == "/std/collections/mapQuint") {
      return "/std/collections/experimental_map/mapQuint";
    }
    if (resolvedPath == "/std/collections/mapSext") {
      return "/std/collections/experimental_map/mapSext";
    }
    if (resolvedPath == "/std/collections/mapSept") {
      return "/std/collections/experimental_map/mapSept";
    }
    if (resolvedPath == "/std/collections/mapOct") {
      return "/std/collections/experimental_map/mapOct";
    }
    return {};
  };
  auto rewriteCanonicalExperimentalMapConstructorValue = [&](Expr &valueExpr) {
    if (valueExpr.kind != Expr::Kind::Call || valueExpr.isBinding || valueExpr.isMethodCall) {
      return;
    }
    const std::string originalPath = resolveCalleePath(valueExpr, def.namespacePrefix, ctx);
    const std::string helperPath =
        experimentalMapConstructorRewritePath(originalPath, valueExpr.args.size());
    if (helperPath.empty() || ctx.sourceDefs.count(helperPath) == 0) {
      return;
    }
    valueExpr.name = helperPath;
    valueExpr.namespacePrefix.clear();
    if (valueExpr.templateArgs.empty()) {
      auto defIt = ctx.sourceDefs.find(helperPath);
      if (defIt != ctx.sourceDefs.end()) {
        std::vector<std::string> inferredArgs;
        std::string inferError;
        if (inferImplicitTemplateArgs(defIt->second,
                                      valueExpr,
                                      locals,
                                      params,
                                      mapping,
                                      allowedParams,
                                      def.namespacePrefix,
                                      ctx,
                                      allowMathBare,
                                      inferredArgs,
                                      inferError)) {
          valueExpr.templateArgs = std::move(inferredArgs);
        } else {
          defIt = ctx.sourceDefs.find(helperPath);
          if (defIt == ctx.sourceDefs.end()) {
            return;
          }
          if (inferError.empty() &&
              inferStdlibCollectionHelperTemplateArgs(defIt->second,
                                                      valueExpr,
                                                      locals,
                                                      params,
                                                      mapping,
                                                      allowedParams,
                                                      def.namespacePrefix,
                                                      ctx,
                                                      allowMathBare,
                                                      inferredArgs)) {
          valueExpr.templateArgs = std::move(inferredArgs);
          } else if (!inferError.empty()) {
            error = inferError;
            const size_t helperPos = error.find(helperPath);
            if (helperPos != std::string::npos) {
              error.replace(helperPos, helperPath.size(), originalPath);
            }
          }
        }
      }
    }
  };
  auto experimentalVectorConstructorRewritePath =
      [&](const std::string &resolvedPath, size_t argumentCount) -> std::string {
    (void)argumentCount;
    if (resolvedPath == "/std/collections/vectorNew") {
      return "/std/collections/experimental_vector/vectorNew";
    }
    if (resolvedPath == "/std/collections/vectorSingle") {
      return "/std/collections/experimental_vector/vectorSingle";
    }
    if (resolvedPath == "/std/collections/vectorPair") {
      return "/std/collections/experimental_vector/vectorPair";
    }
    if (resolvedPath == "/std/collections/vectorTriple") {
      return "/std/collections/experimental_vector/vectorTriple";
    }
    if (resolvedPath == "/std/collections/vectorQuad") {
      return "/std/collections/experimental_vector/vectorQuad";
    }
    if (resolvedPath == "/std/collections/vectorQuint") {
      return "/std/collections/experimental_vector/vectorQuint";
    }
    if (resolvedPath == "/std/collections/vectorSext") {
      return "/std/collections/experimental_vector/vectorSext";
    }
    if (resolvedPath == "/std/collections/vectorSept") {
      return "/std/collections/experimental_vector/vectorSept";
    }
    if (resolvedPath == "/std/collections/vectorOct") {
      return "/std/collections/experimental_vector/vectorOct";
    }
    return {};
  };
  auto rewriteCanonicalExperimentalVectorConstructorValue = [&](Expr &valueExpr) {
    if (valueExpr.kind != Expr::Kind::Call || valueExpr.isBinding || valueExpr.isMethodCall) {
      return;
    }
    const std::string originalPath = resolveCalleePath(valueExpr, def.namespacePrefix, ctx);
    const std::string helperPath =
        experimentalVectorConstructorRewritePath(originalPath, valueExpr.args.size());
    if (helperPath.empty() || ctx.sourceDefs.count(helperPath) == 0) {
      return;
    }
    valueExpr.name = helperPath;
    valueExpr.namespacePrefix.clear();
    if (valueExpr.templateArgs.empty()) {
      auto defIt = ctx.sourceDefs.find(helperPath);
      if (defIt != ctx.sourceDefs.end()) {
        std::vector<std::string> inferredArgs;
        std::string inferError;
        if (inferImplicitTemplateArgs(defIt->second,
                                      valueExpr,
                                      locals,
                                      params,
                                      mapping,
                                      allowedParams,
                                      def.namespacePrefix,
                                      ctx,
                                      allowMathBare,
                                      inferredArgs,
                                      inferError)) {
          valueExpr.templateArgs = std::move(inferredArgs);
        } else {
          defIt = ctx.sourceDefs.find(helperPath);
          if (defIt == ctx.sourceDefs.end()) {
            return;
          }
          if (inferError.empty() &&
              inferStdlibCollectionHelperTemplateArgs(defIt->second,
                                                      valueExpr,
                                                      locals,
                                                      params,
                                                      mapping,
                                                      allowedParams,
                                                      def.namespacePrefix,
                                                      ctx,
                                                      allowMathBare,
                                                      inferredArgs)) {
            valueExpr.templateArgs = std::move(inferredArgs);
          } else if (!inferError.empty()) {
            error = inferError;
            const size_t helperPos = error.find(helperPath);
            if (helperPos != std::string::npos) {
              error.replace(helperPos, helperPath.size(), originalPath);
            }
          }
        }
      }
    }
  };
  bool hasExplicitNonAutoReturn = false;
  bool expectedExperimentalVectorReturn = [&]() {
    for (const auto &transform : def.transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      if (resolvesExperimentalVectorValueTypeText(transform.templateArgs.front())) {
        return true;
      }
    }
    return false;
  }();
  bool expectedExperimentalMapReturn = [&]() {
    for (const auto &transform : def.transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      if (transform.templateArgs.front() != "auto") {
        hasExplicitNonAutoReturn = true;
      }
      if (resolvesExperimentalMapValueTypeText(transform.templateArgs.front(),
                                               mapping,
                                               allowedParams,
                                               def.namespacePrefix,
                                               ctx)) {
        return true;
      }
    }
    return false;
  }();
  if (!expectedExperimentalVectorReturn && !expectedExperimentalMapReturn && !hasExplicitNonAutoReturn) {
    BindingInfo inferredReturnInfo;
    if (inferDefinitionReturnBindingForTemplatedFallback(def, allowMathBare, ctx, inferredReturnInfo)) {
      std::string inferredReturnType = inferredReturnInfo.typeName;
      if (!inferredReturnInfo.typeTemplateArg.empty()) {
        inferredReturnType += "<" + inferredReturnInfo.typeTemplateArg + ">";
      }
      expectedExperimentalVectorReturn =
          resolvesExperimentalVectorValueTypeText(inferredReturnType);
      expectedExperimentalMapReturn =
          resolvesExperimentalMapValueTypeText(inferredReturnType, mapping, allowedParams, def.namespacePrefix, ctx);
    }
  }
  auto rewriteCanonicalExperimentalVectorReturnConstructors = [&](auto &self, Expr &candidate) -> void {
    rewriteCanonicalExperimentalVectorConstructorValue(candidate);
    for (auto &arg : candidate.args) {
      if (arg.kind == Expr::Kind::Call) {
        self(self, arg);
      }
    }
    bool sawExplicitReturn = false;
    size_t implicitReturnIndex = candidate.bodyArguments.size();
    for (size_t argIndex = 0; argIndex < candidate.bodyArguments.size(); ++argIndex) {
      if (isReturnCall(candidate.bodyArguments[argIndex])) {
        sawExplicitReturn = true;
        break;
      }
      if (!candidate.bodyArguments[argIndex].isBinding) {
        implicitReturnIndex = argIndex;
      }
    }
    for (size_t argIndex = 0; argIndex < candidate.bodyArguments.size(); ++argIndex) {
      Expr &arg = candidate.bodyArguments[argIndex];
      if (isReturnCall(arg) || (!sawExplicitReturn && argIndex == implicitReturnIndex)) {
        self(self, arg);
      }
    }
  };
  auto rewriteCanonicalExperimentalMapReturnConstructors = [&](auto &self, Expr &candidate) -> void {
    rewriteCanonicalExperimentalMapConstructorValue(candidate);
    for (auto &arg : candidate.args) {
      if (arg.kind == Expr::Kind::Call) {
        self(self, arg);
      }
    }
    bool sawExplicitReturn = false;
    size_t implicitReturnIndex = candidate.bodyArguments.size();
    for (size_t argIndex = 0; argIndex < candidate.bodyArguments.size(); ++argIndex) {
      if (isReturnCall(candidate.bodyArguments[argIndex])) {
        sawExplicitReturn = true;
        break;
      }
      if (!candidate.bodyArguments[argIndex].isBinding) {
        implicitReturnIndex = argIndex;
      }
    }
    for (size_t argIndex = 0; argIndex < candidate.bodyArguments.size(); ++argIndex) {
      Expr &arg = candidate.bodyArguments[argIndex];
      if (isReturnCall(arg) || (!sawExplicitReturn && argIndex == implicitReturnIndex)) {
        self(self, arg);
      }
    }
  };
  params.reserve(def.parameters.size());
  locals.reserve(def.parameters.size() + def.statements.size());
  bool sawExplicitReturnStmt = false;
  size_t implicitReturnStmtIndex = def.statements.size();
  for (size_t stmtIndex = 0; stmtIndex < def.statements.size(); ++stmtIndex) {
    if (isReturnCall(def.statements[stmtIndex])) {
      sawExplicitReturnStmt = true;
      break;
    }
    if (!def.statements[stmtIndex].isBinding) {
      implicitReturnStmtIndex = stmtIndex;
    }
  }
  for (auto &param : def.parameters) {
    if (!rewriteExpr(param, mapping, allowedParams, def.namespacePrefix, ctx, error, locals, params, allowMathBare)) {
      return false;
    }
    BindingInfo info;
    if (extractExplicitBindingType(param, info)) {
      if (info.typeName == "auto" && param.args.size() == 1 &&
          inferBindingTypeForMonomorph(param.args.front(), {}, {}, allowMathBare, ctx, info)) {
        locals[param.name] = info;
      } else {
        locals[param.name] = info;
      }
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
  for (size_t stmtIndex = 0; stmtIndex < def.statements.size(); ++stmtIndex) {
    auto &stmt = def.statements[stmtIndex];
    const bool isReturnPathStmt =
        isReturnCall(stmt) || (!sawExplicitReturnStmt && stmtIndex == implicitReturnStmtIndex);
    if (expectedExperimentalVectorReturn && isReturnPathStmt) {
      rewriteCanonicalExperimentalVectorReturnConstructors(rewriteCanonicalExperimentalVectorReturnConstructors, stmt);
      if (!error.empty()) {
        return false;
      }
    }
    if (expectedExperimentalMapReturn && isReturnPathStmt) {
      rewriteCanonicalExperimentalMapReturnConstructors(rewriteCanonicalExperimentalMapReturnConstructors, stmt);
      if (!error.empty()) {
        return false;
      }
    }
    if (!rewriteExpr(stmt, mapping, allowedParams, def.namespacePrefix, ctx, error, locals, params, allowMathBare)) {
      return false;
    }
    if (expectedExperimentalVectorReturn && isReturnPathStmt) {
      rewriteCanonicalExperimentalVectorReturnConstructors(rewriteCanonicalExperimentalVectorReturnConstructors, stmt);
      if (!error.empty()) {
        return false;
      }
    }
    if (expectedExperimentalMapReturn && isReturnPathStmt) {
      rewriteCanonicalExperimentalMapReturnConstructors(rewriteCanonicalExperimentalMapReturnConstructors, stmt);
      if (!error.empty()) {
        return false;
      }
    }
    BindingInfo info;
    if (extractExplicitBindingType(stmt, info)) {
      if (info.typeName == "auto" && stmt.args.size() == 1 &&
          inferBindingTypeForMonomorph(stmt.args.front(), params, locals, allowMathBare, ctx, info)) {
        locals[stmt.name] = info;
      } else {
        locals[stmt.name] = info;
      }
    } else if (stmt.isBinding && stmt.args.size() == 1) {
      if (inferBindingTypeForMonomorph(stmt.args.front(), params, locals, allowMathBare, ctx, info)) {
        locals[stmt.name] = info;
      }
    }
  }
  if (def.returnExpr.has_value()) {
    if (expectedExperimentalVectorReturn) {
      rewriteCanonicalExperimentalVectorReturnConstructors(rewriteCanonicalExperimentalVectorReturnConstructors,
                                                          *def.returnExpr);
      if (!error.empty()) {
        return false;
      }
    }
    if (expectedExperimentalMapReturn) {
      rewriteCanonicalExperimentalMapReturnConstructors(rewriteCanonicalExperimentalMapReturnConstructors,
                                                       *def.returnExpr);
      if (!error.empty()) {
        return false;
      }
    }
    if (!rewriteExpr(*def.returnExpr, mapping, allowedParams, def.namespacePrefix, ctx, error, locals, params,
                     allowMathBare)) {
      return false;
    }
    if (expectedExperimentalVectorReturn) {
      rewriteCanonicalExperimentalVectorReturnConstructors(rewriteCanonicalExperimentalVectorReturnConstructors,
                                                          *def.returnExpr);
      if (!error.empty()) {
        return false;
      }
    }
    if (expectedExperimentalMapReturn) {
      rewriteCanonicalExperimentalMapReturnConstructors(rewriteCanonicalExperimentalMapReturnConstructors,
                                                       *def.returnExpr);
      if (!error.empty()) {
        return false;
      }
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
    error = "template arguments required for " +
            helperOverloadDisplayPath(resolveCalleePath(execExpr, exec.namespacePrefix, ctx), ctx);
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
    error = "template arguments are only supported on templated definitions: " +
            helperOverloadDisplayPath(basePath, ctx);
    return false;
  }
  if (baseDef.templateArgs.size() != resolvedArgs.size()) {
    std::ostringstream out;
    out << "template argument count mismatch for " << helperOverloadDisplayPath(basePath, ctx) << ": expected "
        << baseDef.templateArgs.size()
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
      for (const auto &[publicPath, overloads] : ctx.helperOverloads) {
        (void)overloads;
        if (publicPath.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        const std::string remainder = publicPath.substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string::npos) {
          continue;
        }
        ctx.importAliases.emplace(remainder, publicPath);
      }
      for (const auto &entry : ctx.sourceDefs) {
        const std::string &path = entry.first;
        if (ctx.helperOverloadInternalToPublic.count(path) > 0) {
          continue;
        }
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
      if (ctx.helperOverloads.count(importPath) == 0) {
        continue;
      }
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
  Context ctx{program, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}};
  ctx.sourceDefs.clear();
  ctx.templateDefs.clear();
  ctx.outputDefs.clear();
  ctx.outputExecs.clear();
  ctx.outputPaths.clear();
  ctx.implicitTemplateDefs.clear();
  ctx.implicitTemplateParams.clear();
  ctx.helperOverloads.clear();
  ctx.helperOverloadInternalToPublic.clear();

  if (!applyImplicitAutoTemplates(program, ctx, error)) {
    return false;
  }

  std::unordered_set<std::string> templateRoots;
  templateRoots.clear();
  std::unordered_map<std::string, std::vector<const Definition *>> definitionsByPath;
  definitionsByPath.reserve(program.definitions.size());
  std::unordered_set<std::string> occupiedPaths;
  occupiedPaths.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    definitionsByPath[def.fullPath].push_back(&def);
    occupiedPaths.insert(def.fullPath);
  }
  for (const auto &[publicPath, family] : definitionsByPath) {
    if (family.size() == 1) {
      const Definition &def = *family.front();
      ctx.sourceDefs.emplace(def.fullPath, def);
      if (!def.templateArgs.empty()) {
        ctx.templateDefs.insert(def.fullPath);
        templateRoots.insert(def.fullPath);
      }
      continue;
    }

    bool allowHelperOverloadFamily = true;
    std::unordered_set<size_t> seenParameterCounts;
    std::vector<HelperOverloadEntry> overloads;
    overloads.reserve(family.size());
    for (const Definition *def : family) {
      if (isStructDefinition(*def)) {
        allowHelperOverloadFamily = false;
        break;
      }
      const size_t parameterCount = def->parameters.size();
      if (!seenParameterCounts.insert(parameterCount).second) {
        allowHelperOverloadFamily = false;
        break;
      }
      const std::string internalPath = helperOverloadInternalPath(publicPath, parameterCount);
      if (occupiedPaths.count(internalPath) > 0) {
        error = "helper overload internal path conflicts with existing definition: " + internalPath;
        return false;
      }
      overloads.push_back({internalPath, parameterCount});
    }
    if (!allowHelperOverloadFamily) {
      error = "duplicate definition: " + publicPath;
      return false;
    }
    std::stable_sort(overloads.begin(),
                     overloads.end(),
                     [](const HelperOverloadEntry &left, const HelperOverloadEntry &right) {
                       return left.parameterCount < right.parameterCount;
                     });
    ctx.helperOverloads.emplace(publicPath, overloads);
    for (const Definition *def : family) {
      std::string internalPath;
      std::string internalName;
      if (!resolveHelperOverloadDefinitionIdentity(*def, ctx, internalPath, internalName)) {
        error = "duplicate definition: " + publicPath;
        return false;
      }
      Definition clone = *def;
      clone.fullPath = internalPath;
      clone.name = internalName;
      ctx.sourceDefs.emplace(clone.fullPath, clone);
      ctx.helperOverloadInternalToPublic.emplace(clone.fullPath, publicPath);
      if (!clone.templateArgs.empty()) {
        ctx.templateDefs.insert(clone.fullPath);
        templateRoots.insert(clone.fullPath);
      }
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
    Definition clone = def;
    std::string overloadInternalPath;
    std::string overloadName;
    if (resolveHelperOverloadDefinitionIdentity(def, ctx, overloadInternalPath, overloadName)) {
      clone.fullPath = std::move(overloadInternalPath);
      clone.name = std::move(overloadName);
    }
    if (isUnderTemplateRoot(clone.fullPath)) {
      continue;
    }
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
