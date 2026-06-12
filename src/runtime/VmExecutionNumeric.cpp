#include "VmExecutionNumeric.h"

#include "VmNumericOpcodeShared.h"

namespace primec::vm_detail {

bool handleVmNumericOpcode(const IrInstruction &inst, std::vector<uint64_t> &stack, std::string &error) {
  const VmNumericOpcodeResult result = handleSharedVmNumericOpcode(inst, stack, error);
  if (result == VmNumericOpcodeResult::Continue) {
    return true;
  }
  if (result == VmNumericOpcodeResult::Fault) {
    return false;
  }
  error = "unknown numeric IR opcode";
  return false;
}

} // namespace primec::vm_detail
