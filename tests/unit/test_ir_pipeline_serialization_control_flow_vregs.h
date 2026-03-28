#pragma once

TEST_CASE("virtual-register lowering preserves vm behavior across control flow and calls") {
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
  uint64_t baselineResult = 0;
  REQUIRE(vm.execute(module, baselineResult, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterModule virtualModule;
  REQUIRE(primec::lowerIrModuleToBlockVirtualRegisters(module, virtualModule, error));
  CHECK(error.empty());
  REQUIRE(virtualModule.functions.size() == module.functions.size());
  const auto &virtualMain = virtualModule.functions[0];
  REQUIRE(virtualMain.blocks.size() >= 2);
  CHECK(virtualMain.blocks[0].reachable);

  bool sawConditionalSplit = false;
  bool sawUseDefAssignments = false;
  for (const auto &block : virtualMain.blocks) {
    if (block.successorEdges.size() == 2) {
      sawConditionalSplit = true;
    }
    for (const auto &instruction : block.instructions) {
      if (!instruction.useRegisters.empty() || !instruction.defRegisters.empty()) {
        sawUseDefAssignments = true;
      }
    }
    for (const auto &edge : block.successorEdges) {
      REQUIRE(edge.successorBlockIndex < virtualMain.blocks.size());
      const auto &successor = virtualMain.blocks[edge.successorBlockIndex];
      CHECK(edge.stackMoves.size() == successor.entryRegisters.size());
    }
  }
  CHECK(sawConditionalSplit);
  CHECK(sawUseDefAssignments);

  primec::IrModule loweredBackModule;
  REQUIRE(primec::liftBlockVirtualRegistersToIrModule(virtualModule, loweredBackModule, error));
  CHECK(error.empty());
  REQUIRE(primec::validateIrModule(loweredBackModule, primec::IrValidationTarget::Vm, error));
  CHECK(error.empty());

  uint64_t roundTripResult = 0;
  REQUIRE(vm.execute(loweredBackModule, roundTripResult, error));
  CHECK(error.empty());
  CHECK(roundTripResult == baselineResult);
}

TEST_CASE("virtual-register lowering rejects inconsistent stack depth merges") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 4});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 2});
  mainFn.instructions.push_back({primec::IrOpcode::Jump, 4});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  primec::IrVirtualRegisterModule virtualModule;
  std::string error;
  CHECK_FALSE(primec::lowerIrModuleToBlockVirtualRegisters(module, virtualModule, error));
  CHECK(error.find("inconsistent stack depth") != std::string::npos);
}

TEST_CASE("virtual-register liveness builds deterministic loop intervals") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  mainFn.instructions.push_back({primec::IrOpcode::Dup, 0});
  mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 6});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  mainFn.instructions.push_back({primec::IrOpcode::SubI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Jump, 1});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  std::string error;
  primec::IrVirtualRegisterModule virtualModule;
  REQUIRE(primec::lowerIrModuleToBlockVirtualRegisters(module, virtualModule, error));
  CHECK(error.empty());
  REQUIRE(virtualModule.functions.size() == 1);

  primec::IrVirtualRegisterModuleLiveness liveness;
  REQUIRE(primec::buildIrVirtualRegisterLiveness(virtualModule, liveness, error));
  CHECK(error.empty());
  REQUIRE(liveness.functions.size() == 1);

  const auto &functionLiveness = liveness.functions[0];
  REQUIRE(functionLiveness.blocks.size() == 4);
  CHECK(functionLiveness.blocks[0].liveInRegisters.empty());
  CHECK(functionLiveness.blocks[0].liveOutRegisters == std::vector<uint32_t>{3});
  CHECK(functionLiveness.blocks[1].liveInRegisters == std::vector<uint32_t>{0});
  CHECK(functionLiveness.blocks[1].liveOutRegisters == std::vector<uint32_t>{0});
  CHECK(functionLiveness.blocks[2].liveInRegisters == std::vector<uint32_t>{1});
  CHECK(functionLiveness.blocks[2].liveOutRegisters == std::vector<uint32_t>{6});
  CHECK(functionLiveness.blocks[3].liveInRegisters == std::vector<uint32_t>{2});
  CHECK(functionLiveness.blocks[3].liveOutRegisters.empty());

  REQUIRE(functionLiveness.intervals.size() == 7);
  CHECK(functionLiveness.intervals[0].virtualRegister == 3);
  CHECK(functionLiveness.intervals[0].ranges[0].startPosition == 1u);
  CHECK(functionLiveness.intervals[0].ranges[0].endPosition == 1u);
  CHECK(functionLiveness.intervals[1].virtualRegister == 0);
  CHECK(functionLiveness.intervals[1].ranges[0].startPosition == 2u);
  CHECK(functionLiveness.intervals[1].ranges[0].endPosition == 5u);
  CHECK(functionLiveness.intervals[2].virtualRegister == 4);
  CHECK(functionLiveness.intervals[2].ranges[0].startPosition == 3u);
  CHECK(functionLiveness.intervals[2].ranges[0].endPosition == 4u);
  CHECK(functionLiveness.intervals[3].virtualRegister == 1);
  CHECK(functionLiveness.intervals[3].ranges[0].startPosition == 6u);
  CHECK(functionLiveness.intervals[3].ranges[0].endPosition == 8u);
  CHECK(functionLiveness.intervals[4].virtualRegister == 5);
  CHECK(functionLiveness.intervals[4].ranges[0].startPosition == 7u);
  CHECK(functionLiveness.intervals[4].ranges[0].endPosition == 8u);
  CHECK(functionLiveness.intervals[5].virtualRegister == 6);
  CHECK(functionLiveness.intervals[5].ranges[0].startPosition == 9u);
  CHECK(functionLiveness.intervals[5].ranges[0].endPosition == 11u);
  CHECK(functionLiveness.intervals[6].virtualRegister == 2);
  CHECK(functionLiveness.intervals[6].ranges[0].startPosition == 12u);
  CHECK(functionLiveness.intervals[6].ranges[0].endPosition == 12u);
}

TEST_CASE("virtual-register liveness tie-breaks equal intervals by register id") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 8});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 9});
  mainFn.instructions.push_back({primec::IrOpcode::Jump, 4});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
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
  REQUIRE(liveness.functions.size() == 1);
  const auto &intervals = liveness.functions[0].intervals;

  REQUIRE(intervals.size() == 5);
  CHECK(intervals[2].virtualRegister == 0);
  CHECK(intervals[3].virtualRegister == 1);
  CHECK(intervals[2].ranges[0].startPosition == intervals[3].ranges[0].startPosition);
  CHECK(intervals[2].ranges[0].endPosition == intervals[3].ranges[0].endPosition);
}

TEST_CASE("linear-scan allocator has deterministic spill placement on loop cfg") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  mainFn.instructions.push_back({primec::IrOpcode::Dup, 0});
  mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 6});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  mainFn.instructions.push_back({primec::IrOpcode::SubI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Jump, 1});
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
  options.physicalRegisterCount = 1;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;
  primec::IrLinearScanModuleAllocation allocation;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());
  REQUIRE(allocation.functions.size() == 1);

  const auto &functionAllocation = allocation.functions[0];
  CHECK(functionAllocation.spillSlotCount == 2u);
  REQUIRE(functionAllocation.assignments.size() == 7);

  CHECK(functionAllocation.assignments[0].virtualRegister == 0u);
  CHECK(functionAllocation.assignments[0].spilled);
  CHECK(functionAllocation.assignments[0].spillSlot == 0u);
  CHECK(functionAllocation.assignments[1].virtualRegister == 1u);
  CHECK_FALSE(functionAllocation.assignments[1].spilled);
  CHECK(functionAllocation.assignments[1].physicalRegister == 0u);
  CHECK(functionAllocation.assignments[2].virtualRegister == 2u);
  CHECK_FALSE(functionAllocation.assignments[2].spilled);
  CHECK(functionAllocation.assignments[2].physicalRegister == 0u);
  CHECK(functionAllocation.assignments[3].virtualRegister == 3u);
  CHECK_FALSE(functionAllocation.assignments[3].spilled);
  CHECK(functionAllocation.assignments[3].physicalRegister == 0u);
  CHECK(functionAllocation.assignments[4].virtualRegister == 4u);
  CHECK_FALSE(functionAllocation.assignments[4].spilled);
  CHECK(functionAllocation.assignments[4].physicalRegister == 0u);
  CHECK(functionAllocation.assignments[5].virtualRegister == 5u);
  CHECK(functionAllocation.assignments[5].spilled);
  CHECK(functionAllocation.assignments[5].spillSlot == 1u);
  CHECK(functionAllocation.assignments[6].virtualRegister == 6u);
  CHECK_FALSE(functionAllocation.assignments[6].spilled);
  CHECK(functionAllocation.assignments[6].physicalRegister == 0u);
}

TEST_CASE("linear-scan allocator tie-break keeps lower register id resident") {
  primec::IrVirtualRegisterModuleLiveness liveness;
  primec::IrVirtualRegisterFunctionLiveness function;
  function.functionName = "/main";
  function.intervals = {
      {1, {{0, 4}}},
      {0, {{0, 4}}},
      {2, {{5, 6}}},
  };
  liveness.functions.push_back(std::move(function));

  primec::IrLinearScanAllocatorOptions options;
  options.physicalRegisterCount = 1;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;

  primec::IrLinearScanModuleAllocation allocation;
  std::string error;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());
  REQUIRE(allocation.functions.size() == 1);
  const auto &assignments = allocation.functions[0].assignments;
  REQUIRE(assignments.size() == 3);

  CHECK(assignments[0].virtualRegister == 0u);
  CHECK_FALSE(assignments[0].spilled);
  CHECK(assignments[0].physicalRegister == 0u);
  CHECK(assignments[1].virtualRegister == 1u);
  CHECK(assignments[1].spilled);
  CHECK(assignments[1].spillSlot == 0u);
  CHECK(assignments[2].virtualRegister == 2u);
  CHECK_FALSE(assignments[2].spilled);
  CHECK(assignments[2].physicalRegister == 0u);
}

TEST_CASE("linear-scan allocator rejects unknown spill policy") {
  primec::IrVirtualRegisterModuleLiveness liveness;
  primec::IrVirtualRegisterFunctionLiveness function;
  function.functionName = "/main";
  function.intervals = {
      {0, {{0, 1}}},
  };
  liveness.functions.push_back(std::move(function));

  primec::IrLinearScanAllocatorOptions options;
  options.physicalRegisterCount = 1;
  options.spillPolicy = static_cast<primec::IrLinearScanSpillPolicy>(99);

  primec::IrLinearScanModuleAllocation allocation;
  std::string error;
  CHECK_FALSE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.find("spill policy") != std::string::npos);
}

TEST_CASE("linear-scan allocator rejects invalid interval ranges") {
  primec::IrVirtualRegisterModuleLiveness liveness;
  primec::IrVirtualRegisterFunctionLiveness function;
  function.functionName = "/main";
  function.intervals = {
      {0, {{3, 1}}},
  };
  liveness.functions.push_back(std::move(function));

  primec::IrLinearScanAllocatorOptions options;
  options.physicalRegisterCount = 1;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;

  primec::IrLinearScanModuleAllocation allocation;
  std::string error;
  CHECK_FALSE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.find("invalid interval range") != std::string::npos);
}

TEST_CASE("linear-scan allocator ignores empty intervals deterministically") {
  primec::IrVirtualRegisterModuleLiveness liveness;
  primec::IrVirtualRegisterFunctionLiveness function;
  function.functionName = "/main";
  function.intervals = {
      {2, {}},
      {1, {{0, 1}}},
      {0, {}},
      {3, {{2, 3}}},
  };
  liveness.functions.push_back(std::move(function));

  primec::IrLinearScanAllocatorOptions options;
  options.physicalRegisterCount = 1;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;

  primec::IrLinearScanModuleAllocation allocation;
  std::string error;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());
  REQUIRE(allocation.functions.size() == 1);
  const auto &assignments = allocation.functions[0].assignments;
  REQUIRE(assignments.size() == 2);

  CHECK(assignments[0].virtualRegister == 1u);
  CHECK_FALSE(assignments[0].spilled);
  CHECK(assignments[0].physicalRegister == 0u);
  CHECK(assignments[1].virtualRegister == 3u);
  CHECK_FALSE(assignments[1].spilled);
  CHECK(assignments[1].physicalRegister == 0u);
  CHECK(allocation.functions[0].spillSlotCount == 0u);
}

TEST_CASE("linear-scan allocator spills all intervals with zero registers") {
  primec::IrVirtualRegisterModuleLiveness liveness;
  primec::IrVirtualRegisterFunctionLiveness function;
  function.functionName = "/main";
  function.intervals = {
      {2, {{3, 5}}},
      {0, {{0, 2}}},
      {1, {{1, 4}}},
  };
  liveness.functions.push_back(std::move(function));

  primec::IrLinearScanAllocatorOptions options;
  options.physicalRegisterCount = 0;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;

  primec::IrLinearScanModuleAllocation allocation;
  std::string error;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());
  REQUIRE(allocation.functions.size() == 1);
  const auto &assignments = allocation.functions[0].assignments;
  REQUIRE(assignments.size() == 3);

  for (size_t index = 0; index < assignments.size(); ++index) {
    CHECK(assignments[index].virtualRegister == static_cast<uint32_t>(index));
    CHECK(assignments[index].spilled);
    CHECK(assignments[index].spillSlot == static_cast<uint32_t>(index));
  }
  CHECK(allocation.functions[0].spillSlotCount == 3u);
}

