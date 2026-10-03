#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "TemplateMonomorphContext.h"
#include "primec/ast/Ast.h"
#include "SemanticsHelpers.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "TemplateMonomorphExperimentalCollectionReturnSetup.h"

namespace primec {

struct ExperimentalCollectionReturnRewritePlan {
  bool hasExplicitNonAutoReturn = false;
  bool expectedCollectionVectorReturn = false;
  bool expectedExperimentalKeyValueReturn = false;
};

ExperimentalCollectionReturnRewritePlan inferExperimentalCollectionReturnRewritePlan(
    const Definition &def,
    const SubstMap &mapping,
    const std::unordered_set<std::string> &allowedParams,
    bool allowMathBare,
    Context &ctx);

struct DefinitionReturnStatementSelection {
  bool sawExplicitReturn = false;
  size_t implicitReturnStmtIndex = 0;
};

DefinitionReturnStatementSelection determineDefinitionReturnStatementSelection(const Definition &def);

} // namespace primec
