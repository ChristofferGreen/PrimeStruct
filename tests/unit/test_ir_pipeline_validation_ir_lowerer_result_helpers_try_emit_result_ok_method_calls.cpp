#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer result helpers try emit Result.ok method calls") {
  using EmitResult = primec::ir_lowerer::ResultOkMethodCallEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.isMethodCall = true;
  expr.name = "ok";

  primec::Expr resultType;
  resultType.kind = primec::Expr::Kind::Name;
  resultType.name = "Result";

  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Literal;
  valueExpr.literalValue = 7;

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;
  const auto inferNoStruct = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
    return std::string{};
  };
  const auto neverFileHandle = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
    return false;
  };
  const auto resolveNoDefinitionCall = [](const primec::Expr &) -> const primec::Definition * {
    return nullptr;
  };
  const auto allocTempLocal = []() { return 19; };
  const auto resolveNoStructLayout = [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) {
    return false;
  };

  expr.args = {resultType};
  CHECK(primec::ir_lowerer::tryEmitResultOkCall(
            expr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return ValueKind::Unknown;
            },
            inferNoStruct,
            resolveNoDefinitionCall,
            neverFileHandle,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            allocTempLocal,
            resolveNoStructLayout,
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) ==
        EmitResult::Emitted);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[0].imm == 0);

  instructions.clear();
  primec::Expr nonResultExpr = expr;
  nonResultExpr.name = "map";
  CHECK(primec::ir_lowerer::tryEmitResultOkCall(
            nonResultExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return ValueKind::Unknown;
            },
            inferNoStruct,
            resolveNoDefinitionCall,
            neverFileHandle,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            allocTempLocal,
            resolveNoStructLayout,
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) ==
        EmitResult::NotHandled);
  CHECK(instructions.empty());

  bool inferCalled = false;
  bool emitCalled = false;
  expr.args = {resultType, valueExpr};
  CHECK(primec::ir_lowerer::tryEmitResultOkCall(
            expr,
            locals,
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              inferCalled = true;
              return ValueKind::Bool;
            },
            inferNoStruct,
            resolveNoDefinitionCall,
            neverFileHandle,
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              emitCalled = true;
              return true;
            },
            allocTempLocal,
            resolveNoStructLayout,
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) ==
        EmitResult::Emitted);
  CHECK(inferCalled);
  CHECK(emitCalled);

  expr.args = {resultType, valueExpr, valueExpr};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitResultOkCall(
            expr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return ValueKind::Int32;
            },
            inferNoStruct,
            resolveNoDefinitionCall,
            neverFileHandle,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            allocTempLocal,
            resolveNoStructLayout,
            [&](primec::IrOpcode, uint64_t) {},
            error) ==
        EmitResult::Error);
  CHECK(error == "Result.ok accepts at most one argument");

  emitCalled = false;
  expr.args = {resultType, valueExpr};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitResultOkCall(
            expr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return ValueKind::String;
            },
            inferNoStruct,
            resolveNoDefinitionCall,
            neverFileHandle,
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              emitCalled = true;
              return true;
            },
            allocTempLocal,
            resolveNoStructLayout,
            [&](primec::IrOpcode, uint64_t) {},
            error) ==
        EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(emitCalled);

  emitCalled = false;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitResultOkCall(
            expr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return ValueKind::Int64;
            },
            inferNoStruct,
            resolveNoDefinitionCall,
            neverFileHandle,
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              emitCalled = true;
              return true;
            },
            allocTempLocal,
            resolveNoStructLayout,
            [&](primec::IrOpcode, uint64_t) {},
            error) ==
        EmitResult::Error);
  CHECK(error == "IR backends only support Result.ok with supported payload values");
  CHECK_FALSE(emitCalled);

  emitCalled = false;
  primec::Expr fileValueExpr;
  fileValueExpr.kind = primec::Expr::Kind::Name;
  fileValueExpr.name = "file";
  expr.args = {resultType, fileValueExpr};
  primec::ir_lowerer::LocalInfo fileInfo;
  fileInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  fileInfo.valueKind = ValueKind::Int64;
  fileInfo.isFileHandle = true;
  fileInfo.index = 23;
  locals.emplace("file", fileInfo);
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitResultOkCall(
            expr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return ValueKind::Int64;
            },
            inferNoStruct,
            resolveNoDefinitionCall,
            [](const primec::Expr &value, const primec::ir_lowerer::LocalMap &valueLocals) {
              if (value.kind != primec::Expr::Kind::Name) {
                return false;
              }
              auto it = valueLocals.find(value.name);
              return it != valueLocals.end() && it->second.isFileHandle;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              emitCalled = true;
              return true;
            },
            allocTempLocal,
            resolveNoStructLayout,
            [&](primec::IrOpcode, uint64_t) {},
            error) ==
        EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(emitCalled);

  error.clear();
  expr.args = {resultType, valueExpr};
  CHECK(primec::ir_lowerer::tryEmitResultOkCall(
            expr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return ValueKind::Int32;
            },
            inferNoStruct,
            resolveNoDefinitionCall,
            neverFileHandle,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            allocTempLocal,
            resolveNoStructLayout,
            [&](primec::IrOpcode, uint64_t) {},
            error) ==
        EmitResult::Error);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer result helpers emit Buffer Result.ok method calls") {
  using EmitResult = primec::ir_lowerer::ResultOkMethodCallEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.isMethodCall = true;
  expr.name = "ok";

  primec::Expr resultType;
  resultType.kind = primec::Expr::Kind::Name;
  resultType.name = "Result";

  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Name;
  valueExpr.name = "buffer";
  expr.args = {resultType, valueExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo bufferInfo;
  bufferInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Buffer;
  bufferInfo.valueKind = ValueKind::Int32;
  bufferInfo.index = 11;
  locals.emplace("buffer", bufferInfo);

  bool emitCalled = false;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitResultOkCall(
            expr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return ValueKind::Int64;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string{}; },
            [](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              emitCalled = true;
              return true;
            },
            []() { return 0; },
            [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return false; },
            [&](primec::IrOpcode, uint64_t) {},
            error) ==
        EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(emitCalled);
}

TEST_CASE("ir lowerer result helpers pack single-slot struct Result.ok values") {
  using EmitResult = primec::ir_lowerer::ResultOkMethodCallEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.isMethodCall = true;
  expr.name = "ok";

  primec::Expr resultType;
  resultType.kind = primec::Expr::Kind::Name;
  resultType.name = "Result";

  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Name;
  valueExpr.name = "label";
  expr.args = {resultType, valueExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo labelInfo;
  labelInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  labelInfo.valueKind = ValueKind::Int64;
  labelInfo.structTypeName = "/pkg/Label";
  labelInfo.index = 7;
  locals.emplace("label", labelInfo);

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitResultOkCall(
            expr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return ValueKind::Int64;
            },
            [](const primec::Expr &value, const primec::ir_lowerer::LocalMap &) {
              return value.kind == primec::Expr::Kind::Name && value.name == "label" ? "/pkg/Label" : std::string{};
            },
            [](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              instructions.push_back({primec::IrOpcode::LoadLocal, 7});
              return true;
            },
            []() { return 23; },
            [](const std::string &structPath, primec::ir_lowerer::StructSlotLayoutInfo &layoutOut) {
              if (structPath != "/pkg/Label") {
                return false;
              }
              layoutOut = primec::ir_lowerer::StructSlotLayoutInfo{};
              layoutOut.structPath = structPath;
              layoutOut.totalSlots = 1;
              primec::ir_lowerer::StructSlotFieldInfo field;
              field.slotOffset = 0;
              field.slotCount = 1;
              field.valueKind = ValueKind::Int32;
              layoutOut.fields.push_back(field);
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) ==
        EmitResult::Emitted);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 4);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[2].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[3].op == primec::IrOpcode::LoadIndirect);
}

TEST_CASE("ir lowerer result helpers preserve multi-slot struct Result.ok values") {
  using EmitResult = primec::ir_lowerer::ResultOkMethodCallEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.isMethodCall = true;
  expr.name = "ok";

  primec::Expr resultType;
  resultType.kind = primec::Expr::Kind::Name;
  resultType.name = "Result";

  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Name;
  valueExpr.name = "pair";
  expr.args = {resultType, valueExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo pairInfo;
  pairInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  pairInfo.valueKind = ValueKind::Int64;
  pairInfo.structTypeName = "/pkg/Pair";
  pairInfo.index = 9;
  locals.emplace("pair", pairInfo);

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitResultOkCall(
            expr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return ValueKind::Int64;
            },
            [](const primec::Expr &value, const primec::ir_lowerer::LocalMap &) {
              return value.kind == primec::Expr::Kind::Name && value.name == "pair" ? "/pkg/Pair" : std::string{};
            },
            [](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              instructions.push_back({primec::IrOpcode::LoadLocal, 9});
              return true;
            },
            []() -> int32_t {
              CHECK(false);
              return 0;
            },
            [](const std::string &structPath, primec::ir_lowerer::StructSlotLayoutInfo &layoutOut) {
              if (structPath != "/pkg/Pair") {
                return false;
              }
              layoutOut = primec::ir_lowerer::StructSlotLayoutInfo{};
              layoutOut.structPath = structPath;
              layoutOut.totalSlots = 2;
              primec::ir_lowerer::StructSlotFieldInfo left;
              left.slotOffset = 0;
              left.slotCount = 1;
              left.valueKind = ValueKind::Int32;
              layoutOut.fields.push_back(left);
              primec::ir_lowerer::StructSlotFieldInfo right;
              right.slotOffset = 1;
              right.slotCount = 1;
              right.valueKind = ValueKind::Int32;
              layoutOut.fields.push_back(right);
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) ==
        EmitResult::Emitted);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
}

TEST_CASE("ir lowerer result helpers resolve definition result metadata") {
  primec::Definition callee;
  callee.fullPath = "/make";

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "make";

  auto lookupLocal = [](const std::string &) { return primec::ir_lowerer::LocalResultInfo{}; };
  auto resolveNone = [](const primec::Expr &) -> const primec::Definition * { return nullptr; };
  auto resolveDirect = [&](const primec::Expr &) -> const primec::Definition * { return &callee; };
  auto lookupDefinition = [](const std::string &path, primec::ir_lowerer::ResultExprInfo &out) {
    if (path != "/make") {
      return false;
    }
    out.isResult = true;
    out.hasValue = false;
    out.errorType = "FileError";
    return true;
  };

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfo(
      callExpr, lookupLocal, resolveNone, resolveDirect, lookupDefinition, out));
  CHECK(out.isResult);
  CHECK_FALSE(out.hasValue);
  CHECK(out.errorType == "FileError");
}

TEST_CASE("ir lowerer result helpers tolerate missing direct result metadata callbacks") {
  primec::Expr localName;
  localName.kind = primec::Expr::Kind::Name;
  localName.name = "value";

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "make";

  primec::ir_lowerer::ResultExprInfo out;
  CHECK_FALSE(primec::ir_lowerer::resolveResultExprInfo(
      localName,
      primec::ir_lowerer::LookupLocalResultInfoFn{},
      primec::ir_lowerer::ResolveCallDefinitionFn{},
      primec::ir_lowerer::ResolveCallDefinitionFn{},
      primec::ir_lowerer::LookupDefinitionResultInfoFn{},
      out));
  CHECK_FALSE(primec::ir_lowerer::resolveResultExprInfo(
      callExpr,
      primec::ir_lowerer::LookupLocalResultInfoFn{},
      primec::ir_lowerer::ResolveCallDefinitionFn{},
      primec::ir_lowerer::ResolveCallDefinitionFn{},
      primec::ir_lowerer::LookupDefinitionResultInfoFn{},
      out));
}

TEST_CASE("ir lowerer result helpers resolve from locals and return-info lookups") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localResult;
  localResult.isResult = true;
  localResult.resultHasValue = true;
  localResult.resultErrorType = "FileError";
  locals.emplace("value", localResult);

  primec::Expr localNameExpr;
  localNameExpr.kind = primec::Expr::Kind::Name;
  localNameExpr.name = "value";

  primec::Definition methodDef;
  methodDef.fullPath = "/method";
  primec::Expr methodReceiver;
  methodReceiver.kind = primec::Expr::Kind::Name;
  methodReceiver.name = "obj";
  primec::Expr methodCallExpr;
  methodCallExpr.kind = primec::Expr::Kind::Call;
  methodCallExpr.isMethodCall = true;
  methodCallExpr.name = "do_it";
  methodCallExpr.args = {methodReceiver};

  auto resolveMethodCall = [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return &methodDef;
  };
  auto resolveDefinitionCall = [](const primec::Expr &) -> const primec::Definition * {
    return nullptr;
  };
  auto lookupReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
    if (path != "/method") {
      return false;
    }
    info = primec::ir_lowerer::ReturnInfo{};
    info.isResult = true;
    info.resultHasValue = false;
    info.resultErrorType = "MethodError";
    return true;
  };
  const primec::ir_lowerer::InferExprKindWithLocalsFn inferExprKind = {};

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      localNameExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.errorType == "FileError");

  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      methodCallExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK_FALSE(out.hasValue);
  CHECK(out.errorType == "MethodError");

  primec::Expr unknownName;
  unknownName.kind = primec::Expr::Kind::Name;
  unknownName.name = "missing";
  CHECK_FALSE(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      unknownName, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
}

TEST_CASE("ir lowerer result helpers require semantic query facts for generic call result metadata") {
  primec::Expr queryExpr;
  queryExpr.kind = primec::Expr::Kind::Call;
  queryExpr.name = "lookup";
  queryExpr.semanticNodeId = 63;

  const auto resolveMethodCall = [](const primec::Expr &,
                                    const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  const auto resolveDefinitionCall = [](const primec::Expr &) -> const primec::Definition * {
    return nullptr;
  };
  const auto lookupReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) {
    return false;
  };
  const primec::ir_lowerer::InferExprKindWithLocalsFn inferExprKind = {};

  primec::SemanticProgram semanticProgram;
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<i32, FileError>",
      .bindingTypeText = "Result<i32, FileError>",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .sourceLine = 11,
      .sourceColumn = 7,
      .semanticNodeId = 63,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  primec::ir_lowerer::ResultExprInfo out;
  std::string error;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(queryExpr,
                                                            {},
                                                            resolveMethodCall,
                                                            resolveDefinitionCall,
                                                            lookupReturnInfo,
                                                            inferExprKind,
                                                            out,
                                                            &semanticTargets,
                                                            &error));
  CHECK(error.empty());
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(out.errorType == "FileError");

  primec::SemanticProgram missingSemanticProgram;
  const auto missingTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&missingSemanticProgram);
  out = {};
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveResultExprInfoFromLocals(queryExpr,
                                                                  {},
                                                                  resolveMethodCall,
                                                                  resolveDefinitionCall,
                                                                  lookupReturnInfo,
                                                                  inferExprKind,
                                                                  out,
                                                                  &missingTargets,
                                                                  &error));
  CHECK(error == "missing semantic-product query fact: lookup");
}

TEST_CASE("ir lowerer result helpers use semantic query facts for direct Result ok payload metadata") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr resultType;
  resultType.kind = primec::Expr::Kind::Name;
  resultType.name = "Result";

  primec::Expr queryExpr;
  queryExpr.kind = primec::Expr::Kind::Call;
  queryExpr.name = "lookup";
  queryExpr.semanticNodeId = 263;

  primec::Expr okExpr;
  okExpr.kind = primec::Expr::Kind::Call;
  okExpr.isMethodCall = true;
  okExpr.name = "ok";
  okExpr.args = {resultType, queryExpr};

  const auto resolveMethodCall = [](const primec::Expr &,
                                    const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  const auto resolveDefinitionCall = [](const primec::Expr &) -> const primec::Definition * {
    return nullptr;
  };
  const auto lookupReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) {
    return false;
  };

  primec::SemanticProgram semanticProgram;
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "i32",
      .bindingTypeText = "i32",
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .sourceLine = 11,
      .sourceColumn = 7,
      .semanticNodeId = 263,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .queryTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
  });
  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  bool fallbackCalled = false;
  const primec::ir_lowerer::InferExprKindWithLocalsFn inferExprKind =
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        fallbackCalled = true;
        return ValueKind::Int64;
      };

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(okExpr,
                                                            {},
                                                            resolveMethodCall,
                                                            resolveDefinitionCall,
                                                            lookupReturnInfo,
                                                            inferExprKind,
                                                            out,
                                                            &semanticTargets));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueKind == ValueKind::Int32);
  CHECK_FALSE(fallbackCalled);

  primec::SemanticProgram missingSemanticProgram;
  const auto missingTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&missingSemanticProgram);
  out = {};
  fallbackCalled = false;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(okExpr,
                                                            {},
                                                            resolveMethodCall,
                                                            resolveDefinitionCall,
                                                            lookupReturnInfo,
                                                            inferExprKind,
                                                            out,
                                                            &missingTargets));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueKind == ValueKind::Unknown);
  CHECK_FALSE(fallbackCalled);
}

TEST_CASE("ir lowerer result helpers reject resolved-path semantic query fallback in production mode") {
  primec::Expr queryExpr;
  queryExpr.kind = primec::Expr::Kind::Call;
  queryExpr.name = "lookup";
  queryExpr.semanticNodeId = 163;

  const auto resolveMethodCall = [](const primec::Expr &,
                                    const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  const auto resolveDefinitionCall = [](const primec::Expr &) -> const primec::Definition * {
    return nullptr;
  };
  const auto lookupReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) {
    return false;
  };
  const primec::ir_lowerer::InferExprKindWithLocalsFn inferExprKind = {};

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 11,
      .sourceColumn = 7,
      .semanticNodeId = 163,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .stdlibSurfaceId = std::nullopt,
  });
  const primec::SymbolId callNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "lookup");
  const primec::SymbolId resolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup");
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<i32, FileError>",
      .bindingTypeText = "Result<i32, FileError>",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .sourceLine = 11,
      .sourceColumn = 7,
      .semanticNodeId = 0,
      .callNameId = callNameId,
      .resolvedPathId = resolvedPathId,
  });
  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  primec::ir_lowerer::ResultExprInfo out;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::resolveResultExprInfoFromLocals(queryExpr,
                                                                  {},
                                                                  resolveMethodCall,
                                                                  resolveDefinitionCall,
                                                                  lookupReturnInfo,
                                                                  inferExprKind,
                                                                  out,
                                                                  &semanticTargets,
                                                                  &error));
  CHECK(error == "missing semantic-product query fact: lookup");
}

TEST_CASE("ir lowerer inference dispatch requires semantic try facts") {
  const std::unordered_map<std::string, const primec::Definition *> defMap;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.inferLiteralOrNameExprKind = [](const primec::Expr &,
                                        const primec::ir_lowerer::LocalMap &,
                                        primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };
  state.inferCallExprBaseKind = [](const primec::Expr &,
                                   const primec::ir_lowerer::LocalMap &,
                                   primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };
  state.inferCallExprDirectReturnKind =
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &) {
        return primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved;
      };
  state.inferCallExprCountAccessGpuFallbackKind = [](const primec::Expr &,
                                                     const primec::ir_lowerer::LocalMap &,
                                                     primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };
  state.inferCallExprOperatorFallbackKind = [](const primec::Expr &,
                                               const primec::ir_lowerer::LocalMap &,
                                               primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };
  state.inferCallExprControlFlowFallbackKind = [](const primec::Expr &,
                                                  const primec::ir_lowerer::LocalMap &,
                                                  std::string &,
                                                  primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };
  state.inferCallExprPointerFallbackKind = [](const primec::Expr &,
                                              const primec::ir_lowerer::LocalMap &,
                                              primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };

  primec::Expr operandExpr;
  operandExpr.kind = primec::Expr::Kind::Name;
  operandExpr.name = "source";

  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.args = {operandExpr};
  tryExpr.semanticNodeId = 64;

  primec::SemanticProgram semanticProgram;
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<bool, FileError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<bool, FileError>",
      .valueType = "bool",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .sourceLine = 12,
      .sourceColumn = 9,
      .semanticNodeId = 64,
      .operandResolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindDispatchSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &) { return std::string{}; },
          .error = &error,
      },
      state,
      error));
  CHECK(state.inferExprKind(tryExpr, {}) == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
  CHECK(error.empty());

  primec::SemanticProgram missingSemanticProgram;
  const auto missingSemanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&missingSemanticProgram);
  state.semanticProgram = &missingSemanticProgram;
  state.semanticIndex = &missingSemanticIndex;
  error.clear();
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindDispatchSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &) { return std::string{}; },
          .error = &error,
      },
      state,
      error));
  CHECK(state.inferExprKind(tryExpr, {}) == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(error == "missing semantic-product try fact: try");
}

TEST_CASE("ir lowerer inference dispatch rejects operand-path semantic try fallback in production mode") {
  const std::unordered_map<std::string, const primec::Definition *> defMap;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.inferLiteralOrNameExprKind = [](const primec::Expr &,
                                        const primec::ir_lowerer::LocalMap &,
                                        primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };
  state.inferCallExprBaseKind = [](const primec::Expr &,
                                   const primec::ir_lowerer::LocalMap &,
                                   primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };
  state.inferCallExprDirectReturnKind =
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo::ValueKind &) {
        return primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved;
      };
  state.inferCallExprCountAccessGpuFallbackKind = [](const primec::Expr &,
                                                     const primec::ir_lowerer::LocalMap &,
                                                     primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };
  state.inferCallExprOperatorFallbackKind = [](const primec::Expr &,
                                               const primec::ir_lowerer::LocalMap &,
                                               primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };
  state.inferCallExprControlFlowFallbackKind = [](const primec::Expr &,
                                                  const primec::ir_lowerer::LocalMap &,
                                                  std::string &,
                                                  primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };
  state.inferCallExprPointerFallbackKind = [](const primec::Expr &,
                                              const primec::ir_lowerer::LocalMap &,
                                              primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };

  primec::Expr operandExpr;
  operandExpr.kind = primec::Expr::Kind::Call;
  operandExpr.name = "lookup";
  operandExpr.semanticNodeId = 164;

  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.args = {operandExpr};
  tryExpr.semanticNodeId = 165;
  tryExpr.sourceLine = 12;
  tryExpr.sourceColumn = 9;

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 12,
      .sourceColumn = 9,
      .semanticNodeId = 164,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<bool, FileError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<bool, FileError>",
      .valueType = "bool",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .sourceLine = 12,
      .sourceColumn = 9,
      .semanticNodeId = 0,
      .operandResolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindDispatchSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &) { return std::string{}; },
          .error = &error,
      },
      state,
      error));
  CHECK(state.inferExprKind(tryExpr, {}) == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(error == "missing semantic-product try fact: try");
}

TEST_CASE("ir lowerer result helpers resolve direct Result.ok struct payload metadata") {
  primec::Definition labelDef;
  labelDef.fullPath = "/pkg/Label";
  labelDef.transforms.push_back(primec::Transform{"struct"});

  primec::Expr resultName;
  resultName.kind = primec::Expr::Kind::Name;
  resultName.name = "Result";

  primec::Expr labelCall;
  labelCall.kind = primec::Expr::Kind::Call;
  labelCall.name = "Label";

  primec::Expr okExpr;
  okExpr.kind = primec::Expr::Kind::Call;
  okExpr.isMethodCall = true;
  okExpr.name = "ok";
  okExpr.args = {resultName, labelCall};

  auto resolveMethodCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  auto resolveDefinitionCall = [&](const primec::Expr &expr) -> const primec::Definition * {
    if (expr.kind == primec::Expr::Kind::Call && expr.name == "Label") {
      return &labelDef;
    }
    return nullptr;
  };
  auto lookupReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; };
  const primec::ir_lowerer::InferExprKindWithLocalsFn inferExprKind = {};

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      okExpr, {}, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(out.valueStructType == "/pkg/Label");
}

TEST_CASE("ir lowerer result helpers resolve direct Result.ok comparison payload metadata") {
  primec::Expr resultName;
  resultName.kind = primec::Expr::Kind::Name;
  resultName.name = "Result";

  primec::Expr lhs;
  lhs.kind = primec::Expr::Kind::Literal;
  lhs.literalValue = 1;

  primec::Expr rhs = lhs;
  rhs.literalValue = 0;

  primec::Expr comparisonExpr;
  comparisonExpr.kind = primec::Expr::Kind::Call;
  comparisonExpr.name = "greater_than";
  comparisonExpr.args = {lhs, rhs};

  primec::Expr okExpr;
  okExpr.kind = primec::Expr::Kind::Call;
  okExpr.isMethodCall = true;
  okExpr.name = "ok";
  okExpr.args = {resultName, comparisonExpr};

  auto resolveMethodCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  auto resolveDefinitionCall = [](const primec::Expr &) -> const primec::Definition * { return nullptr; };
  auto lookupReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; };
  auto inferExprKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      okExpr, {}, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
  CHECK(out.valueStructType.empty());
}

TEST_CASE("ir lowerer result helpers emit direct Result.ok comparison payloads") {
  using EmitResult = primec::ir_lowerer::ResultOkMethodCallEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr resultType;
  resultType.kind = primec::Expr::Kind::Name;
  resultType.name = "Result";

  primec::Expr lhs;
  lhs.kind = primec::Expr::Kind::Literal;
  lhs.literalValue = 1;

  primec::Expr rhs = lhs;
  rhs.literalValue = 0;

  primec::Expr comparisonExpr;
  comparisonExpr.kind = primec::Expr::Kind::Call;
  comparisonExpr.name = "greater_than";
  comparisonExpr.args = {lhs, rhs};

  primec::Expr okExpr;
  okExpr.kind = primec::Expr::Kind::Call;
  okExpr.isMethodCall = true;
  okExpr.name = "ok";
  okExpr.args = {resultType, comparisonExpr};

  primec::ir_lowerer::LocalMap locals;
  bool emitCalled = false;
  std::string error;
  const auto inferNoStruct = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
    return std::string{};
  };
  const auto resolveNoDefinitionCall = [](const primec::Expr &) -> const primec::Definition * {
    return nullptr;
  };
  const auto resolveNoStructLayout = [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) {
    return false;
  };

  CHECK(primec::ir_lowerer::tryEmitResultOkCall(
            okExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return ValueKind::Unknown;
            },
            inferNoStruct,
            resolveNoDefinitionCall,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              emitCalled = true;
              return true;
            },
            []() { return 17; },
            resolveNoStructLayout,
            [&](primec::IrOpcode, uint64_t) {},
            error) ==
        EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(emitCalled);
}

TEST_CASE("ir lowerer result helpers resolve Result.map struct payload metadata") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localResult;
  localResult.isResult = true;
  localResult.resultHasValue = true;
  localResult.resultValueStructType = "/pkg/Label";
  localResult.resultErrorType = "FileError";
  locals.emplace("source", localResult);

  primec::Expr resultName;
  resultName.kind = primec::Expr::Kind::Name;
  resultName.name = "Result";

  primec::Expr sourceExpr;
  sourceExpr.kind = primec::Expr::Kind::Name;
  sourceExpr.name = "source";

  primec::Expr paramExpr;
  paramExpr.kind = primec::Expr::Kind::Name;
  paramExpr.name = "value";

  primec::Expr lambdaExpr;
  lambdaExpr.isLambda = true;
  lambdaExpr.args.push_back(paramExpr);
  lambdaExpr.bodyArguments.push_back(paramExpr);

  primec::Expr mapExpr;
  mapExpr.kind = primec::Expr::Kind::Call;
  mapExpr.isMethodCall = true;
  mapExpr.name = "map";
  mapExpr.args = {resultName, sourceExpr, lambdaExpr};

  auto resolveMethodCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  auto resolveDefinitionCall = [](const primec::Expr &) -> const primec::Definition * {
    return nullptr;
  };
  auto lookupReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; };
  auto inferExprKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      mapExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueStructType == "/pkg/Label");
}

TEST_SUITE_END();
