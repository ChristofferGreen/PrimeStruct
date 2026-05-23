#include "VmExecution.h"

#include "VmHeapHelpers.h"
#include "VmIoHelpers.h"
#include "primec/VmExecutionKernel.h"

#include <string>
#include <string_view>
#include <vector>

namespace primec::vm_detail {

namespace {

class RuntimeVmKernelHost final : public VmKernelHost {
public:
  RuntimeVmKernelHost(uint64_t argCount,
                      const std::vector<std::string_view> *args)
      : argCount_(argCount),
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

private:
  uint64_t argCount_ = 0;
  const std::vector<std::string_view> *args_ = nullptr;
  std::vector<uint64_t> heapSlots_;
  std::vector<VmDebugSession::HeapAllocation> heapAllocations_;
};

} // namespace

bool executeVmModule(const IrModule &module,
                     uint64_t &result,
                     std::string &error,
                     uint64_t argCount,
                     const std::vector<std::string_view> *args) {
  RuntimeVmKernelHost host(argCount, args);
  return executeVmKernel(module, host, result, error);
}

} // namespace primec::vm_detail
