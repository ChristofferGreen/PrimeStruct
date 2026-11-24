#include "primec/ir_lowerer/IrLowererInlineExpansionCounter.h"

namespace primec::ir_lowerer {

void InlineExpansionCounter::enter(size_t instructionCount) {
  frames_.push_back(Frame{instructionCount, 0});
}

void InlineExpansionCounter::leave(const std::string &calleePath, size_t instructionCount) {
  if (frames_.empty()) {
    return;
  }
  const Frame frame = frames_.back();
  frames_.pop_back();
  const uint64_t emitted = instructionCount > frame.start ? instructionCount - frame.start : 0;
  const uint64_t own = emitted > frame.calleeInstructions ? emitted - frame.calleeInstructions : 0;
  Totals &totals = totals_[calleePath];
  totals.callSites += 1;
  totals.added += own;
  totals.withCallees += emitted;
  if (!frames_.empty()) {
    frames_.back().calleeInstructions += emitted;
  }
}

} // namespace primec::ir_lowerer
