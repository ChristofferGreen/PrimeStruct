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
  emitCompareAndPushFloat(false, CondCode::Lt);
}

inline void Arm64Emitter::emitCmpLeF32() {
  emitCompareAndPushFloat(false, CondCode::Le);
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
  emitCompareAndPushFloat(true, CondCode::Lt);
}

inline void Arm64Emitter::emitCmpLeF64() {
  emitCompareAndPushFloat(true, CondCode::Le);
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
  return emitCondBranchPlaceholder(CondCode::Eq);
}

inline void Arm64Emitter::patchJump(size_t index, int32_t offsetWords) {
  patchWord(index, encodeB(offsetWords));
}

inline void Arm64Emitter::patchCall(size_t index, int32_t offsetWords) {
  patchWord(index, encodeBl(offsetWords));
}

inline void Arm64Emitter::patchJumpIfZero(size_t index, int32_t offsetWords) {
  patchCondBranch(index, offsetWords, CondCode::Eq);
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
  emit(encodeBCond(6, static_cast<uint8_t>(cond)));
  emitMovImm64(0, 0);
  emit(encodeB(5));
  emitMovImm64(0, 1);
  emitPushReg(0);
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
  emit(encodeB(0));
  return index;
}

inline void Arm64Emitter::patchCondBranch(size_t index, int32_t offsetWords, CondCode cond) {
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
  emit(encodeBCond(6, static_cast<uint8_t>(cond)));
  emitMovImm64(0, 0);
  emit(encodeB(5));
  emitMovImm64(0, 1);
  emitPushReg(0);
}
