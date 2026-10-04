#include "VmFastKernel.h"
#include "VmFastKernelProgram.h"

#include "primec/ir/IrCfg.h"
#include "primec/ir/IrPureSemantics.h"
#include "primec/runtime/VmStringHeap.h"
#include "primec/testing/VmKernelSelection.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <string_view>
#include <vector>

namespace primec {
namespace testing {
namespace {
std::atomic<bool> FastKernelEnabled{true};
} // namespace

void setVmFastKernelEnabled(bool enabled) {
  FastKernelEnabled.store(enabled, std::memory_order_relaxed);
}

} // namespace testing

namespace vm_detail {

bool vmFastKernelEnabled() {
  if (!testing::FastKernelEnabled.load(std::memory_order_relaxed)) {
    return false;
  }
  // Diagnostic switch for comparing the two kernels from the command line, e.g.
  // scripts/differential_opt_check.py --baseline-kernel step.
  const char *selection = std::getenv("PRIMEVM_KERNEL");
  return selection == nullptr || std::string_view(selection) != "step";
}

namespace {

// Float arithmetic, comparisons and conversions: each gets a threaded handler that evaluates it
// with the shared pure semantics (IrPureSemantics.h), the opcode a constant so the switch folds.
// The three-address forms of VmFastKernelProgram.h (results stored straight into a local).
// clang-format off
#define FAST_STORE_FORMS(X) \
  X(FastOpLocalAddLocalStore) \
  X(FastOpLocalSubLocalStore) \
  X(FastOpLocalMulLocalStore) \
  X(FastOpLocalAddLocalSextStore) \
  X(FastOpLocalSubLocalSextStore) \
  X(FastOpLocalMulLocalSextStore) \
  X(FastOpAddStore) \
  X(FastOpSubStore) \
  X(FastOpMulStore) \
  X(FastOpAddSextStore) \
  X(FastOpSubSextStore) \
  X(FastOpMulSextStore) \
  X(FastOpLocalAddLocalStoreF64) \
  X(FastOpLocalSubLocalStoreF64) \
  X(FastOpLocalMulLocalStoreF64) \
  X(FastOpLocalDivLocalStoreF64) \
  X(FastOpLocalAddImmStoreF64) \
  X(FastOpLocalSubImmStoreF64) \
  X(FastOpLocalMulImmStoreF64) \
  X(FastOpLocalDivImmStoreF64) \
  X(FastOpAddStoreF64) \
  X(FastOpSubStoreF64) \
  X(FastOpMulStoreF64) \
  X(FastOpDivStoreF64) \
  X(FastOpLocalNegStoreF64)
// clang-format on

// clang-format off
#define FAST_FLOAT_BINARY(X) \
  X(AddF32) \
  X(SubF32) \
  X(MulF32) \
  X(DivF32) \
  X(AddF64) \
  X(SubF64) \
  X(MulF64) \
  X(DivF64) \
  X(CmpEqF32) \
  X(CmpNeF32) \
  X(CmpLtF32) \
  X(CmpLeF32) \
  X(CmpGtF32) \
  X(CmpGeF32) \
  X(CmpEqF64) \
  X(CmpNeF64) \
  X(CmpLtF64) \
  X(CmpLeF64) \
  X(CmpGtF64) \
  X(CmpGeF64)
#define FAST_FLOAT_UNARY(X) \
  X(NegF32) \
  X(NegF64) \
  X(ConvertI32ToF32) \
  X(ConvertI32ToF64) \
  X(ConvertI64ToF32) \
  X(ConvertI64ToF64) \
  X(ConvertU64ToF32) \
  X(ConvertU64ToF64) \
  X(ConvertF32ToI32) \
  X(ConvertF32ToI64) \
  X(ConvertF32ToU64) \
  X(ConvertF64ToI32) \
  X(ConvertF64ToI64) \
  X(ConvertF64ToU64) \
  X(ConvertF32ToF64) \
  X(ConvertF64ToF32)
// clang-format on

struct FastFrame {
  const FastFunction *function = nullptr;
  const FastInst *returnIp = nullptr;
  size_t localsBase = 0;
  bool returnsValue = false;
};

constexpr size_t InitialStackSlots = 4096;

#define OP(name) static_cast<uint16_t>(IrOpcode::name)

// Every FastOp and IrOpcode value is below this, so the dispatch table needs no bounds check.
constexpr size_t VmFastDispatchSlots = 0x200;
static_assert(FastOpEnd <= VmFastDispatchSlots, "dispatch table too small for the fused opcodes");
static_assert(static_cast<size_t>(IrOpcode::SextI32) < 0x100,
              "IR opcodes must stay below the fused range");

// Float arithmetic with the shared pure semantics; the opcode is a constant at every use, so the
// switch inside folds away.
inline uint64_t pureF64(IrOpcode op, uint64_t lhs, uint64_t rhs) {
  uint64_t value = 0;
  (void)evalPureOpcode(op, lhs, rhs, value);
  return value;
}

// Fault messages are built out of line so the handlers that can fault stay small.
[[gnu::cold, gnu::noinline]] bool
addressFault(std::string &error, const char *message, uint64_t address) {
  error = message + std::to_string(address);
  return false;
}

} // namespace

// Threaded dispatch uses the GNU labels-as-values extension (clang and GCC).
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-label-as-value"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

bool executeVmFastKernel(const IrModule &module,
                         VmKernelHost &host,
                         uint64_t &result,
                         std::string &error,
                         bool &executed) {
  executed = false;
  std::vector<FastFunction> functions;
  // The loop addresses locals with the constant slot size, so a shift and a mask replace the
  // division a runtime size would need.
  if (host.slotBytes() != IrSlotBytes || !prepareModule(module, functions)) {
    return false;
  }
  executed = true;

  const size_t maxCallDepth = host.maxCallDepth();
  constexpr uint64_t slotBytes = IrSlotBytes;
  static_assert((IrSlotBytes & (IrSlotBytes - 1)) == 0, "slot size must be a power of two");
  const uint64_t argc =
      static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(host.argumentCount())));
  const VmStringHeap *stringHeap = host.stringHeap();
  const auto resolveString = [&](uint64_t index, const std::string *&text) {
    return resolveVmString(module, stringHeap, index, text, error);
  };

  std::vector<uint64_t> stack(InitialStackSlots +
                              functions[static_cast<size_t>(module.entryIndex)].stackHeadroom);
  uint64_t *sp = stack.data();
  std::vector<uint64_t> localsArena(256, 0);
  std::vector<FastFrame> frames;
  frames.reserve(64);
  std::vector<uint64_t> scratch;
  std::vector<uint64_t> noLocals;
  std::vector<uint64_t> frameLocalsCopy;

  const FastFunction *current = &functions[static_cast<size_t>(module.entryIndex)];
  size_t localsBase = 0;
  if (localsArena.size() < current->localCount) {
    localsArena.resize(current->localCount, 0);
  }
  uint64_t *locals = localsArena.data();
  const FastInst *ip = current->code.data();

  // The slot an indirect address names, or false with the fault in `error`.
  const auto indirectSlot = [&](uint64_t address, uint64_t *&slot) -> bool {
    if ((address & (slotBytes - 1)) != 0) [[unlikely]] {
      return addressFault(error, "unaligned indirect address in IR: ", address);
    }
    if ((address & (uint64_t{1} << 63)) != 0) {
      return host.resolveIndirectAddress(address, noLocals, slot, error);
    }
    const uint64_t index = address / slotBytes;
    if (index >= current->localCount) [[unlikely]] {
      return addressFault(error, "invalid indirect address in IR: ", address);
    }
    slot = locals + index;
    return true;
  };

  // Reserves `extra` operand slots above sp, moving the stack if it must grow.
  const auto ensureStack = [&](size_t extra) {
    const size_t used = static_cast<size_t>(sp - stack.data());
    if (used + extra > stack.size()) {
      stack.resize(std::max(stack.size() * 2, used + extra));
      sp = stack.data() + used;
    }
  };

#define FAULT(message)                                                                             \
  do {                                                                                             \
    error = (message);                                                                             \
    return false;                                                                                  \
  } while (0)

  // Threaded dispatch: every case ends by jumping straight to the next instruction's
  // handler through this table, which gives each opcode its own indirect branch for the
  // predictor instead of one shared by the whole switch. Opcodes without a case of their
  // own (pure arithmetic and host calls) go to the generic tail.
  const void *table[VmFastDispatchSlots];
  for (const void *&entry : table) {
    entry = &&lbl_generic;
  }
  table[OP(PushI32)] = &&lbl_PushI32;
  table[OP(PushI64)] = &&lbl_PushI64;
  table[OP(PushF32)] = &&lbl_PushF32;
  table[OP(PushF64)] = &&lbl_PushF64;
  table[OP(PushArgc)] = &&lbl_PushArgc;
  table[OP(LoadLocal)] = &&lbl_LoadLocal;
  table[OP(StoreLocal)] = &&lbl_StoreLocal;
  table[OP(AddressOfLocal)] = &&lbl_AddressOfLocal;
  table[OP(Dup)] = &&lbl_Dup;
  table[OP(Pop)] = &&lbl_Pop;
  table[OP(Jump)] = &&lbl_Jump;
  table[OP(JumpIfZero)] = &&lbl_JumpIfZero;
  table[FastOpStoreLocalDupPop] = &&lbl_FastOpStoreLocalDupPop;
  table[FastOpStoreLocalImm] = &&lbl_FastOpStoreLocalImm;
  table[FastOpCopyLocal] = &&lbl_FastOpCopyLocal;
  table[FastOpJmpLocalZero] = &&lbl_FastOpJmpLocalZero;
  table[FastOpPushLocalAddImm] = &&lbl_FastOpPushLocalAddImm;
  table[FastOpPushLocalSubImm] = &&lbl_FastOpPushLocalSubImm;
  table[FastOpPushLocalMulImm] = &&lbl_FastOpPushLocalMulImm;
  table[FastOpPushLocalAddLocal] = &&lbl_FastOpPushLocalAddLocal;
  table[FastOpPushLocalSubLocal] = &&lbl_FastOpPushLocalSubLocal;
  table[FastOpPushLocalMulLocal] = &&lbl_FastOpPushLocalMulLocal;
  table[FastOpLocalAddImmStore] = &&lbl_FastOpLocalAddImmStore;
  table[FastOpLocalSubImmStore] = &&lbl_FastOpLocalSubImmStore;
  table[FastOpPushLocalAddImmSext] = &&lbl_FastOpPushLocalAddImmSext;
  table[FastOpPushLocalSubImmSext] = &&lbl_FastOpPushLocalSubImmSext;
  table[FastOpPushLocalMulImmSext] = &&lbl_FastOpPushLocalMulImmSext;
  table[FastOpPushLocalAddLocalSext] = &&lbl_FastOpPushLocalAddLocalSext;
  table[FastOpPushLocalSubLocalSext] = &&lbl_FastOpPushLocalSubLocalSext;
  table[FastOpPushLocalMulLocalSext] = &&lbl_FastOpPushLocalMulLocalSext;
  table[FastOpLocalAddImmStoreSext] = &&lbl_FastOpLocalAddImmStoreSext;
  table[FastOpLocalSubImmStoreSext] = &&lbl_FastOpLocalSubImmStoreSext;
  table[FastOpLocalStringByteStore] = &&lbl_FastOpLocalStringByteStore;
  table[FastOpPushLocalStringByte] = &&lbl_FastOpPushLocalStringByte;
#define FAST_TABLE_ENTRY(N) table[N] = &&lbl_##N;
  FAST_STORE_FORMS(FAST_TABLE_ENTRY)
#undef FAST_TABLE_ENTRY
  table[FastOpSwitchLocal] = &&lbl_FastOpSwitchLocal;
  table[FastOpAddSext] = &&lbl_FastOpAddSext;
  table[FastOpSubSext] = &&lbl_FastOpSubSext;
  table[FastOpMulSext] = &&lbl_FastOpMulSext;
  table[FastOpPushLocalAddLocalF64] = &&lbl_FastOpPushLocalAddLocalF64;
  table[FastOpPushLocalSubLocalF64] = &&lbl_FastOpPushLocalSubLocalF64;
  table[FastOpPushLocalMulLocalF64] = &&lbl_FastOpPushLocalMulLocalF64;
  table[FastOpPushLocalDivLocalF64] = &&lbl_FastOpPushLocalDivLocalF64;
  table[FastOpPushLocalAddImmF64] = &&lbl_FastOpPushLocalAddImmF64;
  table[FastOpPushLocalSubImmF64] = &&lbl_FastOpPushLocalSubImmF64;
  table[FastOpPushLocalMulImmF64] = &&lbl_FastOpPushLocalMulImmF64;
  table[FastOpPushLocalDivImmF64] = &&lbl_FastOpPushLocalDivImmF64;
  table[OP(AddI32)] = &&lbl_AddI32;
  table[OP(AddI64)] = &&lbl_AddI64;
  table[OP(SubI32)] = &&lbl_SubI32;
  table[OP(SubI64)] = &&lbl_SubI64;
  table[OP(MulI32)] = &&lbl_MulI32;
  table[OP(MulI64)] = &&lbl_MulI64;
  table[OP(NegI32)] = &&lbl_NegI32;
  table[OP(NegI64)] = &&lbl_NegI64;
  table[OP(SextI32)] = &&lbl_SextI32;
  table[OP(CmpEqI32)] = &&lbl_CmpEqI32;
  table[OP(CmpEqI64)] = &&lbl_CmpEqI64;
  table[OP(CmpNeI32)] = &&lbl_CmpNeI32;
  table[OP(CmpNeI64)] = &&lbl_CmpNeI64;
  table[OP(CmpLtI32)] = &&lbl_CmpLtI32;
  table[OP(CmpLtI64)] = &&lbl_CmpLtI64;
  table[OP(CmpLeI32)] = &&lbl_CmpLeI32;
  table[OP(CmpLeI64)] = &&lbl_CmpLeI64;
  table[OP(CmpGtI32)] = &&lbl_CmpGtI32;
  table[OP(CmpGtI64)] = &&lbl_CmpGtI64;
  table[OP(CmpGeI32)] = &&lbl_CmpGeI32;
  table[OP(CmpGeI64)] = &&lbl_CmpGeI64;
  table[OP(CmpLtU64)] = &&lbl_CmpLtU64;
  table[OP(CmpLeU64)] = &&lbl_CmpLeU64;
  table[OP(CmpGtU64)] = &&lbl_CmpGtU64;
  table[OP(CmpGeU64)] = &&lbl_CmpGeU64;
  table[OP(LoadIndirect)] = &&lbl_LoadIndirect;
  table[OP(StoreIndirect)] = &&lbl_StoreIndirect;
  table[OP(HeapAlloc)] = &&lbl_HeapAlloc;
  table[OP(HeapFree)] = &&lbl_HeapFree;
  table[OP(HeapRealloc)] = &&lbl_HeapRealloc;
  table[OP(LoadStringByte)] = &&lbl_LoadStringByte;
  table[OP(LoadStringByteDynamic)] = &&lbl_LoadStringByteDynamic;
  table[OP(LoadStringLength)] = &&lbl_LoadStringLength;
  table[OP(Call)] = &&lbl_Call;
  table[OP(CallVoid)] = &&lbl_CallVoid;
  table[OP(ReturnVoid)] = &&lbl_ReturnVoid;
  table[OP(ReturnI32)] = &&lbl_ReturnI32;
  table[OP(ReturnI64)] = &&lbl_ReturnI64;
  table[OP(ReturnF32)] = &&lbl_ReturnF32;
  table[OP(ReturnF64)] = &&lbl_ReturnF64;
  table[FastOpMissingReturn] = &&lbl_FastOpMissingReturn;
#define FAST_TABLE_FLOAT(N) table[OP(N)] = &&lbl_##N;
  FAST_FLOAT_BINARY(FAST_TABLE_FLOAT)
  FAST_FLOAT_UNARY(FAST_TABLE_FLOAT)
#undef FAST_TABLE_FLOAT
#define FAST_TABLE_CMP(N, O)                                                                       \
  table[FastOpJmpCmpLocalImm##N] = &&lbl_FastOpJmpCmpLocalImm##N;                                  \
  table[FastOpJmpCmpLocalLocal##N] = &&lbl_FastOpJmpCmpLocalLocal##N;                              \
  table[FastOpJmpCmp##N] = &&lbl_FastOpJmpCmp##N;
  FAST_CMPS(FAST_TABLE_CMP)
#undef FAST_TABLE_CMP
#define DISPATCH()                                                                                 \
  do {                                                                                             \
    instp = ip;                                                                                    \
    goto *table[instp->op];                                                                        \
  } while (0)
#define inst (*instp)
  const FastInst *instp = ip;

  error.clear();
  uint64_t returnValue = 0;
  for (;;) {
    instp = ip;
    switch (instp->op) {
    case OP(PushI32):
    lbl_PushI32:
      *sp++ = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(inst.imm)));
      ++ip;
      DISPATCH();
    case OP(PushI64):
    lbl_PushI64:
    case OP(PushF32):
    lbl_PushF32:
    case OP(PushF64):
    lbl_PushF64:
      *sp++ = inst.imm;
      ++ip;
      DISPATCH();
    case OP(PushArgc):
    lbl_PushArgc:
      *sp++ = argc;
      ++ip;
      DISPATCH();
    case OP(LoadLocal):
    lbl_LoadLocal:
      *sp++ = locals[inst.imm];
      ++ip;
      DISPATCH();
    case OP(StoreLocal):
    lbl_StoreLocal:
      locals[inst.imm] = *--sp;
      ++ip;
      DISPATCH();
    case OP(AddressOfLocal):
    lbl_AddressOfLocal:
      *sp++ = inst.imm * slotBytes;
      ++ip;
      DISPATCH();
    case OP(Dup):
    lbl_Dup:
      sp[0] = sp[-1];
      ++sp;
      ++ip;
      DISPATCH();
    case OP(Pop):
    lbl_Pop:
      --sp;
      ++ip;
      DISPATCH();
    case OP(Jump):
    lbl_Jump:
      ip = current->code.data() + inst.imm;
      DISPATCH();
    case OP(JumpIfZero):
    lbl_JumpIfZero:
      ip = *--sp == 0 ? current->code.data() + inst.imm : ip + 1;
      DISPATCH();

    case FastOpStoreLocalDupPop:
    lbl_FastOpStoreLocalDupPop:
      locals[inst.a] = *--sp;
      ip += 3;
      DISPATCH();
    case FastOpStoreLocalImm:
    lbl_FastOpStoreLocalImm:
      locals[inst.a] = inst.imm;
      ip += 2;
      DISPATCH();
    case FastOpCopyLocal:
    lbl_FastOpCopyLocal:
      locals[inst.b] = locals[inst.a];
      ip += 2;
      DISPATCH();
    case FastOpJmpLocalZero:
    lbl_FastOpJmpLocalZero:
      ip = locals[inst.a] == 0 ? current->code.data() + inst.b : ip + 2;
      DISPATCH();
    case FastOpPushLocalAddImm:
    lbl_FastOpPushLocalAddImm:
      *sp++ = locals[inst.a] + inst.imm;
      ip += 3;
      DISPATCH();
    case FastOpPushLocalSubImm:
    lbl_FastOpPushLocalSubImm:
      *sp++ = locals[inst.a] - inst.imm;
      ip += 3;
      DISPATCH();
    case FastOpPushLocalMulImm:
    lbl_FastOpPushLocalMulImm:
      *sp++ = locals[inst.a] * inst.imm;
      ip += 3;
      DISPATCH();
    case FastOpPushLocalAddLocal:
    lbl_FastOpPushLocalAddLocal:
      *sp++ = locals[inst.a] + locals[inst.b];
      ip += 3;
      DISPATCH();
    case FastOpPushLocalSubLocal:
    lbl_FastOpPushLocalSubLocal:
      *sp++ = locals[inst.a] - locals[inst.b];
      ip += 3;
      DISPATCH();
    case FastOpPushLocalMulLocal:
    lbl_FastOpPushLocalMulLocal:
      *sp++ = locals[inst.a] * locals[inst.b];
      ip += 3;
      DISPATCH();
    case FastOpLocalAddImmStore:
    lbl_FastOpLocalAddImmStore:
      locals[inst.b] = locals[inst.a] + inst.imm;
      ip += 4;
      DISPATCH();
    case FastOpLocalSubImmStore:
    lbl_FastOpLocalSubImmStore:
      locals[inst.b] = locals[inst.a] - inst.imm;
      ip += 4;
      DISPATCH();
    case FastOpPushLocalAddImmSext:
    lbl_FastOpPushLocalAddImmSext:
      *sp++ = sext32(locals[inst.a] + inst.imm);
      ip += 4;
      DISPATCH();
    case FastOpPushLocalSubImmSext:
    lbl_FastOpPushLocalSubImmSext:
      *sp++ = sext32(locals[inst.a] - inst.imm);
      ip += 4;
      DISPATCH();
    case FastOpPushLocalMulImmSext:
    lbl_FastOpPushLocalMulImmSext:
      *sp++ = sext32(locals[inst.a] * inst.imm);
      ip += 4;
      DISPATCH();
    case FastOpPushLocalAddLocalSext:
    lbl_FastOpPushLocalAddLocalSext:
      *sp++ = sext32(locals[inst.a] + locals[inst.b]);
      ip += 4;
      DISPATCH();
    case FastOpPushLocalSubLocalSext:
    lbl_FastOpPushLocalSubLocalSext:
      *sp++ = sext32(locals[inst.a] - locals[inst.b]);
      ip += 4;
      DISPATCH();
    case FastOpPushLocalMulLocalSext:
    lbl_FastOpPushLocalMulLocalSext:
      *sp++ = sext32(locals[inst.a] * locals[inst.b]);
      ip += 4;
      DISPATCH();
    case FastOpLocalAddImmStoreSext:
    lbl_FastOpLocalAddImmStoreSext:
      locals[inst.b] = sext32(locals[inst.a] + inst.imm);
      ip += 5;
      DISPATCH();
    case FastOpLocalSubImmStoreSext:
    lbl_FastOpLocalSubImmStoreSext:
      locals[inst.b] = sext32(locals[inst.a] - inst.imm);
      ip += 5;
      DISPATCH();
    case FastOpLocalStringByteStore:
    lbl_FastOpLocalStringByteStore:
    case FastOpPushLocalStringByte:
    lbl_FastOpPushLocalStringByte: {
      const std::string *text = nullptr;
      if (!resolveString(inst.imm, text)) {
        return false;
      }
      const uint64_t position = locals[inst.a];
      if (position >= text->size()) {
        FAULT("string index out of bounds in IR");
      }
      const uint64_t byte = static_cast<uint8_t>((*text)[static_cast<size_t>(position)]);
      if (inst.op == FastOpLocalStringByteStore) {
        locals[inst.b] = byte;
        ip += 3;
      } else {
        *sp++ = byte;
        ip += 2;
      }
      DISPATCH();
    }
#define FAST_CASE_JMP_CMP_LOCAL_IMM(N, O)                                                          \
  case FastOpJmpCmpLocalImm##N:                                                                    \
    lbl_FastOpJmpCmpLocalImm##N : ip = static_cast<int64_t>(locals[inst.a])                        \
                                               O static_cast<int64_t>(inst.imm)                    \
                                           ? ip + 4                                                \
                                           : current->code.data() + inst.b;                        \
    DISPATCH();
#define FAST_CASE_JMP_CMP_LOCAL_LOCAL(N, O)                                                        \
  case FastOpJmpCmpLocalLocal##N:                                                                  \
    lbl_FastOpJmpCmpLocalLocal##N : ip = static_cast<int64_t>(locals[inst.a])                      \
                                                 O static_cast<int64_t>(locals[inst.b])            \
                                             ? ip + 4                                              \
                                             : current->code.data() + inst.imm;                    \
    DISPATCH();
#define FAST_CASE_JMP_CMP(N, O)                                                                    \
  case FastOpJmpCmp##N:                                                                            \
    lbl_FastOpJmpCmp##N : {                                                                        \
      const bool taken = static_cast<int64_t>(sp[-2]) O static_cast<int64_t>(sp[-1]);              \
      sp -= 2;                                                                                     \
      ip = taken ? ip + 2 : current->code.data() + inst.b;                                         \
      DISPATCH();                                                                                  \
    }
      FAST_CMPS(FAST_CASE_JMP_CMP_LOCAL_IMM)
      FAST_CMPS(FAST_CASE_JMP_CMP_LOCAL_LOCAL)
      FAST_CMPS(FAST_CASE_JMP_CMP)
#undef FAST_CASE_JMP_CMP_LOCAL_IMM
#undef FAST_CASE_JMP_CMP_LOCAL_LOCAL
#undef FAST_CASE_JMP_CMP

    case OP(AddI32):
    lbl_AddI32:
    case OP(AddI64):
    lbl_AddI64:
      sp[-2] += sp[-1];
      --sp;
      ++ip;
      DISPATCH();
    case OP(SubI32):
    lbl_SubI32:
    case OP(SubI64):
    lbl_SubI64:
      sp[-2] -= sp[-1];
      --sp;
      ++ip;
      DISPATCH();
    case OP(MulI32):
    lbl_MulI32:
    case OP(MulI64):
    lbl_MulI64:
      sp[-2] *= sp[-1];
      --sp;
      ++ip;
      DISPATCH();
    case OP(NegI32):
    lbl_NegI32:
    case OP(NegI64):
    lbl_NegI64:
      sp[-1] = uint64_t{0} - sp[-1];
      ++ip;
      DISPATCH();
    case OP(SextI32):
    lbl_SextI32:
      sp[-1] = sext32(sp[-1]);
      ++ip;
      DISPATCH();
      // Three-address forms.
#define FAST_LOCAL_LOCAL_STORE(NAME, EXPR, LENGTH)                                                 \
  case NAME:                                                                                       \
    lbl_##NAME : {                                                                                 \
      const uint64_t lhs = locals[inst.a];                                                         \
      const uint64_t rhs = locals[inst.b];                                                         \
      locals[inst.imm] = (EXPR);                                                                   \
      ip += (LENGTH);                                                                              \
      DISPATCH();                                                                                  \
    }
      FAST_LOCAL_LOCAL_STORE(FastOpLocalAddLocalStore, lhs + rhs, 4)
      FAST_LOCAL_LOCAL_STORE(FastOpLocalSubLocalStore, lhs - rhs, 4)
      FAST_LOCAL_LOCAL_STORE(FastOpLocalMulLocalStore, lhs * rhs, 4)
      FAST_LOCAL_LOCAL_STORE(FastOpLocalAddLocalSextStore, sext32(lhs + rhs), 5)
      FAST_LOCAL_LOCAL_STORE(FastOpLocalSubLocalSextStore, sext32(lhs - rhs), 5)
      FAST_LOCAL_LOCAL_STORE(FastOpLocalMulLocalSextStore, sext32(lhs * rhs), 5)
      FAST_LOCAL_LOCAL_STORE(FastOpLocalAddLocalStoreF64, pureF64(IrOpcode::AddF64, lhs, rhs), 4)
      FAST_LOCAL_LOCAL_STORE(FastOpLocalSubLocalStoreF64, pureF64(IrOpcode::SubF64, lhs, rhs), 4)
      FAST_LOCAL_LOCAL_STORE(FastOpLocalMulLocalStoreF64, pureF64(IrOpcode::MulF64, lhs, rhs), 4)
      FAST_LOCAL_LOCAL_STORE(FastOpLocalDivLocalStoreF64, pureF64(IrOpcode::DivF64, lhs, rhs), 4)
#undef FAST_LOCAL_LOCAL_STORE
#define FAST_LOCAL_IMM_STORE_F64(NAME, OPCODE)                                                     \
  case NAME:                                                                                       \
    lbl_##NAME : locals[inst.b] = pureF64(IrOpcode::OPCODE, locals[inst.a], inst.imm);             \
    ip += 4;                                                                                       \
    DISPATCH();
      FAST_LOCAL_IMM_STORE_F64(FastOpLocalAddImmStoreF64, AddF64)
      FAST_LOCAL_IMM_STORE_F64(FastOpLocalSubImmStoreF64, SubF64)
      FAST_LOCAL_IMM_STORE_F64(FastOpLocalMulImmStoreF64, MulF64)
      FAST_LOCAL_IMM_STORE_F64(FastOpLocalDivImmStoreF64, DivF64)
#undef FAST_LOCAL_IMM_STORE_F64
#define FAST_STACK_STORE(NAME, EXPR, LENGTH)                                                       \
  case NAME:                                                                                       \
    lbl_##NAME : {                                                                                 \
      const uint64_t lhs = sp[-2];                                                                 \
      const uint64_t rhs = sp[-1];                                                                 \
      sp -= 2;                                                                                     \
      locals[inst.a] = (EXPR);                                                                     \
      ip += (LENGTH);                                                                              \
      DISPATCH();                                                                                  \
    }
      FAST_STACK_STORE(FastOpAddStore, lhs + rhs, 2)
      FAST_STACK_STORE(FastOpSubStore, lhs - rhs, 2)
      FAST_STACK_STORE(FastOpMulStore, lhs * rhs, 2)
      FAST_STACK_STORE(FastOpAddSextStore, sext32(lhs + rhs), 3)
      FAST_STACK_STORE(FastOpSubSextStore, sext32(lhs - rhs), 3)
      FAST_STACK_STORE(FastOpMulSextStore, sext32(lhs * rhs), 3)
      FAST_STACK_STORE(FastOpAddStoreF64, pureF64(IrOpcode::AddF64, lhs, rhs), 2)
      FAST_STACK_STORE(FastOpSubStoreF64, pureF64(IrOpcode::SubF64, lhs, rhs), 2)
      FAST_STACK_STORE(FastOpMulStoreF64, pureF64(IrOpcode::MulF64, lhs, rhs), 2)
      FAST_STACK_STORE(FastOpDivStoreF64, pureF64(IrOpcode::DivF64, lhs, rhs), 2)
#undef FAST_STACK_STORE
    case FastOpLocalNegStoreF64:
    lbl_FastOpLocalNegStoreF64:
      locals[inst.b] = pureF64(IrOpcode::NegF64, locals[inst.a], locals[inst.a]);
      ip += 3;
      DISPATCH();

    case FastOpSwitchLocal:
    lbl_FastOpSwitchLocal: {
      const FastSwitch &chain = current->switches[static_cast<size_t>(inst.imm)];
      const uint64_t offset = locals[inst.a] - chain.low;
      ip = current->code.data() + (offset < chain.targets.size()
                                       ? chain.targets[static_cast<size_t>(offset)]
                                       : chain.otherwise);
      DISPATCH();
    }

    case FastOpAddSext:
    lbl_FastOpAddSext:
      sp[-2] = sext32(sp[-2] + sp[-1]);
      --sp;
      ip += 2;
      DISPATCH();
    case FastOpSubSext:
    lbl_FastOpSubSext:
      sp[-2] = sext32(sp[-2] - sp[-1]);
      --sp;
      ip += 2;
      DISPATCH();
    case FastOpMulSext:
    lbl_FastOpMulSext:
      sp[-2] = sext32(sp[-2] * sp[-1]);
      --sp;
      ip += 2;
      DISPATCH();
    case OP(CmpEqI32):
    lbl_CmpEqI32:
    case OP(CmpEqI64):
    lbl_CmpEqI64:
      sp[-2] = sp[-2] == sp[-1];
      --sp;
      ++ip;
      DISPATCH();
    case OP(CmpNeI32):
    lbl_CmpNeI32:
    case OP(CmpNeI64):
    lbl_CmpNeI64:
      sp[-2] = sp[-2] != sp[-1];
      --sp;
      ++ip;
      DISPATCH();
    case OP(CmpLtI32):
    lbl_CmpLtI32:
    case OP(CmpLtI64):
    lbl_CmpLtI64:
      sp[-2] = static_cast<int64_t>(sp[-2]) < static_cast<int64_t>(sp[-1]);
      --sp;
      ++ip;
      DISPATCH();
    case OP(CmpLeI32):
    lbl_CmpLeI32:
    case OP(CmpLeI64):
    lbl_CmpLeI64:
      sp[-2] = static_cast<int64_t>(sp[-2]) <= static_cast<int64_t>(sp[-1]);
      --sp;
      ++ip;
      DISPATCH();
    case OP(CmpGtI32):
    lbl_CmpGtI32:
    case OP(CmpGtI64):
    lbl_CmpGtI64:
      sp[-2] = static_cast<int64_t>(sp[-2]) > static_cast<int64_t>(sp[-1]);
      --sp;
      ++ip;
      DISPATCH();
    case OP(CmpGeI32):
    lbl_CmpGeI32:
    case OP(CmpGeI64):
    lbl_CmpGeI64:
      sp[-2] = static_cast<int64_t>(sp[-2]) >= static_cast<int64_t>(sp[-1]);
      --sp;
      ++ip;
      DISPATCH();
    case OP(CmpLtU64):
    lbl_CmpLtU64:
      sp[-2] = sp[-2] < sp[-1];
      --sp;
      ++ip;
      DISPATCH();
    case OP(CmpLeU64):
    lbl_CmpLeU64:
      sp[-2] = sp[-2] <= sp[-1];
      --sp;
      ++ip;
      DISPATCH();
    case OP(CmpGtU64):
    lbl_CmpGtU64:
      sp[-2] = sp[-2] > sp[-1];
      --sp;
      ++ip;
      DISPATCH();
    case OP(CmpGeU64):
    lbl_CmpGeU64:
      sp[-2] = sp[-2] >= sp[-1];
      --sp;
      ++ip;
      DISPATCH();

    // Indirect accesses: a frame-local address indexes this frame's locals, a heap address
    // (bit 63 set) goes through the host. Faults build their messages out of line.
    case OP(LoadIndirect):
    lbl_LoadIndirect: {
      uint64_t *slot = nullptr;
      if (!indirectSlot(sp[-1], slot)) [[unlikely]] {
        return false;
      }
      sp[-1] = *slot;
      ++ip;
      DISPATCH();
    }
    case OP(StoreIndirect):
    lbl_StoreIndirect: {
      uint64_t *slot = nullptr;
      if (!indirectSlot(sp[-2], slot)) [[unlikely]] {
        return false;
      }
      *slot = sp[-1];
      sp[-2] = sp[-1];
      --sp;
      ++ip;
      DISPATCH();
    }

    case OP(HeapAlloc):
    lbl_HeapAlloc: {
      uint64_t address = 0;
      if (!host.allocateHeapSlots(sp[-1], address, error)) {
        return false;
      }
      sp[-1] = address;
      ++ip;
      DISPATCH();
    }
    case OP(HeapFree):
    lbl_HeapFree:
      if (!host.freeHeapSlots(sp[-1], error)) {
        return false;
      }
      --sp;
      ++ip;
      DISPATCH();
    case OP(HeapRealloc):
    lbl_HeapRealloc: {
      uint64_t newAddress = 0;
      if (!host.reallocHeapSlots(sp[-2], sp[-1], newAddress, error)) {
        return false;
      }
      --sp;
      sp[-1] = newAddress;
      ++ip;
      DISPATCH();
    }

    case OP(LoadStringByte):
    lbl_LoadStringByte:
    case OP(LoadStringByteDynamic):
    lbl_LoadStringByteDynamic: {
      const bool dynamic = inst.op == OP(LoadStringByteDynamic);
      const uint64_t position = sp[-1];
      const uint64_t stringIndex = dynamic ? sp[-2] : inst.imm;
      const std::string *text = nullptr;
      if (!resolveString(stringIndex, text)) {
        return false;
      }
      if (position >= text->size()) {
        FAULT("string index out of bounds in IR");
      }
      if (dynamic) {
        --sp;
      }
      sp[-1] = static_cast<uint64_t>(static_cast<uint8_t>((*text)[static_cast<size_t>(position)]));
      ++ip;
      DISPATCH();
    }
    case OP(LoadStringLength):
    lbl_LoadStringLength: {
      const std::string *text = nullptr;
      if (!resolveString(sp[-1], text)) {
        return false;
      }
      sp[-1] = static_cast<uint64_t>(text->size());
      ++ip;
      DISPATCH();
    }

    case OP(Call):
    lbl_Call:
    case OP(CallVoid):
    lbl_CallVoid: {
      // prepareModule only accepts modules whose call targets exist.
      // The step kernel counts the current frame too.
      if (frames.size() + 1 >= maxCallDepth) {
        FAULT("VM call stack overflow");
      }
      const FastFunction &callee = functions[static_cast<size_t>(inst.imm)];
      const size_t callerTop = localsBase + current->localCount;
      frames.push_back({current, ip + 1, localsBase, inst.op == OP(Call)});
      localsBase = callerTop;
      if (localsBase + callee.localCount > localsArena.size()) {
        localsArena.resize(std::max(localsArena.size() * 2, localsBase + callee.localCount), 0);
      }
      locals = localsArena.data() + localsBase;
      // Locals start at zero. Most functions have a handful, which a memset call would cost
      // more to clear than the stores do.
      switch (callee.localCount) {
      case 4:
        locals[3] = 0;
        [[fallthrough]];
      case 3:
        locals[2] = 0;
        [[fallthrough]];
      case 2:
        locals[1] = 0;
        [[fallthrough]];
      case 1:
        locals[0] = 0;
        [[fallthrough]];
      case 0:
        break;
      default:
        std::fill_n(locals, callee.localCount, uint64_t{0});
        break;
      }
      current = &callee;
      ip = callee.code.data();
      ensureStack(callee.stackHeadroom);
      DISPATCH();
    }

    // Each return form takes its value off the stack (an i32 sign-extended, an f32 as its low
    // 32 bits) and shares the frame pop.
    case OP(ReturnVoid):
    lbl_ReturnVoid:
      returnValue = 0;
      goto return_to_caller;
    case OP(ReturnI32):
    lbl_ReturnI32:
      returnValue = sext32(*--sp);
      goto return_to_caller;
    case OP(ReturnF32):
    lbl_ReturnF32:
      returnValue = static_cast<uint64_t>(static_cast<uint32_t>(*--sp));
      goto return_to_caller;
    case OP(ReturnI64):
    lbl_ReturnI64:
    case OP(ReturnF64):
    lbl_ReturnF64:
      returnValue = *--sp;
    return_to_caller: {
      if (frames.empty()) {
        result = returnValue;
        return true;
      }
      const FastFrame &frame = frames.back();
      current = frame.function;
      ip = frame.returnIp;
      localsBase = frame.localsBase;
      locals = localsArena.data() + localsBase;
      if (frame.returnsValue) {
        *sp++ = returnValue;
      }
      frames.pop_back();
      DISPATCH();
    }

#define FAST_CASE_FLOAT_BINARY(N)                                                                  \
  case OP(N):                                                                                      \
    lbl_##N : {                                                                                    \
      uint64_t value = 0;                                                                          \
      (void)evalPureOpcode(IrOpcode::N, sp[-2], sp[-1], value);                                    \
      --sp;                                                                                        \
      sp[-1] = value;                                                                              \
      ++ip;                                                                                        \
      DISPATCH();                                                                                  \
    }
#define FAST_CASE_FLOAT_UNARY(N)                                                                   \
  case OP(N):                                                                                      \
    lbl_##N : {                                                                                    \
      uint64_t value = 0;                                                                          \
      (void)evalPureOpcode(IrOpcode::N, sp[-1], sp[-1], value);                                    \
      sp[-1] = value;                                                                              \
      ++ip;                                                                                        \
      DISPATCH();                                                                                  \
    }
#define FAST_CASE_LOCAL_F64(NAME, OPCODE, RHS)                                                     \
  case NAME:                                                                                       \
    lbl_##NAME : {                                                                                 \
      uint64_t value = 0;                                                                          \
      (void)evalPureOpcode(IrOpcode::OPCODE, locals[inst.a], RHS, value);                          \
      *sp++ = value;                                                                               \
      ip += 3;                                                                                     \
      DISPATCH();                                                                                  \
    }
      FAST_CASE_LOCAL_F64(FastOpPushLocalAddLocalF64, AddF64, locals[inst.b])
      FAST_CASE_LOCAL_F64(FastOpPushLocalSubLocalF64, SubF64, locals[inst.b])
      FAST_CASE_LOCAL_F64(FastOpPushLocalMulLocalF64, MulF64, locals[inst.b])
      FAST_CASE_LOCAL_F64(FastOpPushLocalDivLocalF64, DivF64, locals[inst.b])
      FAST_CASE_LOCAL_F64(FastOpPushLocalAddImmF64, AddF64, inst.imm)
      FAST_CASE_LOCAL_F64(FastOpPushLocalSubImmF64, SubF64, inst.imm)
      FAST_CASE_LOCAL_F64(FastOpPushLocalMulImmF64, MulF64, inst.imm)
      FAST_CASE_LOCAL_F64(FastOpPushLocalDivImmF64, DivF64, inst.imm)
#undef FAST_CASE_LOCAL_F64
      // Float operations never fault (division by zero gives an infinity or NaN).
      FAST_FLOAT_BINARY(FAST_CASE_FLOAT_BINARY)
      FAST_FLOAT_UNARY(FAST_CASE_FLOAT_UNARY)
#undef FAST_CASE_FLOAT_BINARY
#undef FAST_CASE_FLOAT_UNARY

    case FastOpMissingReturn:
    lbl_FastOpMissingReturn:
      FAULT(frames.empty() ? std::string("missing return in IR")
                           : "missing return in IR function " + current->function->name);

    default:
      break;
    }
  lbl_generic:

    // Arithmetic, comparisons and conversions not inlined above share the
    // semantics of constant folding and the C++ emitters.
    if (const size_t arity = IrPureOpcodeArityTable[inst.op < 256 ? inst.op : 0]; arity != 0) {
      const uint64_t rhs = sp[-1];
      const uint64_t lhs = arity == 2 ? sp[-2] : rhs;
      uint64_t value = 0;
      if (evalPureOpcode(static_cast<IrOpcode>(inst.op), lhs, rhs, value) != IrPureEval::Ok) {
        FAULT("division by zero in IR");
      }
      if (arity == 2) {
        --sp;
      }
      sp[-1] = value;
      ++ip;
      continue;
    }

    // Print, file and host-call opcodes run through the host handlers, which
    // pop and push on a vector: hand them a scratch stack holding exactly the
    // operands they consume.
    {
      const IrOpcode op = static_cast<IrOpcode>(inst.op);
      scratch.assign(sp - inst.pops, sp);
      sp -= inst.pops;
      bool ok = false;
      if (isVmKernelPrintOpcode(op)) {
        ok = host.handlePrintInstruction(module, *inst.source, scratch, error);
      } else if (isVmKernelFileOpcode(op)) {
        // FileReadByte stores into a local through the handler's locals vector.
        frameLocalsCopy.assign(locals, locals + current->localCount);
        ok = host.handleFileInstruction(module, *inst.source, scratch, frameLocalsCopy, error);
        if (ok) {
          std::copy(frameLocalsCopy.begin(), frameLocalsCopy.end(), locals);
        }
      } else if (op == IrOpcode::CallHost) {
        ok = host.handleHostCall(module, *inst.source, scratch, error);
      } else {
        error = "unknown IR opcode";
      }
      if (!ok) {
        return false;
      }
      ensureStack(scratch.size() + 1);
      for (const uint64_t value : scratch) {
        *sp++ = value;
      }
      ++ip;
    }
  }
#undef FAULT
#undef DISPATCH
#undef inst
}

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#undef OP

} // namespace vm_detail

namespace testing {

bool vmFastKernelAccepts(const IrModule &module) {
  std::vector<vm_detail::FastFunction> functions;
  return vm_detail::prepareModule(module, functions);
}

} // namespace testing
} // namespace primec
