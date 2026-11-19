#include "WasmEmitterInternal.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace primec {

namespace {

// A small assembler for the helper functions below: every method appends one wasm instruction and returns the builder,
// so a sequence reads as the postfix code it emits.
class Asm {
public:
  explicit Asm(std::vector<uint8_t> &out) : out_(out) {
  }

  Asm &get(uint32_t local) {
    return index(0x20, local);
  }
  Asm &set(uint32_t local) {
    return index(0x21, local);
  }
  Asm &tee(uint32_t local) {
    return index(0x22, local);
  }
  Asm &i64(int64_t value) {
    out_.push_back(0x42);
    appendS64Leb(value, out_);
    return *this;
  }
  Asm &f64(uint64_t bits) {
    out_.push_back(0x44);
    appendFixedU64Le(bits, out_);
    return *this;
  }
  Asm &op(uint8_t byte) {
    out_.push_back(byte);
    return *this;
  }

  Asm &ifVoid() {
    out_.push_back(0x04);
    out_.push_back(0x40);
    return *this;
  }
  Asm &elseBranch() {
    return op(0x05);
  }
  Asm &end() {
    return op(0x0b);
  }
  Asm &ret() {
    return op(0x0f);
  }

  // i64 operations
  Asm &eqz() {
    return op(0x50);
  }
  Asm &eq() {
    return op(0x51);
  }
  Asm &ne() {
    return op(0x52);
  }
  Asm &ltS() {
    return op(0x53);
  }
  Asm &ltU() {
    return op(0x54);
  }
  Asm &gtS() {
    return op(0x55);
  }
  Asm &gtU() {
    return op(0x56);
  }
  Asm &geS() {
    return op(0x59);
  }
  Asm &clz() {
    return op(0x79);
  }
  Asm &add() {
    return op(0x7c);
  }
  Asm &sub() {
    return op(0x7d);
  }
  Asm &mul() {
    return op(0x7e);
  }
  Asm &and64() {
    return op(0x83);
  }
  Asm &or64() {
    return op(0x84);
  }
  Asm &xor64() {
    return op(0x85);
  }
  Asm &shl() {
    return op(0x86);
  }
  Asm &shrU() {
    return op(0x88);
  }
  // i32 operations on comparison results
  Asm &and32() {
    return op(0x71);
  }
  Asm &or32() {
    return op(0x72);
  }
  Asm &select() {
    return op(0x1b);
  }
  Asm &extendU() {
    return op(0xad);
  }
  Asm &bitsOfF64() {
    return op(0xbd);
  }
  Asm &f64OfBits() {
    return op(0xbf);
  }

private:
  Asm &index(uint8_t byte, uint32_t local) {
    out_.push_back(byte);
    appendU32Leb(local, out_);
    return *this;
  }

  std::vector<uint8_t> &out_;
};

constexpr uint64_t QuietNan = 0x7ff8000000000000ull;
constexpr uint64_t Infinity = 0x7ff0000000000000ull;
constexpr uint64_t FractionMask = 0x000fffffffffffffull;
constexpr uint64_t ImplicitBit = 0x0010000000000000ull;

// `a * b + c` of f32 with one rounding. The product of two f32 is exact in an f64, and so is the TwoSum error of adding
// the addend; the f64 sum is made odd when it is inexact (round to odd) so that rounding it to f32 afterwards equals
// rounding the exact result once (53 >= 2 * 24 + 2 bits make the two roundings agree).
WasmCodeBody buildFma32Body() {
  // Locals: 0..2 the f32 parameters; 3 product, 4 addend, 5 sum, 6 the sum minus the product, 7 the error (f64);
  // 8 the bits of the sum, 9 the bits of the error (i64).
  constexpr uint32_t Product = 3, Addend = 4, Sum = 5, Virtual = 6, Error = 7, SumBits = 8,
                     ErrorBits = 9;
  WasmCodeBody body;
  appendU32Leb(2, body.localDecls);
  appendU32Leb(5, body.localDecls);
  body.localDecls.push_back(WasmTypeF64);
  appendU32Leb(2, body.localDecls);
  body.localDecls.push_back(WasmTypeI64);

  Asm a(body.instructions);
  a.get(0).op(0xbb).get(1).op(0xbb).op(0xa2).set(Product);
  a.get(2).op(0xbb).set(Addend);
  a.get(Product).get(Addend).op(0xa0).set(Sum);
  a.get(Sum).get(Product).op(0xa1).set(Virtual);
  // error = (product - (sum - virtual)) + (addend - virtual)
  a.get(Product).get(Sum).get(Virtual).op(0xa1).op(0xa1);
  a.get(Addend).get(Virtual).op(0xa1).op(0xa0).set(Error);
  // an inexact, finite sum (a NaN error means an infinite or NaN sum): force the last bit odd, toward the error
  a.get(Error).f64(0).op(0x62).get(Error).get(Error).op(0x61).and32().ifVoid();
  a.get(Sum).bitsOfF64().set(SumBits);
  a.get(SumBits).i64(1).and64().eqz().ifVoid();
  a.get(Error).bitsOfF64().set(ErrorBits);
  // same sign: the magnitude grows toward the error, opposite sign: it shrinks
  a.get(SumBits)
      .i64(1)
      .i64(-1)
      .get(SumBits)
      .get(ErrorBits)
      .xor64()
      .i64(63)
      .shrU()
      .eqz()
      .select()
      .add()
      .set(SumBits);
  a.get(SumBits).f64OfBits().set(Sum);
  a.end().end();
  a.get(Sum).op(0xb6).end();
  return body;
}

// `a * b + c` of f64 with one rounding, on the bit patterns: the exact 128-bit product of the two significands, the
// addend aligned to it with a sticky bit, one addition or subtraction, and one round to nearest even (subnormals,
// infinities and NaNs included).
WasmCodeBody buildFma64Body() {
  // Locals 0..2 are the f64 parameters; the rest are i64.
  enum : uint32_t {
    BitsA = 3,
    BitsB,
    BitsC,
    SignA,
    SignB,
    SignC,
    SignP,
    ExpFieldA,
    ExpFieldB,
    ExpFieldC,
    FractionA,
    FractionB,
    FractionC,
    SigA,
    SigB,
    SigC,
    ExpA,
    ExpB,
    ExpC,
    T0,
    T1,
    T2,
    T3,
    ProdHi,
    ProdLo,
    Xh,
    Xl,
    Xe,
    Xs,
    Yh,
    Yl,
    Ye,
    Ys,
    Sticky,
    BiasedExp,
    Extra,
    Shift,
    Mant,
    Guard,
    Rest,
    LocalCount
  };
  WasmCodeBody body;
  appendU32Leb(1, body.localDecls);
  appendU32Leb(LocalCount - BitsA, body.localDecls);
  body.localDecls.push_back(WasmTypeI64);

  Asm a(body.instructions);
  const auto bitsReturn = [&](Asm &asmRef) { asmRef.f64OfBits().ret(); };
  const auto constReturn = [&](uint64_t bits) {
    a.i64(static_cast<int64_t>(bits));
    bitsReturn(a);
  };

  a.get(0).bitsOfF64().set(BitsA);
  a.get(1).bitsOfF64().set(BitsB);
  a.get(2).bitsOfF64().set(BitsC);
  const uint32_t bits[3] = {BitsA, BitsB, BitsC};
  const uint32_t signs[3] = {SignA, SignB, SignC};
  const uint32_t expFields[3] = {ExpFieldA, ExpFieldB, ExpFieldC};
  const uint32_t fractions[3] = {FractionA, FractionB, FractionC};
  for (int i = 0; i < 3; ++i) {
    a.get(bits[i]).i64(63).shrU().set(signs[i]);
    a.get(bits[i]).i64(52).shrU().i64(0x7ff).and64().set(expFields[i]);
    a.get(bits[i]).i64(static_cast<int64_t>(FractionMask)).and64().set(fractions[i]);
  }
  a.get(SignA).get(SignB).xor64().set(SignP);

  // A NaN operand gives a NaN.
  for (int i = 0; i < 3; ++i) {
    a.get(expFields[i]).i64(0x7ff).eq().get(fractions[i]).i64(0).ne().and32();
    if (i > 0) {
      a.or32();
    }
  }
  a.ifVoid();
  constReturn(QuietNan);
  a.end();

  // An infinite factor: inf * 0 is NaN, and an infinite product meeting an infinite addend of the other sign is NaN.
  a.get(ExpFieldA).i64(0x7ff).eq().get(ExpFieldB).i64(0x7ff).eq().or32().ifVoid();
  a.get(BitsA).i64(1).shl().eqz().get(BitsB).i64(1).shl().eqz().or32().ifVoid();
  constReturn(QuietNan);
  a.end();
  a.get(ExpFieldC).i64(0x7ff).eq().get(SignC).get(SignP).ne().and32().ifVoid();
  constReturn(QuietNan);
  a.end();
  a.get(SignP).i64(63).shl().i64(static_cast<int64_t>(Infinity)).or64();
  bitsReturn(a);
  a.end();
  // A finite product and an infinite addend: the addend.
  a.get(ExpFieldC).i64(0x7ff).eq().ifVoid();
  a.get(BitsC);
  bitsReturn(a);
  a.end();
  // A zero factor: the product is a signed zero, so the result is the addend, or a zero (negative only when both
  // are negative) when the addend is zero too.
  a.get(BitsA).i64(1).shl().eqz().get(BitsB).i64(1).shl().eqz().or32().ifVoid();
  a.get(BitsC).i64(1).shl().eqz().ifVoid();
  a.get(SignP).get(SignC).and64().i64(63).shl();
  bitsReturn(a);
  a.end();
  a.get(BitsC);
  bitsReturn(a);
  a.end();

  // Significands with the leading bit at bit 52 (subnormals shifted up) and unbiased exponents:
  // value = significand * 2^(exponent - 52).
  const uint32_t significands[3] = {SigA, SigB, SigC};
  const uint32_t exponents[3] = {ExpA, ExpB, ExpC};
  for (int i = 0; i < 3; ++i) {
    if (i == 2) {
      // A zero addend has no significand and an exponent below every product.
      a.get(BitsC).i64(1).shl().eqz().ifVoid();
      a.i64(0).set(SigC).i64(-100000).set(ExpC);
      a.elseBranch();
    }
    a.get(expFields[i]).i64(0).ne().ifVoid();
    a.get(fractions[i]).i64(static_cast<int64_t>(ImplicitBit)).or64().set(significands[i]);
    a.get(expFields[i]).i64(1023).sub().set(exponents[i]);
    a.elseBranch();
    a.get(fractions[i]).clz().i64(11).sub().set(T0);
    a.get(fractions[i]).get(T0).shl().set(significands[i]);
    a.i64(-1022).get(T0).sub().set(exponents[i]);
    a.end();
    if (i == 2) {
      a.end();
    }
  }

  // The exact product of the two 53-bit significands as 32-bit limbs: hh*2^64 + (lh + hl)*2^32 + ll.
  a.get(SigA).i64(0xffffffff).and64().get(SigB).i64(0xffffffff).and64().mul().set(T0); // ll
  a.get(SigA).i64(0xffffffff).and64().get(SigB).i64(32).shrU().mul().set(T1);          // lh
  a.get(SigA).i64(32).shrU().get(SigB).i64(0xffffffff).and64().mul().set(T2);          // hl
  a.get(SigA).i64(32).shrU().get(SigB).i64(32).shrU().mul().set(T3);                   // hh
  a.get(T1).get(T2).add().get(T0).i64(32).shrU().add().set(T1);                        // mid
  a.get(T1).i64(32).shl().get(T0).i64(0xffffffff).and64().or64().set(ProdLo);
  a.get(T3).get(T1).i64(32).shrU().add().set(ProdHi);
  // Normalize it to bit 127 (the product is below 2^106, so the high word is not zero).
  a.get(ProdHi).clz().set(T0);
  a.get(ProdHi).get(T0).shl();
  a.get(ProdLo).i64(1).shrU().i64(63).get(T0).sub().shrU().or64().set(Xh);
  a.get(ProdLo).get(T0).shl().set(Xl);
  a.get(ExpA).get(ExpB).add().i64(23).add().get(T0).sub().set(Xe);
  a.get(SignP).set(Xs);
  a.get(SigC).i64(11).shl().set(Yh);
  a.i64(0).set(Yl);
  a.get(ExpC).set(Ye);
  a.get(SignC).set(Ys);

  // The operand with the larger magnitude is X: swap when the addend has the larger exponent, or the same exponent, the
  // opposite sign and the larger significand.
  const auto swap = [&](uint32_t x, uint32_t y) { a.get(x).set(T0).get(y).set(x).get(T0).set(y); };
  a.get(Ye).get(Xe).gtS();
  a.get(Ye).get(Xe).eq().get(Xs).get(Ys).ne().and32();
  a.get(Yh).get(Xh).gtU().get(Yh).get(Xh).eq().get(Yl).get(Xl).gtU().and32().or32().and32();
  a.or32().ifVoid();
  swap(Xh, Yh);
  swap(Xl, Yl);
  swap(Xe, Ye);
  swap(Xs, Ys);
  a.end();

  // Align Y to X, keeping the bits shifted out as a sticky flag.
  a.i64(0).set(Sticky);
  a.get(Xe).get(Ye).sub().set(T0); // the shift distance, never negative
  a.get(T0).i64(128).geS().ifVoid();
  a.get(Yh).get(Yl).or64().i64(0).ne().extendU().set(Sticky);
  a.i64(0).set(Yh).i64(0).set(Yl);
  a.elseBranch();
  a.get(T0).i64(64).geS().ifVoid();
  a.get(T0).i64(64).sub().set(T1);
  a.get(Yl)
      .i64(0)
      .ne()
      .get(Yh)
      .i64(1)
      .get(T1)
      .shl()
      .i64(1)
      .sub()
      .and64()
      .i64(0)
      .ne()
      .or32()
      .extendU()
      .set(Sticky);
  a.get(Yh).get(T1).shrU().set(Yl);
  a.i64(0).set(Yh);
  a.elseBranch();
  a.get(T0).i64(0).gtS().ifVoid();
  a.get(Yl).i64(1).get(T0).shl().i64(1).sub().and64().i64(0).ne().extendU().set(Sticky);
  a.get(Yl).get(T0).shrU().get(Yh).i64(64).get(T0).sub().shl().or64().set(Yl);
  a.get(Yh).get(T0).shrU().set(Yh);
  a.end();
  a.end();
  a.end();

  // Add or subtract the magnitudes (X is not smaller than Y).
  a.get(Xs).get(Ys).eq().ifVoid();
  a.get(Xl).get(Yl).add().set(T0);
  a.get(T0).get(Xl).ltU().extendU().set(T1); // carry out of the low word
  a.get(T0).set(Xl);
  a.get(Xh).get(Yh).add().set(T2);
  a.get(T2).get(Xh).ltU().extendU().set(T3);
  a.get(T2).get(T1).add().set(Xh);
  a.get(Xh).get(T2).ltU().extendU().get(T3).or64().set(T3); // carry out of the high word
  a.get(T3).i64(0).ne().ifVoid();
  a.get(Sticky).get(Xl).i64(1).and64().or64().set(Sticky);
  a.get(Xl).i64(1).shrU().get(Xh).i64(63).shl().or64().set(Xl);
  a.get(Xh).i64(1).shrU().i64(static_cast<int64_t>(0x8000000000000000ull)).or64().set(Xh);
  a.get(Xe).i64(1).add().set(Xe);
  a.end();
  a.elseBranch();
  a.get(Xl).get(Yl).sub().set(T0);
  a.get(Xl).get(Yl).ltU().extendU().set(T1); // borrow
  a.get(T0).set(Xl);
  a.get(Xh).get(Yh).sub().get(T1).sub().set(Xh);
  // A discarded fraction makes the true difference a little below the integer one: take one unit off, and keep the
  // sticky flag.
  a.get(Sticky).i64(0).ne().ifVoid();
  a.get(Xl).eqz().extendU().set(T1);
  a.get(Xl).i64(1).sub().set(Xl);
  a.get(Xh).get(T1).sub().set(Xh);
  a.end();
  a.end();

  // An exact zero (a cancellation) is +0.
  a.get(Xh).get(Xl).or64().eqz().ifVoid();
  constReturn(0);
  a.end();

  // Normalize the leading bit to bit 127.
  a.get(Xh).i64(0).ne().ifVoid();
  a.get(Xh).clz().set(T0);
  a.elseBranch();
  a.get(Xl).clz().i64(64).add().set(T0);
  a.end();
  a.get(T0).i64(0).ne().ifVoid();
  a.get(T0).i64(64).geS().ifVoid();
  a.get(Xl).get(T0).i64(64).sub().shl().set(Xh);
  a.i64(0).set(Xl);
  a.elseBranch();
  a.get(Xh).get(T0).shl().get(Xl).i64(1).shrU().i64(63).get(T0).sub().shrU().or64().set(Xh);
  a.get(Xl).get(T0).shl().set(Xl);
  a.end();
  a.get(Xe).get(T0).sub().set(Xe);
  a.end();

  // Round to 53 bits (fewer for a subnormal result), to nearest even.
  a.get(Xe).i64(1023).add().set(BiasedExp);
  a.get(BiasedExp).i64(2047).geS().ifVoid();
  a.get(Xs).i64(63).shl().i64(static_cast<int64_t>(Infinity)).or64();
  bitsReturn(a);
  a.end();
  a.i64(0).set(Extra);
  a.get(BiasedExp).i64(1).ltS().ifVoid();
  a.i64(1).get(BiasedExp).sub().set(Extra);
  a.i64(1).set(BiasedExp);
  a.end();
  // Far below the smallest subnormal: a zero.
  a.get(Extra).i64(54).geS().ifVoid();
  a.get(Xs).i64(63).shl();
  bitsReturn(a);
  a.end();
  // T0 is the shift of the high word that leaves the kept bits: 75 + Extra bits are dropped in all, 64 of them the low
  // word.
  a.i64(11).get(Extra).add().set(T0);
  a.i64(0).set(Mant);
  a.get(T0).i64(64).ltU().ifVoid();
  a.get(Xh).get(T0).shrU().set(Mant);
  a.end();
  a.get(Xh).get(T0).i64(1).sub().shrU().i64(1).and64().set(Guard);
  a.get(Xh)
      .i64(1)
      .get(T0)
      .i64(1)
      .sub()
      .shl()
      .i64(1)
      .sub()
      .and64()
      .get(Xl)
      .or64()
      .get(Sticky)
      .or64()
      .i64(0)
      .ne()
      .extendU()
      .set(Rest);
  a.get(Guard).get(Rest).get(Mant).i64(1).and64().or64().and64().get(Mant).add().set(Mant);
  a.get(BiasedExp)
      .i64(1)
      .sub()
      .i64(52)
      .shl()
      .get(Mant)
      .add()
      .get(Xs)
      .i64(63)
      .shl()
      .or64()
      .f64OfBits()
      .end();
  return body;
}

} // namespace

WasmCodeBody buildWasmFmaHelperBody(bool isF64) {
  return isF64 ? buildFma64Body() : buildFma32Body();
}

} // namespace primec
