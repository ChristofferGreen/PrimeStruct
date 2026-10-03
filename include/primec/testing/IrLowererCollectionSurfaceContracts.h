#pragma once

#include <string>
#include <string_view>

#include "primec/ast/Ast.h"
#include "primec/frontend/SemanticProduct.h"
#include "primec/support/StdlibSurfaceRegistry.h"

// Focused collection-surface lowerer contracts for validation tests. Keep this
// surface narrower than the full IrLowererHelpers umbrella.
#include "primec/support/CallbackTypes.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"

