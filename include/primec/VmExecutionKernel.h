#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "primec/Ir.h"

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
};

bool executeVmKernel(const IrModule &module,
                     VmKernelHost &host,
                     uint64_t &result,
                     std::string &error);

bool isVmKernelPrintOpcode(IrOpcode op);
bool isVmKernelFileOpcode(IrOpcode op);

} // namespace primec::vm_detail
