#include "primec/ir_lowerer/IrLowererFlowHelpers.h"

#include "primec/ir_lowerer/IrLowererBindingTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererStructTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"

#include <string_view>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec::ir_lowerer {

void emitFileCloseIfValid(std::vector<IrInstruction> &instructions, int32_t localIndex) {
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(localIndex)});
  instructions.push_back({IrOpcode::PushI64, 0});
  instructions.push_back({IrOpcode::CmpGeI64, 0});
  const size_t jumpSkip = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(localIndex)});
  instructions.push_back({IrOpcode::FileClose, 0});
  instructions.push_back({IrOpcode::Pop, 0});
  const size_t skipIndex = instructions.size();
  instructions[jumpSkip].imm = static_cast<int32_t>(skipIndex);
}

void emitFileScopeCleanup(std::vector<IrInstruction> &instructions, const std::vector<int32_t> &scope) {
  for (auto it = scope.rbegin(); it != scope.rend(); ++it) {
    emitFileCloseIfValid(instructions, *it);
  }
}

void emitAllFileScopeCleanup(std::vector<IrInstruction> &instructions,
                             const std::vector<std::vector<int32_t>> &fileScopeStack) {
  for (auto it = fileScopeStack.rbegin(); it != fileScopeStack.rend(); ++it) {
    emitFileScopeCleanup(instructions, *it);
  }
}

bool emitDestroyHelperFromPtr(
    int32_t valuePtrLocal,
    const std::string &structPath,
    const Definition *destroyHelper,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::string &error) {
  if (destroyHelper == nullptr) {
    return true;
  }
  if (structPath.empty()) {
    error = "internal error: destroy helper requires concrete struct path";
    return false;
  }

  LocalMap destroyLocals = localsIn;
  std::string receiverName = "__primec_destroy_slot";
  while (destroyLocals.count(receiverName) > 0) {
    receiverName += "_";
  }

  LocalInfo receiverInfo;
  receiverInfo.index = valuePtrLocal;
  receiverInfo.kind = LocalInfo::Kind::Reference;
  receiverInfo.isMutable = true;
  receiverInfo.structTypeName = structPath;
  destroyLocals.emplace(receiverName, receiverInfo);

  Expr receiverExpr;
  receiverExpr.kind = Expr::Kind::Name;
  receiverExpr.name = receiverName;

  Expr destroyCallExpr;
  destroyCallExpr.kind = Expr::Kind::Call;
  destroyCallExpr.isMethodCall = true;
  const size_t slash = destroyHelper->fullPath.find_last_of('/');
  destroyCallExpr.name = slash == std::string::npos ? destroyHelper->fullPath
                                                    : destroyHelper->fullPath.substr(slash + 1);
  destroyCallExpr.args.push_back(receiverExpr);
  destroyCallExpr.argNames.resize(1);

  return emitInlineDefinitionCall(destroyCallExpr, *destroyHelper, destroyLocals, false);
}

bool emitMoveHelperFromPtrs(
    int32_t destPtrLocal,
    int32_t srcPtrLocal,
    const std::string &structPath,
    const Definition *moveHelper,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::string &error) {
  if (moveHelper == nullptr) {
    error = "internal error: move helper requires concrete helper definition";
    return false;
  }
  if (structPath.empty()) {
    error = "internal error: move helper requires concrete struct path";
    return false;
  }

  LocalMap moveLocals = localsIn;
  std::string receiverName = "__primec_move_dest";
  while (moveLocals.count(receiverName) > 0) {
    receiverName += "_";
  }
  std::string sourceName = "__primec_move_src";
  while (moveLocals.count(sourceName) > 0 || sourceName == receiverName) {
    sourceName += "_";
  }

  LocalInfo receiverInfo;
  receiverInfo.index = destPtrLocal;
  receiverInfo.kind = LocalInfo::Kind::Reference;
  receiverInfo.isMutable = true;
  receiverInfo.structTypeName = structPath;
  moveLocals.emplace(receiverName, receiverInfo);

  LocalInfo sourceInfo;
  sourceInfo.index = srcPtrLocal;
  sourceInfo.kind = LocalInfo::Kind::Reference;
  sourceInfo.isMutable = true;
  sourceInfo.structTypeName = structPath;
  moveLocals.emplace(sourceName, sourceInfo);

  Expr receiverExpr;
  receiverExpr.kind = Expr::Kind::Name;
  receiverExpr.name = receiverName;

  Expr sourceExpr;
  sourceExpr.kind = Expr::Kind::Name;
  sourceExpr.name = sourceName;

  Expr moveCallExpr;
  moveCallExpr.kind = Expr::Kind::Call;
  moveCallExpr.isMethodCall = true;
  const size_t slash = moveHelper->fullPath.find_last_of('/');
  moveCallExpr.name = slash == std::string::npos ? moveHelper->fullPath : moveHelper->fullPath.substr(slash + 1);
  moveCallExpr.args.push_back(receiverExpr);
  moveCallExpr.args.push_back(sourceExpr);
  moveCallExpr.argNames.resize(2);

  return emitInlineDefinitionCall(moveCallExpr, *moveHelper, moveLocals, false);
}

namespace {

bool isCopyableBuiltinVectorField(const StructSlotFieldInfo &field) {
  return field.builtinVectorElementKnown && field.slotCount == 5 &&
         field.structPath == vectorBuiltinStructNormalizedPath();
}

// `withBuiltinVectors` also counts builtin `vector<T>` fields, which a copy gives their own
// buffer and a destroy frees.
bool structNeedsCopyHelpers(
    const std::string &structPath,
    const std::function<const Definition *(const std::string &)> &findCopyHelper,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    int depth,
    bool withBuiltinVectors) {
  if (structPath.empty() || depth > 16) {
    return false;
  }
  if (isNestedBuiltinVectorElementPath(structPath)) {
    return withBuiltinVectors;
  }
  if (findCopyHelper(structPath) != nullptr) {
    return true;
  }
  StructSlotLayoutInfo layout;
  if (!resolveStructSlotLayout(structPath, layout)) {
    return false;
  }
  for (const StructSlotFieldInfo &field : layout.fields) {
    if (withBuiltinVectors && isCopyableBuiltinVectorField(field)) {
      return true;
    }
    if (structNeedsCopyHelpers(field.structPath,
                               findCopyHelper,
                               resolveStructSlotLayout,
                               depth + 1,
                               withBuiltinVectors)) {
      return true;
    }
  }
  // A sum needs what the payload of any of its variants needs.
  for (const SumPayloadLayoutInfo &payload : layout.sumPayloads) {
    if (structNeedsCopyHelpers(payload.lifecyclePath,
                               findCopyHelper,
                               resolveStructSlotLayout,
                               depth + 1,
                               withBuiltinVectors)) {
      return true;
    }
  }
  return false;
}

// Runs `body(payload)` for the payload of the active variant of the sum at `sumPtrLocal` (tag in
// slot 1), for the variants `needs(payload)` holds for.
bool emitForActiveSumPayload(int32_t sumPtrLocal,
                             const std::vector<SumPayloadLayoutInfo> &payloads,
                             std::vector<IrInstruction> &instructions,
                             const std::function<bool(const SumPayloadLayoutInfo &)> &needs,
                             const std::function<bool(const SumPayloadLayoutInfo &)> &body) {
  std::vector<size_t> endJumps;
  for (const SumPayloadLayoutInfo &payload : payloads) {
    if (!needs(payload)) {
      continue;
    }
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sumPtrLocal)});
    instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::LoadIndirect, 0});
    instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(payload.tagValue)});
    instructions.push_back({IrOpcode::CmpEqI32, 0});
    const size_t nextVariantJump = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    if (!body(payload)) {
      return false;
    }
    endJumps.push_back(instructions.size());
    instructions.push_back({IrOpcode::Jump, 0});
    instructions[nextVariantJump].imm = static_cast<uint64_t>(instructions.size());
  }
  for (size_t jumpIndex : endJumps) {
    instructions[jumpIndex].imm = static_cast<uint64_t>(instructions.size());
  }
  return true;
}

// Stores the address of the payload (slot 2) of the sum at `sumPtrLocal` in a new local.
int32_t emitSumPayloadAddress(int32_t sumPtrLocal,
                              const Int32ProviderFn &allocTempLocal,
                              std::vector<IrInstruction> &instructions) {
  const int32_t payloadPtrLocal = allocTempLocal();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sumPtrLocal)});
  instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(2) * IrSlotBytes});
  instructions.push_back({IrOpcode::AddI64, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(payloadPtrLocal)});
  return payloadPtrLocal;
}

bool emitStructCopyHelpersAtDepth(
    int32_t destPtrLocal,
    int32_t srcPtrLocal,
    const std::string &structPath,
    const std::function<const Definition *(const std::string &)> &findCopyHelper,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const Int32ProviderFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)>
        &emitInlineDefinitionCall,
    bool &ranHelper,
    std::string &error,
    int depth);

// The record at `destPtrLocal` (count, capacity, data and ownsData in slots 1-4) is a slot copy of
// the one at `srcPtrLocal`, so both name one buffer. Give `dest` a buffer of its own that holds a
// copy of each element (docs/spec/value-lifecycle.md, Copies).
bool emitBuiltinVectorCopyAtDepth(
    int32_t destPtrLocal,
    int32_t srcPtrLocal,
    const std::string &elementStructPath,
    const std::function<const Definition *(const std::string &)> &findCopyHelper,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const Int32ProviderFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)>
        &emitInlineDefinitionCall,
    bool &ranHelper,
    std::string &error,
    int depth) {
  int32_t elementSlots = 1;
  bool elementNeedsCopy = false;
  if (!elementStructPath.empty()) {
    StructSlotLayoutInfo elementLayout;
    if (!resolveStructSlotLayout(elementStructPath, elementLayout) ||
        elementLayout.totalSlots <= 0) {
      error = "native backend cannot resolve vector element layout: " + elementStructPath;
      return false;
    }
    elementSlots = elementLayout.totalSlots;
    elementNeedsCopy = structNeedsCopyHelpers(
        elementStructPath, findCopyHelper, resolveStructSlotLayout, depth + 1, true);
  }
  auto emit = [&](IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); };
  auto pushFieldAddress = [&](int32_t recordPtrLocal, uint64_t slot) {
    emit(IrOpcode::LoadLocal, static_cast<uint64_t>(recordPtrLocal));
    emit(IrOpcode::PushI64, slot * IrSlotBytes);
    emit(IrOpcode::AddI64, 0);
  };
  auto storeDestField = [&](uint64_t slot, int32_t valueLocal) {
    pushFieldAddress(destPtrLocal, slot);
    emit(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emit(IrOpcode::StoreIndirect, 0);
    emit(IrOpcode::Pop, 0);
  };
  // The address of slot `indexLocal * stride` in the buffer at `dataLocal`.
  auto pushSlotAddress = [&](int32_t dataLocal, int32_t indexLocal, int32_t strideSlots) {
    emit(IrOpcode::LoadLocal, static_cast<uint64_t>(dataLocal));
    emit(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
    emit(IrOpcode::PushI32, static_cast<uint64_t>(strideSlots * IrSlotBytesI32));
    emit(IrOpcode::MulI32, 0);
    emit(IrOpcode::AddI64, 0);
  };
  // for (index = 0; index < limit; ++index) body()
  auto emitCountedLoop =
      [&](int32_t indexLocal, int32_t limitLocal, const std::function<bool()> &body) {
        emit(IrOpcode::PushI32, 0);
        emit(IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal));
        const size_t loopStart = instructions.size();
        emit(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
        emit(IrOpcode::LoadLocal, static_cast<uint64_t>(limitLocal));
        emit(IrOpcode::CmpLtI32, 0);
        const size_t loopExit = instructions.size();
        emit(IrOpcode::JumpIfZero, 0);
        if (!body()) {
          return false;
        }
        emit(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
        emit(IrOpcode::PushI32, 1);
        emit(IrOpcode::AddI32, 0);
        emit(IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal));
        emit(IrOpcode::Jump, static_cast<uint64_t>(loopStart));
        instructions[loopExit].imm = static_cast<uint64_t>(instructions.size());
        return true;
      };
  const int32_t countLocal = allocTempLocal();
  const int32_t capacityLocal = allocTempLocal();
  const int32_t slotTotalLocal = allocTempLocal();
  const int32_t srcDataLocal = allocTempLocal();
  const int32_t destDataLocal = allocTempLocal();
  const int32_t indexLocal = allocTempLocal();
  const int32_t ownsLocal = allocTempLocal();
  pushFieldAddress(srcPtrLocal, 1);
  emit(IrOpcode::LoadIndirect, 0);
  emit(IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal));
  pushFieldAddress(srcPtrLocal, 3);
  emit(IrOpcode::LoadIndirect, 0);
  emit(IrOpcode::StoreLocal, static_cast<uint64_t>(srcDataLocal));
  // capacity = count, or 1 for an empty vector, so the new buffer is never empty.
  emit(IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal));
  emit(IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal));
  emit(IrOpcode::PushI32, 0);
  emit(IrOpcode::CmpEqI32, 0);
  emit(IrOpcode::AddI32, 0);
  emit(IrOpcode::StoreLocal, static_cast<uint64_t>(capacityLocal));
  emit(IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal));
  emit(IrOpcode::PushI32, static_cast<uint64_t>(elementSlots));
  emit(IrOpcode::MulI32, 0);
  emit(IrOpcode::HeapAlloc, 0);
  emit(IrOpcode::StoreLocal, static_cast<uint64_t>(destDataLocal));
  emit(IrOpcode::PushI32, 1);
  emit(IrOpcode::StoreLocal, static_cast<uint64_t>(ownsLocal));
  storeDestField(2, capacityLocal);
  storeDestField(3, destDataLocal);
  storeDestField(4, ownsLocal);
  // Copy the element slots, then run each element's copy helpers on its copy.
  emit(IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal));
  emit(IrOpcode::PushI32, static_cast<uint64_t>(elementSlots));
  emit(IrOpcode::MulI32, 0);
  emit(IrOpcode::StoreLocal, static_cast<uint64_t>(slotTotalLocal));
  if (!emitCountedLoop(indexLocal, slotTotalLocal, [&]() {
        pushSlotAddress(destDataLocal, indexLocal, 1);
        pushSlotAddress(srcDataLocal, indexLocal, 1);
        emit(IrOpcode::LoadIndirect, 0);
        emit(IrOpcode::StoreIndirect, 0);
        emit(IrOpcode::Pop, 0);
        return true;
      })) {
    return false;
  }
  if (!elementNeedsCopy) {
    return true;
  }
  const int32_t elementDestPtrLocal = allocTempLocal();
  const int32_t elementSrcPtrLocal = allocTempLocal();
  return emitCountedLoop(indexLocal, countLocal, [&]() {
    pushSlotAddress(destDataLocal, indexLocal, elementSlots);
    emit(IrOpcode::StoreLocal, static_cast<uint64_t>(elementDestPtrLocal));
    pushSlotAddress(srcDataLocal, indexLocal, elementSlots);
    emit(IrOpcode::StoreLocal, static_cast<uint64_t>(elementSrcPtrLocal));
    return emitStructCopyHelpersAtDepth(elementDestPtrLocal,
                                        elementSrcPtrLocal,
                                        elementStructPath,
                                        findCopyHelper,
                                        resolveStructSlotLayout,
                                        allocTempLocal,
                                        instructions,
                                        localsIn,
                                        emitInlineDefinitionCall,
                                        ranHelper,
                                        error,
                                        depth + 1);
  });
}

bool emitStructCopyHelpersAtDepth(
    int32_t destPtrLocal,
    int32_t srcPtrLocal,
    const std::string &structPath,
    const std::function<const Definition *(const std::string &)> &findCopyHelper,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const Int32ProviderFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)>
        &emitInlineDefinitionCall,
    bool &ranHelper,
    std::string &error,
    int depth) {
  // A builtin vector element of a builtin vector gets a buffer of its own.
  if (std::string innerElementPath;
      isNestedBuiltinVectorElementPath(structPath, &innerElementPath)) {
    return emitBuiltinVectorCopyAtDepth(destPtrLocal,
                                        srcPtrLocal,
                                        innerElementPath,
                                        findCopyHelper,
                                        resolveStructSlotLayout,
                                        allocTempLocal,
                                        instructions,
                                        localsIn,
                                        emitInlineDefinitionCall,
                                        ranHelper,
                                        error,
                                        depth + 1);
  }
  // `dest` starts as a slot copy of `src`. Copy the fields that have helpers of their own first,
  // so a user `Copy` helper starts from a `this` that owns its fields instead of aliasing the
  // source's storage (docs/spec/value-lifecycle.md).
  const Definition *copyHelper = findCopyHelper(structPath);
  StructSlotLayoutInfo layout;
  if (depth > 16 || !resolveStructSlotLayout(structPath, layout)) {
    layout.fields.clear();
  }
  for (const StructSlotFieldInfo &field : layout.fields) {
    const bool builtinVector = isCopyableBuiltinVectorField(field);
    if (!builtinVector &&
        !structNeedsCopyHelpers(
            field.structPath, findCopyHelper, resolveStructSlotLayout, depth + 1, true)) {
      continue;
    }
    const uint64_t offsetBytes = static_cast<uint64_t>(field.slotOffset) * IrSlotBytes;
    const int32_t fieldDestPtrLocal = allocTempLocal();
    const int32_t fieldSrcPtrLocal = allocTempLocal();
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
    instructions.push_back({IrOpcode::PushI64, offsetBytes});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(fieldDestPtrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(srcPtrLocal)});
    instructions.push_back({IrOpcode::PushI64, offsetBytes});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(fieldSrcPtrLocal)});
    const bool copied = builtinVector
                            ? emitBuiltinVectorCopyAtDepth(fieldDestPtrLocal,
                                                           fieldSrcPtrLocal,
                                                           field.builtinVectorElementStructPath,
                                                           findCopyHelper,
                                                           resolveStructSlotLayout,
                                                           allocTempLocal,
                                                           instructions,
                                                           localsIn,
                                                           emitInlineDefinitionCall,
                                                           ranHelper,
                                                           error,
                                                           depth + 1)
                            : emitStructCopyHelpersAtDepth(fieldDestPtrLocal,
                                                           fieldSrcPtrLocal,
                                                           field.structPath,
                                                           findCopyHelper,
                                                           resolveStructSlotLayout,
                                                           allocTempLocal,
                                                           instructions,
                                                           localsIn,
                                                           emitInlineDefinitionCall,
                                                           ranHelper,
                                                           error,
                                                           depth + 1);
    if (!copied) {
      return false;
    }
  }
  // A sum is a slot copy of its source so far: the payload of its active variant gets its own
  // resources.
  if (!emitForActiveSumPayload(
          destPtrLocal,
          layout.sumPayloads,
          instructions,
          [&](const SumPayloadLayoutInfo &payload) {
            return structNeedsCopyHelpers(
                payload.lifecyclePath, findCopyHelper, resolveStructSlotLayout, depth + 1, true);
          },
          [&](const SumPayloadLayoutInfo &payload) {
            const int32_t payloadDestPtrLocal =
                emitSumPayloadAddress(destPtrLocal, allocTempLocal, instructions);
            const int32_t payloadSrcPtrLocal =
                emitSumPayloadAddress(srcPtrLocal, allocTempLocal, instructions);
            return emitStructCopyHelpersAtDepth(payloadDestPtrLocal,
                                                payloadSrcPtrLocal,
                                                payload.lifecyclePath,
                                                findCopyHelper,
                                                resolveStructSlotLayout,
                                                allocTempLocal,
                                                instructions,
                                                localsIn,
                                                emitInlineDefinitionCall,
                                                ranHelper,
                                                error,
                                                depth + 1);
          })) {
    return false;
  }
  if (copyHelper != nullptr) {
    ranHelper = true;
    return emitMoveHelperFromPtrs(destPtrLocal,
                                  srcPtrLocal,
                                  structPath,
                                  copyHelper,
                                  localsIn,
                                  emitInlineDefinitionCall,
                                  error);
  }
  return true;
}

bool emitStructDestroyHelpersAtDepth(
    int32_t valuePtrLocal,
    const std::string &structPath,
    const std::function<const Definition *(const std::string &)> &findDestroyHelper,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const Int32ProviderFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)>
        &emitInlineDefinitionCall,
    std::string &error,
    int depth);

// Destroys the builtin vector record at `recordPtrLocal` (count, data and ownsData in slots 1, 3
// and 4) when it owns its buffer: destroys each element, frees the buffer and clears ownsData and
// data, so a second destroy of the record is a no-op. A record that does not own its buffer (a
// view, a disarmed temporary) is left alone (docs/spec/value-lifecycle.md).
bool emitBuiltinVectorDestroyAtDepth(
    int32_t recordPtrLocal,
    const std::string &elementStructPath,
    const std::function<const Definition *(const std::string &)> &findDestroyHelper,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const Int32ProviderFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)>
        &emitInlineDefinitionCall,
    std::string &error,
    int depth) {
  int32_t elementSlots = 1;
  bool elementNeedsDestroy = false;
  if (!elementStructPath.empty()) {
    StructSlotLayoutInfo elementLayout;
    if (!resolveStructSlotLayout(elementStructPath, elementLayout) ||
        elementLayout.totalSlots <= 0) {
      error = "native backend cannot resolve vector element layout: " + elementStructPath;
      return false;
    }
    elementSlots = elementLayout.totalSlots;
    elementNeedsDestroy = structNeedsCopyHelpers(
        elementStructPath, findDestroyHelper, resolveStructSlotLayout, depth + 1, true);
  }
  auto emit = [&](IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); };
  auto pushFieldAddress = [&](uint64_t slot) {
    emit(IrOpcode::LoadLocal, static_cast<uint64_t>(recordPtrLocal));
    emit(IrOpcode::PushI64, slot * IrSlotBytes);
    emit(IrOpcode::AddI64, 0);
  };
  auto storeZeroField = [&](uint64_t slot) {
    pushFieldAddress(slot);
    emit(IrOpcode::PushI64, 0);
    emit(IrOpcode::StoreIndirect, 0);
    emit(IrOpcode::Pop, 0);
  };
  const int32_t dataLocal = allocTempLocal();
  // if (ownsData && data != 0) { destroy the elements; free(data) } then ownsData = data = 0
  pushFieldAddress(4);
  emit(IrOpcode::LoadIndirect, 0);
  const size_t skipNotOwned = instructions.size();
  emit(IrOpcode::JumpIfZero, 0);
  pushFieldAddress(3);
  emit(IrOpcode::LoadIndirect, 0);
  emit(IrOpcode::StoreLocal, static_cast<uint64_t>(dataLocal));
  emit(IrOpcode::LoadLocal, static_cast<uint64_t>(dataLocal));
  emit(IrOpcode::PushI64, 0);
  emit(IrOpcode::CmpNeI64, 0);
  const size_t skipEmpty = instructions.size();
  emit(IrOpcode::JumpIfZero, 0);
  if (elementNeedsDestroy) {
    const int32_t countLocal = allocTempLocal();
    const int32_t indexLocal = allocTempLocal();
    const int32_t elementPtrLocal = allocTempLocal();
    pushFieldAddress(1);
    emit(IrOpcode::LoadIndirect, 0);
    emit(IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal));
    emit(IrOpcode::PushI32, 0);
    emit(IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal));
    const size_t loopStart = instructions.size();
    emit(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
    emit(IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal));
    emit(IrOpcode::CmpLtI32, 0);
    const size_t loopExit = instructions.size();
    emit(IrOpcode::JumpIfZero, 0);
    emit(IrOpcode::LoadLocal, static_cast<uint64_t>(dataLocal));
    emit(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
    emit(IrOpcode::PushI32, static_cast<uint64_t>(elementSlots * IrSlotBytesI32));
    emit(IrOpcode::MulI32, 0);
    emit(IrOpcode::AddI64, 0);
    emit(IrOpcode::StoreLocal, static_cast<uint64_t>(elementPtrLocal));
    if (!emitStructDestroyHelpersAtDepth(elementPtrLocal,
                                         elementStructPath,
                                         findDestroyHelper,
                                         resolveStructSlotLayout,
                                         allocTempLocal,
                                         instructions,
                                         localsIn,
                                         emitInlineDefinitionCall,
                                         error,
                                         depth + 1)) {
      return false;
    }
    emit(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
    emit(IrOpcode::PushI32, 1);
    emit(IrOpcode::AddI32, 0);
    emit(IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal));
    emit(IrOpcode::Jump, static_cast<uint64_t>(loopStart));
    instructions[loopExit].imm = static_cast<uint64_t>(instructions.size());
  }
  emit(IrOpcode::LoadLocal, static_cast<uint64_t>(dataLocal));
  emit(IrOpcode::HeapFree, 0);
  instructions[skipEmpty].imm = static_cast<uint64_t>(instructions.size());
  storeZeroField(4);
  storeZeroField(3);
  instructions[skipNotOwned].imm = static_cast<uint64_t>(instructions.size());
  return true;
}

bool emitStructDestroyHelpersAtDepth(
    int32_t valuePtrLocal,
    const std::string &structPath,
    const std::function<const Definition *(const std::string &)> &findDestroyHelper,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const Int32ProviderFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)>
        &emitInlineDefinitionCall,
    std::string &error,
    int depth) {
  // A builtin vector element of a builtin vector frees its buffer.
  if (std::string innerElementPath;
      isNestedBuiltinVectorElementPath(structPath, &innerElementPath)) {
    return depth > 16 || emitBuiltinVectorDestroyAtDepth(valuePtrLocal,
                                                         innerElementPath,
                                                         findDestroyHelper,
                                                         resolveStructSlotLayout,
                                                         allocTempLocal,
                                                         instructions,
                                                         localsIn,
                                                         emitInlineDefinitionCall,
                                                         error,
                                                         depth + 1);
  }
  // A user `Destroy` helper runs first; the fields it leaves are destroyed after it, last
  // declared first (docs/spec/value-lifecycle.md).
  if (const Definition *destroyHelper = findDestroyHelper(structPath);
      destroyHelper != nullptr &&
      !emitDestroyHelperFromPtr(
          valuePtrLocal, structPath, destroyHelper, localsIn, emitInlineDefinitionCall, error)) {
    return false;
  }
  StructSlotLayoutInfo layout;
  if (depth > 16 || !resolveStructSlotLayout(structPath, layout)) {
    return true;
  }
  for (auto fieldIt = layout.fields.rbegin(); fieldIt != layout.fields.rend(); ++fieldIt) {
    const bool builtinVector = isCopyableBuiltinVectorField(*fieldIt);
    if (!builtinVector &&
        !structNeedsCopyHelpers(
            fieldIt->structPath, findDestroyHelper, resolveStructSlotLayout, depth + 1, true)) {
      continue;
    }
    const int32_t fieldPtrLocal = allocTempLocal();
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valuePtrLocal)});
    instructions.push_back(
        {IrOpcode::PushI64, static_cast<uint64_t>(fieldIt->slotOffset) * IrSlotBytes});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(fieldPtrLocal)});
    const bool destroyed =
        builtinVector ? emitBuiltinVectorDestroyAtDepth(fieldPtrLocal,
                                                        fieldIt->builtinVectorElementStructPath,
                                                        findDestroyHelper,
                                                        resolveStructSlotLayout,
                                                        allocTempLocal,
                                                        instructions,
                                                        localsIn,
                                                        emitInlineDefinitionCall,
                                                        error,
                                                        depth + 1)
                      : emitStructDestroyHelpersAtDepth(fieldPtrLocal,
                                                        fieldIt->structPath,
                                                        findDestroyHelper,
                                                        resolveStructSlotLayout,
                                                        allocTempLocal,
                                                        instructions,
                                                        localsIn,
                                                        emitInlineDefinitionCall,
                                                        error,
                                                        depth + 1);
    if (!destroyed) {
      return false;
    }
  }
  // A sum destroys the payload of its active variant.
  return emitForActiveSumPayload(
      valuePtrLocal,
      layout.sumPayloads,
      instructions,
      [&](const SumPayloadLayoutInfo &payload) {
        return structNeedsCopyHelpers(
            payload.lifecyclePath, findDestroyHelper, resolveStructSlotLayout, depth + 1, true);
      },
      [&](const SumPayloadLayoutInfo &payload) {
        return emitStructDestroyHelpersAtDepth(
            emitSumPayloadAddress(valuePtrLocal, allocTempLocal, instructions),
            payload.lifecyclePath,
            findDestroyHelper,
            resolveStructSlotLayout,
            allocTempLocal,
            instructions,
            localsIn,
            emitInlineDefinitionCall,
            error,
            depth + 1);
      });
}

} // namespace

const Definition *
findStackDestroyHelper(const std::unordered_map<std::string, const Definition *> &defMap,
                       const std::string &structPath) {
  for (const char *helperName : {"/DestroyStack", "/Destroy"}) {
    auto helperIt = defMap.find(structPath + helperName);
    if (helperIt == defMap.end() || helperIt->second == nullptr) {
      continue;
    }
    const Definition &helper = *helperIt->second;
    return helper.statements.empty() && !helper.returnExpr.has_value() ? nullptr : &helper;
  }
  return nullptr;
}

bool parameterHasTransform(const Expr &param, std::string_view name) {
  for (const Transform &transform : param.transforms) {
    if (transform.name == name) {
      return true;
    }
  }
  return false;
}

void emitReleaseDropFlag(const LocalMap &locals,
                         const std::string &name,
                         const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  auto localIt = locals.find(name);
  if (localIt != locals.end() && localIt->second.dropFlagLocal >= 0) {
    emitInstruction(IrOpcode::PushI32, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(localIt->second.dropFlagLocal));
  }
}

bool structNeedsDestroyHelpers(
    const std::string &structPath,
    const std::function<const Definition *(const std::string &)> &findDestroyHelper,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)>
        &resolveStructSlotLayout) {
  return structNeedsCopyHelpers(structPath, findDestroyHelper, resolveStructSlotLayout, 0, true);
}

bool emitStructDestroyHelpersFromPtr(
    int32_t valuePtrLocal,
    const std::string &structPath,
    const std::function<const Definition *(const std::string &)> &findDestroyHelper,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const Int32ProviderFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)>
        &emitInlineDefinitionCall,
    std::string &error) {
  return emitStructDestroyHelpersAtDepth(valuePtrLocal,
                                         structPath,
                                         findDestroyHelper,
                                         resolveStructSlotLayout,
                                         allocTempLocal,
                                         instructions,
                                         localsIn,
                                         emitInlineDefinitionCall,
                                         error,
                                         0);
}

bool emitStructCopyHelpersFromPtrs(
    int32_t destPtrLocal,
    int32_t srcPtrLocal,
    const std::string &structPath,
    const std::function<const Definition *(const std::string &)> &findCopyHelper,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const Int32ProviderFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)>
        &emitInlineDefinitionCall,
    bool &ranHelper,
    std::string &error) {
  ranHelper = false;
  return emitStructCopyHelpersAtDepth(destPtrLocal,
                                      srcPtrLocal,
                                      structPath,
                                      findCopyHelper,
                                      resolveStructSlotLayout,
                                      allocTempLocal,
                                      instructions,
                                      localsIn,
                                      emitInlineDefinitionCall,
                                      ranHelper,
                                      error,
                                      0);
}

bool emitStructFieldCopyHelpersFromPtrs(
    int32_t destPtrLocal,
    int32_t srcPtrLocal,
    const StructSlotFieldInfo &field,
    const std::function<const Definition *(const std::string &)> &findCopyHelper,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const Int32ProviderFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)>
        &emitInlineDefinitionCall,
    bool &ranHelper,
    std::string &error) {
  if (!isCopyableBuiltinVectorField(field)) {
    return emitStructCopyHelpersFromPtrs(destPtrLocal,
                                         srcPtrLocal,
                                         field.structPath,
                                         findCopyHelper,
                                         resolveStructSlotLayout,
                                         allocTempLocal,
                                         instructions,
                                         localsIn,
                                         emitInlineDefinitionCall,
                                         ranHelper,
                                         error);
  }
  ranHelper = false;
  return emitBuiltinVectorCopyAtDepth(destPtrLocal,
                                      srcPtrLocal,
                                      field.builtinVectorElementStructPath,
                                      findCopyHelper,
                                      resolveStructSlotLayout,
                                      allocTempLocal,
                                      instructions,
                                      localsIn,
                                      emitInlineDefinitionCall,
                                      ranHelper,
                                      error,
                                      0);
}

bool emitBuiltinVectorCopyFromPtrs(
    int32_t destPtrLocal,
    int32_t srcPtrLocal,
    const std::string &elementStructPath,
    const std::function<const Definition *(const std::string &)> &findCopyHelper,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const Int32ProviderFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)>
        &emitInlineDefinitionCall,
    std::string &error) {
  bool ranHelper = false;
  return emitBuiltinVectorCopyAtDepth(destPtrLocal,
                                      srcPtrLocal,
                                      elementStructPath,
                                      findCopyHelper,
                                      resolveStructSlotLayout,
                                      allocTempLocal,
                                      instructions,
                                      localsIn,
                                      emitInlineDefinitionCall,
                                      ranHelper,
                                      error,
                                      0);
}

bool structNeedsCopyHelpersOrVectorCopies(
    const std::string &structPath,
    const std::function<const Definition *(const std::string &)> &findCopyHelper,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)>
        &resolveStructSlotLayout) {
  return structNeedsCopyHelpers(structPath, findCopyHelper, resolveStructSlotLayout, 0, true);
}

bool emitStructCopyFromPtrs(std::vector<IrInstruction> &instructions,
                            int32_t destPtrLocal,
                            int32_t srcPtrLocal,
                            int32_t slotCount) {
  if (slotCount <= 0) {
    return true;
  }
  for (int32_t i = 0; i < slotCount; ++i) {
    const uint64_t offsetBytes = static_cast<uint64_t>(i * 16);
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
    if (offsetBytes != 0) {
      instructions.push_back({IrOpcode::PushI64, offsetBytes});
      instructions.push_back({IrOpcode::AddI64, 0});
    }
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(srcPtrLocal)});
    if (offsetBytes != 0) {
      instructions.push_back({IrOpcode::PushI64, offsetBytes});
      instructions.push_back({IrOpcode::AddI64, 0});
    }
    instructions.push_back({IrOpcode::LoadIndirect, 0});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
  }
  return true;
}

bool emitStructCopySlots(std::vector<IrInstruction> &instructions,
                         int32_t destBaseLocal,
                         int32_t srcPtrLocal,
                         int32_t slotCount,
                         const std::function<int32_t()> &allocTempLocal) {
  const int32_t destPtrLocal = allocTempLocal();
  instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(destBaseLocal)});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
  return emitStructCopyFromPtrs(instructions, destPtrLocal, srcPtrLocal, slotCount);
}

bool emitDisarmTemporaryStructAfterCopy(
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    int32_t srcPtrLocal,
    const std::string &structPath) {
  auto emitStoreFalseAtOffset = [&](uint64_t offsetBytes) {
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(srcPtrLocal));
    if (offsetBytes != 0) {
      emitInstruction(IrOpcode::PushI64, offsetBytes);
      emitInstruction(IrOpcode::AddI64, 0);
    }
    emitInstruction(IrOpcode::PushI32, 0);
    emitInstruction(IrOpcode::StoreIndirect, 0);
    emitInstruction(IrOpcode::Pop, 0);
  };

  auto generatedStructLeaf = [](const std::string &path) {
    const size_t leafStart = path.find_last_of('/');
    std::string leaf = path.substr(leafStart == std::string::npos ? 0 : leafStart + 1);
    const size_t suffixStart = leaf.find("__");
    if (suffixStart != std::string::npos) {
      leaf.erase(suffixStart);
    }
    return leaf;
  };

  if (structPath.rfind(collection_helpers::kCanonicalVectorVectorType, 0) == 0) {
    emitStoreFalseAtOffset(4ull * IrSlotBytes);
    return true;
  }

  if (isExperimentalCollectionTypeName(structPath, "vector", "Vector")) {
    emitStoreFalseAtOffset(3ull * IrSlotBytes);
    return true;
  }

  const std::string leaf = generatedStructLeaf(structPath);
  if (structPath.rfind(collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "SoaColumns"), 0) == 0 ||
      leaf.rfind("SoaColumns", 0) == 0) {
    constexpr std::string_view Prefix = "SoaColumns";
    size_t cursor = Prefix.size();
    int columnCount = 0;
    while (cursor < leaf.size() && leaf[cursor] >= '0' && leaf[cursor] <= '9') {
      columnCount = columnCount * 10 + static_cast<int>(leaf[cursor] - '0');
      ++cursor;
    }
    if (columnCount >= 2 && columnCount <= 16) {
      for (int column = 0; column < columnCount; ++column) {
        emitStoreFalseAtOffset(static_cast<uint64_t>(1 + column * 5 + 4) * IrSlotBytes);
      }
    }
    return true;
  }

  if (structPath.rfind(collection_paths::memberPath(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName), 0) == 0 ||
      leaf == "SoaVector") {
    emitStoreFalseAtOffset(5ull * IrSlotBytes);
    return true;
  }

  if (structPath.rfind(collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, collection_paths::kSoaColumnTypeName), 0) == 0 ||
      leaf == "SoaColumn") {
    emitStoreFalseAtOffset(4ull * IrSlotBytes);
    return true;
  }

  if (structPath.rfind(collection_helpers::kCanonicalMapMapValueType, 0) == 0) {
    emitStoreFalseAtOffset(5ull * IrSlotBytes);
    emitStoreFalseAtOffset(10ull * IrSlotBytes);
    return true;
  }

  if (isKeyValueStorageStructPath(structPath)) {
    emitStoreFalseAtOffset(3ull * IrSlotBytes);
    emitStoreFalseAtOffset(7ull * IrSlotBytes);
    return true;
  }
  return false;
}

bool shouldDisarmStructCopySourceExpr(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  if (expr.isFieldAccess) {
    return false;
  }
  if (isSimpleCallName(expr, "dereference") || isSimpleCallName(expr, "location")) {
    return false;
  }
  // `try(result)` on a named Result reads the payload out of the Result's own storage, which keeps owning it: the
  // value is a copy of a place, not a temporary.
  if (isSimpleCallName(expr, "try") && expr.args.size() == 1 && expr.args.front().kind == Expr::Kind::Name) {
    return false;
  }
  // `wait(task)` reads the result out of the task binding, which owns it (docs/spec/vm-design.md, Tasks).
  if (isSimpleCallName(expr, "wait") && expr.args.size() == 1 && !expr.isMethodCall &&
      expr.args.front().kind == Expr::Kind::Name) {
    return false;
  }
  // An element access (`v[i]`, `at(v, i)`, `v.at(i)`) reads a place: the slots it yields are
  // the element's own, which the container keeps owning, so a copy of them runs the Copy
  // helpers instead of disarming the element (docs/spec/value-lifecycle.md, Copies).
  if (isElementAccessCopySourceExpr(expr)) {
    return false;
  }
  return true;
}

bool isTemporaryStructCopySourceExpr(
    const Expr &expr, const std::function<const Definition *(const Expr &)> &resolveCallee) {
  if (isElementAccessCopySourceExpr(expr)) {
    return resolveCallee && resolveCallee(expr) != nullptr;
  }
  return shouldDisarmStructCopySourceExpr(expr);
}

bool isElementAccessCopySourceExpr(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.isFieldAccess || expr.args.size() != 2) {
    return false;
  }
  std::string accessName;
  return getBuiltinArrayAccessName(expr, accessName) || isSimpleCallName(expr, "at") ||
         isSimpleCallName(expr, "at_unsafe");
}

// An argument that produces a value nobody else owns (a constructor, or a call returning a value
// rather than a reference or pointer), which the callee takes over instead of borrowing: not a
// field, `dereference`/`location`, `move(x)` of a binding, a builtin element access or a method
// call result.
bool isOwnedTemporaryArgumentExpr(
    const Expr &expr,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall) {
  // `block() { temporary }` yields that temporary: a lambda converted to a `Function` boxes
  // `block() { /closure_make(captures) }` (docs/spec/value-lifecycle.md, Lambdas).
  if (isSimpleCallName(expr, "block") && expr.args.empty() && expr.bodyArguments.size() == 1) {
    return isOwnedTemporaryArgumentExpr(expr.bodyArguments.front(), resolveDefinitionCall);
  }
  if (!shouldDisarmStructCopySourceExpr(expr) || isSimpleCallName(expr, "move") ||
      isSimpleCallName(expr, "borrow") || isSimpleCallName(expr, "at") ||
      isSimpleCallName(expr, "at_unsafe")) {
    return false;
  }
  if (expr.isBraceConstructor) {
    return true;
  }
  // A `pick` expression builds a result of its own, which owns what its arm yielded (`tryEmitPickExpr`).
  if (isSimpleCallName(expr, "pick") && expr.args.size() == 1 && !expr.bodyArguments.empty()) {
    return true;
  }
  // The builtin vector constructor `vector<T>(...)` has no definition and makes a vector nobody else owns.
  if (std::string collectionName; !expr.isMethodCall &&
                                  getBuiltinCollectionName(expr, collectionName) &&
                                  collectionName == "vector") {
    return true;
  }
  const Definition *callee =
      (expr.isMethodCall || !resolveDefinitionCall) ? nullptr : resolveDefinitionCall(expr);
  if (callee == nullptr) {
    return false;
  }
  for (const auto &transform : callee->transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(
            trimTemplateTypeText(transform.templateArgs.front()), base, argText)) {
      const std::string normalizedBase = trimTemplateTypeText(base);
      return normalizedBase != "Reference" && normalizedBase != "Pointer";
    }
  }
  return true;
}

bool isFreshBuiltinVectorExpr(
    const Expr &expr,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall) {
  if (expr.kind != Expr::Kind::Call || expr.isFieldAccess || expr.isMethodCall) {
    return false;
  }
  std::string collectionName;
  if (getBuiltinCollectionName(expr, collectionName)) {
    return collectionName == "vector";
  }
  // A `pick` expression that yields a vector builds a record of its own (`tryEmitPickExpr`).
  if (isSimpleCallName(expr, "pick") && expr.args.size() == 1 && !expr.bodyArguments.empty()) {
    return true;
  }
  const Definition *callee = resolveDefinitionCall ? resolveDefinitionCall(expr) : nullptr;
  if (callee == nullptr) {
    return false;
  }
  if (callee->fullPath.rfind("/std/", 0) == 0) {
    std::string leaf = callee->fullPath.substr(callee->fullPath.find_last_of('/') + 1);
    leaf.erase(std::min(leaf.find("__"), leaf.size()));
    return callee->fullPath.rfind(vectorBackingMemberRoot(), 0) == 0 && leaf == "vector";
  }
  for (const auto &transform : callee->transforms) {
    std::string base;
    std::string argText;
    if (transform.name == "return" && transform.templateArgs.size() == 1 &&
        splitTemplateTypeName(
            trimTemplateTypeText(transform.templateArgs.front()), base, argText)) {
      return normalizeCollectionBindingTypeName(trimTemplateTypeText(base)) == "vector";
    }
  }
  return false;
}

} // namespace primec::ir_lowerer
