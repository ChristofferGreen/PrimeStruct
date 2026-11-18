#pragma once

// Debug sessions on programs that spawn tasks (docs/spec/vm-design.md, Tasks): the session runs the deterministic
// scheduler, every task is a thread of the adapter, and a stop is in the task that hit it.

namespace {

primec::IrFunction debugTaskFunction(std::string name,
                                     uint32_t parameterCount,
                                     std::vector<primec::IrInstruction> instructions) {
  primec::IrFunction function;
  function.name = std::move(name);
  function.parameterCount = parameterCount;
  function.instructions = std::move(instructions);
  return function;
}

// main: spawns double(20) and double(30), prints 1 between the spawns and 2 after them, waits both and returns the
// sum. double(x) prints x and returns x * 2. Function 1 starts with `StoreLocal 0` at ip 0.
primec::IrModule debugTwoTasksModule() {
  using primec::IrOpcode;
  primec::IrModule module;
  module.functions.push_back(debugTaskFunction("/main",
                                               0,
                                               {{IrOpcode::PushI32, 20},
                                                {IrOpcode::TaskSpawn, 1},
                                                {IrOpcode::StoreLocal, 0},
                                                {IrOpcode::PushI32, 1},
                                                {IrOpcode::PrintI32, primec::PrintFlagNewline},
                                                {IrOpcode::PushI32, 30},
                                                {IrOpcode::TaskSpawn, 1},
                                                {IrOpcode::StoreLocal, 1},
                                                {IrOpcode::PushI32, 2},
                                                {IrOpcode::PrintI32, primec::PrintFlagNewline},
                                                {IrOpcode::LoadLocal, 0},
                                                {IrOpcode::TaskWait, 0},
                                                {IrOpcode::LoadLocal, 1},
                                                {IrOpcode::TaskWait, 0},
                                                {IrOpcode::AddI32, 0},
                                                {IrOpcode::ReturnI32, 0}}));
  module.functions.push_back(debugTaskFunction("/double",
                                               1,
                                               {{IrOpcode::StoreLocal, 0},
                                                {IrOpcode::LoadLocal, 0},
                                                {IrOpcode::PrintI32, primec::PrintFlagNewline},
                                                {IrOpcode::LoadLocal, 0},
                                                {IrOpcode::PushI32, 2},
                                                {IrOpcode::MulI32, 0},
                                                {IrOpcode::ReturnI32, 0}}));
  module.entryIndex = 0;
  return module;
}

struct DebugTaskRun {
  bool ok = false;
  uint64_t result = 0;
  std::string error;
  std::string out;
};

void appendDebugTaskOutput(int fd, std::string_view chunk, void *userData) {
  if (fd == 1) {
    static_cast<std::string *>(userData)->append(chunk);
  }
}

DebugTaskRun plainTaskRun(const primec::IrModule &module, std::optional<uint64_t> seed) {
  DebugTaskRun run;
  primec::Vm vm;
  vm.setOutputSink({&appendDebugTaskOutput, &run.out});
  vm.setScheduleSeed(seed);
  run.ok = vm.execute(module, run.result, run.error, std::vector<std::string_view>{"prog"});
  return run;
}

DebugTaskRun
debugTaskRun(const primec::IrModule &module, std::optional<uint64_t> seed, std::string_view stem) {
  DebugTaskRun run;
  const CapturedVmIo captured = captureVmIo(stem, [&]() {
    primec::VmDebugSession session;
    session.setScheduleSeed(seed);
    if (!session.start(module, run.error, std::vector<std::string_view>{"prog"})) {
      return;
    }
    primec::VmDebugStopReason reason = primec::VmDebugStopReason::Step;
    run.ok =
        session.continueExecution(reason, run.error) && reason == primec::VmDebugStopReason::Exit;
    run.result = session.snapshot().result;
  });
  REQUIRE(captured.error.empty());
  run.out = captured.stdoutText;
  return run;
}

} // namespace

TEST_CASE("vm debug session runs tasks to the result and output of a plain run") {
  const primec::IrModule module = debugTwoTasksModule();
  const DebugTaskRun plain = plainTaskRun(module, std::nullopt);
  REQUIRE_MESSAGE(plain.ok, plain.error);
  const DebugTaskRun debugged = debugTaskRun(module, std::nullopt, "tasks_eager");
  REQUIRE_MESSAGE(debugged.ok, debugged.error);
  CHECK(debugged.result == 100);
  CHECK(debugged.result == plain.result);
  CHECK(debugged.out == "20\n1\n30\n2\n");
  CHECK(debugged.out == plain.out);
}

TEST_CASE("vm debug session follows a schedule seed like a plain run") {
  const primec::IrModule module = debugTwoTasksModule();
  for (uint64_t seed = 0; seed < 6; ++seed) {
    CAPTURE(seed);
    const DebugTaskRun plain = plainTaskRun(module, seed);
    REQUIRE_MESSAGE(plain.ok, plain.error);
    const DebugTaskRun debugged =
        debugTaskRun(module, seed, "tasks_seeded_" + std::to_string(seed));
    REQUIRE_MESSAGE(debugged.ok, debugged.error);
    CHECK(debugged.result == plain.result);
    CHECK(debugged.out == plain.out);
  }
}

TEST_CASE("vm debug session stops inside a spawned task and lists every task") {
  const primec::IrModule module = debugTwoTasksModule();
  const CapturedVmIo captured = captureVmIo("tasks_breakpoint", [&]() {
    primec::VmDebugSession session;
    std::string error;
    REQUIRE(session.start(module, error));
    REQUIRE(session.addBreakpoint(1, 0, error));

    // Before anything ran there is the main task alone.
    std::vector<primec::VmDebugThread> threads = session.threads();
    REQUIRE(threads.size() == 1);
    CHECK(threads[0].id == 1);
    CHECK(threads[0].name == "main");

    primec::VmDebugStopReason reason = primec::VmDebugStopReason::Step;
    REQUIRE(session.continueExecution(reason, error));
    CHECK(reason == primec::VmDebugStopReason::Breakpoint);
    CHECK(session.currentThreadId() == 2);
    CHECK(session.snapshot().functionIndex == 1);
    CHECK(session.snapshot().instructionPointer == 0);

    threads = session.threads();
    REQUIRE(threads.size() == 2);
    CHECK(threads[0].id == 1);
    CHECK(threads[0].name == "main");
    CHECK(threads[1].id == 2);
    CHECK(threads[1].name == "task 1: /double");

    primec::VmDebugSnapshotPayload payload;
    REQUIRE(session.threadSnapshotPayload(2, payload));
    REQUIRE(payload.callStack.size() == 1);
    CHECK(payload.callStack[0].functionIndex == 1);
    REQUIRE(payload.operandStack.size() == 1);
    CHECK(payload.operandStack[0] == 20);
    REQUIRE(session.threadSnapshotPayload(1, payload));
    REQUIRE(payload.callStack.size() == 1);
    CHECK(payload.callStack[0].functionIndex == 0);
    // The spawner is past its TaskSpawn.
    CHECK(payload.callStack[0].instructionPointer == 2);
    CHECK_FALSE(session.threadSnapshotPayload(3, payload));
    CHECK_FALSE(session.threadSnapshotPayload(0, payload));

    // The first task ended before the second was spawned, so the second is the only one beside main.
    REQUIRE(session.continueExecution(reason, error));
    CHECK(reason == primec::VmDebugStopReason::Breakpoint);
    CHECK(session.currentThreadId() == 3);
    threads = session.threads();
    REQUIRE(threads.size() == 2);
    CHECK(threads[1].id == 3);
    REQUIRE(session.threadSnapshotPayload(3, payload));
    REQUIRE(payload.operandStack.size() == 1);
    CHECK(payload.operandStack[0] == 30);

    REQUIRE(session.continueExecution(reason, error));
    CHECK(reason == primec::VmDebugStopReason::Exit);
    CHECK(session.snapshot().result == 100);
    CHECK(session.threads().size() == 1);
  });
  CHECK(captured.error.empty());
  CHECK(captured.stdoutText == "20\n1\n30\n2\n");
}

TEST_CASE("vm debug session steps from a spawn into the spawned task") {
  const primec::IrModule module = debugTwoTasksModule();
  const CapturedVmIo captured = captureVmIo("tasks_step", [&]() {
    primec::VmDebugSession session;
    std::string error;
    REQUIRE(session.start(module, error));
    primec::VmDebugStopReason reason = primec::VmDebugStopReason::Pause;
    // PushI32 20, then TaskSpawn: the spawn makes the new task the one that runs next.
    REQUIRE(session.step(reason, error));
    CHECK(session.currentThreadId() == 1);
    REQUIRE(session.step(reason, error));
    CHECK(reason == primec::VmDebugStopReason::Step);
    CHECK(session.currentThreadId() == 2);
    CHECK(session.snapshot().functionIndex == 1);
    CHECK(session.snapshot().instructionPointer == 0);
    CHECK(session.snapshot().callDepth == 1);
  });
  CHECK(captured.error.empty());
}

TEST_CASE("vm debug adapter reports a stack per task") {
  const primec::IrModule module = debugTwoTasksModule();
  const CapturedVmIo captured = captureVmIo("tasks_adapter", [&]() {
    primec::VmDebugAdapter adapter;
    std::string error;
    REQUIRE(adapter.launch(module, error));
    REQUIRE(adapter.setInstructionBreakpoints({{1, 0}}, error));
    primec::VmDebugAdapterStopEvent stop;
    REQUIRE(adapter.continueExecution(stop, error));
    CHECK(stop.reason == primec::VmDebugStopReason::Breakpoint);
    CHECK(stop.threadId == 2);

    std::vector<primec::VmDebugAdapterThread> threads;
    REQUIRE(adapter.threads(threads, error));
    REQUIRE(threads.size() == 2);
    CHECK(threads[0].id == 1);
    CHECK(threads[1].id == 2);

    std::vector<primec::VmDebugAdapterStackFrame> mainFrames;
    REQUIRE(adapter.stackTrace(1, mainFrames, error));
    REQUIRE(mainFrames.size() == 1);
    CHECK(mainFrames[0].name == "/main");
    CHECK(mainFrames[0].id == 1);
    std::vector<primec::VmDebugAdapterStackFrame> taskFrames;
    REQUIRE(adapter.stackTrace(2, taskFrames, error));
    REQUIRE(taskFrames.size() == 1);
    CHECK(taskFrames[0].name == "/double");
    // A frame id names its thread, so it differs from the main task's innermost frame.
    CHECK(taskFrames[0].id != mainFrames[0].id);

    std::vector<primec::VmDebugAdapterScope> scopes;
    REQUIRE(adapter.scopes(taskFrames[0].id, scopes, error));
    REQUIRE(scopes.size() == 2);
    std::vector<primec::VmDebugAdapterVariable> variables;
    REQUIRE(adapter.variables(scopes[1].variablesReference, variables, error));
    REQUIRE(variables.size() == 1);
    CHECK(variables[0].value == "20");

    std::vector<primec::VmDebugAdapterStackFrame> none;
    CHECK_FALSE(adapter.stackTrace(9, none, error));
    CHECK(error == "invalid thread id");

    REQUIRE(adapter.setInstructionBreakpoints({}, error));
    REQUIRE(adapter.continueExecution(stop, error));
    CHECK(stop.reason == primec::VmDebugStopReason::Exit);
  });
  CHECK(captured.error.empty());
}

TEST_CASE("vm debug session hooks see a spawned task end") {
  const primec::IrModule module = debugTwoTasksModule();
  struct Counts {
    int pushes = 0;
    int pops = 0;
  } counts;
  const CapturedVmIo captured = captureVmIo("tasks_hooks", [&]() {
    primec::VmDebugSession session;
    primec::VmDebugHooks hooks;
    hooks.userData = &counts;
    hooks.callPop = [](const primec::VmDebugCallHookEvent &, void *userData) {
      ++static_cast<Counts *>(userData)->pops;
    };
    hooks.callPush = [](const primec::VmDebugCallHookEvent &, void *userData) {
      ++static_cast<Counts *>(userData)->pushes;
    };
    session.setHooks(hooks);
    std::string error;
    REQUIRE(session.start(module, error));
    primec::VmDebugStopReason reason = primec::VmDebugStopReason::Step;
    REQUIRE(session.continueExecution(reason, error));
    CHECK(reason == primec::VmDebugStopReason::Exit);
  });
  CHECK(captured.error.empty());
  // Two spawned tasks end, and the program's entry returns: three pops; the program makes no call.
  CHECK(counts.pushes == 0);
  CHECK(counts.pops == 3);
}
