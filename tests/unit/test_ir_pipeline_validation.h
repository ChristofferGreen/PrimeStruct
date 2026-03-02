TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir validator accepts lowered canonical module") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
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

  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Vm, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Native, error));
  CHECK(error.empty());
}

TEST_CASE("ir opcode allowlist matches vm/native support matrix") {
  const std::vector<primec::IrOpcode> expected = {
      primec::IrOpcode::PushI32,
      primec::IrOpcode::PushI64,
      primec::IrOpcode::LoadLocal,
      primec::IrOpcode::StoreLocal,
      primec::IrOpcode::AddressOfLocal,
      primec::IrOpcode::LoadIndirect,
      primec::IrOpcode::StoreIndirect,
      primec::IrOpcode::Dup,
      primec::IrOpcode::Pop,
      primec::IrOpcode::AddI32,
      primec::IrOpcode::SubI32,
      primec::IrOpcode::MulI32,
      primec::IrOpcode::DivI32,
      primec::IrOpcode::NegI32,
      primec::IrOpcode::AddI64,
      primec::IrOpcode::SubI64,
      primec::IrOpcode::MulI64,
      primec::IrOpcode::DivI64,
      primec::IrOpcode::DivU64,
      primec::IrOpcode::NegI64,
      primec::IrOpcode::CmpEqI32,
      primec::IrOpcode::CmpNeI32,
      primec::IrOpcode::CmpLtI32,
      primec::IrOpcode::CmpLeI32,
      primec::IrOpcode::CmpGtI32,
      primec::IrOpcode::CmpGeI32,
      primec::IrOpcode::CmpEqI64,
      primec::IrOpcode::CmpNeI64,
      primec::IrOpcode::CmpLtI64,
      primec::IrOpcode::CmpLeI64,
      primec::IrOpcode::CmpGtI64,
      primec::IrOpcode::CmpGeI64,
      primec::IrOpcode::CmpLtU64,
      primec::IrOpcode::CmpLeU64,
      primec::IrOpcode::CmpGtU64,
      primec::IrOpcode::CmpGeU64,
      primec::IrOpcode::JumpIfZero,
      primec::IrOpcode::Jump,
      primec::IrOpcode::ReturnVoid,
      primec::IrOpcode::ReturnI32,
      primec::IrOpcode::ReturnI64,
      primec::IrOpcode::PrintI32,
      primec::IrOpcode::PrintI64,
      primec::IrOpcode::PrintU64,
      primec::IrOpcode::PrintString,
      primec::IrOpcode::PushArgc,
      primec::IrOpcode::PrintArgv,
      primec::IrOpcode::PrintArgvUnsafe,
      primec::IrOpcode::LoadStringByte,
      primec::IrOpcode::FileOpenRead,
      primec::IrOpcode::FileOpenWrite,
      primec::IrOpcode::FileOpenAppend,
      primec::IrOpcode::FileClose,
      primec::IrOpcode::FileFlush,
      primec::IrOpcode::FileWriteI32,
      primec::IrOpcode::FileWriteI64,
      primec::IrOpcode::FileWriteU64,
      primec::IrOpcode::FileWriteString,
      primec::IrOpcode::FileWriteByte,
      primec::IrOpcode::FileWriteNewline,
      primec::IrOpcode::PushF32,
      primec::IrOpcode::PushF64,
      primec::IrOpcode::AddF32,
      primec::IrOpcode::SubF32,
      primec::IrOpcode::MulF32,
      primec::IrOpcode::DivF32,
      primec::IrOpcode::NegF32,
      primec::IrOpcode::AddF64,
      primec::IrOpcode::SubF64,
      primec::IrOpcode::MulF64,
      primec::IrOpcode::DivF64,
      primec::IrOpcode::NegF64,
      primec::IrOpcode::CmpEqF32,
      primec::IrOpcode::CmpNeF32,
      primec::IrOpcode::CmpLtF32,
      primec::IrOpcode::CmpLeF32,
      primec::IrOpcode::CmpGtF32,
      primec::IrOpcode::CmpGeF32,
      primec::IrOpcode::CmpEqF64,
      primec::IrOpcode::CmpNeF64,
      primec::IrOpcode::CmpLtF64,
      primec::IrOpcode::CmpLeF64,
      primec::IrOpcode::CmpGtF64,
      primec::IrOpcode::CmpGeF64,
      primec::IrOpcode::ConvertI32ToF32,
      primec::IrOpcode::ConvertI32ToF64,
      primec::IrOpcode::ConvertI64ToF32,
      primec::IrOpcode::ConvertI64ToF64,
      primec::IrOpcode::ConvertU64ToF32,
      primec::IrOpcode::ConvertU64ToF64,
      primec::IrOpcode::ConvertF32ToI32,
      primec::IrOpcode::ConvertF32ToI64,
      primec::IrOpcode::ConvertF32ToU64,
      primec::IrOpcode::ConvertF64ToI32,
      primec::IrOpcode::ConvertF64ToI64,
      primec::IrOpcode::ConvertF64ToU64,
      primec::IrOpcode::ConvertF32ToF64,
      primec::IrOpcode::ConvertF64ToF32,
      primec::IrOpcode::ReturnF32,
      primec::IrOpcode::ReturnF64,
      primec::IrOpcode::PrintStringDynamic,
      primec::IrOpcode::Call,
      primec::IrOpcode::CallVoid,
  };

  CHECK(expected.size() == static_cast<size_t>(static_cast<uint8_t>(primec::IrOpcode::CallVoid)));
  for (size_t i = 0; i < expected.size(); ++i) {
    const auto expectedValue = static_cast<uint8_t>(i + 1);
    CHECK(static_cast<uint8_t>(expected[i]) == expectedValue);
  }
}

TEST_CASE("ir call semantics matrix accepts recursive call opcodes with tail metadata") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.metadata.instrumentationFlags = primec::InstrumentationTailExecution;
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction factFn;
  factFn.name = "/fact";
  factFn.instructions.push_back({primec::IrOpcode::Dup, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::CmpEqI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 7});
  factFn.instructions.push_back({primec::IrOpcode::Pop, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  factFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::Dup, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  factFn.instructions.push_back({primec::IrOpcode::SubI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::Call, 1});
  factFn.instructions.push_back({primec::IrOpcode::MulI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(factFn));

  std::string error;
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Vm, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Native, error));
  CHECK(error.empty());
}

TEST_CASE("ir call semantics matrix rejects non-direct call targets for vm and native") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::Call, 9});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Vm, error));
  CHECK(error.find("invalid call target") != std::string::npos);

  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Native, error));
  CHECK(error.find("invalid call target") != std::string::npos);
}

TEST_CASE("ir validator rejects invalid jump targets") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::Jump, 3});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("invalid jump target") != std::string::npos);
}

TEST_CASE("ir validator rejects invalid call targets") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::Call, 1});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("invalid call target") != std::string::npos);
}

TEST_CASE("ir validator rejects invalid print flags") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  fn.instructions.push_back({primec::IrOpcode::PrintI32, 4});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("invalid print flags") != std::string::npos);
}

TEST_CASE("ir validator rejects invalid string indices") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::FileOpenRead, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("invalid string index") != std::string::npos);
}

TEST_CASE("ir validator rejects local indices beyond 32-bit") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::LoadLocal,
                             static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1u});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("local index exceeds 32-bit limit") != std::string::npos);
}

TEST_CASE("ir validator rejects unknown opcodes") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({static_cast<primec::IrOpcode>(0), 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("unsupported opcode") != std::string::npos);
}

TEST_CASE("ir validator rejects unknown metadata bits") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.metadata.effectMask = (1ull << 63);
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("unsupported effect mask bits") != std::string::npos);
}

TEST_SUITE_END();
