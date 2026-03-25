#include "SemanticsHelpers.h"

#include <array>
#include <cctype>
#include <functional>
#include <limits>
#include <string_view>

namespace primec::semantics {

bool validateAlignTransform(const Transform &transform, const std::string &context, std::string &error);
std::string resolveTypePath(const std::string &name, const std::string &namespacePrefix);

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

bool isRemovedVectorCompatibilityHelper(std::string_view helperName) {
  return helperName == "count" || helperName == "capacity" || helperName == "at" || helperName == "at_unsafe" ||
         helperName == "push" || helperName == "pop" || helperName == "reserve" || helperName == "clear" ||
         helperName == "remove_at" || helperName == "remove_swap";
}

bool isRemovedMapCompatibilityHelper(std::string_view helperName) {
  return helperName == "count" || helperName == "at" || helperName == "at_unsafe";
}

struct HelperSuffixInfo {
  std::string_view suffix;
  std::string_view placement;
};

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

ReturnKind getReturnKind(const Definition &def,
                         const std::unordered_set<std::string> &structNames,
                         const std::unordered_map<std::string, std::string> &importAliases,
                         std::string &error) {
  auto containsUninitializedType = [](const std::string &text) -> bool {
    auto trim = [](const std::string &value) -> std::string {
      size_t start = 0;
      while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
      }
      size_t end = value.size();
      while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
      }
      return value.substr(start, end - start);
    };
    std::function<bool(const std::string &)> contains;
    contains = [&](const std::string &value) -> bool {
      std::string trimmed = trim(value);
      if (trimmed.empty()) {
        return false;
      }
      if (!trimmed.empty() && trimmed[0] == '/') {
        trimmed.erase(0, 1);
      }
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(trimmed, base, arg)) {
        base = trim(base);
        if (!base.empty() && base[0] == '/') {
          base.erase(0, 1);
        }
        if (base == "uninitialized") {
          return true;
        }
        std::vector<std::string> args;
        if (splitTopLevelTemplateArgs(arg, args)) {
          for (const auto &entry : args) {
            if (contains(entry)) {
              return true;
            }
          }
        }
        return false;
      }
      return trimmed == "uninitialized";
    };
    return contains(text);
  };
  ReturnKind kind = ReturnKind::Unknown;
  bool sawReturn = false;
  for (const auto &transform : def.transforms) {
    if (transform.name != "return") {
      continue;
    }
    if (transform.templateArgs.size() != 1) {
      error = "return transform requires a type on " + def.fullPath;
      return ReturnKind::Unknown;
    }
    const std::string &typeName = transform.templateArgs.front();
    if (containsUninitializedType(typeName)) {
      auto extractTopLevelUninitializedReturnTarget =
          [&](const std::string &typeText, std::string &innerTypeOut) -> bool {
        std::string base;
        std::string argText;
        if (!splitTemplateTypeName(typeText, base, argText)) {
          return false;
        }
        if (normalizeBindingTypeName(base) != "uninitialized") {
          return false;
        }
        innerTypeOut = argText;
        return !innerTypeOut.empty();
      };
      std::string returnBase;
      std::string returnArgText;
      std::string wrappedTargetType;
      const bool allowWrappedUninitializedReturn =
          splitTemplateTypeName(typeName, returnBase, returnArgText) &&
          (normalizeBindingTypeName(returnBase) == "Pointer" ||
           normalizeBindingTypeName(returnBase) == "Reference") &&
          extractTopLevelUninitializedReturnTarget(returnArgText, wrappedTargetType) &&
          !containsUninitializedType(wrappedTargetType);
      if (!allowWrappedUninitializedReturn) {
        error = "uninitialized storage is not allowed as return type on " + def.fullPath;
        return ReturnKind::Unknown;
      }
    }
    if (typeName == "auto") {
      if (sawReturn) {
        if (kind == ReturnKind::Unknown) {
          error = "duplicate return transform on " + def.fullPath;
          return ReturnKind::Unknown;
        }
        error = "conflicting return types on " + def.fullPath;
        return ReturnKind::Unknown;
      }
      sawReturn = true;
      kind = ReturnKind::Unknown;
      continue;
    }
    auto isAllowedCollectionTypeArg = [&](const std::string &typeArg) -> bool {
      const std::string normalizedArg = normalizeBindingTypeName(typeArg);
      if (isPrimitiveBindingTypeName(normalizedArg)) {
        return true;
      }
      for (const auto &templateArg : def.templateArgs) {
        if (templateArg == normalizedArg) {
          return true;
        }
      }
      return false;
    };
    std::function<ReturnKind(const std::string &)> resolveReturnTypeKind =
        [&](const std::string &candidateType) -> ReturnKind {
      const std::string normalizedType = normalizeBindingTypeName(candidateType);
      ReturnKind resolvedKind = returnKindForTypeName(normalizedType);
      if (resolvedKind != ReturnKind::Unknown) {
        return resolvedKind;
      }
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(normalizedType, base, arg) && normalizeBindingTypeName(base) == "uninitialized") {
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
          error = "uninitialized return type requires exactly one template argument on " + def.fullPath;
          return ReturnKind::Unknown;
        }
        return resolveReturnTypeKind(args.front());
      }
      if (splitTemplateTypeName(normalizedType, base, arg) && (base == "Reference" || base == "Pointer")) {
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
          error = base + " return type requires exactly one template argument on " + def.fullPath;
          return ReturnKind::Unknown;
        }
        return resolveReturnTypeKind(args.front());
      }
      if (splitTemplateTypeName(normalizedType, base, arg) && base == "vector") {
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
          error = "vector return type requires exactly one template argument on " + def.fullPath;
          return ReturnKind::Unknown;
        }
        if (!isAllowedCollectionTypeArg(args.front())) {
          error = "unsupported return type on " + def.fullPath;
          return ReturnKind::Unknown;
        }
        return ReturnKind::Array;
      }
      if (splitTemplateTypeName(normalizedType, base, arg) && normalizeBindingTypeName(base) == "map") {
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 2) {
          error = "map return type requires exactly two template arguments on " + def.fullPath;
          return ReturnKind::Unknown;
        }
        if (!validateBuiltinMapKeyType(args.front(), &def.templateArgs, error)) {
          return ReturnKind::Unknown;
        }
        if (!isAllowedCollectionTypeArg(args.front()) || !isAllowedCollectionTypeArg(args.back())) {
          error = "unsupported return type on " + def.fullPath;
          return ReturnKind::Unknown;
        }
        return ReturnKind::Array;
      }
      if (splitTemplateTypeName(normalizedType, base, arg) && base == "soa_vector") {
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
          error = "soa_vector return type requires exactly one template argument on " + def.fullPath;
          return ReturnKind::Unknown;
        }
        if (!isSoaVectorStructElementType(args.front(), def.namespacePrefix, structNames, importAliases)) {
          error = "soa_vector return type requires struct element type on " + def.fullPath;
          return ReturnKind::Unknown;
        }
        return ReturnKind::Array;
      }
      if (normalizedType == "array") {
        error = "array return type requires exactly one template argument on " + def.fullPath;
        return ReturnKind::Unknown;
      }
      if (normalizedType == "soa_vector") {
        error = "soa_vector return type requires exactly one template argument on " + def.fullPath;
        return ReturnKind::Unknown;
      }
      if (splitTemplateTypeName(normalizedType, base, arg) && base == "array") {
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
          error = "array return type requires exactly one template argument on " + def.fullPath;
          return ReturnKind::Unknown;
        }
        const std::string elem = normalizeBindingTypeName(args.front());
        if (!isPrimitiveBindingTypeName(elem)) {
          error = "unsupported return type on " + def.fullPath;
          return ReturnKind::Unknown;
        }
        return ReturnKind::Array;
      }
      std::string resolvedType = resolveStructTypePath(normalizedType, def.namespacePrefix, structNames);
      if (resolvedType.empty()) {
        auto importIt = importAliases.find(normalizedType);
        if (importIt != importAliases.end() && structNames.count(importIt->second) > 0) {
          resolvedType = importIt->second;
        }
      }
      if (resolvedType.empty()) {
        error = "unsupported return type on " + def.fullPath;
        return ReturnKind::Unknown;
      }
      return ReturnKind::Array;
    };
    ReturnKind nextKind = resolveReturnTypeKind(typeName);
    if (nextKind == ReturnKind::Unknown && error.empty()) {
      error = "unsupported return type on " + def.fullPath;
      return ReturnKind::Unknown;
    }
    if (sawReturn) {
      if (nextKind == kind) {
        error = "duplicate return transform on " + def.fullPath;
        return ReturnKind::Unknown;
      }
      error = "conflicting return types on " + def.fullPath;
      return ReturnKind::Unknown;
    }
    sawReturn = true;
    kind = nextKind;
  }
  return kind;
}

bool getBuiltinOperatorName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("std/gpu/", 0) == 0) {
    name.erase(0, 8);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "plus" || name == "minus" || name == "multiply" || name == "divide" || name == "negate") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinComparisonName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("std/gpu/", 0) == 0) {
    name.erase(0, 8);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "greater_than" || name == "less_than" || name == "equal" || name == "not_equal" ||
      name == "greater_equal" || name == "less_equal" || name == "and" || name == "or" || name == "not") {
    out = name;
    return true;
  }
  return false;
}

namespace {

bool parseMathName(const std::string &name, std::string &out, bool allowBare);
bool parseGpuName(const std::string &name, std::string &out);
bool parseMemoryName(const std::string &name, std::string &out);

} // namespace

bool getBuiltinMutationName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "increment" || name == "decrement") {
    out = name;
    return true;
  }
  return false;
}

bool isRootBuiltinName(const std::string &name) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  std::string gpuName;
  if (parseGpuName(normalized, gpuName)) {
    return gpuName == "global_id_x" || gpuName == "global_id_y" || gpuName == "global_id_z";
  }
  std::string memoryName;
  if (parseMemoryName(normalized, memoryName)) {
    return memoryName == "alloc" || memoryName == "free" || memoryName == "realloc";
  }
  bool isStdGpuQualified = false;
  if (normalized.rfind("std/gpu/", 0) == 0) {
    normalized = normalized.substr(8);
    isStdGpuQualified = true;
  }
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  Expr probe;
  probe.name = normalized;
  std::string builtinName;
  if (getBuiltinOperatorName(probe, builtinName) || getBuiltinComparisonName(probe, builtinName)) {
    return true;
  }
  if (normalized == "increment" || normalized == "decrement") {
    return true;
  }
  return normalized == "assign" || normalized == "move" || normalized == "if" || normalized == "then" ||
         normalized == "else" ||
         normalized == "loop" || normalized == "while" || normalized == "for" || normalized == "repeat" ||
         normalized == "return" || normalized == "array" || normalized == "vector" || normalized == "map" ||
         normalized == "File" || normalized == "try" || normalized == "count" || normalized == "capacity" ||
         normalized == "to_soa" || normalized == "to_aos" ||
         normalized == "push" || normalized == "pop" ||
         normalized == "reserve" || normalized == "clear" || normalized == "remove_at" || normalized == "remove_swap" ||
         normalized == "at" || normalized == "at_unsafe" || normalized == "convert" ||
         normalized == "location" || normalized == "dereference" ||
         normalized == "block" || normalized == "print" || normalized == "print_line" ||
         normalized == "print_error" || normalized == "print_line_error" || normalized == "notify" ||
         normalized == "insert" || normalized == "take" ||
         (isStdGpuQualified &&
          (normalized == "dispatch" || normalized == "buffer" || normalized == "upload" || normalized == "readback" ||
           normalized == "buffer_load" || normalized == "buffer_store"));
}

bool getBuiltinClampName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "clamp";
}

bool getBuiltinMinMaxName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "min" || out == "max";
}

bool getBuiltinAbsSignName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "abs" || out == "sign";
}

bool getBuiltinSaturateName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  return out == "saturate";
}

namespace {

bool parseMathName(const std::string &name, std::string &out, bool allowBare) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/math/", 0) == 0) {
    out = normalized.substr(9);
    return true;
  }
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  if (!allowBare) {
    return false;
  }
  out = normalized;
  return true;
}

bool parseGpuName(const std::string &name, std::string &out) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/gpu/", 0) == 0) {
    out = normalized.substr(8);
    return true;
  }
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  return false;
}

bool parseMemoryName(const std::string &name, std::string &out) {
  if (name.empty()) {
    return false;
  }
  std::string normalized = name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("std/intrinsics/memory/", 0) == 0) {
    out = normalized.substr(22);
    return true;
  }
  return false;
}

} // namespace

bool getBuiltinMathName(const Expr &expr, std::string &out, bool allowBare) {
  if (!parseMathName(expr.name, out, allowBare)) {
    return false;
  }
  if (out == "lerp" || out == "floor" || out == "ceil" || out == "round" || out == "trunc" || out == "fract" ||
      out == "sqrt" || out == "cbrt" || out == "pow" || out == "exp" || out == "exp2" || out == "log" ||
      out == "log2" || out == "log10" || out == "sin" || out == "cos" || out == "tan" || out == "asin" ||
      out == "acos" || out == "atan" || out == "atan2" || out == "radians" || out == "degrees" || out == "sinh" ||
      out == "cosh" || out == "tanh" || out == "asinh" || out == "acosh" || out == "atanh" || out == "fma" ||
      out == "hypot" || out == "copysign" || out == "is_nan" || out == "is_inf" || out == "is_finite") {
    return true;
  }
  return false;
}

bool isBuiltinMathConstant(const std::string &name, bool allowBare) {
  std::string candidate;
  if (!parseMathName(name, candidate, allowBare)) {
    return false;
  }
  return candidate == "pi" || candidate == "tau" || candidate == "e";
}

bool isExplicitRemovedCollectionMethodAlias(const std::string &receiverPath, std::string rawMethodName) {
  if (!rawMethodName.empty() && rawMethodName.front() == '/') {
    rawMethodName.erase(rawMethodName.begin());
  }

  std::string_view helperName;
  const bool isVectorFamilyReceiver = receiverPath == "/array" || receiverPath == "/vector";
  if (isVectorFamilyReceiver) {
    if (rawMethodName.rfind("array/", 0) == 0) {
      helperName = std::string_view(rawMethodName).substr(std::string_view("array/").size());
    } else if (rawMethodName.rfind("vector/", 0) == 0) {
      helperName = std::string_view(rawMethodName).substr(std::string_view("vector/").size());
    } else if (rawMethodName.rfind("std/collections/vector/", 0) == 0) {
      helperName = std::string_view(rawMethodName).substr(std::string_view("std/collections/vector/").size());
    }
    return !helperName.empty() && isRemovedVectorCompatibilityHelper(helperName);
  }

  if (receiverPath != "/map") {
    return false;
  }
  if (rawMethodName.rfind("map/", 0) == 0) {
    helperName = std::string_view(rawMethodName).substr(std::string_view("map/").size());
  } else if (rawMethodName.rfind("std/collections/map/", 0) == 0) {
    helperName = std::string_view(rawMethodName).substr(std::string_view("std/collections/map/").size());
  }
  return !helperName.empty() && isRemovedMapCompatibilityHelper(helperName);
}

bool isExplicitRemovedCollectionCallAlias(std::string rawPath) {
  if (!rawPath.empty() && rawPath.front() == '/') {
    rawPath.erase(rawPath.begin());
  }

  std::string_view helperName;
  if (rawPath.rfind("array/", 0) == 0) {
    helperName = std::string_view(rawPath).substr(std::string_view("array/").size());
    return !helperName.empty() && isRemovedVectorCompatibilityHelper(helperName);
  }
  if (rawPath.rfind("vector/", 0) == 0) {
    helperName = std::string_view(rawPath).substr(std::string_view("vector/").size());
    return !helperName.empty() && isRemovedVectorCompatibilityHelper(helperName);
  }
  if (rawPath.rfind("map/", 0) == 0) {
    helperName = std::string_view(rawPath).substr(std::string_view("map/").size());
    return !helperName.empty() && isRemovedMapCompatibilityHelper(helperName);
  }
  return false;
}

bool isLifecycleHelperName(const std::string &fullPath) {
  static const std::array<HelperSuffixInfo, 10> suffixes = {{
      {"Create", ""},
      {"Destroy", ""},
      {"Copy", ""},
      {"Move", ""},
      {"CreateStack", "stack"},
      {"DestroyStack", "stack"},
      {"CreateHeap", "heap"},
      {"DestroyHeap", "heap"},
      {"CreateBuffer", "buffer"},
      {"DestroyBuffer", "buffer"},
  }};
  for (const auto &info : suffixes) {
    const std::string_view suffix = info.suffix;
    if (fullPath.size() < suffix.size() + 1) {
      continue;
    }
    const size_t suffixStart = fullPath.size() - suffix.size();
    if (fullPath[suffixStart - 1] != '/') {
      continue;
    }
    if (fullPath.compare(suffixStart, suffix.size(), suffix.data(), suffix.size()) != 0) {
      continue;
    }
    return true;
  }
  return false;
}

bool isMathBuiltinName(const std::string &name) {
  Expr probe;
  probe.name = name;
  std::string builtinName;
  return getBuiltinMathName(probe, builtinName, true) || getBuiltinClampName(probe, builtinName, true) ||
         getBuiltinMinMaxName(probe, builtinName, true) || getBuiltinAbsSignName(probe, builtinName, true) ||
         getBuiltinSaturateName(probe, builtinName, true) || isBuiltinMathConstant(name, true);
}

bool getBuiltinGpuName(const Expr &expr, std::string &out) {
  if (!parseGpuName(expr.name, out)) {
    return false;
  }
  return out == "global_id_x" || out == "global_id_y" || out == "global_id_z";
}

bool getBuiltinMemoryName(const Expr &expr, std::string &out) {
  if (!parseMemoryName(expr.name, out)) {
    return false;
  }
  return out == "alloc" || out == "free" || out == "realloc" || out == "at" || out == "at_unsafe";
}

bool getBuiltinPointerName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name == "dereference" || name == "location" || name == "soa_vector/dereference" ||
      name == "soa_vector/location") {
    if (name == "soa_vector/dereference") {
      out = "dereference";
      return true;
    }
    if (name == "soa_vector/location") {
      out = "location";
      return true;
    }
    out = name;
    return true;
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return false;
}

bool getBuiltinConvertName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "convert") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinCollectionName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("vector/", 0) == 0) {
    std::string alias = name.substr(std::string("vector/").size());
    if (alias == "vector") {
      out = "vector";
      return true;
    }
    return false;
  }
  if (name.rfind("std/collections/vector/", 0) == 0) {
    std::string alias = name.substr(std::string("std/collections/vector/").size());
    return false;
  }
  if (name.rfind("map/", 0) == 0) {
    std::string alias = name.substr(std::string("map/").size());
    if (alias == "map") {
      out = "map";
      return true;
    }
    return false;
  }
  if (name.rfind("std/collections/map/", 0) == 0) {
    return false;
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "array" || name == "vector" || name == "map" || name == "soa_vector") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinArrayAccessName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  auto stripTemplateSpecializationSuffix = [](std::string value) {
    const size_t suffix = value.find("__t");
    if (suffix != std::string::npos) {
      value.erase(suffix);
    }
    return value;
  };
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("std/collections/vector/", 0) == 0) {
    std::string alias = stripTemplateSpecializationSuffix(name.substr(std::string("std/collections/vector/").size()));
    if (alias == "at" || alias == "at_unsafe") {
      out = alias;
      return true;
    }
    return false;
  }
  if (name.rfind("vector/", 0) == 0) {
    std::string alias = stripTemplateSpecializationSuffix(name.substr(std::string("vector/").size()));
    if (alias == "at" || alias == "at_unsafe") {
      out = alias;
      return true;
    }
    return false;
  }
  if (name.rfind("array/", 0) == 0) {
    return false;
  }
  if (name.rfind("map/", 0) == 0) {
    std::string alias = stripTemplateSpecializationSuffix(name.substr(std::string("map/").size()));
    if (alias == "at" || alias == "at_unsafe") {
      out = alias;
      return true;
    }
    return false;
  }
  if (name.rfind("std/collections/map/", 0) == 0) {
    std::string alias = stripTemplateSpecializationSuffix(name.substr(std::string("std/collections/map/").size()));
    if (alias == "at" || alias == "at_unsafe") {
      out = alias;
      return true;
    }
    return false;
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  name = stripTemplateSpecializationSuffix(name);
  if (name == "at" || name == "at_unsafe") {
    out = name;
    return true;
  }
  return false;
}

bool getNamespacedCollectionHelperName(const Expr &expr, std::string &collectionOut, std::string &helperOut) {
  collectionOut.clear();
  helperOut.clear();
  if (expr.name.empty()) {
    return false;
  }
  auto stripTemplateSpecializationSuffix = [](std::string value) {
    const size_t suffix = value.find("__t");
    if (suffix != std::string::npos) {
      value.erase(suffix);
    }
    return value;
  };
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }

  auto extractHelper = [&](const std::string &prefix, const std::string &collectionName) -> bool {
    if (normalized.rfind(prefix, 0) != 0) {
      return false;
    }
    collectionOut = collectionName;
    helperOut = stripTemplateSpecializationSuffix(normalized.substr(prefix.size()));
    return !helperOut.empty();
  };

  if (extractHelper("vector/", "vector")) {
    return true;
  }
  if (extractHelper("std/collections/vector/", "vector") || extractHelper("map/", "map") ||
      extractHelper("std/collections/map/", "map")) {
    return true;
  }
  if (extractHelper("array/", "vector")) {
    if (helperOut == "count" || helperOut == "capacity" || helperOut == "at" || helperOut == "at_unsafe" ||
        helperOut == "push" || helperOut == "pop" || helperOut == "reserve" || helperOut == "clear" ||
        helperOut == "remove_at" || helperOut == "remove_swap") {
      collectionOut.clear();
      helperOut.clear();
      return false;
    }
    return true;
  }
  collectionOut.clear();
  helperOut.clear();
  return false;
}

bool isAssignCall(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == "assign";
}

const BindingInfo *findBinding(const std::vector<ParameterInfo> &params,
                               const std::unordered_map<std::string, BindingInfo> &locals,
                               const std::string &name) {
  auto it = locals.find(name);
  if (it != locals.end()) {
    return &it->second;
  }
  for (const auto &param : params) {
    if (param.name == name) {
      return &param.binding;
    }
  }
  return nullptr;
}

bool isPointerExpr(const Expr &expr,
                   const std::vector<ParameterInfo> &params,
                   const std::unordered_map<std::string, BindingInfo> &locals) {
  if (expr.kind == Expr::Kind::Name) {
    const BindingInfo *binding = findBinding(params, locals, expr.name);
    return binding != nullptr && binding->typeName == "Pointer";
  }
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  std::string builtinName;
  if (getBuiltinPointerName(expr, builtinName) && builtinName == "location" && expr.args.size() == 1) {
    return true;
  }
  if (getBuiltinMemoryName(expr, builtinName)) {
    if (builtinName == "alloc") {
      return expr.templateArgs.size() == 1 && expr.args.size() == 1;
    }
    if (builtinName == "realloc" && expr.args.size() == 2) {
      return isPointerExpr(expr.args.front(), params, locals);
    }
    if (builtinName == "at" && expr.templateArgs.empty() && expr.args.size() == 3) {
      return isPointerExpr(expr.args.front(), params, locals);
    }
    if (builtinName == "at_unsafe" && expr.templateArgs.empty() && expr.args.size() == 2) {
      return isPointerExpr(expr.args.front(), params, locals);
    }
  }
  if (getBuiltinOperatorName(expr, builtinName) && (builtinName == "plus" || builtinName == "minus") &&
      expr.args.size() == 2) {
    return isPointerExpr(expr.args[0], params, locals) && !isPointerExpr(expr.args[1], params, locals);
  }
  return false;
}

bool isPointerLikeExpr(const Expr &expr,
                       const std::vector<ParameterInfo> &params,
                       const std::unordered_map<std::string, BindingInfo> &locals) {
  if (expr.kind == Expr::Kind::Name) {
    const BindingInfo *binding = findBinding(params, locals, expr.name);
    return binding != nullptr && (binding->typeName == "Pointer" || binding->typeName == "Reference");
  }
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  std::string accessName;
  if (getBuiltinArrayAccessName(expr, accessName) && expr.args.size() == 2) {
    size_t receiverIndex = 0;
    if (!expr.isMethodCall) {
      bool foundValues = false;
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i < expr.argNames.size() && expr.argNames[i].has_value() && *expr.argNames[i] == "values") {
          receiverIndex = i;
          foundValues = true;
          break;
        }
      }
      if (!foundValues) {
        receiverIndex = 0;
      }
    }
    if (receiverIndex < expr.args.size() && expr.args[receiverIndex].kind == Expr::Kind::Name) {
      const BindingInfo *binding = findBinding(params, locals, expr.args[receiverIndex].name);
      std::string elementType;
      if (binding != nullptr && getArgsPackElementType(*binding, elementType)) {
        std::string base;
        std::string argText;
        if (splitTemplateTypeName(normalizeBindingTypeName(elementType), base, argText)) {
          base = normalizeBindingTypeName(base);
          if (base == "Pointer" || base == "Reference") {
            return true;
          }
        }
      }
    }
  }
  std::string builtinName;
  if (getBuiltinPointerName(expr, builtinName) && builtinName == "location" && expr.args.size() == 1) {
    return true;
  }
  if (getBuiltinMemoryName(expr, builtinName)) {
    if (builtinName == "alloc") {
      return expr.templateArgs.size() == 1 && expr.args.size() == 1;
    }
    if (builtinName == "realloc" && expr.args.size() == 2) {
      return isPointerExpr(expr.args.front(), params, locals);
    }
    if (builtinName == "at" && expr.templateArgs.empty() && expr.args.size() == 3) {
      return isPointerExpr(expr.args.front(), params, locals);
    }
    if (builtinName == "at_unsafe" && expr.templateArgs.empty() && expr.args.size() == 2) {
      return isPointerExpr(expr.args.front(), params, locals);
    }
  }
  if (getBuiltinOperatorName(expr, builtinName) && (builtinName == "plus" || builtinName == "minus") &&
      expr.args.size() == 2) {
    return isPointerLikeExpr(expr.args[0], params, locals) && !isPointerLikeExpr(expr.args[1], params, locals);
  }
  return false;
}

bool isSimpleCallName(const Expr &expr, const char *nameToMatch) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  auto stripTemplateSpecializationSuffix = [](std::string value) {
    const size_t suffix = value.find("__t");
    if (suffix != std::string::npos) {
      value.erase(suffix);
    }
    return value;
  };
  const std::string targetName = nameToMatch == nullptr ? std::string() : std::string(nameToMatch);
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("std/gpu/", 0) == 0) {
    name.erase(0, 8);
  }
  if (name.rfind("vector/", 0) == 0) {
    std::string alias = stripTemplateSpecializationSuffix(name.substr(std::string("vector/").size()));
    if (alias.find('/') == std::string::npos && alias == "vector") {
      return alias == targetName;
    }
  }
  if (name.rfind("std/collections/vector/", 0) == 0) {
    std::string alias =
        stripTemplateSpecializationSuffix(name.substr(std::string("std/collections/vector/").size()));
    if (alias.find('/') == std::string::npos &&
        (alias == "count" || alias == "capacity" || alias == "at" || alias == "at_unsafe" || alias == "push" ||
         alias == "pop" || alias == "reserve" || alias == "clear" || alias == "remove_at" ||
         alias == "remove_swap")) {
      return alias == targetName;
    }
  }
  if (name.rfind("map/", 0) == 0) {
    std::string alias = stripTemplateSpecializationSuffix(name.substr(std::string("map/").size()));
    if (alias.find('/') == std::string::npos &&
        (alias == "map" || alias == "count" || alias == "at" || alias == "at_unsafe")) {
      return alias == targetName;
    }
  }
  if (name.rfind("std/collections/map/", 0) == 0) {
    std::string alias = stripTemplateSpecializationSuffix(name.substr(std::string("std/collections/map/").size()));
    if (alias.find('/') == std::string::npos &&
        (alias == "map" || alias == "count" || alias == "at" || alias == "at_unsafe")) {
      return alias == targetName;
    }
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  name = stripTemplateSpecializationSuffix(name);
  return name == targetName;
}

bool isIfCall(const Expr &expr) {
  return isSimpleCallName(expr, "if");
}

bool isMatchCall(const Expr &expr) {
  return isSimpleCallName(expr, "match");
}

bool isReturnCall(const Expr &expr) {
  return isSimpleCallName(expr, "return");
}

bool isLoopCall(const Expr &expr) {
  return isSimpleCallName(expr, "loop");
}

bool isWhileCall(const Expr &expr) {
  return isSimpleCallName(expr, "while");
}

bool isForCall(const Expr &expr) {
  return isSimpleCallName(expr, "for");
}

bool isRepeatCall(const Expr &expr) {
  return isSimpleCallName(expr, "repeat");
}

bool isBlockCall(const Expr &expr) {
  return isSimpleCallName(expr, "block");
}

bool lowerMatchToIf(const Expr &expr, Expr &out, std::string &error) {
  if (!isMatchCall(expr)) {
    out = expr;
    return true;
  }
  if (hasNamedArguments(expr.argNames)) {
    error = "named arguments not supported for builtin calls";
    return false;
  }
  if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
    error = "match does not accept trailing block arguments";
    return false;
  }
  if (expr.args.size() < 3) {
    error = "match requires value, cases, else";
    return false;
  }
  auto isBlockEnvelope = [&](const Expr &candidate, size_t expectedArgs) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    if (candidate.args.size() != expectedArgs) {
      return false;
    }
    if (!candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
      return false;
    }
    if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
      return false;
    }
    return true;
  };
  auto wrapInBlockEnvelope = [&](const Expr &value) -> Expr {
    Expr block;
    block.kind = Expr::Kind::Call;
    block.name = "else";
    block.namespacePrefix = expr.namespacePrefix;
    block.hasBodyArguments = true;
    block.bodyArguments = {value};
    return block;
  };
  if (expr.args.size() == 3 && isBlockEnvelope(expr.args[1], 0) && isBlockEnvelope(expr.args[2], 0)) {
    Expr ifCall;
    ifCall.kind = Expr::Kind::Call;
    ifCall.name = "if";
    ifCall.namespacePrefix = expr.namespacePrefix;
    ifCall.transforms = expr.transforms;
    ifCall.args = {expr.args[0], expr.args[1], expr.args[2]};
    ifCall.argNames = {std::nullopt, std::nullopt, std::nullopt};
    out = std::move(ifCall);
    return true;
  }

  const Expr &elseArg = expr.args.back();
  if (!isBlockEnvelope(elseArg, 0)) {
    error = "match requires final else block";
    return false;
  }
  for (size_t i = 1; i + 1 < expr.args.size(); ++i) {
    if (!isBlockEnvelope(expr.args[i], 1)) {
      error = "match cases require pattern and block";
      return false;
    }
  }

  Expr currentElse = elseArg;
  for (size_t i = expr.args.size() - 1; i-- > 1;) {
    const Expr &caseExpr = expr.args[i];
    Expr branch = caseExpr;
    branch.args.clear();
    branch.argNames.clear();
    Expr cond;
    cond.kind = Expr::Kind::Call;
    cond.name = "equal";
    cond.namespacePrefix = expr.namespacePrefix;
    cond.args = {expr.args[0], caseExpr.args.front()};
    cond.argNames = {std::nullopt, std::nullopt};

    Expr ifCall;
    ifCall.kind = Expr::Kind::Call;
    ifCall.name = "if";
    ifCall.namespacePrefix = expr.namespacePrefix;
    Expr elseBranch = currentElse;
    if (!isBlockEnvelope(elseBranch, 0)) {
      elseBranch = wrapInBlockEnvelope(currentElse);
    }
    ifCall.args = {std::move(cond), std::move(branch), std::move(elseBranch)};
    ifCall.argNames = {std::nullopt, std::nullopt, std::nullopt};
    currentElse = std::move(ifCall);
  }
  currentElse.transforms = expr.transforms;
  currentElse.namespacePrefix = expr.namespacePrefix;
  out = std::move(currentElse);
  return true;
}

bool getPrintBuiltin(const Expr &expr, PrintBuiltin &out) {
  if (isSimpleCallName(expr, "print")) {
    out.target = PrintTarget::Out;
    out.newline = false;
    out.name = "print";
    return true;
  }
  if (isSimpleCallName(expr, "print_line")) {
    out.target = PrintTarget::Out;
    out.newline = true;
    out.name = "print_line";
    return true;
  }
  if (isSimpleCallName(expr, "print_error")) {
    out.target = PrintTarget::Err;
    out.newline = false;
    out.name = "print_error";
    return true;
  }
  if (isSimpleCallName(expr, "print_line_error")) {
    out.target = PrintTarget::Err;
    out.newline = true;
    out.name = "print_line_error";
    return true;
  }
  return false;
}

bool getPathSpaceBuiltin(const Expr &expr, PathSpaceBuiltin &out) {
  if (!expr.isMethodCall && isSimpleCallName(expr, "notify")) {
    out.target = PathSpaceTarget::Notify;
    out.name = "notify";
    out.requiredEffect = "pathspace_notify";
    out.argumentCount = 2;
    return true;
  }
  if (!expr.isMethodCall && isSimpleCallName(expr, "insert")) {
    out.target = PathSpaceTarget::Insert;
    out.name = "insert";
    out.requiredEffect = "pathspace_insert";
    out.argumentCount = 2;
    return true;
  }
  if (!expr.isMethodCall && isSimpleCallName(expr, "take")) {
    out.target = PathSpaceTarget::Take;
    out.name = "take";
    out.requiredEffect = "pathspace_take";
    out.argumentCount = 1;
    return true;
  }
  if (!expr.isMethodCall && isSimpleCallName(expr, "bind")) {
    out.target = PathSpaceTarget::Bind;
    out.name = "bind";
    out.requiredEffect = "pathspace_bind";
    out.argumentCount = 2;
    return true;
  }
  if (isSimpleCallName(expr, "unbind")) {
    out.target = PathSpaceTarget::Unbind;
    out.name = "unbind";
    out.requiredEffect = "pathspace_bind";
    out.argumentCount = 1;
    return true;
  }
  if (isSimpleCallName(expr, "schedule")) {
    out.target = PathSpaceTarget::Schedule;
    out.name = "schedule";
    out.requiredEffect = "pathspace_schedule";
    out.argumentCount = 2;
    return true;
  }
  return false;
}

std::string resolveTypePath(const std::string &name, const std::string &namespacePrefix) {
  if (!name.empty() && name[0] == '/') {
    return name;
  }
  if (!namespacePrefix.empty()) {
    const size_t lastSlash = namespacePrefix.find_last_of('/');
    const std::string_view suffix = lastSlash == std::string::npos
                                        ? std::string_view(namespacePrefix)
                                        : std::string_view(namespacePrefix).substr(lastSlash + 1);
    if (suffix == name) {
      return namespacePrefix;
    }
    return namespacePrefix + "/" + name;
  }
  return "/" + name;
}

bool parseBindingInfo(const Expr &expr,
                      const std::string &namespacePrefix,
                      const std::unordered_set<std::string> &structTypes,
                      const std::unordered_map<std::string, std::string> &importAliases,
                      BindingInfo &info,
                      std::optional<std::string> &restrictTypeOut,
                      std::string &error) {
  std::string typeName;
  bool typeHasTemplate = false;
  std::optional<std::string> restrictType;
  std::optional<std::string> visibilityTransform;
  bool sawStatic = false;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      error = "binding does not accept " + transform.name + " transform";
      return false;
    }
    if (transform.name == "return") {
      error = "binding does not accept return transform";
      return false;
    }
    if (transform.name == "stack" || transform.name == "heap" || transform.name == "buffer") {
      error = "placement transforms are not supported on bindings";
      return false;
    }
    if (transform.name == "mut") {
      if (!transform.templateArgs.empty()) {
        error = "binding transforms do not take template arguments";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      info.isMutable = true;
      continue;
    }
    if (transform.name == "copy") {
      if (!transform.templateArgs.empty()) {
        error = "binding transforms do not take template arguments";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      continue;
    }
    if (transform.name == "pod" || transform.name == "gpu_lane") {
      if (!transform.templateArgs.empty()) {
        error = "binding transforms do not take template arguments";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      continue;
    }
    if (transform.name == "handle") {
      if (transform.templateArgs.size() > 1) {
        error = "handle accepts at most one template argument";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      continue;
    }
    if (transform.name == "restrict") {
      if (transform.templateArgs.size() != 1) {
        error = "restrict requires a template argument";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      if (restrictType.has_value()) {
        error = "duplicate restrict transform";
        return false;
      }
      restrictType = transform.templateArgs.front();
      continue;
    }
    if (isBindingQualifierName(transform.name)) {
      if (transform.name == "static") {
        if (sawStatic) {
          error = "duplicate static transform on binding";
          return false;
        }
        sawStatic = true;
      } else {
        if (visibilityTransform.has_value()) {
          error = "binding visibility transforms are mutually exclusive";
          return false;
        }
        visibilityTransform = transform.name;
      }
      if (!transform.templateArgs.empty()) {
        error = "binding transforms do not take template arguments";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      continue;
    }
    if (transform.name == "align_bytes" || transform.name == "align_kbytes") {
      if (!validateAlignTransform(transform, "binding " + expr.name, error)) {
        return false;
      }
      continue;
    }
    if (isPrimitiveBindingTypeName(transform.name)) {
      if (!transform.templateArgs.empty()) {
        error = "binding transforms do not take template arguments";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
    }
    if (transform.name == "Pointer") {
      if (transform.templateArgs.size() != 1) {
        error = "Pointer requires a template argument";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      if (!typeName.empty()) {
        error = "binding requires exactly one type";
        return false;
      }
      typeName = transform.name;
      typeHasTemplate = true;
      info.typeTemplateArg = transform.templateArgs.front();
      continue;
    }
    if (transform.name == "Reference") {
      if (transform.templateArgs.size() != 1) {
        error = "Reference requires a template argument";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      if (!typeName.empty()) {
        error = "binding requires exactly one type";
        return false;
      }
      typeName = transform.name;
      typeHasTemplate = true;
      info.typeTemplateArg = transform.templateArgs.front();
      continue;
    }
    if (!transform.templateArgs.empty()) {
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      if (!typeName.empty()) {
        error = "binding requires exactly one type";
        return false;
      }
      const std::string normalizedTypeName = normalizeBindingTypeName(transform.name);
      if ((normalizedTypeName == "array" || normalizedTypeName == "vector" || normalizedTypeName == "Buffer") &&
          transform.templateArgs.size() != 1) {
        if (normalizedTypeName == "array" && transform.templateArgs.size() > 1) {
          error = "array<T, N> is unsupported; use array<T> (runtime-count array)";
          return false;
        }
        error = normalizedTypeName + " requires exactly one template argument";
        return false;
      }
      if (normalizedTypeName == "map" && transform.templateArgs.size() != 2) {
        error = "map requires exactly two template arguments";
        return false;
      }
      if (transform.name == "soa_vector") {
        if (transform.templateArgs.size() != 1) {
          error = "soa_vector requires exactly one template argument";
          return false;
        }
        if (!isSoaVectorStructElementType(transform.templateArgs.front(), namespacePrefix, structTypes, importAliases)) {
          error = "soa_vector requires struct element type";
          return false;
        }
        typeName = transform.name;
        typeHasTemplate = true;
        info.typeTemplateArg = joinTemplateArgs(transform.templateArgs);
        continue;
      }
      if (transform.name == "uninitialized" && transform.templateArgs.size() != 1) {
        error = "uninitialized requires exactly one template argument";
        return false;
      }
      typeName = transform.name;
      typeHasTemplate = true;
      info.typeTemplateArg = joinTemplateArgs(transform.templateArgs);
      continue;
    }
    if (!transform.arguments.empty()) {
      error = "binding transforms do not take arguments";
      return false;
    }
    if (!typeName.empty()) {
      error = "binding requires exactly one type";
      return false;
    }
    typeName = transform.name;
  }
  if (typeName.empty()) {
    typeName = "int";
  }
  if (typeName == "Self") {
    if (structTypes.count(namespacePrefix) == 0) {
      error = "Self is only valid inside struct definitions";
      return false;
    }
    typeName = namespacePrefix;
  }
  if (typeHasTemplate && info.typeTemplateArg == "Self") {
    if (structTypes.count(namespacePrefix) == 0) {
      error = "Self is only valid inside struct definitions";
      return false;
    }
    info.typeTemplateArg = namespacePrefix;
  }
  restrictTypeOut = restrictType;
  std::string fullType = typeName;
  if (typeHasTemplate) {
    fullType += "<" + info.typeTemplateArg + ">";
  }
  if (!isPrimitiveBindingTypeName(typeName) && !typeHasTemplate) {
    std::string resolved = resolveStructTypePath(typeName, namespacePrefix, structTypes);
    if (resolved.empty()) {
      auto importIt = importAliases.find(typeName);
      if (importIt != importAliases.end() && structTypes.count(importIt->second) > 0) {
        resolved = importIt->second;
      }
    }
    if (resolved.empty()) {
      if (typeName != "FileError") {
        error = "unsupported binding type: " + typeName;
        return false;
      }
    }
  }
  auto isStructTypeName = [&](const std::string &candidate) -> bool {
    if (candidate.empty() || candidate.find('<') != std::string::npos) {
      return false;
    }
    std::string resolved = resolveStructTypePath(candidate, namespacePrefix, structTypes);
    if (!resolved.empty()) {
      return true;
    }
    auto importIt = importAliases.find(candidate);
    if (importIt != importAliases.end()) {
      return structTypes.count(importIt->second) > 0;
    }
    return false;
  };
  auto trimTypeText = [](const std::string &value) -> std::string {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
      ++start;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
      --end;
    }
    return value.substr(start, end - start);
  };
  auto containsUninitializedType = [&](const std::string &text) -> bool {
    std::function<bool(const std::string &)> contains;
    contains = [&](const std::string &value) -> bool {
      std::string trimmed = trimTypeText(value);
      if (trimmed.empty()) {
        return false;
      }
      if (!trimmed.empty() && trimmed[0] == '/') {
        trimmed.erase(0, 1);
      }
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(trimmed, base, arg)) {
        base = trimTypeText(base);
        if (!base.empty() && base[0] == '/') {
          base.erase(0, 1);
        }
        if (base == "uninitialized") {
          return true;
        }
        std::vector<std::string> args;
        if (splitTopLevelTemplateArgs(arg, args)) {
          for (const auto &entry : args) {
            if (contains(entry)) {
              return true;
            }
          }
        }
        return false;
      }
      return trimmed == "uninitialized";
    };
    return contains(text);
  };
  auto extractTopLevelUninitializedTarget = [&](const std::string &typeText, std::string &innerTypeOut) -> bool {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(trimTypeText(typeText), base, argText)) {
      return false;
    }
    if (normalizeBindingTypeName(trimTypeText(base)) != "uninitialized") {
      return false;
    }
    innerTypeOut = trimTypeText(argText);
    return !innerTypeOut.empty();
  };
  auto isSupportedPointerReferenceTarget = [&](const std::string &targetType, std::string &unsupportedError) -> bool {
    const std::string normalizedTarget = normalizeBindingTypeName(targetType);
    if (isPrimitiveBindingTypeName(normalizedTarget) || isStructTypeName(normalizedTarget) ||
        normalizedTarget == "FileError") {
      return true;
    }

    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(targetType, base, arg)) {
      unsupportedError = targetType;
      return false;
    }

    base = normalizeBindingTypeName(base);
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(arg, args)) {
      unsupportedError = targetType;
      return false;
    }

    if ((base == "array" || base == "vector" || base == "soa_vector" || base == "Buffer" || base == "File") &&
        args.size() == 1) {
      return true;
    }
    if (base == "map" && args.size() == 2) {
      return true;
    }
    if (base == "Result" && (args.size() == 1 || args.size() == 2)) {
      return true;
    }

    unsupportedError = targetType;
    return false;
  };
  auto isSupportedArgsUninitializedElementType = [&](const std::string &elementType, std::string &elementError) -> bool {
    elementError.clear();
    std::string elementBase;
    std::string elementArgText;
    if (!splitTemplateTypeName(trimTypeText(elementType), elementBase, elementArgText)) {
      return false;
    }
    elementBase = normalizeBindingTypeName(trimTypeText(elementBase));
    const bool isPointerElement = elementBase == "Pointer";
    const bool isReferenceElement = elementBase == "Reference";
    if (!isPointerElement && !isReferenceElement) {
      return false;
    }
    std::vector<std::string> elementArgs;
    if (!splitTopLevelTemplateArgs(elementArgText, elementArgs) || elementArgs.size() != 1) {
      return false;
    }

    std::string targetType = elementArgs.front();
    if (extractTopLevelUninitializedTarget(targetType, targetType)) {
      if (containsUninitializedType(targetType)) {
        elementError = isPointerElement ? "uninitialized storage is not allowed in pointer targets"
                                        : "uninitialized storage is not allowed in reference targets";
        return false;
      }
    } else if (containsUninitializedType(elementArgs.front())) {
      elementError = "uninitialized storage is not allowed as template argument to user-defined types";
      return false;
    }

    std::string unsupportedTarget;
    if (!isSupportedPointerReferenceTarget(targetType, unsupportedTarget)) {
      elementError = isPointerElement ? "unsupported pointer target type: " + unsupportedTarget
                                      : "unsupported reference target type: " + unsupportedTarget;
      return false;
    }
    return true;
  };
  const std::string normalizedTypeName = normalizeBindingTypeName(typeName);
  if (typeHasTemplate &&
      (normalizedTypeName == "array" || normalizedTypeName == "vector" || normalizedTypeName == "Buffer")) {
    if (containsUninitializedType(info.typeTemplateArg)) {
      error = "uninitialized storage is not allowed in " + normalizedTypeName + " element types";
      return false;
    }
  }
  if (typeHasTemplate && normalizedTypeName == "map") {
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(info.typeTemplateArg, args) || args.size() != 2) {
      error = "map requires exactly two template arguments";
      return false;
    }
    if (containsUninitializedType(args[0]) || containsUninitializedType(args[1])) {
      error = "uninitialized storage is not allowed in map key/value types";
      return false;
    }
  }
  if (typeHasTemplate && normalizedTypeName != "Pointer" && normalizedTypeName != "Reference" &&
      normalizedTypeName != "uninitialized" && normalizedTypeName != "array" &&
      normalizedTypeName != "vector" && normalizedTypeName != "map" && normalizedTypeName != "Buffer" &&
      normalizedTypeName != "Result" && normalizedTypeName != "args") {
    if (containsUninitializedType(info.typeTemplateArg)) {
      error = "uninitialized storage is not allowed as template argument to user-defined types";
      return false;
    }
  }
  if (typeHasTemplate && normalizedTypeName == "args" && containsUninitializedType(info.typeTemplateArg)) {
    std::string elementError;
    if (!isSupportedArgsUninitializedElementType(info.typeTemplateArg, elementError)) {
      error = elementError.empty() ? "uninitialized storage is not allowed as template argument to user-defined types"
                                   : elementError;
      return false;
    }
  }
  if (typeHasTemplate && typeName == "Pointer") {
    if (info.typeTemplateArg.empty()) {
      error = "Pointer requires a template argument";
      return false;
    }
    std::string pointerTargetType = info.typeTemplateArg;
    if (extractTopLevelUninitializedTarget(info.typeTemplateArg, pointerTargetType)) {
      if (containsUninitializedType(pointerTargetType)) {
        error = "uninitialized storage is not allowed in pointer targets";
        return false;
      }
    } else if (containsUninitializedType(info.typeTemplateArg)) {
      error = "uninitialized storage is not allowed in pointer targets";
      return false;
    }
    std::string unsupportedTarget;
    if (!isSupportedPointerReferenceTarget(pointerTargetType, unsupportedTarget)) {
      error = "unsupported pointer target type: " + unsupportedTarget;
      return false;
    }
  }
  if (typeHasTemplate && typeName == "Reference") {
    if (info.typeTemplateArg.empty()) {
      error = "Reference requires a template argument";
      return false;
    }
    std::string referenceTargetType = info.typeTemplateArg;
    if (extractTopLevelUninitializedTarget(info.typeTemplateArg, referenceTargetType)) {
      if (containsUninitializedType(referenceTargetType)) {
        error = "uninitialized storage is not allowed in reference targets";
        return false;
      }
    } else if (containsUninitializedType(info.typeTemplateArg)) {
      error = "uninitialized storage is not allowed in reference targets";
      return false;
    }
    std::string unsupportedTarget;
    if (!isSupportedPointerReferenceTarget(referenceTargetType, unsupportedTarget)) {
      error = "unsupported reference target type: " + unsupportedTarget;
      return false;
    }
  }
  if (isPrimitiveBindingTypeName(typeName)) {
    info.typeName = typeName;
    return true;
  }
  if (typeHasTemplate) {
    info.typeName = typeName;
    return true;
  }
  info.typeName = typeName;
  return true;
}

} // namespace primec::semantics
