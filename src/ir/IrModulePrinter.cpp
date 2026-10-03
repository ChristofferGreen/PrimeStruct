#include "primec/ir/IrModulePrinter.h"

#include "primec/ir/IrLocalEscape.h"
#include "primec/ir/IrOpcodeTable.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <sstream>

namespace primec {
namespace {

std::string hex(uint64_t value, int digits) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "0x%0*llx", digits, static_cast<unsigned long long>(value));
  return buffer;
}

std::string quote(const std::string &text) {
  std::string out = "\"";
  for (const unsigned char c : text) {
    switch (c) {
      case '\n':
        out += "\\n";
        break;
      case '\t':
        out += "\\t";
        break;
      case '\r':
        out += "\\r";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      default:
        if (c < 0x20 || c >= 0x7F) {
          char buffer[8];
          std::snprintf(buffer, sizeof(buffer), "\\x%02x", c);
          out += buffer;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  out += "\"";
  return out;
}

// Finite values print with enough digits to be unambiguous; NaN and infinities
// print by name because their text is platform-dependent.
std::string floatText(double value) {
  if (std::isnan(value)) {
    return "nan";
  }
  if (std::isinf(value)) {
    return value < 0 ? "-inf" : "inf";
  }
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.17g", value);
  return buffer;
}

std::string printFlags(uint64_t imm) {
  const uint64_t flags = decodePrintFlags(imm);
  std::string text;
  if ((flags & PrintFlagNewline) != 0) {
    text += "newline";
  }
  if ((flags & PrintFlagStderr) != 0) {
    text += text.empty() ? "stderr" : "|stderr";
  }
  return text.empty() ? "none" : text;
}

std::string stringRef(const IrModule &module, uint64_t index) {
  std::string text = "#" + std::to_string(index);
  if (index < module.stringTable.size()) {
    text += " " + quote(module.stringTable[static_cast<size_t>(index)]);
  } else {
    text += " <invalid>";
  }
  return text;
}

} // namespace

std::string formatIrInstruction(const IrModule &module, const IrInstruction &instruction) {
  const IrOpcodeInfo *info = irOpcodeInfo(instruction.op);
  std::string text = info != nullptr ? info->name : "Unknown(" + std::to_string(static_cast<int>(instruction.op)) + ")";
  const uint64_t imm = instruction.imm;
  switch (instruction.op) {
    case IrOpcode::PushI32:
      text += " " + std::to_string(static_cast<int32_t>(imm));
      break;
    case IrOpcode::PushI64:
      text += " " + std::to_string(static_cast<int64_t>(imm));
      break;
    case IrOpcode::PushF32: {
      const float value = std::bit_cast<float>(static_cast<uint32_t>(imm));
      text += " " + hex(imm & 0xFFFFFFFFull, 8) + " (" + floatText(value) + ")";
      break;
    }
    case IrOpcode::PushF64:
      text += " " + hex(imm, 16) + " (" + floatText(std::bit_cast<double>(imm)) + ")";
      break;
    case IrOpcode::LoadLocal:
    case IrOpcode::StoreLocal:
    case IrOpcode::AddressOfLocal:
    case IrOpcode::FileReadByte:
      text += " local " + std::to_string(imm);
      break;
    case IrOpcode::Jump:
    case IrOpcode::JumpIfZero:
      text += " -> " + std::to_string(imm);
      break;
    case IrOpcode::Call:
    case IrOpcode::CallVoid:
      text += " " + std::to_string(imm);
      if (imm < module.functions.size()) {
        text += " " + module.functions[static_cast<size_t>(imm)].name;
      }
      break;
    case IrOpcode::CallHost:
      text += " " + std::to_string(imm);
      if (imm < module.hostImports.size()) {
        text += " " + module.hostImports[static_cast<size_t>(imm)].name;
      }
      break;
    case IrOpcode::PrintI32:
    case IrOpcode::PrintI64:
    case IrOpcode::PrintU64:
    case IrOpcode::PrintStringDynamic:
    case IrOpcode::PrintArgv:
    case IrOpcode::PrintArgvUnsafe:
      text += " flags=" + printFlags(imm);
      break;
    case IrOpcode::PrintString:
      text += " " + stringRef(module, decodePrintStringIndex(imm)) + " flags=" + printFlags(imm);
      break;
    case IrOpcode::FileOpenRead:
    case IrOpcode::FileOpenWrite:
    case IrOpcode::FileOpenAppend:
    case IrOpcode::FileWriteString:
    case IrOpcode::LoadStringByte:
      text += " " + stringRef(module, imm);
      break;
    default:
      // Opcodes without an immediate; show a stray one rather than hide data.
      if (imm != 0) {
        text += " imm=" + std::to_string(imm);
      }
      break;
  }
  return text;
}

std::string formatIrFunction(const IrModule &module, size_t functionIndex) {
  std::ostringstream out;
  const IrFunction &function = module.functions[functionIndex];
  const IrLocalEscapeInfo escape = analyzeIrLocalEscape(function);
  out << "function " << functionIndex << " " << function.name << " parameters=" << function.parameterCount
      << " effects=" << hex(function.metadata.effectMask, 1) << " capabilities=" << hex(function.metadata.capabilityMask, 1)
      << " locals=" << escape.localCount << " instructions=" << function.instructions.size() << "\n";
  for (size_t index = 0; index < function.instructions.size(); ++index) {
    std::string label = std::to_string(index);
    if (label.size() < 4) {
      label.insert(0, 4 - label.size(), '0');
    }
    out << "  " << label << "  " << formatIrInstruction(module, function.instructions[index]) << "\n";
  }
  return out.str();
}

std::string formatIrModule(const IrModule &module) {
  std::ostringstream out;
  out << "ir_module_v1 schema=" << module.schemaVersion << "\n";
  out << "entry=";
  if (module.entryIndex >= 0 && static_cast<size_t>(module.entryIndex) < module.functions.size()) {
    out << module.functions[static_cast<size_t>(module.entryIndex)].name << " (function " << module.entryIndex << ")";
  } else {
    out << "<invalid " << module.entryIndex << ">";
  }
  out << "\n";

  out << "string_table: " << module.stringTable.size() << "\n";
  for (size_t i = 0; i < module.stringTable.size(); ++i) {
    out << "  " << i << ": " << quote(module.stringTable[i]) << "\n";
  }

  out << "host_imports: " << module.hostImports.size() << "\n";
  const auto kindName = [](IrHostValueKind kind) -> const char * {
    switch (kind) {
      case IrHostValueKind::Void:
        return "void";
      case IrHostValueKind::I32:
        return "i32";
      case IrHostValueKind::I64:
        return "i64";
      case IrHostValueKind::U64:
        return "u64";
      case IrHostValueKind::F32:
        return "f32";
      case IrHostValueKind::F64:
        return "f64";
      case IrHostValueKind::Bool:
        return "bool";
      case IrHostValueKind::String:
        return "string";
    }
    return "?";
  };
  for (size_t i = 0; i < module.hostImports.size(); ++i) {
    const IrHostImport &import = module.hostImports[i];
    out << "  " << i << ": " << import.name << "(";
    for (size_t p = 0; p < import.parameters.size(); ++p) {
      out << (p == 0 ? "" : ", ") << kindName(import.parameters[p]);
    }
    out << ") -> " << kindName(import.returnKind) << "\n";
  }

  out << "struct_layouts: " << module.structLayouts.size() << "\n";
  for (const IrStructLayout &layout : module.structLayouts) {
    out << "  " << layout.name << " size=" << layout.totalSizeBytes << " align=" << layout.alignmentBytes
        << " fields=" << layout.fields.size() << "\n";
  }

  out << "functions: " << module.functions.size() << "\n";
  for (size_t i = 0; i < module.functions.size(); ++i) {
    out << formatIrFunction(module, i);
  }
  return out.str();
}

} // namespace primec
