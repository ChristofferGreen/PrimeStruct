#pragma once

TEST_CASE("scheduler is dependency-safe and latency-aware") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 12});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 100});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  mainFn.instructions.push_back({primec::IrOpcode::DivI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  std::string error;
  primec::IrVirtualRegisterModule virtualModule;
  REQUIRE(primec::lowerIrModuleToBlockVirtualRegisters(module, virtualModule, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterModuleLiveness liveness;
  REQUIRE(primec::buildIrVirtualRegisterLiveness(virtualModule, liveness, error));
  CHECK(error.empty());

  primec::IrLinearScanAllocatorOptions options;
  options.physicalRegisterCount = 2;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;
  primec::IrLinearScanModuleAllocation allocation;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterScheduledModule scheduledOnce;
  REQUIRE(primec::scheduleIrVirtualRegisters(virtualModule, allocation, scheduledOnce, error));
  CHECK(error.empty());
  REQUIRE(scheduledOnce.functions.size() == 1);
  REQUIRE(scheduledOnce.functions[0].blocks.size() == 1);
  const auto &scheduledBlock = scheduledOnce.functions[0].blocks[0];
  REQUIRE(scheduledBlock.instructions.size() == 8);

  std::vector<size_t> scheduledOrder;
  for (const auto &instruction : scheduledBlock.instructions) {
    scheduledOrder.push_back(instruction.originalInstructionIndex);
  }
  CHECK(scheduledOrder == std::vector<size_t>{0, 1, 2, 3, 4, 5, 6, 7});

  std::vector<size_t> positionByInstruction(8, 0);
  for (size_t position = 0; position < scheduledBlock.instructions.size(); ++position) {
    positionByInstruction[scheduledBlock.instructions[position].originalInstructionIndex] = position;
  }
  for (const auto &instruction : scheduledBlock.instructions) {
    const size_t instructionPos = positionByInstruction[instruction.originalInstructionIndex];
    for (size_t dependency : instruction.dependencyInstructionIndices) {
      CHECK(positionByInstruction[dependency] < instructionPos);
    }
  }

  primec::IrVirtualRegisterScheduledModule scheduledAgain;
  REQUIRE(primec::scheduleIrVirtualRegisters(virtualModule, allocation, scheduledAgain, error));
  CHECK(error.empty());
  REQUIRE(scheduledAgain.functions.size() == 1);
  REQUIRE(scheduledAgain.functions[0].blocks.size() == 1);
  REQUIRE(scheduledAgain.functions[0].blocks[0].instructions.size() == 8);
  for (size_t index = 0; index < 8; ++index) {
    CHECK(scheduledAgain.functions[0].blocks[0].instructions[index].originalInstructionIndex ==
          scheduledBlock.instructions[index].originalInstructionIndex);
  }
}

TEST_CASE("scheduler prioritizes spilled-register latency penalty") {
  primec::IrVirtualRegisterModule module;
  primec::IrVirtualRegisterFunction function;
  function.name = "/main";
  primec::IrVirtualRegisterBlock block;
  block.startInstructionIndex = 0;
  block.endInstructionIndex = 7;
  block.reachable = true;

  primec::IrVirtualRegisterInstruction inst0;
  inst0.instruction = {primec::IrOpcode::PushI32, 11};
  inst0.defRegisters = {0};
  block.instructions.push_back(inst0);

  primec::IrVirtualRegisterInstruction inst1;
  inst1.instruction = {primec::IrOpcode::PushI32, 22};
  inst1.defRegisters = {1};
  block.instructions.push_back(inst1);

  primec::IrVirtualRegisterInstruction inst2;
  inst2.instruction = {primec::IrOpcode::PushI32, 33};
  inst2.defRegisters = {2};
  block.instructions.push_back(inst2);

  primec::IrVirtualRegisterInstruction inst3;
  inst3.instruction = {primec::IrOpcode::PushI32, 44};
  inst3.defRegisters = {3};
  block.instructions.push_back(inst3);

  primec::IrVirtualRegisterInstruction inst4;
  inst4.instruction = {primec::IrOpcode::AddI32, 0};
  inst4.useRegisters = {0, 1};
  inst4.defRegisters = {4};
  block.instructions.push_back(inst4);

  primec::IrVirtualRegisterInstruction inst5;
  inst5.instruction = {primec::IrOpcode::AddI32, 0};
  inst5.useRegisters = {2, 3};
  inst5.defRegisters = {5};
  block.instructions.push_back(inst5);

  primec::IrVirtualRegisterInstruction inst6;
  inst6.instruction = {primec::IrOpcode::ReturnI32, 0};
  inst6.useRegisters = {4};
  block.instructions.push_back(inst6);

  function.blocks.push_back(std::move(block));
  module.functions.push_back(std::move(function));

  primec::IrLinearScanModuleAllocation allocation;
  primec::IrLinearScanFunctionAllocation functionAllocation;
  functionAllocation.functionName = "/main";
  for (uint32_t reg = 0; reg <= 5; ++reg) {
    primec::IrLinearScanRegisterAssignment assignment;
    assignment.virtualRegister = reg;
    assignment.spilled = (reg == 2);
    functionAllocation.assignments.push_back(assignment);
  }
  allocation.functions.push_back(std::move(functionAllocation));

  primec::IrVirtualRegisterScheduledModule scheduled;
  std::string error;
  REQUIRE(primec::scheduleIrVirtualRegisters(module, allocation, scheduled, error));
  CHECK(error.empty());
  REQUIRE(scheduled.functions.size() == 1);
  REQUIRE(scheduled.functions[0].blocks.size() == 1);
  const auto &scheduledBlock = scheduled.functions[0].blocks[0];
  REQUIRE(scheduledBlock.instructions.size() == 7);

  std::vector<size_t> scheduledOrder;
  for (const auto &instruction : scheduledBlock.instructions) {
    scheduledOrder.push_back(instruction.originalInstructionIndex);
  }
  CHECK(scheduledOrder == std::vector<size_t>{2, 0, 1, 4, 3, 5, 6});

  uint32_t addNormalLatency = 0;
  uint32_t addSpilledLatency = 0;
  for (const auto &instruction : scheduledBlock.instructions) {
    if (instruction.originalInstructionIndex == 4) {
      addNormalLatency = instruction.latencyScore;
    } else if (instruction.originalInstructionIndex == 5) {
      addSpilledLatency = instruction.latencyScore;
    }
  }
  CHECK(addNormalLatency == 2u);
  CHECK(addSpilledLatency == 4u);
}

TEST_CASE("scheduler ties pick lower original instruction index") {
  primec::IrVirtualRegisterModule module;
  primec::IrVirtualRegisterFunction function;
  function.name = "/main";
  primec::IrVirtualRegisterBlock block;
  block.startInstructionIndex = 0;
  block.endInstructionIndex = 7;
  block.reachable = true;

  primec::IrVirtualRegisterInstruction inst0;
  inst0.instruction = {primec::IrOpcode::PushI32, 11};
  inst0.defRegisters = {0};
  block.instructions.push_back(inst0);

  primec::IrVirtualRegisterInstruction inst1;
  inst1.instruction = {primec::IrOpcode::PushI32, 22};
  inst1.defRegisters = {1};
  block.instructions.push_back(inst1);

  primec::IrVirtualRegisterInstruction inst2;
  inst2.instruction = {primec::IrOpcode::PushI32, 33};
  inst2.defRegisters = {2};
  block.instructions.push_back(inst2);

  primec::IrVirtualRegisterInstruction inst3;
  inst3.instruction = {primec::IrOpcode::PushI32, 44};
  inst3.defRegisters = {3};
  block.instructions.push_back(inst3);

  primec::IrVirtualRegisterInstruction inst4;
  inst4.instruction = {primec::IrOpcode::AddI32, 0};
  inst4.useRegisters = {0, 1};
  inst4.defRegisters = {4};
  block.instructions.push_back(inst4);

  primec::IrVirtualRegisterInstruction inst5;
  inst5.instruction = {primec::IrOpcode::SubI32, 0};
  inst5.useRegisters = {2, 3};
  inst5.defRegisters = {5};
  block.instructions.push_back(inst5);

  primec::IrVirtualRegisterInstruction inst6;
  inst6.instruction = {primec::IrOpcode::ReturnI32, 0};
  inst6.useRegisters = {4};
  block.instructions.push_back(inst6);

  function.blocks.push_back(std::move(block));
  module.functions.push_back(std::move(function));

  primec::IrLinearScanModuleAllocation allocation;
  primec::IrLinearScanFunctionAllocation functionAllocation;
  functionAllocation.functionName = "/main";
  for (uint32_t reg = 0; reg <= 5; ++reg) {
    primec::IrLinearScanRegisterAssignment assignment;
    assignment.virtualRegister = reg;
    assignment.spilled = false;
    functionAllocation.assignments.push_back(assignment);
  }
  allocation.functions.push_back(std::move(functionAllocation));

  primec::IrVirtualRegisterScheduledModule scheduled;
  std::string error;
  REQUIRE(primec::scheduleIrVirtualRegisters(module, allocation, scheduled, error));
  CHECK(error.empty());
  REQUIRE(scheduled.functions.size() == 1);
  REQUIRE(scheduled.functions[0].blocks.size() == 1);
  const auto &scheduledBlock = scheduled.functions[0].blocks[0];
  REQUIRE(scheduledBlock.instructions.size() == 7);

  std::vector<size_t> scheduledOrder;
  for (const auto &instruction : scheduledBlock.instructions) {
    scheduledOrder.push_back(instruction.originalInstructionIndex);
  }
  CHECK(scheduledOrder == std::vector<size_t>{0, 1, 4, 2, 3, 5, 6});

  uint32_t addLatency = 0;
  uint32_t subLatency = 0;
  for (const auto &instruction : scheduledBlock.instructions) {
    if (instruction.originalInstructionIndex == 4) {
      addLatency = instruction.latencyScore;
    } else if (instruction.originalInstructionIndex == 5) {
      subLatency = instruction.latencyScore;
    }
  }
  CHECK(addLatency == 2u);
  CHECK(subLatency == 2u);
}

TEST_CASE("scheduler enforces barrier ordering boundaries") {
  primec::IrVirtualRegisterModule module;
  primec::IrVirtualRegisterFunction function;
  function.name = "/main";
  primec::IrVirtualRegisterBlock block;
  block.startInstructionIndex = 0;
  block.endInstructionIndex = 8;
  block.reachable = true;

  primec::IrVirtualRegisterInstruction inst0;
  inst0.instruction = {primec::IrOpcode::PushI32, 12};
  inst0.defRegisters = {0};
  block.instructions.push_back(inst0);

  primec::IrVirtualRegisterInstruction inst1;
  inst1.instruction = {primec::IrOpcode::PushI32, 3};
  inst1.defRegisters = {1};
  block.instructions.push_back(inst1);

  primec::IrVirtualRegisterInstruction inst2;
  inst2.instruction = {primec::IrOpcode::DivI32, 0};
  inst2.useRegisters = {0, 1};
  inst2.defRegisters = {2};
  block.instructions.push_back(inst2);

  primec::IrVirtualRegisterInstruction inst3;
  inst3.instruction = {primec::IrOpcode::PushI32, 9};
  inst3.defRegisters = {3};
  block.instructions.push_back(inst3);

  // Barrier: should remain after all prior instructions.
  primec::IrVirtualRegisterInstruction inst4;
  inst4.instruction = {primec::IrOpcode::PrintI32, 0};
  inst4.useRegisters = {2};
  block.instructions.push_back(inst4);

  primec::IrVirtualRegisterInstruction inst5;
  inst5.instruction = {primec::IrOpcode::PushI32, 5};
  inst5.defRegisters = {4};
  block.instructions.push_back(inst5);

  primec::IrVirtualRegisterInstruction inst6;
  inst6.instruction = {primec::IrOpcode::AddI32, 0};
  inst6.useRegisters = {3, 4};
  inst6.defRegisters = {5};
  block.instructions.push_back(inst6);

  // Barrier: should remain after all previous instructions.
  primec::IrVirtualRegisterInstruction inst7;
  inst7.instruction = {primec::IrOpcode::ReturnI32, 0};
  inst7.useRegisters = {5};
  block.instructions.push_back(inst7);

  function.blocks.push_back(std::move(block));
  module.functions.push_back(std::move(function));

  primec::IrLinearScanModuleAllocation allocation;
  primec::IrLinearScanFunctionAllocation functionAllocation;
  functionAllocation.functionName = "/main";
  for (uint32_t reg = 0; reg <= 5; ++reg) {
    primec::IrLinearScanRegisterAssignment assignment;
    assignment.virtualRegister = reg;
    assignment.spilled = false;
    functionAllocation.assignments.push_back(assignment);
  }
  allocation.functions.push_back(std::move(functionAllocation));

  primec::IrVirtualRegisterScheduledModule scheduled;
  std::string error;
  REQUIRE(primec::scheduleIrVirtualRegisters(module, allocation, scheduled, error));
  CHECK(error.empty());
  REQUIRE(scheduled.functions.size() == 1);
  REQUIRE(scheduled.functions[0].blocks.size() == 1);
  const auto &scheduledBlock = scheduled.functions[0].blocks[0];
  REQUIRE(scheduledBlock.instructions.size() == 8);

  std::vector<size_t> positionByInstruction(8, 0);
  for (size_t position = 0; position < scheduledBlock.instructions.size(); ++position) {
    positionByInstruction[scheduledBlock.instructions[position].originalInstructionIndex] = position;
  }

  CHECK(positionByInstruction[0] < positionByInstruction[4]);
  CHECK(positionByInstruction[1] < positionByInstruction[4]);
  CHECK(positionByInstruction[2] < positionByInstruction[4]);
  CHECK(positionByInstruction[3] < positionByInstruction[4]);
  CHECK(positionByInstruction[4] < positionByInstruction[5]);
  CHECK(positionByInstruction[4] < positionByInstruction[6]);
  CHECK(positionByInstruction[4] < positionByInstruction[7]);
  CHECK(positionByInstruction[5] < positionByInstruction[7]);
  CHECK(positionByInstruction[6] < positionByInstruction[7]);
}

TEST_CASE("scheduler prioritizes spilled-def latency penalty") {
  primec::IrVirtualRegisterModule module;
  primec::IrVirtualRegisterFunction function;
  function.name = "/main";
  primec::IrVirtualRegisterBlock block;
  block.startInstructionIndex = 0;
  block.endInstructionIndex = 7;
  block.reachable = true;

  primec::IrVirtualRegisterInstruction inst0;
  inst0.instruction = {primec::IrOpcode::PushI32, 10};
  inst0.defRegisters = {0};
  block.instructions.push_back(inst0);

  primec::IrVirtualRegisterInstruction inst1;
  inst1.instruction = {primec::IrOpcode::PushI32, 20};
  inst1.defRegisters = {1};
  block.instructions.push_back(inst1);

  primec::IrVirtualRegisterInstruction inst2;
  inst2.instruction = {primec::IrOpcode::PushI32, 30};
  inst2.defRegisters = {2};
  block.instructions.push_back(inst2);

  primec::IrVirtualRegisterInstruction inst3;
  inst3.instruction = {primec::IrOpcode::PushI32, 40};
  inst3.defRegisters = {3};
  block.instructions.push_back(inst3);

  primec::IrVirtualRegisterInstruction inst4;
  inst4.instruction = {primec::IrOpcode::AddI32, 0};
  inst4.useRegisters = {0, 1};
  inst4.defRegisters = {4};
  block.instructions.push_back(inst4);

  primec::IrVirtualRegisterInstruction inst5;
  inst5.instruction = {primec::IrOpcode::AddI32, 0};
  inst5.useRegisters = {2, 3};
  inst5.defRegisters = {5};
  block.instructions.push_back(inst5);

  primec::IrVirtualRegisterInstruction inst6;
  inst6.instruction = {primec::IrOpcode::ReturnI32, 0};
  inst6.useRegisters = {4};
  block.instructions.push_back(inst6);

  function.blocks.push_back(std::move(block));
  module.functions.push_back(std::move(function));

  primec::IrLinearScanModuleAllocation allocation;
  primec::IrLinearScanFunctionAllocation functionAllocation;
  functionAllocation.functionName = "/main";
  for (uint32_t reg = 0; reg <= 5; ++reg) {
    primec::IrLinearScanRegisterAssignment assignment;
    assignment.virtualRegister = reg;
    assignment.spilled = (reg == 5);
    functionAllocation.assignments.push_back(assignment);
  }
  allocation.functions.push_back(std::move(functionAllocation));

  primec::IrVirtualRegisterScheduledModule scheduled;
  std::string error;
  REQUIRE(primec::scheduleIrVirtualRegisters(module, allocation, scheduled, error));
  CHECK(error.empty());
  REQUIRE(scheduled.functions.size() == 1);
  REQUIRE(scheduled.functions[0].blocks.size() == 1);
  const auto &scheduledBlock = scheduled.functions[0].blocks[0];
  REQUIRE(scheduledBlock.instructions.size() == 7);

  std::vector<size_t> scheduledOrder;
  for (const auto &instruction : scheduledBlock.instructions) {
    scheduledOrder.push_back(instruction.originalInstructionIndex);
  }
  CHECK(scheduledOrder == std::vector<size_t>{0, 1, 4, 2, 3, 5, 6});

  uint32_t normalDefLatency = 0;
  uint32_t spilledDefLatency = 0;
  for (const auto &instruction : scheduledBlock.instructions) {
    if (instruction.originalInstructionIndex == 4) {
      normalDefLatency = instruction.latencyScore;
    } else if (instruction.originalInstructionIndex == 5) {
      spilledDefLatency = instruction.latencyScore;
    }
  }
  CHECK(normalDefLatency == 2u);
  CHECK(spilledDefLatency == 4u);
}

TEST_CASE("scheduler prioritizes combined spilled use-def penalty") {
  primec::IrVirtualRegisterModule module;
  primec::IrVirtualRegisterFunction function;
  function.name = "/main";
  primec::IrVirtualRegisterBlock block;
  block.startInstructionIndex = 0;
  block.endInstructionIndex = 7;
  block.reachable = true;

  primec::IrVirtualRegisterInstruction inst0;
  inst0.instruction = {primec::IrOpcode::PushI32, 1};
  inst0.defRegisters = {0};
  block.instructions.push_back(inst0);

  primec::IrVirtualRegisterInstruction inst1;
  inst1.instruction = {primec::IrOpcode::PushI32, 2};
  inst1.defRegisters = {1};
  block.instructions.push_back(inst1);

  primec::IrVirtualRegisterInstruction inst2;
  inst2.instruction = {primec::IrOpcode::PushI32, 3};
  inst2.defRegisters = {2};
  block.instructions.push_back(inst2);

  primec::IrVirtualRegisterInstruction inst3;
  inst3.instruction = {primec::IrOpcode::PushI32, 4};
  inst3.defRegisters = {3};
  block.instructions.push_back(inst3);

  primec::IrVirtualRegisterInstruction inst4;
  inst4.instruction = {primec::IrOpcode::AddI32, 0};
  inst4.useRegisters = {0, 1};
  inst4.defRegisters = {4};
  block.instructions.push_back(inst4);

  primec::IrVirtualRegisterInstruction inst5;
  inst5.instruction = {primec::IrOpcode::AddI32, 0};
  inst5.useRegisters = {2, 3};
  inst5.defRegisters = {5};
  block.instructions.push_back(inst5);

  primec::IrVirtualRegisterInstruction inst6;
  inst6.instruction = {primec::IrOpcode::ReturnI32, 0};
  inst6.useRegisters = {4};
  block.instructions.push_back(inst6);

  function.blocks.push_back(std::move(block));
  module.functions.push_back(std::move(function));

  primec::IrLinearScanModuleAllocation allocation;
  primec::IrLinearScanFunctionAllocation functionAllocation;
  functionAllocation.functionName = "/main";
  for (uint32_t reg = 0; reg <= 5; ++reg) {
    primec::IrLinearScanRegisterAssignment assignment;
    assignment.virtualRegister = reg;
    assignment.spilled = (reg == 2 || reg == 5);
    functionAllocation.assignments.push_back(assignment);
  }
  allocation.functions.push_back(std::move(functionAllocation));

  primec::IrVirtualRegisterScheduledModule scheduled;
  std::string error;
  REQUIRE(primec::scheduleIrVirtualRegisters(module, allocation, scheduled, error));
  CHECK(error.empty());
  REQUIRE(scheduled.functions.size() == 1);
  REQUIRE(scheduled.functions[0].blocks.size() == 1);
  const auto &scheduledBlock = scheduled.functions[0].blocks[0];
  REQUIRE(scheduledBlock.instructions.size() == 7);

  std::vector<size_t> scheduledOrder;
  for (const auto &instruction : scheduledBlock.instructions) {
    scheduledOrder.push_back(instruction.originalInstructionIndex);
  }
  CHECK(scheduledOrder == std::vector<size_t>{2, 0, 1, 4, 3, 5, 6});

  uint32_t normalLatency = 0;
  uint32_t combinedSpillLatency = 0;
  for (const auto &instruction : scheduledBlock.instructions) {
    if (instruction.originalInstructionIndex == 4) {
      normalLatency = instruction.latencyScore;
    } else if (instruction.originalInstructionIndex == 5) {
      combinedSpillLatency = instruction.latencyScore;
    }
  }
  CHECK(normalLatency == 2u);
  CHECK(combinedSpillLatency == 6u);
}

TEST_CASE("scheduler rejects allocation name mismatch") {
  primec::IrVirtualRegisterModule module;
  primec::IrVirtualRegisterFunction function;
  function.name = "/main";
  module.functions.push_back(std::move(function));

  primec::IrLinearScanModuleAllocation allocation;
  primec::IrLinearScanFunctionAllocation functionAllocation;
  functionAllocation.functionName = "/other";
  allocation.functions.push_back(std::move(functionAllocation));

  primec::IrVirtualRegisterScheduledModule scheduled;
  std::string error;
  CHECK_FALSE(primec::scheduleIrVirtualRegisters(module, allocation, scheduled, error));
  CHECK(error.find("matching function names") != std::string::npos);
}

TEST_CASE("scheduler rejects allocation function count mismatch") {
  primec::IrVirtualRegisterModule module;
  primec::IrVirtualRegisterFunction function;
  function.name = "/main";
  module.functions.push_back(std::move(function));

  primec::IrLinearScanModuleAllocation allocation;

  primec::IrVirtualRegisterScheduledModule scheduled;
  std::string error;
  CHECK_FALSE(primec::scheduleIrVirtualRegisters(module, allocation, scheduled, error));
  CHECK(error.find("matching function counts") != std::string::npos);
}

