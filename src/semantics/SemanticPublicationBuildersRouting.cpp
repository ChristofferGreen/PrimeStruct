// soa-surface-audit: exempt
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


std::string normalizeCollectionSpecializationTypeName(std::string typeName) {
  typeName = normalizeBindingTypeName(typeName);
  if (typeName == collection_helpers::kRootedVector ||
      typeName == collectionTypeRootForPublication("vector") ||
      typeName == collectionTypeRootForPublication("vector", true) ||
      typeName == "Vector" ||
      typeName == experimentalCollectionTypeForPublication("vector", "Vector") ||
      typeName == experimentalCollectionTypeForPublication("vector", "Vector", true)) {
    return "vector";
  }
  if (matchesStdlibSurfaceRootForPublication(typeName,
                                             keyValueHelperSurfaceMetadataLocal()) ||
      isUnspecializedExperimentalKeyValueBackingTypeForPublication(typeName)) {
    return "map";
  }
  if (typeName == collection_helpers::kRootedSoa ||
      typeName == collection_paths::moduleRootBare(collection_paths::kLegacySoaVectorFolder) ||
      typeName == collection_paths::moduleRoot(collection_paths::kLegacySoaVectorFolder) ||
      typeName == collection_paths::kSoaVectorTypeName || typeName == "/SoaVector" ||
      typeName == collection_paths::memberPathBare(collection_paths::kSoaFolder,
                                                   collection_paths::kSoaVectorTypeName) ||
      typeName == collection_paths::memberPath(collection_paths::kSoaFolder,
                                               collection_paths::kSoaVectorTypeName)) {
    return std::string(collection_paths::kLegacySoaVectorFolder);
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
    if (base == "soa") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      draftOut.collectionFamily = "soa";
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

bool classifyArrayExtentBinding(std::string typeText, ArrayExtentDraft &draftOut) {
  draftOut = {};
  typeText = normalizeBindingTypeName(typeText);
  auto normalizeArrayExtentBase = [](std::string base) {
    base = normalizeBindingTypeName(std::move(base));
    return base == "args" ? std::string("array") : base;
  };
  while (true) {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(typeText, base, argText)) {
      return false;
    }
    base = normalizeArrayExtentBase(std::move(base));
    if (base == "Reference") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      draftOut.isReference = true;
      typeText = normalizeBindingTypeName(args.front());
      continue;
    }
    if (base != "array") {
      return false;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
      return false;
    }
    draftOut.elementTypeText = normalizeBindingTypeName(args.front());
    return true;
  }
}

std::string mangleSemanticTemplateArgsSuffix(const std::vector<std::string> &args) {
  auto stripWhitespace = [](const std::string &text) {
    std::string result;
    result.reserve(text.size());
    for (unsigned char ch : text) {
      if (!std::isspace(ch)) {
        result.push_back(static_cast<char>(ch));
      }
    }
    return result;
  };

  std::ostringstream canonicalArgs;
  for (size_t index = 0; index < args.size(); ++index) {
    if (index != 0) {
      canonicalArgs << ",";
    }
    canonicalArgs << "type:" << stripWhitespace(args[index]);
  }

  uint64_t hash = 1469598103934665603ULL;
  const std::string canonicalText = canonicalArgs.str();
  for (unsigned char ch : canonicalText) {
    hash ^= static_cast<uint64_t>(ch);
    hash *= 1099511628211ULL;
  }

  std::ostringstream suffix;
  suffix << "__t" << std::hex << hash;
  return suffix.str();
}

std::string collectionSpecializationStructPath(const CollectionSpecializationDraft &draft) {
  if (draft.collectionFamily != "map" || draft.keyTypeText.empty() ||
      draft.valueTypeText.empty()) {
    return {};
  }
  const auto *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr || metadata->canonicalPath.empty()) {
    return {};
  }
  return stdlibSurfaceBackingTypePath(*metadata) +
         mangleSemanticTemplateArgsSuffix({draft.keyTypeText, draft.valueTypeText});
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
  entry.structPath = collectionSpecializationStructPath(draft);
  entry.isReference = draft.isReference;
  entry.isPointer = draft.isPointer;
  entry.sourceLine = bindingEntry.sourceLine;
  entry.sourceColumn = bindingEntry.sourceColumn;
  entry.semanticNodeId = bindingEntry.semanticNodeId;
  entry.provenanceHandle = bindingEntry.provenanceHandle;
  if (entry.collectionFamily == "vector") {
    if (const auto *helperMetadata = vectorHelperSurfaceMetadataLocal();
        helperMetadata != nullptr) {
      entry.helperSurfaceId = helperMetadata->id;
    }
    if (const auto *constructorMetadata = vectorConstructorSurfaceMetadataLocal();
        constructorMetadata != nullptr) {
      entry.constructorSurfaceId = constructorMetadata->id;
    }
  } else if (entry.collectionFamily == "soa") {
    entry.helperSurfaceId = StdlibSurfaceId::CollectionsColumnarHelpers;
    entry.constructorSurfaceId = StdlibSurfaceId::CollectionsColumnarConstructors;
  } else if (entry.collectionFamily == "map") {
    if (const auto *helperMetadata = keyValueHelperSurfaceMetadataLocal();
        helperMetadata != nullptr) {
      entry.helperSurfaceId = helperMetadata->id;
    }
    if (const auto *constructorMetadata = keyValueConstructorSurfaceMetadataLocal();
        constructorMetadata != nullptr) {
      entry.constructorSurfaceId = constructorMetadata->id;
    }
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
  entry.structPathId = semanticProgramInternCallTargetString(state.semanticProgram, entry.structPath);

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
      definition.templateParameters = def.templateArgs;
      definition.templateParameterIsPack.reserve(def.templateArgs.size());
      for (std::size_t index = 0; index < def.templateArgs.size(); ++index) {
        definition.templateParameterIsPack.push_back(index < def.templateArgIsPack.size() &&
                                                     def.templateArgIsPack[index]);
      }
      definition.templatePackBindings = def.templatePackBindings;
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
    facts.hasSoftwareNumericType = true;
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
    facts.hasRuntimeReflectionPath = true;
    facts.firstRuntimeReflectionPathId =
        semanticProgramInternCallTargetString(state.semanticProgram, found);
    facts.firstRuntimeReflectionPathIsObjectTable = isRuntimeReflectionPath(found);
  };

  for (const auto &def : state.program.definitions) {
    if (facts.firstSoftwareNumericTypeId == InvalidSymbolId) {
      if (std::string found = scanTransformsForSoftwareNumeric(def.transforms);
          !found.empty()) {
        facts.hasSoftwareNumericType = true;
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
        facts.hasSoftwareNumericType = true;
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

} // namespace publicationBuilders
} // namespace semantics
} // namespace primec
