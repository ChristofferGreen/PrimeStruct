#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer inline param helper rejects borrowed FileError variadic alias type mismatch") {
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
  sourceInfo.isFileError = true;
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
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
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

TEST_CASE("ir lowerer inline param helper rejects borrowed File handle variadic alias type mismatch") {
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
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  sourceInfo.isArgsPack = true;
  sourceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  sourceInfo.isFileHandle = true;
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
        return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
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

TEST_CASE("ir lowerer inline param helper clears stale struct paths on file-handle params") {
  primec::Expr fileParam;
  fileParam.kind = primec::Expr::Kind::Name;
  fileParam.isBinding = true;
  fileParam.name = "file";

  primec::Expr fileArg;
  fileArg.kind = primec::Expr::Kind::Name;
  fileArg.name = "source";

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.index = 18;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  sourceInfo.isFileHandle = true;
  callerLocals.emplace("source", sourceInfo);

  int32_t nextLocal = 2;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::vector<int32_t> trackedFileLocals;
  std::string error;
  int layoutLookups = 0;

  REQUIRE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {fileParam},
      {&fileArg},
      {},
      1,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
        infoOut.isFileHandle = true;
        infoOut.structTypeName = "/std/file/File<Read>";
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
        return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      },
      [&](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) {
        ++layoutLookups;
        return false;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 0; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](int32_t index) { trackedFileLocals.push_back(index); },
      error));

  CHECK(error.empty());
  CHECK(layoutLookups == 0);
  REQUIRE(calleeLocals.count("file") == 1u);
  CHECK(calleeLocals.at("file").isFileHandle);
  CHECK(calleeLocals.at("file").structTypeName.empty());
  CHECK(calleeLocals.at("file").index == 18);
  CHECK(instructions.empty());
  CHECK(trackedFileLocals.empty());
}

TEST_CASE("ir lowerer inline param helper materializes pointer File handle variadic args packs") {
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
  leftInfo.index = 31;
  leftInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  leftInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  leftInfo.isFileHandle = true;
  callerLocals.emplace("left", leftInfo);

  primec::ir_lowerer::LocalInfo rightInfo;
  rightInfo.index = 32;
  rightInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  rightInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  rightInfo.isFileHandle = true;
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
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
        infoOut.isFileHandle = true;
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
        return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      },
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return true; },
      [&](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back(
            {primec::IrOpcode::LoadLocal, static_cast<uint64_t>(arg.name == "left" ? 31 : 32)});
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
  CHECK(calleeLocals.at("values").isFileHandle);
  CHECK(calleeLocals.at("values").argsPackElementCount == 2);
  REQUIRE(instructions.size() == 8u);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[0].imm == 2u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 4u);
  CHECK(instructions[2].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[2].imm == 31u);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 5u);
  CHECK(instructions[4].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[4].imm == 32u);
  CHECK(instructions[5].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[5].imm == 6u);
  CHECK(instructions[6].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[6].imm == 4u);
  CHECK(instructions[7].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[7].imm == 3u);
}

TEST_CASE("ir lowerer inline param helper aliases pure pointer File handle variadic forwarding") {
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
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  sourceInfo.isArgsPack = true;
  sourceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  sourceInfo.isFileHandle = true;
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
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
        infoOut.isFileHandle = true;
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
        return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
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
  CHECK(calleeLocals.at("values").isFileHandle);
  CHECK(calleeLocals.at("values").argsPackElementCount == 2);
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 18u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 2u);
}

TEST_CASE("ir lowerer inline param helper rejects pointer File handle variadic alias type mismatch") {
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
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  sourceInfo.isArgsPack = true;
  sourceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  sourceInfo.isFileHandle = true;
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
        return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
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

TEST_CASE("ir lowerer inline param helper materializes pointer FileError variadic args packs") {
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
  leftInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  leftInfo.isFileError = true;
  callerLocals.emplace("left", leftInfo);

  primec::ir_lowerer::LocalInfo rightInfo;
  rightInfo.index = 22;
  rightInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  rightInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  rightInfo.isFileError = true;
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
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
        infoOut.isFileError = true;
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
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
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
  CHECK(calleeLocals.at("values").isFileError);
  CHECK(calleeLocals.at("values").argsPackElementCount == 2);
  REQUIRE(instructions.size() == 6u);
}

TEST_CASE("ir lowerer inline param helper aliases pure pointer FileError variadic forwarding") {
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
  sourceInfo.isFileError = true;
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
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
        infoOut.isFileError = true;
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
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
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
  CHECK(calleeLocals.at("values").isFileError);
  CHECK(calleeLocals.at("values").argsPackElementCount == 2);
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 18u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 2u);
}

TEST_CASE("ir lowerer inline param helper rejects pointer FileError variadic alias type mismatch") {
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
  sourceInfo.isFileError = true;
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
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
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


TEST_SUITE_END();
