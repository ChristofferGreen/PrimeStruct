// Register-allocated function bodies for the x86_64 emitter (see NativeEmitterRegAlloc.h for the
// plan). Integer arithmetic, comparisons, branches, frame locals and string bytes are emitted on
// the planned registers; every other opcode runs its ordinary template, which works on the
// memory operand stack: its operands are pushed there first and its results popped into their
// registers afterwards, with the registers live across it saved in their frame slots.
//
// rax, rcx and rdx are never allocated: they are the scratch registers for spilled values,
// immediates, parallel-move cycles and the templates themselves. Values only float operations
// touch live in xmm2-xmm15; xmm0 and xmm1 are the float scratch registers.

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
    case RegAllocLocationKind::Xmm:
      emitMovqRegFromXmm(scratch, where.reg);
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
    } else if (where.kind == RegAllocLocationKind::Xmm) {
      emitMovqXmmFromReg(where.reg, reg);
    }
  };
  // A register usable as a memory base holding `value` (rcx when it is spilled, a constant, or
  // in r12, whose low bits mean "SIB byte follows" in the base encoding used here).
  const auto addressReg = [&](uint32_t value) -> uint8_t {
    const uint8_t reg = valueReg(value, Rcx);
    if ((reg & 7) == 4) {
      emitMovRegReg(Rcx, reg);
      return Rcx;
    }
    return reg;
  };
  // JIT mode: a VM frame address (a byte offset below vmLocalCount * 16, a multiple of 16)
  // checked as the VM checks it and turned into the slot's machine address. Local k sits
  // at the frame slot disp(k) = disp(0) + 16 k below rbp.
  //
  // A heap address (bit 63 set, when the module allocates) names heap slot (bits 0-47) / 16, which
  // must be below the slot count with a state equal to the address's top 16 bits (live, same
  // generation; VmHeapCore), or the VM's "invalid indirect address" fault. The machine address goes to rdx either way; the address
  // register (rcx or an allocated one, never rax or rdx) is left as it was.
  const auto jitFrameAddress = [&](uint32_t value) -> uint8_t {
    constexpr uint8_t Rdx = 2;
    const uint8_t address = valueReg(value, Rcx);
    emitRex(true, 0, address); // test address, 15
    emitByte(0xF7);
    emitModRmReg(0, address);
    emitU32(IrSlotBytes - 1);
    emitJitFaultIfWithRegister(CondCode::Ne, JitFault::UnalignedIndirectAddress, address);
    size_t toHeap = 0;
    if (hooks.heapAddresses) {
      emitTestRegReg(address);
      toHeap = emitCondJumpPlaceholder(CondCode::Lt); // js
    }
    emitCmpRegImm32(address, static_cast<int32_t>(hooks.vmLocalCount * IrSlotBytes));
    emitJitFaultIfWithRegister(CondCode::AboveEq, JitFault::InvalidIndirectAddress, address);
    emitMovRegReg(Rdx, address);
    emitAddRegReg(Rdx, 5);
    emitAddRegImm32(Rdx, regAllocSlotDisp(0));
    if (hooks.heapAddresses) {
      const size_t done = emitJumpPlaceholderRaw();
      patchCondJumpHere(toHeap);
      emitMovRegReg(Rdx, address);
      for (const uint8_t byte : {0x48, 0xC1, 0xE2, 0x10, 0x48, 0xC1, 0xEA, 0x14}) {
        emitByte(byte); // shl rdx, 16; shr rdx, 20: the slot index (offset bits / 16)
      }
      emitByte(0x48); // cmp rdx, [rip + slot count]
      emitByte(0x3B);
      emitJitDataOperand(Rdx, JitDataHeapSlots);
      emitJitFaultIfWithRegister(CondCode::AboveEq, JitFault::InvalidIndirectAddress, address);
      emitByte(0x48); // mov [rip + scratch], rax
      emitByte(0x89);
      emitJitDataOperand(Rax, JitDataScratch);
      emitByte(0x48); // mov rax, [rip + slot states]
      emitByte(0x8B);
      emitJitDataOperand(Rax, JitDataHeapStates);
      for (const uint8_t byte : {0x0F, 0xB7, 0x04, 0x50, 0x48, 0xC1, 0xE0, 0x30}) {
        emitByte(byte); // movzx eax, word [rax + rdx*2]; shl rax, 48
      }
      emitXorRegReg(Rax, address); // the state must equal the address's top 16 bits
      for (const uint8_t byte : {0x48, 0xC1, 0xE8, 0x30}) {
        emitByte(byte); // shr rax, 48
      }
      emitByte(0x48); // mov rax, [rip + scratch] (flags unchanged)
      emitByte(0x8B);
      emitJitDataOperand(Rax, JitDataScratch);
      emitJitFaultIfWithRegister(CondCode::Ne, JitFault::InvalidIndirectAddress, address);
      for (const uint8_t byte : {0x48, 0xC1, 0xE2, 0x03}) {
        emitByte(byte); // shl rdx, 3
      }
      emitByte(0x48); // add rdx, [rip + heap base]
      emitByte(0x03);
      emitJitDataOperand(Rdx, JitDataHeapBase);
      patchJumpHere(done);
    }
    return Rdx;
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
  // The float counterparts: the xmm register holding `value` (loaded into xmm `scratch` unless it
  // lives in one; an immediate goes through rcx), writing `value` from an xmm register, and
  // where to compute it (its own xmm register unless that is `avoid`, otherwise xmm0).
  const auto xmmOf = [&](uint32_t value, uint8_t scratch) -> uint8_t {
    const RegAllocLocation &where = location(value);
    switch (where.kind) {
    case RegAllocLocationKind::Xmm:
      return where.reg;
    case RegAllocLocationKind::Reg:
      emitMovqXmmFromReg(scratch, where.reg);
      return scratch;
    case RegAllocLocationKind::Slot:
      emitMovqXmmFromMem(scratch, 5, slotDisp(where.slot));
      return scratch;
    case RegAllocLocationKind::Imm:
      if (where.imm == 0) {
        emitXorpsXmm(scratch, scratch);
      } else {
        emitLoadXmmImm64(scratch, where.imm, Rcx);
      }
      return scratch;
    case RegAllocLocationKind::None:
      break;
    }
    emitXorpsXmm(scratch, scratch);
    return scratch;
  };
  const auto storeXmm = [&](uint32_t value, uint8_t xmm) {
    const RegAllocLocation &where = location(value);
    if (where.kind == RegAllocLocationKind::Xmm) {
      if (where.reg != xmm) {
        emitMovapsXmm(where.reg, xmm);
      }
    } else if (where.kind == RegAllocLocationKind::Reg) {
      emitMovqRegFromXmm(where.reg, xmm);
    } else if (where.kind == RegAllocLocationKind::Slot) {
      emitMovqMemFromXmm(5, slotDisp(where.slot), xmm);
    }
  };
  const auto xmmTarget = [&](uint32_t value, int avoid) -> uint8_t {
    const RegAllocLocation &where = location(value);
    if (where.kind == RegAllocLocationKind::Xmm && static_cast<int>(where.reg) != avoid) {
      return where.reg;
    }
    return 0;
  };
  // Saves and restores a register numbered as regAllocRegistersLiveAcross numbers them.
  const auto saveRegister = [&](uint8_t reg) {
    const int32_t disp = regAllocSlotDisp(saveBaseLocal + reg);
    if (reg >= 16) {
      emitMovqMemFromXmm(5, disp, static_cast<uint8_t>(reg - 16));
    } else {
      emitStoreMem(5, disp, reg);
    }
  };
  const auto restoreRegister = [&](uint8_t reg) {
    const int32_t disp = regAllocSlotDisp(saveBaseLocal + reg);
    if (reg >= 16) {
      emitMovqXmmFromMem(static_cast<uint8_t>(reg - 16), 5, disp);
    } else {
      emitLoadMem(reg, 5, disp);
    }
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
    if (a.kind == RegAllocLocationKind::Reg || a.kind == RegAllocLocationKind::Xmm) {
      return a.reg == b.reg;
    }
    if (a.kind == RegAllocLocationKind::Slot) {
      return a.slot == b.slot;
    }
    return false;
  };
  const auto emitCopy = [&](const RegAllocLocation &source, const RegAllocLocation &destination) {
    if (destination.kind == RegAllocLocationKind::Xmm) {
      if (source.kind == RegAllocLocationKind::Xmm) {
        emitMovapsXmm(destination.reg, source.reg);
      } else if (source.kind == RegAllocLocationKind::Reg) {
        emitMovqXmmFromReg(destination.reg, source.reg);
      } else if (source.kind == RegAllocLocationKind::Slot) {
        emitMovqXmmFromMem(destination.reg, 5, slotDisp(source.slot));
      } else if (source.kind == RegAllocLocationKind::Imm) {
        if (source.imm == 0) {
          emitXorpsXmm(destination.reg, destination.reg);
        } else {
          emitLoadXmmImm64(destination.reg, source.imm, Rcx);
        }
      }
      return;
    }
    if (source.kind == RegAllocLocationKind::Xmm) {
      if (destination.kind == RegAllocLocationKind::Reg) {
        emitMovqRegFromXmm(destination.reg, source.reg);
      } else if (destination.kind == RegAllocLocationKind::Slot) {
        emitMovqMemFromXmm(5, slotDisp(destination.slot), source.reg);
      }
      return;
    }
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
          destination.kind != RegAllocLocationKind::Slot &&
          destination.kind != RegAllocLocationKind::Xmm) {
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

  // Runs `body` on the memory operand stack: the operands pushed there first, the results
  // popped into their locations afterwards, and (with `saveLive`) every register that must
  // survive it saved around it.
  const auto onOperandStack = [&](size_t blockIndex,
                                  const RegAllocInstruction &instruction,
                                  bool saveLive,
                                  const auto &body) -> bool {
    std::vector<uint8_t> saved;
    if (saveLive) {
      saved = regAllocRegistersLiveAcross(plan, blockIndex, instruction.irIndex);
    }
    for (const uint8_t reg : saved) {
      saveRegister(reg);
    }
    for (const uint32_t use : instruction.uses) {
      emitSpillReg(valueReg(use, Rax));
    }
    if (!body()) {
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
      restoreRegister(reg);
    }
    return true;
  };

  // Entry: the first three parameters come from rax, rcx and rdx when the function takes register
  // arguments, the others off the operand stack, top first (rax is free once the register
  // arguments are home); locals read before any store start at zero.
  if (jitMode_ && hooks.zeroFrameLocals && hooks.vmLocalCount > 0) {
    // The VM starts every local at zero. rax, rcx and rdx may hold register arguments here,
    // but no allocatable register holds anything yet.
    emitJitZeroFrameLocals(hooks.vmLocalCount);
  }
  if (!plan.blocks.empty() && plan.blocks[0].reachable) {
    const RegAllocBlock &first = plan.blocks[0];
    constexpr uint8_t ArgumentRegisters[] = {Rax, Rcx, 2};
    const size_t inRegisters =
        hooks.argumentsInRegisters ? std::min<size_t>(3, first.entryValues.size()) : 0;
    for (size_t k = 0; k < inRegisters; ++k) {
      storeValue(first.entryValues[k], ArgumentRegisters[k]);
    }
    for (size_t k = first.entryValues.size(); k-- > inRegisters;) {
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
      } else if (where.kind == RegAllocLocationKind::Xmm) {
        emitXorpsXmm(where.reg, where.reg);
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
      if (jitMode_ && !native_emitter::nativeJitRunsOpcodeInline(ir.op)) {
        // The runtime runs it on the operands this pushes; its results replace them.
        if (!onOperandStack(blockIndex, instruction, true, [&] {
              emitJitHostCall(hooks.functionIndex, static_cast<uint32_t>(instruction.irIndex));
              const int64_t dropped = static_cast<int64_t>(instruction.uses.size()) -
                                      static_cast<int64_t>(instruction.defs.size());
              if (dropped != 0) {
                emitAddRegImm32(15, static_cast<int32_t>(dropped * 16));
              }
              return true;
            })) {
          return false;
        }
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
      case IrOpcode::AddF32:
      case IrOpcode::SubF32:
      case IrOpcode::MulF32:
      case IrOpcode::DivF32:
      case IrOpcode::AddF64:
      case IrOpcode::SubF64:
      case IrOpcode::MulF64:
      case IrOpcode::DivF64: {
        // `d = a OP b` in d's xmm register (xmm0 when d lives elsewhere or b is there); xmm0,
        // xmm1 and rcx are scratch, so no register is saved.
        const bool isF64 = ir.op == IrOpcode::AddF64 || ir.op == IrOpcode::SubF64 ||
                           ir.op == IrOpcode::MulF64 || ir.op == IrOpcode::DivF64;
        uint8_t opcode = 0x58; // add
        if (ir.op == IrOpcode::SubF32 || ir.op == IrOpcode::SubF64) {
          opcode = 0x5C;
        } else if (ir.op == IrOpcode::MulF32 || ir.op == IrOpcode::MulF64) {
          opcode = 0x59;
        } else if (ir.op == IrOpcode::DivF32 || ir.op == IrOpcode::DivF64) {
          opcode = 0x5E;
        }
        uint32_t a = instruction.uses[0];
        uint32_t b = instruction.uses[1];
        const uint32_t d = instruction.defs[0];
        const bool commutes = opcode == 0x58 || opcode == 0x59;
        if (commutes && location(d).kind == RegAllocLocationKind::Xmm &&
            location(b).kind == RegAllocLocationKind::Xmm && location(b).reg == location(d).reg) {
          std::swap(a, b);
        }
        const RegAllocLocation &right = location(b);
        const uint8_t target =
            xmmTarget(d, right.kind == RegAllocLocationKind::Xmm ? right.reg : -1);
        const uint8_t left = xmmOf(a, target);
        if (left != target) {
          emitMovapsXmm(target, left);
        }
        emitSseBinaryOp(isF64, opcode, target, xmmOf(b, 1));
        if (!isF64 && jitMode_) {
          emitClearXmmHigh32(target);
        }
        storeXmm(d, target);
        break;
      }
      case IrOpcode::NegF32:
      case IrOpcode::NegF64: {
        const uint32_t d = instruction.defs[0];
        if (location(d).kind == RegAllocLocationKind::Xmm) {
          // xor with the sign mask.
          const uint8_t target = xmmTarget(d, -1);
          const uint8_t source = xmmOf(instruction.uses[0], target);
          if (source != target) {
            emitMovapsXmm(target, source);
          }
          emitLoadXmmImm64(
              1, ir.op == IrOpcode::NegF64 ? 0x8000000000000000ull : 0x80000000ull, Rcx);
          emitXorpsXmm(target, 1);
          if (ir.op == IrOpcode::NegF32 && jitMode_) {
            emitClearXmmHigh32(target);
          }
          break;
        }
        // Flip the sign bit in place: btc target, 31 or 63.
        const uint8_t target = targetReg(d, -1);
        loadInto(instruction.uses[0], target);
        emitRex(true, 0, target);
        emitByte(0x0F);
        emitByte(0xBA);
        emitByte(static_cast<uint8_t>(0xC0 | (7 << 3) | (target & 7)));
        emitByte(ir.op == IrOpcode::NegF64 ? 63 : 31);
        if (ir.op == IrOpcode::NegF32 && jitMode_) {
          emitMovRegReg32(target, target); // the VM's f32 values are zero-extended
        }
        storeValue(d, target);
        break;
      }
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
      case IrOpcode::CmpGeF64: {
        // The conditions of emitFloatCompareAndPush (false on NaN except for !=).
        CondCode cc = CondCode::Eq;
        switch (ir.op) {
        case IrOpcode::CmpNeF32:
        case IrOpcode::CmpNeF64:
          cc = CondCode::Ne;
          break;
        case IrOpcode::CmpLtF32:
        case IrOpcode::CmpLtF64:
          cc = CondCode::Below;
          break;
        case IrOpcode::CmpLeF32:
        case IrOpcode::CmpLeF64:
          cc = CondCode::BelowEq;
          break;
        case IrOpcode::CmpGtF32:
        case IrOpcode::CmpGtF64:
          cc = CondCode::Above;
          break;
        case IrOpcode::CmpGeF32:
        case IrOpcode::CmpGeF64:
          cc = CondCode::AboveEq;
          break;
        default:
          break;
        }
        const bool isF64 = ir.op >= IrOpcode::CmpEqF64 && ir.op <= IrOpcode::CmpGeF64;
        const uint8_t left = xmmOf(instruction.uses[0], 0);
        const uint8_t right = xmmOf(instruction.uses[1], 1);
        const uint32_t d = instruction.defs[0];
        const uint8_t target = targetReg(d, -1);
        emitFloatCompareToReg(isF64, cc, left, right, target);
        storeValue(d, target);
        break;
      }
      case IrOpcode::ConvertI32ToF32:
      case IrOpcode::ConvertI64ToF32:
      case IrOpcode::ConvertI32ToF64:
      case IrOpcode::ConvertI64ToF64: {
        const bool isF64 = ir.op == IrOpcode::ConvertI32ToF64 || ir.op == IrOpcode::ConvertI64ToF64;
        const uint8_t source = valueReg(instruction.uses[0], Rax);
        // cvtsi2s writes only the low lane of its target and so waits for the last writer;
        // clearing the target first breaks that dependency.
        const uint32_t d = instruction.defs[0];
        const uint8_t target = xmmTarget(d, -1);
        emitXorpsXmm(target, target);
        // The I32 forms convert the low 32 bits, as the VM does.
        emitCvtsi2s(isF64,
                    target,
                    source,
                    ir.op == IrOpcode::ConvertI32ToF32 || ir.op == IrOpcode::ConvertI32ToF64);
        storeXmm(d, target);
        break;
      }
      case IrOpcode::ConvertF32ToI32:
      case IrOpcode::ConvertF32ToI64:
      case IrOpcode::ConvertF64ToI32:
      case IrOpcode::ConvertF64ToI64: {
        const bool isF64 = ir.op == IrOpcode::ConvertF64ToI32 || ir.op == IrOpcode::ConvertF64ToI64;
        const uint8_t source = xmmOf(instruction.uses[0], 0);
        const uint32_t d = instruction.defs[0];
        const uint8_t target = targetReg(d, -1);
        emitCvtts2si(isF64, target, source);
        storeValue(d, target);
        break;
      }
      case IrOpcode::ConvertF32ToF64:
      case IrOpcode::ConvertF64ToF32: {
        const uint8_t source = xmmOf(instruction.uses[0], 0);
        const uint32_t d = instruction.defs[0];
        const uint8_t target = xmmTarget(d, -1);
        if (ir.op == IrOpcode::ConvertF32ToF64) {
          emitCvtss2sd(target, source);
        } else {
          emitCvtsd2ss(target, source);
          if (jitMode_) {
            emitClearXmmHigh32(target);
          }
        }
        storeXmm(d, target);
        break;
      }
      case IrOpcode::DivI32:
      case IrOpcode::DivI64:
      case IrOpcode::DivU64: {
        // rax:rdx / divisor; only scratch registers change, so nothing is saved. A zero divisor
        // traps as in the template, or is a VM fault in JIT mode. A divisor of -1 negates, as in
        // the VM (INT64_MIN / -1 wraps instead of trapping).
        loadInto(instruction.uses[0], Rax);
        const uint8_t divisor = valueReg(instruction.uses[1], Rcx);
        if (jitMode_) {
          emitRex(true, divisor, divisor); // test divisor, divisor
          emitByte(0x85);
          emitModRmReg(divisor, divisor);
          emitJitFaultIf(CondCode::Eq, JitFault::DivisionByZero);
        }
        if (ir.op == IrOpcode::DivU64) {
          emitXorRegReg(2, 2);
          emitDivReg(divisor);
        } else {
          emitCmpRegImm32(divisor, -1);
          const size_t divide = emitCondJumpPlaceholder(CondCode::Ne);
          emitNegReg(Rax);
          const size_t done = emitJumpPlaceholderRaw();
          patchCondJumpHere(divide);
          emitCqo();
          emitIdivReg(divisor);
          patchJumpHere(done);
        }
        storeValue(instruction.defs[0], Rax);
        break;
      }
      case IrOpcode::AddressOfLocal: {
        const uint32_t d = instruction.defs[0];
        const uint8_t target = targetReg(d, -1);
        if (jitMode_) {
          // The VM's address: the slot's byte offset in this frame.
          emitMovRegImm64(target, ir.imm * IrSlotBytes);
        } else {
          emitMovRegReg(target, 5);
          emitAddRegImm32(target, regAllocSlotDisp(static_cast<uint32_t>(ir.imm)));
        }
        storeValue(d, target);
        break;
      }
      case IrOpcode::LoadIndirect: {
        const uint32_t d = instruction.defs[0];
        const uint8_t target = targetReg(d, -1);
        const uint8_t address =
            jitMode_ ? jitFrameAddress(instruction.uses[0]) : addressReg(instruction.uses[0]);
        emitLoadMem(target, address, 0);
        storeValue(d, target);
        break;
      }
      case IrOpcode::StoreIndirect: {
        // Stores the value at the address and leaves the value as the result.
        const uint8_t value = valueReg(instruction.uses[1], Rax);
        const uint8_t address =
            jitMode_ ? jitFrameAddress(instruction.uses[0]) : addressReg(instruction.uses[0]);
        emitStoreMem(address, 0, value);
        if (!instruction.defs.empty()) {
          storeValue(instruction.defs[0], value);
        }
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
        if (jitMode_) {
          // The VM faults on a position at or past the end (unsigned, so negatives too).
          emitCmpRegImm32(index, static_cast<int32_t>(hooks.stringLength(ir.imm)));
          emitJitFaultIf(CondCode::AboveEq, JitFault::StringIndexOutOfBounds);
        }
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
      case IrOpcode::Call:
      case IrOpcode::CallVoid: {
        // The callee takes its arguments off the operand stack and returns its result in rax;
        // every register still needed afterwards is saved around the call.
        const std::vector<uint8_t> saved =
            regAllocRegistersLiveAcross(plan, blockIndex, instruction.irIndex);
        for (const uint8_t reg : saved) {
          saveRegister(reg);
        }
        if (hooks.calleeTakesRegisterArguments(ir.imm)) {
          // No argument lives in a scratch register, so loading them in order clobbers none.
          // Arguments after the third go on the operand stack first (through rax); then the
          // first three are loaded, which clobbers no argument since none lives in a scratch
          // register.
          constexpr uint8_t ArgumentRegisters[] = {Rax, Rcx, 2};
          for (size_t k = 3; k < instruction.uses.size(); ++k) {
            emitSpillReg(valueReg(instruction.uses[k], Rax));
          }
          for (size_t k = 0; k < instruction.uses.size() && k < 3; ++k) {
            loadInto(instruction.uses[k], ArgumentRegisters[k]);
          }
        } else {
          for (const uint32_t use : instruction.uses) {
            emitSpillReg(valueReg(use, Rax));
          }
        }
        if (jitMode_) {
          emitJitEnterCall();
        }
        if (!hooks.recordCallFixup(emitCallPlaceholder(), ir.imm)) {
          return false;
        }
        if (jitMode_) {
          emitJitLeaveCall();
        }
        if (ir.op == IrOpcode::Call && !instruction.defs.empty()) {
          storeValue(instruction.defs[0], Rax);
        }
        for (const uint8_t reg : saved) {
          restoreRegister(reg);
        }
        break;
      }
      case IrOpcode::ReturnI32:
      case IrOpcode::ReturnI64:
      case IrOpcode::ReturnF32:
      case IrOpcode::ReturnF64: {
        endsWithBranch = true;
        const uint8_t value = valueReg(instruction.uses[0], Rax);
        if (ir.op == IrOpcode::ReturnI32) {
          emitMovsxdRegReg(Rax, value);
        } else if (value != Rax) {
          emitMovRegReg(Rax, value);
        }
        if (isEntryFunction_ && !jitMode_) {
          emitExitSyscall();
        } else {
          emitMovRegReg(4, 5); // mov rsp, rbp
          emitPopReg64(5);     // pop rbp
          emitRet();
        }
        break;
      }
      default: {
        if (jitMode_ && ir.op == IrOpcode::LoadStringLength) {
          // The VM's string checks: a tagged (dynamic) index names no string here, and a module
          // index must exist.
          const uint8_t index = valueReg(instruction.uses[0], Rax);
          emitCmpRegImm32(index, 0);
          emitJitFaultIf(CondCode::Lt, JitFault::InvalidDynamicStringIndex);
          emitCmpRegImm32(index, static_cast<int32_t>(hooks.stringCount));
          emitJitFaultIf(CondCode::AboveEq, JitFault::InvalidStringIndex);
        }
        // Template: operands onto the operand stack, results back off it, and every register
        // that must survive the template saved around it.
        const bool returns = ir.op == IrOpcode::ReturnVoid || ir.op == IrOpcode::ReturnI32 ||
                             ir.op == IrOpcode::ReturnI64 || ir.op == IrOpcode::ReturnF32 ||
                             ir.op == IrOpcode::ReturnF64;
        if (returns) {
          endsWithBranch = true;
        }
        if (!onOperandStack(blockIndex, instruction, !returns, [&] {
              return hooks.emitTemplate(instruction.irIndex);
            })) {
          return false;
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
  if (jitMode_) {
    // Falling off the end (or jumping to it) is the VM's "missing return" fault.
    emitJitFault(JitFault::MissingReturn, hooks.functionIndex);
  }

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
