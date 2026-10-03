#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer count access helpers emit count access calls") {
  using Result = primec::ir_lowerer::CountAccessCallEmitResult;

  primec::Expr targetName;
  targetName.kind = primec::Expr::Kind::Name;
  targetName.name = "values";

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "noop";
  callExpr.args = {targetName};

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::IrInstruction> instructions;
  std::string error = "stale";

  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::NotHandled);
  CHECK(error == "stale");
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  callExpr.name = "count";
  callExpr.args = {targetName};
  int arrayEmitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++arrayEmitExprCalls;
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(arrayEmitExprCalls == 0);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::PushArgc);

  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++arrayEmitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI32, 9});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(arrayEmitExprCalls == 1);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);

  primec::ir_lowerer::LocalMap vectorLocals;
  primec::ir_lowerer::LocalInfo vectorInfo;
  vectorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  vectorLocals.emplace("values", vectorInfo);

  instructions.clear();
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            vectorLocals,
            [](const primec::Expr &candidate, const primec::ir_lowerer::LocalMap &candidateLocals) {
              return primec::ir_lowerer::isArrayCountCall(candidate, candidateLocals, false, "argv");
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](primec::IrOpcode, uint64_t) {},
            error) == Result::NotHandled);
  CHECK(error == "stale");
  CHECK(instructions.empty());

  primec::ir_lowerer::LocalMap primitiveArgsLocals;
  primec::ir_lowerer::LocalInfo primitiveArgsInfo;
  primitiveArgsInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  primitiveArgsInfo.isArgsPack = true;
  primitiveArgsInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Value;
  primitiveArgsInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  primitiveArgsInfo.index = 4;
  primitiveArgsInfo.argsPackElementCount = 0;
  primitiveArgsLocals.emplace("values", primitiveArgsInfo);

  instructions.clear();
  error.clear();
  callExpr.name = "count";
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            primitiveArgsLocals,
            [](const primec::Expr &candidate, const primec::ir_lowerer::LocalMap &candidateLocals) {
              return primec::ir_lowerer::isArrayCountCall(candidate, candidateLocals, false, "argv");
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 4);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);

  primec::ir_lowerer::LocalMap experimentalVectorLocals;
  primec::ir_lowerer::LocalInfo experimentalVectorInfo;
  experimentalVectorInfo.index = 3;
  experimentalVectorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  experimentalVectorInfo.valueKind =
      primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  experimentalVectorInfo.structTypeName =
      "/std/collections/vector/Vector__t25a78a513414c3bf";
  experimentalVectorLocals.emplace("values", experimentalVectorInfo);

  instructions.clear();
  error.clear();
  callExpr.name = "/std/collections/experimental_vector/vectorCount";
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            experimentalVectorLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::NotHandled);
  CHECK(error.empty());
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  callExpr.name = "/std/collections/vector/vectorCount__t25a78a513414c3bf";
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            experimentalVectorLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK_EQ(error, "");
  REQUIRE(instructions.size() == 4);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 3);
  CHECK(instructions[1].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].imm == 16);
  CHECK(instructions[2].op == primec::IrOpcode::AddI64);
  CHECK(instructions[3].op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  error.clear();
  callExpr.name = "/std/collections/vector/vectorCapacity__t25a78a513414c3bf";
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            experimentalVectorLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 4);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 3);
  CHECK(instructions[1].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].imm == 32);
  CHECK(instructions[2].op == primec::IrOpcode::AddI64);
  CHECK(instructions[3].op == primec::IrOpcode::LoadIndirect);

  primec::ir_lowerer::LocalMap experimentalSoaVectorLocals;
  primec::ir_lowerer::LocalInfo experimentalSoaVectorInfo;
  experimentalSoaVectorInfo.index = 5;
  experimentalSoaVectorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  experimentalSoaVectorInfo.structTypeName =
      "/std/collections/soa/SoaVector__t25a78a513414c3bf";
  experimentalSoaVectorLocals.emplace("values", experimentalSoaVectorInfo);

  instructions.clear();
  error.clear();
  callExpr.name = "/std/collections/soa/soaVectorCount";
  callExpr.isMethodCall = false;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            experimentalSoaVectorLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::NotHandled);
  CHECK(error.empty());
  CHECK(instructions.empty());

  instructions.clear();
  error = "stale";
  callExpr.name = "field_count";
  callExpr.isMethodCall = true;
  int fieldCountEmitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            experimentalVectorLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++fieldCountEmitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 21});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::NotHandled);
  CHECK(fieldCountEmitExprCalls == 0);
  CHECK(error == "stale");
  CHECK(instructions.empty());

  instructions.clear();
  error = "stale";
  callExpr.name = "field_capacity";
  int fieldCapacityEmitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            experimentalVectorLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++fieldCapacityEmitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 21});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::NotHandled);
  CHECK(fieldCapacityEmitExprCalls == 0);
  CHECK(error == "stale");
  CHECK(instructions.empty());

  instructions.clear();
  error = "stale";
  callExpr.name = "field_count";
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::NotHandled);
  CHECK(error == "stale");
  CHECK(instructions.empty());

  primec::ir_lowerer::LocalMap soaStorageLocals;
  primec::ir_lowerer::LocalInfo soaColumnInfo;
  soaColumnInfo.index = 4;
  soaColumnInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  soaColumnInfo.structTypeName =
      "/std/collections/soa_storage/SoaColumn__ti32";
  soaStorageLocals.emplace("values", soaColumnInfo);

  instructions.clear();
  error.clear();
  callExpr.name = "field_count";
  callExpr.isMethodCall = true;
  int soaFieldCountEmitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            soaStorageLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++soaFieldCountEmitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 21});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(soaFieldCountEmitExprCalls == 0);
  REQUIRE(instructions.size() == 4);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 4);
  CHECK(instructions[1].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].imm == primec::IrSlotBytes);
  CHECK(instructions[2].op == primec::IrOpcode::AddI64);
  CHECK(instructions[3].op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  error.clear();
  callExpr.name = "field_capacity";
  int soaFieldCapacityEmitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            soaStorageLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++soaFieldCapacityEmitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 21});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(soaFieldCapacityEmitExprCalls == 0);
  REQUIRE(instructions.size() == 4);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 4);
  CHECK(instructions[1].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].imm == primec::IrSlotBytes * 2);
  CHECK(instructions[2].op == primec::IrOpcode::AddI64);
  CHECK(instructions[3].op == primec::IrOpcode::LoadIndirect);

  primec::ir_lowerer::LocalMap genericSoaStorageLocals;
  primec::ir_lowerer::LocalInfo genericSoaColumnInfo;
  genericSoaColumnInfo.index = 5;
  genericSoaColumnInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  genericSoaColumnInfo.structTypeName = "SoaColumn<i32>";
  genericSoaStorageLocals.emplace("values", genericSoaColumnInfo);

  instructions.clear();
  error.clear();
  callExpr.name = "field_count";
  int genericSoaFieldCountEmitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            genericSoaStorageLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++genericSoaFieldCountEmitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 34});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(genericSoaFieldCountEmitExprCalls == 0);
  REQUIRE(instructions.size() == 4);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 5);
  CHECK(instructions[1].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].imm == primec::IrSlotBytes);
  CHECK(instructions[2].op == primec::IrOpcode::AddI64);
  CHECK(instructions[3].op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  error.clear();
  callExpr.isMethodCall = false;
  callExpr.name = "/std/collections/soa_storage/SoaColumn/field_capacity";
  int scopedSoaFieldCapacityEmitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            genericSoaStorageLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++scopedSoaFieldCapacityEmitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 55});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(scopedSoaFieldCapacityEmitExprCalls == 0);
  REQUIRE(instructions.size() == 4);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 5);
  CHECK(instructions[1].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].imm == primec::IrSlotBytes * 2);
  CHECK(instructions[2].op == primec::IrOpcode::AddI64);
  CHECK(instructions[3].op == primec::IrOpcode::LoadIndirect);

  callExpr.isMethodCall = false;
  instructions.clear();
  error.clear();
  callExpr.name = "capacity";
  int capacityEmitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            vectorLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &candidate, const primec::ir_lowerer::LocalMap &candidateLocals) {
              return primec::ir_lowerer::isVectorCapacityCall(candidate, candidateLocals);
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++capacityEmitExprCalls;
              instructions.push_back({primec::IrOpcode::AddressOfLocal, 2});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::NotHandled);
  CHECK(capacityEmitExprCalls == 0);
  CHECK(error.empty());
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            vectorLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &candidate, const primec::ir_lowerer::LocalMap &candidateLocals) {
              return primec::ir_lowerer::isVectorCapacityCall(candidate, candidateLocals);
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::NotHandled);
  CHECK(error.empty());
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  callExpr.name = "count";
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndex, size_t &length) {
              stringIndex = 2;
              length = 11;
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[0].imm == 11);

  instructions.clear();
  error.clear();
  callExpr.name = "/std/collections/vector/count";
  int dynamicCountEmitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++dynamicCountEmitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 3});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::NotHandled);
  CHECK(dynamicCountEmitExprCalls == 0);
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  callExpr.name = "/std/collections/vector/count";
  callExpr.args = {targetName};
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::PushArgc);
  CHECK(instructions[0].imm == 0);

  instructions.clear();
  error.clear();
  primec::Expr dynamicVectorCallTarget;
  dynamicVectorCallTarget.kind = primec::Expr::Kind::Call;
  dynamicVectorCallTarget.name = "/wrapVector__ti32";
  primec::Expr wrappedLiteral;
  wrappedLiteral.kind = primec::Expr::Kind::Literal;
  wrappedLiteral.literalValue = 6;
  dynamicVectorCallTarget.args = {wrappedLiteral};
  callExpr.name = "/std/collections/vector/count";
  callExpr.args = {dynamicVectorCallTarget};
  dynamicCountEmitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++dynamicCountEmitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 5});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(dynamicCountEmitExprCalls == 1);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 5);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  error = "stale";
  primec::Expr namedArgVectorTemporary;
  namedArgVectorTemporary.kind = primec::Expr::Kind::Call;
  namedArgVectorTemporary.name = "/std/collections/vector/vector";
  namedArgVectorTemporary.templateArgs = {"i32"};
  primec::Expr namedArgSecondLiteral;
  namedArgSecondLiteral.kind = primec::Expr::Kind::Literal;
  namedArgSecondLiteral.literalValue = 4;
  primec::Expr namedArgFirstLiteral;
  namedArgFirstLiteral.kind = primec::Expr::Kind::Literal;
  namedArgFirstLiteral.literalValue = 5;
  namedArgVectorTemporary.args = {namedArgSecondLiteral, namedArgFirstLiteral};
  namedArgVectorTemporary.argNames = {std::string("second"), std::string("first")};
  callExpr.name = "/std/collections/vector/count";
  callExpr.namespacePrefix.clear();
  callExpr.args = {namedArgVectorTemporary};
  int namedArgDynamicCountEmitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++namedArgDynamicCountEmitExprCalls;
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Error);
  CHECK(namedArgDynamicCountEmitExprCalls == 0);
  CHECK(instructions.empty());
  CHECK(error == "count requires array, vector, map, or string target");

  instructions.clear();
  error.clear();
  callExpr.name = "count";
  callExpr.args = {targetName};
  callExpr.argNames.clear();
  // namespacePrefix reflects enclosing namespace context (populated for
  // every call parsed inside any `namespace` block), not call-site
  // qualification - a bare `count(values)` written inside a namespace,
  // over a plain Name receiver, must be emitted the same way as the
  // identical unprefixed call tested above, not deferred/rejected.
  callExpr.namespacePrefix = "/std/collections/vector";
  dynamicCountEmitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++dynamicCountEmitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 7});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(dynamicCountEmitExprCalls == 1);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 7);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);
  callExpr.namespacePrefix.clear();

  instructions.clear();
  error.clear();
  callExpr.name = "/vector/capacity";
  int dynamicCapacityEmitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++dynamicCapacityEmitExprCalls;
              instructions.push_back({primec::IrOpcode::AddressOfLocal, 2});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::NotHandled);
  CHECK(dynamicCapacityEmitExprCalls == 0);
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  callExpr.name = "/std/collections/vector/capacity";
  callExpr.args = {dynamicVectorCallTarget};
  dynamicCapacityEmitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++dynamicCapacityEmitExprCalls;
              instructions.push_back({primec::IrOpcode::AddressOfLocal, 9});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(dynamicCapacityEmitExprCalls == 1);
  REQUIRE(instructions.size() == 4);
  CHECK(instructions[0].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[0].imm == 9);
  CHECK(instructions[1].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].imm == primec::IrSlotBytes);
  CHECK(instructions[2].op == primec::IrOpcode::AddI64);
  CHECK(instructions[3].op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  error.clear();
  callExpr.name = "capacity";
  callExpr.namespacePrefix = "/std/collections/vector";
  dynamicCapacityEmitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++dynamicCapacityEmitExprCalls;
              instructions.push_back({primec::IrOpcode::AddressOfLocal, 4});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(dynamicCapacityEmitExprCalls == 1);
  REQUIRE(instructions.size() == 4);
  CHECK(instructions[0].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[0].imm == 4);
  CHECK(instructions[1].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].imm == primec::IrSlotBytes);
  CHECK(instructions[2].op == primec::IrOpcode::AddI64);
  CHECK(instructions[3].op == primec::IrOpcode::LoadIndirect);
  callExpr.namespacePrefix.clear();

  instructions.clear();
  error.clear();
  callExpr.name = "count";
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Error);
  CHECK(error == "native backend only supports count() on string literals or string bindings");
}

TEST_CASE("ir lowerer count access helpers build count classifier adapters") {
  primec::ir_lowerer::LocalMap locals;
  auto isEntryArgsName = primec::ir_lowerer::makeIsEntryArgsName(true, "argv");
  auto isArrayCountCall = primec::ir_lowerer::makeIsArrayCountCall(true, "argv");
  auto isVectorCapacityCall = primec::ir_lowerer::makeIsVectorCapacityCall();
  auto isStringCountCall = primec::ir_lowerer::makeIsStringCountCall();

  primec::Expr entryName;
  entryName.kind = primec::Expr::Kind::Name;
  entryName.name = "argv";
  CHECK(isEntryArgsName(entryName, locals));

  primec::Expr countEntry;
  countEntry.kind = primec::Expr::Kind::Call;
  countEntry.name = "count";
  countEntry.args = {entryName};
  CHECK(isArrayCountCall(countEntry, locals));
  // A bare `count` call's namespacePrefix reflects the enclosing
  // definition's namespace context (populated for every call parsed inside
  // any `namespace` block), not whether the call was written with an
  // explicit qualification - that only shows up in `.name` (see the two
  // `/std/collections/vector/count`-style cases below). A namespace-context
  // prefix must not change classification for a plain Name receiver.
  countEntry.namespacePrefix = "/std/collections/vector";
  CHECK(isArrayCountCall(countEntry, locals));
  countEntry.namespacePrefix.clear();
  countEntry.name = "/std/collections/vector/count";
  CHECK_FALSE(isArrayCountCall(countEntry, locals));
  countEntry.name = "/vector/count";
  CHECK_FALSE(isArrayCountCall(countEntry, locals));
  countEntry.name = "/soa/count";
  CHECK_FALSE(isArrayCountCall(countEntry, locals));
  primec::Expr namedArgVectorTemporary;
  namedArgVectorTemporary.kind = primec::Expr::Kind::Call;
  namedArgVectorTemporary.name = "/std/collections/vector/vector";
  namedArgVectorTemporary.templateArgs = {"i32"};
  primec::Expr secondLiteral;
  secondLiteral.kind = primec::Expr::Kind::Literal;
  secondLiteral.literalValue = 4;
  primec::Expr firstLiteral;
  firstLiteral.kind = primec::Expr::Kind::Literal;
  firstLiteral.literalValue = 5;
  namedArgVectorTemporary.args = {secondLiteral, firstLiteral};
  namedArgVectorTemporary.argNames = {std::string("second"), std::string("first")};
  countEntry.name = "/std/collections/vector/count";
  countEntry.args = {namedArgVectorTemporary};
  CHECK_FALSE(isArrayCountCall(countEntry, locals));

  primec::ir_lowerer::LocalInfo vecInfo;
  vecInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  locals.emplace("values", vecInfo);
  primec::ir_lowerer::LocalInfo soaInfo;
  soaInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  soaInfo.isSoaVector = true;
  locals.emplace("packed", soaInfo);
  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";
  primec::Expr packedName;
  packedName.kind = primec::Expr::Kind::Name;
  packedName.name = "packed";
  primec::Expr capacityCall;
  capacityCall.kind = primec::Expr::Kind::Call;
  capacityCall.name = "capacity";
  capacityCall.args = {valuesName};
  CHECK(isVectorCapacityCall(capacityCall, locals));
  capacityCall.namespacePrefix = "/std/collections/vector";
  CHECK(isVectorCapacityCall(capacityCall, locals));
  capacityCall.namespacePrefix.clear();
  capacityCall.name = "/std/collections/vector/capacity";
  CHECK_FALSE(isVectorCapacityCall(capacityCall, locals));

  for (const char *soaToAosPath : {"/std/collections/soa/to_aos__t0",
                                   "/std/collections/soa/to_aos__t0"}) {
    primec::Expr canonicalToAosCall;
    canonicalToAosCall.kind = primec::Expr::Kind::Call;
    canonicalToAosCall.name = soaToAosPath;
    canonicalToAosCall.args = {packedName};
    primec::Expr countCanonicalToAos;
    countCanonicalToAos.kind = primec::Expr::Kind::Call;
    countCanonicalToAos.name = "count";
    countCanonicalToAos.args = {canonicalToAosCall};
    CHECK_FALSE(isArrayCountCall(countCanonicalToAos, locals));
  }

  primec::Expr zeroIndex;
  zeroIndex.kind = primec::Expr::Kind::Literal;
  zeroIndex.intWidth = 32;
  zeroIndex.literalValue = 0;

  primec::Expr borrowedSoaValues;
  borrowedSoaValues.kind = primec::Expr::Kind::Name;
  borrowedSoaValues.name = "borrowedSoaValues";
  primec::ir_lowerer::LocalInfo borrowedSoaPackInfo;
  borrowedSoaPackInfo.isArgsPack = true;
  borrowedSoaPackInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  borrowedSoaPackInfo.referenceToVector = true;
  borrowedSoaPackInfo.isSoaVector = true;
  locals.emplace("borrowedSoaValues", borrowedSoaPackInfo);

  primec::Expr borrowedSoaAccess;
  borrowedSoaAccess.kind = primec::Expr::Kind::Call;
  borrowedSoaAccess.name = "at";
  borrowedSoaAccess.args = {borrowedSoaValues, zeroIndex};
  primec::Expr borrowedSoaDeref;
  borrowedSoaDeref.kind = primec::Expr::Kind::Call;
  borrowedSoaDeref.name = "dereference";
  borrowedSoaDeref.args = {borrowedSoaAccess};
  countEntry.name = "count";
  countEntry.args = {borrowedSoaDeref};
  CHECK(isArrayCountCall(countEntry, locals));

  primec::Expr stringCount;
  stringCount.kind = primec::Expr::Kind::Call;
  stringCount.name = "count";
  primec::Expr literal;
  literal.kind = primec::Expr::Kind::StringLiteral;
  literal.stringValue = "\"ok\"utf8";
  stringCount.args = {literal};
  CHECK(isStringCountCall(stringCount, locals));
  stringCount.name = "/std/collections/vector/count";
  CHECK_FALSE(isStringCountCall(stringCount, locals));
}

TEST_CASE("ir lowerer count access helpers build bundled classifiers") {
  primec::ir_lowerer::LocalMap locals;
  auto classifiers = primec::ir_lowerer::makeCountAccessClassifiers(true, "argv");

  primec::Expr entryName;
  entryName.kind = primec::Expr::Kind::Name;
  entryName.name = "argv";
  CHECK(classifiers.isEntryArgsName(entryName, locals));

  primec::Expr countEntry;
  countEntry.kind = primec::Expr::Kind::Call;
  countEntry.name = "count";
  countEntry.args = {entryName};
  CHECK(classifiers.isArrayCountCall(countEntry, locals));
  // See the matching case in "ir lowerer count access helpers build count
  // classifier adapters" above: namespacePrefix reflects enclosing
  // namespace context, not call-site qualification, and must not change
  // classification for a bare `count` call over a plain Name receiver.
  countEntry.namespacePrefix = "/std/collections/vector";
  CHECK(classifiers.isArrayCountCall(countEntry, locals));
  countEntry.namespacePrefix.clear();
  countEntry.name = "/std/collections/vector/count";
  CHECK_FALSE(classifiers.isArrayCountCall(countEntry, locals));
  countEntry.name = "/vector/count";
  CHECK_FALSE(classifiers.isArrayCountCall(countEntry, locals));
  countEntry.name = "/soa/count";
  CHECK_FALSE(classifiers.isArrayCountCall(countEntry, locals));
  primec::Expr namedArgVectorTemporary;
  namedArgVectorTemporary.kind = primec::Expr::Kind::Call;
  namedArgVectorTemporary.name = "/std/collections/vector/vector";
  namedArgVectorTemporary.templateArgs = {"i32"};
  primec::Expr secondLiteral;
  secondLiteral.kind = primec::Expr::Kind::Literal;
  secondLiteral.literalValue = 4;
  primec::Expr firstLiteral;
  firstLiteral.kind = primec::Expr::Kind::Literal;
  firstLiteral.literalValue = 5;
  namedArgVectorTemporary.args = {secondLiteral, firstLiteral};
  namedArgVectorTemporary.argNames = {std::string("second"), std::string("first")};
  countEntry.name = "/std/collections/vector/count";
  countEntry.args = {namedArgVectorTemporary};
  CHECK_FALSE(classifiers.isArrayCountCall(countEntry, locals));

  primec::ir_lowerer::LocalInfo vecInfo;
  vecInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  locals.emplace("values", vecInfo);
  primec::ir_lowerer::LocalInfo soaInfo;
  soaInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  soaInfo.isSoaVector = true;
  locals.emplace("packed", soaInfo);
  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";
  primec::Expr packedName;
  packedName.kind = primec::Expr::Kind::Name;
  packedName.name = "packed";
  primec::Expr capacityCall;
  capacityCall.kind = primec::Expr::Kind::Call;
  capacityCall.name = "capacity";
  capacityCall.args = {valuesName};
  CHECK(classifiers.isVectorCapacityCall(capacityCall, locals));
  capacityCall.namespacePrefix = "/std/collections/vector";
  CHECK(classifiers.isVectorCapacityCall(capacityCall, locals));
  capacityCall.namespacePrefix.clear();
  capacityCall.name = "/vector/capacity";
  CHECK_FALSE(classifiers.isVectorCapacityCall(capacityCall, locals));
  for (const char *soaToAosPath : {"/std/collections/soa/to_aos__t0",
                                   "/std/collections/soa/to_aos__t0"}) {
    primec::Expr canonicalToAosCall;
    canonicalToAosCall.kind = primec::Expr::Kind::Call;
    canonicalToAosCall.name = soaToAosPath;
    canonicalToAosCall.args = {packedName};
    primec::Expr countCanonicalToAos;
    countCanonicalToAos.kind = primec::Expr::Kind::Call;
    countCanonicalToAos.name = "count";
    countCanonicalToAos.args = {canonicalToAosCall};
    CHECK_FALSE(classifiers.isArrayCountCall(countCanonicalToAos, locals));
  }

  primec::Expr zeroIndex;
  zeroIndex.kind = primec::Expr::Kind::Literal;
  zeroIndex.intWidth = 32;
  zeroIndex.literalValue = 0;

  primec::Expr pointerSoaValues;
  pointerSoaValues.kind = primec::Expr::Kind::Name;
  pointerSoaValues.name = "pointerSoaValues";
  primec::ir_lowerer::LocalInfo pointerSoaPackInfo;
  pointerSoaPackInfo.isArgsPack = true;
  pointerSoaPackInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerSoaPackInfo.pointerToVector = true;
  pointerSoaPackInfo.isSoaVector = true;
  locals.emplace("pointerSoaValues", pointerSoaPackInfo);

  primec::Expr pointerSoaAccess;
  pointerSoaAccess.kind = primec::Expr::Kind::Call;
  pointerSoaAccess.name = "at";
  pointerSoaAccess.args = {pointerSoaValues, zeroIndex};
  primec::Expr pointerSoaDeref;
  pointerSoaDeref.kind = primec::Expr::Kind::Call;
  pointerSoaDeref.name = "dereference";
  pointerSoaDeref.args = {pointerSoaAccess};
  countEntry.name = "count";
  countEntry.args = {pointerSoaDeref};
  CHECK(classifiers.isArrayCountCall(countEntry, locals));
  CHECK_FALSE(classifiers.isStringCountCall(capacityCall, locals));
}

TEST_SUITE_END();
