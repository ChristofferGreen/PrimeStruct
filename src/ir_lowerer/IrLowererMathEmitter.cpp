#include "IrLowererMathEmitter.h"

#include <cstring>

namespace primec::ir_lowerer {

bool isFloatValueKind(LocalInfo::ValueKind kind) {
  return kind == LocalInfo::ValueKind::Float32 || kind == LocalInfo::ValueKind::Float64;
}

FloatOpcodes floatOpcodesFor(LocalInfo::ValueKind kind) {
  if (kind == LocalInfo::ValueKind::Float64) {
    return {IrOpcode::AddF64,
            IrOpcode::SubF64,
            IrOpcode::MulF64,
            IrOpcode::DivF64,
            IrOpcode::NegF64,
            IrOpcode::CmpEqF64,
            IrOpcode::CmpNeF64,
            IrOpcode::CmpLtF64,
            IrOpcode::CmpGtF64,
            IrOpcode::ConvertI32ToF64,
            IrOpcode::PushF64};
  }
  return {IrOpcode::AddF32,
          IrOpcode::SubF32,
          IrOpcode::MulF32,
          IrOpcode::DivF32,
          IrOpcode::NegF32,
          IrOpcode::CmpEqF32,
          IrOpcode::CmpNeF32,
          IrOpcode::CmpLtF32,
          IrOpcode::CmpGtF32,
          IrOpcode::ConvertI32ToF32,
          IrOpcode::PushF32};
}

bool checkUnaryFloatArgument(const Expr &expr,
                             const std::string &name,
                             const FloatArgKindFn &inferExprKind,
                             const LocalMap &locals,
                             LocalInfo::ValueKind &argKind,
                             std::string &error) {
  if (expr.args.size() != 1) {
    error = name + " requires exactly one argument";
    return false;
  }
  argKind = inferExprKind(expr.args.front(), locals);
  if (!isFloatValueKind(argKind)) {
    error = name + " requires float argument";
    return false;
  }
  return true;
}

bool argsMixSignedAndUnsigned(const std::vector<Expr> &args,
                              const FloatArgKindFn &inferExprKind,
                              const LocalMap &locals) {
  bool sawUnsigned = false;
  bool sawSigned = false;
  for (const auto &arg : args) {
    LocalInfo::ValueKind kind = inferExprKind(arg, locals);
    if (arg.kind == Expr::Kind::Literal && arg.isUnsigned) {
      sawUnsigned = true;
    }
    if (kind == LocalInfo::ValueKind::UInt64 || kind == LocalInfo::ValueKind::UInt8) {
      sawUnsigned = true;
    } else if (kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Int64) {
      sawSigned = true;
    }
  }
  return sawUnsigned && sawSigned;
}

MathEmitter &MathEmitter::push(double value) {
  if (isF64()) {
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return op(IrOpcode::PushF64, bits);
  }
  float f32 = static_cast<float>(value);
  uint32_t bits = 0;
  std::memcpy(&bits, &f32, sizeof(bits));
  return op(IrOpcode::PushF32, static_cast<uint64_t>(bits));
}

void emitExpSeries(MathEmitter &fe,
                   const Int32ProviderFn &allocTempLocal,
                   int32_t valueLocal,
                   int32_t outLocal) {
  int32_t tempTerm = allocTempLocal();
  int32_t tempSum = allocTempLocal();
  int32_t tempIter = allocTempLocal();

  fe.push(1.0).store(tempTerm).push(1.0).store(tempSum).pushI32(1).store(tempIter);

  const int32_t iterations = fe.isF64() ? 12 : 10;
  const size_t loopStart = fe.here();
  fe.load(tempIter).pushI32(static_cast<uint64_t>(iterations + 1)).op(IrOpcode::CmpLtI32);
  size_t jumpEndLoop = fe.jumpIfZero();

  fe.load(tempTerm).load(valueLocal).mul().store(tempTerm);

  fe.load(tempTerm).load(tempIter).fromI32().div().store(tempTerm);

  fe.load(tempSum).load(tempTerm).add().store(tempSum);

  fe.load(tempIter).pushI32(1).op(IrOpcode::AddI32).store(tempIter).jumpTo(loopStart);

  fe.patch(jumpEndLoop);
  fe.load(tempSum).store(outLocal);
}

void emitLogSeries(MathEmitter &fe,
                   const Int32ProviderFn &allocTempLocal,
                   int32_t valueLocal,
                   int32_t outLocal) {
  int32_t tempZ = allocTempLocal();
  int32_t tempZ2 = allocTempLocal();
  int32_t tempTerm = allocTempLocal();
  int32_t tempSum = allocTempLocal();

  fe.load(valueLocal).push(0.0).cmpLt();
  size_t jumpIfNotNegative = fe.jumpIfZero();
  fe.push(0.0).push(0.0).div().store(outLocal);
  size_t jumpToEnd = fe.jump();

  fe.patch(jumpIfNotNegative);

  fe.load(valueLocal).push(0.0).cmpEq();
  size_t jumpIfNotZero = fe.jumpIfZero();
  fe.push(-1.0).push(0.0).div().store(outLocal);
  size_t jumpToEndZero = fe.jump();

  fe.patch(jumpIfNotZero);

  fe.load(valueLocal).push(1.0).sub().load(valueLocal).push(1.0).add().div().store(tempZ);

  fe.load(tempZ).load(tempZ).mul().store(tempZ2);

  fe.load(tempZ).store(tempTerm).load(tempZ).store(tempSum);

  auto appendSeries = [&](double divisor) {
    fe.load(tempTerm)
        .load(tempZ2)
        .mul()
        .store(tempTerm)
        .load(tempSum)
        .load(tempTerm)
        .push(1.0 / divisor)
        .mul()
        .add()
        .store(tempSum);
  };

  appendSeries(3.0);
  appendSeries(5.0);
  appendSeries(7.0);
  appendSeries(9.0);

  fe.load(tempSum).push(2.0).mul().store(outLocal);

  fe.patch(jumpToEnd);
  fe.patch(jumpToEndZero);
}

void emitSqrtSeries(MathEmitter &fe,
                    const Int32ProviderFn &allocTempLocal,
                    int32_t valueLocal,
                    int32_t outLocal) {
  int32_t tempX = allocTempLocal();
  int32_t tempIter = allocTempLocal();

  fe.load(valueLocal).push(0.0).cmpEq();
  size_t jumpIfNotZero = fe.jumpIfZero();
  fe.push(0.0).store(outLocal);
  size_t jumpToEndZero = fe.jump();

  fe.patch(jumpIfNotZero);
  fe.load(valueLocal).push(0.0).cmpLt();
  size_t jumpIfNonNegative = fe.jumpIfZero();
  fe.push(0.0).push(0.0).div().store(outLocal);
  size_t jumpToEndNegative = fe.jump();

  fe.patch(jumpIfNonNegative);

  fe.load(valueLocal).store(tempX).op(IrOpcode::PushI32).store(tempIter);

  const int32_t iterations = fe.isF64() ? 8 : 6;
  const size_t loopStart = fe.here();
  fe.load(tempIter).pushI32(static_cast<uint64_t>(iterations)).op(IrOpcode::CmpLtI32);
  size_t jumpEndLoop = fe.jumpIfZero();

  fe.load(tempX).load(valueLocal).load(tempX).div().add().push(0.5).mul().store(tempX);

  fe.load(tempIter).pushI32(1).op(IrOpcode::AddI32).store(tempIter).jumpTo(loopStart);

  fe.patch(jumpEndLoop);
  fe.load(tempX).store(outLocal);

  fe.patch(jumpToEndZero);
  fe.patch(jumpToEndNegative);
}

} // namespace primec::ir_lowerer
