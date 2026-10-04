#include "primec/backend/NativeJit.h"

#include "primec/backend/NativeEmitter.h"

#include <cstdio>
#include <cstring>

#if defined(__linux__) && defined(__x86_64__)
#include <sys/mman.h>
#endif

namespace primec {
namespace {

bool opcodeHasNativeVmSemantics(IrOpcode op) {
  switch (op) {
  case IrOpcode::PushI32:
  case IrOpcode::PushI64:
  case IrOpcode::PushF64:
  case IrOpcode::LoadLocal:
  case IrOpcode::StoreLocal:
  case IrOpcode::Dup:
  case IrOpcode::Pop:
  case IrOpcode::AddI32:
  case IrOpcode::SubI32:
  case IrOpcode::MulI32:
  case IrOpcode::DivI32:
  case IrOpcode::NegI32:
  case IrOpcode::AddI64:
  case IrOpcode::SubI64:
  case IrOpcode::MulI64:
  case IrOpcode::DivI64:
  case IrOpcode::DivU64:
  case IrOpcode::NegI64:
  case IrOpcode::SextI32:
  case IrOpcode::CmpEqI32:
  case IrOpcode::CmpNeI32:
  case IrOpcode::CmpLtI32:
  case IrOpcode::CmpLeI32:
  case IrOpcode::CmpGtI32:
  case IrOpcode::CmpGeI32:
  case IrOpcode::CmpEqI64:
  case IrOpcode::CmpNeI64:
  case IrOpcode::CmpLtI64:
  case IrOpcode::CmpLeI64:
  case IrOpcode::CmpGtI64:
  case IrOpcode::CmpGeI64:
  case IrOpcode::CmpLtU64:
  case IrOpcode::CmpLeU64:
  case IrOpcode::CmpGtU64:
  case IrOpcode::CmpGeU64:
  case IrOpcode::AddF64:
  case IrOpcode::SubF64:
  case IrOpcode::MulF64:
  case IrOpcode::DivF64:
  case IrOpcode::NegF64:
  case IrOpcode::CmpEqF64:
  case IrOpcode::CmpNeF64:
  case IrOpcode::CmpLtF64:
  case IrOpcode::CmpLeF64:
  case IrOpcode::CmpGtF64:
  case IrOpcode::CmpGeF64:
  case IrOpcode::ConvertI32ToF64:
  case IrOpcode::ConvertI64ToF64:
  case IrOpcode::ConvertU64ToF64:
  case IrOpcode::ConvertF64ToI64:
  case IrOpcode::JumpIfZero:
  case IrOpcode::Jump:
  case IrOpcode::Call:
  case IrOpcode::CallVoid:
  case IrOpcode::ReturnVoid:
  case IrOpcode::ReturnI32:
  case IrOpcode::ReturnI64:
  case IrOpcode::ReturnF64:
  case IrOpcode::PrintI32:
  case IrOpcode::PrintI64:
  case IrOpcode::PrintU64:
  case IrOpcode::PrintString:
  case IrOpcode::PushArgc:
  case IrOpcode::LoadStringByte:
  case IrOpcode::LoadStringLength:
    return true;
  default:
    return false;
  }
}

} // namespace

bool nativeJitAccepts(const IrModule &module, std::string &reason) {
#if !(defined(__linux__) && defined(__x86_64__))
  (void)module;
  reason = "native JIT needs Linux x86_64";
  return false;
#else
  if (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size() ||
      module.functions[static_cast<size_t>(module.entryIndex)].parameterCount != 0) {
    reason = "entry function";
    return false;
  }
  if (module.stringTable.size() > INT32_MAX) {
    reason = "too many strings";
    return false;
  }
  for (const std::string &text : module.stringTable) {
    if (text.size() > INT32_MAX) {
      reason = "string too long";
      return false;
    }
  }
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &instruction : function.instructions) {
      if (!opcodeHasNativeVmSemantics(instruction.op)) {
        reason =
            "opcode " + std::to_string(static_cast<int>(instruction.op)) + " in " + function.name;
        return false;
      }
      if ((instruction.op == IrOpcode::Call || instruction.op == IrOpcode::CallVoid) &&
          instruction.imm >= module.functions.size()) {
        reason = "call target";
        return false;
      }
    }
  }
  return true;
#endif
}

NativeJitResult runNativeJit(const IrModule &module, const std::vector<std::string_view> &args) {
  NativeJitResult outcome;
#if !(defined(__linux__) && defined(__x86_64__))
  (void)module;
  (void)args;
  outcome.reason = "native JIT needs Linux x86_64";
  return outcome;
#else
  if (!nativeJitAccepts(module, outcome.reason)) {
    return outcome;
  }
  NativeJitImage image;
  if (!NativeEmitter().emitJitImage(module, image, outcome.reason)) {
    return outcome;
  }
  constexpr uint64_t PageBytes = 4096;
  constexpr uint64_t MaxStackBytes = uint64_t{1} << 30;
  if (image.stackBytes > MaxStackBytes) {
    outcome.reason = "frames too large for the call depth limit";
    return outcome;
  }
  void *code =
      mmap(nullptr, image.bytes.size(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (code == MAP_FAILED) {
    outcome.reason = "mmap failed";
    return outcome;
  }
  std::memcpy(code, image.bytes.data(), image.bytes.size());
  const uint64_t stackBytes = (image.stackBytes + PageBytes - 1) / PageBytes * PageBytes;
  void *stack = mmap(nullptr,
                     stackBytes + PageBytes,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_STACK,
                     -1,
                     0);
  if (mprotect(code, image.codeBytes, PROT_READ | PROT_EXEC) != 0 || stack == MAP_FAILED ||
      mprotect(stack, PageBytes, PROT_NONE) != 0) { // guard page below the stack
    munmap(code, image.bytes.size());
    if (stack != MAP_FAILED) {
      munmap(stack, stackBytes + PageBytes);
    }
    outcome.reason = "mmap failed";
    return outcome;
  }
  std::vector<std::string> argStorage(args.begin(), args.end());
  std::vector<char *> argv;
  for (std::string &arg : argStorage) {
    argv.push_back(arg.data());
  }
  argv.push_back(nullptr);
  // The program writes to the descriptors directly; flush whatever this process buffered.
  std::fflush(nullptr);

  using Trampoline = uint64_t (*)(uint64_t argc, char **argv, void *stackTop);
  const auto trampoline =
      reinterpret_cast<Trampoline>(static_cast<uint8_t *>(code) + image.trampolineOffset);
  void *stackTop = static_cast<uint8_t *>(stack) + PageBytes + stackBytes;
  const uint64_t result = trampoline(args.size(), argv.data(), stackTop);

  const auto *data =
      reinterpret_cast<const uint64_t *>(static_cast<uint8_t *>(code) + image.dataOffset);
  const uint64_t depth = data[1];
  const uint64_t fault = data[2];
  const uint64_t argument = data[3];
  munmap(stack, stackBytes + PageBytes);
  munmap(code, image.bytes.size());

  outcome.executed = true;
  switch (fault) {
  case 0:
    outcome.ok = true;
    outcome.result = result;
    break;
  case 1:
    outcome.error = "division by zero in IR";
    break;
  case 2:
    outcome.error = "string index out of bounds in IR";
    break;
  case 3:
    outcome.error = "VM call stack overflow";
    break;
  case 4:
    outcome.error = depth == 0 || argument >= module.functions.size()
                        ? std::string("missing return in IR")
                        : "missing return in IR function " +
                              module.functions[static_cast<size_t>(argument)].name;
    break;
  case 5:
    outcome.error = "invalid dynamic string index in IR";
    break;
  case 6:
    outcome.error = "invalid string index in IR";
    break;
  default:
    outcome.error = "native JIT fault " + std::to_string(fault);
    break;
  }
  return outcome;
#endif
}

} // namespace primec
