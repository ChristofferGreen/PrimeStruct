#include "EmitterExprControlBuiltinBlockBindingQualifiersStep.h"

namespace primec::emitter {

EmitterExprControlBuiltinBlockBindingQualifiersStepResult runEmitterExprControlBuiltinBlockBindingQualifiersStep(
    const Emitter::BindingInfo &binding,
    bool hasInitializer,
    const EmitterExprControlBuiltinBlockBindingQualifiersIsReferenceCandidateFn &isReferenceCandidate) {
  EmitterExprControlBuiltinBlockBindingQualifiersStepResult result;
  result.needsConst = !binding.isMutable;
  result.useRef = !binding.isMutable && !binding.isCopy && hasInitializer && isReferenceCandidate &&
                  isReferenceCandidate(binding);
  return result;
}

} // namespace primec::emitter
