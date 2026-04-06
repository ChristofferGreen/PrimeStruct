#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "primec/Ast.h"
#include "primec/SemanticProduct.h"

namespace primec::ir_lowerer {

struct SemanticProductTargetAdapter {
  bool hasSemanticProduct = false;
  std::unordered_map<uint64_t, std::string> directCallTargetsByExpr;
  std::unordered_map<uint64_t, std::string> methodCallTargetsByExpr;
  std::unordered_map<uint64_t, std::string> bridgePathChoicesByExpr;
  std::unordered_map<std::string, const SemanticProgramCallableSummary *> callableSummariesByPath;
  std::unordered_map<std::string, const SemanticProgramOnErrorFact *> onErrorFactsByDefinitionPath;
  std::unordered_map<std::string, const SemanticProgramTypeMetadata *> typeMetadataByPath;
  std::vector<const SemanticProgramTypeMetadata *> orderedStructTypeMetadata;
  std::unordered_map<std::string, std::vector<const SemanticProgramStructFieldMetadata *>>
      structFieldMetadataByStructPath;
  std::unordered_map<std::string, const SemanticProgramReturnFact *> returnFactsByDefinitionPath;
  std::unordered_map<uint64_t, const SemanticProgramBindingFact *> bindingFactsByExpr;
};

SemanticProductTargetAdapter buildSemanticProductTargetAdapter(const SemanticProgram *semanticProgram);

std::string findSemanticProductDirectCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr);
std::string findSemanticProductMethodCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr);
std::string findSemanticProductBridgePathChoice(const SemanticProductTargetAdapter &adapter, const Expr &expr);
const SemanticProgramCallableSummary *findSemanticProductCallableSummary(
    const SemanticProductTargetAdapter &adapter,
    const std::string &fullPath);
const SemanticProgramOnErrorFact *findSemanticProductOnErrorFact(const SemanticProductTargetAdapter &adapter,
                                                                const std::string &definitionPath);
const SemanticProgramTypeMetadata *findSemanticProductTypeMetadata(const SemanticProductTargetAdapter &adapter,
                                                                  const std::string &fullPath);
const std::vector<const SemanticProgramStructFieldMetadata *> *findSemanticProductStructFieldMetadata(
    const SemanticProductTargetAdapter &adapter,
    const std::string &structPath);
const SemanticProgramReturnFact *findSemanticProductReturnFact(const SemanticProductTargetAdapter &adapter,
                                                              const std::string &definitionPath);
const SemanticProgramBindingFact *findSemanticProductBindingFact(const SemanticProductTargetAdapter &adapter,
                                                                const Expr &expr);

} // namespace primec::ir_lowerer
