#include "primec/IrLowerer.h"
#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/StringLiteral.h"

#include "IrLowererCallHelpers.h"
#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererFlowHelpers.h"
#include "IrLowererFileWriteHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererInlineCallContextHelpers.h"
#include "IrLowererInlineParamHelpers.h"
#include "IrLowererInlineStructArgHelpers.h"
#include "IrLowererCountAccessHelpers.h"
#include "IrLowererLowerEffects.h"
#include "IrLowererLowerEntrySetup.h"
#include "IrLowererLowerInferenceSetup.h"
#include "IrLowererLowerInlineCallActiveContextStep.h"
#include "IrLowererLowerInlineCallCleanupStep.h"
#include "IrLowererLowerInlineCallContextSetupStep.h"
#include "IrLowererLowerInlineCallGpuLocalsStep.h"
#include "IrLowererLowerInlineCallReturnValueStep.h"
#include "IrLowererLowerImportsStructsSetup.h"
#include "IrLowererLowerInlineCallStatementStep.h"
#include "IrLowererLowerLocalsSetup.h"
#include "IrLowererLowerExprEmitSetup.h"
#include "IrLowererLowerReturnCallsSetup.h"
#include "IrLowererLowerStatementsCallsStep.h"
#include "IrLowererLowerStatementsEntryExecutionStep.h"
#include "IrLowererLowerStatementsEntryStatementStep.h"
#include "IrLowererLowerStatementsFunctionTableStep.h"
#include "IrLowererLowerStatementsSourceMapStep.h"
#include "IrLowererOnErrorHelpers.h"
#include "IrLowererOperatorArithmeticHelpers.h"
#include "IrLowererOperatorArcHyperbolicHelpers.h"
#include "IrLowererOperatorClampMinMaxTrigHelpers.h"
#include "IrLowererOperatorComparisonHelpers.h"
#include "IrLowererOperatorConversionsAndCallsHelpers.h"
#include "IrLowererOperatorPowAbsSignHelpers.h"
#include "IrLowererOperatorSaturateRoundingRootsHelpers.h"
#include "IrLowererReturnInferenceHelpers.h"
#include "IrLowererResultHelpers.h"
#include "IrLowererRuntimeErrorHelpers.h"
#include "IrLowererSetupInferenceHelpers.h"
#include "IrLowererSetupLocalsHelpers.h"
#include "IrLowererSetupMathHelpers.h"
#include "IrLowererStatementBindingHelpers.h"
#include "IrLowererStatementCallHelpers.h"
#include "IrLowererStructFieldBindingHelpers.h"
#include "IrLowererStructLayoutHelpers.h"
#include "IrLowererStructReturnPathHelpers.h"
#include "IrLowererStructTypeHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererUninitializedTypeHelpers.h"
#include "IrLowererStringLiteralHelpers.h"
#include "IrLowererSharedTypes.h"
#include "IrLowererStringCallHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace primec {
using namespace ir_lowerer;

#include "IrLowererLowerSetupEntryEffects.h"
#include "IrLowererLowerSetupImportsStructs.h"
#include "IrLowererLowerSetupLocals.h"
#include "IrLowererLowerSetupInference.h"
#include "IrLowererLowerReturnAndCalls.h"
#include "IrLowererLowerOperators.h"
#include "IrLowererLowerStatementsExpr.h"
#include "IrLowererLowerStatementsBindings.h"
#include "IrLowererLowerStatementsLoops.h"
#include "IrLowererLowerStatementsCalls.h"

} // namespace primec
