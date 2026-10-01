#include "VmExecution.h"

#include "VmHeapHelpers.h"
#include "VmIoHelpers.h"
#include "primec/runtime/VmExecutionKernel.h"

#include <exception>
#include <string>
#include <string_view>
#include <vector>

namespace primec::vm_detail {

namespace {

class RuntimeVmKernelHost final : public VmKernelHost {
public:
  RuntimeVmKernelHost(uint64_t argCount,
                      const std::vector<std::string_view> *args,
                      const VmHostFunctions *hostFunctions)
      : argCount_(argCount),
        hostFunctions_(hostFunctions),
        args_(args) {}

  uint64_t argumentCount() const override { return argCount_; }
  uint64_t slotBytes() const override { return IrSlotBytes; }
  size_t maxCallDepth() const override { return 4096; }

  bool resolveIndirectAddress(uint64_t address,
                              std::vector<uint64_t> &locals,
                              uint64_t *&slot,
                              std::string &error) override {
    return vm_detail::resolveIndirectAddress(address,
                                             slotBytes(),
                                             locals,
                                             heapSlots_,
                                             heapAllocations_,
                                             slot,
                                             error);
  }

  bool allocateHeapSlots(uint64_t slotCount,
                         uint64_t &address,
                         std::string &error) override {
    return allocateVmHeapSlots(slotCount,
                               slotBytes(),
                               heapSlots_,
                               heapAllocations_,
                               address,
                               error);
  }

  bool freeHeapSlots(uint64_t address, std::string &error) override {
    return freeVmHeapSlots(address,
                           slotBytes(),
                           heapSlots_,
                           heapAllocations_,
                           error);
  }

  bool reallocHeapSlots(uint64_t address,
                        uint64_t slotCount,
                        uint64_t &newAddress,
                        std::string &error) override {
    return reallocVmHeapSlots(address,
                              slotCount,
                              slotBytes(),
                              heapSlots_,
                              heapAllocations_,
                              newAddress,
                              error);
  }

  bool handlePrintInstruction(const IrModule &module,
                              const IrInstruction &inst,
                              std::vector<uint64_t> &stack,
                              std::string &error) override {
    return handlePrintOpcode(module, inst, stack, args_, error);
  }

  bool handleFileInstruction(const IrModule &module,
                             const IrInstruction &inst,
                             std::vector<uint64_t> &stack,
                             std::vector<uint64_t> &locals,
                             std::string &error) override {
    return handleFileOpcode(module, inst, stack, locals, error);
  }

  bool handleHostCall(const IrModule &module,
                      const IrInstruction &inst,
                      std::vector<uint64_t> &stack,
                      std::string &error) override {
    if (inst.imm >= module.hostImports.size()) {
      error = "invalid host import index in IR";
      return false;
    }
    const IrHostImport &import = module.hostImports[static_cast<size_t>(inst.imm)];
    const VmHostBinding *binding = hostFunctions_ != nullptr ? hostFunctions_->find(import.name) : nullptr;
    if (binding == nullptr || !binding->invoke) {
      error = "unbound host function: " + import.name;
      return false;
    }
    const size_t argCount = import.parameters.size();
    if (stack.size() < argCount) {
      error = "IR stack underflow on host call " + import.name;
      return false;
    }
    const size_t base = stack.size() - argCount;
    for (size_t i = 0; i < argCount; ++i) {
      if (import.parameters[i] == IrHostValueKind::String) {
        uint64_t &slot = stack[base + i];
        if (slot >= module.stringTable.size()) {
          error = "invalid string index passed to host function " + import.name;
          return false;
        }
        slot = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&module.stringTable[static_cast<size_t>(slot)]));
      }
    }
    uint64_t result = 0;
    std::string hostError;
    bool ok = false;
    try {
      ok = binding->invoke(stack.data() + base, result, hostError);
    } catch (const std::exception &exception) {
      hostError = std::string("exception: ") + exception.what();
    } catch (...) {
      hostError = "unknown exception";
    }
    if (!ok) {
      error = "host function " + import.name + " failed" + (hostError.empty() ? "" : ": " + hostError);
      return false;
    }
    if (import.returnKind == IrHostValueKind::String && result >= module.stringTable.size()) {
      error = "host function " + import.name + " returned an invalid string index";
      return false;
    }
    stack.resize(base);
    switch (import.returnKind) {
    case IrHostValueKind::Void:
      break;
    case IrHostValueKind::I32:
      stack.push_back(static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(result))));
      break;
    case IrHostValueKind::Bool:
      stack.push_back(result != 0 ? 1u : 0u);
      break;
    case IrHostValueKind::I64:
    case IrHostValueKind::U64:
    case IrHostValueKind::F64:
    case IrHostValueKind::String:
      stack.push_back(result);
      break;
    case IrHostValueKind::F32:
      stack.push_back(result & 0xFFFFFFFFull);
      break;
    }
    return true;
  }

private:
  uint64_t argCount_ = 0;
  const VmHostFunctions *hostFunctions_ = nullptr;
  const std::vector<std::string_view> *args_ = nullptr;
  std::vector<uint64_t> heapSlots_;
  std::vector<VmDebugSession::HeapAllocation> heapAllocations_;
};

} // namespace

bool executeVmModule(const IrModule &module,
                     uint64_t &result,
                     std::string &error,
                     uint64_t argCount,
                     const std::vector<std::string_view> *args,
                     const VmHostFunctions *hostFunctions) {
  if (!module.hostImports.empty()) {
    // Not a function-local static: statics must not hold arena-allocated state.
    const VmHostFunctions noBindings;
    if (!(hostFunctions != nullptr ? *hostFunctions : noBindings).verify(module, error)) {
      return false;
    }
  }
  RuntimeVmKernelHost host(argCount, args, hostFunctions);
  return executeVmKernel(module, host, result, error);
}

} // namespace primec::vm_detail
