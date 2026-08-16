#pragma once

TEST_CASE("native emitter debug dump format is deterministic and ordered") {
  primec::NativeEmitterInstrumentation instrumentation;
  instrumentation.totalInstructionCount = 11;
  instrumentation.totalValueStackPushCount = 7;
  instrumentation.totalValueStackPopCount = 6;
  instrumentation.totalSpillCount = 5;
  instrumentation.totalReloadCount = 4;
  primec::NativeEmitterFunctionInstrumentation zetaStats;
  zetaStats.functionIndex = 2;
  zetaStats.functionName = "/zeta";
  zetaStats.instructionTotal = 1;
  zetaStats.valueStackPushCount = 1;
  zetaStats.valueStackPopCount = 1;
  zetaStats.spillCount = 1;
  zetaStats.reloadCount = 1;
  instrumentation.perFunction.push_back(zetaStats);

  primec::NativeEmitterFunctionInstrumentation alphaStats;
  alphaStats.functionIndex = 0;
  alphaStats.functionName = "/alpha";
  alphaStats.instructionTotal = 4;
  alphaStats.valueStackPushCount = 3;
  alphaStats.valueStackPopCount = 3;
  alphaStats.spillCount = 2;
  alphaStats.reloadCount = 2;
  instrumentation.perFunction.push_back(alphaStats);

  primec::NativeEmitterFunctionInstrumentation middleStats;
  middleStats.functionIndex = 1;
  middleStats.functionName = "/middle";
  middleStats.instructionTotal = 6;
  middleStats.valueStackPushCount = 3;
  middleStats.valueStackPopCount = 2;
  middleStats.spillCount = 2;
  middleStats.reloadCount = 1;
  instrumentation.perFunction.push_back(middleStats);

  primec::NativeEmitterOptimizationInstrumentation optimization;
  optimization.applied = true;
  optimization.instructionTotalBefore = 11;
  optimization.instructionTotalAfter = 9;
  optimization.valueStackPushCountBefore = 7;
  optimization.valueStackPushCountAfter = 5;
  optimization.valueStackPopCountBefore = 6;
  optimization.valueStackPopCountAfter = 5;
  optimization.spillCountBefore = 5;
  optimization.spillCountAfter = 2;
  optimization.reloadCountBefore = 4;
  optimization.reloadCountAfter = 2;

  const std::string expected =
      "native_emitter_debug_v1\n"
      "[instrumentation]\n"
      "total_instruction_count=11\n"
      "total_value_stack_push_count=7\n"
      "total_value_stack_pop_count=6\n"
      "total_spill_count=5\n"
      "total_reload_count=4\n"
      "function_count=3\n"
      "function[0].index=0\n"
      "function[0].name=/alpha\n"
      "function[0].instruction_total=4\n"
      "function[0].value_stack_push_count=3\n"
      "function[0].value_stack_pop_count=3\n"
      "function[0].spill_count=2\n"
      "function[0].reload_count=2\n"
      "function[1].index=1\n"
      "function[1].name=/middle\n"
      "function[1].instruction_total=6\n"
      "function[1].value_stack_push_count=3\n"
      "function[1].value_stack_pop_count=2\n"
      "function[1].spill_count=2\n"
      "function[1].reload_count=1\n"
      "function[2].index=2\n"
      "function[2].name=/zeta\n"
      "function[2].instruction_total=1\n"
      "function[2].value_stack_push_count=1\n"
      "function[2].value_stack_pop_count=1\n"
      "function[2].spill_count=1\n"
      "function[2].reload_count=1\n"
      "[optimization]\n"
      "applied=1\n"
      "instruction_total_before=11\n"
      "instruction_total_after=9\n"
      "value_stack_push_count_before=7\n"
      "value_stack_push_count_after=5\n"
      "value_stack_pop_count_before=6\n"
      "value_stack_pop_count_after=5\n"
      "spill_count_before=5\n"
      "spill_count_after=2\n"
      "reload_count_before=4\n"
      "reload_count_after=2\n";

  const std::string dumpA = primec::formatNativeEmitterDebugDump(instrumentation, optimization);
  const std::string dumpB = primec::formatNativeEmitterDebugDump(instrumentation, optimization);
  CHECK(dumpA == expected);
  CHECK(dumpB == expected);
}

TEST_CASE("native emitter debug dump includes optimization defaults") {
  primec::NativeEmitterInstrumentation instrumentation;
  instrumentation.totalInstructionCount = 2;
  instrumentation.totalValueStackPushCount = 1;
  instrumentation.totalValueStackPopCount = 1;
  instrumentation.totalSpillCount = 1;
  instrumentation.totalReloadCount = 1;
  primec::NativeEmitterFunctionInstrumentation mainStats;
  mainStats.functionIndex = 0;
  mainStats.functionName = "/main";
  mainStats.instructionTotal = 2;
  mainStats.valueStackPushCount = 1;
  mainStats.valueStackPopCount = 1;
  mainStats.spillCount = 1;
  mainStats.reloadCount = 1;
  instrumentation.perFunction.push_back(mainStats);

  const std::string dump = primec::formatNativeEmitterDebugDump(instrumentation);
  CHECK(dump.find("[optimization]\n") != std::string::npos);
  CHECK(dump.find("applied=0\n") != std::string::npos);
  CHECK(dump.find("instruction_total_before=0\n") != std::string::npos);
  CHECK(dump.find("instruction_total_after=0\n") != std::string::npos);
}

TEST_CASE("native emitter debug dump sorts duplicate indices by name") {
  primec::NativeEmitterInstrumentation instrumentation;
  instrumentation.totalInstructionCount = 6;
  instrumentation.totalValueStackPushCount = 4;
  instrumentation.totalValueStackPopCount = 4;
  instrumentation.totalSpillCount = 1;
  instrumentation.totalReloadCount = 1;

  primec::NativeEmitterFunctionInstrumentation zetaStats;
  zetaStats.functionIndex = 3;
  zetaStats.functionName = "/zeta";
  zetaStats.instructionTotal = 2;
  zetaStats.valueStackPushCount = 1;
  zetaStats.valueStackPopCount = 1;
  instrumentation.perFunction.push_back(zetaStats);

  primec::NativeEmitterFunctionInstrumentation alphaStats;
  alphaStats.functionIndex = 3;
  alphaStats.functionName = "/alpha";
  alphaStats.instructionTotal = 4;
  alphaStats.valueStackPushCount = 3;
  alphaStats.valueStackPopCount = 3;
  instrumentation.perFunction.push_back(alphaStats);

  const std::string expected =
      "native_emitter_debug_v1\n"
      "[instrumentation]\n"
      "total_instruction_count=6\n"
      "total_value_stack_push_count=4\n"
      "total_value_stack_pop_count=4\n"
      "total_spill_count=1\n"
      "total_reload_count=1\n"
      "function_count=2\n"
      "function[0].index=3\n"
      "function[0].name=/alpha\n"
      "function[0].instruction_total=4\n"
      "function[0].value_stack_push_count=3\n"
      "function[0].value_stack_pop_count=3\n"
      "function[0].spill_count=0\n"
      "function[0].reload_count=0\n"
      "function[1].index=3\n"
      "function[1].name=/zeta\n"
      "function[1].instruction_total=2\n"
      "function[1].value_stack_push_count=1\n"
      "function[1].value_stack_pop_count=1\n"
      "function[1].spill_count=0\n"
      "function[1].reload_count=0\n"
      "[optimization]\n"
      "applied=0\n"
      "instruction_total_before=0\n"
      "instruction_total_after=0\n"
      "value_stack_push_count_before=0\n"
      "value_stack_push_count_after=0\n"
      "value_stack_pop_count_before=0\n"
      "value_stack_pop_count_after=0\n"
      "spill_count_before=0\n"
      "spill_count_after=0\n"
      "reload_count_before=0\n"
      "reload_count_after=0\n";

  CHECK(primec::formatNativeEmitterDebugDump(instrumentation) == expected);
}

TEST_CASE("native emitter debug dump docs applied example snapshot stays stable") {
  primec::NativeEmitterInstrumentation instrumentation;
  instrumentation.totalInstructionCount = 4;
  instrumentation.totalValueStackPushCount = 3;
  instrumentation.totalValueStackPopCount = 2;
  instrumentation.totalSpillCount = 1;
  instrumentation.totalReloadCount = 1;
  primec::NativeEmitterFunctionInstrumentation mainStats;
  mainStats.functionIndex = 0;
  mainStats.functionName = "/main";
  mainStats.instructionTotal = 4;
  mainStats.valueStackPushCount = 3;
  mainStats.valueStackPopCount = 2;
  mainStats.spillCount = 1;
  mainStats.reloadCount = 1;
  instrumentation.perFunction.push_back(mainStats);

  primec::NativeEmitterOptimizationInstrumentation optimization;
  optimization.applied = true;
  optimization.instructionTotalBefore = 4;
  optimization.instructionTotalAfter = 3;
  optimization.valueStackPushCountBefore = 3;
  optimization.valueStackPushCountAfter = 2;
  optimization.valueStackPopCountBefore = 2;
  optimization.valueStackPopCountAfter = 2;
  optimization.spillCountBefore = 1;
  optimization.spillCountAfter = 0;
  optimization.reloadCountBefore = 1;
  optimization.reloadCountAfter = 0;

  // Keep in sync with docs/PrimeStruct.md "Native Allocator & Scheduler (IR Optimization Path)".
  const std::string expected =
      "native_emitter_debug_v1\n"
      "[instrumentation]\n"
      "total_instruction_count=4\n"
      "total_value_stack_push_count=3\n"
      "total_value_stack_pop_count=2\n"
      "total_spill_count=1\n"
      "total_reload_count=1\n"
      "function_count=1\n"
      "function[0].index=0\n"
      "function[0].name=/main\n"
      "function[0].instruction_total=4\n"
      "function[0].value_stack_push_count=3\n"
      "function[0].value_stack_pop_count=2\n"
      "function[0].spill_count=1\n"
      "function[0].reload_count=1\n"
      "[optimization]\n"
      "applied=1\n"
      "instruction_total_before=4\n"
      "instruction_total_after=3\n"
      "value_stack_push_count_before=3\n"
      "value_stack_push_count_after=2\n"
      "value_stack_pop_count_before=2\n"
      "value_stack_pop_count_after=2\n"
      "spill_count_before=1\n"
      "spill_count_after=0\n"
      "reload_count_before=1\n"
      "reload_count_after=0\n";

  CHECK(primec::formatNativeEmitterDebugDump(instrumentation, optimization) == expected);
}

TEST_CASE("native emitter debug dump docs default example snapshot stays stable") {
  primec::NativeEmitterInstrumentation instrumentation;
  instrumentation.totalInstructionCount = 2;
  instrumentation.totalValueStackPushCount = 1;
  instrumentation.totalValueStackPopCount = 1;
  instrumentation.totalSpillCount = 1;
  instrumentation.totalReloadCount = 1;
  primec::NativeEmitterFunctionInstrumentation mainStats;
  mainStats.functionIndex = 0;
  mainStats.functionName = "/main";
  mainStats.instructionTotal = 2;
  mainStats.valueStackPushCount = 1;
  mainStats.valueStackPopCount = 1;
  mainStats.spillCount = 1;
  mainStats.reloadCount = 1;
  instrumentation.perFunction.push_back(mainStats);

  // Keep in sync with docs/PrimeStruct.md "Native Allocator & Scheduler (IR Optimization Path)".
  const std::string expected =
      "native_emitter_debug_v1\n"
      "[instrumentation]\n"
      "total_instruction_count=2\n"
      "total_value_stack_push_count=1\n"
      "total_value_stack_pop_count=1\n"
      "total_spill_count=1\n"
      "total_reload_count=1\n"
      "function_count=1\n"
      "function[0].index=0\n"
      "function[0].name=/main\n"
      "function[0].instruction_total=2\n"
      "function[0].value_stack_push_count=1\n"
      "function[0].value_stack_pop_count=1\n"
      "function[0].spill_count=1\n"
      "function[0].reload_count=1\n"
      "[optimization]\n"
      "applied=0\n"
      "instruction_total_before=0\n"
      "instruction_total_after=0\n"
      "value_stack_push_count_before=0\n"
      "value_stack_push_count_after=0\n"
      "value_stack_pop_count_before=0\n"
      "value_stack_pop_count_after=0\n"
      "spill_count_before=0\n"
      "spill_count_after=0\n"
      "reload_count_before=0\n"
      "reload_count_after=0\n";

  CHECK(primec::formatNativeEmitterDebugDump(instrumentation) == expected);
}

