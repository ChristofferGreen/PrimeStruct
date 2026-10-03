#pragma once

// Deferred operand handling for X64Emitter (see setOperandDeferralEnabled in
// NativeEmitterInternalsX64.h). Included at the end of that header.
//
// The operand stack of an instruction sequence is tracked at compile time:
// `pending_` holds the top entries as constants, promoted locals (still living in
// their registers) or values in cache registers, and nothing has been emitted for
// them yet. An instruction that consumes operands emits the work directly on
// those registers and immediates, and the memory-backed stack (r15) only receives
// entries when they must outlive the instruction sequence: at branch targets, in
// front of jumps and calls, before opcodes whose templates clobber the cache
// registers, and when more operands are live than there are registers.

inline bool X64Emitter::pendingRegBusy(uint8_t reg) const {
  for (const PendingOperand &entry : pending_) {
    if (entry.kind == PendingOperand::Kind::Reg && entry.reg == reg) {
      return true;
    }
  }
  return false;
}

// A free cache register, not in `excludeMask` (bit n = register n). When none is
// free the oldest pending operands are written to the memory stack until one is.
inline uint8_t X64Emitter::allocPendingReg(uint32_t excludeMask) {
  const size_t limit = inComplexOp_ ? 1 : sizeof(PendingRegs);
  for (;;) {
    for (size_t i = 0; i < limit; ++i) {
      const uint8_t reg = PendingRegs[i];
      if ((excludeMask & (1u << reg)) == 0 && !pendingRegBusy(reg)) {
        return reg;
      }
    }
    if (pending_.empty()) {
      // Only reachable when the excluded registers cover the whole cache; the
      // callers never exclude more than two.
      return PendingRegs[0];
    }
    spillFrontPending();
  }
}

inline void X64Emitter::emitStoreImm64Mem(uint8_t base, int32_t disp, uint64_t imm) {
  const int64_t value = static_cast<int64_t>(imm);
  if (value >= INT32_MIN && value <= INT32_MAX) {
    emitRex(true, 0, base);
    emitByte(0xC7); // mov r/m64, imm32 (sign-extended)
    emitModRmBaseDisp32(0, base, disp);
    emitU32(static_cast<uint32_t>(value));
    return;
  }
  emitRex(false, 0, base);
  emitByte(0xC7); // mov r/m32, imm32: low half
  emitModRmBaseDisp32(0, base, disp);
  emitU32(static_cast<uint32_t>(imm & 0xFFFFFFFFull));
  emitRex(false, 0, base);
  emitByte(0xC7); // high half
  emitModRmBaseDisp32(0, base, disp + 4);
  emitU32(static_cast<uint32_t>(imm >> 32));
}

inline void X64Emitter::emitTestRegReg(uint8_t reg) {
  emitRex(true, reg, reg);
  emitByte(0x85); // test r/m64, r64
  emitModRmReg(reg, reg);
}

// Pushes one entry on the memory stack, leaving every register as it was.
inline void X64Emitter::spillPendingEntry(const PendingOperand &entry) {
  if (entry.kind == PendingOperand::Kind::Imm) {
    counters_.spillCount += 1;
    emitSubRegImm32(15, 16);
    emitStoreImm64Mem(15, 8, entry.imm);
    return;
  }
  emitSpillReg(entry.reg);
}

inline void X64Emitter::spillFrontPending() {
  const PendingOperand entry = pending_.front();
  pending_.erase(pending_.begin());
  spillPendingEntry(entry);
}

inline void X64Emitter::materializePending(const PendingOperand &entry, uint8_t reg) {
  if (entry.kind == PendingOperand::Kind::Imm) {
    emitMovRegImm64(reg, entry.imm);
    return;
  }
  if (entry.reg != reg) {
    emitMovRegReg(reg, entry.reg);
  }
}

// Takes the top operand. Its cache register, if any, stays reserved through
// `usedMask` so allocations made before it is consumed cannot hand it out again.
// With nothing pending the operand comes off the memory stack into a cache
// register.
inline X64Emitter::PendingOperand X64Emitter::popOperand(uint32_t &usedMask) {
  if (!pending_.empty()) {
    const PendingOperand entry = pending_.back();
    pending_.pop_back();
    if (entry.kind == PendingOperand::Kind::Reg) {
      usedMask |= 1u << entry.reg;
    }
    return entry;
  }
  PendingOperand entry;
  entry.kind = PendingOperand::Kind::Reg;
  entry.reg = allocPendingReg(usedMask);
  emitReloadReg(entry.reg);
  usedMask |= 1u << entry.reg;
  return entry;
}

inline void X64Emitter::pushPendingOperand(const PendingOperand &entry) {
  // A bounded pending list keeps flushes short.
  constexpr size_t MaxPending = 12;
  while (pending_.size() >= MaxPending) {
    spillFrontPending();
  }
  pending_.push_back(entry);
}

inline void X64Emitter::emitPushImmDeferred(uint64_t imm) {
  counters_.valueStackPushCount += 1;
  PendingOperand entry;
  entry.kind = PendingOperand::Kind::Imm;
  entry.imm = imm;
  pushPendingOperand(entry);
}

inline void X64Emitter::emitLoadLocalDeferred(uint32_t index) {
  counters_.valueStackPushCount += 1;
  PendingOperand entry;
  if (const int promoted = promotedRegister(index); promoted >= 0) {
    entry.kind = PendingOperand::Kind::Local;
    entry.reg = static_cast<uint8_t>(promoted);
    entry.local = index;
    pushPendingOperand(entry);
    return;
  }
  entry.kind = PendingOperand::Kind::Reg;
  entry.reg = allocPendingReg(0);
  emitLoadMem(entry.reg, 5, -static_cast<int32_t>(frameSize_ - localOffset(index)));
  pushPendingOperand(entry);
}

// A promoted local about to be overwritten must not still be referenced lazily
// by an operand below the one being stored: write the pending operands out.
inline void X64Emitter::flushPendingForAlias(uint32_t local) {
  for (const PendingOperand &entry : pending_) {
    if (entry.kind == PendingOperand::Kind::Local && entry.local == local) {
      flushValueStackCache();
      return;
    }
  }
}

inline void X64Emitter::emitStoreLocalDeferred(uint32_t index) {
  counters_.valueStackPopCount += 1;
  uint32_t used = 0;
  const PendingOperand value = popOperand(used);
  if (const int promoted = promotedRegister(index); promoted >= 0) {
    flushPendingForAlias(index);
    materializePending(value, static_cast<uint8_t>(promoted));
    return;
  }
  const int32_t disp = -static_cast<int32_t>(frameSize_ - localOffset(index));
  if (value.kind == PendingOperand::Kind::Imm) {
    emitStoreImm64Mem(5, disp, value.imm);
  } else {
    emitStoreMem(5, disp, value.reg);
  }
}

inline void X64Emitter::emitDupDeferred() {
  counters_.valueStackPopCount += 1;
  counters_.valueStackPushCount += 2;
  uint32_t used = 0;
  const PendingOperand top = popOperand(used);
  pushPendingOperand(top);
  PendingOperand copy = top;
  if (top.kind == PendingOperand::Kind::Reg) {
    copy.reg = allocPendingReg(used);
    emitMovRegReg(copy.reg, top.reg);
  }
  pushPendingOperand(copy);
}

inline void X64Emitter::emitPopDeferred() {
  counters_.valueStackPopCount += 1;
  if (pending_.empty()) {
    // Discard the memory stack's top slot.
    counters_.reloadCount += 1;
    emitAddRegImm32(15, 16);
    return;
  }
  pending_.pop_back();
}

template <typename Op> inline void X64Emitter::emitBinaryDeferred(Op &&op) {
  counters_.valueStackPopCount += 2;
  counters_.valueStackPushCount += 1;
  uint32_t used = 0;
  const PendingOperand b = popOperand(used);
  const PendingOperand a = popOperand(used);
  uint8_t dst = a.reg;
  if (a.kind != PendingOperand::Kind::Reg) {
    dst = allocPendingReg(used);
    materializePending(a, dst);
  }
  op(dst, b);
  PendingOperand result;
  result.kind = PendingOperand::Kind::Reg;
  result.reg = dst;
  pushPendingOperand(result);
}

inline void X64Emitter::emitNegDeferred() {
  counters_.valueStackPopCount += 1;
  counters_.valueStackPushCount += 1;
  uint32_t used = 0;
  const PendingOperand a = popOperand(used);
  uint8_t dst = a.reg;
  if (a.kind != PendingOperand::Kind::Reg) {
    dst = allocPendingReg(used);
    materializePending(a, dst);
  }
  emitNegReg(dst);
  PendingOperand result;
  result.kind = PendingOperand::Kind::Reg;
  result.reg = dst;
  pushPendingOperand(result);
}

inline void X64Emitter::emitCompareDeferred(CondCode cc) {
  counters_.valueStackPopCount += 2;
  counters_.valueStackPushCount += 1;
  uint32_t used = 0;
  const PendingOperand b = popOperand(used);
  const PendingOperand a = popOperand(used);
  // The result register is chosen before the compare: spilling would clobber flags.
  const uint8_t dst = a.kind == PendingOperand::Kind::Reg ? a.reg : allocPendingReg(used);
  uint8_t left = a.reg;
  if (a.kind == PendingOperand::Kind::Imm) {
    emitMovRegImm64(0, a.imm);
    left = 0;
  }
  if (b.kind == PendingOperand::Kind::Imm &&
      static_cast<int64_t>(b.imm) == static_cast<int32_t>(b.imm)) {
    emitCmpRegImm32(left, static_cast<int32_t>(b.imm));
  } else {
    uint8_t right = b.reg;
    if (b.kind == PendingOperand::Kind::Imm) {
      emitMovRegImm64(1, b.imm);
      right = 1;
    }
    emitCmpRegReg(left, right);
  }
  emitSetccReg(dst, cc);
  emitMovzxReg8(dst, dst);
  PendingOperand result;
  result.kind = PendingOperand::Kind::Reg;
  result.reg = dst;
  pushPendingOperand(result);
}

inline size_t X64Emitter::emitJumpIfZeroDeferred() {
  counters_.valueStackPopCount += 1;
  uint32_t used = 0;
  const PendingOperand condition = popOperand(used);
  // Whatever is still pending must be on the memory stack before the branch so
  // both successors see the same state; the condition stays in its register.
  flushValueStackCache();
  uint8_t reg = condition.reg;
  if (condition.kind == PendingOperand::Kind::Imm) {
    emitMovRegImm64(0, condition.imm);
    reg = 0;
  }
  emitTestRegReg(reg);
  emitByte(0x0F);
  emitByte(0x84); // jz rel32
  const size_t fixupIndex = code_.size();
  emitU32(0);
  return fixupIndex;
}

inline bool X64Emitter::compareCondition(IrOpcode op, CondCode &cc) const {
  switch (op) {
  case IrOpcode::CmpEqI32:
  case IrOpcode::CmpEqI64:
    cc = CondCode::Eq;
    return true;
  case IrOpcode::CmpNeI32:
  case IrOpcode::CmpNeI64:
    cc = CondCode::Ne;
    return true;
  case IrOpcode::CmpLtI32:
  case IrOpcode::CmpLtI64:
    cc = CondCode::Lt;
    return true;
  case IrOpcode::CmpLeI32:
  case IrOpcode::CmpLeI64:
    cc = CondCode::Le;
    return true;
  case IrOpcode::CmpGtI32:
  case IrOpcode::CmpGtI64:
    cc = CondCode::Gt;
    return true;
  case IrOpcode::CmpGeI32:
  case IrOpcode::CmpGeI64:
    cc = CondCode::Ge;
    return true;
  case IrOpcode::CmpLtU64:
    cc = CondCode::Below;
    return true;
  case IrOpcode::CmpLeU64:
    cc = CondCode::BelowEq;
    return true;
  case IrOpcode::CmpGtU64:
    cc = CondCode::Above;
    return true;
  case IrOpcode::CmpGeU64:
    cc = CondCode::AboveEq;
    return true;
  default:
    return false;
  }
}

inline bool X64Emitter::tryEmitCompareBranch(IrOpcode compareOp, size_t &fixupIndex) {
  CondCode cc = CondCode::Eq;
  if (!deferOperands_ || !compareCondition(compareOp, cc)) {
    return false;
  }
  counters_.valueStackPopCount += 2;
  uint32_t used = 0;
  const PendingOperand b = popOperand(used);
  const PendingOperand a = popOperand(used);
  // Entries below the operands go to the memory stack before the compare sets the
  // flags; the operands themselves stay in their registers.
  flushValueStackCache();
  uint8_t left = a.reg;
  if (a.kind == PendingOperand::Kind::Imm) {
    emitMovRegImm64(0, a.imm);
    left = 0;
  }
  if (b.kind == PendingOperand::Kind::Imm &&
      static_cast<int64_t>(b.imm) == static_cast<int32_t>(b.imm)) {
    emitCmpRegImm32(left, static_cast<int32_t>(b.imm));
  } else {
    uint8_t right = b.reg;
    if (b.kind == PendingOperand::Kind::Imm) {
      emitMovRegImm64(1, b.imm);
      right = 1;
    }
    emitCmpRegReg(left, right);
  }
  // JumpIfZero branches when the comparison is false.
  fixupIndex = emitCondJumpPlaceholder(invertCond(cc));
  return true;
}

inline void X64Emitter::beginComplexOp() {
  if (deferOperands_) {
    flushValueStackCache();
  }
  emitSpillPromotedLocals();
  inComplexOp_ = true;
}

inline void X64Emitter::endComplexOp() {
  inComplexOp_ = false;
  emitReloadPromotedLocals();
}

inline void X64Emitter::emitPromotedLocalUpdate(
    uint32_t local, int kind, bool operandIsImm, uint64_t imm, uint32_t operandLocal, bool sext) {
  counters_.valueStackPopCount += 1;
  const uint8_t target = static_cast<uint8_t>(promotedRegister(local));
  flushPendingForAlias(local);
  uint8_t source = 0;
  if (operandIsImm) {
    if (kind != 2 && static_cast<int64_t>(imm) == static_cast<int32_t>(imm)) {
      if (kind == 0) {
        emitAddRegImm32(target, static_cast<int32_t>(imm));
      } else {
        emitSubRegImm32(target, static_cast<int32_t>(imm));
      }
      if (sext) {
        emitMovsxdRegReg(target, target);
      }
      return;
    }
    emitMovRegImm64(0, imm);
    source = 0;
  } else if (const int promoted = promotedRegister(operandLocal); promoted >= 0) {
    source = static_cast<uint8_t>(promoted);
  } else {
    emitLoadMem(0, 5, -static_cast<int32_t>(frameSize_ - localOffset(operandLocal)));
    source = 0;
  }
  if (kind == 0) {
    emitAddRegReg(target, source);
  } else if (kind == 1) {
    emitSubRegReg(target, source);
  } else {
    emitImulRegReg(target, source);
  }
  if (sext) {
    emitMovsxdRegReg(target, target);
  }
}
