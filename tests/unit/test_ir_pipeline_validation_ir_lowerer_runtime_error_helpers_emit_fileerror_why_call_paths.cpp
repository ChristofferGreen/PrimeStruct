#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer runtime error helpers emit FileError.why call paths") {
  using Result = primec::ir_lowerer::FileErrorWhyCallEmitResult;

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.isMethodCall = true;
  expr.name = "noop";
  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::IrInstruction> instructions;
  std::string error = "stale";
  int32_t emittedWhyLocal = -1;
  primec::Expr fileErrorType;
  fileErrorType.kind = primec::Expr::Kind::Name;
  fileErrorType.name = "FileError";
  primec::Expr errValue;
  errValue.kind = primec::Expr::Kind::Name;
  errValue.name = "err";
  int emitExprCalls = 0;

  CHECK(primec::ir_lowerer::tryEmitFileErrorWhyCall(
            expr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](int32_t local) { emittedWhyLocal = local; },
            error) == Result::NotHandled);
  CHECK(error == "stale");
  CHECK(instructions.empty());
  CHECK(emittedWhyLocal == -1);

  instructions.clear();
  error.clear();
  emittedWhyLocal = -1;
  expr.isMethodCall = false;
  expr.namespacePrefix = "/file_error";
  expr.name = "why";
  expr.args = {errValue};
  emitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitFileErrorWhyCall(
            expr,
            locals,
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 66});
              return true;
            },
            []() { return 41; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](int32_t local) { emittedWhyLocal = local; },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  CHECK(emittedWhyLocal == 41);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 41);

  instructions.clear();
  error.clear();
  emittedWhyLocal = -1;
  expr.isMethodCall = true;
  expr.namespacePrefix.clear();
  expr.name = "why";
  expr.args = {fileErrorType, errValue};
  emitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitFileErrorWhyCall(
            expr,
            locals,
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 77});
              return true;
            },
            []() { return 44; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](int32_t local) { emittedWhyLocal = local; },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  CHECK(emittedWhyLocal == 44);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 44);

  instructions.clear();
  error.clear();
  emittedWhyLocal = -1;
  primec::Expr scopedFileErrorType = fileErrorType;
  scopedFileErrorType.namespacePrefix = "/std/file";
  expr.args = {scopedFileErrorType, errValue};
  emitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitFileErrorWhyCall(
            expr,
            locals,
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 78});
              return true;
            },
            []() { return 45; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](int32_t local) { emittedWhyLocal = local; },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  CHECK(emittedWhyLocal == 45);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 45);

  instructions.clear();
  error.clear();
  emittedWhyLocal = -1;
  expr.args = {fileErrorType};
  CHECK(primec::ir_lowerer::tryEmitFileErrorWhyCall(
            expr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 3; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](int32_t local) { emittedWhyLocal = local; },
            error) == Result::Error);
  CHECK(error == "FileError.why requires exactly one argument");
  CHECK(instructions.empty());
  CHECK(emittedWhyLocal == -1);

  instructions.clear();
  error.clear();
  emittedWhyLocal = -1;
  expr.args = {fileErrorType, errValue};
  CHECK(primec::ir_lowerer::tryEmitFileErrorWhyCall(
            expr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            []() { return 5; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](int32_t local) { emittedWhyLocal = local; },
            error) == Result::Error);
  CHECK(instructions.empty());
  CHECK(emittedWhyLocal == -1);

  instructions.clear();
  error.clear();
  emittedWhyLocal = -1;
  expr.args = {errValue};
  primec::ir_lowerer::LocalInfo fileErrorInfo;
  fileErrorInfo.index = 12;
  fileErrorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  fileErrorInfo.isFileError = true;
  locals.emplace("err", fileErrorInfo);
  CHECK(primec::ir_lowerer::tryEmitFileErrorWhyCall(
            expr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            []() { return 9; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](int32_t local) { emittedWhyLocal = local; },
            error) == Result::Emitted);
  CHECK(instructions.empty());
  CHECK(emittedWhyLocal == 12);

  instructions.clear();
  error.clear();
  emittedWhyLocal = -1;
  fileErrorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  fileErrorInfo.index = 33;
  locals["err"] = fileErrorInfo;
  CHECK(primec::ir_lowerer::tryEmitFileErrorWhyCall(
            expr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            []() { return 91; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](int32_t local) { emittedWhyLocal = local; },
            error) == Result::Emitted);
  REQUIRE(instructions.size() == 3);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 33);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[2].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[2].imm == 91);
  CHECK(emittedWhyLocal == 91);

  instructions.clear();
  error.clear();
  emittedWhyLocal = -1;
  fileErrorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  fileErrorInfo.index = 34;
  locals["err"] = fileErrorInfo;
  CHECK(primec::ir_lowerer::tryEmitFileErrorWhyCall(
            expr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            []() { return 92; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](int32_t local) { emittedWhyLocal = local; },
            error) == Result::Emitted);
  REQUIRE(instructions.size() == 3);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 34);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[2].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[2].imm == 92);
  CHECK(emittedWhyLocal == 92);

  instructions.clear();
  error.clear();
  emittedWhyLocal = -1;
  locals.clear();
  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 1;
  primec::Expr indexedReceiver;
  indexedReceiver.kind = primec::Expr::Kind::Call;
  indexedReceiver.name = "at";
  indexedReceiver.args = {valuesName, indexExpr};
  indexedReceiver.argNames = {std::nullopt, std::nullopt};
  expr.args = {indexedReceiver};
  primec::ir_lowerer::LocalInfo fileErrorPackInfo;
  fileErrorPackInfo.index = 21;
  fileErrorPackInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  fileErrorPackInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  fileErrorPackInfo.isArgsPack = true;
  fileErrorPackInfo.isFileError = true;
  fileErrorPackInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Value;
  locals.emplace("values", fileErrorPackInfo);
  emitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitFileErrorWhyCall(
            expr,
            locals,
            [&](const primec::Expr &emittedExpr, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              CHECK(emittedExpr.kind == primec::Expr::Kind::Call);
              instructions.push_back({primec::IrOpcode::PushI64, 81});
              return true;
            },
            []() { return 73; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](int32_t local) { emittedWhyLocal = local; },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 81u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 73u);
  CHECK(emittedWhyLocal == 73);

  instructions.clear();
  error.clear();
  emittedWhyLocal = -1;
  locals.clear();
  primec::Expr dereferenceReceiver;
  dereferenceReceiver.kind = primec::Expr::Kind::Call;
  dereferenceReceiver.name = "dereference";
  dereferenceReceiver.args = {indexedReceiver};
  dereferenceReceiver.argNames = {std::nullopt};
  expr.args = {dereferenceReceiver};
  fileErrorPackInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  locals.emplace("values", fileErrorPackInfo);
  emitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitFileErrorWhyCall(
            expr,
            locals,
            [&](const primec::Expr &emittedExpr, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              CHECK(emittedExpr.kind == primec::Expr::Kind::Call);
              instructions.push_back({primec::IrOpcode::PushI64, 91});
              return true;
            },
            []() { return 74; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](int32_t local) { emittedWhyLocal = local; },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 91u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 74u);
  CHECK(emittedWhyLocal == 74);

  instructions.clear();
  error.clear();
  emittedWhyLocal = -1;
  locals.clear();
  fileErrorPackInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  locals.emplace("values", fileErrorPackInfo);
  emitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitFileErrorWhyCall(
            expr,
            locals,
            [&](const primec::Expr &emittedExpr, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              CHECK(emittedExpr.kind == primec::Expr::Kind::Call);
              instructions.push_back({primec::IrOpcode::PushI64, 101});
              return true;
            },
            []() { return 75; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](int32_t local) { emittedWhyLocal = local; },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 101u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 75u);
  CHECK(emittedWhyLocal == 75);
}

TEST_CASE("ir lowerer runtime error helpers build scoped emitters") {
  primec::IrFunction function;
  std::vector<std::string> stringTable;
  auto internString = [&](const std::string &text) -> int32_t {
    for (size_t i = 0; i < stringTable.size(); ++i) {
      if (stringTable[i] == text) {
        return static_cast<int32_t>(i);
      }
    }
    stringTable.push_back(text);
    return static_cast<int32_t>(stringTable.size() - 1);
  };

  auto emitArrayIndexOutOfBounds = primec::ir_lowerer::makeEmitArrayIndexOutOfBounds(function, internString);
  auto emitMapKeyNotFound = primec::ir_lowerer::makeEmitMapKeyNotFound(function, internString);
  auto emitFloatToIntNonFinite = primec::ir_lowerer::makeEmitFloatToIntNonFinite(function, internString);

  emitArrayIndexOutOfBounds();
  emitMapKeyNotFound();
  emitFloatToIntNonFinite();
  emitArrayIndexOutOfBounds();

  const std::vector<std::string> expectedMessages = {
      "array index out of bounds",
      "map key not found",
      "float to int conversion requires finite value"};
  CHECK(stringTable == expectedMessages);
  REQUIRE(function.instructions.size() == 12);
  CHECK(primec::decodePrintStringIndex(function.instructions[0].imm) == 0);
  CHECK(primec::decodePrintStringIndex(function.instructions[3].imm) == 1);
  CHECK(primec::decodePrintStringIndex(function.instructions[6].imm) == 2);
  CHECK(primec::decodePrintStringIndex(function.instructions[9].imm) == 0);
}

TEST_CASE("ir lowerer runtime error helpers build bundled emitters") {
  primec::IrFunction function;
  std::vector<std::string> stringTable;
  auto internString = [&](const std::string &text) -> int32_t {
    for (size_t i = 0; i < stringTable.size(); ++i) {
      if (stringTable[i] == text) {
        return static_cast<int32_t>(i);
      }
    }
    stringTable.push_back(text);
    return static_cast<int32_t>(stringTable.size() - 1);
  };

  auto emitters = primec::ir_lowerer::makeRuntimeErrorEmitters(function, internString);
  emitters.emitStringIndexOutOfBounds();
  emitters.emitPointerIndexOutOfBounds();
  emitters.emitVectorPopOnEmpty();
  emitters.emitLoopCountNegative();
  emitters.emitStringIndexOutOfBounds();

  const std::vector<std::string> expectedMessages = {"string index out of bounds",
                                                     "pointer index out of bounds",
                                                     "container empty",
                                                     "loop count must be non-negative"};
  CHECK(stringTable == expectedMessages);
  REQUIRE(function.instructions.size() == 15);
  CHECK(primec::decodePrintStringIndex(function.instructions[0].imm) == 0);
  CHECK(primec::decodePrintStringIndex(function.instructions[3].imm) == 1);
  CHECK(primec::decodePrintStringIndex(function.instructions[6].imm) == 2);
  CHECK(primec::decodePrintStringIndex(function.instructions[9].imm) == 3);
  CHECK(primec::decodePrintStringIndex(function.instructions[12].imm) == 0);
}

TEST_CASE("ir lowerer runtime error helpers build bundled string-literal and emitters setup") {
  primec::IrFunction function;
  std::vector<std::string> stringTable;
  std::string error;

  const auto setup =
      primec::ir_lowerer::makeRuntimeErrorAndStringLiteralSetup(stringTable, function, error);
  CHECK(setup.stringLiteralHelpers.internString("hello") == 0);

  setup.runtimeErrorEmitters.emitArrayIndexOutOfBounds();
  REQUIRE(function.instructions.size() == 3);
  CHECK(function.instructions[0].op == primec::IrOpcode::PrintString);
  CHECK(primec::decodePrintStringIndex(function.instructions[0].imm) == 1);

  primec::Expr literalExpr;
  literalExpr.kind = primec::Expr::Kind::StringLiteral;
  literalExpr.stringValue = "\"hello\"utf8";
  int32_t stringIndex = -1;
  size_t length = 0;
  REQUIRE(setup.stringLiteralHelpers.resolveStringTableTarget(
      literalExpr, primec::ir_lowerer::LocalMap{}, stringIndex, length));
  CHECK(stringIndex == 0);
  CHECK(length == 5);

  REQUIRE(stringTable.size() == 2);
  CHECK(stringTable[0] == "hello");
  CHECK(stringTable[1] == "array index out of bounds");
}

TEST_CASE("ir lowerer index kind helpers normalize and validate supported kinds") {
  CHECK(primec::ir_lowerer::normalizeIndexKind(primec::ir_lowerer::LocalInfo::ValueKind::Bool) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(primec::ir_lowerer::normalizeIndexKind(primec::ir_lowerer::LocalInfo::ValueKind::Int64) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  CHECK(primec::ir_lowerer::isSupportedIndexKind(primec::ir_lowerer::LocalInfo::ValueKind::Int32));
  CHECK(primec::ir_lowerer::isSupportedIndexKind(primec::ir_lowerer::LocalInfo::ValueKind::Int64));
  CHECK(primec::ir_lowerer::isSupportedIndexKind(primec::ir_lowerer::LocalInfo::ValueKind::UInt64));
  CHECK_FALSE(primec::ir_lowerer::isSupportedIndexKind(primec::ir_lowerer::LocalInfo::ValueKind::Bool));
  CHECK_FALSE(primec::ir_lowerer::isSupportedIndexKind(primec::ir_lowerer::LocalInfo::ValueKind::Unknown));
}

TEST_CASE("ir lowerer index kind helpers map index opcodes by kind") {
  CHECK(primec::ir_lowerer::pushZeroForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int32) ==
        primec::IrOpcode::PushI32);
  CHECK(primec::ir_lowerer::pushZeroForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int64) ==
        primec::IrOpcode::PushI64);
  CHECK(primec::ir_lowerer::pushZeroForIndex(primec::ir_lowerer::LocalInfo::ValueKind::UInt64) ==
        primec::IrOpcode::PushI64);

  CHECK(primec::ir_lowerer::cmpLtForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int32) ==
        primec::IrOpcode::CmpLtI32);
  CHECK(primec::ir_lowerer::cmpLtForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int64) ==
        primec::IrOpcode::CmpLtI64);
  CHECK(primec::ir_lowerer::cmpLtForIndex(primec::ir_lowerer::LocalInfo::ValueKind::UInt64) ==
        primec::IrOpcode::CmpLtI64);

  CHECK(primec::ir_lowerer::cmpGeForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int32) ==
        primec::IrOpcode::CmpGeI32);
  CHECK(primec::ir_lowerer::cmpGeForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int64) ==
        primec::IrOpcode::CmpGeI64);
  CHECK(primec::ir_lowerer::cmpGeForIndex(primec::ir_lowerer::LocalInfo::ValueKind::UInt64) ==
        primec::IrOpcode::CmpGeU64);

  CHECK(primec::ir_lowerer::pushOneForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int32) ==
        primec::IrOpcode::PushI32);
  CHECK(primec::ir_lowerer::pushOneForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int64) ==
        primec::IrOpcode::PushI64);
  CHECK(primec::ir_lowerer::pushOneForIndex(primec::ir_lowerer::LocalInfo::ValueKind::UInt64) ==
        primec::IrOpcode::PushI64);

  CHECK(primec::ir_lowerer::addForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int32) ==
        primec::IrOpcode::AddI32);
  CHECK(primec::ir_lowerer::addForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int64) ==
        primec::IrOpcode::AddI64);
  CHECK(primec::ir_lowerer::addForIndex(primec::ir_lowerer::LocalInfo::ValueKind::UInt64) ==
        primec::IrOpcode::AddI64);

  CHECK(primec::ir_lowerer::mulForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int32) ==
        primec::IrOpcode::MulI32);
  CHECK(primec::ir_lowerer::mulForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int64) ==
        primec::IrOpcode::MulI64);
  CHECK(primec::ir_lowerer::mulForIndex(primec::ir_lowerer::LocalInfo::ValueKind::UInt64) ==
        primec::IrOpcode::MulI64);
}

TEST_CASE("ir lowerer setup math helper resolves namespaced builtins") {
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/std/math/sin";

  std::string builtinName;
  CHECK(primec::ir_lowerer::getSetupMathBuiltinName(callExpr, false, builtinName));
  CHECK(builtinName == "sin");
}

TEST_CASE("ir lowerer setup math helper resolves root builtin paths without imports") {
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/round";

  std::string builtinName;
  CHECK(primec::ir_lowerer::getSetupMathBuiltinName(callExpr, false, builtinName));
  CHECK(builtinName == "round");
}

TEST_CASE("ir lowerer setup math helper resolves root namespace builtin paths without imports") {
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "abs";
  callExpr.namespacePrefix = "/";

  std::string builtinName;
  CHECK(primec::ir_lowerer::getSetupMathBuiltinName(callExpr, false, builtinName));
  CHECK(builtinName == "abs");
}

TEST_CASE("ir lowerer builtin root helper rejects rooted paths without imports") {
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/sqrt";

  std::string builtinName;
  CHECK_FALSE(primec::ir_lowerer::getBuiltinRootName(callExpr, builtinName, false));
  CHECK(builtinName.empty());
}

TEST_CASE("ir lowerer builtin root helper rejects root namespace paths without imports") {
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "cbrt";
  callExpr.namespacePrefix = "/";

  std::string builtinName;
  CHECK_FALSE(primec::ir_lowerer::getBuiltinRootName(callExpr, builtinName, false));
  CHECK(builtinName.empty());
}

TEST_CASE("ir lowerer pointer helper resolves parser-shaped soa_vector builtins") {
  primec::Expr dereferenceCall;
  dereferenceCall.kind = primec::Expr::Kind::Call;
  dereferenceCall.name = "dereference";
  dereferenceCall.namespacePrefix = "soa_vector";

  std::string builtinName;
  CHECK(primec::ir_lowerer::getBuiltinPointerName(dereferenceCall, builtinName));
  CHECK(builtinName == "dereference");

  primec::Expr locationCall;
  locationCall.kind = primec::Expr::Kind::Call;
  locationCall.name = "location";
  locationCall.namespacePrefix = "soa_vector";

  CHECK(primec::ir_lowerer::getBuiltinPointerName(locationCall, builtinName));
  CHECK(builtinName == "location");
}

TEST_CASE("ir lowerer setup math helper validates builtin support names") {
  CHECK(primec::ir_lowerer::isSupportedMathBuiltinName("sin"));
  CHECK(primec::ir_lowerer::isSupportedMathBuiltinName("is_finite"));
  CHECK(primec::ir_lowerer::isSupportedMathBuiltinName("copysign"));
  CHECK_FALSE(primec::ir_lowerer::isSupportedMathBuiltinName("unknown_math"));
}

TEST_CASE("ir lowerer setup math helper requires import for bare names") {
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "sin";

  std::string builtinName;
  CHECK_FALSE(primec::ir_lowerer::getSetupMathBuiltinName(callExpr, false, builtinName));
  CHECK(primec::ir_lowerer::getSetupMathBuiltinName(callExpr, true, builtinName));
  CHECK(builtinName == "sin");
}

TEST_CASE("ir lowerer setup math helper resolves constants only for supported names") {
  std::string constantName;
  CHECK(primec::ir_lowerer::getSetupMathConstantName("/std/math/pi", false, constantName));
  CHECK(constantName == "pi");
  CHECK_FALSE(primec::ir_lowerer::getSetupMathConstantName("phi", true, constantName));
}

TEST_CASE("ir lowerer setup math helper builds scoped name resolvers") {
  auto resolveBuiltinWithImport = primec::ir_lowerer::makeGetSetupMathBuiltinName(true);
  auto resolveBuiltinWithoutImport = primec::ir_lowerer::makeGetSetupMathBuiltinName(false);
  auto resolveConstantWithImport = primec::ir_lowerer::makeGetSetupMathConstantName(true);
  auto resolveConstantWithoutImport = primec::ir_lowerer::makeGetSetupMathConstantName(false);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "sin";
  std::string builtinName;
  CHECK(resolveBuiltinWithImport(callExpr, builtinName));
  CHECK(builtinName == "sin");
  CHECK_FALSE(resolveBuiltinWithoutImport(callExpr, builtinName));

  std::string constantName;
  CHECK(resolveConstantWithImport("pi", constantName));
  CHECK(constantName == "pi");
  CHECK_FALSE(resolveConstantWithoutImport("pi", constantName));
}

TEST_CASE("ir lowerer setup math helper builds bundled name resolvers") {
  auto resolversWithImport = primec::ir_lowerer::makeSetupMathResolvers(true);
  auto resolversWithoutImport = primec::ir_lowerer::makeSetupMathResolvers(false);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "sin";

  std::string builtinName;
  CHECK(resolversWithImport.getMathBuiltinName(callExpr, builtinName));
  CHECK(builtinName == "sin");
  CHECK_FALSE(resolversWithoutImport.getMathBuiltinName(callExpr, builtinName));

  std::string constantName;
  CHECK(resolversWithImport.getMathConstantName("pi", constantName));
  CHECK(constantName == "pi");
  CHECK_FALSE(resolversWithoutImport.getMathConstantName("pi", constantName));
}

TEST_CASE("ir lowerer setup math helper keeps current bundled binding adapter routes") {
  const auto adapters = primec::ir_lowerer::makeSetupMathAndBindingAdapters(true);

  primec::Expr mathCall;
  mathCall.kind = primec::Expr::Kind::Call;
  mathCall.name = "sin";
  std::string builtinName;
  CHECK(adapters.setupMathResolvers.getMathBuiltinName(mathCall, builtinName));
  CHECK(builtinName == "sin");

  primec::Expr stringExpr;
  primec::Transform stringTransform;
  stringTransform.name = "string";
  stringExpr.transforms.push_back(stringTransform);
  CHECK(adapters.bindingTypeAdapters.isStringBinding(stringExpr));
  CHECK_FALSE(adapters.bindingTypeAdapters.isFileErrorBinding(stringExpr));

  primec::Expr mapExpr;
  primec::Transform mapTransform;
  mapTransform.name = "map";
  mapTransform.templateArgs = {"bool", "f64"};
  mapExpr.transforms.push_back(mapTransform);
  CHECK(adapters.bindingTypeAdapters.bindingValueKind(mapExpr, primec::ir_lowerer::LocalInfo::Kind::Map) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Float64);
}

TEST_CASE("ir lowerer setup math helpers keep semantic product FileError bindings") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "temporary",
      .name = "readFile",
      .bindingTypeText = "FileError",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 21,
      .sourceColumn = 4,
      .semanticNodeId = 33,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/readFile"),
  });

  const auto adapters = primec::ir_lowerer::makeSetupMathAndBindingAdapters(true, &semanticProgram);

  primec::Expr helperResultCall;
  helperResultCall.kind = primec::Expr::Kind::Call;
  helperResultCall.name = "readFile";
  helperResultCall.semanticNodeId = 33;
  helperResultCall.sourceLine = 21;
  helperResultCall.sourceColumn = 4;

  CHECK(adapters.bindingTypeAdapters.isFileErrorBinding(helperResultCall));
  CHECK(adapters.bindingTypeAdapters.bindingKind(helperResultCall) ==
        primec::ir_lowerer::LocalInfo::Kind::Value);
}

TEST_CASE("ir lowerer setup inference helper infers pointer target kinds") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("ptr", pointerInfo);

  primec::ir_lowerer::LocalInfo referenceInfo;
  referenceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  referenceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Float32;
  locals.emplace("refValue", referenceInfo);

  primec::Expr ptrName;
  ptrName.kind = primec::Expr::Kind::Name;
  ptrName.name = "ptr";
  CHECK(primec::ir_lowerer::inferPointerTargetValueKind(
            ptrName,
            locals,
            [](const primec::Expr &expr, std::string &builtinName) {
              return primec::ir_lowerer::getBuiltinOperatorName(expr, builtinName);
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr refName;
  refName.kind = primec::Expr::Kind::Name;
  refName.name = "refValue";
  CHECK(primec::ir_lowerer::inferPointerTargetValueKind(
            refName,
            locals,
            [](const primec::Expr &expr, std::string &builtinName) {
              return primec::ir_lowerer::getBuiltinOperatorName(expr, builtinName);
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Float32);

  primec::Expr locationCall;
  locationCall.kind = primec::Expr::Kind::Call;
  locationCall.name = "location";
  locationCall.args = {refName};
  CHECK(primec::ir_lowerer::inferPointerTargetValueKind(
            locationCall,
            locals,
            [](const primec::Expr &expr, std::string &builtinName) {
              return primec::ir_lowerer::getBuiltinOperatorName(expr, builtinName);
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Float32);

  primec::Expr one;
  one.kind = primec::Expr::Kind::Literal;
  one.literalValue = 1;

  primec::Expr allocCall;
  allocCall.kind = primec::Expr::Kind::Call;
  allocCall.name = "/std/intrinsics/memory/alloc";
  allocCall.templateArgs = {"i32"};
  allocCall.args = {one};
  CHECK(primec::ir_lowerer::inferPointerTargetValueKind(
            allocCall,
            locals,
            [](const primec::Expr &expr, std::string &builtinName) {
              return primec::ir_lowerer::getBuiltinOperatorName(expr, builtinName);
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::Expr plusCall;
  plusCall.kind = primec::Expr::Kind::Call;
  plusCall.name = "plus";
  plusCall.args = {ptrName, one};
  CHECK(primec::ir_lowerer::inferPointerTargetValueKind(
            plusCall,
            locals,
            [](const primec::Expr &expr, std::string &builtinName) {
              return primec::ir_lowerer::getBuiltinOperatorName(expr, builtinName);
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr referencePlusCall;
  referencePlusCall.kind = primec::Expr::Kind::Call;
  referencePlusCall.name = "plus";
  referencePlusCall.args = {refName, one};
  CHECK(primec::ir_lowerer::inferPointerTargetValueKind(
            referencePlusCall,
            locals,
            [](const primec::Expr &expr, std::string &builtinName) {
              return primec::ir_lowerer::getBuiltinOperatorName(expr, builtinName);
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Float32);

  primec::ir_lowerer::LocalInfo argsPackReferenceInfo;
  argsPackReferenceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  argsPackReferenceInfo.isArgsPack = true;
  argsPackReferenceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  argsPackReferenceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("values", argsPackReferenceInfo);

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";
  primec::Expr atCall;
  atCall.kind = primec::Expr::Kind::Call;
  atCall.name = "at";
  atCall.args = {valuesName, one};
  CHECK(primec::ir_lowerer::inferPointerTargetValueKind(
            atCall,
            locals,
            [](const primec::Expr &expr, std::string &builtinName) {
              return primec::ir_lowerer::getBuiltinOperatorName(expr, builtinName);
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::ir_lowerer::LocalInfo argsPackPointerInfo;
  argsPackPointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  argsPackPointerInfo.isArgsPack = true;
  argsPackPointerInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  argsPackPointerInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("ptrValues", argsPackPointerInfo);

  primec::Expr ptrValuesName;
  ptrValuesName.kind = primec::Expr::Kind::Name;
  ptrValuesName.name = "ptrValues";
  primec::Expr atPointerCall;
  atPointerCall.kind = primec::Expr::Kind::Call;
  atPointerCall.name = "at";
  atPointerCall.args = {ptrValuesName, one};
  CHECK(primec::ir_lowerer::inferPointerTargetValueKind(
            atPointerCall,
            locals,
            [](const primec::Expr &expr, std::string &builtinName) {
              return primec::ir_lowerer::getBuiltinOperatorName(expr, builtinName);
            }) == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_SUITE_END();
