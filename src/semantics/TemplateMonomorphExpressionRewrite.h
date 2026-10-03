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

bool isCompileTimeTypeofPredicateArg(const std::string &arg);

bool rewriteCompileTimePredicateExpr(Expr &expr,
                                     const SubstMap &mapping,
                                     const std::unordered_set<std::string> &allowedParams,
                                     const std::string &namespacePrefix,
                                     Context &ctx,
                                     std::string &error);

bool rewriteExpr(Expr &expr,
                 const SubstMap &mapping,
                 const std::unordered_set<std::string> &allowedParams,
                 const std::string &namespacePrefix,
                 Context &ctx,
                 std::string &error,
                 const LocalTypeMap &locals,
                 const std::vector<semantics::ParameterInfo> &params,
                 bool allowMathBare);

} // namespace primec
