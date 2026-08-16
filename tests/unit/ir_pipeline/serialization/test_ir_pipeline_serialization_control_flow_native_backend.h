#pragma once

#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
namespace {
std::string quoteIrPipelineShellArg(const std::string &value) {
  std::string quoted = "'";
  for (char c : value) {
    if (c == '\'') {
      quoted += "'\\''";
    } else {
      quoted += c;
    }
  }
  quoted += "'";
  return quoted;
}

int runIrPipelineNativeBinary(const std::string &path) {
  int code = std::system(quoteIrPipelineShellArg(path).c_str());
  if (code == -1) {
    return -1;
  }
  if (WIFEXITED(code)) {
    return WEXITSTATUS(code);
  }
  if (WIFSIGNALED(code)) {
    return 128 + WTERMSIG(code);
  }
  return -1;
}
} // namespace

TEST_CASE("native backend executes call and callvoid opcodes") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::CallVoid, 2});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 2});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction valueFn;
  valueFn.name = "/value";
  valueFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  valueFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction ignoredFn;
  ignoredFn.name = "/ignored";
  ignoredFn.instructions.push_back({primec::IrOpcode::PushI32, 999});
  ignoredFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(valueFn));
  module.functions.push_back(std::move(ignoredFn));

  primec::NativeEmitter emitter;
  std::string error;
  const std::string exePath = irSerializationControlFlowPath("primec_native_ir_call_exec").string();
  REQUIRE(emitter.emitExecutable(module, exePath, error));
  CHECK(error.empty());
  CHECK(runIrPipelineNativeBinary(exePath) == 7);
}

TEST_CASE("native backend executes recursive call opcodes") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction factFn;
  factFn.name = "/fact";
  factFn.instructions.push_back({primec::IrOpcode::Dup, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::CmpEqI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 7});
  factFn.instructions.push_back({primec::IrOpcode::Pop, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  factFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::Dup, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  factFn.instructions.push_back({primec::IrOpcode::SubI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::Call, 1});
  factFn.instructions.push_back({primec::IrOpcode::MulI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(factFn));

  primec::NativeEmitter emitter;
  std::string error;
  const std::string exePath = irSerializationControlFlowPath("primec_native_ir_recursive_call_exec").string();
  REQUIRE(emitter.emitExecutable(module, exePath, error));
  CHECK(error.empty());
  CHECK(runIrPipelineNativeBinary(exePath) == 24);
}

TEST_CASE("native backend rejects invalid callable IR target") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::Call, 9});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  primec::NativeEmitter emitter;
  std::string error;
  const std::string exePath = irSerializationControlFlowPath("primec_native_ir_invalid_call_exec").string();
  CHECK_FALSE(emitter.emitExecutable(module, exePath, error));
  CHECK(error.find("invalid call target") != std::string::npos);
}

TEST_CASE("virtual-register lowering preserves native baseline parity") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 12});
  mainFn.instructions.push_back({primec::IrOpcode::CmpGtI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 9});
  mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x40400000u}); // 3.0f
  mainFn.instructions.push_back({primec::IrOpcode::ConvertF32ToI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction helperFn;
  helperFn.name = "/helper";
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  helperFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  helperFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(helperFn));

  primec::Vm vm;
  std::string error;
  uint64_t vmResult = 0;
  REQUIRE(vm.execute(module, vmResult, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterModule virtualModule;
  REQUIRE(primec::lowerIrModuleToBlockVirtualRegisters(module, virtualModule, error));
  CHECK(error.empty());

  primec::IrModule loweredBackModule;
  REQUIRE(primec::liftBlockVirtualRegistersToIrModule(virtualModule, loweredBackModule, error));
  CHECK(error.empty());

  primec::NativeEmitter emitter;
  const std::string baselinePath = irSerializationControlFlowPath("primec_native_ir_virtual_baseline_exec").string();
  const std::string loweredPath = irSerializationControlFlowPath("primec_native_ir_virtual_lowered_exec").string();
  REQUIRE(emitter.emitExecutable(module, baselinePath, error));
  CHECK(error.empty());
  REQUIRE(emitter.emitExecutable(loweredBackModule, loweredPath, error));
  CHECK(error.empty());

  const int baselineExit = runIrPipelineNativeBinary(baselinePath);
  const int loweredExit = runIrPipelineNativeBinary(loweredPath);
  CHECK(baselineExit == static_cast<int>(vmResult));
  CHECK(loweredExit == static_cast<int>(vmResult));
  CHECK(loweredExit == baselineExit);
}

TEST_CASE("native backend reports instrumentation counters per function") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction helperFn;
  helperFn.name = "/helper";
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  helperFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(helperFn));

  primec::NativeEmitter emitter;
  primec::NativeEmitterInstrumentation instrumentation;
  std::string error;
  const std::string exePath = irSerializationControlFlowPath("primec_native_ir_instrumentation_exec").string();
  REQUIRE(emitter.emitExecutable(module, exePath, error, &instrumentation));
  CHECK(error.empty());
  REQUIRE(instrumentation.perFunction.size() == 2);

  CHECK(instrumentation.perFunction[0].functionName == "/main");
  CHECK(instrumentation.perFunction[0].instructionTotal == 4u);
  CHECK(instrumentation.perFunction[0].valueStackPushCount == 3u);
  CHECK(instrumentation.perFunction[0].valueStackPopCount == 3u);
  CHECK(instrumentation.perFunction[0].spillCount == 1u);
  CHECK(instrumentation.perFunction[0].reloadCount == 1u);

  CHECK(instrumentation.perFunction[1].functionName == "/helper");
  CHECK(instrumentation.perFunction[1].instructionTotal == 2u);
  CHECK(instrumentation.perFunction[1].valueStackPushCount == 1u);
  CHECK(instrumentation.perFunction[1].valueStackPopCount == 1u);
  CHECK(instrumentation.perFunction[1].spillCount == 0u);
  CHECK(instrumentation.perFunction[1].reloadCount == 0u);

  CHECK(instrumentation.totalInstructionCount == 6u);
  CHECK(instrumentation.totalValueStackPushCount == 4u);
  CHECK(instrumentation.totalValueStackPopCount == 4u);
  CHECK(instrumentation.totalSpillCount == 1u);
  CHECK(instrumentation.totalReloadCount == 1u);
}

TEST_CASE("native backend instrumentation counts local load and store ops") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 7});
  mainFn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  primec::NativeEmitter emitter;
  primec::NativeEmitterInstrumentation instrumentation;
  std::string error;
  const std::string exePath = irSerializationControlFlowPath("primec_native_ir_instrumentation_locals_exec").string();
  REQUIRE(emitter.emitExecutable(module, exePath, error, &instrumentation));
  CHECK(error.empty());
  REQUIRE(instrumentation.perFunction.size() == 1);

  CHECK(instrumentation.perFunction[0].instructionTotal == 4u);
  CHECK(instrumentation.perFunction[0].valueStackPushCount == 2u);
  CHECK(instrumentation.perFunction[0].valueStackPopCount == 2u);
  CHECK(instrumentation.perFunction[0].spillCount == 0u);
  CHECK(instrumentation.perFunction[0].reloadCount == 0u);
  CHECK(instrumentation.totalInstructionCount == 4u);
  CHECK(instrumentation.totalValueStackPushCount == 2u);
  CHECK(instrumentation.totalValueStackPopCount == 2u);
  CHECK(instrumentation.totalSpillCount == 0u);
  CHECK(instrumentation.totalReloadCount == 0u);
}

TEST_CASE("native backend integer stack cache preserves parity and reduces spills") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 2});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  mainFn.instructions.push_back({primec::IrOpcode::MulI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  primec::NativeEmitter emitter;
  primec::NativeEmitterInstrumentation instrumentation;
  std::string error;
  const std::string exePath = irSerializationControlFlowPath("primec_native_ir_stack_cache_exec").string();
  REQUIRE(emitter.emitExecutable(module, exePath, error, &instrumentation));
  CHECK(error.empty());
  CHECK(runIrPipelineNativeBinary(exePath) == 13);
  REQUIRE(instrumentation.perFunction.size() == 1);
  const auto &mainStats = instrumentation.perFunction[0];
  CHECK(mainStats.valueStackPushCount == 7u);
  CHECK(mainStats.valueStackPopCount == 7u);
  CHECK(mainStats.spillCount == 3u);
  CHECK(mainStats.reloadCount == 3u);
  CHECK(mainStats.spillCount < mainStats.valueStackPushCount);
  CHECK(mainStats.reloadCount < mainStats.valueStackPopCount);
}

TEST_CASE("native backend float stack cache preserves parity and reduces spills") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x3FC00000u}); // 1.5f
  mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x40200000u}); // 2.5f
  mainFn.instructions.push_back({primec::IrOpcode::AddF32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x3F800000u}); // 1.0f
  mainFn.instructions.push_back({primec::IrOpcode::SubF32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ConvertF32ToI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushF64, 0x3FF8000000000000ull}); // 1.5
  mainFn.instructions.push_back({primec::IrOpcode::PushF64, 0x4014000000000000ull}); // 5.0
  mainFn.instructions.push_back({primec::IrOpcode::MulF64, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ConvertF64ToI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  primec::Vm vm;
  uint64_t vmResult = 0;
  std::string error;
  REQUIRE(vm.execute(module, vmResult, error));
  CHECK(error.empty());

  primec::NativeEmitter emitter;
  primec::NativeEmitterInstrumentation instrumentation;
  const std::string exePath = irSerializationControlFlowPath("primec_native_ir_float_stack_cache_exec").string();
  REQUIRE(emitter.emitExecutable(module, exePath, error, &instrumentation));
  CHECK(error.empty());
  const int nativeExit = runIrPipelineNativeBinary(exePath);
  CHECK(nativeExit == static_cast<int>(vmResult));

  REQUIRE(instrumentation.perFunction.size() == 1);
  const auto &mainStats = instrumentation.perFunction[0];
  CHECK(mainStats.valueStackPushCount == 11u);
  CHECK(mainStats.valueStackPopCount == 11u);
  CHECK(mainStats.spillCount == 4u);
  CHECK(mainStats.reloadCount == 4u);
  CHECK(mainStats.spillCount < mainStats.valueStackPushCount);
  CHECK(mainStats.reloadCount < mainStats.valueStackPopCount);
}

TEST_CASE("native backend cache toggle preserves dual-mode parity") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction helperFn;
  helperFn.name = "/helper";
  helperFn.instructions.push_back({primec::IrOpcode::PushF32, 0x3F800000u}); // 1.0f
  helperFn.instructions.push_back({primec::IrOpcode::ConvertF32ToI32, 0});
  helperFn.instructions.push_back({primec::IrOpcode::PushI32, 2});
  helperFn.instructions.push_back({primec::IrOpcode::MulI32, 0});
  helperFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushF64, 0x4008000000000000ull}); // 3.0
  mainFn.instructions.push_back({primec::IrOpcode::ConvertF64ToI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(helperFn));

  primec::Vm vm;
  uint64_t vmResult = 0;
  std::string error;
  REQUIRE(vm.execute(module, vmResult, error));
  CHECK(error.empty());

  primec::NativeEmitter emitter;
  primec::NativeEmitterOptions cacheOnOptions;
  cacheOnOptions.enableRegisterCache = true;
  primec::NativeEmitterOptions cacheOffOptions;
  cacheOffOptions.enableRegisterCache = false;
  primec::NativeEmitterInstrumentation cacheOnInstrumentation;
  primec::NativeEmitterInstrumentation cacheOffInstrumentation;
  const std::string exeOnPath = irSerializationControlFlowPath("primec_native_ir_cache_toggle_on_exec").string();
  const std::string exeOffPath = irSerializationControlFlowPath("primec_native_ir_cache_toggle_off_exec").string();
  REQUIRE(emitter.emitExecutable(module, exeOnPath, error, &cacheOnInstrumentation, cacheOnOptions));
  CHECK(error.empty());
  REQUIRE(emitter.emitExecutable(module, exeOffPath, error, &cacheOffInstrumentation, cacheOffOptions));
  CHECK(error.empty());

  const int nativeOnExit = runIrPipelineNativeBinary(exeOnPath);
  const int nativeOffExit = runIrPipelineNativeBinary(exeOffPath);
  CHECK(nativeOnExit == static_cast<int>(vmResult));
  CHECK(nativeOffExit == static_cast<int>(vmResult));
  CHECK(nativeOnExit == nativeOffExit);
  CHECK(cacheOnInstrumentation.totalSpillCount < cacheOffInstrumentation.totalSpillCount);
  CHECK(cacheOnInstrumentation.totalReloadCount < cacheOffInstrumentation.totalReloadCount);
}

TEST_CASE("native backend cache mode regression matrix covers branches and call depth") {
  struct CacheModeCase {
    std::string name;
    primec::IrModule module;
  };

  std::vector<CacheModeCase> cases;

  {
    primec::IrModule module;
    module.entryIndex = 0;
    primec::IrFunction mainFn;
    mainFn.name = "/main";
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
    mainFn.instructions.push_back({primec::IrOpcode::CmpGtI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 11});
    mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x3FC00000u}); // 1.5f
    mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x40000000u}); // 2.0f
    mainFn.instructions.push_back({primec::IrOpcode::MulF32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::ConvertF32ToI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
    mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    module.functions.push_back(std::move(mainFn));
    cases.push_back({"branch_float_conversion", std::move(module)});
  }

  {
    primec::IrModule module;
    module.entryIndex = 0;
    primec::IrFunction mainFn;
    mainFn.name = "/main";
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
    mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 4});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 42});
    mainFn.instructions.push_back({primec::IrOpcode::Jump, 5});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 7});
    mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    module.functions.push_back(std::move(mainFn));
    cases.push_back({"jump_carries_cached_value_over_branch_arm", std::move(module)});
  }

  {
    primec::IrModule module;
    module.entryIndex = 0;

    primec::IrFunction mainFn;
    mainFn.name = "/main";
    mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
    mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

    primec::IrFunction level1Fn;
    level1Fn.name = "/level1";
    level1Fn.instructions.push_back({primec::IrOpcode::Call, 2});
    level1Fn.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull}); // 2.0
    level1Fn.instructions.push_back({primec::IrOpcode::ConvertF64ToI32, 0});
    level1Fn.instructions.push_back({primec::IrOpcode::AddI32, 0});
    level1Fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

    primec::IrFunction level2Fn;
    level2Fn.name = "/level2";
    level2Fn.instructions.push_back({primec::IrOpcode::PushF32, 0x3FC00000u}); // 1.5f
    level2Fn.instructions.push_back({primec::IrOpcode::PushF32, 0x40000000u}); // 2.0f
    level2Fn.instructions.push_back({primec::IrOpcode::AddF32, 0});
    level2Fn.instructions.push_back({primec::IrOpcode::ConvertF32ToI32, 0});
    level2Fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

    module.functions.push_back(std::move(mainFn));
    module.functions.push_back(std::move(level1Fn));
    module.functions.push_back(std::move(level2Fn));
    cases.push_back({"call_depth_float_int_mix", std::move(module)});
  }

  {
    primec::IrModule module;
    module.entryIndex = 0;

    primec::IrFunction mainFn;
    mainFn.name = "/main";
    mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
    mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

    primec::IrFunction branchFn;
    branchFn.name = "/branchFn";
    branchFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
    branchFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
    branchFn.instructions.push_back({primec::IrOpcode::CmpEqI32, 0});
    branchFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 6});
    branchFn.instructions.push_back({primec::IrOpcode::Call, 2});
    branchFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    branchFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
    branchFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

    primec::IrFunction leafFn;
    leafFn.name = "/leaf";
    leafFn.instructions.push_back({primec::IrOpcode::PushI64, 8});
    leafFn.instructions.push_back({primec::IrOpcode::ConvertI64ToF64, 0});
    leafFn.instructions.push_back({primec::IrOpcode::ConvertF64ToI32, 0});
    leafFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
    leafFn.instructions.push_back({primec::IrOpcode::SubI32, 0});
    leafFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

    module.functions.push_back(std::move(mainFn));
    module.functions.push_back(std::move(branchFn));
    module.functions.push_back(std::move(leafFn));
    cases.push_back({"branch_and_call_depth", std::move(module)});
  }

  primec::NativeEmitter emitter;
  primec::NativeEmitterOptions cacheOnOptions;
  cacheOnOptions.enableRegisterCache = true;
  primec::NativeEmitterOptions cacheOffOptions;
  cacheOffOptions.enableRegisterCache = false;
  bool sawSpillOrReloadImprovement = false;

  for (const auto &testCase : cases) {
    primec::Vm vm;
    uint64_t vmResult = 0;
    std::string error;
    INFO("cache matrix case: " << testCase.name);
    REQUIRE(vm.execute(testCase.module, vmResult, error));
    CHECK(error.empty());

    primec::NativeEmitterInstrumentation cacheOnInstrumentation;
    primec::NativeEmitterInstrumentation cacheOffInstrumentation;
    const std::string exeOnPath =
        irSerializationControlFlowPath("primec_native_ir_cache_matrix_on_" + testCase.name).string();
    const std::string exeOffPath =
        irSerializationControlFlowPath("primec_native_ir_cache_matrix_off_" + testCase.name).string();

    REQUIRE(emitter.emitExecutable(testCase.module, exeOnPath, error, &cacheOnInstrumentation, cacheOnOptions));
    CHECK(error.empty());
    REQUIRE(emitter.emitExecutable(testCase.module, exeOffPath, error, &cacheOffInstrumentation, cacheOffOptions));
    CHECK(error.empty());

    const int nativeOnExit = runIrPipelineNativeBinary(exeOnPath);
    const int nativeOffExit = runIrPipelineNativeBinary(exeOffPath);
    CHECK(nativeOnExit == static_cast<int>(vmResult));
    CHECK(nativeOffExit == static_cast<int>(vmResult));
    CHECK(nativeOnExit == nativeOffExit);
    CHECK(cacheOnInstrumentation.totalValueStackPushCount == cacheOffInstrumentation.totalValueStackPushCount);
    CHECK(cacheOnInstrumentation.totalValueStackPopCount == cacheOffInstrumentation.totalValueStackPopCount);
    CHECK(cacheOnInstrumentation.totalSpillCount <= cacheOffInstrumentation.totalSpillCount);
    CHECK(cacheOnInstrumentation.totalReloadCount <= cacheOffInstrumentation.totalReloadCount);

    if (cacheOnInstrumentation.totalSpillCount < cacheOffInstrumentation.totalSpillCount ||
        cacheOnInstrumentation.totalReloadCount < cacheOffInstrumentation.totalReloadCount) {
      sawSpillOrReloadImprovement = true;
    }
  }

  CHECK(sawSpillOrReloadImprovement);
}

TEST_CASE("native backend optimization conformance perf gates enforce parity and thresholds") {
  struct PerfGateCase {
    std::string name;
    primec::IrModule module;
    uint64_t minSpillReduction = 0;
    uint64_t minReloadReduction = 0;
  };

  std::vector<PerfGateCase> cases;

  {
    primec::IrModule module;
    module.entryIndex = 0;
    primec::IrFunction mainFn;
    mainFn.name = "/main";
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 10});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 2});
    mainFn.instructions.push_back({primec::IrOpcode::DivI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
    mainFn.instructions.push_back({primec::IrOpcode::MulI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 7});
    mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
    module.functions.push_back(std::move(mainFn));
    cases.push_back({"arithmetic_chain", std::move(module), 1u, 1u});
  }

  {
    primec::IrModule module;
    module.entryIndex = 0;

    primec::IrFunction mainFn;
    mainFn.name = "/main";
    mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
    mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
    mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
    mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

    primec::IrFunction helperFn;
    helperFn.name = "/helper";
    helperFn.instructions.push_back({primec::IrOpcode::PushF32, 0x3FC00000u}); // 1.5f
    helperFn.instructions.push_back({primec::IrOpcode::PushF32, 0x40000000u}); // 2.0f
    helperFn.instructions.push_back({primec::IrOpcode::AddF32, 0});
    helperFn.instructions.push_back({primec::IrOpcode::ConvertF32ToI32, 0});
    helperFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

    module.functions.push_back(std::move(mainFn));
    module.functions.push_back(std::move(helperFn));
    cases.push_back({"call_float_mix", std::move(module), 1u, 1u});
  }

  primec::NativeEmitter emitter;
  primec::NativeEmitterOptions cacheOnOptions;
  cacheOnOptions.enableRegisterCache = true;
  primec::NativeEmitterOptions cacheOffOptions;
  cacheOffOptions.enableRegisterCache = false;

  uint64_t totalSpillReduction = 0;
  uint64_t totalReloadReduction = 0;

  for (const auto &testCase : cases) {
    primec::Vm vm;
    uint64_t vmResult = 0;
    std::string error;
    INFO("perf gate case: " << testCase.name);
    REQUIRE(vm.execute(testCase.module, vmResult, error));
    CHECK(error.empty());

    primec::NativeEmitterInstrumentation cacheOffStatsA;
    primec::NativeEmitterInstrumentation cacheOnStatsA;
    primec::NativeEmitterInstrumentation cacheOffStatsB;
    primec::NativeEmitterInstrumentation cacheOnStatsB;

    const std::string exeOffPathA =
        irSerializationControlFlowPath("primec_native_ir_perf_gate_off_a_" + testCase.name).string();
    const std::string exeOnPathA =
        irSerializationControlFlowPath("primec_native_ir_perf_gate_on_a_" + testCase.name).string();
    const std::string exeOffPathB =
        irSerializationControlFlowPath("primec_native_ir_perf_gate_off_b_" + testCase.name).string();
    const std::string exeOnPathB =
        irSerializationControlFlowPath("primec_native_ir_perf_gate_on_b_" + testCase.name).string();

    REQUIRE(emitter.emitExecutable(testCase.module, exeOffPathA, error, &cacheOffStatsA, cacheOffOptions));
    CHECK(error.empty());
    REQUIRE(emitter.emitExecutable(testCase.module, exeOnPathA, error, &cacheOnStatsA, cacheOnOptions));
    CHECK(error.empty());
    REQUIRE(emitter.emitExecutable(testCase.module, exeOffPathB, error, &cacheOffStatsB, cacheOffOptions));
    CHECK(error.empty());
    REQUIRE(emitter.emitExecutable(testCase.module, exeOnPathB, error, &cacheOnStatsB, cacheOnOptions));
    CHECK(error.empty());

    const int offExitA = runIrPipelineNativeBinary(exeOffPathA);
    const int onExitA = runIrPipelineNativeBinary(exeOnPathA);
    const int offExitB = runIrPipelineNativeBinary(exeOffPathB);
    const int onExitB = runIrPipelineNativeBinary(exeOnPathB);
    CHECK(offExitA == static_cast<int>(vmResult));
    CHECK(onExitA == static_cast<int>(vmResult));
    CHECK(offExitB == static_cast<int>(vmResult));
    CHECK(onExitB == static_cast<int>(vmResult));
    CHECK(offExitA == onExitA);
    CHECK(offExitB == onExitB);

    CHECK(cacheOffStatsA.totalValueStackPushCount == cacheOnStatsA.totalValueStackPushCount);
    CHECK(cacheOffStatsA.totalValueStackPopCount == cacheOnStatsA.totalValueStackPopCount);
    CHECK(cacheOffStatsA.totalSpillCount >= cacheOnStatsA.totalSpillCount);
    CHECK(cacheOffStatsA.totalReloadCount >= cacheOnStatsA.totalReloadCount);

    const uint64_t spillReduction = cacheOffStatsA.totalSpillCount - cacheOnStatsA.totalSpillCount;
    const uint64_t reloadReduction = cacheOffStatsA.totalReloadCount - cacheOnStatsA.totalReloadCount;
    CHECK(spillReduction >= testCase.minSpillReduction);
    CHECK(reloadReduction >= testCase.minReloadReduction);
    totalSpillReduction += spillReduction;
    totalReloadReduction += reloadReduction;

    // Parity lock: repeated runs must produce stable optimization counters.
    CHECK(cacheOffStatsA.totalValueStackPushCount == cacheOffStatsB.totalValueStackPushCount);
    CHECK(cacheOffStatsA.totalValueStackPopCount == cacheOffStatsB.totalValueStackPopCount);
    CHECK(cacheOffStatsA.totalSpillCount == cacheOffStatsB.totalSpillCount);
    CHECK(cacheOffStatsA.totalReloadCount == cacheOffStatsB.totalReloadCount);
    CHECK(cacheOnStatsA.totalValueStackPushCount == cacheOnStatsB.totalValueStackPushCount);
    CHECK(cacheOnStatsA.totalValueStackPopCount == cacheOnStatsB.totalValueStackPopCount);
    CHECK(cacheOnStatsA.totalSpillCount == cacheOnStatsB.totalSpillCount);
    CHECK(cacheOnStatsA.totalReloadCount == cacheOnStatsB.totalReloadCount);
  }

  CHECK(totalSpillReduction >= 2u);
  CHECK(totalReloadReduction >= 2u);
}
#endif
