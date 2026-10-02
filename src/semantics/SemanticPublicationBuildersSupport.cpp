#include "SemanticPublicationBuilders.h"

#include "RequirementPredicateFacts.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/support/CollectionHelperNames.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <utility>
#include "SemanticPublicationBuildersInternal.h"

namespace primec {
namespace semantics {
namespace publicationBuilders {


std::string returnKindSnapshotName(ReturnKind kind) {
  switch (kind) {
    case ReturnKind::Unknown:
      return "unknown";
    case ReturnKind::Int:
      return "i32";
    case ReturnKind::Int64:
      return "i64";
    case ReturnKind::UInt64:
      return "u64";
    case ReturnKind::Float32:
      return "f32";
    case ReturnKind::Float64:
      return "f64";
    case ReturnKind::Integer:
      return "integer";
    case ReturnKind::Decimal:
      return "decimal";
    case ReturnKind::Complex:
      return "complex";
    case ReturnKind::Bool:
      return "bool";
    case ReturnKind::String:
      return "string";
    case ReturnKind::Void:
      return "void";
    case ReturnKind::Array:
      return "array";
  }
  return "unknown";
}

std::string bindingTypeTextForSemanticProduct(const BindingInfo &binding) {
  if (binding.typeName.empty()) {
    return {};
  }
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

uint64_t makeSemanticProvenanceHandle(uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return 0;
  }

  constexpr uint64_t FnvOffsetBasis = 14695981039346656037ull;
  constexpr uint64_t FnvPrime = 1099511628211ull;
  constexpr std::string_view Domain = "semantic_provenance";

  uint64_t hash = FnvOffsetBasis;
  for (unsigned char ch : Domain) {
    hash ^= static_cast<uint64_t>(ch);
    hash *= FnvPrime;
  }
  for (size_t i = 0; i < sizeof(semanticNodeId); ++i) {
    const auto byte = static_cast<unsigned char>((semanticNodeId >> (i * 8u)) & 0xffu);
    hash ^= static_cast<uint64_t>(byte);
    hash *= FnvPrime;
  }
  return hash == 0 ? 1 : hash;
}

std::string normalizeSemanticModulePathKey(std::string_view path) {
  if (path.empty()) {
    return "/";
  }

  std::string normalized(path);
  if (normalized.front() != '/') {
    normalized.insert(normalized.begin(), '/');
  }
  return normalized;
}

bool semanticSourceUnitImportMatchesPath(std::string_view importPath, std::string_view path) {
  const std::string normalizedImportPath = normalizeSemanticModulePathKey(importPath);
  const std::string normalizedPath = normalizeSemanticModulePathKey(path);
  if (normalizedImportPath == "/") {
    return true;
  }

  std::string_view importPrefix = normalizedImportPath;
  if (normalizedImportPath.size() >= 2 &&
      normalizedImportPath.compare(normalizedImportPath.size() - 2, 2, "/*") == 0) {
    importPrefix = std::string_view(normalizedImportPath).substr(0, normalizedImportPath.size() - 2);
  }

  if (normalizedPath == importPrefix) {
    return true;
  }
  return normalizedPath.size() > importPrefix.size() &&
         normalizedPath.rfind(importPrefix, 0) == 0 &&
         normalizedPath[importPrefix.size()] == '/';
}

std::string semanticSourceUnitModuleKeyForPath(
    std::string_view path,
    const std::vector<std::string> &sourceImports,
    const std::vector<std::string> &imports) {
  const std::string normalizedPath = normalizeSemanticModulePathKey(path);

  std::string bestMatch;
  std::size_t bestSpecificity = 0;
  auto considerImportPath = [&](std::string_view importPath) {
    if (!semanticSourceUnitImportMatchesPath(importPath, normalizedPath)) {
      return;
    }

    const std::string normalizedImportPath = normalizeSemanticModulePathKey(importPath);
    const bool isWildcard =
        normalizedImportPath.size() >= 2 &&
        normalizedImportPath.compare(normalizedImportPath.size() - 2, 2, "/*") == 0;
    const std::size_t specificity =
        normalizedImportPath.size() * 2 + (isWildcard ? 0u : 1u);
    if (!bestMatch.empty() && specificity <= bestSpecificity) {
      return;
    }
    bestMatch = normalizedImportPath;
    bestSpecificity = specificity;
  };

  for (const auto &importPath : sourceImports) {
    considerImportPath(importPath);
  }
  for (const auto &importPath : imports) {
    considerImportPath(importPath);
  }

  if (!bestMatch.empty()) {
    return bestMatch;
  }
  return "/";
}

std::size_t semanticSourceUnitImportOrderKeyForModuleKey(
    std::string_view moduleKey,
    const std::vector<std::string> &sourceImports,
    const std::vector<std::string> &imports) {
  const std::string normalizedModuleKey =
      normalizeSemanticModulePathKey(moduleKey);
  if (normalizedModuleKey == "/") {
    return 0;
  }

  auto findExactImportOrder = [&](const std::vector<std::string> &paths,
                                  std::size_t baseOrder) -> std::optional<std::size_t> {
    for (std::size_t i = 0; i < paths.size(); ++i) {
      if (normalizeSemanticModulePathKey(paths[i]) == normalizedModuleKey) {
        return baseOrder + i;
      }
    }
    return std::nullopt;
  };

  if (const auto sourceOrder =
          findExactImportOrder(sourceImports, 1);
      sourceOrder.has_value()) {
    return *sourceOrder;
  }
  if (const auto importOrder =
          findExactImportOrder(imports, 1 + sourceImports.size());
      importOrder.has_value()) {
    return *importOrder;
  }

  return 1 + sourceImports.size() + imports.size();
}

std::string fallbackBindingResolvedPathForSemanticProduct(std::string_view scopePath,
                                                          std::string_view bindingName) {
  if (scopePath.empty() || bindingName.empty()) {
    return {};
  }
  if (bindingName.front() == '/') {
    return std::string(bindingName);
  }
  std::string normalizedScope(scopePath);
  if (!normalizedScope.empty() && normalizedScope.front() != '/') {
    normalizedScope.insert(normalizedScope.begin(), '/');
  }
  if (!normalizedScope.empty() && normalizedScope.back() != '/') {
    normalizedScope.push_back('/');
  }
  normalizedScope.append(bindingName);
  return normalizedScope;
}

std::optional<StdlibSurfaceId> classifyPublishedStdlibSurfaceId(std::string_view resolvedPath) {
  if (const auto *metadata = findStdlibSurfaceMetadataByResolvedPath(resolvedPath);
      metadata != nullptr) {
    return metadata->id;
  }
  return std::nullopt;
}

bool isSoftwareNumericName(std::string_view name) {
  return name == "integer" || name == "decimal" || name == "complex";
}

bool isReflectionMetadataQueryName(std::string_view name) {
  return name == "type_name" || name == "type_kind" || name == "is_struct" ||
         name == "field_count" || name == "field_name" || name == "field_type" ||
         name == "field_visibility" || name == "has_transform" ||
         name == "has_trait";
}

bool isReflectionMetadataQueryPath(std::string_view path) {
  constexpr std::string_view prefix = "/meta/";
  if (!path.starts_with(prefix)) {
    return false;
  }
  const std::string_view queryName = path.substr(prefix.size());
  return !queryName.empty() && queryName.find('/') == std::string_view::npos &&
         isReflectionMetadataQueryName(queryName);
}

bool isRuntimeReflectionPath(std::string_view path) {
  return path == "/meta/object" || path == "/meta/table" ||
         path.starts_with("/meta/object/") || path.starts_with("/meta/table/");
}

bool splitTopLevelTemplateArgs(std::string_view text, std::vector<std::string> &out) {
  out.clear();
  int depth = 0;
  size_t start = 0;
  auto pushSegment = [&](size_t end) {
    size_t segStart = start;
    while (segStart < end &&
           std::isspace(static_cast<unsigned char>(text[segStart]))) {
      ++segStart;
    }
    size_t segEnd = end;
    while (segEnd > segStart &&
           std::isspace(static_cast<unsigned char>(text[segEnd - 1]))) {
      --segEnd;
    }
    out.emplace_back(text.substr(segStart, segEnd - segStart));
  };
  for (size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
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
  for (const auto &segment : out) {
    if (segment.empty()) {
      return false;
    }
  }
  return !out.empty();
}

std::string findSoftwareNumericType(std::string_view typeName) {
  if (typeName.empty()) {
    return {};
  }
  std::string base;
  std::string arg;
  if (!splitTemplateTypeName(std::string(typeName), base, arg)) {
    return isSoftwareNumericName(typeName) ? std::string(typeName) : std::string{};
  }
  if (isSoftwareNumericName(base)) {
    return base;
  }
  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(arg, args)) {
    return {};
  }
  for (const auto &nested : args) {
    std::string found = findSoftwareNumericType(nested);
    if (!found.empty()) {
      return found;
    }
  }
  return {};
}

std::string scanTransformsForSoftwareNumeric(const std::vector<Transform> &transforms) {
  for (const auto &transform : transforms) {
    std::string found = findSoftwareNumericType(transform.name);
    if (!found.empty()) {
      return found;
    }
    for (const auto &arg : transform.templateArgs) {
      found = findSoftwareNumericType(arg);
      if (!found.empty()) {
        return found;
      }
    }
  }
  return {};
}

std::string scanExprForSoftwareNumeric(const Expr &expr) {
  std::string found = scanTransformsForSoftwareNumeric(expr.transforms);
  if (!found.empty()) {
    return found;
  }
  for (const auto &arg : expr.templateArgs) {
    found = findSoftwareNumericType(arg);
    if (!found.empty()) {
      return found;
    }
  }
  for (const auto &arg : expr.args) {
    found = scanExprForSoftwareNumeric(arg);
    if (!found.empty()) {
      return found;
    }
  }
  for (const auto &arg : expr.bodyArguments) {
    found = scanExprForSoftwareNumeric(arg);
    if (!found.empty()) {
      return found;
    }
  }
  return {};
}

std::string scanExprForRuntimeReflectionQuery(const Expr &expr) {
  if (expr.kind == Expr::Kind::Call &&
      (isReflectionMetadataQueryPath(expr.name) ||
       isRuntimeReflectionPath(expr.name))) {
    return expr.name;
  }
  for (const auto &arg : expr.args) {
    std::string found = scanExprForRuntimeReflectionQuery(arg);
    if (!found.empty()) {
      return found;
    }
  }
  for (const auto &arg : expr.bodyArguments) {
    std::string found = scanExprForRuntimeReflectionQuery(arg);
    if (!found.empty()) {
      return found;
    }
  }
  return {};
}

void releaseInternedField(std::string &text, SymbolId id) {
  if (id != InvalidSymbolId) {
    text.clear();
  }
}

uint64_t makeLocalAutoInitPathBindingNameKey(SymbolId initializerPathId, SymbolId bindingNameId) {
  return (static_cast<uint64_t>(initializerPathId) << 32) |
         static_cast<uint64_t>(bindingNameId);
}

uint64_t makeQueryFactResolvedPathCallNameKey(SymbolId resolvedPathId, SymbolId callNameId) {
  return (static_cast<uint64_t>(resolvedPathId) << 32) |
         static_cast<uint64_t>(callNameId);
}

uint64_t makeTryFactOperandPathSourceKey(SymbolId operandPathId, int sourceLine, int sourceColumn) {
  const uint64_t lineBits = static_cast<uint64_t>(
      static_cast<uint32_t>(sourceLine > 0 ? sourceLine : 0));
  const uint64_t columnBits = static_cast<uint64_t>(
      static_cast<uint32_t>(sourceColumn > 0 ? sourceColumn : 0));
  return (static_cast<uint64_t>(operandPathId) << 32) ^
         (lineBits * 1315423911ULL) ^
         columnBits;
}

uint64_t makeSumVariantMetadataSumPathVariantNameKey(SymbolId sumPathId, SymbolId variantNameId) {
  return (static_cast<uint64_t>(sumPathId) << 32) |
         static_cast<uint64_t>(variantNameId);
}

SemanticProgramRequirementPredicateFact classifyRequirementPredicateFact(
    const Definition &definition,
    const Transform &transform,
    std::vector<std::string> compileTimeEffects,
    std::string sourceText,
    const RequirementPredicateDefinitionContext &context) {
  SemanticProgramRequirementPredicateFact fact;
  fact.definitionPath = definition.fullPath;
  fact.sourceLine = transform.sourceLine > 0 ? transform.sourceLine : definition.sourceLine;
  fact.sourceColumn = transform.sourceColumn > 0 ? transform.sourceColumn : definition.sourceColumn;
  fact.semanticNodeId = definition.semanticNodeId;
  fact.provenanceHandle = makeSemanticProvenanceHandle(definition.semanticNodeId);
  const RequirementPredicateFactDraft draft =
      buildRequirementPredicateFactDraft(std::move(sourceText),
                                         fact.sourceLine,
                                         fact.sourceColumn,
                                         context);
  fact.predicateKind = draft.predicateKind;
  fact.predicateName = draft.predicateName;
  fact.relationOperator = draft.relationOperator;
  fact.sourceText = draft.sourceText;
  fact.compileTimeEffects = std::move(compileTimeEffects);
  fact.evaluationOutcome = draft.evaluationOutcome;
  fact.evaluationDiagnostic = draft.evaluationDiagnostic;
  fact.operands.reserve(draft.operands.size());
  for (const auto &draftOperand : draft.operands) {
    SemanticProgramRequirementPredicateOperand operand;
    operand.kind = draftOperand.kind;
    operand.text = draftOperand.text;
    operand.stableHandle = draftOperand.stableHandle;
    operand.sourceLine = draftOperand.sourceLine;
    operand.sourceColumn = draftOperand.sourceColumn;
    fact.operands.push_back(std::move(operand));
  }
  return fact;
}

std::vector<std::string> definitionCompileTimeEffects(
    const Definition &definition) {
  std::vector<std::string> effects;
  for (const Transform &transform : definition.transforms) {
    if (transform.name == "effects" && transform.templateArgs.size() == 1 &&
        transform.templateArgs.front() == "compiletime") {
      effects.insert(effects.end(),
                     transform.arguments.begin(),
                     transform.arguments.end());
    }
  }
  std::sort(effects.begin(), effects.end());
  effects.erase(std::unique(effects.begin(), effects.end()), effects.end());
  return effects;
}

std::string collectionTypeRootForPublication(std::string_view collectionName,
                                             bool leadingSlash) {
  std::string root = leadingSlash ? "/" : "";
  root += "std/collections/";
  root += std::string(collectionName);
  return root;
}

std::string experimentalCollectionTypeForPublication(std::string_view collectionName,
                                                     std::string_view typeName,
                                                     bool leadingSlash) {
  const std::string folder = collection_paths::typeIdentityFolder(collectionName);
  return leadingSlash ? collection_paths::memberPath(folder, typeName)
                      : collection_paths::memberPathBare(folder, typeName);
}

bool matchesStdlibSurfaceRootForPublication(std::string_view typeName,
                                            const StdlibSurfaceMetadata *metadata) {
  if (metadata == nullptr) {
    return false;
  }
  const std::string normalizedType = normalizeBindingTypeName(std::string(typeName));
  auto matches = [&](std::string_view spelling) {
    if (spelling.empty()) {
      return false;
    }
    std::string rootedSpelling(spelling);
    if (rootedSpelling.front() != '/') {
      rootedSpelling.insert(rootedSpelling.begin(), '/');
    }
    std::string unrootedSpelling = rootedSpelling;
    unrootedSpelling.erase(unrootedSpelling.begin());
    return normalizedType == rootedSpelling || normalizedType == unrootedSpelling;
  };
  if (matches(metadata->canonicalPath)) {
    return true;
  }
  return std::any_of(metadata->importAliasSpellings.begin(),
                     metadata->importAliasSpellings.end(),
                     matches);
}

bool isUnspecializedExperimentalKeyValueBackingTypeForPublication(std::string typeName) {
  typeName = normalizeBindingTypeName(std::move(typeName));
  if (!typeName.empty() && typeName.front() == '/') {
    typeName.erase(typeName.begin());
  }
  const size_t leafStart = typeName.find_last_of('/');
  const std::string leaf =
      leafStart == std::string::npos ? typeName : typeName.substr(leafStart + 1);
  return leaf == "Map" &&
         isExperimentalCollectionBackingTypeName("map", "Map", typeName);
}

} // namespace publicationBuilders
} // namespace semantics
} // namespace primec
