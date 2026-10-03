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

bool inferBlockBodyBindingTypeForMonomorph(const Expr &initializer,
                                           const std::vector<semantics::ParameterInfo> &params,
                                           const LocalTypeMap &locals,
                                           bool allowMathBare,
                                           Context &ctx,
                                           semantics::BindingInfo &infoOut);

} // namespace primec
