#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer result helpers resolve indexed dereferenced args-pack Result references") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo resultPack;
  resultPack.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  resultPack.isArgsPack = true;
  resultPack.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  resultPack.isResult = true;
  resultPack.resultHasValue = true;
  resultPack.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  resultPack.resultErrorType = "ParseError";
  locals.emplace("values", resultPack);

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "at";
  accessExpr.args = {valuesName, indexExpr};

  primec::Expr dereferenceExpr;
  dereferenceExpr.kind = primec::Expr::Kind::Call;
  dereferenceExpr.name = "dereference";
  dereferenceExpr.args = {accessExpr};

  auto resolveMethodCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  auto resolveDefinitionCall = [](const primec::Expr &) -> const primec::Definition * {
    return nullptr;
  };
  auto lookupReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) {
    return false;
  };
  const primec::ir_lowerer::InferExprKindWithLocalsFn inferExprKind = {};

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      dereferenceExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(out.errorType == "ParseError");
}

TEST_CASE("ir lowerer result helpers resolve indexed dereferenced args-pack Result pointers") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo resultPack;
  resultPack.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  resultPack.isArgsPack = true;
  resultPack.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  resultPack.isResult = true;
  resultPack.resultHasValue = true;
  resultPack.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  resultPack.resultErrorType = "ParseError";
  locals.emplace("values", resultPack);

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "at";
  accessExpr.args = {valuesName, indexExpr};

  primec::Expr dereferenceExpr;
  dereferenceExpr.kind = primec::Expr::Kind::Call;
  dereferenceExpr.name = "dereference";
  dereferenceExpr.args = {accessExpr};

  auto resolveMethodCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  auto resolveDefinitionCall = [](const primec::Expr &) -> const primec::Definition * {
    return nullptr;
  };
  auto lookupReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) {
    return false;
  };
  const primec::ir_lowerer::InferExprKindWithLocalsFn inferExprKind = {};

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      dereferenceExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(out.errorType == "ParseError");
}

TEST_CASE("ir lowerer result helpers resolve Result.why call info") {
  primec::Expr resultWhyExpr;
  resultWhyExpr.kind = primec::Expr::Kind::Call;
  resultWhyExpr.isMethodCall = true;
  resultWhyExpr.name = "why";
  resultWhyExpr.args.resize(2);
  resultWhyExpr.args[0].kind = primec::Expr::Kind::Name;
  resultWhyExpr.args[0].name = "Result";
  resultWhyExpr.args[1].kind = primec::Expr::Kind::Name;
  resultWhyExpr.args[1].name = "res";

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::ResolveResultExprInfoWithLocalsFn resolveResultExprInfo =
      [](const primec::Expr &valueExpr,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::ResultExprInfo &resultInfoOut) {
        if (valueExpr.kind == primec::Expr::Kind::Name && valueExpr.name == "res") {
          resultInfoOut = primec::ir_lowerer::ResultExprInfo{};
          resultInfoOut.isResult = true;
          resultInfoOut.hasValue = true;
          resultInfoOut.errorType = "AnyError";
          return true;
        }
        return false;
      };

  primec::ir_lowerer::ResultExprInfo resultInfo;
  std::string error;
  CHECK(primec::ir_lowerer::resolveResultWhyCallInfo(
      resultWhyExpr, locals, resolveResultExprInfo, resultInfo, error));
  CHECK(resultInfo.isResult);
  CHECK(resultInfo.hasValue);
  CHECK(resultInfo.errorType == "AnyError");

  resultWhyExpr.args.resize(1);
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveResultWhyCallInfo(
      resultWhyExpr, locals, resolveResultExprInfo, resultInfo, error));
  CHECK(error == "Result.why requires exactly one argument");

  resultWhyExpr.args.resize(2);
  resultWhyExpr.args[1].name = "plainValue";
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveResultWhyCallInfo(
      resultWhyExpr, locals, resolveResultExprInfo, resultInfo, error));
  CHECK(error == "Result.why requires Result argument");
}

TEST_CASE("ir lowerer result helpers resolve Result.error call info") {
  primec::Expr resultErrorExpr;
  resultErrorExpr.kind = primec::Expr::Kind::Call;
  resultErrorExpr.isMethodCall = true;
  resultErrorExpr.name = "error";
  resultErrorExpr.args.resize(2);
  resultErrorExpr.args[0].kind = primec::Expr::Kind::Name;
  resultErrorExpr.args[0].name = "Result";
  resultErrorExpr.args[1].kind = primec::Expr::Kind::Name;
  resultErrorExpr.args[1].name = "status";

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::ResolveResultExprInfoWithLocalsFn resolveResultExprInfo =
      [](const primec::Expr &valueExpr,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::ResultExprInfo &resultInfoOut) {
        if (valueExpr.kind == primec::Expr::Kind::Name && valueExpr.name == "status") {
          resultInfoOut = primec::ir_lowerer::ResultExprInfo{};
          resultInfoOut.isResult = true;
          resultInfoOut.hasValue = false;
          resultInfoOut.errorType = "ParseError";
          return true;
        }
        return false;
      };

  primec::ir_lowerer::ResultExprInfo resultInfo;
  std::string error;
  CHECK(primec::ir_lowerer::resolveResultErrorCallInfo(
      resultErrorExpr, locals, resolveResultExprInfo, resultInfo, error));
  CHECK(resultInfo.isResult);
  CHECK_FALSE(resultInfo.hasValue);
  CHECK(resultInfo.errorType == "ParseError");

  resultErrorExpr.args.resize(1);
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveResultErrorCallInfo(
      resultErrorExpr, locals, resolveResultExprInfo, resultInfo, error));
  CHECK(error == "Result.error requires exactly one argument");

  resultErrorExpr.args.resize(2);
  resultErrorExpr.args[1].name = "plainValue";
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveResultErrorCallInfo(
      resultErrorExpr, locals, resolveResultExprInfo, resultInfo, error));
  CHECK(error == "Result.error requires Result argument");
}

TEST_CASE("ir lowerer result helpers classify Result.why error kinds") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  CHECK(primec::ir_lowerer::isSupportedResultWhyErrorKind(ValueKind::Int32));
  CHECK(primec::ir_lowerer::isSupportedResultWhyErrorKind(ValueKind::Int64));
  CHECK(primec::ir_lowerer::isSupportedResultWhyErrorKind(ValueKind::UInt64));
  CHECK(primec::ir_lowerer::isSupportedResultWhyErrorKind(ValueKind::Bool));
  CHECK_FALSE(primec::ir_lowerer::isSupportedResultWhyErrorKind(ValueKind::Float32));
  CHECK_FALSE(primec::ir_lowerer::isSupportedResultWhyErrorKind(ValueKind::String));
  CHECK_FALSE(primec::ir_lowerer::isSupportedResultWhyErrorKind(ValueKind::Unknown));
}

TEST_CASE("ir lowerer result helpers normalize Result.why type names") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  CHECK(primec::ir_lowerer::normalizeResultWhyErrorName("FileError", ValueKind::Int32) == "FileError");
  CHECK(primec::ir_lowerer::normalizeResultWhyErrorName("Other", ValueKind::Int32) == "i32");
  CHECK(primec::ir_lowerer::normalizeResultWhyErrorName("Other", ValueKind::Int64) == "i64");
  CHECK(primec::ir_lowerer::normalizeResultWhyErrorName("Other", ValueKind::UInt64) == "u64");
  CHECK(primec::ir_lowerer::normalizeResultWhyErrorName("Other", ValueKind::Bool) == "bool");
  CHECK(primec::ir_lowerer::normalizeResultWhyErrorName("CustomError", ValueKind::Float64) == "CustomError");
}

TEST_CASE("ir lowerer result helpers emit Result.why local expressions") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  const primec::Expr errorExpr = primec::ir_lowerer::makeResultWhyErrorValueExpr(
      7, ValueKind::Int64, "/pkg", 3, locals);
  CHECK(errorExpr.kind == primec::Expr::Kind::Name);
  CHECK(errorExpr.name == "__result_why_err_3");
  CHECK(errorExpr.namespacePrefix == "/pkg");
  auto errorIt = locals.find("__result_why_err_3");
  REQUIRE(errorIt != locals.end());
  CHECK(errorIt->second.index == 7);
  CHECK(errorIt->second.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(errorIt->second.valueKind == ValueKind::Int64);

  std::vector<primec::IrInstruction> instructions;
  int internCalls = 0;
  CHECK(primec::ir_lowerer::emitResultWhyEmptyString(
      [&](const std::string &value) {
        ++internCalls;
        CHECK(value.empty());
        return 11;
      },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); }));
  CHECK(internCalls == 1);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 11);

  int allocCalls = 0;
  const primec::Expr boolExpr = primec::ir_lowerer::makeResultWhyBoolErrorExpr(
      9,
      "/pkg",
      4,
      locals,
      [&]() {
        ++allocCalls;
        return 42;
      },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); });
  CHECK(boolExpr.kind == primec::Expr::Kind::Name);
  CHECK(boolExpr.name == "__result_why_bool_4");
  CHECK(boolExpr.namespacePrefix == "/pkg");
  CHECK(allocCalls == 1);
  auto boolIt = locals.find("__result_why_bool_4");
  REQUIRE(boolIt != locals.end());
  CHECK(boolIt->second.index == 42);
  CHECK(boolIt->second.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(boolIt->second.valueKind == ValueKind::Bool);

  REQUIRE(instructions.size() == 7);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 9);
  CHECK(instructions[2].op == primec::IrOpcode::PushI64);
  CHECK(instructions[2].imm == 0);
  CHECK(instructions[3].op == primec::IrOpcode::CmpEqI64);
  CHECK(instructions[3].imm == 0);
  CHECK(instructions[4].op == primec::IrOpcode::PushI64);
  CHECK(instructions[4].imm == 0);
  CHECK(instructions[5].op == primec::IrOpcode::CmpEqI64);
  CHECK(instructions[5].imm == 0);
  CHECK(instructions[6].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[6].imm == 42);
}

TEST_CASE("ir lowerer result helpers emit Result.why error-local extraction") {
  std::vector<primec::IrInstruction> instructions;
  primec::ir_lowerer::ResultExprInfo statusInfo;

  primec::ir_lowerer::emitResultWhyErrorLocalFromResult(
      5,
      statusInfo,
      8,
      []() { return -1; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); });
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 5);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 8);

  instructions.clear();
  primec::ir_lowerer::ResultExprInfo valueInfo;
  valueInfo.hasValue = true;
  valueInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  primec::ir_lowerer::emitResultWhyErrorLocalFromResult(
      6,
      valueInfo,
      9,
      []() { return -1; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); });
  REQUIRE(instructions.size() == 4);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 6);
  CHECK(instructions[1].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].imm == 4294967296ull);
  CHECK(instructions[2].op == primec::IrOpcode::DivI64);
  CHECK(instructions[2].imm == 0);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 9);

  instructions.clear();
  int allocCounter = 0;
  primec::ir_lowerer::ResultExprInfo bufferInfo;
  bufferInfo.hasValue = true;
  bufferInfo.valueCollectionKind = primec::ir_lowerer::LocalInfo::Kind::Buffer;
  bufferInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  primec::ir_lowerer::emitResultWhyErrorLocalFromResult(
      7,
      bufferInfo,
      12,
      [&]() { return ++allocCounter; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); });
  REQUIRE(allocCounter == 3);
  REQUIRE(instructions.size() == 18);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 7);
  CHECK(instructions[1].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].imm == 4294967296ull);
  CHECK(instructions[2].op == primec::IrOpcode::DivI64);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 1);
  CHECK(instructions[9].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[9].imm == 2);
  CHECK(instructions[12].op == primec::IrOpcode::CmpEqI64);
  CHECK(instructions[13].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[13].imm == 3);
  CHECK(instructions[14].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[14].imm == 1);
  CHECK(instructions[15].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[15].imm == 3);
  CHECK(instructions[16].op == primec::IrOpcode::MulI64);
  CHECK(instructions[17].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[17].imm == 12);
}

TEST_CASE("ir lowerer result helpers emit Result.why value-local setup") {
  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Name;
  valueExpr.name = "res";
  primec::ir_lowerer::LocalMap locals;

  std::vector<primec::IrInstruction> instructions;
  int allocCounter = 0;
  int32_t errorLocal = -1;
  bool emitCalled = false;
  primec::ir_lowerer::ResultExprInfo valueInfo;
  valueInfo.hasValue = true;
  valueInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  CHECK(primec::ir_lowerer::emitResultWhyLocalsFromValueExpr(
      valueExpr,
      locals,
      valueInfo,
      [&](const primec::Expr &emittedExpr, const primec::ir_lowerer::LocalMap &emittedLocals) {
        emitCalled = true;
        CHECK(emittedExpr.name == "res");
        CHECK(emittedLocals.empty());
        return true;
      },
      [&]() {
        ++allocCounter;
        return allocCounter == 1 ? 13 : 17;
      },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      errorLocal));
  CHECK(emitCalled);
  CHECK(errorLocal == 17);
  REQUIRE(instructions.size() == 5);
  CHECK(instructions[0].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[0].imm == 13);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 13);
  CHECK(instructions[2].op == primec::IrOpcode::PushI64);
  CHECK(instructions[2].imm == 4294967296ull);
  CHECK(instructions[3].op == primec::IrOpcode::DivI64);
  CHECK(instructions[3].imm == 0);
  CHECK(instructions[4].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[4].imm == 17);

  instructions.clear();
  allocCounter = 0;
  errorLocal = -1;
  primec::ir_lowerer::ResultExprInfo statusInfo;
  CHECK(primec::ir_lowerer::emitResultWhyLocalsFromValueExpr(
      valueExpr,
      locals,
      statusInfo,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [&]() {
        ++allocCounter;
        return allocCounter == 1 ? 20 : 21;
      },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      errorLocal));
  CHECK(errorLocal == 21);
  REQUIRE(instructions.size() == 3);
  CHECK(instructions[0].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[0].imm == 20);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 20);
  CHECK(instructions[2].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[2].imm == 21);

  instructions.clear();
  allocCounter = 0;
  errorLocal = -1;
  CHECK_FALSE(primec::ir_lowerer::emitResultWhyLocalsFromValueExpr(
      valueExpr,
      locals,
      valueInfo,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&]() {
        ++allocCounter;
        return 99;
      },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      errorLocal));
  CHECK(allocCounter == 0);
  CHECK(errorLocal == -1);
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer result helpers try emit Result.error method calls") {
  using EmitResult = primec::ir_lowerer::ResultErrorMethodCallEmitResult;

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.isMethodCall = true;
  expr.name = "error";
  expr.args.resize(2);
  expr.args[0].kind = primec::Expr::Kind::Name;
  expr.args[0].name = "Result";
  expr.args[1].kind = primec::Expr::Kind::Name;
  expr.args[1].name = "status";

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::ResolveResultExprInfoWithLocalsFn resolveResultExprInfo =
      [](const primec::Expr &valueExpr,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::ResultExprInfo &resultInfoOut) {
        if (valueExpr.kind == primec::Expr::Kind::Name && valueExpr.name == "status") {
          resultInfoOut = primec::ir_lowerer::ResultExprInfo{};
          resultInfoOut.isResult = true;
          resultInfoOut.hasValue = false;
          resultInfoOut.errorType = "ParseError";
          return true;
        }
        return false;
      };

  std::vector<primec::IrInstruction> instructions;
  int allocCounter = 0;
  int emitCalls = 0;
  std::string error;
  const EmitResult emitted = primec::ir_lowerer::tryEmitResultErrorCall(
      expr,
      locals,
      resolveResultExprInfo,
      [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &valueLocals) {
        ++emitCalls;
        CHECK(valueExpr.kind == primec::Expr::Kind::Name);
        CHECK(valueExpr.name == "status");
        CHECK(valueLocals.empty());
        instructions.push_back({primec::IrOpcode::LoadLocal, 7});
        return true;
      },
      [&]() {
        ++allocCounter;
        return allocCounter == 1 ? 13 : 17;
      },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      error);
  CHECK(emitted == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(emitCalls == 1);
  CHECK(allocCounter == 2);
  REQUIRE(instructions.size() == 9);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 7);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 13);
  CHECK(instructions[2].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[2].imm == 13);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 17);
  CHECK(instructions[4].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[4].imm == 17);
  CHECK(instructions[5].op == primec::IrOpcode::PushI64);
  CHECK(instructions[5].imm == 0);
  CHECK(instructions[6].op == primec::IrOpcode::CmpEqI64);
  CHECK(instructions[6].imm == 0);
  CHECK(instructions[7].op == primec::IrOpcode::PushI64);
  CHECK(instructions[7].imm == 0);
  CHECK(instructions[8].op == primec::IrOpcode::CmpEqI64);
  CHECK(instructions[8].imm == 0);

  instructions.clear();
  allocCounter = 0;
  emitCalls = 0;
  error.clear();
  expr.args.resize(1);
  const EmitResult badCall = primec::ir_lowerer::tryEmitResultErrorCall(
      expr,
      locals,
      resolveResultExprInfo,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [&]() { return 0; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      error);
  CHECK(badCall == EmitResult::Error);
  CHECK(error == "Result.error requires exactly one argument");
  CHECK(instructions.empty());
}


TEST_SUITE_END();
