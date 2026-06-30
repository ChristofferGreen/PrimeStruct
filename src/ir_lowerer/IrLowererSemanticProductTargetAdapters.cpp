#include "IrLowererSemanticProductTargetAdapters.h"

#include "IrLowererTemplateTypeParseHelpers.h"

#include <optional>

namespace primec::ir_lowerer {

namespace {

template <typename FactT>
const FactT *findDefinitionScopedSemanticFact(const std::unordered_map<uint64_t, const FactT *> &factsByDefinitionId,
                                              const Definition &definition) {
  if (definition.semanticNodeId == 0) {
    return nullptr;
  }
  if (const auto it = factsByDefinitionId.find(definition.semanticNodeId); it != factsByDefinitionId.end()) {
    return it->second;
  }
  return nullptr;
}

template <typename FactT>
const FactT *findExpressionScopedSemanticFact(const std::unordered_map<uint64_t, const FactT *> &factsByExpr,
                                              const Expr &expr) {
  if (expr.semanticNodeId == 0) {
    return nullptr;
  }
  if (const auto it = factsByExpr.find(expr.semanticNodeId); it != factsByExpr.end()) {
    return it->second;
  }
  return nullptr;
}

template <typename EntryT>
const EntryT *findSemanticProductEntryByPublishedIndex(const std::vector<EntryT> &entries,
                                                       std::size_t entryIndex) {
  if (entryIndex >= entries.size()) {
    return nullptr;
  }
  return &entries[entryIndex];
}

template <typename KeyT, typename EntryT>
void populateSemanticFactIndex(std::unordered_map<KeyT, const EntryT *> &destination,
                               const std::unordered_map<KeyT, std::size_t> &publishedIndices,
                               const std::vector<EntryT> &entries) {
  destination.reserve(publishedIndices.size());
  for (const auto &[key, entryIndex] : publishedIndices) {
    if (const auto *entry = findSemanticProductEntryByPublishedIndex(entries, entryIndex);
        entry != nullptr) {
      destination.insert_or_assign(key, entry);
    }
  }
}

struct SemanticProductIndexBuilder {
  const SemanticProgram *semanticProgram = nullptr;

  SemanticProductIndex build() const {
    SemanticProductIndex index;
    if (semanticProgram == nullptr) {
      return index;
    }
    buildOnErrorIndex(index);
    buildReturnIndex(index);
    buildLocalAutoIndex(index);
    buildQueryIndex(index);
    buildTryIndex(index);
    buildCollectionSpecializationIndex(index);
    buildArrayExtentIndex(index);
    buildBindingIndex(index);
    return index;
  }

  void buildOnErrorIndex(SemanticProductIndex &index) const {
    populateSemanticFactIndex(index.onErrorFactsByDefinitionId,
                              semanticProgram->publishedRoutingLookups.onErrorFactIndicesByDefinitionId,
                              semanticProgram->onErrorFacts);
  }

  void buildReturnIndex(SemanticProductIndex &index) const {
    populateSemanticFactIndex(
        index.returnFactsByDefinitionId,
        semanticProgram->publishedRoutingLookups.returnFactIndicesByDefinitionId,
        semanticProgram->returnFacts);
  }

  void buildLocalAutoIndex(SemanticProductIndex &index) const {
    populateSemanticFactIndex(index.localAutoFactsByExpr,
                              semanticProgram->publishedRoutingLookups.localAutoFactIndicesByExpr,
                              semanticProgram->localAutoFacts);
  }

  void buildQueryIndex(SemanticProductIndex &index) const {
    populateSemanticFactIndex(index.queryFactsByExpr,
                              semanticProgram->publishedRoutingLookups.queryFactIndicesByExpr,
                              semanticProgram->queryFacts);
  }

  void buildTryIndex(SemanticProductIndex &index) const {
    populateSemanticFactIndex(index.tryFactsByExpr,
                              semanticProgram->publishedRoutingLookups.tryFactIndicesByExpr,
                              semanticProgram->tryFacts);
  }

  void buildBindingIndex(SemanticProductIndex &index) const {
    populateSemanticFactIndex(index.bindingFactsByExpr,
                              semanticProgram->publishedRoutingLookups.bindingFactIndicesByExpr,
                              semanticProgram->bindingFacts);
  }

  void buildCollectionSpecializationIndex(SemanticProductIndex &index) const {
    populateSemanticFactIndex(
        index.collectionSpecializationsByExpr,
        semanticProgram->publishedRoutingLookups.collectionSpecializationIndicesByExpr,
        semanticProgram->collectionSpecializations);
  }

  void buildArrayExtentIndex(SemanticProductIndex &index) const {
    populateSemanticFactIndex(index.arrayExtentFactsByExpr,
                              semanticProgram->publishedRoutingLookups.arrayExtentFactIndicesByExpr,
                              semanticProgram->arrayExtentFacts);
  }
};

} // namespace

SemanticProductIndex buildSemanticProductIndex(const SemanticProgram *semanticProgram) {
  return SemanticProductIndexBuilder{semanticProgram}.build();
}

SemanticProductTargetAdapter buildSemanticProductTargetAdapter(const SemanticProgram *semanticProgram) {
  SemanticProductTargetAdapter adapter;
  if (semanticProgram == nullptr) {
    return adapter;
  }
  adapter.hasSemanticProduct = true;
  adapter.semanticProgram = semanticProgram;
  adapter.publishedRoutingLookups = &semanticProgram->publishedRoutingLookups;
  adapter.semanticIndex = buildSemanticProductIndex(semanticProgram);

  return adapter;
}

std::string describeSyntaxCallSite(const SyntaxCallSiteProvenance &syntax) {
  const std::string scopePath =
      syntax.scopePath.empty() ? std::string("<unknown>") : std::string(syntax.scopePath);
  if (syntax.expr == nullptr) {
    return scopePath + " -> <missing>";
  }

  const Expr &expr = *syntax.expr;
  std::string displayName;
  if (!expr.name.empty() && expr.name.front() == '/') {
    displayName = expr.name;
  } else if (!expr.namespacePrefix.empty()) {
    displayName = expr.namespacePrefix;
    if (!displayName.empty() && displayName.front() != '/') {
      displayName.insert(displayName.begin(), '/');
    }
    displayName += "/" + expr.name;
  } else if (!expr.name.empty()) {
    displayName = expr.name;
  } else {
    displayName = "<unnamed>";
  }
  return scopePath + " -> " + displayName;
}

bool lookupSemanticProductCallTarget(const SemanticProductMeaningContext &meaning,
                                     const Expr &expr,
                                     SemanticProductCallTargetKind kind,
                                     SemanticProductCallTarget &targetOut) {
  targetOut = {};
  if (meaning.targets == nullptr || meaning.targets->semanticProgram == nullptr) {
    return false;
  }

  switch (kind) {
    case SemanticProductCallTargetKind::DirectCall:
      targetOut.resolvedPath =
          findSemanticProductDirectCallTarget(*meaning.targets, expr);
      targetOut.stdlibSurfaceId =
          findSemanticProductDirectCallStdlibSurfaceId(*meaning.targets, expr);
      break;
    case SemanticProductCallTargetKind::MethodCall:
      targetOut.resolvedPath =
          findSemanticProductMethodCallTarget(*meaning.targets, expr);
      targetOut.stdlibSurfaceId =
          findSemanticProductMethodCallStdlibSurfaceId(*meaning.targets, expr);
      break;
    case SemanticProductCallTargetKind::BridgePathChoice:
      targetOut.resolvedPath =
          findSemanticProductBridgePathChoice(*meaning.targets, expr);
      targetOut.stdlibSurfaceId =
          findSemanticProductBridgePathChoiceStdlibSurfaceId(*meaning.targets, expr);
      break;
  }

  return !targetOut.resolvedPath.empty();
}

bool requireSemanticProductCallTarget(const SemanticProductCallTargetContext &context,
                                      SemanticProductCallTargetKind kind,
                                      SemanticProductCallTarget &targetOut,
                                      std::string &errorOut) {
  if (context.syntax.expr != nullptr &&
      lookupSemanticProductCallTarget(context.meaning, *context.syntax.expr, kind, targetOut)) {
    return true;
  }

  targetOut = {};
  std::string family;
  switch (kind) {
    case SemanticProductCallTargetKind::DirectCall:
      family = "direct-call target";
      break;
    case SemanticProductCallTargetKind::MethodCall:
      family = "method-call target";
      break;
    case SemanticProductCallTargetKind::BridgePathChoice:
      family = "bridge-path choice";
      break;
  }
  errorOut = "missing semantic-product " + family + ": " +
             describeSyntaxCallSite(context.syntax);
  return false;
}

std::string findSemanticProductDirectCallTarget(const SemanticProgram *semanticProgram, const Expr &expr) {
  if (semanticProgram == nullptr) {
    return {};
  }
  auto resolvePathId = [&](SymbolId pathId) -> std::string {
    if (pathId == InvalidSymbolId) {
      return {};
    }
    const std::string_view resolvedPath =
        semanticProgramResolveCallTargetString(*semanticProgram, pathId);
    if (resolvedPath.empty()) {
      return {};
    }
    return std::string(resolvedPath);
  };
  if (expr.semanticNodeId != 0) {
    if (const auto pathId = semanticProgramLookupPublishedDirectCallTargetId(*semanticProgram,
                                                                             expr.semanticNodeId);
        pathId.has_value()) {
      return resolvePathId(*pathId);
    }
  }
  return {};
}

std::string findSemanticProductDirectCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr) {
  return findSemanticProductDirectCallTarget(adapter.semanticProgram, expr);
}

std::optional<StdlibSurfaceId> findSemanticProductDirectCallStdlibSurfaceId(
    const SemanticProgram *semanticProgram,
    const Expr &expr) {
  if (semanticProgram == nullptr) {
    return std::nullopt;
  }
  if (expr.semanticNodeId != 0) {
    if (const auto surfaceId = semanticProgramLookupPublishedDirectCallTargetStdlibSurfaceId(
            *semanticProgram, expr.semanticNodeId);
        surfaceId.has_value()) {
      return surfaceId;
    }
  }
  return std::nullopt;
}

std::optional<StdlibSurfaceId> findSemanticProductDirectCallStdlibSurfaceId(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr) {
  return findSemanticProductDirectCallStdlibSurfaceId(adapter.semanticProgram, expr);
}

std::string findSemanticProductMethodCallTarget(const SemanticProgram *semanticProgram, const Expr &expr) {
  if (semanticProgram == nullptr) {
    return {};
  }
  auto resolvePathId = [&](SymbolId pathId) -> std::string {
    if (pathId == InvalidSymbolId) {
      return {};
    }
    const std::string_view resolvedPath =
        semanticProgramResolveCallTargetString(*semanticProgram, pathId);
    if (resolvedPath.empty()) {
      return {};
    }
    return std::string(resolvedPath);
  };
  if (expr.semanticNodeId != 0) {
    if (const auto pathId = semanticProgramLookupPublishedMethodCallTargetId(*semanticProgram,
                                                                             expr.semanticNodeId);
        pathId.has_value()) {
      return resolvePathId(*pathId);
    }
  }
  return {};
}

std::string findSemanticProductMethodCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr) {
  return findSemanticProductMethodCallTarget(adapter.semanticProgram, expr);
}

std::optional<StdlibSurfaceId> findSemanticProductMethodCallStdlibSurfaceId(
    const SemanticProgram *semanticProgram,
    const Expr &expr) {
  if (semanticProgram == nullptr) {
    return std::nullopt;
  }
  if (expr.semanticNodeId != 0) {
    if (const auto surfaceId = semanticProgramLookupPublishedMethodCallTargetStdlibSurfaceId(
            *semanticProgram, expr.semanticNodeId);
        surfaceId.has_value()) {
      return surfaceId;
    }
  }
  return std::nullopt;
}

std::optional<StdlibSurfaceId> findSemanticProductMethodCallStdlibSurfaceId(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr) {
  return findSemanticProductMethodCallStdlibSurfaceId(adapter.semanticProgram, expr);
}

std::string findSemanticProductBridgePathChoice(const SemanticProgram *semanticProgram, const Expr &expr) {
  if (semanticProgram == nullptr || expr.semanticNodeId == 0) {
    return {};
  }
  if (const auto pathId = semanticProgramLookupPublishedBridgePathChoiceId(*semanticProgram,
                                                                           expr.semanticNodeId);
      pathId.has_value() && *pathId != InvalidSymbolId) {
    const std::string_view chosenPath =
        semanticProgramResolveCallTargetString(*semanticProgram, *pathId);
    if (!chosenPath.empty()) {
      return std::string(chosenPath);
    }
  }
  return {};
}

std::string findSemanticProductBridgePathChoice(const SemanticProductTargetAdapter &adapter, const Expr &expr) {
  return findSemanticProductBridgePathChoice(adapter.semanticProgram, expr);
}

std::optional<StdlibSurfaceId> findSemanticProductBridgePathChoiceStdlibSurfaceId(
    const SemanticProgram *semanticProgram,
    const Expr &expr) {
  if (semanticProgram == nullptr || expr.semanticNodeId == 0) {
    return std::nullopt;
  }
  if (const auto surfaceId = semanticProgramLookupPublishedBridgePathChoiceStdlibSurfaceId(
          *semanticProgram, expr.semanticNodeId);
      surfaceId.has_value()) {
    return surfaceId;
  }
  return std::nullopt;
}

std::optional<StdlibSurfaceId> findSemanticProductBridgePathChoiceStdlibSurfaceId(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr) {
  return findSemanticProductBridgePathChoiceStdlibSurfaceId(adapter.semanticProgram, expr);
}

const SemanticProgramCallableSummary *findSemanticProductCallableSummary(const SemanticProgram *semanticProgram,
                                                                        const std::string &fullPath) {
  if (fullPath.empty() || semanticProgram == nullptr) {
    return nullptr;
  }
  const auto fullPathId =
      semanticProgramLookupCallTargetStringId(*semanticProgram, fullPath);
  if (!fullPathId.has_value()) {
    return nullptr;
  }
  const auto &indicesByPath =
      semanticProgram->publishedRoutingLookups.callableSummaryIndicesByPathId;
  if (const auto it = indicesByPath.find(*fullPathId);
      it != indicesByPath.end() && it->second < semanticProgram->callableSummaries.size()) {
    return &semanticProgram->callableSummaries[it->second];
  }
  return nullptr;
}

const SemanticProgramCallableSummary *findSemanticProductCallableSummary(const SemanticProductTargetAdapter &adapter,
                                                                        const std::string &fullPath) {
  return findSemanticProductCallableSummary(adapter.semanticProgram, fullPath);
}

const SemanticProgramOnErrorFact *findSemanticProductOnErrorFactBySemanticId(
    const SemanticProductIndex &semanticIndex,
    const Definition &definition) {
  return findDefinitionScopedSemanticFact(semanticIndex.onErrorFactsByDefinitionId, definition);
}

const SemanticProgramOnErrorFact *findSemanticProductOnErrorFactBySemanticId(
    const SemanticProductTargetAdapter &adapter,
    const Definition &definition) {
  return findSemanticProductOnErrorFactBySemanticId(adapter.semanticIndex, definition);
}

const SemanticProgramOnErrorFact *findSemanticProductOnErrorFact(
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex &semanticIndex,
    const Definition &definition) {
  if (semanticProgram != nullptr) {
    if (const auto *fact = semanticProgramLookupPublishedOnErrorFactByDefinitionSemanticId(
            *semanticProgram, definition.semanticNodeId);
        fact != nullptr) {
      return fact;
    }
  }
  return findSemanticProductOnErrorFactBySemanticId(semanticIndex, definition);
}

const SemanticProgramOnErrorFact *findSemanticProductOnErrorFact(const SemanticProductTargetAdapter &adapter,
                                                                const Definition &definition) {
  return findSemanticProductOnErrorFact(adapter.semanticProgram, adapter.semanticIndex, definition);
}

const SemanticProgramReturnFact *findSemanticProductReturnFactBySemanticId(
    const SemanticProductIndex &semanticIndex,
    const Definition &definition) {
  return findDefinitionScopedSemanticFact(semanticIndex.returnFactsByDefinitionId, definition);
}

const SemanticProgramReturnFact *findSemanticProductReturnFactByPath(
    const SemanticProductTargetAdapter &adapter,
    std::string_view definitionPath) {
  if (adapter.semanticProgram == nullptr || definitionPath.empty()) {
    return nullptr;
  }
  const auto definitionPathId =
      semanticProgramLookupCallTargetStringId(*adapter.semanticProgram, definitionPath);
  if (!definitionPathId.has_value()) {
    return nullptr;
  }
  return semanticProgramLookupPublishedReturnFactByDefinitionPathId(
      *adapter.semanticProgram, *definitionPathId);
}

const SemanticProgramReturnFact *findSemanticProductReturnFact(
    const SemanticProgram *,
    const SemanticProductIndex &semanticIndex,
    const Definition &definition) {
  return findSemanticProductReturnFactBySemanticId(semanticIndex, definition);
}

const SemanticProgramReturnFact *findSemanticProductReturnFact(const SemanticProductTargetAdapter &adapter,
                                                              const Definition &definition) {
  return findSemanticProductReturnFact(adapter.semanticProgram, adapter.semanticIndex, definition);
}

const SemanticProgramLocalAutoFact *findSemanticProductLocalAutoFactBySemanticId(
    const SemanticProductIndex &semanticIndex,
    const Expr &expr) {
  return findExpressionScopedSemanticFact(semanticIndex.localAutoFactsByExpr, expr);
}

const SemanticProgramLocalAutoFact *findSemanticProductLocalAutoFactBySemanticId(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr) {
  return findSemanticProductLocalAutoFactBySemanticId(adapter.semanticIndex, expr);
}

const SemanticProgramLocalAutoFact *findSemanticProductLocalAutoFact(
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex &semanticIndex,
    const Expr &expr) {
  if (semanticProgram != nullptr) {
    if (const auto *fact = semanticProgramLookupPublishedLocalAutoFactBySemanticId(
            *semanticProgram, expr.semanticNodeId);
        fact != nullptr) {
      return fact;
    }
  }
  return findSemanticProductLocalAutoFactBySemanticId(semanticIndex, expr);
}

const SemanticProgramLocalAutoFact *findSemanticProductLocalAutoFact(const SemanticProductTargetAdapter &adapter,
                                                                    const Expr &expr) {
  return findSemanticProductLocalAutoFact(adapter.semanticProgram, adapter.semanticIndex, expr);
}

const SemanticProgramQueryFact *findSemanticProductQueryFactBySemanticId(
    const SemanticProductIndex &semanticIndex,
    const Expr &expr) {
  return findExpressionScopedSemanticFact(semanticIndex.queryFactsByExpr, expr);
}

const SemanticProgramQueryFact *findSemanticProductQueryFactBySemanticId(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr) {
  return findSemanticProductQueryFactBySemanticId(adapter.semanticIndex, expr);
}

const SemanticProgramQueryFact *findSemanticProductQueryFact(
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex &semanticIndex,
    const Expr &expr) {
  if (semanticProgram != nullptr) {
    if (const auto *fact = semanticProgramLookupPublishedQueryFactBySemanticId(
            *semanticProgram, expr.semanticNodeId);
        fact != nullptr) {
      return fact;
    }
  }
  return findSemanticProductQueryFactBySemanticId(semanticIndex, expr);
}

const SemanticProgramQueryFact *findSemanticProductQueryFact(const SemanticProductTargetAdapter &adapter,
                                                            const Expr &expr) {
  return findSemanticProductQueryFact(adapter.semanticProgram, adapter.semanticIndex, expr);
}

const SemanticProgramTryFact *findSemanticProductTryFactBySemanticId(
    const SemanticProductIndex &semanticIndex,
    const Expr &expr) {
  return findExpressionScopedSemanticFact(semanticIndex.tryFactsByExpr, expr);
}

const SemanticProgramTryFact *findSemanticProductTryFactBySemanticId(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr) {
  return findSemanticProductTryFactBySemanticId(adapter.semanticIndex, expr);
}

const SemanticProgramTryFact *findSemanticProductTryFact(
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex &semanticIndex,
    const Expr &expr) {
  if (semanticProgram != nullptr) {
    if (const auto *fact = semanticProgramLookupPublishedTryFactBySemanticId(
            *semanticProgram, expr.semanticNodeId);
        fact != nullptr) {
      return fact;
    }
  }
  return findSemanticProductTryFactBySemanticId(semanticIndex, expr);
}

const SemanticProgramTryFact *findSemanticProductTryFact(const SemanticProductTargetAdapter &adapter,
                                                        const Expr &expr) {
  return findSemanticProductTryFact(adapter.semanticProgram, adapter.semanticIndex, expr);
}

const SemanticProgramBindingFact *findSemanticProductBindingFact(const SemanticProductIndex &semanticIndex,
                                                                const Expr &expr) {
  if (const auto *fact = findExpressionScopedSemanticFact(semanticIndex.bindingFactsByExpr, expr);
      fact != nullptr) {
    return fact;
  }
  return nullptr;
}

const SemanticProgramBindingFact *findSemanticProductBindingFact(const SemanticProductTargetAdapter &adapter,
                                                                const Expr &expr) {
  return findSemanticProductBindingFact(adapter.semanticIndex, expr);
}

const SemanticProgramSumTypeMetadata *findSemanticProductSumTypeMetadata(
    const SemanticProductTargetAdapter &adapter,
    std::string_view fullPath) {
  if (adapter.semanticProgram == nullptr || fullPath.empty()) {
    return nullptr;
  }
  const auto fullPathId =
      semanticProgramLookupCallTargetStringId(*adapter.semanticProgram, fullPath);
  if (!fullPathId.has_value()) {
    return nullptr;
  }
  return semanticProgramLookupPublishedSumTypeMetadataByPathId(
      *adapter.semanticProgram, *fullPathId);
}

const SemanticProgramSumVariantMetadata *findSemanticProductSumVariantMetadata(
    const SemanticProductTargetAdapter &adapter,
    std::string_view sumPath,
    std::string_view variantName) {
  if (adapter.semanticProgram == nullptr || sumPath.empty() || variantName.empty()) {
    return nullptr;
  }
  const auto sumPathId =
      semanticProgramLookupCallTargetStringId(*adapter.semanticProgram, sumPath);
  const auto variantNameId =
      semanticProgramLookupCallTargetStringId(*adapter.semanticProgram, variantName);
  if (!sumPathId.has_value() || !variantNameId.has_value()) {
    return nullptr;
  }
  return semanticProgramLookupPublishedSumVariantMetadataBySumPathAndVariantNameId(
      *adapter.semanticProgram, *sumPathId, *variantNameId);
}

const SemanticProgramCollectionSpecialization *findSemanticProductCollectionSpecialization(
    const SemanticProductIndex &semanticIndex,
    const Expr &expr) {
  return findExpressionScopedSemanticFact(semanticIndex.collectionSpecializationsByExpr, expr);
}

const SemanticProgramCollectionSpecialization *findSemanticProductCollectionSpecialization(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr) {
  return findSemanticProductCollectionSpecialization(adapter.semanticIndex, expr);
}

const SemanticProgramArrayExtentFact *findSemanticProductArrayExtentFact(
    const SemanticProductIndex &semanticIndex,
    const Expr &expr) {
  return findExpressionScopedSemanticFact(semanticIndex.arrayExtentFactsByExpr, expr);
}

const SemanticProgramArrayExtentFact *findSemanticProductArrayExtentFact(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr) {
  return findSemanticProductArrayExtentFact(adapter.semanticIndex, expr);
}

bool semanticKeyValueAccessHelperKeepsBuiltinReturn(
    const SemanticProgram *semanticProgram,
    std::string_view helperPath) {
  if (semanticProgram == nullptr || helperPath.empty()) {
    return true;
  }
  const auto pathId =
      semanticProgramLookupCallTargetStringId(*semanticProgram, helperPath);
  if (!pathId.has_value()) {
    return true;
  }
  const SemanticProgramReturnFact *returnFact =
      semanticProgramLookupPublishedReturnFactByDefinitionPathId(
          *semanticProgram, *pathId);
  if (returnFact == nullptr) {
    return true;
  }
  std::string structPath =
      returnFact->structPathId != InvalidSymbolId
          ? std::string(semanticProgramResolveCallTargetString(
                *semanticProgram, returnFact->structPathId))
          : returnFact->structPath;
  structPath = trimTemplateTypeText(structPath);
  if (!structPath.empty() && structPath.front() == '/') {
    structPath.erase(structPath.begin());
  }
  if (structPath.empty()) {
    return true;
  }
  return structPath == "map" || structPath == "vector" ||
         structPath == "array";
}

} // namespace primec::ir_lowerer
