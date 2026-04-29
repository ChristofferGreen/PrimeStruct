#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "primec/Ast.h"
#include "primec/SemanticProduct.h"
#include "primec/SymbolInterner.h"

namespace primec::ir_lowerer {

struct SemanticProductIndex {
  std::unordered_map<uint64_t, const SemanticProgramOnErrorFact *> onErrorFactsByDefinitionId;
  std::unordered_map<SymbolId, const SemanticProgramOnErrorFact *> onErrorFactsByDefinitionPathId;
  std::unordered_map<uint64_t, const SemanticProgramReturnFact *> returnFactsByDefinitionId;
  std::unordered_map<SymbolId, const SemanticProgramReturnFact *> returnFactsByDefinitionPathId;
  std::unordered_map<uint64_t, const SemanticProgramLocalAutoFact *> localAutoFactsByExpr;
  std::unordered_map<uint64_t, const SemanticProgramLocalAutoFact *> localAutoFactsByInitPathAndBindingNameId;
  std::unordered_map<uint64_t, const SemanticProgramQueryFact *> queryFactsByExpr;
  std::unordered_map<uint64_t, const SemanticProgramQueryFact *> queryFactsByResolvedPathAndCallNameId;
  std::unordered_map<uint64_t, const SemanticProgramTryFact *> tryFactsByExpr;
  std::unordered_map<uint64_t, const SemanticProgramTryFact *> tryFactsByOperandPathAndSource;
  std::unordered_map<uint64_t, const SemanticProgramCollectionSpecialization *> collectionSpecializationsByExpr;
  std::unordered_map<uint64_t, const SemanticProgramBindingFact *> bindingFactsByExpr;
};

struct SemanticProductTargetAdapter {
  bool hasSemanticProduct = false;
  const SemanticProgram *semanticProgram = nullptr;
  const SemanticProgramPublishedRoutingLookups *publishedRoutingLookups = nullptr;
  SemanticProductIndex semanticIndex;
};

SemanticProductIndex buildSemanticProductIndex(const SemanticProgram *semanticProgram);
SemanticProductTargetAdapter buildSemanticProductTargetAdapter(const SemanticProgram *semanticProgram);

std::string findSemanticProductDirectCallTarget(const SemanticProgram *semanticProgram, const Expr &expr);
std::string findSemanticProductMethodCallTarget(const SemanticProgram *semanticProgram, const Expr &expr);
std::string findSemanticProductBridgePathChoice(const SemanticProgram *semanticProgram, const Expr &expr);
std::optional<StdlibSurfaceId> findSemanticProductDirectCallStdlibSurfaceId(
    const SemanticProgram *semanticProgram,
    const Expr &expr);
std::optional<StdlibSurfaceId> findSemanticProductMethodCallStdlibSurfaceId(
    const SemanticProgram *semanticProgram,
    const Expr &expr);
std::optional<StdlibSurfaceId> findSemanticProductBridgePathChoiceStdlibSurfaceId(
    const SemanticProgram *semanticProgram,
    const Expr &expr);
const SemanticProgramCallableSummary *findSemanticProductCallableSummary(
    const SemanticProgram *semanticProgram,
    const std::string &fullPath);
std::string findSemanticProductDirectCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr);
std::string findSemanticProductMethodCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr);
std::string findSemanticProductBridgePathChoice(const SemanticProductTargetAdapter &adapter, const Expr &expr);
std::optional<StdlibSurfaceId> findSemanticProductDirectCallStdlibSurfaceId(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr);
std::optional<StdlibSurfaceId> findSemanticProductMethodCallStdlibSurfaceId(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr);
std::optional<StdlibSurfaceId> findSemanticProductBridgePathChoiceStdlibSurfaceId(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr);
const SemanticProgramCallableSummary *findSemanticProductCallableSummary(
    const SemanticProductTargetAdapter &adapter,
    const std::string &fullPath);
const SemanticProgramOnErrorFact *findSemanticProductOnErrorFactBySemanticId(
    const SemanticProductIndex &semanticIndex,
    const Definition &definition);
const SemanticProgramOnErrorFact *findSemanticProductOnErrorFactBySemanticId(
    const SemanticProductTargetAdapter &adapter,
    const Definition &definition);
const SemanticProgramOnErrorFact *findSemanticProductOnErrorFact(
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex &semanticIndex,
    const Definition &definition);
const SemanticProgramOnErrorFact *findSemanticProductOnErrorFact(const SemanticProductTargetAdapter &adapter,
                                                                const Definition &definition);
const SemanticProgramReturnFact *findSemanticProductReturnFactBySemanticId(
    const SemanticProductIndex &semanticIndex,
    const Definition &definition);
const SemanticProgramReturnFact *findSemanticProductReturnFactByPath(
    const SemanticProductTargetAdapter &adapter,
    std::string_view definitionPath);
const SemanticProgramReturnFact *findSemanticProductReturnFact(
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex &semanticIndex,
    const Definition &definition);
const SemanticProgramReturnFact *findSemanticProductReturnFact(const SemanticProductTargetAdapter &adapter,
                                                              const Definition &definition);
const SemanticProgramLocalAutoFact *findSemanticProductLocalAutoFactBySemanticId(
    const SemanticProductIndex &semanticIndex,
    const Expr &expr);
const SemanticProgramLocalAutoFact *findSemanticProductLocalAutoFactBySemanticId(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr);
const SemanticProgramLocalAutoFact *findSemanticProductLocalAutoFact(
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex &semanticIndex,
    const Expr &expr);
const SemanticProgramLocalAutoFact *findSemanticProductLocalAutoFact(const SemanticProductTargetAdapter &adapter,
                                                                    const Expr &expr);
const SemanticProgramQueryFact *findSemanticProductQueryFactBySemanticId(
    const SemanticProductIndex &semanticIndex,
    const Expr &expr);
const SemanticProgramQueryFact *findSemanticProductQueryFactBySemanticId(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr);
const SemanticProgramQueryFact *findSemanticProductQueryFact(
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex &semanticIndex,
    const Expr &expr);
const SemanticProgramQueryFact *findSemanticProductQueryFact(const SemanticProductTargetAdapter &adapter,
                                                            const Expr &expr);
const SemanticProgramTryFact *findSemanticProductTryFactBySemanticId(
    const SemanticProductIndex &semanticIndex,
    const Expr &expr);
const SemanticProgramTryFact *findSemanticProductTryFactBySemanticId(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr);
const SemanticProgramTryFact *findSemanticProductTryFact(
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex &semanticIndex,
    const Expr &expr);
const SemanticProgramTryFact *findSemanticProductTryFact(const SemanticProductTargetAdapter &adapter,
                                                        const Expr &expr);
const SemanticProgramBindingFact *findSemanticProductBindingFact(const SemanticProductIndex &semanticIndex,
                                                                const Expr &expr);
const SemanticProgramBindingFact *findSemanticProductBindingFact(const SemanticProductTargetAdapter &adapter,
                                                                const Expr &expr);
const SemanticProgramBindingFact *findSemanticProductBindingFactByScopeAndName(
    const SemanticProductTargetAdapter &adapter,
    std::string_view scopePath,
    std::string_view name);
const SemanticProgramSumTypeMetadata *findSemanticProductSumTypeMetadata(
    const SemanticProductTargetAdapter &adapter,
    std::string_view fullPath);
const SemanticProgramSumVariantMetadata *findSemanticProductSumVariantMetadata(
    const SemanticProductTargetAdapter &adapter,
    std::string_view sumPath,
    std::string_view variantName);
const SemanticProgramCollectionSpecialization *findSemanticProductCollectionSpecialization(
    const SemanticProductIndex &semanticIndex,
    const Expr &expr);
const SemanticProgramCollectionSpecialization *findSemanticProductCollectionSpecialization(
    const SemanticProductTargetAdapter &adapter,
    const Expr &expr);

} // namespace primec::ir_lowerer
