#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"
#include "primec/frontend/SemanticProduct.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererSemanticProductTargetAdapters.h"

namespace primec::ir_lowerer {

// Focused count/access lowerer contracts for validation tests. Keep this
// surface narrower than the full IrLowererHelpers umbrella.
#include "primec/testing/ir_lowerer_helpers/IrLowererSharedTypes.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererCountAccessHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererCallDispatchHelpers.h"

} // namespace primec::ir_lowerer
