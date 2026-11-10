#include "primec/ir_lowerer/IrLowererFlowHelpers.h"

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
// buffer; a destroy leaves them alone.
bool structNeedsCopyHelpers(
    const std::string &structPath,
    const std::function<const Definition *(const std::string &)> &findCopyHelper,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    int depth,
    bool withBuiltinVectors) {
  if (structPath.empty() || depth > 16) {
    return false;
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
  return false;
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
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)>
        &emitInlineDefinitionCall,
    std::string &error,
    int depth) {
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
    if (!structNeedsCopyHelpers(
            fieldIt->structPath, findDestroyHelper, resolveStructSlotLayout, depth + 1, false)) {
      continue;
    }
    const int32_t fieldPtrLocal = allocTempLocal();
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valuePtrLocal));
    emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(fieldIt->slotOffset) * IrSlotBytes);
    emitInstruction(IrOpcode::AddI64, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(fieldPtrLocal));
    if (!emitStructDestroyHelpersAtDepth(fieldPtrLocal,
                                         fieldIt->structPath,
                                         findDestroyHelper,
                                         resolveStructSlotLayout,
                                         allocTempLocal,
                                         emitInstruction,
                                         localsIn,
                                         emitInlineDefinitionCall,
                                         error,
                                         depth + 1)) {
      return false;
    }
  }
  return true;
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
  return structNeedsCopyHelpers(structPath, findDestroyHelper, resolveStructSlotLayout, 0, false);
}

bool emitStructDestroyHelpersFromPtr(
    int32_t valuePtrLocal,
    const std::string &structPath,
    const std::function<const Definition *(const std::string &)> &findDestroyHelper,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const Int32ProviderFn &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)>
        &emitInlineDefinitionCall,
    std::string &error) {
  return emitStructDestroyHelpersAtDepth(valuePtrLocal,
                                         structPath,
                                         findDestroyHelper,
                                         resolveStructSlotLayout,
                                         allocTempLocal,
                                         emitInstruction,
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
  if (!shouldDisarmStructCopySourceExpr(expr) || isSimpleCallName(expr, "move") ||
      isSimpleCallName(expr, "borrow") || isSimpleCallName(expr, "at") ||
      isSimpleCallName(expr, "at_unsafe")) {
    return false;
  }
  if (expr.isBraceConstructor) {
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

const char *resolveGpuBuiltinLocalName(const std::string &gpuBuiltin) {
  if (gpuBuiltin == "global_id_x") {
    return kGpuGlobalIdXName;
  }
  if (gpuBuiltin == "global_id_y") {
    return kGpuGlobalIdYName;
  }
  if (gpuBuiltin == "global_id_z") {
    return kGpuGlobalIdZName;
  }
  return nullptr;
}

bool emitGpuBuiltinLoad(
    const std::string &gpuBuiltin,
    const std::function<std::optional<int32_t>(const char *)> &resolveLocalIndex,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error) {
  const char *localName = resolveGpuBuiltinLocalName(gpuBuiltin);
  if (!localName) {
    error = "native backend does not support gpu builtin: " + gpuBuiltin;
    return false;
  }

  const std::optional<int32_t> localIndex = resolveLocalIndex(localName);
  if (!localIndex.has_value()) {
    error = "gpu builtin requires dispatch context: " + gpuBuiltin;
    return false;
  }

  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(*localIndex));
  return true;
}

bool resolveCountedLoopKind(LocalInfo::ValueKind inferredKind,
                            bool allowBool,
                            const char *errorMessage,
                            LocalInfo::ValueKind &countKindOut,
                            std::string &error) {
  countKindOut = inferredKind;
  if (allowBool && countKindOut == LocalInfo::ValueKind::Bool) {
    countKindOut = LocalInfo::ValueKind::Int32;
  }
  if (countKindOut == LocalInfo::ValueKind::Int32 ||
      countKindOut == LocalInfo::ValueKind::Int64 ||
      (countKindOut == LocalInfo::ValueKind::UInt64 || countKindOut == LocalInfo::ValueKind::UInt8)) {
    return true;
  }
  error = errorMessage;
  return false;
}

bool emitCountedLoopPrologue(
    LocalInfo::ValueKind countKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, int32_t)> &patchInstructionImm,
    const std::function<void()> &emitLoopCountNegative,
    CountedLoopControl &out,
    std::string &error) {
  if (countKind != LocalInfo::ValueKind::Int32 &&
      countKind != LocalInfo::ValueKind::Int64 &&
      (countKind != LocalInfo::ValueKind::UInt64 && countKind != LocalInfo::ValueKind::UInt8)) {
    error = "counted loop requires integer count";
    return false;
  }

  out = CountedLoopControl{};
  out.counterLocal = allocTempLocal();
  out.countKind = countKind;
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(out.counterLocal));

  if (emitLoopCountNegative) {
    if (countKind == LocalInfo::ValueKind::Int32) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(out.counterLocal));
      emitInstruction(IrOpcode::PushI32, 0);
      emitInstruction(IrOpcode::CmpLtI32, 0);
      const size_t jumpNonNegative = instructionCount();
      emitInstruction(IrOpcode::JumpIfZero, 0);
      emitLoopCountNegative();
      const size_t nonNegativeIndex = instructionCount();
      patchInstructionImm(jumpNonNegative, static_cast<int32_t>(nonNegativeIndex));
    } else if (countKind == LocalInfo::ValueKind::Int64) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(out.counterLocal));
      emitInstruction(IrOpcode::PushI64, 0);
      emitInstruction(IrOpcode::CmpLtI64, 0);
      const size_t jumpNonNegative = instructionCount();
      emitInstruction(IrOpcode::JumpIfZero, 0);
      emitLoopCountNegative();
      const size_t nonNegativeIndex = instructionCount();
      patchInstructionImm(jumpNonNegative, static_cast<int32_t>(nonNegativeIndex));
    }
  }

  out.checkIndex = instructionCount();
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(out.counterLocal));
  if (countKind == LocalInfo::ValueKind::Int32) {
    emitInstruction(IrOpcode::PushI32, 0);
    emitInstruction(IrOpcode::CmpGtI32, 0);
  } else if (countKind == LocalInfo::ValueKind::Int64) {
    emitInstruction(IrOpcode::PushI64, 0);
    emitInstruction(IrOpcode::CmpGtI64, 0);
  } else {
    emitInstruction(IrOpcode::PushI64, 0);
    emitInstruction(IrOpcode::CmpNeI64, 0);
  }
  out.jumpEndIndex = instructionCount();
  emitInstruction(IrOpcode::JumpIfZero, 0);
  return true;
}

void emitCountedLoopIterationStep(
    const CountedLoopControl &control,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(control.counterLocal));
  if (control.countKind == LocalInfo::ValueKind::Int32) {
    emitInstruction(IrOpcode::PushI32, 1);
    emitInstruction(IrOpcode::SubI32, 0);
  } else {
    emitInstruction(IrOpcode::PushI64, 1);
    emitInstruction(IrOpcode::SubI64, 0);
  }
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(control.counterLocal));
  emitInstruction(IrOpcode::Jump, static_cast<uint64_t>(control.checkIndex));
}

void patchCountedLoopEnd(
    const CountedLoopControl &control,
    const std::function<size_t()> &instructionCount,
    const std::function<void(size_t, int32_t)> &patchInstructionImm) {
  const size_t endIndex = instructionCount();
  patchInstructionImm(control.jumpEndIndex, static_cast<int32_t>(endIndex));
}

bool emitBodyStatements(
    const std::vector<Expr> &bodyStatements,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, LocalMap &)> &emitStatement) {
  LocalMap bodyLocals = localsIn;
  for (const auto &bodyStmt : bodyStatements) {
    if (!emitStatement(bodyStmt, bodyLocals)) {
      return false;
    }
  }
  return true;
}

bool emitBodyStatementsWithFileScope(
    const std::vector<Expr> &bodyStatements,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, LocalMap &)> &emitStatement,
    const std::function<bool()> &emitAfterBody,
    const std::function<void()> &pushFileScope,
    const std::function<void()> &emitCurrentFileScopeCleanup,
    const std::function<void()> &popFileScope) {
  pushFileScope();
  if (!emitBodyStatements(bodyStatements, localsIn, emitStatement)) {
    return false;
  }
  if (emitAfterBody && !emitAfterBody()) {
    return false;
  }
  emitCurrentFileScopeCleanup();
  popFileScope();
  return true;
}

bool declareForConditionBinding(
    const Expr &binding,
    LocalMap &locals,
    int32_t &nextLocal,
    const std::function<bool(const Expr &)> &isBindingMutable,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const std::function<bool(const Expr &)> &hasExplicitBindingTypeTransform,
    const std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)> &bindingValueKind,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const std::function<void(const Expr &, LocalInfo &)> &applyStructArrayInfo,
    const std::function<void(const Expr &, LocalInfo &)> &applyStructValueInfo,
    std::string &error) {
  if (binding.args.size() != 1) {
    error = "binding requires exactly one argument";
    return false;
  }
  if (locals.count(binding.name) > 0) {
    error = "binding redefines existing name: " + binding.name;
    return false;
  }

  LocalInfo info;
  info.index = nextLocal++;
  info.isMutable = isBindingMutable(binding);
  info.kind = bindingKind(binding);
  info.valueKind = LocalInfo::ValueKind::Unknown;
  info.keyValueKeyKind = LocalInfo::ValueKind::Unknown;
  info.keyValueValueKind = LocalInfo::ValueKind::Unknown;
  if (hasExplicitBindingTypeTransform(binding)) {
    info.valueKind = bindingValueKind(binding, info.kind);
  } else if (info.kind == LocalInfo::Kind::Value) {
    info.valueKind = inferExprKind(binding.args.front(), locals);
    if (info.valueKind == LocalInfo::ValueKind::Unknown) {
      std::string builtinComparison;
      if (getBuiltinComparisonName(binding.args.front(), builtinComparison)) {
        info.valueKind = LocalInfo::ValueKind::Bool;
      }
    }
    if (info.valueKind == LocalInfo::ValueKind::Unknown) {
      std::string inferredStruct = inferStructExprPath(binding.args.front(), locals);
      if (!inferredStruct.empty()) {
        info.structTypeName = inferredStruct;
      } else {
        info.valueKind = LocalInfo::ValueKind::Int32;
      }
    }
  }
  applyStructArrayInfo(binding, info);
  applyStructValueInfo(binding, info);
  if (info.structTypeName.empty() && info.kind == LocalInfo::Kind::Value) {
    std::string inferredStruct = inferStructExprPath(binding.args.front(), locals);
    if (!inferredStruct.empty()) {
      info.structTypeName = inferredStruct;
    }
  }
  if (info.kind == LocalInfo::Kind::Value && !info.structTypeName.empty()) {
    info.valueKind = LocalInfo::ValueKind::Int64;
  }
  locals.emplace(binding.name, info);
  return true;
}

bool emitForConditionBindingInit(
    const Expr &binding,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error) {
  if (binding.args.size() != 1) {
    error = "binding requires exactly one argument";
    return false;
  }
  auto it = localsIn.find(binding.name);
  if (it == localsIn.end()) {
    error = "binding missing local: " + binding.name;
    return false;
  }
  if (!emitExpr(binding.args.front(), localsIn)) {
    return false;
  }
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(it->second.index));
  return true;
}

} // namespace primec::ir_lowerer
