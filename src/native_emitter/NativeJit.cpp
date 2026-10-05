#include "primec/backend/NativeJit.h"

#include "NativeJitOpcodes.h"
#include "../runtime/VmNativeJitHost.h"
#include "primec/backend/NativeEmitter.h"

#include <cstdio>
#include <cstring>

#if defined(__linux__) && defined(__x86_64__)
#include <sys/mman.h>
#endif

namespace primec {

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
  if (!module.hostImports.empty()) {
    reason = "host imports";
    return false;
  }
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &instruction : function.instructions) {
      if (!native_emitter::nativeJitRunsOpcodeInline(instruction.op) &&
          !native_emitter::nativeJitBridgesOpcode(instruction.op)) {
        reason =
            "opcode " + std::to_string(static_cast<int>(instruction.op)) + " in " + function.name;
        return false;
      }
      if ((instruction.op == IrOpcode::Call || instruction.op == IrOpcode::CallVoid) &&
          instruction.imm >= module.functions.size()) {
        reason = "call target";
        return false;
      }
      // The code materializes an f32 constant from its low 32 bits.
      if (instruction.op == IrOpcode::PushF32 && instruction.imm > UINT32_MAX) {
        reason = "f32 constant";
        return false;
      }
      // Frame addresses are checked against the local count with a 32-bit immediate.
      if ((instruction.op == IrOpcode::LoadLocal || instruction.op == IrOpcode::StoreLocal ||
           instruction.op == IrOpcode::AddressOfLocal) &&
          instruction.imm >= INT32_MAX / IrSlotBytes) {
        reason = "local index";
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
  constexpr uint64_t MaxStackBytes = uint64_t{1} << 35; // reserved, not committed
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

  auto *data = reinterpret_cast<uint64_t *>(static_cast<uint8_t *>(code) + image.dataOffset);
  vm_detail::VmNativeJitHost host(module, args);
  host.attach(data);

  using Trampoline = uint64_t (*)(uint64_t argc, char **argv, void *stackTop);
  const auto trampoline =
      reinterpret_cast<Trampoline>(static_cast<uint8_t *>(code) + image.trampolineOffset);
  void *stackTop = static_cast<uint8_t *>(stack) + PageBytes + stackBytes;
  const uint64_t result = trampoline(args.size(), argv.data(), stackTop);

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
  case 7:
    outcome.error = "unaligned indirect address in IR: " + std::to_string(argument);
    break;
  case 8:
    outcome.error = "invalid indirect address in IR: " + std::to_string(argument);
    break;
  case 9:
    outcome.error = host.error();
    break;
  default:
    outcome.error = "native JIT fault " + std::to_string(fault);
    break;
  }
  return outcome;
#endif
}

} // namespace primec
