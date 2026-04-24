#include "SemanticsWorkerSymbolMerge.h"

#include <limits>
#include <string_view>
#include <unordered_map>

namespace primec::semantics {
namespace {

std::string_view returnKindSnapshotName(ReturnKind kind) {
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

uint32_t clampOriginOrder(std::size_t value) {
  if (value >= static_cast<std::size_t>(std::numeric_limits<uint32_t>::max())) {
    return std::numeric_limits<uint32_t>::max();
  }
  return static_cast<uint32_t>(value);
}

void internWithOrigin(SymbolInterner &interner,
                      std::string_view text,
                      uint32_t declarationOrder,
                      uint64_t semanticNodeId,
                      uint32_t fieldOrder) {
  if (text.empty()) {
    return;
  }
  interner.intern(text,
                  SymbolOriginKey{
                      .moduleOrder = 0,
                      .declarationOrder = declarationOrder,
                      .semanticNodeOrder = semanticNodeId != 0
                                               ? semanticNodeId
                                               : std::numeric_limits<uint64_t>::max(),
                      .fieldOrder = fieldOrder,
                  });
}

std::unordered_map<std::string_view, uint32_t> buildExecutionOrderByPath(
    const Program &program,
    uint32_t definitionCount) {
  std::unordered_map<std::string_view, uint32_t> executionOrderByPath;
  executionOrderByPath.reserve(program.executions.size());
  for (std::size_t i = 0; i < program.executions.size(); ++i) {
    executionOrderByPath.emplace(program.executions[i].fullPath,
                                 clampOriginOrder(definitionCount + i));
  }
  return executionOrderByPath;
}

uint32_t declarationOrderForPath(
    std::string_view fullPath,
    const DefinitionPrepassSnapshot &definitionPrepassSnapshot,
    const std::unordered_map<std::string_view, uint32_t> &executionOrderByPath) {
  if (const auto it = definitionPrepassSnapshot.firstDeclarationIndexByPath.find(fullPath);
      it != definitionPrepassSnapshot.firstDeclarationIndexByPath.end()) {
    return clampOriginOrder(it->second);
  }
  if (const auto executionIt = executionOrderByPath.find(fullPath);
      executionIt != executionOrderByPath.end()) {
    return executionIt->second;
  }
  return std::numeric_limits<uint32_t>::max();
}

void appendCallableSummaryStrings(
    SymbolInterner &interner,
    const DefinitionPrepassSnapshot &definitionPrepassSnapshot,
    const std::unordered_map<std::string_view, uint32_t> &executionOrderByPath,
    const std::vector<CollectedCallableSummaryEntry> &callableSummaries) {
  for (const auto &entry : callableSummaries) {
    const uint32_t declarationOrder =
        declarationOrderForPath(entry.fullPath,
                                definitionPrepassSnapshot,
                                executionOrderByPath);
    internWithOrigin(interner, entry.fullPath, declarationOrder, entry.semanticNodeId, 0);
    internWithOrigin(interner,
                     returnKindSnapshotName(entry.returnKind),
                     declarationOrder,
                     entry.semanticNodeId,
                     1);
    for (std::size_t i = 0; i < entry.activeEffects.size(); ++i) {
      internWithOrigin(interner,
                       entry.activeEffects[i],
                       declarationOrder,
                       entry.semanticNodeId,
                       clampOriginOrder(2 + i));
    }
    for (std::size_t i = 0; i < entry.activeCapabilities.size(); ++i) {
      internWithOrigin(interner,
                       entry.activeCapabilities[i],
                       declarationOrder,
                       entry.semanticNodeId,
                       clampOriginOrder(32 + i));
    }
    internWithOrigin(interner,
                     entry.resultValueType,
                     declarationOrder,
                     entry.semanticNodeId,
                     64);
    internWithOrigin(interner,
                     entry.resultErrorType,
                     declarationOrder,
                     entry.semanticNodeId,
                     65);
    internWithOrigin(interner,
                     entry.onErrorHandlerPath,
                     declarationOrder,
                     entry.semanticNodeId,
                     66);
    internWithOrigin(interner,
                     entry.onErrorErrorType,
                     declarationOrder,
                     entry.semanticNodeId,
                     67);
  }
}

void appendOnErrorStrings(
    SymbolInterner &interner,
    const DefinitionPrepassSnapshot &definitionPrepassSnapshot,
    const std::unordered_map<std::string_view, uint32_t> &executionOrderByPath,
    const std::vector<OnErrorSnapshotEntry> &onErrorFacts) {
  for (const auto &entry : onErrorFacts) {
    const uint32_t declarationOrder =
        declarationOrderForPath(entry.definitionPath,
                                definitionPrepassSnapshot,
                                executionOrderByPath);
    internWithOrigin(interner, entry.definitionPath, declarationOrder, entry.semanticNodeId, 0);
    internWithOrigin(interner,
                     returnKindSnapshotName(entry.returnKind),
                     declarationOrder,
                     entry.semanticNodeId,
                     1);
    internWithOrigin(interner, entry.handlerPath, declarationOrder, entry.semanticNodeId, 2);
    internWithOrigin(interner, entry.errorType, declarationOrder, entry.semanticNodeId, 3);
    for (std::size_t i = 0; i < entry.boundArgTexts.size(); ++i) {
      internWithOrigin(interner,
                       entry.boundArgTexts[i],
                       declarationOrder,
                       entry.semanticNodeId,
                       clampOriginOrder(4 + i));
    }
    internWithOrigin(interner,
                     entry.returnResultValueType,
                     declarationOrder,
                     entry.semanticNodeId,
                     64);
    internWithOrigin(interner,
                     entry.returnResultErrorType,
                     declarationOrder,
                     entry.semanticNodeId,
                     65);
  }
}

} // namespace

void appendSemanticPublicationStringOrigins(
    SymbolInterner &interner,
    const Program &program,
    const DefinitionPrepassSnapshot &definitionPrepassSnapshot,
    const std::vector<CollectedCallableSummaryEntry> &callableSummaries,
    const std::vector<OnErrorSnapshotEntry> &onErrorFacts) {
  const auto executionOrderByPath =
      buildExecutionOrderByPath(program,
                                clampOriginOrder(
                                    definitionPrepassSnapshot.declarationsInStableOrder.size()));
  appendCallableSummaryStrings(interner,
                               definitionPrepassSnapshot,
                               executionOrderByPath,
                               callableSummaries);
  appendOnErrorStrings(interner,
                       definitionPrepassSnapshot,
                       executionOrderByPath,
                       onErrorFacts);
}

} // namespace primec::semantics
