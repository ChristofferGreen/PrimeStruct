#include "primec/runtime/Vm.h"

#include "VmExecution.h"

namespace primec {

namespace {
const VmOutputSink *sinkOrNull(const VmOutputSink &sink) {
  return sink.write != nullptr ? &sink : nullptr;
}
} // namespace

bool Vm::execute(const IrModule &module, uint64_t &result, std::string &error, uint64_t argCount) const {
  return vm_detail::executeVmModule(
      module, result, error, argCount, nullptr, nullptr, sinkOrNull(outputSink_));
}

bool Vm::execute(const IrModule &module,
                 uint64_t &result,
                 std::string &error,
                 const std::vector<std::string_view> &args) const {
  return vm_detail::executeVmModule(module,
                                    result,
                                    error,
                                    static_cast<uint64_t>(args.size()),
                                    &args,
                                    nullptr,
                                    sinkOrNull(outputSink_));
}

bool Vm::execute(const IrModule &module,
                 uint64_t &result,
                 std::string &error,
                 const std::vector<std::string_view> &args,
                 const VmHostFunctions &hostFunctions) const {
  return vm_detail::executeVmModule(module,
                                    result,
                                    error,
                                    static_cast<uint64_t>(args.size()),
                                    &args,
                                    &hostFunctions,
                                    sinkOrNull(outputSink_));
}

} // namespace primec
