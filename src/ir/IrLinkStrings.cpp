#include "primec/ir/IrLinkStrings.h"

#include "primec/ir/IrOpcodeTable.h"

#include <optional>

namespace primec {
namespace {

// How many operand-stack slots lie above the string index when the instruction runs; nullopt for an opcode that
// does not read a dynamic string index.
std::optional<size_t> dynamicStringDepth(IrOpcode op) {
  switch (op) {
  case IrOpcode::PrintStringDynamic:
  case IrOpcode::LoadStringLength:
  case IrOpcode::FileOpenReadDynamic:
  case IrOpcode::FileOpenWriteDynamic:
  case IrOpcode::FileOpenAppendDynamic:
  case IrOpcode::FileWriteStringDynamic:
    return 0;
  case IrOpcode::LoadStringByteDynamic:
    return 1;
  default:
    return std::nullopt;
  }
}

bool pushesOneSlot(IrOpcode op) {
  return op == IrOpcode::PushI32 || op == IrOpcode::PushI64 || op == IrOpcode::LoadLocal;
}

// The PushI32 or PushI64 that put the string index `depth` slots below the top before instruction `consumer`, when
// only single-slot pushes lie between them; nullopt otherwise.
std::optional<size_t>
feedingPush(const std::vector<IrInstruction> &code, size_t consumer, size_t depth) {
  size_t at = consumer;
  for (size_t step = 0; step < depth; ++step) {
    if (at == 0 || !pushesOneSlot(code[at - 1].op)) {
      return std::nullopt;
    }
    --at;
  }
  if (at == 0 || (code[at - 1].op != IrOpcode::PushI32 && code[at - 1].op != IrOpcode::PushI64)) {
    return std::nullopt;
  }
  return at - 1;
}

const char *opcodeName(IrOpcode op) {
  const IrOpcodeInfo *info = irOpcodeInfo(op);
  return info == nullptr ? "?" : info->name;
}

} // namespace

IrStringPushes findIrStringLiteralPushes(const IrModule &module, const IrFunction &function) {
  IrStringPushes result;
  const std::vector<IrInstruction> &code = function.instructions;
  result.isStringPush.assign(code.size(), false);
  const auto note = [&](size_t consumer, std::optional<size_t> push) {
    if (push.has_value()) {
      result.isStringPush[*push] = true;
    } else if (result.unprovenConsumer.empty()) {
      result.unprovenConsumer = opcodeName(code[consumer].op);
    }
  };
  for (size_t at = 0; at < code.size(); ++at) {
    const IrInstruction &inst = code[at];
    if (const std::optional<size_t> depth = dynamicStringDepth(inst.op)) {
      note(at, feedingPush(code, at, *depth));
    } else if (inst.op == IrOpcode::CallHost && inst.imm < module.hostImports.size()) {
      const std::vector<IrHostValueKind> &parameters =
          module.hostImports[static_cast<size_t>(inst.imm)].parameters;
      for (size_t index = 0; index < parameters.size(); ++index) {
        if (parameters[index] == IrHostValueKind::String) {
          note(at, feedingPush(code, at, parameters.size() - 1 - index));
        }
      }
    }
  }
  return result;
}

} // namespace primec
