#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer call helpers try emit map access lookup") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;
  using Result = primec::ir_lowerer::MapAccessLookupEmitResult;

  primec::Expr targetExpr;
  targetExpr.kind = primec::Expr::Kind::Name;
  targetExpr.name = "items";
  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Name;
  keyExpr.name = "mapKey";

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo mapInfo;
  mapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  mapInfo.mapKeyKind = Kind::Int32;
  mapInfo.mapValueKind = Kind::Float64;
  locals.emplace(targetExpr.name, mapInfo);

  std::vector<primec::IrInstruction> instructions;
  int nextLocal = 20;
  int emitExprCalls = 0;
  int inferCalls = 0;
  int keyNotFoundCalls = 0;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitMapAccessLookup(
            "at",
            targetExpr,
            keyExpr,
            locals,
            [&]() { return nextLocal++; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++inferCalls;
              return Kind::Int32;
            },
            [&]() { ++keyNotFoundCalls; },
            [&]() { return instructions.size(); },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 2);
  CHECK(inferCalls == 1);
  CHECK(keyNotFoundCalls == 1);
  CHECK_FALSE(instructions.empty());
  CHECK(instructions.front().op == primec::IrOpcode::StoreLocal);

  instructions.clear();
  emitExprCalls = 0;
  inferCalls = 0;
  keyNotFoundCalls = 0;
  error = "stale";
  primec::Expr nonMapTarget = targetExpr;
  nonMapTarget.name = "plain";
  CHECK(primec::ir_lowerer::tryEmitMapAccessLookup(
            "at",
            nonMapTarget,
            keyExpr,
            locals,
            [&]() { return nextLocal++; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++inferCalls;
              return Kind::Int32;
            },
            [&]() { ++keyNotFoundCalls; },
            [&]() { return instructions.size(); },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
            error) == Result::NotHandled);
  CHECK(error == "stale");
  CHECK(emitExprCalls == 0);
  CHECK(inferCalls == 0);
  CHECK(keyNotFoundCalls == 0);
  CHECK(instructions.empty());

  instructions.clear();
  emitExprCalls = 0;
  inferCalls = 0;
  keyNotFoundCalls = 0;
  error.clear();
  primec::ir_lowerer::LocalMap untypedLocals;
  primec::ir_lowerer::LocalInfo untypedMapInfo;
  untypedMapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  untypedMapInfo.mapKeyKind = Kind::Unknown;
  untypedMapInfo.mapValueKind = Kind::Int32;
  untypedLocals.emplace(targetExpr.name, untypedMapInfo);
  CHECK(primec::ir_lowerer::tryEmitMapAccessLookup(
            "at",
            targetExpr,
            keyExpr,
            untypedLocals,
            [&]() { return nextLocal++; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++inferCalls;
              return Kind::Int32;
            },
            [&]() { ++keyNotFoundCalls; },
            [&]() { return instructions.size(); },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
            error) == Result::Error);
  CHECK(error == "native backend requires typed map bindings for at");
  CHECK(emitExprCalls == 0);
  CHECK(inferCalls == 0);
  CHECK(keyNotFoundCalls == 0);
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer call helpers resolve validated access index kind") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Name;
  indexExpr.name = "idx";
  primec::ir_lowerer::LocalMap locals;
  std::string error;
  Kind indexKind = Kind::Unknown;

  int inferCalls = 0;
  CHECK(primec::ir_lowerer::resolveValidatedAccessIndexKind(
      indexExpr,
      locals,
      "at",
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::Bool;
      },
      indexKind,
      error));
  CHECK(inferCalls == 1);
  CHECK(indexKind == Kind::Int32);
  CHECK(error.empty());

  inferCalls = 0;
  error.clear();
  CHECK(primec::ir_lowerer::resolveValidatedAccessIndexKind(
      indexExpr,
      locals,
      "index",
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::UInt64;
      },
      indexKind,
      error));
  CHECK(inferCalls == 1);
  CHECK(indexKind == Kind::UInt64);
  CHECK(error.empty());

  inferCalls = 0;
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveValidatedAccessIndexKind(
      indexExpr,
      locals,
      "at",
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::Float32;
      },
      indexKind,
      error));
  CHECK(inferCalls == 1);
  CHECK(error == "native backend requires integer indices for at");
}

TEST_CASE("ir lowerer call helpers emit string table access load") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;
  using Result = primec::ir_lowerer::StringTableAccessEmitResult;

  primec::Expr targetExpr;
  targetExpr.kind = primec::Expr::Kind::Name;
  targetExpr.name = "text";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Name;
  indexExpr.name = "idx";

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::IrInstruction> instructions;
  std::string error = "stale";

  int allocCalls = 0;
  int inferCalls = 0;
  int emitExprCalls = 0;
  int stringIndexOutOfBoundsCalls = 0;

  CHECK(primec::ir_lowerer::tryEmitStringTableAccessLoad(
            "at",
            targetExpr,
            indexExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++inferCalls;
              return Kind::Int32;
            },
            [&]() {
              ++allocCalls;
              return 9;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              return true;
            },
            [&]() { ++stringIndexOutOfBoundsCalls; },
            [&]() { return instructions.size(); },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
            error) == Result::NotHandled);
  CHECK(error == "stale");
  CHECK(allocCalls == 0);
  CHECK(inferCalls == 0);
  CHECK(emitExprCalls == 0);
  CHECK(stringIndexOutOfBoundsCalls == 0);
  CHECK(instructions.empty());

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitStringTableAccessLoad(
            "at",
            targetExpr,
            indexExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndex, size_t &length) {
              stringIndex = 7;
              length = 5;
              return true;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++inferCalls;
              return Kind::Float32;
            },
            [&]() {
              ++allocCalls;
              return 10;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              return true;
            },
            [&]() { ++stringIndexOutOfBoundsCalls; },
            [&]() { return instructions.size(); },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
            error) == Result::Error);
  CHECK(error == "native backend requires integer indices for at");
  CHECK(allocCalls == 0);
  CHECK(inferCalls == 1);
  CHECK(emitExprCalls == 0);
  CHECK(stringIndexOutOfBoundsCalls == 0);
  CHECK(instructions.empty());

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitStringTableAccessLoad(
            "at",
            targetExpr,
            indexExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndex, size_t &length) {
              stringIndex = 8;
              length = static_cast<size_t>(std::numeric_limits<int32_t>::max()) + 1;
              return true;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++inferCalls;
              return Kind::Int32;
            },
            [&]() {
              ++allocCalls;
              return 11;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              return true;
            },
            [&]() { ++stringIndexOutOfBoundsCalls; },
            [&]() { return instructions.size(); },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
            error) == Result::Error);
  CHECK(error == "native backend string too large for indexing");
  CHECK(allocCalls == 0);
  CHECK(inferCalls == 2);
  CHECK(emitExprCalls == 0);
  CHECK(stringIndexOutOfBoundsCalls == 0);
  CHECK(instructions.empty());

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitStringTableAccessLoad(
            "at",
            targetExpr,
            indexExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndex, size_t &length) {
              stringIndex = 9;
              length = 4;
              return true;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++inferCalls;
              return Kind::Int32;
            },
            [&]() {
              ++allocCalls;
              return 12;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              return false;
            },
            [&]() { ++stringIndexOutOfBoundsCalls; },
            [&]() { return instructions.size(); },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
            error) == Result::Error);
  CHECK(error.empty());
  CHECK(allocCalls == 1);
  CHECK(inferCalls == 3);
  CHECK(emitExprCalls == 1);
  CHECK(stringIndexOutOfBoundsCalls == 0);
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitStringTableAccessLoad(
            "at",
            targetExpr,
            indexExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndex, size_t &length) {
              stringIndex = 12;
              length = 5;
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            []() { return 21; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              instructions.push_back({primec::IrOpcode::PushI32, 3});
              return true;
            },
            [&]() { ++stringIndexOutOfBoundsCalls; },
            [&]() { return instructions.size(); },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
            error) == Result::Emitted);
  CHECK(stringIndexOutOfBoundsCalls == 2);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 12);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[0].imm == 3);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 21);
  CHECK(instructions[2].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[2].imm == 21);
  CHECK(instructions[11].op == primec::IrOpcode::LoadStringByte);
  CHECK(instructions[11].imm == 12);
}

TEST_CASE("ir lowerer call helpers emit array vector indexed access") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr targetExpr;
  targetExpr.kind = primec::Expr::Kind::Name;
  targetExpr.name = "vec";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Name;
  indexExpr.name = "idx";

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo vectorLocalInfo;
  vectorLocalInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  vectorLocalInfo.valueKind = Kind::Int32;
  locals.emplace("vec", vectorLocalInfo);

  std::vector<primec::IrInstruction> instructions;
  std::string error;

  int inferCalls = 0;
  int allocCalls = 0;
  int emitExprCalls = 0;
  int arrayIndexOutOfBoundsCalls = 0;

  error = "stale";
  CHECK_FALSE(primec::ir_lowerer::emitArrayVectorIndexedAccess(
      "at",
      targetExpr,
      indexExpr,
      primec::ir_lowerer::LocalMap{},
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::Int32;
      },
      [&]() {
        ++allocCalls;
        return 0;
      },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        return true;
      },
      [&]() { ++arrayIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error ==
        "native backend only supports at() on numeric/bool/string arrays or vectors, plus args<Struct>/args<map<K, V>>/args<Pointer<T>>/args<Reference<T>>/args<Pointer<Struct>>/args<Reference<Struct>>/args<Pointer<map<K, V>>>/args<Reference<map<K, V>>>/args<vector<T>>/args<Pointer<vector<T>>>/args<Reference<vector<T>>>/args<Pointer<soa_vector<T>>>/args<Reference<soa_vector<T>>> packs");
  CHECK(inferCalls == 0);
  CHECK(allocCalls == 0);
  CHECK(emitExprCalls == 0);
  CHECK(arrayIndexOutOfBoundsCalls == 0);
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitArrayVectorIndexedAccess(
      "at",
      targetExpr,
      indexExpr,
      locals,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::Float32;
      },
      [&]() {
        ++allocCalls;
        return 1;
      },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        return true;
      },
      [&]() { ++arrayIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error == "native backend requires integer indices for at");
  CHECK(inferCalls == 1);
  CHECK(allocCalls == 0);
  CHECK(emitExprCalls == 0);
  CHECK(arrayIndexOutOfBoundsCalls == 0);
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitArrayVectorIndexedAccess(
      "at",
      targetExpr,
      indexExpr,
      locals,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::Int32;
      },
      [&]() { return 40 + allocCalls++; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        return false;
      },
      [&]() { ++arrayIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error.empty());
  CHECK(inferCalls == 2);
  CHECK(allocCalls == 1);
  CHECK(emitExprCalls == 1);
  CHECK(arrayIndexOutOfBoundsCalls == 0);
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  inferCalls = 0;
  allocCalls = 0;
  emitExprCalls = 0;
  arrayIndexOutOfBoundsCalls = 0;
  CHECK(primec::ir_lowerer::emitArrayVectorIndexedAccess(
      "at",
      targetExpr,
      indexExpr,
      locals,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::Int32;
      },
      [&]() { return 50 + allocCalls++; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        instructions.push_back({primec::IrOpcode::PushI32, emitExprCalls == 1 ? 101u : 3u});
        return true;
      },
      [&]() { ++arrayIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error.empty());
  CHECK(inferCalls == 1);
  CHECK(allocCalls == 4);
  CHECK(emitExprCalls == 2);
  CHECK(arrayIndexOutOfBoundsCalls == 2);
  REQUIRE(instructions.size() == 26);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[0].imm == 101);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 50);
  CHECK(instructions[2].op == primec::IrOpcode::PushI32);
  CHECK(instructions[2].imm == 3);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 51);
  CHECK(instructions[16].op == primec::IrOpcode::PushI64);
  CHECK(instructions[16].imm == 32);
  CHECK(instructions[19].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[19].imm == 53);
  CHECK(instructions[20].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[20].imm == 53);
  CHECK(instructions[22].op == primec::ir_lowerer::pushOneForIndex(Kind::Int32));
  CHECK(instructions[22].imm == primec::IrSlotBytesI32);
  CHECK(instructions.back().op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  error.clear();
  inferCalls = 0;
  allocCalls = 0;
  emitExprCalls = 0;
  arrayIndexOutOfBoundsCalls = 0;
  primec::ir_lowerer::LocalMap structLocals;
  primec::ir_lowerer::LocalInfo structPackInfo;
  structPackInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  structPackInfo.isArgsPack = true;
  structPackInfo.structTypeName = "/pkg/Pair";
  structPackInfo.structSlotCount = 2;
  structLocals.emplace("vec", structPackInfo);
  CHECK(primec::ir_lowerer::emitArrayVectorIndexedAccess(
      "at",
      targetExpr,
      indexExpr,
      structLocals,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::Int32;
      },
      [&]() { return 60 + allocCalls++; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        instructions.push_back({primec::IrOpcode::PushI32, emitExprCalls == 1 ? 201u : 4u});
        return true;
      },
      [&]() { ++arrayIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error.empty());
  CHECK(inferCalls == 1);
  CHECK(allocCalls == 3);
  CHECK(emitExprCalls == 2);
  CHECK(arrayIndexOutOfBoundsCalls == 2);
  REQUIRE(instructions.size() == 24u);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 60u);
  CHECK(instructions[2].op == primec::IrOpcode::PushI32);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 61u);
  CHECK(instructions[4].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[4].imm == 60u);
  CHECK(instructions.back().op == primec::IrOpcode::AddI64);
}

TEST_CASE("ir lowerer call helpers emit scalar borrowed args pack indexed access") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;
  using LocalKind = primec::ir_lowerer::LocalInfo::Kind;

  primec::Expr targetExpr;
  targetExpr.kind = primec::Expr::Kind::Name;
  targetExpr.name = "values";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Name;
  indexExpr.name = "idx";

  auto checkScalarPack = [&](LocalKind packElementKind) {
    primec::ir_lowerer::LocalMap locals;
    primec::ir_lowerer::LocalInfo argsPackInfo;
    argsPackInfo.kind = LocalKind::Array;
    argsPackInfo.isArgsPack = true;
    argsPackInfo.argsPackElementKind = packElementKind;
    argsPackInfo.valueKind = Kind::Int32;
    locals.emplace("values", argsPackInfo);

    const auto resolved =
        primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(targetExpr, locals);
    CHECK(resolved.isArrayOrVectorTarget);
    CHECK(resolved.elemKind == Kind::Int32);
    CHECK_FALSE(resolved.isVectorTarget);
    CHECK(resolved.isArgsPackTarget);
    CHECK(resolved.argsPackElementKind == packElementKind);
    std::string error;
    CHECK(primec::ir_lowerer::validateArrayVectorAccessTargetInfo(resolved, error));
    CHECK(error.empty());

    std::vector<primec::IrInstruction> instructions;
    int inferCalls = 0;
    int emitExprCalls = 0;
    int arrayIndexOutOfBoundsCalls = 0;
    int allocCalls = 0;

    CHECK(primec::ir_lowerer::emitArrayVectorIndexedAccess(
        "at",
        targetExpr,
        indexExpr,
        locals,
        [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          ++inferCalls;
          return Kind::Int32;
        },
        [&]() { return 70 + allocCalls++; },
        [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
          ++emitExprCalls;
          if (expr.kind == primec::Expr::Kind::Name && expr.name == "values") {
            instructions.push_back({primec::IrOpcode::LoadLocal, 12});
            return true;
          }
          instructions.push_back({primec::IrOpcode::PushI32, 1});
          return true;
        },
        [&]() { ++arrayIndexOutOfBoundsCalls; },
        [&]() { return instructions.size(); },
        [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
        [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
        error));
    CHECK(error.empty());
    CHECK(inferCalls == 1);
    CHECK(emitExprCalls == 2);
    CHECK(arrayIndexOutOfBoundsCalls == 2);
    REQUIRE(instructions.size() >= 6);
    CHECK(instructions.front().op == primec::IrOpcode::LoadLocal);
    CHECK(instructions.front().imm == 12u);
    CHECK(instructions.back().op == primec::IrOpcode::LoadIndirect);
    CHECK(std::count_if(instructions.begin(),
                        instructions.end(),
                        [](const primec::IrInstruction &instruction) {
                          return instruction.op == primec::IrOpcode::LoadIndirect;
                        }) == 2);
  };

  checkScalarPack(LocalKind::Reference);
  checkScalarPack(LocalKind::Pointer);
}

TEST_SUITE_END();
