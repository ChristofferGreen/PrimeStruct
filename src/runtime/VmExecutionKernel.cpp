#include "primec/runtime/VmExecutionKernel.h"

#include "VmControlFlowOpcodeShared.h"
#include "VmFastKernel.h"
#include "primec/ir/IrPureSemantics.h"
#include "primec/runtime/VmStringLookup.h"
#include "primec/runtime/VmKernelBoundary.h"

#include <algorithm>
#include <utility>

namespace primec::vm_detail {

namespace {

// A local address names a slot of some live frame (docs/spec/vm-design.md, Real calls). Rewrites it to an address
// relative to that frame and selects its locals; a heap address, or a local address outside every live frame, is
// left as it is against the current frame's locals (the host then faults on the latter).
inline void localizeVmAddress(std::vector<VmKernelFrame> &frames,
                              uint64_t slotBytes,
                              uint64_t &address,
                              std::vector<uint64_t> *&locals) {
  VmKernelFrame &top = frames.back();
  locals = &top.locals;
  if ((address & (uint64_t{1} << 63)) != 0) {
    return;
  }
  const uint64_t index = address / slotBytes;
  for (size_t i = frames.size(); i-- > 0;) {
    VmKernelFrame &frame = frames[i];
    if (index >= frame.localsBase) {
      if (index - frame.localsBase < frame.locals.size()) {
        address -= static_cast<uint64_t>(frame.localsBase) * slotBytes;
        locals = &frame.locals;
      }
      return;
    }
  }
}

} // namespace

bool isVmKernelPrintOpcode(IrOpcode op) {
  switch (op) {
  case IrOpcode::PrintI32:
  case IrOpcode::PrintI64:
  case IrOpcode::PrintU64:
  case IrOpcode::PrintString:
  case IrOpcode::PrintStringDynamic:
  case IrOpcode::PrintArgv:
  case IrOpcode::PrintArgvUnsafe:
    return true;
  default:
    return false;
  }
}

bool isVmKernelFileOpcode(IrOpcode op) {
  switch (op) {
  case IrOpcode::FileOpenRead:
  case IrOpcode::FileOpenWrite:
  case IrOpcode::FileOpenAppend:
  case IrOpcode::FileOpenReadDynamic:
  case IrOpcode::FileOpenWriteDynamic:
  case IrOpcode::FileOpenAppendDynamic:
  case IrOpcode::FileClose:
  case IrOpcode::FileReadByte:
  case IrOpcode::FileFlush:
  case IrOpcode::FileWriteI32:
  case IrOpcode::FileWriteI64:
  case IrOpcode::FileWriteU64:
  case IrOpcode::FileWriteString:
  case IrOpcode::FileWriteStringDynamic:
  case IrOpcode::FileWriteByte:
  case IrOpcode::FileWriteNewline:
    return true;
  default:
    return false;
  }
}

size_t computeVmKernelLocalCount(const IrFunction &function) {
  size_t localCount = 0;
  for (const auto &inst : function.instructions) {
    if (inst.op == IrOpcode::LoadLocal || inst.op == IrOpcode::StoreLocal ||
        inst.op == IrOpcode::AddressOfLocal) {
      localCount = std::max(localCount, static_cast<size_t>(inst.imm) + 1);
    }
  }
  return localCount;
}

namespace {

// The single dispatch implementation: `executeVmKernel` loops over it and
// debug sessions call it through `stepVmKernel`. Defined in this unit so the
// run loop can inline it.
// `TrackEvents` is false for plain runs, which skip filling the step event.
template <bool TrackEvents>
[[gnu::always_inline]] inline VmKernelStepOutcome stepImpl(const IrModule &module,
                                                           VmKernelHost &host,
                                                           std::vector<uint64_t> &stack,
                                                           std::vector<VmKernelFrame> &frames,
                                                           const std::vector<size_t> &localCounts,
                                                           uint64_t &result,
                                                           VmKernelStepEvent &event,
                                                           std::string &error) {
  if constexpr (TrackEvents) {
    event = {};
  }
  VmKernelFrame &frame = frames.back();
  const IrFunction &fn = *frame.function;
  std::vector<uint64_t> &locals = frame.locals;
  size_t &ip = frame.ip;
  if (ip >= fn.instructions.size()) {
    if (frames.size() == 1) {
      error = "missing return in IR";
    } else {
      error = "missing return in IR function " + fn.name;
    }
    return VmKernelStepOutcome::Fault;
  }
  const auto &inst = fn.instructions[ip];
  const auto controlFlowOutcome =
      handleSharedVmControlFlowOpcode(inst,
                                      stack,
                                      fn.instructions.size(),
                                      module.functions.size(),
                                      frames.size(),
                                      host.maxCallDepth(),
                                      frame.returnValueToCaller,
                                      ip,
                                      error);
  if (controlFlowOutcome.result == VmControlFlowOpcodeResult::Fault) {
    return VmKernelStepOutcome::Fault;
  }
  if (controlFlowOutcome.result == VmControlFlowOpcodeResult::Continue) {
    return VmKernelStepOutcome::Continue;
  }
  if (controlFlowOutcome.result == VmControlFlowOpcodeResult::Call) {
    VmKernelFrame calleeFrame;
    calleeFrame.localsBase = frame.localsBase + locals.size();
    calleeFrame.functionIndex = controlFlowOutcome.targetFunctionIndex;
    calleeFrame.function = &module.functions[controlFlowOutcome.targetFunctionIndex];
    calleeFrame.locals.assign(localCounts[controlFlowOutcome.targetFunctionIndex], 0);
    calleeFrame.returnValueToCaller = controlFlowOutcome.returnValueToCaller;
    frames.push_back(std::move(calleeFrame));
    if constexpr (TrackEvents) {
      event.kind = VmKernelStepKind::Call;
      event.functionIndex = controlFlowOutcome.targetFunctionIndex;
      event.returnsValueToCaller = controlFlowOutcome.returnValueToCaller;
    }
    return VmKernelStepOutcome::Continue;
  }
  if (controlFlowOutcome.result == VmControlFlowOpcodeResult::Exit) {
    result = controlFlowOutcome.returnValue;
    if constexpr (TrackEvents) {
      event.kind = VmKernelStepKind::Exit;
      event.functionIndex = frame.functionIndex;
      event.returnsValueToCaller = controlFlowOutcome.returnValueToCaller;
    }
    return VmKernelStepOutcome::Exit;
  }
  if (controlFlowOutcome.result == VmControlFlowOpcodeResult::Return) {
    const bool returnToCaller = controlFlowOutcome.returnValueToCaller;
    if constexpr (TrackEvents) {
      event.kind = VmKernelStepKind::Return;
      event.functionIndex = frame.functionIndex;
      event.returnsValueToCaller = returnToCaller;
    }
    frames.pop_back();
    if (returnToCaller) {
      stack.push_back(controlFlowOutcome.returnValue);
    }
    return VmKernelStepOutcome::Continue;
  }
  switch (inst.op) {
  case IrOpcode::PushI32:
    stack.push_back(static_cast<uint64_t>(
        static_cast<int64_t>(static_cast<int32_t>(inst.imm))));
    ip += 1;
    return VmKernelStepOutcome::Continue;
  case IrOpcode::PushI64:
  case IrOpcode::PushF32:
  case IrOpcode::PushF64:
    stack.push_back(inst.imm);
    ip += 1;
    return VmKernelStepOutcome::Continue;
  case IrOpcode::PushArgc: {
    const int32_t count32 = static_cast<int32_t>(host.argumentCount());
    stack.push_back(static_cast<uint64_t>(static_cast<int64_t>(count32)));
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::LoadLocal: {
    if (static_cast<size_t>(inst.imm) >= locals.size()) {
      error = "invalid local index in IR";
      return VmKernelStepOutcome::Fault;
    }
    stack.push_back(locals[static_cast<size_t>(inst.imm)]);
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::StoreLocal: {
    if (static_cast<size_t>(inst.imm) >= locals.size()) {
      error = "invalid local index in IR";
      return VmKernelStepOutcome::Fault;
    }
    if (stack.empty()) {
      error = "IR stack underflow on store";
      return VmKernelStepOutcome::Fault;
    }
    locals[static_cast<size_t>(inst.imm)] = stack.back();
    stack.pop_back();
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::AddressOfLocal: {
    if (static_cast<size_t>(inst.imm) >= locals.size()) {
      error = "invalid local index in IR";
      return VmKernelStepOutcome::Fault;
    }
    stack.push_back((static_cast<uint64_t>(frame.localsBase) + static_cast<uint64_t>(inst.imm)) *
                    host.slotBytes());
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::LoadIndirect: {
    if (stack.empty()) {
      error = "IR stack underflow on load indirect";
      return VmKernelStepOutcome::Fault;
    }
    uint64_t address = stack.back();
    stack.pop_back();
    uint64_t *slot = nullptr;
    std::vector<uint64_t> *addressLocals = nullptr;
    localizeVmAddress(frames, host.slotBytes(), address, addressLocals);
    if (!host.resolveIndirectAddress(address, *addressLocals, slot, error)) {
      return VmKernelStepOutcome::Fault;
    }
    stack.push_back(*slot);
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::StoreIndirect: {
    if (stack.size() < 2) {
      error = "IR stack underflow on store indirect";
      return VmKernelStepOutcome::Fault;
    }
    const uint64_t value = stack.back();
    stack.pop_back();
    uint64_t address = stack.back();
    stack.pop_back();
    uint64_t *slot = nullptr;
    std::vector<uint64_t> *addressLocals = nullptr;
    localizeVmAddress(frames, host.slotBytes(), address, addressLocals);
    if (!host.resolveIndirectAddress(address, *addressLocals, slot, error)) {
      return VmKernelStepOutcome::Fault;
    }
    *slot = value;
    stack.push_back(value);
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::HeapAlloc: {
    if (stack.empty()) {
      error = "IR stack underflow on heap alloc";
      return VmKernelStepOutcome::Fault;
    }
    const uint64_t slotCount = stack.back();
    stack.pop_back();
    uint64_t address = 0;
    if (!host.allocateHeapSlots(slotCount, address, error)) {
      return VmKernelStepOutcome::Fault;
    }
    stack.push_back(address);
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::HeapAllocBytes: {
    if (stack.empty()) {
      error = "IR stack underflow on heap alloc bytes";
      return VmKernelStepOutcome::Fault;
    }
    const uint64_t byteCount = stack.back();
    stack.pop_back();
    uint64_t address = 0;
    if (!host.allocateHeapBytes(byteCount, address, error)) {
      return VmKernelStepOutcome::Fault;
    }
    stack.push_back(address);
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::LoadU8: {
    if (stack.empty()) {
      error = "IR stack underflow on load u8";
      return VmKernelStepOutcome::Fault;
    }
    uint64_t address = stack.back();
    stack.pop_back();
    uint8_t byte = 0;
    std::vector<uint64_t> *addressLocals = nullptr;
    localizeVmAddress(frames, host.slotBytes(), address, addressLocals);
    if (!host.loadHeapByte(address, *addressLocals, byte, error)) {
      return VmKernelStepOutcome::Fault;
    }
    stack.push_back(static_cast<uint64_t>(byte));
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::StoreU8: {
    if (stack.size() < 2) {
      error = "IR stack underflow on store u8";
      return VmKernelStepOutcome::Fault;
    }
    const uint64_t value = stack.back();
    stack.pop_back();
    uint64_t address = stack.back();
    stack.pop_back();
    const uint8_t byte = static_cast<uint8_t>(value & 0xFFull);
    std::vector<uint64_t> *addressLocals = nullptr;
    localizeVmAddress(frames, host.slotBytes(), address, addressLocals);
    if (!host.storeHeapByte(address, *addressLocals, byte, error)) {
      return VmKernelStepOutcome::Fault;
    }
    stack.push_back(static_cast<uint64_t>(byte));
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::HeapFree: {
    if (stack.empty()) {
      error = "IR stack underflow on heap free";
      return VmKernelStepOutcome::Fault;
    }
    const uint64_t address = stack.back();
    stack.pop_back();
    if (!host.freeHeapSlots(address, error)) {
      return VmKernelStepOutcome::Fault;
    }
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::HeapRealloc: {
    if (stack.size() < 2) {
      error = "IR stack underflow on heap realloc";
      return VmKernelStepOutcome::Fault;
    }
    const uint64_t slotCount = stack.back();
    stack.pop_back();
    const uint64_t address = stack.back();
    stack.pop_back();
    uint64_t newAddress = 0;
    if (!host.reallocHeapSlots(address, slotCount, newAddress, error)) {
      return VmKernelStepOutcome::Fault;
    }
    stack.push_back(newAddress);
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::Dup:
    if (stack.empty()) {
      error = "IR stack underflow on dup";
      return VmKernelStepOutcome::Fault;
    }
    stack.push_back(stack.back());
    ip += 1;
    return VmKernelStepOutcome::Continue;
  case IrOpcode::Pop:
    if (stack.empty()) {
      error = "IR stack underflow on pop";
      return VmKernelStepOutcome::Fault;
    }
    stack.pop_back();
    ip += 1;
    return VmKernelStepOutcome::Continue;
  case IrOpcode::LoadStringByte:
  case IrOpcode::LoadStringByteDynamic: {
    // LoadStringByte carries the string index as an immediate; the dynamic form
    // pops it (below the byte position).
    const bool dynamic = inst.op == IrOpcode::LoadStringByteDynamic;
    if (stack.size() < (dynamic ? 2u : 1u)) {
      error = "IR stack underflow on string index";
      return VmKernelStepOutcome::Fault;
    }
    const uint64_t indexRaw = stack.back();
    stack.pop_back();
    uint64_t stringIndex = inst.imm;
    if (dynamic) {
      stringIndex = stack.back();
      stack.pop_back();
    }
    const std::string *text = nullptr;
    if (!resolveVmString(module, stringIndex, text, error)) {
      return VmKernelStepOutcome::Fault;
    }
    const size_t index = static_cast<size_t>(indexRaw);
    if (index >= text->size()) {
      error = "string index out of bounds in IR";
      return VmKernelStepOutcome::Fault;
    }
    const uint8_t byte = static_cast<uint8_t>((*text)[index]);
    stack.push_back(static_cast<uint64_t>(
        static_cast<int64_t>(static_cast<int32_t>(byte))));
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::TaskSpawn:
  case IrOpcode::TaskWait:
    // executeVmKernel's scheduler runs these; a debug session steps one call stack.
    error = "tasks run only in a plain VM run, not in a debug session";
    return VmKernelStepOutcome::Fault;
  case IrOpcode::AtomicLoad:
  case IrOpcode::AtomicStore:
  case IrOpcode::AtomicAdd:
  case IrOpcode::AtomicExchange:
  case IrOpcode::AtomicCompareExchange: {
    // Every task of the VM runs on this thread, so the slot is read and written plainly; under a schedule seed the
    // scheduler makes each of these a switch point (docs/spec/vm-design.md, Atomics).
    const size_t operandCount = inst.op == IrOpcode::AtomicLoad              ? 1u
                                : inst.op == IrOpcode::AtomicCompareExchange ? 3u
                                                                             : 2u;
    if (stack.size() < operandCount) {
      error = "IR stack underflow on atomic operation";
      return VmKernelStepOutcome::Fault;
    }
    if (inst.imm != 32u && inst.imm != 64u) {
      error = "invalid atomic width in IR";
      return VmKernelStepOutcome::Fault;
    }
    // A 32-bit value is kept sign-extended in its slot, like every i32.
    const bool narrow = inst.imm == 32u;
    const auto canonical = [narrow](uint64_t value) {
      return narrow ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(value)))
                    : value;
    };
    const uint64_t second = operandCount >= 2 ? canonical(stack[stack.size() - 1]) : 0;
    const uint64_t first = operandCount == 3 ? canonical(stack[stack.size() - 2]) : second;
    uint64_t address = stack[stack.size() - operandCount];
    stack.resize(stack.size() - operandCount);
    uint64_t *slot = nullptr;
    std::vector<uint64_t> *addressLocals = nullptr;
    localizeVmAddress(frames, host.slotBytes(), address, addressLocals);
    if (!host.resolveIndirectAddress(address, *addressLocals, slot, error)) {
      return VmKernelStepOutcome::Fault;
    }
    const uint64_t before = canonical(*slot);
    switch (inst.op) {
    case IrOpcode::AtomicLoad:
      stack.push_back(before);
      break;
    case IrOpcode::AtomicStore:
      *slot = second;
      break;
    case IrOpcode::AtomicAdd:
      *slot = canonical(before + second);
      stack.push_back(before);
      break;
    case IrOpcode::AtomicExchange:
      *slot = second;
      stack.push_back(before);
      break;
    default:
      // first is the expected value and second the desired one.
      if (before == first) {
        *slot = second;
        stack.push_back(1);
      } else {
        stack.push_back(0);
      }
      break;
    }
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::MutexLock:
  case IrOpcode::MutexUnlock: {
    // Reached only where one call stack runs (a program without tasks, a debug session): executeVmKernel's
    // scheduler runs these itself when there are tasks (docs/spec/vm-design.md, Mutexes). The lock slot holds 0
    // while free and the holder's task number plus one otherwise; this lone call stack is task 0.
    if (stack.empty()) {
      error = "IR stack underflow on mutex operation";
      return VmKernelStepOutcome::Fault;
    }
    uint64_t address = stack.back();
    stack.pop_back();
    uint64_t *slot = nullptr;
    std::vector<uint64_t> *addressLocals = nullptr;
    localizeVmAddress(frames, host.slotBytes(), address, addressLocals);
    if (!host.resolveIndirectAddress(address, *addressLocals, slot, error)) {
      return VmKernelStepOutcome::Fault;
    }
    if (inst.op == IrOpcode::MutexLock) {
      if (*slot != 0) {
        error = "mutex locked again by the task that holds it";
        return VmKernelStepOutcome::Fault;
      }
      *slot = 1;
    } else {
      if (*slot != 1) {
        error = "mutex unlocked by a task that does not hold it";
        return VmKernelStepOutcome::Fault;
      }
      *slot = 0;
    }
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  case IrOpcode::LoadStringLength: {
    if (stack.empty()) {
      error = "IR stack underflow on string index";
      return VmKernelStepOutcome::Fault;
    }
    const uint64_t stringIndex = stack.back();
    stack.pop_back();
    const std::string *text = nullptr;
    if (!resolveVmString(module, stringIndex, text, error)) {
      return VmKernelStepOutcome::Fault;
    }
    stack.push_back(static_cast<uint64_t>(text->size()));
    ip += 1;
    return VmKernelStepOutcome::Continue;
  }
  default:
    // Arithmetic, comparisons and conversions: operands are read in place and
    // the result overwrites the lower operand, with the value semantics shared
    // with constant folding and the C++ emitters (IrPureSemantics.h).
    if (const size_t arity = IrPureOpcodeArityTable[static_cast<uint8_t>(inst.op)]; arity != 0) {
      const size_t size = stack.size();
      if (size < arity) {
        error = vm_kernel::pureOpcodeUnderflowMessage(inst.op);
        return VmKernelStepOutcome::Fault;
      }
      const uint64_t rhs = stack[size - 1];
      const uint64_t lhs = arity == 2 ? stack[size - 2] : rhs;
      uint64_t result = 0;
      if (evalPureOpcode(inst.op, lhs, rhs, result) != IrPureEval::Ok) {
        // The only fault a pure opcode has; the operands are consumed.
        stack.resize(size - arity);
        error = "division by zero in IR";
        return VmKernelStepOutcome::Fault;
      }
      if (arity == 2) {
        stack.pop_back();
      }
      stack.back() = result;
      ip += 1;
      return VmKernelStepOutcome::Continue;
    }
    if (isVmKernelPrintOpcode(inst.op)) {
      if (!host.handlePrintInstruction(module, inst, stack, error)) {
        return VmKernelStepOutcome::Fault;
      }
      ip += 1;
      return VmKernelStepOutcome::Continue;
    }
    if (isVmKernelFileOpcode(inst.op)) {
      if (!host.handleFileInstruction(module, inst, stack, locals, error)) {
        return VmKernelStepOutcome::Fault;
      }
      ip += 1;
      return VmKernelStepOutcome::Continue;
    }
    if (inst.op == IrOpcode::CallHost) {
      if (!host.handleHostCall(module, inst, stack, error)) {
        return VmKernelStepOutcome::Fault;
      }
      ip += 1;
      return VmKernelStepOutcome::Continue;
    }
    error = "unknown IR opcode";
    return VmKernelStepOutcome::Fault;
  }
}

} // namespace

VmKernelStepOutcome stepVmKernel(const IrModule &module,
                                 VmKernelHost &host,
                                 std::vector<uint64_t> &stack,
                                 std::vector<VmKernelFrame> &frames,
                                 const std::vector<size_t> &localCounts,
                                 uint64_t &result,
                                 VmKernelStepEvent &event,
                                 std::string &error) {
  return stepImpl<true>(module, host, stack, frames, localCounts, result, event, error);
}

namespace {

bool isAtomicOpcode(IrOpcode op) {
  return op == IrOpcode::AtomicLoad || op == IrOpcode::AtomicStore || op == IrOpcode::AtomicAdd ||
         op == IrOpcode::AtomicExchange || op == IrOpcode::AtomicCompareExchange;
}

bool moduleUsesTasks(const IrModule &module) {
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &inst : function.instructions) {
      if (inst.op == IrOpcode::TaskSpawn || inst.op == IrOpcode::TaskWait) {
        return true;
      }
    }
  }
  return false;
}

// A task handle: a tag in bits 48-62 (bit 63 clear, so it is never a heap address), the slot's generation in bits
// 32-47 and the task slot in bits 0-31. A slot is reused once its task was waited, with the next generation.
constexpr uint64_t TaskHandleTag = uint64_t{0x7A5C} << 48;
constexpr uint64_t TaskHandleTagMask = uint64_t{0xFFFF} << 48;

// The VM's tasks (docs/spec/vm-design.md, Tasks): each is a call stack of its own; all run on this one thread and
// share the heap. A task switch happens only at a spawn, a wait and a task's end. The default schedule is eager: a
// spawn runs the new task until it ends or blocks, then its spawner continues, so a program prints what it prints
// with every spawned call run inline; a blocked task yields to the lowest-numbered runnable task. A seed picks the
// next task at every switch point with a splitmix64 generator, so the same seed gives the same run.
class VmTaskScheduler {
public:
  VmTaskScheduler(const IrModule &module,
                  VmKernelHost &host,
                  const std::vector<size_t> &localCounts,
                  std::optional<uint64_t> seed)
      : module_(module), host_(host), localCounts_(localCounts), seeded_(seed.has_value()),
        rng_(seed.value_or(0)) {
  }

  bool run(uint64_t &result, std::string &error) {
    const size_t mainSlot = createTask(static_cast<size_t>(module_.entryIndex), {}, NoTask);
    current_ = mainSlot;
    VmKernelStepEvent event;
    const std::atomic<bool> *cancel = host_.cancelFlag();
    uint32_t steps = 0;
    while (true) {
      if (cancel != nullptr && (++steps & 0x3FF) == 0 && cancel->load(std::memory_order_relaxed)) {
        error = "execution stopped";
        return false;
      }
      Task &task = tasks_[current_];
      const VmKernelFrame &top = task.frames.back();
      if (top.ip < top.function->instructions.size()) {
        const IrOpcode op = top.function->instructions[top.ip].op;
        if (op == IrOpcode::TaskSpawn) {
          if (!spawn(top.function->instructions[top.ip].imm, error)) {
            return false;
          }
          continue;
        }
        if (op == IrOpcode::TaskWait) {
          if (!wait(error)) {
            return false;
          }
          continue;
        }
        if (op == IrOpcode::MutexLock || op == IrOpcode::MutexUnlock) {
          if (!lockStep(op == IrOpcode::MutexLock, error)) {
            return false;
          }
          continue;
        }
      }
      // An atomic operation is a switch point of a seeded schedule: any runnable task may run after it.
      const bool atomicStep = seeded_ && top.ip < top.function->instructions.size() &&
                              isAtomicOpcode(top.function->instructions[top.ip].op);
      uint64_t value = 0;
      switch (stepImpl<false>(
          module_, host_, task.stack, task.frames, localCounts_, value, event, error)) {
      case VmKernelStepOutcome::Continue:
        if (atomicStep) {
          pickSeeded();
        }
        continue;
      case VmKernelStepOutcome::Fault:
        return false;
      case VmKernelStepOutcome::Exit:
        break;
      }
      // The main task's return, or a trap in any task, ends the program; another task's return ends that task.
      const VmKernelFrame &exitFrame = tasks_[current_].frames.back();
      const bool trapped = exitFrame.ip < exitFrame.function->instructions.size() &&
                           exitFrame.function->instructions[exitFrame.ip].op == IrOpcode::Trap;
      if (current_ == mainSlot || trapped) {
        result = value;
        return true;
      }
      if (!finishCurrent(value, error)) {
        return false;
      }
    }
  }

private:
  static constexpr size_t NoTask = static_cast<size_t>(-1);
  static constexpr size_t MaxLiveTasks = size_t{1} << 20;
  enum class TaskState : uint8_t { Runnable, Waiting, Done, Free };
  struct Task {
    std::vector<uint64_t> stack;
    std::vector<VmKernelFrame> frames;
    TaskState state = TaskState::Free;
    // Tasks are numbered in spawn order (the main task is 0); a slot gets a new number when it is reused.
    uint64_t number = 0;
    uint16_t generation = 0;
    size_t spawner = NoTask;
    size_t waiter = NoTask;
    // The lock slot this task waits for (it is Waiting then), or 0.
    uint64_t lockAddress = 0;
    uint64_t result = 0;
  };

  uint64_t nextRandom() {
    uint64_t z = (rng_ += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }

  size_t createTask(size_t functionIndex, std::vector<uint64_t> arguments, size_t spawner) {
    size_t slot = 0;
    if (!freeSlots_.empty()) {
      slot = freeSlots_.back();
      freeSlots_.pop_back();
    } else {
      slot = tasks_.size();
      tasks_.emplace_back();
    }
    Task &task = tasks_[slot];
    // The arguments are the task's operand stack, where the function's first instructions store its parameters.
    task.stack = std::move(arguments);
    task.frames.clear();
    VmKernelFrame frame;
    frame.functionIndex = functionIndex;
    frame.function = &module_.functions[functionIndex];
    frame.locals.assign(localCounts_[functionIndex], 0);
    // Each call stack numbers its own locals from 0, so a local address does not name a slot of another task.
    frame.localsBase = 0;
    task.frames.push_back(std::move(frame));
    task.state = TaskState::Runnable;
    task.number = nextNumber_++;
    task.spawner = spawner;
    task.waiter = NoTask;
    task.lockAddress = 0;
    task.result = 0;
    addRunnable(slot);
    return slot;
  }

  // runnable_ holds the tasks that may run (not waiting, not ended), ordered by number.
  void addRunnable(size_t slot) {
    const std::pair<uint64_t, size_t> key{tasks_[slot].number, slot};
    runnable_.insert(std::lower_bound(runnable_.begin(), runnable_.end(), key), key);
  }

  void removeRunnable(size_t slot) {
    const std::pair<uint64_t, size_t> key{tasks_[slot].number, slot};
    const auto it = std::lower_bound(runnable_.begin(), runnable_.end(), key);
    if (it != runnable_.end() && *it == key) {
      runnable_.erase(it);
    }
  }

  // A switch point of a seeded schedule: any runnable task may run next.
  void pickSeeded() {
    current_ = runnable_[static_cast<size_t>(nextRandom() % runnable_.size())].second;
  }

  // The current task cannot go on. The default schedule runs `preferred` (the spawner of a task that ended) when it
  // can, else the lowest-numbered runnable task.
  bool pickAfterStop(size_t preferred, std::string &error) {
    if (runnable_.empty()) {
      error = "deadlock: every task is waiting";
      return false;
    }
    if (seeded_) {
      pickSeeded();
    } else if (preferred != NoTask && tasks_[preferred].state == TaskState::Runnable) {
      current_ = preferred;
    } else {
      current_ = runnable_.front().second;
    }
    return true;
  }

  bool spawn(uint64_t functionImm, std::string &error) {
    if (functionImm >= module_.functions.size()) {
      error = "invalid task function in IR";
      return false;
    }
    const size_t functionIndex = static_cast<size_t>(functionImm);
    const size_t parameterCount = module_.functions[functionIndex].parameterCount;
    const size_t spawner = current_;
    std::vector<uint64_t> &spawnerStack = tasks_[spawner].stack;
    if (spawnerStack.size() < parameterCount) {
      error = "IR stack underflow on task spawn";
      return false;
    }
    if (tasks_.size() - freeSlots_.size() >= MaxLiveTasks) {
      error = "too many live tasks";
      return false;
    }
    std::vector<uint64_t> arguments(
        spawnerStack.end() - static_cast<std::ptrdiff_t>(parameterCount), spawnerStack.end());
    spawnerStack.resize(spawnerStack.size() - parameterCount);
    // createTask may grow tasks_, so the spawner is found again by its slot afterwards.
    const size_t child = createTask(functionIndex, std::move(arguments), spawner);
    Task &task = tasks_[spawner];
    task.stack.push_back(TaskHandleTag | (static_cast<uint64_t>(tasks_[child].generation) << 32) |
                         static_cast<uint64_t>(child));
    task.frames.back().ip += 1;
    if (seeded_) {
      pickSeeded();
    } else {
      current_ = child;
    }
    return true;
  }

  bool wait(std::string &error) {
    Task &task = tasks_[current_];
    if (task.stack.empty()) {
      error = "IR stack underflow on task wait";
      return false;
    }
    const uint64_t handle = task.stack.back();
    const size_t slot = static_cast<size_t>(handle & 0xFFFFFFFFull);
    const uint16_t generation = static_cast<uint16_t>((handle >> 32) & 0xFFFFull);
    if ((handle & TaskHandleTagMask) != TaskHandleTag || slot >= tasks_.size() ||
        slot == current_) {
      error = "invalid task handle in IR";
      return false;
    }
    Task &target = tasks_[slot];
    if (target.generation != generation || target.state == TaskState::Free ||
        (target.waiter != NoTask && target.waiter != current_)) {
      error = "task handle already waited";
      return false;
    }
    if (target.state == TaskState::Done) {
      task.stack.back() = target.result;
      task.frames.back().ip += 1;
      target.state = TaskState::Free;
      target.generation = static_cast<uint16_t>(target.generation + 1);
      freeSlots_.push_back(slot);
      if (seeded_) {
        pickSeeded();
      }
      return true;
    }
    // Block until the task ends; this wait then runs again and finds it done.
    target.waiter = current_;
    task.state = TaskState::Waiting;
    removeRunnable(current_);
    return pickAfterStop(NoTask, error);
  }

  // MutexLock and MutexUnlock (docs/spec/vm-design.md, Mutexes). The lock slot holds 0 while free and the holder's
  // task number plus one otherwise. A task that finds the lock held waits, which counts for deadlock detection, and
  // runs its MutexLock again once the holder unlocked. Both are switch points of a seeded schedule.
  bool lockStep(bool lock, std::string &error) {
    Task &task = tasks_[current_];
    if (task.stack.empty()) {
      error = "IR stack underflow on mutex operation";
      return false;
    }
    const uint64_t lockAddress = task.stack.back();
    uint64_t address = lockAddress;
    uint64_t *slot = nullptr;
    std::vector<uint64_t> *addressLocals = nullptr;
    localizeVmAddress(task.frames, host_.slotBytes(), address, addressLocals);
    if (!host_.resolveIndirectAddress(address, *addressLocals, slot, error)) {
      return false;
    }
    const uint64_t holder = task.number + 1;
    if (lock) {
      if (*slot == holder) {
        error = "mutex locked again by the task that holds it";
        return false;
      }
      if (*slot != 0) {
        task.lockAddress = lockAddress;
        task.state = TaskState::Waiting;
        removeRunnable(current_);
        return pickAfterStop(NoTask, error);
      }
      *slot = holder;
    } else {
      if (*slot != holder) {
        error = "mutex unlocked by a task that does not hold it";
        return false;
      }
      *slot = 0;
      for (size_t waiting = 0; waiting < tasks_.size(); ++waiting) {
        Task &other = tasks_[waiting];
        if (other.state == TaskState::Waiting && other.lockAddress == lockAddress) {
          other.lockAddress = 0;
          other.state = TaskState::Runnable;
          addRunnable(waiting);
        }
      }
    }
    task.stack.pop_back();
    task.frames.back().ip += 1;
    if (seeded_) {
      pickSeeded();
    }
    return true;
  }

  bool finishCurrent(uint64_t value, std::string &error) {
    Task &task = tasks_[current_];
    task.result = value;
    removeRunnable(current_);
    task.state = TaskState::Done;
    task.frames = {};
    task.stack = {};
    if (task.waiter != NoTask) {
      tasks_[task.waiter].state = TaskState::Runnable;
      addRunnable(task.waiter);
    }
    return pickAfterStop(task.spawner, error);
  }

  const IrModule &module_;
  VmKernelHost &host_;
  const std::vector<size_t> &localCounts_;
  bool seeded_ = false;
  uint64_t rng_ = 0;
  std::vector<Task> tasks_;
  std::vector<size_t> freeSlots_;
  std::vector<std::pair<uint64_t, size_t>> runnable_;
  uint64_t nextNumber_ = 0;
  size_t current_ = 0;
};

} // namespace

bool executeVmKernel(const IrModule &module,
                     VmKernelHost &host,
                     uint64_t &result,
                     std::string &error) {
  if (module.entryIndex < 0 ||
      static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
    error = "invalid IR entry index";
    return false;
  }

  if (moduleUsesTasks(module)) {
    std::vector<size_t> localCounts(module.functions.size(), 0);
    for (size_t i = 0; i < module.functions.size(); ++i) {
      localCounts[i] = computeVmKernelLocalCount(module.functions[i]);
    }
    VmTaskScheduler scheduler(module, host, localCounts, host.scheduleSeed());
    return scheduler.run(result, error);
  }

  if (vmFastKernelEnabled() && host.cancelFlag() == nullptr) {
    bool executed = false;
    const bool ok = executeVmFastKernel(module, host, result, error, executed);
    if (executed) {
      return ok;
    }
  }

  std::vector<size_t> localCounts(module.functions.size(), 0);
  for (size_t i = 0; i < module.functions.size(); ++i) {
    localCounts[i] = computeVmKernelLocalCount(module.functions[i]);
  }

  std::vector<uint64_t> stack;
  std::vector<VmKernelFrame> frames;
  frames.reserve(64);
  VmKernelFrame entryFrame;
  entryFrame.functionIndex = static_cast<size_t>(module.entryIndex);
  entryFrame.function = &module.functions[entryFrame.functionIndex];
  entryFrame.locals.assign(localCounts[entryFrame.functionIndex], 0);
  frames.push_back(std::move(entryFrame));

  VmKernelStepEvent event;
  const std::atomic<bool> *cancel = host.cancelFlag();
  uint32_t steps = 0;
  while (!frames.empty()) {
    if (cancel != nullptr && (++steps & 0x3FF) == 0 && cancel->load(std::memory_order_relaxed)) {
      error = "execution stopped";
      return false;
    }
    switch (stepImpl<false>(module, host, stack, frames, localCounts, result, event, error)) {
    case VmKernelStepOutcome::Continue:
      break;
    case VmKernelStepOutcome::Exit:
      return true;
    case VmKernelStepOutcome::Fault:
      return false;
    }
  }

  error = "missing return in IR";
  return false;
}

} // namespace primec::vm_detail
