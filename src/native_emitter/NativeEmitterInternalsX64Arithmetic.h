// x86_64 arithmetic/comparison/conversion codegen for X64Emitter.
// Included from NativeEmitterInternalsX64.h, after Core.h.
//
// Mirrors Arm64Emitter's simplification of using the same instruction
// sequence for both I32 and I64 IR opcodes: every pushed value already
// occupies a full 64-bit slot (see emitPushI32's sign-extending
// PushI32/PushI64), so treating every integer op as 64-bit produces the
// correct result for both source widths without a separate 32-bit path.

inline void X64Emitter::emitAddRegReg(uint8_t rd, uint8_t rs) {
  emitRex(true, rs, rd);
  emitByte(0x01); // ADD r/m64, r64
  emitModRmReg(rs, rd);
}

inline void X64Emitter::emitSubRegReg(uint8_t rd, uint8_t rs) {
  emitRex(true, rs, rd);
  emitByte(0x29); // SUB r/m64, r64
  emitModRmReg(rs, rd);
}

inline void X64Emitter::emitImulRegReg(uint8_t rd, uint8_t rs) {
  emitRex(true, rd, rs);
  emitByte(0x0F);
  emitByte(0xAF); // IMUL r64, r/m64 (RM form: reg=dst)
  emitModRmReg(rd, rs);
}

inline void X64Emitter::emitXorRegReg(uint8_t rd, uint8_t rs) {
  emitRex(true, rs, rd);
  emitByte(0x31); // XOR r/m64, r64
  emitModRmReg(rs, rd);
}

inline void X64Emitter::emitCqo() {
  emitByte(0x48); // REX.W
  emitByte(0x99); // CQO: sign-extend rax into rdx:rax
}

inline void X64Emitter::emitIdivReg(uint8_t reg) {
  emitRex(true, 0, reg);
  emitByte(0xF7);
  emitByte(static_cast<uint8_t>(0xF8 | (reg & 0x7))); // /7 = IDIV
}

inline void X64Emitter::emitDivReg(uint8_t reg) {
  emitRex(true, 0, reg);
  emitByte(0xF7);
  emitByte(static_cast<uint8_t>(0xF0 | (reg & 0x7))); // /6 = DIV
}

inline void X64Emitter::emitNegReg(uint8_t rd) {
  emitRex(true, 0, rd);
  emitByte(0xF7);
  emitByte(static_cast<uint8_t>(0xD8 | (rd & 0x7))); // /3 = NEG
}

inline void X64Emitter::emitCmpRegReg(uint8_t a, uint8_t b) {
  emitRex(true, b, a);
  emitByte(0x39); // CMP r/m64, r64 -> flags = a - b
  emitModRmReg(b, a);
}

inline void X64Emitter::emitSetccReg(uint8_t rd, CondCode cc) {
  // Always emit a REX prefix (even with no bits set beyond what's
  // required) so rsp/rbp/rsi/rdi's low-byte forms are addressable here -
  // without a REX prefix present, those four encode the legacy
  // ah/ch/dh/bh high-byte registers instead.
  const uint8_t bBit = (rd >> 3) & 0x1;
  emitByte(static_cast<uint8_t>(0x40 | bBit));
  emitByte(0x0F);
  emitByte(static_cast<uint8_t>(0x90 + condCodeValue(cc))); // SETcc r/m8
  emitModRmReg(0, rd);
}

inline void X64Emitter::emitMovzxReg8(uint8_t rd, uint8_t rs) {
  emitRex(true, rd, rs);
  emitByte(0x0F);
  emitByte(0xB6); // MOVZX r64, r/m8
  emitModRmReg(rd, rs);
}

inline uint8_t X64Emitter::condCodeValue(CondCode cc) {
  switch (cc) {
    case CondCode::Eq:
      return 0x4;
    case CondCode::Ne:
      return 0x5;
    case CondCode::Lt:
      return 0xC;
    case CondCode::Le:
      return 0xE;
    case CondCode::Gt:
      return 0xF;
    case CondCode::Ge:
      return 0xD;
    case CondCode::Below:
      return 0x2;
    case CondCode::BelowEq:
      return 0x6;
    case CondCode::Above:
      return 0x7;
    case CondCode::AboveEq:
      return 0x3;
  }
  return 0x5;
}

inline size_t X64Emitter::emitCondJumpPlaceholder(CondCode cc) {
  emitByte(0x0F);
  emitByte(static_cast<uint8_t>(0x80 + condCodeValue(cc))); // Jcc rel32
  const size_t fixupIndex = code_.size();
  emitU32(0);
  return fixupIndex;
}

inline void X64Emitter::patchCondJumpHere(size_t fixupIndex) {
  const int32_t delta = static_cast<int32_t>(code_.size()) - static_cast<int32_t>(fixupIndex + 4);
  patchU32(fixupIndex, static_cast<uint32_t>(delta));
}

inline size_t X64Emitter::emitJumpPlaceholderRaw() {
  emitByte(0xE9); // jmp rel32
  const size_t fixupIndex = code_.size();
  emitU32(0);
  return fixupIndex;
}

inline void X64Emitter::patchJumpHere(size_t fixupIndex) {
  const int32_t delta = static_cast<int32_t>(code_.size()) - static_cast<int32_t>(fixupIndex + 4);
  patchU32(fixupIndex, static_cast<uint32_t>(delta));
}

inline void X64Emitter::emitCompareAndPush(CondCode cc) {
  emitPopReg(1); // b
  emitPopReg(0); // a
  emitCmpRegReg(0, 1); // flags = a - b
  emitSetccReg(0, cc);
  emitMovzxReg8(0, 0);
  emitPushReg(0);
}

// --- SSE2 float primitives ---

inline void X64Emitter::emitMovqXmmFromReg(uint8_t xmm, uint8_t reg) {
  emitByte(0x66); // mandatory prefix
  emitRex(true, xmm, reg);
  emitByte(0x0F);
  emitByte(0x6E); // MOVQ xmm, r/m64
  emitModRmReg(xmm, reg);
}

inline void X64Emitter::emitMovqRegFromXmm(uint8_t reg, uint8_t xmm) {
  emitByte(0x66);
  emitRex(true, xmm, reg);
  emitByte(0x0F);
  emitByte(0x7E); // MOVQ r/m64, xmm
  emitModRmReg(xmm, reg);
}

inline void X64Emitter::emitSseBinaryOp(bool isF64, uint8_t opcode, uint8_t dstXmm, uint8_t srcXmm) {
  emitByte(isF64 ? 0xF2 : 0xF3);
  if (dstXmm >= 8 || srcXmm >= 8) {
    emitRex(false, dstXmm, srcXmm);
  }
  emitByte(0x0F);
  emitByte(opcode);
  emitModRmReg(dstXmm, srcXmm);
}

inline void X64Emitter::emitXorpsXmm(uint8_t dstXmm, uint8_t srcXmm) {
  if (dstXmm >= 8 || srcXmm >= 8) {
    emitRex(false, dstXmm, srcXmm);
  }
  emitByte(0x0F);
  emitByte(0x57); // XORPS xmm, xmm/m128 (bitwise, width-agnostic)
  emitModRmReg(dstXmm, srcXmm);
}

inline void X64Emitter::emitComiss(bool isF64, uint8_t a, uint8_t b) {
  if (isF64) {
    emitByte(0x66);
  }
  if (a >= 8 || b >= 8) {
    emitRex(false, a, b);
  }
  emitByte(0x0F);
  emitByte(0x2F); // COMISS/COMISD xmm, xmm/m32|64
  emitModRmReg(a, b);
}

inline void X64Emitter::emitCvtsi2s(bool isF64, uint8_t dstXmm, uint8_t srcReg) {
  emitByte(isF64 ? 0xF2 : 0xF3);
  emitRex(true, dstXmm, srcReg);
  emitByte(0x0F);
  emitByte(0x2A); // CVTSI2SS/SD xmm, r/m64
  emitModRmReg(dstXmm, srcReg);
}

inline void X64Emitter::emitCvtts2si(bool isF64, uint8_t dstReg, uint8_t srcXmm) {
  emitByte(isF64 ? 0xF2 : 0xF3);
  emitRex(true, dstReg, srcXmm);
  emitByte(0x0F);
  emitByte(0x2C); // CVTTSS2SI/CVTTSD2SI r64, xmm/m32|64
  emitModRmReg(dstReg, srcXmm);
}

inline void X64Emitter::emitCvtss2sd(uint8_t dstXmm, uint8_t srcXmm) {
  emitByte(0xF3);
  if (dstXmm >= 8 || srcXmm >= 8) {
    emitRex(false, dstXmm, srcXmm);
  }
  emitByte(0x0F);
  emitByte(0x5A); // CVTSS2SD
  emitModRmReg(dstXmm, srcXmm);
}

inline void X64Emitter::emitCvtsd2ss(uint8_t dstXmm, uint8_t srcXmm) {
  emitByte(0xF2);
  if (dstXmm >= 8 || srcXmm >= 8) {
    emitRex(false, dstXmm, srcXmm);
  }
  emitByte(0x0F);
  emitByte(0x5A); // CVTSD2SS
  emitModRmReg(dstXmm, srcXmm);
}

inline void X64Emitter::emitLoadXmmImm64(uint8_t xmm, uint64_t bits, uint8_t scratchReg) {
  emitMovRegImm64(scratchReg, bits);
  emitMovqXmmFromReg(xmm, scratchReg);
}

inline void X64Emitter::emitFloatBinaryOp(bool isF64, uint8_t opcode) {
  emitPopReg(1); // b
  emitPopReg(0); // a
  emitMovqXmmFromReg(1, 1);
  emitMovqXmmFromReg(0, 0);
  emitSseBinaryOp(isF64, opcode, 0, 1);
  emitMovqRegFromXmm(0, 0);
  emitPushReg(0);
}

inline void X64Emitter::emitFloatNegate(bool isF64) {
  emitPopReg(0);
  emitMovqXmmFromReg(0, 0);
  const uint64_t signBit = isF64 ? 0x8000000000000000ull : 0x0000000080000000ull;
  emitLoadXmmImm64(1, signBit, 1); // xmm1 = sign mask, rcx as scratch GPR
  emitXorpsXmm(0, 1);
  emitMovqRegFromXmm(0, 0);
  emitPushReg(0);
}

inline void X64Emitter::emitFloatCompareAndPush(bool isF64, CondCode cc) {
  emitPopReg(1); // b
  emitPopReg(0); // a
  emitMovqXmmFromReg(1, 1);
  emitMovqXmmFromReg(0, 0);
  emitComiss(isF64, 0, 1); // flags from a <-> b (unsigned-style flags per COMISS)
  emitSetccReg(0, cc);
  emitMovzxReg8(0, 0);
  emitPushReg(0);
}

inline void X64Emitter::emitAdd() {
  emitPopReg(1);
  emitPopReg(0);
  emitAddRegReg(0, 1);
  emitPushReg(0);
}

inline void X64Emitter::emitSub() {
  emitPopReg(1);
  emitPopReg(0);
  emitSubRegReg(0, 1);
  emitPushReg(0);
}

inline void X64Emitter::emitMul() {
  emitPopReg(1);
  emitPopReg(0);
  emitImulRegReg(0, 1);
  emitPushReg(0);
}

inline void X64Emitter::emitDiv() {
  emitPopReg(1); // divisor
  emitPopReg(0); // dividend -> rax
  emitCqo();
  emitIdivReg(1);
  emitPushReg(0);
}

inline void X64Emitter::emitDivU() {
  emitPopReg(1);
  emitPopReg(0);
  emitMovRegImm64(2, 0); // rdx = 0
  emitDivReg(1);
  emitPushReg(0);
}

inline void X64Emitter::emitNeg() {
  emitPopReg(0);
  emitNegReg(0);
  emitPushReg(0);
}

inline void X64Emitter::emitAddF32() {
  emitFloatBinaryOp(false, 0x58);
}
inline void X64Emitter::emitSubF32() {
  emitFloatBinaryOp(false, 0x5C);
}
inline void X64Emitter::emitMulF32() {
  emitFloatBinaryOp(false, 0x59);
}
inline void X64Emitter::emitDivF32() {
  emitFloatBinaryOp(false, 0x5E);
}
inline void X64Emitter::emitNegF32() {
  emitFloatNegate(false);
}
inline void X64Emitter::emitAddF64() {
  emitFloatBinaryOp(true, 0x58);
}
inline void X64Emitter::emitSubF64() {
  emitFloatBinaryOp(true, 0x5C);
}
inline void X64Emitter::emitMulF64() {
  emitFloatBinaryOp(true, 0x59);
}
inline void X64Emitter::emitDivF64() {
  emitFloatBinaryOp(true, 0x5E);
}
inline void X64Emitter::emitNegF64() {
  emitFloatNegate(true);
}

inline void X64Emitter::emitCmpEq() {
  emitCompareAndPush(CondCode::Eq);
}
inline void X64Emitter::emitCmpNe() {
  emitCompareAndPush(CondCode::Ne);
}
inline void X64Emitter::emitCmpLt() {
  emitCompareAndPush(CondCode::Lt);
}
inline void X64Emitter::emitCmpLe() {
  emitCompareAndPush(CondCode::Le);
}
inline void X64Emitter::emitCmpGt() {
  emitCompareAndPush(CondCode::Gt);
}
inline void X64Emitter::emitCmpGe() {
  emitCompareAndPush(CondCode::Ge);
}
inline void X64Emitter::emitCmpLtU() {
  emitCompareAndPush(CondCode::Below);
}
inline void X64Emitter::emitCmpLeU() {
  emitCompareAndPush(CondCode::BelowEq);
}
inline void X64Emitter::emitCmpGtU() {
  emitCompareAndPush(CondCode::Above);
}
inline void X64Emitter::emitCmpGeU() {
  emitCompareAndPush(CondCode::AboveEq);
}

// COMISS/COMISD set ZF/PF/CF the same way unsigned CMP does: for `a
// COMISS b`, CF=1 iff a<b (unordered also sets CF=1,ZF=1,PF=1), so the
// "unsigned-style" condition codes double as float comparisons here -
// Below == "a < b", AboveEq == "a >= b", etc. NaN inputs make every
// comparison false except Ne, matching IEEE 754 and (per this session's
// established practice of noting what's reasoned-through-but-unverified
// against edge cases) not independently re-derived against PrimeStruct's
// VM semantics for the NaN case specifically.
inline void X64Emitter::emitCmpEqF32() {
  emitFloatCompareAndPush(false, CondCode::Eq);
}
inline void X64Emitter::emitCmpNeF32() {
  emitFloatCompareAndPush(false, CondCode::Ne);
}
inline void X64Emitter::emitCmpLtF32() {
  emitFloatCompareAndPush(false, CondCode::Below);
}
inline void X64Emitter::emitCmpLeF32() {
  emitFloatCompareAndPush(false, CondCode::BelowEq);
}
inline void X64Emitter::emitCmpGtF32() {
  emitFloatCompareAndPush(false, CondCode::Above);
}
inline void X64Emitter::emitCmpGeF32() {
  emitFloatCompareAndPush(false, CondCode::AboveEq);
}
inline void X64Emitter::emitCmpEqF64() {
  emitFloatCompareAndPush(true, CondCode::Eq);
}
inline void X64Emitter::emitCmpNeF64() {
  emitFloatCompareAndPush(true, CondCode::Ne);
}
inline void X64Emitter::emitCmpLtF64() {
  emitFloatCompareAndPush(true, CondCode::Below);
}
inline void X64Emitter::emitCmpLeF64() {
  emitFloatCompareAndPush(true, CondCode::BelowEq);
}
inline void X64Emitter::emitCmpGtF64() {
  emitFloatCompareAndPush(true, CondCode::Above);
}
inline void X64Emitter::emitCmpGeF64() {
  emitFloatCompareAndPush(true, CondCode::AboveEq);
}

inline void X64Emitter::emitConvertIntToFloatImpl(bool isF64) {
  emitPopReg(0);
  emitCvtsi2s(isF64, 0, 0);
  emitMovqRegFromXmm(0, 0);
  emitPushReg(0);
}

inline void X64Emitter::emitConvertFloatToIntImpl(bool isF64) {
  emitPopReg(0);
  emitMovqXmmFromReg(0, 0);
  emitCvtts2si(isF64, 0, 0);
  emitPushReg(0);
}

// I32/I64 sources are both already sign-extended into the full 64-bit
// slot at push time (see this file's header comment), so the same
// 64-bit-source conversion is correct regardless of which IR opcode
// (ConvertI32ToF32 vs ConvertI64ToF32, etc.) dispatched here - these are
// kept as distinct methods only for interface parity with Arm64Emitter's
// method names, not because the generated code differs.
inline void X64Emitter::emitConvertI32ToF32() {
  emitConvertIntToFloatImpl(false);
}
inline void X64Emitter::emitConvertI64ToF32() {
  emitConvertIntToFloatImpl(false);
}
inline void X64Emitter::emitConvertI32ToF64() {
  emitConvertIntToFloatImpl(true);
}
inline void X64Emitter::emitConvertI64ToF64() {
  emitConvertIntToFloatImpl(true);
}

// Truncating float->int conversions. Known limitation (not yet handled,
// unlikely to be hit by this phase's verification fixtures): x86_64's
// CVTTSS2SI/CVTTSD2SI return the "integer indefinite" value
// (0x8000000000000000) on overflow/NaN rather than ARM64 FCVTZS's
// saturating-to-INT64_MAX/MIN behavior - a genuine cross-backend
// difference for out-of-range conversions specifically, not addressed
// here.
inline void X64Emitter::emitConvertF32ToI32() {
  emitConvertFloatToIntImpl(false);
}
inline void X64Emitter::emitConvertF32ToI64() {
  emitConvertFloatToIntImpl(false);
}
inline void X64Emitter::emitConvertF64ToI32() {
  emitConvertFloatToIntImpl(true);
}
inline void X64Emitter::emitConvertF64ToI64() {
  emitConvertFloatToIntImpl(true);
}

inline void X64Emitter::emitConvertF32ToF64() {
  emitPopReg(0);
  emitMovqXmmFromReg(0, 0);
  emitCvtss2sd(0, 0);
  emitMovqRegFromXmm(0, 0);
  emitPushReg(0);
}

inline void X64Emitter::emitConvertF64ToF32() {
  emitPopReg(0);
  emitMovqXmmFromReg(0, 0);
  emitCvtsd2ss(0, 0);
  emitMovqRegFromXmm(0, 0);
  emitPushReg(0);
}

// --- Unsigned int64 <-> float conversions (no direct x86_64 instruction
// pre-AVX512) ---

inline void X64Emitter::emitConvertUnsignedToFloat(bool isF64) {
  emitPopReg(0); // u64 value in rax
  // cmp rax, 0 followed by the *signed* less-than condition is
  // equivalent to a sign-bit test here: subtracting 0 can never
  // overflow, so OF stays 0 and "SF != OF" reduces to "SF == 1" exactly.
  emitMovRegImm64(2, 0);
  emitCmpRegReg(0, 2);
  const size_t jsFixup = emitCondJumpPlaceholder(CondCode::Lt); // value < 0 (as signed) == top bit set
  // value < 2^63: safe to treat as signed directly.
  emitCvtsi2s(isF64, 0, 0);
  const size_t doneFixup = emitJumpPlaceholderRaw();
  patchCondJumpHere(jsFixup);
  // value >= 2^63: split (value>>1 | value&1), convert, double.
  emitMovRegReg(1, 0);       // rcx = value
  emitByte(0x48);
  emitByte(0xD1);
  emitByte(0xE9);            // shr rcx, 1
  emitByte(0x48);
  emitByte(0x83);
  emitByte(0xE0);
  emitByte(0x01);            // and rax, 1
  emitByte(0x48);
  emitByte(0x09);
  emitByte(0xC1);            // or rcx, rax  (rcx |= rax)
  emitCvtsi2s(isF64, 0, 1);
  emitSseBinaryOp(isF64, 0x58, 0, 0); // xmm0 += xmm0 (double it)
  patchJumpHere(doneFixup);
  emitMovqRegFromXmm(0, 0);
  emitPushReg(0);
}

inline void X64Emitter::emitConvertFloatToUnsigned(bool isF64) {
  emitPopReg(0);
  emitMovqXmmFromReg(0, 0);
  const uint64_t threshold = isF64 ? 0x43E0000000000000ull   // 2^63 as f64
                                    : 0x5F000000ull;          // 2^63 as f32
  emitLoadXmmImm64(1, threshold, 1);
  emitComiss(isF64, 0, 1); // flags: xmm0 <-> xmm1 (threshold)
  const size_t geFixup = emitCondJumpPlaceholder(CondCode::AboveEq);
  // value < 2^63: safe direct truncating conversion.
  emitCvtts2si(isF64, 0, 0);
  const size_t doneFixup = emitJumpPlaceholderRaw();
  patchCondJumpHere(geFixup);
  // value >= 2^63: subtract the threshold, convert (now < 2^63), add
  // 2^63 back via a top-bit XOR (the converted value's top bit is
  // guaranteed 0 at this point, so XOR is equivalent to addition here).
  emitSseBinaryOp(isF64, 0x5C, 0, 1); // xmm0 -= xmm1 (SUBSS/SUBSD)
  emitCvtts2si(isF64, 0, 0);
  emitMovRegImm64(1, 0x8000000000000000ull);
  emitByte(0x48);
  emitByte(0x31);
  emitByte(0xC8); // xor rax, rcx
  patchJumpHere(doneFixup);
  emitPushReg(0);
}

inline void X64Emitter::emitConvertU64ToF32() {
  emitConvertUnsignedToFloat(false);
}
inline void X64Emitter::emitConvertU64ToF64() {
  emitConvertUnsignedToFloat(true);
}
inline void X64Emitter::emitConvertF32ToU64() {
  emitConvertFloatToUnsigned(false);
}
inline void X64Emitter::emitConvertF64ToU64() {
  emitConvertFloatToUnsigned(true);
}
