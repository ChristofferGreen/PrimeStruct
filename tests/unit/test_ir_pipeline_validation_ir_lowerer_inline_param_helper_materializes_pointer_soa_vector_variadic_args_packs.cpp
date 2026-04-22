#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer inline param helper materializes pointer soa_vector variadic args packs") {
  primec::Expr valuesParam;
  valuesParam.kind = primec::Expr::Kind::Name;
  valuesParam.isBinding = true;
  valuesParam.name = "values";

  primec::Expr firstArg;
  firstArg.kind = primec::Expr::Kind::Name;
  firstArg.name = "left";
  primec::Expr secondArg;
  secondArg.kind = primec::Expr::Kind::Name;
  secondArg.name = "right";

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo leftInfo;
  leftInfo.index = 21;
  leftInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  leftInfo.pointerToVector = true;
  leftInfo.isSoaVector = true;
  leftInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  callerLocals.emplace("left", leftInfo);

  primec::ir_lowerer::LocalInfo rightInfo = leftInfo;
  rightInfo.index = 22;
  callerLocals.emplace("right", rightInfo);

  int32_t nextLocal = 3;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  REQUIRE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {valuesParam},
      {nullptr},
      {&firstArg, &secondArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
        infoOut.pointerToVector = true;
        infoOut.isSoaVector = true;
        return true;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo::StringSource &,
         int32_t &,
         bool &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 0; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [](int32_t) {},
      error));

  CHECK(error.empty());
  CHECK(nextLocal == 7);
  REQUIRE(calleeLocals.count("values") == 1u);
  CHECK(calleeLocals.at("values").isArgsPack);
  CHECK(calleeLocals.at("values").argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(calleeLocals.at("values").pointerToVector);
  CHECK(calleeLocals.at("values").isSoaVector);
  CHECK(calleeLocals.at("values").argsPackElementCount == 2);
  REQUIRE(instructions.size() == 6u);
}

TEST_CASE("ir lowerer inline param helper aliases pure pointer soa_vector variadic forwarding") {
  primec::Expr valuesParam;
  valuesParam.kind = primec::Expr::Kind::Name;
  valuesParam.isBinding = true;
  valuesParam.name = "values";

  primec::Expr spreadArg;
  spreadArg.kind = primec::Expr::Kind::Name;
  spreadArg.name = "source";
  spreadArg.isSpread = true;

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.index = 18;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  sourceInfo.isArgsPack = true;
  sourceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  sourceInfo.pointerToVector = true;
  sourceInfo.isSoaVector = true;
  sourceInfo.argsPackElementCount = 2;
  callerLocals.emplace("source", sourceInfo);

  int32_t nextLocal = 2;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  REQUIRE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {valuesParam},
      {nullptr},
      {&spreadArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
        infoOut.pointerToVector = true;
        infoOut.isSoaVector = true;
        return true;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo::StringSource &,
         int32_t &,
         bool &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 0; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [](int32_t) {},
      error));

  CHECK(error.empty());
  CHECK(nextLocal == 3);
  REQUIRE(calleeLocals.count("values") == 1u);
  CHECK(calleeLocals.at("values").argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(calleeLocals.at("values").pointerToVector);
  CHECK(calleeLocals.at("values").isSoaVector);
  CHECK(calleeLocals.at("values").argsPackElementCount == 2);
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 18u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 2u);
}

TEST_CASE("ir lowerer inline param helper rejects pointer map variadic alias type mismatch") {
  primec::Expr valuesParam;
  valuesParam.kind = primec::Expr::Kind::Name;
  valuesParam.isBinding = true;
  valuesParam.name = "values";

  primec::Expr spreadArg;
  spreadArg.kind = primec::Expr::Kind::Name;
  spreadArg.name = "source";
  spreadArg.isSpread = true;

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.index = 18;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceInfo.isArgsPack = true;
  sourceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  sourceInfo.pointerToMap = true;
  sourceInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceInfo.argsPackElementCount = 2;
  callerLocals.emplace("source", sourceInfo);

  int32_t nextLocal = 2;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {valuesParam},
      {nullptr},
      {&spreadArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
        infoOut.pointerToMap = true;
        infoOut.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Bool;
        infoOut.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        return true;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo::StringSource &,
         int32_t &,
         bool &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 0; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [](int32_t) {},
      error));

  CHECK(error == "variadic parameter type mismatch");
  CHECK(calleeLocals.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer inline param helper rejects pointer vector variadic alias type mismatch") {
  primec::Expr valuesParam;
  valuesParam.kind = primec::Expr::Kind::Name;
  valuesParam.isBinding = true;
  valuesParam.name = "values";

  primec::Expr spreadArg;
  spreadArg.kind = primec::Expr::Kind::Name;
  spreadArg.name = "source";
  spreadArg.isSpread = true;

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.index = 18;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceInfo.isArgsPack = true;
  sourceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  sourceInfo.pointerToVector = true;
  sourceInfo.argsPackElementCount = 2;
  callerLocals.emplace("source", sourceInfo);

  int32_t nextLocal = 2;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {valuesParam},
      {nullptr},
      {&spreadArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
        return true;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo::StringSource &,
         int32_t &,
         bool &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 0; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [](int32_t) {},
      error));

  CHECK(error == "variadic parameter type mismatch");
  CHECK(calleeLocals.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer inline param helper rejects pointer soa_vector variadic alias type mismatch") {
  primec::Expr valuesParam;
  valuesParam.kind = primec::Expr::Kind::Name;
  valuesParam.isBinding = true;
  valuesParam.name = "values";

  primec::Expr spreadArg;
  spreadArg.kind = primec::Expr::Kind::Name;
  spreadArg.name = "source";
  spreadArg.isSpread = true;

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.index = 18;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  sourceInfo.isArgsPack = true;
  sourceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  sourceInfo.pointerToVector = true;
  sourceInfo.isSoaVector = true;
  sourceInfo.argsPackElementCount = 2;
  callerLocals.emplace("source", sourceInfo);

  int32_t nextLocal = 2;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {valuesParam},
      {nullptr},
      {&spreadArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
        infoOut.pointerToVector = true;
        return true;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo::StringSource &,
         int32_t &,
         bool &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 0; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [](int32_t) {},
      error));

  CHECK(error == "variadic parameter type mismatch");
  CHECK(calleeLocals.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer inline param helper rejects pointer array variadic alias type mismatch") {
  primec::Expr valuesParam;
  valuesParam.kind = primec::Expr::Kind::Name;
  valuesParam.isBinding = true;
  valuesParam.name = "values";

  primec::Expr spreadArg;
  spreadArg.kind = primec::Expr::Kind::Name;
  spreadArg.name = "source";
  spreadArg.isSpread = true;

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.index = 18;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceInfo.isArgsPack = true;
  sourceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  sourceInfo.pointerToArray = true;
  sourceInfo.argsPackElementCount = 2;
  callerLocals.emplace("source", sourceInfo);

  int32_t nextLocal = 2;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {valuesParam},
      {nullptr},
      {&spreadArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
        return true;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo::StringSource &,
         int32_t &,
         bool &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 0; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [](int32_t) {},
      error));

  CHECK(error == "variadic parameter type mismatch");
  CHECK(calleeLocals.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer inline param helper rejects Buffer variadic alias type mismatch") {
  primec::Expr valuesParam;
  valuesParam.kind = primec::Expr::Kind::Name;
  valuesParam.isBinding = true;
  valuesParam.name = "values";

  primec::Expr spreadArg;
  spreadArg.kind = primec::Expr::Kind::Name;
  spreadArg.name = "source";
  spreadArg.isSpread = true;

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.index = 18;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceInfo.isArgsPack = true;
  sourceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Buffer;
  sourceInfo.argsPackElementCount = 2;
  callerLocals.emplace("source", sourceInfo);

  int32_t nextLocal = 2;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {valuesParam},
      {nullptr},
      {&spreadArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Float32;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Buffer;
        return true;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo::StringSource &,
         int32_t &,
         bool &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 0; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [](int32_t) {},
      error));

  CHECK(error == "variadic parameter type mismatch");
  CHECK(calleeLocals.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer inline param helper rejects borrowed Buffer variadic alias type mismatch") {
  primec::Expr valuesParam;
  valuesParam.kind = primec::Expr::Kind::Name;
  valuesParam.isBinding = true;
  valuesParam.name = "values";

  primec::Expr spreadArg;
  spreadArg.kind = primec::Expr::Kind::Name;
  spreadArg.name = "source";
  spreadArg.isSpread = true;

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.index = 18;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceInfo.isArgsPack = true;
  sourceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  sourceInfo.referenceToBuffer = true;
  sourceInfo.argsPackElementCount = 2;
  callerLocals.emplace("source", sourceInfo);

  int32_t nextLocal = 2;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {valuesParam},
      {nullptr},
      {&spreadArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Float32;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
        infoOut.referenceToBuffer = true;
        return true;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo::StringSource &,
         int32_t &,
         bool &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 0; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [](int32_t) {},
      error));

  CHECK(error == "variadic parameter type mismatch");
  CHECK(calleeLocals.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer inline param helper rejects pointer Buffer variadic alias type mismatch") {
  primec::Expr valuesParam;
  valuesParam.kind = primec::Expr::Kind::Name;
  valuesParam.isBinding = true;
  valuesParam.name = "values";

  primec::Expr spreadArg;
  spreadArg.kind = primec::Expr::Kind::Name;
  spreadArg.name = "source";
  spreadArg.isSpread = true;

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.index = 18;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceInfo.isArgsPack = true;
  sourceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  sourceInfo.pointerToBuffer = true;
  sourceInfo.argsPackElementCount = 2;
  callerLocals.emplace("source", sourceInfo);

  int32_t nextLocal = 2;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {valuesParam},
      {nullptr},
      {&spreadArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Float32;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
        infoOut.pointerToBuffer = true;
        return true;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo::StringSource &,
         int32_t &,
         bool &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 0; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [](int32_t) {},
      error));

  CHECK(error == "variadic parameter type mismatch");
  CHECK(calleeLocals.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer inline param helper materializes direct pointer imported SoaVector values") {
  primec::Expr valuesParam;
  valuesParam.kind = primec::Expr::Kind::Name;
  valuesParam.isBinding = true;
  valuesParam.name = "values";

  primec::Expr firstArg;
  firstArg.kind = primec::Expr::Kind::Name;
  firstArg.name = "left";
  primec::Expr secondArg;
  secondArg.kind = primec::Expr::Kind::Name;
  secondArg.name = "right";

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo leftInfo;
  leftInfo.index = 21;
  leftInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  leftInfo.pointerToVector = true;
  leftInfo.isSoaVector = true;
  leftInfo.structTypeName =
      "/std/collections/experimental_soa_vector/SoaVector__Particle";
  callerLocals.emplace("left", leftInfo);

  primec::ir_lowerer::LocalInfo rightInfo = leftInfo;
  rightInfo.index = 22;
  callerLocals.emplace("right", rightInfo);

  int32_t nextLocal = 3;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  REQUIRE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {valuesParam},
      {nullptr},
      {&firstArg, &secondArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
        infoOut.pointerToVector = true;
        infoOut.isSoaVector = true;
        infoOut.structTypeName =
            "/std/collections/experimental_soa_vector/SoaVector__Particle";
        return true;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo::StringSource &,
         int32_t &,
         bool &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return true; },
      [&](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &) {
        const auto it = callerLocals.find(arg.name);
        instructions.push_back(
            {primec::IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
        return true;
      },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 0; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [](int32_t) {},
      error));

  CHECK(error.empty());
  CHECK(nextLocal == 7);
  REQUIRE(calleeLocals.count("values") == 1u);
  CHECK(calleeLocals.at("values").argsPackElementKind ==
        primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(calleeLocals.at("values").pointerToVector);
  CHECK(calleeLocals.at("values").isSoaVector);
  CHECK(calleeLocals.at("values").argsPackElementCount == 2);
  CHECK(calleeLocals.at("values").structTypeName ==
        "/std/collections/experimental_soa_vector/SoaVector__Particle");
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


TEST_SUITE_END();
