#include "VmIoHelpers.h"

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

namespace primec::vm_detail {
namespace {

uint32_t currentIoErrorCode() {
  return errno == 0 ? 1u : static_cast<uint32_t>(errno);
}

int writeAll(int fd, const void *data, size_t size) {
  const char *cursor = static_cast<const char *>(data);
  size_t remaining = size;
  while (remaining > 0) {
    const ssize_t wrote = ::write(fd, cursor, remaining);
    if (wrote < 0) {
      return static_cast<int>(currentIoErrorCode());
    }
    if (wrote == 0) {
      return EIO;
    }
    remaining -= static_cast<size_t>(wrote);
    cursor += wrote;
  }
  return 0;
}

void emitPrintedText(std::string_view text, uint64_t flags) {
  FILE *out = (flags & PrintFlagStderr) ? stderr : stdout;
  std::fwrite(text.data(), 1, text.size(), out);
  if ((flags & PrintFlagNewline) != 0) {
    std::fputc('\n', out);
  }
}

bool popStackValue(std::vector<uint64_t> &stack,
                   std::string_view underflowMessage,
                   std::string &error,
                   uint64_t &value) {
  if (stack.empty()) {
    error = std::string(underflowMessage);
    return false;
  }
  value = stack.back();
  stack.pop_back();
  return true;
}

bool popStackPair(std::vector<uint64_t> &stack,
                  std::string_view underflowMessage,
                  std::string &error,
                  uint64_t &first,
                  uint64_t &second) {
  if (stack.size() < 2) {
    error = std::string(underflowMessage);
    return false;
  }
  second = stack.back();
  stack.pop_back();
  first = stack.back();
  stack.pop_back();
  return true;
}

bool resolveStringIndex(const IrModule &module, uint64_t stringIndex, std::string &error, const std::string *&textOut) {
  if (stringIndex >= module.stringTable.size()) {
    error = "invalid string index in IR";
    return false;
  }
  textOut = &module.stringTable[static_cast<size_t>(stringIndex)];
  return true;
}

uint64_t packFileHandle(int fd) {
  if (fd < 0) {
    return static_cast<uint64_t>(currentIoErrorCode()) << 32;
  }
  return static_cast<uint64_t>(static_cast<uint32_t>(fd));
}

int resolveFileOpenFlags(IrOpcode opcode) {
  switch (opcode) {
    case IrOpcode::FileOpenWrite:
    case IrOpcode::FileOpenWriteDynamic:
      return O_WRONLY | O_CREAT | O_TRUNC;
    case IrOpcode::FileOpenAppend:
    case IrOpcode::FileOpenAppendDynamic:
      return O_WRONLY | O_CREAT | O_APPEND;
    case IrOpcode::FileOpenRead:
    case IrOpcode::FileOpenReadDynamic:
      return O_RDONLY;
    default:
      return O_RDONLY;
  }
}

} // namespace

bool handlePrintOpcode(const IrModule &module,
                       const IrInstruction &inst,
                       std::vector<uint64_t> &stack,
                       const std::vector<std::string_view> *args,
                       std::string &error) {
  switch (inst.op) {
    case IrOpcode::PrintI32: {
      uint64_t raw = 0;
      if (!popStackValue(stack, "IR stack underflow on print", error, raw)) {
        return false;
      }
      const std::string text = std::to_string(static_cast<int32_t>(raw));
      emitPrintedText(text, decodePrintFlags(inst.imm));
      return true;
    }
    case IrOpcode::PrintI64: {
      uint64_t raw = 0;
      if (!popStackValue(stack, "IR stack underflow on print", error, raw)) {
        return false;
      }
      const std::string text = std::to_string(static_cast<int64_t>(raw));
      emitPrintedText(text, decodePrintFlags(inst.imm));
      return true;
    }
    case IrOpcode::PrintU64: {
      uint64_t raw = 0;
      if (!popStackValue(stack, "IR stack underflow on print", error, raw)) {
        return false;
      }
      const std::string text = std::to_string(raw);
      emitPrintedText(text, decodePrintFlags(inst.imm));
      return true;
    }
    case IrOpcode::PrintString: {
      const std::string *text = nullptr;
      if (!resolveStringIndex(module, decodePrintStringIndex(inst.imm), error, text)) {
        return false;
      }
      emitPrintedText(*text, decodePrintFlags(inst.imm));
      return true;
    }
    case IrOpcode::PrintStringDynamic: {
      uint64_t stringIndex = 0;
      if (!popStackValue(stack, "IR stack underflow on print", error, stringIndex)) {
        return false;
      }
      const std::string *text = nullptr;
      if (!resolveStringIndex(module, stringIndex, error, text)) {
        return false;
      }
      emitPrintedText(*text, decodePrintFlags(inst.imm));
      return true;
    }
    case IrOpcode::PrintArgv:
    case IrOpcode::PrintArgvUnsafe: {
      uint64_t rawIndex = 0;
      if (!popStackValue(stack, "IR stack underflow on print", error, rawIndex)) {
        return false;
      }
      if (!args) {
        error = inst.op == IrOpcode::PrintArgv ? "VM missing argv data for PrintArgv"
                                               : "VM missing argv data for PrintArgvUnsafe";
        return false;
      }
      const int64_t index = static_cast<int64_t>(rawIndex);
      if (index < 0 || static_cast<size_t>(index) >= args->size()) {
        if (inst.op == IrOpcode::PrintArgvUnsafe) {
          return true;
        }
        error = "invalid argv index in IR";
        return false;
      }
      emitPrintedText((*args)[static_cast<size_t>(index)], decodePrintFlags(inst.imm));
      return true;
    }
    default:
      error = "unsupported print opcode in VM helper";
      return false;
  }
}

bool handleFileOpcode(const IrModule &module,
                      const IrInstruction &inst,
                      std::vector<uint64_t> &stack,
                      std::vector<uint64_t> &locals,
                      std::string &error) {
  switch (inst.op) {
    case IrOpcode::FileOpenRead:
    case IrOpcode::FileOpenWrite:
    case IrOpcode::FileOpenAppend: {
      const std::string *path = nullptr;
      if (!resolveStringIndex(module, inst.imm, error, path)) {
        return false;
      }
      const int fd = ::open(path->c_str(), resolveFileOpenFlags(inst.op), 0644);
      stack.push_back(packFileHandle(fd));
      return true;
    }
    case IrOpcode::FileOpenReadDynamic:
    case IrOpcode::FileOpenWriteDynamic:
    case IrOpcode::FileOpenAppendDynamic: {
      uint64_t stringIndex = 0;
      if (!popStackValue(stack, "IR stack underflow on file open", error, stringIndex)) {
        return false;
      }
      const std::string *path = nullptr;
      if (!resolveStringIndex(module, stringIndex, error, path)) {
        return false;
      }
      const int fd = ::open(path->c_str(), resolveFileOpenFlags(inst.op), 0644);
      stack.push_back(packFileHandle(fd));
      return true;
    }
    case IrOpcode::FileClose: {
      uint64_t handle = 0;
      if (!popStackValue(stack, "IR stack underflow on file close", error, handle)) {
        return false;
      }
      const int fd = static_cast<int>(handle & 0xffffffffu);
      const int rc = ::close(fd);
      stack.push_back((rc < 0) ? static_cast<uint64_t>(currentIoErrorCode()) : 0u);
      return true;
    }
    case IrOpcode::FileReadByte: {
      uint64_t handle = 0;
      if (!popStackValue(stack, "IR stack underflow on file read", error, handle)) {
        return false;
      }
      if (inst.imm >= locals.size()) {
        error = "invalid local index in IR";
        return false;
      }
      const int fd = static_cast<int>(handle & 0xffffffffu);
      uint8_t value = 0;
      const ssize_t rc = ::read(fd, &value, 1);
      uint32_t err = 0u;
      if (rc < 0) {
        err = currentIoErrorCode();
      } else if (rc == 0) {
        err = FileReadEofCode;
      } else {
        locals[static_cast<size_t>(inst.imm)] = static_cast<uint64_t>(value);
      }
      stack.push_back(static_cast<uint64_t>(err));
      return true;
    }
    case IrOpcode::FileFlush: {
      uint64_t handle = 0;
      if (!popStackValue(stack, "IR stack underflow on file flush", error, handle)) {
        return false;
      }
      const int fd = static_cast<int>(handle & 0xffffffffu);
      const int rc = ::fsync(fd);
      stack.push_back((rc < 0) ? static_cast<uint64_t>(currentIoErrorCode()) : 0u);
      return true;
    }
    case IrOpcode::FileWriteI32: {
      uint64_t handle = 0;
      uint64_t rawValue = 0;
      if (!popStackPair(stack, "IR stack underflow on file write", error, handle, rawValue)) {
        return false;
      }
      const int fd = static_cast<int>(handle & 0xffffffffu);
      const std::string text = std::to_string(static_cast<int64_t>(static_cast<int32_t>(rawValue)));
      stack.push_back(static_cast<uint64_t>(writeAll(fd, text.data(), text.size())));
      return true;
    }
    case IrOpcode::FileWriteI64: {
      uint64_t handle = 0;
      uint64_t rawValue = 0;
      if (!popStackPair(stack, "IR stack underflow on file write", error, handle, rawValue)) {
        return false;
      }
      const int fd = static_cast<int>(handle & 0xffffffffu);
      const std::string text = std::to_string(static_cast<int64_t>(rawValue));
      stack.push_back(static_cast<uint64_t>(writeAll(fd, text.data(), text.size())));
      return true;
    }
    case IrOpcode::FileWriteU64: {
      uint64_t handle = 0;
      uint64_t rawValue = 0;
      if (!popStackPair(stack, "IR stack underflow on file write", error, handle, rawValue)) {
        return false;
      }
      const int fd = static_cast<int>(handle & 0xffffffffu);
      const std::string text = std::to_string(static_cast<unsigned long long>(rawValue));
      stack.push_back(static_cast<uint64_t>(writeAll(fd, text.data(), text.size())));
      return true;
    }
    case IrOpcode::FileWriteString: {
      uint64_t handle = 0;
      if (!popStackValue(stack, "IR stack underflow on file write", error, handle)) {
        return false;
      }
      const std::string *text = nullptr;
      if (!resolveStringIndex(module, inst.imm, error, text)) {
        return false;
      }
      const int fd = static_cast<int>(handle & 0xffffffffu);
      stack.push_back(static_cast<uint64_t>(writeAll(fd, text->data(), text->size())));
      return true;
    }
    case IrOpcode::FileWriteStringDynamic: {
      uint64_t handle = 0;
      uint64_t stringIndex = 0;
      if (!popStackPair(stack, "IR stack underflow on file write", error, handle, stringIndex)) {
        return false;
      }
      const std::string *text = nullptr;
      if (!resolveStringIndex(module, stringIndex, error, text)) {
        return false;
      }
      const int fd = static_cast<int>(handle & 0xffffffffu);
      stack.push_back(static_cast<uint64_t>(writeAll(fd, text->data(), text->size())));
      return true;
    }
    case IrOpcode::FileWriteByte: {
      uint64_t handle = 0;
      uint64_t rawValue = 0;
      if (!popStackPair(stack, "IR stack underflow on file write", error, handle, rawValue)) {
        return false;
      }
      const uint8_t value = static_cast<uint8_t>(rawValue & 0xffu);
      const int fd = static_cast<int>(handle & 0xffffffffu);
      stack.push_back(static_cast<uint64_t>(writeAll(fd, &value, 1)));
      return true;
    }
    case IrOpcode::FileWriteNewline: {
      uint64_t handle = 0;
      if (!popStackValue(stack, "IR stack underflow on file write", error, handle)) {
        return false;
      }
      const int fd = static_cast<int>(handle & 0xffffffffu);
      const char newline = '\n';
      stack.push_back(static_cast<uint64_t>(writeAll(fd, &newline, 1)));
      return true;
    }
    default:
      error = "unsupported file opcode in VM helper";
      return false;
  }
}

} // namespace primec::vm_detail
