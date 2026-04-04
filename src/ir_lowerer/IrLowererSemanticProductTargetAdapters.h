#pragma once

#include <string>
#include <unordered_map>

#include "primec/Ast.h"
#include "primec/SemanticProduct.h"

namespace primec::ir_lowerer {

struct SemanticProductTargetAdapter {
  std::unordered_map<std::string, std::string> directCallTargetsByExpr;
  std::unordered_map<std::string, std::string> methodCallTargetsByExpr;
  std::unordered_map<std::string, std::string> bridgePathChoicesByExpr;
  std::unordered_map<std::string, const SemanticProgramCallableSummary *> callableSummariesByPath;
  std::unordered_map<std::string, const SemanticProgramReturnFact *> returnFactsByDefinitionPath;
};

SemanticProductTargetAdapter buildSemanticProductTargetAdapter(const SemanticProgram *semanticProgram);

std::string findSemanticProductDirectCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr);
std::string findSemanticProductMethodCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr);
std::string findSemanticProductBridgePathChoice(const SemanticProductTargetAdapter &adapter, const Expr &expr);
const SemanticProgramCallableSummary *findSemanticProductCallableSummary(
    const SemanticProductTargetAdapter &adapter,
    const std::string &fullPath);
const SemanticProgramReturnFact *findSemanticProductReturnFact(const SemanticProductTargetAdapter &adapter,
                                                              const std::string &definitionPath);

} // namespace primec::ir_lowerer
