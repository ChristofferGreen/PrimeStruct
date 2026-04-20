#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer file write helpers resolve write opcodes") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::IrOpcode opcode = primec::IrOpcode::PushI32;
  CHECK(primec::ir_lowerer::resolveFileWriteValueOpcode(ValueKind::Int32, opcode));
  CHECK(opcode == primec::IrOpcode::FileWriteI32);

  CHECK(primec::ir_lowerer::resolveFileWriteValueOpcode(ValueKind::Bool, opcode));
  CHECK(opcode == primec::IrOpcode::FileWriteI32);

  CHECK(primec::ir_lowerer::resolveFileWriteValueOpcode(ValueKind::Int64, opcode));
  CHECK(opcode == primec::IrOpcode::FileWriteI64);

  CHECK(primec::ir_lowerer::resolveFileWriteValueOpcode(ValueKind::UInt64, opcode));
  CHECK(opcode == primec::IrOpcode::FileWriteU64);

  CHECK_FALSE(primec::ir_lowerer::resolveFileWriteValueOpcode(ValueKind::Float64, opcode));
}

TEST_CASE("ir lowerer file write helpers resolve and emit file open modes") {
  primec::IrOpcode opcode = primec::IrOpcode::PushI32;
  CHECK(primec::ir_lowerer::resolveFileOpenModeOpcode("Read", opcode));
  CHECK(opcode == primec::IrOpcode::FileOpenRead);
  CHECK(primec::ir_lowerer::resolveFileOpenModeOpcode("Write", opcode));
  CHECK(opcode == primec::IrOpcode::FileOpenWrite);
  CHECK(primec::ir_lowerer::resolveFileOpenModeOpcode("Append", opcode));
  CHECK(opcode == primec::IrOpcode::FileOpenAppend);
  CHECK_FALSE(primec::ir_lowerer::resolveFileOpenModeOpcode("Invalid", opcode));

  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };

  std::string error;
  CHECK(primec::ir_lowerer::emitFileOpenCall("Read", 7, emitInstruction, error));
  CHECK(primec::ir_lowerer::emitFileOpenCall("Write", 8, emitInstruction, error));
  CHECK(primec::ir_lowerer::emitFileOpenCall("Append", 9, emitInstruction, error));
  REQUIRE(instructions.size() == 3);
  CHECK(instructions[0].op == primec::IrOpcode::FileOpenRead);
  CHECK(instructions[0].imm == 7);
  CHECK(instructions[1].op == primec::IrOpcode::FileOpenWrite);
  CHECK(instructions[1].imm == 8);
  CHECK(instructions[2].op == primec::IrOpcode::FileOpenAppend);
  CHECK(instructions[2].imm == 9);
  CHECK(error.empty());

  CHECK_FALSE(primec::ir_lowerer::emitFileOpenCall("Invalid", 10, emitInstruction, error));
  CHECK(error == "File requires Read, Write, or Append mode");
  CHECK(instructions.size() == 3);
}

TEST_CASE("ir lowerer file write helpers dispatch File constructor calls") {
  using Result = primec::ir_lowerer::FileConstructorCallEmitResult;

  primec::Expr fileCallExpr;
  fileCallExpr.kind = primec::Expr::Kind::Call;
  fileCallExpr.name = "File";
  fileCallExpr.templateArgs = {"Read"};
  primec::Expr pathArg;
  pathArg.kind = primec::Expr::Kind::StringLiteral;
  pathArg.stringValue = "\"out.txt\"";
  fileCallExpr.args = {pathArg};

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };

  std::string error;
  CHECK(primec::ir_lowerer::tryEmitFileConstructorCall(
            fileCallExpr,
            locals,
            [&](const primec::Expr &valueExpr,
                const primec::ir_lowerer::LocalMap &localMap,
                int32_t &stringIndexOut,
                size_t &lengthOut) {
              CHECK(valueExpr.kind == primec::Expr::Kind::StringLiteral);
              CHECK(localMap.empty());
              stringIndexOut = 41;
              lengthOut = 7;
              return true;
            },
            emitInstruction,
            error) ==
        Result::Emitted);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::FileOpenRead);
  CHECK(instructions[0].imm == 41);

  instructions.clear();
  primec::Expr scopedFileCallExpr = fileCallExpr;
  scopedFileCallExpr.namespacePrefix = "/std/file";
  CHECK(primec::ir_lowerer::tryEmitFileConstructorCall(
            scopedFileCallExpr,
            locals,
            [&](const primec::Expr &valueExpr,
                const primec::ir_lowerer::LocalMap &localMap,
                int32_t &stringIndexOut,
                size_t &lengthOut) {
              CHECK(valueExpr.kind == primec::Expr::Kind::StringLiteral);
              CHECK(localMap.empty());
              stringIndexOut = 52;
              lengthOut = 7;
              return true;
            },
            emitInstruction,
            error) ==
        Result::Emitted);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::FileOpenRead);
  CHECK(instructions[0].imm == 52);

  instructions.clear();
  primec::Expr nonMatchExpr = fileCallExpr;
  nonMatchExpr.name = "open_file";
  CHECK(primec::ir_lowerer::tryEmitFileConstructorCall(
            nonMatchExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            emitInstruction,
            error) == Result::NotMatched);
  CHECK(instructions.empty());

  primec::Expr badTemplateExpr = fileCallExpr;
  badTemplateExpr.templateArgs.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitFileConstructorCall(
            badTemplateExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return true;
            },
            emitInstruction,
            error) == Result::Error);
  CHECK(error == "File requires exactly one template argument");

  primec::Expr badArityExpr = fileCallExpr;
  badArityExpr.args.push_back(pathArg);
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitFileConstructorCall(
            badArityExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return true;
            },
            emitInstruction,
            error) == Result::Error);
  CHECK(error == "File requires exactly one path argument");

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitFileConstructorCall(
            fileCallExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            emitInstruction,
            error) == Result::Error);
  CHECK(error == "native backend only supports File() with string literals or literal-backed bindings");

  primec::Expr invalidModeExpr = fileCallExpr;
  invalidModeExpr.templateArgs = {"BadMode"};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitFileConstructorCall(
            invalidModeExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndexOut, size_t &lengthOut) {
              stringIndexOut = 9;
              lengthOut = 3;
              return true;
            },
            emitInstruction,
            error) == Result::Error);
  CHECK(error == "File requires Read, Write, or Append mode");
}

TEST_CASE("parseAndValidate rewrites imported File constructor entry points through stdlib open helpers") {
  const std::string source = R"(
import /std/file/*

[return<int> effects(file_write)]
main() {
  [Result<File<Write>, FileError>] opened{File<Write>("out.txt"utf8)}
  return(0i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  const primec::Definition *mainDef = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == "/main") {
      mainDef = &def;
      break;
    }
  }
  REQUIRE(mainDef != nullptr);
  REQUIRE(mainDef->statements.size() == 2);
  const primec::Expr &binding = mainDef->statements.front();
  REQUIRE(binding.isBinding);
  REQUIRE(binding.args.size() == 1);
  REQUIRE(binding.args.front().kind == primec::Expr::Kind::Call);
  CHECK(binding.args.front().name == "/File/open_write");
  CHECK(binding.args.front().templateArgs.empty());
  REQUIRE(binding.args.front().args.size() == 1);
  CHECK(binding.args.front().args.front().kind == primec::Expr::Kind::StringLiteral);
}

TEST_CASE("ir lowerer file write helpers emit write steps") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  std::string error;

  primec::Expr arg;
  arg.kind = primec::Expr::Kind::Name;
  arg.name = "value";

  bool emitExprCalled = false;
  CHECK(primec::ir_lowerer::emitFileWriteStep(
      arg,
      9,
      17,
      [](const primec::Expr &, int32_t &stringIndex, size_t &length) {
        stringIndex = 23;
        length = 5;
        return true;
      },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::ValueKind::Unknown; },
      [&](const primec::Expr &) {
        emitExprCalled = true;
        return true;
      },
      emitInstruction,
      error));
  CHECK_FALSE(emitExprCalled);
  REQUIRE(instructions.size() == 3);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 9);
  CHECK(instructions[1].op == primec::IrOpcode::FileWriteString);
  CHECK(instructions[1].imm == 23);
  CHECK(instructions[2].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[2].imm == 17);
  CHECK(error.empty());

  instructions.clear();
  emitExprCalled = false;
  CHECK(primec::ir_lowerer::emitFileWriteStep(
      arg,
      4,
      8,
      [](const primec::Expr &, int32_t &, size_t &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::ValueKind::Int64; },
      [&](const primec::Expr &) {
        emitExprCalled = true;
        emitInstruction(primec::IrOpcode::PushI64, 77);
        return true;
      },
      emitInstruction,
      error));
  CHECK(emitExprCalled);
  REQUIRE(instructions.size() == 4);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 4);
  CHECK(instructions[1].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].imm == 77);
  CHECK(instructions[2].op == primec::IrOpcode::FileWriteI64);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 8);

  instructions.clear();
  emitExprCalled = false;
  CHECK(primec::ir_lowerer::emitFileWriteStep(
      arg,
      3,
      6,
      [](const primec::Expr &, int32_t &, size_t &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::ValueKind::String; },
      [&](const primec::Expr &) {
        emitExprCalled = true;
        emitInstruction(primec::IrOpcode::LoadLocal, 11);
        return true;
      },
      emitInstruction,
      error));
  CHECK(emitExprCalled);
  REQUIRE(instructions.size() == 4);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 3);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 11);
  CHECK(instructions[2].op == primec::IrOpcode::FileWriteStringDynamic);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 6);

  instructions.clear();
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitFileWriteStep(
      arg,
      4,
      8,
      [](const primec::Expr &, int32_t &, size_t &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::ValueKind::Float32; },
      [](const primec::Expr &) { return true; },
      emitInstruction,
      error));
  CHECK(error == "file write requires integer/bool or string arguments");

  instructions.clear();
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitFileWriteStep(
      arg,
      4,
      8,
      [](const primec::Expr &, int32_t &, size_t &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::ValueKind::Int32; },
      [](const primec::Expr &) { return false; },
      emitInstruction,
      error));
  CHECK(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 4);
}

TEST_CASE("ir lowerer file write helpers emit write and write_line calls") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto getInstructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, int32_t imm) { instructions.at(index).imm = imm; };

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "file";
  primec::Expr argA = receiver;
  argA.name = "a";
  primec::Expr argB = receiver;
  argB.name = "b";

  primec::Expr writeExpr;
  writeExpr.kind = primec::Expr::Kind::Call;
  writeExpr.name = "write";
  writeExpr.args = {receiver, argA, argB};

  int nextLocal = 20;
  int writeStepCalls = 0;
  CHECK(primec::ir_lowerer::emitFileWriteCall(
      writeExpr,
      5,
      [&](const primec::Expr &, int32_t errorLocal) {
        ++writeStepCalls;
        CHECK(errorLocal == 20);
        emitInstruction(primec::IrOpcode::PushI32, static_cast<uint64_t>(100 + writeStepCalls));
        emitInstruction(primec::IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal));
        return true;
      },
      [&]() { return nextLocal++; },
      emitInstruction,
      getInstructionCount,
      patchInstructionImm));
  CHECK(writeStepCalls == 2);
  REQUIRE(instructions.size() == 15);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 20);
  CHECK(instructions[5].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[5].imm == 8);
  CHECK(instructions[11].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[11].imm == 14);
  CHECK(instructions[14].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[14].imm == 20);

  instructions.clear();
  primec::Expr writeLineExpr;
  writeLineExpr.kind = primec::Expr::Kind::Call;
  writeLineExpr.name = "write_line";
  writeLineExpr.args = {receiver};
  nextLocal = 30;
  writeStepCalls = 0;
  CHECK(primec::ir_lowerer::emitFileWriteCall(
      writeLineExpr,
      7,
      [&](const primec::Expr &, int32_t) {
        ++writeStepCalls;
        return true;
      },
      [&]() { return nextLocal++; },
      emitInstruction,
      getInstructionCount,
      patchInstructionImm));
  CHECK(writeStepCalls == 0);
  REQUIRE(instructions.size() == 10);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 30);
  CHECK(instructions[5].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[5].imm == 9);
  CHECK(instructions[6].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[6].imm == 7);
  CHECK(instructions[7].op == primec::IrOpcode::FileWriteNewline);
  CHECK(instructions[8].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[8].imm == 30);
  CHECK(instructions[9].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[9].imm == 30);

  instructions.clear();
  primec::Expr failingWriteExpr;
  failingWriteExpr.kind = primec::Expr::Kind::Call;
  failingWriteExpr.name = "write";
  failingWriteExpr.args = {receiver, argA};
  nextLocal = 40;
  CHECK_FALSE(primec::ir_lowerer::emitFileWriteCall(
      failingWriteExpr,
      9,
      [](const primec::Expr &, int32_t) { return false; },
      [&]() { return nextLocal++; },
      emitInstruction,
      getInstructionCount,
      patchInstructionImm));
  REQUIRE(instructions.size() == 6);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 40);
  CHECK(instructions[5].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[5].imm == 0);
}

TEST_CASE("ir lowerer file write helpers emit write_byte calls") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "file";
  primec::Expr byteArg;
  byteArg.kind = primec::Expr::Kind::Name;
  byteArg.name = "value";

  primec::Expr writeByteExpr;
  writeByteExpr.kind = primec::Expr::Kind::Call;
  writeByteExpr.name = "write_byte";
  writeByteExpr.args = {receiver, byteArg};

  int emitExprCalls = 0;
  std::string error;
  CHECK(primec::ir_lowerer::emitFileWriteByteCall(
      writeByteExpr,
      14,
      [&](const primec::Expr &) {
        ++emitExprCalls;
        emitInstruction(primec::IrOpcode::PushI32, 255);
        return true;
      },
      emitInstruction,
      error));
  CHECK(emitExprCalls == 1);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 3);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 14);
  CHECK(instructions[1].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].imm == 255);
  CHECK(instructions[2].op == primec::IrOpcode::FileWriteByte);

  instructions.clear();
  error.clear();
  primec::Expr badArityExpr = writeByteExpr;
  badArityExpr.args = {receiver};
  CHECK_FALSE(primec::ir_lowerer::emitFileWriteByteCall(
      badArityExpr, 14, [](const primec::Expr &) { return true; }, emitInstruction, error));
  CHECK(error == "write_byte requires exactly one argument");
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  emitExprCalls = 0;
  CHECK_FALSE(primec::ir_lowerer::emitFileWriteByteCall(
      writeByteExpr,
      6,
      [&](const primec::Expr &) {
        ++emitExprCalls;
        return false;
      },
      emitInstruction,
      error));
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 6);
}

TEST_CASE("ir lowerer file write helpers emit read_byte calls") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "file";
  primec::Expr valueArg;
  valueArg.kind = primec::Expr::Kind::Name;
  valueArg.name = "value";

  primec::Expr readByteExpr;
  readByteExpr.kind = primec::Expr::Kind::Call;
  readByteExpr.name = "read_byte";
  readByteExpr.args = {receiver, valueArg};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valueInfo;
  valueInfo.index = 17;
  valueInfo.isMutable = true;
  valueInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("value", valueInfo);

  std::string error;
  CHECK(primec::ir_lowerer::emitFileReadByteCall(readByteExpr, locals, 14, emitInstruction, error));
  CHECK(error.empty());
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 14);
  CHECK(instructions[1].op == primec::IrOpcode::FileReadByte);
  CHECK(instructions[1].imm == 17);

  instructions.clear();
  error.clear();
  primec::Expr badArityExpr = readByteExpr;
  badArityExpr.args = {receiver};
  CHECK_FALSE(primec::ir_lowerer::emitFileReadByteCall(badArityExpr, locals, 14, emitInstruction, error));
  CHECK(error == "read_byte requires exactly one argument");
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  valueInfo.isMutable = false;
  locals["value"] = valueInfo;
  CHECK_FALSE(primec::ir_lowerer::emitFileReadByteCall(readByteExpr, locals, 14, emitInstruction, error));
  CHECK(error == "read_byte requires mutable integer binding");
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer file write helpers emit write_bytes calls") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto getInstructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, int32_t imm) { instructions.at(index).imm = imm; };

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "file";
  primec::Expr bytesArg;
  bytesArg.kind = primec::Expr::Kind::Name;
  bytesArg.name = "bytes";

  primec::Expr writeBytesExpr;
  writeBytesExpr.kind = primec::Expr::Kind::Call;
  writeBytesExpr.name = "write_bytes";
  writeBytesExpr.args = {receiver, bytesArg};

  int nextLocal = 50;
  int emitExprCalls = 0;
  std::string error;
  CHECK(primec::ir_lowerer::emitFileWriteBytesCall(
      writeBytesExpr,
      3,
      [&](const primec::Expr &) {
        ++emitExprCalls;
        emitInstruction(primec::IrOpcode::PushI64, 55);
        return true;
      },
      [&]() { return nextLocal++; },
      emitInstruction,
      getInstructionCount,
      patchInstructionImm,
      error));
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 34);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 50);
  CHECK(instructions[33].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[33].imm == 53);

  instructions.clear();
  error.clear();
  primec::Expr badArityExpr = writeBytesExpr;
  badArityExpr.args = {receiver};
  CHECK_FALSE(primec::ir_lowerer::emitFileWriteBytesCall(
      badArityExpr,
      3,
      [](const primec::Expr &) { return true; },
      [&]() { return 0; },
      emitInstruction,
      getInstructionCount,
      patchInstructionImm,
      error));
  CHECK(error == "write_bytes requires exactly one argument");
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  emitExprCalls = 0;
  nextLocal = 60;
  CHECK_FALSE(primec::ir_lowerer::emitFileWriteBytesCall(
      writeBytesExpr,
      3,
      [&](const primec::Expr &) {
        ++emitExprCalls;
        return false;
      },
      [&]() { return nextLocal++; },
      emitInstruction,
      getInstructionCount,
      patchInstructionImm,
      error));
  CHECK(emitExprCalls == 1);
  CHECK(error.empty());
  CHECK(instructions.empty());
}


TEST_SUITE_END();
