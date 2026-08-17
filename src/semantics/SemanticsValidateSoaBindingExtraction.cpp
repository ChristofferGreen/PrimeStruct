#include "SemanticsValidateSoaBindingExtraction.h"

#include "SemanticsHelpers.h"
#include "StdlibCollectionSurfaceHelpers.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "primec/ir/StdlibCollectionPaths.h"

namespace primec {

bool isExperimentalKeyValueTypeText(const std::string &typeText) {
  std::string keyType;
  std::string valueType;
  if (!semantics::extractKeyValueCollectionTypesFromTypeText(typeText, keyType, valueType)) {
    return false;
  }
  const std::string normalizedInner = semantics::normalizeBindingTypeName(typeText);
  std::string candidate = normalizedInner;
  std::string base;
  std::string argText;
  if (semantics::splitTemplateTypeName(normalizedInner, base, argText) &&
      !base.empty()) {
    candidate = semantics::normalizeBindingTypeName(base);
  }
  if (!candidate.empty() && candidate.front() == '/') {
    candidate.erase(candidate.begin());
  }
  return isExperimentalCollectionBackingTypeName("map", "Map", candidate);
}

std::optional<semantics::BindingInfo> extractBorrowedExperimentalKeyValueBinding(const Expr &expr) {
  semantics::BindingInfo info;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "Reference" &&
        transform.templateArgs.size() == 1 &&
        isExperimentalKeyValueTypeText(transform.templateArgs.front())) {
      info.typeName = "Reference";
      info.typeTemplateArg = transform.templateArgs.front();
      return info;
    }
  }
  return std::nullopt;
}

std::string bindingTypeText(const semantics::BindingInfo &binding) {
  if (binding.typeName.empty()) {
    return {};
  }
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

bool isExperimentalKeyValueValueBinding(const semantics::BindingInfo &binding) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(binding.typeName);
  if (normalizedType == "Reference" || normalizedType == "Pointer") {
    return false;
  }
  return isExperimentalKeyValueTypeText(bindingTypeText(binding));
}

std::optional<semantics::BindingInfo> extractExperimentalKeyValueValueBinding(const Expr &expr) {
  static const std::unordered_set<std::string> emptyStructTypes;
  static const std::unordered_map<std::string, std::string> emptyImportAliases;
  semantics::BindingInfo binding;
  std::optional<std::string> restrictType;
  std::string parseError;
  if (!semantics::parseBindingInfo(
          expr, expr.namespacePrefix, emptyStructTypes, emptyImportAliases, binding, restrictType, parseError)) {
    return std::nullopt;
  }
  if (!isExperimentalKeyValueValueBinding(binding)) {
    return std::nullopt;
  }
  return binding;
}

bool isBorrowedExperimentalKeyValueBinding(const semantics::BindingInfo &binding) {
  return semantics::normalizeBindingTypeName(binding.typeName) == "Reference" &&
         isExperimentalKeyValueTypeText(binding.typeTemplateArg);
}

std::optional<semantics::BindingInfo> extractBorrowedExperimentalKeyValueReturnBinding(
    const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    std::string base;
    std::string argText;
    if (!semantics::splitTemplateTypeName(transform.templateArgs.front(), base, argText)) {
      continue;
    }
    if (semantics::normalizeBindingTypeName(base) != "Reference") {
      continue;
    }
    std::vector<std::string> args;
    if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1 ||
        !isExperimentalKeyValueTypeText(args.front())) {
      continue;
    }
    semantics::BindingInfo info;
    info.typeName = "Reference";
    info.typeTemplateArg = args.front();
    return info;
  }
  return std::nullopt;
}

std::optional<semantics::BindingInfo> extractExperimentalKeyValueValueReturnBinding(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string normalizedReturnType =
        semantics::normalizeBindingTypeName(transform.templateArgs.front());
    std::string base;
    std::string argText;
    semantics::BindingInfo binding;
    if (semantics::splitTemplateTypeName(normalizedReturnType, base, argText)) {
      if (semantics::normalizeBindingTypeName(base) == "Reference" ||
          semantics::normalizeBindingTypeName(base) == "Pointer") {
        continue;
      }
      binding.typeName = semantics::normalizeBindingTypeName(base);
      binding.typeTemplateArg = argText;
    } else {
      binding.typeName = normalizedReturnType;
    }
    if (isExperimentalKeyValueValueBinding(binding)) {
      return binding;
    }
  }
  return std::nullopt;
}

std::string borrowedExperimentalKeyValueHelperName(std::string_view methodName) {
  if (methodName == "count") {
    return "count_ref";
  }
  if (methodName == "contains") {
    return "contains_ref";
  }
  if (methodName == "tryAt") {
    return "tryAt_ref";
  }
  if (methodName == "at") {
    return "at_ref";
  }
  if (methodName == "at_unsafe") {
    return "at_unsafe_ref";
  }
  if (methodName == "insert") {
    return "insert_ref";
  }
  return {};
}

std::string experimentalKeyValueValueHelperName(std::string_view methodName) {
  if (methodName == "count") {
    return "count";
  }
  if (methodName == "contains") {
    return "contains";
  }
  if (methodName == "tryAt") {
    return "tryAt";
  }
  if (methodName == "at") {
    return "at";
  }
  if (methodName == "at_unsafe") {
    return "at_unsafe";
  }
  if (methodName == "insert") {
    return "insert";
  }
  return {};
}

bool isBuiltinVectorTypeText(const std::string &typeText) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(typeText);
  return normalizedType == "vector" || normalizedType.rfind("vector<", 0) == 0;
}

bool isBuiltinSoaVectorTypeText(const std::string &typeText) {
  std::string rawType = typeText;
  if (!rawType.empty() && rawType.front() == '/') {
    rawType.erase(rawType.begin());
  }
  if (semantics::isExperimentalSoaVectorTypePath(rawType)) {
    return false;
  }
  const std::string normalizedType = semantics::normalizeBindingTypeName(typeText);
  return semantics::isInternalSoaCollectionTypeName(normalizedType);
}

bool isExperimentalSoaVectorBaseName(const std::string &typeName) {
  std::string normalizedType = typeName;
  if (!normalizedType.empty() && normalizedType.front() == '/') {
    normalizedType.erase(normalizedType.begin());
  }
  return semantics::isExperimentalSoaVectorTypePath(normalizedType);
}

bool isExperimentalSoaVectorBinding(const semantics::BindingInfo &binding) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(binding.typeName);
  if (normalizedType == "Reference" || normalizedType == "Pointer") {
    return false;
  }
  return isExperimentalSoaVectorBaseName(binding.typeName);
}

bool isExperimentalSoaVectorOrBorrowedTypeText(std::string typeText) {
  typeText = semantics::normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string argText;
    if (!semantics::splitTemplateTypeName(typeText, base, argText) || base.empty()) {
      return isExperimentalSoaVectorBaseName(typeText);
    }
    const std::string normalizedBase = semantics::normalizeBindingTypeName(base);
    if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
      std::vector<std::string> args;
      if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      typeText = args.front();
      continue;
    }
    return isExperimentalSoaVectorBaseName(base);
  }
}

bool extractExperimentalSoaVectorElementTypeForFieldViewRewrite(const semantics::BindingInfo &binding,
                                                                std::string &elemTypeOut);

bool isBuiltinVectorBinding(const semantics::BindingInfo &binding) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(binding.typeName);
  if (normalizedType == "Reference" || normalizedType == "Pointer") {
    return false;
  }
  return isBuiltinVectorTypeText(bindingTypeText(binding));
}

bool isBuiltinSoaVectorBinding(const semantics::BindingInfo &binding) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(binding.typeName);
  if (normalizedType == "Reference" || normalizedType == "Pointer") {
    return false;
  }
  return isBuiltinSoaVectorTypeText(bindingTypeText(binding));
}

bool isBuiltinSoaVectorOrBorrowedBinding(const semantics::BindingInfo &binding) {
  if (isBuiltinSoaVectorBinding(binding)) {
    return true;
  }
  std::string elemType;
  return extractExperimentalSoaVectorElementTypeForFieldViewRewrite(binding, elemType);
}

std::optional<semantics::BindingInfo> extractBuiltinVectorBinding(const Expr &expr) {
  semantics::BindingInfo binding;
  std::optional<std::string> restrictType;
  std::string parseError;
  static const std::unordered_set<std::string> emptyStructTypes;
  static const std::unordered_map<std::string, std::string> emptyImportAliases;
  if (!semantics::parseBindingInfo(
          expr, expr.namespacePrefix, emptyStructTypes, emptyImportAliases, binding, restrictType, parseError)) {
    return std::nullopt;
  }
  return isBuiltinVectorBinding(binding) ? std::optional<semantics::BindingInfo>(binding) : std::nullopt;
}

std::optional<semantics::BindingInfo> extractBuiltinSoaVectorBinding(const Expr &expr) {
  semantics::BindingInfo binding;
  std::optional<std::string> restrictType;
  std::string parseError;
  static const std::unordered_set<std::string> emptyStructTypes;
  static const std::unordered_map<std::string, std::string> emptyImportAliases;
  if (!semantics::parseBindingInfo(
          expr, expr.namespacePrefix, emptyStructTypes, emptyImportAliases, binding, restrictType, parseError)) {
    return std::nullopt;
  }
  return isBuiltinSoaVectorBinding(binding) ? std::optional<semantics::BindingInfo>(binding) : std::nullopt;
}

std::optional<semantics::BindingInfo> extractExperimentalSoaVectorBinding(const Expr &expr) {
  semantics::BindingInfo binding;
  std::optional<std::string> restrictType;
  std::string parseError;
  static const std::unordered_set<std::string> emptyStructTypes;
  static const std::unordered_map<std::string, std::string> emptyImportAliases;
  if (!semantics::parseBindingInfo(
          expr, expr.namespacePrefix, emptyStructTypes, emptyImportAliases, binding, restrictType, parseError)) {
    return std::nullopt;
  }
  return isExperimentalSoaVectorBinding(binding) ? std::optional<semantics::BindingInfo>(binding) : std::nullopt;
}

std::optional<semantics::BindingInfo> extractBuiltinVectorReturnBinding(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    semantics::BindingInfo binding;
    std::string base;
    std::string argText;
    const std::string normalizedReturnType =
        semantics::normalizeBindingTypeName(transform.templateArgs.front());
    if (semantics::splitTemplateTypeName(normalizedReturnType, base, argText)) {
      if (semantics::normalizeBindingTypeName(base) == "Reference" ||
          semantics::normalizeBindingTypeName(base) == "Pointer") {
        continue;
      }
      binding.typeName = semantics::normalizeBindingTypeName(base);
      binding.typeTemplateArg = argText;
    } else {
      binding.typeName = normalizedReturnType;
    }
    if (isBuiltinVectorBinding(binding)) {
      return binding;
    }
  }
  return std::nullopt;
}

std::optional<semantics::BindingInfo> extractBuiltinSoaVectorReturnBindingImpl(
    const Definition &def,
    bool allowBorrowed) {
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    semantics::BindingInfo binding;
    std::string base;
    std::string argText;
    const std::string normalizedReturnType =
        semantics::normalizeBindingTypeName(transform.templateArgs.front());
    if (semantics::splitTemplateTypeName(normalizedReturnType, base, argText)) {
      const std::string normalizedBase = semantics::normalizeBindingTypeName(base);
      if ((normalizedBase == "Reference" || normalizedBase == "Pointer") &&
          !allowBorrowed) {
        continue;
      }
      binding.typeName = normalizedBase;
      binding.typeTemplateArg = argText;
    } else {
      binding.typeName = normalizedReturnType;
    }
    if ((allowBorrowed && isBuiltinSoaVectorOrBorrowedBinding(binding)) ||
        (!allowBorrowed && isBuiltinSoaVectorBinding(binding))) {
      return binding;
    }
  }
  return std::nullopt;
}

std::optional<semantics::BindingInfo> extractBuiltinSoaVectorReturnBinding(const Definition &def) {
  return extractBuiltinSoaVectorReturnBindingImpl(def, false);
}

std::optional<semantics::BindingInfo> extractBuiltinSoaVectorOrBorrowedReturnBinding(
    const Definition &def) {
  return extractBuiltinSoaVectorReturnBindingImpl(def, true);
}

std::optional<semantics::BindingInfo> extractExperimentalSoaVectorReturnBindingImpl(
    const Definition &def,
    bool allowBorrowed) {
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    semantics::BindingInfo binding;
    std::string base;
    std::string argText;
    const std::string returnType = transform.templateArgs.front();
    if (semantics::splitTemplateTypeName(returnType, base, argText)) {
      const std::string normalizedBase = semantics::normalizeBindingTypeName(base);
      if ((normalizedBase == "Reference" || normalizedBase == "Pointer") &&
          !allowBorrowed) {
        continue;
      }
      binding.typeName = base;
      binding.typeTemplateArg = argText;
    } else {
      binding.typeName = returnType;
    }
    if ((allowBorrowed && isExperimentalSoaVectorOrBorrowedTypeText(bindingTypeText(binding))) ||
        (!allowBorrowed && isExperimentalSoaVectorBinding(binding))) {
      return binding;
    }
  }
  return std::nullopt;
}

std::optional<semantics::BindingInfo> extractExperimentalSoaVectorOrBorrowedReturnBinding(
    const Definition &def) {
  return extractExperimentalSoaVectorReturnBindingImpl(def, true);
}

bool extractExperimentalSoaVectorElementTypeForFieldViewRewrite(const semantics::BindingInfo &binding,
                                                                const std::unordered_map<std::string, std::string>
                                                                    &specializedSoaVectorElementTypes,
                                                                std::string &elemTypeOut) {
  auto extractFromTypeText = [&](std::string normalizedType) {
    while (true) {
      std::string base;
      std::string argText;
      if (semantics::splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
        base = semantics::normalizeBindingTypeName(base);
        if (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          normalizedType = semantics::normalizeBindingTypeName(args.front());
          continue;
        }
        if (!base.empty() && base.front() == '/') {
          base.erase(base.begin());
        }
        if (semantics::isInternalOrExperimentalSoaStorageTypePath(base) &&
            !argText.empty()) {
          std::vector<std::string> args;
          if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          elemTypeOut = args.front();
          return true;
        }
      }

      std::string resolvedPath = normalizedType;
      if (!resolvedPath.empty() && resolvedPath.front() != '/') {
        resolvedPath.insert(resolvedPath.begin(), '/');
      }
      auto specializedIt = specializedSoaVectorElementTypes.find(resolvedPath);
      if (specializedIt != specializedSoaVectorElementTypes.end()) {
        elemTypeOut = specializedIt->second;
        return true;
      }
      std::string normalizedResolvedPath = semantics::normalizeBindingTypeName(resolvedPath);
      if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
        normalizedResolvedPath.erase(normalizedResolvedPath.begin());
      }
      if (!semantics::isExperimentalSoaVectorSpecializedTypePath(normalizedResolvedPath)) {
        return false;
      }
      auto normalizedIt = specializedSoaVectorElementTypes.find("/" + normalizedResolvedPath);
      if (normalizedIt == specializedSoaVectorElementTypes.end()) {
        normalizedIt = specializedSoaVectorElementTypes.find(normalizedResolvedPath);
      }
      if (normalizedIt == specializedSoaVectorElementTypes.end()) {
        return false;
      }
      elemTypeOut = normalizedIt->second;
      return true;
    }
  };

  elemTypeOut.clear();
  if (binding.typeTemplateArg.empty()) {
    return extractFromTypeText(semantics::normalizeBindingTypeName(binding.typeName));
  }
  return extractFromTypeText(
      semantics::normalizeBindingTypeName(binding.typeName + "<" + binding.typeTemplateArg + ">"));
}

bool extractExperimentalSoaVectorElementTypeForFieldViewRewrite(const semantics::BindingInfo &binding,
                                                                std::string &elemTypeOut) {
  static const std::unordered_map<std::string, std::string> emptySpecializedSoaVectorElementTypes;
  return extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
      binding, emptySpecializedSoaVectorElementTypes, elemTypeOut);
}

Expr makeI32LiteralExpr(uint64_t value, int sourceLine, int sourceColumn) {
  Expr expr;
  expr.kind = Expr::Kind::Literal;
  expr.literalValue = value;
  expr.intWidth = 32;
  expr.isUnsigned = false;
  expr.sourceLine = sourceLine;
  expr.sourceColumn = sourceColumn;
  return expr;
}

std::optional<size_t> extractNonNegativeI32LiteralIndex(const Expr &expr) {
  if (expr.kind != Expr::Kind::Literal || expr.isUnsigned ||
      (expr.intWidth != 0 && expr.intWidth != 32)) {
    return std::nullopt;
  }
  if (expr.literalValue > static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
    return std::nullopt;
  }
  return static_cast<size_t>(expr.literalValue);
}

bool extractSoaFieldViewElementTypeText(std::string typeText, std::string &elemTypeOut) {
  elemTypeOut.clear();
  std::string normalized = semantics::normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string argText;
    if (!semantics::splitTemplateTypeName(normalized, base, argText) || base.empty()) {
      return false;
    }
    base = semantics::normalizeBindingTypeName(base);
    if (base == "Reference" || base == "Pointer") {
      std::vector<std::string> args;
      if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      normalized = semantics::normalizeBindingTypeName(args.front());
      continue;
    }
    if (semantics::isSoaFieldViewTypePath(base)) {
      std::vector<std::string> args;
      if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      elemTypeOut = args.front();
      return true;
    }
    return false;
  }
}

bool extractSoaFieldViewElementTypeFromBinding(const semantics::BindingInfo &binding,
                                               std::string &elemTypeOut) {
  if (binding.typeName.empty()) {
    elemTypeOut.clear();
    return false;
  }
  const std::string typeText = binding.typeTemplateArg.empty()
                                   ? binding.typeName
                                   : binding.typeName + "<" + binding.typeTemplateArg + ">";
  return extractSoaFieldViewElementTypeText(typeText, elemTypeOut);
}

std::string qualifySoaFieldViewTypeText(const std::string &typeText,
                                        const std::string &namespacePrefix,
                                        const std::unordered_set<std::string> &structPaths) {
  const std::string normalized = semantics::normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (semantics::splitTemplateTypeName(normalized, base, argText) && !base.empty()) {
    std::vector<std::string> args;
    if (!semantics::splitTopLevelTemplateArgs(argText, args)) {
      return normalized;
    }
    for (std::string &arg : args) {
      arg = qualifySoaFieldViewTypeText(arg, namespacePrefix, structPaths);
    }
    const std::string resolvedBase = semantics::resolveStructTypePath(
        base, namespacePrefix, structPaths);
    const std::string qualifiedBase = resolvedBase.empty() ? base : resolvedBase;
    std::string rebuilt = qualifiedBase;
    rebuilt.push_back('<');
    for (size_t i = 0; i < args.size(); ++i) {
      if (i != 0) {
        rebuilt.append(", ");
      }
      rebuilt.append(args[i]);
    }
    rebuilt.push_back('>');
    return rebuilt;
  }
  const std::string resolved = semantics::resolveStructTypePath(
      normalized, namespacePrefix, structPaths);
  return resolved.empty() ? normalized : resolved;
}

bool extractExperimentalSoaColumnElementTypeFromSpecializedDefinition(
    const Definition &def,
    std::string &elemTypeOut) {
  elemTypeOut.clear();
  if (def.fullPath.rfind(collection_paths::specializedTypePrefix(collection_paths::kInternalSoaStorageFolder, collection_paths::kSoaColumnTypeName), 0) != 0) {
    return false;
  }
  for (const auto &fieldExpr : def.statements) {
    if (!fieldExpr.isBinding || fieldExpr.name != "data") {
      continue;
    }
    semantics::BindingInfo fieldBinding;
    std::optional<std::string> restrictType;
    std::string parseError;
    static const std::unordered_set<std::string> emptyStructTypes;
    static const std::unordered_map<std::string, std::string> emptyImportAliases;
    if (!semantics::parseBindingInfo(fieldExpr,
                                     def.namespacePrefix,
                                     emptyStructTypes,
                                     emptyImportAliases,
                                     fieldBinding,
                                     restrictType,
                                     parseError)) {
      continue;
    }
    if (semantics::normalizeBindingTypeName(fieldBinding.typeName) != "Pointer" ||
        fieldBinding.typeTemplateArg.empty()) {
      continue;
    }
    std::string pointeeBase;
    std::string pointeeArgText;
    if (!semantics::splitTemplateTypeName(semantics::normalizeBindingTypeName(fieldBinding.typeTemplateArg),
                                          pointeeBase,
                                          pointeeArgText) ||
        semantics::normalizeBindingTypeName(pointeeBase) != "uninitialized") {
      continue;
    }
    std::vector<std::string> pointeeArgs;
    if (!semantics::splitTopLevelTemplateArgs(pointeeArgText, pointeeArgs) ||
        pointeeArgs.size() != 1) {
      continue;
    }
    elemTypeOut = pointeeArgs.front();
    return true;
  }
  return false;
}

bool extractExperimentalSoaVectorElementTypeFromSpecializedDefinition(
    const Definition &def,
    const std::unordered_map<std::string, const Definition *> &definitionMap,
    std::string &elemTypeOut) {
  elemTypeOut.clear();
  if (!semantics::isExperimentalSoaVectorSpecializedTypePath(def.fullPath)) {
    return false;
  }
  for (const auto &fieldExpr : def.statements) {
    if (!fieldExpr.isBinding || fieldExpr.name != "storage") {
      continue;
    }
    semantics::BindingInfo fieldBinding;
    std::optional<std::string> restrictType;
    std::string parseError;
    static const std::unordered_set<std::string> emptyStructTypes;
    static const std::unordered_map<std::string, std::string> emptyImportAliases;
    if (!semantics::parseBindingInfo(fieldExpr,
                                     def.namespacePrefix,
                                     emptyStructTypes,
                                     emptyImportAliases,
                                     fieldBinding,
                                     restrictType,
                                     parseError)) {
      continue;
    }
    std::string normalizedFieldType = semantics::normalizeBindingTypeName(fieldBinding.typeName);
    if (!normalizedFieldType.empty() && normalizedFieldType.front() == '/') {
      normalizedFieldType.erase(normalizedFieldType.begin());
    }
    if (!fieldBinding.typeTemplateArg.empty() &&
        (normalizedFieldType == "SoaColumn" ||
         normalizedFieldType == collection_paths::memberPathBare(collection_paths::kInternalSoaStorageFolder, collection_paths::kSoaColumnTypeName))) {
      std::vector<std::string> args;
      if (!semantics::splitTopLevelTemplateArgs(fieldBinding.typeTemplateArg, args) || args.size() != 1) {
        continue;
      }
      elemTypeOut = args.front();
      return true;
    }
    std::string resolvedFieldPath = semantics::normalizeBindingTypeName(fieldBinding.typeName);
    if (!resolvedFieldPath.empty() && resolvedFieldPath.front() != '/') {
      resolvedFieldPath.insert(resolvedFieldPath.begin(), '/');
    }
    auto defIt = definitionMap.find(resolvedFieldPath);
    if (defIt == definitionMap.end() || defIt->second == nullptr) {
      continue;
    }
    if (!extractExperimentalSoaColumnElementTypeFromSpecializedDefinition(*defIt->second, elemTypeOut)) {
      continue;
    }
    return true;
  }
  return false;
}

std::unordered_map<std::string, std::string> buildSpecializedExperimentalSoaVectorElementTypes(
    const Program &program) {
  std::unordered_map<std::string, const Definition *> definitionMap;
  for (const Definition &def : program.definitions) {
    definitionMap[def.fullPath] = &def;
  }

  std::unordered_map<std::string, std::string> elemTypes;
  for (const Definition &def : program.definitions) {
    std::string elemType;
    if (!extractExperimentalSoaVectorElementTypeFromSpecializedDefinition(def, definitionMap, elemType)) {
      continue;
    }
    elemTypes[def.fullPath] = elemType;
    if (!def.fullPath.empty() && def.fullPath.front() == '/') {
      elemTypes[def.fullPath.substr(1)] = elemType;
    }
  }
  return elemTypes;
}

bool extractExperimentalSoaVectorElementTypeForToAosRewrite(const semantics::BindingInfo &binding,
                                                            std::string &elemTypeOut) {
  auto extractFromTypeText = [&](std::string normalizedType) {
    while (true) {
      std::string base;
      std::string argText;
      if (semantics::splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
        base = semantics::normalizeBindingTypeName(base);
        if (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          normalizedType = semantics::normalizeBindingTypeName(args.front());
          continue;
        }
        std::string normalizedBase = base;
        if (!normalizedBase.empty() && normalizedBase.front() == '/') {
          normalizedBase.erase(normalizedBase.begin());
        }
        if (semantics::isExperimentalSoaVectorTypePath(normalizedBase) &&
            !argText.empty()) {
          std::vector<std::string> args;
          if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          elemTypeOut = args.front();
          return true;
        }
      }

      std::string resolvedPath = normalizedType;
      if (!resolvedPath.empty() && resolvedPath.front() != '/') {
        resolvedPath.insert(resolvedPath.begin(), '/');
      }
      std::string normalizedResolvedPath = semantics::normalizeBindingTypeName(resolvedPath);
      if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
        normalizedResolvedPath.erase(normalizedResolvedPath.begin());
      }
      if (!semantics::isExperimentalSoaVectorSpecializedTypePath(normalizedResolvedPath)) {
        return false;
      }
      elemTypeOut = resolvedPath;
      return true;
    }
  };

  elemTypeOut.clear();
  if (binding.typeTemplateArg.empty()) {
    return extractFromTypeText(semantics::normalizeBindingTypeName(binding.typeName));
  }
  return extractFromTypeText(
      semantics::normalizeBindingTypeName(binding.typeName + "<" + binding.typeTemplateArg + ">"));
}

std::optional<semantics::BindingInfo> extractExperimentalSoaVectorFieldViewReceiverBinding(const Expr &expr) {
  if (auto directBinding = extractExperimentalSoaVectorBinding(expr); directBinding.has_value()) {
    return directBinding;
  }

  for (const auto &transform : expr.transforms) {
    if ((transform.name != "Reference" && transform.name != "Pointer") ||
        transform.templateArgs.size() != 1 || !transform.arguments.empty()) {
      continue;
    }
    semantics::BindingInfo binding;
    binding.typeName = transform.name;
    binding.typeTemplateArg = transform.templateArgs.front();
    std::string elemType;
    if (extractExperimentalSoaVectorElementTypeForFieldViewRewrite(binding, elemType)) {
      return binding;
    }
  }
  return std::nullopt;
}

} // namespace primec
