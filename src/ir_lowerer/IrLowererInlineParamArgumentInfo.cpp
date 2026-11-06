#include "IrLowererInlineParamArgumentInfo.h"

namespace primec::ir_lowerer {

bool isDirectSequentialHandle(const LocalInfo &info) {
  return info.kind == LocalInfo::Kind::Array ||
         info.kind == LocalInfo::Kind::Vector ||
         info.kind == LocalInfo::Kind::Buffer;
}

bool sequentialHandleKindsCompatible(const LocalInfo &paramInfo,
                                     const LocalInfo &argInfo) {
  if (!isDirectSequentialHandle(paramInfo) || !isDirectSequentialHandle(argInfo)) {
    return false;
  }
  if (paramInfo.kind != argInfo.kind) {
    return false;
  }
  return paramInfo.valueKind == LocalInfo::ValueKind::Unknown ||
         argInfo.valueKind == LocalInfo::ValueKind::Unknown ||
         paramInfo.valueKind == argInfo.valueKind;
}

void preserveSequentialHandleArgumentInfo(LocalInfo &paramInfo,
                                          const LocalInfo &argInfo) {
  const int32_t paramIndex = paramInfo.index;
  const bool paramIsMutable = paramInfo.isMutable;
  const bool paramIsArgsPack = paramInfo.isArgsPack;
  const int32_t paramArgsPackElementCount = paramInfo.argsPackElementCount;
  if (argInfo.valueKind != LocalInfo::ValueKind::Unknown) {
    paramInfo.valueKind = argInfo.valueKind;
  }
  if (!argInfo.structTypeName.empty()) {
    paramInfo.structTypeName = argInfo.structTypeName;
  }
  if (argInfo.structSlotCount > 0) {
    paramInfo.structSlotCount = argInfo.structSlotCount;
  }
  paramInfo.vectorStructElementSlotCount = argInfo.vectorStructElementSlotCount;
  paramInfo.isSoaVector = paramInfo.isSoaVector || argInfo.isSoaVector;
  paramInfo.usesBuiltinCollectionLayout =
      paramInfo.usesBuiltinCollectionLayout || argInfo.usesBuiltinCollectionLayout;
  paramInfo.index = paramIndex;
  paramInfo.isMutable = paramIsMutable;
  paramInfo.isArgsPack = paramIsArgsPack;
  paramInfo.argsPackElementCount = paramArgsPackElementCount;
}

void preservePointerTargetArgumentInfo(LocalInfo &paramInfo,
                                       const LocalInfo &argInfo) {
  if (paramInfo.kind != LocalInfo::Kind::Pointer ||
      (argInfo.kind != LocalInfo::Kind::Pointer &&
       argInfo.kind != LocalInfo::Kind::Reference)) {
    return;
  }
  if (paramInfo.valueKind == LocalInfo::ValueKind::Unknown &&
      argInfo.valueKind != LocalInfo::ValueKind::Unknown) {
    paramInfo.valueKind = argInfo.valueKind;
  }
  if (paramInfo.structTypeName.empty() && !argInfo.structTypeName.empty()) {
    paramInfo.structTypeName = argInfo.structTypeName;
  }
  if (paramInfo.structSlotCount <= 0 && argInfo.structSlotCount > 0) {
    paramInfo.structSlotCount = argInfo.structSlotCount;
  }
  // Ensure pointer target flags are properly preserved for variadic arguments
  if (paramInfo.isArgsPack) {
    paramInfo.pointerToArray = paramInfo.pointerToArray || argInfo.pointerToArray;
    paramInfo.pointerToVector = paramInfo.pointerToVector || argInfo.pointerToVector;
    paramInfo.pointerToBuffer = paramInfo.pointerToBuffer || argInfo.pointerToBuffer;
  } else {
    paramInfo.pointerToArray = paramInfo.pointerToArray || argInfo.pointerToArray;
    paramInfo.pointerToVector = paramInfo.pointerToVector || argInfo.pointerToVector;
    paramInfo.pointerToBuffer = paramInfo.pointerToBuffer || argInfo.pointerToBuffer;
  }
  paramInfo.targetsUninitializedStorage =
      paramInfo.targetsUninitializedStorage || argInfo.targetsUninitializedStorage;
  paramInfo.isSoaVector = paramInfo.isSoaVector || argInfo.isSoaVector;
  paramInfo.usesBuiltinCollectionLayout =
      paramInfo.usesBuiltinCollectionLayout || argInfo.usesBuiltinCollectionLayout;
  if (paramInfo.keyValueKeyKind == LocalInfo::ValueKind::Unknown &&
      argInfo.keyValueKeyKind != LocalInfo::ValueKind::Unknown) {
    paramInfo.keyValueKeyKind = argInfo.keyValueKeyKind;
  }
  if (paramInfo.keyValueValueKind == LocalInfo::ValueKind::Unknown &&
      argInfo.keyValueValueKind != LocalInfo::ValueKind::Unknown) {
    paramInfo.keyValueValueKind = argInfo.keyValueValueKind;
  }
}

} // namespace primec::ir_lowerer
