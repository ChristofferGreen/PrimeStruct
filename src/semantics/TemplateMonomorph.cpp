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

ResolvedType resolveTypeStringImpl(std::string input,
                                   const SubstMap &mapping,
                                   const std::unordered_set<std::string> &allowedParams,
                                   const std::string &namespacePrefix,
                                   Context &ctx,
                                   std::string &error,
                                   std::unordered_set<std::string> &substitutionStack);

bool isGeneratedTemplateSpecializationPath(std::string_view path);
bool isEnclosingTemplateParamName(const std::string &name,
                                  const std::string &namespacePrefix,
                                  const Context &ctx);

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

bool importPathCoversTarget(const std::string &importPath, const std::string &targetPath) {
  if (importPath.empty() || importPath.front() != '/') {
    return false;
  }
  if (importPath == targetPath) {
    return true;
  }
  if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
    const std::string prefix = importPath.substr(0, importPath.size() - 2);
    return targetPath == prefix || targetPath.rfind(prefix + "/", 0) == 0;
  }
  return false;
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

std::string experimentalVectorConstructorInferencePath(const std::string &resolvedPath) {
  auto matchesPath = [&](std::string_view basePath) {
    return resolvedPath == basePath || resolvedPath.rfind(std::string(basePath) + "__t", 0) == 0;
  };
  if (matchesPath("/std/collections/vectorNew")) {
    return "/std/collections/experimental_vector/vectorNew";
  }
  if (matchesPath("/std/collections/vectorSingle")) {
    return "/std/collections/experimental_vector/vectorSingle";
  }
  if (matchesPath("/std/collections/vectorPair")) {
    return "/std/collections/experimental_vector/vectorPair";
  }
  if (matchesPath("/std/collections/vectorTriple")) {
    return "/std/collections/experimental_vector/vectorTriple";
  }
  if (matchesPath("/std/collections/vectorQuad")) {
    return "/std/collections/experimental_vector/vectorQuad";
  }
  if (matchesPath("/std/collections/vectorQuint")) {
    return "/std/collections/experimental_vector/vectorQuint";
  }
  if (matchesPath("/std/collections/vectorSext")) {
    return "/std/collections/experimental_vector/vectorSext";
  }
  if (matchesPath("/std/collections/vectorSept")) {
    return "/std/collections/experimental_vector/vectorSept";
  }
  if (matchesPath("/std/collections/vectorOct")) {
    return "/std/collections/experimental_vector/vectorOct";
  }
  return {};
}

bool isExperimentalVectorConstructorHelperPath(const std::string &resolvedPath) {
  auto matchesPath = [&](std::string_view basePath) {
    return resolvedPath == basePath || resolvedPath.rfind(std::string(basePath) + "__t", 0) == 0;
  };
  return matchesPath("/std/collections/experimental_vector/vectorNew") ||
         matchesPath("/std/collections/experimental_vector/vectorSingle") ||
         matchesPath("/std/collections/experimental_vector/vectorPair") ||
         matchesPath("/std/collections/experimental_vector/vectorTriple") ||
         matchesPath("/std/collections/experimental_vector/vectorQuad") ||
         matchesPath("/std/collections/experimental_vector/vectorQuint") ||
         matchesPath("/std/collections/experimental_vector/vectorSext") ||
         matchesPath("/std/collections/experimental_vector/vectorSept") ||
         matchesPath("/std/collections/experimental_vector/vectorOct");
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

bool extractVectorValueTypeFromTypeText(const std::string &typeText, std::string &valueTypeOut) {
  valueTypeOut.clear();
  std::string normalizedType = normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(normalizedType, base, argText) || argText.empty()) {
    return false;
  }
  const std::string normalizedBase = normalizeBindingTypeName(base);
  if (normalizedBase != "vector" &&
      normalizedBase != "Vector" &&
      normalizedBase != "/std/collections/experimental_vector/Vector" &&
      normalizedBase != "std/collections/experimental_vector/Vector") {
    return false;
  }
  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
    return false;
  }
  valueTypeOut = args.front();
  return true;
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

bool isGeneratedTemplateSpecializationPath(std::string_view path) {
  const size_t marker = path.rfind("__t");
  if (marker == std::string::npos || marker + 3 >= path.size()) {
    return false;
  }
  for (size_t i = marker + 3; i < path.size(); ++i) {
    if (!std::isxdigit(static_cast<unsigned char>(path[i]))) {
      return false;
    }
  }
  return true;
}

bool isEnclosingTemplateParamName(const std::string &name,
                                  const std::string &namespacePrefix,
                                  const Context &ctx) {
  std::string prefix = namespacePrefix;
  while (!prefix.empty()) {
    auto matchesDefinitionParams = [&](const std::string &path) {
      auto it = ctx.sourceDefs.find(path);
      if (it == ctx.sourceDefs.end()) {
        return false;
      }
      return std::find(it->second.templateArgs.begin(), it->second.templateArgs.end(), name) !=
             it->second.templateArgs.end();
    };
    if (matchesDefinitionParams(prefix)) {
      return true;
    }
    if (isGeneratedTemplateSpecializationPath(prefix)) {
      const size_t marker = prefix.rfind("__t");
      if (marker != std::string::npos && matchesDefinitionParams(prefix.substr(0, marker))) {
        return true;
      }
    }
    const size_t slash = prefix.find_last_of('/');
    if (slash == std::string::npos) {
      break;
    }
    prefix.erase(slash);
  }
  return false;
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
  auto includesMathImport = [](const std::vector<std::string> &importPaths) {
    for (const auto &importPath : importPaths) {
      if (importPath.rfind("/std/math/", 0) == 0 && importPath.size() > 10) {
        return true;
      }
    }
    return false;
  };
  if (includesMathImport(ctx.program.sourceImports)) {
    return true;
  }
  if (includesMathImport(ctx.program.imports)) {
    return true;
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
  auto isStdlibCollectionWrapperImplicitAutoSurface = [](std::string_view path) {
    return path == "/std/collections/vectorCount" ||
           path == "/std/collections/vectorCapacity" ||
           path == "/std/collections/vectorPush" ||
           path == "/std/collections/vectorPop" ||
           path == "/std/collections/vectorReserve" ||
           path == "/std/collections/vectorClear" ||
           path == "/std/collections/vectorRemoveAt" ||
           path == "/std/collections/vectorRemoveSwap" ||
           path == "/std/collections/vectorAt" ||
           path == "/std/collections/vectorAtUnsafe";
  };
  for (auto &def : program.definitions) {
    std::vector<std::string> implicitParams;
    const bool allowTemplatedImplicitAutoParams =
        isStdlibCollectionWrapperImplicitAutoSurface(def.fullPath);
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
        if (extractExplicitBindingType(param, info) &&
            info.typeName == "auto" &&
            !allowTemplatedImplicitAutoParams) {
          error = "implicit auto parameters are only supported on non-templated definitions: " + def.fullPath;
          return false;
        }
      }
      if (!allowTemplatedImplicitAutoParams) {
        continue;
      }
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
  if (value == "Vector" || value.rfind("Vector__", 0) == 0) {
    return "vector";
  }
  if (value == "std/collections/experimental_vector/Vector" ||
      value.rfind("std/collections/experimental_vector/Vector__", 0) == 0) {
    return "vector";
  }
  if (value == "std/collections/map") {
    return "map";
  }
  if (value == "Map" || value.rfind("Map__", 0) == 0) {
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

bool instantiateTemplate(const std::string &basePath,
                         const std::vector<std::string> &resolvedArgs,
                         Context &ctx,
                         std::string &error,
                         std::string &specializedPathOut);

#include "TemplateMonomorphFallbackTypeInference.h"
#include "TemplateMonomorphMethodTargets.h"
#include "TemplateMonomorphBindingCallInference.h"
#include "TemplateMonomorphBindingBlockInference.h"
#include "TemplateMonomorphTypeResolution.h"
#include "TemplateMonomorphCollectionHelperInference.h"
#include "TemplateMonomorphAssignmentTargetResolution.h"
#include "TemplateMonomorphExperimentalCollectionArgumentRewrites.h"
#include "TemplateMonomorphExperimentalCollectionConstructorRewrites.h"
#include "TemplateMonomorphExperimentalCollectionTargetValueRewrites.h"
#include "TemplateMonomorphExperimentalCollectionValueRewrites.h"
#include "TemplateMonomorphExperimentalCollectionReceiverResolution.h"
#include "TemplateMonomorphExperimentalCollectionConstructorPaths.h"
#include "TemplateMonomorphExperimentalCollectionReturnRewrites.h"
#include "TemplateMonomorphExperimentalCollectionReturnSetup.h"
#include "TemplateMonomorphDefinitionBindingSetup.h"
#include "TemplateMonomorphDefinitionReturnOrchestration.h"

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
  if (!callExpr.templateArgs.empty()) {
    if (callExpr.templateArgs.size() > def.templateArgs.size()) {
      error = "template argument count mismatch for " + def.fullPath;
      return false;
    }
    for (size_t i = 0; i < callExpr.templateArgs.size(); ++i) {
      ResolvedType resolvedArg =
          resolveTypeString(callExpr.templateArgs[i], mapping, allowedParams, namespacePrefix, ctx, error);
      if (!error.empty()) {
        return false;
      }
      if (!resolvedArg.concrete) {
        error = "implicit template arguments must be concrete on " + def.fullPath;
        return false;
      }
      inferred.emplace(def.templateArgs[i], resolvedArg.text);
    }
  }
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
  auto resolvesBuiltinVectorReceiver = [&](const Expr *receiverExpr) {
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
      if (receiverType == "vector") {
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
      return normalizeCollectionReceiverTypeName(receiverBase) == "vector";
    }
    return normalizeCollectionReceiverTypeName(inferredReceiverType) == "vector";
  };
  auto shouldDeferStdlibCollectionHelperTemplateRewrite = [&](const std::string &path) {
    if (!expr.templateArgs.empty() || !isCanonicalStdlibCollectionHelperPath(path)) {
      return false;
    }
    if (hasVisibleStdCollectionsImportForPath(ctx, path) && ctx.templateDefs.count(path) > 0) {
      if (path.rfind("/std/collections/vector/", 0) == 0 &&
          !resolvesBuiltinVectorReceiver(mapHelperReceiverExpr(expr)) &&
          !resolvesExperimentalVectorValueReceiver(
              mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
        return true;
      }
      return false;
    }
    if (isCanonicalBuiltinMapHelperPath(path)) {
      return resolvesBuiltinMapReceiver(mapHelperReceiverExpr(expr)) && ctx.templateDefs.count(path) == 0;
    }
    return true;
  };
  std::function<bool(Expr &)> rewriteNestedExperimentalMapConstructorValue;
  std::function<bool(Expr &)> rewriteNestedExperimentalMapResultOkPayloadValue;
  std::function<bool(Expr &)> rewriteNestedExperimentalVectorConstructorValue;
  std::function<bool(const std::string &, Expr &)> rewriteExperimentalMapTargetValueForType;
  std::function<bool(const std::string &, Expr &)> rewriteExperimentalVectorTargetValueForType;

  rewriteExperimentalMapTargetValueForType = [&](const std::string &typeText, Expr &valueExpr) -> bool {
    return ::rewriteExperimentalMapTargetValueForType(typeText,
                                                      valueExpr,
                                                      mapping,
                                                      allowedParams,
                                                      namespacePrefix,
                                                      ctx,
                                                      rewriteNestedExperimentalMapConstructorValue,
                                                      rewriteNestedExperimentalMapResultOkPayloadValue);
  };
  rewriteExperimentalVectorTargetValueForType = [&](const std::string &typeText, Expr &valueExpr) -> bool {
    return ::rewriteExperimentalVectorTargetValueForType(typeText,
                                                         valueExpr,
                                                         rewriteNestedExperimentalVectorConstructorValue);
  };

  auto rewriteCanonicalExperimentalMapConstructorBinding = [&](Expr &bindingExpr) -> bool {
    return rewriteExperimentalConstructorBinding(
        bindingExpr,
        params,
        locals,
        allowMathBare,
        ctx,
        [&](const std::string &bindingTypeText) {
          return resolvesExperimentalMapValueTypeText(bindingTypeText,
                                                      mapping,
                                                      allowedParams,
                                                      namespacePrefix,
                                                      ctx);
        },
        "map",
        rewriteExperimentalMapTargetValueForType);
  };
  auto rewriteCanonicalExperimentalVectorConstructorBinding = [&](Expr &bindingExpr) -> bool {
    return rewriteExperimentalConstructorBinding(
        bindingExpr,
        params,
        locals,
        allowMathBare,
        ctx,
        [&](const std::string &bindingTypeText) {
          return resolvesExperimentalVectorValueTypeText(bindingTypeText);
        },
        "vector",
        rewriteExperimentalVectorTargetValueForType);
  };
  rewriteNestedExperimentalMapConstructorValue = [&](Expr &candidate) -> bool {
    return rewriteExperimentalConstructorValueTree(candidate, [&](Expr &current) {
      return ::rewriteCanonicalExperimentalMapConstructorExpr(current,
                                                             locals,
                                                             params,
                                                             mapping,
                                                             allowedParams,
                                                             namespacePrefix,
                                                             ctx,
                                                             allowMathBare,
                                                             error);
    });
  };

  rewriteNestedExperimentalMapResultOkPayloadValue = [&](Expr &candidate) -> bool {
    return rewriteExperimentalMapResultOkPayloadTree(candidate, rewriteNestedExperimentalMapConstructorValue);
  };
  rewriteNestedExperimentalVectorConstructorValue = [&](Expr &candidate) -> bool {
    return rewriteExperimentalConstructorValueTree(candidate, [&](Expr &current) {
      return ::rewriteCanonicalExperimentalVectorConstructorExpr(current,
                                                                locals,
                                                                params,
                                                                mapping,
                                                                allowedParams,
                                                                namespacePrefix,
                                                                ctx,
                                                                allowMathBare,
                                                                error);
    });
  };

  if (expr.isBinding) {
    if (!rewriteCanonicalExperimentalVectorConstructorBinding(expr)) {
      return false;
    }
    if (!rewriteCanonicalExperimentalMapConstructorBinding(expr)) {
      return false;
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

  if (!expr.isMethodCall && !expr.isBinding) {
    if (Expr *receiverExpr = mutableMapHelperReceiverExpr(expr)) {
      if (!rewriteNestedExperimentalMapConstructorValue(*receiverExpr)) {
        return false;
      }
      if (!rewriteNestedExperimentalVectorConstructorValue(*receiverExpr)) {
        return false;
      }
      if (!rewriteExpr(*receiverExpr,
                       mapping,
                       allowedParams,
                       namespacePrefix,
                       ctx,
                       error,
                       locals,
                       params,
                       allowMathBare)) {
        return false;
      }
    }
    std::string resolvedPath = resolveCalleePath(expr, namespacePrefix, ctx);
    const std::string borrowedCanonicalMapUnknownTarget = canonicalMapHelperUnknownTargetPath(resolvedPath);
    if (!borrowedCanonicalMapUnknownTarget.empty() &&
        resolvesExperimentalMapBorrowedReceiver(
            mapHelperReceiverExpr(expr), params, locals, allowMathBare, mapping, allowedParams, namespacePrefix, ctx)) {
      error = "unknown call target: " + borrowedCanonicalMapUnknownTarget;
      return false;
    }
    const std::string experimentalMapPath = experimentalMapHelperPathForCanonicalHelper(resolvedPath);
    if (!experimentalMapPath.empty() && ctx.sourceDefs.count(experimentalMapPath) > 0 &&
        resolvesExperimentalMapValueReceiver(
            mapHelperReceiverExpr(expr), params, locals, allowMathBare, mapping, allowedParams, namespacePrefix, ctx)) {
      resolvedPath = experimentalMapPath;
      expr.name = experimentalMapPath;
      expr.namespacePrefix.clear();
      if (expr.templateArgs.empty()) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalMapValueReceiverTemplateArgs(
                mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
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
        resolvesExperimentalMapValueReceiver(
            mapHelperReceiverExpr(expr), params, locals, allowMathBare, mapping, allowedParams, namespacePrefix, ctx)) {
      resolvedPath = experimentalWrapperMapPath;
      expr.name = experimentalWrapperMapPath;
      expr.namespacePrefix.clear();
      if (expr.templateArgs.empty()) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalMapValueReceiverTemplateArgs(
                mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
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
        hasVisibleStdCollectionsImportForPath(ctx, resolvedPath) &&
        resolvesExperimentalVectorValueReceiver(
            mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
      resolvedPath = experimentalVectorPath;
      expr.name = experimentalVectorPath;
      expr.namespacePrefix.clear();
      if (expr.templateArgs.empty()) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalVectorValueReceiverTemplateArgs(
                mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
        }
      }
      if (Expr *receiverExpr = mutableMapHelperReceiverExpr(expr)) {
        if (!rewriteNestedExperimentalVectorConstructorValue(*receiverExpr)) {
          return false;
        }
      }
    }
    if (expr.templateArgs.empty() &&
        resolvedPath.rfind("/std/collections/experimental_map/", 0) == 0 &&
        resolvesExperimentalMapValueReceiver(
            mapHelperReceiverExpr(expr), params, locals, allowMathBare, mapping, allowedParams, namespacePrefix, ctx)) {
      std::vector<std::string> receiverTemplateArgs;
      if (resolveExperimentalMapValueReceiverTemplateArgs(
              mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
        expr.templateArgs = std::move(receiverTemplateArgs);
      }
    }
    if (expr.templateArgs.empty() &&
        resolvedPath.rfind("/std/collections/experimental_vector/", 0) == 0 &&
        resolvesExperimentalVectorValueReceiver(
            mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
      std::vector<std::string> receiverTemplateArgs;
      if (resolveExperimentalVectorValueReceiverTemplateArgs(
              mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
        expr.templateArgs = std::move(receiverTemplateArgs);
      }
    }
    if (resolvedPath.rfind("/std/collections/experimental_map/", 0) == 0 &&
        resolvedPath.find("__t") != std::string::npos) {
      expr.templateArgs.clear();
    }
    if (resolvedPath.rfind("/std/collections/experimental_vector/", 0) == 0 &&
        resolvedPath.find("__t") != std::string::npos) {
      expr.templateArgs.clear();
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
      auto defIt = ctx.sourceDefs.find(resolvedPath);
      const bool shouldInferImplicitTemplateTail =
          defIt != ctx.sourceDefs.end() &&
          !expr.templateArgs.empty() &&
          ctx.implicitTemplateDefs.count(resolvedPath) > 0 &&
          expr.templateArgs.size() < defIt->second.templateArgs.size();
      if (expr.templateArgs.empty() || shouldInferImplicitTemplateTail) {
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
              allConcrete = inferredTemplateArgsAreConcrete(defIt->second, inferredArgs);
              expr.templateArgs = std::move(inferredArgs);
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
      if (!rewriteExperimentalConstructorArgsForTarget(
              expr,
              defIt->second,
              false,
              allowMathBare,
              ctx,
              [&](const std::string &typeText, Expr &argExpr) {
                return rewriteExperimentalVectorTargetValueForType(typeText, argExpr);
              })) {
        return false;
      }
      if (!rewriteExperimentalConstructorArgsForTarget(
              expr,
              defIt->second,
              false,
              allowMathBare,
              ctx,
              [&](const std::string &typeText, Expr &argExpr) {
                return rewriteExperimentalMapTargetValueForType(typeText, argExpr);
              })) {
        return false;
      }
    }
    rewriteExperimentalAssignTargetValue(
        expr,
        params,
        locals,
        allowMathBare,
        namespacePrefix,
        ctx,
        [&](const std::string &targetTypeText, Expr &valueExpr) {
          return rewriteExperimentalVectorTargetValueForType(targetTypeText, valueExpr);
        });
    rewriteExperimentalInitTargetValue(
        expr,
        params,
        locals,
        allowMathBare,
        namespacePrefix,
        ctx,
        [&](const std::string &targetTypeText, Expr &valueExpr) {
          return rewriteExperimentalVectorTargetValueForType(targetTypeText, valueExpr);
        });
    rewriteExperimentalAssignTargetValue(
        expr,
        params,
        locals,
        allowMathBare,
        namespacePrefix,
        ctx,
        [&](const std::string &targetTypeText, Expr &valueExpr) {
          return rewriteExperimentalMapTargetValueForType(targetTypeText, valueExpr);
        });
    rewriteExperimentalInitTargetValue(
        expr,
        params,
        locals,
        allowMathBare,
        namespacePrefix,
        ctx,
        [&](const std::string &targetTypeText, Expr &valueExpr) {
          return rewriteExperimentalMapTargetValueForType(targetTypeText, valueExpr);
        });
  }
  if (expr.isMethodCall) {
    if (!expr.args.empty()) {
      if (!rewriteNestedExperimentalMapConstructorValue(expr.args.front())) {
        return false;
      }
      if (!rewriteNestedExperimentalVectorConstructorValue(expr.args.front())) {
        return false;
      }
    }
    const bool methodCallSyntax = expr.isMethodCall;
    std::string methodPath;
    if (resolveMethodCallTemplateTarget(expr, locals, ctx, methodPath)) {
      const std::string experimentalVectorMethodPath =
          experimentalVectorHelperPathForCanonicalHelper(methodPath);
      const bool shouldRewriteCanonicalVectorMethodToExperimental =
          ctx.sourceDefs.count(methodPath) == 0 && ctx.helperOverloads.count(methodPath) == 0 &&
          hasVisibleStdCollectionsImportForPath(ctx, methodPath);
      if (shouldRewriteCanonicalVectorMethodToExperimental &&
          !experimentalVectorMethodPath.empty() &&
          ctx.sourceDefs.count(experimentalVectorMethodPath) > 0 &&
          resolvesExperimentalVectorValueReceiver(
              mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
        methodPath = experimentalVectorMethodPath;
        if (expr.templateArgs.empty()) {
          std::vector<std::string> receiverTemplateArgs;
          if (resolveExperimentalVectorValueReceiverTemplateArgs(
                  mapHelperReceiverExpr(expr),
                  params,
                  locals,
                  allowMathBare,
                  namespacePrefix,
                  ctx,
                  receiverTemplateArgs)) {
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
      if (expr.templateArgs.empty() &&
          isExperimentalVectorPublicHelperPath(methodPath) &&
          resolvesExperimentalVectorValueReceiver(
              mapHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalVectorValueReceiverTemplateArgs(
                mapHelperReceiverExpr(expr),
                params,
                locals,
                allowMathBare,
                namespacePrefix,
                ctx,
                receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
          allConcrete = true;
        }
      }
      if (methodPath.rfind("/std/collections/experimental_map/", 0) == 0 &&
          methodPath.find("__t") != std::string::npos) {
        expr.templateArgs.clear();
      }
      if (methodPath.rfind("/std/collections/experimental_vector/", 0) == 0 &&
          methodPath.find("__t") != std::string::npos) {
        expr.templateArgs.clear();
      }
      if (ctx.helperOverloadInternalToPublic.count(methodPath) > 0) {
        expr.name = methodPath;
        expr.namespacePrefix.clear();
        expr.isMethodCall = false;
      }
      const bool isTemplateDef = ctx.templateDefs.count(methodPath) > 0;
      const bool isKnownDef = ctx.sourceDefs.count(methodPath) > 0;
      if (isTemplateDef) {
        auto defIt = ctx.sourceDefs.find(methodPath);
        const bool shouldInferImplicitTemplateTail =
            defIt != ctx.sourceDefs.end() &&
            !expr.templateArgs.empty() &&
            ctx.implicitTemplateDefs.count(methodPath) > 0 &&
            expr.templateArgs.size() < defIt->second.templateArgs.size();
        if (expr.templateArgs.empty() || shouldInferImplicitTemplateTail) {
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
                allConcrete = inferredTemplateArgsAreConcrete(defIt->second, inferredArgs);
                expr.templateArgs = std::move(inferredArgs);
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
      std::string rewrittenMethodPath =
          expr.isMethodCall ? methodPath : resolveCalleePath(expr, namespacePrefix, ctx);
      auto methodDefIt = ctx.sourceDefs.find(rewrittenMethodPath);
      if (methodDefIt == ctx.sourceDefs.end()) {
        methodDefIt = ctx.sourceDefs.find(resolveCalleePath(expr, namespacePrefix, ctx));
      }
      if (methodDefIt != ctx.sourceDefs.end()) {
        if (!rewriteExperimentalConstructorArgsForTarget(
                expr,
                methodDefIt->second,
                methodCallSyntax,
                allowMathBare,
                ctx,
                [&](const std::string &typeText, Expr &argExpr) {
                  return rewriteExperimentalVectorTargetValueForType(typeText, argExpr);
                })) {
          return false;
        }
        if (!rewriteExperimentalConstructorArgsForTarget(
                expr,
                methodDefIt->second,
                methodCallSyntax,
                allowMathBare,
                ctx,
                [&](const std::string &typeText, Expr &argExpr) {
                  return rewriteExperimentalMapTargetValueForType(typeText, argExpr);
                })) {
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
  auto rewriteCanonicalExperimentalMapConstructorValue = [&](Expr &valueExpr) {
    (void)::rewriteCanonicalExperimentalMapConstructorExpr(valueExpr,
                                                           locals,
                                                           params,
                                                           mapping,
                                                           allowedParams,
                                                           def.namespacePrefix,
                                                           ctx,
                                                           allowMathBare,
                                                           error);
  };
  auto rewriteCanonicalExperimentalVectorConstructorValue = [&](Expr &valueExpr) {
    (void)::rewriteCanonicalExperimentalVectorConstructorExpr(valueExpr,
                                                              locals,
                                                              params,
                                                              mapping,
                                                              allowedParams,
                                                              def.namespacePrefix,
                                                              ctx,
                                                              allowMathBare,
                                                              error);
  };
  const ExperimentalCollectionReturnRewritePlan returnRewritePlan =
      inferExperimentalCollectionReturnRewritePlan(def, mapping, allowedParams, allowMathBare, ctx);
  auto rewriteCanonicalExperimentalVectorReturnConstructors = [&](Expr &candidate) {
    rewriteExperimentalConstructorReturnTree(candidate, rewriteCanonicalExperimentalVectorConstructorValue);
  };
  auto rewriteCanonicalExperimentalMapReturnConstructors = [&](Expr &candidate) {
    rewriteExperimentalConstructorReturnTree(candidate, rewriteCanonicalExperimentalMapConstructorValue);
  };
  params.reserve(def.parameters.size());
  locals.reserve(def.parameters.size() + def.statements.size());
  const DefinitionReturnStatementSelection returnStatementSelection =
      determineDefinitionReturnStatementSelection(def);
  if (!rewriteDefinitionParameters(
          def.parameters, mapping, allowedParams, def.namespacePrefix, ctx, error, locals, params, allowMathBare)) {
    return false;
  }
  for (size_t stmtIndex = 0; stmtIndex < def.statements.size(); ++stmtIndex) {
    auto &stmt = def.statements[stmtIndex];
    const bool isReturnPathStmt = isDefinitionReturnPathStatement(stmt, stmtIndex, returnStatementSelection);
    if (isReturnPathStmt &&
        !rewriteDefinitionReturnConstructors(stmt,
                                             returnRewritePlan,
                                             rewriteCanonicalExperimentalVectorReturnConstructors,
                                             rewriteCanonicalExperimentalMapReturnConstructors,
                                             error)) {
      return false;
    }
    if (!rewriteExpr(stmt, mapping, allowedParams, def.namespacePrefix, ctx, error, locals, params, allowMathBare)) {
      return false;
    }
    if (isReturnPathStmt &&
        !rewriteDefinitionReturnConstructors(stmt,
                                             returnRewritePlan,
                                             rewriteCanonicalExperimentalVectorReturnConstructors,
                                             rewriteCanonicalExperimentalMapReturnConstructors,
                                             error)) {
      return false;
    }
    recordDefinitionStatementBindingLocal(stmt, params, locals, allowMathBare, ctx, locals);
  }
  if (def.returnExpr.has_value()) {
    if (!rewriteDefinitionReturnConstructors(*def.returnExpr,
                                             returnRewritePlan,
                                             rewriteCanonicalExperimentalVectorReturnConstructors,
                                             rewriteCanonicalExperimentalMapReturnConstructors,
                                             error)) {
      return false;
    }
    if (!rewriteExpr(*def.returnExpr, mapping, allowedParams, def.namespacePrefix, ctx, error, locals, params,
                     allowMathBare)) {
      return false;
    }
    if (!rewriteDefinitionReturnConstructors(*def.returnExpr,
                                             returnRewritePlan,
                                             rewriteCanonicalExperimentalVectorReturnConstructors,
                                             rewriteCanonicalExperimentalMapReturnConstructors,
                                             error)) {
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
  const auto &importPaths = ctx.program.imports;
  auto shouldSkipWildcardAlias = [](const std::string &prefix, const std::string &remainder) {
    return prefix == "/std/collections" && (remainder == "vector" || remainder == "map");
  };
  for (const auto &importPath : importPaths) {
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
        if (shouldSkipWildcardAlias(prefix, remainder)) {
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
        if (shouldSkipWildcardAlias(prefix, remainder)) {
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
