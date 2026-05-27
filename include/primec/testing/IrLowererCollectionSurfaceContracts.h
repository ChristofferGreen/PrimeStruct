#pragma once

#include <string>
#include <string_view>

#include "primec/Ast.h"
#include "primec/SemanticProduct.h"
#include "primec/StdlibSurfaceRegistry.h"

namespace primec::ir_lowerer {

// Focused collection-surface lowerer contracts for validation tests. Keep this
// surface narrower than the full IrLowererHelpers umbrella.
#include "primec/testing/ir_lowerer_helpers/IrLowererSetupTypeCollectionHelpers.h"

} // namespace primec::ir_lowerer
