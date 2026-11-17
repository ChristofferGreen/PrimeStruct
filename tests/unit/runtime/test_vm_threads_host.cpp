#include "primec/ir/Ir.h"
#include "primec/runtime/Vm.h"
#include "primec/runtime/VmHost.h"

#include "third_party/doctest.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using namespace primec;

// Host calls under the VM's threads mode (docs/spec/vm-design.md, Tasks on threads; `--vm-threads`) with hand-built
// IR: a host function registered reentrant runs without the global host lock, a main-thread-only one faults off the
// main thread, and the default is serialized. Nothing here sleeps or measures time: a program either has one answer
// whatever the interleaving or faults with a diagnostic, a call that must overlap another waits for it on a
// counter, and each case runs its program Repeats times, so a race shows as a failure of the whole entry.
TEST_SUITE_BEGIN("primestruct.runtime.vm_threads_host");

namespace {

constexpr int Repeats = 20;
constexpr uint64_t Newline = PrintFlagNewline;

IrFunction
functionOf(std::string name, uint32_t parameterCount, std::vector<IrInstruction> instructions) {
  IrFunction function;
  function.name = std::move(name);
  function.parameterCount = parameterCount;
  function.instructions = std::move(instructions);
  return function;
}

// Host imports: index 0 is `first`, index 1 is `second`; both take nothing and return nothing.
IrModule moduleOf(std::vector<IrFunction> functions, std::vector<std::string> imports) {
  IrModule module;
  module.functions = std::move(functions);
  module.entryIndex = 0;
  for (std::string &name : imports) {
    module.hostImports.push_back({std::move(name), {}, IrHostValueKind::Void});
  }
  return module;
}

struct Run {
  bool ok = false;
  std::string error;
  std::string out;
};

void appendOutput(int fd, std::string_view chunk, void *userData) {
  if (fd == 1) {
    static_cast<std::string *>(userData)->append(chunk);
  }
}

Run run(const IrModule &module, const VmHostFunctions &hosts, bool threads) {
  Run result;
  Vm vm;
  vm.setOutputSink({&appendOutput, &result.out});
  vm.setTaskThreads(threads);
  uint64_t raw = 0;
  result.ok = vm.execute(module, raw, result.error, std::vector<std::string_view>{"prog"}, hosts);
  return result;
}

template <class F>
void bindHost(VmHostFunctions &hosts, std::string name, VmHostThreading threading, F body) {
  VmHostBinding binding;
  binding.returnKind = IrHostValueKind::Void;
  binding.threading = threading;
  binding.invoke = [body = std::move(body)](const uint64_t *,
                                            uint64_t &,
                                            std::string &error) mutable { return body(error); };
  hosts.bind(std::move(name), std::move(binding));
}

void append(std::vector<IrInstruction> &out, std::vector<IrInstruction> more) {
  out.insert(out.end(), more.begin(), more.end());
}

std::vector<IrInstruction> callOf(uint64_t import) {
  return {{IrOpcode::CallHost, import}};
}

// A task with no parameter that calls host import `import` `count` times.
IrFunction callingTask(std::string name, uint64_t import, int64_t count) {
  std::vector<IrInstruction> code = {{IrOpcode::PushI64, 0}, {IrOpcode::StoreLocal, 0}};
  const uint64_t head = code.size();
  append(code,
         {{IrOpcode::LoadLocal, 0},
          {IrOpcode::PushI64, static_cast<uint64_t>(count)},
          {IrOpcode::CmpLtI64, 0}});
  const uint64_t exitJump = code.size();
  code.push_back({IrOpcode::JumpIfZero, 0});
  append(code, callOf(import));
  append(code,
         {{IrOpcode::LoadLocal, 0},
          {IrOpcode::PushI64, 1},
          {IrOpcode::AddI64, 0},
          {IrOpcode::StoreLocal, 0},
          {IrOpcode::Jump, head}});
  code[exitJump].imm = code.size();
  code.push_back({IrOpcode::ReturnVoid, 0});
  return functionOf(std::move(name), 0, std::move(code));
}

// A task that calls host import `import` once.
IrFunction oneCallTask(std::string name, uint64_t import) {
  return callingTask(std::move(name), import, 1);
}

// Instructions of main: spawns function `function` (no arguments) into local `handle`.
std::vector<IrInstruction> spawnOf(uint64_t function, uint64_t handle) {
  return {{IrOpcode::TaskSpawn, function}, {IrOpcode::StoreLocal, handle}};
}

std::vector<IrInstruction> waitTask(uint64_t handle) {
  return {{IrOpcode::LoadLocal, handle}, {IrOpcode::TaskWait, 0}, {IrOpcode::Pop, 0}};
}

std::vector<IrInstruction> finishWithOne() {
  return {{IrOpcode::PushI32, 1},
          {IrOpcode::PrintI32, Newline},
          {IrOpcode::PushI32, 0},
          {IrOpcode::ReturnI32, 0}};
}

} // namespace

TEST_CASE("a serialized host function is never entered by two threads at once") {
  // main: four tasks of 50 calls each (and the default threading, which is serialized), then prints 1.
  std::vector<IrInstruction> main;
  for (uint64_t task = 0; task < 4; ++task) {
    append(main, spawnOf(1, task));
  }
  for (uint64_t task = 0; task < 4; ++task) {
    append(main, waitTask(task));
  }
  append(main, finishWithOne());
  const IrModule module =
      moduleOf({functionOf("/main", 0, std::move(main)), callingTask("/call", 0, 50)}, {"enter"});
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    CAPTURE(repeat);
    std::atomic<int> inside{0};
    std::atomic<int> calls{0};
    VmHostFunctions hosts;
    bindHost(hosts, "enter", VmHostThreading::Serialized, [&](std::string &error) {
      if (inside.fetch_add(1) != 0) {
        error = "entered while another thread was inside";
        inside.fetch_sub(1);
        return false;
      }
      // A fixed number of yields gives another thread every chance to enter; no clock is read.
      for (int turn = 0; turn < 5; ++turn) {
        std::this_thread::yield();
      }
      inside.fetch_sub(1);
      calls.fetch_add(1);
      return true;
    });
    const Run threaded = run(module, hosts, true);
    REQUIRE_MESSAGE(threaded.ok, threaded.error);
    CHECK(threaded.out == "1\n");
    CHECK(calls.load() == 200);
  }
}

TEST_CASE("two tasks inside one reentrant host function at the same time finish") {
  // Each task calls `meet`, which returns only once two threads are inside it. Under the host lock neither could
  // finish; the run ends only when the calls overlap.
  std::vector<IrInstruction> main = spawnOf(1, 0);
  append(main, spawnOf(1, 1));
  append(main, waitTask(0));
  append(main, waitTask(1));
  append(main, finishWithOne());
  const IrModule module =
      moduleOf({functionOf("/main", 0, std::move(main)), oneCallTask("/meet", 0)}, {"meet"});
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    CAPTURE(repeat);
    std::atomic<int> arrived{0};
    VmHostFunctions hosts;
    bindHost(hosts, "meet", VmHostThreading::Reentrant, [&](std::string &) {
      arrived.fetch_add(1);
      while (arrived.load() < 2) {
        std::this_thread::yield();
      }
      return true;
    });
    const Run threaded = run(module, hosts, true);
    REQUIRE_MESSAGE(threaded.ok, threaded.error);
    CHECK(threaded.out == "1\n");
  }
}

TEST_CASE("a reentrant call that blocks is released by a call from another task") {
  // The task blocks in `hold` until main calls `release`; both are reentrant, so the call that waits holds nothing
  // the call that releases needs.
  std::vector<IrInstruction> main = spawnOf(1, 0);
  append(main, callOf(1));
  append(main, waitTask(0));
  append(main, finishWithOne());
  const IrModule module = moduleOf(
      {functionOf("/main", 0, std::move(main)), oneCallTask("/hold", 0)}, {"hold", "release"});
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    CAPTURE(repeat);
    std::atomic<bool> released{false};
    VmHostFunctions hosts;
    bindHost(hosts, "hold", VmHostThreading::Reentrant, [&](std::string &) {
      while (!released.load()) {
        std::this_thread::yield();
      }
      return true;
    });
    bindHost(hosts, "release", VmHostThreading::Reentrant, [&](std::string &) {
      released.store(true);
      return true;
    });
    const Run threaded = run(module, hosts, true);
    REQUIRE_MESSAGE(threaded.ok, threaded.error);
    CHECK(threaded.out == "1\n");
  }
}

TEST_CASE("a blocked reentrant call does not stop a serialized call or a print of another task") {
  // The task blocks in `hold`. Main then calls the serialized `ping` and prints a line (both need the host lock),
  // and only then calls `release`; if `hold` kept the lock, main would never get past `ping`.
  std::vector<IrInstruction> main = spawnOf(1, 0);
  append(main, callOf(1));
  append(main, {{IrOpcode::PushI32, 7}, {IrOpcode::PrintI32, Newline}});
  append(main, callOf(2));
  append(main, waitTask(0));
  append(main, finishWithOne());
  const IrModule module =
      moduleOf({functionOf("/main", 0, std::move(main)), oneCallTask("/hold", 0)},
               {"hold", "ping", "release"});
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    CAPTURE(repeat);
    std::atomic<bool> released{false};
    VmHostFunctions hosts;
    bindHost(hosts, "hold", VmHostThreading::Reentrant, [&](std::string &) {
      while (!released.load()) {
        std::this_thread::yield();
      }
      return true;
    });
    bindHost(hosts, "ping", VmHostThreading::Serialized, [&](std::string &) { return true; });
    bindHost(hosts, "release", VmHostThreading::Reentrant, [&](std::string &) {
      released.store(true);
      return true;
    });
    const Run threaded = run(module, hosts, true);
    REQUIRE_MESSAGE(threaded.ok, threaded.error);
    CHECK(threaded.out == "7\n1\n");
  }
}

TEST_CASE("a main-thread-only host function faults when a spawned task calls it") {
  std::vector<IrInstruction> main = spawnOf(1, 0);
  append(main, waitTask(0));
  append(main, finishWithOne());
  const IrModule module =
      moduleOf({functionOf("/main", 0, std::move(main)), oneCallTask("/call", 0)}, {"draw"});
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    CAPTURE(repeat);
    std::atomic<int> calls{0};
    VmHostFunctions hosts;
    bindHost(hosts, "draw", VmHostThreading::MainThreadOnly, [&](std::string &) {
      calls.fetch_add(1);
      return true;
    });
    const Run threaded = run(module, hosts, true);
    CHECK_FALSE(threaded.ok);
    CHECK(threaded.error == "draw called off the main thread");
    CHECK(calls.load() == 0);
  }
}

TEST_CASE("a main-thread-only host function runs for the main task and without threads") {
  // main calls `draw` itself, then a task calls the serialized `note`; the same program with and without threads
  // prints the same.
  std::vector<IrInstruction> main = callOf(0);
  append(main, spawnOf(1, 0));
  append(main, waitTask(0));
  append(main, finishWithOne());
  const IrModule module = moduleOf(
      {functionOf("/main", 0, std::move(main)), oneCallTask("/call", 1)}, {"draw", "note"});
  for (const bool threads : {false, true}) {
    CAPTURE(threads);
    std::atomic<int> draws{0};
    std::atomic<int> notes{0};
    VmHostFunctions hosts;
    bindHost(hosts, "draw", VmHostThreading::MainThreadOnly, [&](std::string &) {
      draws.fetch_add(1);
      return true;
    });
    bindHost(hosts, "note", VmHostThreading::Serialized, [&](std::string &) {
      notes.fetch_add(1);
      return true;
    });
    const Run result = run(module, hosts, threads);
    REQUIRE_MESSAGE(result.ok, result.error);
    CHECK(result.out == "1\n");
    CHECK(draws.load() == 1);
    CHECK(notes.load() == 1);
  }
}

TEST_SUITE_END();
