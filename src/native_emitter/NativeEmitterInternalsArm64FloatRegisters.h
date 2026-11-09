#pragma once

// The float registers of the arm64 register allocator (NativeEmitterInternalsArm64RegAlloc.h,
// docs/OptimizingBackendsPlan.md 12.14): its pool, how the save slots number them, and the
// `fmov` immediate encoder.
// Included inside namespace primec::native_emitter by NativeEmitterInternals.h.

// Float registers, d8-d15 first: AAPCS64 has C functions keep their low 64 bits, so no template,
// host call or heap routine changes them and they are never saved around one. d16-d31 and d2-d7
// follow, saved around templates while live. Across a call every live float register is saved,
// as every general one is (allocated callees use the same pool). Float compares before a
// JumpIfZero are fused into `fcmp` + `b.cond`.
inline const RegAllocFloatRegisters Arm64RegAllocFloatRegisters = {
    {8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
     23, 24, 25, 26, 27, 28, 29, 30, 31, 2,  3,  4,  5,  6,  7},
    true};
// regAllocRegistersLiveAcross and the save slots number float register dN as 32 + N.
inline constexpr uint8_t Arm64RegAllocFloatBase = 32;

// The imm8 of `fmov dN, #imm` (or `fmov sN, #imm` with `isDouble` false) that builds `bits`, or
// -1 when no imm8 does (VFPExpandImm).
inline int arm64FloatImmediate(uint64_t bits, bool isDouble) {
  for (uint32_t imm8 = 0; imm8 < 256; ++imm8) {
    const uint64_t sign = imm8 >> 7;
    const uint64_t b = (imm8 >> 6) & 1u;
    const uint64_t cd = (imm8 >> 4) & 3u;
    const uint64_t fraction = imm8 & 0xFu;
    uint64_t expanded = 0;
    if (isDouble) {
      const uint64_t exponent = ((b ^ 1u) << 10) | ((b != 0 ? 0xFFu : 0u) << 2) | cd;
      expanded = (sign << 63) | (exponent << 52) | (fraction << 48);
    } else {
      const uint64_t exponent = ((b ^ 1u) << 7) | ((b != 0 ? 0x1Fu : 0u) << 2) | cd;
      expanded = (sign << 31) | (exponent << 23) | (fraction << 19);
    }
    if (expanded == bits) {
      return static_cast<int>(imm8);
    }
  }
  return -1;
}
