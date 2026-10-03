#include "primec/ir/IrCfg.h"

#include "third_party/doctest.h"

#include <string>
#include <vector>

TEST_SUITE_BEGIN("primestruct.ir.cfg");

namespace {

using primec::IrCfg;
using primec::IrCfgError;
using primec::IrCfgErrorKind;
using primec::IrFunction;
using primec::IrInstruction;
using primec::IrModule;
using primec::IrOpcode;

IrFunction makeFunction(std::vector<IrInstruction> instructions, uint32_t parameterCount = 0) {
  IrFunction function;
  function.name = "/f";
  function.parameterCount = parameterCount;
  function.instructions = std::move(instructions);
  return function;
}

IrModule makeModule(IrFunction function) {
  IrModule module;
  module.functions.push_back(std::move(function));
  module.entryIndex = 0;
  return module;
}

bool build(const IrModule &module, IrCfg &cfg, IrCfgError &error) {
  return primec::buildIrCfg(module.functions[0], module, cfg, error);
}

} // namespace

TEST_CASE("cfg of straight-line code is one block with depth bookkeeping") {
  const IrModule module = makeModule(makeFunction({
      {IrOpcode::PushI32, 1},
      {IrOpcode::PushI32, 2},
      {IrOpcode::PushI32, 3},
      {IrOpcode::AddI32, 0},
      {IrOpcode::AddI32, 0},
      {IrOpcode::ReturnI32, 0},
  }));
  IrCfg cfg;
  IrCfgError error;
  REQUIRE(build(module, cfg, error));
  CHECK(error.kind == IrCfgErrorKind::None);
  REQUIRE(cfg.blocks.size() == 1);
  const auto &block = cfg.blocks[0];
  CHECK(block.start == 0);
  CHECK(block.end == 6);
  CHECK(block.reachable);
  CHECK(block.entryDepth == 0);
  CHECK(block.exitDepth == 0);
  CHECK(block.maxDepth == 3);
  CHECK(block.successors.empty());
  CHECK(block.predecessors.empty());
  CHECK(cfg.maxStackDepth == 3);
}

TEST_CASE("cfg of a counted loop lists jump target before fallthrough") {
  // 0: PushI32 5; 1: StoreLocal 0
  // 2: LoadLocal 0; 3: JumpIfZero 8        <- loop header
  // 4: LoadLocal 0; 5: PushI32 1; 6: SubI32; 7: StoreLocal 0 ... falls into jump
  // (the jump back is the next instruction)
  const IrModule module = makeModule(makeFunction({
      {IrOpcode::PushI32, 5},
      {IrOpcode::StoreLocal, 0},
      {IrOpcode::LoadLocal, 0},
      {IrOpcode::JumpIfZero, 10},
      {IrOpcode::LoadLocal, 0},
      {IrOpcode::PushI32, 1},
      {IrOpcode::SubI32, 0},
      {IrOpcode::StoreLocal, 0},
      {IrOpcode::Jump, 2},
      {IrOpcode::PushI32, 99}, // unreachable
      {IrOpcode::PushI32, 0},
      {IrOpcode::ReturnI32, 0},
  }));
  IrCfg cfg;
  IrCfgError error;
  REQUIRE(build(module, cfg, error));
  REQUIRE(cfg.blocks.size() == 5);
  // Blocks: [0,2) entry, [2,4) header, [4,9) body, [9,10) dead, [10,12) exit.
  CHECK(cfg.blocks[0].start == 0);
  CHECK(cfg.blocks[1].start == 2);
  CHECK(cfg.blocks[2].start == 4);
  CHECK(cfg.blocks[3].start == 9);
  CHECK(cfg.blocks[4].start == 10);
  CHECK(cfg.blocks[4].end == 12);

  CHECK(cfg.blocks[0].successors == std::vector<size_t>{1});
  // Header: jump target (exit block) first, then fallthrough (body).
  CHECK(cfg.blocks[1].successors == std::vector<size_t>{4, 2});
  CHECK(cfg.blocks[2].successors == std::vector<size_t>{1});
  CHECK(cfg.blocks[3].successors == std::vector<size_t>{4});
  CHECK(cfg.blocks[4].successors.empty());

  CHECK(cfg.blocks[1].predecessors == std::vector<size_t>{0, 2});
  CHECK(cfg.blocks[4].predecessors == std::vector<size_t>{1, 3});

  CHECK(cfg.blocks[0].reachable);
  CHECK(cfg.blocks[1].reachable);
  CHECK(cfg.blocks[2].reachable);
  CHECK_FALSE(cfg.blocks[3].reachable);
  CHECK(cfg.blocks[4].reachable);
  // Depth is zero at every join in this loop.
  CHECK(cfg.blocks[1].entryDepth == 0);
  CHECK(cfg.blocks[4].entryDepth == 0);
  CHECK(cfg.maxStackDepth == 2);
}

TEST_CASE("cfg entry block starts at the function's parameter count") {
  const IrModule module = makeModule(makeFunction(
      {
          {IrOpcode::StoreLocal, 1},
          {IrOpcode::StoreLocal, 0},
          {IrOpcode::LoadLocal, 0},
          {IrOpcode::LoadLocal, 1},
          {IrOpcode::AddI32, 0},
          {IrOpcode::ReturnI32, 0},
      },
      2));
  IrCfg cfg;
  IrCfgError error;
  REQUIRE(build(module, cfg, error));
  REQUIRE(cfg.blocks.size() == 1);
  CHECK(cfg.blocks[0].entryDepth == 2);
  CHECK(cfg.blocks[0].maxDepth == 2);
  CHECK(cfg.maxStackDepth == 2);

  // The same body without declared parameters underflows at the first store.
  const IrModule noParams = makeModule(makeFunction({{IrOpcode::StoreLocal, 0}, {IrOpcode::ReturnVoid, 0}}));
  CHECK_FALSE(build(noParams, cfg, error));
  CHECK(error.kind == IrCfgErrorKind::StackUnderflow);
  CHECK(error.instructionIndex == 0);
  CHECK(error.opcode == IrOpcode::StoreLocal);
}

TEST_CASE("cfg treats a jump to the function end as having no successor edge") {
  const IrModule module = makeModule(makeFunction({
      {IrOpcode::PushI32, 1},
      {IrOpcode::JumpIfZero, 4},
      {IrOpcode::PushI32, 7},
      {IrOpcode::ReturnI32, 0},
  }));
  IrCfg cfg;
  IrCfgError error;
  REQUIRE(build(module, cfg, error));
  REQUIRE(cfg.blocks.size() == 2);
  // The conditional's only edge is the fallthrough; the taken edge leaves the function.
  CHECK(cfg.blocks[0].successors == std::vector<size_t>{1});
  CHECK(cfg.blocks[1].reachable);
}

TEST_CASE("cfg of an empty function has no blocks") {
  const IrModule module = makeModule(makeFunction({}));
  IrCfg cfg;
  IrCfgError error;
  REQUIRE(build(module, cfg, error));
  CHECK(cfg.blocks.empty());
  CHECK(cfg.maxStackDepth == 0);
}

TEST_CASE("cfg reports structured errors with the offending instruction") {
  IrCfg cfg;
  IrCfgError error;

  SUBCASE("jump target past the end") {
    const IrModule module = makeModule(makeFunction({{IrOpcode::Jump, 9}, {IrOpcode::ReturnVoid, 0}}));
    CHECK_FALSE(build(module, cfg, error));
    CHECK(error.kind == IrCfgErrorKind::InvalidJumpTarget);
    CHECK(error.instructionIndex == 0);
    CHECK(error.opcode == IrOpcode::Jump);
  }
  SUBCASE("stack underflow") {
    const IrModule module = makeModule(makeFunction({
        {IrOpcode::PushI32, 1},
        {IrOpcode::AddI32, 0},
        {IrOpcode::ReturnI32, 0},
    }));
    CHECK_FALSE(build(module, cfg, error));
    CHECK(error.kind == IrCfgErrorKind::StackUnderflow);
    CHECK(error.instructionIndex == 1);
    CHECK(error.opcode == IrOpcode::AddI32);
  }
  SUBCASE("dup with nothing to read") {
    const IrModule module = makeModule(makeFunction({{IrOpcode::Dup, 0}, {IrOpcode::ReturnVoid, 0}}));
    CHECK_FALSE(build(module, cfg, error));
    CHECK(error.kind == IrCfgErrorKind::InvalidDup);
    CHECK(error.instructionIndex == 0);
    CHECK(error.opcode == IrOpcode::Dup);
  }
  SUBCASE("paths reaching a join with different depths") {
    const IrModule module = makeModule(makeFunction({
        {IrOpcode::PushI32, 1},
        {IrOpcode::JumpIfZero, 4}, // taken: depth 0 at the join
        {IrOpcode::PushI32, 5},    // fallthrough: depth 1 at the join
        {IrOpcode::Jump, 4},
        {IrOpcode::ReturnVoid, 0}, // join, instruction 4
    }));
    CHECK_FALSE(build(module, cfg, error));
    CHECK(error.kind == IrCfgErrorKind::InconsistentDepth);
    CHECK(error.instructionIndex == 4);
    CHECK(error.opcode == IrOpcode::ReturnVoid);
  }
  SUBCASE("unreachable code is not analyzed") {
    const IrModule module = makeModule(makeFunction({
        {IrOpcode::ReturnVoid, 0},
        {IrOpcode::AddI32, 0}, // would underflow, but nothing reaches it
        {IrOpcode::ReturnVoid, 0},
    }));
    CHECK(build(module, cfg, error));
    REQUIRE(cfg.blocks.size() == 2);
    CHECK_FALSE(cfg.blocks[1].reachable);
  }
}

TEST_CASE("cfg block lookup maps every instruction to its block") {
  const IrModule module = makeModule(makeFunction({
      {IrOpcode::PushI32, 1},
      {IrOpcode::JumpIfZero, 3},
      {IrOpcode::PushI32, 2},
      {IrOpcode::PushI32, 0},
      {IrOpcode::ReturnI32, 0},
  }));
  IrCfg cfg;
  IrCfgError error;
  REQUIRE(build(module, cfg, error));
  REQUIRE(cfg.blocks.size() == 3);
  CHECK(primec::irCfgBlockIndexForInstruction(cfg, 0) == 0);
  CHECK(primec::irCfgBlockIndexForInstruction(cfg, 1) == 0);
  CHECK(primec::irCfgBlockIndexForInstruction(cfg, 2) == 1);
  CHECK(primec::irCfgBlockIndexForInstruction(cfg, 3) == 2);
  CHECK(primec::irCfgBlockIndexForInstruction(cfg, 4) == 2);
  CHECK(primec::irCfgBlockIndexForInstruction(cfg, 99) == 2);
}

TEST_CASE("stack effects of calls follow the callee signature and host imports") {
  IrModule module;
  IrFunction callee = makeFunction({{IrOpcode::ReturnVoid, 0}}, 3);
  module.functions.push_back(makeFunction({{IrOpcode::ReturnVoid, 0}}));
  module.functions.push_back(std::move(callee));
  primec::IrHostImport import;
  import.name = "host.pick";
  import.parameters = {primec::IrHostValueKind::I32, primec::IrHostValueKind::String};
  import.returnKind = primec::IrHostValueKind::I64;
  module.hostImports.push_back(import);
  primec::IrHostImport voidImport;
  voidImport.name = "host.log";
  voidImport.parameters = {primec::IrHostValueKind::String};
  module.hostImports.push_back(voidImport);

  primec::IrStackEffect effect;
  REQUIRE(primec::computeIrStackEffect({IrOpcode::Call, 1}, module, effect));
  CHECK(effect.pops == 3);
  CHECK(effect.pushes == 1);
  REQUIRE(primec::computeIrStackEffect({IrOpcode::CallVoid, 1}, module, effect));
  CHECK(effect.pops == 3);
  CHECK(effect.pushes == 0);
  REQUIRE(primec::computeIrStackEffect({IrOpcode::CallHost, 0}, module, effect));
  CHECK(effect.pops == 2);
  CHECK(effect.pushes == 1);
  REQUIRE(primec::computeIrStackEffect({IrOpcode::CallHost, 1}, module, effect));
  CHECK(effect.pops == 1);
  CHECK(effect.pushes == 0);
  // Out-of-range targets are left to IR validation: no effect, not an error.
  REQUIRE(primec::computeIrStackEffect({IrOpcode::Call, 77}, module, effect));
  CHECK(effect.pops == 0);
  CHECK(effect.pushes == 1);
  // A value outside the opcode table has no effect entry.
  CHECK_FALSE(primec::computeIrStackEffect({static_cast<IrOpcode>(250), 0}, module, effect));
}
