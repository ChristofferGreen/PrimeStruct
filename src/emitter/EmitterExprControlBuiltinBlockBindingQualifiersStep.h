#pragma once

#include <functional>

#include "primec/Emitter.h"

namespace primec::emitter {

using EmitterExprControlBuiltinBlockBindingQualifiersIsReferenceCandidateFn =
    std::function<bool(const Emitter::BindingInfo &)>;

struct EmitterExprControlBuiltinBlockBindingQualifiersStepResult {
  bool needsConst = false;
  bool useRef = false;
};

EmitterExprControlBuiltinBlockBindingQualifiersStepResult runEmitterExprControlBuiltinBlockBindingQualifiersStep(
    const Emitter::BindingInfo &binding,
    bool hasInitializer,
    const EmitterExprControlBuiltinBlockBindingQualifiersIsReferenceCandidateFn &isReferenceCandidate);

} // namespace primec::emitter
