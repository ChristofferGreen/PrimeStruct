#include "primec/SemanticProduct.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <string_view>

namespace primec {
namespace {

std::string quoteSemanticString(std::string_view value) {
  std::string out;
  out.reserve(value.size() + 2);
  out.push_back('"');
  for (char ch : value) {
    switch (ch) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out.push_back(ch);
        break;
    }
  }
  out.push_back('"');
  return out;
}

std::string formatSemanticBool(bool value) {
  return value ? "true" : "false";
}

std::string formatSemanticStringList(const std::vector<std::string> &values) {
  std::ostringstream out;
  out << "[";
  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << quoteSemanticString(values[i]);
  }
  out << "]";
  return out.str();
}

std::string formatSemanticStdlibSurfaceId(StdlibSurfaceId id) {
  if (const auto *metadata = findStdlibSurfaceMetadata(id); metadata != nullptr) {
    return std::string(metadata->bridgeKey);
  }
  return std::to_string(static_cast<int>(id));
}

void appendSemanticHeaderLine(std::ostringstream &out, std::string_view label, const std::string &value) {
  out << "  " << label << ": " << value << "\n";
}

void appendSemanticIndexedLine(std::ostringstream &out,
                               std::string_view label,
                               size_t index,
                               const std::string &value) {
  out << "  " << label << "[" << index << "]: " << value << "\n";
}

std::string formatSemanticSourceLocation(int line, int column) {
  return std::to_string(line) + ":" + std::to_string(column);
}

template <typename EntryT>
const EntryT *lookupPublishedSemanticEntryByIndex(const std::vector<EntryT> &entries, std::size_t entryIndex) {
  if (entryIndex >= entries.size()) {
    return nullptr;
  }
  return &entries[entryIndex];
}

template <typename EntryT, typename ModuleIndicesFn>
std::vector<const EntryT *> makeSemanticProgramModuleResolvedView(
    const SemanticProgram &semanticProgram,
    const std::vector<EntryT> &storage,
    ModuleIndicesFn moduleIndices) {
  std::vector<const EntryT *> entries;
  if (!semanticProgram.moduleResolvedArtifacts.empty()) {
    size_t moduleEntryCount = 0;
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      moduleEntryCount += moduleIndices(module).size();
    }
    entries.reserve(moduleEntryCount);
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      for (const std::size_t entryIndex : moduleIndices(module)) {
        if (entryIndex < storage.size()) {
          entries.push_back(&storage[entryIndex]);
        }
      }
    }
    if (!entries.empty() || storage.empty() || semanticProgram.publishedStorageFrozen) {
      return entries;
    }
  }
  if (semanticProgram.publishedStorageFrozen) {
    return entries;
  }

  entries.reserve(storage.size());
  for (const auto &entry : storage) {
    entries.push_back(&entry);
  }
  return entries;
}

uint64_t makeLocalAutoInitPathBindingNameKey(SymbolId initializerPathId, SymbolId bindingNameId) {
  return (static_cast<uint64_t>(initializerPathId) << 32) |
         static_cast<uint64_t>(bindingNameId);
}

uint64_t makeQueryFactResolvedPathCallNameKey(SymbolId resolvedPathId, SymbolId callNameId) {
  return (static_cast<uint64_t>(resolvedPathId) << 32) |
         static_cast<uint64_t>(callNameId);
}

uint64_t makeSumVariantMetadataKey(SymbolId sumPathId, SymbolId variantNameId) {
  return (static_cast<uint64_t>(sumPathId) << 32) |
         static_cast<uint64_t>(variantNameId);
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

} // namespace

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
  static const std::vector<SemanticProgramFactFamilyInfo> Families = {
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
      {"onErrorFacts",
       SemanticProgramFactOwnership::SemanticProduct,
       "published on_error facts"},
  };
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
  if (semanticProgram.publishedStorageFrozen) {
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
  return semanticProgramResolveCallTargetString(
      semanticProgram,
      semanticProgram.publishedLowererPreflightFacts.firstSoftwareNumericTypeId);
}

std::string_view semanticProgramLookupPublishedLowererRuntimeReflectionPath(
    const SemanticProgram &semanticProgram) {
  return semanticProgramResolveCallTargetString(
      semanticProgram,
      semanticProgram.publishedLowererPreflightFacts.firstRuntimeReflectionPathId);
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
  if (semanticProgram.publishedStorageFrozen) {
    return nullptr;
  }
  for (const auto &entry : semanticProgram.callableSummaries) {
    if (entry.fullPathId != fullPathId) {
      continue;
    }
    if (!semanticProgramCallableSummaryFullPath(semanticProgram, entry).empty()) {
      return &entry;
    }
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

const SemanticProgramOnErrorFact *semanticProgramLookupPublishedOnErrorFactByDefinitionSemanticId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return nullptr;
  }
  if (const auto it =
          semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionId.find(semanticNodeId);
      it != semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionId.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.onErrorFacts, it->second);
  }
  return nullptr;
}

const SemanticProgramOnErrorFact *semanticProgramLookupPublishedOnErrorFactByDefinitionPathId(
    const SemanticProgram &semanticProgram,
    SymbolId definitionPathId) {
  if (definitionPathId == InvalidSymbolId) {
    return nullptr;
  }
  if (const auto it =
          semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionPathId.find(
              definitionPathId);
      it != semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionPathId.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.onErrorFacts, it->second);
  }
  return nullptr;
}

const SemanticProgramCollectionSpecialization *
semanticProgramLookupPublishedCollectionSpecializationBySemanticId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return nullptr;
  }
  if (const auto it =
          semanticProgram.publishedRoutingLookups.collectionSpecializationIndicesByExpr.find(
              semanticNodeId);
      it != semanticProgram.publishedRoutingLookups.collectionSpecializationIndicesByExpr.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.collectionSpecializations,
                                               it->second);
  }
  return nullptr;
}

const SemanticProgramLocalAutoFact *semanticProgramLookupPublishedLocalAutoFactBySemanticId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return nullptr;
  }
  if (const auto it =
          semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.find(semanticNodeId);
      it != semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.localAutoFacts, it->second);
  }
  return nullptr;
}

const SemanticProgramLocalAutoFact *semanticProgramLookupPublishedLocalAutoFactByInitializerPathAndBindingNameId(
    const SemanticProgram &semanticProgram,
    SymbolId initializerPathId,
    SymbolId bindingNameId) {
  if (initializerPathId == InvalidSymbolId || bindingNameId == InvalidSymbolId) {
    return nullptr;
  }
  const uint64_t compositeKey =
      makeLocalAutoInitPathBindingNameKey(initializerPathId, bindingNameId);
  if (const auto it =
          semanticProgram.publishedRoutingLookups.localAutoFactIndicesByInitPathAndBindingNameId.find(
              compositeKey);
      it != semanticProgram.publishedRoutingLookups.localAutoFactIndicesByInitPathAndBindingNameId.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.localAutoFacts, it->second);
  }
  return nullptr;
}

const SemanticProgramQueryFact *semanticProgramLookupPublishedQueryFactBySemanticId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return nullptr;
  }
  if (const auto it = semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.find(semanticNodeId);
      it != semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.queryFacts, it->second);
  }
  return nullptr;
}

const SemanticProgramQueryFact *semanticProgramLookupPublishedQueryFactByResolvedPathAndCallNameId(
    const SemanticProgram &semanticProgram,
    SymbolId resolvedPathId,
    SymbolId callNameId) {
  if (resolvedPathId == InvalidSymbolId || callNameId == InvalidSymbolId) {
    return nullptr;
  }
  const uint64_t compositeKey = makeQueryFactResolvedPathCallNameKey(resolvedPathId, callNameId);
  if (const auto it =
          semanticProgram.publishedRoutingLookups.queryFactIndicesByResolvedPathAndCallNameId.find(
              compositeKey);
      it != semanticProgram.publishedRoutingLookups.queryFactIndicesByResolvedPathAndCallNameId.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.queryFacts, it->second);
  }
  return nullptr;
}

const SemanticProgramTryFact *semanticProgramLookupPublishedTryFactBySemanticId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId) {
  if (semanticNodeId == 0) {
    return nullptr;
  }
  if (const auto it = semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.find(semanticNodeId);
      it != semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.tryFacts, it->second);
  }
  return nullptr;
}

const SemanticProgramTryFact *semanticProgramLookupPublishedTryFactByOperandPathAndSource(
    const SemanticProgram &semanticProgram,
    SymbolId operandPathId,
    int sourceLine,
    int sourceColumn) {
  if (operandPathId == InvalidSymbolId || sourceLine <= 0 || sourceColumn <= 0) {
    return nullptr;
  }
  const uint64_t compositeKey =
      makeTryFactOperandPathSourceKey(operandPathId, sourceLine, sourceColumn);
  if (const auto it =
          semanticProgram.publishedRoutingLookups.tryFactIndicesByOperandPathAndSource.find(
              compositeKey);
      it != semanticProgram.publishedRoutingLookups.tryFactIndicesByOperandPathAndSource.end()) {
    return lookupPublishedSemanticEntryByIndex(semanticProgram.tryFacts, it->second);
  }
  return nullptr;
}

const SemanticProgramTypeMetadata *semanticProgramLookupTypeMetadata(
    const SemanticProgram &semanticProgram,
    std::string_view fullPath) {
  if (fullPath.empty()) {
    return nullptr;
  }
  if (const auto fullPathId = semanticProgramLookupCallTargetStringId(semanticProgram, fullPath);
      fullPathId.has_value()) {
    if (const auto it = semanticProgram.publishedRoutingLookups.typeMetadataIndicesByPathId.find(*fullPathId);
        it != semanticProgram.publishedRoutingLookups.typeMetadataIndicesByPathId.end()) {
      return lookupPublishedSemanticEntryByIndex(semanticProgram.typeMetadata, it->second);
    }
  }
  if (semanticProgram.publishedStorageFrozen) {
    return nullptr;
  }
  for (const auto &entry : semanticProgram.typeMetadata) {
    if (entry.fullPath == fullPath) {
      return &entry;
    }
  }
  return nullptr;
}

std::vector<const SemanticProgramTypeMetadata *>
semanticProgramStructTypeMetadataView(const SemanticProgram &semanticProgram) {
  std::vector<const SemanticProgramTypeMetadata *> view;
  view.reserve(semanticProgram.typeMetadata.size());
  for (const auto &entry : semanticProgram.typeMetadata) {
    if (entry.category == "struct" || entry.category == "pod" || entry.category == "handle" ||
        entry.category == "gpu_lane") {
      view.push_back(&entry);
    }
  }
  return view;
}

std::vector<const SemanticProgramStructFieldMetadata *>
semanticProgramStructFieldMetadataView(const SemanticProgram &semanticProgram,
                                       std::string_view structPath) {
  std::vector<const SemanticProgramStructFieldMetadata *> view;
  if (structPath.empty()) {
    return view;
  }
  if (const auto structPathId = semanticProgramLookupCallTargetStringId(semanticProgram, structPath);
      structPathId.has_value()) {
    if (const auto it =
            semanticProgram.publishedRoutingLookups.structFieldMetadataIndicesByStructPathId.find(
                *structPathId);
        it != semanticProgram.publishedRoutingLookups.structFieldMetadataIndicesByStructPathId.end()) {
      view.reserve(it->second.size());
      for (const std::size_t entryIndex : it->second) {
        if (entryIndex < semanticProgram.structFieldMetadata.size()) {
          view.push_back(&semanticProgram.structFieldMetadata[entryIndex]);
        }
      }
    }
  }
  if (view.empty()) {
    if (semanticProgram.publishedStorageFrozen) {
      return view;
    }
    for (const auto &entry : semanticProgram.structFieldMetadata) {
      if (entry.structPath == structPath) {
        view.push_back(&entry);
      }
    }
  }
  std::stable_sort(view.begin(),
                   view.end(),
                   [](const SemanticProgramStructFieldMetadata *left,
                      const SemanticProgramStructFieldMetadata *right) {
                     if (left->fieldIndex != right->fieldIndex) {
                       return left->fieldIndex < right->fieldIndex;
                     }
                     return left->fieldName < right->fieldName;
                   });
  return view;
}

const SemanticProgramSumTypeMetadata *semanticProgramLookupPublishedSumTypeMetadata(
    const SemanticProgram &semanticProgram,
    std::string_view fullPath) {
  if (fullPath.empty()) {
    return nullptr;
  }
  if (const auto fullPathId = semanticProgramLookupCallTargetStringId(semanticProgram, fullPath);
      fullPathId.has_value()) {
    if (const auto it =
            semanticProgram.publishedRoutingLookups.sumTypeMetadataIndicesByPathId.find(
                *fullPathId);
        it != semanticProgram.publishedRoutingLookups.sumTypeMetadataIndicesByPathId.end()) {
      return lookupPublishedSemanticEntryByIndex(semanticProgram.sumTypeMetadata, it->second);
    }
  }
  if (semanticProgram.publishedStorageFrozen) {
    return nullptr;
  }
  for (const auto &entry : semanticProgram.sumTypeMetadata) {
    if (entry.fullPath == fullPath) {
      return &entry;
    }
  }
  return nullptr;
}

const SemanticProgramSumVariantMetadata *semanticProgramLookupPublishedSumVariantMetadata(
    const SemanticProgram &semanticProgram,
    std::string_view sumPath,
    std::string_view variantName) {
  if (sumPath.empty() || variantName.empty()) {
    return nullptr;
  }
  const auto sumPathId = semanticProgramLookupCallTargetStringId(semanticProgram, sumPath);
  const auto variantNameId = semanticProgramLookupCallTargetStringId(semanticProgram, variantName);
  if (sumPathId.has_value() && variantNameId.has_value()) {
    const uint64_t compositeKey =
        makeSumVariantMetadataKey(*sumPathId, *variantNameId);
    if (const auto it =
            semanticProgram.publishedRoutingLookups
                .sumVariantMetadataIndicesBySumPathAndVariantNameId.find(compositeKey);
        it != semanticProgram.publishedRoutingLookups
                  .sumVariantMetadataIndicesBySumPathAndVariantNameId.end()) {
      return lookupPublishedSemanticEntryByIndex(semanticProgram.sumVariantMetadata, it->second);
    }
  }
  if (semanticProgram.publishedStorageFrozen) {
    return nullptr;
  }
  for (const auto &entry : semanticProgram.sumVariantMetadata) {
    if (entry.sumPath == sumPath && entry.variantName == variantName) {
      return &entry;
    }
  }
  return nullptr;
}

std::string_view semanticProgramDirectCallTargetResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramDirectCallTarget &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.resolvedPathId);
}

std::string_view semanticProgramMethodCallTargetResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramMethodCallTarget &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.resolvedPathId);
}

std::string_view semanticProgramBridgePathChoiceHelperName(
    const SemanticProgram &semanticProgram,
    const SemanticProgramBridgePathChoice &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.helperNameId);
}

std::string_view semanticProgramCallableSummaryFullPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramCallableSummary &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.fullPathId);
}

std::string_view semanticProgramBindingFactResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramBindingFact &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.resolvedPathId);
}

std::string_view semanticProgramReturnFactDefinitionPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramReturnFact &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.definitionPathId);
}

std::string_view semanticProgramLocalAutoFactInitializerResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramLocalAutoFact &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.initializerResolvedPathId);
}

std::string_view semanticProgramQueryFactResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramQueryFact &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.resolvedPathId);
}

std::string_view semanticProgramTryFactOperandResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramTryFact &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.operandResolvedPathId);
}

std::string_view semanticProgramOnErrorFactDefinitionPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramOnErrorFact &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.definitionPathId);
}

std::string_view semanticProgramOnErrorFactHandlerPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramOnErrorFact &entry) {
  return semanticProgramResolveCallTargetString(semanticProgram, entry.handlerPathId);
}

std::string formatSemanticStringListFromIds(const SemanticProgram &semanticProgram,
                                            const std::vector<SymbolId> &ids,
                                            const std::vector<std::string> &fallbackValues) {
  if (ids.empty()) {
    return formatSemanticStringList(fallbackValues);
  }
  std::vector<std::string> resolvedValues;
  resolvedValues.reserve(ids.size());
  for (std::size_t i = 0; i < ids.size(); ++i) {
    const std::string_view resolved = semanticProgramResolveCallTargetString(semanticProgram, ids[i]);
    if (!resolved.empty()) {
      resolvedValues.emplace_back(resolved);
    } else if (i < fallbackValues.size()) {
      resolvedValues.push_back(fallbackValues[i]);
    } else {
      resolvedValues.emplace_back();
    }
  }
  return formatSemanticStringList(resolvedValues);
}

std::vector<const SemanticProgramDirectCallTarget *>
semanticProgramDirectCallTargetView(const SemanticProgram &semanticProgram) {
  return makeSemanticProgramModuleResolvedView(
      semanticProgram,
      semanticProgram.directCallTargets,
      [](const SemanticProgramModuleResolvedArtifacts &module) -> const std::vector<std::size_t> & {
        return module.directCallTargetIndices;
      });
}

std::vector<const SemanticProgramMethodCallTarget *>
semanticProgramMethodCallTargetView(const SemanticProgram &semanticProgram) {
  return makeSemanticProgramModuleResolvedView(
      semanticProgram,
      semanticProgram.methodCallTargets,
      [](const SemanticProgramModuleResolvedArtifacts &module) -> const std::vector<std::size_t> & {
        return module.methodCallTargetIndices;
      });
}

std::vector<const SemanticProgramBridgePathChoice *>
semanticProgramBridgePathChoiceView(const SemanticProgram &semanticProgram) {
  return makeSemanticProgramModuleResolvedView(
      semanticProgram,
      semanticProgram.bridgePathChoices,
      [](const SemanticProgramModuleResolvedArtifacts &module) -> const std::vector<std::size_t> & {
        return module.bridgePathChoiceIndices;
      });
}

std::vector<const SemanticProgramCallableSummary *>
semanticProgramCallableSummaryView(const SemanticProgram &semanticProgram) {
  return makeSemanticProgramModuleResolvedView(
      semanticProgram,
      semanticProgram.callableSummaries,
      [](const SemanticProgramModuleResolvedArtifacts &module) -> const std::vector<std::size_t> & {
        return module.callableSummaryIndices;
      });
}

std::vector<const SemanticProgramBindingFact *>
semanticProgramBindingFactView(const SemanticProgram &semanticProgram) {
  return makeSemanticProgramModuleResolvedView(
      semanticProgram,
      semanticProgram.bindingFacts,
      [](const SemanticProgramModuleResolvedArtifacts &module) -> const std::vector<std::size_t> & {
        return module.bindingFactIndices;
      });
}

std::vector<const SemanticProgramCollectionSpecialization *>
semanticProgramCollectionSpecializationView(const SemanticProgram &semanticProgram) {
  return makeSemanticProgramModuleResolvedView(
      semanticProgram,
      semanticProgram.collectionSpecializations,
      [](const SemanticProgramModuleResolvedArtifacts &module) -> const std::vector<std::size_t> & {
        return module.collectionSpecializationIndices;
      });
}

std::vector<const SemanticProgramReturnFact *>
semanticProgramReturnFactView(const SemanticProgram &semanticProgram) {
  return makeSemanticProgramModuleResolvedView(
      semanticProgram,
      semanticProgram.returnFacts,
      [](const SemanticProgramModuleResolvedArtifacts &module) -> const std::vector<std::size_t> & {
        return module.returnFactIndices;
      });
}

std::vector<const SemanticProgramLocalAutoFact *>
semanticProgramLocalAutoFactView(const SemanticProgram &semanticProgram) {
  return makeSemanticProgramModuleResolvedView(
      semanticProgram,
      semanticProgram.localAutoFacts,
      [](const SemanticProgramModuleResolvedArtifacts &module) -> const std::vector<std::size_t> & {
        return module.localAutoFactIndices;
      });
}

std::vector<const SemanticProgramQueryFact *>
semanticProgramQueryFactView(const SemanticProgram &semanticProgram) {
  return makeSemanticProgramModuleResolvedView(
      semanticProgram,
      semanticProgram.queryFacts,
      [](const SemanticProgramModuleResolvedArtifacts &module) -> const std::vector<std::size_t> & {
        return module.queryFactIndices;
      });
}

std::vector<const SemanticProgramTryFact *>
semanticProgramTryFactView(const SemanticProgram &semanticProgram) {
  return makeSemanticProgramModuleResolvedView(
      semanticProgram,
      semanticProgram.tryFacts,
      [](const SemanticProgramModuleResolvedArtifacts &module) -> const std::vector<std::size_t> & {
        return module.tryFactIndices;
      });
}

std::vector<const SemanticProgramOnErrorFact *>
semanticProgramOnErrorFactView(const SemanticProgram &semanticProgram) {
  return makeSemanticProgramModuleResolvedView(
      semanticProgram,
      semanticProgram.onErrorFacts,
      [](const SemanticProgramModuleResolvedArtifacts &module) -> const std::vector<std::size_t> & {
        return module.onErrorFactIndices;
      });
}

std::string formatSemanticProgram(const SemanticProgram &semanticProgram) {
  std::ostringstream out;
  out << "semantic_product {\n";
  appendSemanticHeaderLine(out, "entry_path", quoteSemanticString(semanticProgram.entryPath));
  for (size_t i = 0; i < semanticProgram.sourceImports.size(); ++i) {
    appendSemanticIndexedLine(out, "source_imports", i, quoteSemanticString(semanticProgram.sourceImports[i]));
  }
  for (size_t i = 0; i < semanticProgram.imports.size(); ++i) {
    appendSemanticIndexedLine(out, "imports", i, quoteSemanticString(semanticProgram.imports[i]));
  }
  for (size_t i = 0; i < semanticProgram.definitions.size(); ++i) {
    const auto &entry = semanticProgram.definitions[i];
    appendSemanticIndexedLine(out,
                              "definitions",
                              i,
                              "full_path=" + quoteSemanticString(entry.fullPath) + " name=" +
                                  quoteSemanticString(entry.name) + " namespace_prefix=" +
                                  quoteSemanticString(entry.namespacePrefix) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  for (size_t i = 0; i < semanticProgram.executions.size(); ++i) {
    const auto &entry = semanticProgram.executions[i];
    appendSemanticIndexedLine(out,
                              "executions",
                              i,
                              "full_path=" + quoteSemanticString(entry.fullPath) + " name=" +
                                  quoteSemanticString(entry.name) + " namespace_prefix=" +
                                  quoteSemanticString(entry.namespacePrefix) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto directCallTargets = semanticProgramDirectCallTargetView(semanticProgram);
  for (size_t i = 0; i < directCallTargets.size(); ++i) {
    const auto &entry = *directCallTargets[i];
    const std::string_view scopePath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId);
    const std::string_view callName =
        semanticProgramResolveCallTargetString(semanticProgram, entry.callNameId);
    const std::string_view resolvedPath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.resolvedPathId);
    appendSemanticIndexedLine(out,
                              "direct_call_targets",
                              i,
                              "scope_path=" +
                                  quoteSemanticString(scopePath.empty() ? entry.scopePath : scopePath) +
                                  " call_name=" +
                                  quoteSemanticString(callName.empty() ? entry.callName : callName) +
                                  " resolved_path=" +
                                  quoteSemanticString(resolvedPath) +
                                  (entry.stdlibSurfaceId.has_value()
                                       ? " stdlib_surface_id=" +
                                             quoteSemanticString(
                                                 formatSemanticStdlibSurfaceId(*entry.stdlibSurfaceId))
                                       : "") +
                                  " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto methodCallTargets = semanticProgramMethodCallTargetView(semanticProgram);
  for (size_t i = 0; i < methodCallTargets.size(); ++i) {
    const auto &entry = *methodCallTargets[i];
    const std::string_view scopePath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId);
    const std::string_view methodName =
        semanticProgramResolveCallTargetString(semanticProgram, entry.methodNameId);
    const std::string_view receiverTypeText =
        semanticProgramResolveCallTargetString(semanticProgram, entry.receiverTypeTextId);
    const std::string_view resolvedPath =
        semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry);
    appendSemanticIndexedLine(out,
                              "method_call_targets",
                              i,
                              "scope_path=" +
                                  quoteSemanticString(scopePath.empty() ? entry.scopePath : scopePath) +
                                  " method_name=" +
                                  quoteSemanticString(methodName.empty() ? entry.methodName : methodName) +
                                  " receiver_type_text=" +
                                  quoteSemanticString(receiverTypeText.empty()
                                                          ? entry.receiverTypeText
                                                          : receiverTypeText) +
                                  " resolved_path=" +
                                  quoteSemanticString(resolvedPath) +
                                  (entry.stdlibSurfaceId.has_value()
                                       ? " stdlib_surface_id=" +
                                             quoteSemanticString(
                                                 formatSemanticStdlibSurfaceId(*entry.stdlibSurfaceId))
                                       : "") +
                                  " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto bridgePathChoices = semanticProgramBridgePathChoiceView(semanticProgram);
  for (size_t i = 0; i < bridgePathChoices.size(); ++i) {
    const auto &entry = *bridgePathChoices[i];
    const std::string_view scopePath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId);
    const std::string_view collectionFamily =
        semanticProgramResolveCallTargetString(semanticProgram, entry.collectionFamilyId);
    const std::string_view helperName =
        semanticProgramBridgePathChoiceHelperName(semanticProgram, entry);
    const std::string_view chosenPath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.chosenPathId);
    appendSemanticIndexedLine(out,
                              "bridge_path_choices",
                              i,
                              "scope_path=" +
                                  quoteSemanticString(scopePath.empty() ? entry.scopePath : scopePath) +
                                  " collection_family=" +
                                  quoteSemanticString(collectionFamily.empty()
                                                          ? entry.collectionFamily
                                                          : collectionFamily) +
                                  " helper_name=" +
                                  quoteSemanticString(helperName) +
                                  " chosen_path=" +
                                  quoteSemanticString(chosenPath) +
                                  (entry.stdlibSurfaceId.has_value()
                                       ? " stdlib_surface_id=" +
                                             quoteSemanticString(
                                                 formatSemanticStdlibSurfaceId(*entry.stdlibSurfaceId))
                                       : "") +
                                  " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto callableSummaries = semanticProgramCallableSummaryView(semanticProgram);
  for (size_t i = 0; i < callableSummaries.size(); ++i) {
    const auto &entry = *callableSummaries[i];
    const std::string_view fullPath =
        semanticProgramCallableSummaryFullPath(semanticProgram, entry);
    const std::string_view returnKind =
        semanticProgramResolveCallTargetString(semanticProgram, entry.returnKindId);
    const std::string formattedActiveEffects =
        formatSemanticStringListFromIds(semanticProgram, entry.activeEffectIds, entry.activeEffects);
    const std::string formattedActiveCapabilities =
        formatSemanticStringListFromIds(semanticProgram, entry.activeCapabilityIds, entry.activeCapabilities);
    const std::string_view resultValueType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.resultValueTypeId);
    const std::string_view resultErrorType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.resultErrorTypeId);
    const std::string_view onErrorHandlerPath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.onErrorHandlerPathId);
    const std::string_view onErrorErrorType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.onErrorErrorTypeId);
    appendSemanticIndexedLine(out,
                              "callable_summaries",
                              i,
                              "full_path=" +
                                  quoteSemanticString(fullPath) +
                                  " is_execution=" +
                                  formatSemanticBool(entry.isExecution) + " return_kind=" +
                                  quoteSemanticString(returnKind.empty() ? entry.returnKind : returnKind) +
                                  " is_compute=" +
                                  formatSemanticBool(entry.isCompute) + " is_unsafe=" +
                                  formatSemanticBool(entry.isUnsafe) + " active_effects=" +
                                  formattedActiveEffects + " active_capabilities=" +
                                  formattedActiveCapabilities + " has_result_type=" +
                                  formatSemanticBool(entry.hasResultType) + " result_type_has_value=" +
                                  formatSemanticBool(entry.resultTypeHasValue) + " result_value_type=" +
                                  quoteSemanticString(resultValueType.empty()
                                                          ? entry.resultValueType
                                                          : resultValueType) +
                                  " result_error_type=" +
                                  quoteSemanticString(resultErrorType.empty()
                                                          ? entry.resultErrorType
                                                          : resultErrorType) +
                                  " has_on_error=" +
                                  formatSemanticBool(entry.hasOnError) + " on_error_handler_path=" +
                                  quoteSemanticString(onErrorHandlerPath.empty()
                                                          ? entry.onErrorHandlerPath
                                                          : onErrorHandlerPath) +
                                  " on_error_error_type=" +
                                  quoteSemanticString(onErrorErrorType.empty()
                                                          ? entry.onErrorErrorType
                                                          : onErrorErrorType) +
                                  " on_error_bound_arg_count=" +
                                  std::to_string(entry.onErrorBoundArgCount) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle));
  }
  for (size_t i = 0; i < semanticProgram.typeMetadata.size(); ++i) {
    const auto &entry = semanticProgram.typeMetadata[i];
    appendSemanticIndexedLine(out,
                              "type_metadata",
                              i,
                              "full_path=" + quoteSemanticString(entry.fullPath) + " category=" +
                                  quoteSemanticString(entry.category) + " is_public=" +
                                  formatSemanticBool(entry.isPublic) + " has_no_padding=" +
                                  formatSemanticBool(entry.hasNoPadding) + " has_platform_independent_padding=" +
                                  formatSemanticBool(entry.hasPlatformIndependentPadding) +
                                  " has_explicit_alignment=" + formatSemanticBool(entry.hasExplicitAlignment) +
                                  " explicit_alignment_bytes=" + std::to_string(entry.explicitAlignmentBytes) +
                                  " field_count=" + std::to_string(entry.fieldCount) + " enum_value_count=" +
                                  std::to_string(entry.enumValueCount) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  for (size_t i = 0; i < semanticProgram.structFieldMetadata.size(); ++i) {
    const auto &entry = semanticProgram.structFieldMetadata[i];
    appendSemanticIndexedLine(out,
                              "struct_field_metadata",
                              i,
                              "struct_path=" + quoteSemanticString(entry.structPath) + " field_name=" +
                                  quoteSemanticString(entry.fieldName) + " field_index=" +
                                  std::to_string(entry.fieldIndex) + " binding_type_text=" +
                                  quoteSemanticString(entry.bindingTypeText) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  for (size_t i = 0; i < semanticProgram.sumTypeMetadata.size(); ++i) {
    const auto &entry = semanticProgram.sumTypeMetadata[i];
    appendSemanticIndexedLine(out,
                              "sum_type_metadata",
                              i,
                              "full_path=" + quoteSemanticString(entry.fullPath) + " is_public=" +
                                  formatSemanticBool(entry.isPublic) + " active_tag_type_text=" +
                                  quoteSemanticString(entry.activeTagTypeText) + " payload_storage_text=" +
                                  quoteSemanticString(entry.payloadStorageText) + " variant_count=" +
                                  std::to_string(entry.variantCount) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  for (size_t i = 0; i < semanticProgram.sumVariantMetadata.size(); ++i) {
    const auto &entry = semanticProgram.sumVariantMetadata[i];
    appendSemanticIndexedLine(out,
                              "sum_variant_metadata",
                              i,
                              "sum_path=" + quoteSemanticString(entry.sumPath) + " variant_name=" +
                                  quoteSemanticString(entry.variantName) + " variant_index=" +
                                  std::to_string(entry.variantIndex) + " tag_value=" +
                                  std::to_string(entry.tagValue) + " payload_type_text=" +
                                  quoteSemanticString(entry.payloadTypeText) + " has_payload=" +
                                  formatSemanticBool(entry.hasPayload) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto collectionSpecializations = semanticProgramCollectionSpecializationView(semanticProgram);
  for (size_t i = 0; i < collectionSpecializations.size(); ++i) {
    const auto &entry = *collectionSpecializations[i];
    const auto specializationText = [&](SymbolId id, const std::string &fallback) -> std::string_view {
      const std::string_view resolved = semanticProgramResolveCallTargetString(semanticProgram, id);
      return resolved.empty() ? std::string_view(fallback) : resolved;
    };
    appendSemanticIndexedLine(out,
                              "collection_specializations",
                              i,
                              "scope_path=" +
                                  quoteSemanticString(specializationText(entry.scopePathId, entry.scopePath)) +
                                  " site_kind=" +
                                  quoteSemanticString(specializationText(entry.siteKindId, entry.siteKind)) +
                                  " name=" +
                                  quoteSemanticString(specializationText(entry.nameId, entry.name)) +
                                  " collection_family=" +
                                  quoteSemanticString(specializationText(entry.collectionFamilyId,
                                                                         entry.collectionFamily)) +
                                  " binding_type_text=" +
                                  quoteSemanticString(specializationText(entry.bindingTypeTextId,
                                                                         entry.bindingTypeText)) +
                                  " element_type_text=" +
                                  quoteSemanticString(specializationText(entry.elementTypeTextId,
                                                                         entry.elementTypeText)) +
                                  " key_type_text=" +
                                  quoteSemanticString(specializationText(entry.keyTypeTextId,
                                                                         entry.keyTypeText)) +
                                  " value_type_text=" +
                                  quoteSemanticString(specializationText(entry.valueTypeTextId,
                                                                         entry.valueTypeText)) +
                                  " is_reference=" +
                                  formatSemanticBool(entry.isReference) + " is_pointer=" +
                                  formatSemanticBool(entry.isPointer) +
                                  (entry.helperSurfaceId.has_value()
                                       ? " helper_surface_id=" +
                                             quoteSemanticString(
                                                 formatSemanticStdlibSurfaceId(*entry.helperSurfaceId))
                                       : "") +
                                  (entry.constructorSurfaceId.has_value()
                                       ? " constructor_surface_id=" +
                                             quoteSemanticString(formatSemanticStdlibSurfaceId(
                                                 *entry.constructorSurfaceId))
                                       : "") +
                                  " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto bindingFacts = semanticProgramBindingFactView(semanticProgram);
  for (size_t i = 0; i < bindingFacts.size(); ++i) {
    const auto &entry = *bindingFacts[i];
    const std::string_view scopePath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId);
    const std::string_view siteKind =
        semanticProgramResolveCallTargetString(semanticProgram, entry.siteKindId);
    const std::string_view name =
        semanticProgramResolveCallTargetString(semanticProgram, entry.nameId);
    const std::string_view resolvedPath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.resolvedPathId);
    const std::string_view bindingTypeText =
        semanticProgramResolveCallTargetString(semanticProgram, entry.bindingTypeTextId);
    const std::string_view referenceRoot =
        semanticProgramResolveCallTargetString(semanticProgram, entry.referenceRootId);
    appendSemanticIndexedLine(out,
                              "binding_facts",
                              i,
                              "scope_path=" +
                                  quoteSemanticString(scopePath.empty() ? entry.scopePath : scopePath) +
                                  " site_kind=" +
                                  quoteSemanticString(siteKind.empty() ? entry.siteKind : siteKind) +
                                  " name=" + quoteSemanticString(name.empty() ? entry.name : name) +
                                  " resolved_path=" +
                                  quoteSemanticString(resolvedPath) +
                                  " binding_type_text=" +
                                  quoteSemanticString(bindingTypeText.empty()
                                                          ? entry.bindingTypeText
                                                          : bindingTypeText) +
                                  " is_mutable=" +
                                  formatSemanticBool(entry.isMutable) + " is_entry_arg_string=" +
                                  formatSemanticBool(entry.isEntryArgString) + " is_unsafe_reference=" +
                                  formatSemanticBool(entry.isUnsafeReference) + " reference_root=" +
                                  quoteSemanticString(referenceRoot.empty() ? entry.referenceRoot : referenceRoot) +
                                  " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto returnFacts = semanticProgramReturnFactView(semanticProgram);
  for (size_t i = 0; i < returnFacts.size(); ++i) {
    const auto &entry = *returnFacts[i];
    const std::string_view definitionPath =
        semanticProgramReturnFactDefinitionPath(semanticProgram, entry);
    const std::string_view returnKind =
        semanticProgramResolveCallTargetString(semanticProgram, entry.returnKindId);
    const std::string_view structPath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.structPathId);
    const std::string_view bindingTypeText =
        semanticProgramResolveCallTargetString(semanticProgram, entry.bindingTypeTextId);
    const std::string_view referenceRoot =
        semanticProgramResolveCallTargetString(semanticProgram, entry.referenceRootId);
    appendSemanticIndexedLine(out,
                              "return_facts",
                              i,
                              "definition_path=" +
                                  quoteSemanticString(definitionPath) +
                                  " return_kind=" +
                                  quoteSemanticString(returnKind.empty() ? entry.returnKind : returnKind) +
                                  " struct_path=" +
                                  quoteSemanticString(structPath.empty() ? entry.structPath : structPath) +
                                  " binding_type_text=" +
                                  quoteSemanticString(bindingTypeText.empty() ? entry.bindingTypeText
                                                                              : bindingTypeText) +
                                  " is_mutable=" +
                                  formatSemanticBool(entry.isMutable) + " is_entry_arg_string=" +
                                  formatSemanticBool(entry.isEntryArgString) + " is_unsafe_reference=" +
                                  formatSemanticBool(entry.isUnsafeReference) + " reference_root=" +
                                  quoteSemanticString(referenceRoot.empty() ? entry.referenceRoot
                                                                            : referenceRoot) +
                                  " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto localAutoFacts = semanticProgramLocalAutoFactView(semanticProgram);
  for (size_t i = 0; i < localAutoFacts.size(); ++i) {
    const auto &entry = *localAutoFacts[i];
    const auto localAutoText = [&](SymbolId id, const std::string &fallback) -> std::string_view {
      const std::string_view resolved = semanticProgramResolveCallTargetString(semanticProgram, id);
      return resolved.empty() ? std::string_view(fallback) : resolved;
    };
    const std::string initializerStdlibSurfaceText =
        entry.initializerStdlibSurfaceId.has_value()
            ? " initializer_stdlib_surface_id=" +
                  quoteSemanticString(
                      formatSemanticStdlibSurfaceId(*entry.initializerStdlibSurfaceId))
            : std::string{};
    const std::string initializerDirectCallStdlibSurfaceText =
        entry.initializerDirectCallStdlibSurfaceId.has_value()
            ? " initializer_direct_call_stdlib_surface_id=" +
                  quoteSemanticString(formatSemanticStdlibSurfaceId(
                      *entry.initializerDirectCallStdlibSurfaceId))
            : std::string{};
    const std::string initializerMethodCallStdlibSurfaceText =
        entry.initializerMethodCallStdlibSurfaceId.has_value()
            ? " initializer_method_call_stdlib_surface_id=" +
                  quoteSemanticString(formatSemanticStdlibSurfaceId(
                      *entry.initializerMethodCallStdlibSurfaceId))
            : std::string{};
    appendSemanticIndexedLine(out,
                              "local_auto_facts",
                              i,
                              "scope_path=" +
                                  quoteSemanticString(localAutoText(entry.scopePathId, entry.scopePath)) +
                                  " binding_name=" +
                                  quoteSemanticString(localAutoText(entry.bindingNameId, entry.bindingName)) +
                                  " binding_type_text=" +
                                  quoteSemanticString(localAutoText(entry.bindingTypeTextId,
                                                                    entry.bindingTypeText)) +
                                  " initializer_resolved_path=" +
                                  quoteSemanticString(
                                      semanticProgramLocalAutoFactInitializerResolvedPath(
                                          semanticProgram, entry)) +
                                  initializerStdlibSurfaceText +
                                  " initializer_binding_type_text=" +
                                  quoteSemanticString(localAutoText(entry.initializerBindingTypeTextId,
                                                                    entry.initializerBindingTypeText)) +
                                  " initializer_receiver_binding_type_text=" +
                                  quoteSemanticString(
                                      localAutoText(entry.initializerReceiverBindingTypeTextId,
                                                    entry.initializerReceiverBindingTypeText)) +
                                  " initializer_query_type_text=" +
                                  quoteSemanticString(localAutoText(entry.initializerQueryTypeTextId,
                                                                    entry.initializerQueryTypeText)) +
                                  " initializer_result_has_value=" +
                                  formatSemanticBool(entry.initializerResultHasValue) +
                                  " initializer_result_value_type=" +
                                  quoteSemanticString(localAutoText(entry.initializerResultValueTypeId,
                                                                    entry.initializerResultValueType)) +
                                  " initializer_result_error_type=" +
                                  quoteSemanticString(localAutoText(entry.initializerResultErrorTypeId,
                                                                    entry.initializerResultErrorType)) +
                                  " initializer_has_try=" + formatSemanticBool(entry.initializerHasTry) +
                                  " initializer_try_operand_resolved_path=" +
                                  quoteSemanticString(localAutoText(
                                      entry.initializerTryOperandResolvedPathId,
                                      entry.initializerTryOperandResolvedPath)) +
                                  " initializer_try_operand_binding_type_text=" +
                                  quoteSemanticString(
                                      localAutoText(entry.initializerTryOperandBindingTypeTextId,
                                                    entry.initializerTryOperandBindingTypeText)) +
                                  " initializer_try_operand_receiver_binding_type_text=" +
                                  quoteSemanticString(localAutoText(
                                      entry.initializerTryOperandReceiverBindingTypeTextId,
                                      entry.initializerTryOperandReceiverBindingTypeText)) +
                                  " initializer_try_operand_query_type_text=" +
                                  quoteSemanticString(localAutoText(
                                      entry.initializerTryOperandQueryTypeTextId,
                                      entry.initializerTryOperandQueryTypeText)) +
                                  " initializer_try_value_type=" +
                                  quoteSemanticString(localAutoText(entry.initializerTryValueTypeId,
                                                                    entry.initializerTryValueType)) +
                                  " initializer_try_error_type=" +
                                  quoteSemanticString(localAutoText(entry.initializerTryErrorTypeId,
                                                                    entry.initializerTryErrorType)) +
                                  " initializer_try_context_return_kind=" +
                                  quoteSemanticString(
                                      localAutoText(entry.initializerTryContextReturnKindId,
                                                    entry.initializerTryContextReturnKind)) +
                                  " initializer_try_on_error_handler_path=" +
                                  quoteSemanticString(localAutoText(
                                      entry.initializerTryOnErrorHandlerPathId,
                                      entry.initializerTryOnErrorHandlerPath)) +
                                  " initializer_try_on_error_error_type=" +
                                  quoteSemanticString(localAutoText(entry.initializerTryOnErrorErrorTypeId,
                                                                    entry.initializerTryOnErrorErrorType)) +
                                  " initializer_try_on_error_bound_arg_count=" +
                                  std::to_string(entry.initializerTryOnErrorBoundArgCount) +
                                  " initializer_direct_call_resolved_path=" +
                                  quoteSemanticString(localAutoText(
                                      entry.initializerDirectCallResolvedPathId,
                                      entry.initializerDirectCallResolvedPath)) +
                                  initializerDirectCallStdlibSurfaceText +
                                  " initializer_direct_call_return_kind=" +
                                  quoteSemanticString(localAutoText(entry.initializerDirectCallReturnKindId,
                                                                    entry.initializerDirectCallReturnKind)) +
                                  " initializer_method_call_resolved_path=" +
                                  quoteSemanticString(localAutoText(
                                      entry.initializerMethodCallResolvedPathId,
                                      entry.initializerMethodCallResolvedPath)) +
                                  initializerMethodCallStdlibSurfaceText +
                                  " initializer_method_call_return_kind=" +
                                  quoteSemanticString(localAutoText(entry.initializerMethodCallReturnKindId,
                                                                    entry.initializerMethodCallReturnKind)) +
                                  " provenance_handle=" + std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto queryFacts = semanticProgramQueryFactView(semanticProgram);
  for (size_t i = 0; i < queryFacts.size(); ++i) {
    const auto &entry = *queryFacts[i];
    const std::string_view scopePath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId);
    const std::string_view callName =
        semanticProgramResolveCallTargetString(semanticProgram, entry.callNameId);
    const std::string_view resolvedPath = semanticProgramQueryFactResolvedPath(semanticProgram, entry);
    const std::string_view queryTypeText =
        semanticProgramResolveCallTargetString(semanticProgram, entry.queryTypeTextId);
    const std::string_view bindingTypeText =
        semanticProgramResolveCallTargetString(semanticProgram, entry.bindingTypeTextId);
    const std::string_view receiverBindingTypeText =
        semanticProgramResolveCallTargetString(semanticProgram, entry.receiverBindingTypeTextId);
    const std::string_view resultValueType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.resultValueTypeId);
    const std::string_view resultErrorType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.resultErrorTypeId);
    appendSemanticIndexedLine(out,
                              "query_facts",
                              i,
                              "scope_path=" +
                                  quoteSemanticString(scopePath.empty() ? entry.scopePath : scopePath) +
                                  " call_name=" +
                                  quoteSemanticString(callName.empty() ? entry.callName : callName) +
                                  " resolved_path=" +
                                  quoteSemanticString(resolvedPath) +
                                  " query_type_text=" +
                                  quoteSemanticString(queryTypeText.empty() ? entry.queryTypeText : queryTypeText) +
                                  " binding_type_text=" +
                                  quoteSemanticString(bindingTypeText.empty() ? entry.bindingTypeText
                                                                            : bindingTypeText) +
                                  " receiver_binding_type_text=" +
                                  quoteSemanticString(receiverBindingTypeText.empty()
                                                        ? entry.receiverBindingTypeText
                                                        : receiverBindingTypeText) +
                                  " has_result_type=" +
                                  formatSemanticBool(entry.hasResultType) + " result_type_has_value=" +
                                  formatSemanticBool(entry.resultTypeHasValue) + " result_value_type=" +
                                  quoteSemanticString(resultValueType.empty() ? entry.resultValueType
                                                                              : resultValueType) +
                                  " result_error_type=" +
                                  quoteSemanticString(resultErrorType.empty() ? entry.resultErrorType
                                                                              : resultErrorType) +
                                  " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto tryFacts = semanticProgramTryFactView(semanticProgram);
  for (size_t i = 0; i < tryFacts.size(); ++i) {
    const auto &entry = *tryFacts[i];
    const std::string_view scopePath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId);
    const std::string_view operandResolvedPath =
        semanticProgramTryFactOperandResolvedPath(semanticProgram, entry);
    const std::string_view operandBindingTypeText =
        semanticProgramResolveCallTargetString(semanticProgram, entry.operandBindingTypeTextId);
    const std::string_view operandReceiverBindingTypeText = semanticProgramResolveCallTargetString(
        semanticProgram, entry.operandReceiverBindingTypeTextId);
    const std::string_view operandQueryTypeText =
        semanticProgramResolveCallTargetString(semanticProgram, entry.operandQueryTypeTextId);
    const std::string_view valueType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.valueTypeId);
    const std::string_view errorType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.errorTypeId);
    const std::string_view contextReturnKind =
        semanticProgramResolveCallTargetString(semanticProgram, entry.contextReturnKindId);
    const std::string_view onErrorHandlerPath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.onErrorHandlerPathId);
    const std::string_view onErrorErrorType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.onErrorErrorTypeId);
    appendSemanticIndexedLine(out,
                              "try_facts",
                              i,
                                  "scope_path=" +
                                  quoteSemanticString(scopePath.empty() ? entry.scopePath : scopePath) +
                                  " operand_resolved_path=" +
                                  quoteSemanticString(operandResolvedPath) +
                                  " operand_binding_type_text=" +
                                  quoteSemanticString(operandBindingTypeText.empty()
                                                          ? entry.operandBindingTypeText
                                                          : operandBindingTypeText) +
                                  " operand_receiver_binding_type_text=" +
                                  quoteSemanticString(operandReceiverBindingTypeText.empty()
                                                          ? entry.operandReceiverBindingTypeText
                                                          : operandReceiverBindingTypeText) +
                                  " operand_query_type_text=" +
                                  quoteSemanticString(operandQueryTypeText.empty()
                                                          ? entry.operandQueryTypeText
                                                          : operandQueryTypeText) +
                                  " value_type=" +
                                  quoteSemanticString(valueType.empty() ? entry.valueType : valueType) +
                                  " error_type=" +
                                  quoteSemanticString(errorType.empty() ? entry.errorType : errorType) +
                                  " context_return_kind=" +
                                  quoteSemanticString(contextReturnKind.empty() ? entry.contextReturnKind
                                                                                : contextReturnKind) +
                                  " on_error_handler_path=" +
                                  quoteSemanticString(onErrorHandlerPath.empty() ? entry.onErrorHandlerPath
                                                                                 : onErrorHandlerPath) +
                                  " on_error_error_type=" +
                                  quoteSemanticString(onErrorErrorType.empty() ? entry.onErrorErrorType
                                                                               : onErrorErrorType) +
                                  " on_error_bound_arg_count=" +
                                  std::to_string(entry.onErrorBoundArgCount) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto onErrorFacts = semanticProgramOnErrorFactView(semanticProgram);
  for (size_t i = 0; i < onErrorFacts.size(); ++i) {
    const auto &entry = *onErrorFacts[i];
    const std::string_view definitionPath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.definitionPathId);
    const std::string_view returnKind =
        semanticProgramResolveCallTargetString(semanticProgram, entry.returnKindId);
    const std::string_view handlerPath = semanticProgramOnErrorFactHandlerPath(semanticProgram, entry);
    const std::string_view errorType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.errorTypeId);
    const std::string boundArgTexts = formatSemanticStringListFromIds(
        semanticProgram, entry.boundArgTextIds, entry.boundArgTexts);
    const std::string_view returnResultValueType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.returnResultValueTypeId);
    const std::string_view returnResultErrorType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.returnResultErrorTypeId);
    appendSemanticIndexedLine(out,
                              "on_error_facts",
                              i,
                              "definition_path=" +
                                  quoteSemanticString(definitionPath.empty() ? entry.definitionPath
                                                                             : definitionPath) +
                                  " return_kind=" +
                                  quoteSemanticString(returnKind.empty() ? entry.returnKind : returnKind) +
                                  " handler_path=" +
                                  quoteSemanticString(handlerPath) +
                                  " error_type=" +
                                  quoteSemanticString(errorType.empty() ? entry.errorType : errorType) +
                                  " bound_arg_count=" +
                                  std::to_string(entry.boundArgCount) + " bound_arg_texts=" +
                                  boundArgTexts + " return_result_has_value=" +
                                  formatSemanticBool(entry.returnResultHasValue) + " return_result_value_type=" +
                                  quoteSemanticString(returnResultValueType.empty()
                                                          ? entry.returnResultValueType
                                                          : returnResultValueType) +
                                  " return_result_error_type=" +
                                  quoteSemanticString(returnResultErrorType.empty()
                                                          ? entry.returnResultErrorType
                                                          : returnResultErrorType) +
                                  " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle));
  }
  out << "}\n";
  return out.str();
}

} // namespace primec
