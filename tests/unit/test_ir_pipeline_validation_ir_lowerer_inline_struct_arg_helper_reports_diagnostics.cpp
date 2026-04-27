#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer inline struct arg helper reports diagnostics") {
  primec::Expr scalarArg;
  scalarArg.kind = primec::Expr::Kind::Name;
  scalarArg.name = "bad";
  const std::vector<const primec::Expr *> orderedArgs = {&scalarArg};

  primec::ir_lowerer::StructSlotLayoutInfo layout;
  layout.structPath = "/pkg/Vec";
  layout.totalSlots = 1;
  primec::ir_lowerer::StructSlotFieldInfo scalarField;
  scalarField.name = "x";
  scalarField.slotOffset = 0;
  scalarField.slotCount = 1;
  scalarField.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  layout.fields.push_back(scalarField);

  int32_t nextLocal = 3;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::emitInlineStructDefinitionArguments(
      "/pkg/Vec",
      {},
      {},
      false,
      nextLocal,
      [&](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &layoutOut) {
        layoutOut = layout;
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 0; },
      [](primec::IrOpcode, uint64_t) {},
      error));
  CHECK(error.find("argument count mismatch") != std::string::npos);

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitInlineStructDefinitionArguments(
      "/pkg/Vec",
      orderedArgs,
      {},
      false,
      nextLocal,
      [&](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &layoutOut) {
        layoutOut = layout;
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::String;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 0; },
      [](primec::IrOpcode, uint64_t) {},
      error));
  CHECK(error == "struct field type mismatch");
}

TEST_CASE("ir lowerer inline struct arg helper accepts compatible soa vector storage") {
  constexpr const char *SpecializedSoaVector =
      "/std/collections/experimental_soa_vector/SoaVector__Particle";
  const std::vector<std::pair<std::string, std::string>> compatiblePaths = {
      {SpecializedSoaVector, "/soa_vector"},
      {"/soa_vector", SpecializedSoaVector},
  };

  for (const auto &[fieldStructPath, argStructPath] : compatiblePaths) {
    primec::Expr arg;
    arg.kind = primec::Expr::Kind::Name;
    arg.name = "values";
    const std::vector<const primec::Expr *> orderedArgs = {&arg};

    primec::ir_lowerer::StructSlotLayoutInfo layout;
    layout.structPath = "/pkg/Holder";
    layout.totalSlots = 4;
    primec::ir_lowerer::StructSlotFieldInfo field;
    field.name = "values";
    field.slotOffset = 1;
    field.slotCount = 3;
    field.structPath = fieldStructPath;
    layout.fields.push_back(field);

    int32_t nextLocal = 20;
    int32_t nextTempLocal = 90;
    int copyCalls = 0;
    int32_t copiedDest = -1;
    int32_t copiedSrc = -1;
    int32_t copiedCount = -1;
    std::vector<primec::IrInstruction> instructions;
    std::string error;

    REQUIRE(primec::ir_lowerer::emitInlineStructDefinitionArguments(
        "/pkg/Holder",
        orderedArgs,
        {},
        false,
        nextLocal,
        [&](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &layoutOut) {
          layoutOut = layout;
          return true;
        },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
        },
        [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          return argStructPath;
        },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          return true;
        },
        [&](int32_t destBaseLocal, int32_t srcPtrLocal, int32_t slotCount) {
          ++copyCalls;
          copiedDest = destBaseLocal;
          copiedSrc = srcPtrLocal;
          copiedCount = slotCount;
          return true;
        },
        [&]() { return nextTempLocal++; },
        [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
        error));

    CHECK(error.empty());
    CHECK(nextLocal == 24);
    CHECK(copyCalls == 1);
    CHECK(copiedDest == 21);
    CHECK(copiedSrc == 90);
    CHECK(copiedCount == 3);
    REQUIRE(instructions.size() == 1u);
    CHECK(instructions.front().op == primec::IrOpcode::StoreLocal);
    CHECK(instructions.front().imm == 90u);
  }
}

TEST_CASE("ir lowerer inline struct arg helper accepts internal soa storage aliases") {
  const std::vector<std::pair<std::string, std::string>> compatiblePaths = {
      {"/std/collections/internal_soa_storage/SoaColumn__ti32", "SoaColumn"},
      {"SoaColumn", "/std/collections/internal_soa_storage/SoaColumn__ti32"},
      {"/std/collections/internal_soa_storage/SoaColumn__ti32", "SoaColumn<i32>"},
      {"SoaColumn<i32>", "/std/collections/internal_soa_storage/SoaColumn__ti32"},
      {"/std/collections/internal_soa_storage/SoaColumns2__ti32_i64", "SoaColumns2"},
      {"/std/collections/internal_soa_storage/SoaColumns2__ti32_i64",
       "SoaColumns2<i32, i64>"},
      {"SoaFieldView", "/std/collections/internal_soa_storage/SoaFieldView__ti32"},
      {"SoaFieldView<i32>", "/std/collections/internal_soa_storage/SoaFieldView__ti32"},
  };

  for (const auto &[fieldStructPath, argStructPath] : compatiblePaths) {
    primec::Expr arg;
    arg.kind = primec::Expr::Kind::Name;
    arg.name = "storage";
    const std::vector<const primec::Expr *> orderedArgs = {&arg};

    primec::ir_lowerer::StructSlotLayoutInfo layout;
    layout.structPath = "/pkg/Holder";
    layout.totalSlots = 5;
    primec::ir_lowerer::StructSlotFieldInfo field;
    field.name = "storage";
    field.slotOffset = 1;
    field.slotCount = 4;
    field.structPath = fieldStructPath;
    layout.fields.push_back(field);

    int32_t nextLocal = 20;
    int32_t nextTempLocal = 90;
    int copyCalls = 0;
    int32_t copiedDest = -1;
    int32_t copiedSrc = -1;
    int32_t copiedCount = -1;
    std::vector<primec::IrInstruction> instructions;
    std::string error;

    REQUIRE(primec::ir_lowerer::emitInlineStructDefinitionArguments(
        "/pkg/Holder",
        orderedArgs,
        {},
        false,
        nextLocal,
        [&](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &layoutOut) {
          layoutOut = layout;
          return true;
        },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
        },
        [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          return argStructPath;
        },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          return true;
        },
        [&](int32_t destBaseLocal, int32_t srcPtrLocal, int32_t slotCount) {
          ++copyCalls;
          copiedDest = destBaseLocal;
          copiedSrc = srcPtrLocal;
          copiedCount = slotCount;
          return true;
        },
        [&]() { return nextTempLocal++; },
        [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
        error));

    CHECK(error.empty());
    CHECK(nextLocal == 25);
    CHECK(copyCalls == 1);
    CHECK(copiedDest == 21);
    CHECK(copiedSrc == 90);
    CHECK(copiedCount == 4);
    REQUIRE(instructions.size() == 1u);
    CHECK(instructions.front().op == primec::IrOpcode::StoreLocal);
    CHECK(instructions.front().imm == 90u);
  }
}

TEST_CASE("ir lowerer inline struct arg helper rejects incompatible internal soa storage aliases") {
  primec::Expr arg;
  arg.kind = primec::Expr::Kind::Name;
  arg.name = "storage";
  const std::vector<const primec::Expr *> orderedArgs = {&arg};

  primec::ir_lowerer::StructSlotLayoutInfo layout;
  layout.structPath = "/pkg/Holder";
  layout.totalSlots = 5;
  primec::ir_lowerer::StructSlotFieldInfo field;
  field.name = "storage";
  field.slotOffset = 1;
  field.slotCount = 4;
  field.structPath = "/std/collections/internal_soa_storage/SoaColumn__ti32";
  layout.fields.push_back(field);

  int32_t nextLocal = 20;
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::emitInlineStructDefinitionArguments(
      "/pkg/Holder",
      orderedArgs,
      {},
      false,
      nextLocal,
      [&](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &layoutOut) {
        layoutOut = layout;
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return std::string("/std/collections/internal_soa_storage/SoaColumns2__ti32_i32");
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return true;
      },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 90; },
      [](primec::IrOpcode, uint64_t) {},
      error));

  CHECK(error.find("struct field type mismatch") != std::string::npos);
}

TEST_CASE("ir lowerer inline param helper emits non-struct parameter flow") {
  primec::Expr stringParam;
  stringParam.kind = primec::Expr::Kind::Name;
  stringParam.name = "text";
  primec::Expr structValueParam;
  structValueParam.kind = primec::Expr::Kind::Name;
  structValueParam.name = "value";
  primec::Expr structRefParam;
  structRefParam.kind = primec::Expr::Kind::Name;
  structRefParam.name = "ref";
  primec::Expr scalarParam;
  scalarParam.kind = primec::Expr::Kind::Name;
  scalarParam.name = "count";
  const std::vector<primec::Expr> callParams = {stringParam, structValueParam, structRefParam, scalarParam};

  primec::Expr stringArg;
  stringArg.kind = primec::Expr::Kind::Name;
  stringArg.name = "textArg";
  primec::Expr structValueArg;
  structValueArg.kind = primec::Expr::Kind::Name;
  structValueArg.name = "valueArg";
  primec::Expr structRefArg;
  structRefArg.kind = primec::Expr::Kind::Name;
  structRefArg.name = "refArg";
  primec::Expr scalarArg;
  scalarArg.kind = primec::Expr::Kind::Name;
  scalarArg.name = "countArg";
  const std::vector<const primec::Expr *> orderedArgs = {&stringArg, &structValueArg, &structRefArg, &scalarArg};

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo refArgInfo;
  refArgInfo.index = 40;
  refArgInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  refArgInfo.structTypeName = "/pkg/Vec";
  callerLocals.emplace("refArg", refArgInfo);

  int32_t nextLocal = 10;
  int32_t nextTempLocal = 90;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::vector<int32_t> fileHandleLocals;
  int emitExprCalls = 0;
  int emitStringCalls = 0;
  int copyCalls = 0;
  int32_t copiedDest = -1;
  int32_t copiedSrc = -1;
  int32_t copiedCount = -1;
  std::string error;

  REQUIRE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      callParams,
      orderedArgs,
      {},
      callParams.size(),
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &param, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        if (param.name == "text") {
          infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
          infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
          return true;
        }
        if (param.name == "value") {
          infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
          infoOut.structTypeName = "/pkg/Vec";
          return true;
        }
        if (param.name == "ref") {
          infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
          infoOut.structTypeName = "/pkg/Vec";
          return true;
        }
        if (param.name == "count") {
          infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
          infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
          infoOut.isFileHandle = true;
          return true;
        }
        return false;
      },
      [](const primec::Expr &param) { return param.name == "text"; },
      [&](const primec::Expr &arg,
          const primec::ir_lowerer::LocalMap &,
          primec::ir_lowerer::LocalInfo::StringSource &sourceOut,
          int32_t &indexOut,
          bool &argvCheckedOut) {
        ++emitStringCalls;
        CHECK(arg.name == "textArg");
        sourceOut = primec::ir_lowerer::LocalInfo::StringSource::RuntimeIndex;
        indexOut = -1;
        argvCheckedOut = true;
        return true;
      },
      [](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &) {
        if (arg.name == "valueArg" || arg.name == "refArg") {
          return std::string("/pkg/Vec");
        }
        return std::string();
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &path, primec::ir_lowerer::StructSlotLayoutInfo &layoutOut) {
        if (path != "/pkg/Vec") {
          return false;
        }
        layoutOut.structPath = path;
        layoutOut.totalSlots = 3;
        return true;
      },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        return true;
      },
      [&](int32_t destBaseLocal, int32_t srcPtrLocal, int32_t slotCount) {
        ++copyCalls;
        copiedDest = destBaseLocal;
        copiedSrc = srcPtrLocal;
        copiedCount = slotCount;
        return true;
      },
      [&]() { return nextTempLocal++; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](int32_t localIndex) { fileHandleLocals.push_back(localIndex); },
      error));

  CHECK(error.empty());
  CHECK(nextLocal == 17);
  CHECK(emitExprCalls == 2);
  CHECK(emitStringCalls == 1);
  CHECK(copyCalls == 1);
  CHECK(copiedDest == 12);
  CHECK(copiedSrc == 90);
  CHECK(copiedCount == 3);
  REQUIRE(fileHandleLocals.size() == 1u);
  CHECK(fileHandleLocals.front() == 16);
  REQUIRE(calleeLocals.size() == 4u);
  CHECK(calleeLocals.count("text") == 1u);
  CHECK(calleeLocals.count("value") == 1u);
  CHECK(calleeLocals.count("ref") == 1u);
  CHECK(calleeLocals.count("count") == 1u);
  CHECK(calleeLocals.at("text").valueKind == primec::ir_lowerer::LocalInfo::ValueKind::String);
  CHECK(calleeLocals.at("text").stringSource == primec::ir_lowerer::LocalInfo::StringSource::RuntimeIndex);
  CHECK(calleeLocals.at("value").structTypeName == "/pkg/Vec");
  CHECK(calleeLocals.at("ref").kind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(calleeLocals.at("count").isFileHandle);
  REQUIRE(instructions.size() == 9u);
  CHECK(instructions[0].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[0].imm == 10u);
  CHECK(instructions[1].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].imm == 2u);
  CHECK(instructions[2].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[2].imm == 12u);
  CHECK(instructions[3].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[3].imm == 12u);
  CHECK(instructions[4].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[4].imm == 11u);
  CHECK(instructions[5].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[5].imm == 90u);
  CHECK(instructions[6].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[6].imm == 40u);
  CHECK(instructions[7].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[7].imm == 15u);
  CHECK(instructions[8].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[8].imm == 16u);
}

TEST_CASE("ir lowerer inline param helper reports diagnostics") {
  primec::Expr param;
  param.kind = primec::Expr::Kind::Name;
  param.name = "bad";
  primec::Expr arg;
  arg.kind = primec::Expr::Kind::Name;
  arg.name = "badArg";

  int32_t nextLocal = 7;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {param},
      {},
      {},
      1,
      {},
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
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
      [](primec::IrOpcode, uint64_t) {},
      [](int32_t) {},
      error));
  CHECK(error.find("argument count mismatch") != std::string::npos);

  error.clear();
  nextLocal = 7;
  calleeLocals.clear();
  CHECK_FALSE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {param},
      {&arg},
      {},
      1,
      {},
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
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
      [](primec::IrOpcode, uint64_t) {},
      [](int32_t) {},
      error));
  CHECK(error == "native backend only supports numeric/bool, string, or struct parameters");

  error.clear();
  nextLocal = 7;
  calleeLocals.clear();
  CHECK_FALSE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {param},
      {&arg},
      {},
      1,
      {},
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
        infoOut.structTypeName = "/pkg/Expected";
        return true;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo::StringSource &,
         int32_t &,
         bool &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string("/pkg/Actual"); },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 0; },
      [](primec::IrOpcode, uint64_t) {},
      [](int32_t) {},
      error));
  CHECK(error == "struct parameter type mismatch: expected /pkg/Expected, got /pkg/Actual");
}

TEST_CASE("ir lowerer inline param helper adopts inferred vector auto params") {
  primec::Expr param;
  param.kind = primec::Expr::Kind::Name;
  param.name = "values";

  primec::Expr arg;
  arg.kind = primec::Expr::Kind::Name;
  arg.name = "items";

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo itemInfo;
  itemInfo.index = 14;
  itemInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  itemInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  itemInfo.structTypeName = "/vector";
  callerLocals.emplace("items", itemInfo);

  int32_t nextLocal = 7;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  CHECK(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {param},
      {&arg},
      {},
      1,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
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
      error,
      [](const primec::Expr &expr,
         const primec::ir_lowerer::LocalMap &locals,
         primec::ir_lowerer::LocalInfo &infoOut,
         std::string &) {
        auto it = locals.find(expr.name);
        if (it == locals.end()) {
          return false;
        }
        infoOut = it->second;
        return true;
      }));
  CHECK(error.empty());
  REQUIRE(calleeLocals.count("values") == 1);
  const auto &valuesInfo = calleeLocals.at("values");
  CHECK(valuesInfo.kind == primec::ir_lowerer::LocalInfo::Kind::Vector);
  CHECK(valuesInfo.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(valuesInfo.structTypeName == "/vector");
  REQUIRE(instructions.size() == 1);
  CHECK(instructions.front().op == primec::IrOpcode::StoreLocal);
  CHECK(instructions.front().imm == 7u);
}

TEST_CASE("ir lowerer inline param helper materializes variadic args packs") {
  primec::Expr valuesParam;
  valuesParam.kind = primec::Expr::Kind::Name;
  valuesParam.isBinding = true;
  valuesParam.name = "values";

  int32_t nextLocal = 5;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  primec::Expr firstArg;
  firstArg.kind = primec::Expr::Kind::Literal;
  firstArg.literalValue = 10;
  primec::Expr secondArg;
  secondArg.kind = primec::Expr::Kind::Literal;
  secondArg.literalValue = 20;
  const std::vector<const primec::Expr *> packedArgs = {&firstArg, &secondArg};

  REQUIRE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {valuesParam},
      {nullptr},
      packedArgs,
      0,
      {},
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.isArgsPack = true;
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
  CHECK(nextLocal == 9);
  REQUIRE(calleeLocals.count("values") == 1u);
  CHECK(calleeLocals.at("values").isArgsPack);
  CHECK(calleeLocals.at("values").kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  REQUIRE(instructions.size() == 6u);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[0].imm == 2u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 6u);
  CHECK(instructions[2].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[2].imm == 7u);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 8u);
  CHECK(instructions[4].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[4].imm == 6u);
  CHECK(instructions[5].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[5].imm == 5u);
}

TEST_CASE("ir lowerer inline param helper materializes FileError variadic args packs") {
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
        infoOut.isArgsPack = true;
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
  CHECK(calleeLocals.at("values").isFileError);
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

TEST_CASE("ir lowerer inline param helper materializes File handle variadic args packs") {
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
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
        infoOut.isArgsPack = true;
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
  CHECK(calleeLocals.at("values").isFileHandle);
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

TEST_CASE("ir lowerer inline param helper aliases spread args packs") {
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
  sourceInfo.index = 33;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceInfo.isArgsPack = true;
  callerLocals.emplace("source", sourceInfo);

  int32_t nextLocal = 4;
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
  CHECK(nextLocal == 5);
  REQUIRE(calleeLocals.count("values") == 1u);
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 33u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 4u);
}


TEST_SUITE_END();
