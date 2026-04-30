#include "test_ir_pipeline_validation_helpers.h"

namespace {
const primec::Definition *noResolveDefinition(const primec::Expr &) {
  return nullptr;
}
} // namespace

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer result helpers try emit Result.why method calls") {
  using EmitResult = primec::ir_lowerer::ResultWhyMethodCallEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.isMethodCall = true;
  expr.name = "why";
  expr.namespacePrefix = "/pkg";
  primec::Expr resultType;
  resultType.kind = primec::Expr::Kind::Name;
  resultType.name = "Result";
  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Name;
  valueExpr.name = "res";
  expr.args = {resultType, valueExpr};

  primec::Definition i32WhyDef;
  i32WhyDef.fullPath = "/i32/why";
  i32WhyDef.parameters.resize(1);
  std::unordered_map<std::string, const primec::Definition *> defMap{
      {"/i32/why", &i32WhyDef},
  };

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::IrInstruction> instructions;
  int32_t tempCounter = 0;
  int allocCounter = 0;
  bool inlineCalled = false;
  bool fileErrorCalled = false;
  std::string error;

  CHECK(primec::ir_lowerer::tryEmitResultWhyCall(
            expr,
            locals,
            defMap,
            tempCounter,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::ResultExprInfo &out) {
              out = primec::ir_lowerer::ResultExprInfo{};
              out.isResult = true;
              out.hasValue = true;
              out.errorType = "AnyInt";
              return true;
            },
            noResolveDefinition,
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&]() {
              ++allocCounter;
              return allocCounter;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](const std::string &) { return 0; },
            [](const std::string &, const std::string &, std::string &) { return false; },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &returnInfoOut) {
              if (path != "/i32/why") {
                return false;
              }
              returnInfoOut = primec::ir_lowerer::ReturnInfo{};
              returnInfoOut.kind = ValueKind::String;
              return true;
            },
            [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
            [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return false; },
            [](const std::string &typeName) {
              if (typeName == "AnyInt") {
                return ValueKind::Int32;
              }
              return ValueKind::Unknown;
            },
            [&](const primec::Expr &, const primec::Definition &callee, const primec::ir_lowerer::LocalMap &) {
              inlineCalled = true;
              CHECK(callee.fullPath == "/i32/why");
              return true;
            },
            [&](int32_t) {
              fileErrorCalled = true;
              return true;
            },
            nullptr,
            &instructions,
            error) ==
        EmitResult::Emitted);
  CHECK(inlineCalled);
  CHECK_FALSE(fileErrorCalled);

  primec::Expr nonResultExpr = expr;
  nonResultExpr.name = "map";
  CHECK(primec::ir_lowerer::tryEmitResultWhyCall(
            nonResultExpr,
            locals,
            defMap,
            tempCounter,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::ResultExprInfo &) {
              return false;
            },
            noResolveDefinition,
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&]() { return 0; },
            [&](primec::IrOpcode, uint64_t) {},
            [&](const std::string &) { return 0; },
            [](const std::string &, const std::string &, std::string &) { return false; },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; },
            [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
            [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return false; },
            [](const std::string &) { return ValueKind::Unknown; },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              return false;
            },
            [&](int32_t) { return false; },
            nullptr,
            nullptr,
            error) ==
        EmitResult::NotHandled);

  primec::Expr invalidArgExpr = expr;
  invalidArgExpr.args = {resultType};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitResultWhyCall(
            invalidArgExpr,
            locals,
            defMap,
            tempCounter,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::ResultExprInfo &) {
              return false;
            },
            noResolveDefinition,
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&]() { return 0; },
            [&](primec::IrOpcode, uint64_t) {},
            [&](const std::string &) { return 0; },
            [](const std::string &, const std::string &, std::string &) { return false; },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; },
            [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
            [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return false; },
            [](const std::string &) { return ValueKind::Unknown; },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              return false;
            },
            [&](int32_t) { return false; },
            nullptr,
            nullptr,
            error) ==
        EmitResult::Error);
  CHECK(error == "Result.why requires exactly one argument");
}

TEST_CASE("ir lowerer result helpers dispatch Result.why and FileError.why") {
  using EmitResult = primec::ir_lowerer::ResultWhyDispatchEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr resultWhyExpr;
  resultWhyExpr.kind = primec::Expr::Kind::Call;
  resultWhyExpr.isMethodCall = true;
  resultWhyExpr.name = "why";
  resultWhyExpr.namespacePrefix = "/pkg";
  primec::Expr resultType;
  resultType.kind = primec::Expr::Kind::Name;
  resultType.name = "Result";
  primec::Expr resultValue;
  resultValue.kind = primec::Expr::Kind::Name;
  resultValue.name = "res";
  resultWhyExpr.args = {resultType, resultValue};

  primec::Definition i32WhyDef;
  i32WhyDef.fullPath = "/i32/why";
  i32WhyDef.parameters.resize(1);
  std::unordered_map<std::string, const primec::Definition *> defMap{
      {"/i32/why", &i32WhyDef},
  };

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::IrInstruction> instructions;
  int32_t tempCounter = 0;
  int allocCounter = 0;
  bool inlineCalled = false;
  bool fileErrorCalled = false;
  std::string error;

  CHECK(primec::ir_lowerer::tryEmitResultWhyDispatchCall(
            resultWhyExpr,
            locals,
            defMap,
            tempCounter,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::ResultExprInfo &out) {
              out = primec::ir_lowerer::ResultExprInfo{};
              out.isResult = true;
              out.hasValue = true;
              out.errorType = "AnyInt";
              return true;
            },
            noResolveDefinition,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&]() {
              ++allocCounter;
              return allocCounter;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [](const std::string &) { return 0; },
            [](const std::string &, const std::string &, std::string &) { return false; },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &returnInfoOut) {
              if (path != "/i32/why") {
                return false;
              }
              returnInfoOut = primec::ir_lowerer::ReturnInfo{};
              returnInfoOut.kind = ValueKind::String;
              return true;
            },
            [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
            [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return false; },
            [](const std::string &typeName) {
              if (typeName == "AnyInt") {
                return ValueKind::Int32;
              }
              return ValueKind::Unknown;
            },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              inlineCalled = true;
              return true;
            },
            [&](int32_t) {
              fileErrorCalled = true;
              return true;
            },
            nullptr,
            &instructions,
            error) ==
        EmitResult::Emitted);
  CHECK(inlineCalled);
  CHECK_FALSE(fileErrorCalled);

  primec::Definition fileErrorWhyDef;
  fileErrorWhyDef.fullPath = "/std/file/FileError/why";
  fileErrorWhyDef.parameters.resize(1);
  std::unordered_map<std::string, const primec::Definition *> fileErrorDefMap{
      {"/std/file/FileError/why", &fileErrorWhyDef},
  };

  inlineCalled = false;
  fileErrorCalled = false;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitResultWhyDispatchCall(
            resultWhyExpr,
            locals,
            fileErrorDefMap,
            tempCounter,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::ResultExprInfo &out) {
              out = primec::ir_lowerer::ResultExprInfo{};
              out.isResult = true;
              out.hasValue = false;
              out.errorType = "FileError";
              return true;
            },
            noResolveDefinition,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            [&](primec::IrOpcode, uint64_t) {},
            [](const std::string &) { return 0; },
            [](const std::string &, const std::string &, std::string &) { return false; },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &returnInfoOut) {
              if (path != "/std/file/FileError/why") {
                return false;
              }
              returnInfoOut = primec::ir_lowerer::ReturnInfo{};
              returnInfoOut.kind = ValueKind::String;
              return true;
            },
            [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
            [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return false; },
            [](const std::string &typeName) {
              if (typeName == "FileError") {
                return ValueKind::Int32;
              }
              return ValueKind::Unknown;
            },
            [&](const primec::Expr &, const primec::Definition &callee, const primec::ir_lowerer::LocalMap &) {
              inlineCalled = true;
              CHECK(callee.fullPath == "/std/file/FileError/why");
              return true;
            },
            [&](int32_t) {
              fileErrorCalled = true;
              return true;
            },
            nullptr,
            &instructions,
            error) ==
        EmitResult::Emitted);
  CHECK_FALSE(inlineCalled);
  CHECK(fileErrorCalled);

  inlineCalled = false;
  fileErrorCalled = false;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitResultWhyDispatchCall(
            resultWhyExpr,
            locals,
            {},
            tempCounter,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::ResultExprInfo &out) {
              out = primec::ir_lowerer::ResultExprInfo{};
              out.isResult = true;
              out.hasValue = false;
              out.errorType = "/std/file/FileError";
              return true;
            },
            noResolveDefinition,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            [&](primec::IrOpcode, uint64_t) {},
            [](const std::string &) { return 0; },
            [](const std::string &, const std::string &, std::string &) { return false; },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; },
            [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
            [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return false; },
            [](const std::string &) { return ValueKind::Unknown; },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              inlineCalled = true;
              return true;
            },
            [&](int32_t) {
              fileErrorCalled = true;
              return true;
            },
            nullptr,
            &instructions,
            error) ==
        EmitResult::Emitted);
  CHECK_FALSE(inlineCalled);
  CHECK(fileErrorCalled);

  primec::Expr fileErrorWhyExpr;
  fileErrorWhyExpr.kind = primec::Expr::Kind::Call;
  fileErrorWhyExpr.isMethodCall = true;
  fileErrorWhyExpr.name = "why";
  primec::Expr fileErrorName;
  fileErrorName.kind = primec::Expr::Kind::Name;
  fileErrorName.name = "fileErr";
  fileErrorWhyExpr.args = {fileErrorName};

  primec::ir_lowerer::LocalMap fileErrorLocals;
  primec::ir_lowerer::LocalInfo fileErrorInfo;
  fileErrorInfo.index = 33;
  fileErrorInfo.isFileError = true;
  fileErrorLocals.emplace("fileErr", fileErrorInfo);
  int32_t emittedFileErrorLocal = -1;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitResultWhyDispatchCall(
            fileErrorWhyExpr,
            fileErrorLocals,
            {},
            tempCounter,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::ResultExprInfo &) {
              return false;
            },
            noResolveDefinition,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            []() { return 0; },
            [&](primec::IrOpcode, uint64_t) {},
            [](const std::string &) { return 0; },
            [](const std::string &, const std::string &, std::string &) { return false; },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; },
            [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
            [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return false; },
            [](const std::string &) { return ValueKind::Unknown; },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              return false;
            },
            [&](int32_t errorLocal) {
              emittedFileErrorLocal = errorLocal;
              return true;
            },
            nullptr,
            nullptr,
            error) ==
        EmitResult::Emitted);
  CHECK(emittedFileErrorLocal == 33);

  primec::Expr nonWhyExpr;
  nonWhyExpr.kind = primec::Expr::Kind::Call;
  nonWhyExpr.isMethodCall = true;
  nonWhyExpr.name = "map";
  nonWhyExpr.args = {resultType};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitResultWhyDispatchCall(
            nonWhyExpr,
            {},
            {},
            tempCounter,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::ResultExprInfo &) {
              return false;
            },
            noResolveDefinition,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            []() { return 0; },
            [](primec::IrOpcode, uint64_t) {},
            [](const std::string &) { return 0; },
            [](const std::string &, const std::string &, std::string &) { return false; },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; },
            [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
            [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return false; },
            [](const std::string &) { return ValueKind::Unknown; },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              return false;
            },
            [](int32_t) { return false; },
            nullptr,
            nullptr,
            error) ==
        EmitResult::NotHandled);

  primec::Expr badResultWhyExpr = resultWhyExpr;
  badResultWhyExpr.args = {resultType};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitResultWhyDispatchCall(
            badResultWhyExpr,
            {},
            {},
            tempCounter,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::ResultExprInfo &) {
              return false;
            },
            noResolveDefinition,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            []() { return 0; },
            [](primec::IrOpcode, uint64_t) {},
            [](const std::string &) { return 0; },
            [](const std::string &, const std::string &, std::string &) { return false; },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; },
            [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
            [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return false; },
            [](const std::string &) { return ValueKind::Unknown; },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              return false;
            },
            [](int32_t) { return false; },
            nullptr,
            nullptr,
            error) ==
        EmitResult::Error);
  CHECK(error == "Result.why requires exactly one argument");

  primec::Expr badFileErrorWhyExpr;
  badFileErrorWhyExpr.kind = primec::Expr::Kind::Call;
  badFileErrorWhyExpr.isMethodCall = true;
  badFileErrorWhyExpr.name = "why";
  primec::Expr fileErrorType;
  fileErrorType.kind = primec::Expr::Kind::Name;
  fileErrorType.name = "FileError";
  badFileErrorWhyExpr.args = {fileErrorType};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitResultWhyDispatchCall(
            badFileErrorWhyExpr,
            {},
            {},
            tempCounter,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::ResultExprInfo &) {
              return false;
            },
            noResolveDefinition,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            []() { return 0; },
            [](primec::IrOpcode, uint64_t) {},
            [](const std::string &) { return 0; },
            [](const std::string &, const std::string &, std::string &) { return false; },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; },
            [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
            [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return false; },
            [](const std::string &) { return ValueKind::Unknown; },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              return false;
            },
            [](int32_t) { return false; },
            nullptr,
            nullptr,
            error) ==
        EmitResult::Error);
  CHECK(error == "FileError.why requires exactly one argument");
}

TEST_CASE("ir lowerer result helpers compose Result.why expression ops") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  std::vector<primec::IrInstruction> instructions;
  int32_t tempCounter = 5;
  int internCalls = 0;
  int allocCalls = 0;

  const primec::ir_lowerer::ResultWhyExprOps exprOps = primec::ir_lowerer::makeResultWhyExprOps(
      17,
      "/pkg",
      tempCounter,
      [&](const std::string &value) {
        ++internCalls;
        CHECK(value.empty());
        return 23;
      },
      [&]() {
        ++allocCalls;
        return 41;
      },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); });

  CHECK(exprOps.emitEmptyString());
  CHECK(internCalls == 1);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 23);

  primec::ir_lowerer::LocalMap locals;
  const primec::Expr errorExpr = exprOps.makeErrorValueExpr(locals, ValueKind::Int32);
  CHECK(errorExpr.kind == primec::Expr::Kind::Name);
  CHECK(errorExpr.name == "__result_why_err_5");
  CHECK(errorExpr.namespacePrefix == "/pkg");
  CHECK(tempCounter == 6);
  auto errorIt = locals.find("__result_why_err_5");
  REQUIRE(errorIt != locals.end());
  CHECK(errorIt->second.index == 17);
  CHECK(errorIt->second.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(errorIt->second.valueKind == ValueKind::Int32);

  instructions.clear();
  const primec::Expr boolExpr = exprOps.makeBoolErrorExpr(locals);
  CHECK(boolExpr.kind == primec::Expr::Kind::Name);
  CHECK(boolExpr.name == "__result_why_bool_6");
  CHECK(boolExpr.namespacePrefix == "/pkg");
  CHECK(tempCounter == 7);
  CHECK(allocCalls == 1);
  auto boolIt = locals.find("__result_why_bool_6");
  REQUIRE(boolIt != locals.end());
  CHECK(boolIt->second.index == 41);
  CHECK(boolIt->second.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(boolIt->second.valueKind == ValueKind::Bool);

  REQUIRE(instructions.size() == 6);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 17);
  CHECK(instructions[1].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].imm == 0);
  CHECK(instructions[2].op == primec::IrOpcode::CmpEqI64);
  CHECK(instructions[2].imm == 0);
  CHECK(instructions[3].op == primec::IrOpcode::PushI64);
  CHECK(instructions[3].imm == 0);
  CHECK(instructions[4].op == primec::IrOpcode::CmpEqI64);
  CHECK(instructions[4].imm == 0);
  CHECK(instructions[5].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[5].imm == 41);
}

TEST_CASE("ir lowerer result helpers compose Result.why call ops") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  bool resolveStructCalled = false;
  bool getReturnInfoCalled = false;
  bool bindingKindCalled = false;
  bool resolveLayoutCalled = false;
  bool valueKindCalled = false;
  bool makeErrorExprCalled = false;
  bool makeBoolExprCalled = false;
  bool emitInlineCalled = false;
  bool emitFileErrorCalled = false;
  bool emitEmptyStringCalled = false;
  int32_t seenFileErrorLocal = -1;

  const primec::ir_lowerer::ResultWhyCallOps ops = primec::ir_lowerer::makeResultWhyCallOps(
      [&](const std::string &typeName, const std::string &nsPrefix, std::string &structPathOut) {
        resolveStructCalled = true;
        CHECK(typeName == "ErrType");
        CHECK(nsPrefix == "/pkg");
        structPathOut = "/ErrType";
        return true;
      },
      [&](const std::string &path, primec::ir_lowerer::ReturnInfo &returnInfoOut) {
        getReturnInfoCalled = true;
        CHECK(path == "/ErrType/why");
        returnInfoOut = primec::ir_lowerer::ReturnInfo{};
        returnInfoOut.kind = ValueKind::String;
        return true;
      },
      [&](const primec::Expr &bindingExpr) {
        bindingKindCalled = true;
        CHECK(bindingExpr.kind == primec::Expr::Kind::Name);
        return primec::ir_lowerer::LocalInfo::Kind::Value;
      },
      [&](const std::string &structPath, primec::ir_lowerer::StructSlotLayoutInfo &layoutOut) {
        resolveLayoutCalled = true;
        CHECK(structPath == "/ErrType");
        layoutOut = primec::ir_lowerer::StructSlotLayoutInfo{};
        primec::ir_lowerer::StructSlotFieldInfo field;
        field.slotOffset = 0;
        field.slotCount = 1;
        field.valueKind = ValueKind::Int32;
        layoutOut.fields.push_back(field);
        return true;
      },
      [&](const std::string &typeName) {
        valueKindCalled = true;
        CHECK(typeName == "ErrType");
        return ValueKind::Int32;
      },
      [&](primec::ir_lowerer::LocalMap &, ValueKind kind) {
        makeErrorExprCalled = true;
        CHECK(kind == ValueKind::Int32);
        primec::Expr valueExpr;
        valueExpr.kind = primec::Expr::Kind::Name;
        valueExpr.name = "err_value";
        return valueExpr;
      },
      [&](primec::ir_lowerer::LocalMap &) {
        makeBoolExprCalled = true;
        primec::Expr valueExpr;
        valueExpr.kind = primec::Expr::Kind::Name;
        valueExpr.name = "err_bool";
        return valueExpr;
      },
      [&](const primec::Expr &callExpr,
          const primec::Definition &callee,
          const primec::ir_lowerer::LocalMap &) {
        emitInlineCalled = true;
        CHECK(callExpr.kind == primec::Expr::Kind::Call);
        CHECK(callee.fullPath == "/ErrType/why");
        return true;
      },
      [&](int32_t errorLocal) {
        emitFileErrorCalled = true;
        seenFileErrorLocal = errorLocal;
        return true;
      },
      [&]() {
        emitEmptyStringCalled = true;
        return true;
      });

  std::string resolvedPath;
  CHECK(ops.resolveStructTypeName("ErrType", "/pkg", resolvedPath));
  CHECK(resolvedPath == "/ErrType");
  CHECK(resolveStructCalled);

  primec::ir_lowerer::ReturnInfo returnInfo;
  CHECK(ops.getReturnInfo("/ErrType/why", returnInfo));
  CHECK(returnInfo.kind == ValueKind::String);
  CHECK(getReturnInfoCalled);

  primec::Expr typedBinding;
  typedBinding.kind = primec::Expr::Kind::Name;
  primec::Transform qualifier;
  qualifier.name = "mut";
  typedBinding.transforms.push_back(qualifier);
  primec::Transform typeTransform;
  typeTransform.name = "ErrType";
  typedBinding.transforms.push_back(typeTransform);
  std::string typeName;
  std::vector<std::string> templateArgs;
  CHECK(ops.extractFirstBindingTypeTransform(typedBinding, typeName, templateArgs));
  CHECK(typeName == "ErrType");
  CHECK(templateArgs.empty());

  primec::Expr untypedBinding;
  CHECK_FALSE(ops.extractFirstBindingTypeTransform(untypedBinding, typeName, templateArgs));
  CHECK(typeName.empty());
  CHECK(templateArgs.empty());

  CHECK(ops.bindingKind(typedBinding) == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(bindingKindCalled);

  primec::ir_lowerer::StructSlotLayoutInfo layout;
  CHECK(ops.resolveStructSlotLayout("/ErrType", layout));
  REQUIRE(layout.fields.size() == 1);
  CHECK(layout.fields.front().slotOffset == 0);
  CHECK(layout.fields.front().valueKind == ValueKind::Int32);
  CHECK(resolveLayoutCalled);

  CHECK(ops.valueKindFromTypeName("ErrType") == ValueKind::Int32);
  CHECK(valueKindCalled);

  primec::ir_lowerer::LocalMap callLocals;
  primec::Expr errorExpr = ops.makeErrorValueExpr(callLocals, ValueKind::Int32);
  CHECK(errorExpr.kind == primec::Expr::Kind::Name);
  CHECK(errorExpr.name == "err_value");
  CHECK(makeErrorExprCalled);

  primec::Expr boolExpr = ops.makeBoolErrorExpr(callLocals);
  CHECK(boolExpr.kind == primec::Expr::Kind::Name);
  CHECK(boolExpr.name == "err_bool");
  CHECK(makeBoolExprCalled);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  primec::Definition callee;
  callee.fullPath = "/ErrType/why";
  CHECK(ops.emitInlineDefinitionCall(callExpr, callee, callLocals));
  CHECK(emitInlineCalled);

  CHECK(ops.emitFileErrorWhy(42));
  CHECK(emitFileErrorCalled);
  CHECK(seenFileErrorLocal == 42);

  CHECK(ops.emitEmptyString());
  CHECK(emitEmptyStringCalled);
}


TEST_SUITE_END();
