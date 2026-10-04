#include "primec/ir/IrVirtualRegisterLiveness.h"
#include "primec/ir/IrVirtualRegisterLowering.h"
#include "primec/ir/IrVirtualRegisterVerifier.h"

#include "test_ir_optimizer_helpers.h"
#include "test_ir_random_programs.h"
#include "test_ir_runtime_programs.h"
#include "test_ir_vm_run.h"

#include <string>
#include <vector>

TEST_SUITE_BEGIN("primestruct.ir.register_locals");

// The block virtual-register form can also give every local that no memory access
// reaches its own registers, with values carried across block edges by moves like the stack's.
// The lift back to stack IR keeps the original instructions, so a round trip must leave the module
// and every VM result untouched.

namespace {

using optimizer_test::assemble;
using optimizer_test::moduleOf;

primec::IrVirtualRegisterModule lowerPromoted(const primec::IrModule &module) {
  primec::IrVirtualRegisterModule lowered;
  std::string error;
  primec::IrVirtualRegisterLoweringOptions options;
  options.promoteLocals = true;
  REQUIRE_MESSAGE(primec::lowerIrModuleToBlockVirtualRegisters(module, lowered, error, options), error);
  return lowered;
}

void expectRoundTrip(const primec::IrModule &module, const std::string &name) {
  INFO(name);
  const primec::IrVirtualRegisterModule lowered = lowerPromoted(module);
  std::string error;
  CHECK_MESSAGE(primec::verifyIrVirtualRegisterLocalForm(lowered, error), error);

  primec::IrVirtualRegisterModuleLiveness liveness;
  CHECK_MESSAGE(primec::buildIrVirtualRegisterLiveness(lowered, liveness, error), error);

  primec::IrModule lifted;
  REQUIRE_MESSAGE(primec::liftBlockVirtualRegistersToIrModule(lowered, lifted, error), error);
  REQUIRE(lifted.functions.size() == module.functions.size());
  for (size_t f = 0; f < module.functions.size(); ++f) {
    REQUIRE(lifted.functions[f].instructions.size() == module.functions[f].instructions.size());
    for (size_t i = 0; i < module.functions[f].instructions.size(); ++i) {
      CHECK(lifted.functions[f].instructions[i].op == module.functions[f].instructions[i].op);
      CHECK(lifted.functions[f].instructions[i].imm == module.functions[f].instructions[i].imm);
    }
  }
  lifted.stringTable = module.stringTable;
  lifted.entryIndex = module.entryIndex;
  CHECK(optimizer_test::run(lifted) == optimizer_test::run(module));
}

// A counted loop: local 0 is the counter, local 1 the running total.
primec::IrModule loopModule() {
  return moduleOf(assemble({"PushI32 3",
                            "StoreLocal 0",
                            "PushI32 0",
                            "StoreLocal 1",
                            "LoadLocal 0",
                            "JumpIfZero 15",
                            "LoadLocal 1",
                            "LoadLocal 0",
                            "AddI32",
                            "StoreLocal 1",
                            "LoadLocal 0",
                            "PushI32 1",
                            "SubI32",
                            "StoreLocal 0",
                            "Jump 4",
                            "LoadLocal 1",
                            "ReturnI32"}));
}

} // namespace

TEST_CASE("promoted-locals form round-trips random programs with identical results") {
  for (uint64_t seed = 0; seed < 200; ++seed) {
    expectRoundTrip(optimizer_test::makeModule(seed + 91000), "seed " + std::to_string(seed));
  }
}

TEST_CASE("promoted-locals form round-trips calls, heap, io and every fused form") {
  expectRoundTrip(optimizer_test::callsProgram(), "calls");
  expectRoundTrip(optimizer_test::heapProgram(), "heap");
  expectRoundTrip(optimizer_test::fusedFormsProgram(), "fused forms");
}

TEST_CASE("a loop carries its promoted locals across the back edge with moves") {
  const primec::IrVirtualRegisterModule lowered = lowerPromoted(loopModule());
  const primec::IrVirtualRegisterFunction &function = lowered.functions[0];
  CHECK(function.promotedLocals == std::vector<uint32_t>{0, 1});

  // The loop head (instruction 4) is entered from the preamble and from the back edge: both
  // locals are live into it, so it has two entry locals and each incoming edge two local moves.
  size_t head = function.blocks.size();
  for (size_t b = 0; b < function.blocks.size(); ++b) {
    if (function.blocks[b].startInstructionIndex == 4) {
      head = b;
    }
  }
  REQUIRE(head < function.blocks.size());
  CHECK(function.blocks[head].entryLocals.size() == 2);
  size_t incoming = 0;
  for (const primec::IrVirtualRegisterBlock &block : function.blocks) {
    for (const primec::IrVirtualRegisterEdge &edge : block.successorEdges) {
      if (edge.successorBlockIndex == head) {
        ++incoming;
        CHECK(edge.localMoves.size() == 2);
      }
    }
  }
  CHECK(incoming == 2);

  // Every store defines a fresh register and every load reads the current one.
  for (const primec::IrVirtualRegisterBlock &block : function.blocks) {
    for (const primec::IrVirtualRegisterInstruction &instruction : block.instructions) {
      CHECK(instruction.localUseRegister.has_value() ==
            (instruction.instruction.op == primec::IrOpcode::LoadLocal));
      CHECK(instruction.localDefRegister.has_value() ==
            (instruction.instruction.op == primec::IrOpcode::StoreLocal));
    }
  }

  primec::IrVirtualRegisterModuleLiveness liveness;
  std::string error;
  REQUIRE_MESSAGE(primec::buildIrVirtualRegisterLiveness(lowered, liveness, error), error);
  CHECK(liveness.functions[0].blocks[head].liveInRegisters.size() >= 2);
}

TEST_CASE("a promoted local read before any definition is rejected") {
  const primec::IrVirtualRegisterModule direct =
      lowerPromoted(moduleOf(assemble({"LoadLocal 0", "ReturnI32"})));
  std::string error;
  CHECK_FALSE(primec::verifyIrVirtualRegisterLocalForm(direct, error));
  CHECK(error.find("is read before any definition on some path") != std::string::npos);

  // Defined on one branch only: still read before a definition on the other path.
  const primec::IrVirtualRegisterModule oneBranch = lowerPromoted(moduleOf(assemble({"PushI32 1",
                                                                                     "JumpIfZero 5",
                                                                                     "PushI32 7",
                                                                                     "StoreLocal 0",
                                                                                     "Jump 5",
                                                                                     "LoadLocal 0",
                                                                                     "ReturnI32"})));
  CHECK_FALSE(primec::verifyIrVirtualRegisterLocalForm(oneBranch, error));
  CHECK(error.find("is read before any definition on some path") != std::string::npos);
}

TEST_CASE("locals whose address is taken stay in memory") {
  const primec::IrModule module = moduleOf(assemble({"PushI32 5",
                                                     "StoreLocal 0",
                                                     "AddressOfLocal 0",
                                                     "LoadIndirect",
                                                     "ReturnI32"}));
  const primec::IrVirtualRegisterModule lowered = lowerPromoted(module);
  CHECK(lowered.functions[0].promotedLocals.empty());
  for (const primec::IrVirtualRegisterBlock &block : lowered.functions[0].blocks) {
    CHECK(block.entryLocals.empty());
    for (const primec::IrVirtualRegisterInstruction &instruction : block.instructions) {
      CHECK_FALSE(instruction.localUseRegister.has_value());
      CHECK_FALSE(instruction.localDefRegister.has_value());
    }
  }
  std::string error;
  CHECK_MESSAGE(primec::verifyIrVirtualRegisterLocalForm(lowered, error), error);
}

TEST_CASE("the local-form verifier rejects corrupted registers and edge moves") {
  const primec::IrVirtualRegisterModule good = lowerPromoted(loopModule());
  std::string error;
  REQUIRE_MESSAGE(primec::verifyIrVirtualRegisterLocalForm(good, error), error);

  {
    primec::IrVirtualRegisterModule bad = good;
    bool changed = false;
    for (primec::IrVirtualRegisterBlock &block : bad.functions[0].blocks) {
      for (primec::IrVirtualRegisterEdge &edge : block.successorEdges) {
        if (!edge.localMoves.empty() && !changed) {
          edge.localMoves.front().destinationRegister += 1;
          changed = true;
        }
      }
    }
    REQUIRE(changed);
    CHECK_FALSE(primec::verifyIrVirtualRegisterLocalForm(bad, error));
  }
  {
    primec::IrVirtualRegisterModule bad = good;
    bool changed = false;
    for (primec::IrVirtualRegisterBlock &block : bad.functions[0].blocks) {
      for (primec::IrVirtualRegisterInstruction &instruction : block.instructions) {
        if (instruction.localUseRegister.has_value() && !changed) {
          instruction.localUseRegister = *instruction.localUseRegister + 1;
          changed = true;
        }
      }
    }
    REQUIRE(changed);
    CHECK_FALSE(primec::verifyIrVirtualRegisterLocalForm(bad, error));
  }
  {
    primec::IrVirtualRegisterModule bad = good;
    for (primec::IrVirtualRegisterBlock &block : bad.functions[0].blocks) {
      for (primec::IrVirtualRegisterInstruction &instruction : block.instructions) {
        if (instruction.localDefRegister.has_value()) {
          instruction.localDefRegister.reset();
          break;
        }
      }
    }
    CHECK_FALSE(primec::verifyIrVirtualRegisterLocalForm(bad, error));
  }
}

TEST_CASE("the default lowering is unchanged by the promoted-locals option") {
  const primec::IrModule module = loopModule();
  primec::IrVirtualRegisterModule plain;
  std::string error;
  REQUIRE_MESSAGE(primec::lowerIrModuleToBlockVirtualRegisters(module, plain, error), error);
  for (const primec::IrVirtualRegisterBlock &block : plain.functions[0].blocks) {
    CHECK(block.entryLocals.empty());
    CHECK(block.exitLocals.empty());
    for (const primec::IrVirtualRegisterEdge &edge : block.successorEdges) {
      CHECK(edge.localMoves.empty());
    }
    for (const primec::IrVirtualRegisterInstruction &instruction : block.instructions) {
      CHECK_FALSE(instruction.localUseRegister.has_value());
    }
  }
  CHECK(plain.functions[0].promotedLocals.empty());
}

TEST_SUITE_END();
