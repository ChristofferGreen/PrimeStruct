#pragma once

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "IrLowererStructTypeHelpers.h"
#include "primec/Ir.h"

namespace primec::ir_lowerer {

struct VectorRecordFieldSlots {
  int32_t totalSlots = 0;
  int32_t count = -1;
  int32_t capacity = -1;
  int32_t data = -1;
  int32_t ownsData = -1;
};

inline bool resolveVectorRecordFieldSlotsFromLayout(
    const StructSlotLayoutInfo &layout,
    VectorRecordFieldSlots &out) {
  auto findField = [&](std::initializer_list<const char *> names,
                       bool required,
                       int32_t &slotOut) {
    for (const char *name : names) {
      auto fieldIt = std::find_if(
          layout.fields.begin(),
          layout.fields.end(),
          [&](const StructSlotFieldInfo &field) {
            return field.name == name && field.slotCount == 1 &&
                   field.slotOffset >= 0;
          });
      if (fieldIt != layout.fields.end()) {
        slotOut = fieldIt->slotOffset;
        return true;
      }
    }
    return !required;
  };

  VectorRecordFieldSlots slots;
  if (!findField({"fieldCount", "count"}, true, slots.count) ||
      !findField({"fieldCapacity", "capacity"}, true, slots.capacity) ||
      !findField({"data"}, true, slots.data) ||
      !findField({"ownsData"}, false, slots.ownsData)) {
    return false;
  }
  slots.totalSlots = layout.totalSlots;
  for (const auto &field : layout.fields) {
    if (field.slotOffset >= 0 && field.slotCount > 0) {
      slots.totalSlots =
          std::max(slots.totalSlots, field.slotOffset + field.slotCount);
    }
  }
  if (slots.totalSlots <= 0) {
    return false;
  }
  out = slots;
  return true;
}

template <typename ResolveFieldFn>
bool resolveVectorRecordFieldSlotsFromFields(const std::string &structPath,
                                             const ResolveFieldFn &resolveField,
                                             VectorRecordFieldSlots &out) {
  auto findField = [&](std::initializer_list<const char *> names,
                       bool required,
                       int32_t &slotOut,
                       int32_t &totalSlots) {
    for (const char *name : names) {
      int32_t slotOffset = -1;
      int32_t slotCount = 0;
      if (resolveField(structPath, name, slotOffset, slotCount) &&
          slotOffset >= 0 && slotCount == 1) {
        slotOut = slotOffset;
        totalSlots = std::max(totalSlots, slotOffset + slotCount);
        return true;
      }
    }
    return !required;
  };

  VectorRecordFieldSlots slots;
  int32_t totalSlots = 0;
  if (!findField({"fieldCount", "count"}, true, slots.count, totalSlots) ||
      !findField({"fieldCapacity", "capacity"}, true, slots.capacity, totalSlots) ||
      !findField({"data"}, true, slots.data, totalSlots) ||
      !findField({"ownsData"}, false, slots.ownsData, totalSlots)) {
    return false;
  }
  if (totalSlots <= 0) {
    return false;
  }
  slots.totalSlots = totalSlots;
  out = slots;
  return true;
}

inline void emitVectorRecordHeader(std::vector<IrInstruction> &instructions,
                                   int32_t baseLocal,
                                   const VectorRecordFieldSlots &slots,
                                   int32_t count,
                                   int32_t capacity,
                                   int32_t heapAllocSlots,
                                   bool nullDataPointer,
                                   bool ownsData) {
  instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(count)});
  instructions.push_back(
      {IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + slots.count)});
  instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(capacity)});
  instructions.push_back({IrOpcode::StoreLocal,
                          static_cast<uint64_t>(baseLocal + slots.capacity)});
  if (nullDataPointer) {
    instructions.push_back({IrOpcode::PushI64, 0});
  } else {
    instructions.push_back(
        {IrOpcode::PushI32, static_cast<uint64_t>(heapAllocSlots)});
    instructions.push_back({IrOpcode::HeapAlloc, 0});
  }
  instructions.push_back(
      {IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + slots.data)});
  if (slots.ownsData >= 0) {
    instructions.push_back({IrOpcode::PushI32, ownsData ? 1ull : 0ull});
    instructions.push_back({IrOpcode::StoreLocal,
                            static_cast<uint64_t>(baseLocal + slots.ownsData)});
  }
}

} // namespace primec::ir_lowerer
