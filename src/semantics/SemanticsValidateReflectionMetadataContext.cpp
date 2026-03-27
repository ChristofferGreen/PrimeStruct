#include "SemanticsValidateReflectionMetadataInternal.h"

#include "SemanticsHelpers.h"

#include <algorithm>
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
    for (const auto &transform : def.transforms) {
      if (isStructTransformName(transform.name)) {
        hasStructTransform = true;
      }
      if (transform.name == "return") {
        hasReturnTransform = true;
      }
    }
    bool fieldOnlyStruct = false;
    if (!hasStructTransform && !hasReturnTransform && def.parameters.empty() && !def.hasReturnStatement &&
        !def.returnExpr.has_value()) {
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
            return transform.name == "reflect";
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
  }

  return context;
}

} // namespace primec::semantics
