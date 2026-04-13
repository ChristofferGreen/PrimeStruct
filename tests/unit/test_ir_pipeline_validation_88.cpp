#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer flow helpers resolve buffer load info") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr bufferLoadExpr;
  bufferLoadExpr.kind = primec::Expr::Kind::Call;
  bufferLoadExpr.name = "buffer_load";

  primec::Expr bufferNameExpr;
  bufferNameExpr.kind = primec::Expr::Kind::Name;
  bufferNameExpr.name = "buf";
  bufferLoadExpr.args.push_back(bufferNameExpr);

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;
  bufferLoadExpr.args.push_back(indexExpr);

  primec::ir_lowerer::BufferLoadInfo info;
  std::string error;
  CHECK(primec::ir_lowerer::resolveBufferLoadInfo(
      bufferLoadExpr,
      [](const primec::Expr &candidate) -> std::optional<ValueKind> {
        if (candidate.kind == primec::Expr::Kind::Name && candidate.name == "buf") {
          return ValueKind::Int64;
        }
        return std::nullopt;
      },
      [](const std::string &) { return ValueKind::Unknown; },
      [](const primec::Expr &) { return ValueKind::Int32; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.elemKind == ValueKind::Int64);
  CHECK(info.indexKind == ValueKind::Int32);

  primec::Expr inlineBufferExpr = bufferLoadExpr;
  inlineBufferExpr.args.front().kind = primec::Expr::Kind::Call;
  inlineBufferExpr.args.front().name = "buffer";
  inlineBufferExpr.args.front().templateArgs = {"f32"};
  CHECK(primec::ir_lowerer::resolveBufferLoadInfo(
      inlineBufferExpr,
      [](const primec::Expr &) -> std::optional<ValueKind> { return std::nullopt; },
      [](const std::string &typeName) {
        if (typeName == "f32") {
          return ValueKind::Float32;
        }
        return ValueKind::Unknown;
      },
      [](const primec::Expr &) { return ValueKind::UInt64; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.elemKind == ValueKind::Float32);
  CHECK(info.indexKind == ValueKind::UInt64);

  primec::Expr wrongArityExpr = bufferLoadExpr;
  wrongArityExpr.args.pop_back();
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveBufferLoadInfo(
      wrongArityExpr,
      [](const primec::Expr &) -> std::optional<ValueKind> { return std::nullopt; },
      [](const std::string &) { return ValueKind::Unknown; },
      [](const primec::Expr &) { return ValueKind::Int32; },
      info,
      error));
  CHECK(error == "buffer_load requires buffer and index");

  primec::Expr unknownBufferExpr = bufferLoadExpr;
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveBufferLoadInfo(
      unknownBufferExpr,
      [](const primec::Expr &) -> std::optional<ValueKind> { return std::nullopt; },
      [](const std::string &) { return ValueKind::Unknown; },
      [](const primec::Expr &) { return ValueKind::Int32; },
      info,
      error));
  CHECK(error == "buffer_load requires numeric/bool buffer");

  primec::Expr packedBuffersName;
  packedBuffersName.kind = primec::Expr::Kind::Name;
  packedBuffersName.name = "buffers";
  primec::Expr packedBufferIndex;
  packedBufferIndex.kind = primec::Expr::Kind::Literal;
  packedBufferIndex.intWidth = 32;
  packedBufferIndex.literalValue = 0;
  primec::Expr packedBufferAccess;
  packedBufferAccess.kind = primec::Expr::Kind::Call;
  packedBufferAccess.name = "at";
  packedBufferAccess.args = {packedBuffersName, packedBufferIndex};
  primec::Expr indexedArgsPackExpr = bufferLoadExpr;
  indexedArgsPackExpr.args.front() = packedBufferAccess;
  error.clear();
  CHECK(primec::ir_lowerer::resolveBufferLoadInfo(
      indexedArgsPackExpr,
      [](const primec::Expr &candidate) -> std::optional<ValueKind> {
        if (candidate.kind == primec::Expr::Kind::Call && candidate.name == "at" &&
            candidate.args.size() == 2 && candidate.args.front().kind == primec::Expr::Kind::Name &&
            candidate.args.front().name == "buffers") {
          return ValueKind::Int32;
        }
        return std::nullopt;
      },
      [](const std::string &) { return ValueKind::Unknown; },
      [](const primec::Expr &) { return ValueKind::Int32; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.elemKind == ValueKind::Int32);
  CHECK(info.indexKind == ValueKind::Int32);

  primec::Expr borrowedBuffersName;
  borrowedBuffersName.kind = primec::Expr::Kind::Name;
  borrowedBuffersName.name = "borrowedBuffers";
  primec::Expr borrowedBufferIndex;
  borrowedBufferIndex.kind = primec::Expr::Kind::Literal;
  borrowedBufferIndex.intWidth = 32;
  borrowedBufferIndex.literalValue = 0;
  primec::Expr borrowedBufferAccess;
  borrowedBufferAccess.kind = primec::Expr::Kind::Call;
  borrowedBufferAccess.name = "at";
  borrowedBufferAccess.args = {borrowedBuffersName, borrowedBufferIndex};
  primec::Expr borrowedBufferDeref;
  borrowedBufferDeref.kind = primec::Expr::Kind::Call;
  borrowedBufferDeref.name = "dereference";
  borrowedBufferDeref.args = {borrowedBufferAccess};
  primec::Expr borrowedArgsPackExpr = bufferLoadExpr;
  borrowedArgsPackExpr.args.front() = borrowedBufferDeref;
  error.clear();
  CHECK(primec::ir_lowerer::resolveBufferLoadInfo(
      borrowedArgsPackExpr,
      [](const primec::Expr &candidate) -> std::optional<ValueKind> {
        if (candidate.kind == primec::Expr::Kind::Call && candidate.name == "dereference" &&
            candidate.args.size() == 1 && candidate.args.front().kind == primec::Expr::Kind::Call &&
            candidate.args.front().name == "at" && candidate.args.front().args.size() == 2 &&
            candidate.args.front().args.front().kind == primec::Expr::Kind::Name &&
            candidate.args.front().args.front().name == "borrowedBuffers") {
          return ValueKind::Float32;
        }
        return std::nullopt;
      },
      [](const std::string &) { return ValueKind::Unknown; },
      [](const primec::Expr &) { return ValueKind::Int32; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.elemKind == ValueKind::Float32);
  CHECK(info.indexKind == ValueKind::Int32);

  primec::Expr pointerBuffersName;
  pointerBuffersName.kind = primec::Expr::Kind::Name;
  pointerBuffersName.name = "pointerBuffers";
  primec::Expr pointerBufferIndex;
  pointerBufferIndex.kind = primec::Expr::Kind::Literal;
  pointerBufferIndex.intWidth = 32;
  pointerBufferIndex.literalValue = 0;
  primec::Expr pointerBufferAccess;
  pointerBufferAccess.kind = primec::Expr::Kind::Call;
  pointerBufferAccess.name = "at";
  pointerBufferAccess.args = {pointerBuffersName, pointerBufferIndex};
  primec::Expr pointerBufferDeref;
  pointerBufferDeref.kind = primec::Expr::Kind::Call;
  pointerBufferDeref.name = "dereference";
  pointerBufferDeref.args = {pointerBufferAccess};
  primec::Expr pointerArgsPackExpr = bufferLoadExpr;
  pointerArgsPackExpr.args.front() = pointerBufferDeref;
  error.clear();
  CHECK(primec::ir_lowerer::resolveBufferLoadInfo(
      pointerArgsPackExpr,
      [](const primec::Expr &candidate) -> std::optional<ValueKind> {
        if (candidate.kind == primec::Expr::Kind::Call && candidate.name == "dereference" &&
            candidate.args.size() == 1 && candidate.args.front().kind == primec::Expr::Kind::Call &&
            candidate.args.front().name == "at" && candidate.args.front().args.size() == 2 &&
            candidate.args.front().args.front().kind == primec::Expr::Kind::Name &&
            candidate.args.front().args.front().name == "pointerBuffers") {
          return ValueKind::UInt64;
        }
        return std::nullopt;
      },
      [](const std::string &) { return ValueKind::Unknown; },
      [](const primec::Expr &) { return ValueKind::Int32; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.elemKind == ValueKind::UInt64);
  CHECK(info.indexKind == ValueKind::Int32);

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveBufferLoadInfo(
      bufferLoadExpr,
      [](const primec::Expr &) -> std::optional<ValueKind> { return ValueKind::Int32; },
      [](const std::string &) { return ValueKind::Unknown; },
      [](const primec::Expr &) { return ValueKind::Float32; },
      info,
      error));
  CHECK(error == "buffer_load requires integer index");
}

TEST_CASE("ir lowerer flow helpers emit buffer load calls") {
  primec::Expr bufferLoadExpr;
  bufferLoadExpr.kind = primec::Expr::Kind::Call;
  bufferLoadExpr.name = "buffer_load";

  primec::Expr targetExpr;
  targetExpr.kind = primec::Expr::Kind::Name;
  targetExpr.name = "buf";
  bufferLoadExpr.args.push_back(targetExpr);

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 2;
  bufferLoadExpr.args.push_back(indexExpr);

  std::vector<primec::IrInstruction> instructions;
  int emitStep = 0;
  int nextLocal = 40;
  CHECK(primec::ir_lowerer::emitBufferLoadCall(
      bufferLoadExpr,
      primec::ir_lowerer::LocalInfo::ValueKind::Int32,
      [&](const primec::Expr &) {
        if (emitStep == 0) {
          instructions.push_back({primec::IrOpcode::PushI64, 123});
        } else {
          instructions.push_back({primec::IrOpcode::PushI32, 7});
        }
        ++emitStep;
        return true;
      },
      [&]() { return nextLocal++; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); }));
  REQUIRE(instructions.size() == 12);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 40);
  CHECK(instructions[2].op == primec::IrOpcode::PushI32);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 41);
  CHECK(instructions[4].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[5].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[6].op == primec::IrOpcode::PushI32);
  CHECK(instructions[7].op == primec::IrOpcode::AddI32);
  CHECK(instructions[8].op == primec::IrOpcode::PushI32);
  CHECK(instructions[9].op == primec::IrOpcode::MulI32);
  CHECK(instructions[10].op == primec::IrOpcode::AddI64);
  CHECK(instructions[11].op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  emitStep = 0;
  nextLocal = 10;
  CHECK_FALSE(primec::ir_lowerer::emitBufferLoadCall(
      bufferLoadExpr,
      primec::ir_lowerer::LocalInfo::ValueKind::Int32,
      [&](const primec::Expr &) {
        ++emitStep;
        return false;
      },
      [&]() { return nextLocal++; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); }));
  CHECK(emitStep == 1);
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer flow helpers emit buffer builtin calls") {
  using Result = primec::ir_lowerer::BufferBuiltinCallEmitResult;
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "upload";

  std::vector<primec::IrInstruction> instructions;
  primec::ir_lowerer::LocalMap locals;
  std::string error = "stale";

  CHECK(primec::ir_lowerer::tryEmitBufferBuiltinCall(
            expr,
            locals,
            [](const std::string &) { return Kind::Unknown; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            [](int32_t) { return 0; },
            []() { return 0; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::NotMatched);
  CHECK(error == "stale");
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  expr.name = "buffer";
  expr.templateArgs = {"i32"};
  primec::Expr countExpr;
  countExpr.kind = primec::Expr::Kind::Literal;
  countExpr.intWidth = 32;
  countExpr.literalValue = 2;
  expr.args = {countExpr};
  int32_t nextLocal = 20;
  CHECK(primec::ir_lowerer::tryEmitBufferBuiltinCall(
            expr,
            locals,
            [](const std::string &typeName) {
              if (typeName == "i32") {
                return Kind::Int32;
              }
              return Kind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            [&](int32_t localCount) {
              const int32_t baseLocal = nextLocal;
              nextLocal += localCount;
              return baseLocal;
            },
            []() { return 0; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(nextLocal == 23);
  REQUIRE(instructions.size() == 7);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 20);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 21);
  CHECK(instructions[5].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[5].imm == 22);
  CHECK(instructions[6].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[6].imm == 20);

  instructions.clear();
  error.clear();
  expr.name = "buffer_load";
  expr.templateArgs.clear();
  primec::Expr bufferName;
  bufferName.kind = primec::Expr::Kind::Name;
  bufferName.name = "buf";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;
  expr.args = {bufferName, indexExpr};
  primec::ir_lowerer::LocalInfo bufferInfo;
  bufferInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Buffer;
  bufferInfo.valueKind = Kind::Int64;
  locals.emplace("buf", bufferInfo);
  int emitStep = 0;
  bool allocRangeCalled = false;
  int32_t nextTemp = 40;
  CHECK(primec::ir_lowerer::tryEmitBufferBuiltinCall(
            expr,
            locals,
            [](const std::string &typeName) {
              if (typeName == "i64") {
                return Kind::Int64;
              }
              return Kind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            [&](int32_t) {
              allocRangeCalled = true;
              return 0;
            },
            [&]() { return nextTemp++; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              if (emitStep == 0) {
                instructions.push_back({primec::IrOpcode::PushI64, 555});
              } else {
                instructions.push_back({primec::IrOpcode::PushI32, 3});
              }
              ++emitStep;
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK_FALSE(allocRangeCalled);
  CHECK(emitStep == 2);
  REQUIRE(instructions.size() == 12);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 40);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 41);
  CHECK(instructions.back().op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  error.clear();
  locals.clear();
  primec::ir_lowerer::LocalInfo bufferArgsInfo;
  bufferArgsInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  bufferArgsInfo.isArgsPack = true;
  bufferArgsInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Buffer;
  bufferArgsInfo.valueKind = Kind::Int32;
  locals.emplace("values", bufferArgsInfo);
  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";
  primec::Expr accessIndex;
  accessIndex.kind = primec::Expr::Kind::Literal;
  accessIndex.intWidth = 32;
  accessIndex.literalValue = 0;
  primec::Expr valuesAccess;
  valuesAccess.kind = primec::Expr::Kind::Call;
  valuesAccess.name = "at";
  valuesAccess.args = {valuesName, accessIndex};
  expr.args = {valuesAccess, indexExpr};
  emitStep = 0;
  nextTemp = 60;
  CHECK(primec::ir_lowerer::tryEmitBufferBuiltinCall(
            expr,
            locals,
            [](const std::string &) { return Kind::Unknown; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            [](int32_t) { return 0; },
            [&]() { return nextTemp++; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              if (emitStep == 0) {
                instructions.push_back({primec::IrOpcode::PushI64, 777});
              } else {
                instructions.push_back({primec::IrOpcode::PushI32, 2});
              }
              ++emitStep;
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitStep == 2);
  REQUIRE(instructions.size() == 12);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 60);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 61);
  CHECK(instructions.back().op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  error.clear();
  locals.clear();
  primec::ir_lowerer::LocalInfo borrowedBufferArgsInfo;
  borrowedBufferArgsInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  borrowedBufferArgsInfo.isArgsPack = true;
  borrowedBufferArgsInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  borrowedBufferArgsInfo.referenceToBuffer = true;
  borrowedBufferArgsInfo.valueKind = Kind::Float32;
  locals.emplace("borrowedValues", borrowedBufferArgsInfo);
  primec::Expr borrowedValuesName;
  borrowedValuesName.kind = primec::Expr::Kind::Name;
  borrowedValuesName.name = "borrowedValues";
  primec::Expr borrowedAccessIndex;
  borrowedAccessIndex.kind = primec::Expr::Kind::Literal;
  borrowedAccessIndex.intWidth = 32;
  borrowedAccessIndex.literalValue = 0;
  primec::Expr borrowedValuesAccess;
  borrowedValuesAccess.kind = primec::Expr::Kind::Call;
  borrowedValuesAccess.name = "at";
  borrowedValuesAccess.args = {borrowedValuesName, borrowedAccessIndex};
  primec::Expr borrowedValuesDeref;
  borrowedValuesDeref.kind = primec::Expr::Kind::Call;
  borrowedValuesDeref.name = "dereference";
  borrowedValuesDeref.args = {borrowedValuesAccess};
  expr.args = {borrowedValuesDeref, indexExpr};
  emitStep = 0;
  nextTemp = 80;
  CHECK(primec::ir_lowerer::tryEmitBufferBuiltinCall(
            expr,
            locals,
            [](const std::string &) { return Kind::Unknown; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            [](int32_t) { return 0; },
            [&]() { return nextTemp++; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              if (emitStep == 0) {
                instructions.push_back({primec::IrOpcode::PushI64, 999});
              } else {
                instructions.push_back({primec::IrOpcode::PushI32, 4});
              }
              ++emitStep;
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitStep == 2);
  REQUIRE(instructions.size() == 12);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 80);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 81);
  CHECK(instructions.back().op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  error.clear();
  locals.clear();
  primec::ir_lowerer::LocalInfo pointerBufferArgsInfo;
  pointerBufferArgsInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  pointerBufferArgsInfo.isArgsPack = true;
  pointerBufferArgsInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerBufferArgsInfo.pointerToBuffer = true;
  pointerBufferArgsInfo.valueKind = Kind::UInt64;
  locals.emplace("pointerValues", pointerBufferArgsInfo);
  primec::Expr pointerValuesName;
  pointerValuesName.kind = primec::Expr::Kind::Name;
  pointerValuesName.name = "pointerValues";
  primec::Expr pointerAccessIndex;
  pointerAccessIndex.kind = primec::Expr::Kind::Literal;
  pointerAccessIndex.intWidth = 32;
  pointerAccessIndex.literalValue = 0;
  primec::Expr pointerValuesAccess;
  pointerValuesAccess.kind = primec::Expr::Kind::Call;
  pointerValuesAccess.name = "at";
  pointerValuesAccess.args = {pointerValuesName, pointerAccessIndex};
  primec::Expr pointerValuesDeref;
  pointerValuesDeref.kind = primec::Expr::Kind::Call;
  pointerValuesDeref.name = "dereference";
  pointerValuesDeref.args = {pointerValuesAccess};
  expr.args = {pointerValuesDeref, indexExpr};
  emitStep = 0;
  nextTemp = 90;
  CHECK(primec::ir_lowerer::tryEmitBufferBuiltinCall(
            expr,
            locals,
            [](const std::string &) { return Kind::Unknown; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            [](int32_t) { return 0; },
            [&]() { return nextTemp++; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              if (emitStep == 0) {
                instructions.push_back({primec::IrOpcode::PushI64, 111});
              } else {
                instructions.push_back({primec::IrOpcode::PushI32, 5});
              }
              ++emitStep;
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitStep == 2);
  REQUIRE(instructions.size() == 12);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 90);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 91);
  CHECK(instructions.back().op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  error.clear();
  locals.clear();
  CHECK(primec::ir_lowerer::tryEmitBufferBuiltinCall(
            expr,
            locals,
            [](const std::string &) { return Kind::Unknown; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            [](int32_t) { return 0; },
            []() { return 0; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Error);
  CHECK(error == "buffer_load requires numeric/bool buffer");
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  locals.emplace("buf", bufferInfo);
  CHECK(primec::ir_lowerer::tryEmitBufferBuiltinCall(
            expr,
            locals,
            [](const std::string &) { return Kind::Unknown; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            [](int32_t) { return 0; },
            []() { return 70; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Error);
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer string call helpers emit literal and binding values") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto internString = [](const std::string &text) -> int32_t {
    if (text == "hello") {
      return 7;
    }
    return 9;
  };

  primec::Expr literalArg;
  literalArg.kind = primec::Expr::Kind::StringLiteral;
  literalArg.stringValue = "\"hello\"utf8";

  primec::ir_lowerer::StringCallSource source = primec::ir_lowerer::StringCallSource::None;
  int32_t stringIndex = -1;
  bool argvChecked = true;
  std::string error;

  CHECK(primec::ir_lowerer::emitLiteralOrBindingStringCallValue(
            literalArg,
            internString,
            emitInstruction,
            [](const std::string &) { return primec::ir_lowerer::StringBindingInfo{}; },
            source,
            stringIndex,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Handled);
  CHECK(source == primec::ir_lowerer::StringCallSource::TableIndex);
  CHECK(stringIndex == 7);
  CHECK(argvChecked);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 7);

  instructions.clear();
  primec::Expr nameArg;
  nameArg.kind = primec::Expr::Kind::Name;
  nameArg.name = "text";
  CHECK(primec::ir_lowerer::emitLiteralOrBindingStringCallValue(
            nameArg,
            internString,
            emitInstruction,
            [](const std::string &name) {
              primec::ir_lowerer::StringBindingInfo binding;
              if (name == "text") {
                binding.found = true;
                binding.isString = true;
                binding.localIndex = 42;
                binding.source = primec::ir_lowerer::StringCallSource::RuntimeIndex;
                binding.argvChecked = true;
              }
              return binding;
            },
            source,
            stringIndex,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Handled);
  CHECK(source == primec::ir_lowerer::StringCallSource::RuntimeIndex);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 42);
}


TEST_SUITE_END();
