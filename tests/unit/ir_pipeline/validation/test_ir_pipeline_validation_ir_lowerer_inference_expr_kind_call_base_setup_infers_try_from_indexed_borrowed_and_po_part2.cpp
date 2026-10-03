#include "test_ir_pipeline_validation_callback_factories.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer inference expr-kind call-base setup uses semantic File receiver facts") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  auto addBindingFact = [](primec::SemanticProgram &semanticProgram,
                           uint64_t semanticNodeId,
                           const std::string &bindingTypeText) {
    const size_t index = semanticProgram.bindingFacts.size();
    semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
        .scopePath = "/main",
        .siteKind = "local",
        .name = "file",
        .bindingTypeText = bindingTypeText,
        .isMutable = false,
        .isEntryArgString = false,
        .isUnsafeReference = false,
        .referenceRoot = "",
        .sourceLine = 0,
        .sourceColumn = 0,
        .semanticNodeId = semanticNodeId,
        .provenanceHandle = 0,
        .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
        .siteKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "local"),
        .nameId = primec::semanticProgramInternCallTargetString(semanticProgram, "file"),
        .resolvedPathId = primec::InvalidSymbolId,
        .bindingTypeTextId =
            primec::semanticProgramInternCallTargetString(semanticProgram, bindingTypeText),
        .referenceRootId = primec::InvalidSymbolId,
    });
    semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
        semanticNodeId, index);
  };

  auto makeFileMethodExpr = [](const std::string &methodName,
                               uint64_t receiverSemanticNodeId) {
    primec::Expr receiver;
    receiver.kind = primec::Expr::Kind::Name;
    receiver.name = "file";
    receiver.semanticNodeId = receiverSemanticNodeId;

    primec::Expr methodExpr;
    methodExpr.kind = primec::Expr::Kind::Call;
    methodExpr.isMethodCall = true;
    methodExpr.name = methodName;
    methodExpr.args = {receiver};
    return methodExpr;
  };

  auto makeState = [](const primec::SemanticProgram *semanticProgram,
                      const primec::ir_lowerer::SemanticProductIndex *semanticIndex) {
    primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
    state.semanticProgram = semanticProgram;
    state.semanticIndex = semanticIndex;
    std::string error;
    CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallBaseSetup(
        {
            .inferStructExprPath =
                [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                  return std::string();
                },
            .resolveStructFieldSlot =
                [](const std::string &,
                   const std::string &,
                   primec::ir_lowerer::StructSlotFieldInfo &) { return false; },
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
    return state;
  };

  primec::SemanticProgram semanticProgram;
  addBindingFact(semanticProgram, 921, "/std/file/File<Write>");
  addBindingFact(semanticProgram, 922, "i32");
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  auto state = makeState(&semanticProgram, &semanticIndex);

  ValueKind kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(
      makeFileMethodExpr("write", 921), primec::ir_lowerer::LocalMap{}, kindOut));
  CHECK(kindOut == ValueKind::Int32);

  primec::ir_lowerer::LocalInfo staleLocal;
  staleLocal.isFileHandle = true;
  primec::ir_lowerer::LocalMap staleLocals;
  staleLocals.emplace("file", staleLocal);
  kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(
      makeFileMethodExpr("flush", 922), staleLocals, kindOut));
  CHECK(kindOut == ValueKind::Unknown);

  auto syntaxState = makeState(nullptr, nullptr);
  kindOut = ValueKind::Unknown;
  CHECK(syntaxState.inferCallExprBaseKind(
      makeFileMethodExpr("close", 0), staleLocals, kindOut));
  CHECK(kindOut == ValueKind::Int32);
}

TEST_CASE("ir lowerer inference expr-kind call-base setup uses semantic query and local-auto receiver facts") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  auto addQueryFact = [](primec::SemanticProgram &semanticProgram,
                         uint64_t semanticNodeId,
                         const std::string &queryTypeText) {
    const size_t index = semanticProgram.queryFacts.size();
    semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
        .scopePath = "/main",
        .callName = "receiver",
        .queryTypeText = queryTypeText,
        .bindingTypeText = queryTypeText,
        .receiverBindingTypeText = "",
        .hasResultType = false,
        .resultTypeHasValue = false,
        .resultValueType = "",
        .resultErrorType = "",
        .sourceLine = 0,
        .sourceColumn = 0,
        .semanticNodeId = semanticNodeId,
        .provenanceHandle = 0,
        .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
        .callNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "receiver"),
        .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main/receiver"),
        .queryTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, queryTypeText),
        .bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, queryTypeText),
    });
    semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(
        semanticNodeId, index);
  };

  auto addLocalAutoFact = [](primec::SemanticProgram &semanticProgram,
                             uint64_t semanticNodeId,
                             const std::string &bindingTypeText) {
    const size_t index = semanticProgram.localAutoFacts.size();
    semanticProgram.localAutoFacts.push_back(primec::SemanticProgramLocalAutoFact{
        .scopePath = "/main",
        .bindingName = "receiver",
        .bindingTypeText = bindingTypeText,
        .initializerBindingTypeText = bindingTypeText,
        .initializerReceiverBindingTypeText = "",
        .initializerQueryTypeText = "",
        .initializerResultHasValue = false,
        .initializerResultValueType = "",
        .initializerResultErrorType = "",
        .initializerHasTry = false,
        .initializerTryOperandResolvedPath = "",
        .initializerTryOperandBindingTypeText = "",
        .initializerTryOperandReceiverBindingTypeText = "",
        .initializerTryOperandQueryTypeText = "",
        .initializerTryValueType = "",
        .initializerTryErrorType = "",
        .initializerTryContextReturnKind = "",
        .initializerTryOnErrorHandlerPath = "",
        .initializerTryOnErrorErrorType = "",
        .initializerTryOnErrorBoundArgCount = 0,
        .sourceLine = 0,
        .sourceColumn = 0,
        .semanticNodeId = semanticNodeId,
        .provenanceHandle = 0,
        .initializerDirectCallResolvedPath = "",
        .initializerDirectCallReturnKind = "",
        .initializerMethodCallResolvedPath = "",
        .initializerMethodCallReturnKind = "",
        .initializerStdlibSurfaceId = std::nullopt,
        .initializerDirectCallStdlibSurfaceId = std::nullopt,
        .initializerMethodCallStdlibSurfaceId = std::nullopt,
        .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
        .bindingNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "receiver"),
        .bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, bindingTypeText),
    });
    semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.insert_or_assign(
        semanticNodeId, index);
  };

  auto makeNameExpr = [](const std::string &name, uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };

  auto makeLiteralExpr = [] {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Literal;
    expr.intWidth = 32;
    expr.literalValue = 0;
    return expr;
  };

  auto makeCallExpr = [](const std::string &name, uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = name;
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };

  auto makeAtExpr = [&](const std::string &name, uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "at";
    expr.args = {makeNameExpr(name, 0), makeLiteralExpr()};
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };

  auto makeDereferenceExpr = [](primec::Expr target) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "dereference";
    expr.args = {target};
    return expr;
  };

  auto makeMethodExpr = [](const std::string &methodName, primec::Expr receiver) {
    primec::Expr methodExpr;
    methodExpr.kind = primec::Expr::Kind::Call;
    methodExpr.isMethodCall = true;
    methodExpr.name = methodName;
    methodExpr.args = {receiver};
    return methodExpr;
  };

  auto makeState = [](const primec::SemanticProgram *semanticProgram,
                      const primec::ir_lowerer::SemanticProductIndex *semanticIndex) {
    primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
    state.semanticProgram = semanticProgram;
    state.semanticIndex = semanticIndex;
    std::string error;
    CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallBaseSetup(
        {
            .inferStructExprPath =
                [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                  return std::string();
                },
            .resolveStructFieldSlot =
                [](const std::string &,
                   const std::string &,
                   primec::ir_lowerer::StructSlotFieldInfo &) { return false; },
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
    return state;
  };

  primec::SemanticProgram semanticProgram;
  addQueryFact(semanticProgram, 941, "/std/file/FileError");
  addQueryFact(semanticProgram, 942, "i32");
  addQueryFact(semanticProgram, 945, "/std/file/File<Read>");
  addQueryFact(semanticProgram, 946, "/std/file/FileError");
  addQueryFact(semanticProgram, 947, "i32");
  addQueryFact(semanticProgram, 948, "/std/file/File<Write>");
  addQueryFact(semanticProgram, 949, "i32");
  addLocalAutoFact(semanticProgram, 943, "/std/file/File<Write>");
  addLocalAutoFact(semanticProgram, 944, "i32");
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  auto state = makeState(&semanticProgram, &semanticIndex);

  ValueKind kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(
      makeMethodExpr("why", makeCallExpr("load_error", 941)),
      primec::ir_lowerer::LocalMap{},
      kindOut));
  CHECK(kindOut == ValueKind::String);

  kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(
      makeMethodExpr("read_byte", makeCallExpr("open_file", 945)),
      primec::ir_lowerer::LocalMap{},
      kindOut));
  CHECK(kindOut == ValueKind::Int32);

  kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(
      makeMethodExpr("write_line", makeNameExpr("file", 943)),
      primec::ir_lowerer::LocalMap{},
      kindOut));
  CHECK(kindOut == ValueKind::Int32);

  primec::ir_lowerer::LocalInfo staleFileErrorPack;
  staleFileErrorPack.isArgsPack = true;
  staleFileErrorPack.isFileError = true;
  staleFileErrorPack.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Value;

  primec::ir_lowerer::LocalInfo staleBorrowedFileErrorPack;
  staleBorrowedFileErrorPack.isArgsPack = true;
  staleBorrowedFileErrorPack.isFileError = true;
  staleBorrowedFileErrorPack.argsPackElementKind =
      primec::ir_lowerer::LocalInfo::Kind::Reference;

  primec::ir_lowerer::LocalInfo staleBorrowedFileHandlePack;
  staleBorrowedFileHandlePack.isArgsPack = true;
  staleBorrowedFileHandlePack.isFileHandle = true;
  staleBorrowedFileHandlePack.argsPackElementKind =
      primec::ir_lowerer::LocalInfo::Kind::Reference;

  primec::ir_lowerer::LocalInfo staleFileHandle;
  staleFileHandle.isFileHandle = true;

  primec::ir_lowerer::LocalMap staleLocals;
  staleLocals.emplace("errors", staleFileErrorPack);
  staleLocals.emplace("file", staleFileHandle);

  primec::ir_lowerer::LocalMap borrowedStaleLocals;
  borrowedStaleLocals.emplace("errors", staleBorrowedFileErrorPack);

  primec::ir_lowerer::LocalMap borrowedStaleFileLocals;
  borrowedStaleFileLocals.emplace("files", staleBorrowedFileHandlePack);

  kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(
      makeMethodExpr("why", makeAtExpr("errors", 942)), staleLocals, kindOut));
  CHECK(kindOut == ValueKind::Unknown);

  kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(
      makeMethodExpr("why",
                     makeDereferenceExpr(makeAtExpr("errors", 946))),
      borrowedStaleLocals,
      kindOut));
  CHECK(kindOut == ValueKind::String);

  kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(
      makeMethodExpr("why",
                     makeDereferenceExpr(makeAtExpr("errors", 947))),
      borrowedStaleLocals,
      kindOut));
  CHECK(kindOut == ValueKind::Unknown);

  kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(
      makeMethodExpr("flush",
                     makeDereferenceExpr(makeAtExpr("files", 948))),
      borrowedStaleFileLocals,
      kindOut));
  CHECK(kindOut == ValueKind::Int32);

  kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(
      makeMethodExpr("flush",
                     makeDereferenceExpr(makeAtExpr("files", 949))),
      borrowedStaleFileLocals,
      kindOut));
  CHECK(kindOut == ValueKind::Unknown);

  kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(
      makeMethodExpr("flush", makeNameExpr("file", 944)), staleLocals, kindOut));
  CHECK(kindOut == ValueKind::Unknown);

  auto syntaxState = makeState(nullptr, nullptr);
  kindOut = ValueKind::Unknown;
  CHECK_FALSE(syntaxState.inferCallExprBaseKind(
      makeMethodExpr("why",
                     makeDereferenceExpr(makeAtExpr("errors", 0))),
      borrowedStaleLocals,
      kindOut));
  CHECK(kindOut == ValueKind::Unknown);

  kindOut = ValueKind::Unknown;
  CHECK_FALSE(syntaxState.inferCallExprBaseKind(
      makeMethodExpr("flush",
                     makeDereferenceExpr(makeAtExpr("files", 0))),
      borrowedStaleFileLocals,
      kindOut));
  CHECK(kindOut == ValueKind::Unknown);
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
      primec::validation_test_support::defaultCallBaseSetupInput(),
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
  semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.insert_or_assign(901, 0);
  const auto semanticIndex = primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallBaseSetup(
      primec::validation_test_support::defaultCallBaseSetupInput(),
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
      primec::validation_test_support::defaultCallBaseSetupInput(),
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

TEST_CASE("ir lowerer inference expr-kind call-base setup uses semantic map receiver facts") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  auto intern = [](primec::SemanticProgram &semanticProgram, const std::string &text) {
    return primec::semanticProgramInternCallTargetString(semanticProgram, text);
  };

  auto addCollectionFact = [&](primec::SemanticProgram &semanticProgram,
                               uint64_t semanticNodeId,
                               const std::string &family,
                               const std::string &keyType,
                               const std::string &valueType) {
    primec::SemanticProgramCollectionSpecialization fact;
    fact.scopePath = "/main";
    fact.siteKind = "local";
    fact.name = "values";
    fact.collectionFamily = family;
    fact.bindingTypeText = family + "<" + keyType + ", " + valueType + ">";
    fact.keyTypeText = keyType;
    fact.valueTypeText = valueType;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = intern(semanticProgram, "/main");
    fact.siteKindId = intern(semanticProgram, "local");
    fact.nameId = intern(semanticProgram, "values");
    fact.collectionFamilyId = intern(semanticProgram, family);
    fact.bindingTypeTextId = intern(semanticProgram, fact.bindingTypeText);
    fact.keyTypeTextId = intern(semanticProgram, keyType);
    fact.valueTypeTextId = intern(semanticProgram, valueType);
    const size_t index = semanticProgram.collectionSpecializations.size();
    semanticProgram.collectionSpecializations.push_back(std::move(fact));
    semanticProgram.publishedRoutingLookups.collectionSpecializationIndicesByExpr
        .insert_or_assign(semanticNodeId, index);
  };

  auto addBindingFact = [&](primec::SemanticProgram &semanticProgram,
                            uint64_t semanticNodeId,
                            const std::string &bindingTypeText) {
    primec::SemanticProgramBindingFact fact;
    fact.scopePath = "/main";
    fact.siteKind = "local";
    fact.name = "values";
    fact.bindingTypeText = bindingTypeText;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = intern(semanticProgram, "/main");
    fact.siteKindId = intern(semanticProgram, "local");
    fact.nameId = intern(semanticProgram, "values");
    fact.bindingTypeTextId = intern(semanticProgram, bindingTypeText);
    const size_t index = semanticProgram.bindingFacts.size();
    semanticProgram.bindingFacts.push_back(std::move(fact));
    semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr
        .insert_or_assign(semanticNodeId, index);
  };

  auto installBaseSetup =
      [](primec::ir_lowerer::LowerInferenceSetupBootstrapState &state) {
    std::string error;
    CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallBaseSetup(
        {
            .inferStructExprPath =
                [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                  return std::string();
                },
            .resolveStructFieldSlot =
                [](const std::string &,
                   const std::string &,
                   primec::ir_lowerer::StructSlotFieldInfo &) { return false; },
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
  };

  auto makeNameExpr = [](const std::string &name, uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };

  auto makeLiteralExpr = [] {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Literal;
    expr.intWidth = 32;
    expr.literalValue = 7;
    return expr;
  };

  auto makeTryAtExpr = [&](primec::Expr receiver) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "tryAt";
    expr.args = {std::move(receiver), makeLiteralExpr()};
    return expr;
  };

  auto makeTryExpr = [](primec::Expr operand) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "try";
    expr.args = {std::move(operand)};
    return expr;
  };

  primec::SemanticProgram semanticProgram;
  addCollectionFact(semanticProgram, 9931, "map", "i32", "string");
  addBindingFact(semanticProgram, 9932, "/std/collections/map<i32, bool>");
  addBindingFact(semanticProgram, 9933, "i32");
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LocalInfo staleMapInfo;
  staleMapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleMapInfo.keyValueKeyKind = ValueKind::Int32;
  staleMapInfo.keyValueValueKind = ValueKind::Int32;

  primec::ir_lowerer::LocalMap staleLocals;
  staleLocals.emplace("values", staleMapInfo);

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;
  installBaseSetup(state);

  ValueKind kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(
      makeTryExpr(makeTryAtExpr(makeNameExpr("values", 9931))),
      staleLocals,
      kindOut));
  CHECK(kindOut == ValueKind::String);

  kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(
      makeTryExpr(makeTryAtExpr(makeNameExpr("values", 9932))),
      staleLocals,
      kindOut));
  CHECK(kindOut == ValueKind::Bool);

  kindOut = ValueKind::Unknown;
  CHECK(state.inferCallExprBaseKind(
      makeTryExpr(makeTryAtExpr(makeNameExpr("values", 9933))),
      staleLocals,
      kindOut));
  CHECK(kindOut == ValueKind::Unknown);

  primec::ir_lowerer::LowerInferenceSetupBootstrapState syntaxState;
  installBaseSetup(syntaxState);
  kindOut = ValueKind::Unknown;
  CHECK(syntaxState.inferCallExprBaseKind(
      makeTryExpr(makeTryAtExpr(makeNameExpr("values", 0))),
      staleLocals,
      kindOut));
  CHECK(kindOut == ValueKind::Int32);
}

TEST_CASE("ir lowerer semantic-product index requires published query and try semantic-id maps") {
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
      .semanticNodeId = 9301,
      .provenanceHandle = 0,
  });
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<i64, FileError>",
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
      .semanticNodeId = 9302,
  });

  const auto semanticIndex = primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::Expr queryExpr;
  queryExpr.kind = primec::Expr::Kind::Call;
  queryExpr.name = "lookup";
  queryExpr.semanticNodeId = 9301;
  const auto *queryFact =
      primec::ir_lowerer::findSemanticProductQueryFactBySemanticId(semanticIndex, queryExpr);
  CHECK(queryFact == nullptr);

  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(9301, 0);
  const auto mappedSemanticIndex = primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  const auto *mappedQueryFact =
      primec::ir_lowerer::findSemanticProductQueryFactBySemanticId(mappedSemanticIndex, queryExpr);
  REQUIRE(mappedQueryFact != nullptr);
  CHECK(mappedQueryFact->semanticNodeId == 9301);

  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.semanticNodeId = 9302;
  const auto *tryFact =
      primec::ir_lowerer::findSemanticProductTryFactBySemanticId(semanticIndex, tryExpr);
  CHECK(tryFact == nullptr);

  semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.insert_or_assign(9302, 0);
  const auto mappedTrySemanticIndex = primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  const auto *mappedTryFact =
      primec::ir_lowerer::findSemanticProductTryFactBySemanticId(mappedTrySemanticIndex, tryExpr);
  REQUIRE(mappedTryFact != nullptr);
  CHECK(mappedTryFact->semanticNodeId == 9302);
}

TEST_CASE("ir lowerer inference expr-kind call-base setup leaves builtin comparison kind unresolved without semantic facts") {
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallBaseSetup(
      primec::validation_test_support::defaultCallBaseSetupInput(),
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
  mapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  mapInfo.keyValueKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  mapInfo.keyValueValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
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

  valuesName.semanticNodeId = 4242;
  tryAtCall.args = {valuesName};
  containsCall.args = {valuesName};

  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "values",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = valuesName.semanticNodeId,
      .resolvedPathId = primec::InvalidSymbolId,
      .bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      valuesName.semanticNodeId, 0);
  const auto semanticIndex = primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK_FALSE(primec::ir_lowerer::inferMapTryAtResultValueKind(
      tryAtCall, locals, kindOut, &semanticProgram, &semanticIndex));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK_FALSE(primec::ir_lowerer::inferMapContainsResultKind(
      containsCall, locals, kindOut, &semanticProgram, &semanticIndex));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
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
