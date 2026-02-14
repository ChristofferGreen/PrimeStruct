#include "primec/IrLowerer.h"
#include "primec/StringLiteral.h"

#include "IrLowererHelpers.h"

#include <algorithm>
#include <cctype>
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
