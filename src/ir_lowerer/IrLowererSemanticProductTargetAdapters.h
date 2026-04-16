#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "primec/Ast.h"
#include "primec/SemanticProduct.h"
#include "primec/SymbolInterner.h"

namespace primec::ir_lowerer {

struct SemanticProductIndex {
  std::unordered_map<uint64_t, SymbolId> directCallTargetIdsByExpr;
  std::unordered_map<uint64_t, SymbolId> methodCallTargetIdsByExpr;
  std::unordered_map<uint64_t, SymbolId> bridgePathChoiceIdsByExpr;
  std::unordered_map<uint64_t, const SemanticProgramOnErrorFact *> onErrorFactsByDefinitionId;
  std::unordered_map<SymbolId, const SemanticProgramOnErrorFact *> onErrorFactsByDefinitionPathId;
  std::unordered_map<uint64_t, const SemanticProgramLocalAutoFact *> localAutoFactsByExpr;
  std::unordered_map<uint64_t, const SemanticProgramLocalAutoFact *> localAutoFactsByInitPathAndBindingNameId;
  std::unordered_map<uint64_t, const SemanticProgramQueryFact *> queryFactsByExpr;
  std::unordered_map<uint64_t, const SemanticProgramQueryFact *> queryFactsByResolvedPathAndCallNameId;
  std::unordered_map<uint64_t, const SemanticProgramTryFact *> tryFactsByExpr;
  std::unordered_map<uint64_t, const SemanticProgramTryFact *> tryFactsByOperandPathAndSource;
  std::unordered_map<uint64_t, const SemanticProgramBindingFact *> bindingFactsByExpr;
};

struct SemanticProductTargetAdapter {
  bool hasSemanticProduct = false;
  const SemanticProgram *semanticProgram = nullptr;
  SemanticProductIndex semanticIndex;
  std::unordered_map<SymbolId, const SemanticProgramCallableSummary *> callableSummariesByPathId;
  std::unordered_map<std::string_view, const SemanticProgramTypeMetadata *> typeMetadataByPath;
  std::vector<const SemanticProgramTypeMetadata *> orderedStructTypeMetadata;
  std::unordered_map<std::string_view, std::vector<const SemanticProgramStructFieldMetadata *>>
      structFieldMetadataByStructPath;
  std::unordered_map<uint64_t, const SemanticProgramReturnFact *> returnFactsByDefinitionId;
  std::unordered_map<SymbolId, const SemanticProgramReturnFact *> returnFactsByDefinitionPathId;
};

SemanticProductTargetAdapter buildSemanticProductTargetAdapter(const SemanticProgram *semanticProgram);

std::string findSemanticProductDirectCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr);
std::string findSemanticProductMethodCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr);
std::string findSemanticProductBridgePathChoice(const SemanticProductTargetAdapter &adapter, const Expr &expr);
const SemanticProgramCallableSummary *findSemanticProductCallableSummary(
    const SemanticProductTargetAdapter &adapter,
    const std::string &fullPath);
const SemanticProgramOnErrorFact *findSemanticProductOnErrorFact(const SemanticProductTargetAdapter &adapter,
                                                                const Definition &definition);
const SemanticProgramTypeMetadata *findSemanticProductTypeMetadata(const SemanticProductTargetAdapter &adapter,
                                                                  const std::string &fullPath);
const std::vector<const SemanticProgramStructFieldMetadata *> *findSemanticProductStructFieldMetadata(
    const SemanticProductTargetAdapter &adapter,
    const std::string &structPath);
const SemanticProgramReturnFact *findSemanticProductReturnFact(const SemanticProductTargetAdapter &adapter,
                                                              const Definition &definition);
const SemanticProgramLocalAutoFact *findSemanticProductLocalAutoFact(const SemanticProductTargetAdapter &adapter,
                                                                    const Expr &expr);
const SemanticProgramQueryFact *findSemanticProductQueryFact(const SemanticProductTargetAdapter &adapter,
                                                            const Expr &expr);
const SemanticProgramTryFact *findSemanticProductTryFact(const SemanticProductTargetAdapter &adapter,
                                                        const Expr &expr);
const SemanticProgramBindingFact *findSemanticProductBindingFact(const SemanticProductTargetAdapter &adapter,
                                                                const Expr &expr);

} // namespace primec::ir_lowerer
