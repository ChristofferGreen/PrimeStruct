#include "VmFastKernel.h"

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

// The six integer comparisons the fused compare-and-branch forms cover.
#define FAST_CMPS(X) X(Eq, ==) X(Ne, !=) X(Lt, <) X(Le, <=) X(Gt, >) X(Ge, >=)

#define FAST_ENUM_JMP_CMP_LOCAL_IMM(N, O) FastOpJmpCmpLocalImm##N,
#define FAST_ENUM_JMP_CMP_LOCAL_LOCAL(N, O) FastOpJmpCmpLocalLocal##N,
#define FAST_ENUM_JMP_CMP(N, O) FastOpJmpCmp##N,

// Internal opcodes live above the IrOpcode range. Most are fused sequences of
// IR instructions that stay inside one basic block (see fuseInstructions); a
// fused instruction sits in the slot of the first original instruction and
// advances `ip` past the slots it replaced, which stay in the array untouched
// so every jump target keeps its index.
enum FastOp : uint16_t {
  FastOpMissingReturn = 0x100,
  FastOpStoreLocalDupPop,                  // Dup; StoreLocal a; Pop
  FastOpStoreLocalImm,                     // Push c; StoreLocal a
  FastOpCopyLocal,                         // LoadLocal a; StoreLocal b
  FastOpJmpLocalZero,                      // LoadLocal a; JumpIfZero b
  FastOpPushLocalAddImm,                   // LoadLocal a; Push c; Add
  FastOpPushLocalSubImm,                   // LoadLocal a; Push c; Sub
  FastOpPushLocalMulImm,                   // LoadLocal a; Push c; Mul
  FastOpPushLocalAddLocal,                 // LoadLocal a; LoadLocal b; Add
  FastOpPushLocalSubLocal,                 // LoadLocal a; LoadLocal b; Sub
  FastOpPushLocalMulLocal,                 // LoadLocal a; LoadLocal b; Mul
  FastOpLocalAddImmStore,                  // LoadLocal a; Push c; Add; StoreLocal b
  FastOpLocalSubImmStore,                  // LoadLocal a; Push c; Sub; StoreLocal b
  FastOpLocalStringByteStore,              // LoadLocal a; LoadStringByte #imm; StoreLocal b
  FastOpPushLocalStringByte,               // LoadLocal a; LoadStringByte #imm
  FAST_CMPS(FAST_ENUM_JMP_CMP_LOCAL_IMM)   // LoadLocal a; Push c; Cmp; JumpIfZero b
  FAST_CMPS(FAST_ENUM_JMP_CMP_LOCAL_LOCAL) // LoadLocal a; LoadLocal b; Cmp; JumpIfZero imm
  FAST_CMPS(FAST_ENUM_JMP_CMP)             // Cmp; JumpIfZero b
};

#undef FAST_ENUM_JMP_CMP_LOCAL_IMM
#undef FAST_ENUM_JMP_CMP_LOCAL_LOCAL
#undef FAST_ENUM_JMP_CMP

struct FastInst {
  uint16_t op = 0;
  // Operands the instruction pops and results it pushes, from the shared stack
  // effect table. Used by the opcodes that go through the host handlers.
  uint16_t pops = 0;
  uint16_t pushes = 0;
  // Local indices and jump targets of the fused forms.
  uint32_t a = 0;
  uint32_t b = 0;
  uint64_t imm = 0;
  const IrInstruction *source = nullptr;
};

struct FastFunction {
  const IrFunction *function = nullptr;
  // The function's instructions followed by a sentinel that faults with the
  // step kernel's "missing return" message, so falling off the end (or jumping
  // to it) needs no bounds check in the loop.
  std::vector<FastInst> code;
  size_t localCount = 0;
  // Operand-stack slots the function needs above its arguments.
  size_t stackHeadroom = 0;
};

struct FastFrame {
  const FastFunction *function = nullptr;
  const FastInst *returnIp = nullptr;
  size_t localsBase = 0;
  bool returnsValue = false;
};

bool isReturnOpcode(IrOpcode op) {
  switch (op) {
  case IrOpcode::ReturnVoid:
  case IrOpcode::ReturnI32:
  case IrOpcode::ReturnI64:
  case IrOpcode::ReturnF32:
  case IrOpcode::ReturnF64:
    return true;
  default:
    return false;
  }
}

int comparisonKind(IrOpcode op) {
  switch (op) {
  case IrOpcode::CmpEqI32:
  case IrOpcode::CmpEqI64:
    return 0;
  case IrOpcode::CmpNeI32:
  case IrOpcode::CmpNeI64:
    return 1;
  case IrOpcode::CmpLtI32:
  case IrOpcode::CmpLtI64:
    return 2;
  case IrOpcode::CmpLeI32:
  case IrOpcode::CmpLeI64:
    return 3;
  case IrOpcode::CmpGtI32:
  case IrOpcode::CmpGtI64:
    return 4;
  case IrOpcode::CmpGeI32:
  case IrOpcode::CmpGeI64:
    return 5;
  default:
    return -1;
  }
}

// 0 add, 1 sub, 2 mul; -1 otherwise. The I32 and I64 forms share semantics.
int arithmeticKind(IrOpcode op) {
  switch (op) {
  case IrOpcode::AddI32:
  case IrOpcode::AddI64:
    return 0;
  case IrOpcode::SubI32:
  case IrOpcode::SubI64:
    return 1;
  case IrOpcode::MulI32:
  case IrOpcode::MulI64:
    return 2;
  default:
    return -1;
  }
}

bool isConstantPush(IrOpcode op) {
  return op == IrOpcode::PushI32 || op == IrOpcode::PushI64;
}

uint64_t constantOf(const IrInstruction &instruction) {
  return instruction.op == IrOpcode::PushI32
             ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(instruction.imm)))
             : instruction.imm;
}

bool fitsIndex(uint64_t value) {
  return value <= UINT32_MAX;
}

// Rewrites the first slot of recognised instruction sequences into a fused
// instruction. A sequence never spans a basic-block leader, so nothing can jump
// into its middle, and every fused form is free of faults and host effects,
// which keeps results and fault order identical to executing the originals
// (the string-byte forms fault on a bad index exactly where the original
// LoadStringByte would, before anything has been stored or popped).
void fuseInstructions(const IrFunction &function, const IrCfg &cfg, FastFunction &out) {
  const size_t count = function.instructions.size();
  std::vector<bool> leader(count + 1, false);
  for (const IrCfgBlock &block : cfg.blocks) {
    leader[block.start] = true;
  }
  const auto op = [&](size_t index) { return function.instructions[index].op; };
  const auto imm = [&](size_t index) { return function.instructions[index].imm; };
  // True when `length` instructions starting at `index` exist and none after the
  // first starts a block.
  const auto window = [&](size_t index, size_t length) {
    if (index + length > count) {
      return false;
    }
    for (size_t k = 1; k < length; ++k) {
      if (leader[index + k]) {
        return false;
      }
    }
    return true;
  };
  for (size_t i = 0; i < count;) {
    FastInst &slot = out.code[i];
    size_t length = 1;
    if (op(i) == IrOpcode::LoadLocal && fitsIndex(imm(i))) {
      const uint32_t first = static_cast<uint32_t>(imm(i));
      if (window(i, 4) && isConstantPush(op(i + 1)) && comparisonKind(op(i + 2)) >= 0 &&
          op(i + 3) == IrOpcode::JumpIfZero && fitsIndex(imm(i + 3))) {
        slot.op = static_cast<uint16_t>(FastOpJmpCmpLocalImmEq + comparisonKind(op(i + 2)));
        slot.a = first;
        slot.imm = constantOf(function.instructions[i + 1]);
        slot.b = static_cast<uint32_t>(imm(i + 3));
        length = 4;
      } else if (window(i, 4) && op(i + 1) == IrOpcode::LoadLocal && fitsIndex(imm(i + 1)) &&
                 comparisonKind(op(i + 2)) >= 0 && op(i + 3) == IrOpcode::JumpIfZero) {
        slot.op = static_cast<uint16_t>(FastOpJmpCmpLocalLocalEq + comparisonKind(op(i + 2)));
        slot.a = first;
        slot.b = static_cast<uint32_t>(imm(i + 1));
        slot.imm = imm(i + 3);
        length = 4;
      } else if (window(i, 4) && isConstantPush(op(i + 1)) && arithmeticKind(op(i + 2)) >= 0 &&
                 arithmeticKind(op(i + 2)) <= 1 && op(i + 3) == IrOpcode::StoreLocal &&
                 fitsIndex(imm(i + 3))) {
        slot.op = arithmeticKind(op(i + 2)) == 0 ? FastOpLocalAddImmStore : FastOpLocalSubImmStore;
        slot.a = first;
        slot.imm = constantOf(function.instructions[i + 1]);
        slot.b = static_cast<uint32_t>(imm(i + 3));
        length = 4;
      } else if (window(i, 3) && op(i + 1) == IrOpcode::LoadStringByte &&
                 op(i + 2) == IrOpcode::StoreLocal && fitsIndex(imm(i + 2))) {
        slot.op = FastOpLocalStringByteStore;
        slot.a = first;
        slot.b = static_cast<uint32_t>(imm(i + 2));
        slot.imm = imm(i + 1);
        length = 3;
      } else if (window(i, 2) && op(i + 1) == IrOpcode::LoadStringByte) {
        slot.op = FastOpPushLocalStringByte;
        slot.a = first;
        slot.imm = imm(i + 1);
        length = 2;
      } else if (window(i, 3) && isConstantPush(op(i + 1)) && arithmeticKind(op(i + 2)) >= 0) {
        slot.op = static_cast<uint16_t>(FastOpPushLocalAddImm + arithmeticKind(op(i + 2)));
        slot.a = first;
        slot.imm = constantOf(function.instructions[i + 1]);
        length = 3;
      } else if (window(i, 3) && op(i + 1) == IrOpcode::LoadLocal && fitsIndex(imm(i + 1)) &&
                 arithmeticKind(op(i + 2)) >= 0) {
        slot.op = static_cast<uint16_t>(FastOpPushLocalAddLocal + arithmeticKind(op(i + 2)));
        slot.a = first;
        slot.b = static_cast<uint32_t>(imm(i + 1));
        length = 3;
      } else if (window(i, 2) && op(i + 1) == IrOpcode::JumpIfZero && fitsIndex(imm(i + 1))) {
        slot.op = FastOpJmpLocalZero;
        slot.a = first;
        slot.b = static_cast<uint32_t>(imm(i + 1));
        length = 2;
      } else if (window(i, 2) && op(i + 1) == IrOpcode::StoreLocal && fitsIndex(imm(i + 1))) {
        slot.op = FastOpCopyLocal;
        slot.a = first;
        slot.b = static_cast<uint32_t>(imm(i + 1));
        length = 2;
      }
    } else if (comparisonKind(op(i)) >= 0 && window(i, 2) && op(i + 1) == IrOpcode::JumpIfZero &&
               fitsIndex(imm(i + 1))) {
      slot.op = static_cast<uint16_t>(FastOpJmpCmpEq + comparisonKind(op(i)));
      slot.b = static_cast<uint32_t>(imm(i + 1));
      length = 2;
    } else if (isConstantPush(op(i)) && window(i, 2) && op(i + 1) == IrOpcode::StoreLocal &&
               fitsIndex(imm(i + 1))) {
      slot.op = FastOpStoreLocalImm;
      slot.imm = constantOf(function.instructions[i]);
      slot.a = static_cast<uint32_t>(imm(i + 1));
      length = 2;
    } else if (op(i) == IrOpcode::Dup && window(i, 3) && op(i + 1) == IrOpcode::StoreLocal &&
               op(i + 2) == IrOpcode::Pop && fitsIndex(imm(i + 1))) {
      slot.op = FastOpStoreLocalDupPop;
      slot.a = static_cast<uint32_t>(imm(i + 1));
      length = 3;
    }
    i += length;
  }
}

// Builds the flat form of `function`, or returns false when the loop's
// assumptions do not hold for it.
bool prepareFunction(const IrModule &module, const IrFunction &function, FastFunction &out) {
  IrCfg cfg;
  IrCfgError cfgError;
  if (!buildIrCfg(function, module, cfg, cfgError)) {
    return false;
  }
  out.function = &function;
  out.code.reserve(function.instructions.size() + 1);
  for (const IrInstruction &instruction : function.instructions) {
    IrStackEffect effect;
    if (!computeIrStackEffect(instruction, module, effect) || effect.pops > UINT16_MAX ||
        effect.pushes > UINT16_MAX) {
      return false;
    }
    FastInst inst;
    inst.op = static_cast<uint16_t>(instruction.op);
    inst.pops = static_cast<uint16_t>(effect.pops);
    inst.pushes = static_cast<uint16_t>(effect.pushes);
    inst.imm = instruction.imm;
    inst.source = &instruction;
    out.code.push_back(inst);
  }
  FastInst sentinel;
  sentinel.op = FastOpMissingReturn;
  out.code.push_back(sentinel);

  // A return must leave exactly the caller's operands behind: the step kernel
  // would leave anything extra on the shared stack, which no valid lowering
  // produces and the loop does not model.
  for (const IrCfgBlock &block : cfg.blocks) {
    if (!block.reachable) {
      continue;
    }
    int64_t depth = block.entryDepth;
    for (size_t i = block.start; i < block.end; ++i) {
      const FastInst &inst = out.code[i];
      if (isReturnOpcode(function.instructions[i].op) && depth != static_cast<int64_t>(inst.pops)) {
        return false;
      }
      depth += static_cast<int64_t>(inst.pushes) - static_cast<int64_t>(inst.pops);
    }
  }
  fuseInstructions(function, cfg, out);
  out.localCount = computeVmKernelLocalCount(function);
  const int64_t headroom = cfg.maxStackDepth - static_cast<int64_t>(function.parameterCount);
  out.stackHeadroom = static_cast<size_t>(std::max<int64_t>(headroom, 0)) + 2;
  return true;
}

// Prepares every function; false when any of them (or the entry) is not eligible.
bool prepareModule(const IrModule &module, std::vector<FastFunction> &functions) {
  if (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
    return false;
  }
  if (module.functions[static_cast<size_t>(module.entryIndex)].parameterCount != 0) {
    return false;
  }
  functions.assign(module.functions.size(), FastFunction{});
  for (size_t i = 0; i < module.functions.size(); ++i) {
    if (!prepareFunction(module, module.functions[i], functions[i])) {
      return false;
    }
  }
  return true;
}

constexpr size_t InitialStackSlots = 4096;

#define OP(name) static_cast<uint16_t>(IrOpcode::name)

} // namespace

bool executeVmFastKernel(const IrModule &module,
                         VmKernelHost &host,
                         uint64_t &result,
                         std::string &error,
                         bool &executed) {
  executed = false;
  std::vector<FastFunction> functions;
  if (!prepareModule(module, functions)) {
    return false;
  }
  executed = true;

  const size_t maxCallDepth = host.maxCallDepth();
  const uint64_t slotBytes = host.slotBytes();
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

  error.clear();
  for (;;) {
    const FastInst &inst = *ip;
    switch (inst.op) {
    case OP(PushI32):
      *sp++ = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(inst.imm)));
      ++ip;
      continue;
    case OP(PushI64):
    case OP(PushF32):
    case OP(PushF64):
      *sp++ = inst.imm;
      ++ip;
      continue;
    case OP(PushArgc):
      *sp++ = argc;
      ++ip;
      continue;
    case OP(LoadLocal):
      *sp++ = locals[inst.imm];
      ++ip;
      continue;
    case OP(StoreLocal):
      locals[inst.imm] = *--sp;
      ++ip;
      continue;
    case OP(AddressOfLocal):
      *sp++ = inst.imm * slotBytes;
      ++ip;
      continue;
    case OP(Dup):
      sp[0] = sp[-1];
      ++sp;
      ++ip;
      continue;
    case OP(Pop):
      --sp;
      ++ip;
      continue;
    case OP(Jump):
      ip = current->code.data() + inst.imm;
      continue;
    case OP(JumpIfZero):
      ip = *--sp == 0 ? current->code.data() + inst.imm : ip + 1;
      continue;

    case FastOpStoreLocalDupPop:
      locals[inst.a] = *--sp;
      ip += 3;
      continue;
    case FastOpStoreLocalImm:
      locals[inst.a] = inst.imm;
      ip += 2;
      continue;
    case FastOpCopyLocal:
      locals[inst.b] = locals[inst.a];
      ip += 2;
      continue;
    case FastOpJmpLocalZero:
      ip = locals[inst.a] == 0 ? current->code.data() + inst.b : ip + 2;
      continue;
    case FastOpPushLocalAddImm:
      *sp++ = locals[inst.a] + inst.imm;
      ip += 3;
      continue;
    case FastOpPushLocalSubImm:
      *sp++ = locals[inst.a] - inst.imm;
      ip += 3;
      continue;
    case FastOpPushLocalMulImm:
      *sp++ = locals[inst.a] * inst.imm;
      ip += 3;
      continue;
    case FastOpPushLocalAddLocal:
      *sp++ = locals[inst.a] + locals[inst.b];
      ip += 3;
      continue;
    case FastOpPushLocalSubLocal:
      *sp++ = locals[inst.a] - locals[inst.b];
      ip += 3;
      continue;
    case FastOpPushLocalMulLocal:
      *sp++ = locals[inst.a] * locals[inst.b];
      ip += 3;
      continue;
    case FastOpLocalAddImmStore:
      locals[inst.b] = locals[inst.a] + inst.imm;
      ip += 4;
      continue;
    case FastOpLocalSubImmStore:
      locals[inst.b] = locals[inst.a] - inst.imm;
      ip += 4;
      continue;
    case FastOpLocalStringByteStore:
    case FastOpPushLocalStringByte: {
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
      continue;
    }
#define FAST_CASE_JMP_CMP_LOCAL_IMM(N, O)                                                          \
  case FastOpJmpCmpLocalImm##N:                                                                    \
    ip = static_cast<int64_t>(locals[inst.a]) O static_cast<int64_t>(inst.imm)                     \
             ? ip + 4                                                                              \
             : current->code.data() + inst.b;                                                      \
    continue;
#define FAST_CASE_JMP_CMP_LOCAL_LOCAL(N, O)                                                        \
  case FastOpJmpCmpLocalLocal##N:                                                                  \
    ip = static_cast<int64_t>(locals[inst.a]) O static_cast<int64_t>(locals[inst.b])               \
             ? ip + 4                                                                              \
             : current->code.data() + inst.imm;                                                    \
    continue;
#define FAST_CASE_JMP_CMP(N, O)                                                                    \
  case FastOpJmpCmp##N: {                                                                          \
    const bool taken = static_cast<int64_t>(sp[-2]) O static_cast<int64_t>(sp[-1]);                \
    sp -= 2;                                                                                       \
    ip = taken ? ip + 2 : current->code.data() + inst.b;                                           \
    continue;                                                                                      \
  }
      FAST_CMPS(FAST_CASE_JMP_CMP_LOCAL_IMM)
      FAST_CMPS(FAST_CASE_JMP_CMP_LOCAL_LOCAL)
      FAST_CMPS(FAST_CASE_JMP_CMP)
#undef FAST_CASE_JMP_CMP_LOCAL_IMM
#undef FAST_CASE_JMP_CMP_LOCAL_LOCAL
#undef FAST_CASE_JMP_CMP

    case OP(AddI32):
    case OP(AddI64):
      sp[-2] += sp[-1];
      --sp;
      ++ip;
      continue;
    case OP(SubI32):
    case OP(SubI64):
      sp[-2] -= sp[-1];
      --sp;
      ++ip;
      continue;
    case OP(MulI32):
    case OP(MulI64):
      sp[-2] *= sp[-1];
      --sp;
      ++ip;
      continue;
    case OP(NegI32):
    case OP(NegI64):
      sp[-1] = uint64_t{0} - sp[-1];
      ++ip;
      continue;
    case OP(CmpEqI32):
    case OP(CmpEqI64):
      sp[-2] = sp[-2] == sp[-1];
      --sp;
      ++ip;
      continue;
    case OP(CmpNeI32):
    case OP(CmpNeI64):
      sp[-2] = sp[-2] != sp[-1];
      --sp;
      ++ip;
      continue;
    case OP(CmpLtI32):
    case OP(CmpLtI64):
      sp[-2] = static_cast<int64_t>(sp[-2]) < static_cast<int64_t>(sp[-1]);
      --sp;
      ++ip;
      continue;
    case OP(CmpLeI32):
    case OP(CmpLeI64):
      sp[-2] = static_cast<int64_t>(sp[-2]) <= static_cast<int64_t>(sp[-1]);
      --sp;
      ++ip;
      continue;
    case OP(CmpGtI32):
    case OP(CmpGtI64):
      sp[-2] = static_cast<int64_t>(sp[-2]) > static_cast<int64_t>(sp[-1]);
      --sp;
      ++ip;
      continue;
    case OP(CmpGeI32):
    case OP(CmpGeI64):
      sp[-2] = static_cast<int64_t>(sp[-2]) >= static_cast<int64_t>(sp[-1]);
      --sp;
      ++ip;
      continue;
    case OP(CmpLtU64):
      sp[-2] = sp[-2] < sp[-1];
      --sp;
      ++ip;
      continue;
    case OP(CmpLeU64):
      sp[-2] = sp[-2] <= sp[-1];
      --sp;
      ++ip;
      continue;
    case OP(CmpGtU64):
      sp[-2] = sp[-2] > sp[-1];
      --sp;
      ++ip;
      continue;
    case OP(CmpGeU64):
      sp[-2] = sp[-2] >= sp[-1];
      --sp;
      ++ip;
      continue;

    case OP(LoadIndirect):
    case OP(StoreIndirect): {
      const bool isStore = inst.op == OP(StoreIndirect);
      const uint64_t value = isStore ? sp[-1] : 0;
      const uint64_t address = isStore ? sp[-2] : sp[-1];
      uint64_t *slot = nullptr;
      if (address % slotBytes != 0) {
        FAULT("unaligned indirect address in IR: " + std::to_string(address));
      }
      if ((address & (uint64_t{1} << 63)) != 0) {
        if (!host.resolveIndirectAddress(address, noLocals, slot, error)) {
          return false;
        }
      } else {
        const uint64_t index = address / slotBytes;
        if (index >= current->localCount) {
          FAULT("invalid indirect address in IR: " + std::to_string(address));
        }
        slot = locals + index;
      }
      if (isStore) {
        *slot = value;
        sp[-2] = value;
        --sp;
      } else {
        sp[-1] = *slot;
      }
      ++ip;
      continue;
    }

    case OP(HeapAlloc): {
      uint64_t address = 0;
      if (!host.allocateHeapSlots(sp[-1], address, error)) {
        return false;
      }
      sp[-1] = address;
      ++ip;
      continue;
    }
    case OP(HeapFree):
      if (!host.freeHeapSlots(sp[-1], error)) {
        return false;
      }
      --sp;
      ++ip;
      continue;
    case OP(HeapRealloc): {
      uint64_t newAddress = 0;
      if (!host.reallocHeapSlots(sp[-2], sp[-1], newAddress, error)) {
        return false;
      }
      --sp;
      sp[-1] = newAddress;
      ++ip;
      continue;
    }

    case OP(LoadStringByte):
    case OP(LoadStringByteDynamic): {
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
      continue;
    }
    case OP(LoadStringLength): {
      const std::string *text = nullptr;
      if (!resolveString(sp[-1], text)) {
        return false;
      }
      sp[-1] = static_cast<uint64_t>(text->size());
      ++ip;
      continue;
    }

    case OP(Call):
    case OP(CallVoid): {
      if (inst.imm >= functions.size()) {
        FAULT("invalid call target in IR");
      }
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
      std::fill_n(localsArena.begin() + static_cast<std::ptrdiff_t>(localsBase),
                  callee.localCount,
                  uint64_t{0});
      locals = localsArena.data() + localsBase;
      current = &callee;
      ip = callee.code.data();
      ensureStack(callee.stackHeadroom);
      continue;
    }

    case OP(ReturnVoid):
    case OP(ReturnI32):
    case OP(ReturnI64):
    case OP(ReturnF32):
    case OP(ReturnF64): {
      uint64_t value = 0;
      switch (inst.op) {
      case OP(ReturnI32):
        value = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(*--sp)));
        break;
      case OP(ReturnI64):
      case OP(ReturnF64):
        value = *--sp;
        break;
      case OP(ReturnF32):
        value = static_cast<uint64_t>(static_cast<uint32_t>(*--sp));
        break;
      default:
        break;
      }
      if (frames.empty()) {
        result = value;
        return true;
      }
      const FastFrame frame = frames.back();
      frames.pop_back();
      current = frame.function;
      ip = frame.returnIp;
      localsBase = frame.localsBase;
      locals = localsArena.data() + localsBase;
      if (frame.returnsValue) {
        *sp++ = value;
      }
      continue;
    }

    case FastOpMissingReturn:
      FAULT(frames.empty() ? std::string("missing return in IR")
                           : "missing return in IR function " + current->function->name);

    default:
      break;
    }

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
}

#undef OP

} // namespace vm_detail

namespace testing {

bool vmFastKernelAccepts(const IrModule &module) {
  std::vector<vm_detail::FastFunction> functions;
  return vm_detail::prepareModule(module, functions);
}

} // namespace testing
} // namespace primec
