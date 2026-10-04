// Register-allocated function bodies for the x86_64 emitter (see NativeEmitterRegAlloc.h for the
// plan). Integer arithmetic, comparisons, branches, frame locals and string bytes are emitted on
// the planned registers; every other opcode runs its ordinary template, which works on the
// memory operand stack: its operands are pushed there first and its results popped into their
// registers afterwards, with the registers live across it saved in their frame slots.
//
// rax, rcx and rdx are never allocated: they are the scratch registers for spilled values,
// immediates, parallel-move cycles and the templates themselves.

inline int32_t X64Emitter::regAllocSlotDisp(uint32_t pseudoLocal) const {
  return -static_cast<int32_t>(frameSize_ - localOffset(pseudoLocal));
}

inline bool X64Emitter::emitRegisterAllocatedFunction(const IrFunction &fn,
                                                      const RegAllocFunctionPlan &plan,
                                                      uint32_t spillBaseLocal,
                                                      std::vector<size_t> &instOffsets,
                                                      const RegAllocHooks &hooks,
                                                      std::string &error) {
  constexpr uint8_t Rax = 0;
  constexpr uint8_t Rcx = 1;
  // The templates must see a plain memory operand stack.
  flushValueStackCache();
  const bool savedDefer = deferOperands_;
  const bool savedCache = valueStackCacheEnabled_;
  deferOperands_ = false;
  valueStackCacheEnabled_ = false;
  hasValueStackCache_ = false;
  clearPromotedLocals();

  const uint32_t saveBaseLocal = spillBaseLocal + plan.spillSlotCount;
  const auto location = [&](uint32_t value) -> const RegAllocLocation & {
    return plan.locations[value];
  };
  const auto slotDisp = [&](uint32_t slot) { return regAllocSlotDisp(spillBaseLocal + slot); };
  const auto fitsImm32 = [](uint64_t imm) {
    const int64_t value = static_cast<int64_t>(imm);
    return value >= INT32_MIN && value <= INT32_MAX;
  };
  // The register holding `value`, loading it into `scratch` when it is spilled or a constant.
  const auto valueReg = [&](uint32_t value, uint8_t scratch) -> uint8_t {
    const RegAllocLocation &where = location(value);
    switch (where.kind) {
    case RegAllocLocationKind::Reg:
      return where.reg;
    case RegAllocLocationKind::Slot:
      emitLoadMem(scratch, 5, slotDisp(where.slot));
      return scratch;
    case RegAllocLocationKind::Imm:
      emitMovRegImm64(scratch, where.imm);
      return scratch;
    case RegAllocLocationKind::None:
      break;
    }
    emitMovRegImm64(scratch, 0);
    return scratch;
  };
  // Copies `value` into `target` (which may already hold it).
  const auto loadInto = [&](uint32_t value, uint8_t target) {
    const RegAllocLocation &where = location(value);
    if (where.kind == RegAllocLocationKind::Reg) {
      if (where.reg != target) {
        emitMovRegReg(target, where.reg);
      }
      return;
    }
    valueReg(value, target);
  };
  const auto storeValue = [&](uint32_t value, uint8_t reg) {
    const RegAllocLocation &where = location(value);
    if (where.kind == RegAllocLocationKind::Reg) {
      if (where.reg != reg) {
        emitMovRegReg(where.reg, reg);
      }
    } else if (where.kind == RegAllocLocationKind::Slot) {
      emitStoreMem(5, slotDisp(where.slot), reg);
    }
  };
  // Where to compute `value`: its own register unless that is `avoid` (an operand still
  // needed), otherwise rax.
  const auto targetReg = [&](uint32_t value, int avoid) -> uint8_t {
    const RegAllocLocation &where = location(value);
    if (where.kind == RegAllocLocationKind::Reg && static_cast<int>(where.reg) != avoid) {
      return where.reg;
    }
    return Rax;
  };

  // Parallel copies on an edge: every destination receives its source's value from before the
  // copies. rax breaks cycles; rcx carries slot-to-slot and wide-immediate copies.
  struct PendingMove {
    RegAllocLocation source;
    RegAllocLocation destination;
  };
  const auto sameLocation = [](const RegAllocLocation &a, const RegAllocLocation &b) {
    if (a.kind != b.kind) {
      return false;
    }
    if (a.kind == RegAllocLocationKind::Reg) {
      return a.reg == b.reg;
    }
    if (a.kind == RegAllocLocationKind::Slot) {
      return a.slot == b.slot;
    }
    return false;
  };
  const auto emitCopy = [&](const RegAllocLocation &source, const RegAllocLocation &destination) {
    uint8_t reg = Rcx;
    if (source.kind == RegAllocLocationKind::Reg) {
      reg = source.reg;
    } else if (source.kind == RegAllocLocationKind::Imm) {
      if (destination.kind == RegAllocLocationKind::Slot && fitsImm32(source.imm)) {
        emitStoreImm64Mem(5, slotDisp(destination.slot), source.imm);
        return;
      }
      reg = destination.kind == RegAllocLocationKind::Reg ? destination.reg : Rcx;
      emitMovRegImm64(reg, source.imm);
    } else if (source.kind == RegAllocLocationKind::Slot) {
      reg = destination.kind == RegAllocLocationKind::Reg ? destination.reg : Rcx;
      emitLoadMem(reg, 5, slotDisp(source.slot));
    }
    if (destination.kind == RegAllocLocationKind::Reg) {
      if (destination.reg != reg) {
        emitMovRegReg(destination.reg, reg);
      }
    } else if (destination.kind == RegAllocLocationKind::Slot) {
      emitStoreMem(5, slotDisp(destination.slot), reg);
    }
  };
  const auto emitEdgeMoves = [&](const RegAllocEdge &edge) {
    std::vector<PendingMove> moves;
    for (const RegAllocMove &move : edge.moves) {
      const RegAllocLocation &destination = location(move.destination);
      if (destination.kind != RegAllocLocationKind::Reg &&
          destination.kind != RegAllocLocationKind::Slot) {
        continue;
      }
      const RegAllocLocation &source = location(move.source);
      if (sameLocation(source, destination)) {
        continue;
      }
      moves.push_back({source, destination});
    }
    while (!moves.empty()) {
      bool progressed = false;
      for (size_t i = 0; i < moves.size(); ++i) {
        bool blocked = false;
        for (size_t j = 0; j < moves.size(); ++j) {
          if (j != i && sameLocation(moves[j].source, moves[i].destination)) {
            blocked = true;
            break;
          }
        }
        if (!blocked) {
          emitCopy(moves[i].source, moves[i].destination);
          moves.erase(moves.begin() + static_cast<std::ptrdiff_t>(i));
          progressed = true;
          break;
        }
      }
      if (progressed) {
        continue;
      }
      // Every destination is still some source: a cycle. Park the first destination's value in
      // rax and let its readers take it from there.
      const RegAllocLocation parked = moves.front().destination;
      RegAllocLocation rax;
      rax.kind = RegAllocLocationKind::Reg;
      rax.reg = Rax;
      emitCopy(parked, rax);
      for (PendingMove &move : moves) {
        if (sameLocation(move.source, parked)) {
          move.source = rax;
        }
      }
    }
  };

  // Branch targets are blocks; jumps into a block that needs edge copies go through a
  // trampoline emitted after the body.
  struct JumpFixup {
    size_t codeIndex = 0;
    size_t targetBlock = 0;
    size_t trampoline = SIZE_MAX;
    bool conditional = false;
  };
  struct Trampoline {
    size_t block = 0;
    size_t edge = 0;
  };
  std::vector<JumpFixup> fixups;
  std::vector<Trampoline> trampolines;
  std::vector<size_t> blockOffset(plan.blocks.size(), SIZE_MAX);
  std::vector<bool> loopHeader(plan.blocks.size(), false);
  for (size_t blockIndex = 0; blockIndex < plan.blocks.size(); ++blockIndex) {
    for (const RegAllocEdge &edge : plan.blocks[blockIndex].edges) {
      if (edge.successorBlock <= blockIndex) {
        loopHeader[edge.successorBlock] = true;
      }
    }
  }
  const auto findEdge = [&](const RegAllocBlock &block, size_t successor) -> const RegAllocEdge * {
    for (const RegAllocEdge &edge : block.edges) {
      if (edge.successorBlock == successor) {
        return &edge;
      }
    }
    return nullptr;
  };
  const auto blockStartingAt = [&](size_t instruction) -> size_t {
    for (size_t blockIndex = 0; blockIndex < plan.blocks.size(); ++blockIndex) {
      if (plan.blocks[blockIndex].start == instruction) {
        return blockIndex;
      }
    }
    return SIZE_MAX;
  };
  // A jump along `edge` of `blockIndex`. A conditional jump runs the edge's copies in a
  // trampoline; an unconditional one is emitted after them.
  const auto emitEdgeJump =
      [&](size_t blockIndex, const RegAllocEdge &edge, bool conditional, CondCode cc) {
        JumpFixup fixup;
        fixup.conditional = conditional;
        fixup.targetBlock = edge.successorBlock;
        bool needsMoves = false;
        for (const RegAllocMove &move : edge.moves) {
          if (conditional && !sameLocation(location(move.source), location(move.destination))) {
            needsMoves = true;
            break;
          }
        }
        if (needsMoves) {
          fixup.trampoline = trampolines.size();
          trampolines.push_back(
              {blockIndex, static_cast<size_t>(&edge - plan.blocks[blockIndex].edges.data())});
        }
        fixup.codeIndex = conditional ? emitCondJumpPlaceholder(cc) : emitJumpPlaceholder();
        fixups.push_back(fixup);
      };

  // Sets the flags from `compare`'s operands; returns the condition under which it is true.
  const auto emitCompareFlags = [&](const RegAllocInstruction &compare, CondCode &cc) -> bool {
    if (!compareCondition(fn.instructions[compare.irIndex].op, cc)) {
      return false;
    }
    const uint32_t a = compare.uses[0];
    const uint32_t b = compare.uses[1];
    const uint8_t left = valueReg(a, Rax);
    const RegAllocLocation &right = location(b);
    if (right.kind == RegAllocLocationKind::Imm && fitsImm32(right.imm)) {
      compareStart_ = code_.size();
      emitCmpRegImm32(left, static_cast<int32_t>(right.imm));
    } else {
      const uint8_t rightReg = valueReg(b, Rcx);
      compareStart_ = code_.size();
      emitCmpRegReg(left, rightReg);
    }
    return true;
  };

  // Entry: the parameters come off the operand stack, top first; locals read before any store
  // start at zero.
  if (!plan.blocks.empty() && plan.blocks[0].reachable) {
    const RegAllocBlock &first = plan.blocks[0];
    for (size_t k = first.entryValues.size(); k-- > 0;) {
      const RegAllocLocation &where = location(first.entryValues[k]);
      if (where.kind == RegAllocLocationKind::Reg) {
        emitReloadReg(where.reg);
      } else {
        emitReloadReg(Rax);
        storeValue(first.entryValues[k], Rax);
      }
    }
    for (const uint32_t value : first.zeroValues) {
      const RegAllocLocation &where = location(value);
      if (where.kind == RegAllocLocationKind::Reg) {
        emitMovRegImm64(where.reg, 0);
      } else if (where.kind == RegAllocLocationKind::Slot) {
        emitStoreImm64Mem(5, slotDisp(where.slot), 0);
      }
    }
  }

  for (size_t blockIndex = 0; blockIndex < plan.blocks.size(); ++blockIndex) {
    const RegAllocBlock &block = plan.blocks[blockIndex];
    if (!block.reachable) {
      for (size_t index = block.start; index < block.end; ++index) {
        instOffsets[index] = code_.size();
      }
      continue;
    }
    if (loopHeader[blockIndex]) {
      alignLoopHeader();
    }
    blockOffset[blockIndex] = code_.size();
    bool endsWithBranch = false;
    for (size_t position = 0; position < block.instructions.size(); ++position) {
      const RegAllocInstruction &instruction = block.instructions[position];
      const IrInstruction &ir = fn.instructions[instruction.irIndex];
      instOffsets[instruction.irIndex] = code_.size();
      if (instruction.folded || instruction.fusedIntoBranch) {
        continue;
      }
      switch (ir.op) {
      case IrOpcode::AddI32:
      case IrOpcode::AddI64:
      case IrOpcode::SubI32:
      case IrOpcode::SubI64:
      case IrOpcode::MulI32:
      case IrOpcode::MulI64: {
        uint32_t a = instruction.uses[0];
        uint32_t b = instruction.uses[1];
        const uint32_t d = instruction.defs[0];
        const bool commutes = ir.op != IrOpcode::SubI32 && ir.op != IrOpcode::SubI64;
        // `d = a OP b` with d in b's register: compute b OP a in place when OP commutes, and an
        // immediate goes on the right.
        if (commutes && ((location(d).kind == RegAllocLocationKind::Reg &&
                          location(b).kind == RegAllocLocationKind::Reg &&
                          location(b).reg == location(d).reg) ||
                         location(a).kind == RegAllocLocationKind::Imm)) {
          std::swap(a, b);
        }
        const RegAllocLocation &right = location(b);
        const int avoid = right.kind == RegAllocLocationKind::Reg ? right.reg : -1;
        const uint8_t target = targetReg(d, avoid);
        loadInto(a, target);
        const bool isMul = ir.op == IrOpcode::MulI32 || ir.op == IrOpcode::MulI64;
        const bool isSub = ir.op == IrOpcode::SubI32 || ir.op == IrOpcode::SubI64;
        if (!isMul && right.kind == RegAllocLocationKind::Imm && fitsImm32(right.imm)) {
          if (isSub) {
            emitSubRegImm32(target, static_cast<int32_t>(right.imm));
          } else {
            emitAddRegImm32(target, static_cast<int32_t>(right.imm));
          }
        } else {
          const uint8_t rightReg = valueReg(b, Rcx);
          if (isMul) {
            emitImulRegReg(target, rightReg);
          } else if (isSub) {
            emitSubRegReg(target, rightReg);
          } else {
            emitAddRegReg(target, rightReg);
          }
        }
        storeValue(d, target);
        break;
      }
      case IrOpcode::NegI32:
      case IrOpcode::NegI64: {
        const uint32_t d = instruction.defs[0];
        const uint8_t target = targetReg(d, -1);
        loadInto(instruction.uses[0], target);
        emitNegReg(target);
        storeValue(d, target);
        break;
      }
      case IrOpcode::SextI32: {
        const uint32_t d = instruction.defs[0];
        const uint8_t target = targetReg(d, -1);
        const uint8_t source = valueReg(instruction.uses[0], Rcx);
        emitMovsxdRegReg(target, source);
        storeValue(d, target);
        break;
      }
      case IrOpcode::CmpEqI32:
      case IrOpcode::CmpNeI32:
      case IrOpcode::CmpLtI32:
      case IrOpcode::CmpLeI32:
      case IrOpcode::CmpGtI32:
      case IrOpcode::CmpGeI32:
      case IrOpcode::CmpEqI64:
      case IrOpcode::CmpNeI64:
      case IrOpcode::CmpLtI64:
      case IrOpcode::CmpLeI64:
      case IrOpcode::CmpGtI64:
      case IrOpcode::CmpGeI64:
      case IrOpcode::CmpLtU64:
      case IrOpcode::CmpLeU64:
      case IrOpcode::CmpGtU64:
      case IrOpcode::CmpGeU64: {
        CondCode cc = CondCode::Eq;
        const uint32_t d = instruction.defs[0];
        const uint8_t target = targetReg(d, -1);
        // setcc writes only the low byte, which on x86 merges with (and so waits for) whatever
        // the register held before. Zeroing it first with xor, the dependency-breaking idiom,
        // removes that wait; it has to come before the compare because it sets the flags.
        const auto operandRegister = [&](uint32_t value, uint8_t scratch) -> int {
          const RegAllocLocation &where = location(value);
          if (where.kind == RegAllocLocationKind::Reg) {
            return where.reg;
          }
          if (where.kind == RegAllocLocationKind::Imm && fitsImm32(where.imm) &&
              value == instruction.uses[1]) {
            return -1;
          }
          return scratch;
        };
        const bool zeroFirst =
            static_cast<int>(target) != operandRegister(instruction.uses[0], Rax) &&
            static_cast<int>(target) != operandRegister(instruction.uses[1], Rcx);
        if (zeroFirst) {
          emitXorRegReg(target, target);
        }
        emitCompareFlags(instruction, cc);
        emitSetccReg(target, cc);
        if (!zeroFirst) {
          emitMovzxReg8(target, target);
        }
        storeValue(d, target);
        break;
      }
      case IrOpcode::LoadLocal: {
        // A local some memory access can reach stays in its frame slot.
        const uint32_t d = instruction.defs[0];
        const uint8_t target = targetReg(d, -1);
        emitLoadMem(target, 5, regAllocSlotDisp(static_cast<uint32_t>(ir.imm)));
        storeValue(d, target);
        break;
      }
      case IrOpcode::StoreLocal: {
        const RegAllocLocation &value = location(instruction.uses[0]);
        const int32_t disp = regAllocSlotDisp(static_cast<uint32_t>(ir.imm));
        if (value.kind == RegAllocLocationKind::Imm && fitsImm32(value.imm)) {
          emitStoreImm64Mem(5, disp, value.imm);
        } else {
          emitStoreMem(5, disp, valueReg(instruction.uses[0], Rax));
        }
        break;
      }
      case IrOpcode::LoadStringByte: {
        const uint32_t d = instruction.defs[0];
        const uint8_t target = targetReg(d, -1);
        const uint8_t index = valueReg(instruction.uses[0], Rax);
        const size_t fixup = emitLeaRipPlaceholder(Rcx);
        emitLoadMemByteIndexed(target, Rcx, index);
        hooks.recordStringFixup(fixup, static_cast<uint32_t>(ir.imm));
        storeValue(d, target);
        break;
      }
      case IrOpcode::JumpIfZero: {
        endsWithBranch = true;
        const size_t taken = blockStartingAt(static_cast<size_t>(ir.imm));
        const size_t fallthrough = blockIndex + 1;
        const RegAllocEdge *takenEdge = taken == SIZE_MAX ? nullptr : findEdge(block, taken);
        const RegAllocEdge *fallthroughEdge = findEdge(block, fallthrough);
        if (takenEdge == nullptr) {
          error = "register allocation lost a branch edge";
          return false;
        }
        if (fallthroughEdge == nullptr || taken == fallthrough) {
          // Both outcomes continue at the same place.
          emitEdgeMoves(*takenEdge);
          if (taken != fallthrough) {
            emitEdgeJump(blockIndex, *takenEdge, false, CondCode::Eq);
          }
          break;
        }
        CondCode jumpWhen = CondCode::Eq;
        const RegAllocInstruction *compare =
            position > 0 && block.instructions[position - 1].fusedIntoBranch
                ? &block.instructions[position - 1]
                : nullptr;
        if (compare != nullptr) {
          CondCode cc = CondCode::Eq;
          emitCompareFlags(*compare, cc);
          // JumpIfZero is taken when the comparison is false.
          alignBranchSequence(compareStart_, code_.size() - compareStart_ + 6);
          jumpWhen = invertCond(cc);
        } else {
          const RegAllocLocation &condition = location(instruction.uses[0]);
          if (condition.kind == RegAllocLocationKind::Imm) {
            if (condition.imm == 0) {
              emitEdgeMoves(*takenEdge);
              emitEdgeJump(blockIndex, *takenEdge, false, CondCode::Eq);
            } else {
              emitEdgeMoves(*fallthroughEdge);
            }
            break;
          }
          const uint8_t reg = valueReg(instruction.uses[0], Rax);
          const size_t testStart = code_.size();
          emitTestRegReg(reg);
          alignBranchSequence(testStart, code_.size() - testStart + 6);
          jumpWhen = CondCode::Eq;
        }
        emitEdgeJump(blockIndex, *takenEdge, true, jumpWhen);
        emitEdgeMoves(*fallthroughEdge);
        break;
      }
      case IrOpcode::Jump: {
        endsWithBranch = true;
        const size_t target = blockStartingAt(static_cast<size_t>(ir.imm));
        const RegAllocEdge *edge = target == SIZE_MAX ? nullptr : findEdge(block, target);
        if (edge == nullptr) {
          error = "register allocation lost a jump edge";
          return false;
        }
        emitEdgeMoves(*edge);
        if (target != blockIndex + 1) {
          emitEdgeJump(blockIndex, *edge, false, CondCode::Eq);
        }
        break;
      }
      default: {
        // Template: operands onto the operand stack, results back off it, and every register
        // that must survive the template saved around it.
        const bool returns = ir.op == IrOpcode::ReturnVoid || ir.op == IrOpcode::ReturnI32 ||
                             ir.op == IrOpcode::ReturnI64 || ir.op == IrOpcode::ReturnF32 ||
                             ir.op == IrOpcode::ReturnF64;
        if (returns) {
          endsWithBranch = true;
        }
        std::vector<uint8_t> saved;
        if (!returns) {
          saved = regAllocRegistersLiveAcross(plan, blockIndex, instruction.irIndex);
        }
        for (const uint8_t reg : saved) {
          emitStoreMem(5, regAllocSlotDisp(saveBaseLocal + reg), reg);
        }
        for (const uint32_t use : instruction.uses) {
          emitSpillReg(valueReg(use, Rax));
        }
        if (!hooks.emitTemplate(instruction.irIndex)) {
          return false;
        }
        for (size_t k = instruction.defs.size(); k-- > 0;) {
          const RegAllocLocation &where = location(instruction.defs[k]);
          if (where.kind == RegAllocLocationKind::Reg) {
            emitReloadReg(where.reg);
          } else {
            emitReloadReg(Rax);
            storeValue(instruction.defs[k], Rax);
          }
        }
        for (const uint8_t reg : saved) {
          emitLoadMem(reg, 5, regAllocSlotDisp(saveBaseLocal + reg));
        }
        break;
      }
      }
    }
    if (!endsWithBranch) {
      // The block falls into the next one, which starts at a branch target.
      const RegAllocEdge *edge = findEdge(block, blockIndex + 1);
      if (edge != nullptr) {
        emitEdgeMoves(*edge);
      }
    }
  }
  instOffsets[fn.instructions.size()] = code_.size();

  for (size_t i = 0; i < trampolines.size(); ++i) {
    const Trampoline &trampoline = trampolines[i];
    const RegAllocEdge &edge = plan.blocks[trampoline.block].edges[trampoline.edge];
    const size_t start = code_.size();
    for (JumpFixup &fixup : fixups) {
      if (fixup.trampoline == i) {
        if (fixup.conditional) {
          patchJumpIfZero(fixup.codeIndex,
                          static_cast<int32_t>(static_cast<int64_t>(start) -
                                               static_cast<int64_t>(fixup.codeIndex)));
        } else {
          patchJump(fixup.codeIndex,
                    static_cast<int32_t>(static_cast<int64_t>(start) -
                                         static_cast<int64_t>(fixup.codeIndex)));
        }
        fixup.trampoline = SIZE_MAX - 1; // patched
      }
    }
    emitEdgeMoves(edge);
    JumpFixup back;
    back.targetBlock = edge.successorBlock;
    back.codeIndex = emitJumpPlaceholder();
    fixups.push_back(back);
  }
  for (const JumpFixup &fixup : fixups) {
    if (fixup.trampoline != SIZE_MAX) {
      continue;
    }
    const size_t target = blockOffset[fixup.targetBlock];
    if (target == SIZE_MAX) {
      error = "register allocation jumps to an unreachable block";
      return false;
    }
    const int32_t delta =
        static_cast<int32_t>(static_cast<int64_t>(target) - static_cast<int64_t>(fixup.codeIndex));
    if (fixup.conditional) {
      patchJumpIfZero(fixup.codeIndex, delta);
    } else {
      patchJump(fixup.codeIndex, delta);
    }
  }

  deferOperands_ = savedDefer;
  valueStackCacheEnabled_ = savedCache;
  return true;
}
