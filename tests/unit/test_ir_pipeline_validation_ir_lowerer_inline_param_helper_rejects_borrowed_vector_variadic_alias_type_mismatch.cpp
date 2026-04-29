#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer inline param helper rejects borrowed vector variadic alias type mismatch") {
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
  sourceInfo.referenceToVector = true;
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
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Bool;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
        infoOut.referenceToVector = true;
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

TEST_CASE("ir lowerer inline param helper rejects borrowed soa_vector variadic alias type mismatch") {
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
  sourceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  sourceInfo.referenceToVector = true;
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
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
        infoOut.referenceToVector = true;
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

TEST_CASE("ir lowerer inline param helper preserves borrowed soa_vector spread struct metadata") {
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
  sourceInfo.structTypeName =
      "/std/collections/experimental_soa_vector/SoaVector__Particle";
  sourceInfo.isArgsPack = true;
  sourceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  sourceInfo.referenceToVector = true;
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
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
        infoOut.referenceToVector = true;
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
  REQUIRE(calleeLocals.count("values") == 1u);
  const auto &valuesInfo = calleeLocals.at("values");
  CHECK(valuesInfo.structTypeName ==
        "/std/collections/experimental_soa_vector/SoaVector__Particle");
  CHECK(valuesInfo.isSoaVector);
  CHECK(valuesInfo.referenceToVector);
  CHECK(valuesInfo.argsPackElementCount == 2);
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 18u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 2u);
}

TEST_CASE("ir lowerer inline param helper materializes struct pointer variadic args packs") {
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
  leftInfo.structTypeName = "/pkg/Pair";
  callerLocals.emplace("left", leftInfo);

  primec::ir_lowerer::LocalInfo rightInfo;
  rightInfo.index = 22;
  rightInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  rightInfo.structTypeName = "/pkg/Pair";
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
        infoOut.structTypeName = "/pkg/Pair";
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
      [&](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &locals) {
        auto it = locals.find(arg.name);
        if (it == locals.end()) {
          return false;
        }
        instructions.push_back({primec::IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
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
  CHECK(calleeLocals.at("values").isArgsPack);
  CHECK(calleeLocals.at("values").argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(calleeLocals.at("values").structTypeName == "/pkg/Pair");
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

TEST_CASE("ir lowerer inline param helper aliases pure struct pointer variadic forwarding") {
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
  sourceInfo.structTypeName = "/pkg/Pair";
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
        infoOut.structTypeName = "/pkg/Pair";
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
  CHECK(calleeLocals.at("values").structTypeName == "/pkg/Pair");
  CHECK(calleeLocals.at("values").argsPackElementCount == 2);
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 18u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 2u);
}

TEST_CASE("ir lowerer inline param helper rejects struct pointer variadic alias type mismatch") {
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
  sourceInfo.structTypeName = "/pkg/Pair";
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
        infoOut.structTypeName = "/pkg/OtherPair";
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

TEST_CASE("ir lowerer inline param helper rejects borrowed array variadic alias type mismatch") {
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
  sourceInfo.referenceToArray = true;
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
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
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

TEST_CASE("ir lowerer inline param helper rejects array variadic alias type mismatch") {
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
  sourceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Array;
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
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Array;
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

TEST_CASE("ir lowerer inline param helper materializes map variadic args packs") {
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

  int32_t nextLocal = 3;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  REQUIRE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {valuesParam},
      {nullptr},
      {&firstArg, &secondArg},
      0,
      {},
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Map;
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
        instructions.push_back(
            {primec::IrOpcode::LoadLocal, static_cast<uint64_t>(arg.name == "left" ? 21 : 22)});
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
  CHECK(calleeLocals.at("values").isArgsPack);
  CHECK(calleeLocals.at("values").argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Map);
  CHECK(calleeLocals.at("values").mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(calleeLocals.at("values").mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
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

TEST_CASE("ir lowerer inline param helper aliases immutable concrete map params from map locals") {
  primec::Expr entriesParam;
  entriesParam.kind = primec::Expr::Kind::Name;
  entriesParam.isBinding = true;
  entriesParam.name = "entries";

  primec::Expr argExpr;
  argExpr.kind = primec::Expr::Kind::Name;
  argExpr.name = "values";

  constexpr const char *MapStructPath = "/std/collections/experimental_map/Map__tabc";

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo valuesInfo;
  valuesInfo.index = 21;
  valuesInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  valuesInfo.structTypeName = MapStructPath;
  valuesInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  valuesInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  callerLocals.emplace("values", valuesInfo);

  int32_t nextLocal = 3;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  REQUIRE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {entriesParam},
      {&argExpr},
      {},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.index = 7;
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
        infoOut.structTypeName = MapStructPath;
        infoOut.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        return true;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo::StringSource &,
         int32_t &,
         bool &) { return true; },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        return expr.name == "values" ? std::string(MapStructPath) : std::string();
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        FAIL("emitExpr should not materialize map locals for immutable concrete map params");
        return false;
      },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 0; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [](int32_t) {},
      error));

  CHECK(error.empty());
  REQUIRE(calleeLocals.count("entries") == 1u);
  CHECK(calleeLocals.at("entries").structTypeName == MapStructPath);
  CHECK_FALSE(instructions.empty());
}

TEST_CASE("ir lowerer inline param helper aliases mutable concrete map params from map locals") {
  primec::Expr entriesParam;
  entriesParam.kind = primec::Expr::Kind::Name;
  entriesParam.isBinding = true;
  entriesParam.name = "entries";

  primec::Expr argExpr;
  argExpr.kind = primec::Expr::Kind::Name;
  argExpr.name = "values";

  constexpr const char *MapStructPath = "/std/collections/experimental_map/Map__tabc";

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo valuesInfo;
  valuesInfo.index = 23;
  valuesInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  valuesInfo.structTypeName = MapStructPath;
  valuesInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  valuesInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  callerLocals.emplace("values", valuesInfo);

  int32_t nextLocal = 5;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  REQUIRE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {entriesParam},
      {&argExpr},
      {},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.index = 11;
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
        infoOut.isMutable = true;
        infoOut.structTypeName = MapStructPath;
        infoOut.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        return true;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo::StringSource &,
         int32_t &,
         bool &) { return true; },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        return expr.name == "values" ? std::string(MapStructPath) : std::string();
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        FAIL("emitExpr should not materialize map locals for mutable concrete map params");
        return false;
      },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 0; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [](int32_t) {},
      error));

  CHECK(error.empty());
  REQUIRE(calleeLocals.count("entries") == 1u);
  CHECK(calleeLocals.at("entries").structTypeName == MapStructPath);
  CHECK_FALSE(instructions.empty());
}

TEST_CASE("ir lowerer inline param helper accepts bare std ui mutable struct params") {
  primec::Expr commandsParam;
  commandsParam.kind = primec::Expr::Kind::Name;
  commandsParam.isBinding = true;
  commandsParam.name = "commands";

  primec::Expr argExpr;
  argExpr.kind = primec::Expr::Kind::Name;
  argExpr.name = "commands";

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo commandsInfo;
  commandsInfo.index = 21;
  commandsInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  commandsInfo.structTypeName = "/std/ui/CommandList";
  callerLocals.emplace("commands", commandsInfo);

  int32_t nextLocal = 3;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  REQUIRE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {commandsParam},
      {&argExpr},
      {},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.index = 7;
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
        infoOut.isMutable = true;
        infoOut.structTypeName = "CommandList";
        return true;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo::StringSource &,
         int32_t &,
         bool &) { return true; },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        return expr.name == "commands" ? std::string("/std/ui/CommandList") : std::string();
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        FAIL("emitExpr should not materialize mutable ui struct locals");
        return false;
      },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 0; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [](int32_t) {},
      error));

  CHECK(error.empty());
  CHECK(nextLocal == 4);
  REQUIRE(calleeLocals.count("commands") == 1u);
  CHECK(calleeLocals.at("commands").kind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(calleeLocals.at("commands").structTypeName == "CommandList");
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 21u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 3u);
}

TEST_CASE("ir lowerer inline param helper accepts bare std ui immutable struct params") {
  primec::Expr layoutParam;
  layoutParam.kind = primec::Expr::Kind::Name;
  layoutParam.isBinding = true;
  layoutParam.name = "layout";

  primec::Expr argExpr;
  argExpr.kind = primec::Expr::Kind::Name;
  argExpr.name = "layout";

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo layoutInfo;
  layoutInfo.index = 31;
  layoutInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  layoutInfo.structTypeName = "/std/ui/LayoutTree";
  callerLocals.emplace("layout", layoutInfo);

  int32_t nextLocal = 5;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  REQUIRE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {layoutParam},
      {&argExpr},
      {},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.index = 9;
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
        infoOut.structTypeName = "LayoutTree";
        return true;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo::StringSource &,
         int32_t &,
         bool &) { return true; },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        return expr.name == "layout" ? std::string("/std/ui/LayoutTree") : std::string();
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &structPath, primec::ir_lowerer::StructSlotLayoutInfo &layout) {
        CHECK(structPath == "LayoutTree");
        layout.totalSlots = 2;
        return true;
      },
      [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.name != "layout") {
          return false;
        }
        instructions.push_back({primec::IrOpcode::LoadLocal, 31u});
        return true;
      },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 13; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [](int32_t) {},
      error));

  CHECK(error.empty());
  CHECK(nextLocal == 7);
  REQUIRE(calleeLocals.count("layout") == 1u);
  CHECK(calleeLocals.at("layout").structTypeName == "LayoutTree");
  CHECK_FALSE(instructions.empty());
}

TEST_SUITE_END();
