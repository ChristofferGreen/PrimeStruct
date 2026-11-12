#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"
#include "primec/ir_lowerer/IrLowererSharedTypes.h"

namespace primec::ir_lowerer {

// The f32 or f64 member of every float opcode pair the math lowering uses,
// chosen once from the argument kind.
struct FloatOpcodes {
  IrOpcode add;
  IrOpcode sub;
  IrOpcode mul;
  IrOpcode div;
  IrOpcode neg;
  IrOpcode cmpEq;
  IrOpcode cmpNe;
  IrOpcode cmpLt;
  IrOpcode cmpGt;
  IrOpcode fromI32;
  IrOpcode push;
};

bool isFloatValueKind(LocalInfo::ValueKind kind);
FloatOpcodes floatOpcodesFor(LocalInfo::ValueKind kind);

using FloatArgKindFn = ExprLocalsValueKindFn;

// Shared argument checks: exactly one argument, and a float one. On success
// `argKind` is the kind of that argument; on failure `error` is set.
bool checkUnaryFloatArgument(const Expr &expr,
                             const std::string &name,
                             const FloatArgKindFn &inferExprKind,
                             const LocalMap &locals,
                             LocalInfo::ValueKind &argKind,
                             std::string &error);

// True when the arguments mix signed and unsigned integer operands (an unsigned
// literal counts as unsigned).
bool argsMixSignedAndUnsigned(const std::vector<Expr> &args,
                              const FloatArgKindFn &inferExprKind,
                              const LocalMap &locals);

// Appends IR for the math builtins. Every emit call returns the emitter, so a
// straight-line sequence reads as one chain:
//   fe.load(x).load(y).mul().store(out);
class MathEmitter {
public:
  MathEmitter(LocalInfo::ValueKind kind, std::vector<IrInstruction> &out)
      : ops(floatOpcodesFor(kind)), kind_(kind), out_(out) {
  }
  // For integer-only sequences: the float opcode members must not be used.
  explicit MathEmitter(std::vector<IrInstruction> &out)
      : MathEmitter(LocalInfo::ValueKind::Unknown, out) {
  }

  FloatOpcodes ops;

  // Re-points the float opcode members and constants at another float kind.
  void useFloatKind(LocalInfo::ValueKind kind) {
    kind_ = kind;
    ops = floatOpcodesFor(kind);
  }
  bool isF64() const {
    return kind_ == LocalInfo::ValueKind::Float64;
  }
  size_t here() const {
    return out_.size();
  }

  MathEmitter &op(IrOpcode opcode, uint64_t imm = 0) {
    out_.push_back({opcode, imm});
    return *this;
  }
  MathEmitter &load(int32_t local) {
    return op(IrOpcode::LoadLocal, static_cast<uint64_t>(local));
  }
  MathEmitter &store(int32_t local) {
    return op(IrOpcode::StoreLocal, static_cast<uint64_t>(local));
  }
  MathEmitter &push(double value);
  MathEmitter &pushI32(uint64_t value) {
    return op(IrOpcode::PushI32, value);
  }
  MathEmitter &jumpTo(size_t target) {
    return op(IrOpcode::Jump, static_cast<uint64_t>(target));
  }
  MathEmitter &add() {
    return op(ops.add);
  }
  MathEmitter &sub() {
    return op(ops.sub);
  }
  MathEmitter &mul() {
    return op(ops.mul);
  }
  MathEmitter &div() {
    return op(ops.div);
  }
  MathEmitter &neg() {
    return op(ops.neg);
  }
  MathEmitter &cmpEq() {
    return op(ops.cmpEq);
  }
  MathEmitter &cmpNe() {
    return op(ops.cmpNe);
  }
  MathEmitter &cmpLt() {
    return op(ops.cmpLt);
  }
  MathEmitter &cmpGt() {
    return op(ops.cmpGt);
  }
  MathEmitter &fromI32() {
    return op(ops.fromI32);
  }

  // Jumps return the index of the emitted instruction for patch().
  size_t jump(size_t target = 0) {
    op(IrOpcode::Jump, static_cast<uint64_t>(target));
    return out_.size() - 1;
  }
  size_t jumpIfZero() {
    op(IrOpcode::JumpIfZero);
    return out_.size() - 1;
  }
  // Points the jump at `jumpIndex` to the next instruction to be emitted.
  void patch(size_t jumpIndex) {
    out_[jumpIndex].imm = static_cast<int32_t>(out_.size());
  }

private:
  LocalInfo::ValueKind kind_;
  std::vector<IrInstruction> &out_;
};

// Series shared by the exp/log/sqrt-based builtins. `fe` already has the float kind
// selected; each call allocates its own temporaries, then leaves the result in
// `outLocal` (NaN or -inf for out-of-domain inputs, as the builtins specify).
void emitExpSeries(MathEmitter &fe,
                   const Int32ProviderFn &allocTempLocal,
                   int32_t valueLocal,
                   int32_t outLocal);
void emitLogSeries(MathEmitter &fe,
                   const Int32ProviderFn &allocTempLocal,
                   int32_t valueLocal,
                   int32_t outLocal);
void emitSqrtSeries(MathEmitter &fe,
                    const Int32ProviderFn &allocTempLocal,
                    int32_t valueLocal,
                    int32_t outLocal);

} // namespace primec::ir_lowerer
