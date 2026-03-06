#include "primec/Emitter.h"

#include "EmitterHelpers.h"
#include "EmitterExprControlBuiltinBlockEarlyReturnStep.h"
#include "EmitterExprControlBuiltinBlockBindingAutoStep.h"
#include "EmitterExprControlBuiltinBlockBindingExplicitStep.h"
#include "EmitterExprControlBuiltinBlockBindingPreludeStep.h"
#include "EmitterExprControlBuiltinBlockFinalValueStep.h"
#include "EmitterExprControlBoolLiteralStep.h"
#include "EmitterExprControlBuiltinBlockPreludeStep.h"
#include "EmitterExprControlBodyWrapperStep.h"
#include "EmitterExprControlCallPathStep.h"
#include "EmitterExprControlCountRewriteStep.h"
#include "EmitterExprControlFieldAccessStep.h"
#include "EmitterExprControlFloatLiteralStep.h"
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
