TEST_SUITE_BEGIN("primestruct.vm.debug.session");

TEST_CASE("vm debug stop reasons are stable and fully covered") {
  const auto reasons = primec::vmDebugStopReasons();
  CHECK(reasons.size() == 5);

  std::unordered_set<std::string_view> names;
  for (const auto reason : reasons) {
    const std::string_view name = primec::vmDebugStopReasonName(reason);
    CHECK(name != "<invalid>");
    CHECK(name != "");
    names.insert(name);
  }

  CHECK(names.size() == reasons.size());
  CHECK(names.count("Breakpoint") == 1);
  CHECK(names.count("Step") == 1);
  CHECK(names.count("Pause") == 1);
  CHECK(names.count("Fault") == 1);
  CHECK(names.count("Exit") == 1);
}

TEST_CASE("vm debug command transitions enforce session model legality") {
  using primec::VmDebugSessionCommand;
  using primec::VmDebugSessionState;

  auto startFromIdle = primec::vmDebugApplyCommand(VmDebugSessionState::Idle, VmDebugSessionCommand::Start);
  CHECK(startFromIdle.valid);
  CHECK(startFromIdle.state == VmDebugSessionState::Running);

  auto startFromPaused = primec::vmDebugApplyCommand(VmDebugSessionState::Paused, VmDebugSessionCommand::Start);
  CHECK_FALSE(startFromPaused.valid);
  CHECK(startFromPaused.state == VmDebugSessionState::Paused);

  auto continueFromPaused = primec::vmDebugApplyCommand(VmDebugSessionState::Paused, VmDebugSessionCommand::Continue);
  CHECK(continueFromPaused.valid);
  CHECK(continueFromPaused.state == VmDebugSessionState::Running);

  auto continueFromIdle = primec::vmDebugApplyCommand(VmDebugSessionState::Idle, VmDebugSessionCommand::Continue);
  CHECK_FALSE(continueFromIdle.valid);
  CHECK(continueFromIdle.state == VmDebugSessionState::Idle);

  auto resetFromPaused = primec::vmDebugApplyCommand(VmDebugSessionState::Paused, VmDebugSessionCommand::Reset);
  CHECK(resetFromPaused.valid);
  CHECK(resetFromPaused.state == VmDebugSessionState::Idle);

  auto resetFromFaulted = primec::vmDebugApplyCommand(VmDebugSessionState::Faulted, VmDebugSessionCommand::Reset);
  CHECK(resetFromFaulted.valid);
  CHECK(resetFromFaulted.state == VmDebugSessionState::Idle);

  auto resetFromExited = primec::vmDebugApplyCommand(VmDebugSessionState::Exited, VmDebugSessionCommand::Reset);
  CHECK(resetFromExited.valid);
  CHECK(resetFromExited.state == VmDebugSessionState::Idle);

  auto resetFromRunning = primec::vmDebugApplyCommand(VmDebugSessionState::Running, VmDebugSessionCommand::Reset);
  CHECK_FALSE(resetFromRunning.valid);
  CHECK(resetFromRunning.state == VmDebugSessionState::Running);
}

TEST_CASE("vm debug stop reasons transition running state deterministically") {
  using primec::VmDebugSessionState;
  using primec::VmDebugStopReason;

  auto byBreakpoint = primec::vmDebugApplyStopReason(VmDebugSessionState::Running, VmDebugStopReason::Breakpoint);
  CHECK(byBreakpoint.valid);
  CHECK(byBreakpoint.state == VmDebugSessionState::Paused);

  auto byStep = primec::vmDebugApplyStopReason(VmDebugSessionState::Running, VmDebugStopReason::Step);
  CHECK(byStep.valid);
  CHECK(byStep.state == VmDebugSessionState::Paused);

  auto byPause = primec::vmDebugApplyStopReason(VmDebugSessionState::Running, VmDebugStopReason::Pause);
  CHECK(byPause.valid);
  CHECK(byPause.state == VmDebugSessionState::Paused);

  auto byFault = primec::vmDebugApplyStopReason(VmDebugSessionState::Running, VmDebugStopReason::Fault);
  CHECK(byFault.valid);
  CHECK(byFault.state == VmDebugSessionState::Faulted);

  auto byExit = primec::vmDebugApplyStopReason(VmDebugSessionState::Running, VmDebugStopReason::Exit);
  CHECK(byExit.valid);
  CHECK(byExit.state == VmDebugSessionState::Exited);
}

TEST_CASE("vm debug stop reasons are rejected outside running state") {
  using primec::VmDebugSessionState;

  for (const auto reason : primec::vmDebugStopReasons()) {
    auto fromIdle = primec::vmDebugApplyStopReason(VmDebugSessionState::Idle, reason);
    CHECK_FALSE(fromIdle.valid);
    CHECK(fromIdle.state == VmDebugSessionState::Idle);

    auto fromPaused = primec::vmDebugApplyStopReason(VmDebugSessionState::Paused, reason);
    CHECK_FALSE(fromPaused.valid);
    CHECK(fromPaused.state == VmDebugSessionState::Paused);

    auto fromFaulted = primec::vmDebugApplyStopReason(VmDebugSessionState::Faulted, reason);
    CHECK_FALSE(fromFaulted.valid);
    CHECK(fromFaulted.state == VmDebugSessionState::Faulted);

    auto fromExited = primec::vmDebugApplyStopReason(VmDebugSessionState::Exited, reason);
    CHECK_FALSE(fromExited.valid);
    CHECK(fromExited.state == VmDebugSessionState::Exited);
  }
}

TEST_CASE("vm debug session step transcript is deterministic") {
  primec::IrModule module;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction calleeFn;
  calleeFn.name = "/double";
  calleeFn.instructions.push_back({primec::IrOpcode::PushI32, 2});
  calleeFn.instructions.push_back({primec::IrOpcode::MulI32, 0});
  calleeFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(calleeFn));
  module.entryIndex = 0;

  primec::VmDebugSession session;
  std::string error;
  REQUIRE(session.start(module, error));
  CHECK(error.empty());

  const primec::VmDebugSnapshot start = session.snapshot();
  CHECK(start.state == primec::VmDebugSessionState::Paused);
  CHECK(start.functionIndex == 0);
  CHECK(start.instructionPointer == 0);
  CHECK(start.callDepth == 1);
  CHECK(start.operandStackSize == 0);

  std::vector<std::string> transcript;
  for (size_t i = 0; i < 8; ++i) {
    primec::VmDebugStopReason reason = primec::VmDebugStopReason::Step;
    REQUIRE(session.step(reason, error));
    const primec::VmDebugSnapshot snap = session.snapshot();
    transcript.push_back(std::string(primec::vmDebugStopReasonName(reason)) + " f" + std::to_string(snap.functionIndex) +
                         " ip" + std::to_string(snap.instructionPointer) + " d" + std::to_string(snap.callDepth) +
                         " s" + std::to_string(snap.operandStackSize) + " r" + std::to_string(snap.result));
    if (reason == primec::VmDebugStopReason::Exit) {
      break;
    }
  }

  const std::vector<std::string> expected = {
      "Step f0 ip1 d1 s1 r0",
      "Step f1 ip0 d2 s1 r0",
      "Step f1 ip1 d2 s2 r0",
      "Step f1 ip2 d2 s1 r0",
      "Step f0 ip2 d1 s1 r0",
      "Step f0 ip3 d1 s2 r0",
      "Step f0 ip4 d1 s1 r0",
      "Exit f0 ip0 d0 s0 r9",
  };
  CHECK(transcript == expected);
}

TEST_CASE("vm debug session continue and pause controls are deterministic") {
  primec::IrModule module;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 2});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.entryIndex = 0;

  primec::VmDebugSession session;
  std::string error;
  REQUIRE(session.start(module, error));
  CHECK(error.empty());

  REQUIRE(session.pause(error));
  CHECK(error.empty());

  primec::VmDebugStopReason reason = primec::VmDebugStopReason::Step;
  REQUIRE(session.continueExecution(reason, error));
  CHECK(error.empty());
  CHECK(reason == primec::VmDebugStopReason::Pause);
  const primec::VmDebugSnapshot paused = session.snapshot();
  CHECK(paused.state == primec::VmDebugSessionState::Paused);
  CHECK(paused.instructionPointer == 0);

  REQUIRE(session.step(reason, error));
  CHECK(error.empty());
  CHECK(reason == primec::VmDebugStopReason::Step);
  const primec::VmDebugSnapshot afterStep = session.snapshot();
  CHECK(afterStep.instructionPointer == 1);

  REQUIRE(session.continueExecution(reason, error));
  CHECK(error.empty());
  CHECK(reason == primec::VmDebugStopReason::Exit);
  const primec::VmDebugSnapshot exited = session.snapshot();
  CHECK(exited.state == primec::VmDebugSessionState::Exited);
  CHECK(exited.result == 3);
}

TEST_CASE("vm debug snapshot payload tracks locals and operand stack across steps") {
  primec::IrModule module;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  mainFn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.entryIndex = 0;

  primec::VmDebugSession session;
  std::string error;
  REQUIRE(session.start(module, error));
  CHECK(error.empty());

  auto payload = session.snapshotPayload();
  REQUIRE(payload.callStack.size() == 1);
  CHECK(payload.callStack[0].functionIndex == 0);
  CHECK(payload.callStack[0].instructionPointer == 0);
  REQUIRE(payload.currentFrameLocals.size() == 1);
  CHECK(payload.currentFrameLocals[0] == 0);
  CHECK(payload.operandStack.empty());
  CHECK(payload.instructionPointer == 0);

  primec::VmDebugStopReason reason = primec::VmDebugStopReason::Step;
  REQUIRE(session.step(reason, error));
  CHECK(reason == primec::VmDebugStopReason::Step);
  payload = session.snapshotPayload();
  REQUIRE(payload.currentFrameLocals.size() == 1);
  CHECK(payload.currentFrameLocals[0] == 0);
  REQUIRE(payload.operandStack.size() == 1);
  CHECK(static_cast<int32_t>(payload.operandStack.back()) == 3);
  CHECK(payload.instructionPointer == 1);

  REQUIRE(session.step(reason, error));
  CHECK(reason == primec::VmDebugStopReason::Step);
  payload = session.snapshotPayload();
  REQUIRE(payload.currentFrameLocals.size() == 1);
  CHECK(static_cast<int32_t>(payload.currentFrameLocals[0]) == 3);
  CHECK(payload.operandStack.empty());
  CHECK(payload.instructionPointer == 2);

  REQUIRE(session.step(reason, error));
  CHECK(reason == primec::VmDebugStopReason::Step);
  payload = session.snapshotPayload();
  REQUIRE(payload.operandStack.size() == 1);
  CHECK(static_cast<int32_t>(payload.operandStack.back()) == 3);
  CHECK(payload.instructionPointer == 3);
}

TEST_CASE("vm debug session validates control-state preconditions") {
  primec::VmDebugSession session;
  std::string error;
  primec::VmDebugStopReason reason = primec::VmDebugStopReason::Step;

  CHECK_FALSE(session.step(reason, error));
  CHECK(error.find("not started") != std::string::npos);

  error.clear();
  CHECK_FALSE(session.continueExecution(reason, error));
  CHECK(error.find("not started") != std::string::npos);

  primec::IrModule module;
  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 9});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));
  module.entryIndex = 0;

  error.clear();
  REQUIRE(session.start(module, error));
  REQUIRE(session.continueExecution(reason, error));
  CHECK(reason == primec::VmDebugStopReason::Exit);

  error.clear();
  CHECK_FALSE(session.step(reason, error));
  CHECK(error.find("not paused") != std::string::npos);
}

TEST_CASE("vm debug runtime hooks cover all event kinds") {
  struct HookRecorder {
    size_t beforeCount = 0;
    size_t afterCount = 0;
    size_t callPushCount = 0;
    size_t callPopCount = 0;
    size_t faultCount = 0;
    std::vector<size_t> pushedFunctions;
    std::vector<size_t> poppedFunctions;
    std::vector<primec::IrOpcode> beforeOpcodes;
    std::vector<primec::IrOpcode> afterOpcodes;
    std::vector<primec::IrOpcode> faultOpcodes;
    std::vector<std::string> faultMessages;
  };

  auto makeHooks = [](HookRecorder &recorder) {
    primec::VmDebugHooks hooks;
    hooks.userData = &recorder;
    hooks.beforeInstruction = [](const primec::VmDebugInstructionHookEvent &event, void *userData) {
      auto &record = *static_cast<HookRecorder *>(userData);
      record.beforeCount += 1;
      record.beforeOpcodes.push_back(event.opcode);
    };
    hooks.afterInstruction = [](const primec::VmDebugInstructionHookEvent &event, void *userData) {
      auto &record = *static_cast<HookRecorder *>(userData);
      record.afterCount += 1;
      record.afterOpcodes.push_back(event.opcode);
    };
    hooks.callPush = [](const primec::VmDebugCallHookEvent &event, void *userData) {
      auto &record = *static_cast<HookRecorder *>(userData);
      record.callPushCount += 1;
      record.pushedFunctions.push_back(event.functionIndex);
    };
    hooks.callPop = [](const primec::VmDebugCallHookEvent &event, void *userData) {
      auto &record = *static_cast<HookRecorder *>(userData);
      record.callPopCount += 1;
      record.poppedFunctions.push_back(event.functionIndex);
    };
    hooks.fault = [](const primec::VmDebugFaultHookEvent &event, void *userData) {
      auto &record = *static_cast<HookRecorder *>(userData);
      record.faultCount += 1;
      record.faultOpcodes.push_back(event.opcode);
      record.faultMessages.emplace_back(event.message);
    };
    return hooks;
  };

  HookRecorder successRecorder;
  primec::VmDebugSession successSession;
  successSession.setHooks(makeHooks(successRecorder));

  primec::IrModule successModule;
  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  primec::IrFunction calleeFn;
  calleeFn.name = "/double";
  calleeFn.instructions.push_back({primec::IrOpcode::PushI32, 2});
  calleeFn.instructions.push_back({primec::IrOpcode::MulI32, 0});
  calleeFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  successModule.functions.push_back(std::move(mainFn));
  successModule.functions.push_back(std::move(calleeFn));
  successModule.entryIndex = 0;

  std::string error;
  REQUIRE(successSession.start(successModule, error));
  CHECK(error.empty());

  primec::VmDebugStopReason stopReason = primec::VmDebugStopReason::Step;
  REQUIRE(successSession.continueExecution(stopReason, error));
  CHECK(error.empty());
  CHECK(stopReason == primec::VmDebugStopReason::Exit);
  CHECK(successSession.snapshot().result == 9);

  CHECK(successRecorder.beforeCount == 8);
  CHECK(successRecorder.afterCount == 8);
  CHECK(successRecorder.callPushCount == 1);
  CHECK(successRecorder.callPopCount == 2);
  CHECK(successRecorder.faultCount == 0);
  CHECK(successRecorder.pushedFunctions == std::vector<size_t>{1});
  CHECK(successRecorder.poppedFunctions == std::vector<size_t>{1, 0});

  HookRecorder faultRecorder;
  primec::VmDebugSession faultSession;
  faultSession.setHooks(makeHooks(faultRecorder));

  primec::IrModule faultModule;
  primec::IrFunction faultFn;
  faultFn.name = "/main";
  faultFn.instructions.push_back({static_cast<primec::IrOpcode>(255), 0});
  faultModule.functions.push_back(std::move(faultFn));
  faultModule.entryIndex = 0;

  error.clear();
  REQUIRE(faultSession.start(faultModule, error));
  CHECK(error.empty());

  stopReason = primec::VmDebugStopReason::Step;
  error.clear();
  CHECK_FALSE(faultSession.step(stopReason, error));
  CHECK(stopReason == primec::VmDebugStopReason::Fault);
  CHECK(error == "unknown IR opcode");

  CHECK(faultRecorder.beforeCount == 1);
  CHECK(faultRecorder.afterCount == 0);
  CHECK(faultRecorder.callPushCount == 0);
  CHECK(faultRecorder.callPopCount == 0);
  CHECK(faultRecorder.faultCount == 1);
  REQUIRE(faultRecorder.faultOpcodes.size() == 1);
  CHECK(faultRecorder.faultOpcodes.front() == static_cast<primec::IrOpcode>(255));
  REQUIRE(faultRecorder.faultMessages.size() == 1);
  CHECK(faultRecorder.faultMessages.front() == "unknown IR opcode");
}

TEST_CASE("vm debug fault diagnostics include mapped source stack traces") {
  primec::IrModule module;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1, 101});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0, 102});

  primec::IrFunction helperFn;
  helperFn.name = "/helper";
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 1, 201});
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 0, 202});
  helperFn.instructions.push_back({primec::IrOpcode::DivI32, 0, 203});
  helperFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0, 204});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(helperFn));
  module.entryIndex = 0;
  module.instructionSourceMap.push_back({101u, 40u, 5u, primec::IrSourceMapProvenance::CanonicalAst});
  module.instructionSourceMap.push_back({102u, 41u, 3u, primec::IrSourceMapProvenance::SyntheticIr});
  module.instructionSourceMap.push_back({201u, 90u, 2u, primec::IrSourceMapProvenance::CanonicalAst});
  module.instructionSourceMap.push_back({202u, 91u, 2u, primec::IrSourceMapProvenance::CanonicalAst});
  module.instructionSourceMap.push_back({203u, 92u, 7u, primec::IrSourceMapProvenance::CanonicalAst});

  primec::VmDebugSession session;
  std::string error;
  REQUIRE(session.start(module, error));
  CHECK(error.empty());

  primec::VmDebugStopReason stopReason = primec::VmDebugStopReason::Step;
  CHECK_FALSE(session.continueExecution(stopReason, error));
  CHECK(stopReason == primec::VmDebugStopReason::Fault);
  CHECK(error.find("division by zero in IR") != std::string::npos);
  CHECK(error.find("stack trace:") != std::string::npos);
  CHECK(error.find("#0 /helper ip 2 debug_id 203 at 92:7 [canonical_ast]") != std::string::npos);
  CHECK(error.find("#1 /main ip 0 debug_id 101 at 40:5 [canonical_ast]") != std::string::npos);
}

TEST_CASE("vm debug adapter emits deterministic protocol transcripts") {
  auto collectTranscript = [&](std::vector<std::string> &transcript, std::string &error) {
    error.clear();
    primec::IrModule module;

    primec::IrFunction mainFn;
    mainFn.name = "/main";
    mainFn.localDebugSlots.push_back({0u, "value", "i32"});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 5, 101});
    mainFn.instructions.push_back({primec::IrOpcode::StoreLocal, 0, 102});
    mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0, 103});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1, 104});
    mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0, 105});
    mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0, 106});

    module.functions.push_back(std::move(mainFn));
    module.entryIndex = 0;
    module.instructionSourceMap.push_back({101u, 10u, 1u, primec::IrSourceMapProvenance::CanonicalAst});
    module.instructionSourceMap.push_back({102u, 11u, 3u, primec::IrSourceMapProvenance::CanonicalAst});
    module.instructionSourceMap.push_back({103u, 12u, 4u, primec::IrSourceMapProvenance::CanonicalAst});
    module.instructionSourceMap.push_back({104u, 14u, 2u, primec::IrSourceMapProvenance::CanonicalAst});
    module.instructionSourceMap.push_back({105u, 15u, 2u, primec::IrSourceMapProvenance::CanonicalAst});
    module.instructionSourceMap.push_back({106u, 16u, 1u, primec::IrSourceMapProvenance::CanonicalAst});

    primec::VmDebugAdapter adapter;
    if (!adapter.launch(module, error)) {
      return false;
    }

    std::vector<primec::VmDebugAdapterThread> threads;
    if (!adapter.threads(threads, error)) {
      return false;
    }
    if (threads.size() != 1 || threads[0].id != 1) {
      error = "unexpected thread inventory";
      return false;
    }

    std::vector<primec::VmDebugAdapterBreakpointResult> breakpoints;
    if (!adapter.setSourceBreakpoints({{14u, 2u}}, breakpoints, error)) {
      return false;
    }
    if (breakpoints.size() != 1 || !breakpoints.front().verified || breakpoints.front().resolvedCount != 1) {
      error = "source breakpoints did not verify";
      return false;
    }

    primec::VmDebugAdapterStopEvent stopEvent;
    if (!adapter.continueExecution(stopEvent, error)) {
      return false;
    }
    if (stopEvent.reason != primec::VmDebugStopReason::Breakpoint || stopEvent.dapReason != "breakpoint") {
      error = "unexpected stop reason";
      return false;
    }

    std::vector<primec::VmDebugAdapterStackFrame> frames;
    if (!adapter.stackTrace(1, frames, error)) {
      return false;
    }
    if (frames.size() != 1 || frames[0].line != 14u || frames[0].column != 2u || frames[0].debugId != 104u) {
      error = "unexpected stack frame mapping";
      return false;
    }

    std::vector<primec::VmDebugAdapterScope> scopes;
    if (!adapter.scopes(frames[0].id, scopes, error)) {
      return false;
    }
    if (scopes.size() != 2) {
      error = "unexpected scopes";
      return false;
    }

    int64_t localsReference = 0;
    int64_t operandReference = 0;
    for (const primec::VmDebugAdapterScope &scope : scopes) {
      if (scope.name == "Locals") {
        localsReference = scope.variablesReference;
      } else if (scope.name == "Operand Stack") {
        operandReference = scope.variablesReference;
      }
    }
    if (localsReference == 0 || operandReference == 0) {
      error = "missing variable references";
      return false;
    }

    std::vector<primec::VmDebugAdapterVariable> locals;
    if (!adapter.variables(localsReference, locals, error)) {
      return false;
    }
    if (locals.size() != 1 || locals[0].name != "value" || locals[0].value != "5" || locals[0].typeName != "i32") {
      error = "unexpected local variable payload";
      return false;
    }

    std::vector<primec::VmDebugAdapterVariable> operand;
    if (!adapter.variables(operandReference, operand, error)) {
      return false;
    }
    if (operand.size() != 1 || operand[0].name != "stack[0]" || operand[0].value != "5") {
      error = "unexpected operand stack payload";
      return false;
    }

    transcript = adapter.transcript();
    return true;
  };

  std::string error;
  std::vector<std::string> first;
  REQUIRE(collectTranscript(first, error));
  CHECK(error.empty());

  const std::vector<std::string> expected = {
      "{\"type\":\"request\",\"command\":\"launch\",\"arg_count\":0}",
      "{\"type\":\"response\",\"command\":\"launch\",\"success\":true}",
      "{\"type\":\"event\",\"event\":\"initialized\"}",
      "{\"type\":\"event\",\"event\":\"stopped\",\"reason\":\"entry\"}",
      "{\"type\":\"request\",\"command\":\"threads\"}",
      "{\"type\":\"response\",\"command\":\"threads\",\"success\":true,\"count\":1}",
      "{\"type\":\"request\",\"command\":\"setBreakpoints\",\"count\":1}",
      "{\"type\":\"response\",\"command\":\"setBreakpoints\",\"success\":true,\"verified_count\":1}",
      "{\"type\":\"request\",\"command\":\"continue\"}",
      "{\"type\":\"event\",\"event\":\"stopped\",\"reason\":\"breakpoint\",\"vm_reason\":\"Breakpoint\"}",
      "{\"type\":\"response\",\"command\":\"continue\",\"success\":true,\"reason\":\"breakpoint\"}",
      "{\"type\":\"request\",\"command\":\"stackTrace\",\"thread_id\":1}",
      "{\"type\":\"response\",\"command\":\"stackTrace\",\"success\":true,\"frame_count\":1}",
      "{\"type\":\"request\",\"command\":\"scopes\",\"frame_id\":1}",
      "{\"type\":\"response\",\"command\":\"scopes\",\"success\":true,\"scope_count\":2}",
      "{\"type\":\"request\",\"command\":\"variables\",\"reference\":257}",
      "{\"type\":\"response\",\"command\":\"variables\",\"success\":true,\"variable_count\":1}",
      "{\"type\":\"request\",\"command\":\"variables\",\"reference\":258}",
      "{\"type\":\"response\",\"command\":\"variables\",\"success\":true,\"variable_count\":1}",
  };
  CHECK(first == expected);

  std::vector<std::string> second;
  error.clear();
  REQUIRE(collectTranscript(second, error));
  CHECK(error.empty());
  CHECK(second == first);
}

TEST_CASE("vm debug adapter reports invalid debug protocol queries") {
  primec::IrModule module;
  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1, 11});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0, 12});
  module.functions.push_back(std::move(mainFn));
  module.entryIndex = 0;
  module.instructionSourceMap.push_back({11u, 20u, 2u, primec::IrSourceMapProvenance::CanonicalAst});

  primec::VmDebugAdapter adapter;
  std::string error;
  REQUIRE(adapter.launch(module, error));
  CHECK(error.empty());

  std::vector<primec::VmDebugAdapterStackFrame> frames;
  CHECK_FALSE(adapter.stackTrace(2, frames, error));
  CHECK(error.find("invalid thread id") != std::string::npos);

  std::vector<primec::VmDebugAdapterScope> scopes;
  error.clear();
  CHECK_FALSE(adapter.scopes(99, scopes, error));
  CHECK(error.find("invalid frame id") != std::string::npos);

  std::vector<primec::VmDebugAdapterVariable> vars;
  error.clear();
  CHECK_FALSE(adapter.variables(999, vars, error));
  CHECK(error.find("invalid variable reference") != std::string::npos);

  std::vector<primec::VmDebugAdapterBreakpointResult> breakpoints;
  error.clear();
  REQUIRE(adapter.setSourceBreakpoints({{999u, std::nullopt}}, breakpoints, error));
  CHECK(error.empty());
  REQUIRE(breakpoints.size() == 1);
  CHECK_FALSE(breakpoints[0].verified);
  CHECK(breakpoints[0].message.find("source breakpoint not found") != std::string::npos);
}

TEST_CASE("vm debug hook event ordering is deterministic and replayable") {
  auto collectEventSequence = [&](std::vector<std::string> &out, std::string &error) {
    primec::VmDebugSession session;
    primec::VmDebugHooks hooks;
    hooks.userData = &out;
    hooks.beforeInstruction = [](const primec::VmDebugInstructionHookEvent &event, void *userData) {
      auto &events = *static_cast<std::vector<std::string> *>(userData);
      events.push_back(std::to_string(event.sequence) + " before " + std::to_string(static_cast<uint32_t>(event.opcode)) +
                       " f" + std::to_string(event.snapshot.functionIndex) + " ip" +
                       std::to_string(event.snapshot.instructionPointer) + " d" + std::to_string(event.snapshot.callDepth));
    };
    hooks.afterInstruction = [](const primec::VmDebugInstructionHookEvent &event, void *userData) {
      auto &events = *static_cast<std::vector<std::string> *>(userData);
      events.push_back(std::to_string(event.sequence) + " after " + std::to_string(static_cast<uint32_t>(event.opcode)) +
                       " f" + std::to_string(event.snapshot.functionIndex) + " ip" +
                       std::to_string(event.snapshot.instructionPointer) + " d" + std::to_string(event.snapshot.callDepth));
    };
    hooks.callPush = [](const primec::VmDebugCallHookEvent &event, void *userData) {
      auto &events = *static_cast<std::vector<std::string> *>(userData);
      events.push_back(std::to_string(event.sequence) + " callPush t" + std::to_string(event.functionIndex) + " rv" +
                       std::to_string(event.returnsValueToCaller ? 1 : 0) + " f" +
                       std::to_string(event.snapshot.functionIndex) + " ip" +
                       std::to_string(event.snapshot.instructionPointer) + " d" + std::to_string(event.snapshot.callDepth));
    };
    hooks.callPop = [](const primec::VmDebugCallHookEvent &event, void *userData) {
      auto &events = *static_cast<std::vector<std::string> *>(userData);
      events.push_back(std::to_string(event.sequence) + " callPop t" + std::to_string(event.functionIndex) + " rv" +
                       std::to_string(event.returnsValueToCaller ? 1 : 0) + " f" +
                       std::to_string(event.snapshot.functionIndex) + " ip" +
                       std::to_string(event.snapshot.instructionPointer) + " d" + std::to_string(event.snapshot.callDepth));
    };
    hooks.fault = [](const primec::VmDebugFaultHookEvent &event, void *userData) {
      auto &events = *static_cast<std::vector<std::string> *>(userData);
      events.push_back(std::to_string(event.sequence) + " fault " + std::to_string(static_cast<uint32_t>(event.opcode)) +
                       " f" + std::to_string(event.snapshot.functionIndex) + " ip" +
                       std::to_string(event.snapshot.instructionPointer) + " d" + std::to_string(event.snapshot.callDepth) +
                       " m=" + std::string(event.message));
    };
    session.setHooks(hooks);

    primec::IrModule module;
    primec::IrFunction mainFn;
    mainFn.name = "/main";
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
    mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
    mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    primec::IrFunction calleeFn;
    calleeFn.name = "/double";
    calleeFn.instructions.push_back({primec::IrOpcode::PushI32, 2});
    calleeFn.instructions.push_back({primec::IrOpcode::MulI32, 0});
    calleeFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    module.functions.push_back(std::move(mainFn));
    module.functions.push_back(std::move(calleeFn));
    module.entryIndex = 0;

    if (!session.start(module, error)) {
      return false;
    }
    primec::VmDebugStopReason stopReason = primec::VmDebugStopReason::Step;
    if (!session.continueExecution(stopReason, error)) {
      return false;
    }
    return stopReason == primec::VmDebugStopReason::Exit && session.snapshot().result == 9;
  };

  std::vector<std::string> firstRun;
  std::string error;
  REQUIRE(collectEventSequence(firstRun, error));
  CHECK(error.empty());

  const auto opcodeId = [](primec::IrOpcode opcode) { return std::to_string(static_cast<uint32_t>(opcode)); };
  const auto beforeLine = [&](size_t sequence, primec::IrOpcode opcode, std::string_view suffix) {
    return std::to_string(sequence) + " before " + opcodeId(opcode) + std::string(suffix);
  };
  const auto afterLine = [&](size_t sequence, primec::IrOpcode opcode, std::string_view suffix) {
    return std::to_string(sequence) + " after " + opcodeId(opcode) + std::string(suffix);
  };
  const std::vector<std::string> expected = {
      beforeLine(0, primec::IrOpcode::PushI32, " f0 ip0 d1"),
      afterLine(1, primec::IrOpcode::PushI32, " f0 ip1 d1"),
      beforeLine(2, primec::IrOpcode::Call, " f0 ip1 d1"),
      "3 callPush t1 rv1 f1 ip0 d2",
      afterLine(4, primec::IrOpcode::Call, " f1 ip0 d2"),
      beforeLine(5, primec::IrOpcode::PushI32, " f1 ip0 d2"),
      afterLine(6, primec::IrOpcode::PushI32, " f1 ip1 d2"),
      beforeLine(7, primec::IrOpcode::MulI32, " f1 ip1 d2"),
      afterLine(8, primec::IrOpcode::MulI32, " f1 ip2 d2"),
      beforeLine(9, primec::IrOpcode::ReturnI32, " f1 ip2 d2"),
      "10 callPop t1 rv1 f0 ip2 d1",
      afterLine(11, primec::IrOpcode::ReturnI32, " f0 ip2 d1"),
      beforeLine(12, primec::IrOpcode::PushI32, " f0 ip2 d1"),
      afterLine(13, primec::IrOpcode::PushI32, " f0 ip3 d1"),
      beforeLine(14, primec::IrOpcode::AddI32, " f0 ip3 d1"),
      afterLine(15, primec::IrOpcode::AddI32, " f0 ip4 d1"),
      beforeLine(16, primec::IrOpcode::ReturnI32, " f0 ip4 d1"),
      "17 callPop t0 rv0 f0 ip0 d0",
      afterLine(18, primec::IrOpcode::ReturnI32, " f0 ip0 d0"),
  };
  CHECK(firstRun == expected);

  std::vector<std::string> secondRun;
  error.clear();
  REQUIRE(collectEventSequence(secondRun, error));
  CHECK(error.empty());
  CHECK(secondRun == firstRun);
}

TEST_CASE("vm debug session breakpoints validate and support multi-hit flows") {
  primec::VmDebugSession session;
  std::string error;

  CHECK_FALSE(session.addBreakpoint(0, 0, error));
  CHECK(error.find("not started") != std::string::npos);

  primec::IrModule module;
  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction calleeFn;
  calleeFn.name = "/double";
  calleeFn.instructions.push_back({primec::IrOpcode::PushI32, 2});
  calleeFn.instructions.push_back({primec::IrOpcode::MulI32, 0});
  calleeFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(calleeFn));
  module.entryIndex = 0;

  error.clear();
  REQUIRE(session.start(module, error));
  CHECK(error.empty());

  error.clear();
  CHECK_FALSE(session.addBreakpoint(3, 0, error));
  CHECK(error.find("function index") != std::string::npos);

  error.clear();
  CHECK_FALSE(session.addBreakpoint(0, 9, error));
  CHECK(error.find("instruction pointer") != std::string::npos);

  error.clear();
  REQUIRE(session.addBreakpoint(0, 1, error));
  REQUIRE(session.addBreakpoint(1, 1, error));
  REQUIRE(session.addBreakpoint(0, 3, error));
  CHECK(error.empty());

  error.clear();
  CHECK_FALSE(session.removeBreakpoint(0, 2, error));
  CHECK(error.find("not found") != std::string::npos);

  primec::VmDebugStopReason stopReason = primec::VmDebugStopReason::Step;
  error.clear();
  REQUIRE(session.continueExecution(stopReason, error));
  CHECK(stopReason == primec::VmDebugStopReason::Breakpoint);
  CHECK(error.empty());
  auto snap = session.snapshot();
  CHECK(snap.functionIndex == 0);
  CHECK(snap.instructionPointer == 1);

  error.clear();
  REQUIRE(session.continueExecution(stopReason, error));
  CHECK(stopReason == primec::VmDebugStopReason::Breakpoint);
  CHECK(error.empty());
  snap = session.snapshot();
  CHECK(snap.functionIndex == 1);
  CHECK(snap.instructionPointer == 1);

  error.clear();
  REQUIRE(session.continueExecution(stopReason, error));
  CHECK(stopReason == primec::VmDebugStopReason::Breakpoint);
  CHECK(error.empty());
  snap = session.snapshot();
  CHECK(snap.functionIndex == 0);
  CHECK(snap.instructionPointer == 3);

  error.clear();
  REQUIRE(session.removeBreakpoint(0, 3, error));
  CHECK(error.empty());
  session.clearBreakpoints();

  error.clear();
  REQUIRE(session.continueExecution(stopReason, error));
  CHECK(stopReason == primec::VmDebugStopReason::Exit);
  CHECK(error.empty());
  CHECK(session.snapshot().result == 7);
}

TEST_CASE("vm resolves source breakpoints for single and multi-hit mappings") {
  primec::IrModule module;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3, 11});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1, 12});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0, 13});

  primec::IrFunction helperFn;
  helperFn.name = "/helper";
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 2, 21});
  helperFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0, 22});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(helperFn));
  module.entryIndex = 0;
  module.instructionSourceMap.push_back({11u, 100u, 4u, primec::IrSourceMapProvenance::CanonicalAst});
  module.instructionSourceMap.push_back({21u, 100u, 4u, primec::IrSourceMapProvenance::CanonicalAst});
  module.instructionSourceMap.push_back({13u, 101u, 2u, primec::IrSourceMapProvenance::CanonicalAst});
  module.instructionSourceMap.push_back({22u, 100u, 4u, primec::IrSourceMapProvenance::SyntheticIr});

  std::vector<primec::VmResolvedSourceBreakpoint> matches;
  std::string error;
  REQUIRE(primec::resolveSourceBreakpoints(module, 100u, 4u, matches, error));
  CHECK(error.empty());
  REQUIRE(matches.size() == 2);
  CHECK(matches[0].functionIndex == 0);
  CHECK(matches[0].instructionPointer == 0);
  CHECK(matches[1].functionIndex == 1);
  CHECK(matches[1].instructionPointer == 0);

  matches.clear();
  error.clear();
  REQUIRE(primec::resolveSourceBreakpoints(module, 101u, std::nullopt, matches, error));
  CHECK(error.empty());
  REQUIRE(matches.size() == 1);
  CHECK(matches[0].functionIndex == 0);
  CHECK(matches[0].instructionPointer == 2);
}

TEST_CASE("vm source breakpoint resolution reports ambiguous span diagnostics") {
  primec::IrModule module;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1, 1});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 2, 2});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0, 3});
  module.functions.push_back(std::move(mainFn));
  module.entryIndex = 0;
  module.instructionSourceMap.push_back({1u, 200u, 3u, primec::IrSourceMapProvenance::CanonicalAst});
  module.instructionSourceMap.push_back({2u, 200u, 9u, primec::IrSourceMapProvenance::CanonicalAst});

  std::vector<primec::VmResolvedSourceBreakpoint> matches;
  std::string error;
  CHECK_FALSE(primec::resolveSourceBreakpoints(module, 200u, std::nullopt, matches, error));
  CHECK(error.find("ambiguous source breakpoint") != std::string::npos);
  CHECK(error.find("line 200") != std::string::npos);
  CHECK(error.find("3") != std::string::npos);
  CHECK(error.find("9") != std::string::npos);
}

TEST_CASE("vm debug session adds source breakpoints and hits all mapped locations") {
  primec::IrModule module;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3, 31});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1, 32});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0, 33});

  primec::IrFunction helperFn;
  helperFn.name = "/double";
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 2, 41});
  helperFn.instructions.push_back({primec::IrOpcode::MulI32, 0, 42});
  helperFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0, 43});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(helperFn));
  module.entryIndex = 0;
  module.instructionSourceMap.push_back({31u, 300u, 6u, primec::IrSourceMapProvenance::CanonicalAst});
  module.instructionSourceMap.push_back({41u, 300u, 6u, primec::IrSourceMapProvenance::CanonicalAst});

  primec::VmDebugSession session;
  std::string error;
  REQUIRE(session.start(module, error));
  CHECK(error.empty());

  size_t resolvedCount = 0;
  REQUIRE(session.addSourceBreakpoint(300u, 6u, resolvedCount, error));
  CHECK(error.empty());
  CHECK(resolvedCount == 2);

  primec::VmDebugStopReason stopReason = primec::VmDebugStopReason::Step;
  REQUIRE(session.continueExecution(stopReason, error));
  CHECK(error.empty());
  CHECK(stopReason == primec::VmDebugStopReason::Breakpoint);
  auto snap = session.snapshot();
  CHECK(snap.functionIndex == 0);
  CHECK(snap.instructionPointer == 0);

  REQUIRE(session.continueExecution(stopReason, error));
  CHECK(error.empty());
  CHECK(stopReason == primec::VmDebugStopReason::Breakpoint);
  snap = session.snapshot();
  CHECK(snap.functionIndex == 1);
  CHECK(snap.instructionPointer == 0);
}

TEST_CASE("vm debug breakpoints follow executed branch path deterministically") {
  auto runBranchCase = [](uint64_t conditionValue, size_t expectedBreakpointIp, uint64_t expectedResult, std::string &error) {
    error.clear();
    primec::VmDebugSession session;
    primec::IrModule module;
    primec::IrFunction mainFn;
    mainFn.name = "/main";
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, conditionValue});
    mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 4});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 11});
    mainFn.instructions.push_back({primec::IrOpcode::Jump, 5});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 22});
    mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    module.functions.push_back(std::move(mainFn));
    module.entryIndex = 0;

    if (!session.start(module, error)) {
      return false;
    }
    if (!session.addBreakpoint(0, 2, error) || !session.addBreakpoint(0, 4, error)) {
      return false;
    }

    primec::VmDebugStopReason stopReason = primec::VmDebugStopReason::Step;
    error.clear();
    if (!session.continueExecution(stopReason, error)) {
      return false;
    }
    if (stopReason != primec::VmDebugStopReason::Breakpoint) {
      return false;
    }
    const auto snap = session.snapshot();
    if (snap.functionIndex != 0 || snap.instructionPointer != expectedBreakpointIp) {
      return false;
    }

    error.clear();
    if (!session.continueExecution(stopReason, error)) {
      return false;
    }
    if (stopReason != primec::VmDebugStopReason::Exit) {
      return false;
    }
    if (session.snapshot().result != expectedResult) {
      return false;
    }
    return true;
  };

  std::string error;
  CHECK(runBranchCase(0, 4, 22, error));
  CHECK(error.empty());

  error.clear();
  CHECK(runBranchCase(1, 2, 11, error));
  CHECK(error.empty());
}

TEST_CASE("vm debug opcode matrix matches vm execute for expanded families") {
  struct OpcodeMatrixCase {
    std::string name;
    primec::IrModule module;
    std::vector<std::string> args;
  };

  auto f32Bits = [](float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return static_cast<uint64_t>(bits);
  };
  auto f64Bits = [](double value) {
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
  };

  auto make_views = [](const std::vector<std::string> &args) {
    std::vector<std::string_view> views;
    views.reserve(args.size());
    for (const std::string &arg : args) {
      views.push_back(arg);
    }
    return views;
  };

  auto runVm = [&](const primec::IrModule &module, const std::vector<std::string> &args, uint64_t &result, std::string &error) {
    primec::Vm vm;
    if (args.empty()) {
      return vm.execute(module, result, error, 0);
    }
    const std::vector<std::string_view> views = make_views(args);
    return vm.execute(module, result, error, views);
  };

  auto runDebugByStep =
      [&](const primec::IrModule &module, const std::vector<std::string> &args, uint64_t &result, std::string &error) {
        primec::VmDebugSession session;
        if (args.empty()) {
          if (!session.start(module, error, 0)) {
            return false;
          }
        } else {
          const std::vector<std::string_view> views = make_views(args);
          if (!session.start(module, error, views)) {
            return false;
          }
        }

        constexpr size_t MaxSteps = 4096;
        for (size_t stepIndex = 0; stepIndex < MaxSteps; ++stepIndex) {
          primec::VmDebugStopReason reason = primec::VmDebugStopReason::Step;
          const bool ok = session.step(reason, error);
          if (!ok) {
            return false;
          }
          if (reason == primec::VmDebugStopReason::Exit) {
            result = session.snapshot().result;
            return true;
          }
          if (reason != primec::VmDebugStopReason::Step) {
            error = "unexpected stop reason while stepping";
            return false;
          }
        }
        error = "debug step limit exceeded";
        return false;
      };

  std::vector<OpcodeMatrixCase> cases;

  {
    OpcodeMatrixCase c;
    c.name = "indirect and integer families";
    primec::IrFunction fn;
    fn.name = "/main";
    fn.instructions.push_back({primec::IrOpcode::PushI32, 7});
    fn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
    fn.instructions.push_back({primec::IrOpcode::AddressOfLocal, 0});
    fn.instructions.push_back({primec::IrOpcode::LoadIndirect, 0});
    fn.instructions.push_back({primec::IrOpcode::NegI32, 0});
    fn.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int64_t>(-7))});
    fn.instructions.push_back({primec::IrOpcode::CmpEqI32, 0});
    fn.instructions.push_back({primec::IrOpcode::PushI64, 11});
    fn.instructions.push_back({primec::IrOpcode::NegI64, 0});
    fn.instructions.push_back({primec::IrOpcode::Pop, 0});
    fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    c.module.functions.push_back(std::move(fn));
    c.module.entryIndex = 0;
    cases.push_back(std::move(c));
  }

  {
    OpcodeMatrixCase c;
    c.name = "float compare and convert families";
    primec::IrFunction fn;
    fn.name = "/main";
    fn.instructions.push_back({primec::IrOpcode::PushF32, f32Bits(1.5f)});
    fn.instructions.push_back({primec::IrOpcode::PushF32, f32Bits(2.0f)});
    fn.instructions.push_back({primec::IrOpcode::AddF32, 0});
    fn.instructions.push_back({primec::IrOpcode::PushF32, f32Bits(0.5f)});
    fn.instructions.push_back({primec::IrOpcode::SubF32, 0});
    fn.instructions.push_back({primec::IrOpcode::PushF32, f32Bits(4.0f)});
    fn.instructions.push_back({primec::IrOpcode::MulF32, 0});
    fn.instructions.push_back({primec::IrOpcode::PushF32, f32Bits(4.0f)});
    fn.instructions.push_back({primec::IrOpcode::DivF32, 0});
    fn.instructions.push_back({primec::IrOpcode::NegF32, 0});
    fn.instructions.push_back({primec::IrOpcode::ConvertF32ToI32, 0});
    fn.instructions.push_back({primec::IrOpcode::ConvertI32ToF64, 0});
    fn.instructions.push_back({primec::IrOpcode::PushF64, f64Bits(-3.0)});
    fn.instructions.push_back({primec::IrOpcode::CmpEqF64, 0});
    fn.instructions.push_back({primec::IrOpcode::PushF64, f64Bits(5.0)});
    fn.instructions.push_back({primec::IrOpcode::NegF64, 0});
    fn.instructions.push_back({primec::IrOpcode::ConvertF64ToI64, 0});
    fn.instructions.push_back({primec::IrOpcode::ConvertI64ToF32, 0});
    fn.instructions.push_back({primec::IrOpcode::ConvertF32ToU64, 0});
    fn.instructions.push_back({primec::IrOpcode::Pop, 0});
    fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    c.module.functions.push_back(std::move(fn));
    c.module.entryIndex = 0;
    cases.push_back(std::move(c));
  }

  {
    OpcodeMatrixCase c;
    c.name = "return f32 and f64 families";

    primec::IrFunction mainFn;
    mainFn.name = "/main";
    mainFn.instructions.push_back({primec::IrOpcode::CallVoid, 1});
    mainFn.instructions.push_back({primec::IrOpcode::CallVoid, 2});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
    mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

    primec::IrFunction returnF32Fn;
    returnF32Fn.name = "/retf32";
    returnF32Fn.instructions.push_back({primec::IrOpcode::PushF32, f32Bits(1.0f)});
    returnF32Fn.instructions.push_back({primec::IrOpcode::ReturnF32, 0});

    primec::IrFunction returnF64Fn;
    returnF64Fn.name = "/retf64";
    returnF64Fn.instructions.push_back({primec::IrOpcode::PushF64, f64Bits(2.0)});
    returnF64Fn.instructions.push_back({primec::IrOpcode::ReturnF64, 0});

    c.module.functions.push_back(std::move(mainFn));
    c.module.functions.push_back(std::move(returnF32Fn));
    c.module.functions.push_back(std::move(returnF64Fn));
    c.module.entryIndex = 0;
    cases.push_back(std::move(c));
  }

  {
    OpcodeMatrixCase c;
    c.name = "print argv and string-byte families";
    c.args = {""};
    c.module.stringTable = {"", "abc"};

    primec::IrFunction fn;
    fn.name = "/main";
    fn.instructions.push_back({primec::IrOpcode::PushI32, 0});
    fn.instructions.push_back({primec::IrOpcode::PrintArgv, 0});
    fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
    fn.instructions.push_back({primec::IrOpcode::PrintArgvUnsafe, 0});
    fn.instructions.push_back({primec::IrOpcode::PrintString, primec::encodePrintStringImm(0, 0)});
    fn.instructions.push_back({primec::IrOpcode::PushI32, 0});
    fn.instructions.push_back({primec::IrOpcode::PrintStringDynamic, 0});
    fn.instructions.push_back({primec::IrOpcode::PushI32, 0});
    fn.instructions.push_back({primec::IrOpcode::PrintI32, 0});
    fn.instructions.push_back({primec::IrOpcode::PushI64, 0});
    fn.instructions.push_back({primec::IrOpcode::PrintI64, 0});
    fn.instructions.push_back({primec::IrOpcode::PushI64, 0});
    fn.instructions.push_back({primec::IrOpcode::PrintU64, 0});
    fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
    fn.instructions.push_back({primec::IrOpcode::LoadStringByte, 1});
    fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    c.module.functions.push_back(std::move(fn));
    c.module.entryIndex = 0;
    cases.push_back(std::move(c));
  }

  const std::filesystem::path filePath =
      std::filesystem::temp_directory_path() / "primestruct_vm_debug_opcode_matrix_file.txt";
  std::filesystem::remove(filePath);
  {
    OpcodeMatrixCase c;
    c.name = "file opcode family";
    c.module.stringTable = {filePath.string(), "text"};

    primec::IrFunction fn;
    fn.name = "/main";
    fn.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
    fn.instructions.push_back({primec::IrOpcode::Dup, 0});
    fn.instructions.push_back({primec::IrOpcode::FileWriteString, 1});
    fn.instructions.push_back({primec::IrOpcode::Pop, 0});
    fn.instructions.push_back({primec::IrOpcode::Dup, 0});
    fn.instructions.push_back({primec::IrOpcode::PushI32, 65});
    fn.instructions.push_back({primec::IrOpcode::FileWriteByte, 0});
    fn.instructions.push_back({primec::IrOpcode::Pop, 0});
    fn.instructions.push_back({primec::IrOpcode::Dup, 0});
    fn.instructions.push_back({primec::IrOpcode::FileWriteNewline, 0});
    fn.instructions.push_back({primec::IrOpcode::Pop, 0});
    fn.instructions.push_back({primec::IrOpcode::Dup, 0});
    fn.instructions.push_back({primec::IrOpcode::PushI32, 7});
    fn.instructions.push_back({primec::IrOpcode::FileWriteI32, 0});
    fn.instructions.push_back({primec::IrOpcode::Pop, 0});
    fn.instructions.push_back({primec::IrOpcode::Dup, 0});
    fn.instructions.push_back({primec::IrOpcode::PushI64, 8});
    fn.instructions.push_back({primec::IrOpcode::FileWriteI64, 0});
    fn.instructions.push_back({primec::IrOpcode::Pop, 0});
    fn.instructions.push_back({primec::IrOpcode::Dup, 0});
    fn.instructions.push_back({primec::IrOpcode::PushI64, 9});
    fn.instructions.push_back({primec::IrOpcode::FileWriteU64, 0});
    fn.instructions.push_back({primec::IrOpcode::Pop, 0});
    fn.instructions.push_back({primec::IrOpcode::Dup, 0});
    fn.instructions.push_back({primec::IrOpcode::FileFlush, 0});
    fn.instructions.push_back({primec::IrOpcode::Pop, 0});
    fn.instructions.push_back({primec::IrOpcode::FileClose, 0});
    fn.instructions.push_back({primec::IrOpcode::Pop, 0});
    fn.instructions.push_back({primec::IrOpcode::FileOpenAppend, 0});
    fn.instructions.push_back({primec::IrOpcode::Dup, 0});
    fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
    fn.instructions.push_back({primec::IrOpcode::FileWriteI32, 0});
    fn.instructions.push_back({primec::IrOpcode::Pop, 0});
    fn.instructions.push_back({primec::IrOpcode::FileClose, 0});
    fn.instructions.push_back({primec::IrOpcode::Pop, 0});
    fn.instructions.push_back({primec::IrOpcode::FileOpenRead, 0});
    fn.instructions.push_back({primec::IrOpcode::FileClose, 0});
    fn.instructions.push_back({primec::IrOpcode::Pop, 0});
    fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
    fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    c.module.functions.push_back(std::move(fn));
    c.module.entryIndex = 0;
    cases.push_back(std::move(c));
  }

  for (const OpcodeMatrixCase &c : cases) {
    uint64_t vmResult = 0;
    std::string vmError;
    INFO("vm case: " << c.name);
    REQUIRE(runVm(c.module, c.args, vmResult, vmError));
    CHECK(vmError.empty());

    uint64_t debugResult = 0;
    std::string debugError;
    INFO("debug case: " << c.name);
    REQUIRE(runDebugByStep(c.module, c.args, debugResult, debugError));
    CHECK(debugError.empty());
    CHECK(debugResult == vmResult);
  }

  std::filesystem::remove(filePath);
}

TEST_SUITE_END();
