#include "primec/Vm.h"

#include <algorithm>

namespace primec {
namespace {

size_t vmDebugLocalCount(const IrFunction &function) {
  size_t localCount = 0;
  for (const auto &inst : function.instructions) {
    if (inst.op == IrOpcode::LoadLocal || inst.op == IrOpcode::StoreLocal || inst.op == IrOpcode::AddressOfLocal) {
      localCount = std::max(localCount, static_cast<size_t>(inst.imm) + 1);
    }
  }
  return localCount;
}

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
  frames_.clear();
  result_ = 0;
  pauseRequested_ = false;
  lastStopWasBreakpoint_ = false;
  breakpoints_.clear();
  nextHookSequence_ = 0;
  for (size_t i = 0; i < module.functions.size(); ++i) {
    localCounts_[i] = vmDebugLocalCount(module.functions[i]);
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
  copyOwnedArgv(args, ownedArgStorage_, ownedArgViews_);
  if (!initFromModule(module, static_cast<uint64_t>(ownedArgViews_.size()), &ownedArgViews_)) {
    error = "failed to initialize debug session";
    return false;
  }
  return true;
}

} // namespace primec
