#pragma once

inline void Arm64Emitter::emitDup() {
  emitPopReg(0);
  emitPushReg(0);
  emitPushReg(0);
}

inline void Arm64Emitter::emitPop() {
  emitPopReg(0);
}

inline void Arm64Emitter::emitAdd() {
  emitBinaryOp(encodeAddReg(0, 1, 0));
}

inline void Arm64Emitter::emitSub() {
  emitBinaryOp(encodeSubReg(0, 1, 0));
}

inline void Arm64Emitter::emitMul() {
  emitBinaryOp(encodeMulReg(0, 1, 0));
}

inline void Arm64Emitter::emitDiv() {
  emitBinaryOp(encodeSdivReg(0, 1, 0));
}

inline void Arm64Emitter::emitDivU() {
  emitBinaryOp(encodeUdivReg(0, 1, 0));
}

inline void Arm64Emitter::emitNeg() {
  emitPopReg(0);
  emit(encodeSubReg(0, 31, 0));
  emitPushReg(0);
}

inline void Arm64Emitter::emitSextI32() {
  emitPopReg(0);
  emit(0x93407C00u); // SXTW x0, w0 (SBFM x0, x0, #0, #31)
  emitPushReg(0);
}

inline void Arm64Emitter::emitAddF32() {
  emitFloatBinaryOp(false, kFaddD0D0D1, kFaddS0S0S1);
}

inline void Arm64Emitter::emitSubF32() {
  emitFloatBinaryOp(false, kFsubD0D0D1, kFsubS0S0S1);
}

inline void Arm64Emitter::emitMulF32() {
  emitFloatBinaryOp(false, kFmulD0D0D1, kFmulS0S0S1);
}

inline void Arm64Emitter::emitDivF32() {
  emitFloatBinaryOp(false, kFdivD0D0D1, kFdivS0S0S1);
}

inline void Arm64Emitter::emitNegF32() {
  emitFloatUnaryOp(false, kFnegD0, kFnegS0);
}

inline void Arm64Emitter::emitAddF64() {
  emitFloatBinaryOp(true, kFaddD0D0D1, kFaddS0S0S1);
}

inline void Arm64Emitter::emitSubF64() {
  emitFloatBinaryOp(true, kFsubD0D0D1, kFsubS0S0S1);
}

inline void Arm64Emitter::emitMulF64() {
  emitFloatBinaryOp(true, kFmulD0D0D1, kFmulS0S0S1);
}

inline void Arm64Emitter::emitDivF64() {
  emitFloatBinaryOp(true, kFdivD0D0D1, kFdivS0S0S1);
}

inline void Arm64Emitter::emitNegF64() {
  emitFloatUnaryOp(true, kFnegD0, kFnegS0);
}

inline void Arm64Emitter::emitCmpEq() {
  emitCompareAndPush(CondCode::Eq);
}

inline void Arm64Emitter::emitCmpNe() {
  emitCompareAndPush(CondCode::Ne);
}

inline void Arm64Emitter::emitCmpLt() {
  emitCompareAndPush(CondCode::Lt);
}

inline void Arm64Emitter::emitCmpLe() {
  emitCompareAndPush(CondCode::Le);
}

inline void Arm64Emitter::emitCmpGt() {
  emitCompareAndPush(CondCode::Gt);
}

inline void Arm64Emitter::emitCmpGe() {
  emitCompareAndPush(CondCode::Ge);
}

inline void Arm64Emitter::emitCmpLtU() {
  emitCompareAndPush(CondCode::Lo);
}

inline void Arm64Emitter::emitCmpLeU() {
  emitCompareAndPush(CondCode::Ls);
}

inline void Arm64Emitter::emitCmpGtU() {
  emitCompareAndPush(CondCode::Hi);
}

inline void Arm64Emitter::emitCmpGeU() {
  emitCompareAndPush(CondCode::Hs);
}

inline void Arm64Emitter::emitCmpEqF32() {
  emitCompareAndPushFloat(false, CondCode::Eq);
}

inline void Arm64Emitter::emitCmpNeF32() {
  emitCompareAndPushFloat(false, CondCode::Ne);
}

inline void Arm64Emitter::emitCmpLtF32() {
  emitCompareAndPushFloat(false, CondCode::Mi);
}

inline void Arm64Emitter::emitCmpLeF32() {
  emitCompareAndPushFloat(false, CondCode::Ls);
}

inline void Arm64Emitter::emitCmpGtF32() {
  emitCompareAndPushFloat(false, CondCode::Gt);
}

inline void Arm64Emitter::emitCmpGeF32() {
  emitCompareAndPushFloat(false, CondCode::Ge);
}

inline void Arm64Emitter::emitCmpEqF64() {
  emitCompareAndPushFloat(true, CondCode::Eq);
}

inline void Arm64Emitter::emitCmpNeF64() {
  emitCompareAndPushFloat(true, CondCode::Ne);
}

inline void Arm64Emitter::emitCmpLtF64() {
  emitCompareAndPushFloat(true, CondCode::Mi);
}

inline void Arm64Emitter::emitCmpLeF64() {
  emitCompareAndPushFloat(true, CondCode::Ls);
}

inline void Arm64Emitter::emitCmpGtF64() {
  emitCompareAndPushFloat(true, CondCode::Gt);
}

inline void Arm64Emitter::emitCmpGeF64() {
  emitCompareAndPushFloat(true, CondCode::Ge);
}

inline void Arm64Emitter::emitConvertI32ToF32() {
  emitConvertIntToFloat(false, false);
}

inline void Arm64Emitter::emitConvertI32ToF64() {
  emitConvertIntToFloat(true, false);
}

inline void Arm64Emitter::emitConvertI64ToF32() {
  emitConvertIntToFloat(false, false);
}

inline void Arm64Emitter::emitConvertI64ToF64() {
  emitConvertIntToFloat(true, false);
}

inline void Arm64Emitter::emitConvertU64ToF32() {
  emitConvertIntToFloat(false, true);
}

inline void Arm64Emitter::emitConvertU64ToF64() {
  emitConvertIntToFloat(true, true);
}

inline void Arm64Emitter::emitConvertF32ToI32() {
  emitConvertFloatToInt(false, false);
}

inline void Arm64Emitter::emitConvertF32ToI64() {
  emitConvertFloatToInt(false, false);
}

inline void Arm64Emitter::emitConvertF32ToU64() {
  emitConvertFloatToInt(false, true);
}

inline void Arm64Emitter::emitConvertF64ToI32() {
  emitConvertFloatToInt(true, false);
}

inline void Arm64Emitter::emitConvertF64ToI64() {
  emitConvertFloatToInt(true, false);
}

inline void Arm64Emitter::emitConvertF64ToU64() {
  emitConvertFloatToInt(true, true);
}

inline void Arm64Emitter::emitConvertF32ToF64() {
  emitConvertFloatToFloat(false);
}

inline void Arm64Emitter::emitConvertF64ToF32() {
  emitConvertFloatToFloat(true);
}

inline size_t Arm64Emitter::emitJumpPlaceholder() {
  flushValueStackCache();
  size_t index = currentWordIndex();
  emit(encodeB(0));
  return index;
}

inline size_t Arm64Emitter::emitCallPlaceholder() {
  flushValueStackCache();
  size_t index = currentWordIndex();
  emit(encodeBl(0));
  return index;
}

inline size_t Arm64Emitter::emitJumpIfZeroPlaceholder() {
  emitPopReg(0);
  emitCompareRegZero(0);
  return emitFarCondBranchPlaceholder(CondCode::Eq);
}

inline void Arm64Emitter::patchJump(size_t index, int32_t offsetWords) {
  patchWord(index, encodeB(offsetWords));
}

inline void Arm64Emitter::patchCall(size_t index, int32_t offsetWords) {
  patchWord(index, encodeBl(offsetWords));
}

inline void Arm64Emitter::patchJumpIfZero(size_t index, int32_t offsetWords) {
  patchFarCondBranch(index, offsetWords, CondCode::Eq);
}

inline void Arm64Emitter::emitPushReg0() {
  emitPushReg(0);
}

inline void Arm64Emitter::emitBinaryOp(uint32_t opWord) {
  emitPopReg(0);
  emitPopReg(1);
  emit(opWord);
  emitPushReg(0);
}

inline void Arm64Emitter::emitFloatBinaryOp(bool isF64, uint32_t opD, uint32_t opS) {
  emitPopReg(0);
  emitPopReg(1);
  if (isF64) {
    emit(encodeFmovDX(0, 1));
    emit(encodeFmovDX(1, 0));
    emit(opD);
    emit(encodeFmovXD(0, 0));
  } else {
    emit(encodeFmovSW(0, 1));
    emit(encodeFmovSW(1, 0));
    emit(opS);
    emit(encodeFmovWS(0, 0));
  }
  emitPushReg(0);
}

inline void Arm64Emitter::emitFloatUnaryOp(bool isF64, uint32_t opD, uint32_t opS) {
  emitPopReg(0);
  if (isF64) {
    emit(encodeFmovDX(0, 0));
    emit(opD);
    emit(encodeFmovXD(0, 0));
  } else {
    emit(encodeFmovSW(0, 0));
    emit(opS);
    emit(encodeFmovWS(0, 0));
  }
  emitPushReg(0);
}

// fcmp sets NZCV to 0011 for an unordered pair (a NaN operand), which the signed LT and LE
// conditions would read as true; float less-than uses MI and less-or-equal LS instead, so every
// comparison but != is false on NaN, as on the VM.
inline void Arm64Emitter::emitCompareAndPushFloat(bool isF64, CondCode cond) {
  emitPopReg(0);
  emitPopReg(1);
  if (isF64) {
    emit(encodeFmovDX(0, 1));
    emit(encodeFmovDX(1, 0));
    emit(kFcmpD0D1);
  } else {
    emit(encodeFmovSW(0, 1));
    emit(encodeFmovSW(1, 0));
    emit(kFcmpS0S1);
  }
  emitCompareFlagsAndPush(cond);
}

inline void Arm64Emitter::emitConvertIntToFloat(bool toF64, bool isUnsigned) {
  emitPopReg(0);
  if (isUnsigned) {
    emit(toF64 ? kUcvtfD0X0 : kUcvtfS0X0);
  } else {
    emit(toF64 ? kScvtfD0X0 : kScvtfS0X0);
  }
  if (toF64) {
    emit(encodeFmovXD(0, 0));
  } else {
    emit(encodeFmovWS(0, 0));
  }
  emitPushReg(0);
}

inline void Arm64Emitter::emitConvertFloatToInt(bool fromF64, bool isUnsigned) {
  emitPopReg(0);
  if (fromF64) {
    emit(encodeFmovDX(0, 0));
  } else {
    emit(encodeFmovSW(0, 0));
    emit(kFcvtD0S0);
  }
  emit(isUnsigned ? kFcvtzuX0D0 : kFcvtzsX0D0);
  emitPushReg(0);
}

inline void Arm64Emitter::emitConvertFloatToFloat(bool fromF64) {
  emitPopReg(0);
  if (fromF64) {
    emit(encodeFmovDX(0, 0));
    emit(kFcvtS0D0);
    emit(encodeFmovWS(0, 0));
  } else {
    emit(encodeFmovSW(0, 0));
    emit(kFcvtD0S0);
    emit(encodeFmovXD(0, 0));
  }
  emitPushReg(0);
}

inline void Arm64Emitter::emitCompareRegZero(uint8_t reg) {
  emit(encodeSubsReg(31, reg, 31));
}

inline void Arm64Emitter::emitCompareReg(uint8_t left, uint8_t right) {
  emit(encodeSubsReg(31, left, right));
}

inline size_t Arm64Emitter::emitCondBranchPlaceholder(CondCode cond) {
  flushValueStackCache();
  size_t index = currentWordIndex();
  emit(encodeBCond(0, static_cast<uint8_t>(cond)));
  return index;
}

inline void Arm64Emitter::patchCondBranch(size_t index, int32_t offsetWords, CondCode cond) {
  patchWord(index, encodeBCond(offsetWords, static_cast<uint8_t>(cond)));
}

inline size_t Arm64Emitter::emitFarCondBranchPlaceholder(CondCode cond) {
  flushValueStackCache();
  size_t index = currentWordIndex();
  emit(encodeBCond(0, static_cast<uint8_t>(cond)));
  emit(encodeB(0));
  return index;
}

inline void Arm64Emitter::patchFarCondBranch(size_t index, int32_t offsetWords, CondCode cond) {
  constexpr int32_t kMinCond = -(1 << 18);
  constexpr int32_t kMaxCond = (1 << 18) - 1;
  if (offsetWords >= kMinCond && offsetWords <= kMaxCond) {
    patchWord(index, encodeBCond(offsetWords, static_cast<uint8_t>(cond)));
    patchWord(index + 1, encodeB(1));
    return;
  }
  CondCode inverted = invertCond(cond);
  patchWord(index, encodeBCond(2, static_cast<uint8_t>(inverted)));
  patchWord(index + 1, encodeB(offsetWords - 1));
}

inline size_t Arm64Emitter::emitCbzPlaceholder(uint8_t reg) {
  size_t index = currentWordIndex();
  emit(encodeCbz(reg, 0));
  return index;
}

inline void Arm64Emitter::patchCbz(size_t index, uint8_t reg, int32_t offsetWords) {
  patchWord(index, encodeCbz(reg, offsetWords));
}

inline void Arm64Emitter::emitCompareAndPush(CondCode cond) {
  emitPopReg(0);
  emitPopReg(1);
  emit(encodeSubsReg(31, 1, 0));
  emitCompareFlagsAndPush(cond);
}

// Pushes 1 when the flags satisfy `cond`, else 0: `cset x0, cond` (csinc x0, xzr, xzr, !cond).
inline void Arm64Emitter::emitCompareFlagsAndPush(CondCode cond) {
  emit(0x9A9F07E0u | (static_cast<uint32_t>(invertCond(cond)) << 12));
  emitPushReg(0);
}

inline bool Arm64Emitter::tryEmitImmediateOperand(IrOpcode op, uint64_t constant) {
  bool isAdd = false;
  bool isSub = false;
  CondCode cond = CondCode::Eq;
  switch (op) {
  case IrOpcode::AddI32:
  case IrOpcode::AddI64:
    isAdd = true;
    break;
  case IrOpcode::SubI32:
  case IrOpcode::SubI64:
    isSub = true;
    break;
  case IrOpcode::CmpEqI32:
  case IrOpcode::CmpEqI64:
    cond = CondCode::Eq;
    break;
  case IrOpcode::CmpNeI32:
  case IrOpcode::CmpNeI64:
    cond = CondCode::Ne;
    break;
  case IrOpcode::CmpLtI32:
  case IrOpcode::CmpLtI64:
    cond = CondCode::Lt;
    break;
  case IrOpcode::CmpLeI32:
  case IrOpcode::CmpLeI64:
    cond = CondCode::Le;
    break;
  case IrOpcode::CmpGtI32:
  case IrOpcode::CmpGtI64:
    cond = CondCode::Gt;
    break;
  case IrOpcode::CmpGeI32:
  case IrOpcode::CmpGeI64:
    cond = CondCode::Ge;
    break;
  case IrOpcode::CmpLtU64:
    cond = CondCode::Lo;
    break;
  case IrOpcode::CmpLeU64:
    cond = CondCode::Ls;
    break;
  case IrOpcode::CmpGtU64:
    cond = CondCode::Hi;
    break;
  case IrOpcode::CmpGeU64:
    cond = CondCode::Hs;
    break;
  default:
    return false;
  }
  uint16_t imm = 0;
  bool shifted = false;
  const uint64_t negated = 0 - constant;
  if (isAdd || isSub) {
    if (constant == 0) {
      return true; // the operand stays as it is
    }
    uint32_t base = 0;
    if (addSubImmediate(constant, imm, shifted)) {
      base = isAdd ? AddImmBase : SubImmBase;
    } else if (addSubImmediate(negated, imm, shifted)) {
      base = isAdd ? SubImmBase : AddImmBase;
    } else {
      return false;
    }
    emitPopReg(0);
    emit(encodeAddSubImm(base, 0, 0, imm, shifted));
    emitPushReg(0);
    return true;
  }
  // x - c and x + (-c) set the same flags for every c but 0 and the most negative value, so CMN of
  // the negated constant stands in for CMP when only the negation fits.
  uint32_t base = 0;
  if (addSubImmediate(constant, imm, shifted)) {
    base = SubsImmBase;
  } else if (constant != (uint64_t{1} << 63) && addSubImmediate(negated, imm, shifted)) {
    base = AddsImmBase;
  } else {
    return false;
  }
  emitPopReg(0);
  emit(encodeAddSubImm(base, 31, 0, imm, shifted));
  emitCompareFlagsAndPush(cond);
  return true;
}
