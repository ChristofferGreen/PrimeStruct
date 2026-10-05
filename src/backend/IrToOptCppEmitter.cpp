#include "primec/backend/IrToOptCppEmitter.h"

#include "primec/ir/IrCfg.h"
#include "primec/ir/IrOpcodeTable.h"
#include "primec/ir/IrPureSemantics.h"

#include "IrToOptCppRuntime.h"
#include "primec/runtime/VmHeapCore.h"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace primec {
namespace {

std::string hex(uint64_t value) {
  std::ostringstream out;
  out << "UINT64_C(0x" << std::hex << value << ")";
  return out.str();
}

// The slot the VM leaves for a constant push.
uint64_t pushedSlot(const IrInstruction &instruction) {
  if (instruction.op == IrOpcode::PushI32) {
    return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(instruction.imm)));
  }
  return instruction.imm;
}

std::string opcodeName(IrOpcode op) {
  const IrOpcodeInfo *info = irOpcodeInfo(op);
  return info != nullptr ? info->name : "Unknown";
}

// C++ for a C++-quoted string literal that preserves every byte (octal escapes
// are always three digits, so they never swallow a following character).
std::string quote(const std::string &text) {
  std::string out = "\"";
  for (const unsigned char c : text) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += static_cast<char>(c);
    } else if (c >= 0x20 && c < 0x7F && c != '?') {
      out += static_cast<char>(c);
    } else {
      out += '\\';
      out += static_cast<char>('0' + ((c >> 6) & 7));
      out += static_cast<char>('0' + ((c >> 3) & 7));
      out += static_cast<char>('0' + (c & 7));
    }
  }
  out += "\"";
  return out;
}

// Expression for a pure opcode over operand expressions `a` (and `b`). Mirrors
// evalPureOpcode in IrPureSemantics.h; the emitter's tests compare the two.
bool pureExpression(IrOpcode op, const std::string &a, const std::string &b, std::string &expr) {
  const auto cmp = [&](const char *oper, bool isSigned) {
    expr = isSigned ? "static_cast<uint64_t>(ps_s64(" + a + ") " + oper + " ps_s64(" + b + "))"
                    : "static_cast<uint64_t>(" + a + " " + oper + " " + b + ")";
  };
  const auto f32cmp = [&](const char *oper) {
    expr = "static_cast<uint64_t>(ps_f32(" + a + ") " + oper + " ps_f32(" + b + "))";
  };
  const auto f64cmp = [&](const char *oper) {
    expr = "static_cast<uint64_t>(ps_f64(" + a + ") " + oper + " ps_f64(" + b + "))";
  };
  const auto f32bin = [&](const char *oper) {
    expr = "ps_slot_f32(ps_f32(" + a + ") " + oper + " ps_f32(" + b + "))";
  };
  const auto f64bin = [&](const char *oper) {
    expr = "ps_slot_f64(ps_f64(" + a + ") " + oper + " ps_f64(" + b + "))";
  };
  switch (op) {
  case IrOpcode::AddI32:
  case IrOpcode::AddI64:
    expr = "(" + a + " + " + b + ")";
    return true;
  case IrOpcode::SubI32:
  case IrOpcode::SubI64:
    expr = "(" + a + " - " + b + ")";
    return true;
  case IrOpcode::MulI32:
  case IrOpcode::MulI64:
    expr = "(" + a + " * " + b + ")";
    return true;
  case IrOpcode::DivI32:
  case IrOpcode::DivI64:
    expr = "ps_div_s64(" + a + ", " + b + ")";
    return true;
  case IrOpcode::DivU64:
    expr = "ps_div_u64(" + a + ", " + b + ")";
    return true;
  case IrOpcode::NegI32:
  case IrOpcode::NegI64:
    expr = "(uint64_t{0} - " + a + ")";
    return true;
  case IrOpcode::SextI32:
    expr = "static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(" + a + ")))";
    return true;
  case IrOpcode::AddF32:
    f32bin("+");
    return true;
  case IrOpcode::SubF32:
    f32bin("-");
    return true;
  case IrOpcode::MulF32:
    f32bin("*");
    return true;
  case IrOpcode::DivF32:
    f32bin("/");
    return true;
  case IrOpcode::NegF32:
    expr = "ps_slot_f32(-ps_f32(" + a + "))";
    return true;
  case IrOpcode::AddF64:
    f64bin("+");
    return true;
  case IrOpcode::SubF64:
    f64bin("-");
    return true;
  case IrOpcode::MulF64:
    f64bin("*");
    return true;
  case IrOpcode::DivF64:
    f64bin("/");
    return true;
  case IrOpcode::NegF64:
    expr = "ps_slot_f64(-ps_f64(" + a + "))";
    return true;
  case IrOpcode::CmpEqI32:
  case IrOpcode::CmpEqI64:
    cmp("==", false);
    return true;
  case IrOpcode::CmpNeI32:
  case IrOpcode::CmpNeI64:
    cmp("!=", false);
    return true;
  case IrOpcode::CmpLtI32:
  case IrOpcode::CmpLtI64:
    cmp("<", true);
    return true;
  case IrOpcode::CmpLeI32:
  case IrOpcode::CmpLeI64:
    cmp("<=", true);
    return true;
  case IrOpcode::CmpGtI32:
  case IrOpcode::CmpGtI64:
    cmp(">", true);
    return true;
  case IrOpcode::CmpGeI32:
  case IrOpcode::CmpGeI64:
    cmp(">=", true);
    return true;
  case IrOpcode::CmpLtU64:
    cmp("<", false);
    return true;
  case IrOpcode::CmpLeU64:
    cmp("<=", false);
    return true;
  case IrOpcode::CmpGtU64:
    cmp(">", false);
    return true;
  case IrOpcode::CmpGeU64:
    cmp(">=", false);
    return true;
  case IrOpcode::CmpEqF32:
    f32cmp("==");
    return true;
  case IrOpcode::CmpNeF32:
    f32cmp("!=");
    return true;
  case IrOpcode::CmpLtF32:
    f32cmp("<");
    return true;
  case IrOpcode::CmpLeF32:
    f32cmp("<=");
    return true;
  case IrOpcode::CmpGtF32:
    f32cmp(">");
    return true;
  case IrOpcode::CmpGeF32:
    f32cmp(">=");
    return true;
  case IrOpcode::CmpEqF64:
    f64cmp("==");
    return true;
  case IrOpcode::CmpNeF64:
    f64cmp("!=");
    return true;
  case IrOpcode::CmpLtF64:
    f64cmp("<");
    return true;
  case IrOpcode::CmpLeF64:
    f64cmp("<=");
    return true;
  case IrOpcode::CmpGtF64:
    f64cmp(">");
    return true;
  case IrOpcode::CmpGeF64:
    f64cmp(">=");
    return true;
  case IrOpcode::ConvertI32ToF32:
    expr = "ps_slot_f32(static_cast<float>(static_cast<int32_t>(" + a + ")))";
    return true;
  case IrOpcode::ConvertI64ToF32:
    expr = "ps_slot_f32(static_cast<float>(ps_s64(" + a + ")))";
    return true;
  case IrOpcode::ConvertU64ToF32:
    expr = "ps_slot_f32(static_cast<float>(" + a + "))";
    return true;
  case IrOpcode::ConvertI32ToF64:
    expr = "ps_slot_f64(static_cast<double>(static_cast<int32_t>(" + a + ")))";
    return true;
  case IrOpcode::ConvertI64ToF64:
    expr = "ps_slot_f64(static_cast<double>(ps_s64(" + a + ")))";
    return true;
  case IrOpcode::ConvertU64ToF64:
    expr = "ps_slot_f64(static_cast<double>(" + a + "))";
    return true;
  case IrOpcode::ConvertF32ToI32:
    expr = "static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(ps_f32(" + a + "))))";
    return true;
  case IrOpcode::ConvertF32ToI64:
    expr = "static_cast<uint64_t>(static_cast<int64_t>(ps_f32(" + a + ")))";
    return true;
  case IrOpcode::ConvertF32ToU64:
    expr = "static_cast<uint64_t>(ps_f32(" + a + "))";
    return true;
  case IrOpcode::ConvertF64ToI32:
    expr = "static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(ps_f64(" + a + "))))";
    return true;
  case IrOpcode::ConvertF64ToI64:
    expr = "static_cast<uint64_t>(static_cast<int64_t>(ps_f64(" + a + ")))";
    return true;
  case IrOpcode::ConvertF64ToU64:
    expr = "static_cast<uint64_t>(ps_f64(" + a + "))";
    return true;
  case IrOpcode::ConvertF32ToF64:
    expr = "ps_slot_f64(static_cast<double>(ps_f32(" + a + ")))";
    return true;
  case IrOpcode::ConvertF64ToF32:
    expr = "ps_slot_f32(static_cast<float>(ps_f64(" + a + ")))";
    return true;
  default:
    return false;
  }
}

const char *fileOpenMode(IrOpcode op) {
  switch (op) {
  case IrOpcode::FileOpenWrite:
  case IrOpcode::FileOpenWriteDynamic:
    return "1";
  case IrOpcode::FileOpenAppend:
  case IrOpcode::FileOpenAppendDynamic:
    return "2";
  default:
    return "0";
  }
}

std::string functionSymbol(size_t index) {
  return "ps_f" + std::to_string(index);
}

std::string parameterList(uint32_t count, bool withTypes) {
  std::string out = withTypes ? "Rt &rt" : "rt";
  for (uint32_t i = 0; i < count; ++i) {
    out += withTypes ? ", uint64_t a" : ", a";
    out += std::to_string(i);
  }
  return out;
}

class FunctionEmitter {
public:
  FunctionEmitter(const IrModule &module, size_t index, std::ostringstream &out)
      : module_(module), index_(index), function_(module.functions[index]), out_(out) {
  }

  bool emit(std::string &error) {
    IrCfg cfg;
    IrCfgError cfgError;
    if (!buildIrCfg(function_, module_, cfg, cfgError)) {
      error = "optexe cannot analyze " + function_.name + " at instruction " +
              std::to_string(cfgError.instructionIndex) + " (" + opcodeName(cfgError.opcode) +
              "): inconsistent or underflowing operand stack";
      return false;
    }

    out_ << "uint64_t " << functionSymbol(index_) << "("
         << parameterList(function_.parameterCount, true) << ") {\n";
    // The arguments are the callee's initial stack: first pushed is slot 0.
    const int64_t slotCount = cfg.maxStackDepth;
    if (slotCount > 0) {
      out_ << "  uint64_t ";
      for (int64_t slot = 0; slot < slotCount; ++slot) {
        out_ << (slot == 0 ? "" : ", ") << "s" << slot << " = "
             << (slot < static_cast<int64_t>(function_.parameterCount) ? "a" + std::to_string(slot)
                                                                       : "0");
      }
      out_ << ";\n";
    }
    // Locals are scalars unless the function can address its frame: the VM
    // resolves LoadIndirect/StoreIndirect against the *current* frame, so any
    // function with an indirect access (not just AddressOfLocal) needs one.
    localCount_ = 0;
    frameMode_ = false;
    for (const IrInstruction &instruction : function_.instructions) {
      switch (instruction.op) {
      case IrOpcode::LoadLocal:
      case IrOpcode::StoreLocal:
        localCount_ = std::max<uint32_t>(localCount_, static_cast<uint32_t>(instruction.imm) + 1);
        break;
      case IrOpcode::AddressOfLocal:
        localCount_ = std::max<uint32_t>(localCount_, static_cast<uint32_t>(instruction.imm) + 1);
        frameMode_ = true;
        break;
      case IrOpcode::LoadIndirect:
      case IrOpcode::StoreIndirect:
        frameMode_ = true;
        break;
      default:
        break;
      }
    }
    if (frameMode_) {
      out_ << "  uint64_t frame[" << std::max<uint32_t>(localCount_, 1) << "] = {};\n";
    } else if (localCount_ > 0) {
      out_ << "  uint64_t ";
      for (uint32_t local = 0; local < localCount_; ++local) {
        out_ << (local == 0 ? "" : ", ") << "l" << local << " = 0";
      }
      out_ << ";\n";
    }
    out_ << "  if (rt.depth >= PsMaxCallDepth) {\n    ps_fault(\"VM call stack overflow\");\n  }\n";
    out_ << "  ++rt.depth;\n";

    for (const IrCfgBlock &block : cfg.blocks) {
      if (!block.reachable) {
        continue;
      }
      out_ << "L" << block.start << ":;\n";
      int64_t depth = block.entryDepth;
      for (size_t i = block.start; i < block.end; ++i) {
        if (!emitInstruction(function_.instructions[i], depth, error)) {
          return false;
        }
        IrStackEffect effect;
        computeIrStackEffect(function_.instructions[i], module_, effect);
        depth += static_cast<int64_t>(effect.pushes) - static_cast<int64_t>(effect.pops);
      }
    }
    // Falling off the end, or jumping to it, is a fault in the VM too.
    out_ << "L" << function_.instructions.size() << ":;\n";
    out_ << "  ps_fault(\"missing return in IR function " << function_.name << "\");\n";
    out_ << "}\n\n";
    return true;
  }

private:
  std::string slot(int64_t depth) const {
    return "s" + std::to_string(depth);
  }

  void line(const std::string &text) {
    out_ << "  " << text << "\n";
  }

  // Epilogue shared by every return: pop the frame, then yield the slot value.
  void emitReturn(const std::string &value) {
    line("--rt.depth;");
    line("return " + value + ";");
  }

  std::string local(uint64_t index) const {
    return frameMode_ ? "frame[" + std::to_string(index) + "]" : "l" + std::to_string(index);
  }

  std::string stringRef(const std::string &index) const {
    return "ps_string(ps_strings, PsStringCount, " + index + ")";
  }

  std::string label(uint64_t target) const {
    return "L" + std::to_string(target);
  }

  bool emitInstruction(const IrInstruction &instruction, int64_t depth, std::string &error) {
    const IrOpcode op = instruction.op;
    if (isIrPureOpcode(op)) {
      std::string expr;
      if (irPureOpcodeArity(op) == 2) {
        pureExpression(op, slot(depth - 2), slot(depth - 1), expr);
        line(slot(depth - 2) + " = " + expr + ";");
      } else {
        pureExpression(op, slot(depth - 1), "", expr);
        line(slot(depth - 1) + " = " + expr + ";");
      }
      return true;
    }
    switch (op) {
    case IrOpcode::PushI32:
    case IrOpcode::PushI64:
    case IrOpcode::PushF32:
    case IrOpcode::PushF64:
      line(slot(depth) + " = " + hex(pushedSlot(instruction)) + ";");
      return true;
    case IrOpcode::PushArgc:
      line(slot(depth) +
           " = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(rt.argc)));");
      return true;
    case IrOpcode::LoadLocal:
      line(slot(depth) + " = " + local(instruction.imm) + ";");
      return true;
    case IrOpcode::StoreLocal:
      line(local(instruction.imm) + " = " + slot(depth - 1) + ";");
      return true;
    case IrOpcode::Dup:
      line(slot(depth) + " = " + slot(depth - 1) + ";");
      return true;
    case IrOpcode::Pop:
      return true;
    case IrOpcode::Jump:
      line("goto " + label(instruction.imm) + ";");
      return true;
    case IrOpcode::JumpIfZero:
      line("if (" + slot(depth - 1) + " == 0) goto " + label(instruction.imm) + ";");
      return true;
    case IrOpcode::ReturnVoid:
      emitReturn("0");
      return true;
    case IrOpcode::ReturnI32:
      emitReturn("static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(" +
                 slot(depth - 1) + ")))");
      return true;
    case IrOpcode::ReturnI64:
    case IrOpcode::ReturnF64:
      emitReturn(slot(depth - 1));
      return true;
    case IrOpcode::ReturnF32:
      emitReturn("static_cast<uint64_t>(static_cast<uint32_t>(" + slot(depth - 1) + "))");
      return true;
    case IrOpcode::Call:
    case IrOpcode::CallVoid: {
      if (instruction.imm >= module_.functions.size()) {
        error = "optexe found an invalid call target in " + function_.name;
        return false;
      }
      const IrFunction &callee = module_.functions[static_cast<size_t>(instruction.imm)];
      std::string call = functionSymbol(static_cast<size_t>(instruction.imm)) + "(rt";
      for (uint32_t i = 0; i < callee.parameterCount; ++i) {
        call += ", " + slot(depth - static_cast<int64_t>(callee.parameterCount) + i);
      }
      call += ")";
      if (op == IrOpcode::Call) {
        line(slot(depth - static_cast<int64_t>(callee.parameterCount)) + " = " + call + ";");
      } else {
        line(call + ";");
      }
      return true;
    }
    case IrOpcode::PrintI32:
      line("ps_emit(std::to_string(static_cast<int32_t>(" + slot(depth - 1) + ")), " +
           std::to_string(decodePrintFlags(instruction.imm)) + ");");
      return true;
    case IrOpcode::PrintI64:
      line("ps_emit(std::to_string(ps_s64(" + slot(depth - 1) + ")), " +
           std::to_string(decodePrintFlags(instruction.imm)) + ");");
      return true;
    case IrOpcode::PrintU64:
      line("ps_emit(std::to_string(" + slot(depth - 1) + "), " +
           std::to_string(decodePrintFlags(instruction.imm)) + ");");
      return true;
    case IrOpcode::PrintString: {
      const uint64_t stringIndex = decodePrintStringIndex(instruction.imm);
      if (stringIndex >= module_.stringTable.size()) {
        line("ps_fault(\"invalid string index in IR\");");
        return true;
      }
      const std::string &text = module_.stringTable[static_cast<size_t>(stringIndex)];
      line("ps_emit(" + quote(text) + ", " + std::to_string(text.size()) + ", " +
           std::to_string(decodePrintFlags(instruction.imm)) + ");");
      return true;
    }
    case IrOpcode::AddressOfLocal:
      line(slot(depth) + " = UINT64_C(" + std::to_string(instruction.imm * IrSlotBytes) + ");");
      return true;
    case IrOpcode::LoadIndirect:
      line(slot(depth - 1) + " = *ps_resolve(rt, frame, " + std::to_string(localCount_) + ", " +
           slot(depth - 1) + ");");
      return true;
    case IrOpcode::StoreIndirect:
      line("*ps_resolve(rt, frame, " + std::to_string(localCount_) + ", " + slot(depth - 2) +
           ") = " + slot(depth - 1) + ";");
      line(slot(depth - 2) + " = " + slot(depth - 1) + ";");
      return true;
    case IrOpcode::HeapAlloc:
      line(slot(depth - 1) + " = ps_heap_alloc(rt, " + slot(depth - 1) + ");");
      return true;
    case IrOpcode::HeapFree:
      line("ps_heap_free(rt, " + slot(depth - 1) + ");");
      return true;
    case IrOpcode::HeapRealloc:
      line(slot(depth - 2) + " = ps_heap_realloc(rt, " + slot(depth - 2) + ", " + slot(depth - 1) +
           ");");
      return true;
    case IrOpcode::LoadStringByte:
    case IrOpcode::LoadStringByteDynamic: {
      const bool dynamic = op == IrOpcode::LoadStringByteDynamic;
      const std::string position = slot(depth - 1);
      const std::string stringIndex = dynamic ? slot(depth - 2) : std::to_string(instruction.imm);
      line("{");
      line("  const PsString &text = " + stringRef(stringIndex) + ";");
      line("  if (" + position +
           " >= text.size) {\n    ps_fault(\"string index out of bounds in IR\");\n  }");
      line("  " + slot(dynamic ? depth - 2 : depth - 1) +
           " = static_cast<uint64_t>(static_cast<uint8_t>(text.data[" + position + "]));");
      line("}");
      return true;
    }
    case IrOpcode::LoadStringLength:
      line(slot(depth - 1) + " = static_cast<uint64_t>(" + stringRef(slot(depth - 1)) + ".size);");
      return true;
    case IrOpcode::PrintStringDynamic: {
      line("{");
      line("  const PsString &text = " + stringRef(slot(depth - 1)) + ";");
      line("  ps_emit(text.data, text.size, " + std::to_string(decodePrintFlags(instruction.imm)) +
           ");");
      line("}");
      return true;
    }
    case IrOpcode::PrintArgv:
    case IrOpcode::PrintArgvUnsafe:
      line("ps_print_argv(rt, " + slot(depth - 1) + ", " +
           std::to_string(decodePrintFlags(instruction.imm)) + ", " +
           (op == IrOpcode::PrintArgvUnsafe ? "true" : "false") + ");");
      return true;
    case IrOpcode::FileOpenRead:
    case IrOpcode::FileOpenWrite:
    case IrOpcode::FileOpenAppend:
      line(slot(depth) + " = ps_file_open(" + stringRef(std::to_string(instruction.imm)) + ", " +
           fileOpenMode(op) + ");");
      return true;
    case IrOpcode::FileOpenReadDynamic:
    case IrOpcode::FileOpenWriteDynamic:
    case IrOpcode::FileOpenAppendDynamic:
      line(slot(depth - 1) + " = ps_file_open(" + stringRef(slot(depth - 1)) + ", " +
           fileOpenMode(op) + ");");
      return true;
    case IrOpcode::FileClose:
      line(slot(depth - 1) + " = ps_file_close(" + slot(depth - 1) + ");");
      return true;
    case IrOpcode::FileFlush:
      line(slot(depth - 1) + " = ps_file_flush(" + slot(depth - 1) + ");");
      return true;
    case IrOpcode::FileReadByte:
      // The VM sizes its locals from Load/Store/AddressOf only, so a read
      // target beyond them is its "invalid local index" fault.
      if (instruction.imm >= localCount_) {
        line("ps_fault(\"invalid local index in IR\");");
      } else {
        line(slot(depth - 1) + " = ps_file_read_byte(" + slot(depth - 1) + ", " +
             local(instruction.imm) + ");");
      }
      return true;
    case IrOpcode::FileWriteI32:
      line(slot(depth - 2) + " = ps_file_write_text(" + slot(depth - 2) +
           ", std::to_string(static_cast<int32_t>(" + slot(depth - 1) + ")));");
      return true;
    case IrOpcode::FileWriteI64:
      line(slot(depth - 2) + " = ps_file_write_text(" + slot(depth - 2) +
           ", std::to_string(ps_s64(" + slot(depth - 1) + ")));");
      return true;
    case IrOpcode::FileWriteU64:
      line(slot(depth - 2) + " = ps_file_write_text(" + slot(depth - 2) + ", std::to_string(" +
           slot(depth - 1) + "));");
      return true;
    case IrOpcode::FileWriteString:
      line("{");
      line("  const PsString &text = " + stringRef(std::to_string(instruction.imm)) + ";");
      line("  " + slot(depth - 1) + " = ps_write_all(" + slot(depth - 1) +
           ", text.data, text.size);");
      line("}");
      return true;
    case IrOpcode::FileWriteStringDynamic:
      line("{");
      line("  const PsString &text = " + stringRef(slot(depth - 1)) + ";");
      line("  " + slot(depth - 2) + " = ps_write_all(" + slot(depth - 2) +
           ", text.data, text.size);");
      line("}");
      return true;
    case IrOpcode::FileWriteByte:
      line(slot(depth - 2) + " = ps_file_write_byte(" + slot(depth - 2) + ", " + slot(depth - 1) +
           ");");
      return true;
    case IrOpcode::FileWriteNewline:
      line(slot(depth - 1) + " = ps_file_write_newline(" + slot(depth - 1) + ");");
      return true;
    default:
      error =
          "optexe does not support opcode " + opcodeName(op) + " yet (in " + function_.name + ")";
      return false;
    }
  }

  const IrModule &module_;
  size_t index_;
  const IrFunction &function_;
  std::ostringstream &out_;
  uint32_t localCount_ = 0;
  bool frameMode_ = false;
};

} // namespace

bool IrToOptCppEmitter::emitSource(const IrModule &module,
                                   std::string &out,
                                   std::string &error) const {
  error.clear();
  if (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
    error = "invalid IR entry index";
    return false;
  }
  if (module.functions[static_cast<size_t>(module.entryIndex)].parameterCount != 0) {
    error = "optexe requires an entry function without parameters";
    return false;
  }
  std::ostringstream source;
  source << OptCppRuntimeHeaders << VmHeapCoreSource << "\n" << OptCppRuntimePreamble;
  source << "namespace {\n\n";
  source << "const PsString ps_strings[] = {";
  for (const std::string &text : module.stringTable) {
    source << "{" << quote(text) << ", " << text.size() << "}, ";
  }
  if (module.stringTable.empty()) {
    source << "{nullptr, 0}";
  }
  source << "};\nconstexpr size_t PsStringCount = " << module.stringTable.size() << ";\n\n";
  for (size_t i = 0; i < module.functions.size(); ++i) {
    source << "uint64_t " << functionSymbol(i) << "("
           << parameterList(module.functions[i].parameterCount, true) << ");\n";
  }
  source << "\n";
  for (size_t i = 0; i < module.functions.size(); ++i) {
    FunctionEmitter emitter(module, i, source);
    if (!emitter.emit(error)) {
      return false;
    }
  }
  source << "} // namespace\n\n";
  source << "int main(int argc, char **argv) {\n";
  source << "  Rt rt{argc, argv, 0};\n";
  source << "  return static_cast<int>(static_cast<int32_t>("
         << functionSymbol(static_cast<size_t>(module.entryIndex)) << "(rt)));\n";
  source << "}\n";
  out = source.str();
  return true;
}

} // namespace primec
