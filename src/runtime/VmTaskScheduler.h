#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "primec/ir/Ir.h"
#include "primec/runtime/VmExecutionKernel.h"

namespace primec::vm_detail {

class VmTaskScheduler;

// A task of a stepped run, for a debug session to show: its call stack (innermost frame last) and operand stack.
struct VmSteppedTaskView {
  uint64_t number = 0;
  const std::vector<VmKernelFrame> *frames = nullptr;
  const std::vector<uint64_t> *stack = nullptr;
};

// The task scheduler run one step at a time on the step kernel, deterministic (eager, or `seed`): what a debug
// session steps when the program spawns tasks. `module`, `host` and `localCounts` outlive it.
class VmSteppedTasks {
public:
  VmSteppedTasks(const IrModule &module,
                 VmKernelHost &host,
                 const std::vector<size_t> &localCounts,
                 std::optional<uint64_t> seed);
  ~VmSteppedTasks();
  VmSteppedTasks(const VmSteppedTasks &) = delete;
  VmSteppedTasks &operator=(const VmSteppedTasks &) = delete;

  // One instruction of the current task, or one scheduler opcode. Exit ends the program with `result`.
  VmKernelStepOutcome step(VmKernelStepEvent &event, uint64_t &result, std::string &error);
  // The task that runs next (the main task is number 0).
  VmSteppedTaskView currentView() const;
  // The tasks that have not ended, in number order.
  void listLive(std::vector<VmSteppedTaskView> &out) const;

private:
  std::unique_ptr<VmTaskScheduler> impl_;
};

// Whether the module holds a TaskSpawn or a TaskWait (docs/spec/vm-design.md, Tasks).
bool moduleUsesTasks(const IrModule &module);

// Runs a module with tasks under the VM's task scheduler (src/runtime/VmTaskScheduler.cpp): every task on the flat
// loop between the scheduler's opcodes when the loop takes the module, else on the step kernel.
bool runVmTaskScheduler(const IrModule &module,
                        VmKernelHost &host,
                        uint64_t &result,
                        std::string &error);

// One step of the step kernel without the event bookkeeping debug sessions need.
VmKernelStepOutcome stepVmKernelUntracked(const IrModule &module,
                                          VmKernelHost &host,
                                          std::vector<uint64_t> &stack,
                                          std::vector<VmKernelFrame> &frames,
                                          const std::vector<size_t> &localCounts,
                                          uint64_t &result,
                                          VmKernelStepEvent &event,
                                          std::string &error);

} // namespace primec::vm_detail
