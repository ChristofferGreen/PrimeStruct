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

void rewriteDefinitionExperimentalKeyValueConstructorValue(Expr &valueExpr,
                                                           LocalTypeMap &locals,
                                                           std::vector<semantics::ParameterInfo> &params,
                                                           const SubstMap &mapping,
                                                           const std::unordered_set<std::string> &allowedParams,
                                                           const std::string &namespacePrefix,
                                                           Context &ctx,
                                                           bool allowMathBare,
                                                           std::string &error);

void rewriteDefinitionExperimentalVectorConstructorValue(Expr &valueExpr,
                                                         LocalTypeMap &locals,
                                                         std::vector<semantics::ParameterInfo> &params,
                                                         const SubstMap &mapping,
                                                         const std::unordered_set<std::string> &allowedParams,
                                                         const std::string &namespacePrefix,
                                                         Context &ctx,
                                                         bool allowMathBare,
                                                         std::string &error);

void rewriteDefinitionExperimentalVectorReturnConstructors(Expr &candidate,
                                                           LocalTypeMap &locals,
                                                           std::vector<semantics::ParameterInfo> &params,
                                                           const SubstMap &mapping,
                                                           const std::unordered_set<std::string> &allowedParams,
                                                           const std::string &namespacePrefix,
                                                           Context &ctx,
                                                           bool allowMathBare,
                                                           std::string &error);

void rewriteDefinitionExperimentalKeyValueReturnConstructors(Expr &candidate,
                                                             LocalTypeMap &locals,
                                                             std::vector<semantics::ParameterInfo> &params,
                                                             const SubstMap &mapping,
                                                             const std::unordered_set<std::string> &allowedParams,
                                                             const std::string &namespacePrefix,
                                                             Context &ctx,
                                                             bool allowMathBare,
                                                             std::string &error);

bool rewriteDefinitionExperimentalReturnConstructors(Expr &expr,
                                                     const ExperimentalCollectionReturnRewritePlan &plan,
                                                     LocalTypeMap &locals,
                                                     std::vector<semantics::ParameterInfo> &params,
                                                     const SubstMap &mapping,
                                                     const std::unordered_set<std::string> &allowedParams,
                                                     const std::string &namespacePrefix,
                                                     Context &ctx,
                                                     bool allowMathBare,
                                                     std::string &error);

} // namespace primec
