#include "EmitterExprControlIfBlockBindingQualifiersStep.h"

namespace primec::emitter {

EmitterExprControlIfBlockBindingQualifiersStepResult runEmitterExprControlIfBlockBindingQualifiersStep(
    const Emitter::BindingInfo &binding,
    bool hasInitializer,
    const EmitterExprControlIfBlockBindingQualifiersIsReferenceCandidateFn &isReferenceCandidate) {
  EmitterExprControlIfBlockBindingQualifiersStepResult result;
  result.needsConst = !binding.isMutable;
  result.useRef = !binding.isMutable && !binding.isCopy && hasInitializer && isReferenceCandidate &&
                  isReferenceCandidate(binding);
  return result;
}

} // namespace primec::emitter
