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

bool inferCallTargetBinding(const Expr &bindingExpr,
                            bool allowMathBare,
                            Context &ctx,
                            semantics::BindingInfo &bindingOut);

bool resolveAssignmentTargetBinding(const Expr &target,
                                    const std::vector<semantics::ParameterInfo> &params,
                                    const LocalTypeMap &locals,
                                    bool allowMathBare,
                                    const std::string &namespacePrefix,
                                    Context &ctx,
                                    semantics::BindingInfo &bindingOut);

bool resolveFieldBindingTarget(const Expr &target,
                               const std::vector<semantics::ParameterInfo> &params,
                               const LocalTypeMap &locals,
                               bool allowMathBare,
                               const std::string &namespacePrefix,
                               Context &ctx,
                               semantics::BindingInfo &bindingOut);

bool resolveDereferenceBindingTarget(const Expr &target,
                                     const std::vector<semantics::ParameterInfo> &params,
                                     const LocalTypeMap &locals,
                                     bool allowMathBare,
                                     const std::string &namespacePrefix,
                                     Context &ctx,
                                     semantics::BindingInfo &bindingOut);

bool resolveAssignmentTargetBinding(const Expr &target,
                                    const std::vector<semantics::ParameterInfo> &params,
                                    const LocalTypeMap &locals,
                                    bool allowMathBare,
                                    const std::string &namespacePrefix,
                                    Context &ctx,
                                    semantics::BindingInfo &bindingOut);

} // namespace primec
