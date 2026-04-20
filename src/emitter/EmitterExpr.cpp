#include "primec/Emitter.h"

#include "EmitterHelpers.h"
#include "EmitterBuiltinCallPathHelpersInternal.h"
#include "EmitterExprControlBuiltinBlockEarlyReturnStep.h"
#include "EmitterExprControlBuiltinBlockBindingAutoStep.h"
#include "EmitterExprControlBuiltinBlockBindingExplicitStep.h"
#include "EmitterExprControlBuiltinBlockBindingFallbackStep.h"
#include "EmitterExprControlBuiltinBlockBindingPreludeStep.h"
#include "EmitterExprControlBuiltinBlockBindingQualifiersStep.h"
#include "EmitterExprControlBuiltinBlockStatementStep.h"
#include "EmitterExprControlBuiltinBlockFinalValueStep.h"
#include "EmitterExprControlBoolLiteralStep.h"
#include "EmitterExprControlBuiltinBlockPreludeStep.h"
#include "EmitterExprControlBodyWrapperStep.h"
#include "EmitterExprControlCallPathStep.h"
#include "EmitterExprControlCountRewriteStep.h"
#include "EmitterExprControlFieldAccessStep.h"
#include "EmitterExprControlFloatLiteralStep.h"
#include "EmitterExprControlIfBlockBindingAutoStep.h"
#include "EmitterExprControlIfBlockBindingExplicitStep.h"
#include "EmitterExprControlIfBlockBindingFallbackStep.h"
#include "EmitterExprControlIfBranchBodyBindingStep.h"
#include "EmitterExprControlIfBranchBodyHandlersStep.h"
#include "EmitterExprControlIfBranchBodyStep.h"
#include "EmitterExprControlIfBranchBodyReturnStep.h"
#include "EmitterExprControlIfBranchBodyStatementStep.h"
#include "EmitterExprControlIfBranchEmitStep.h"
#include "EmitterExprControlIfBranchValueStep.h"
#include "EmitterExprControlIfBranchPreludeStep.h"
#include "EmitterExprControlIfBranchWrapperStep.h"
#include "EmitterExprControlIfBlockBindingPreludeStep.h"
#include "EmitterExprControlIfBlockBindingQualifiersStep.h"
#include "EmitterExprControlIfBlockStatementStep.h"
#include "EmitterExprControlIfBlockFinalValueStep.h"
#include "EmitterExprControlIfBlockEarlyReturnStep.h"
#include "EmitterExprControlIfTernaryFallbackStep.h"
#include "EmitterExprControlIfTernaryStep.h"
#include "EmitterExprControlIfEnvelopeStep.h"
#include "EmitterExprControlIntegerLiteralStep.h"
#include "EmitterExprControlMethodPathStep.h"
#include "EmitterExprControlStringLiteralStep.h"
#include "EmitterExprControlNameStep.h"

#include <functional>
#include <sstream>
#include <unordered_map>
#include <utility>


namespace primec {
using namespace emitter;

std::string Emitter::toCppName(const std::string &fullPath) const {
  std::string name = "ps";
  for (char c : fullPath) {
    if (c == '/') {
      name += "_";
    } else {
      name += c;
    }
  }
  return name;
}

#include "EmitterExprLambda.h"
#include "EmitterExprControl.h"
#include "EmitterExprCalls.h"

} // namespace primec
