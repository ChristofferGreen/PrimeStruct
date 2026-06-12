#include "VmNumericOpcodeShared.h"

#include "primec/VmKernelBoundary.h"

namespace primec::vm_detail {

VmNumericOpcodeResult handleSharedVmNumericOpcode(
    const IrInstruction &inst,
    std::vector<uint64_t> &stack,
    std::string &error) {
  const vm_kernel::PureOpcodeResult result =
      vm_kernel::executePureNumericOpcode(inst, stack, error);
  switch (result) {
  case vm_kernel::PureOpcodeResult::Continue:
    return VmNumericOpcodeResult::Continue;
  case vm_kernel::PureOpcodeResult::Fault:
    return VmNumericOpcodeResult::Fault;
  case vm_kernel::PureOpcodeResult::NotHandled:
    return VmNumericOpcodeResult::NotHandled;
  }
  return VmNumericOpcodeResult::Fault;
}

} // namespace primec::vm_detail
