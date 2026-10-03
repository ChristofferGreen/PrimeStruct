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

std::string canonicalizeExperimentalCollectionResolvedPath(std::string path);

bool isExperimentalMapEntryArgument(const Expr &argExpr,
                                    const std::vector<semantics::ParameterInfo> &params,
                                    const LocalTypeMap &locals,
                                    bool allowMathBare,
                                    const std::string &namespacePrefix,
                                    Context &ctx);

bool inferExperimentalCollectionConstructorTemplateArgs(const std::string &originalPath,
                                                        const std::string &helperPath,
                                                        Expr &valueExpr,
                                                        const LocalTypeMap &locals,
                                                        const std::vector<semantics::ParameterInfo> &params,
                                                        const SubstMap &mapping,
                                                        const std::unordered_set<std::string> &allowedParams,
                                                        const std::string &namespacePrefix,
                                                        Context &ctx,
                                                        bool allowMathBare,
                                                        std::string &error);

bool isCanonicalMapConstructorRewriteSourcePath(std::string_view originalPath);

bool rewriteCanonicalExperimentalKeyValueConstructorExpr(Expr &valueExpr,
                                                         const LocalTypeMap &locals,
                                                         const std::vector<semantics::ParameterInfo> &params,
                                                         const SubstMap &mapping,
                                                         const std::unordered_set<std::string> &allowedParams,
                                                         const std::string &namespacePrefix,
                                                         Context &ctx,
                                                         bool allowMathBare,
                                                         std::string &error);

bool rewriteCanonicalExperimentalVectorConstructorExpr(Expr &valueExpr,
                                                       const LocalTypeMap &locals,
                                                       const std::vector<semantics::ParameterInfo> &params,
                                                       const SubstMap &mapping,
                                                       const std::unordered_set<std::string> &allowedParams,
                                                       const std::string &namespacePrefix,
                                                       Context &ctx,
                                                       bool allowMathBare,
                                                       std::string &error);

} // namespace primec
