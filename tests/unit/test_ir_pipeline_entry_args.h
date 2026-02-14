TEST_SUITE_BEGIN("primestruct.ir.pipeline.entry_args");

TEST_CASE("ir lowerer supports entry args print u64 index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1u64])
  return(0i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);
  bool sawPrintArgv = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PrintArgv) {
      sawPrintArgv = true;
      break;
    }
  }
  CHECK(sawPrintArgv);
}

TEST_CASE("ir lowerer supports entry args print unsafe index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(at_unsafe(args, 1i32))
  return(0i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);
  bool sawPrintArgvUnsafe = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PrintArgvUnsafe) {
      sawPrintArgvUnsafe = true;
      break;
    }
  }
  CHECK(sawPrintArgvUnsafe);
}

TEST_CASE("ir lowerer supports entry args print unsafe u64 index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(at_unsafe(args, 1u64))
  return(0i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);
  bool sawPrintArgvUnsafe = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PrintArgvUnsafe) {
      sawPrintArgvUnsafe = true;
      break;
    }
  }
  CHECK(sawPrintArgvUnsafe);
}

TEST_CASE("ir lowerer tracks unsafe argv bindings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  [string] first{at_unsafe(args, 1i32)}
  print_line(first)
  return(0i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);
  bool sawPrintArgvUnsafe = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PrintArgvUnsafe) {
      sawPrintArgvUnsafe = true;
      break;
    }
  }
  CHECK(sawPrintArgvUnsafe);
}

TEST_CASE("ir serialize roundtrip preserves unsafe argv prints") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(at_unsafe(args, 1i32))
  return(0i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);
  bool sawUnsafe = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PrintArgvUnsafe) {
      sawUnsafe = true;
      break;
    }
  }
  CHECK(sawUnsafe);

  std::vector<uint8_t> data;
  REQUIRE(primec::serializeIr(module, data, error));
  CHECK(error.empty());

  primec::IrModule decoded;
  REQUIRE(primec::deserializeIr(data, decoded, error));
  CHECK(error.empty());
  REQUIRE(decoded.functions.size() == 1);
  bool sawUnsafeDecoded = false;
  for (const auto &inst : decoded.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PrintArgvUnsafe) {
      sawUnsafeDecoded = true;
      break;
    }
  }
  CHECK(sawUnsafeDecoded);
}

TEST_CASE("ir lowers map literal call as statement") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{0i32}
  map<i32, i32>(1i32, assign(value, 7i32))
  return(value)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 7);
}

TEST_SUITE_END();
