#include "primec/ir/IrLocalEscape.h"

#include "../test_ir_pipeline_helpers.h"

#include "third_party/doctest.h"

#include <string>
#include <vector>

TEST_SUITE_BEGIN("primestruct.ir.local_escape");

namespace {

using primec::IrFunction;
using primec::IrInstruction;
using primec::IrOpcode;

IrFunction makeFunction(std::vector<IrInstruction> instructions) {
  IrFunction function;
  function.name = "/f";
  function.instructions = std::move(instructions);
  return function;
}

std::vector<uint32_t> slots(std::initializer_list<uint32_t> values) {
  return std::vector<uint32_t>(values);
}

} // namespace

TEST_CASE("a function with only scalar local traffic pins nothing") {
  const auto info = primec::analyzeIrLocalEscape(makeFunction({
      {IrOpcode::PushI32, 1},
      {IrOpcode::StoreLocal, 0},
      {IrOpcode::LoadLocal, 0},
      {IrOpcode::StoreLocal, 3},
      {IrOpcode::LoadLocal, 3},
      {IrOpcode::ReturnI32, 0},
  }));
  CHECK(info.localCount == 4);
  CHECK(info.pinnedSlots.empty());
  CHECK(info.canPromoteAll());
  CHECK_FALSE(info.addressTaken);
  CHECK_FALSE(info.isPinned(0));
}

TEST_CASE("taking any local's address pins every slot of the frame") {
  const auto info = primec::analyzeIrLocalEscape(makeFunction({
      {IrOpcode::PushI32, 1},
      {IrOpcode::StoreLocal, 0},
      {IrOpcode::AddressOfLocal, 2},
      {IrOpcode::LoadIndirect, 0},
      {IrOpcode::StoreLocal, 4},
      {IrOpcode::ReturnVoid, 0},
  }));
  CHECK(info.localCount == 5);
  // Negative pointer offsets are legal, so slots below the addressed one are reachable too.
  CHECK(info.pinnedSlots == slots({0, 1, 2, 3, 4}));
  CHECK(info.addressTaken);
  CHECK(info.lowestAddressedSlot == 2);
  CHECK(info.isPinned(0));
  CHECK_FALSE(info.isPinned(5));
  CHECK_FALSE(info.canPromoteAll());
}

TEST_CASE("the lowest addressed slot is recorded across several address-of instructions") {
  const auto info = primec::analyzeIrLocalEscape(makeFunction({
      {IrOpcode::AddressOfLocal, 3},
      {IrOpcode::Pop, 0},
      {IrOpcode::AddressOfLocal, 1},
      {IrOpcode::Pop, 0},
      {IrOpcode::AddressOfLocal, 6},
      {IrOpcode::Pop, 0},
      {IrOpcode::ReturnVoid, 0},
  }));
  CHECK(info.lowestAddressedSlot == 1);
  CHECK(info.localCount == 7);
}

TEST_CASE("FileReadByte pins the local it writes even when no address is taken") {
  const auto info = primec::analyzeIrLocalEscape(makeFunction({
      {IrOpcode::PushI32, 0},
      {IrOpcode::StoreLocal, 0},
      {IrOpcode::LoadLocal, 0},
      {IrOpcode::FileReadByte, 2},
      {IrOpcode::StoreLocal, 1},
      {IrOpcode::LoadLocal, 2},
      {IrOpcode::FileReadByte, 2},
      {IrOpcode::ReturnVoid, 0},
  }));
  CHECK(info.localCount == 3);
  CHECK(info.pinnedSlots == slots({2}));
  CHECK_FALSE(info.addressTaken);
}

TEST_CASE("an empty function has no locals and nothing pinned") {
  const auto info = primec::analyzeIrLocalEscape(makeFunction({}));
  CHECK(info.localCount == 0);
  CHECK(info.pinnedSlots.empty());
  CHECK(info.canPromoteAll());
}

TEST_CASE("lowered scalar programs promote every local and address-taking programs pin") {
  struct Program {
    const char *name;
    const char *source;
    bool expectAddressTaken;
  };
  const std::vector<Program> programs = {
      {"scalar loop",
       R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  [i32 mut] index{0i32}
  repeat(5i32) {
    assign(total, plus(total, index))
    assign(index, plus(index, 1i32))
  }
  return(total)
}
)",
       false},
      {"struct field read",
       R"(
[struct]
Pair() {
  [i32] left{0i32}
  [i32] right{0i32}
}

[return<int>]
main() {
  [Pair] value{Pair{3i32, 9i32}}
  return(value.right)
}
)",
       true},
      {"explicit pointer",
       R"(
[return<int>]
main() {
  [i32] value{5i32}
  return(dereference(location(value)))
}
)",
       true},
  };
  for (const Program &program : programs) {
    CAPTURE(program.name);
    PreparedCompilePipelineIrForTesting prepared;
    std::string error;
    REQUIRE_MESSAGE(prepareIrThroughCompilePipeline(program.source, "/main", "vm", prepared, error),
                    error);
    bool sawAddressTaken = false;
    for (const IrFunction &function : prepared.ir.functions) {
      const auto info = primec::analyzeIrLocalEscape(function);
      sawAddressTaken = sawAddressTaken || info.addressTaken;
      // Pinned slots are always in range and sorted.
      uint32_t previous = 0;
      bool first = true;
      for (const uint32_t slot : info.pinnedSlots) {
        CHECK(slot < info.localCount);
        if (!first) {
          CHECK(slot > previous);
        }
        previous = slot;
        first = false;
      }
      if (!info.addressTaken) {
        CHECK(info.pinnedSlots.size() <= 1);
      }
    }
    CHECK(sawAddressTaken == program.expectAddressTaken);
  }
}
