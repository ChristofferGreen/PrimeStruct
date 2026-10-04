#include "VmFastKernelProgram.h"

#include "primec/ir/IrCfg.h"
#include "primec/runtime/VmExecutionKernel.h"

#include <algorithm>
#include <vector>

namespace primec::vm_detail {
namespace {

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

int arithmeticKindF64(IrOpcode op) {
  switch (op) {
  case IrOpcode::AddF64:
    return 0;
  case IrOpcode::SubF64:
    return 1;
  case IrOpcode::MulF64:
    return 2;
  case IrOpcode::DivF64:
    return 3;
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
      const bool secondLocal =
          window(i, 2) && op(i + 1) == IrOpcode::LoadLocal && fitsIndex(imm(i + 1));
      if (secondLocal && window(i, 5) && arithmeticKind(op(i + 2)) >= 0 &&
          op(i + 3) == IrOpcode::SextI32 && op(i + 4) == IrOpcode::StoreLocal &&
          fitsIndex(imm(i + 4))) {
        slot.op = static_cast<uint16_t>(FastOpLocalAddLocalSextStore + arithmeticKind(op(i + 2)));
        slot.a = first;
        slot.b = static_cast<uint32_t>(imm(i + 1));
        slot.imm = imm(i + 4);
        length = 5;
      } else if (secondLocal && window(i, 4) && arithmeticKind(op(i + 2)) >= 0 &&
                 op(i + 3) == IrOpcode::StoreLocal && fitsIndex(imm(i + 3))) {
        slot.op = static_cast<uint16_t>(FastOpLocalAddLocalStore + arithmeticKind(op(i + 2)));
        slot.a = first;
        slot.b = static_cast<uint32_t>(imm(i + 1));
        slot.imm = imm(i + 3);
        length = 4;
      } else if (secondLocal && window(i, 4) && arithmeticKindF64(op(i + 2)) >= 0 &&
                 op(i + 3) == IrOpcode::StoreLocal && fitsIndex(imm(i + 3))) {
        slot.op = static_cast<uint16_t>(FastOpLocalAddLocalStoreF64 + arithmeticKindF64(op(i + 2)));
        slot.a = first;
        slot.b = static_cast<uint32_t>(imm(i + 1));
        slot.imm = imm(i + 3);
        length = 4;
      } else if (window(i, 4) && op(i + 1) == IrOpcode::PushF64 &&
                 arithmeticKindF64(op(i + 2)) >= 0 && op(i + 3) == IrOpcode::StoreLocal &&
                 fitsIndex(imm(i + 3))) {
        slot.op = static_cast<uint16_t>(FastOpLocalAddImmStoreF64 + arithmeticKindF64(op(i + 2)));
        slot.a = first;
        slot.imm = imm(i + 1);
        slot.b = static_cast<uint32_t>(imm(i + 3));
        length = 4;
      } else if (window(i, 3) && op(i + 1) == IrOpcode::NegF64 &&
                 op(i + 2) == IrOpcode::StoreLocal && fitsIndex(imm(i + 2))) {
        slot.op = FastOpLocalNegStoreF64;
        slot.a = first;
        slot.b = static_cast<uint32_t>(imm(i + 2));
        length = 3;
      } else if (window(i, 5) && isConstantPush(op(i + 1)) && arithmeticKind(op(i + 2)) >= 0 &&
                 arithmeticKind(op(i + 2)) <= 1 && op(i + 3) == IrOpcode::SextI32 &&
                 op(i + 4) == IrOpcode::StoreLocal && fitsIndex(imm(i + 4))) {
        slot.op = arithmeticKind(op(i + 2)) == 0 ? FastOpLocalAddImmStoreSext
                                                 : FastOpLocalSubImmStoreSext;
        slot.a = first;
        slot.imm = constantOf(function.instructions[i + 1]);
        slot.b = static_cast<uint32_t>(imm(i + 4));
        length = 5;
      } else if (window(i, 4) && isConstantPush(op(i + 1)) && arithmeticKind(op(i + 2)) >= 0 &&
                 op(i + 3) == IrOpcode::SextI32) {
        slot.op = static_cast<uint16_t>(FastOpPushLocalAddImmSext + arithmeticKind(op(i + 2)));
        slot.a = first;
        slot.imm = constantOf(function.instructions[i + 1]);
        length = 4;
      } else if (window(i, 4) && op(i + 1) == IrOpcode::LoadLocal && fitsIndex(imm(i + 1)) &&
                 arithmeticKind(op(i + 2)) >= 0 && op(i + 3) == IrOpcode::SextI32) {
        slot.op = static_cast<uint16_t>(FastOpPushLocalAddLocalSext + arithmeticKind(op(i + 2)));
        slot.a = first;
        slot.b = static_cast<uint32_t>(imm(i + 1));
        length = 4;
      } else if (window(i, 4) && isConstantPush(op(i + 1)) && comparisonKind(op(i + 2)) >= 0 &&
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
      } else if (window(i, 3) && op(i + 1) == IrOpcode::LoadLocal && fitsIndex(imm(i + 1)) &&
                 arithmeticKindF64(op(i + 2)) >= 0) {
        slot.op = static_cast<uint16_t>(FastOpPushLocalAddLocalF64 + arithmeticKindF64(op(i + 2)));
        slot.a = first;
        slot.b = static_cast<uint32_t>(imm(i + 1));
        length = 3;
      } else if (window(i, 3) && op(i + 1) == IrOpcode::PushF64 &&
                 arithmeticKindF64(op(i + 2)) >= 0) {
        slot.op = static_cast<uint16_t>(FastOpPushLocalAddImmF64 + arithmeticKindF64(op(i + 2)));
        slot.a = first;
        slot.imm = imm(i + 1);
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
    } else if (op(i) == IrOpcode::StoreLocal && fitsIndex(imm(i)) && window(i, 5) &&
               op(i + 1) == IrOpcode::LoadLocal && imm(i + 1) == imm(i) &&
               isConstantPush(op(i + 2)) && comparisonKind(op(i + 3)) >= 0 &&
               op(i + 4) == IrOpcode::JumpIfZero && fitsIndex(imm(i + 4))) {
      slot.op = static_cast<uint16_t>(FastOpStoreJmpCmpLocalImmEq + comparisonKind(op(i + 3)));
      slot.a = static_cast<uint32_t>(imm(i));
      slot.imm = constantOf(function.instructions[i + 2]);
      slot.b = static_cast<uint32_t>(imm(i + 4));
      length = 5;
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
    } else if ((op(i) == IrOpcode::AddI32 || op(i) == IrOpcode::SubI32 ||
                op(i) == IrOpcode::MulI32) &&
               window(i, 3) && op(i + 1) == IrOpcode::SextI32 &&
               op(i + 2) == IrOpcode::StoreLocal && fitsIndex(imm(i + 2))) {
      slot.op = static_cast<uint16_t>(FastOpAddSextStore + arithmeticKind(op(i)));
      slot.a = static_cast<uint32_t>(imm(i + 2));
      length = 3;
    } else if ((op(i) == IrOpcode::AddI32 || op(i) == IrOpcode::SubI32 ||
                op(i) == IrOpcode::MulI32) &&
               window(i, 2) && op(i + 1) == IrOpcode::SextI32) {
      slot.op = static_cast<uint16_t>(FastOpAddSext + arithmeticKind(op(i)));
      length = 2;
    } else if (arithmeticKind(op(i)) >= 0 && window(i, 2) && op(i + 1) == IrOpcode::StoreLocal &&
               fitsIndex(imm(i + 1))) {
      slot.op = static_cast<uint16_t>(FastOpAddStore + arithmeticKind(op(i)));
      slot.a = static_cast<uint32_t>(imm(i + 1));
      length = 2;
    } else if (arithmeticKindF64(op(i)) >= 0 && window(i, 2) && op(i + 1) == IrOpcode::StoreLocal &&
               fitsIndex(imm(i + 1))) {
      slot.op = static_cast<uint16_t>(FastOpAddStoreF64 + arithmeticKindF64(op(i)));
      slot.a = static_cast<uint32_t>(imm(i + 1));
      length = 2;
    }
    i += length;
  }
}

// Turns compare chains into table lookups. A fused `local == c` test continues at its own next
// instruction when it holds and jumps to its target otherwise; when that target is another such
// test of the same local, the chain behaves like a switch on the local's value: the first
// constant equal to it picks its continuation, and a value matching none ends at the last
// target. Every test that starts a chain of at least MinimumChain tests whose constants span at
// most MaximumSpan values becomes FastOpSwitchLocal with a dense table; the tests behind it keep
// their own form, so jumps into the middle of a chain still work.
void buildSwitchChains(FastFunction &out) {
  constexpr size_t MinimumChain = 3;
  constexpr size_t MaximumChain = 256;
  constexpr uint64_t MaximumSpan = 256;
  const size_t count = out.code.size();
  const auto isEqualityTest = [&](size_t index, uint32_t local) {
    return index < count && out.code[index].op == FastOpJmpCmpLocalImmEq &&
           out.code[index].a == local;
  };
  // Decide every chain against the unmodified tests, then rewrite.
  std::vector<std::pair<size_t, FastSwitch>> rewrites;
  for (size_t head = 0; head < count; ++head) {
    if (out.code[head].op != FastOpJmpCmpLocalImmEq) {
      continue;
    }
    const uint32_t local = out.code[head].a;
    std::vector<std::pair<uint64_t, uint32_t>> cases; // first occurrence of each constant wins
    std::vector<size_t> visited;
    size_t at = head;
    while (isEqualityTest(at, local) && visited.size() < MaximumChain &&
           std::find(visited.begin(), visited.end(), at) == visited.end()) {
      visited.push_back(at);
      const FastInst &test = out.code[at];
      bool seen = false;
      for (const auto &existing : cases) {
        seen = seen || existing.first == test.imm;
      }
      if (!seen) {
        cases.emplace_back(test.imm, static_cast<uint32_t>(at + 4));
      }
      at = test.b;
    }
    // A chain cut off by the length cap (or a cycle) still ends at a test, which is fine: the
    // lookup continues there.
    if (cases.size() < MinimumChain || at > UINT32_MAX) {
      continue;
    }
    uint64_t low = cases.front().first;
    uint64_t high = low;
    for (const auto &entry : cases) {
      low = std::min(low, entry.first);
      high = std::max(high, entry.first);
    }
    if (high - low >= MaximumSpan) {
      continue;
    }
    FastSwitch table;
    table.low = low;
    table.otherwise = static_cast<uint32_t>(at);
    table.targets.assign(static_cast<size_t>(high - low + 1), table.otherwise);
    for (const auto &entry : cases) {
      table.targets[static_cast<size_t>(entry.first - low)] = entry.second;
    }
    rewrites.emplace_back(head, std::move(table));
  }
  for (auto &[head, table] : rewrites) {
    out.code[head].op = FastOpSwitchLocal;
    out.code[head].imm = out.switches.size();
    out.switches.push_back(std::move(table));
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
    // A call to a missing function is left to the step kernel, which faults on it when (and only
    // if) it runs, so the loop needs no target check.
    if ((instruction.op == IrOpcode::Call || instruction.op == IrOpcode::CallVoid) &&
        instruction.imm >= module.functions.size()) {
      return false;
    }
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
  buildSwitchChains(out);
  out.localCount = computeVmKernelLocalCount(function);
  const int64_t headroom = cfg.maxStackDepth - static_cast<int64_t>(function.parameterCount);
  out.stackHeadroom = static_cast<size_t>(std::max<int64_t>(headroom, 0)) + 2;
  return true;
}

} // namespace

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

} // namespace primec::vm_detail
