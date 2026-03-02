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

} // namespace primec::ir_lowerer
