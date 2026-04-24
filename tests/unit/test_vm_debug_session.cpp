#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>
#include <unistd.h>

#include "third_party/doctest.h"

#include "primec/Vm.h"
#include "primec/VmDebugAdapter.h"
#include "primec/testing/TestScratch.h"

TEST_SUITE_BEGIN("primestruct.vm.debug.session");

namespace {
std::filesystem::path vmDebugSessionPath(const std::string &relativePath) {
  return primec::testing::testScratchPath("vm_debug_session/" + relativePath);
}

class ScopedFdRedirect {
public:
  ScopedFdRedirect(int targetFd, const std::filesystem::path &capturePath) : targetFd_(targetFd) {
    flushTarget();
    savedFd_ = ::dup(targetFd_);
    if (savedFd_ < 0) {
      error_ = "failed to duplicate file descriptor";
      return;
    }
    captureFd_ = ::open(capturePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (captureFd_ < 0) {
      error_ = "failed to open capture path: " + capturePath.string();
      return;
    }
    if (::dup2(captureFd_, targetFd_) < 0) {
      error_ = "failed to redirect file descriptor";
    }
  }

  ~ScopedFdRedirect() { restore(); }

  bool ok() const { return error_.empty(); }
  const std::string &error() const { return error_; }

  void restore() {
    if (restored_) {
      return;
    }
    flushTarget();
    if (savedFd_ >= 0) {
      ::dup2(savedFd_, targetFd_);
      ::close(savedFd_);
      savedFd_ = -1;
    }
    if (captureFd_ >= 0) {
      ::close(captureFd_);
      captureFd_ = -1;
    }
    restored_ = true;
  }

private:
  void flushTarget() const {
    if (targetFd_ == STDOUT_FILENO) {
      std::fflush(stdout);
    } else if (targetFd_ == STDERR_FILENO) {
      std::fflush(stderr);
    }
  }

  int targetFd_ = -1;
  int savedFd_ = -1;
  int captureFd_ = -1;
  bool restored_ = false;
  std::string error_;
};

struct CapturedVmIo {
  std::string stdoutText;
  std::string stderrText;
  std::string error;
};

std::string readCapturedText(const std::filesystem::path &path, std::string &error) {
  std::ifstream input(path, std::ios::binary);
  if (!input.good()) {
    error = "failed to read capture path: " + path.string();
    return {};
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  if (!input.good() && !input.eof()) {
    error = "failed to read capture path: " + path.string();
    return {};
  }
  return buffer.str();
}

template <typename Fn>
CapturedVmIo captureVmIo(std::string_view stem, Fn &&fn) {
  CapturedVmIo captured;
  const std::filesystem::path stdoutPath = vmDebugSessionPath(std::string(stem) + ".stdout.txt");
  const std::filesystem::path stderrPath = vmDebugSessionPath(std::string(stem) + ".stderr.txt");
  std::filesystem::create_directories(stdoutPath.parent_path());
  std::filesystem::remove(stdoutPath);
  std::filesystem::remove(stderrPath);

  {
    ScopedFdRedirect stdoutRedirect(STDOUT_FILENO, stdoutPath);
    if (!stdoutRedirect.ok()) {
      captured.error = stdoutRedirect.error();
      return captured;
    }
    ScopedFdRedirect stderrRedirect(STDERR_FILENO, stderrPath);
    if (!stderrRedirect.ok()) {
      captured.error = stderrRedirect.error();
      return captured;
    }
    std::forward<Fn>(fn)();
  }

  captured.stdoutText = readCapturedText(stdoutPath, captured.error);
  if (!captured.error.empty()) {
    return captured;
  }
  captured.stderrText = readCapturedText(stderrPath, captured.error);
  return captured;
}

std::vector<std::string_view> stringViewsOf(const std::vector<std::string> &args) {
  std::vector<std::string_view> views;
  views.reserve(args.size());
  for (const std::string &arg : args) {
    views.emplace_back(arg);
  }
  return views;
}

uint64_t f32Bits(float value) {
  uint32_t bits = 0;
  static_assert(sizeof(bits) == sizeof(value));
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

uint64_t f64Bits(double value) {
  uint64_t bits = 0;
  static_assert(sizeof(bits) == sizeof(value));
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

void overwriteStringInPlace(std::string &target, std::string_view replacement) {
  REQUIRE(target.size() == replacement.size());
  for (size_t i = 0; i < replacement.size(); ++i) {
    target[i] = replacement[i];
  }
}

primec::IrModule makeArgvPrintModule() {
  primec::IrModule module;
  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PrintArgv, primec::PrintFlagNewline});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  mainFn.instructions.push_back({primec::IrOpcode::PrintArgvUnsafe,
                                 static_cast<uint64_t>(primec::PrintFlagStderr | primec::PrintFlagNewline)});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 7});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));
  module.entryIndex = 0;
  return module;
}

struct VmNumericRunOutcome {
  bool ok = false;
  uint64_t result = 0;
  std::string error;
  primec::VmDebugStopReason stopReason = primec::VmDebugStopReason::Step;
};

VmNumericRunOutcome runVmModule(const primec::IrModule &module) {
  VmNumericRunOutcome outcome;
  primec::Vm vm;
  outcome.ok = vm.execute(module, outcome.result, outcome.error);
  return outcome;
}

VmNumericRunOutcome runVmDebugSession(const primec::IrModule &module) {
  VmNumericRunOutcome outcome;
  primec::VmDebugSession session;
  if (!session.start(module, outcome.error)) {
    return outcome;
  }
  outcome.error.clear();
  outcome.ok = session.continueExecution(outcome.stopReason, outcome.error);
  outcome.result = session.snapshot().result;
  return outcome;
}
} // namespace

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

TEST_CASE("vm debug snapshot payload exposes concrete locals for non-top frames") {
  primec::IrModule module;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 9});
  mainFn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction helperFn;
  helperFn.name = "/helper";
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 7});
  helperFn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  helperFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  helperFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(helperFn));
  module.entryIndex = 0;

  primec::VmDebugSession session;
  std::string error;
  REQUIRE(session.start(module, error));
  CHECK(error.empty());

  primec::VmDebugStopReason reason = primec::VmDebugStopReason::Step;
  REQUIRE(session.step(reason, error));
  REQUIRE(session.step(reason, error));
  REQUIRE(session.step(reason, error));
  CHECK(reason == primec::VmDebugStopReason::Step);

  auto payload = session.snapshotPayload();
  REQUIRE(payload.callStack.size() == 2);
  REQUIRE(payload.frameLocals.size() == 2);
  REQUIRE(payload.frameLocals[0].size() == 1);
  CHECK(static_cast<int32_t>(payload.frameLocals[0][0]) == 9);
  REQUIRE(payload.frameLocals[1].size() == 1);
  CHECK(static_cast<int32_t>(payload.frameLocals[1][0]) == 0);

  REQUIRE(session.step(reason, error));
  REQUIRE(session.step(reason, error));
  CHECK(reason == primec::VmDebugStopReason::Step);

  payload = session.snapshotPayload();
  REQUIRE(payload.callStack.size() == 2);
  REQUIRE(payload.frameLocals.size() == 2);
  REQUIRE(payload.frameLocals[0].size() == 1);
  CHECK(static_cast<int32_t>(payload.frameLocals[0][0]) == 9);
  REQUIRE(payload.frameLocals[1].size() == 1);
  CHECK(static_cast<int32_t>(payload.frameLocals[1][0]) == 7);
  CHECK(payload.currentFrameLocals == payload.frameLocals[1]);
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

TEST_CASE("vm debug session owns argv text after startup") {
  primec::IrModule module = makeArgvPrintModule();
  std::vector<std::string> sourceArgs = {"alpha", "bravo"};
  const std::vector<std::string_view> args = stringViewsOf(sourceArgs);

  primec::VmDebugSession session;
  std::string error;
  REQUIRE(session.start(module, error, args));
  CHECK(error.empty());

  overwriteStringInPlace(sourceArgs[0], "omega");
  overwriteStringInPlace(sourceArgs[1], "delta");

  primec::VmDebugStopReason stopReason = primec::VmDebugStopReason::Step;
  bool continued = false;
  CapturedVmIo captured = captureVmIo("argv_session_ownership", [&] {
    continued = session.continueExecution(stopReason, error);
  });

  REQUIRE(captured.error.empty());
  REQUIRE(continued);
  CHECK(error.empty());
  CHECK(stopReason == primec::VmDebugStopReason::Exit);
  CHECK(session.snapshot().result == 7);
  CHECK(captured.stdoutText == "alpha\n");
  CHECK(captured.stderrText == "bravo\n");
}

TEST_CASE("vm and debug session share numeric opcode behavior") {
  struct NumericParityCase {
    std::string name;
    primec::IrModule module;
    bool expectedOk = true;
    uint64_t expectedResult = 0;
    std::string expectedError;
    primec::VmDebugStopReason expectedStopReason = primec::VmDebugStopReason::Exit;
  };

  std::vector<NumericParityCase> cases;

  {
    NumericParityCase c;
    c.name = "integer arithmetic";
    c.expectedResult = 15;

    primec::IrFunction fn;
    fn.name = "/main";
    fn.instructions.push_back({primec::IrOpcode::PushI32, 9});
    fn.instructions.push_back({primec::IrOpcode::PushI32, 4});
    fn.instructions.push_back({primec::IrOpcode::SubI32, 0});
    fn.instructions.push_back({primec::IrOpcode::PushI64, 3});
    fn.instructions.push_back({primec::IrOpcode::MulI64, 0});
    fn.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
    c.module.functions.push_back(std::move(fn));
    c.module.entryIndex = 0;
    cases.push_back(std::move(c));
  }

  {
    NumericParityCase c;
    c.name = "comparison";
    c.expectedResult = 1;

    primec::IrFunction fn;
    fn.name = "/main";
    fn.instructions.push_back({primec::IrOpcode::PushF64, f64Bits(4.5)});
    fn.instructions.push_back({primec::IrOpcode::PushF64, f64Bits(2.0)});
    fn.instructions.push_back({primec::IrOpcode::CmpGtF64, 0});
    fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    c.module.functions.push_back(std::move(fn));
    c.module.entryIndex = 0;
    cases.push_back(std::move(c));
  }

  {
    NumericParityCase c;
    c.name = "conversion";
    c.expectedResult = static_cast<uint64_t>(42);

    primec::IrFunction fn;
    fn.name = "/main";
    fn.instructions.push_back({primec::IrOpcode::PushF32, f32Bits(42.75f)});
    fn.instructions.push_back({primec::IrOpcode::ConvertF32ToI64, 0});
    fn.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
    c.module.functions.push_back(std::move(fn));
    c.module.entryIndex = 0;
    cases.push_back(std::move(c));
  }

  {
    NumericParityCase c;
    c.name = "fault";
    c.expectedOk = false;
    c.expectedError = "division by zero in IR";
    c.expectedStopReason = primec::VmDebugStopReason::Fault;

    primec::IrFunction fn;
    fn.name = "/main";
    fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
    fn.instructions.push_back({primec::IrOpcode::PushI32, 0});
    fn.instructions.push_back({primec::IrOpcode::DivI32, 0});
    fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    c.module.functions.push_back(std::move(fn));
    c.module.entryIndex = 0;
    cases.push_back(std::move(c));
  }

  for (const auto &c : cases) {
    INFO(c.name);

    const VmNumericRunOutcome vmOutcome = runVmModule(c.module);
    const VmNumericRunOutcome debugOutcome = runVmDebugSession(c.module);

    CHECK(vmOutcome.ok == c.expectedOk);
    CHECK(debugOutcome.ok == c.expectedOk);
    CHECK(debugOutcome.stopReason == c.expectedStopReason);
    CHECK(vmOutcome.error == c.expectedError);
    CHECK(debugOutcome.error == c.expectedError);
    CHECK(vmOutcome.result == c.expectedResult);
    CHECK(debugOutcome.result == c.expectedResult);
  }
}

TEST_CASE("vm and debug session share control flow opcode behavior") {
  struct ControlFlowParityCase {
    std::string name;
    primec::IrModule module;
    bool expectedOk = true;
    uint64_t expectedResult = 0;
    std::string expectedError;
    primec::VmDebugStopReason expectedStopReason = primec::VmDebugStopReason::Exit;
  };

  std::vector<ControlFlowParityCase> cases;

  {
    ControlFlowParityCase c;
    c.name = "conditional jump taken";
    c.expectedResult = 7;

    primec::IrFunction fn;
    fn.name = "/main";
    fn.instructions.push_back({primec::IrOpcode::PushI32, 0});
    fn.instructions.push_back({primec::IrOpcode::JumpIfZero, 4});
    fn.instructions.push_back({primec::IrOpcode::PushI32, 99});
    fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    fn.instructions.push_back({primec::IrOpcode::PushI32, 7});
    fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    c.module.functions.push_back(std::move(fn));
    c.module.entryIndex = 0;
    cases.push_back(std::move(c));
  }

  {
    ControlFlowParityCase c;
    c.name = "jump skips middle block";
    c.expectedResult = 5;

    primec::IrFunction fn;
    fn.name = "/main";
    fn.instructions.push_back({primec::IrOpcode::Jump, 3});
    fn.instructions.push_back({primec::IrOpcode::PushI32, 99});
    fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    fn.instructions.push_back({primec::IrOpcode::PushI32, 5});
    fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    c.module.functions.push_back(std::move(fn));
    c.module.entryIndex = 0;
    cases.push_back(std::move(c));
  }

  {
    ControlFlowParityCase c;
    c.name = "call and return to caller";
    c.expectedResult = 11;

    primec::IrFunction mainFn;
    mainFn.name = "/main";
    mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
    mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

    primec::IrFunction calleeFn;
    calleeFn.name = "/callee";
    calleeFn.instructions.push_back({primec::IrOpcode::PushI32, 11});
    calleeFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

    c.module.functions.push_back(std::move(mainFn));
    c.module.functions.push_back(std::move(calleeFn));
    c.module.entryIndex = 0;
    cases.push_back(std::move(c));
  }

  {
    ControlFlowParityCase c;
    c.name = "void call keeps caller stack unchanged";
    c.expectedResult = 8;

    primec::IrFunction mainFn;
    mainFn.name = "/main";
    mainFn.instructions.push_back({primec::IrOpcode::CallVoid, 1});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 8});
    mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

    primec::IrFunction calleeFn;
    calleeFn.name = "/callee";
    calleeFn.instructions.push_back({primec::IrOpcode::PushI32, 22});
    calleeFn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});

    c.module.functions.push_back(std::move(mainFn));
    c.module.functions.push_back(std::move(calleeFn));
    c.module.entryIndex = 0;
    cases.push_back(std::move(c));
  }

  {
    ControlFlowParityCase c;
    c.name = "invalid jump target faults";
    c.expectedOk = false;
    c.expectedError = "invalid jump target in IR";
    c.expectedStopReason = primec::VmDebugStopReason::Fault;

    primec::IrFunction fn;
    fn.name = "/main";
    fn.instructions.push_back({primec::IrOpcode::Jump, 9});
    c.module.functions.push_back(std::move(fn));
    c.module.entryIndex = 0;
    cases.push_back(std::move(c));
  }

  for (const auto &c : cases) {
    INFO(c.name);

    const VmNumericRunOutcome vmOutcome = runVmModule(c.module);
    const VmNumericRunOutcome debugOutcome = runVmDebugSession(c.module);

    CHECK(vmOutcome.ok == c.expectedOk);
    CHECK(debugOutcome.ok == c.expectedOk);
    CHECK(debugOutcome.stopReason == c.expectedStopReason);
    CHECK(vmOutcome.error == c.expectedError);
    CHECK(debugOutcome.error == c.expectedError);
    CHECK(vmOutcome.result == c.expectedResult);
    CHECK(debugOutcome.result == c.expectedResult);
  }
}

#include "test_vm_debug_session_protocol.h"

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

#include "test_vm_debug_session_breakpoints.h"

TEST_SUITE_END();
