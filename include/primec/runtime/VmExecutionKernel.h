#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec::vm_detail {

class VmKernelHost {
public:
  virtual ~VmKernelHost() = default;

  virtual uint64_t argumentCount() const = 0;
  virtual uint64_t slotBytes() const = 0;
  virtual size_t maxCallDepth() const = 0;

  virtual bool resolveIndirectAddress(uint64_t address,
                                      std::vector<uint64_t> &locals,
                                      uint64_t *&slot,
                                      std::string &error) = 0;
  virtual bool allocateHeapSlots(uint64_t slotCount,
                                 uint64_t &address,
                                 std::string &error) = 0;
  virtual bool freeHeapSlots(uint64_t address, std::string &error) = 0;
  virtual bool reallocHeapSlots(uint64_t address,
                                uint64_t slotCount,
                                uint64_t &newAddress,
                                std::string &error) = 0;

  virtual bool handlePrintInstruction(const IrModule &module,
                                      const IrInstruction &inst,
                                      std::vector<uint64_t> &stack,
                                      std::string &error) = 0;
  virtual bool handleFileInstruction(const IrModule &module,
                                     const IrInstruction &inst,
                                     std::vector<uint64_t> &stack,
                                     std::vector<uint64_t> &locals,
                                     std::string &error) = 0;
  // Executes IrOpcode::CallHost. Hosts that cannot serve host calls keep this
  // default, which faults with a diagnostic.
  virtual bool handleHostCall(const IrModule &module,
                              const IrInstruction &inst,
                              std::vector<uint64_t> &stack,
                              std::string &error) {
    (void)module;
    (void)inst;
    (void)stack;
    error = "host calls are not supported by this VM host";
    return false;
  }
};

// One activation record of the kernel (and of debug sessions, which step the
// same kernel).
struct VmKernelFrame {
  const IrFunction *function = nullptr;
  size_t functionIndex = 0;
  std::vector<uint64_t> locals;
  size_t ip = 0;
  bool returnValueToCaller = false;
};

enum class VmKernelStepOutcome { Continue, Exit, Fault };

// What a single step did to the call stack, for debug hooks. `functionIndex` is
// the callee for Call and the popped function for Return/Exit.
enum class VmKernelStepKind { Other, Call, Return, Exit };

struct VmKernelStepEvent {
  VmKernelStepKind kind = VmKernelStepKind::Other;
  size_t functionIndex = 0;
  bool returnsValueToCaller = false;
};

// Number of local slots the function needs (highest local index used + 1).
size_t computeVmKernelLocalCount(const IrFunction &function);

// Executes exactly one instruction of the top frame. `localCounts[i]` is the
// local slot count of function i. On Exit the final value is stored in
// `result` and `frames` is left as is.
VmKernelStepOutcome stepVmKernel(const IrModule &module,
                                 VmKernelHost &host,
                                 std::vector<uint64_t> &stack,
                                 std::vector<VmKernelFrame> &frames,
                                 const std::vector<size_t> &localCounts,
                                 uint64_t &result,
                                 VmKernelStepEvent &event,
                                 std::string &error);

bool executeVmKernel(const IrModule &module,
                     VmKernelHost &host,
                     uint64_t &result,
                     std::string &error);

bool isVmKernelPrintOpcode(IrOpcode op);
bool isVmKernelFileOpcode(IrOpcode op);

} // namespace primec::vm_detail
