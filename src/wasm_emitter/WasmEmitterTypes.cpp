#include "WasmEmitterInternal.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace primec {

namespace {

constexpr uint8_t WasmOpLocalGet = 0x20;
constexpr uint8_t WasmOpLocalSet = 0x21;
constexpr uint8_t WasmOpLocalTee = 0x22;
constexpr uint8_t WasmOpI64Const = 0x42;
constexpr uint8_t WasmOpI64Ne = 0x52;
constexpr uint8_t WasmOpI32WrapI64 = 0xa7;
constexpr uint8_t WasmOpI64ExtendI32S = 0xac;

bool isInteger(uint8_t type) {
  return type == WasmTypeI32 || type == WasmTypeI64;
}

// The operand and result types of an instruction with a fixed signature; operands run from the bottom of the stack to
// the top. False for an instruction the typed emitter handles itself or does not know.
struct OpSignature {
  std::vector<uint8_t> operands;
  uint8_t result = WasmTypeUnknown;
};

bool opSignature(IrOpcode op, OpSignature &sig) {
  constexpr uint8_t I32 = WasmTypeI32;
  constexpr uint8_t I64 = WasmTypeI64;
  constexpr uint8_t F32 = WasmTypeF32;
  constexpr uint8_t F64 = WasmTypeF64;
  switch (op) {
  case IrOpcode::PushI32:
    sig = {{}, I32};
    return true;
  case IrOpcode::PushI64:
    sig = {{}, I64};
    return true;
  case IrOpcode::PushF32:
    sig = {{}, F32};
    return true;
  case IrOpcode::PushF64:
    sig = {{}, F64};
    return true;
  case IrOpcode::AddI32:
  case IrOpcode::SubI32:
  case IrOpcode::MulI32:
  case IrOpcode::DivI32:
    sig = {{I32, I32}, I32};
    return true;
  case IrOpcode::NegI32:
  case IrOpcode::SextI32:
    sig = {{I32}, I32};
    return true;
  case IrOpcode::AddI64:
  case IrOpcode::SubI64:
  case IrOpcode::MulI64:
  case IrOpcode::DivI64:
  case IrOpcode::DivU64:
    sig = {{I64, I64}, I64};
    return true;
  case IrOpcode::NegI64:
    sig = {{I64}, I64};
    return true;
  case IrOpcode::CmpEqI32:
  case IrOpcode::CmpNeI32:
  case IrOpcode::CmpLtI32:
  case IrOpcode::CmpLeI32:
  case IrOpcode::CmpGtI32:
  case IrOpcode::CmpGeI32:
    sig = {{I32, I32}, I32};
    return true;
  case IrOpcode::CmpEqI64:
  case IrOpcode::CmpNeI64:
  case IrOpcode::CmpLtI64:
  case IrOpcode::CmpLeI64:
  case IrOpcode::CmpGtI64:
  case IrOpcode::CmpGeI64:
  case IrOpcode::CmpLtU64:
  case IrOpcode::CmpLeU64:
  case IrOpcode::CmpGtU64:
  case IrOpcode::CmpGeU64:
    sig = {{I64, I64}, I32};
    return true;
  case IrOpcode::AddF32:
  case IrOpcode::SubF32:
  case IrOpcode::MulF32:
  case IrOpcode::DivF32:
    sig = {{F32, F32}, F32};
    return true;
  case IrOpcode::NegF32:
    sig = {{F32}, F32};
    return true;
  case IrOpcode::AddF64:
  case IrOpcode::SubF64:
  case IrOpcode::MulF64:
  case IrOpcode::DivF64:
    sig = {{F64, F64}, F64};
    return true;
  case IrOpcode::NegF64:
    sig = {{F64}, F64};
    return true;
  case IrOpcode::FmaF32:
    sig = {{F32, F32, F32}, F32};
    return true;
  case IrOpcode::FmaF64:
    sig = {{F64, F64, F64}, F64};
    return true;
  case IrOpcode::CmpEqF32:
  case IrOpcode::CmpNeF32:
  case IrOpcode::CmpLtF32:
  case IrOpcode::CmpLeF32:
  case IrOpcode::CmpGtF32:
  case IrOpcode::CmpGeF32:
    sig = {{F32, F32}, I32};
    return true;
  case IrOpcode::CmpEqF64:
  case IrOpcode::CmpNeF64:
  case IrOpcode::CmpLtF64:
  case IrOpcode::CmpLeF64:
  case IrOpcode::CmpGtF64:
  case IrOpcode::CmpGeF64:
    sig = {{F64, F64}, I32};
    return true;
  case IrOpcode::ConvertI32ToF32:
    sig = {{I32}, F32};
    return true;
  case IrOpcode::ConvertI32ToF64:
    sig = {{I32}, F64};
    return true;
  case IrOpcode::ConvertI64ToF32:
  case IrOpcode::ConvertU64ToF32:
    sig = {{I64}, F32};
    return true;
  case IrOpcode::ConvertI64ToF64:
  case IrOpcode::ConvertU64ToF64:
    sig = {{I64}, F64};
    return true;
  case IrOpcode::ConvertF32ToI32:
    sig = {{F32}, I32};
    return true;
  case IrOpcode::ConvertF64ToI32:
    sig = {{F64}, I32};
    return true;
  case IrOpcode::ConvertF32ToI64:
  case IrOpcode::ConvertF32ToU64:
    sig = {{F32}, I64};
    return true;
  case IrOpcode::ConvertF64ToI64:
  case IrOpcode::ConvertF64ToU64:
    sig = {{F64}, I64};
    return true;
  case IrOpcode::ConvertF32ToF64:
    sig = {{F32}, F64};
    return true;
  case IrOpcode::ConvertF64ToF32:
    sig = {{F64}, F32};
    return true;
  case IrOpcode::PushArgc:
    sig = {{}, I32};
    return true;
  case IrOpcode::PrintI32:
    sig = {{I32}, WasmTypeUnknown};
    return true;
  case IrOpcode::PrintI64:
  case IrOpcode::PrintU64:
  case IrOpcode::PrintStringDynamic:
    sig = {{I64}, WasmTypeUnknown};
    return true;
  case IrOpcode::PrintString:
    sig = {{}, WasmTypeUnknown};
    return true;
  case IrOpcode::PrintArgv:
  case IrOpcode::PrintArgvUnsafe:
    sig = {{I32}, WasmTypeUnknown};
    return true;
  case IrOpcode::FileOpenRead:
  case IrOpcode::FileOpenWrite:
  case IrOpcode::FileOpenAppend:
    sig = {{}, I32};
    return true;
  case IrOpcode::FileOpenReadDynamic:
  case IrOpcode::FileOpenWriteDynamic:
  case IrOpcode::FileOpenAppendDynamic:
    sig = {{I64}, I32};
    return true;
  case IrOpcode::FileClose:
  case IrOpcode::FileReadByte:
  case IrOpcode::FileFlush:
  case IrOpcode::FileWriteString:
  case IrOpcode::FileWriteNewline:
    sig = {{I32}, I32};
    return true;
  case IrOpcode::FileWriteStringDynamic:
  case IrOpcode::FileWriteI64:
  case IrOpcode::FileWriteU64:
    sig = {{I32, I64}, I32};
    return true;
  case IrOpcode::FileWriteByte:
  case IrOpcode::FileWriteI32:
    sig = {{I32, I32}, I32};
    return true;
  case IrOpcode::ReturnI32:
    sig = {{I32}, WasmTypeUnknown};
    return true;
  case IrOpcode::ReturnI64:
    sig = {{I64}, WasmTypeUnknown};
    return true;
  case IrOpcode::ReturnF32:
    sig = {{F32}, WasmTypeUnknown};
    return true;
  case IrOpcode::ReturnF64:
    sig = {{F64}, WasmTypeUnknown};
    return true;
  case IrOpcode::ReturnVoid:
    sig = {{}, WasmTypeUnknown};
    return true;
  default:
    return false;
  }
}

// The type of the value `depth` places below the top of the tracked stack (0 is the top), unknown when the stack is
// shorter (a value the emitter cannot see, such as one pushed outside a structured block).
uint8_t stackType(const WasmLocalLayout &layout, size_t depth) {
  const std::vector<uint8_t> &stack = layout.typeState.stack;
  if (depth >= stack.size()) {
    return WasmTypeUnknown;
  }
  return stack[stack.size() - 1 - depth];
}

void popTypes(WasmLocalLayout &layout, size_t count) {
  std::vector<uint8_t> &stack = layout.typeState.stack;
  stack.resize(stack.size() > count ? stack.size() - count : 0);
}

void pushType(WasmLocalLayout &layout, uint8_t type) {
  layout.typeState.stack.push_back(type);
}

uint32_t acquireScratch(WasmLocalLayout &layout, uint8_t type) {
  if (type == WasmTypeUnknown) {
    type = WasmTypeI32;
  }
  for (WasmScratchSlot &slot : layout.typeState.scratch) {
    if (!slot.inUse && slot.type == type) {
      slot.inUse = true;
      return slot.index;
    }
  }
  WasmScratchSlot slot;
  slot.type = type;
  slot.index = layout.totalLocals + static_cast<uint32_t>(layout.typeState.scratch.size());
  slot.inUse = true;
  layout.typeState.scratch.push_back(slot);
  return slot.index;
}

void releaseScratch(WasmLocalLayout &layout) {
  for (WasmScratchSlot &slot : layout.typeState.scratch) {
    slot.inUse = false;
  }
}

// Converts the value on top of the stack from one type to another: narrowing an i64 wraps modulo 2^32, widening an i32
// sign-extends (an i32 is a signed 32-bit value in a 64-bit IR slot).
bool emitConversion(uint8_t from,
                    uint8_t to,
                    std::vector<uint8_t> &out,
                    std::string &error,
                    const std::string &fn) {
  if (from == WasmTypeUnknown || to == WasmTypeUnknown || from == to) {
    return true;
  }
  if (from == WasmTypeI64 && to == WasmTypeI32) {
    out.push_back(WasmOpI32WrapI64);
    return true;
  }
  if (from == WasmTypeI32 && to == WasmTypeI64) {
    out.push_back(WasmOpI64ExtendI32S);
    return true;
  }
  error = "wasm emitter value type mismatch in function: " + fn;
  return false;
}

bool needsConversion(uint8_t from, uint8_t to) {
  return from != WasmTypeUnknown && to != WasmTypeUnknown && from != to;
}

// Converts the top `wanted.size()` stack values (bottom to top) to the wanted types. A mismatch below the top parks the
// values above it in scratch locals while it is converted.
bool coerceOperands(WasmLocalLayout &layout,
                    const std::vector<uint8_t> &wanted,
                    std::vector<uint8_t> &out,
                    std::string &error,
                    const std::string &fn) {
  const size_t count = wanted.size();
  std::vector<uint8_t> actual(count);
  size_t firstMismatch = count;
  for (size_t k = 0; k < count; ++k) {
    actual[k] = stackType(layout, count - 1 - k);
    if (needsConversion(actual[k], wanted[k]) && firstMismatch == count) {
      firstMismatch = k;
    }
  }
  if (firstMismatch == count) {
    return true;
  }
  for (size_t k = firstMismatch; k < count; ++k) {
    if (needsConversion(actual[k], wanted[k]) && !(isInteger(actual[k]) && isInteger(wanted[k]))) {
      error = "wasm emitter value type mismatch in function: " + fn;
      return false;
    }
  }
  std::vector<uint32_t> parked;
  if (firstMismatch + 1 == count) {
    if (!emitConversion(actual[firstMismatch], wanted[firstMismatch], out, error, fn)) {
      return false;
    }
  } else {
    parked.assign(count, 0);
    for (size_t k = count; k-- > firstMismatch;) {
      parked[k] = acquireScratch(layout, actual[k]);
      out.push_back(WasmOpLocalSet);
      appendU32Leb(parked[k], out);
    }
    for (size_t k = firstMismatch; k < count; ++k) {
      out.push_back(WasmOpLocalGet);
      appendU32Leb(parked[k], out);
      if (!emitConversion(actual[k], wanted[k], out, error, fn)) {
        return false;
      }
    }
  }
  std::vector<uint8_t> &stack = layout.typeState.stack;
  for (size_t k = firstMismatch; k < count; ++k) {
    const size_t position = stack.size() - (count - k);
    if (position < stack.size() && wanted[k] != WasmTypeUnknown) {
      stack[position] = wanted[k];
    }
  }
  return true;
}

// Records that a value of `type` is stored into local `index` (inference): the local becomes the wider type.
bool observeLocalStore(WasmLocalLayout &layout,
                       uint64_t index,
                       uint8_t type,
                       std::string &error,
                       const std::string &fn) {
  if (type == WasmTypeUnknown || index >= layout.localTypes.size()) {
    return true;
  }
  uint8_t merged = WasmTypeUnknown;
  if (!mergeWasmTypes(layout.localTypes[static_cast<size_t>(index)], type, merged)) {
    error = "wasm emitter local holds values of different kinds in function: " + fn;
    return false;
  }
  if (merged != layout.localTypes[static_cast<size_t>(index)]) {
    layout.localTypes[static_cast<size_t>(index)] = merged;
    layout.typeState.changed = true;
  }
  return true;
}

} // namespace

bool mergeWasmTypes(uint8_t current, uint8_t incoming, uint8_t &merged) {
  if (current == WasmTypeUnknown || current == incoming) {
    merged = incoming;
    return true;
  }
  if (incoming == WasmTypeUnknown) {
    merged = current;
    return true;
  }
  if (isInteger(current) && isInteger(incoming)) {
    merged = WasmTypeI64;
    return true;
  }
  return false;
}

bool coerceWasmTopValue(WasmLocalLayout &localLayout,
                        uint8_t type,
                        std::vector<uint8_t> &out,
                        std::string &error,
                        const std::string &functionName) {
  return coerceOperands(localLayout, {type}, out, error, functionName);
}

std::vector<uint8_t> enterWasmBlock(WasmLocalLayout &localLayout) {
  std::vector<uint8_t> outer;
  outer.swap(localLayout.typeState.stack);
  return outer;
}

void leaveWasmBlock(WasmLocalLayout &localLayout, std::vector<uint8_t> &&outerStack) {
  localLayout.typeState.stack = std::move(outerStack);
}

bool consumeWasmCondition(WasmLocalLayout &localLayout,
                          std::vector<uint8_t> &out,
                          std::string &error,
                          const std::string &functionName) {
  const uint8_t type = stackType(localLayout, 0);
  if (type == WasmTypeI64) {
    // A 64-bit condition is true when it is not zero.
    out.push_back(WasmOpI64Const);
    appendS64Leb(0, out);
    out.push_back(WasmOpI64Ne);
  } else if (type == WasmTypeF32 || type == WasmTypeF64) {
    error = "wasm emitter value type mismatch in function: " + functionName;
    return false;
  }
  popTypes(localLayout, 1);
  return true;
}

bool emitTypedInstruction(const IrInstruction &inst,
                          WasmLocalLayout &localLayout,
                          const std::vector<WasmFunctionType> &functionTypes,
                          const WasmRuntimeContext &runtime,
                          std::vector<uint8_t> &out,
                          std::string &error,
                          const std::string &functionName) {
  WasmTypeState &state = localLayout.typeState;
  releaseScratch(localLayout);
  switch (inst.op) {
  case IrOpcode::LoadLocal: {
    if (!emitSimpleInstruction(
            inst, localLayout, functionTypes, runtime, out, error, functionName)) {
      return false;
    }
    pushType(localLayout, localLayout.localTypes[static_cast<size_t>(inst.imm)]);
    return true;
  }
  case IrOpcode::StoreLocal: {
    if (inst.imm >= localLayout.irLocalCount) {
      error = "wasm emitter local index out of range in function: " + functionName;
      return false;
    }
    const uint8_t valueType = stackType(localLayout, 0);
    if (state.inferring) {
      if (!observeLocalStore(localLayout, inst.imm, valueType, error, functionName)) {
        return false;
      }
    } else if (!coerceOperands(localLayout,
                               {localLayout.localTypes[static_cast<size_t>(inst.imm)]},
                               out,
                               error,
                               functionName)) {
      return false;
    }
    if (!emitSimpleInstruction(
            inst, localLayout, functionTypes, runtime, out, error, functionName)) {
      return false;
    }
    popTypes(localLayout, 1);
    return true;
  }
  case IrOpcode::Pop:
    out.push_back(0x1a);
    popTypes(localLayout, 1);
    return true;
  case IrOpcode::Dup: {
    const uint8_t type = stackType(localLayout, 0);
    const uint32_t scratch = acquireScratch(localLayout, type);
    out.push_back(WasmOpLocalTee);
    appendU32Leb(scratch, out);
    out.push_back(WasmOpLocalGet);
    appendU32Leb(scratch, out);
    pushType(localLayout, type);
    return true;
  }
  case IrOpcode::Call:
  case IrOpcode::CallVoid: {
    if (inst.imm >= functionTypes.size()) {
      error = "wasm emitter invalid call target in function: " + functionName;
      return false;
    }
    const WasmFunctionType &callee = functionTypes[static_cast<size_t>(inst.imm)];
    const size_t argumentCount = callee.params.size();
    if (state.inferring) {
      if (state.observedParams != nullptr) {
        std::vector<uint8_t> &observed = (*state.observedParams)[static_cast<size_t>(inst.imm)];
        for (size_t k = 0; k < argumentCount && k < observed.size(); ++k) {
          const uint8_t argument = stackType(localLayout, argumentCount - 1 - k);
          uint8_t merged = WasmTypeUnknown;
          if (!mergeWasmTypes(observed[k], argument, merged)) {
            error =
                "wasm emitter call passes values of different kinds in function: " + functionName;
            return false;
          }
          if (merged != observed[k]) {
            observed[k] = merged;
            state.changed = true;
          }
        }
      }
    } else if (!coerceOperands(localLayout, callee.params, out, error, functionName)) {
      return false;
    }
    if (!emitSimpleInstruction(
            inst, localLayout, functionTypes, runtime, out, error, functionName)) {
      return false;
    }
    popTypes(localLayout, argumentCount);
    if (inst.op == IrOpcode::Call && !callee.results.empty()) {
      pushType(localLayout, callee.results[0]);
    }
    return true;
  }
  default:
    break;
  }

  OpSignature sig;
  if (!opSignature(inst.op, sig)) {
    return emitSimpleInstruction(
        inst, localLayout, functionTypes, runtime, out, error, functionName);
  }
  if (inst.op == IrOpcode::FileReadByte && inst.imm < localLayout.irLocalCount && state.inferring) {
    if (!observeLocalStore(localLayout, inst.imm, WasmTypeI32, error, functionName)) {
      return false;
    }
  }
  if (!coerceOperands(localLayout, sig.operands, out, error, functionName)) {
    return false;
  }
  if (!emitSimpleInstruction(inst, localLayout, functionTypes, runtime, out, error, functionName)) {
    return false;
  }
  popTypes(localLayout, sig.operands.size());
  if (sig.result != WasmTypeUnknown) {
    pushType(localLayout, sig.result);
  }
  const bool isReturn = inst.op == IrOpcode::ReturnI32 || inst.op == IrOpcode::ReturnI64 ||
                        inst.op == IrOpcode::ReturnF32 || inst.op == IrOpcode::ReturnF64 ||
                        inst.op == IrOpcode::ReturnVoid;
  if (isReturn) {
    state.stack.clear();
  }
  return true;
}

} // namespace primec
