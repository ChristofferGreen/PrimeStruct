#pragma once

// Register-allocated function bodies for the arm64 emitter (NativeEmitterRegAlloc.h has the plan;
// docs/OptimizingBackendsPlan.md section 12). Integer arithmetic, comparisons (fused into the branch
// or as `cset`), branches, constants as immediates, frame locals, byte and word memory accesses and
// string bytes are emitted on the planned registers; every other opcode runs its ordinary template on
// the memory value stack (x28): its operands are pushed there first and its results popped into their
// locations afterwards, with the template-clobbered registers live across it saved in their frame slots.
//
// Only functions without float values or local addresses come here (TODO-5773 and TODO-5775 take
// the rest). A call saves every register live across it in its frame slot and restores it after
// (callees, allocated or not, may change any of x0-x25); the result arrives in x0. A callee whose
// callers are all register-allocated takes its first eight arguments in x0-x7, the
// rest on the value stack as template callees take them all.
//
// Registers: x0 and x1 are the scratch registers for spilled values, immediates, parallel-move cycles
// (x0) and slot-to-slot copies (x1); string addresses are built in x1 (patchAdr writes x1). x17 builds
// frame addresses past the 32 KB reach of `ldr`/`str`. x16 is the syscall number and breaks the
// cycles of argument moves (whose registers include x0), x18 the platform
// register, x26 the template operand cache (off here), x27 the frame, x28 the value stack, x29/x30
// the frame record and link register (saved in the frame's link local at entry).

// Allocatable registers, the hottest classes first: x21-x25 are kept by every template, host call
// (AAPCS64 callee-saved) and heap routine, so values there are never saved around a template; x21 holds
// the caller's frame only in the prologue, before it is stored. x19 and x20 hold argc and argv for the
// whole program while some function reads them (the WithArgRegs pool when none does). x3-x15 are
// template scratch registers, saved around templates while live.
inline const std::vector<uint8_t> Arm64RegAllocPool = {
    21, 22, 23, 24, 25, 9, 10, 11, 12, 13, 14, 15, 3, 4, 5, 6, 7, 8};
inline const std::vector<uint8_t> Arm64RegAllocPoolWithArgRegs = {
    21, 22, 23, 24, 25, 19, 20, 9, 10, 11, 12, 13, 14, 15, 3, 4, 5, 6, 7, 8};

// Arguments a register-argument callee takes in x0-x7.
inline constexpr size_t Arm64RegAllocArgumentRegisters = 8;

// Whether a template (or the C function or heap routine it calls) may change `reg`.
inline bool arm64TemplateClobbers(uint8_t reg) {
  return reg < 19;
}

// Why the arm64 register allocator leaves `function` on templates; empty when it takes it.
inline std::string arm64RegisterAllocationBlocker(const IrFunction &function) {
  for (const IrInstruction &instruction : function.instructions) {
    switch (instruction.op) {
    case IrOpcode::AddressOfLocal:
      return "takes a local address (TODO-5775)";
    case IrOpcode::PushF32:
    case IrOpcode::PushF64:
    case IrOpcode::AddF32:
    case IrOpcode::SubF32:
    case IrOpcode::MulF32:
    case IrOpcode::DivF32:
    case IrOpcode::NegF32:
    case IrOpcode::AddF64:
    case IrOpcode::SubF64:
    case IrOpcode::MulF64:
    case IrOpcode::DivF64:
    case IrOpcode::NegF64:
    case IrOpcode::CmpEqF32:
    case IrOpcode::CmpNeF32:
    case IrOpcode::CmpLtF32:
    case IrOpcode::CmpLeF32:
    case IrOpcode::CmpGtF32:
    case IrOpcode::CmpGeF32:
    case IrOpcode::CmpEqF64:
    case IrOpcode::CmpNeF64:
    case IrOpcode::CmpLtF64:
    case IrOpcode::CmpLeF64:
    case IrOpcode::CmpGtF64:
    case IrOpcode::CmpGeF64:
    case IrOpcode::ConvertI32ToF32:
    case IrOpcode::ConvertI32ToF64:
    case IrOpcode::ConvertI64ToF32:
    case IrOpcode::ConvertI64ToF64:
    case IrOpcode::ConvertU64ToF32:
    case IrOpcode::ConvertU64ToF64:
    case IrOpcode::ConvertF32ToI32:
    case IrOpcode::ConvertF32ToI64:
    case IrOpcode::ConvertF32ToU64:
    case IrOpcode::ConvertF64ToI32:
    case IrOpcode::ConvertF64ToI64:
    case IrOpcode::ConvertF64ToU64:
    case IrOpcode::ConvertF32ToF64:
    case IrOpcode::ConvertF64ToF32:
    case IrOpcode::ReturnF32:
    case IrOpcode::ReturnF64:
      return "float values (TODO-5773)";
    default:
      break;
    }
  }
  return {};
}

inline bool Arm64Emitter::emitRegisterAllocatedFunction(const IrFunction &fn,
                                                        const RegAllocFunctionPlan &plan,
                                                        uint32_t spillBaseLocal,
                                                        uint32_t frameLocalIndex,
                                                        uint32_t linkLocalIndex,
                                                        std::vector<size_t> &instOffsets,
                                                        const RegAllocHooks &hooks,
                                                        std::string &error) {
  constexpr uint8_t X0 = 0;
  constexpr uint8_t X1 = 1;
  constexpr uint8_t X16 = 16;
  constexpr uint8_t X17 = 17;
  constexpr uint8_t Zr = 31; // xzr where an encoding reads register 31 as zero
  constexpr uint8_t Frame = 27;
  // The templates must see a plain memory value stack.
  flushValueStackCache();
  const bool savedCache = valueStackCacheEnabled_;
  valueStackCacheEnabled_ = false;
  hasValueStackCache_ = false;

  const uint32_t saveBaseLocal = spillBaseLocal + plan.spillSlotCount;
  const auto location = [&](uint32_t value) -> const RegAllocLocation & {
    return plan.locations[value];
  };
  const auto movReg = [&](uint8_t rd, uint8_t rm) {
    if (rd != rm) {
      emit(0xAA0003E0u | (static_cast<uint32_t>(rm) << 16) | rd); // ORR rd, xzr, rm
    }
  };
  // Frame slot `local` (x27 + 16 local + 8), through x17 past the reach of the scaled offset.
  const auto frameLoad = [&](uint8_t reg, uint32_t local) {
    const uint64_t offset = localOffset(local);
    if (offset <= MaxLdrStrOffsetBytes) {
      emit(encodeLdrRegBase(reg, Frame, static_cast<uint16_t>(offset)));
      return;
    }
    emitMovImm64(X17, offset);
    emit(encodeAddReg(X17, Frame, X17));
    emit(encodeLdrRegBase(reg, X17, 0));
  };
  const auto frameStore = [&](uint8_t reg, uint32_t local) {
    const uint64_t offset = localOffset(local);
    if (offset <= MaxLdrStrOffsetBytes) {
      emit(encodeStrRegBase(reg, Frame, static_cast<uint16_t>(offset)));
      return;
    }
    emitMovImm64(X17, offset);
    emit(encodeAddReg(X17, Frame, X17));
    emit(encodeStrRegBase(reg, X17, 0));
  };
  // The register holding `value`, loading it into `scratch` when it is spilled or a constant.
  const auto valueReg = [&](uint32_t value, uint8_t scratch) -> uint8_t {
    const RegAllocLocation &where = location(value);
    switch (where.kind) {
    case RegAllocLocationKind::Reg:
      return where.reg;
    case RegAllocLocationKind::Slot:
      frameLoad(scratch, spillBaseLocal + where.slot);
      return scratch;
    case RegAllocLocationKind::Imm:
      emitMovImm64(scratch, where.imm);
      return scratch;
    case RegAllocLocationKind::Xmm:
    case RegAllocLocationKind::None:
      break;
    }
    emitMovImm64(scratch, 0);
    return scratch;
  };
  const auto storeValue = [&](uint32_t value, uint8_t reg) {
    const RegAllocLocation &where = location(value);
    if (where.kind == RegAllocLocationKind::Reg) {
      movReg(where.reg, reg);
    } else if (where.kind == RegAllocLocationKind::Slot) {
      frameStore(reg, spillBaseLocal + where.slot);
    }
  };
  // Where to compute `value`: its own register, otherwise x0.
  const auto targetReg = [&](uint32_t value) -> uint8_t {
    const RegAllocLocation &where = location(value);
    return where.kind == RegAllocLocationKind::Reg ? where.reg : X0;
  };

  // Parallel copies on an edge: every destination receives its source's value from before the
  // copies. x0 breaks cycles; x1 carries slot-to-slot and constant-to-slot copies.
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
    if (destination.kind == RegAllocLocationKind::Reg) {
      if (source.kind == RegAllocLocationKind::Reg) {
        movReg(destination.reg, source.reg);
      } else if (source.kind == RegAllocLocationKind::Imm) {
        emitMovImm64(destination.reg, source.imm);
      } else if (source.kind == RegAllocLocationKind::Slot) {
        frameLoad(destination.reg, spillBaseLocal + source.slot);
      }
      return;
    }
    if (destination.kind != RegAllocLocationKind::Slot) {
      return;
    }
    uint8_t reg = X1;
    if (source.kind == RegAllocLocationKind::Reg) {
      reg = source.reg;
    } else if (source.kind == RegAllocLocationKind::Imm) {
      if (source.imm == 0) {
        reg = Zr;
      } else {
        emitMovImm64(X1, source.imm);
      }
    } else if (source.kind == RegAllocLocationKind::Slot) {
      frameLoad(X1, spillBaseLocal + source.slot);
    } else {
      return;
    }
    frameStore(reg, spillBaseLocal + destination.slot);
  };
  // Runs `moves` as parallel copies, breaking cycles through `park` (a register no move names).
  const auto emitParallelMoves = [&](std::vector<PendingMove> moves, uint8_t park) {
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
      // `park` and let its readers take it from there.
      const RegAllocLocation parked = moves.front().destination;
      RegAllocLocation parking;
      parking.kind = RegAllocLocationKind::Reg;
      parking.reg = park;
      emitCopy(parked, parking);
      for (PendingMove &move : moves) {
        if (sameLocation(move.source, parked)) {
          move.source = parking;
        }
      }
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
    emitParallelMoves(std::move(moves), X0);
  };
  const auto registerLocation = [](uint8_t reg) {
    RegAllocLocation where;
    where.kind = RegAllocLocationKind::Reg;
    where.reg = reg;
    return where;
  };

  // Branches. Targets are blocks; a conditional jump into a block that needs edge copies goes
  // through a trampoline emitted after the body.
  enum class BranchKind : uint8_t { Always, Cond, Cbz };
  struct JumpFixup {
    size_t codeIndex = 0;
    size_t targetBlock = 0;
    size_t trampoline = SIZE_MAX;
    BranchKind kind = BranchKind::Always;
    CondCode cond = CondCode::Eq;
    uint8_t reg = 0;
  };
  struct Trampoline {
    size_t block = 0;
    size_t edge = 0;
  };
  std::vector<JumpFixup> fixups;
  std::vector<Trampoline> trampolines;
  std::vector<size_t> blockOffset(plan.blocks.size(), SIZE_MAX);
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
  // A branch placeholder: `b`, `b.cond` or `cbz reg` in one word, or with far branches a
  // two-word `b.cond` over a `b` (a cbz becomes `cmp reg, #0` and that pair).
  const auto emitBranch = [&](JumpFixup &fixup) {
    if (fixup.kind == BranchKind::Cbz && hooks.farBranches) {
      emitCompareRegZero(fixup.reg);
      fixup.kind = BranchKind::Cond;
      fixup.cond = CondCode::Eq;
    }
    fixup.codeIndex = currentWordIndex();
    switch (fixup.kind) {
    case BranchKind::Always:
      emit(encodeB(0));
      break;
    case BranchKind::Cond:
      emit(encodeBCond(0, static_cast<uint8_t>(fixup.cond)));
      if (hooks.farBranches) {
        emit(encodeB(1));
      }
      break;
    case BranchKind::Cbz:
      emit(encodeCbz(fixup.reg, 0));
      break;
    }
  };
  bool outOfRange = false;
  const auto patchBranch = [&](const JumpFixup &fixup, size_t target) {
    const int64_t delta = static_cast<int64_t>(target) - static_cast<int64_t>(fixup.codeIndex);
    constexpr int64_t MinCond = -(int64_t{1} << 18);
    constexpr int64_t MaxCond = (int64_t{1} << 18) - 1;
    switch (fixup.kind) {
    case BranchKind::Always:
      patchWord(fixup.codeIndex, encodeB(static_cast<int32_t>(delta)));
      break;
    case BranchKind::Cond:
      if (hooks.farBranches) {
        patchCondBranch(fixup.codeIndex, static_cast<int32_t>(delta), fixup.cond);
      } else if (delta < MinCond || delta > MaxCond) {
        outOfRange = true;
      } else {
        patchWord(fixup.codeIndex,
                  encodeBCond(static_cast<int32_t>(delta), static_cast<uint8_t>(fixup.cond)));
      }
      break;
    case BranchKind::Cbz:
      if (delta < MinCond || delta > MaxCond) {
        outOfRange = true;
      } else {
        patchWord(fixup.codeIndex, encodeCbz(fixup.reg, static_cast<int32_t>(delta)));
      }
      break;
    }
  };
  // A jump along `edge` of `blockIndex`. A conditional jump runs the edge's copies in a
  // trampoline; an unconditional one is emitted after them.
  const auto emitEdgeJump = [&](size_t blockIndex, const RegAllocEdge &edge, JumpFixup fixup) {
    fixup.targetBlock = edge.successorBlock;
    if (fixup.kind != BranchKind::Always) {
      for (const RegAllocMove &move : edge.moves) {
        const RegAllocLocation &destination = location(move.destination);
        if ((destination.kind == RegAllocLocationKind::Reg ||
             destination.kind == RegAllocLocationKind::Slot) &&
            !sameLocation(location(move.source), destination)) {
          fixup.trampoline = trampolines.size();
          trampolines.push_back(
              {blockIndex, static_cast<size_t>(&edge - plan.blocks[blockIndex].edges.data())});
          break;
        }
      }
    }
    emitBranch(fixup);
    fixups.push_back(fixup);
  };

  // The condition under which an integer comparison is true.
  const auto compareCondition = [](IrOpcode op, CondCode &cond) {
    switch (op) {
    case IrOpcode::CmpEqI32:
    case IrOpcode::CmpEqI64:
      cond = CondCode::Eq;
      return true;
    case IrOpcode::CmpNeI32:
    case IrOpcode::CmpNeI64:
      cond = CondCode::Ne;
      return true;
    case IrOpcode::CmpLtI32:
    case IrOpcode::CmpLtI64:
      cond = CondCode::Lt;
      return true;
    case IrOpcode::CmpLeI32:
    case IrOpcode::CmpLeI64:
      cond = CondCode::Le;
      return true;
    case IrOpcode::CmpGtI32:
    case IrOpcode::CmpGtI64:
      cond = CondCode::Gt;
      return true;
    case IrOpcode::CmpGeI32:
    case IrOpcode::CmpGeI64:
      cond = CondCode::Ge;
      return true;
    case IrOpcode::CmpLtU64:
      cond = CondCode::Lo;
      return true;
    case IrOpcode::CmpLeU64:
      cond = CondCode::Ls;
      return true;
    case IrOpcode::CmpGtU64:
      cond = CondCode::Hi;
      return true;
    case IrOpcode::CmpGeU64:
      cond = CondCode::Hs;
      return true;
    default:
      return false;
    }
  };
  // Sets the flags from `compare`'s operands (64-bit, as the templates compare); returns the
  // condition under which it is true. A constant right operand is a `cmp`/`cmn` immediate: x - c
  // and x + (-c) set the same flags for every c but 0 and the most negative value.
  const auto emitCompareFlags = [&](const RegAllocInstruction &compare, CondCode &cond) {
    compareCondition(fn.instructions[compare.irIndex].op, cond);
    const uint8_t left = valueReg(compare.uses[0], X0);
    const RegAllocLocation &right = location(compare.uses[1]);
    if (right.kind == RegAllocLocationKind::Imm) {
      uint16_t imm = 0;
      bool shifted = false;
      if (addSubImmediate(right.imm, imm, shifted)) {
        emit(encodeAddSubImm(SubsImmBase, Zr, left, imm, shifted));
        return;
      }
      if (right.imm != (uint64_t{1} << 63) && addSubImmediate(0 - right.imm, imm, shifted)) {
        emit(encodeAddSubImm(AddsImmBase, Zr, left, imm, shifted));
        return;
      }
    }
    emit(encodeSubsReg(Zr, left, valueReg(compare.uses[1], X1)));
  };

  // Runs `body` on the memory value stack: the operands pushed there first, the results popped
  // into their locations afterwards, and (with `saveLive`) every template-clobbered register that
  // must survive it saved around it.
  const auto onValueStack = [&](size_t blockIndex,
                                const RegAllocInstruction &instruction,
                                bool saveLive,
                                const auto &body) -> bool {
    std::vector<uint8_t> saved;
    if (saveLive) {
      for (const uint8_t reg : regAllocRegistersLiveAcross(plan, blockIndex, instruction.irIndex)) {
        if (arm64TemplateClobbers(reg)) {
          saved.push_back(reg);
        }
      }
    }
    for (const uint8_t reg : saved) {
      frameStore(reg, saveBaseLocal + reg);
    }
    for (const uint32_t use : instruction.uses) {
      emitSpillReg(valueReg(use, X0));
    }
    if (!body()) {
      return false;
    }
    for (size_t k = instruction.defs.size(); k-- > 0;) {
      const RegAllocLocation &where = location(instruction.defs[k]);
      if (where.kind == RegAllocLocationKind::Reg) {
        emitReloadReg(where.reg);
      } else {
        emitReloadReg(X0);
        storeValue(instruction.defs[k], X0);
      }
    }
    for (const uint8_t reg : saved) {
      frameLoad(reg, saveBaseLocal + reg);
    }
    return true;
  };
  const auto emitReturnSequence = [&]() {
    emitLoadLocalToReg(30, linkLocalIndex);
    emitLoadLocalToReg(27, frameLocalIndex);
    if (frameSize_ > 0) {
      emitAdjustSp(frameSize_, true);
    }
    emit(encodeRet());
  };

  // Entry: the register arguments go home first (x16 breaks cycles), then the other parameters
  // come off the value stack, top (the last) first; locals read before any store start at zero.
  if (!plan.blocks.empty() && plan.blocks[0].reachable) {
    const RegAllocBlock &first = plan.blocks[0];
    const size_t inRegisters = hooks.argumentsInRegisters ? std::min(Arm64RegAllocArgumentRegisters,
                                                                     first.entryValues.size())
                                                          : 0;
    std::vector<PendingMove> arguments;
    for (size_t k = 0; k < inRegisters; ++k) {
      const RegAllocLocation &destination = location(first.entryValues[k]);
      if ((destination.kind == RegAllocLocationKind::Reg ||
           destination.kind == RegAllocLocationKind::Slot) &&
          !sameLocation(registerLocation(static_cast<uint8_t>(k)), destination)) {
        arguments.push_back({registerLocation(static_cast<uint8_t>(k)), destination});
      }
    }
    emitParallelMoves(std::move(arguments), X16);
    for (size_t k = first.entryValues.size(); k-- > inRegisters;) {
      const RegAllocLocation &where = location(first.entryValues[k]);
      if (where.kind == RegAllocLocationKind::Reg) {
        emitReloadReg(where.reg);
      } else {
        emitReloadReg(X0);
        storeValue(first.entryValues[k], X0);
      }
    }
    for (const uint32_t value : first.zeroValues) {
      const RegAllocLocation &where = location(value);
      if (where.kind == RegAllocLocationKind::Reg) {
        emitMovImm64(where.reg, 0);
      } else if (where.kind == RegAllocLocationKind::Slot) {
        frameStore(Zr, spillBaseLocal + where.slot);
      }
    }
  }

  for (size_t blockIndex = 0; blockIndex < plan.blocks.size(); ++blockIndex) {
    const RegAllocBlock &block = plan.blocks[blockIndex];
    if (!block.reachable) {
      for (size_t index = block.start; index < block.end; ++index) {
        instOffsets[index] = currentWordIndex();
      }
      continue;
    }
    blockOffset[blockIndex] = currentWordIndex();
    bool endsWithBranch = false;
    for (size_t position = 0; position < block.instructions.size(); ++position) {
      const RegAllocInstruction &instruction = block.instructions[position];
      const IrInstruction &ir = fn.instructions[instruction.irIndex];
      instOffsets[instruction.irIndex] = currentWordIndex();
      if (instruction.folded || instruction.fusedIntoBranch) {
        continue;
      }
      switch (ir.op) {
      case IrOpcode::AddI32:
      case IrOpcode::AddI64:
      case IrOpcode::SubI32:
      case IrOpcode::SubI64:
      case IrOpcode::MulI32:
      case IrOpcode::MulI64:
      case IrOpcode::DivI32:
      case IrOpcode::DivI64:
      case IrOpcode::DivU64: {
        // 64-bit, as the templates compute; i32 results are sign-extended by a SextI32 after.
        uint32_t a = instruction.uses[0];
        uint32_t b = instruction.uses[1];
        const uint32_t d = instruction.defs[0];
        const bool isAdd = ir.op == IrOpcode::AddI32 || ir.op == IrOpcode::AddI64;
        const bool isSub = ir.op == IrOpcode::SubI32 || ir.op == IrOpcode::SubI64;
        const bool isMul = ir.op == IrOpcode::MulI32 || ir.op == IrOpcode::MulI64;
        if ((isAdd || isMul) && location(a).kind == RegAllocLocationKind::Imm &&
            location(b).kind != RegAllocLocationKind::Imm) {
          std::swap(a, b);
        }
        const uint8_t target = targetReg(d);
        if ((isAdd || isSub) && location(b).kind == RegAllocLocationKind::Imm) {
          // add/sub of a 12-bit immediate (or `#imm, lsl #12`, or the negated constant through
          // the other instruction); adding or subtracting 0 is a copy.
          const uint64_t constant = location(b).imm;
          uint16_t imm = 0;
          bool shifted = false;
          uint32_t base = 0;
          if (constant != 0) {
            if (addSubImmediate(constant, imm, shifted)) {
              base = isAdd ? AddImmBase : SubImmBase;
            } else if (addSubImmediate(0 - constant, imm, shifted)) {
              base = isAdd ? SubImmBase : AddImmBase;
            }
          }
          if (constant == 0 || base != 0) {
            const uint8_t left = valueReg(a, X0);
            if (constant == 0) {
              movReg(target, left);
            } else {
              emit(encodeAddSubImm(base, target, left, imm, shifted));
            }
            storeValue(d, target);
            break;
          }
        }
        const uint8_t left = valueReg(a, X0);
        const uint8_t right = valueReg(b, X1);
        if (isAdd) {
          emit(encodeAddReg(target, left, right));
        } else if (isSub) {
          emit(encodeSubReg(target, left, right));
        } else if (isMul) {
          emit(encodeMulReg(target, left, right));
        } else if (ir.op == IrOpcode::DivU64) {
          emit(encodeUdivReg(target, left, right));
        } else {
          emit(encodeSdivReg(target, left, right));
        }
        storeValue(d, target);
        break;
      }
      case IrOpcode::NegI32:
      case IrOpcode::NegI64: {
        const uint32_t d = instruction.defs[0];
        const uint8_t target = targetReg(d);
        emit(encodeSubReg(target, Zr, valueReg(instruction.uses[0], X0)));
        storeValue(d, target);
        break;
      }
      case IrOpcode::SextI32: {
        const uint32_t d = instruction.defs[0];
        const uint8_t target = targetReg(d);
        const uint8_t source = valueReg(instruction.uses[0], X0);
        emit(0x93407C00u | (static_cast<uint32_t>(source) << 5) | target); // SXTW
        storeValue(d, target);
        break;
      }
      case IrOpcode::ZextU8: {
        const uint32_t d = instruction.defs[0];
        const uint8_t target = targetReg(d);
        const uint8_t source = valueReg(instruction.uses[0], X0);
        emit(0x53001C00u | (static_cast<uint32_t>(source) << 5) | target); // UXTB
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
        CondCode cond = CondCode::Eq;
        emitCompareFlags(instruction, cond);
        const uint32_t d = instruction.defs[0];
        const uint8_t target = targetReg(d);
        // CSET target, cond (CSINC target, xzr, xzr, !cond).
        emit(0x9A9F07E0u | (static_cast<uint32_t>(invertCond(cond)) << 12) | target);
        storeValue(d, target);
        break;
      }
      case IrOpcode::LoadIndirect:
      case IrOpcode::LoadU8: {
        const uint32_t d = instruction.defs[0];
        const uint8_t target = targetReg(d);
        const uint8_t address = valueReg(instruction.uses[0], X1);
        emit(ir.op == IrOpcode::LoadU8 ? encodeLdrbRegBase(target, address, 0)
                                       : encodeLdrRegBase(target, address, 0));
        storeValue(d, target);
        break;
      }
      case IrOpcode::StoreIndirect:
      case IrOpcode::StoreU8: {
        // Stores the value (top) at the address (below it) and leaves the value (the stored byte
        // for StoreU8) as the result.
        const uint8_t value = valueReg(instruction.uses[1], X0);
        const uint8_t address = valueReg(instruction.uses[0], X1);
        if (ir.op == IrOpcode::StoreU8) {
          emit(encodeStrbRegBase(value, address, 0));
          if (!instruction.defs.empty()) {
            const uint8_t target = targetReg(instruction.defs[0]);
            emit(0x53001C00u | (static_cast<uint32_t>(value) << 5) | target); // UXTB
            storeValue(instruction.defs[0], target);
          }
        } else {
          emit(encodeStrRegBase(value, address, 0));
          if (!instruction.defs.empty()) {
            storeValue(instruction.defs[0], value);
          }
        }
        break;
      }
      case IrOpcode::LoadLocal: {
        // A local some memory access can reach stays in its frame slot.
        const uint32_t d = instruction.defs[0];
        const uint8_t target = targetReg(d);
        frameLoad(target, static_cast<uint32_t>(ir.imm));
        storeValue(d, target);
        break;
      }
      case IrOpcode::StoreLocal: {
        const RegAllocLocation &value = location(instruction.uses[0]);
        const uint8_t source = value.kind == RegAllocLocationKind::Imm && value.imm == 0
                                   ? Zr
                                   : valueReg(instruction.uses[0], X0);
        frameStore(source, static_cast<uint32_t>(ir.imm));
        break;
      }
      case IrOpcode::LoadStringByte: {
        // ldrb target, [string address, index]; the address is built in x1 for patchAdr.
        const uint32_t d = instruction.defs[0];
        const uint8_t target = targetReg(d);
        const uint8_t index = valueReg(instruction.uses[0], X0);
        const size_t fixup = emitAdrPlaceholder(X1);
        hooks.recordStringFixup(fixup, static_cast<uint32_t>(ir.imm));
        emit(0x38606800u | (static_cast<uint32_t>(index) << 16) | (static_cast<uint32_t>(X1) << 5) |
             target); // LDRB target, [x1, index]
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
          valueStackCacheEnabled_ = savedCache;
          return false;
        }
        if (fallthroughEdge == nullptr || taken == fallthrough) {
          // Both outcomes continue at the same place.
          emitEdgeMoves(*takenEdge);
          if (taken != fallthrough) {
            emitEdgeJump(blockIndex, *takenEdge, JumpFixup{});
          }
          break;
        }
        JumpFixup branch;
        const RegAllocInstruction *compare =
            position > 0 && block.instructions[position - 1].fusedIntoBranch
                ? &block.instructions[position - 1]
                : nullptr;
        if (compare != nullptr) {
          CondCode cond = CondCode::Eq;
          emitCompareFlags(*compare, cond);
          // JumpIfZero is taken when the comparison is false.
          branch.kind = BranchKind::Cond;
          branch.cond = invertCond(cond);
        } else {
          const RegAllocLocation &condition = location(instruction.uses[0]);
          if (condition.kind == RegAllocLocationKind::Imm) {
            if (condition.imm == 0) {
              emitEdgeMoves(*takenEdge);
              emitEdgeJump(blockIndex, *takenEdge, JumpFixup{});
            } else {
              emitEdgeMoves(*fallthroughEdge);
            }
            break;
          }
          branch.kind = BranchKind::Cbz;
          branch.reg = valueReg(instruction.uses[0], X0);
        }
        emitEdgeJump(blockIndex, *takenEdge, branch);
        emitEdgeMoves(*fallthroughEdge);
        break;
      }
      case IrOpcode::Jump: {
        endsWithBranch = true;
        const size_t target = blockStartingAt(static_cast<size_t>(ir.imm));
        const RegAllocEdge *edge = target == SIZE_MAX ? nullptr : findEdge(block, target);
        if (edge == nullptr) {
          error = "register allocation lost a jump edge";
          valueStackCacheEnabled_ = savedCache;
          return false;
        }
        emitEdgeMoves(*edge);
        if (target != blockIndex + 1) {
          emitEdgeJump(blockIndex, *edge, JumpFixup{});
        }
        break;
      }
      case IrOpcode::Call:
      case IrOpcode::CallVoid: {
        // Every register still needed afterwards is saved around the call: any callee may change
        // x0-x25. Arguments past the register ones go on the value stack first (through x0),
        // then the first eight move into x0-x7 as parallel copies (x16 breaks cycles).
        const std::vector<uint8_t> saved =
            regAllocRegistersLiveAcross(plan, blockIndex, instruction.irIndex);
        for (const uint8_t reg : saved) {
          frameStore(reg, saveBaseLocal + reg);
        }
        const size_t inRegisters =
            hooks.calleeTakesRegisterArguments && hooks.calleeTakesRegisterArguments(ir.imm)
                ? std::min(Arm64RegAllocArgumentRegisters, instruction.uses.size())
                : 0;
        for (size_t k = inRegisters; k < instruction.uses.size(); ++k) {
          emitSpillReg(valueReg(instruction.uses[k], X0));
        }
        std::vector<PendingMove> arguments;
        for (size_t k = 0; k < inRegisters; ++k) {
          const RegAllocLocation &source = location(instruction.uses[k]);
          const RegAllocLocation destination = registerLocation(static_cast<uint8_t>(k));
          if (!sameLocation(source, destination)) {
            arguments.push_back({source, destination});
          }
        }
        emitParallelMoves(std::move(arguments), X16);
        if (!hooks.recordCallFixup || !hooks.recordCallFixup(emitCallPlaceholder(), ir.imm)) {
          if (error.empty()) {
            error = "native backend detected invalid call target";
          }
          valueStackCacheEnabled_ = savedCache;
          return false;
        }
        if (ir.op == IrOpcode::Call && !instruction.defs.empty()) {
          storeValue(instruction.defs[0], X0);
        }
        for (const uint8_t reg : saved) {
          frameLoad(reg, saveBaseLocal + reg);
        }
        break;
      }
      case IrOpcode::Trap:
        endsWithBranch = true;
        emitTrap(static_cast<int32_t>(ir.imm));
        break;
      case IrOpcode::ReturnI32:
      case IrOpcode::ReturnI64: {
        // The result in x0 as the templates return it (an i32 already sign-extended).
        endsWithBranch = true;
        movReg(X0, valueReg(instruction.uses[0], X0));
        emitReturnSequence();
        break;
      }
      case IrOpcode::ReturnVoid:
        endsWithBranch = true;
        emitMovImm64(X0, 0);
        emitReturnSequence();
        break;
      default:
        // Template: operands onto the value stack, results back off it, and every
        // template-clobbered register that must survive the template saved around it.
        if (!onValueStack(blockIndex, instruction, true, [&] {
              return hooks.emitTemplate(instruction.irIndex);
            })) {
          valueStackCacheEnabled_ = savedCache;
          return false;
        }
        break;
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
  instOffsets[fn.instructions.size()] = currentWordIndex();

  for (size_t i = 0; i < trampolines.size(); ++i) {
    const Trampoline &trampoline = trampolines[i];
    const RegAllocEdge &edge = plan.blocks[trampoline.block].edges[trampoline.edge];
    const size_t start = currentWordIndex();
    for (JumpFixup &fixup : fixups) {
      if (fixup.trampoline == i) {
        patchBranch(fixup, start);
        fixup.trampoline = SIZE_MAX - 1; // patched
      }
    }
    emitEdgeMoves(edge);
    JumpFixup back;
    back.targetBlock = edge.successorBlock;
    emitBranch(back);
    fixups.push_back(back);
  }
  valueStackCacheEnabled_ = savedCache;
  for (const JumpFixup &fixup : fixups) {
    if (fixup.trampoline != SIZE_MAX) {
      continue;
    }
    const size_t target = blockOffset[fixup.targetBlock];
    if (target == SIZE_MAX) {
      error = "register allocation jumps to an unreachable block";
      return false;
    }
    patchBranch(fixup, target);
  }
  if (outOfRange) {
    if (hooks.branchOutOfRange != nullptr) {
      *hooks.branchOutOfRange = true;
    }
    error = "register-allocated function too large for one-word conditional branches";
    return false;
  }
  return true;
}
