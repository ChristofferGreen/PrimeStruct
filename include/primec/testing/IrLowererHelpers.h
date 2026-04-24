#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "primec/Ast.h"
#include "primec/Ir.h"
#include "primec/SemanticProduct.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererSemanticProductTargetAdapters.h"

namespace primec::ir_lowerer {

#include "primec/testing/ir_lowerer_helpers/IrLowererSharedTypes.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererSetupTypeHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererCallDispatchHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererCallAccessHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererBindingTransformHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererBindingTypeHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererFileWriteHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererFlowHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererResultHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererStatementBindingHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererCountAccessHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererIndexKindHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererOnErrorHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererInlineCallContextHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererStructFieldBindingHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererStructTypeHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererInlineParamHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererInlineStructArgHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererStringCallHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererOperatorArithmeticHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererLowerEffects.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererVmEffects.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererLowerEntrySetup.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererStatementCallHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererLowerInlineCallActiveContextStep.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererLowerExprEmitSetup.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererReturnInferenceHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererSetupMathHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererSetupInferenceHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererLowerInferenceBaseKindHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererLowerInlineCallCleanupStep.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererLowerInlineCallContextSetupStep.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererLowerInlineCallGpuLocalsStep.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererLowerInlineCallReturnValueStep.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererLowerImportsStructsSetup.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererLowerInlineCallStatementStep.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererStringLiteralHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererRuntimeErrorHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererUninitializedTypeHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererSetupLocalsHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererLowerLocalsSetup.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererLowerReturnCallsSetup.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererOperatorArcHyperbolicHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererOperatorClampMinMaxTrigHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererOperatorComparisonHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererOperatorConversionsAndCallsHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererOperatorPowAbsSignHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererOperatorSaturateRoundingRootsHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererStructLayoutHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererStructReturnPathHelpers.h"
#include "primec/testing/ir_lowerer_helpers/IrLowererTemplateTypeParseHelpers.h"

} // namespace primec::ir_lowerer
