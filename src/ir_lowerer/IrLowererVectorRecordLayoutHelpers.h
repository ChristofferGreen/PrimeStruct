// soa-surface-audit: exempt
#pragma once

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "IrLowererStructTypeHelpers.h"
#include "primec/Ir.h"
#include "primec/StdlibCollectionPaths.h"

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
  auto isExperimentalSoaVectorPath = [](const std::string &path) {
    return path == collection_paths::memberPath(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName) ||
           path == collection_paths::memberPathBare(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName) ||
           path.rfind(collection_paths::specializedTypePrefix(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName), 0) == 0 ||
           path.rfind(collection_paths::specializedTypePrefixBare(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName), 0) == 0;
  };
  if (isExperimentalSoaVectorPath(structPath)) {
    int32_t storageOffset = -1;
    int32_t storageSlotCount = 0;
    if (resolveField(structPath, "storage", storageOffset, storageSlotCount) &&
        storageOffset >= 0 && storageSlotCount >= 5) {
      VectorRecordFieldSlots slots;
      slots.count = storageOffset + 1;
      slots.capacity = storageOffset + 2;
      slots.data = storageOffset + 3;
      slots.ownsData = storageOffset + 4;
      slots.totalSlots = storageOffset + storageSlotCount;
      out = slots;
      return true;
    }
    // Fallback when the SoaVector struct is not in scope (e.g. IR-only tests without
    // stdlib imports). SoaVector layout: [type_tag, SoaColumn[type_tag, count, cap,
    // data, ownsData]] — so storage starts at slot 1 and SoaColumn.count is at +1.
    out.count = 2;
    out.capacity = 3;
    out.data = 4;
    out.ownsData = 5;
    out.totalSlots = 6;
    return true;
  }

  auto findField = [&](const std::string &lookupPath,
                       std::initializer_list<const char *> names,
                       bool required,
                       int32_t &slotOut,
                       int32_t &totalSlots) {
    for (const char *name : names) {
      int32_t slotOffset = -1;
      int32_t slotCount = 0;
      if (resolveField(lookupPath, name, slotOffset, slotCount) &&
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
  if (!findField(structPath, {"fieldCount", "count"}, true, slots.count, totalSlots) ||
      !findField(structPath, {"fieldCapacity", "capacity"}, true, slots.capacity, totalSlots) ||
      !findField(structPath, {"data"}, true, slots.data, totalSlots) ||
      !findField(structPath, {"ownsData"}, false, slots.ownsData, totalSlots)) {
    const size_t specializationMarker = structPath.find("__t");
    if (specializationMarker == std::string::npos) {
      return false;
    }
    const std::string unspecializedPath =
        structPath.substr(0, specializationMarker);
    slots = {};
    totalSlots = 0;
    if (!findField(unspecializedPath, {"fieldCount", "count"}, true, slots.count, totalSlots) ||
        !findField(unspecializedPath, {"fieldCapacity", "capacity"}, true, slots.capacity, totalSlots) ||
        !findField(unspecializedPath, {"data"}, true, slots.data, totalSlots) ||
        !findField(unspecializedPath, {"ownsData"}, false, slots.ownsData, totalSlots)) {
      const bool isSpecializedStdlibVector =
          structPath.find("/Vector__t") != std::string::npos ||
          structPath.find("Vector__t") == 0;
      if (!isSpecializedStdlibVector) {
        return false;
      }
      slots.count = 1;
      slots.capacity = 2;
      slots.data = 3;
      slots.ownsData = 4;
      totalSlots = 5;
    }
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
