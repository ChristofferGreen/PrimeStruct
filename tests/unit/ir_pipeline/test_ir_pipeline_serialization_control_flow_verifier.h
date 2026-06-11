#pragma once

TEST_CASE("virtual-register verifier accepts valid scheduled allocation output") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 12});
  mainFn.instructions.push_back({primec::IrOpcode::CmpGtI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 8});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
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

  primec::IrVirtualRegisterScheduledModule scheduled;
  REQUIRE(primec::scheduleIrVirtualRegisters(virtualModule, allocation, scheduled, error));
  CHECK(error.empty());

  REQUIRE(primec::verifyIrVirtualRegisterScheduleAndAllocation(virtualModule, allocation, scheduled, error));
  CHECK(error.empty());
}

TEST_CASE("virtual-register verifier rejects use-before-def schedules") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 8});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 9});
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

  primec::IrVirtualRegisterScheduledModule scheduled;
  REQUIRE(primec::scheduleIrVirtualRegisters(virtualModule, allocation, scheduled, error));
  CHECK(error.empty());
  REQUIRE(scheduled.functions.size() == 1);
  REQUIRE(scheduled.functions[0].blocks.size() == 1);
  REQUIRE(scheduled.functions[0].blocks[0].instructions.size() == 4);

  auto &instructions = scheduled.functions[0].blocks[0].instructions;
  std::swap(instructions[0], instructions[2]);
  CHECK_FALSE(primec::verifyIrVirtualRegisterScheduleAndAllocation(virtualModule, allocation, scheduled, error));
  CHECK(error.find("uses register before definition") != std::string::npos);
}

TEST_CASE("virtual-register verifier rejects branch-edge disagreement") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  mainFn.instructions.push_back({primec::IrOpcode::CmpGtI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 6});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 9});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
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

  primec::IrVirtualRegisterScheduledModule scheduled;
  REQUIRE(primec::scheduleIrVirtualRegisters(virtualModule, allocation, scheduled, error));
  CHECK(error.empty());
  REQUIRE(scheduled.functions.size() == 1);
  REQUIRE(scheduled.functions[0].blocks.size() >= 1);
  REQUIRE(!scheduled.functions[0].blocks[0].successorEdges.empty());

  scheduled.functions[0].blocks[0].successorEdges[0].successorBlockIndex += 1;
  CHECK_FALSE(primec::verifyIrVirtualRegisterScheduleAndAllocation(virtualModule, allocation, scheduled, error));
  CHECK(error.find("branch-edge value agreement") != std::string::npos);
}

