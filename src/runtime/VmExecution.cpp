#include "VmExecution.h"

#include "VmHeapHelpers.h"
#include "VmIoHelpers.h"
#include "primec/runtime/VmExecutionKernel.h"
#include "primec/runtime/VmTextHeap.h"

#include <exception>
#include <string>
#include <string_view>
#include <vector>

namespace primec::vm_detail {

bool handleVmHostCall(const VmHostFunctions *hostFunctions,
                      const IrModule &module,
                      const IrInstruction &inst,
                      std::vector<uint64_t> &stack,
                      std::string &error,
                      TextHeapContext *text) {
  if (inst.imm >= module.hostImports.size()) {
    error = "invalid host import index in IR";
    return false;
  }
  const IrHostImport &import = module.hostImports[static_cast<size_t>(inst.imm)];
  const std::string_view textName = textHostFunctionName(import.name);
  if (text != nullptr && !textName.empty()) {
    const size_t textArgCount = import.parameters.size();
    if (stack.size() < textArgCount) {
      error = "IR stack underflow on host call " + import.name;
      return false;
    }
    const size_t textBase = stack.size() - textArgCount;
    uint64_t textResult = 0;
    bool known = false;
    if (textName == "__pstext_argument") {
      // The entry argument at the index, copied into a fresh string-heap block (0 for none or empty).
      const int64_t index = static_cast<int32_t>(stack[textBase]);
      std::string_view argument;
      if (text->args != nullptr && index >= 0 && static_cast<size_t>(index) < text->args->size()) {
        argument = (*text->args)[static_cast<size_t>(index)];
      }
      if (!text->heap->allocate(*text->core, argument.size() + (argument.empty() ? 0 : 1), textResult, error)) {
        return false;
      }
      if (!argument.empty()) {
        std::copy(argument.begin(), argument.end(), reinterpret_cast<char *>(text->core->byteAt(textResult)));
      }
      stack.resize(textBase);
      stack.push_back(textResult);
      return true;
    }
    if (!callTextHostFunction(*text->heap, *text->core, textName, stack.data() + textBase, textResult, known, error)) {
      return false;
    }
    if (known) {
      stack.resize(textBase);
      if (import.returnKind != IrHostValueKind::Void) {
        stack.push_back(textResult);
      }
      return true;
    }
  }
  const VmHostBinding *binding = hostFunctions != nullptr ? hostFunctions->find(import.name) : nullptr;
  if (binding == nullptr || (!binding->invoke && !binding->invokeString)) {
    error = "unbound host function: " + import.name;
    return false;
  }
  const size_t argCount = import.parameters.size();
  if (stack.size() < argCount) {
    error = "IR stack underflow on host call " + import.name;
    return false;
  }
  const size_t base = stack.size() - argCount;
  // Text parameters: the host receives a pointer to a string_view over the NUL-terminated bytes (valid for
  // the duration of the call); `textViews` keeps those views at stable addresses.
  std::vector<std::string_view> textViews;
  textViews.reserve(argCount);
  for (size_t i = 0; i < argCount; ++i) {
    if (import.parameters[i] == IrHostValueKind::Text) {
      uint64_t &slot = stack[base + i];
      std::string_view view;
      const std::string *literal = nullptr;
      std::string literalError;
      if (slot != 0 && !VmHeapCore::isByteAddress(slot) &&
          resolveVmString(module, slot, literal, literalError)) {
        view = *literal;
      } else if (slot == 0) {
        view = std::string_view();
      } else if (text != nullptr && VmHeapCore::isByteAddress(slot)) {
        const uint8_t *first = text->core->byteAt(slot);
        if (first == nullptr) {
          error = "invalid text address passed to host function " + import.name;
          return false;
        }
        const uint8_t *end = text->core->bytes.data() + text->core->bytes.size();
        const uint8_t *last = first;
        while (last != end && *last != 0) {
          ++last;
        }
        view = std::string_view(reinterpret_cast<const char *>(first), static_cast<size_t>(last - first));
      } else {
        error = "invalid text passed to host function " + import.name;
        return false;
      }
      textViews.push_back(view);
      slot = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&textViews.back()));
      continue;
    }
    if (import.parameters[i] == IrHostValueKind::String) {
      uint64_t &slot = stack[base + i];
      const std::string *text = nullptr;
      std::string lookupError;
      if (!resolveVmString(module, slot, text, lookupError)) {
        error = "invalid string index passed to host function " + import.name;
        return false;
      }
      slot = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(text));
    }
  }
  uint64_t result = 0;
  std::string hostError;
  bool ok = false;
  if (import.returnKind == IrHostValueKind::String) {
    error = "host function " + import.name + " returns a string, which a host cannot do; it returns text as a String";
    return false;
  }
  const bool returnsText = import.returnKind == IrHostValueKind::Text && binding->invokeString;
  if (returnsText && text == nullptr) {
    error = "host function " + import.name + " returns text, which this VM host does not support";
    return false;
  }
  try {
    if (returnsText) {
      std::string produced;
      ok = binding->invokeString(stack.data() + base, produced, hostError);
      if (ok) {
        // The text becomes a fresh string-heap block (0 for empty text) that the program owns.
        std::string allocateError;
        if (!text->heap->allocate(*text->core, produced.size() + (produced.empty() ? 0 : 1), result, allocateError)) {
          ok = false;
          hostError = allocateError;
        } else if (!produced.empty()) {
          uint8_t *block = text->core->byteAt(result);
          std::copy(produced.begin(), produced.end(), reinterpret_cast<char *>(block));
        }
      }
    } else {
      ok = binding->invoke(stack.data() + base, result, hostError);
    }
  } catch (const std::exception &exception) {
    hostError = std::string("exception: ") + exception.what();
  } catch (...) {
    hostError = "unknown exception";
  }
  if (!ok) {
    error = "host function " + import.name + " failed" + (hostError.empty() ? "" : ": " + hostError);
    return false;
  }
  stack.resize(base);
  switch (import.returnKind) {
  case IrHostValueKind::Void:
    break;
  case IrHostValueKind::I32:
    stack.push_back(static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(result))));
    break;
  case IrHostValueKind::Bool:
    stack.push_back(result != 0 ? 1u : 0u);
    break;
  case IrHostValueKind::U8:
    stack.push_back(result & 0xFFull);
    break;
  case IrHostValueKind::I64:
  case IrHostValueKind::U64:
  case IrHostValueKind::F64:
  case IrHostValueKind::String:
  case IrHostValueKind::Text:
    stack.push_back(result);
    break;
  case IrHostValueKind::F32:
    stack.push_back(result & 0xFFFFFFFFull);
    break;
  }
  return true;
}

namespace {

class RuntimeVmKernelHost final : public VmKernelHost {
public:
  RuntimeVmKernelHost(uint64_t argCount,
                      const std::vector<std::string_view> *args,
                      const VmHostFunctions *hostFunctions,
                      const VmOutputSink *outputSink,
                      const std::atomic<bool> *cancel)
      : argCount_(argCount), hostFunctions_(hostFunctions), args_(args), outputSink_(outputSink), cancel_(cancel) {
  }

  const std::atomic<bool> *cancelFlag() const override { return cancel_; }

  uint64_t argumentCount() const override { return argCount_; }
  uint64_t slotBytes() const override { return IrSlotBytes; }
  size_t maxCallDepth() const override { return 4096; }

  bool resolveIndirectAddress(uint64_t address,
                              std::vector<uint64_t> &locals,
                              uint64_t *&slot,
                              std::string &error) override {
    return vm_detail::resolveIndirectAddress(address, slotBytes(), locals, heap_, slot, error);
  }

  bool allocateHeapSlots(uint64_t slotCount, uint64_t &address, std::string &error) override {
    return heap_.allocate(slotCount, address, error);
  }

  bool allocateHeapBytes(uint64_t byteCount, uint64_t &address, std::string &error) override {
    return heap_.allocateBytes(byteCount, address, error);
  }

  bool loadHeapByte(uint64_t address, std::vector<uint64_t> &locals, uint8_t &value, std::string &error) override {
    return vm_detail::loadHeapByte(address, slotBytes(), locals, heap_, value, error);
  }

  bool storeHeapByte(uint64_t address, std::vector<uint64_t> &locals, uint8_t value, std::string &error) override {
    return vm_detail::storeHeapByte(address, slotBytes(), locals, heap_, value, error);
  }

  bool freeHeapSlots(uint64_t address, std::string &error) override {
    return heap_.release(address, error);
  }

  bool reallocHeapSlots(uint64_t address,
                        uint64_t slotCount,
                        uint64_t &newAddress,
                        std::string &error) override {
    return heap_.reallocate(address, slotCount, newAddress, error);
  }

  bool handlePrintInstruction(const IrModule &module,
                              const IrInstruction &inst,
                              std::vector<uint64_t> &stack,
                              std::string &error) override {
    return handlePrintOpcode(module, inst, stack, args_, error, outputSink_);
  }

  bool handleFileInstruction(const IrModule &module,
                             const IrInstruction &inst,
                             std::vector<uint64_t> &stack,
                             std::vector<uint64_t> &locals,
                             std::string &error) override {
    return handleFileOpcode(module, inst, stack, locals, error, outputSink_);
  }

  bool handleHostCall(const IrModule &module,
                      const IrInstruction &inst,
                      std::vector<uint64_t> &stack,
                      std::string &error) override {
    TextHeapContext text{&heap_, &textHeap_, args_};
    return handleVmHostCall(hostFunctions_, module, inst, stack, error, &text);
  }

private:
  uint64_t argCount_ = 0;
  const VmHostFunctions *hostFunctions_ = nullptr;
  const std::vector<std::string_view> *args_ = nullptr;
  const VmOutputSink *outputSink_ = nullptr;
  const std::atomic<bool> *cancel_ = nullptr;
  VmHeapCore heap_;
  VmTextHeap textHeap_;
};

} // namespace

bool executeVmModule(const IrModule &module,
                     uint64_t &result,
                     std::string &error,
                     uint64_t argCount,
                     const std::vector<std::string_view> *args,
                     const VmHostFunctions *hostFunctions,
                     const VmOutputSink *outputSink,
                     const std::atomic<bool> *cancel) {
  if (!module.hostImports.empty()) {
    // Not a function-local static: statics must not hold arena-allocated state.
    const VmHostFunctions noBindings;
    if (!(hostFunctions != nullptr ? *hostFunctions : noBindings).verify(module, error)) {
      return false;
    }
  }
  RuntimeVmKernelHost host(argCount, args, hostFunctions, outputSink, cancel);
  return executeVmKernel(module, host, result, error);
}

} // namespace primec::vm_detail
