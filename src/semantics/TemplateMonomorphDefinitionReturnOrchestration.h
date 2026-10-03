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
#include "TemplateMonomorphUsings.h"

namespace primec {

bool isDefinitionReturnPathStatement(const Expr &stmt,
                                     size_t stmtIndex,
                                     const DefinitionReturnStatementSelection &selection);

template <typename RewriteVectorFn, typename RewriteKeyValueFn>
bool rewriteDefinitionReturnConstructors(Expr &expr,
                                         const ExperimentalCollectionReturnRewritePlan &plan,
                                         RewriteVectorFn &&rewriteVectorReturn,
                                         RewriteKeyValueFn &&rewriteKeyValueReturn,
                                         std::string &error) {
  if (plan.expectedCollectionVectorReturn) {
    rewriteVectorReturn(expr);
    if (!error.empty()) {
      return false;
    }
  }
  if (plan.expectedExperimentalKeyValueReturn) {
    rewriteKeyValueReturn(expr);
    if (!error.empty()) {
      return false;
    }
  }
  return true;
}

} // namespace primec
