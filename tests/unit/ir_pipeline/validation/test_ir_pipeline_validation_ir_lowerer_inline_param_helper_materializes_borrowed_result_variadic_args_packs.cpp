#include "test_ir_pipeline_validation_callback_factories.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer inline param helper materializes borrowed Result variadic args packs") {
  primec::Expr valuesParam = primec::validation_test_support::makeBindingNameExpr("values");

  primec::Expr firstArg;
  firstArg.kind = primec::Expr::Kind::Name;
  firstArg.name = "left";
  primec::Expr secondArg;
  secondArg.kind = primec::Expr::Kind::Name;
  secondArg.name = "right";

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo leftInfo;
  leftInfo.index = 21;
  leftInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  leftInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  leftInfo.isResult = true;
  leftInfo.resultHasValue = true;
  leftInfo.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  leftInfo.resultErrorType = "ParseError";
  callerLocals.emplace("left", leftInfo);

  primec::ir_lowerer::LocalInfo rightInfo;
  rightInfo.index = 22;
  rightInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  rightInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  rightInfo.isResult = true;
  rightInfo.resultHasValue = true;
  rightInfo.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  rightInfo.resultErrorType = "ParseError";
  callerLocals.emplace("right", rightInfo);

  int32_t nextLocal = 3;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  REQUIRE(primec::validation_test_support::emitInlineParamsInert(
      {valuesParam},
      {nullptr},
      {&firstArg, &secondArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
        infoOut.isResult = true;
        infoOut.resultHasValue = true;
        infoOut.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.resultErrorType = "ParseError";
        return true;
      },
      instructions,
      error));

  CHECK(error.empty());
  CHECK(nextLocal == 7);
  REQUIRE(calleeLocals.count("values") == 1u);
  CHECK(calleeLocals.at("values").isArgsPack);
  CHECK(calleeLocals.at("values").argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(calleeLocals.at("values").isResult);
  CHECK(calleeLocals.at("values").resultHasValue);
  CHECK(calleeLocals.at("values").resultValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(calleeLocals.at("values").resultErrorType == "ParseError");
  CHECK(calleeLocals.at("values").argsPackElementCount == 2);
  REQUIRE(instructions.size() == 8u);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[0].imm == 2u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 4u);
  CHECK(instructions[2].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[2].imm == 21u);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 5u);
  CHECK(instructions[4].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[4].imm == 22u);
  CHECK(instructions[5].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[5].imm == 6u);
  CHECK(instructions[6].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[6].imm == 4u);
  CHECK(instructions[7].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[7].imm == 3u);
}

TEST_CASE("ir lowerer inline param helper aliases pure borrowed Result variadic forwarding") {
  primec::Expr valuesParam = primec::validation_test_support::makeBindingNameExpr("values");

  primec::Expr spreadArg = primec::validation_test_support::makeSpreadNameExpr("source");

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.index = 18;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  sourceInfo.isArgsPack = true;
  sourceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  sourceInfo.isResult = true;
  sourceInfo.resultHasValue = true;
  sourceInfo.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceInfo.resultErrorType = "ParseError";
  sourceInfo.argsPackElementCount = 2;
  callerLocals.emplace("source", sourceInfo);

  int32_t nextLocal = 2;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  REQUIRE(primec::validation_test_support::emitInlineParamsInert(
      {valuesParam},
      {nullptr},
      {&spreadArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
        infoOut.isResult = true;
        infoOut.resultHasValue = true;
        infoOut.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.resultErrorType = "ParseError";
        return true;
      },
      instructions,
      error));

  CHECK(error.empty());
  CHECK(nextLocal == 3);
  REQUIRE(calleeLocals.count("values") == 1u);
  CHECK(calleeLocals.at("values").argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(calleeLocals.at("values").isResult);
  CHECK(calleeLocals.at("values").resultErrorType == "ParseError");
  CHECK(calleeLocals.at("values").argsPackElementCount == 2);
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 18u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 2u);
}

TEST_CASE("ir lowerer inline param helper rejects borrowed Result variadic alias type mismatch") {
  primec::Expr valuesParam = primec::validation_test_support::makeBindingNameExpr("values");

  primec::Expr spreadArg = primec::validation_test_support::makeSpreadNameExpr("source");

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.index = 18;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  sourceInfo.isArgsPack = true;
  sourceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  sourceInfo.isResult = true;
  sourceInfo.resultHasValue = true;
  sourceInfo.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceInfo.resultErrorType = "ParseError";
  sourceInfo.argsPackElementCount = 2;
  callerLocals.emplace("source", sourceInfo);

  int32_t nextLocal = 2;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  CHECK_FALSE(primec::validation_test_support::emitInlineParamsInert(
      {valuesParam},
      {nullptr},
      {&spreadArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
        infoOut.isResult = true;
        infoOut.resultHasValue = true;
        infoOut.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.resultErrorType = "OtherError";
        return true;
      },
      instructions,
      error));

  CHECK(error == "variadic parameter type mismatch");
  CHECK(calleeLocals.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer inline param helper materializes pointer Result variadic args packs") {
  primec::Expr valuesParam = primec::validation_test_support::makeBindingNameExpr("values");

  primec::Expr firstArg;
  firstArg.kind = primec::Expr::Kind::Name;
  firstArg.name = "left";
  primec::Expr secondArg;
  secondArg.kind = primec::Expr::Kind::Name;
  secondArg.name = "right";

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo leftInfo;
  leftInfo.index = 31;
  leftInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  leftInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  leftInfo.isResult = true;
  leftInfo.resultHasValue = true;
  leftInfo.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  leftInfo.resultErrorType = "ParseError";
  callerLocals.emplace("left", leftInfo);

  primec::ir_lowerer::LocalInfo rightInfo;
  rightInfo.index = 32;
  rightInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  rightInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  rightInfo.isResult = true;
  rightInfo.resultHasValue = true;
  rightInfo.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  rightInfo.resultErrorType = "ParseError";
  callerLocals.emplace("right", rightInfo);

  int32_t nextLocal = 3;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  REQUIRE(primec::validation_test_support::emitInlineParamsInert(
      {valuesParam},
      {nullptr},
      {&firstArg, &secondArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
        infoOut.isResult = true;
        infoOut.resultHasValue = true;
        infoOut.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.resultErrorType = "ParseError";
        return true;
      },
      instructions,
      error));

  CHECK(error.empty());
  CHECK(nextLocal == 7);
  REQUIRE(calleeLocals.count("values") == 1u);
  CHECK(calleeLocals.at("values").isArgsPack);
  CHECK(calleeLocals.at("values").argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(calleeLocals.at("values").isResult);
  CHECK(calleeLocals.at("values").resultHasValue);
  CHECK(calleeLocals.at("values").resultValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(calleeLocals.at("values").resultErrorType == "ParseError");
  CHECK(calleeLocals.at("values").argsPackElementCount == 2);
  REQUIRE(instructions.size() == 6u);
}

TEST_CASE("ir lowerer inline param helper aliases pure pointer Result variadic forwarding") {
  primec::Expr valuesParam = primec::validation_test_support::makeBindingNameExpr("values");

  primec::Expr spreadArg = primec::validation_test_support::makeSpreadNameExpr("source");

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.index = 18;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  sourceInfo.isArgsPack = true;
  sourceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  sourceInfo.isResult = true;
  sourceInfo.resultHasValue = true;
  sourceInfo.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceInfo.resultErrorType = "ParseError";
  sourceInfo.argsPackElementCount = 2;
  callerLocals.emplace("source", sourceInfo);

  int32_t nextLocal = 2;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  REQUIRE(primec::validation_test_support::emitInlineParamsInert(
      {valuesParam},
      {nullptr},
      {&spreadArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
        infoOut.isResult = true;
        infoOut.resultHasValue = true;
        infoOut.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.resultErrorType = "ParseError";
        return true;
      },
      instructions,
      error));

  CHECK(error.empty());
  CHECK(nextLocal == 3);
  REQUIRE(calleeLocals.count("values") == 1u);
  CHECK(calleeLocals.at("values").argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(calleeLocals.at("values").isResult);
  CHECK(calleeLocals.at("values").resultErrorType == "ParseError");
  CHECK(calleeLocals.at("values").argsPackElementCount == 2);
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 18u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 2u);
}

TEST_CASE("ir lowerer inline param helper rejects pointer Result variadic alias type mismatch") {
  primec::Expr valuesParam = primec::validation_test_support::makeBindingNameExpr("values");

  primec::Expr spreadArg = primec::validation_test_support::makeSpreadNameExpr("source");

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.index = 18;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  sourceInfo.isArgsPack = true;
  sourceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  sourceInfo.isResult = true;
  sourceInfo.resultHasValue = true;
  sourceInfo.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceInfo.resultErrorType = "ParseError";
  sourceInfo.argsPackElementCount = 2;
  callerLocals.emplace("source", sourceInfo);

  int32_t nextLocal = 2;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  CHECK_FALSE(primec::validation_test_support::emitInlineParamsInert(
      {valuesParam},
      {nullptr},
      {&spreadArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
        infoOut.isResult = true;
        infoOut.resultHasValue = true;
        infoOut.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.resultErrorType = "OtherError";
        return true;
      },
      instructions,
      error));

  CHECK(error == "variadic parameter type mismatch");
  CHECK(calleeLocals.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer inline param helper rejects variadic pointer string packs with arg-pack diagnostic") {
  primec::Expr valuesParam = primec::validation_test_support::makeBindingNameExpr("values");

  primec::Expr firstArg;
  firstArg.kind = primec::Expr::Kind::Name;
  firstArg.name = "left";

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo leftInfo;
  leftInfo.index = 31;
  leftInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  leftInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  callerLocals.emplace("left", leftInfo);

  int32_t nextLocal = 3;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  CHECK_FALSE(primec::validation_test_support::emitInlineParamsInert(
      {valuesParam},
      {nullptr},
      {&firstArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
        return true;
      },
      instructions,
      error));

  CHECK(error == "variadic args<T> does not support string pointers or references");
  CHECK(calleeLocals.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer inline param helper rejects variadic string reference packs with arg-pack diagnostic") {
  primec::Expr valuesParam = primec::validation_test_support::makeBindingNameExpr("values");

  primec::Expr firstArg;
  firstArg.kind = primec::Expr::Kind::Name;
  firstArg.name = "left";

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo leftInfo;
  leftInfo.index = 31;
  leftInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  leftInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  callerLocals.emplace("left", leftInfo);

  int32_t nextLocal = 3;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  CHECK_FALSE(primec::validation_test_support::emitInlineParamsInert(
      {valuesParam},
      {nullptr},
      {&firstArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
        return true;
      },
      instructions,
      error));

  CHECK(error == "variadic args<T> does not support string pointers or references");
  CHECK(calleeLocals.empty());
  CHECK(instructions.empty());
}


TEST_SUITE_END();
