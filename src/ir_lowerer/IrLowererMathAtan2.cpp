#include "IrLowererMathEmitter.h"

namespace primec::ir_lowerer {

namespace {

constexpr double PiOver2 = 1.5707963267948966;
constexpr double PiOver2Low = 6.123233995736766e-17;
constexpr double PiOver4 = 0.7853981633974483;
constexpr double Pi = 3.141592653589793;
constexpr double PiLow = 1.2246467991473532e-16;

// Rational approximation of atan on [0, 0.66] (Cephes), with the argument
// reduction (t - 1) / (t + 1) around pi/4 above that. Accurate to about 2 ulp
// of f64 on [0, 1], which is all atan2 needs after it divides the smaller
// magnitude by the larger one.
void emitAtanUnit(MathEmitter &fe, const Int32ProviderFn &allocTempLocal, int32_t tLocal, int32_t outLocal) {
  const double numerator[] = {-8.750608600031904122785e-1,
                              -1.615753718733365076637e1,
                              -7.500855792314704667340e1,
                              -1.228866684490136173410e2,
                              -6.485021904942025371773e1};
  const double denominator[] = {2.485846490142306297962e1,
                                1.650270098316988542046e2,
                                4.328810604912902668951e2,
                                4.853903996359136964868e2,
                                1.945506571482613964425e2};
  int32_t tempArg = allocTempLocal();
  int32_t tempBase = allocTempLocal();
  int32_t tempExtra = allocTempLocal();
  int32_t tempSquare = allocTempLocal();
  int32_t tempNum = allocTempLocal();
  int32_t tempDen = allocTempLocal();
  int32_t tempRatio = allocTempLocal();

  fe.load(tLocal).push(0.66).cmpGt();
  size_t jumpSmall = fe.jumpIfZero();
  fe.load(tLocal).push(1.0).sub().load(tLocal).push(1.0).add().div().store(tempArg);
  fe.push(PiOver4).store(tempBase);
  fe.push(0.5 * PiOver2Low).store(tempExtra);
  size_t jumpReduced = fe.jump();
  fe.patch(jumpSmall);
  fe.load(tLocal).store(tempArg);
  fe.push(0.0).store(tempBase);
  fe.push(0.0).store(tempExtra);
  fe.patch(jumpReduced);

  fe.load(tempArg).load(tempArg).mul().store(tempSquare);
  fe.push(numerator[0]);
  for (size_t i = 1; i < 5; ++i) {
    fe.load(tempSquare).mul().push(numerator[i]).add();
  }
  fe.store(tempNum);
  fe.push(1.0);
  for (size_t i = 0; i < 5; ++i) {
    fe.load(tempSquare).mul().push(denominator[i]).add();
  }
  fe.store(tempDen);
  fe.load(tempSquare).load(tempNum).mul().load(tempDen).div().store(tempRatio);
  fe.load(tempArg).load(tempRatio).mul().load(tempArg).add().load(tempExtra).add().load(tempBase).add();
  fe.store(outLocal);
}

// Stores -1.0 in `outLocal` when the float in `valueLocal` has its sign bit set
// (below zero, or -0.0, where 1 / value is -inf) and 1.0 otherwise.
void emitSignFactor(MathEmitter &fe, int32_t valueLocal, int32_t outLocal) {
  fe.push(1.0).store(outLocal);
  fe.load(valueLocal).push(0.0).cmpLt();
  size_t jumpNotBelow = fe.jumpIfZero();
  fe.push(-1.0).store(outLocal);
  size_t jumpBelowEnd = fe.jump();
  fe.patch(jumpNotBelow);
  fe.load(valueLocal).push(0.0).cmpEq();
  size_t jumpNotZero = fe.jumpIfZero();
  fe.push(1.0).load(valueLocal).div().push(0.0).cmpLt();
  size_t jumpPositiveZero = fe.jumpIfZero();
  fe.push(-1.0).store(outLocal);
  fe.patch(jumpPositiveZero);
  fe.patch(jumpNotZero);
  fe.patch(jumpBelowEnd);
}

void emitMagnitude(MathEmitter &fe, int32_t valueLocal, int32_t outLocal) {
  fe.load(valueLocal).store(outLocal);
  fe.load(valueLocal).push(0.0).cmpLt();
  size_t jumpNotBelow = fe.jumpIfZero();
  fe.load(valueLocal).neg().store(outLocal);
  fe.patch(jumpNotBelow);
}

} // namespace

void emitAtan2(MathEmitter &fe,
               const Int32ProviderFn &allocTempLocal,
               int32_t yLocal,
               int32_t xLocal,
               int32_t outLocal) {
  int32_t tempAbsY = allocTempLocal();
  int32_t tempAbsX = allocTempLocal();
  int32_t tempSignY = allocTempLocal();
  int32_t tempSignX = allocTempLocal();
  int32_t tempRatio = allocTempLocal();
  int32_t tempAtan = allocTempLocal();
  int32_t tempAngle = allocTempLocal();

  // NaN in either argument is the result.
  fe.load(yLocal).load(yLocal).cmpNe();
  size_t jumpYNotNan = fe.jumpIfZero();
  fe.load(yLocal).store(outLocal);
  size_t jumpYNan = fe.jump();
  fe.patch(jumpYNotNan);
  fe.load(xLocal).load(xLocal).cmpNe();
  size_t jumpXNotNan = fe.jumpIfZero();
  fe.load(xLocal).store(outLocal);
  size_t jumpXNan = fe.jump();
  fe.patch(jumpXNotNan);

  emitMagnitude(fe, yLocal, tempAbsY);
  emitMagnitude(fe, xLocal, tempAbsX);
  emitSignFactor(fe, yLocal, tempSignY);
  emitSignFactor(fe, xLocal, tempSignX);

  // The smaller magnitude over the larger: 0 for (0, 0), 1 for (inf, inf).
  fe.load(tempAbsY).load(tempAbsX).cmpGt();
  size_t jumpYNotLarger = fe.jumpIfZero();
  fe.load(tempAbsX).load(tempAbsY).div().store(tempRatio);
  size_t jumpRatioEnd = fe.jump();
  fe.patch(jumpYNotLarger);
  fe.load(tempAbsY).load(tempAbsX).cmpEq();
  size_t jumpNotEqual = fe.jumpIfZero();
  fe.push(1.0).store(tempRatio);
  fe.load(tempAbsX).push(0.0).cmpEq();
  size_t jumpEqualNonZero = fe.jumpIfZero();
  fe.push(0.0).store(tempRatio);
  fe.patch(jumpEqualNonZero);
  size_t jumpEqualEnd = fe.jump();
  fe.patch(jumpNotEqual);
  fe.load(tempAbsY).load(tempAbsX).div().store(tempRatio);
  fe.patch(jumpEqualEnd);
  fe.patch(jumpRatioEnd);

  emitAtanUnit(fe, allocTempLocal, tempRatio, tempAtan);

  fe.load(tempAbsY).load(tempAbsX).cmpGt();
  size_t jumpAngleDirect = fe.jumpIfZero();
  fe.push(PiOver2).load(tempAtan).push(PiOver2Low).sub().sub().store(tempAngle);
  size_t jumpAngleEnd = fe.jump();
  fe.patch(jumpAngleDirect);
  fe.load(tempAtan).store(tempAngle);
  fe.patch(jumpAngleEnd);

  fe.load(tempSignX).push(0.0).cmpLt();
  size_t jumpRight = fe.jumpIfZero();
  fe.push(Pi).load(tempAngle).push(PiLow).sub().sub().store(tempAngle);
  fe.patch(jumpRight);

  fe.load(tempAngle).load(tempSignY).mul().store(outLocal);
  fe.patch(jumpYNan);
  fe.patch(jumpXNan);
}

} // namespace primec::ir_lowerer
