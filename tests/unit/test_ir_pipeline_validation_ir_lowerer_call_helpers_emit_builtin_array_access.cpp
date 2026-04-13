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
  mapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  mapInfo.mapKeyKind = Kind::Unknown;
  mapInfo.mapValueKind = Kind::Int32;
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
      []() {},
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error == "native backend requires typed map bindings for at");
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  primec::Expr vectorTarget;
  vectorTarget.kind = primec::Expr::Kind::Name;
  vectorTarget.name = "v";
  primec::ir_lowerer::LocalInfo vectorInfo;
  vectorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  vectorInfo.valueKind = Kind::Int32;
  locals.clear();
  locals.emplace("v", vectorInfo);
  int arrayIndexOutOfBoundsCalls = 0;
  int nextLocal = 20;
  CHECK(primec::ir_lowerer::emitBuiltinArrayAccess(
      "at",
      vectorTarget,
      indexExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&]() { return nextLocal++; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI32, 2});
        return true;
      },
      []() {},
      []() {},
      [&]() { ++arrayIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error.empty());
  CHECK(arrayIndexOutOfBoundsCalls == 2);
  REQUIRE(instructions.size() >= 6);
  CHECK(instructions.back().op == primec::IrOpcode::LoadIndirect);
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
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            error) == Result::Continue);
  CHECK(error == "unchanged");
}

TEST_CASE("ir lowerer call helpers map key compare opcode selection") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;
  using primec::IrOpcode;

  CHECK(primec::ir_lowerer::mapKeyCompareOpcode(Kind::Int32) == IrOpcode::CmpEqI32);
  CHECK(primec::ir_lowerer::mapKeyCompareOpcode(Kind::Bool) == IrOpcode::CmpEqI32);
  CHECK(primec::ir_lowerer::mapKeyCompareOpcode(Kind::Int64) == IrOpcode::CmpEqI64);
  CHECK(primec::ir_lowerer::mapKeyCompareOpcode(Kind::UInt64) == IrOpcode::CmpEqI64);
  CHECK(primec::ir_lowerer::mapKeyCompareOpcode(Kind::Float32) == IrOpcode::CmpEqF32);
  CHECK(primec::ir_lowerer::mapKeyCompareOpcode(Kind::Float64) == IrOpcode::CmpEqF64);
}

TEST_CASE("ir lowerer call helpers resolve map lookup string keys") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;
  using Result = primec::ir_lowerer::MapLookupStringKeyResult;

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Name;
  keyExpr.name = "k";
  primec::ir_lowerer::LocalMap locals;
  std::string error;
  int32_t stringIndex = -1;

  CHECK(primec::ir_lowerer::tryResolveMapLookupStringKey(
            Kind::Int32,
            keyExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return true; },
            stringIndex,
            error) == Result::NotHandled);

  CHECK(primec::ir_lowerer::tryResolveMapLookupStringKey(
            Kind::String,
            keyExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            stringIndex,
            error) == Result::NotHandled);
  CHECK(error.empty());

  error.clear();
  CHECK(primec::ir_lowerer::tryResolveMapLookupStringKey(
            Kind::String,
            keyExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &indexOut, size_t &lengthOut) {
              indexOut = 17;
              lengthOut = 3;
              return true;
            },
            stringIndex,
            error) == Result::Resolved);
  CHECK(stringIndex == 17);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer call helpers emit map lookup string key locals") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;
  using Result = primec::ir_lowerer::MapLookupKeyLocalEmitResult;

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Name;
  keyExpr.name = "k";
  primec::ir_lowerer::LocalMap locals;
  std::string error;
  int32_t pushed = -1;
  int32_t stored = -1;

  CHECK(primec::ir_lowerer::tryEmitMapLookupStringKeyLocal(
            Kind::Int64,
            keyExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return true; },
            [&](int32_t imm) { pushed = imm; },
            [&](int32_t local) { stored = local; },
            4,
            error) == Result::NotHandled);

  CHECK(primec::ir_lowerer::tryEmitMapLookupStringKeyLocal(
            Kind::String,
            keyExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](int32_t imm) { pushed = imm; },
            [&](int32_t local) { stored = local; },
            4,
            error) == Result::NotHandled);
  CHECK(error.empty());

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitMapLookupStringKeyLocal(
            Kind::String,
            keyExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &indexOut, size_t &lengthOut) {
              indexOut = 23;
              lengthOut = 8;
              return true;
            },
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

  CHECK_FALSE(primec::ir_lowerer::emitMapLookupNonStringKeyLocal(
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
  CHECK_FALSE(primec::ir_lowerer::emitMapLookupNonStringKeyLocal(
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
  CHECK_FALSE(primec::ir_lowerer::emitMapLookupNonStringKeyLocal(
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
  CHECK_FALSE(primec::ir_lowerer::emitMapLookupNonStringKeyLocal(
      Kind::Int32,
      keyExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&](int32_t local) { stored = local; },
      6,
      error));

  error.clear();
  CHECK(primec::ir_lowerer::emitMapLookupNonStringKeyLocal(
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
  CHECK(primec::ir_lowerer::emitMapLookupTargetPointerLocal(
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
  CHECK_FALSE(primec::ir_lowerer::emitMapLookupTargetPointerLocal(
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

  CHECK(primec::ir_lowerer::emitMapLookupKeyLocal(
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
  CHECK(keyLocal == 50);
  CHECK(pushed == 13);
  CHECK(stored == 50);
  CHECK_FALSE(inferCalled);
  CHECK_FALSE(emitCalled);
  CHECK(error.empty());

  pushed = -1;
  stored = -1;
  inferCalled = false;
  emitCalled = false;
  error.clear();
  CHECK(primec::ir_lowerer::emitMapLookupKeyLocal(
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
  CHECK(keyLocal == 51);
  CHECK(pushed == -1);
  CHECK(stored == 51);
  CHECK(inferCalled);
  CHECK(emitCalled);
  CHECK(error.empty());

  pushed = -1;
  stored = -1;
  inferCalled = false;
  emitCalled = false;
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitMapLookupKeyLocal(
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
  CHECK(keyLocal == 52);
  CHECK(pushed == -1);
  CHECK(stored == -1);
  CHECK(inferCalled);
  CHECK_FALSE(emitCalled);
  CHECK(error == "native backend requires map lookup key type to match map key type");
}


TEST_SUITE_END();
