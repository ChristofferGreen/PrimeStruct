#pragma once

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

TEST_CASE("vm debug adapter launch keeps owned argv text alive") {
  primec::IrModule module = makeArgvPrintModule();
  std::vector<std::string> sourceArgs = {"alpha", "bravo"};
  const std::vector<std::string_view> args = stringViewsOf(sourceArgs);

  primec::VmDebugAdapter adapter;
  std::string error;
  REQUIRE(adapter.launch(module, error, args));
  CHECK(error.empty());

  overwriteStringInPlace(sourceArgs[0], "omega");
  overwriteStringInPlace(sourceArgs[1], "delta");

  primec::VmDebugAdapterStopEvent stopEvent;
  bool continued = false;
  CapturedVmIo captured = captureVmIo("argv_adapter_ownership", [&] {
    continued = adapter.continueExecution(stopEvent, error);
  });

  REQUIRE(captured.error.empty());
  REQUIRE(continued);
  CHECK(error.empty());
  CHECK(stopEvent.reason == primec::VmDebugStopReason::Exit);
  CHECK(stopEvent.dapReason == "exited");
  CHECK(adapter.snapshot().result == 7);
  CHECK(captured.stdoutText == "alpha\n");
  CHECK(captured.stderrText == "bravo\n");
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

TEST_CASE("vm debug adapter exposes caller locals for non-top frames") {
  primec::IrModule module;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.localDebugSlots.push_back({0u, "outer", "i32"});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 9, 101});
  mainFn.instructions.push_back({primec::IrOpcode::StoreLocal, 0, 102});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1, 103});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0, 104});

  primec::IrFunction helperFn;
  helperFn.name = "/helper";
  helperFn.localDebugSlots.push_back({0u, "inner", "i32"});
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 7, 201});
  helperFn.instructions.push_back({primec::IrOpcode::StoreLocal, 0, 202});
  helperFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0, 203});
  helperFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0, 204});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(helperFn));
  module.entryIndex = 0;
  module.instructionSourceMap.push_back({103u, 40u, 5u, primec::IrSourceMapProvenance::CanonicalAst});
  module.instructionSourceMap.push_back({203u, 80u, 7u, primec::IrSourceMapProvenance::CanonicalAst});

  primec::VmDebugAdapter adapter;
  std::string error;
  REQUIRE(adapter.launch(module, error));
  CHECK(error.empty());

  REQUIRE(adapter.setInstructionBreakpoints({{1u, 2u}}, error));
  CHECK(error.empty());

  primec::VmDebugAdapterStopEvent stopEvent;
  REQUIRE(adapter.continueExecution(stopEvent, error));
  CHECK(error.empty());
  CHECK(stopEvent.reason == primec::VmDebugStopReason::Breakpoint);

  std::vector<primec::VmDebugAdapterStackFrame> frames;
  REQUIRE(adapter.stackTrace(1, frames, error));
  CHECK(error.empty());
  REQUIRE(frames.size() == 2);
  CHECK(frames[0].name == "/helper");
  CHECK(frames[1].name == "/main");

  std::vector<primec::VmDebugAdapterScope> callerScopes;
  REQUIRE(adapter.scopes(frames[1].id, callerScopes, error));
  CHECK(error.empty());
  REQUIRE(callerScopes.size() == 2);

  int64_t callerLocalsReference = 0;
  for (const primec::VmDebugAdapterScope &scope : callerScopes) {
    if (scope.name == "Locals") {
      callerLocalsReference = scope.variablesReference;
      break;
    }
  }
  REQUIRE(callerLocalsReference != 0);

  std::vector<primec::VmDebugAdapterVariable> callerLocals;
  REQUIRE(adapter.variables(callerLocalsReference, callerLocals, error));
  CHECK(error.empty());
  REQUIRE(callerLocals.size() == 1);
  CHECK(callerLocals[0].name == "outer");
  CHECK(callerLocals[0].value == "9");
  CHECK(callerLocals[0].typeName == "i32");
}
