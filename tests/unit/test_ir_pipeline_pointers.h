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
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 9);
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
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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

TEST_CASE("ir lowers pointer minus u64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  return(dereference(minus(location(second), 16u64)))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 9);
}

TEST_CASE("pointer minus uses byte offsets in VM") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  return(dereference(minus(location(second), 16i32)))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 4);
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
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 4);
}

TEST_CASE("pointer plus accepts i64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  return(dereference(plus(location(first), 16i64)))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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

TEST_CASE("pointer plus accepts u64 offsets via PushI64") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  return(dereference(plus(location(first), 16u64)))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
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
  bool ok = vm.execute(module, result, error);
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(result == 8);
}

TEST_CASE("ir lowers location on reference without extra address-of") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32> mut] ref{location(value)}
  [Pointer<i32> mut] ptr{location(ref)}
  assign(dereference(ptr), 9i32)
  return(value)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  int addressOfCount = 0;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::AddressOfLocal) {
      addressOfCount += 1;
    }
  }
  CHECK(addressOfCount == 1);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 9);
}

TEST_CASE("ir returns reference handles without implicit dereference") {
  const std::string source = R"(
[return<Reference<i32>>]
borrow_ref([Reference<i32>] value) {
  return(value)
}

[return<int>]
main() {
  [i32 mut] value{7i32}
  return(dereference(borrow_ref(location(value))))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  bool sawLoadIndirect = false;
  bool sawReturnI64 = false;
  for (const auto &function : module.functions) {
    if (function.name != "/borrow_ref") {
      continue;
    }
    for (const auto &inst : function.instructions) {
      if (inst.op == primec::IrOpcode::LoadIndirect) {
        sawLoadIndirect = true;
      }
      if (inst.op == primec::IrOpcode::ReturnI64) {
        sawReturnI64 = true;
      }
    }
  }
  CHECK_FALSE(sawLoadIndirect);
  CHECK(sawReturnI64);
}

TEST_CASE("ir materializes direct location arguments for scalar reference helpers") {
  const std::string source = R"(
[return<Reference<i32>>]
borrow_ref([Reference<i32>] value) {
  return(value)
}

[return<int>]
main() {
  [i32 mut] value{11i32}
  return(dereference(borrow_ref(location(value))))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  bool sawLoadIndirect = false;
  bool sawBorrowRefReturn = false;
  for (const auto &function : module.functions) {
    for (const auto &inst : function.instructions) {
      if (inst.op == primec::IrOpcode::LoadIndirect) {
        sawLoadIndirect = true;
      }
    }
    if (function.name == "/borrow_ref") {
      for (const auto &inst : function.instructions) {
        if (inst.op == primec::IrOpcode::ReturnI64) {
          sawBorrowRefReturn = true;
        }
      }
    }
  }
  CHECK(sawLoadIndirect);
  CHECK(sawBorrowRefReturn);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 11);
}

#include "test_ir_pipeline_pointers_numeric.h"

TEST_SUITE_END();
