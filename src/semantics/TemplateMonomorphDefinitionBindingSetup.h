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

bool tryAppendDefinitionParameterBinding(Expr &param,
                                         bool allowMathBare,
                                         Context &ctx,
                                         LocalTypeMap &locals,
                                         std::vector<semantics::ParameterInfo> &paramsOut);

bool rewriteDefinitionParameters(std::vector<Expr> &parameters,
                                 const SubstMap &mapping,
                                 const std::unordered_set<std::string> &allowedParams,
                                 const std::string &namespacePrefix,
                                 Context &ctx,
                                 std::string &error,
                                 LocalTypeMap &locals,
                                 std::vector<semantics::ParameterInfo> &paramsOut,
                                 bool allowMathBare);

void recordDefinitionStatementBindingLocal(Expr &stmt,
                                           const std::vector<semantics::ParameterInfo> &params,
                                           const LocalTypeMap &locals,
                                           bool allowMathBare,
                                           Context &ctx,
                                           LocalTypeMap &localsOut);

} // namespace primec
