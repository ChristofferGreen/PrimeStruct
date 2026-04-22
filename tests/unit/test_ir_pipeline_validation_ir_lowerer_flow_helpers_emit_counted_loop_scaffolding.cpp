#include "test_ir_pipeline_validation_helpers.h"

#include <fstream>

namespace {

std::string readTextFile(const std::string &path) {
  std::ifstream file(path);
  REQUIRE(file.is_open());
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

} // namespace

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer flow helpers emit counted loop scaffolding") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto instructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, int32_t imm) { instructions.at(index).imm = imm; };

  int nextLocal = 50;
  int loopCountNegativeCalls = 0;
  primec::ir_lowerer::CountedLoopControl control;
  std::string error;
  CHECK(primec::ir_lowerer::emitCountedLoopPrologue(
      ValueKind::Int32,
      [&]() { return nextLocal++; },
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      [&]() {
        ++loopCountNegativeCalls;
        emitInstruction(primec::IrOpcode::PushI32, 999);
      },
      control,
      error));
  CHECK(error.empty());
  CHECK(loopCountNegativeCalls == 1);
  CHECK(control.counterLocal == 50);
  CHECK(control.countKind == ValueKind::Int32);
  CHECK(control.checkIndex == 6);
  CHECK(control.jumpEndIndex == 9);
  REQUIRE(instructions.size() == 10);
  CHECK(instructions[0].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[0].imm == 50);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[2].op == primec::IrOpcode::PushI32);
  CHECK(instructions[3].op == primec::IrOpcode::CmpLtI32);
  CHECK(instructions[4].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[4].imm == 6);
  CHECK(instructions[6].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[7].op == primec::IrOpcode::PushI32);
  CHECK(instructions[8].op == primec::IrOpcode::CmpGtI32);
  CHECK(instructions[9].op == primec::IrOpcode::JumpIfZero);

  primec::ir_lowerer::emitCountedLoopIterationStep(control, emitInstruction);
  REQUIRE(instructions.size() == 15);
  CHECK(instructions[10].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[10].imm == 50);
  CHECK(instructions[11].op == primec::IrOpcode::PushI32);
  CHECK(instructions[11].imm == 1);
  CHECK(instructions[12].op == primec::IrOpcode::SubI32);
  CHECK(instructions[13].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[13].imm == 50);
  CHECK(instructions[14].op == primec::IrOpcode::Jump);
  CHECK(instructions[14].imm == 6);

  primec::ir_lowerer::patchCountedLoopEnd(control, instructionCount, patchInstructionImm);
  CHECK(instructions[9].imm == 15);

  instructions.clear();
  loopCountNegativeCalls = 0;
  error.clear();
  CHECK(primec::ir_lowerer::emitCountedLoopPrologue(
      ValueKind::UInt64,
      [&]() { return nextLocal++; },
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      [&]() { ++loopCountNegativeCalls; },
      control,
      error));
  CHECK(error.empty());
  CHECK(loopCountNegativeCalls == 0);
  REQUIRE(instructions.size() == 5);
  CHECK(instructions[0].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[2].op == primec::IrOpcode::PushI64);
  CHECK(instructions[3].op == primec::IrOpcode::CmpNeI64);
  CHECK(instructions[4].op == primec::IrOpcode::JumpIfZero);
}

TEST_CASE("ir lowerer flow helpers recover bool while conditions from builtin comparisons") {
  const std::string source = readTextFile(
      "/Users/chrgre01/src/PrimeStruct/src/ir_lowerer/IrLowererLowerStatementsLoops.h");
  CHECK(source.find("if (condKind == LocalInfo::ValueKind::Unknown)") != std::string::npos);
  CHECK(source.find("if (getBuiltinComparisonName(cond, builtinComparison))") != std::string::npos);
  CHECK(source.find("condKind = LocalInfo::ValueKind::Bool;") != std::string::npos);
  CHECK(source.find("if (condKind != LocalInfo::ValueKind::Bool)") != std::string::npos);
  CHECK(source.find("error = \"while condition requires bool\"") != std::string::npos);
  CHECK(source.find("error = \"for condition requires bool\"") != std::string::npos);
}

TEST_CASE("ir lowerer binding local info recovers bool comparison initializers") {
  const std::string source = readTextFile(
      "/Users/chrgre01/src/PrimeStruct/src/ir_lowerer/IrLowererLowerStatementsBindingLocalInfo.h");
  CHECK(source.find("if (info.valueKind == LocalInfo::ValueKind::Unknown)") != std::string::npos);
  CHECK(source.find("if (getBuiltinComparisonName(init, builtinComparison))") != std::string::npos);
  CHECK(source.find("info.valueKind = LocalInfo::ValueKind::Bool;") != std::string::npos);
  CHECK(source.find("info.valueKind = LocalInfo::ValueKind::Int32;") != std::string::npos);
}

TEST_CASE("ir lowerer statement bindings prefer struct constructor full paths") {
  const std::string source = readTextFile(
      "/Users/chrgre01/src/PrimeStruct/src/ir_lowerer/IrLowererLowerStatementsBindings.h");
  CHECK(source.find("info.structTypeName = initCallee->fullPath;") != std::string::npos);
  CHECK(source.find("if (ir_lowerer::isStructDefinition(*initCallee))") != std::string::npos);
  CHECK(source.find("initStruct = initCallee->fullPath;") != std::string::npos);
  CHECK(source.find("} else if (initStruct.empty()) {") != std::string::npos);
  CHECK(source.find("inferStructReturnPathFromDefinition(") != std::string::npos);
}

TEST_CASE("ir lowerer statement bindings only rehydrate unresolved value structs") {
  const std::string source = readTextFile(
      "/Users/chrgre01/src/PrimeStruct/src/ir_lowerer/IrLowererLowerStatementsBindings.h");
  CHECK(source.find("if (info.kind == LocalInfo::Kind::Value &&") != std::string::npos);
  CHECK(source.find("info.valueKind == LocalInfo::ValueKind::Unknown &&") != std::string::npos);
  CHECK(source.find("info.structTypeName.empty())") != std::string::npos);
}

TEST_CASE("ir lowerer flow helpers emit body statements") {
  primec::Expr firstStmt;
  firstStmt.kind = primec::Expr::Kind::Name;
  firstStmt.name = "first";
  primec::Expr secondStmt;
  secondStmt.kind = primec::Expr::Kind::Name;
  secondStmt.name = "second";

  std::vector<std::string> seen;
  primec::ir_lowerer::LocalMap locals;
  CHECK(primec::ir_lowerer::emitBodyStatements(
      {firstStmt, secondStmt},
      locals,
      [&](const primec::Expr &stmt, primec::ir_lowerer::LocalMap &bodyLocals) {
        seen.push_back(stmt.name);
        if (stmt.name == "first") {
          primec::ir_lowerer::LocalInfo temp;
          temp.index = 5;
          bodyLocals.emplace("temp", temp);
          return true;
        }
        return bodyLocals.count("temp") == 1;
      }));
  REQUIRE(seen.size() == 2);
  CHECK(seen[0] == "first");
  CHECK(seen[1] == "second");

  seen.clear();
  CHECK_FALSE(primec::ir_lowerer::emitBodyStatements(
      {firstStmt, secondStmt},
      locals,
      [&](const primec::Expr &stmt, primec::ir_lowerer::LocalMap &) {
        seen.push_back(stmt.name);
        return stmt.name != "second";
      }));
  REQUIRE(seen.size() == 2);
  CHECK(seen[0] == "first");
  CHECK(seen[1] == "second");
}

TEST_CASE("ir lowerer flow helpers emit body statements with file scope") {
  primec::Expr bodyStmt;
  bodyStmt.kind = primec::Expr::Kind::Name;
  bodyStmt.name = "body";

  primec::ir_lowerer::LocalMap locals;
  std::vector<std::string> events;
  CHECK(primec::ir_lowerer::emitBodyStatementsWithFileScope(
      {bodyStmt},
      locals,
      [&](const primec::Expr &, primec::ir_lowerer::LocalMap &) {
        events.push_back("body");
        return true;
      },
      [&]() {
        events.push_back("after");
        return true;
      },
      [&]() { events.push_back("push"); },
      [&]() { events.push_back("cleanup"); },
      [&]() { events.push_back("pop"); }));
  REQUIRE(events.size() == 5);
  CHECK(events[0] == "push");
  CHECK(events[1] == "body");
  CHECK(events[2] == "after");
  CHECK(events[3] == "cleanup");
  CHECK(events[4] == "pop");

  events.clear();
  CHECK_FALSE(primec::ir_lowerer::emitBodyStatementsWithFileScope(
      {bodyStmt},
      locals,
      [&](const primec::Expr &, primec::ir_lowerer::LocalMap &) {
        events.push_back("body");
        return true;
      },
      [&]() {
        events.push_back("after");
        return false;
      },
      [&]() { events.push_back("push"); },
      [&]() { events.push_back("cleanup"); },
      [&]() { events.push_back("pop"); }));
  REQUIRE(events.size() == 3);
  CHECK(events[0] == "push");
  CHECK(events[1] == "body");
  CHECK(events[2] == "after");
}

TEST_CASE("ir lowerer flow helpers declare for-condition bindings") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  using Kind = primec::ir_lowerer::LocalInfo::Kind;

  primec::Expr initExpr;
  initExpr.kind = primec::Expr::Kind::Literal;
  initExpr.intWidth = 32;
  initExpr.literalValue = 1;

  primec::Expr bindingExpr;
  bindingExpr.kind = primec::Expr::Kind::Name;
  bindingExpr.isBinding = true;
  bindingExpr.name = "cond";
  bindingExpr.args = {initExpr};

  primec::ir_lowerer::LocalMap locals;
  int32_t nextLocal = 7;
  std::string error;
  CHECK(primec::ir_lowerer::declareForConditionBinding(
      bindingExpr,
      locals,
      nextLocal,
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &) { return Kind::Value; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, Kind) { return ValueKind::Int64; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Unknown; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      error));
  CHECK(error.empty());
  CHECK(nextLocal == 8);
  auto it = locals.find("cond");
  REQUIRE(it != locals.end());
  CHECK(it->second.index == 7);
  CHECK(it->second.valueKind == ValueKind::Int32);

  primec::Expr structBindingExpr = bindingExpr;
  structBindingExpr.name = "cond_struct";
  CHECK(primec::ir_lowerer::declareForConditionBinding(
      structBindingExpr,
      locals,
      nextLocal,
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return Kind::Value; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, Kind) { return ValueKind::Unknown; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Unknown; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string("/Vec3"); },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      error));
  auto structIt = locals.find("cond_struct");
  REQUIRE(structIt != locals.end());
  CHECK(structIt->second.structTypeName == "/Vec3");
  CHECK(structIt->second.valueKind == ValueKind::Int64);

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::declareForConditionBinding(
      bindingExpr,
      locals,
      nextLocal,
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &) { return Kind::Value; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, Kind) { return ValueKind::Int64; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Unknown; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      error));
  CHECK(error == "binding redefines existing name: cond");

  primec::Expr badArityExpr = bindingExpr;
  badArityExpr.args.clear();
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::declareForConditionBinding(
      badArityExpr,
      locals,
      nextLocal,
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &) { return Kind::Value; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, Kind) { return ValueKind::Int64; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Unknown; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      error));
  CHECK(error == "binding requires exactly one argument");
}

TEST_CASE("ir lowerer flow helpers recover bool for comparison-backed for-condition bindings") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  using Kind = primec::ir_lowerer::LocalInfo::Kind;

  primec::Expr lhs;
  lhs.kind = primec::Expr::Kind::Literal;
  lhs.intWidth = 32;
  lhs.literalValue = 1;

  primec::Expr rhs = lhs;
  rhs.literalValue = 0;

  primec::Expr comparisonExpr;
  comparisonExpr.kind = primec::Expr::Kind::Call;
  comparisonExpr.name = "less_than";
  comparisonExpr.args = {lhs, rhs};

  primec::Expr bindingExpr;
  bindingExpr.kind = primec::Expr::Kind::Name;
  bindingExpr.isBinding = true;
  bindingExpr.name = "cond_bool";
  bindingExpr.args = {comparisonExpr};

  primec::ir_lowerer::LocalMap locals;
  int32_t nextLocal = 9;
  std::string error;
  CHECK(primec::ir_lowerer::declareForConditionBinding(
      bindingExpr,
      locals,
      nextLocal,
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return Kind::Value; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, Kind) { return ValueKind::Unknown; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Unknown; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      error));
  CHECK(error.empty());
  auto it = locals.find("cond_bool");
  REQUIRE(it != locals.end());
  CHECK(it->second.kind == Kind::Value);
  CHECK(it->second.valueKind == ValueKind::Bool);
  CHECK(nextLocal == 10);
}

TEST_CASE("ir lowerer flow helpers init for-condition bindings") {
  primec::Expr initExpr;
  initExpr.kind = primec::Expr::Kind::Literal;
  initExpr.intWidth = 32;
  initExpr.literalValue = 9;

  primec::Expr bindingExpr;
  bindingExpr.kind = primec::Expr::Kind::Name;
  bindingExpr.isBinding = true;
  bindingExpr.name = "cond";
  bindingExpr.args = {initExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo info;
  info.index = 13;
  locals.emplace("cond", info);

  std::vector<primec::IrInstruction> instructions;
  int emitExprCalls = 0;
  std::string error;
  CHECK(primec::ir_lowerer::emitForConditionBindingInit(
      bindingExpr,
      locals,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        return true;
      },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      error));
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[0].imm == 13);

  primec::Expr missingBindingExpr = bindingExpr;
  missingBindingExpr.name = "missing";
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitForConditionBindingInit(
      missingBindingExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [&](primec::IrOpcode, uint64_t) {},
      error));
  CHECK(error == "binding missing local: missing");

  primec::Expr badArityExpr = bindingExpr;
  badArityExpr.args.clear();
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitForConditionBindingInit(
      badArityExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [&](primec::IrOpcode, uint64_t) {},
      error));
  CHECK(error == "binding requires exactly one argument");

  instructions.clear();
  emitExprCalls = 0;
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitForConditionBindingInit(
      bindingExpr,
      locals,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        return false;
      },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      error));
  CHECK(emitExprCalls == 1);
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer flow helpers emit vector statement helper paths") {
  using EmitResult = primec::ir_lowerer::VectorStatementHelperEmitResult;
  using Kind = primec::ir_lowerer::LocalInfo::Kind;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  auto makeTarget = [] {
    primec::Expr target;
    target.kind = primec::Expr::Kind::Name;
    target.name = "v";
    return target;
  };
  auto makeI32Literal = [](uint64_t value) {
    primec::Expr literal;
    literal.kind = primec::Expr::Kind::Literal;
    literal.intWidth = 32;
    literal.literalValue = value;
    return literal;
  };
  auto makeI64Literal = [](int64_t value) {
    primec::Expr literal;
    literal.kind = primec::Expr::Kind::Literal;
    literal.intWidth = 64;
    literal.literalValue = static_cast<uint64_t>(value);
    return literal;
  };
  auto makeU64Literal = [](uint64_t value) {
    primec::Expr literal;
    literal.kind = primec::Expr::Kind::Literal;
    literal.intWidth = 64;
    literal.isUnsigned = true;
    literal.literalValue = value;
    return literal;
  };
  auto makeCall = [&](const std::string &name, const std::vector<primec::Expr> &args) {
    primec::Expr call;
    call.kind = primec::Expr::Kind::Call;
    call.name = name;
    call.args = args;
    return call;
  };

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo vectorInfo;
  vectorInfo.kind = Kind::Vector;
  vectorInfo.valueKind = ValueKind::Int32;
  vectorInfo.isMutable = true;
  vectorInfo.index = 6;
  locals.emplace("v", vectorInfo);

  auto inferExprKindForExpr = [&](const auto &self, const primec::Expr &expr) -> ValueKind {
    if (expr.kind == primec::Expr::Kind::Literal) {
      if (expr.isUnsigned) {
        return ValueKind::UInt64;
      }
      if (expr.intWidth == 64) {
        return ValueKind::Int64;
      }
      return ValueKind::Int32;
    }
    if (expr.kind == primec::Expr::Kind::Call &&
        (expr.name == "plus" || expr.name == "minus" || expr.name == "negate")) {
      if (expr.args.empty()) {
        return ValueKind::Unknown;
      }
      if (expr.name == "negate") {
        return self(self, expr.args.front());
      }
      if (expr.args.size() < 2) {
        return ValueKind::Unknown;
      }
      const ValueKind lhs = self(self, expr.args[0]);
      const ValueKind rhs = self(self, expr.args[1]);
      if (lhs == ValueKind::Unknown || rhs == ValueKind::Unknown) {
        return ValueKind::Unknown;
      }
      if (lhs == ValueKind::UInt64 || rhs == ValueKind::UInt64) {
        return ValueKind::UInt64;
      }
      if (lhs == ValueKind::Int64 || rhs == ValueKind::Int64) {
        return ValueKind::Int64;
      }
      return ValueKind::Int32;
    }
    return ValueKind::Unknown;
  };

  auto inferExprKind = [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
    return inferExprKindForExpr(inferExprKindForExpr, expr);
  };

  auto runHelper = [&](const primec::Expr &stmt,
                       int &capacityExceededCalls,
                       int &popOnEmptyCalls,
                       int &indexOutOfBoundsCalls,
                       int &reserveNegativeCalls,
                       int &reserveExceededCalls,
                       std::vector<primec::IrInstruction> *capturedInstructions,
                       std::string &error) {
    std::vector<primec::IrInstruction> instructions;
    int32_t nextTempLocal = 20;
    const EmitResult result = primec::ir_lowerer::tryEmitVectorStatementHelper(
        stmt,
        locals,
        instructions,
        [&]() { return nextTempLocal++; },
        inferExprKind,
        [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
          if (expr.kind == primec::Expr::Kind::Literal && expr.intWidth == 32 && !expr.isUnsigned) {
            instructions.push_back({primec::IrOpcode::PushI32, expr.literalValue});
          } else {
            instructions.push_back({primec::IrOpcode::PushI64, expr.literalValue});
          }
          return true;
        },
        [](const primec::Expr &) { return false; },
        [&]() {
          ++capacityExceededCalls;
          instructions.push_back({primec::IrOpcode::PushI32, 901});
        },
        [&]() {
          ++popOnEmptyCalls;
          instructions.push_back({primec::IrOpcode::PushI32, 902});
        },
        [&]() {
          ++indexOutOfBoundsCalls;
          instructions.push_back({primec::IrOpcode::PushI32, 903});
        },
        [&]() {
          ++reserveNegativeCalls;
          instructions.push_back({primec::IrOpcode::PushI32, 904});
        },
        [&]() {
          ++reserveExceededCalls;
          instructions.push_back({primec::IrOpcode::PushI32, 905});
        },
        error);
    if (capturedInstructions != nullptr) {
      *capturedInstructions = instructions;
    }
    return result;
  };

  int capacityExceededCalls = 0;
  int popOnEmptyCalls = 0;
  int indexOutOfBoundsCalls = 0;
  int reserveNegativeCalls = 0;
  int reserveExceededCalls = 0;
  std::string error;

  CHECK(runHelper(
            makeCall("clear", {makeTarget()}),
            capacityExceededCalls,
            popOnEmptyCalls,
            indexOutOfBoundsCalls,
            reserveNegativeCalls,
            reserveExceededCalls,
            nullptr,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(capacityExceededCalls == 0);
  CHECK(popOnEmptyCalls == 0);
  CHECK(indexOutOfBoundsCalls == 0);
  CHECK(reserveNegativeCalls == 0);
  CHECK(reserveExceededCalls == 0);

  CHECK(runHelper(
            makeCall("push", {makeTarget(), makeI32Literal(7)}),
            capacityExceededCalls,
            popOnEmptyCalls,
            indexOutOfBoundsCalls,
            reserveNegativeCalls,
            reserveExceededCalls,
            nullptr,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(capacityExceededCalls == 2);

  std::vector<primec::IrInstruction> legacyAliasInstructions;
  CHECK(runHelper(
            makeCall("/vector/push", {makeTarget(), makeI32Literal(7)}),
            capacityExceededCalls,
            popOnEmptyCalls,
            indexOutOfBoundsCalls,
            reserveNegativeCalls,
            reserveExceededCalls,
            &legacyAliasInstructions,
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(legacyAliasInstructions.empty());
  CHECK(capacityExceededCalls == 2);

  CHECK(runHelper(
            makeCall("/std/collections/vector/push", {makeTarget(), makeI32Literal(7)}),
            capacityExceededCalls,
            popOnEmptyCalls,
            indexOutOfBoundsCalls,
            reserveNegativeCalls,
            reserveExceededCalls,
            nullptr,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(capacityExceededCalls == 4);

  CHECK(runHelper(
            makeCall("pop", {makeTarget()}),
            capacityExceededCalls,
            popOnEmptyCalls,
            indexOutOfBoundsCalls,
            reserveNegativeCalls,
            reserveExceededCalls,
            nullptr,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(popOnEmptyCalls == 1);

  std::vector<primec::IrInstruction> reserveInstructions;
  CHECK(runHelper(
            makeCall("reserve", {makeTarget(), makeI32Literal(12)}),
            capacityExceededCalls,
            popOnEmptyCalls,
            indexOutOfBoundsCalls,
            reserveNegativeCalls,
            reserveExceededCalls,
            &reserveInstructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(reserveNegativeCalls == 1);
  CHECK(reserveExceededCalls == 1);
  bool reserveHasHeapAlloc = false;
  bool reserveHasAllocFailureCheck = false;
  for (const auto &inst : reserveInstructions) {
    if (inst.op == primec::IrOpcode::HeapAlloc) {
      reserveHasHeapAlloc = true;
    }
    if (inst.op == primec::IrOpcode::CmpEqI64) {
      reserveHasAllocFailureCheck = true;
    }
  }
  CHECK(reserveHasHeapAlloc);
  CHECK(reserveHasAllocFailureCheck);

  const int reserveNegativeBaseline = reserveNegativeCalls;
  const int reserveExceededBaseline = reserveExceededCalls;

  error.clear();
  CHECK(runHelper(
            makeCall("reserve",
                     {makeTarget(), makeCall("plus", {makeI32Literal(200), makeI32Literal(57)})}),
            capacityExceededCalls,
            popOnEmptyCalls,
            indexOutOfBoundsCalls,
            reserveNegativeCalls,
            reserveExceededCalls,
            nullptr,
            error) == EmitResult::Error);
  CHECK(error == "vector reserve exceeds local capacity limit (256)");
  CHECK(reserveNegativeCalls == reserveNegativeBaseline);
  CHECK(reserveExceededCalls == reserveExceededBaseline);

  error.clear();
  CHECK(runHelper(
            makeCall("reserve",
                     {makeTarget(), makeCall("minus", {makeI32Literal(1), makeI32Literal(2)})}),
            capacityExceededCalls,
            popOnEmptyCalls,
            indexOutOfBoundsCalls,
            reserveNegativeCalls,
            reserveExceededCalls,
            nullptr,
            error) == EmitResult::Error);
  CHECK(error == "vector reserve expects non-negative capacity");
  CHECK(reserveNegativeCalls == reserveNegativeBaseline);
  CHECK(reserveExceededCalls == reserveExceededBaseline);

  error.clear();
  CHECK(runHelper(
            makeCall("reserve",
                     {makeTarget(),
                      makeCall("plus",
                               {makeU64Literal(std::numeric_limits<uint64_t>::max()), makeU64Literal(1)})}),
            capacityExceededCalls,
            popOnEmptyCalls,
            indexOutOfBoundsCalls,
            reserveNegativeCalls,
            reserveExceededCalls,
            nullptr,
            error) == EmitResult::Error);
  CHECK(error == "vector reserve literal expression overflow");
  CHECK(reserveNegativeCalls == reserveNegativeBaseline);
  CHECK(reserveExceededCalls == reserveExceededBaseline);

  error.clear();
  CHECK(runHelper(
            makeCall("reserve",
                     {makeTarget(),
                      makeCall("negate", {makeI64Literal(std::numeric_limits<int64_t>::min())})}),
            capacityExceededCalls,
            popOnEmptyCalls,
            indexOutOfBoundsCalls,
            reserveNegativeCalls,
            reserveExceededCalls,
            nullptr,
            error) == EmitResult::Error);
  CHECK(error == "vector reserve literal expression overflow");
  CHECK(reserveNegativeCalls == reserveNegativeBaseline);
  CHECK(reserveExceededCalls == reserveExceededBaseline);

  error.clear();
  CHECK(runHelper(
            makeCall("remove_at", {makeTarget(), makeI32Literal(1)}),
            capacityExceededCalls,
            popOnEmptyCalls,
            indexOutOfBoundsCalls,
            reserveNegativeCalls,
            reserveExceededCalls,
            nullptr,
            error) == EmitResult::Emitted);
  CHECK(error.empty());

  error.clear();
  CHECK(runHelper(
            makeCall("remove_swap", {makeTarget(), makeI32Literal(1)}),
            capacityExceededCalls,
            popOnEmptyCalls,
            indexOutOfBoundsCalls,
            reserveNegativeCalls,
            reserveExceededCalls,
            nullptr,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(indexOutOfBoundsCalls == 5);

  std::vector<primec::IrInstruction> pushInstructions;
  error.clear();
  CHECK(runHelper(
            makeCall("push", {makeTarget(), makeI32Literal(7)}),
            capacityExceededCalls,
            popOnEmptyCalls,
            indexOutOfBoundsCalls,
            reserveNegativeCalls,
            reserveExceededCalls,
            &pushInstructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  bool pushHasHeapAlloc = false;
  bool pushHasAllocFailureCheck = false;
  for (const auto &inst : pushInstructions) {
    if (inst.op == primec::IrOpcode::HeapAlloc) {
      pushHasHeapAlloc = true;
    }
    if (inst.op == primec::IrOpcode::CmpEqI64) {
      pushHasAllocFailureCheck = true;
    }
  }
  CHECK(pushHasHeapAlloc);
  CHECK(pushHasAllocFailureCheck);
}

TEST_SUITE_END();
