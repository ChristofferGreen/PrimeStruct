#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer inference expr-kind call-base setup infers try from indexed borrowed and pointer Result args packs") {
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallBaseSetup(
      {
          .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
          .resolveStructFieldSlot =
              [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) { return false; },
          .resolveUninitializedStorage =
              [](const primec::Expr &,
                 const primec::ir_lowerer::LocalMap &,
                 primec::ir_lowerer::UninitializedStorageAccessInfo &,
                 bool &resolved) {
                resolved = false;
                return true;
              },
      },
      state,
      error));
  CHECK(error.empty());
  REQUIRE(static_cast<bool>(state.inferCallExprBaseKind));

  auto makeResultPackInfo = [](primec::ir_lowerer::LocalInfo::Kind elementKind) {
    primec::ir_lowerer::LocalInfo info;
    info.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
    info.isArgsPack = true;
    info.argsPackElementKind = elementKind;
    info.isResult = true;
    info.resultHasValue = true;
    info.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
    info.resultErrorType = "ParseError";
    return info;
  };
  auto makeNameExpr = [](const std::string &name) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    return expr;
  };
  auto makeLiteralExpr = [](int64_t value) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Literal;
    expr.intWidth = 32;
    expr.literalValue = value;
    return expr;
  };
  auto makeTryDereferenceAtExpr = [&](const std::string &packName) {
    primec::Expr atExpr;
    atExpr.kind = primec::Expr::Kind::Call;
    atExpr.name = "at";
    atExpr.args = {makeNameExpr(packName), makeLiteralExpr(0)};

    primec::Expr dereferenceExpr;
    dereferenceExpr.kind = primec::Expr::Kind::Call;
    dereferenceExpr.name = "dereference";
    dereferenceExpr.args = {atExpr};

    primec::Expr tryExpr;
    tryExpr.kind = primec::Expr::Kind::Call;
    tryExpr.name = "try";
    tryExpr.args = {dereferenceExpr};
    return tryExpr;
  };

  primec::ir_lowerer::LocalMap locals;
  locals.emplace("borrowedValues", makeResultPackInfo(primec::ir_lowerer::LocalInfo::Kind::Reference));
  locals.emplace("pointerValues", makeResultPackInfo(primec::ir_lowerer::LocalInfo::Kind::Pointer));

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(makeTryDereferenceAtExpr("borrowedValues"), locals, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::String);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(makeTryDereferenceAtExpr("pointerValues"), locals, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::String);
}

TEST_CASE("ir lowerer inference expr-kind call-base setup uses semantic query facts for scalar call kinds") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/Holder/check",
      .callName = "greater_than",
      .queryTypeText = "i64",
      .bindingTypeText = "i64",
      .receiverBindingTypeText = "",
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 501,
      .provenanceHandle = 0,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/Holder/check"),
      .callNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "greater_than"),
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/Holder"),
      .queryTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "bool"),
      .bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "bool"),
  });
  const auto semanticIndex = primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallBaseSetup(
      {
          .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
          .resolveStructFieldSlot =
              [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) { return false; },
          .resolveUninitializedStorage =
              [](const primec::Expr &,
                 const primec::ir_lowerer::LocalMap &,
                 primec::ir_lowerer::UninitializedStorageAccessInfo &,
                 bool &resolved) {
                resolved = false;
                return true;
              },
      },
      state,
      error));
  CHECK(error.empty());
  REQUIRE(static_cast<bool>(state.inferCallExprBaseKind));

  primec::Expr lhs;
  lhs.kind = primec::Expr::Kind::Literal;
  lhs.intWidth = 32;
  lhs.literalValue = 1;

  primec::Expr rhs = lhs;
  rhs.literalValue = 0;

  primec::Expr comparisonExpr;
  comparisonExpr.kind = primec::Expr::Kind::Call;
  comparisonExpr.name = "greater_than";
  comparisonExpr.args = {lhs, rhs};
  comparisonExpr.semanticNodeId = 501;

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(comparisonExpr, primec::ir_lowerer::LocalMap{}, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
}

TEST_CASE("ir lowerer inference expr-kind call-base setup uses semantic Result.ok payload facts") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::SemanticProgram semanticProgram;
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "bool",
      .bindingTypeText = "bool",
      .receiverBindingTypeText = "",
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 801,
      .provenanceHandle = 0,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .callNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "lookup"),
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main/lookup"),
      .queryTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "bool"),
      .bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "bool"),
  });
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "payload",
      .bindingTypeText = "",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 803,
      .resolvedPathId = primec::InvalidSymbolId,
      .bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "bool"),
  });
  const auto semanticIndex = primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallBaseSetup(
      {
          .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
          .resolveStructFieldSlot =
              [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) { return false; },
          .resolveUninitializedStorage =
              [](const primec::Expr &,
                 const primec::ir_lowerer::LocalMap &,
                 primec::ir_lowerer::UninitializedStorageAccessInfo &,
                 bool &resolved) {
                resolved = false;
                return true;
              },
      },
      state,
      error));
  CHECK(error.empty());
  REQUIRE(static_cast<bool>(state.inferCallExprBaseKind));

  bool fallbackCalled = false;
  state.inferExprKind = [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
    fallbackCalled = true;
    return ValueKind::Int64;
  };

  primec::Expr resultType;
  resultType.kind = primec::Expr::Kind::Name;
  resultType.name = "Result";

  primec::Expr payloadExpr;
  payloadExpr.kind = primec::Expr::Kind::Call;
  payloadExpr.name = "lookup";
  payloadExpr.semanticNodeId = 801;

  primec::Expr okExpr;
  okExpr.kind = primec::Expr::Kind::Call;
  okExpr.isMethodCall = true;
  okExpr.name = "ok";
  okExpr.args = {resultType, payloadExpr};
  okExpr.semanticNodeId = 802;

  ValueKind kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(okExpr, primec::ir_lowerer::LocalMap{}, kindOut));
  CHECK(kindOut == ValueKind::Bool);
  CHECK_FALSE(fallbackCalled);

  primec::Expr payloadName;
  payloadName.kind = primec::Expr::Kind::Name;
  payloadName.name = "payload";
  payloadName.semanticNodeId = 803;

  primec::Expr okNameExpr = okExpr;
  okNameExpr.args = {resultType, payloadName};
  okNameExpr.semanticNodeId = 804;

  primec::ir_lowerer::LocalInfo stalePayload;
  stalePayload.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  stalePayload.valueKind = ValueKind::Int64;
  const primec::ir_lowerer::LocalMap locals{{"payload", stalePayload}};

  fallbackCalled = false;
  kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(okNameExpr, locals, kindOut));
  CHECK(kindOut == ValueKind::Bool);
  CHECK_FALSE(fallbackCalled);
}

TEST_CASE("ir lowerer inference expr-kind call-base setup does not infer missing Result method facts from fallback") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::SemanticProgram semanticProgram;
  const auto semanticIndex = primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallBaseSetup(
      {
          .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
          .resolveStructFieldSlot =
              [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) { return false; },
          .resolveUninitializedStorage =
              [](const primec::Expr &,
                 const primec::ir_lowerer::LocalMap &,
                 primec::ir_lowerer::UninitializedStorageAccessInfo &,
                 bool &resolved) {
                resolved = false;
                return true;
              },
      },
      state,
      error));
  CHECK(error.empty());
  REQUIRE(static_cast<bool>(state.inferCallExprBaseKind));

  bool fallbackCalled = false;
  state.inferExprKind = [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
    fallbackCalled = true;
    return ValueKind::Int64;
  };

  primec::Expr resultType;
  resultType.kind = primec::Expr::Kind::Name;
  resultType.name = "Result";

  primec::Expr payloadExpr;
  payloadExpr.kind = primec::Expr::Kind::Call;
  payloadExpr.name = "lookup";
  payloadExpr.semanticNodeId = 811;

  primec::Expr okExpr;
  okExpr.kind = primec::Expr::Kind::Call;
  okExpr.isMethodCall = true;
  okExpr.name = "ok";
  okExpr.args = {resultType, payloadExpr};
  okExpr.semanticNodeId = 812;

  ValueKind kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(okExpr, primec::ir_lowerer::LocalMap{}, kindOut));
  CHECK(kindOut == ValueKind::Unknown);

  primec::Expr errorExpr = okExpr;
  errorExpr.name = "error";
  errorExpr.args = {resultType};
  errorExpr.semanticNodeId = 813;

  kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(errorExpr, primec::ir_lowerer::LocalMap{}, kindOut));
  CHECK(kindOut == ValueKind::Unknown);

  primec::Expr whyExpr = errorExpr;
  whyExpr.name = "why";
  whyExpr.semanticNodeId = 814;

  kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(whyExpr, primec::ir_lowerer::LocalMap{}, kindOut));
  CHECK(kindOut == ValueKind::Unknown);
  CHECK_FALSE(fallbackCalled);
}

TEST_CASE("ir lowerer inference expr-kind call-base setup keeps syntax Result method fallback") {
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallBaseSetup(
      {
          .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
          .resolveStructFieldSlot =
              [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) { return false; },
          .resolveUninitializedStorage =
              [](const primec::Expr &,
                 const primec::ir_lowerer::LocalMap &,
                 primec::ir_lowerer::UninitializedStorageAccessInfo &,
                 bool &resolved) {
                resolved = false;
                return true;
              },
      },
      state,
      error));
  CHECK(error.empty());
  REQUIRE(static_cast<bool>(state.inferCallExprBaseKind));

  primec::Expr resultType;
  resultType.kind = primec::Expr::Kind::Name;
  resultType.name = "Result";

  primec::Expr payloadExpr;
  payloadExpr.kind = primec::Expr::Kind::Literal;
  payloadExpr.intWidth = 32;
  payloadExpr.literalValue = 1;

  primec::Expr okExpr;
  okExpr.kind = primec::Expr::Kind::Call;
  okExpr.isMethodCall = true;
  okExpr.name = "ok";
  okExpr.args = {resultType, payloadExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut =
      primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(okExpr, primec::ir_lowerer::LocalMap{}, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr errorExpr = okExpr;
  errorExpr.name = "error";
  errorExpr.args = {resultType};

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(errorExpr, primec::ir_lowerer::LocalMap{}, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Bool);

  primec::Expr whyExpr = errorExpr;
  whyExpr.name = "why";

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(whyExpr, primec::ir_lowerer::LocalMap{}, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::String);
}

TEST_CASE("ir lowerer inference expr-kind call-base setup does not infer missing query facts from fallback") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::SemanticProgram semanticProgram;
  const auto semanticIndex = primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallBaseSetup(
      {
          .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
          .resolveStructFieldSlot =
              [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) { return false; },
          .resolveUninitializedStorage =
              [](const primec::Expr &,
                 const primec::ir_lowerer::LocalMap &,
                 primec::ir_lowerer::UninitializedStorageAccessInfo &,
                 bool &resolved) {
                resolved = false;
                return true;
              },
      },
      state,
      error));
  CHECK(error.empty());
  REQUIRE(static_cast<bool>(state.inferCallExprBaseKind));

  bool fallbackCalled = false;
  state.inferExprKind = [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
    fallbackCalled = true;
    return ValueKind::Int64;
  };

  primec::Expr resultType;
  resultType.kind = primec::Expr::Kind::Name;
  resultType.name = "Result";

  primec::Expr queryExpr;
  queryExpr.kind = primec::Expr::Kind::Call;
  queryExpr.name = "lookup";
  queryExpr.semanticNodeId = 701;

  primec::Expr okExpr;
  okExpr.kind = primec::Expr::Kind::Call;
  okExpr.isMethodCall = true;
  okExpr.name = "ok";
  okExpr.args = {resultType, queryExpr};

  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.args = {okExpr};

  ValueKind kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(tryExpr, primec::ir_lowerer::LocalMap{}, kindOut));
  CHECK(kindOut == ValueKind::Unknown);
  CHECK_FALSE(fallbackCalled);
}

TEST_CASE("ir lowerer inference expr-kind call-base setup uses semantic try facts before local Result state") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::SemanticProgram semanticProgram;
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<string, FileError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "",
      .valueType = "i64",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 901,
      .valueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "i64"),
      .errorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
  });
  const auto semanticIndex = primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallBaseSetup(
      {
          .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
          .resolveStructFieldSlot =
              [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) { return false; },
          .resolveUninitializedStorage =
              [](const primec::Expr &,
                 const primec::ir_lowerer::LocalMap &,
                 primec::ir_lowerer::UninitializedStorageAccessInfo &,
                 bool &resolved) {
                resolved = false;
                return true;
              },
      },
      state,
      error));
  CHECK(error.empty());
  REQUIRE(static_cast<bool>(state.inferCallExprBaseKind));

  primec::ir_lowerer::LocalInfo staleLocalResult;
  staleLocalResult.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleLocalResult.isResult = true;
  staleLocalResult.resultHasValue = true;
  staleLocalResult.resultValueKind = ValueKind::String;

  primec::ir_lowerer::LocalMap locals;
  locals.emplace("result", staleLocalResult);

  primec::Expr resultExpr;
  resultExpr.kind = primec::Expr::Kind::Name;
  resultExpr.name = "result";

  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.args = {resultExpr};
  tryExpr.semanticNodeId = 901;

  ValueKind kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(tryExpr, locals, kindOut));
  CHECK(kindOut == ValueKind::Int64);
}

TEST_CASE("ir lowerer inference expr-kind call-base setup does not infer missing try facts from local Result state") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::SemanticProgram semanticProgram;
  const auto semanticIndex = primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallBaseSetup(
      {
          .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
          .resolveStructFieldSlot =
              [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) { return false; },
          .resolveUninitializedStorage =
              [](const primec::Expr &,
                 const primec::ir_lowerer::LocalMap &,
                 primec::ir_lowerer::UninitializedStorageAccessInfo &,
                 bool &resolved) {
                resolved = false;
                return true;
              },
      },
      state,
      error));
  CHECK(error.empty());
  REQUIRE(static_cast<bool>(state.inferCallExprBaseKind));

  primec::ir_lowerer::LocalInfo localResult;
  localResult.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  localResult.isResult = true;
  localResult.resultHasValue = true;
  localResult.resultValueKind = ValueKind::String;

  primec::ir_lowerer::LocalMap locals;
  locals.emplace("result", localResult);

  primec::Expr resultExpr;
  resultExpr.kind = primec::Expr::Kind::Name;
  resultExpr.name = "result";

  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.args = {resultExpr};
  tryExpr.semanticNodeId = 902;

  ValueKind kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(tryExpr, locals, kindOut));
  CHECK(kindOut == ValueKind::Unknown);
}

TEST_CASE("ir lowerer inference expr-kind call-base setup leaves builtin comparison kind unresolved without semantic facts") {
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallBaseSetup(
      {
          .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
          .resolveStructFieldSlot =
              [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) { return false; },
          .resolveUninitializedStorage =
              [](const primec::Expr &,
                 const primec::ir_lowerer::LocalMap &,
                 primec::ir_lowerer::UninitializedStorageAccessInfo &,
                 bool &resolved) {
                resolved = false;
                return true;
              },
      },
      state,
      error));
  CHECK(error.empty());
  REQUIRE(static_cast<bool>(state.inferCallExprBaseKind));

  state.inferExprKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  primec::Expr lhs;
  lhs.kind = primec::Expr::Kind::Literal;
  lhs.intWidth = 32;
  lhs.literalValue = 1;

  primec::Expr rhs = lhs;
  rhs.literalValue = 0;

  primec::Expr comparisonExpr;
  comparisonExpr.kind = primec::Expr::Kind::Call;
  comparisonExpr.name = "greater_than";
  comparisonExpr.args = {lhs, rhs};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK_FALSE(state.inferCallExprBaseKind(comparisonExpr, primec::ir_lowerer::LocalMap{}, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer inference expr-kind call-base setup validates dependencies") {
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerInferenceExprKindCallBaseSetup(
      {
          .inferStructExprPath = {},
          .resolveStructFieldSlot =
              [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) { return false; },
          .resolveUninitializedStorage =
              [](const primec::Expr &,
                 const primec::ir_lowerer::LocalMap &,
                 primec::ir_lowerer::UninitializedStorageAccessInfo &,
                 bool &) { return true; },
      },
      state,
      error));
  CHECK(error == "native backend missing inference expr-kind call-base setup dependency: inferStructExprPath");
}

TEST_CASE("ir lowerer inference base-kind helpers resolve parser-shaped canonical map result helpers") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo mapInfo;
  mapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  mapInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  mapInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("values", mapInfo);

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr tryAtCall;
  tryAtCall.kind = primec::Expr::Kind::Call;
  tryAtCall.name = "tryAt";
  tryAtCall.namespacePrefix = "/std/collections/map";
  tryAtCall.args = {valuesName};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut =
      primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::isMapTryAtCallName(tryAtCall));
  CHECK(primec::ir_lowerer::inferMapTryAtResultValueKind(tryAtCall, locals, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr containsCall;
  containsCall.kind = primec::Expr::Kind::Call;
  containsCall.name = "contains";
  containsCall.namespacePrefix = "/std/collections/map";
  containsCall.args = {valuesName};

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::isMapContainsCallName(containsCall));
  CHECK(primec::ir_lowerer::inferMapContainsResultKind(containsCall, locals, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
}

TEST_CASE("ir lowerer inference expr-kind call-return setup wires callback") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/callee", &callee},
  };
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    if (path != "/callee") {
      return false;
    }
    out.returnsVoid = false;
    out.returnsArray = false;
    out.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
    return true;
  };
  state.resolveMethodCallDefinition = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
    return static_cast<const primec::Definition *>(nullptr);
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &) { return std::string("/callee"); },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());
  CHECK(static_cast<bool>(state.inferCallExprDirectReturnKind));

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  const auto result = state.inferCallExprDirectReturnKind(callExpr, primec::ir_lowerer::LocalMap{}, kindOut);
  CHECK(result == primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer inference expr-kind call-return setup supports deferred return-info wiring") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/callee", &callee},
  };

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.resolveMethodCallDefinition = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
    return static_cast<const primec::Definition *>(nullptr);
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &) { return std::string("/callee"); },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());
  CHECK(static_cast<bool>(state.inferCallExprDirectReturnKind));

  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    if (path != "/callee") {
      return false;
    }
    out.returnsVoid = false;
    out.returnsArray = false;
    out.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
    return true;
  };

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  const auto result = state.inferCallExprDirectReturnKind(callExpr, primec::ir_lowerer::LocalMap{}, kindOut);
  CHECK(result == primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer inference call-return setup resolves namespaced count definition directly") {
  primec::Definition receiverCountDef;
  receiverCountDef.fullPath = "/vector/count";
  primec::Definition canonicalCountDef;
  canonicalCountDef.fullPath = "/std/collections/vector/count";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/collections/vector/count", &canonicalCountDef},
  };

  bool resolveReceiverHelper = true;
  int resolveMethodCalls = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    out.returnsVoid = false;
    out.returnsArray = false;
    if (path == "/vector/count") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      return true;
    }
    if (path == "/std/collections/vector/count") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
      return true;
    }
    return false;
  };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    ++resolveMethodCalls;
    if (!resolveReceiverHelper || !methodExpr.isMethodCall || methodExpr.name != "count" || methodExpr.args.empty()) {
      return nullptr;
    }
    if (methodExpr.args.front().kind == primec::Expr::Kind::Name && methodExpr.args.front().name == "values") {
      return &receiverCountDef;
    }
    return nullptr;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/std/collections/vector/count";
  callExpr.args = {receiverExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);
  CHECK(resolveMethodCalls == 0);

  resolveReceiverHelper = false;
  resolveMethodCalls = 0;
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);
  CHECK(resolveMethodCalls == 0);
}

TEST_CASE("ir lowerer inference call-return setup rejects vector alias count without compatibility definition") {
  primec::Definition canonicalCountDef;
  canonicalCountDef.fullPath = "/std/collections/vector/count";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/collections/vector/count", &canonicalCountDef},
  };

  int resolveMethodCalls = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    out.returnsVoid = false;
    out.returnsArray = false;
    if (path != "/std/collections/vector/count") {
      return false;
    }
    out.kind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
    return true;
  };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    ++resolveMethodCalls;
    return nullptr;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/vector/count";
  callExpr.args = {receiverExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(resolveMethodCalls == 0);
}

TEST_CASE("ir lowerer inference call-return setup rejects slashless vector alias count without compatibility definition") {
  primec::Definition canonicalCountDef;
  canonicalCountDef.fullPath = "/std/collections/vector/count";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/collections/vector/count", &canonicalCountDef},
  };

  int resolveMethodCalls = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    out.returnsVoid = false;
    out.returnsArray = false;
    if (path != "/std/collections/vector/count") {
      return false;
    }
    out.kind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
    return true;
  };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    ++resolveMethodCalls;
    return nullptr;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "vector/count";
  callExpr.args = {receiverExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(resolveMethodCalls == 0);
}

TEST_CASE("ir lowerer inference call-return setup rejects canonical return-info forwarding from compatibility vector count defs") {
  primec::Definition aliasCountDef;
  aliasCountDef.fullPath = "/vector/count";
  primec::Definition canonicalCountDef;
  canonicalCountDef.fullPath = "/std/collections/vector/count";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/vector/count", &aliasCountDef},
      {"/std/collections/vector/count", &canonicalCountDef},
  };

  int resolveMethodCalls = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    out.returnsVoid = false;
    out.returnsArray = false;
    if (path != "/std/collections/vector/count") {
      return false;
    }
    out.kind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
    return true;
  };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    ++resolveMethodCalls;
    return nullptr;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/vector/count";
  callExpr.args = {receiverExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::MatchedButUnsupported);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(resolveMethodCalls == 0);
}

TEST_CASE("ir lowerer inference call-return setup treats removed array count aliases as direct definitions") {
  primec::Definition arrayCountDef;
  arrayCountDef.fullPath = "/array/count";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/array/count", &arrayCountDef},
  };

  int resolveMethodCalls = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    out.returnsVoid = false;
    out.returnsArray = false;
    if (path == "/array/count") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
      return true;
    }
    return false;
  };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
        ++resolveMethodCalls;
        return nullptr;
      };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/array/count";
  callExpr.args = {receiverExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);
  CHECK(resolveMethodCalls == 0);
}

TEST_CASE("ir lowerer inference call-return setup keeps removed array count aliases unresolved without definitions") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  int resolveMethodCalls = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.getReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    ++resolveMethodCalls;
    return nullptr;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/array/count";
  callExpr.args = {receiverExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(resolveMethodCalls == 0);
}

TEST_CASE("ir lowerer inference call-return setup treats removed array capacity aliases as direct definitions") {
  primec::Definition arrayCapacityDef;
  arrayCapacityDef.fullPath = "/array/capacity";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/array/capacity", &arrayCapacityDef},
  };

  int resolveMethodCalls = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    out.returnsVoid = false;
    out.returnsArray = false;
    if (path == "/array/capacity") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
      return true;
    }
    return false;
  };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
        ++resolveMethodCalls;
        return nullptr;
      };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/array/capacity";
  callExpr.args = {receiverExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);
  CHECK(resolveMethodCalls == 0);
}

TEST_CASE("ir lowerer inference call-return setup keeps removed array capacity aliases unresolved without definitions") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  int resolveMethodCalls = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.getReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
        ++resolveMethodCalls;
        return nullptr;
      };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/array/capacity";
  callExpr.args = {receiverExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(resolveMethodCalls == 0);
}


TEST_SUITE_END();
