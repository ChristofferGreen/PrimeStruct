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

TEST_SUITE_END();
