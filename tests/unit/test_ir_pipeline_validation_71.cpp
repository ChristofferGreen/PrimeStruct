#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer statement binding helper classifies variadic struct reference parameters") {
  primec::Expr param;
  param.name = "values";
  param.namespacePrefix = "/pkg";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Reference<Pair>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 14;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo &infoOut) {
        for (const auto &transform : expr.transforms) {
          if (transform.name == "Reference" && transform.templateArgs.size() == 1 &&
              transform.templateArgs.front() == "Pair") {
            infoOut.structTypeName = "/pkg/Pair";
            return;
          }
        }
      },
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(info.structTypeName == "/pkg/Pair");
}

TEST_CASE("ir lowerer statement binding helper classifies variadic scalar pointer parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Pointer<i32>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 14;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic struct pointer parameters") {
  primec::Expr param;
  param.name = "values";
  param.namespacePrefix = "/pkg";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Pointer<Pair>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 14;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo &infoOut) {
        for (const auto &transform : expr.transforms) {
          if (transform.name == "Pointer" && transform.templateArgs.size() == 1 &&
              transform.templateArgs.front() == "Pair") {
            infoOut.structTypeName = "/pkg/Pair";
            return;
          }
        }
      },
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(info.structTypeName == "/pkg/Pair");
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic borrowed map parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Reference</std/collections/map<i32, i32>>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 13;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(info.referenceToMap);
  CHECK(info.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic pointer map parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Pointer</std/collections/map<i32, i32>>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 13;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(info.pointerToMap);
  CHECK(info.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic pointer vector parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Pointer<vector<i32>>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 13;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(info.pointerToVector);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic pointer soa_vector parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Pointer<soa_vector<Particle>>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 13;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(info.pointerToVector);
  CHECK(info.isSoaVector);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic soa_vector parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("soa_vector<Particle>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 13;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Vector);
  CHECK(info.isSoaVector);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic map parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("map<i32, bool>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 11;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Map);
  CHECK(info.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
}

TEST_CASE("ir lowerer statement binding helper rejects string reference parameters") {
  primec::Expr param;
  param.name = "label";

  primec::ir_lowerer::LocalInfo info;
  info.index = 4;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Reference; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::String;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return true; },
      info,
      error));
  CHECK(error == "native backend does not support string pointers or references");
}

TEST_CASE("ir lowerer statement binding helper rejects variadic string reference parameters with arg-pack diagnostic") {
  primec::Expr param;
  param.name = "labels";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Reference<string>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 4;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Reference; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::String;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return true; },
      info,
      error));
  CHECK(error == "variadic args<T> does not support string pointers or references");
}

TEST_CASE("ir lowerer statement binding helper selects uninitialized zero opcodes") {
  primec::IrOpcode mapZeroOp = primec::IrOpcode::PushI32;
  uint64_t mapZeroImm = 123;
  std::string error;
  REQUIRE(primec::ir_lowerer::selectUninitializedStorageZeroInstruction(
      primec::ir_lowerer::LocalInfo::Kind::Map,
      primec::ir_lowerer::LocalInfo::ValueKind::Unknown,
      "mapStorage",
      mapZeroOp,
      mapZeroImm,
      error));
  CHECK(mapZeroOp == primec::IrOpcode::PushI64);
  CHECK(mapZeroImm == 0);
  CHECK(error.empty());

  primec::IrOpcode floatZeroOp = primec::IrOpcode::PushI32;
  uint64_t floatZeroImm = 99;
  REQUIRE(primec::ir_lowerer::selectUninitializedStorageZeroInstruction(
      primec::ir_lowerer::LocalInfo::Kind::Value,
      primec::ir_lowerer::LocalInfo::ValueKind::Float32,
      "floatStorage",
      floatZeroOp,
      floatZeroImm,
      error));
  CHECK(floatZeroOp == primec::IrOpcode::PushF32);
  CHECK(floatZeroImm == 0);
}

TEST_CASE("ir lowerer statement binding helper rejects unknown uninitialized value kind") {
  primec::IrOpcode zeroOp = primec::IrOpcode::PushI32;
  uint64_t zeroImm = 0;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::selectUninitializedStorageZeroInstruction(
      primec::ir_lowerer::LocalInfo::Kind::Value,
      primec::ir_lowerer::LocalInfo::ValueKind::Unknown,
      "storageSlot",
      zeroOp,
      zeroImm,
      error));
  CHECK(error == "native backend requires a concrete uninitialized storage type on storageSlot");
}

TEST_CASE("ir lowerer statement binding helper emits literal string bindings") {
  primec::Expr stmt;
  stmt.name = "label";
  stmt.isBinding = true;

  primec::Expr init;
  init.kind = primec::Expr::Kind::StringLiteral;
  init.stringValue = "\"hello\"utf8";

  primec::ir_lowerer::LocalMap locals;
  int32_t nextLocal = 5;
  std::vector<primec::IrInstruction> instructions;
  int emitExprCalls = 0;
  int boundsCalls = 0;
  std::string error;
  REQUIRE(primec::ir_lowerer::emitStringStatementBindingInitializer(
      stmt,
      init,
      locals,
      nextLocal,
      instructions,
      [](const primec::Expr &) { return false; },
      [](const std::string &decoded) {
        CHECK(decoded == "hello");
        return 17;
      },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      []() { return 200; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&]() { ++boundsCalls; },
      error));
  CHECK(error.empty());
  CHECK(emitExprCalls == 0);
  CHECK(boundsCalls == 0);
  CHECK(nextLocal == 6);
  auto it = locals.find("label");
  REQUIRE(it != locals.end());
  CHECK(it->second.index == 5);
  CHECK(it->second.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::String);
  CHECK(it->second.stringSource == primec::ir_lowerer::LocalInfo::StringSource::TableIndex);
  CHECK(it->second.stringIndex == 17);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 17);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 5);
}

TEST_CASE("ir lowerer statement binding helper emits checked argv string bindings") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr stmt;
  stmt.name = "argText";
  stmt.isBinding = true;

  primec::Expr argvName;
  argvName.kind = primec::Expr::Kind::Name;
  argvName.name = "argv";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 2;
  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "at";
  init.args = {argvName, indexExpr};

  primec::ir_lowerer::LocalMap locals;
  int32_t nextLocal = 9;
  std::vector<primec::IrInstruction> instructions;
  int32_t nextTempLocal = 40;
  int boundsCalls = 0;
  std::string error;
  REQUIRE(primec::ir_lowerer::emitStringStatementBindingInitializer(
      stmt,
      init,
      locals,
      nextLocal,
      instructions,
      [](const primec::Expr &) { return true; },
      [](const std::string &) { return 0; },
      [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI32, expr.literalValue});
        return true;
      },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.kind == primec::Expr::Kind::Literal) {
          return ValueKind::Int32;
        }
        return ValueKind::Unknown;
      },
      [&]() { return nextTempLocal++; },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        return expr.kind == primec::Expr::Kind::Name && expr.name == "argv";
      },
      [&]() {
        ++boundsCalls;
        instructions.push_back({primec::IrOpcode::PushI32, 999});
      },
      error));
  CHECK(error.empty());
  CHECK(boundsCalls == 2);
  CHECK(nextLocal == 10);
  auto it = locals.find("argText");
  REQUIRE(it != locals.end());
  CHECK(it->second.index == 9);
  CHECK(it->second.isMutable);
  CHECK(it->second.valueKind == ValueKind::String);
  CHECK(it->second.stringSource == primec::ir_lowerer::LocalInfo::StringSource::ArgvIndex);
  CHECK(it->second.argvChecked);

  bool sawPushArgc = false;
  for (const auto &instruction : instructions) {
    if (instruction.op == primec::IrOpcode::PushArgc) {
      sawPushArgc = true;
      break;
    }
  }
  CHECK(sawPushArgc);
}


TEST_SUITE_END();
