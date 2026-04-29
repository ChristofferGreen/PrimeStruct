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
  primitiveArgsInfo.argsPackElementCount = 6;
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
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[0].imm == 6);

  primec::ir_lowerer::LocalMap experimentalVectorLocals;
  primec::ir_lowerer::LocalInfo experimentalVectorInfo;
  experimentalVectorInfo.index = 3;
  experimentalVectorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  experimentalVectorInfo.valueKind =
      primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  experimentalVectorInfo.structTypeName =
      "/std/collections/experimental_vector/Vector__t25a78a513414c3bf";
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
            error) == Result::Emitted);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[0].imm == 3);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  error.clear();
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
            error) == Result::Emitted);
  CHECK(fieldCountEmitExprCalls == 0);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[0].imm == 3);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  error.clear();
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
            error) == Result::Emitted);
  CHECK(fieldCapacityEmitExprCalls == 0);
  REQUIRE(instructions.size() == 4);
  CHECK(instructions[0].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[0].imm == 3);
  CHECK(instructions[1].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].imm == primec::IrSlotBytes);
  CHECK(instructions[2].op == primec::IrOpcode::AddI64);
  CHECK(instructions[3].op == primec::IrOpcode::LoadIndirect);

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
      "/std/collections/internal_soa_storage/SoaColumn__ti32";
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
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
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
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[0].imm == 4);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);

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
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
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
  CHECK(instructions[0].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[0].imm == 4);
  CHECK(instructions[1].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].imm == primec::IrSlotBytes);
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
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[0].imm == 5);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);

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
            error) == Result::Emitted);
  CHECK(dynamicCountEmitExprCalls == 1);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);

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
            error) == Result::NotHandled);
  CHECK(dynamicCapacityEmitExprCalls == 0);
  CHECK(instructions.empty());
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
  countEntry.namespacePrefix = "/std/collections/vector";
  CHECK(isArrayCountCall(countEntry, locals));
  countEntry.namespacePrefix.clear();
  countEntry.name = "/std/collections/vector/count";
  CHECK(isArrayCountCall(countEntry, locals));
  countEntry.name = "/vector/count";
  CHECK_FALSE(isArrayCountCall(countEntry, locals));
  countEntry.name = "/soa_vector/count";
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
  CHECK_FALSE(isVectorCapacityCall(capacityCall, locals));
  capacityCall.namespacePrefix = "/std/collections/vector";
  CHECK_FALSE(isVectorCapacityCall(capacityCall, locals));
  capacityCall.namespacePrefix.clear();
  capacityCall.name = "/std/collections/vector/capacity";
  CHECK_FALSE(isVectorCapacityCall(capacityCall, locals));

  primec::Expr canonicalToAosCall;
  canonicalToAosCall.kind = primec::Expr::Kind::Call;
  canonicalToAosCall.name = "/std/collections/soa_vector/to_aos__t0";
  canonicalToAosCall.args = {packedName};
  primec::Expr countCanonicalToAos;
  countCanonicalToAos.kind = primec::Expr::Kind::Call;
  countCanonicalToAos.name = "count";
  countCanonicalToAos.args = {canonicalToAosCall};
  CHECK_FALSE(isArrayCountCall(countCanonicalToAos, locals));

  primec::Expr stringCount;
  stringCount.kind = primec::Expr::Kind::Call;
  stringCount.name = "count";
  primec::Expr literal;
  literal.kind = primec::Expr::Kind::StringLiteral;
  literal.stringValue = "\"ok\"utf8";
  stringCount.args = {literal};
  CHECK(isStringCountCall(stringCount, locals));
  stringCount.name = "/std/collections/vector/count";
  CHECK(isStringCountCall(stringCount, locals));
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
  countEntry.namespacePrefix = "/std/collections/vector";
  CHECK(classifiers.isArrayCountCall(countEntry, locals));
  countEntry.namespacePrefix.clear();
  countEntry.name = "/std/collections/vector/count";
  CHECK(classifiers.isArrayCountCall(countEntry, locals));
  countEntry.name = "/vector/count";
  CHECK_FALSE(classifiers.isArrayCountCall(countEntry, locals));
  countEntry.name = "/soa_vector/count";
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
  CHECK_FALSE(classifiers.isVectorCapacityCall(capacityCall, locals));
  capacityCall.namespacePrefix = "/std/collections/vector";
  CHECK_FALSE(classifiers.isVectorCapacityCall(capacityCall, locals));
  capacityCall.namespacePrefix.clear();
  capacityCall.name = "/vector/capacity";
  CHECK_FALSE(classifiers.isVectorCapacityCall(capacityCall, locals));
  primec::Expr canonicalToAosCall;
  canonicalToAosCall.kind = primec::Expr::Kind::Call;
  canonicalToAosCall.name = "/std/collections/soa_vector/to_aos__t0";
  canonicalToAosCall.args = {packedName};
  primec::Expr countCanonicalToAos;
  countCanonicalToAos.kind = primec::Expr::Kind::Call;
  countCanonicalToAos.name = "count";
  countCanonicalToAos.args = {canonicalToAosCall};
  CHECK_FALSE(classifiers.isArrayCountCall(countCanonicalToAos, locals));
  CHECK_FALSE(classifiers.isStringCountCall(capacityCall, locals));
}

TEST_CASE("ir lowerer count access helpers normalize parser-shaped canonical map access receivers") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;
  using Result = primec::ir_lowerer::CountAccessCallEmitResult;

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.literalValue = 1;

  primec::Expr targetExpr;
  targetExpr.kind = primec::Expr::Kind::Call;
  targetExpr.name = "at";
  targetExpr.namespacePrefix = "/std/collections/map";
  targetExpr.args = {valuesName, keyExpr};

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/std/collections/map/count";
  callExpr.args = {targetExpr};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int emitExprCalls = 0;

  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 9});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  error.clear();
  emitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 11});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::NotHandled);
  CHECK(error.empty());
  CHECK(emitExprCalls == 0);
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer string literal helper interns string table values") {
  std::vector<std::string> stringTable;
  CHECK(primec::ir_lowerer::internLowererString("hello", stringTable) == 0);
  CHECK(primec::ir_lowerer::internLowererString("world", stringTable) == 1);
  CHECK(primec::ir_lowerer::internLowererString("hello", stringTable) == 0);
  REQUIRE(stringTable.size() == 2);
  CHECK(stringTable[0] == "hello");
  CHECK(stringTable[1] == "world");
}

TEST_CASE("ir lowerer string literal helper builds string interner") {
  std::vector<std::string> stringTable;
  auto internString = primec::ir_lowerer::makeInternLowererString(stringTable);

  CHECK(internString("hello") == 0);
  CHECK(internString("world") == 1);
  CHECK(internString("hello") == 0);
  REQUIRE(stringTable.size() == 2);
  CHECK(stringTable[0] == "hello");
  CHECK(stringTable[1] == "world");
}

TEST_CASE("ir lowerer string literal helper parses and validates encoding") {
  std::string decoded;
  std::string error;
  REQUIRE(primec::ir_lowerer::parseLowererStringLiteral("\"line\\n\"utf8", decoded, error));
  CHECK(decoded == "line\n");
  CHECK(error.empty());

  std::string asciiToken = "\"";
  asciiToken.push_back(static_cast<char>(0xC3));
  asciiToken.push_back(static_cast<char>(0xA5));
  asciiToken += "\"ascii";
  CHECK_FALSE(primec::ir_lowerer::parseLowererStringLiteral(asciiToken, decoded, error));
  CHECK(error == "ascii string literal contains non-ASCII characters");

  CHECK_FALSE(primec::ir_lowerer::parseLowererStringLiteral("\"missing_suffix\"", decoded, error));
  CHECK(error == "string literal requires utf8/ascii/raw_utf8/raw_ascii suffix");
}

TEST_CASE("ir lowerer string literal helper resolves string table targets") {
  std::vector<std::string> stringTable;
  auto internString = [&](const std::string &text) {
    for (size_t i = 0; i < stringTable.size(); ++i) {
      if (stringTable[i] == text) {
        return static_cast<int32_t>(i);
      }
    }
    stringTable.push_back(text);
    return static_cast<int32_t>(stringTable.size() - 1);
  };

  primec::Expr literalExpr;
  literalExpr.kind = primec::Expr::Kind::StringLiteral;
  literalExpr.stringValue = "\"hello\"utf8";

  int32_t stringIndex = -1;
  size_t length = 0;
  std::string error;
  REQUIRE(primec::ir_lowerer::resolveStringTableTarget(
      literalExpr, primec::ir_lowerer::LocalMap{}, stringTable, internString, stringIndex, length, error));
  CHECK(stringIndex == 0);
  CHECK(length == 5);
  CHECK(stringTable.size() == 1);

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo local;
  local.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  local.stringSource = primec::ir_lowerer::LocalInfo::StringSource::TableIndex;
  local.stringIndex = 0;
  locals.emplace("name", local);

  primec::Expr nameExpr;
  nameExpr.kind = primec::Expr::Kind::Name;
  nameExpr.name = "name";
  REQUIRE(primec::ir_lowerer::resolveStringTableTarget(
      nameExpr, locals, stringTable, internString, stringIndex, length, error));
  CHECK(stringIndex == 0);
  CHECK(length == 5);
}

TEST_CASE("ir lowerer string literal helper builds string table target resolver") {
  std::vector<std::string> stringTable;
  auto internString = [&](const std::string &text) {
    for (size_t i = 0; i < stringTable.size(); ++i) {
      if (stringTable[i] == text) {
        return static_cast<int32_t>(i);
      }
    }
    stringTable.push_back(text);
    return static_cast<int32_t>(stringTable.size() - 1);
  };
  std::string error;
  auto resolveStringTableTarget =
      primec::ir_lowerer::makeResolveStringTableTarget(stringTable, internString, error);

  primec::Expr literalExpr;
  literalExpr.kind = primec::Expr::Kind::StringLiteral;
  literalExpr.stringValue = "\"hello\"utf8";
  int32_t stringIndex = -1;
  size_t length = 0;
  REQUIRE(resolveStringTableTarget(literalExpr, primec::ir_lowerer::LocalMap{}, stringIndex, length));
  CHECK(stringIndex == 0);
  CHECK(length == 5);

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo local;
  local.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  local.stringSource = primec::ir_lowerer::LocalInfo::StringSource::TableIndex;
  local.stringIndex = 0;
  locals.emplace("name", local);
  primec::Expr nameExpr;
  nameExpr.kind = primec::Expr::Kind::Name;
  nameExpr.name = "name";
  REQUIRE(resolveStringTableTarget(nameExpr, locals, stringIndex, length));
  CHECK(stringIndex == 0);
  CHECK(length == 5);
}

TEST_CASE("ir lowerer string literal helper builds bundled context") {
  std::vector<std::string> stringTable;
  std::string error;
  auto helpers = primec::ir_lowerer::makeStringLiteralHelperContext(stringTable, error);

  CHECK(helpers.internString("hello") == 0);
  CHECK(helpers.internString("hello") == 0);

  primec::Expr literalExpr;
  literalExpr.kind = primec::Expr::Kind::StringLiteral;
  literalExpr.stringValue = "\"hello\"utf8";
  int32_t stringIndex = -1;
  size_t length = 0;
  REQUIRE(helpers.resolveStringTableTarget(literalExpr, primec::ir_lowerer::LocalMap{}, stringIndex, length));
  CHECK(stringIndex == 0);
  CHECK(length == 5);
  REQUIRE(stringTable.size() == 1);
  CHECK(stringTable[0] == "hello");

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo local;
  local.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  local.stringSource = primec::ir_lowerer::LocalInfo::StringSource::TableIndex;
  local.stringIndex = -1;
  locals.emplace("bad", local);

  primec::Expr nameExpr;
  nameExpr.kind = primec::Expr::Kind::Name;
  nameExpr.name = "bad";
  CHECK_FALSE(helpers.resolveStringTableTarget(nameExpr, locals, stringIndex, length));
  CHECK(error == "native backend missing string table data for: bad");
}

TEST_CASE("ir lowerer string literal helper reports table-target diagnostics") {
  std::vector<std::string> stringTable;
  auto internString = [&](const std::string &text) {
    stringTable.push_back(text);
    return static_cast<int32_t>(stringTable.size() - 1);
  };

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo local;
  local.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  local.stringSource = primec::ir_lowerer::LocalInfo::StringSource::TableIndex;
  local.stringIndex = -1;
  locals.emplace("bad", local);

  primec::Expr nameExpr;
  nameExpr.kind = primec::Expr::Kind::Name;
  nameExpr.name = "bad";
  int32_t stringIndex = -1;
  size_t length = 0;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::resolveStringTableTarget(
      nameExpr, locals, stringTable, internString, stringIndex, length, error));
  CHECK(error == "native backend missing string table data for: bad");

  error.clear();
  locals["bad"].stringIndex = 42;
  CHECK_FALSE(primec::ir_lowerer::resolveStringTableTarget(
      nameExpr, locals, stringTable, internString, stringIndex, length, error));
  CHECK(error == "native backend encountered invalid string table index");
}

TEST_CASE("ir lowerer template type parse helper splits nested template args") {
  std::vector<std::string> args;
  REQUIRE(primec::ir_lowerer::splitTemplateArgs(" i32 , map<string, array<i64>> , Result<bool, FileError> ", args));
  REQUIRE(args.size() == 3);
  CHECK(args[0] == "i32");
  CHECK(args[1] == "map<string, array<i64>>");
  CHECK(args[2] == "Result<bool, FileError>");

  CHECK_FALSE(primec::ir_lowerer::splitTemplateArgs("i32, map<string, i64", args));
  CHECK_FALSE(primec::ir_lowerer::splitTemplateArgs("i32>", args));
}

TEST_CASE("ir lowerer template type parse helper splits template type names") {
  std::string base;
  std::string arg;

  REQUIRE(primec::ir_lowerer::splitTemplateTypeName("Result< map<string, i64> , FileError >", base, arg));
  CHECK(base == "Result");
  CHECK(arg == " map<string, i64> , FileError ");

  CHECK_FALSE(primec::ir_lowerer::splitTemplateTypeName("Result<i64", base, arg));
  CHECK(base.empty());
  CHECK(arg.empty());

  CHECK_FALSE(primec::ir_lowerer::splitTemplateTypeName("i64", base, arg));
  CHECK(base.empty());
  CHECK(arg.empty());

  CHECK_FALSE(primec::ir_lowerer::splitTemplateTypeName("", base, arg));
  CHECK(base.empty());
  CHECK(arg.empty());
}

TEST_CASE("ir lowerer template type parse helper parses Result return type names") {
  bool hasValue = false;
  primec::ir_lowerer::LocalInfo::ValueKind valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  std::string errorType;

  REQUIRE(primec::ir_lowerer::parseResultTypeName("Result<FileError>", hasValue, valueKind, errorType));
  CHECK_FALSE(hasValue);
  CHECK(valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(errorType == "FileError");

  REQUIRE(primec::ir_lowerer::parseResultTypeName("Result< i64 , FileError >", hasValue, valueKind, errorType));
  CHECK(hasValue);
  CHECK(valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(errorType == "FileError");

  REQUIRE(primec::ir_lowerer::parseResultTypeName(
      "/std/result/Result< i32 , MyError >",
      hasValue,
      valueKind,
      errorType));
  CHECK(hasValue);
  CHECK(valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(errorType == "MyError");

  CHECK_FALSE(primec::ir_lowerer::parseResultTypeName("array<i64>", hasValue, valueKind, errorType));
  CHECK_FALSE(primec::ir_lowerer::parseResultTypeName("Result<i64, FileError, Extra>", hasValue, valueKind, errorType));
}

TEST_CASE("ir lowerer runtime error helpers emit print-and-return sequence") {
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

  primec::ir_lowerer::emitArrayIndexOutOfBounds(function, internString);
  REQUIRE(function.instructions.size() == 3);
  CHECK(function.instructions[0].op == primec::IrOpcode::PrintString);
  CHECK(primec::decodePrintFlags(function.instructions[0].imm) == primec::encodePrintFlags(true, true));
  CHECK(primec::decodePrintStringIndex(function.instructions[0].imm) == 0);
  CHECK(function.instructions[1].op == primec::IrOpcode::PushI32);
  CHECK(function.instructions[1].imm == 3);
  CHECK(function.instructions[2].op == primec::IrOpcode::ReturnI32);
  CHECK(function.instructions[2].imm == 0);
  REQUIRE(stringTable.size() == 1);
  CHECK(stringTable[0] == "array index out of bounds");

  primec::ir_lowerer::emitArrayIndexOutOfBounds(function, internString);
  REQUIRE(function.instructions.size() == 6);
  CHECK(primec::decodePrintStringIndex(function.instructions[3].imm) == 0);
  REQUIRE(stringTable.size() == 1);
}

TEST_CASE("ir lowerer runtime error helpers map each helper to expected message") {
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

  primec::ir_lowerer::emitStringIndexOutOfBounds(function, internString);
  primec::ir_lowerer::emitPointerIndexOutOfBounds(function, internString);
  primec::ir_lowerer::emitMapKeyNotFound(function, internString);
  primec::ir_lowerer::emitVectorIndexOutOfBounds(function, internString);
  primec::ir_lowerer::emitVectorPopOnEmpty(function, internString);
  primec::ir_lowerer::emitVectorCapacityExceeded(function, internString);
  primec::ir_lowerer::emitVectorReserveNegative(function, internString);
  primec::ir_lowerer::emitVectorReserveExceeded(function, internString);
  primec::ir_lowerer::emitLoopCountNegative(function, internString);
  primec::ir_lowerer::emitPowNegativeExponent(function, internString);
  primec::ir_lowerer::emitFloatToIntNonFinite(function, internString);

  const std::vector<std::string> expectedMessages = {"string index out of bounds",
                                                     "pointer index out of bounds",
                                                     "map key not found",
                                                     "container index out of bounds",
                                                     "container empty",
                                                     "vector push allocation failed (out of memory)",
                                                     "vector reserve expects non-negative capacity",
                                                     "vector reserve allocation failed (out of memory)",
                                                     "loop count must be non-negative",
                                                     "pow exponent must be non-negative",
                                                     "float to int conversion requires finite value"};
  CHECK(stringTable == expectedMessages);

  REQUIRE(function.instructions.size() == expectedMessages.size() * 3);
  for (size_t i = 0; i < expectedMessages.size(); ++i) {
    const size_t base = i * 3;
    CHECK(function.instructions[base].op == primec::IrOpcode::PrintString);
    CHECK(primec::decodePrintStringIndex(function.instructions[base].imm) == i);
    CHECK(function.instructions[base + 1].op == primec::IrOpcode::PushI32);
    CHECK(function.instructions[base + 1].imm == 3);
    CHECK(function.instructions[base + 2].op == primec::IrOpcode::ReturnI32);
    CHECK(function.instructions[base + 2].imm == 0);
  }
}

TEST_CASE("ir lowerer runtime error helpers emit file-error why dispatch sequence") {
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

  primec::ir_lowerer::emitFileErrorWhy(function, 7, internString);

  auto emptyIt = std::find(stringTable.begin(), stringTable.end(), "");
  REQUIRE(emptyIt != stringTable.end());
  auto unknownIt = std::find(stringTable.begin(), stringTable.end(), "Unknown file error");
  REQUIRE(unknownIt != stringTable.end());
  const uint64_t unknownIndex = static_cast<uint64_t>(std::distance(stringTable.begin(), unknownIt));

  REQUIRE_FALSE(function.instructions.empty());
  CHECK(function.instructions.back().op == primec::IrOpcode::PushI64);
  CHECK(function.instructions.back().imm == unknownIndex);

  CHECK(std::any_of(function.instructions.begin(),
                    function.instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::JumpIfZero; }));
  CHECK(std::any_of(function.instructions.begin(),
                    function.instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::Jump; }));
}

TEST_SUITE_END();
