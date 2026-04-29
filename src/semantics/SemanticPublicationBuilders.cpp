#include "SemanticPublicationBuilders.h"

#include "primec/StdlibSurfaceRegistry.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace primec {
namespace semantics {
namespace {

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

struct SemanticPublicationBuilderState {
  const Program &program;
  const std::string &entryPath;
  const SemanticProductBuildConfig *buildConfig = nullptr;
  SemanticProgram semanticProgram;
  std::unordered_map<std::string, std::size_t> moduleIndexByKey;

  SemanticPublicationBuilderState(const Program &programIn,
                                  const std::string &entryPathIn,
                                  const SemanticProductBuildConfig *buildConfigIn)
      : program(programIn), entryPath(entryPathIn), buildConfig(buildConfigIn) {}

  bool isCollectorEnabled(std::string_view collectorFamily) const {
    if (buildConfig == nullptr) {
      return true;
    }
    if (buildConfig->disableAllCollectors) {
      return false;
    }
    if (!buildConfig->collectorAllowlistSpecified) {
      return true;
    }
    return std::find(buildConfig->collectorAllowlist.begin(),
                     buildConfig->collectorAllowlist.end(),
                     collectorFamily) != buildConfig->collectorAllowlist.end();
  }

  SemanticProgramModuleResolvedArtifacts &ensureModuleResolvedArtifacts(std::string_view scopePath) {
    const std::string moduleKey = semanticSourceUnitModuleKeyForPath(
        scopePath, semanticProgram.sourceImports, semanticProgram.imports);
    const auto it = moduleIndexByKey.find(moduleKey);
    if (it != moduleIndexByKey.end()) {
      return semanticProgram.moduleResolvedArtifacts[it->second];
    }
    const std::size_t moduleIndex = semanticProgram.moduleResolvedArtifacts.size();
    moduleIndexByKey.emplace(moduleKey, moduleIndex);
    semanticProgram.moduleResolvedArtifacts.push_back(SemanticProgramModuleResolvedArtifacts{});
    auto &module = semanticProgram.moduleResolvedArtifacts.back();
    module.identity.moduleKey = moduleKey;
    module.identity.stableOrder = moduleIndex;
    return module;
  }
};

struct CollectionSpecializationDraft {
  std::string collectionFamily;
  std::string elementTypeText;
  std::string keyTypeText;
  std::string valueTypeText;
  bool isReference = false;
  bool isPointer = false;
};

std::string normalizeCollectionSpecializationTypeName(std::string typeName) {
  typeName = normalizeBindingTypeName(typeName);
  if (typeName == "/vector" || typeName == "std/collections/vector" ||
      typeName == "/std/collections/vector" || typeName == "Vector" ||
      typeName == "std/collections/experimental_vector/Vector" ||
      typeName == "/std/collections/experimental_vector/Vector") {
    return "vector";
  }
  if (typeName == "/map" || typeName == "std/collections/map" ||
      typeName == "/std/collections/map" || typeName == "Map" ||
      typeName == "/Map" || typeName == "std/collections/experimental_map/Map" ||
      typeName == "/std/collections/experimental_map/Map") {
    return "map";
  }
  if (typeName == "/soa_vector" || typeName == "std/collections/soa_vector" ||
      typeName == "/std/collections/soa_vector" || typeName == "SoaVector" ||
      typeName == "/SoaVector" ||
      typeName == "std/collections/experimental_soa_vector/SoaVector" ||
      typeName == "/std/collections/experimental_soa_vector/SoaVector") {
    return "soa_vector";
  }
  return typeName;
}

bool classifyCollectionSpecialization(std::string typeText,
                                      CollectionSpecializationDraft &draftOut) {
  draftOut = {};
  typeText = normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(typeText, base, argText)) {
      return false;
    }
    base = normalizeCollectionSpecializationTypeName(base);
    if (base == "Reference" || base == "Pointer") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      draftOut.isReference = draftOut.isReference || base == "Reference";
      draftOut.isPointer = draftOut.isPointer || base == "Pointer";
      typeText = normalizeBindingTypeName(args.front());
      continue;
    }
    if (base == "vector") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      draftOut.collectionFamily = "vector";
      draftOut.elementTypeText = normalizeBindingTypeName(args.front());
      draftOut.valueTypeText = draftOut.elementTypeText;
      return true;
    }
    if (base == "soa_vector") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      draftOut.collectionFamily = "soa_vector";
      draftOut.elementTypeText = normalizeBindingTypeName(args.front());
      draftOut.valueTypeText = draftOut.elementTypeText;
      return true;
    }
    if (base == "map") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 2) {
        return false;
      }
      draftOut.collectionFamily = "map";
      draftOut.keyTypeText = normalizeBindingTypeName(args.front());
      draftOut.valueTypeText = normalizeBindingTypeName(args.back());
      return true;
    }
    return false;
  }
}

void publishCollectionSpecializationForBinding(
    SemanticPublicationBuilderState &state,
    const SemanticProgramBindingFact &bindingEntry,
    std::string_view moduleScopePath) {
  CollectionSpecializationDraft draft;
  if (!classifyCollectionSpecialization(bindingEntry.bindingTypeText, draft)) {
    return;
  }

  SemanticProgramCollectionSpecialization entry;
  entry.scopePath = bindingEntry.scopePath;
  entry.siteKind = bindingEntry.siteKind;
  entry.name = bindingEntry.name;
  entry.collectionFamily = draft.collectionFamily;
  entry.bindingTypeText = bindingEntry.bindingTypeText;
  entry.elementTypeText = draft.elementTypeText;
  entry.keyTypeText = draft.keyTypeText;
  entry.valueTypeText = draft.valueTypeText;
  entry.isReference = draft.isReference;
  entry.isPointer = draft.isPointer;
  entry.sourceLine = bindingEntry.sourceLine;
  entry.sourceColumn = bindingEntry.sourceColumn;
  entry.semanticNodeId = bindingEntry.semanticNodeId;
  entry.provenanceHandle = bindingEntry.provenanceHandle;
  if (entry.collectionFamily == "vector") {
    entry.helperSurfaceId = StdlibSurfaceId::CollectionsVectorHelpers;
    entry.constructorSurfaceId = StdlibSurfaceId::CollectionsVectorConstructors;
  } else if (entry.collectionFamily == "soa_vector") {
    entry.helperSurfaceId = StdlibSurfaceId::CollectionsSoaVectorHelpers;
    entry.constructorSurfaceId = StdlibSurfaceId::CollectionsSoaVectorConstructors;
  } else if (entry.collectionFamily == "map") {
    entry.helperSurfaceId = StdlibSurfaceId::CollectionsMapHelpers;
    entry.constructorSurfaceId = StdlibSurfaceId::CollectionsMapConstructors;
  }

  entry.scopePathId = semanticProgramInternCallTargetString(state.semanticProgram, entry.scopePath);
  entry.siteKindId = semanticProgramInternCallTargetString(state.semanticProgram, entry.siteKind);
  entry.nameId = semanticProgramInternCallTargetString(state.semanticProgram, entry.name);
  entry.collectionFamilyId =
      semanticProgramInternCallTargetString(state.semanticProgram, entry.collectionFamily);
  entry.bindingTypeTextId =
      semanticProgramInternCallTargetString(state.semanticProgram, entry.bindingTypeText);
  entry.elementTypeTextId =
      semanticProgramInternCallTargetString(state.semanticProgram, entry.elementTypeText);
  entry.keyTypeTextId = semanticProgramInternCallTargetString(state.semanticProgram, entry.keyTypeText);
  entry.valueTypeTextId =
      semanticProgramInternCallTargetString(state.semanticProgram, entry.valueTypeText);

  state.semanticProgram.collectionSpecializations.push_back(std::move(entry));
  const std::size_t entryIndex = state.semanticProgram.collectionSpecializations.size() - 1;
  state.ensureModuleResolvedArtifacts(moduleScopePath).collectionSpecializationIndices.push_back(entryIndex);
  if (state.semanticProgram.collectionSpecializations.back().semanticNodeId != 0) {
    state.semanticProgram.publishedRoutingLookups.collectionSpecializationIndicesByExpr
        .insert_or_assign(state.semanticProgram.collectionSpecializations.back().semanticNodeId,
                          entryIndex);
  }
}

void initializeSemanticProgramPublicationShell(SemanticPublicationBuilderState &state) {
  state.semanticProgram.entryPath = state.entryPath;
  state.semanticProgram.sourceImports = state.program.sourceImports;
  state.semanticProgram.imports = state.program.imports;
  if (state.isCollectorEnabled("definitions")) {
    state.semanticProgram.definitions.reserve(state.program.definitions.size());
    for (const Definition &def : state.program.definitions) {
      SemanticProgramDefinition definition;
      definition.name = def.name;
      definition.fullPath = def.fullPath;
      definition.namespacePrefix = def.namespacePrefix;
      definition.sourceLine = def.sourceLine;
      definition.sourceColumn = def.sourceColumn;
      definition.semanticNodeId = def.semanticNodeId;
      definition.provenanceHandle = makeSemanticProvenanceHandle(def.semanticNodeId);
      state.semanticProgram.definitions.push_back(std::move(definition));
    }
  }
  if (state.isCollectorEnabled("executions")) {
    state.semanticProgram.executions.reserve(state.program.executions.size());
    for (const Execution &exec : state.program.executions) {
      SemanticProgramExecution execution;
      execution.name = exec.name;
      execution.fullPath = exec.fullPath;
      execution.namespacePrefix = exec.namespacePrefix;
      execution.sourceLine = exec.sourceLine;
      execution.sourceColumn = exec.sourceColumn;
      execution.semanticNodeId = exec.semanticNodeId;
      execution.provenanceHandle = makeSemanticProvenanceHandle(exec.semanticNodeId);
      state.semanticProgram.executions.push_back(std::move(execution));
    }
  }
  state.semanticProgram.moduleResolvedArtifacts.reserve(
      state.semanticProgram.definitions.size() + state.semanticProgram.executions.size());
  state.moduleIndexByKey.reserve(
      state.semanticProgram.definitions.size() + state.semanticProgram.executions.size());
}

void preseedSemanticProgramCallTargetStrings(SemanticPublicationBuilderState &state,
                                             const SemanticPublicationSurface &publicationSurface) {
  state.semanticProgram.callTargetStringTable.reserve(
      state.semanticProgram.callTargetStringTable.size() +
      publicationSurface.callTargetSeedStrings.size());
  state.semanticProgram.callTargetStringIdsByText.reserve(
      state.semanticProgram.callTargetStringIdsByText.size() +
      publicationSurface.callTargetSeedStrings.size());
  for (const std::string &seed : publicationSurface.callTargetSeedStrings) {
    (void)semanticProgramInternCallTargetString(state.semanticProgram, seed);
  }
}

void publishRoutingLookupIndexes(SemanticPublicationBuilderState &state) {
  auto &routingLookups = state.semanticProgram.publishedRoutingLookups;
  routingLookups.definitionIndicesByPathId.reserve(state.semanticProgram.definitions.size());
  for (std::size_t definitionIndex = 0;
       definitionIndex < state.semanticProgram.definitions.size();
       ++definitionIndex) {
    const auto &definition = state.semanticProgram.definitions[definitionIndex];
    const SymbolId fullPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, definition.fullPath);
    if (fullPathId != InvalidSymbolId) {
      routingLookups.definitionIndicesByPathId.insert_or_assign(fullPathId, definitionIndex);
    }
  }

  auto publishImportAlias = [&](std::string_view aliasName, std::string_view targetPath) {
    if (aliasName.empty() || targetPath.empty()) {
      return;
    }
    const SymbolId aliasNameId =
        semanticProgramInternCallTargetString(state.semanticProgram, aliasName);
    const SymbolId targetPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, targetPath);
    if (aliasNameId == InvalidSymbolId || targetPathId == InvalidSymbolId) {
      return;
    }
    routingLookups.importAliasTargetPathIdsByNameId.try_emplace(aliasNameId, targetPathId);
  };

  routingLookups.importAliasTargetPathIdsByNameId.reserve(state.semanticProgram.imports.size() +
                                                          state.semanticProgram.definitions.size());
  for (const auto &importPath : state.semanticProgram.imports) {
    if (importPath.empty() || importPath.front() != '/') {
      continue;
    }
    if (importPath.size() >= 2 &&
        importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
      const std::string prefix = importPath.substr(0, importPath.size() - 2);
      const std::string scopedPrefix = prefix + "/";
      for (const auto &definition : state.semanticProgram.definitions) {
        if (definition.fullPath.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        const std::string_view remainder =
            std::string_view(definition.fullPath).substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string_view::npos) {
          continue;
        }
        publishImportAlias(remainder, definition.fullPath);
      }
      continue;
    }

    const SymbolId importPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, importPath);
    if (!routingLookups.definitionIndicesByPathId.contains(importPathId)) {
      continue;
    }
    const std::size_t slashPos = importPath.find_last_of('/');
    if (slashPos == std::string::npos || slashPos + 1 >= importPath.size()) {
      continue;
    }
    publishImportAlias(std::string_view(importPath).substr(slashPos + 1), importPath);
  }
}

void publishLowererPreflightFacts(SemanticPublicationBuilderState &state) {
  auto &facts = state.semanticProgram.publishedLowererPreflightFacts;

  auto publishSoftwareNumericType = [&](const Expr &expr) {
    if (facts.firstSoftwareNumericTypeId != InvalidSymbolId) {
      return;
    }
    std::string found = scanExprForSoftwareNumeric(expr);
    if (found.empty()) {
      return;
    }
    facts.firstSoftwareNumericTypeId =
        semanticProgramInternCallTargetString(state.semanticProgram, found);
  };

  auto publishRuntimeReflectionPath = [&](const Expr &expr) {
    if (facts.firstRuntimeReflectionPathId != InvalidSymbolId) {
      return;
    }
    std::string found = scanExprForRuntimeReflectionQuery(expr);
    if (found.empty()) {
      return;
    }
    facts.firstRuntimeReflectionPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, found);
    facts.firstRuntimeReflectionPathIsObjectTable = isRuntimeReflectionPath(found);
  };

  for (const auto &def : state.program.definitions) {
    if (facts.firstSoftwareNumericTypeId == InvalidSymbolId) {
      if (std::string found = scanTransformsForSoftwareNumeric(def.transforms);
          !found.empty()) {
        facts.firstSoftwareNumericTypeId =
            semanticProgramInternCallTargetString(state.semanticProgram, found);
      }
    }
    for (const auto &param : def.parameters) {
      publishSoftwareNumericType(param);
      publishRuntimeReflectionPath(param);
    }
    for (const auto &stmt : def.statements) {
      publishSoftwareNumericType(stmt);
      publishRuntimeReflectionPath(stmt);
    }
    if (def.returnExpr.has_value()) {
      publishSoftwareNumericType(*def.returnExpr);
      publishRuntimeReflectionPath(*def.returnExpr);
    }
    if (facts.firstSoftwareNumericTypeId != InvalidSymbolId &&
        facts.firstRuntimeReflectionPathId != InvalidSymbolId) {
      return;
    }
  }

  for (const auto &exec : state.program.executions) {
    if (facts.firstSoftwareNumericTypeId == InvalidSymbolId) {
      if (std::string found = scanTransformsForSoftwareNumeric(exec.transforms);
          !found.empty()) {
        facts.firstSoftwareNumericTypeId =
            semanticProgramInternCallTargetString(state.semanticProgram, found);
      }
    }
    for (const auto &arg : exec.arguments) {
      publishSoftwareNumericType(arg);
      publishRuntimeReflectionPath(arg);
    }
    for (const auto &arg : exec.bodyArguments) {
      publishSoftwareNumericType(arg);
      publishRuntimeReflectionPath(arg);
    }
    if (facts.firstSoftwareNumericTypeId != InvalidSymbolId &&
        facts.firstRuntimeReflectionPathId != InvalidSymbolId) {
      return;
    }
  }
}

void publishDirectCallTargetFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<CollectedDirectCallTargetEntry> &directCallTargets) {
  if (directCallTargets.empty()) {
    return;
  }
  state.semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.reserve(
      directCallTargets.size());
  state.semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.reserve(
      directCallTargets.size());
  state.semanticProgram.directCallTargets.reserve(directCallTargets.size());
  for (const auto &snapshotEntry : directCallTargets) {
    SemanticProgramDirectCallTarget entry;
    entry.scopePath = snapshotEntry.scopePath;
    entry.callName = snapshotEntry.callName;
    entry.sourceLine = snapshotEntry.sourceLine;
    entry.sourceColumn = snapshotEntry.sourceColumn;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.scopePathId = semanticProgramInternCallTargetString(state.semanticProgram, entry.scopePath);
    entry.callNameId = semanticProgramInternCallTargetString(state.semanticProgram, entry.callName);
    entry.resolvedPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, snapshotEntry.resolvedPath);
    entry.stdlibSurfaceId = classifyPublishedStdlibSurfaceId(snapshotEntry.resolvedPath);
    state.semanticProgram.directCallTargets.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.directCallTargets.size() - 1;
    state.ensureModuleResolvedArtifacts(snapshotEntry.scopePath)
        .directCallTargetIndices.push_back(entryIndex);
    if (snapshotEntry.semanticNodeId != 0 &&
        state.semanticProgram.directCallTargets.back().resolvedPathId != InvalidSymbolId) {
      state.semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
          snapshotEntry.semanticNodeId,
          state.semanticProgram.directCallTargets.back().resolvedPathId);
    }
    if (snapshotEntry.semanticNodeId != 0 &&
        state.semanticProgram.directCallTargets.back().stdlibSurfaceId.has_value()) {
      state.semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.insert_or_assign(
          snapshotEntry.semanticNodeId,
          *state.semanticProgram.directCallTargets.back().stdlibSurfaceId);
    }
  }
}

void publishMethodCallTargetFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<CollectedMethodCallTargetEntry> &methodCallTargets) {
  if (methodCallTargets.empty()) {
    return;
  }
  state.semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.reserve(
      methodCallTargets.size());
  state.semanticProgram.publishedRoutingLookups.methodCallStdlibSurfaceIdsByExpr.reserve(
      methodCallTargets.size());
  state.semanticProgram.methodCallTargets.reserve(methodCallTargets.size());
  for (const auto &snapshotEntry : methodCallTargets) {
    SemanticProgramMethodCallTarget entry;
    entry.scopePath = snapshotEntry.scopePath;
    entry.methodName = snapshotEntry.methodName;
    entry.receiverTypeText = bindingTypeTextForSemanticProduct(snapshotEntry.receiverBinding);
    entry.sourceLine = snapshotEntry.sourceLine;
    entry.sourceColumn = snapshotEntry.sourceColumn;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.scopePathId = semanticProgramInternCallTargetString(state.semanticProgram, entry.scopePath);
    entry.methodNameId = semanticProgramInternCallTargetString(state.semanticProgram, entry.methodName);
    entry.receiverTypeTextId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.receiverTypeText);
    entry.resolvedPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, snapshotEntry.resolvedPath);
    entry.stdlibSurfaceId = classifyPublishedStdlibSurfaceId(snapshotEntry.resolvedPath);
    state.semanticProgram.methodCallTargets.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.methodCallTargets.size() - 1;
    state.ensureModuleResolvedArtifacts(snapshotEntry.scopePath)
        .methodCallTargetIndices.push_back(entryIndex);
    if (snapshotEntry.semanticNodeId != 0 &&
        state.semanticProgram.methodCallTargets.back().resolvedPathId != InvalidSymbolId) {
      state.semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
          snapshotEntry.semanticNodeId,
          state.semanticProgram.methodCallTargets.back().resolvedPathId);
    }
    if (snapshotEntry.semanticNodeId != 0 &&
        state.semanticProgram.methodCallTargets.back().stdlibSurfaceId.has_value()) {
      state.semanticProgram.publishedRoutingLookups.methodCallStdlibSurfaceIdsByExpr.insert_or_assign(
          snapshotEntry.semanticNodeId,
          *state.semanticProgram.methodCallTargets.back().stdlibSurfaceId);
    }
  }
}

void publishBridgePathChoiceFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<CollectedBridgePathChoiceEntry> &bridgePathChoices) {
  if (bridgePathChoices.empty()) {
    return;
  }
  state.semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.reserve(
      bridgePathChoices.size());
  state.semanticProgram.publishedRoutingLookups.bridgePathChoiceStdlibSurfaceIdsByExpr.reserve(
      bridgePathChoices.size());
  state.semanticProgram.bridgePathChoices.reserve(bridgePathChoices.size());
  for (const auto &snapshotEntry : bridgePathChoices) {
    SemanticProgramBridgePathChoice entry;
    entry.scopePath = snapshotEntry.scopePath;
    entry.collectionFamily = snapshotEntry.collectionFamily;
    entry.sourceLine = snapshotEntry.sourceLine;
    entry.sourceColumn = snapshotEntry.sourceColumn;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.scopePathId = semanticProgramInternCallTargetString(state.semanticProgram, entry.scopePath);
    entry.collectionFamilyId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.collectionFamily);
    entry.helperNameId =
        semanticProgramInternCallTargetString(state.semanticProgram, snapshotEntry.helperName);
    entry.chosenPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, snapshotEntry.chosenPath);
    entry.stdlibSurfaceId = classifyPublishedStdlibSurfaceId(snapshotEntry.chosenPath);
    state.semanticProgram.bridgePathChoices.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.bridgePathChoices.size() - 1;
    state.ensureModuleResolvedArtifacts(snapshotEntry.scopePath)
        .bridgePathChoiceIndices.push_back(entryIndex);
    if (snapshotEntry.semanticNodeId != 0 &&
        state.semanticProgram.bridgePathChoices.back().chosenPathId != InvalidSymbolId &&
        state.semanticProgram.bridgePathChoices.back().helperNameId != InvalidSymbolId) {
      state.semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.insert_or_assign(
          snapshotEntry.semanticNodeId,
          state.semanticProgram.bridgePathChoices.back().chosenPathId);
    }
    if (snapshotEntry.semanticNodeId != 0 &&
        state.semanticProgram.bridgePathChoices.back().stdlibSurfaceId.has_value()) {
      state.semanticProgram.publishedRoutingLookups.bridgePathChoiceStdlibSurfaceIdsByExpr
          .insert_or_assign(snapshotEntry.semanticNodeId,
                            *state.semanticProgram.bridgePathChoices.back().stdlibSurfaceId);
    }
  }
}

void publishCallableSummaryFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<CollectedCallableSummaryEntry> &callableSummaries) {
  if (callableSummaries.empty()) {
    return;
  }
  state.semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId.reserve(
      callableSummaries.size());
  state.semanticProgram.callableSummaries.reserve(callableSummaries.size());
  for (const auto &snapshotEntry : callableSummaries) {
    SemanticProgramCallableSummary entry;
    entry.isExecution = snapshotEntry.isExecution;
    entry.returnKind = returnKindSnapshotName(snapshotEntry.returnKind);
    entry.isCompute = snapshotEntry.isCompute;
    entry.isUnsafe = snapshotEntry.isUnsafe;
    entry.activeEffects = snapshotEntry.activeEffects;
    entry.activeCapabilities = snapshotEntry.activeCapabilities;
    entry.hasResultType = snapshotEntry.hasResultType;
    entry.resultTypeHasValue = snapshotEntry.resultTypeHasValue;
    entry.resultValueType = snapshotEntry.resultValueType;
    entry.resultErrorType = snapshotEntry.resultErrorType;
    entry.hasOnError = snapshotEntry.hasOnError;
    entry.onErrorHandlerPath = snapshotEntry.onErrorHandlerPath;
    entry.onErrorErrorType = snapshotEntry.onErrorErrorType;
    entry.onErrorBoundArgCount = snapshotEntry.onErrorBoundArgCount;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.fullPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, snapshotEntry.fullPath);
    entry.returnKindId = semanticProgramInternCallTargetString(state.semanticProgram, entry.returnKind);
    entry.activeEffectIds.reserve(entry.activeEffects.size());
    for (const auto &activeEffect : entry.activeEffects) {
      entry.activeEffectIds.push_back(
          semanticProgramInternCallTargetString(state.semanticProgram, activeEffect));
    }
    entry.activeCapabilityIds.reserve(entry.activeCapabilities.size());
    for (const auto &activeCapability : entry.activeCapabilities) {
      entry.activeCapabilityIds.push_back(
          semanticProgramInternCallTargetString(state.semanticProgram, activeCapability));
    }
    entry.resultValueTypeId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.resultValueType);
    entry.resultErrorTypeId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.resultErrorType);
    entry.onErrorHandlerPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.onErrorHandlerPath);
    entry.onErrorErrorTypeId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.onErrorErrorType);
    state.semanticProgram.callableSummaries.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.callableSummaries.size() - 1;
    state.ensureModuleResolvedArtifacts(snapshotEntry.fullPath)
        .callableSummaryIndices.push_back(entryIndex);
    if (state.semanticProgram.callableSummaries.back().fullPathId != InvalidSymbolId) {
      state.semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId.insert_or_assign(
          state.semanticProgram.callableSummaries.back().fullPathId,
          entryIndex);
    }
  }
}

void publishSemanticRoutingFamilies(
    SemanticPublicationBuilderState &state,
    SemanticPublicationSurface &publicationSurface) {
  publishDirectCallTargetFacts(state, publicationSurface.directCallTargets);
  publishMethodCallTargetFacts(state, publicationSurface.methodCallTargets);
  publishBridgePathChoiceFacts(state, publicationSurface.bridgePathChoices);
  publishCallableSummaryFacts(state, publicationSurface.callableSummaries);
}

void publishTypeMetadataFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<TypeMetadataSnapshotEntry> &typeMetadata) {
  if (typeMetadata.empty()) {
    return;
  }
  state.semanticProgram.typeMetadata.reserve(typeMetadata.size());
  for (const auto &entry : typeMetadata) {
    state.semanticProgram.typeMetadata.push_back(SemanticProgramTypeMetadata{
        entry.fullPath,
        entry.category,
        entry.isPublic,
        entry.hasNoPadding,
        entry.hasPlatformIndependentPadding,
        entry.hasExplicitAlignment,
        entry.explicitAlignmentBytes,
        entry.fieldCount,
        entry.enumValueCount,
        entry.sourceLine,
        entry.sourceColumn,
        entry.semanticNodeId,
        makeSemanticProvenanceHandle(entry.semanticNodeId),
    });
  }
}

void publishStructFieldMetadataFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<StructFieldMetadataSnapshotEntry> &structFieldMetadata) {
  if (structFieldMetadata.empty()) {
    return;
  }
  state.semanticProgram.structFieldMetadata.reserve(structFieldMetadata.size());
  for (const auto &entry : structFieldMetadata) {
    state.semanticProgram.structFieldMetadata.push_back(SemanticProgramStructFieldMetadata{
        entry.structPath,
        entry.fieldName,
        entry.fieldIndex,
        bindingTypeTextForSemanticProduct(entry.binding),
        entry.sourceLine,
        entry.sourceColumn,
        entry.semanticNodeId,
        makeSemanticProvenanceHandle(entry.semanticNodeId),
    });
  }
}

void publishSumTypeMetadataFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<SumTypeMetadataSnapshotEntry> &sumTypeMetadata) {
  if (sumTypeMetadata.empty()) {
    return;
  }
  state.semanticProgram.sumTypeMetadata.reserve(sumTypeMetadata.size());
  for (const auto &entry : sumTypeMetadata) {
    state.semanticProgram.sumTypeMetadata.push_back(SemanticProgramSumTypeMetadata{
        entry.fullPath,
        entry.isPublic,
        entry.activeTagTypeText,
        entry.payloadStorageText,
        entry.variantCount,
        entry.sourceLine,
        entry.sourceColumn,
        entry.semanticNodeId,
        makeSemanticProvenanceHandle(entry.semanticNodeId),
    });
  }
}

void publishSumVariantMetadataFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<SumVariantMetadataSnapshotEntry> &sumVariantMetadata) {
  if (sumVariantMetadata.empty()) {
    return;
  }
  state.semanticProgram.sumVariantMetadata.reserve(sumVariantMetadata.size());
  for (const auto &entry : sumVariantMetadata) {
    state.semanticProgram.sumVariantMetadata.push_back(SemanticProgramSumVariantMetadata{
        entry.sumPath,
        entry.variantName,
        entry.variantIndex,
        entry.tagValue,
        entry.hasPayload,
        entry.payloadTypeText,
        entry.sourceLine,
        entry.sourceColumn,
        entry.semanticNodeId,
        makeSemanticProvenanceHandle(entry.semanticNodeId),
    });
  }
}

void publishSemanticMetadataFamilies(
    SemanticPublicationBuilderState &state,
    SemanticPublicationSurface &publicationSurface) {
  publishTypeMetadataFacts(state, publicationSurface.typeMetadata);
  publishStructFieldMetadataFacts(state, publicationSurface.structFieldMetadata);
  publishSumTypeMetadataFacts(state, publicationSurface.sumTypeMetadata);
  publishSumVariantMetadataFacts(state, publicationSurface.sumVariantMetadata);
}

void publishBindingFacts(
    SemanticPublicationBuilderState &state,
    std::vector<BindingFactSnapshotEntry> bindingFacts) {
  if (bindingFacts.empty()) {
    return;
  }
  state.semanticProgram.bindingFacts.reserve(bindingFacts.size());
  state.semanticProgram.collectionSpecializations.reserve(
      state.semanticProgram.collectionSpecializations.size() + bindingFacts.size());
  state.semanticProgram.publishedRoutingLookups.collectionSpecializationIndicesByExpr.reserve(
      state.semanticProgram.publishedRoutingLookups.collectionSpecializationIndicesByExpr.size() +
      bindingFacts.size());
  for (auto &snapshotEntry : bindingFacts) {
    SemanticProgramBindingFact entry;
    entry.scopePath = std::move(snapshotEntry.scopePath);
    entry.siteKind = std::move(snapshotEntry.siteKind);
    entry.name = std::move(snapshotEntry.name);
    entry.bindingTypeText = bindingTypeTextForSemanticProduct(snapshotEntry.binding);
    entry.isMutable = snapshotEntry.binding.isMutable;
    entry.isEntryArgString = snapshotEntry.binding.isEntryArgString;
    entry.isUnsafeReference = snapshotEntry.binding.isUnsafeReference;
    entry.referenceRoot = std::move(snapshotEntry.binding.referenceRoot);
    entry.sourceLine = snapshotEntry.sourceLine;
    entry.sourceColumn = snapshotEntry.sourceColumn;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.scopePathId = semanticProgramInternCallTargetString(state.semanticProgram, entry.scopePath);
    entry.siteKindId = semanticProgramInternCallTargetString(state.semanticProgram, entry.siteKind);
    entry.nameId = semanticProgramInternCallTargetString(state.semanticProgram, entry.name);
    const std::string resolvedPath =
        snapshotEntry.resolvedPath.empty()
            ? fallbackBindingResolvedPathForSemanticProduct(entry.scopePath, entry.name)
            : std::move(snapshotEntry.resolvedPath);
    entry.resolvedPathId = semanticProgramInternCallTargetString(state.semanticProgram, resolvedPath);
    entry.bindingTypeTextId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.bindingTypeText);
    entry.referenceRootId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.referenceRoot);
    const std::string moduleScopePath =
        entry.scopePathId != InvalidSymbolId
            ? std::string(semanticProgramResolveCallTargetString(state.semanticProgram, entry.scopePathId))
            : entry.scopePath;
    auto &module = state.ensureModuleResolvedArtifacts(moduleScopePath);
    publishCollectionSpecializationForBinding(state, entry, moduleScopePath);
    releaseInternedField(entry.scopePath, entry.scopePathId);
    releaseInternedField(entry.siteKind, entry.siteKindId);
    releaseInternedField(entry.name, entry.nameId);
    releaseInternedField(entry.referenceRoot, entry.referenceRootId);
    state.semanticProgram.bindingFacts.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.bindingFacts.size() - 1;
    module.bindingFactIndices.push_back(entryIndex);
  }
}

void publishReturnFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<ReturnFactSnapshotEntry> &returnFacts) {
  if (returnFacts.empty()) {
    return;
  }
  state.semanticProgram.returnFacts.reserve(returnFacts.size());
  for (const auto &snapshotEntry : returnFacts) {
    SemanticProgramReturnFact entry;
    entry.returnKind = returnKindSnapshotName(snapshotEntry.kind);
    entry.structPath = snapshotEntry.structPath;
    entry.bindingTypeText = bindingTypeTextForSemanticProduct(snapshotEntry.binding);
    entry.isMutable = snapshotEntry.binding.isMutable;
    entry.isEntryArgString = snapshotEntry.binding.isEntryArgString;
    entry.isUnsafeReference = snapshotEntry.binding.isUnsafeReference;
    entry.referenceRoot = snapshotEntry.binding.referenceRoot;
    entry.sourceLine = snapshotEntry.sourceLine;
    entry.sourceColumn = snapshotEntry.sourceColumn;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.definitionPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, snapshotEntry.definitionPath);
    entry.returnKindId = semanticProgramInternCallTargetString(state.semanticProgram, entry.returnKind);
    entry.structPathId = semanticProgramInternCallTargetString(state.semanticProgram, entry.structPath);
    entry.bindingTypeTextId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.bindingTypeText);
    entry.referenceRootId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.referenceRoot);
    state.semanticProgram.returnFacts.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.returnFacts.size() - 1;
    state.ensureModuleResolvedArtifacts(snapshotEntry.definitionPath).returnFactIndices.push_back(
        entryIndex);
  }
}

void publishLocalAutoFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<LocalAutoBindingSnapshotEntry> &localAutoFacts) {
  if (localAutoFacts.empty()) {
    return;
  }
  state.semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.reserve(
      state.semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.size() +
      localAutoFacts.size());
  state.semanticProgram.publishedRoutingLookups.localAutoFactIndicesByInitPathAndBindingNameId.reserve(
      state.semanticProgram.publishedRoutingLookups.localAutoFactIndicesByInitPathAndBindingNameId.size() +
      localAutoFacts.size());
  state.semanticProgram.localAutoFacts.reserve(localAutoFacts.size());
  for (const auto &snapshotEntry : localAutoFacts) {
    SemanticProgramLocalAutoFact entry;
    entry.scopePath = snapshotEntry.scopePath;
    entry.bindingName = snapshotEntry.bindingName;
    entry.bindingTypeText = bindingTypeTextForSemanticProduct(snapshotEntry.binding);
    entry.initializerBindingTypeText =
        bindingTypeTextForSemanticProduct(snapshotEntry.initializerBinding);
    entry.initializerReceiverBindingTypeText =
        bindingTypeTextForSemanticProduct(snapshotEntry.initializerReceiverBinding);
    entry.initializerQueryTypeText = snapshotEntry.initializerQueryTypeText;
    entry.initializerResultHasValue = snapshotEntry.initializerResultHasValue;
    entry.initializerResultValueType = snapshotEntry.initializerResultValueType;
    entry.initializerResultErrorType = snapshotEntry.initializerResultErrorType;
    entry.initializerHasTry = snapshotEntry.initializerHasTry;
    entry.initializerTryOperandResolvedPath = snapshotEntry.initializerTryOperandResolvedPath;
    entry.initializerTryOperandBindingTypeText =
        bindingTypeTextForSemanticProduct(snapshotEntry.initializerTryOperandBinding);
    entry.initializerTryOperandReceiverBindingTypeText =
        bindingTypeTextForSemanticProduct(snapshotEntry.initializerTryOperandReceiverBinding);
    entry.initializerTryOperandQueryTypeText = snapshotEntry.initializerTryOperandQueryTypeText;
    entry.initializerTryValueType = snapshotEntry.initializerTryValueType;
    entry.initializerTryErrorType = snapshotEntry.initializerTryErrorType;
    entry.initializerTryContextReturnKind =
        returnKindSnapshotName(snapshotEntry.initializerTryContextReturnKind);
    entry.initializerTryOnErrorHandlerPath = snapshotEntry.initializerTryOnErrorHandlerPath;
    entry.initializerTryOnErrorErrorType = snapshotEntry.initializerTryOnErrorErrorType;
    entry.initializerTryOnErrorBoundArgCount = snapshotEntry.initializerTryOnErrorBoundArgCount;
    entry.sourceLine = snapshotEntry.sourceLine;
    entry.sourceColumn = snapshotEntry.sourceColumn;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.initializerDirectCallResolvedPath = snapshotEntry.initializerDirectCallResolvedPath;
    entry.initializerDirectCallReturnKind =
        snapshotEntry.initializerDirectCallReturnKind != ReturnKind::Unknown
            ? returnKindSnapshotName(snapshotEntry.initializerDirectCallReturnKind)
            : std::string{};
    entry.initializerMethodCallResolvedPath = snapshotEntry.initializerMethodCallResolvedPath;
    entry.initializerMethodCallReturnKind =
        snapshotEntry.initializerMethodCallReturnKind != ReturnKind::Unknown
            ? returnKindSnapshotName(snapshotEntry.initializerMethodCallReturnKind)
            : std::string{};
    entry.initializerStdlibSurfaceId =
        classifyPublishedStdlibSurfaceId(snapshotEntry.initializerResolvedPath);
    entry.initializerDirectCallStdlibSurfaceId =
        classifyPublishedStdlibSurfaceId(snapshotEntry.initializerDirectCallResolvedPath);
    entry.initializerMethodCallStdlibSurfaceId =
        classifyPublishedStdlibSurfaceId(snapshotEntry.initializerMethodCallResolvedPath);
    entry.scopePathId = semanticProgramInternCallTargetString(state.semanticProgram, entry.scopePath);
    entry.bindingNameId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.bindingName);
    entry.bindingTypeTextId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.bindingTypeText);
    entry.initializerResolvedPathId = semanticProgramInternCallTargetString(
        state.semanticProgram, snapshotEntry.initializerResolvedPath);
    entry.initializerBindingTypeTextId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerBindingTypeText);
    entry.initializerReceiverBindingTypeTextId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerReceiverBindingTypeText);
    entry.initializerQueryTypeTextId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerQueryTypeText);
    entry.initializerResultValueTypeId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerResultValueType);
    entry.initializerResultErrorTypeId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerResultErrorType);
    entry.initializerTryOperandResolvedPathId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerTryOperandResolvedPath);
    entry.initializerTryOperandBindingTypeTextId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerTryOperandBindingTypeText);
    entry.initializerTryOperandReceiverBindingTypeTextId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerTryOperandReceiverBindingTypeText);
    entry.initializerTryOperandQueryTypeTextId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerTryOperandQueryTypeText);
    entry.initializerTryValueTypeId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.initializerTryValueType);
    entry.initializerTryErrorTypeId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.initializerTryErrorType);
    entry.initializerTryContextReturnKindId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerTryContextReturnKind);
    entry.initializerTryOnErrorHandlerPathId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerTryOnErrorHandlerPath);
    entry.initializerTryOnErrorErrorTypeId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerTryOnErrorErrorType);
    entry.initializerDirectCallResolvedPathId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerDirectCallResolvedPath);
    entry.initializerDirectCallReturnKindId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerDirectCallReturnKind);
    entry.initializerMethodCallResolvedPathId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerMethodCallResolvedPath);
    entry.initializerMethodCallReturnKindId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.initializerMethodCallReturnKind);
    state.semanticProgram.localAutoFacts.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.localAutoFacts.size() - 1;
    const auto &publishedEntry = state.semanticProgram.localAutoFacts.back();
    if (publishedEntry.semanticNodeId != 0) {
      state.semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.insert_or_assign(
          publishedEntry.semanticNodeId, entryIndex);
    }
    if (publishedEntry.initializerResolvedPathId != InvalidSymbolId &&
        publishedEntry.bindingNameId != InvalidSymbolId) {
      state.semanticProgram.publishedRoutingLookups.localAutoFactIndicesByInitPathAndBindingNameId
          .insert_or_assign(makeLocalAutoInitPathBindingNameKey(publishedEntry.initializerResolvedPathId,
                                                                publishedEntry.bindingNameId),
                            entryIndex);
    }
    state.ensureModuleResolvedArtifacts(snapshotEntry.scopePath).localAutoFactIndices.push_back(
        entryIndex);
  }
}

void publishQueryFacts(SemanticPublicationBuilderState &state,
                       std::vector<QueryFactSnapshotEntry> queryFacts) {
  if (queryFacts.empty()) {
    return;
  }
  state.semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.reserve(
      state.semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.size() +
      queryFacts.size());
  state.semanticProgram.publishedRoutingLookups.queryFactIndicesByResolvedPathAndCallNameId.reserve(
      state.semanticProgram.publishedRoutingLookups.queryFactIndicesByResolvedPathAndCallNameId.size() +
      queryFacts.size());
  state.semanticProgram.queryFacts.reserve(queryFacts.size());
  for (auto &snapshotEntry : queryFacts) {
    SemanticProgramQueryFact entry;
    entry.scopePath = std::move(snapshotEntry.scopePath);
    entry.callName = std::move(snapshotEntry.callName);
    entry.queryTypeText = std::move(snapshotEntry.typeText);
    entry.bindingTypeText = bindingTypeTextForSemanticProduct(snapshotEntry.binding);
    entry.receiverBindingTypeText = bindingTypeTextForSemanticProduct(snapshotEntry.receiverBinding);
    entry.hasResultType = snapshotEntry.hasResultType;
    entry.resultTypeHasValue = snapshotEntry.resultTypeHasValue;
    entry.resultValueType = std::move(snapshotEntry.resultValueType);
    entry.resultErrorType = std::move(snapshotEntry.resultErrorType);
    entry.sourceLine = snapshotEntry.sourceLine;
    entry.sourceColumn = snapshotEntry.sourceColumn;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.scopePathId = semanticProgramInternCallTargetString(state.semanticProgram, entry.scopePath);
    entry.callNameId = semanticProgramInternCallTargetString(state.semanticProgram, entry.callName);
    entry.resolvedPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, snapshotEntry.resolvedPath);
    entry.queryTypeTextId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.queryTypeText);
    entry.bindingTypeTextId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.bindingTypeText);
    entry.receiverBindingTypeTextId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.receiverBindingTypeText);
    entry.resultValueTypeId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.resultValueType);
    entry.resultErrorTypeId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.resultErrorType);
    const std::string_view moduleScopePath =
        entry.scopePathId != InvalidSymbolId
            ? semanticProgramResolveCallTargetString(state.semanticProgram, entry.scopePathId)
            : std::string_view(entry.scopePath);
    auto &module = state.ensureModuleResolvedArtifacts(moduleScopePath);
    releaseInternedField(entry.scopePath, entry.scopePathId);
    releaseInternedField(entry.callName, entry.callNameId);
    state.semanticProgram.queryFacts.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.queryFacts.size() - 1;
    const auto &publishedEntry = state.semanticProgram.queryFacts.back();
    if (publishedEntry.semanticNodeId != 0) {
      state.semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(
          publishedEntry.semanticNodeId, entryIndex);
    }
    if (publishedEntry.resolvedPathId != InvalidSymbolId &&
        publishedEntry.callNameId != InvalidSymbolId) {
      state.semanticProgram.publishedRoutingLookups.queryFactIndicesByResolvedPathAndCallNameId
          .insert_or_assign(makeQueryFactResolvedPathCallNameKey(publishedEntry.resolvedPathId,
                                                                 publishedEntry.callNameId),
                            entryIndex);
    }
    module.queryFactIndices.push_back(entryIndex);
  }
}

void publishTryFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<TryValueSnapshotEntry> &tryFacts) {
  if (tryFacts.empty()) {
    return;
  }
  state.semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.reserve(
      state.semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.size() + tryFacts.size());
  state.semanticProgram.publishedRoutingLookups.tryFactIndicesByOperandPathAndSource.reserve(
      state.semanticProgram.publishedRoutingLookups.tryFactIndicesByOperandPathAndSource.size() +
      tryFacts.size());
  state.semanticProgram.tryFacts.reserve(tryFacts.size());
  for (const auto &snapshotEntry : tryFacts) {
    SemanticProgramTryFact entry;
    entry.scopePath = snapshotEntry.scopePath;
    entry.operandBindingTypeText =
        bindingTypeTextForSemanticProduct(snapshotEntry.operandBinding);
    entry.operandReceiverBindingTypeText =
        bindingTypeTextForSemanticProduct(snapshotEntry.operandReceiverBinding);
    entry.operandQueryTypeText = snapshotEntry.operandQueryTypeText;
    entry.valueType = snapshotEntry.valueType;
    entry.errorType = snapshotEntry.errorType;
    entry.contextReturnKind = returnKindSnapshotName(snapshotEntry.contextReturnKind);
    entry.onErrorHandlerPath = snapshotEntry.onErrorHandlerPath;
    entry.onErrorErrorType = snapshotEntry.onErrorErrorType;
    entry.onErrorBoundArgCount = snapshotEntry.onErrorBoundArgCount;
    entry.sourceLine = snapshotEntry.sourceLine;
    entry.sourceColumn = snapshotEntry.sourceColumn;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.scopePathId = semanticProgramInternCallTargetString(state.semanticProgram, entry.scopePath);
    entry.operandResolvedPathId = semanticProgramInternCallTargetString(
        state.semanticProgram, snapshotEntry.operandResolvedPath);
    entry.operandBindingTypeTextId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.operandBindingTypeText);
    entry.operandReceiverBindingTypeTextId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.operandReceiverBindingTypeText);
    entry.operandQueryTypeTextId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.operandQueryTypeText);
    entry.valueTypeId = semanticProgramInternCallTargetString(state.semanticProgram, entry.valueType);
    entry.errorTypeId = semanticProgramInternCallTargetString(state.semanticProgram, entry.errorType);
    entry.contextReturnKindId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.contextReturnKind);
    entry.onErrorHandlerPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.onErrorHandlerPath);
    entry.onErrorErrorTypeId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.onErrorErrorType);
    state.semanticProgram.tryFacts.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.tryFacts.size() - 1;
    const auto &publishedEntry = state.semanticProgram.tryFacts.back();
    if (publishedEntry.semanticNodeId != 0) {
      state.semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.insert_or_assign(
          publishedEntry.semanticNodeId, entryIndex);
    }
    if (publishedEntry.operandResolvedPathId != InvalidSymbolId &&
        publishedEntry.sourceLine > 0 &&
        publishedEntry.sourceColumn > 0) {
      state.semanticProgram.publishedRoutingLookups.tryFactIndicesByOperandPathAndSource
          .insert_or_assign(makeTryFactOperandPathSourceKey(publishedEntry.operandResolvedPathId,
                                                            publishedEntry.sourceLine,
                                                            publishedEntry.sourceColumn),
                            entryIndex);
    }
    state.ensureModuleResolvedArtifacts(snapshotEntry.scopePath).tryFactIndices.push_back(entryIndex);
  }
}

void publishOnErrorFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<OnErrorSnapshotEntry> &onErrorFacts) {
  if (onErrorFacts.empty()) {
    return;
  }
  state.semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionId.reserve(
      state.semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionId.size() +
      onErrorFacts.size());
  state.semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionPathId.reserve(
      state.semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionPathId.size() +
      onErrorFacts.size());
  state.semanticProgram.onErrorFacts.reserve(onErrorFacts.size());
  for (const auto &snapshotEntry : onErrorFacts) {
    SemanticProgramOnErrorFact entry;
    entry.definitionPath = snapshotEntry.definitionPath;
    entry.returnKind = returnKindSnapshotName(snapshotEntry.returnKind);
    entry.errorType = snapshotEntry.errorType;
    entry.boundArgCount = snapshotEntry.boundArgCount;
    entry.boundArgTexts = snapshotEntry.boundArgTexts;
    entry.returnResultHasValue = snapshotEntry.returnResultHasValue;
    entry.returnResultValueType = snapshotEntry.returnResultValueType;
    entry.returnResultErrorType = snapshotEntry.returnResultErrorType;
    entry.semanticNodeId = snapshotEntry.semanticNodeId;
    entry.provenanceHandle = makeSemanticProvenanceHandle(snapshotEntry.semanticNodeId);
    entry.definitionPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, entry.definitionPath);
    entry.returnKindId = semanticProgramInternCallTargetString(state.semanticProgram, entry.returnKind);
    entry.handlerPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, snapshotEntry.handlerPath);
    entry.errorTypeId = semanticProgramInternCallTargetString(state.semanticProgram, entry.errorType);
    entry.boundArgTextIds.reserve(entry.boundArgTexts.size());
    for (const auto &boundArgText : entry.boundArgTexts) {
      entry.boundArgTextIds.push_back(
          semanticProgramInternCallTargetString(state.semanticProgram, boundArgText));
    }
    entry.returnResultValueTypeId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.returnResultValueType);
    entry.returnResultErrorTypeId = semanticProgramInternCallTargetString(
        state.semanticProgram, entry.returnResultErrorType);
    state.semanticProgram.onErrorFacts.push_back(std::move(entry));
    const std::size_t entryIndex = state.semanticProgram.onErrorFacts.size() - 1;
    const auto &publishedEntry = state.semanticProgram.onErrorFacts.back();
    if (publishedEntry.semanticNodeId != 0) {
      state.semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionId.insert_or_assign(
          publishedEntry.semanticNodeId, entryIndex);
    }
    if (publishedEntry.definitionPathId != InvalidSymbolId) {
      state.semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionPathId
          .insert_or_assign(publishedEntry.definitionPathId, entryIndex);
    }
    state.ensureModuleResolvedArtifacts(snapshotEntry.definitionPath).onErrorFactIndices.push_back(
        entryIndex);
  }
}

void publishSemanticScopedFactFamilies(
    SemanticPublicationBuilderState &state,
    SemanticPublicationSurface &publicationSurface) {
  publishBindingFacts(state, std::move(publicationSurface.bindingFacts));
  publishReturnFacts(state, publicationSurface.returnFacts);
  publishLocalAutoFacts(state, publicationSurface.localAutoFacts);
  publishQueryFacts(state, std::move(publicationSurface.queryFacts));
  publishTryFacts(state, publicationSurface.tryFacts);
  publishOnErrorFacts(state, publicationSurface.onErrorFacts);
}

void finalizeSemanticModuleArtifacts(SemanticPublicationBuilderState &state) {
  std::sort(state.semanticProgram.moduleResolvedArtifacts.begin(),
            state.semanticProgram.moduleResolvedArtifacts.end(),
            [&state](const SemanticProgramModuleResolvedArtifacts &left,
                     const SemanticProgramModuleResolvedArtifacts &right) {
              const std::size_t leftImportOrder =
                  semanticSourceUnitImportOrderKeyForModuleKey(
                      left.identity.moduleKey,
                      state.semanticProgram.sourceImports,
                      state.semanticProgram.imports);
              const std::size_t rightImportOrder =
                  semanticSourceUnitImportOrderKeyForModuleKey(
                      right.identity.moduleKey,
                      state.semanticProgram.sourceImports,
                      state.semanticProgram.imports);
              if (leftImportOrder != rightImportOrder) {
                return leftImportOrder < rightImportOrder;
              }
              return left.identity.stableOrder < right.identity.stableOrder;
            });
  for (std::size_t moduleIndex = 0;
       moduleIndex < state.semanticProgram.moduleResolvedArtifacts.size();
       ++moduleIndex) {
    state.semanticProgram.moduleResolvedArtifacts[moduleIndex].identity.stableOrder = moduleIndex;
  }
}

} // namespace

SemanticProgram buildSemanticProgramFromPublicationSurface(
    const Program &program,
    const std::string &entryPath,
    SemanticPublicationSurface publicationSurface,
    const SemanticProductBuildConfig *buildConfig) {
  SemanticPublicationBuilderState state(program, entryPath, buildConfig);
  initializeSemanticProgramPublicationShell(state);
  preseedSemanticProgramCallTargetStrings(state, publicationSurface);
  publishRoutingLookupIndexes(state);
  publishLowererPreflightFacts(state);
  publishSemanticRoutingFamilies(state, publicationSurface);
  publishSemanticMetadataFamilies(state, publicationSurface);
  publishSemanticScopedFactFamilies(state, publicationSurface);
  finalizeSemanticModuleArtifacts(state);
  freezeSemanticProgramPublishedStorage(state.semanticProgram);
  return std::move(state.semanticProgram);
}

} // namespace semantics
} // namespace primec
