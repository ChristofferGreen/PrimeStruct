#include "primec/frontend/SemanticProduct.h"

#include "primec/support/CompileArena.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <string_view>
#include "SemanticProductInternal.h"

namespace primec {
using namespace semantic_product_detail;

void freezeSemanticProgramPublishedStorage(SemanticProgram &semanticProgram) {
  if (semanticProgram.publishedStorageFrozen) {
    return;
  }
  semanticProgram.callTargetStringIdsByText.clear();
  semanticProgram.callTargetStringIdsByText.rehash(0);
  semanticProgram.publishedStorageFrozen = true;
}

bool semanticProgramPublishedStorageFrozen(const SemanticProgram &semanticProgram) {
  return semanticProgram.publishedStorageFrozen;
}

const std::vector<SemanticProgramFactFamilyInfo> &semanticProgramFactFamilyInfos() {
  // TODO-5235: built via systemHeapValue() so this magic static's backing
  // memory is never arena-allocated - see docs/CompilerArenaAllocator.md.
  static const std::vector<SemanticProgramFactFamilyInfo> Families =
      primec::systemHeapValue([]() -> std::vector<SemanticProgramFactFamilyInfo> {
      return {
      {"sourceImports",
       SemanticProgramFactOwnership::AstProvenance,
       "syntax-owned import spelling retained for provenance"},
      {"imports",
       SemanticProgramFactOwnership::AstProvenance,
       "syntax-owned resolved import inventory retained for provenance"},
      {"definitions",
       SemanticProgramFactOwnership::AstProvenance,
       "AST-owned callable body and source provenance inventory"},
      {"executions",
       SemanticProgramFactOwnership::AstProvenance,
       "AST-owned execution body and source provenance inventory"},
      {"callTargetStringTable",
       SemanticProgramFactOwnership::DerivedIndex,
       "interned strings backing published semantic-product ids"},
      {"moduleResolvedArtifacts",
       SemanticProgramFactOwnership::DerivedIndex,
       "deterministic per-module indexes over published fact families"},
      {"publishedRoutingLookups",
       SemanticProgramFactOwnership::DerivedIndex,
       "lowerer lookup indexes derived from semantic-product facts"},
      {"publishedLowererPreflightFacts",
       SemanticProgramFactOwnership::SemanticProduct,
       "lowering-facing preflight facts produced by semantics"},
      {"directCallTargets",
       SemanticProgramFactOwnership::SemanticProduct,
       "resolved direct-call targets"},
      {"methodCallTargets",
       SemanticProgramFactOwnership::SemanticProduct,
       "resolved method-call targets"},
      {"bridgePathChoices",
       SemanticProgramFactOwnership::SemanticProduct,
       "resolved collection/helper bridge choices"},
      {"callableSummaries",
       SemanticProgramFactOwnership::SemanticProduct,
       "callable return, effect, capability, and result summaries"},
      {"typeMetadata",
       SemanticProgramFactOwnership::SemanticProduct,
       "published type metadata"},
      {"structFieldMetadata",
       SemanticProgramFactOwnership::SemanticProduct,
       "published struct-field metadata"},
      {"sumTypeMetadata",
       SemanticProgramFactOwnership::SemanticProduct,
       "published sum-type layout metadata"},
      {"sumVariantMetadata",
       SemanticProgramFactOwnership::SemanticProduct,
       "published sum variant metadata"},
      {"collectionSpecializations",
       SemanticProgramFactOwnership::SemanticProduct,
       "published collection specialization facts"},
      {"arrayExtentFacts",
       SemanticProgramFactOwnership::SemanticProduct,
       "published array extent facts"},
      {"bindingFacts",
       SemanticProgramFactOwnership::SemanticProduct,
       "published binding facts"},
      {"returnFacts",
       SemanticProgramFactOwnership::SemanticProduct,
       "published return facts"},
      {"localAutoFacts",
       SemanticProgramFactOwnership::SemanticProduct,
       "published local-auto inference facts"},
      {"queryFacts",
       SemanticProgramFactOwnership::SemanticProduct,
       "published query call facts"},
      {"tryFacts",
       SemanticProgramFactOwnership::SemanticProduct,
       "published try expression facts"},
      {"requirementPredicateFacts",
       SemanticProgramFactOwnership::SemanticProduct,
       "published requirement predicate facts"},
      {"onErrorFacts",
       SemanticProgramFactOwnership::SemanticProduct,
       "published on_error facts"},
      };
      });
  return Families;
}

std::optional<SemanticProgramFactOwnership>
semanticProgramFactFamilyOwnership(std::string_view familyName) {
  for (const SemanticProgramFactFamilyInfo &family : semanticProgramFactFamilyInfos()) {
    if (family.name == familyName) {
      return family.ownership;
    }
  }
  return std::nullopt;
}

bool semanticProgramFactFamilyIsSemanticProductOwned(std::string_view familyName) {
  return semanticProgramFactFamilyOwnership(familyName) ==
         SemanticProgramFactOwnership::SemanticProduct;
}

bool semanticProgramFactFamilyIsAstProvenanceOwned(std::string_view familyName) {
  return semanticProgramFactFamilyOwnership(familyName) ==
         SemanticProgramFactOwnership::AstProvenance;
}

SymbolId semanticProgramInternCallTargetString(SemanticProgram &semanticProgram, std::string_view text) {
  if (text.empty()) {
    return InvalidSymbolId;
  }
  if (semanticProgram.publishedStorageFrozen) {
    return InvalidSymbolId;
  }
  if (const auto existing = semanticProgram.callTargetStringIdsByText.find(text);
      existing != semanticProgram.callTargetStringIdsByText.end()) {
    return existing->second;
  }
  if (semanticProgram.callTargetStringTable.size() >=
      static_cast<std::size_t>(std::numeric_limits<SymbolId>::max())) {
    return InvalidSymbolId;
  }
  semanticProgram.callTargetStringTable.emplace_back(text);
  const SymbolId id = static_cast<SymbolId>(semanticProgram.callTargetStringTable.size());
  semanticProgram.callTargetStringIdsByText.emplace(semanticProgram.callTargetStringTable.back(), id);
  return id;
}

std::optional<SymbolId> semanticProgramLookupCallTargetStringId(const SemanticProgram &semanticProgram,
                                                                std::string_view text) {
  if (text.empty()) {
    return std::nullopt;
  }
  if (!semanticProgram.callTargetStringIdsByText.empty()) {
    if (const auto existing = semanticProgram.callTargetStringIdsByText.find(text);
        existing != semanticProgram.callTargetStringIdsByText.end()) {
      return existing->second;
    }
    return std::nullopt;
  }
  for (size_t i = 0; i < semanticProgram.callTargetStringTable.size(); ++i) {
    if (semanticProgram.callTargetStringTable[i] == text) {
      return static_cast<SymbolId>(i + 1u);
    }
  }
  return std::nullopt;
}

void releaseSemanticProgramLookupMap(SemanticProgram &semanticProgram) {
  freezeSemanticProgramPublishedStorage(semanticProgram);
}

std::string_view semanticProgramResolveCallTargetString(const SemanticProgram &semanticProgram, SymbolId id) {
  if (id == InvalidSymbolId || id > semanticProgram.callTargetStringTable.size()) {
    return {};
  }
  return semanticProgram.callTargetStringTable[id - 1];
}

const SemanticProgramDefinition *semanticProgramLookupPublishedDefinitionByPathId(
    const SemanticProgram &semanticProgram,
    SymbolId fullPathId) {
  if (fullPathId == InvalidSymbolId) {
    return nullptr;
  }
  if (const auto it = semanticProgram.publishedRoutingLookups.definitionIndicesByPathId.find(fullPathId);
      it != semanticProgram.publishedRoutingLookups.definitionIndicesByPathId.end()) {
    if (it->second < semanticProgram.definitions.size()) {
      return &semanticProgram.definitions[it->second];
    }
    return nullptr;
  }
  const std::string_view resolvedPath = semanticProgramResolveCallTargetString(semanticProgram, fullPathId);
  if (resolvedPath.empty()) {
    return nullptr;
  }
  for (const auto &entry : semanticProgram.definitions) {
    if (entry.fullPath == resolvedPath) {
      return &entry;
    }
  }
  return nullptr;
}

const SemanticProgramDefinition *semanticProgramLookupPublishedDefinition(
    const SemanticProgram &semanticProgram,
    std::string_view fullPath) {
  const auto fullPathId = semanticProgramLookupCallTargetStringId(semanticProgram, fullPath);
  if (!fullPathId.has_value()) {
    return nullptr;
  }
  return semanticProgramLookupPublishedDefinitionByPathId(semanticProgram, *fullPathId);
}

std::optional<SymbolId> semanticProgramLookupPublishedImportAliasTargetPathId(
    const SemanticProgram &semanticProgram,
    SymbolId aliasNameId) {
  if (aliasNameId == InvalidSymbolId) {
    return std::nullopt;
  }
  if (const auto it = semanticProgram.publishedRoutingLookups.importAliasTargetPathIdsByNameId.find(aliasNameId);
      it != semanticProgram.publishedRoutingLookups.importAliasTargetPathIdsByNameId.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<SymbolId> semanticProgramLookupPublishedImportAliasTargetPathId(
    const SemanticProgram &semanticProgram,
    std::string_view aliasName) {
  const auto aliasNameId = semanticProgramLookupCallTargetStringId(semanticProgram, aliasName);
  if (!aliasNameId.has_value()) {
    return std::nullopt;
  }
  return semanticProgramLookupPublishedImportAliasTargetPathId(semanticProgram, *aliasNameId);
}

std::optional<SymbolId> semanticProgramLookupPublishedDirectCallTargetId(const SemanticProgram &semanticProgram,
                                                                         uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return std::nullopt;
  }
  if (const auto it = semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.find(semanticNodeId);
      it != semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<SymbolId> semanticProgramLookupPublishedMethodCallTargetId(const SemanticProgram &semanticProgram,
                                                                         uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return std::nullopt;
  }
  if (const auto it = semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.find(semanticNodeId);
      it != semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<SymbolId> semanticProgramLookupPublishedBridgePathChoiceId(const SemanticProgram &semanticProgram,
                                                                         uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return std::nullopt;
  }
  if (const auto it = semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.find(semanticNodeId);
      it != semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<StdlibSurfaceId> semanticProgramDirectCallTargetStdlibSurfaceId(
    const SemanticProgramDirectCallTarget &entry) {
  return entry.stdlibSurfaceId;
}

std::optional<StdlibSurfaceId> semanticProgramMethodCallTargetStdlibSurfaceId(
    const SemanticProgramMethodCallTarget &entry) {
  return entry.stdlibSurfaceId;
}

std::optional<StdlibSurfaceId> semanticProgramBridgePathChoiceStdlibSurfaceId(
    const SemanticProgramBridgePathChoice &entry) {
  return entry.stdlibSurfaceId;
}

std::optional<StdlibSurfaceId> semanticProgramLookupPublishedDirectCallTargetStdlibSurfaceId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return std::nullopt;
  }
  if (const auto it =
          semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.find(semanticNodeId);
      it != semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<StdlibSurfaceId> semanticProgramLookupPublishedMethodCallTargetStdlibSurfaceId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return std::nullopt;
  }
  if (const auto it =
          semanticProgram.publishedRoutingLookups.methodCallStdlibSurfaceIdsByExpr.find(semanticNodeId);
      it != semanticProgram.publishedRoutingLookups.methodCallStdlibSurfaceIdsByExpr.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<StdlibSurfaceId> semanticProgramLookupPublishedBridgePathChoiceStdlibSurfaceId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return std::nullopt;
  }
  if (const auto it =
          semanticProgram.publishedRoutingLookups.bridgePathChoiceStdlibSurfaceIdsByExpr.find(
              semanticNodeId);
      it != semanticProgram.publishedRoutingLookups.bridgePathChoiceStdlibSurfaceIdsByExpr.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::string_view semanticProgramLookupPublishedLowererSoftwareNumericType(
    const SemanticProgram &semanticProgram) {
  std::string_view softwareNumericType;
  std::string error;
  if (!semanticProgramResolvePublishedLowererSoftwareNumericType(
          semanticProgram, softwareNumericType, error)) {
    return {};
  }
  return softwareNumericType;
}

bool semanticProgramResolvePublishedLowererSoftwareNumericType(
    const SemanticProgram &semanticProgram,
    std::string_view &softwareNumericType,
    std::string &error) {
  softwareNumericType = {};
  const auto &facts = semanticProgram.publishedLowererPreflightFacts;
  const bool hasSoftwareNumericType =
      facts.hasSoftwareNumericType || facts.firstSoftwareNumericTypeId != InvalidSymbolId;
  if (!hasSoftwareNumericType) {
    return true;
  }
  if (facts.firstSoftwareNumericTypeId == InvalidSymbolId) {
    error = "missing semantic-product lowerer preflight software numeric type id";
    return false;
  }
  softwareNumericType =
      semanticProgramResolveCallTargetString(semanticProgram, facts.firstSoftwareNumericTypeId);
  if (softwareNumericType.empty()) {
    error = "stale semantic-product lowerer preflight software numeric type id";
    return false;
  }
  return true;
}

std::string_view semanticProgramLookupPublishedLowererRuntimeReflectionPath(
    const SemanticProgram &semanticProgram) {
  std::string_view runtimeReflectionPath;
  std::string error;
  if (!semanticProgramResolvePublishedLowererRuntimeReflectionPath(
          semanticProgram, runtimeReflectionPath, error)) {
    return {};
  }
  return runtimeReflectionPath;
}

bool semanticProgramResolvePublishedLowererRuntimeReflectionPath(
    const SemanticProgram &semanticProgram,
    std::string_view &runtimeReflectionPath,
    std::string &error) {
  runtimeReflectionPath = {};
  const auto &facts = semanticProgram.publishedLowererPreflightFacts;
  const bool hasRuntimeReflectionPath =
      facts.hasRuntimeReflectionPath ||
      facts.firstRuntimeReflectionPathId != InvalidSymbolId ||
      facts.firstRuntimeReflectionPathIsObjectTable;
  if (!hasRuntimeReflectionPath) {
    return true;
  }
  if (facts.firstRuntimeReflectionPathId == InvalidSymbolId) {
    error = "missing semantic-product lowerer preflight runtime reflection path id";
    return false;
  }
  runtimeReflectionPath =
      semanticProgramResolveCallTargetString(semanticProgram, facts.firstRuntimeReflectionPathId);
  if (runtimeReflectionPath.empty()) {
    error = "stale semantic-product lowerer preflight runtime reflection path id";
    return false;
  }
  return true;
}

bool semanticProgramLookupPublishedLowererRuntimeReflectionUsesObjectTable(
    const SemanticProgram &semanticProgram) {
  return semanticProgram.publishedLowererPreflightFacts.firstRuntimeReflectionPathId !=
             InvalidSymbolId &&
         semanticProgram.publishedLowererPreflightFacts.firstRuntimeReflectionPathIsObjectTable;
}

const SemanticProgramCallableSummary *semanticProgramLookupPublishedCallableSummaryByPathId(
    const SemanticProgram &semanticProgram,
    SymbolId fullPathId) {
  if (fullPathId == InvalidSymbolId) {
    return nullptr;
  }
  if (const auto it = semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId.find(fullPathId);
      it != semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId.end()) {
    if (it->second < semanticProgram.callableSummaries.size()) {
      return &semanticProgram.callableSummaries[it->second];
    }
    return nullptr;
  }
  return nullptr;
}

const SemanticProgramCallableSummary *semanticProgramLookupPublishedCallableSummary(
    const SemanticProgram &semanticProgram,
    std::string_view fullPath) {
  const auto fullPathId = semanticProgramLookupCallTargetStringId(semanticProgram, fullPath);
  if (!fullPathId.has_value()) {
    return nullptr;
  }
  return semanticProgramLookupPublishedCallableSummaryByPathId(semanticProgram, *fullPathId);
}

} // namespace primec
