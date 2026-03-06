#pragma once

#include <functional>

#include "primec/Emitter.h"

namespace primec::emitter {

using EmitterExprControlIfBlockBindingQualifiersIsReferenceCandidateFn =
    std::function<bool(const Emitter::BindingInfo &)>;

struct EmitterExprControlIfBlockBindingQualifiersStepResult {
  bool needsConst = false;
  bool useRef = false;
};

EmitterExprControlIfBlockBindingQualifiersStepResult runEmitterExprControlIfBlockBindingQualifiersStep(
    const Emitter::BindingInfo &binding,
    bool hasInitializer,
    const EmitterExprControlIfBlockBindingQualifiersIsReferenceCandidateFn &isReferenceCandidate);

} // namespace primec::emitter
