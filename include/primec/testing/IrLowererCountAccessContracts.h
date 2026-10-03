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
#include "primec/support/CallbackTypes.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"

// Focused count/access lowerer contracts for validation tests. Keep this
// surface narrower than the full IrLowererHelpers umbrella.
#include "primec/ir_lowerer/IrLowererSharedTypes.h"
#include "primec/ir_lowerer/IrLowererCountAccessHelpers.h"
#include "primec/ir_lowerer/IrLowererCallHelpers.h"

