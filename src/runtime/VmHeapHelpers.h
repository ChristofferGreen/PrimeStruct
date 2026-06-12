#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/Vm.h"

namespace primec::vm_detail {

bool resolveIndirectAddress(uint64_t address,
                            uint64_t slotBytes,
                            std::vector<uint64_t> &locals,
                            std::vector<uint64_t> &heapSlots,
                            std::vector<VmDebugSession::HeapAllocation> &heapAllocations,
                            uint64_t *&slotOut,
                            std::string &error);

bool allocateVmHeapSlots(uint64_t slotCount,
                         uint64_t slotBytes,
                         std::vector<uint64_t> &heapSlots,
                         std::vector<VmDebugSession::HeapAllocation> &heapAllocations,
                         uint64_t &addressOut,
                         std::string &error);

bool freeVmHeapSlots(uint64_t address,
                     uint64_t slotBytes,
                     std::vector<uint64_t> &heapSlots,
                     std::vector<VmDebugSession::HeapAllocation> &heapAllocations,
                     std::string &error);

bool reallocVmHeapSlots(uint64_t address,
                        uint64_t slotCount,
                        uint64_t slotBytes,
                        std::vector<uint64_t> &heapSlots,
                        std::vector<VmDebugSession::HeapAllocation> &heapAllocations,
                        uint64_t &addressOut,
                        std::string &error);

} // namespace primec::vm_detail
