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

bool resolveExperimentalConstructorTargetTypeText(const Expr &targetExpr,
                                                  const std::vector<ParameterInfo> &params,
                                                  const LocalTypeMap &locals,
                                                  bool allowMathBare,
                                                  const std::string &namespacePrefix,
                                                  Context &ctx,
                                                  std::string &targetTypeTextOut);

template <typename RewriteTargetValueFn>
void rewriteExperimentalAssignTargetValue(Expr &callExpr,
                                          const std::vector<ParameterInfo> &params,
                                          const LocalTypeMap &locals,
                                          bool allowMathBare,
                                          const std::string &namespacePrefix,
                                          Context &ctx,
                                          RewriteTargetValueFn &&rewriteTargetValueForType) {
  if (!isAssignCall(callExpr) || callExpr.args.size() != 2) {
    return;
  }
  std::string targetTypeText;
  if (!resolveExperimentalConstructorTargetTypeText(
          callExpr.args.front(), params, locals, allowMathBare, namespacePrefix, ctx, targetTypeText)) {
    return;
  }
  (void)rewriteTargetValueForType(targetTypeText, callExpr.args[1]);
}

template <typename RewriteTargetValueFn>
void rewriteExperimentalInitTargetValue(Expr &callExpr,
                                        const std::vector<ParameterInfo> &params,
                                        const LocalTypeMap &locals,
                                        bool allowMathBare,
                                        const std::string &namespacePrefix,
                                        Context &ctx,
                                        RewriteTargetValueFn &&rewriteTargetValueForType) {
  if (!isSimpleCallName(callExpr, "init") || callExpr.args.size() != 2) {
    return;
  }
  std::string targetTypeText;
  if (!resolveExperimentalConstructorTargetTypeText(
          callExpr.args.front(), params, locals, allowMathBare, namespacePrefix, ctx, targetTypeText)) {
    return;
  }
  (void)rewriteTargetValueForType(targetTypeText, callExpr.args[1]);
}

} // namespace primec
