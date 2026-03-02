#include "primec/IrLowerer.h"
#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/StringLiteral.h"

#include "IrLowererCallHelpers.h"
#include "IrLowererFlowHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererLowerEffects.h"
#include "IrLowererOnErrorHelpers.h"
#include "IrLowererResultHelpers.h"
#include "IrLowererStringCallHelpers.h"

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

#include "IrLowererLowerSetup.h"
#include "IrLowererLowerReturnAndCalls.h"
#include "IrLowererLowerOperators.h"
#include "IrLowererLowerStatements.h"

} // namespace primec
