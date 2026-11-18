#include "IrLowererEntryResultExit.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

namespace {

// How the entry's return value carries its `Result`.
enum class EntryResultForm {
  // A stdlib sum: the value is the address of tag and payload slots.
  StdlibSum,
  // The builtin value-carrying `Result<T, E>`: the error code in the high 32 bits, zero for ok.
  PackedValue,
  // The builtin status-only `Result<E>`: the error code itself, zero for ok.
  PackedStatus,
};

struct EntryResultShape {
  EntryResultForm form = EntryResultForm::PackedStatus;
  // For a stdlib sum: the `error` variant.
  const SumVariant *errorVariant = nullptr;
};

const SumVariant *findVariant(const Definition &sumDef, std::string_view name) {
  for (const auto &variant : sumDef.sumVariants) {
    if (variant.name == name) {
      return &variant;
    }
  }
  return nullptr;
}

std::optional<EntryResultShape>
classifyEntryResult(const Definition &entryDef,
                    const std::unordered_map<std::string, const Definition *> &defMap) {
  for (const auto &transform : entryDef.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &typeText = transform.templateArgs.front();
    if (typeText == "/std/result/Result" || typeText.rfind("/std/result/Result__", 0) == 0) {
      const auto it = defMap.find(typeText);
      if (it == defMap.end() || it->second == nullptr ||
          findVariant(*it->second, "ok") == nullptr) {
        return std::nullopt;
      }
      const SumVariant *errorVariant = findVariant(*it->second, "error");
      if (errorVariant == nullptr) {
        return std::nullopt;
      }
      return EntryResultShape{EntryResultForm::StdlibSum, errorVariant};
    }
    std::string base;
    std::string argText;
    std::vector<std::string> args;
    if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText) || base != "Result" ||
        !splitTemplateArgs(argText, args)) {
      return std::nullopt;
    }
    if (args.size() == 1) {
      return EntryResultShape{EntryResultForm::PackedStatus, nullptr};
    }
    if (args.size() == 2) {
      return EntryResultShape{EntryResultForm::PackedValue, nullptr};
    }
    return std::nullopt;
  }
  return std::nullopt;
}

bool isIntegerPayloadType(const std::string &typeName) {
  return typeName == "i32" || typeName == "i64" || typeName == "u64" || typeName == "u8" ||
         typeName == "int";
}

class DecodeBlock {
public:
  explicit DecodeBlock(std::vector<IrInstruction> &code) : code_(code) {
  }

  void push(IrOpcode op, uint64_t imm = 0) {
    code_.push_back({op, imm});
  }
  size_t pushJump(IrOpcode op) {
    push(op, 0);
    return code_.size() - 1;
  }
  void patchHere(size_t jumpIndex) {
    code_[jumpIndex].imm = static_cast<uint64_t>(code_.size());
  }

  // Stack: a code. Exits with the code when it is 1..255 and with 1 otherwise.
  void exitWithErrorCode() {
    push(IrOpcode::Dup);
    push(IrOpcode::PushI64, 1);
    push(IrOpcode::SubI64);
    push(IrOpcode::PushI64, 255);
    push(IrOpcode::CmpLtU64);
    const size_t toOne = pushJump(IrOpcode::JumpIfZero);
    push(IrOpcode::ReturnI32);
    patchHere(toOne);
    exitWithOne();
  }

  // Stack: one value to discard.
  void exitWithOne() {
    push(IrOpcode::Pop);
    push(IrOpcode::PushI32, 1);
    push(IrOpcode::ReturnI32);
  }

  // Stack: one value to discard.
  void exitWithZero() {
    push(IrOpcode::Pop);
    push(IrOpcode::PushI32, 0);
    push(IrOpcode::ReturnI32);
  }

  // Stack: an error code with zero meaning ok.
  void exitFromStatusCode() {
    push(IrOpcode::Dup);
    push(IrOpcode::PushI64, 0);
    push(IrOpcode::CmpEqI64);
    const size_t toError = pushJump(IrOpcode::JumpIfZero);
    exitWithZero();
    patchHere(toError);
    exitWithErrorCode();
  }

  // Stack: the sum's address.
  void exitFromSum(const SumVariant &errorVariant) {
    push(IrOpcode::Dup);
    push(IrOpcode::PushI64, IrSlotBytes);
    push(IrOpcode::AddI64);
    push(IrOpcode::LoadIndirect);
    push(IrOpcode::PushI64, static_cast<uint64_t>(errorVariant.variantIndex));
    push(IrOpcode::CmpEqI64);
    const size_t toOk = pushJump(IrOpcode::JumpIfZero);
    if (errorVariant.hasPayload && isIntegerPayloadType(errorVariant.payloadType)) {
      push(IrOpcode::PushI64, static_cast<uint64_t>(2) * IrSlotBytes);
      push(IrOpcode::AddI64);
      push(IrOpcode::LoadIndirect);
      exitWithErrorCode();
    } else {
      exitWithOne();
    }
    patchHere(toOk);
    exitWithZero();
  }

  // Stack: a packed value with the error code in the high 32 bits.
  void exitFromPackedValue() {
    push(IrOpcode::PushI64, 4294967296ull);
    push(IrOpcode::DivI64);
    exitFromStatusCode();
  }

private:
  std::vector<IrInstruction> &code_;
};

} // namespace

void applyEntryResultExitCode(const Definition &entryDef,
                              const std::unordered_map<std::string, const Definition *> &defMap,
                              IrFunction &entryFunction) {
  const std::optional<EntryResultShape> shape = classifyEntryResult(entryDef, defMap);
  if (!shape.has_value()) {
    return;
  }
  auto &code = entryFunction.instructions;
  std::vector<size_t> returns;
  for (size_t index = 0; index < code.size(); ++index) {
    if (code[index].op == IrOpcode::ReturnI64 || code[index].op == IrOpcode::ReturnI32) {
      returns.push_back(index);
    }
  }
  if (returns.empty()) {
    return;
  }
  const uint64_t blockStart = static_cast<uint64_t>(code.size());
  for (const size_t index : returns) {
    code[index].op = IrOpcode::Jump;
    code[index].imm = blockStart;
  }
  DecodeBlock block(code);
  switch (shape->form) {
  case EntryResultForm::StdlibSum:
    block.exitFromSum(*shape->errorVariant);
    break;
  case EntryResultForm::PackedValue:
    block.exitFromPackedValue();
    break;
  case EntryResultForm::PackedStatus:
    block.exitFromStatusCode();
    break;
  }
}

} // namespace primec::ir_lowerer
