#include <string>
#include <vector>

#include "third_party/doctest.h"

#include "primec/runtime/Vm.h"
#include "test_ir_pipeline_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.pointers");

TEST_CASE("ir lowers location of reference binding") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{2i32}
  [Reference<i32> mut] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  return(dereference(ptr))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());

  int addressCount = 0;
  bool sawRefLoad = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::AddressOfLocal) {
      addressCount += 1;
    }
    if (inst.op == primec::IrOpcode::LoadLocal && inst.imm == 1) {
      sawRefLoad = true;
    }
  }
  CHECK(addressCount == 1);
  CHECK(sawRefLoad);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 2);
}

TEST_CASE("ir lowers pointer plus") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{5i32}
  return(dereference(plus(location(value), 0i32)))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());

  bool sawAdd = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::AddI64) {
      sawAdd = true;
      break;
    }
  }
  CHECK(sawAdd);

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 5);
}

TEST_CASE("ir lowers pointer plus on reference location") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  [Reference<i32>] ref{location(first)}
  return(dereference(plus(location(ref), 16i32)))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());

  bool sawAdd = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::AddI64) {
      sawAdd = true;
      break;
    }
  }
  CHECK(sawAdd);

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 9);
}

TEST_CASE("assign returns value for reference targets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32> mut] ref{location(value)}
  return(assign(ref, 7i32))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 7);
}

TEST_CASE("ir lowers reference to array bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 8i32, 12i32)}
  [Reference<array<i32>>] ref{location(values)}
  return(at(ref, 1i32))
}
)";
  std::string error;
  primec::IrModule module;
  const bool ok = parseValidateAndLower(source, module, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 8);
}

TEST_CASE("ir lowers location on parameters") {
  const std::string source = R"(
[return<int>]
read([i32] value) {
  [Pointer<i32>] ptr{location(value)}
  return(dereference(ptr))
}

[return<int>]
main() {
  return(read(11i32))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 11);
}

TEST_CASE("assign returns value for dereference targets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [Pointer<i32> mut] ptr{location(value)}
  return(assign(dereference(ptr), 9i32))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 9);
}

TEST_CASE("ir lowers heap realloc pointer sum through dereference offset") {
  const std::string source = R"(
[unsafe return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(ptr), 9i32)
  [Pointer<i32> mut] grown{/std/intrinsics/memory/realloc(ptr, 2i32)}
  assign(dereference(plus(grown, 16i32)), 4i32)
  [i32] sum{plus(dereference(grown), dereference(plus(grown, 16i32)))}
  /std/intrinsics/memory/free(grown)
  return(sum)
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());

  int addCount = 0;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::AddI64) {
      addCount += 1;
    }
  }
  CHECK(addCount >= 2);

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 13);
}

TEST_CASE("ir lowers pointer minus") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  return(dereference(minus(location(second), 16i32)))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());

  bool sawSub = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::SubI64) {
      sawSub = true;
      break;
    }
  }
  CHECK(sawSub);

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 4);
}

TEST_CASE("ir lowers pointer minus negative i64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  return(dereference(minus(location(first), -16i64)))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());

  bool sawSub = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::SubI64) {
      sawSub = true;
      break;
    }
  }
  CHECK(sawSub);

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 9);
}

TEST_CASE("pointer plus uses byte offsets in VM") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  return(dereference(plus(location(first), 16i32)))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 9);
}

TEST_CASE("pointer plus accepts negative i64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  return(dereference(plus(location(second), -16i64)))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 4);
}

TEST_CASE("ir pipeline pointer plus accepts i64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  return(dereference(plus(location(first), 16i64)))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());

  bool sawPushI64 = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PushI64) {
      sawPushI64 = true;
      break;
    }
  }
  CHECK(sawPushI64);

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 9);
}

TEST_CASE("ir lowers reference assignments") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{3i32}
  [Reference<i32> mut] ref{location(value)}
  assign(ref, 8i32)
  return(ref)
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());
  CHECK(error.empty());

  bool sawStoreIndirect = false;
  bool sawLoadIndirect = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::StoreIndirect) {
      sawStoreIndirect = true;
    }
    if (inst.op == primec::IrOpcode::LoadIndirect) {
      sawLoadIndirect = true;
    }
  }
  CHECK(sawStoreIndirect);
  CHECK(sawLoadIndirect);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 8);
}

TEST_CASE("assign to a plain int struct local never rebinds its slots") {
  // A struct of one scalar kind lives as an array local, a pointer to its slots. Assigning to it
  // copies into those slots; it must not store the source's address into the local, which made
  // the target an alias of the source.
  const std::string source = R"(
[struct]
Pair() {
  [i32 mut] left{0i32}
  [i32 mut] right{0i32}
}

[return<int>]
main() {
  [Pair mut] source{Pair{[left] 1i32, [right] 2i32}}
  [Pair mut] target{Pair{}}
  assign(target, source)
  assign(source.left, 7i32)
  return(plus(target.left, multiply(source.left, 10i32)))
}
)";
  std::string error;
  primec::IrModule module;
  INFO(error);
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());

  // The locals that hold a struct's address are stored once, right after its AddressOfLocal.
  const auto &instructions = module.functions[0].instructions;
  std::vector<uint64_t> slotPointerLocals;
  for (size_t i = 0; i + 1 < instructions.size(); ++i) {
    if (instructions[i].op == primec::IrOpcode::AddressOfLocal &&
        instructions[i + 1].op == primec::IrOpcode::StoreLocal) {
      slotPointerLocals.push_back(instructions[i + 1].imm);
    }
  }
  REQUIRE(slotPointerLocals.size() >= 2);
  for (const uint64_t pointerLocal : slotPointerLocals) {
    int stores = 0;
    for (const auto &inst : instructions) {
      if (inst.op == primec::IrOpcode::StoreLocal && inst.imm == pointerLocal) {
        stores += 1;
      }
    }
    CHECK_MESSAGE(stores == 1, "local " << pointerLocal << " holding struct slots is rebound");
  }

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 71);
}

#include "test_ir_pipeline_pointers_numeric.h"

TEST_SUITE_END();
