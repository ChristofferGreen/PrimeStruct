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

TEST_SUITE_END();
