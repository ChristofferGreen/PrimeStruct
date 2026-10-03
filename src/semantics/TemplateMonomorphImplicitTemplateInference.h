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

// collection-surface-audit: exempt
bool inferBindingTypeForMonomorph(const Expr &initializer,
                                  const std::vector<ParameterInfo> &params,
                                  const LocalTypeMap &locals,
                                  bool allowMathBare,
                                  Context &ctx,
                                  BindingInfo &infoOut);

bool isStdlibMapHelperDefinitionPath(std::string_view path);

bool inferImplicitTemplateArgs(const Definition &def,
                               const Expr &callExpr,
                               const LocalTypeMap &locals,
                               const std::vector<ParameterInfo> &params,
                               const SubstMap &mapping,
                               const std::unordered_set<std::string> &allowedParams,
                               const std::string &namespacePrefix,
                               Context &ctx,
                               bool allowMathBare,
                               std::vector<std::string> &outArgs,
                               std::string &error);

} // namespace primec
