#pragma once

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
    c.name = "heap allocation opcode family";
    primec::IrFunction fn;
    fn.name = "/main";
    fn.instructions.push_back({primec::IrOpcode::PushI32, 2});
    fn.instructions.push_back({primec::IrOpcode::HeapAlloc, 0});
    fn.instructions.push_back({primec::IrOpcode::Dup, 0});
    fn.instructions.push_back({primec::IrOpcode::PushI32, 55});
    fn.instructions.push_back({primec::IrOpcode::StoreIndirect, 0});
    fn.instructions.push_back({primec::IrOpcode::Pop, 0});
    fn.instructions.push_back({primec::IrOpcode::LoadIndirect, 0});
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

  const std::filesystem::path filePath = vmDebugSessionPath("primestruct_vm_debug_opcode_matrix_file.txt");
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
