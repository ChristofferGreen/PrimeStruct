#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer file write helpers emit write-bytes loops") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto getInstructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, int32_t imm) { instructions.at(index).imm = imm; };

  primec::Expr bytesExpr;
  bytesExpr.kind = primec::Expr::Kind::Name;
  bytesExpr.name = "bytes";

  int nextLocal = 10;
  int emitExprCalls = 0;
  CHECK(primec::ir_lowerer::emitFileWriteBytesLoop(
      bytesExpr,
      3,
      [&](const primec::Expr &) {
        emitExprCalls++;
        emitInstruction(primec::IrOpcode::PushI64, 55);
        return true;
      },
      [&]() { return nextLocal++; },
      emitInstruction,
      getInstructionCount,
      patchInstructionImm));
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 34);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 10);
  CHECK(instructions[12].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[12].imm == 33);
  CHECK(instructions[16].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[16].imm == 33);
  CHECK(instructions[17].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[17].imm == 3);
  CHECK(instructions[32].op == primec::IrOpcode::Jump);
  CHECK(instructions[32].imm == 9);
  CHECK(instructions[33].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[33].imm == 13);

  instructions.clear();
  nextLocal = 40;
  CHECK_FALSE(primec::ir_lowerer::emitFileWriteBytesLoop(
      bytesExpr,
      7,
      [](const primec::Expr &) { return false; },
      [&]() { return nextLocal++; },
      emitInstruction,
      getInstructionCount,
      patchInstructionImm));
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer file write helpers dispatch file-handle methods") {
  using Result = primec::ir_lowerer::FileHandleMethodCallEmitResult;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo fileInfo;
  fileInfo.index = 7;
  fileInfo.isFileHandle = true;
  locals.emplace("file", fileInfo);

  primec::ir_lowerer::LocalInfo valueInfo;
  valueInfo.index = 11;
  locals.emplace("value", valueInfo);

  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto getInstructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, int32_t imm) { instructions.at(index).imm = imm; };

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "file";
  primec::Expr valueArg;
  valueArg.kind = primec::Expr::Kind::Name;
  valueArg.name = "value";

  primec::Expr writeExpr;
  writeExpr.kind = primec::Expr::Kind::Call;
  writeExpr.name = "write";
  writeExpr.isMethodCall = true;
  writeExpr.args = {receiver, valueArg};

  int emitExprCalls = 0;
  int nextLocal = 20;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitFileHandleMethodCall(
            writeExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &localMap) {
              ++emitExprCalls;
              CHECK(localMap.count("file") == 1);
              emitInstruction(primec::IrOpcode::PushI32, 99);
              return true;
            },
            [&]() { return nextLocal++; },
            emitInstruction,
            getInstructionCount,
            patchInstructionImm,
            error) == Result::Emitted);
  CHECK(emitExprCalls == 1);
  CHECK(error.empty());
  CHECK_FALSE(instructions.empty());

  instructions.clear();
  emitExprCalls = 0;
  nextLocal = 20;
  primec::Expr closeExpr;
  closeExpr.kind = primec::Expr::Kind::Call;
  closeExpr.name = "close";
  closeExpr.isMethodCall = true;
  closeExpr.args = {receiver};
  CHECK(primec::ir_lowerer::tryEmitFileHandleMethodCall(
            closeExpr,
            locals,
            [](const primec::Expr &callExpr, const primec::ir_lowerer::LocalMap &) {
              return callExpr.name == "close";
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              return true;
            },
            [&]() { return nextLocal++; },
            emitInstruction,
            getInstructionCount,
            patchInstructionImm,
            error) == Result::NotMatched);
  CHECK(emitExprCalls == 0);
  CHECK(error.empty());
  CHECK(instructions.empty());

  instructions.clear();
  emitExprCalls = 0;
  nextLocal = 20;
  primec::Expr readExpr;
  readExpr.kind = primec::Expr::Kind::Call;
  readExpr.name = "read_byte";
  readExpr.isMethodCall = true;
  readExpr.args = {receiver, valueArg};
  valueInfo.isMutable = true;
  valueInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals["value"] = valueInfo;
  CHECK(primec::ir_lowerer::tryEmitFileHandleMethodCall(
            readExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              return true;
            },
            [&]() { return nextLocal++; },
            emitInstruction,
            getInstructionCount,
            patchInstructionImm,
            error) == Result::Emitted);
  CHECK(emitExprCalls == 0);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 7);
  CHECK(instructions[1].op == primec::IrOpcode::FileReadByte);
  CHECK(instructions[1].imm == 11);

  primec::Expr badWriteByteExpr;
  badWriteByteExpr.kind = primec::Expr::Kind::Call;
  badWriteByteExpr.name = "write_byte";
  badWriteByteExpr.isMethodCall = true;
  badWriteByteExpr.args = {receiver};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitFileHandleMethodCall(
            badWriteByteExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            [](primec::IrOpcode, uint64_t) {},
            []() { return static_cast<size_t>(0); },
            [](size_t, int32_t) {},
            error) == Result::Error);
  CHECK(error == "write_byte requires exactly one argument");

  primec::Expr unknownMethodExpr;
  unknownMethodExpr.kind = primec::Expr::Kind::Call;
  unknownMethodExpr.name = "noop";
  unknownMethodExpr.isMethodCall = true;
  unknownMethodExpr.args = {receiver, valueArg};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitFileHandleMethodCall(
            unknownMethodExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            [](primec::IrOpcode, uint64_t) {},
            []() { return static_cast<size_t>(0); },
            [](size_t, int32_t) {},
            error) == Result::NotMatched);

  primec::Expr nonFileReceiver;
  nonFileReceiver.kind = primec::Expr::Kind::Name;
  nonFileReceiver.name = "value";
  primec::Expr nonFileExpr;
  nonFileExpr.kind = primec::Expr::Kind::Call;
  nonFileExpr.name = "write";
  nonFileExpr.isMethodCall = true;
  nonFileExpr.args = {nonFileReceiver, valueArg};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitFileHandleMethodCall(
            nonFileExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            [](primec::IrOpcode, uint64_t) {},
            []() { return static_cast<size_t>(0); },
            [](size_t, int32_t) {},
            error) == Result::NotMatched);

  primec::ir_lowerer::LocalInfo borrowedPackInfo;
  borrowedPackInfo.index = 14;
  borrowedPackInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  borrowedPackInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  borrowedPackInfo.isArgsPack = true;
  borrowedPackInfo.isFileHandle = true;
  borrowedPackInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  locals["values"] = borrowedPackInfo;

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "at";
  accessExpr.args = {valuesName, indexExpr};
  primec::Expr derefReceiver;
  derefReceiver.kind = primec::Expr::Kind::Call;
  derefReceiver.name = "dereference";
  derefReceiver.args = {accessExpr};
  primec::Expr borrowedWriteExpr = writeExpr;
  borrowedWriteExpr.args = {derefReceiver, valueArg};

  instructions.clear();
  int receiverEmitCalls = 0;
  int valueEmitCalls = 0;
  nextLocal = 40;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitFileHandleMethodCall(
            borrowedWriteExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            },
            [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
              if (valueExpr.kind == primec::Expr::Kind::Call && valueExpr.name == "dereference") {
                ++receiverEmitCalls;
                emitInstruction(primec::IrOpcode::PushI64, 55);
                return true;
              }
              ++valueEmitCalls;
              emitInstruction(primec::IrOpcode::PushI32, 99);
              return true;
            },
            [&]() { return nextLocal++; },
            emitInstruction,
            getInstructionCount,
            patchInstructionImm,
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(receiverEmitCalls == 1);
  CHECK(valueEmitCalls == 1);
  CHECK_FALSE(instructions.empty());

  primec::ir_lowerer::LocalInfo pointerPackInfo = borrowedPackInfo;
  pointerPackInfo.index = 15;
  pointerPackInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  locals["values_ptr"] = pointerPackInfo;

  primec::Expr pointerValuesName = valuesName;
  pointerValuesName.name = "values_ptr";
  primec::Expr pointerAccessExpr = accessExpr;
  pointerAccessExpr.args = {pointerValuesName, indexExpr};
  primec::Expr pointerDerefReceiver = derefReceiver;
  pointerDerefReceiver.args = {pointerAccessExpr};
  primec::Expr pointerWriteExpr = writeExpr;
  pointerWriteExpr.args = {pointerDerefReceiver, valueArg};

  instructions.clear();
  receiverEmitCalls = 0;
  valueEmitCalls = 0;
  nextLocal = 44;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitFileHandleMethodCall(
            pointerWriteExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            },
            [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
              if (valueExpr.kind == primec::Expr::Kind::Call && valueExpr.name == "dereference") {
                ++receiverEmitCalls;
                emitInstruction(primec::IrOpcode::PushI64, 77);
                return true;
              }
              ++valueEmitCalls;
              emitInstruction(primec::IrOpcode::PushI32, 99);
              return true;
            },
            [&]() { return nextLocal++; },
            emitInstruction,
            getInstructionCount,
            patchInstructionImm,
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(receiverEmitCalls == 1);
  CHECK(valueEmitCalls == 1);
  CHECK_FALSE(instructions.empty());
}

TEST_CASE("ir lowerer file write helpers emit flush and close calls") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };

  primec::ir_lowerer::emitFileFlushCall(12, emitInstruction);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 12);
  CHECK(instructions[1].op == primec::IrOpcode::FileFlush);

  int nextLocal = 30;
  primec::ir_lowerer::emitFileCloseCall(12, [&]() { return nextLocal++; }, emitInstruction);
  REQUIRE(instructions.size() == 8);
  CHECK(instructions[2].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[2].imm == 12);
  CHECK(instructions[3].op == primec::IrOpcode::FileClose);
  CHECK(instructions[4].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[4].imm == 30);
  CHECK(instructions[5].op == primec::IrOpcode::PushI64);
  CHECK(instructions[5].imm == static_cast<uint64_t>(static_cast<int64_t>(-1)));
  CHECK(instructions[6].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[6].imm == 12);
  CHECK(instructions[7].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[7].imm == 30);
}

TEST_CASE("ir lowerer string call helpers emit values from locals") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto internString = [](const std::string &text) -> int32_t {
    return text == "hello" ? 5 : 8;
  };
  auto resolveArrayAccessName = [](const primec::Expr &, std::string &) { return false; };
  auto isEntryArgsName = [](const primec::Expr &) { return false; };
  auto resolveStringIndexOps =
      [](const primec::Expr &, const std::string &, primec::ir_lowerer::StringIndexOps &, std::string &) {
        return false;
      };
  auto emitExpr = [](const primec::Expr &) { return true; };
  auto inferCallReturnsString = [](const primec::Expr &) { return false; };
  auto allocTempLocal = []() { return 0; };
  auto getInstructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, int32_t imm) { instructions.at(index).imm = imm; };
  auto emitArrayIndexOutOfBounds = []() {};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localString;
  localString.index = 42;
  localString.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  localString.stringSource = primec::ir_lowerer::LocalInfo::StringSource::RuntimeIndex;
  localString.stringIndex = 11;
  localString.argvChecked = true;
  locals.emplace("text", localString);

  primec::Expr nameArg;
  nameArg.kind = primec::Expr::Kind::Name;
  nameArg.name = "text";
  primec::ir_lowerer::LocalInfo::StringSource sourceOut = primec::ir_lowerer::LocalInfo::StringSource::None;
  int32_t stringIndexOut = -1;
  bool argvCheckedOut = false;
  std::string error;
  CHECK(primec::ir_lowerer::emitStringValueForCallFromLocals(nameArg,
                                                              locals,
                                                              internString,
                                                              emitInstruction,
                                                              resolveArrayAccessName,
                                                              isEntryArgsName,
                                                              resolveStringIndexOps,
                                                              emitExpr,
                                                              inferCallReturnsString,
                                                              allocTempLocal,
                                                              getInstructionCount,
                                                              patchInstructionImm,
                                                              emitArrayIndexOutOfBounds,
                                                              sourceOut,
                                                              stringIndexOut,
                                                              argvCheckedOut,
                                                              error));
  CHECK(error.empty());
  CHECK(sourceOut == primec::ir_lowerer::LocalInfo::StringSource::RuntimeIndex);
  CHECK(stringIndexOut == 11);
  CHECK(argvCheckedOut);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions.front().op == primec::IrOpcode::LoadLocal);
  CHECK(instructions.front().imm == 42);

  instructions.clear();
  primec::Expr badName = nameArg;
  badName.name = "missing";
  CHECK_FALSE(primec::ir_lowerer::emitStringValueForCallFromLocals(badName,
                                                                    locals,
                                                                    internString,
                                                                    emitInstruction,
                                                                    resolveArrayAccessName,
                                                                    isEntryArgsName,
                                                                    resolveStringIndexOps,
                                                                    emitExpr,
                                                                    inferCallReturnsString,
                                                                    allocTempLocal,
                                                                    getInstructionCount,
                                                                    patchInstructionImm,
                                                                    emitArrayIndexOutOfBounds,
                                                                    sourceOut,
                                                                    stringIndexOut,
                                                                    argvCheckedOut,
                                                                    error));
  CHECK(error == "native backend does not know identifier: missing");
}

TEST_CASE("ir lowerer string call helpers emit entry-args call values from locals") {
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

  primec::ir_lowerer::LocalInfo::StringSource sourceOut = primec::ir_lowerer::LocalInfo::StringSource::None;
  int32_t stringIndexOut = -1;
  bool argvCheckedOut = false;
  std::string error;
  int emittedIndexExprs = 0;

  CHECK(primec::ir_lowerer::emitStringValueForCallFromLocals(
      atCall,
      {},
      [](const std::string &) { return 0; },
      emitInstruction,
      [](const primec::Expr &callExpr, std::string &nameOut) {
        if (callExpr.name != "at" && callExpr.name != "unchecked_at") {
          return false;
        }
        nameOut = callExpr.name;
        return true;
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
      getInstructionCount,
      patchInstructionImm,
      [&]() { emitInstruction(primec::IrOpcode::PushI32, 999); },
      sourceOut,
      stringIndexOut,
      argvCheckedOut,
      error));
  CHECK(error.empty());
  CHECK(sourceOut == primec::ir_lowerer::LocalInfo::StringSource::ArgvIndex);
  CHECK(argvCheckedOut);
  CHECK(emittedIndexExprs == 1);
  REQUIRE_FALSE(instructions.empty());
  CHECK(instructions.back().op == primec::IrOpcode::LoadLocal);
}

TEST_CASE("ir lowerer string call helpers surface errors and not-handled cases") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto internString = [](const std::string &) -> int32_t { return 1; };

  primec::Expr badLiteral;
  badLiteral.kind = primec::Expr::Kind::StringLiteral;
  badLiteral.stringValue = "\"x\"bogus";
  primec::ir_lowerer::StringCallSource source = primec::ir_lowerer::StringCallSource::None;
  int32_t stringIndex = -1;
  bool argvChecked = true;
  std::string error;
  CHECK(primec::ir_lowerer::emitLiteralOrBindingStringCallValue(
            badLiteral,
            internString,
            emitInstruction,
            [](const std::string &) { return primec::ir_lowerer::StringBindingInfo{}; },
            source,
            stringIndex,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Error);
  CHECK(error.find("unknown string literal suffix") != std::string::npos);

  instructions.clear();
  primec::Expr unknownName;
  unknownName.kind = primec::Expr::Kind::Name;
  unknownName.name = "missing";
  CHECK(primec::ir_lowerer::emitLiteralOrBindingStringCallValue(
            unknownName,
            internString,
            emitInstruction,
            [](const std::string &) { return primec::ir_lowerer::StringBindingInfo{}; },
            source,
            stringIndex,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Error);
  CHECK(error == "native backend does not know identifier: missing");

  instructions.clear();
  primec::Expr callArg;
  callArg.kind = primec::Expr::Kind::Call;
  callArg.name = "at";
  CHECK(primec::ir_lowerer::emitLiteralOrBindingStringCallValue(
            callArg,
            internString,
            emitInstruction,
            [](const std::string &) { return primec::ir_lowerer::StringBindingInfo{}; },
            source,
            stringIndex,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::NotHandled);
  CHECK(instructions.empty());
}


TEST_SUITE_END();
