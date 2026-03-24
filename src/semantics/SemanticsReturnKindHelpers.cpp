#include "SemanticsHelpers.h"

#include <cctype>
#include <functional>

namespace primec::semantics {

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

} // namespace primec::semantics
