#include "primec/Vm.h"

#include "VmExecution.h"

namespace primec {

bool Vm::execute(const IrModule &module, uint64_t &result, std::string &error, uint64_t argCount) const {
  return vm_detail::executeVmModule(module, result, error, argCount, nullptr);
}

bool Vm::execute(const IrModule &module,
                 uint64_t &result,
                 std::string &error,
                 const std::vector<std::string_view> &args) const {
  return vm_detail::executeVmModule(module, result, error, static_cast<uint64_t>(args.size()), &args);
}

} // namespace primec
