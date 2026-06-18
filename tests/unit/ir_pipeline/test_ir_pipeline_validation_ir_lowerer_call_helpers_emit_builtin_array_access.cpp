#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer call helpers emit builtin array access") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Name;
  indexExpr.name = "idx";

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::IrInstruction> instructions;
  std::string error = "stale";

  primec::Expr stringLiteralTarget;
  stringLiteralTarget.kind = primec::Expr::Kind::StringLiteral;
  stringLiteralTarget.stringValue = "abc";

  CHECK_FALSE(primec::ir_lowerer::emitBuiltinArrayAccess(
      "at",
      stringLiteralTarget,
      indexExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      []() { return 1; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      []() {},
      []() {},
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error == "stale");
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  int emitExprCalls = 0;
  CHECK(primec::ir_lowerer::emitBuiltinArrayAccess(
      "at",
      stringLiteralTarget,
      indexExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndex, size_t &length) {
        stringIndex = 4;
        length = 3;
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      []() { return 7; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        instructions.push_back({primec::IrOpcode::PushI32, 1});
        return true;
      },
      []() {},
      []() {},
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(emitExprCalls == 1);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 12);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions.back().op == primec::IrOpcode::LoadStringByte);
  CHECK(instructions.back().imm == 4);

  instructions.clear();
  error.clear();
  primec::Expr runtimeStringTarget;
  runtimeStringTarget.kind = primec::Expr::Kind::Name;
  runtimeStringTarget.name = "text";
  primec::ir_lowerer::LocalInfo runtimeStringInfo;
  runtimeStringInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  runtimeStringInfo.valueKind = Kind::String;
  runtimeStringInfo.stringSource = primec::ir_lowerer::LocalInfo::StringSource::RuntimeIndex;
  locals.clear();
  locals.emplace("text", runtimeStringInfo);
  int runtimeEmitExprCalls = 0;
  int stringIndexOutOfBoundsCalls = 0;
  int nextTempLocal = 30;
  CHECK(primec::ir_lowerer::emitBuiltinArrayAccess(
      "at",
      runtimeStringTarget,
      indexExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      3,
      {},
      [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.kind == primec::Expr::Kind::Name && expr.name == "idx") {
          return Kind::Int32;
        }
        return Kind::String;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&]() { return nextTempLocal++; },
      [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        ++runtimeEmitExprCalls;
        if (expr.kind == primec::Expr::Kind::Name && expr.name == "text") {
          instructions.push_back({primec::IrOpcode::PushI64, 2});
          return true;
        }
        instructions.push_back({primec::IrOpcode::PushI32, 1});
        return true;
      },
      [&]() { ++stringIndexOutOfBoundsCalls; },
      []() {},
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error.empty());
  CHECK(runtimeEmitExprCalls == 2);
  CHECK(stringIndexOutOfBoundsCalls == 1);
  REQUIRE(instructions.size() == 25);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 2);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 30);
  CHECK(instructions[2].op == primec::IrOpcode::PushI32);
  CHECK(instructions[2].imm == 1);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 31);
  CHECK(instructions[9].op == primec::IrOpcode::LoadStringByte);
  CHECK(instructions[9].imm == 0);
  CHECK(instructions[16].op == primec::IrOpcode::LoadStringByte);
  CHECK(instructions[16].imm == 1);
  CHECK(instructions[23].op == primec::IrOpcode::LoadStringByte);
  CHECK(instructions[23].imm == 2);

  instructions.clear();
  error.clear();
  primec::Expr mapTarget;
  mapTarget.kind = primec::Expr::Kind::Name;
  mapTarget.name = "m";
  primec::ir_lowerer::LocalInfo mapInfo;
  mapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  mapInfo.keyValueKeyKind = Kind::Unknown;
  mapInfo.keyValueValueKind = Kind::Int32;
  locals.clear();
  locals.emplace("m", mapInfo);
  CHECK_FALSE(primec::ir_lowerer::emitBuiltinArrayAccess(
      "at",
      mapTarget,
      indexExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      []() { return 10; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      []() {},
      []() {},
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error.find("native backend only supports at() on numeric/bool/string arrays or vectors") !=
        std::string::npos);
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  primec::Expr vectorTarget;
  vectorTarget.kind = primec::Expr::Kind::Name;
  vectorTarget.name = "v";
  primec::ir_lowerer::LocalInfo vectorInfo;
  vectorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  vectorInfo.valueKind = Kind::Int32;
  vectorInfo.structTypeName = "/std/collections/vector/Vector__t25a78a513414c3bf";
  locals.clear();
  locals.emplace("v", vectorInfo);
  const auto vectorTargetInfo = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(
      vectorTarget,
      locals);
  CHECK(vectorTargetInfo.isArrayOrVectorTarget);
  CHECK(vectorTargetInfo.isVectorTarget);
  CHECK(vectorTargetInfo.structTypeName ==
        "/std/collections/vector/Vector__t25a78a513414c3bf");
  int arrayIndexOutOfBoundsCalls = 0;
  int nextLocal = 20;
  auto emitVectorAccessExpr = [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
    if (expr.kind == primec::Expr::Kind::Name && expr.name == "v") {
      instructions.push_back({primec::IrOpcode::PushI64, 100});
      return true;
    }
    instructions.push_back({primec::IrOpcode::PushI32, 2});
    return true;
  };
  CHECK(primec::ir_lowerer::emitBuiltinArrayAccess(
      "at",
      vectorTarget,
      indexExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&]() { return nextLocal++; },
      emitVectorAccessExpr,
      []() {},
      [&]() { ++arrayIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error.empty());
  CHECK(arrayIndexOutOfBoundsCalls == 2);
  REQUIRE(instructions.size() == 28);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 100);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 20);
  CHECK(instructions[2].op == primec::IrOpcode::PushI32);
  CHECK(instructions[2].imm == 2);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 21);
  CHECK(instructions[4].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[4].imm == 20);
  CHECK(instructions[5].op == primec::IrOpcode::PushI64);
  CHECK(instructions[5].imm == 16);
  CHECK(instructions[6].op == primec::IrOpcode::AddI64);
  CHECK(instructions[7].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[8].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[8].imm == 22);
  CHECK(instructions[9].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[9].imm == 21);
  CHECK(instructions[10].op == primec::IrOpcode::PushI32);
  CHECK(instructions[10].imm == 0);
  CHECK(instructions[11].op == primec::IrOpcode::CmpLtI32);
  CHECK(instructions[13].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[13].imm == 21);
  CHECK(instructions[14].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[14].imm == 22);
  CHECK(instructions[15].op == primec::IrOpcode::CmpGeI32);
  CHECK(instructions[17].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[17].imm == 20);
  CHECK(instructions[18].op == primec::IrOpcode::PushI64);
  CHECK(instructions[18].imm == 48);
  CHECK(instructions[19].op == primec::IrOpcode::AddI64);
  CHECK(instructions[20].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[21].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[21].imm == 23);
  CHECK(instructions[22].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[22].imm == 23);
  CHECK(instructions[23].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[23].imm == 21);
  CHECK(instructions[24].op == primec::IrOpcode::PushI32);
  CHECK(instructions[24].imm == primec::IrSlotBytesI32);
  CHECK(instructions[25].op == primec::IrOpcode::MulI32);
  CHECK(instructions[26].op == primec::IrOpcode::AddI64);
  CHECK(instructions.back().op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  error.clear();
  arrayIndexOutOfBoundsCalls = 0;
  nextLocal = 20;
  CHECK(primec::ir_lowerer::emitBuiltinArrayAccess(
      "at_unsafe",
      vectorTarget,
      indexExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&]() { return nextLocal++; },
      emitVectorAccessExpr,
      []() {},
      [&]() { ++arrayIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error.empty());
  CHECK(arrayIndexOutOfBoundsCalls == 2);
  REQUIRE(instructions.size() == 28);
  CHECK(instructions[4].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[4].imm == 20);
  CHECK(instructions[5].op == primec::IrOpcode::PushI64);
  CHECK(instructions[5].imm == primec::IrSlotBytesI32);
  CHECK(instructions[6].op == primec::IrOpcode::AddI64);
  CHECK(instructions[7].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[8].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[8].imm == 22);
  CHECK(instructions[9].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[9].imm == 21);
  CHECK(instructions[10].op == primec::IrOpcode::PushI32);
  CHECK(instructions[10].imm == 0);
  CHECK(instructions[11].op == primec::IrOpcode::CmpLtI32);
  CHECK(instructions[13].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[13].imm == 21);
  CHECK(instructions[14].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[14].imm == 22);
  CHECK(instructions[15].op == primec::IrOpcode::CmpGeI32);
  CHECK(instructions[17].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[17].imm == 20);
  CHECK(instructions[18].op == primec::IrOpcode::PushI64);
  CHECK(instructions[18].imm == 48);
  CHECK(instructions[19].op == primec::IrOpcode::AddI64);
  CHECK(instructions[20].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[21].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[21].imm == 23);
  CHECK(instructions[22].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[22].imm == 23);
  CHECK(instructions[23].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[23].imm == 21);
  CHECK(instructions[24].op == primec::IrOpcode::PushI32);
  CHECK(instructions[24].imm == primec::IrSlotBytesI32);
  CHECK(instructions[25].op == primec::IrOpcode::MulI32);
  CHECK(instructions[26].op == primec::IrOpcode::AddI64);
  CHECK(instructions.back().op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  error.clear();
  arrayIndexOutOfBoundsCalls = 0;
  nextLocal = 20;
  CHECK(primec::ir_lowerer::emitBuiltinArrayAccess(
      "at_unsafe",
      vectorTarget,
      indexExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&]() { return nextLocal++; },
      emitVectorAccessExpr,
      []() {},
      [&]() { ++arrayIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error.empty());
  CHECK(arrayIndexOutOfBoundsCalls == 2);
  REQUIRE(instructions.size() == 28);
  CHECK(instructions[4].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[4].imm == 20);
  CHECK(instructions[5].op == primec::IrOpcode::PushI64);
  CHECK(instructions[5].imm == 32);
  CHECK(instructions[6].op == primec::IrOpcode::AddI64);
  CHECK(instructions[7].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[8].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[8].imm == 22);
  CHECK(instructions[9].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[9].imm == 21);
  CHECK(instructions[10].op == primec::IrOpcode::PushI32);
  CHECK(instructions[10].imm == 0);
  CHECK(instructions[11].op == primec::IrOpcode::CmpLtI32);
  CHECK(instructions[13].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[13].imm == 21);
  CHECK(instructions[14].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[14].imm == 22);
  CHECK(instructions[15].op == primec::IrOpcode::CmpGeI32);
  CHECK(instructions[17].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[17].imm == 20);
  CHECK(instructions[18].op == primec::IrOpcode::PushI64);
  CHECK(instructions[18].imm == 48);
  CHECK(instructions[19].op == primec::IrOpcode::AddI64);
  CHECK(instructions[20].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[21].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[21].imm == 23);
  CHECK(instructions[22].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[22].imm == 23);
  CHECK(instructions.back().op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  error.clear();
  primec::Expr argsPackTarget;
  argsPackTarget.kind = primec::Expr::Kind::Name;
  argsPackTarget.name = "packs";
  primec::ir_lowerer::LocalInfo argsPackInfo;
  argsPackInfo.isArgsPack = true;
  argsPackInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  argsPackInfo.referenceToVector = true;
  locals.clear();
  locals.emplace("packs", argsPackInfo);
  int argsPackEmitExprCalls = 0;
  int argsPackArrayIndexOutOfBoundsCalls = 0;
  int argsPackNextLocal = 40;
  CHECK(primec::ir_lowerer::emitBuiltinArrayAccess(
      "at",
      argsPackTarget,
      indexExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&]() { return argsPackNextLocal++; },
      [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        ++argsPackEmitExprCalls;
        if (expr.kind == primec::Expr::Kind::Name && expr.name == "packs") {
          instructions.push_back({primec::IrOpcode::LoadLocal, 12});
          return true;
        }
        instructions.push_back({primec::IrOpcode::PushI32, 1});
        return true;
      },
      []() {},
      [&]() { ++argsPackArrayIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error.empty());
  CHECK(argsPackEmitExprCalls == 2);
  CHECK(argsPackArrayIndexOutOfBoundsCalls == 2);
  REQUIRE(instructions.size() >= 6);
  CHECK(instructions.front().op == primec::IrOpcode::LoadLocal);
  CHECK(instructions.back().op == primec::IrOpcode::LoadIndirect);
}

TEST_CASE("ir lowerer builtin array access prefers semantic target facts") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::SemanticProgram semanticProgram;
  const auto addBindingFact = [&](uint64_t semanticNodeId,
                                  const std::string &bindingTypeText) {
    primec::SemanticProgramBindingFact fact;
    fact.semanticNodeId = semanticNodeId;
    fact.bindingTypeText = bindingTypeText;
    fact.bindingTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, bindingTypeText);
    const size_t factIndex = semanticProgram.bindingFacts.size();
    semanticProgram.bindingFacts.push_back(fact);
    semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
        semanticNodeId, factIndex);
  };
  addBindingFact(7201, "vector<i32>");
  addBindingFact(7202, "i32");
  const primec::ir_lowerer::SemanticProductIndex semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Name;
  indexExpr.name = "idx";

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo staleScalarInfo;
  staleScalarInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleScalarInfo.valueKind = Kind::Int32;
  locals.emplace("values", staleScalarInfo);

  primec::Expr semanticVectorTarget;
  semanticVectorTarget.kind = primec::Expr::Kind::Name;
  semanticVectorTarget.name = "values";
  semanticVectorTarget.semanticNodeId = 7201;

  std::vector<primec::IrInstruction> instructions;
  int nextLocal = 30;
  int emitExprCalls = 0;
  std::string error;
  CHECK(primec::ir_lowerer::emitBuiltinArrayAccess(
      "at",
      semanticVectorTarget,
      indexExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
        return false;
      },
      0,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return Kind::Int32;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&]() { return nextLocal++; },
      [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        if (expr.kind == primec::Expr::Kind::Name && expr.name == "values") {
          instructions.push_back({primec::IrOpcode::PushI64, 100});
          return true;
        }
        instructions.push_back({primec::IrOpcode::PushI32, 1});
        return true;
      },
      []() {},
      []() {},
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error,
      &semanticProgram,
      &semanticIndex));
  CHECK(error.empty());
  CHECK(emitExprCalls == 2);
  REQUIRE(!instructions.empty());
  CHECK(instructions.front().op == primec::IrOpcode::PushI64);
  CHECK(instructions.back().op == primec::IrOpcode::LoadIndirect);

  primec::ir_lowerer::LocalInfo staleVectorInfo;
  staleVectorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  staleVectorInfo.valueKind = Kind::Int32;
  locals.clear();
  locals.emplace("staleVector", staleVectorInfo);

  primec::Expr semanticScalarTarget;
  semanticScalarTarget.kind = primec::Expr::Kind::Name;
  semanticScalarTarget.name = "staleVector";
  semanticScalarTarget.semanticNodeId = 7202;

  instructions.clear();
  error.clear();
  nextLocal = 40;
  CHECK_FALSE(primec::ir_lowerer::emitBuiltinArrayAccess(
      "at",
      semanticScalarTarget,
      indexExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
        return false;
      },
      0,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return Kind::Int32;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&]() { return nextLocal++; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI64, 100});
        return true;
      },
      []() {},
      []() {},
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error,
      &semanticProgram,
      &semanticIndex));
  CHECK(error.find("native backend only supports at() on numeric/bool/string arrays or vectors") !=
        std::string::npos);
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer builtin map access prefers semantic target facts") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::SemanticProgram semanticProgram;
  const auto addCollectionFact = [&](uint64_t semanticNodeId,
                                     const std::string &family,
                                     const std::string &keyType,
                                     const std::string &valueType) {
    primec::SemanticProgramCollectionSpecialization fact;
    fact.semanticNodeId = semanticNodeId;
    fact.collectionFamily = family;
    fact.collectionFamilyId =
        primec::semanticProgramInternCallTargetString(semanticProgram, family);
    fact.keyTypeText = keyType;
    fact.keyTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, keyType);
    fact.valueTypeText = valueType;
    fact.valueTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, valueType);
    const size_t factIndex = semanticProgram.collectionSpecializations.size();
    semanticProgram.collectionSpecializations.push_back(fact);
    semanticProgram.publishedRoutingLookups.collectionSpecializationIndicesByExpr
        .insert_or_assign(semanticNodeId, factIndex);
  };
  const auto addBindingFact = [&](uint64_t semanticNodeId,
                                  const std::string &bindingTypeText) {
    primec::SemanticProgramBindingFact fact;
    fact.semanticNodeId = semanticNodeId;
    fact.bindingTypeText = bindingTypeText;
    fact.bindingTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, bindingTypeText);
    const size_t factIndex = semanticProgram.bindingFacts.size();
    semanticProgram.bindingFacts.push_back(fact);
    semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
        semanticNodeId, factIndex);
  };
  addCollectionFact(7301, "map", "i32", "f64");
  addBindingFact(7302, "i32");
  const primec::ir_lowerer::SemanticProductIndex semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Name;
  indexExpr.name = "key";

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo staleScalarInfo;
  staleScalarInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleScalarInfo.valueKind = Kind::Int32;
  locals.emplace("items", staleScalarInfo);

  primec::Expr semanticMapTarget;
  semanticMapTarget.kind = primec::Expr::Kind::Name;
  semanticMapTarget.name = "items";
  semanticMapTarget.semanticNodeId = 7301;

  std::vector<primec::IrInstruction> instructions;
  int nextLocal = 50;
  int emitExprCalls = 0;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::emitBuiltinArrayAccess(
      "at",
      semanticMapTarget,
      indexExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
        return false;
      },
      0,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return Kind::Int32;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&]() { return nextLocal++; },
      [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        if (expr.kind == primec::Expr::Kind::Name && expr.name == "items") {
          instructions.push_back({primec::IrOpcode::PushI64, 200});
          return true;
        }
        instructions.push_back({primec::IrOpcode::PushI32, 2});
        return true;
      },
      []() {},
      []() {},
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error,
      &semanticProgram,
      &semanticIndex));
  CHECK(error.find("native backend only supports at() on numeric/bool/string arrays or vectors") !=
        std::string::npos);
  CHECK(emitExprCalls == 0);
  CHECK(instructions.empty());

  primec::ir_lowerer::LocalInfo staleMapInfo;
  staleMapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleMapInfo.keyValueKeyKind = Kind::Int32;
  staleMapInfo.keyValueValueKind = Kind::Int32;
  locals.clear();
  locals.emplace("staleMap", staleMapInfo);

  primec::Expr semanticScalarTarget;
  semanticScalarTarget.kind = primec::Expr::Kind::Name;
  semanticScalarTarget.name = "staleMap";
  semanticScalarTarget.semanticNodeId = 7302;

  instructions.clear();
  error.clear();
  nextLocal = 60;
  emitExprCalls = 0;
  CHECK_FALSE(primec::ir_lowerer::emitBuiltinArrayAccess(
      "at",
      semanticScalarTarget,
      indexExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
        return false;
      },
      0,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return Kind::Int32;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&]() { return nextLocal++; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        instructions.push_back({primec::IrOpcode::PushI64, 200});
        return true;
      },
      []() {},
      []() {},
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error,
      &semanticProgram,
      &semanticIndex));
  CHECK(error.find("native backend only supports at() on numeric/bool/string arrays or vectors") !=
        std::string::npos);
  CHECK(emitExprCalls == 0);
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer call helpers validate non literal string access target") {
  using Result = primec::ir_lowerer::NonLiteralStringAccessTargetResult;
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  std::string error = "stale";

  primec::Expr stringLiteralTarget;
  stringLiteralTarget.kind = primec::Expr::Kind::StringLiteral;
  stringLiteralTarget.stringValue = "abc";
  CHECK(primec::ir_lowerer::validateNonLiteralStringAccessTarget(
            stringLiteralTarget,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Unknown; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            error) == Result::Stop);
  CHECK(error == "stale");

  primec::ir_lowerer::LocalInfo stringLocalInfo;
  stringLocalInfo.valueKind = Kind::String;
  locals.emplace("s", stringLocalInfo);
  primec::Expr stringNameTarget;
  stringNameTarget.kind = primec::Expr::Kind::Name;
  stringNameTarget.name = "s";
  error.clear();
  CHECK(primec::ir_lowerer::validateNonLiteralStringAccessTarget(
            stringNameTarget,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Unknown; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            error) == Result::Error);
  CHECK(error == "native backend only supports indexing into string literals or string bindings");

  error = "unchanged";
  CHECK(primec::ir_lowerer::validateNonLiteralStringAccessTarget(
            stringNameTarget,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            error) == Result::Continue);
  CHECK(error == "unchanged");

  primec::SemanticProgram semanticProgram;
  const primec::SymbolId scalarTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  primec::SemanticProgramBindingFact scalarFact;
  scalarFact.semanticNodeId = 7101;
  scalarFact.bindingTypeText = "string";
  scalarFact.bindingTypeTextId = scalarTypeId;
  semanticProgram.bindingFacts.push_back(scalarFact);
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      scalarFact.semanticNodeId, 0);
  const primec::SymbolId stringTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "string");
  primec::SemanticProgramBindingFact stringFact;
  stringFact.semanticNodeId = 7102;
  stringFact.bindingTypeText = "i32";
  stringFact.bindingTypeTextId = stringTypeId;
  semanticProgram.bindingFacts.push_back(stringFact);
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      stringFact.semanticNodeId, 1);
  const primec::ir_lowerer::SemanticProductIndex semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  stringNameTarget.semanticNodeId = scalarFact.semanticNodeId;
  error = "unchanged";
  CHECK(primec::ir_lowerer::classifyAccessTargetSemanticStringKind(
            stringNameTarget, &semanticProgram, &semanticIndex) ==
        primec::ir_lowerer::SemanticStringAccessTargetKind::NonString);
  CHECK(primec::ir_lowerer::validateNonLiteralStringAccessTarget(
            stringNameTarget,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Unknown; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            error,
            &semanticProgram,
            &semanticIndex) == Result::Continue);
  CHECK(error == "unchanged");

  stringNameTarget.semanticNodeId = stringFact.semanticNodeId;
  error.clear();
  CHECK(primec::ir_lowerer::classifyAccessTargetSemanticStringKind(
            stringNameTarget, &semanticProgram, &semanticIndex) ==
        primec::ir_lowerer::SemanticStringAccessTargetKind::String);
  CHECK(primec::ir_lowerer::validateNonLiteralStringAccessTarget(
            stringNameTarget,
            primec::ir_lowerer::LocalMap{},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Unknown; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            error,
            &semanticProgram,
            &semanticIndex) == Result::Error);
  CHECK(error == "native backend only supports indexing into string literals or string bindings");

  error.clear();
  CHECK(primec::ir_lowerer::validateNonLiteralStringAccessTarget(
            stringNameTarget,
            primec::ir_lowerer::LocalMap{},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::String; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            error) == Result::Error);
  CHECK(error == "native backend only supports indexing into string literals or string bindings");

  primec::Expr entryArgsTarget;
  entryArgsTarget.kind = primec::Expr::Kind::Name;
  entryArgsTarget.name = "argv";
  error.clear();
  CHECK(primec::ir_lowerer::validateNonLiteralStringAccessTarget(
            entryArgsTarget,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Unknown; },
            [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) { return expr.name == "argv"; },
            error) == Result::Error);
  CHECK(error == "native backend only supports entry argument indexing in print calls or string bindings");

  primec::Expr otherTarget;
  otherTarget.kind = primec::Expr::Kind::Name;
  otherTarget.name = "arr";
  error = "unchanged";
  CHECK(primec::ir_lowerer::validateNonLiteralStringAccessTarget(
            otherTarget,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Unknown; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            error) == Result::Continue);
  CHECK(error == "unchanged");
}

TEST_CASE("ir lowerer call helpers key/value key compare opcode selection") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;
  using primec::IrOpcode;

  CHECK(primec::ir_lowerer::keyValueKeyCompareOpcode(Kind::Int32) == IrOpcode::CmpEqI32);
  CHECK(primec::ir_lowerer::keyValueKeyCompareOpcode(Kind::Bool) == IrOpcode::CmpEqI32);
  CHECK(primec::ir_lowerer::keyValueKeyCompareOpcode(Kind::Int64) == IrOpcode::CmpEqI64);
  CHECK(primec::ir_lowerer::keyValueKeyCompareOpcode(Kind::UInt64) == IrOpcode::CmpEqI64);
  CHECK(primec::ir_lowerer::keyValueKeyCompareOpcode(Kind::Float32) == IrOpcode::CmpEqF32);
  CHECK(primec::ir_lowerer::keyValueKeyCompareOpcode(Kind::Float64) == IrOpcode::CmpEqF64);
}

TEST_CASE("ir lowerer call helpers resolve map lookup string keys") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;
  using Result = primec::ir_lowerer::KeyValueLookupStringKeyResult;

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Name;
  keyExpr.name = "k";
  primec::ir_lowerer::LocalMap locals;
  std::string error;
  int32_t stringIndex = -1;

  CHECK(primec::ir_lowerer::tryResolveKeyValueLookupStringKey(
            Kind::Int32,
            keyExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::String; },
            stringIndex,
            error) == Result::NotHandled);

  CHECK(primec::ir_lowerer::tryResolveKeyValueLookupStringKey(
            Kind::String,
            keyExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::String; },
            stringIndex,
            error) == Result::NotHandled);
  CHECK(error.empty());

  error.clear();
  stringIndex = -1;
  CHECK(primec::ir_lowerer::tryResolveKeyValueLookupStringKey(
            Kind::String,
            keyExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &indexOut, size_t &lengthOut) {
              indexOut = 99;
              lengthOut = 4;
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            stringIndex,
            error) == Result::NotHandled);
  CHECK(stringIndex == -1);
  CHECK(error.empty());

  error.clear();
  CHECK(primec::ir_lowerer::tryResolveKeyValueLookupStringKey(
            Kind::String,
            keyExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &indexOut, size_t &lengthOut) {
              indexOut = 12;
              lengthOut = 6;
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Unknown; },
            stringIndex,
            error) == Result::Resolved);
  CHECK(stringIndex == 12);
  CHECK(error.empty());

  error.clear();
  CHECK(primec::ir_lowerer::tryResolveKeyValueLookupStringKey(
            Kind::String,
            keyExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &indexOut, size_t &lengthOut) {
              indexOut = 17;
              lengthOut = 3;
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::String; },
            stringIndex,
            error) == Result::Resolved);
  CHECK(stringIndex == 17);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer call helpers emit map lookup string key locals") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;
  using Result = primec::ir_lowerer::KeyValueLookupKeyLocalEmitResult;

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Name;
  keyExpr.name = "k";
  primec::ir_lowerer::LocalMap locals;
  std::string error;
  int32_t pushed = -1;
  int32_t stored = -1;

  CHECK(primec::ir_lowerer::tryEmitKeyValueLookupStringKeyLocal(
            Kind::Int64,
            keyExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::String; },
            [&](int32_t imm) { pushed = imm; },
            [&](int32_t local) { stored = local; },
            4,
            error) == Result::NotHandled);

  CHECK(primec::ir_lowerer::tryEmitKeyValueLookupStringKeyLocal(
            Kind::String,
            keyExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::String; },
            [&](int32_t imm) { pushed = imm; },
            [&](int32_t local) { stored = local; },
            4,
            error) == Result::NotHandled);
  CHECK(error.empty());

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitKeyValueLookupStringKeyLocal(
            Kind::String,
            keyExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &indexOut, size_t &lengthOut) {
              indexOut = 23;
              lengthOut = 8;
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::String; },
            [&](int32_t imm) { pushed = imm; },
            [&](int32_t local) { stored = local; },
            9,
            error) == Result::Emitted);
  CHECK(pushed == 23);
  CHECK(stored == 9);
}

TEST_CASE("ir lowerer call helpers emit map lookup non-string key locals") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Name;
  keyExpr.name = "k";
  primec::ir_lowerer::LocalMap locals;
  std::string error;
  int32_t stored = -1;

  CHECK_FALSE(primec::ir_lowerer::emitKeyValueLookupNonStringKeyLocal(
      Kind::String,
      keyExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Unknown; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [&](int32_t local) { stored = local; },
      3,
      error));
  CHECK(error == "native backend requires map lookup key type to match map key type");

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitKeyValueLookupNonStringKeyLocal(
      Kind::Int32,
      keyExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Unknown; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [&](int32_t local) { stored = local; },
      4,
      error));
  CHECK(error == "native backend requires map lookup key type to match map key type");

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitKeyValueLookupNonStringKeyLocal(
      Kind::Int64,
      keyExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Float64; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [&](int32_t local) { stored = local; },
      5,
      error));
  CHECK(error == "native backend requires map lookup key type to match map key type");

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitKeyValueLookupNonStringKeyLocal(
      Kind::Int32,
      keyExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&](int32_t local) { stored = local; },
      6,
      error));

  error.clear();
  CHECK(primec::ir_lowerer::emitKeyValueLookupNonStringKeyLocal(
      Kind::Int32,
      keyExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [&](int32_t local) { stored = local; },
      7,
      error));
  CHECK(stored == 7);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer call helpers emit map lookup target pointer local") {
  primec::Expr targetExpr;
  targetExpr.kind = primec::Expr::Kind::Name;
  targetExpr.name = "values";

  primec::ir_lowerer::LocalMap locals;
  locals[targetExpr.name] = primec::ir_lowerer::LocalInfo{};

  int nextLocal = 40;
  int stored = -1;
  primec::Expr capturedExpr;
  bool emitCalled = false;
  int32_t ptrLocal = -1;
  CHECK(primec::ir_lowerer::emitKeyValueLookupTargetPointerLocal(
      targetExpr,
      locals,
      [&]() { return nextLocal++; },
      [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &localMap) {
        emitCalled = true;
        capturedExpr = expr;
        return localMap.count("values") == 1;
      },
      [&](int32_t localIndex) { stored = localIndex; },
      ptrLocal));
  CHECK(emitCalled);
  CHECK(capturedExpr.kind == primec::Expr::Kind::Name);
  CHECK(capturedExpr.name == "values");
  CHECK(ptrLocal == 40);
  CHECK(stored == 40);

  stored = -1;
  emitCalled = false;
  CHECK_FALSE(primec::ir_lowerer::emitKeyValueLookupTargetPointerLocal(
      targetExpr,
      locals,
      [&]() { return nextLocal++; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        emitCalled = true;
        return false;
      },
      [&](int32_t localIndex) { stored = localIndex; },
      ptrLocal));
  CHECK(emitCalled);
  CHECK(ptrLocal == 41);
  CHECK(stored == -1);
}

TEST_CASE("ir lowerer call helpers emit map lookup key local") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Name;
  keyExpr.name = "k";
  primec::ir_lowerer::LocalMap locals;
  locals["k"] = primec::ir_lowerer::LocalInfo{};

  int nextLocal = 50;
  int pushed = -1;
  int stored = -1;
  bool inferCalled = false;
  bool emitCalled = false;
  int32_t keyLocal = -1;
  std::string error;

  CHECK(primec::ir_lowerer::emitKeyValueLookupKeyLocal(
      Kind::String,
      keyExpr,
      locals,
      [&]() { return nextLocal++; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndexOut, size_t &lengthOut) {
        stringIndexOut = 13;
        lengthOut = 2;
        return true;
      },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        inferCalled = true;
        return Kind::String;
      },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        emitCalled = true;
        return true;
      },
      [&](int32_t imm) { pushed = imm; },
      [&](int32_t local) { stored = local; },
      keyLocal,
      error));
  CHECK(keyLocal == 50);
  CHECK(pushed == 13);
  CHECK(stored == 50);
  CHECK(inferCalled);
  CHECK_FALSE(emitCalled);
  CHECK(error.empty());

  pushed = -1;
  stored = -1;
  inferCalled = false;
  emitCalled = false;
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitKeyValueLookupKeyLocal(
      Kind::String,
      keyExpr,
      locals,
      [&]() { return nextLocal++; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndexOut, size_t &lengthOut) {
        stringIndexOut = 21;
        lengthOut = 5;
        return true;
      },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        inferCalled = true;
        return Kind::Int32;
      },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        emitCalled = true;
        return true;
      },
      [&](int32_t imm) { pushed = imm; },
      [&](int32_t local) { stored = local; },
      keyLocal,
      error));
  CHECK(keyLocal == 51);
  CHECK(pushed == -1);
  CHECK(stored == -1);
  CHECK(inferCalled);
  CHECK_FALSE(emitCalled);
  CHECK(error == "native backend requires map lookup key type to match map key type");

  pushed = -1;
  stored = -1;
  inferCalled = false;
  emitCalled = false;
  error.clear();
  CHECK(primec::ir_lowerer::emitKeyValueLookupKeyLocal(
      Kind::Int32,
      keyExpr,
      locals,
      [&]() { return nextLocal++; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        inferCalled = true;
        return Kind::Int32;
      },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        emitCalled = true;
        return true;
      },
      [&](int32_t imm) { pushed = imm; },
      [&](int32_t local) { stored = local; },
      keyLocal,
      error));
  CHECK(keyLocal == 52);
  CHECK(pushed == -1);
  CHECK(stored == 52);
  CHECK(inferCalled);
  CHECK(emitCalled);
  CHECK(error.empty());

  pushed = -1;
  stored = -1;
  inferCalled = false;
  emitCalled = false;
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitKeyValueLookupKeyLocal(
      Kind::String,
      keyExpr,
      locals,
      [&]() { return nextLocal++; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        inferCalled = true;
        return Kind::Int32;
      },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        emitCalled = true;
        return true;
      },
      [&](int32_t imm) { pushed = imm; },
      [&](int32_t local) { stored = local; },
      keyLocal,
      error));
  CHECK(keyLocal == 53);
  CHECK(pushed == -1);
  CHECK(stored == -1);
  CHECK(inferCalled);
  CHECK_FALSE(emitCalled);
  CHECK(error == "native backend requires map lookup key type to match map key type");
}


TEST_SUITE_END();
