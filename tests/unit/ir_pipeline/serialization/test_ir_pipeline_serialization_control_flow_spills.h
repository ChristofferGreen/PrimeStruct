#pragma once

TEST_CASE("spill insertion verifies branch and call spill correctness") {
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
  options.physicalRegisterCount = 0;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;
  primec::IrLinearScanModuleAllocation allocation;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterSpillPlan plan;
  REQUIRE(primec::insertIrVirtualRegisterSpills(virtualModule, allocation, plan, error));
  CHECK(error.empty());
  REQUIRE(primec::verifyIrVirtualRegisterSpillPlan(virtualModule, allocation, plan, error));
  CHECK(error.empty());
  REQUIRE(plan.functions.size() == 2);

  bool sawReload = false;
  bool sawSpill = false;
  bool sawEdgeOp = false;
  bool sawCallSpill = false;
  for (const auto &functionPlan : plan.functions) {
    for (const auto &blockPlan : functionPlan.blocks) {
      for (const auto &instructionPlan : blockPlan.instructions) {
        if (!instructionPlan.beforeInstructionOps.empty()) {
          sawReload = true;
        }
        if (!instructionPlan.afterInstructionOps.empty()) {
          sawSpill = true;
        }
        if (instructionPlan.instruction.instruction.op == primec::IrOpcode::Call &&
            !instructionPlan.afterInstructionOps.empty()) {
          sawCallSpill = true;
        }
      }
      for (const auto &edgePlan : blockPlan.successorEdges) {
        if (!edgePlan.edgeOps.empty()) {
          sawEdgeOp = true;
        }
      }
    }
  }
  CHECK(sawReload);
  CHECK(sawSpill);
  const bool sawBoundarySpill = sawEdgeOp || sawCallSpill;
  CHECK(sawBoundarySpill);
  CHECK(sawCallSpill);
}

TEST_CASE("spill insertion uses deterministic op ordering for spilled add uses") {
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
  options.physicalRegisterCount = 0;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;
  primec::IrLinearScanModuleAllocation allocation;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterSpillPlan plan;
  REQUIRE(primec::insertIrVirtualRegisterSpills(virtualModule, allocation, plan, error));
  CHECK(error.empty());
  REQUIRE(plan.functions.size() == 1);
  REQUIRE(plan.functions[0].blocks.size() == 1);
  REQUIRE(plan.functions[0].blocks[0].instructions.size() == 4);

  const auto &addInstructionPlan = plan.functions[0].blocks[0].instructions[2];
  REQUIRE(addInstructionPlan.instruction.instruction.op == primec::IrOpcode::AddI32);
  REQUIRE(addInstructionPlan.beforeInstructionOps.size() == 2);
  CHECK(addInstructionPlan.beforeInstructionOps[0].kind == primec::IrVirtualRegisterSpillOpKind::ReloadFromSpill);
  CHECK(addInstructionPlan.beforeInstructionOps[1].kind == primec::IrVirtualRegisterSpillOpKind::ReloadFromSpill);
  CHECK(addInstructionPlan.beforeInstructionOps[0].virtualRegister <
        addInstructionPlan.beforeInstructionOps[1].virtualRegister);
}

TEST_CASE("spill insertion verifier rejects missing reload ops") {
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
  options.physicalRegisterCount = 0;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;
  primec::IrLinearScanModuleAllocation allocation;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterSpillPlan plan;
  REQUIRE(primec::insertIrVirtualRegisterSpills(virtualModule, allocation, plan, error));
  CHECK(error.empty());
  REQUIRE(plan.functions.size() == 1);
  REQUIRE(plan.functions[0].blocks.size() == 1);
  REQUIRE(plan.functions[0].blocks[0].instructions.size() == 4);

  plan.functions[0].blocks[0].instructions[2].beforeInstructionOps.clear();
  CHECK_FALSE(primec::verifyIrVirtualRegisterSpillPlan(virtualModule, allocation, plan, error));
  CHECK(error.find("missing reload op") != std::string::npos);
}

TEST_CASE("spill insertion verifier rejects missing spill ops") {
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
  options.physicalRegisterCount = 0;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;
  primec::IrLinearScanModuleAllocation allocation;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterSpillPlan plan;
  REQUIRE(primec::insertIrVirtualRegisterSpills(virtualModule, allocation, plan, error));
  CHECK(error.empty());
  REQUIRE(plan.functions.size() == 1);
  REQUIRE(plan.functions[0].blocks.size() == 1);
  REQUIRE(plan.functions[0].blocks[0].instructions.size() == 4);

  plan.functions[0].blocks[0].instructions[2].afterInstructionOps.clear();
  CHECK_FALSE(primec::verifyIrVirtualRegisterSpillPlan(virtualModule, allocation, plan, error));
  CHECK(error.find("missing spill op") != std::string::npos);
}

TEST_CASE("spill insertion verifier rejects successor index mismatch") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  mainFn.instructions.push_back({primec::IrOpcode::CmpGtI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 6});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
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
  options.physicalRegisterCount = 0;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;
  primec::IrLinearScanModuleAllocation allocation;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterSpillPlan plan;
  REQUIRE(primec::insertIrVirtualRegisterSpills(virtualModule, allocation, plan, error));
  CHECK(error.empty());
  REQUIRE(primec::verifyIrVirtualRegisterSpillPlan(virtualModule, allocation, plan, error));
  CHECK(error.empty());
  REQUIRE(plan.functions.size() == 1);
  REQUIRE(!plan.functions[0].blocks.empty());

  bool mutatedEdge = false;
  for (auto &blockPlan : plan.functions[0].blocks) {
    if (!blockPlan.successorEdges.empty()) {
      blockPlan.successorEdges[0].successorBlockIndex += 1;
      mutatedEdge = true;
      break;
    }
  }
  REQUIRE(mutatedEdge);

  CHECK_FALSE(primec::verifyIrVirtualRegisterSpillPlan(virtualModule, allocation, plan, error));
  CHECK(error.find("mismatched successor block index") != std::string::npos);
}

TEST_CASE("spill insertion verifier rejects edge spill-op mismatch") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  mainFn.instructions.push_back({primec::IrOpcode::CmpGtI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 6});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
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
  options.physicalRegisterCount = 0;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;
  primec::IrLinearScanModuleAllocation allocation;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterSpillPlan plan;
  REQUIRE(primec::insertIrVirtualRegisterSpills(virtualModule, allocation, plan, error));
  CHECK(error.empty());
  REQUIRE(primec::verifyIrVirtualRegisterSpillPlan(virtualModule, allocation, plan, error));
  CHECK(error.empty());
  REQUIRE(plan.functions.size() == 1);
  REQUIRE(!plan.functions[0].blocks.empty());

  bool mutatedEdgeOps = false;
  for (auto &blockPlan : plan.functions[0].blocks) {
    if (!blockPlan.successorEdges.empty()) {
      blockPlan.successorEdges[0].edgeOps.push_back(
          {primec::IrVirtualRegisterSpillOpKind::ReloadFromSpill, 999u, 999u});
      mutatedEdgeOps = true;
      break;
    }
  }
  REQUIRE(mutatedEdgeOps);

  CHECK_FALSE(primec::verifyIrVirtualRegisterSpillPlan(virtualModule, allocation, plan, error));
  CHECK(error.find("mismatched edge spill ops") != std::string::npos);
}

TEST_CASE("spill insertion verifier rejects successor edge count mismatch") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  mainFn.instructions.push_back({primec::IrOpcode::CmpGtI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 6});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
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
  options.physicalRegisterCount = 0;
  options.spillPolicy = primec::IrLinearScanSpillPolicy::SpillFarthestEnd;
  primec::IrLinearScanModuleAllocation allocation;
  REQUIRE(primec::allocateIrVirtualRegistersLinearScan(liveness, options, allocation, error));
  CHECK(error.empty());

  primec::IrVirtualRegisterSpillPlan plan;
  REQUIRE(primec::insertIrVirtualRegisterSpills(virtualModule, allocation, plan, error));
  CHECK(error.empty());
  REQUIRE(primec::verifyIrVirtualRegisterSpillPlan(virtualModule, allocation, plan, error));
  CHECK(error.empty());
  REQUIRE(plan.functions.size() == 1);
  REQUIRE(!plan.functions[0].blocks.empty());

  bool mutatedEdgeCount = false;
  for (auto &blockPlan : plan.functions[0].blocks) {
    if (!blockPlan.successorEdges.empty()) {
      blockPlan.successorEdges.pop_back();
      mutatedEdgeCount = true;
      break;
    }
  }
  REQUIRE(mutatedEdgeCount);

  CHECK_FALSE(primec::verifyIrVirtualRegisterSpillPlan(virtualModule, allocation, plan, error));
  CHECK(error.find("matching successor edge counts") != std::string::npos);
}

TEST_CASE("spill insertion verifier rejects function count mismatch") {
  primec::IrVirtualRegisterModule module;
  primec::IrVirtualRegisterFunction function;
  function.name = "/main";
  module.functions.push_back(std::move(function));

  primec::IrLinearScanModuleAllocation allocation;
  primec::IrVirtualRegisterSpillPlan plan;
  std::string error;

  CHECK_FALSE(primec::verifyIrVirtualRegisterSpillPlan(module, allocation, plan, error));
  CHECK(error.find("matching function counts") != std::string::npos);
}

