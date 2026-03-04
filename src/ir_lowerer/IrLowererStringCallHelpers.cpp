#include "IrLowererStringCallHelpers.h"

#include "primec/StringLiteral.h"

namespace primec::ir_lowerer {

StringCallEmitResult emitLiteralOrBindingStringCallValue(const Expr &arg,
                                                         const InternStringFn &internString,
                                                         const EmitInstructionFn &emitInstruction,
                                                         const LookupStringBindingFn &lookupBinding,
                                                         StringCallSource &sourceOut,
                                                         int32_t &stringIndexOut,
                                                         bool &argvCheckedOut,
                                                         std::string &error) {
  sourceOut = StringCallSource::None;
  stringIndexOut = -1;
  argvCheckedOut = true;

  if (arg.kind == Expr::Kind::StringLiteral) {
    ParsedStringLiteral parsed;
    std::string parseError;
    if (!parseStringLiteralToken(arg.stringValue, parsed, parseError)) {
      error = parseError.empty() ? "invalid string literal" : parseError;
      return StringCallEmitResult::Error;
    }
    if (parsed.encoding == StringEncoding::Ascii && !isAsciiText(parsed.decoded)) {
      error = "ascii string literal contains non-ASCII characters";
      return StringCallEmitResult::Error;
    }
    const int32_t index = internString(parsed.decoded);
    emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(index));
    sourceOut = StringCallSource::TableIndex;
    stringIndexOut = index;
    return StringCallEmitResult::Handled;
  }

  if (arg.kind == Expr::Kind::Name) {
    const StringBindingInfo binding = lookupBinding(arg.name);
    if (!binding.found) {
      error = "native backend does not know identifier: " + arg.name;
      return StringCallEmitResult::Error;
    }
    if (!binding.isString || binding.source == StringCallSource::None) {
      error = "native backend requires string arguments to use string literals, bindings, or entry args";
      return StringCallEmitResult::Error;
    }
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(binding.localIndex));
    sourceOut = binding.source;
    stringIndexOut = binding.stringIndex;
    argvCheckedOut = binding.argvChecked;
    return StringCallEmitResult::Handled;
  }

  return StringCallEmitResult::NotHandled;
}

StringCallEmitResult emitCallStringCallValue(const Expr &arg,
                                             const ResolveArrayAccessNameFn &resolveArrayAccessName,
                                             const IsStringCallEntryArgsNameFn &isEntryArgsName,
                                             const ResolveStringIndexOpsFn &resolveStringIndexOps,
                                             const EmitExprFn &emitExpr,
                                             const InferCallReturnsStringFn &inferCallReturnsString,
                                             const AllocTempLocalFn &allocTempLocal,
                                             const EmitInstructionFn &emitInstruction,
                                             const GetInstructionCountFn &getInstructionCount,
                                             const PatchInstructionImmFn &patchInstructionImm,
                                             const EmitArrayIndexOutOfBoundsFn &emitArrayIndexOutOfBounds,
                                             StringCallSource &sourceOut,
                                             bool &argvCheckedOut,
                                             std::string &error) {
  if (arg.kind != Expr::Kind::Call) {
    return StringCallEmitResult::NotHandled;
  }

  std::string accessName;
  if (resolveArrayAccessName(arg, accessName)) {
    if (arg.args.size() != 2) {
      error = accessName + " requires exactly two arguments";
      return StringCallEmitResult::Error;
    }
    if (!isEntryArgsName(arg.args.front())) {
      error = "native backend only supports entry argument indexing";
      return StringCallEmitResult::Error;
    }

    StringIndexOps indexOps;
    if (!resolveStringIndexOps(arg.args[1], accessName, indexOps, error)) {
      return StringCallEmitResult::Error;
    }

    const int32_t argvIndexLocal = allocTempLocal();
    if (!emitExpr(arg.args[1])) {
      return StringCallEmitResult::Error;
    }
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(argvIndexLocal));

    if (accessName == "at") {
      if (!indexOps.skipNegativeCheck) {
        emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal));
        emitInstruction(indexOps.pushZero, 0);
        emitInstruction(indexOps.cmpLt, 0);
        const size_t jumpNonNegative = getInstructionCount();
        emitInstruction(IrOpcode::JumpIfZero, 0);
        emitArrayIndexOutOfBounds();
        const size_t nonNegativeIndex = getInstructionCount();
        patchInstructionImm(jumpNonNegative, static_cast<int32_t>(nonNegativeIndex));
      }
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal));
      emitInstruction(IrOpcode::PushArgc, 0);
      emitInstruction(indexOps.cmpGe, 0);
      const size_t jumpInRange = getInstructionCount();
      emitInstruction(IrOpcode::JumpIfZero, 0);
      emitArrayIndexOutOfBounds();
      const size_t inRangeIndex = getInstructionCount();
      patchInstructionImm(jumpInRange, static_cast<int32_t>(inRangeIndex));
    }

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal));
    sourceOut = StringCallSource::ArgvIndex;
    argvCheckedOut = (accessName == "at");
    return StringCallEmitResult::Handled;
  }

  if (!inferCallReturnsString(arg)) {
    error = "native backend requires string arguments to use string literals, bindings, or entry args";
    return StringCallEmitResult::Error;
  }
  if (!emitExpr(arg)) {
    return StringCallEmitResult::Error;
  }
  sourceOut = StringCallSource::RuntimeIndex;
  return StringCallEmitResult::Handled;
}

} // namespace primec::ir_lowerer
