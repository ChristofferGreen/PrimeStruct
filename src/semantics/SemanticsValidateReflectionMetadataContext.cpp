#include "SemanticsValidateReflectionMetadataInternal.h"

#include "SemanticsHelpers.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>

namespace primec::semantics {

namespace {

bool isDefinitionPublic(const Definition &def) {
  bool sawVisibility = false;
  bool isPublic = false;
  for (const auto &transform : def.transforms) {
    if (transform.name != "public" && transform.name != "private") {
      continue;
    }
    if (sawVisibility) {
      return false;
    }
    sawVisibility = true;
    isPublic = (transform.name == "public");
  }
  return isPublic;
}

std::string trimReflectionMetadataText(const std::string &text) {
  const size_t start = text.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return {};
  }
  const size_t end = text.find_last_not_of(" \t\r\n");
  return text.substr(start, end - start + 1);
}

std::string resolveReflectionFieldTypeName(const ReflectionMetadataRewriteContext &context,
                                           const Definition &def,
                                           const Expr &stmt) {
  std::string typeCandidate;
  bool hasExplicitType = false;
  for (const auto &transform : stmt.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      continue;
    }
    if (transform.name == "restrict" && transform.templateArgs.size() == 1) {
      typeCandidate = transform.templateArgs.front();
      hasExplicitType = true;
      continue;
    }
    if (isBindingAuxTransformName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    if (!transform.templateArgs.empty()) {
      typeCandidate = transform.name + "<" + joinTemplateArgs(transform.templateArgs) + ">";
    } else {
      typeCandidate = transform.name;
    }
    hasExplicitType = true;
    break;
  }

  if (!hasExplicitType && stmt.args.size() == 1) {
    BindingInfo inferred;
    if (tryInferBindingTypeFromInitializer(stmt.args.front(), {}, {}, inferred, true)) {
      typeCandidate = inferred.typeName;
      if (!inferred.typeTemplateArg.empty()) {
        typeCandidate += "<" + inferred.typeTemplateArg + ">";
      }
    }
  }
  if (typeCandidate.empty()) {
    typeCandidate = "int";
  }

  std::string canonicalType;
  std::string ignoredStructPath;
  std::string ignoredError;
  if (context.canonicalizeTypeName(typeCandidate, def.namespacePrefix, canonicalType, ignoredStructPath, ignoredError)) {
    return canonicalType;
  }
  return normalizeBindingTypeName(typeCandidate);
}

struct LayoutInfo {
  uint32_t sizeBytes = 0;
  uint32_t alignmentBytes = 1;
};

bool isStaticReflectionField(const Expr &stmt) {
  return std::any_of(stmt.transforms.begin(), stmt.transforms.end(), [](const Transform &transform) {
    return transform.name == "static";
  });
}

uint32_t alignReflectionValue(uint32_t value, uint32_t alignment) {
  if (alignment == 0) {
    return value;
  }
  const uint32_t remainder = value % alignment;
  return remainder == 0 ? value : value + (alignment - remainder);
}

bool parsePositiveAlignmentLiteral(const std::string &text, int &valueOut) {
  std::string digits = text;
  if (digits.size() > 3 && digits.compare(digits.size() - 3, 3, "i32") == 0) {
    digits.resize(digits.size() - 3);
  }
  if (digits.empty()) {
    return false;
  }
  int parsed = 0;
  for (char c : digits) {
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      return false;
    }
    const int digit = c - '0';
    if (parsed > (std::numeric_limits<int>::max() - digit) / 10) {
      return false;
    }
    parsed = parsed * 10 + digit;
  }
  if (parsed <= 0) {
    return false;
  }
  valueOut = parsed;
  return true;
}

bool extractReflectionAlignment(const std::vector<Transform> &transforms, uint32_t &alignmentOut, bool &hasAlignment) {
  alignmentOut = 1;
  hasAlignment = false;
  std::string ignoredError;
  for (const auto &transform : transforms) {
    if (transform.name != "align_bytes" && transform.name != "align_kbytes") {
      continue;
    }
    if (hasAlignment) {
      return false;
    }
    if (!validateAlignTransform(transform, "reflection layout", ignoredError)) {
      return false;
    }
    int value = 0;
    if (!parsePositiveAlignmentLiteral(transform.arguments[0], value)) {
      return false;
    }
    uint64_t bytes = static_cast<uint64_t>(value);
    if (transform.name == "align_kbytes") {
      bytes *= 1024ull;
    }
    if (bytes > std::numeric_limits<uint32_t>::max()) {
      return false;
    }
    alignmentOut = static_cast<uint32_t>(bytes);
    hasAlignment = true;
  }
  return true;
}

} // namespace

bool ReflectionMetadataRewriteContext::canonicalizeTypeName(const std::string &typeName,
                                                            const std::string &namespacePrefix,
                                                            std::string &canonicalOut,
                                                            std::string &structPathOut,
                                                            std::string &error) const {
  canonicalOut.clear();
  structPathOut.clear();
  const std::string trimmed = trimReflectionMetadataText(typeName);
  if (trimmed.empty()) {
    error = "reflection query requires a type argument";
    return false;
  }
  if (trimmed == "Self" && structNames.count(namespacePrefix) > 0) {
    canonicalOut = namespacePrefix;
    structPathOut = namespacePrefix;
    return true;
  }

  const std::string normalized = normalizeBindingTypeName(trimmed);
  std::string resolvedStruct = resolveStructTypePath(normalized, namespacePrefix, structNames);
  if (resolvedStruct.empty()) {
    auto importIt = importAliases.find(normalized);
    if (importIt != importAliases.end() && structNames.count(importIt->second) > 0) {
      resolvedStruct = importIt->second;
    }
  }
  if (!resolvedStruct.empty()) {
    canonicalOut = resolvedStruct;
    structPathOut = resolvedStruct;
    return true;
  }

  std::string base;
  std::string arg;
  if (!splitTemplateTypeName(normalized, base, arg)) {
    canonicalOut = normalized;
    return true;
  }

  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(arg, args) || args.empty()) {
    canonicalOut = normalized;
    return true;
  }

  std::string canonicalBase;
  std::string baseStructPath;
  if (!canonicalizeTypeName(base, namespacePrefix, canonicalBase, baseStructPath, error)) {
    return false;
  }
  std::vector<std::string> canonicalArgs;
  canonicalArgs.reserve(args.size());
  for (const auto &nestedArg : args) {
    std::string canonicalArg;
    std::string nestedStructPath;
    if (!canonicalizeTypeName(nestedArg, namespacePrefix, canonicalArg, nestedStructPath, error)) {
      return false;
    }
    canonicalArgs.push_back(std::move(canonicalArg));
  }
  canonicalOut = canonicalBase + "<" + joinTemplateArgs(canonicalArgs) + ">";
  return true;
}

ReflectionMetadataRewriteContext buildReflectionMetadataRewriteContext(const Program &program) {
  ReflectionMetadataRewriteContext context;
  context.structNames.reserve(program.definitions.size());
  context.reflectedStructNames.reserve(program.definitions.size());
  context.definitionByPath.reserve(program.definitions.size());

  std::unordered_set<std::string> publicDefinitions;
  publicDefinitions.reserve(program.definitions.size());

  for (const auto &def : program.definitions) {
    context.definitionByPath.emplace(def.fullPath, &def);
    bool hasStructTransform = false;
    bool hasReturnTransform = false;
    bool hasSumTransform = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "sum") {
        hasSumTransform = true;
      }
      if (isStructTransformName(transform.name)) {
        hasStructTransform = true;
      }
      if (transform.name == "return") {
        hasReturnTransform = true;
      }
    }
    bool fieldOnlyStruct = false;
    if (!hasSumTransform && !hasStructTransform && !hasReturnTransform && def.parameters.empty() &&
        !def.hasReturnStatement && !def.returnExpr.has_value()) {
      fieldOnlyStruct = true;
      for (const auto &stmt : def.statements) {
        if (!stmt.isBinding) {
          fieldOnlyStruct = false;
          break;
        }
      }
    }
    if (hasStructTransform || fieldOnlyStruct) {
      context.structNames.insert(def.fullPath);
      if (std::any_of(def.transforms.begin(), def.transforms.end(), [](const Transform &transform) {
            return transform.name == "reflect" || transform.name == "generate";
          })) {
        context.reflectedStructNames.insert(def.fullPath);
      }
    }
    if (isDefinitionPublic(def)) {
      publicDefinitions.insert(def.fullPath);
    }
  }

  for (const auto &importPath : program.imports) {
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
        context.importAliases.emplace(remainder, def.fullPath);
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
    context.importAliases.emplace(remainder, importPath);
  }

  std::unordered_map<std::string, LayoutInfo> layoutCache;
  std::unordered_set<std::string> layoutStack;

  std::function<bool(const std::string &, const std::string &, LayoutInfo &)> typeLayoutForReflectionType;
  std::function<bool(const Definition &, ReflectionStructLayoutMetadata &, LayoutInfo &)> computeStructLayout;

  typeLayoutForReflectionType = [&](const std::string &canonicalType,
                                    const std::string &namespacePrefix,
                                    LayoutInfo &layoutOut) -> bool {
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(canonicalType, base, arg)) {
      const std::string normalizedBase = normalizeBindingTypeName(base);
      if (normalizedBase == "Pointer" || normalizedBase == "Reference") {
        layoutOut = {8u, 8u};
        return true;
      }
      if (normalizedBase == "array" || normalizedBase == "vector" || normalizedBase == "map" ||
          normalizedBase == "soa_vector") {
        layoutOut = {8u, 8u};
        return true;
      }
      if (normalizedBase == "Result") {
        if (arg.empty()) {
          return false;
        }
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(arg, args) || (args.size() != 1 && args.size() != 2)) {
          return false;
        }
        layoutOut = (args.size() == 1) ? LayoutInfo{4u, 4u} : LayoutInfo{8u, 8u};
        return true;
      }
      if (normalizedBase == "uninitialized") {
        if (arg.empty()) {
          return false;
        }
        return typeLayoutForReflectionType(arg, namespacePrefix, layoutOut);
      }
    }

    const std::string normalized = normalizeBindingTypeName(canonicalType);
    if (normalized == "i32" || normalized == "int" || normalized == "f32") {
      layoutOut = {4u, 4u};
      return true;
    }
    if (normalized == "i64" || normalized == "u64" || normalized == "f64") {
      layoutOut = {8u, 8u};
      return true;
    }
    if (normalized == "bool") {
      layoutOut = {1u, 1u};
      return true;
    }
    if (normalized == "string") {
      layoutOut = {8u, 8u};
      return true;
    }

    std::string structPath = canonicalType;
    if (structPath.empty() || structPath.front() != '/') {
      structPath = resolveStructTypePath(canonicalType, namespacePrefix, context.structNames);
      if (structPath.empty()) {
        auto importIt = context.importAliases.find(canonicalType);
        if (importIt != context.importAliases.end() && context.structNames.count(importIt->second) > 0) {
          structPath = importIt->second;
        }
      }
    }
    auto defIt = context.definitionByPath.find(structPath);
    if (defIt == context.definitionByPath.end()) {
      return false;
    }

    ReflectionStructLayoutMetadata ignoredMetadata;
    return computeStructLayout(*defIt->second, ignoredMetadata, layoutOut);
  };

  computeStructLayout = [&](const Definition &def,
                            ReflectionStructLayoutMetadata &metadataOut,
                            LayoutInfo &layoutOut) -> bool {
    auto cached = layoutCache.find(def.fullPath);
    if (cached != layoutCache.end()) {
      layoutOut = cached->second;
      auto metadataIt = context.structLayoutMetadata.find(def.fullPath);
      if (metadataIt != context.structLayoutMetadata.end()) {
        metadataOut = metadataIt->second;
      }
      return true;
    }
    if (!layoutStack.insert(def.fullPath).second) {
      return false;
    }

    bool requireNoPadding = false;
    bool requirePlatformIndependentPadding = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "no_padding") {
        requireNoPadding = true;
      } else if (transform.name == "platform_independent_padding") {
        requirePlatformIndependentPadding = true;
      }
    }

    uint32_t structAlign = 1;
    uint32_t explicitStructAlign = 1;
    bool hasStructAlign = false;
    if (!extractReflectionAlignment(def.transforms, explicitStructAlign, hasStructAlign)) {
      layoutStack.erase(def.fullPath);
      return false;
    }

    uint32_t offset = 0;
    ReflectionStructLayoutMetadata metadata;
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding || isStaticReflectionField(stmt)) {
        continue;
      }

      const std::string fieldTypeName = resolveReflectionFieldTypeName(context, def, stmt);
      LayoutInfo fieldLayout;
      if (!typeLayoutForReflectionType(fieldTypeName, def.namespacePrefix, fieldLayout)) {
        layoutStack.erase(def.fullPath);
        return false;
      }

      uint32_t explicitFieldAlign = 1;
      bool hasFieldAlign = false;
      if (!extractReflectionAlignment(stmt.transforms, explicitFieldAlign, hasFieldAlign)) {
        layoutStack.erase(def.fullPath);
        return false;
      }
      if (hasFieldAlign && explicitFieldAlign < fieldLayout.alignmentBytes) {
        layoutStack.erase(def.fullPath);
        return false;
      }

      const uint32_t fieldAlign =
          hasFieldAlign ? std::max(explicitFieldAlign, fieldLayout.alignmentBytes) : fieldLayout.alignmentBytes;
      const uint32_t alignedOffset = alignReflectionValue(offset, fieldAlign);
      if (requireNoPadding && alignedOffset != offset) {
        layoutStack.erase(def.fullPath);
        return false;
      }
      if (requirePlatformIndependentPadding && alignedOffset != offset && !hasFieldAlign) {
        layoutStack.erase(def.fullPath);
        return false;
      }

      metadata.fieldOffsetBytes.emplace(stmt.name, alignedOffset);
      offset = alignedOffset + fieldLayout.sizeBytes;
      structAlign = std::max(structAlign, fieldAlign);
    }

    if (hasStructAlign && explicitStructAlign < structAlign) {
      layoutStack.erase(def.fullPath);
      return false;
    }
    structAlign = hasStructAlign ? std::max(structAlign, explicitStructAlign) : structAlign;
    const uint32_t totalSize = alignReflectionValue(offset, structAlign);
    if (requireNoPadding && totalSize != offset) {
      layoutStack.erase(def.fullPath);
      return false;
    }
    if (requirePlatformIndependentPadding && totalSize != offset && !hasStructAlign) {
      layoutStack.erase(def.fullPath);
      return false;
    }

    metadata.elementStrideBytes = totalSize;
    const LayoutInfo layout{totalSize, structAlign};
    layoutCache.emplace(def.fullPath, layout);
    context.structLayoutMetadata.emplace(def.fullPath, metadata);
    layoutStack.erase(def.fullPath);
    metadataOut = std::move(metadata);
    layoutOut = layout;
    return true;
  };

  for (const auto &def : program.definitions) {
    if (context.structNames.count(def.fullPath) == 0) {
      continue;
    }
    std::vector<ReflectionFieldMetadata> fields;
    fields.reserve(def.statements.size());
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        continue;
      }
      ReflectionFieldMetadata field;
      field.name = stmt.name;
      field.typeName = resolveReflectionFieldTypeName(context, def, stmt);
      field.visibility = "public";
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "private") {
          field.visibility = "private";
          break;
        }
        if (transform.name == "public") {
          field.visibility = "public";
          break;
        }
      }
      fields.push_back(std::move(field));
    }
    context.structFieldMetadata.emplace(def.fullPath, std::move(fields));

    ReflectionStructLayoutMetadata layoutMetadata;
    LayoutInfo layout;
    computeStructLayout(def, layoutMetadata, layout);
  }

  return context;
}

} // namespace primec::semantics
