#include "EmitterExprControlIfEnvelopeStep.h"

#include "EmitterHelpers.h"

namespace primec::emitter {

bool runEmitterExprControlIfBlockEnvelopeStep(const Expr &candidate) {
  if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
    return false;
  }
  if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
    return false;
  }
  if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
    return false;
  }
  return true;
}

} // namespace primec::emitter
