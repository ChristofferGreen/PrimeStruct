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
};

SemanticProductTargetAdapter buildSemanticProductTargetAdapter(const SemanticProgram *semanticProgram);

std::string findSemanticProductDirectCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr);
std::string findSemanticProductMethodCallTarget(const SemanticProductTargetAdapter &adapter, const Expr &expr);
std::string findSemanticProductBridgePathChoice(const SemanticProductTargetAdapter &adapter, const Expr &expr);

} // namespace primec::ir_lowerer
