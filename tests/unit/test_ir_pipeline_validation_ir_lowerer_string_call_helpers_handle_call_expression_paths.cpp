#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer string call helpers handle call-expression paths") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto getInstructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, int32_t imm) { instructions.at(index).imm = imm; };

  primec::Expr argvExpr;
  argvExpr.kind = primec::Expr::Kind::Name;
  argvExpr.name = "argv";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 2;
  primec::Expr atCall;
  atCall.kind = primec::Expr::Kind::Call;
  atCall.name = "at";
  atCall.args = {argvExpr, indexExpr};

  primec::ir_lowerer::StringCallSource source = primec::ir_lowerer::StringCallSource::None;
  bool argvChecked = false;
  std::string error;
  int emittedIndexExprs = 0;

  CHECK(primec::ir_lowerer::emitCallStringCallValue(
            atCall,
            [](const primec::Expr &callExpr, std::string &nameOut) {
              if (callExpr.name == "at" || callExpr.name == "unchecked_at") {
                nameOut = callExpr.name;
                return true;
              }
              return false;
            },
            [](const primec::Expr &targetExpr) {
              return targetExpr.kind == primec::Expr::Kind::Name && targetExpr.name == "argv";
            },
            [](const primec::Expr &, const std::string &, primec::ir_lowerer::StringIndexOps &ops, std::string &) {
              ops.pushZero = primec::IrOpcode::PushI32;
              ops.cmpLt = primec::IrOpcode::CmpLtI32;
              ops.cmpGe = primec::IrOpcode::CmpGeI32;
              ops.skipNegativeCheck = false;
              return true;
            },
            [&](const primec::Expr &) {
              emittedIndexExprs++;
              emitInstruction(primec::IrOpcode::PushI32, 2);
              return true;
            },
            [](const primec::Expr &) { return false; },
            []() { return 11; },
            emitInstruction,
            getInstructionCount,
            patchInstructionImm,
            [&]() { emitInstruction(primec::IrOpcode::PushI32, 999); },
            source,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Handled);
  CHECK(error.empty());
  CHECK(source == primec::ir_lowerer::StringCallSource::ArgvIndex);
  CHECK(argvChecked);
  CHECK(emittedIndexExprs == 1);
  REQUIRE_FALSE(instructions.empty());
  CHECK(instructions.back().op == primec::IrOpcode::LoadLocal);
  CHECK(instructions.back().imm == 11);

  instructions.clear();
  source = primec::ir_lowerer::StringCallSource::None;
  argvChecked = true;
  bool runtimeExprEmitted = false;
  primec::Expr runtimeCall;
  runtimeCall.kind = primec::Expr::Kind::Call;
  runtimeCall.name = "build_string";
  CHECK(primec::ir_lowerer::emitCallStringCallValue(
            runtimeCall,
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &, const std::string &, primec::ir_lowerer::StringIndexOps &, std::string &) {
              return false;
            },
            [&](const primec::Expr &) {
              runtimeExprEmitted = true;
              emitInstruction(primec::IrOpcode::LoadLocal, 4);
              return true;
            },
            [](const primec::Expr &) { return true; },
            []() { return 0; },
            emitInstruction,
            getInstructionCount,
            patchInstructionImm,
            []() {},
            source,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Handled);
  CHECK(source == primec::ir_lowerer::StringCallSource::RuntimeIndex);
  CHECK(runtimeExprEmitted);
}

TEST_CASE("ir lowerer string call helpers report call-expression diagnostics") {
  primec::Expr badCall;
  badCall.kind = primec::Expr::Kind::Call;
  badCall.name = "bad";

  primec::ir_lowerer::StringCallSource source = primec::ir_lowerer::StringCallSource::None;
  bool argvChecked = true;
  std::string error;
  CHECK(primec::ir_lowerer::emitCallStringCallValue(
            badCall,
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &, const std::string &, primec::ir_lowerer::StringIndexOps &, std::string &) {
              return false;
            },
            [](const primec::Expr &) { return true; },
            [](const primec::Expr &) { return false; },
            []() { return 0; },
            [](primec::IrOpcode, uint64_t) {},
            []() { return size_t{0}; },
            [](size_t, int32_t) {},
            []() {},
            source,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Error);
  CHECK(error == "native backend requires string arguments to use string literals, bindings, or entry args");
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
      primec::IrOpcode::LoadStringLength,
      primec::IrOpcode::FileOpenRead,
      primec::IrOpcode::FileOpenWrite,
      primec::IrOpcode::FileOpenAppend,
      primec::IrOpcode::FileOpenReadDynamic,
      primec::IrOpcode::FileOpenWriteDynamic,
      primec::IrOpcode::FileOpenAppendDynamic,
      primec::IrOpcode::FileReadByte,
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
      primec::IrOpcode::HeapAlloc,
      primec::IrOpcode::HeapFree,
      primec::IrOpcode::HeapRealloc,
      primec::IrOpcode::FileWriteStringDynamic,
  };

  CHECK(expected.size() == static_cast<size_t>(static_cast<uint8_t>(primec::IrOpcode::FileWriteStringDynamic)));
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

TEST_CASE("ir validator glsl target accepts basic integer control-flow subset") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  fn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  fn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  fn.instructions.push_back({primec::IrOpcode::CmpEqI32, 0});
  fn.instructions.push_back({primec::IrOpcode::JumpIfZero, 8});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 7});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Glsl, error));
  CHECK(error.empty());
}

TEST_CASE("ir validator glsl target rejects out-of-range i64 literals") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI64, 5000000000ull});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Glsl, error));
  CHECK(error.find("glsl i64 literal out of i32 range") != std::string::npos);
}

TEST_CASE("ir validator glsl target rejects out-of-range local slots") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::LoadLocal, 1024});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Glsl, error));
  CHECK(error.find("local index exceeds glsl local-slot limit") != std::string::npos);
}

TEST_CASE("ir validator wasm target accepts integer control-flow subset") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  fn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  fn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  fn.instructions.push_back({primec::IrOpcode::CmpEqI32, 0});
  fn.instructions.push_back({primec::IrOpcode::JumpIfZero, 8});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 7});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Wasm, error));
  CHECK(error.empty());
}

TEST_CASE("ir validator wasm target accepts float and conversion subset") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 2});
  fn.instructions.push_back({primec::IrOpcode::ConvertI32ToF32, 0});
  fn.instructions.push_back({primec::IrOpcode::PushF32, 0x3fc00000u});
  fn.instructions.push_back({primec::IrOpcode::AddF32, 0});
  fn.instructions.push_back({primec::IrOpcode::ConvertF32ToF64, 0});
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff0000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::SubF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ConvertF64ToF32, 0});
  fn.instructions.push_back({primec::IrOpcode::ConvertF32ToI64, 0});
  fn.instructions.push_back({primec::IrOpcode::ConvertI64ToF32, 0});
  fn.instructions.push_back({primec::IrOpcode::ConvertF32ToU64, 0});
  fn.instructions.push_back({primec::IrOpcode::ConvertU64ToF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ConvertF64ToI64, 0});
  fn.instructions.push_back({primec::IrOpcode::ConvertI64ToF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ConvertF64ToU64, 0});
  fn.instructions.push_back({primec::IrOpcode::ConvertU64ToF32, 0});
  fn.instructions.push_back({primec::IrOpcode::ConvertF32ToI32, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Wasm, error));
  CHECK(error.empty());
}

TEST_CASE("ir validator wasm target rejects unsupported opcodes") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::HeapAlloc, 1});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Wasm, error));
  CHECK(error.find("unsupported opcode for wasm target") != std::string::npos);
}

TEST_CASE("ir validator wasm target accepts call opcodes") {
  primec::IrModule module;
  module.entryIndex = 2;

  primec::IrFunction voidFn;
  voidFn.name = "/void_fn";
  voidFn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});

  primec::IrFunction valueFn;
  valueFn.name = "/value_fn";
  valueFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  valueFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::CallVoid, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(voidFn));
  module.functions.push_back(std::move(valueFn));
  module.functions.push_back(std::move(mainFn));

  std::string error;
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Wasm, error));
  CHECK(error.empty());
}

TEST_CASE("ir validator wasm target accepts wasi output and argv opcodes") {
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("ok");

  primec::IrFunction fn;
  fn.name = "/main";
  fn.metadata.effectMask = primec::EffectIoOut | primec::EffectIoErr;
  fn.metadata.capabilityMask = primec::EffectIoOut | primec::EffectIoErr;
  fn.instructions.push_back({primec::IrOpcode::PushArgc, 0});
  fn.instructions.push_back({primec::IrOpcode::Pop, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  fn.instructions.push_back({primec::IrOpcode::PrintArgv, primec::PrintFlagNewline});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  fn.instructions.push_back(
      {primec::IrOpcode::PrintArgvUnsafe, primec::PrintFlagStderr | primec::PrintFlagNewline});
  fn.instructions.push_back(
      {primec::IrOpcode::PrintString, primec::encodePrintStringImm(0, primec::PrintFlagStderr)});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(std::move(fn));

  std::string error;
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Wasm, error));
  CHECK(error.empty());
}

TEST_CASE("ir validator wasm target accepts wasi file opcodes and file_write effect") {
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("out.txt");
  module.stringTable.push_back("payload");

  primec::IrFunction fn;
  fn.name = "/main";
  fn.metadata.effectMask = primec::EffectFileWrite;
  fn.metadata.capabilityMask = primec::EffectFileWrite;
  fn.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  fn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  fn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  fn.instructions.push_back({primec::IrOpcode::FileWriteString, 1});
  fn.instructions.push_back({primec::IrOpcode::Pop, 0});
  fn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  fn.instructions.push_back({primec::IrOpcode::FileFlush, 0});
  fn.instructions.push_back({primec::IrOpcode::Pop, 0});
  fn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  fn.instructions.push_back({primec::IrOpcode::FileClose, 0});
  fn.instructions.push_back({primec::IrOpcode::Pop, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(std::move(fn));

  std::string error;
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Wasm, error));
  CHECK(error.empty());
}

TEST_CASE("ir validator wasm target accepts wasi file read opcode and file_read effect") {
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("in.txt");

  primec::IrFunction fn;
  fn.name = "/main";
  fn.metadata.effectMask = primec::EffectFileRead;
  fn.metadata.capabilityMask = primec::EffectFileRead;
  fn.instructions.push_back({primec::IrOpcode::FileOpenRead, 0});
  fn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  fn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  fn.instructions.push_back({primec::IrOpcode::FileReadByte, 1});
  fn.instructions.push_back({primec::IrOpcode::Pop, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(std::move(fn));

  std::string error;
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Wasm, error));
  CHECK(error.empty());
}


TEST_SUITE_END();
