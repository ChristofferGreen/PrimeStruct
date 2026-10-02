#include "primec/runtime/Vm.h"

#include "primec/runtime/VmStringHeap.h"

#include <algorithm>

namespace primec {
namespace {

void copyOwnedArgv(const std::vector<std::string_view> &args,
                   std::vector<std::string> &ownedArgStorage,
                   std::vector<std::string_view> &ownedArgViews) {
  ownedArgStorage.clear();
  ownedArgViews.clear();
  ownedArgStorage.reserve(args.size());
  ownedArgViews.reserve(args.size());
  for (const std::string_view arg : args) {
    ownedArgStorage.emplace_back(arg);
  }
  for (const std::string &arg : ownedArgStorage) {
    ownedArgViews.emplace_back(arg);
  }
}

} // namespace

bool VmDebugSession::initFromModule(const IrModule &module,
                                    uint64_t argCount,
                                    const std::vector<std::string_view> *args) {
  module_ = &module;
  argCount_ = argCount;
  argvViews_ = args;
  localCounts_.assign(module.functions.size(), 0);
  stack_.clear();
  heapSlots_.clear();
  heapAllocations_.clear();
  stringHeap_ = std::make_shared<vm_detail::VmStringHeap>();
  frames_.clear();
  result_ = 0;
  pauseRequested_ = false;
  lastStopWasBreakpoint_ = false;
  breakpoints_.clear();
  nextHookSequence_ = 0;
  for (size_t i = 0; i < module.functions.size(); ++i) {
    localCounts_[i] = vm_detail::computeVmKernelLocalCount(module.functions[i]);
  }

  Frame entryFrame;
  entryFrame.functionIndex = static_cast<size_t>(module.entryIndex);
  entryFrame.function = &module.functions[entryFrame.functionIndex];
  entryFrame.locals.assign(localCounts_[entryFrame.functionIndex], 0);
  frames_.push_back(std::move(entryFrame));

  const VmDebugTransitionResult startTransition =
      vmDebugApplyCommand(VmDebugSessionState::Idle, VmDebugSessionCommand::Start);
  if (!startTransition.valid) {
    return false;
  }
  state_ = startTransition.state;
  const VmDebugTransitionResult pauseTransition = vmDebugApplyStopReason(state_, VmDebugStopReason::Pause);
  if (!pauseTransition.valid) {
    return false;
  }
  state_ = pauseTransition.state;
  return true;
}

bool VmDebugSession::start(const IrModule &module, std::string &error, uint64_t argCount) {
  if (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
    error = "invalid IR entry index";
    return false;
  }
  ownedArgStorage_.clear();
  ownedArgViews_.clear();
  hostFunctions_.reset();
  if (!initFromModule(module, argCount, nullptr)) {
    error = "failed to initialize debug session";
    return false;
  }
  return true;
}

bool VmDebugSession::start(const IrModule &module,
                          std::string &error,
                          const std::vector<std::string_view> &args) {
  if (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
    error = "invalid IR entry index";
    return false;
  }
  hostFunctions_.reset();
  copyOwnedArgv(args, ownedArgStorage_, ownedArgViews_);
  if (!initFromModule(module, static_cast<uint64_t>(ownedArgViews_.size()), &ownedArgViews_)) {
    error = "failed to initialize debug session";
    return false;
  }
  return true;
}

bool VmDebugSession::start(const IrModule &module,
                          std::string &error,
                          const std::vector<std::string_view> &args,
                          const VmHostFunctions &hostFunctions) {
  if (!hostFunctions.verify(module, error)) {
    return false;
  }
  if (!start(module, error, args)) {
    return false;
  }
  hostFunctions_ = hostFunctions;
  return true;
}

} // namespace primec
