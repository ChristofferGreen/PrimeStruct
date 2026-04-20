#include "SemanticPublicationBuilders.h"

#include "primec/StdlibSurfaceRegistry.h"

#include <algorithm>
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

std::string semanticModuleKeyForPath(std::string_view path) {
  if (path.empty()) {
    return "/";
  }

  std::string normalized(path);
  if (normalized.front() != '/') {
    normalized.insert(normalized.begin(), '/');
  }
  if (normalized == "/") {
    return normalized;
  }

  const size_t nextSlash = normalized.find('/', 1);
  if (nextSlash == std::string::npos) {
    return normalized;
  }
  return normalized.substr(0, nextSlash);
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

void releaseInternedField(std::string &text, SymbolId id) {
  if (id != InvalidSymbolId) {
    text.clear();
  }
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
    const std::string moduleKey = semanticModuleKeyForPath(scopePath);
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

void publishDirectCallTargetFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<SemanticsValidator::CollectedDirectCallTargetEntry> &directCallTargets) {
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
    const std::vector<SemanticsValidator::CollectedMethodCallTargetEntry> &methodCallTargets) {
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
    const std::vector<SemanticsValidator::CollectedBridgePathChoiceEntry> &bridgePathChoices) {
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
    const std::vector<SemanticsValidator::CollectedCallableSummaryEntry> &callableSummaries) {
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
    SemanticsValidator::SemanticPublicationSurface &publicationSurface) {
  publishDirectCallTargetFacts(state, publicationSurface.directCallTargets);
  publishMethodCallTargetFacts(state, publicationSurface.methodCallTargets);
  publishBridgePathChoiceFacts(state, publicationSurface.bridgePathChoices);
  publishCallableSummaryFacts(state, publicationSurface.callableSummaries);
}

void publishTypeMetadataFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<SemanticsValidator::TypeMetadataSnapshotEntry> &typeMetadata) {
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
    const std::vector<SemanticsValidator::StructFieldMetadataSnapshotEntry> &structFieldMetadata) {
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

void publishSemanticMetadataFamilies(
    SemanticPublicationBuilderState &state,
    SemanticsValidator::SemanticPublicationSurface &publicationSurface) {
  publishTypeMetadataFacts(state, publicationSurface.typeMetadata);
  publishStructFieldMetadataFacts(state, publicationSurface.structFieldMetadata);
}

void publishBindingFacts(
    SemanticPublicationBuilderState &state,
    std::vector<SemanticsValidator::BindingFactSnapshotEntry> bindingFacts) {
  if (bindingFacts.empty()) {
    return;
  }
  state.semanticProgram.bindingFacts.reserve(bindingFacts.size());
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
    const std::string_view moduleScopePath =
        entry.scopePathId != InvalidSymbolId
            ? semanticProgramResolveCallTargetString(state.semanticProgram, entry.scopePathId)
            : std::string_view(entry.scopePath);
    auto &module = state.ensureModuleResolvedArtifacts(moduleScopePath);
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
    const std::vector<SemanticsValidator::ReturnFactSnapshotEntry> &returnFacts) {
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
    const std::vector<SemanticsValidator::LocalAutoBindingSnapshotEntry> &localAutoFacts) {
  if (localAutoFacts.empty()) {
    return;
  }
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
    state.ensureModuleResolvedArtifacts(snapshotEntry.scopePath).localAutoFactIndices.push_back(
        entryIndex);
  }
}

void publishQueryFacts(SemanticPublicationBuilderState &state,
                       std::vector<SemanticsValidator::QueryFactSnapshotEntry> queryFacts) {
  if (queryFacts.empty()) {
    return;
  }
  state.semanticProgram.queryFacts.reserve(queryFacts.size());
  for (auto &snapshotEntry : queryFacts) {
    const std::string receiverBindingTypeText =
        bindingTypeTextForSemanticProduct(snapshotEntry.receiverBinding);
    SemanticProgramQueryFact entry;
    entry.scopePath = std::move(snapshotEntry.scopePath);
    entry.callName = std::move(snapshotEntry.callName);
    entry.queryTypeText = std::move(snapshotEntry.typeText);
    entry.bindingTypeText = bindingTypeTextForSemanticProduct(snapshotEntry.binding);
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
        semanticProgramInternCallTargetString(state.semanticProgram, receiverBindingTypeText);
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
    module.queryFactIndices.push_back(entryIndex);
  }
}

void publishTryFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<SemanticsValidator::TryValueSnapshotEntry> &tryFacts) {
  if (tryFacts.empty()) {
    return;
  }
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
    state.ensureModuleResolvedArtifacts(snapshotEntry.scopePath).tryFactIndices.push_back(entryIndex);
  }
}

void publishOnErrorFacts(
    SemanticPublicationBuilderState &state,
    const std::vector<SemanticsValidator::OnErrorSnapshotEntry> &onErrorFacts) {
  if (onErrorFacts.empty()) {
    return;
  }
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
    state.ensureModuleResolvedArtifacts(snapshotEntry.definitionPath).onErrorFactIndices.push_back(
        entryIndex);
  }
}

void publishSemanticScopedFactFamilies(
    SemanticPublicationBuilderState &state,
    SemanticsValidator::SemanticPublicationSurface &publicationSurface) {
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
            [](const SemanticProgramModuleResolvedArtifacts &left,
               const SemanticProgramModuleResolvedArtifacts &right) {
              if (left.identity.moduleKey != right.identity.moduleKey) {
                return left.identity.moduleKey < right.identity.moduleKey;
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
    SemanticsValidator::SemanticPublicationSurface publicationSurface,
    const SemanticProductBuildConfig *buildConfig) {
  SemanticPublicationBuilderState state(program, entryPath, buildConfig);
  initializeSemanticProgramPublicationShell(state);
  publishSemanticRoutingFamilies(state, publicationSurface);
  publishSemanticMetadataFamilies(state, publicationSurface);
  publishSemanticScopedFactFamilies(state, publicationSurface);
  finalizeSemanticModuleArtifacts(state);
  return std::move(state.semanticProgram);
}

} // namespace semantics
} // namespace primec
