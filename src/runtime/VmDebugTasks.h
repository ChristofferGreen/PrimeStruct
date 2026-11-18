#pragma once

#include "primec/runtime/Vm.h"

#include "VmExecution.h"
#include "VmHeapHelpers.h"
#include "VmIoHelpers.h"
#include "VmTaskScheduler.h"

namespace primec {

// Serves one kernel step from the session's own heap, argv and host bindings,
// so debug sessions run exactly the dispatch `Vm::execute` runs.
class VmDebugKernelHost final : public vm_detail::VmKernelHost {
public:
  VmDebugKernelHost(uint64_t argCount,
                    const std::vector<std::string_view> *args,
                    const VmHostFunctions *hostFunctions,
                    VmHeapCore &heap,
                    VmTextHeap &textHeap)
      : argCount_(argCount), args_(args), hostFunctions_(hostFunctions), heap_(heap),
        textHeap_(textHeap) {
  }

  uint64_t argumentCount() const override {
    return argCount_;
  }
  uint64_t slotBytes() const override {
    return IrSlotBytes;
  }
  size_t maxCallDepth() const override {
    return 4096;
  }

  bool resolveIndirectAddress(uint64_t address,
                              std::vector<uint64_t> &locals,
                              uint64_t *&slot,
                              std::string &error) override {
    return vm_detail::resolveIndirectAddress(address, slotBytes(), locals, heap_, slot, error);
  }
  bool resolveAtomicAddress(uint64_t address,
                            std::vector<uint64_t> &locals,
                            uint64_t *&slot,
                            std::string &error) override {
    return vm_detail::resolveAtomicAddress(address, slotBytes(), locals, heap_, slot, error);
  }
  bool allocateHeapSlots(uint64_t slotCount, uint64_t &address, std::string &error) override {
    return heap_.allocate(slotCount, address, error);
  }
  bool allocateHeapBytes(uint64_t byteCount, uint64_t &address, std::string &error) override {
    return heap_.allocateBytes(byteCount, address, error);
  }
  bool loadHeapByte(uint64_t address,
                    std::vector<uint64_t> &locals,
                    uint8_t &value,
                    std::string &error) override {
    return vm_detail::loadHeapByte(address, slotBytes(), locals, heap_, value, error);
  }
  bool storeHeapByte(uint64_t address,
                     std::vector<uint64_t> &locals,
                     uint8_t value,
                     std::string &error) override {
    return vm_detail::storeHeapByte(address, slotBytes(), locals, heap_, value, error);
  }
  bool freeHeapSlots(uint64_t address, std::string &error) override {
    return heap_.release(address, error);
  }
  bool reallocHeapSlots(uint64_t address,
                        uint64_t slotCount,
                        uint64_t &newAddress,
                        std::string &error) override {
    return heap_.reallocate(address, slotCount, newAddress, error);
  }
  bool handlePrintInstruction(const IrModule &module,
                              const IrInstruction &inst,
                              std::vector<uint64_t> &stack,
                              std::string &error) override {
    return vm_detail::handlePrintOpcode(module, inst, stack, args_, error);
  }
  bool handleFileInstruction(const IrModule &module,
                             const IrInstruction &inst,
                             std::vector<uint64_t> &stack,
                             std::vector<uint64_t> &locals,
                             std::string &error) override {
    return vm_detail::handleFileOpcode(module, inst, stack, locals, error);
  }
  bool handleHostCall(const IrModule &module,
                      const IrInstruction &inst,
                      std::vector<uint64_t> &stack,
                      std::string &error) override {
    vm_detail::TextHeapContext text{&heap_, &textHeap_};
    return vm_detail::handleVmHostCall(hostFunctions_, module, inst, stack, error, &text);
  }

private:
  uint64_t argCount_ = 0;
  const std::vector<std::string_view> *args_ = nullptr;
  const VmHostFunctions *hostFunctions_ = nullptr;
  VmHeapCore &heap_;
  VmTextHeap &textHeap_;
};

// The task scheduler of a debug session on a program with tasks, with the host it steps on.
struct VmDebugTasks {
  VmDebugTasks(const IrModule &module,
               uint64_t argCount,
               const std::vector<std::string_view> *args,
               const VmHostFunctions *hostFunctions,
               VmHeapCore &heap,
               VmTextHeap &textHeap,
               const std::vector<size_t> &localCounts,
               std::optional<uint64_t> seed)
      : host(argCount, args, hostFunctions, heap, textHeap),
        scheduler(module, host, localCounts, seed) {
  }
  VmDebugKernelHost host;
  vm_detail::VmSteppedTasks scheduler;
};

} // namespace primec
