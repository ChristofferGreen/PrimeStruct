#include "primec/IrLowerer.h"
#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/StringLiteral.h"

#include "IrLowererCallHelpers.h"
#include "IrLowererFlowHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererLowerEffects.h"
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
#include "IrLowererSetupMathHelpers.h"
#include "IrLowererStatementBindingHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
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
