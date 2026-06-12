#include "VmDebugSessionInstructionNumeric.h"

#include "VmNumericOpcodeShared.h"

namespace primec::vm_debug_detail {

OpcodeBlockResult handleVmDebugNumericOpcode(const IrInstruction &inst,
                                             std::vector<uint64_t> &stack,
                                             std::string &error) {
  switch (primec::vm_detail::handleSharedVmNumericOpcode(inst, stack, error)) {
    case primec::vm_detail::VmNumericOpcodeResult::Continue:
      return OpcodeBlockResult::Continue;
    case primec::vm_detail::VmNumericOpcodeResult::Fault:
      return OpcodeBlockResult::Fault;
    case primec::vm_detail::VmNumericOpcodeResult::NotHandled:
      return OpcodeBlockResult::NotHandled;
  }
  return OpcodeBlockResult::NotHandled;
}

} // namespace primec::vm_debug_detail
