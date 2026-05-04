#include "SemanticsHelpers.h"

#include <cctype>
#include <string_view>
#include <utility>

namespace primec::semantics {

bool isBindingQualifierName(const std::string &name) {
  return name == "public" || name == "private" || name == "static";
}

bool isBindingAuxTransformName(const std::string &name) {
  return name == "mut" || name == "copy" || name == "restrict" || name == "align_bytes" || name == "align_kbytes" ||
         name == "pod" || name == "handle" || name == "gpu_lane" || isBindingQualifierName(name);
}

bool hasExplicitBindingTypeTransform(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      continue;
    }
    if (isBindingAuxTransformName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    return true;
  }
  return false;
}

bool isPrimitiveBindingTypeName(const std::string &name) {
  return name == "int" || name == "i32" || name == "i64" || name == "u64" || name == "float" || name == "f32" ||
         name == "f64" || name == "integer" || name == "decimal" || name == "complex" || name == "bool" ||
         name == "string" || name == "auto";
}

bool isSoftwareNumericTypeName(const std::string &name) {
  return name == "integer" || name == "decimal" || name == "complex";
}

std::optional<std::string> findSoftwareNumericType(const std::string &typeName) {
  std::string base;
  std::string arg;
  if (!splitTemplateTypeName(typeName, base, arg)) {
    if (isSoftwareNumericTypeName(normalizeBindingTypeName(typeName))) {
      return normalizeBindingTypeName(typeName);
    }
    return std::nullopt;
  }
  if (isSoftwareNumericTypeName(normalizeBindingTypeName(base))) {
    return normalizeBindingTypeName(base);
  }
  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(arg, args)) {
    return std::nullopt;
  }
  for (const auto &nested : args) {
    if (auto found = findSoftwareNumericType(nested)) {
      return found;
    }
  }
  return std::nullopt;
}

std::string normalizeBindingTypeName(const std::string &name) {
  if (name == "int") {
    return "i32";
  }
  if (name == "float") {
    return "f32";
  }
  if (name == "array") {
    return "array";
  }
  if (name == "Buffer" || name == "std/gfx/Buffer" || name == "/std/gfx/Buffer" ||
      name == "std/gfx/experimental/Buffer" || name == "/std/gfx/experimental/Buffer") {
    return "Buffer";
  }
  if (name.rfind("std/gfx/Buffer<", 0) == 0) {
    return "Buffer" + name.substr(std::string("std/gfx/Buffer").size());
  }
  if (name.rfind("/std/gfx/Buffer<", 0) == 0) {
    return "Buffer" + name.substr(std::string("/std/gfx/Buffer").size());
  }
  if (name.rfind("std/gfx/experimental/Buffer<", 0) == 0) {
    return "Buffer" + name.substr(std::string("std/gfx/experimental/Buffer").size());
  }
  if (name.rfind("/std/gfx/experimental/Buffer<", 0) == 0) {
    return "Buffer" + name.substr(std::string("/std/gfx/experimental/Buffer").size());
  }
  if (name == "/map" || name == "std/collections/map" || name == "/std/collections/map") {
    return "map";
  }
  if (name.rfind("/map<", 0) == 0) {
    return name.substr(1);
  }
  if (name.rfind("std/collections/map<", 0) == 0) {
    return "map" + name.substr(std::string("std/collections/map").size());
  }
  if (name.rfind("/std/collections/map<", 0) == 0) {
    return "map" + name.substr(std::string("/std/collections/map").size());
  }
  if (name == "std/maybe/Maybe" || name == "/std/maybe/Maybe") {
    return "Maybe";
  }
  if (name.rfind("std/maybe/Maybe<", 0) == 0) {
    return "Maybe" + name.substr(std::string("std/maybe/Maybe").size());
  }
  if (name.rfind("/std/maybe/Maybe<", 0) == 0) {
    return "Maybe" + name.substr(std::string("/std/maybe/Maybe").size());
  }
  return name;
}

std::string unwrapReferencePointerTypeText(const std::string &typeText) {
  std::string current = normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(current, base, argText)) {
      return current;
    }
    base = normalizeBindingTypeName(base);
    if (base != "Reference" && base != "Pointer") {
      return current;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
      return current;
    }
    current = normalizeBindingTypeName(args.front());
  }
}

bool isBuiltinMapComparableKeyTypeName(const std::string &name) {
  const std::string normalized = normalizeBindingTypeName(name);
  return normalized == "i32" || normalized == "i64" || normalized == "u64" || normalized == "f32" ||
         normalized == "f64" || normalized == "bool" || normalized == "string";
}

bool validateBuiltinMapKeyType(const std::string &typeName,
                               const std::vector<std::string> *templateArgs,
                               std::string &error) {
  const std::string normalized = normalizeBindingTypeName(typeName);
  if (templateArgs != nullptr) {
    for (const auto &templateArg : *templateArgs) {
      if (normalizeBindingTypeName(templateArg) == normalized) {
        return true;
      }
    }
  }
  if (isBuiltinMapComparableKeyTypeName(normalized)) {
    return true;
  }
  error = "map requires builtin Comparable key type (i32, i64, u64, f32, f64, bool, or string): " + normalized;
  return false;
}

bool validateBuiltinMapKeyType(const BindingInfo &binding,
                               const std::vector<std::string> *templateArgs,
                               std::string &error) {
  std::string keyType;
  std::string valueType;
  if (!extractMapKeyValueTypes(binding, keyType, valueType)) {
    return true;
  }
  return validateBuiltinMapKeyType(keyType, templateArgs, error);
}

bool isMapCollectionTypeName(const std::string &name) {
  const std::string normalized = normalizeBindingTypeName(name);
  return normalized == "map" || normalized == "/map" ||
         normalized == "std/collections/map" || normalized == "/std/collections/map" ||
         normalized == "Map" || normalized == "/Map" ||
         normalized == "std/collections/experimental_map/Map" ||
         normalized == "/std/collections/experimental_map/Map";
}

bool returnsMapCollectionType(const std::string &typeText) {
  std::string normalizedType = normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(normalizedType, base, arg)) {
      return isMapCollectionTypeName(normalizedType);
    }
    base = normalizeBindingTypeName(base);
    if (isMapCollectionTypeName(base)) {
      std::vector<std::string> args;
      return splitTopLevelTemplateArgs(arg, args) && args.size() == 2;
    }
    if (base == "Reference" || base == "Pointer") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
        return false;
      }
      normalizedType = normalizeBindingTypeName(args.front());
      continue;
    }
    return false;
  }
}

bool extractMapKeyValueTypesFromTypeText(const std::string &typeText,
                                         std::string &keyTypeOut,
                                         std::string &valueTypeOut) {
  keyTypeOut.clear();
  valueTypeOut.clear();
  std::string normalizedType = normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalizedType, base, argText)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    if (isMapCollectionTypeName(base)) {
      std::vector<std::string> parts;
      if (!splitTopLevelTemplateArgs(argText, parts) || parts.size() != 2) {
        return false;
      }
      keyTypeOut = parts[0];
      valueTypeOut = parts[1];
      return true;
    }
    if (base == "Reference" || base == "Pointer") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      normalizedType = normalizeBindingTypeName(args.front());
      continue;
    }
    return false;
  }
}

bool extractMapKeyValueTypes(const BindingInfo &binding, std::string &keyTypeOut, std::string &valueTypeOut) {
  if (binding.typeTemplateArg.empty()) {
    return extractMapKeyValueTypesFromTypeText(binding.typeName, keyTypeOut, valueTypeOut);
  }
  return extractMapKeyValueTypesFromTypeText(binding.typeName + "<" + binding.typeTemplateArg + ">",
                                             keyTypeOut,
                                             valueTypeOut);
}

bool getArgsPackElementType(const BindingInfo &binding, std::string &elementTypeOut) {
  elementTypeOut.clear();
  const std::string normalizedTypeName = normalizeBindingTypeName(binding.typeName);
  if (normalizedTypeName == "args") {
    if (binding.typeTemplateArg.empty()) {
      return false;
    }
    elementTypeOut = binding.typeTemplateArg;
    return true;
  }
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(normalizedTypeName, base, argText) || normalizeBindingTypeName(base) != "args") {
    return false;
  }
  elementTypeOut = argText;
  return !elementTypeOut.empty();
}

bool isArgsPackBinding(const BindingInfo &binding) {
  std::string elementType;
  return getArgsPackElementType(binding, elementType);
}

bool resolveArgsPackElementTypeForExpr(const Expr &expr,
                                       const std::vector<ParameterInfo> &params,
                                       const std::unordered_map<std::string, BindingInfo> &locals,
                                       std::string &elementTypeOut) {
  if (expr.kind != Expr::Kind::Name) {
    return false;
  }
  const BindingInfo *binding = findBinding(params, locals, expr.name);
  if (binding == nullptr) {
    return false;
  }
  return getArgsPackElementType(*binding, elementTypeOut);
}

bool findTrailingArgsPackParameter(const std::vector<ParameterInfo> &params,
                                   size_t &indexOut,
                                   std::string *elementTypeOut) {
  indexOut = params.size();
  if (elementTypeOut != nullptr) {
    elementTypeOut->clear();
  }
  if (params.empty()) {
    return false;
  }
  std::string elementType;
  if (!getArgsPackElementType(params.back().binding, elementType)) {
    return false;
  }
  indexOut = params.size() - 1;
  if (elementTypeOut != nullptr) {
    *elementTypeOut = std::move(elementType);
  }
  return true;
}

std::string joinTemplateArgs(const std::vector<std::string> &args) {
  std::string out;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      out += ", ";
    }
    out += args[i];
  }
  return out;
}

bool splitTopLevelTemplateArgs(const std::string &text, std::vector<std::string> &out) {
  out.clear();
  int depth = 0;
  size_t start = 0;
  auto pushSegment = [&](size_t end) {
    size_t segStart = start;
    while (segStart < end && std::isspace(static_cast<unsigned char>(text[segStart]))) {
      ++segStart;
    }
    size_t segEnd = end;
    while (segEnd > segStart && std::isspace(static_cast<unsigned char>(text[segEnd - 1]))) {
      --segEnd;
    }
    out.push_back(text.substr(segStart, segEnd - segStart));
  };
  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (c == '<') {
      ++depth;
      continue;
    }
    if (c == '>') {
      if (depth > 0) {
        --depth;
      }
      continue;
    }
    if (c == ',' && depth == 0) {
      pushSegment(i);
      start = i + 1;
    }
  }
  pushSegment(text.size());
  for (const auto &seg : out) {
    if (seg.empty()) {
      return false;
    }
  }
  return !out.empty();
}

bool splitTemplateTypeName(const std::string &text, std::string &base, std::string &arg) {
  size_t lt = text.find('<');
  if (lt == std::string::npos) {
    base = text;
    arg.clear();
    return false;
  }
  size_t gt = text.rfind('>');
  if (gt == std::string::npos || gt <= lt || gt != text.size() - 1) {
    base = text;
    arg.clear();
    return false;
  }
  base = text.substr(0, lt);
  arg = text.substr(lt + 1, gt - lt - 1);
  return true;
}

bool isBuiltinTemplateTypeName(const std::string &name) {
  return name == "Pointer" || name == "Reference" || name == "Buffer" || name == "uninitialized";
}

bool restrictMatchesBinding(const std::string &restrictType,
                            const std::string &typeName,
                            const std::string &typeTemplateArg,
                            bool typeHasTemplate,
                            const std::string &namespacePrefix) {
  std::string restrictBase;
  std::string restrictArg;
  bool restrictHasTemplate = splitTemplateTypeName(restrictType, restrictBase, restrictArg);
  if (restrictHasTemplate != typeHasTemplate) {
    return false;
  }
  if (typeHasTemplate) {
    std::string bindingBase = typeName;
    std::string bindingArg = typeTemplateArg;
    std::string resolvedBindingBase = bindingBase;
    std::string resolvedRestrictBase = restrictBase;
    if (isPrimitiveBindingTypeName(bindingBase) || isPrimitiveBindingTypeName(restrictBase)) {
      resolvedBindingBase = normalizeBindingTypeName(bindingBase);
      resolvedRestrictBase = normalizeBindingTypeName(restrictBase);
    } else {
      if (!bindingBase.empty() && bindingBase[0] != '/' && !isBuiltinTemplateTypeName(bindingBase)) {
        resolvedBindingBase = resolveTypePath(bindingBase, namespacePrefix);
      }
      if (!restrictBase.empty() && restrictBase[0] != '/' && !isBuiltinTemplateTypeName(restrictBase)) {
        resolvedRestrictBase = resolveTypePath(restrictBase, namespacePrefix);
      }
    }
    if (resolvedBindingBase != resolvedRestrictBase) {
      return false;
    }
    if (isBuiltinTemplateTypeName(bindingBase)) {
      return normalizeBindingTypeName(bindingArg) == normalizeBindingTypeName(restrictArg);
    }
    if (isPrimitiveBindingTypeName(bindingArg) || isPrimitiveBindingTypeName(restrictArg)) {
      return normalizeBindingTypeName(bindingArg) == normalizeBindingTypeName(restrictArg);
    }
    return bindingArg == restrictArg;
  }
  if (isPrimitiveBindingTypeName(typeName) || isPrimitiveBindingTypeName(restrictType)) {
    return normalizeBindingTypeName(typeName) == normalizeBindingTypeName(restrictType);
  }
  return resolveTypePath(typeName, namespacePrefix) == resolveTypePath(restrictType, namespacePrefix);
}

ReturnKind returnKindForTypeName(const std::string &name) {
  if (name == "int" || name == "i32") {
    return ReturnKind::Int;
  }
  if (name == "FileError") {
    return ReturnKind::Int;
  }
  if (name == "i64") {
    return ReturnKind::Int64;
  }
  if (name == "u64") {
    return ReturnKind::UInt64;
  }
  if (name == "bool") {
    return ReturnKind::Bool;
  }
  if (name == "string") {
    return ReturnKind::String;
  }
  if (name == "float" || name == "f32") {
    return ReturnKind::Float32;
  }
  if (name == "f64") {
    return ReturnKind::Float64;
  }
  if (name == "integer") {
    return ReturnKind::Integer;
  }
  if (name == "decimal") {
    return ReturnKind::Decimal;
  }
  if (name == "complex") {
    return ReturnKind::Complex;
  }
  if (name == "void") {
    return ReturnKind::Void;
  }
  std::string base;
  std::string arg;
  if (splitTemplateTypeName(name, base, arg) && base == "Result") {
    std::vector<std::string> args;
    if (splitTopLevelTemplateArgs(arg, args)) {
      if (args.size() == 1) {
        return ReturnKind::Int;
      }
      if (args.size() == 2) {
        return ReturnKind::Int64;
      }
    }
  }
  if (splitTemplateTypeName(name, base, arg)) {
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(arg, args)) {
      return ReturnKind::Unknown;
    }
    const bool isVectorLike =
        (base == "array" || base == "vector" || base == "soa_vector" || base == "Buffer" ||
         base == "Vector" || base == "std/collections/experimental_vector/Vector" ||
         base == "/std/collections/experimental_vector/Vector" ||
         base.rfind("std/collections/experimental_vector/Vector__", 0) == 0 ||
         base.rfind("/std/collections/experimental_vector/Vector__", 0) == 0);
    if (isVectorLike && args.size() == 1) {
      return ReturnKind::Array;
    }
    const bool isMapLike =
        (base == "map" || base == "Map" || base == "std/collections/experimental_map/Map" ||
         base == "/std/collections/experimental_map/Map" ||
         base.rfind("std/collections/experimental_map/Map__", 0) == 0 ||
         base.rfind("/std/collections/experimental_map/Map__", 0) == 0);
    if (isMapLike && args.size() == 2) {
      return ReturnKind::Array;
    }
  }
  return ReturnKind::Unknown;
}

std::string resolveStructTypePath(const std::string &name,
                                  const std::string &namespacePrefix,
                                  const std::unordered_set<std::string> &structTypes) {
  if (name.empty()) {
    return {};
  }
  if (!name.empty() && name[0] == '/') {
    return structTypes.count(name) > 0 ? name : std::string{};
  }
  if (!namespacePrefix.empty()) {
    const size_t lastSlash = namespacePrefix.find_last_of('/');
    const std::string_view suffix = lastSlash == std::string::npos
                                        ? std::string_view(namespacePrefix)
                                        : std::string_view(namespacePrefix).substr(lastSlash + 1);
    if (suffix == name && structTypes.count(namespacePrefix) > 0) {
      return namespacePrefix;
    }
    std::string prefix = namespacePrefix;
    while (!prefix.empty()) {
      std::string candidate = prefix + "/" + name;
      if (structTypes.count(candidate) > 0) {
        return candidate;
      }
      const size_t slash = prefix.find_last_of('/');
      if (slash == std::string::npos) {
        break;
      }
      prefix = prefix.substr(0, slash);
    }
  }
  const std::string rootCandidate = "/" + name;
  if (structTypes.count(rootCandidate) > 0) {
    return rootCandidate;
  }
  return {};
}

bool isSoaVectorStructElementType(const std::string &typeArg,
                                  const std::string &namespacePrefix,
                                  const std::unordered_set<std::string> &structTypes,
                                  const std::unordered_map<std::string, std::string> &importAliases) {
  if (typeArg.empty()) {
    return false;
  }
  std::string normalized = normalizeBindingTypeName(typeArg);
  if (!resolveStructTypePath(normalized, namespacePrefix, structTypes).empty()) {
    return true;
  }
  auto importIt = importAliases.find(normalized);
  if (importIt != importAliases.end() && structTypes.count(importIt->second) > 0) {
    return true;
  }
  return false;
}

} // namespace primec::semantics
