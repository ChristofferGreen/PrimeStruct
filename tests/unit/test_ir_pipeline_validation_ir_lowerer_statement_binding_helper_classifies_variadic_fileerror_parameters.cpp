#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer statement binding helper classifies variadic FileError parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("FileError");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 10;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return false; },
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
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.isFileError);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic File handle parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("File<Write>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 11;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return false; },
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
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.isFileHandle);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer statement binding helper classifies namespaced File handle parameters") {
  primec::Expr param;
  param.name = "file";
  primec::Transform fileTransform;
  fileTransform.name = "/std/file/File";
  fileTransform.templateArgs.push_back("Write");
  param.transforms.push_back(fileTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 14;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return false; },
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
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.isFileHandle);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(info.structTypeName.empty());
}

TEST_CASE("ir lowerer statement binding helper classifies variadic borrowed File handle parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Reference<File<Write>>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 12;
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
  CHECK(info.isFileHandle);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic pointer File handle parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Pointer<File<Write>>");
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
  CHECK(info.isFileHandle);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic borrowed FileError parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Reference<FileError>");
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
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(info.isFileError);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic pointer FileError parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Pointer<FileError>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 12;
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
  CHECK(info.isFileError);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic borrowed Result parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Reference<Result<i32, ParseError>>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 12;
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
  CHECK(info.isResult);
  CHECK(info.resultHasValue);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(info.resultValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.resultErrorType == "ParseError");
}

TEST_CASE("ir lowerer statement binding helper classifies variadic pointer Result parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Pointer<Result<i32, ParseError>>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 12;
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
  CHECK(info.isResult);
  CHECK(info.resultHasValue);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(info.resultValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.resultErrorType == "ParseError");
}

TEST_CASE("ir lowerer statement binding helper classifies variadic vector parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("vector<i32>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 10;
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
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic array parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("array<i32>");
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
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic borrowed array parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Reference<array<i32>>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 12;
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
  CHECK(info.referenceToArray);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic pointer array parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Pointer<array<i32>>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 12;
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
  CHECK(info.pointerToArray);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic Buffer parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Buffer<i32>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 12;
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
  CHECK(info.argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Buffer);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic borrowed Buffer parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Reference<Buffer<i32>>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 12;
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
  CHECK(info.referenceToBuffer);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic pointer Buffer parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Pointer<Buffer<i32>>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 12;
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
  CHECK(info.pointerToBuffer);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic scalar reference parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Reference<i32>");
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
  CHECK_FALSE(info.referenceToArray);
  CHECK_FALSE(info.referenceToMap);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}


TEST_SUITE_END();
