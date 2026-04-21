#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("emitter expr control if branch emit step composes value and handlers") {
  primec::Expr nonEnvelope;
  nonEnvelope.kind = primec::Expr::Kind::Name;
  nonEnvelope.name = "direct";

  std::unordered_map<std::string, primec::Emitter::BindingInfo> branchTypes;
  std::unordered_map<std::string, primec::Emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;

  int directEmitCalls = 0;
  const auto directResult = primec::emitter::runEmitterExprControlIfBranchEmitStep(
      nonEnvelope,
      branchTypes,
      returnKinds,
      false,
      importAliases,
      structTypeMap,
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &) {
        CHECK_FALSE(true);
        return false;
      },
      [&](const primec::Expr &) {
        CHECK_FALSE(true);
        return primec::Emitter::BindingInfo{};
      },
      [&](const primec::Expr &) {
        CHECK_FALSE(true);
        return false;
      },
      [&](const primec::Expr &, const auto &, const auto &, bool) {
        CHECK_FALSE(true);
        return primec::Emitter::ReturnKind::Unknown;
      },
      [&](primec::Emitter::ReturnKind) {
        CHECK_FALSE(true);
        return std::string{};
      },
      [&](const primec::Emitter::BindingInfo &) {
        CHECK_FALSE(true);
        return false;
      },
      [&](const primec::Expr &candidate) {
        ++directEmitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Name);
        CHECK(candidate.name == "direct");
        return std::string("emit_direct");
      });
  CHECK(directResult.handled);
  CHECK(directResult.emittedExpr == "emit_direct");
  CHECK(directEmitCalls == 1);

  primec::Expr returnArg;
  returnArg.kind = primec::Expr::Kind::Name;
  returnArg.name = "value";

  primec::Expr returnStmt;
  returnStmt.kind = primec::Expr::Kind::Call;
  returnStmt.name = "return";
  returnStmt.args = {returnArg};

  primec::Expr envelope;
  envelope.kind = primec::Expr::Kind::Call;
  envelope.bodyArguments = {returnStmt};

  int returnValueEmitCalls = 0;
  const auto wrapped = primec::emitter::runEmitterExprControlIfBranchEmitStep(
      envelope,
      branchTypes,
      returnKinds,
      false,
      importAliases,
      structTypeMap,
      [&](const primec::Expr &) { return true; },
      [&](const primec::Expr &candidate) {
        return candidate.kind == primec::Expr::Kind::Call && candidate.name == "return";
      },
      [&](const primec::Expr &) {
        CHECK_FALSE(true);
        return primec::Emitter::BindingInfo{};
      },
      [&](const primec::Expr &) {
        CHECK_FALSE(true);
        return false;
      },
      [&](const primec::Expr &, const auto &, const auto &, bool) {
        CHECK_FALSE(true);
        return primec::Emitter::ReturnKind::Unknown;
      },
      [&](primec::Emitter::ReturnKind) {
        CHECK_FALSE(true);
        return std::string{};
      },
      [&](const primec::Emitter::BindingInfo &) {
        CHECK_FALSE(true);
        return false;
      },
      [&](const primec::Expr &candidate) {
        ++returnValueEmitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Name);
        CHECK(candidate.name == "value");
        return std::string("emit_value");
      });
  CHECK(wrapped.handled);
  CHECK(wrapped.emittedExpr == "([&]() { return emit_value; }())");
  CHECK(returnValueEmitCalls == 1);

  const auto missingEmit = primec::emitter::runEmitterExprControlIfBranchEmitStep(
      nonEnvelope,
      branchTypes,
      returnKinds,
      false,
      importAliases,
      structTypeMap,
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &) { return primec::Emitter::BindingInfo{}; },
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &, const auto &, const auto &, bool) {
        return primec::Emitter::ReturnKind::Unknown;
      },
      [&](primec::Emitter::ReturnKind) { return std::string{}; },
      [&](const primec::Emitter::BindingInfo &) { return false; },
      {});
  CHECK_FALSE(missingEmit.handled);
  CHECK(missingEmit.emittedExpr.empty());
}

TEST_CASE("emitter expr control if branch body statement step dispatches statements") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Name;
  stmt.name = "value";

  int emitCalls = 0;
  const auto emitted = primec::emitter::runEmitterExprControlIfBranchBodyStatementStep(
      stmt,
      [&](const primec::Expr &candidate) {
        ++emitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Name);
        CHECK(candidate.name == "value");
        return std::string("emit_value");
      });
  CHECK(emitted.handled);
  CHECK(emitted.emitted.handled);
  CHECK(emitCalls == 1);
  CHECK(emitted.emitted.emittedStatement == "(void)emit_value; ");
  CHECK_FALSE(emitted.emitted.shouldBreak);

  const auto missingEmit = primec::emitter::runEmitterExprControlIfBranchBodyStatementStep(
      stmt,
      {});
  CHECK_FALSE(missingEmit.handled);
  CHECK_FALSE(missingEmit.emitted.handled);
  CHECK(missingEmit.emitted.emittedStatement.empty());
  CHECK_FALSE(missingEmit.emitted.shouldBreak);
}

TEST_CASE("emitter expr control if-block statement step emits void statements") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Name;
  stmt.name = "value";

  int emitCalls = 0;
  const auto emitted = primec::emitter::runEmitterExprControlIfBlockStatementStep(
      stmt,
      [&](const primec::Expr &candidate) {
        ++emitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Name);
        CHECK(candidate.name == "value");
        return std::string("emit_value");
      });
  CHECK(emitted.handled);
  CHECK(emitCalls == 1);
  CHECK(emitted.emittedStatement == "(void)emit_value; ");

  const auto missingEmit = primec::emitter::runEmitterExprControlIfBlockStatementStep(
      stmt,
      {});
  CHECK_FALSE(missingEmit.handled);
  CHECK(missingEmit.emittedStatement.empty());
}

TEST_CASE("emitter expr control if ternary step emits conditional expression") {
  int conditionCalls = 0;
  int thenCalls = 0;
  int elseCalls = 0;
  const auto emitted = primec::emitter::runEmitterExprControlIfTernaryStep(
      [&]() {
        ++conditionCalls;
        return std::string("cond_expr");
      },
      [&]() {
        ++thenCalls;
        return std::string("then_expr");
      },
      [&]() {
        ++elseCalls;
        return std::string("else_expr");
      });
  CHECK(emitted.handled);
  CHECK(conditionCalls == 1);
  CHECK(thenCalls == 1);
  CHECK(elseCalls == 1);
  CHECK(emitted.emittedExpr == "(cond_expr ? then_expr : else_expr)");

  const auto missingCondition =
      primec::emitter::runEmitterExprControlIfTernaryStep({}, [] { return std::string("then"); }, [] {
        return std::string("else");
      });
  CHECK_FALSE(missingCondition.handled);
  CHECK(missingCondition.emittedExpr.empty());

  const auto missingThen =
      primec::emitter::runEmitterExprControlIfTernaryStep([] { return std::string("cond"); }, {}, [] {
        return std::string("else");
      });
  CHECK_FALSE(missingThen.handled);
  CHECK(missingThen.emittedExpr.empty());

  const auto missingElse =
      primec::emitter::runEmitterExprControlIfTernaryStep([] { return std::string("cond"); }, [] {
        return std::string("then");
      }, {});
  CHECK_FALSE(missingElse.handled);
  CHECK(missingElse.emittedExpr.empty());
}

TEST_CASE("emitter expr control if ternary fallback step emits conditional expression") {
  int conditionCalls = 0;
  int thenCalls = 0;
  int elseCalls = 0;
  const auto emitted = primec::emitter::runEmitterExprControlIfTernaryFallbackStep(
      [&]() {
        ++conditionCalls;
        return std::string("cond_expr");
      },
      [&]() {
        ++thenCalls;
        return std::string("then_expr");
      },
      [&]() {
        ++elseCalls;
        return std::string("else_expr");
      });
  CHECK(emitted.handled);
  CHECK(conditionCalls == 1);
  CHECK(thenCalls == 1);
  CHECK(elseCalls == 1);
  CHECK(emitted.emittedExpr == "(cond_expr ? then_expr : else_expr)");

  const auto missingCondition =
      primec::emitter::runEmitterExprControlIfTernaryFallbackStep({}, [] { return std::string("then"); }, [] {
        return std::string("else");
      });
  CHECK_FALSE(missingCondition.handled);
  CHECK(missingCondition.emittedExpr.empty());

  const auto missingThen =
      primec::emitter::runEmitterExprControlIfTernaryFallbackStep([] { return std::string("cond"); }, {}, [] {
        return std::string("else");
      });
  CHECK_FALSE(missingThen.handled);
  CHECK(missingThen.emittedExpr.empty());

  const auto missingElse =
      primec::emitter::runEmitterExprControlIfTernaryFallbackStep([] { return std::string("cond"); }, [] {
        return std::string("then");
      }, {});
  CHECK_FALSE(missingElse.handled);
  CHECK(missingElse.emittedExpr.empty());
}

TEST_CASE("semantics validator expr capture split step tokenizes captures") {
  CHECK(primec::semantics::runSemanticsValidatorExprCaptureSplitStep("").empty());
  CHECK(primec::semantics::runSemanticsValidatorExprCaptureSplitStep(" \t \n").empty());

  const auto single =
      primec::semantics::runSemanticsValidatorExprCaptureSplitStep("value");
  REQUIRE(single.size() == 1);
  CHECK(single[0] == "value");

  const auto pair =
      primec::semantics::runSemanticsValidatorExprCaptureSplitStep("ref   item");
  REQUIRE(pair.size() == 2);
  CHECK(pair[0] == "ref");
  CHECK(pair[1] == "item");

  const auto mixed =
      primec::semantics::runSemanticsValidatorExprCaptureSplitStep("  =   ref\tname  ");
  REQUIRE(mixed.size() == 3);
  CHECK(mixed[0] == "=");
  CHECK(mixed[1] == "ref");
  CHECK(mixed[2] == "name");
}

TEST_CASE("semantics validator statement loop-count step resolves iteration bounds") {
  primec::Expr nameExpr;
  nameExpr.kind = primec::Expr::Kind::Name;
  nameExpr.name = "count";
  CHECK_FALSE(primec::semantics::runSemanticsValidatorStatementKnownIterationCountStep(nameExpr, false).has_value());

  primec::Expr boolTrue;
  boolTrue.kind = primec::Expr::Kind::BoolLiteral;
  boolTrue.boolValue = true;
  CHECK_FALSE(primec::semantics::runSemanticsValidatorStatementKnownIterationCountStep(boolTrue, false).has_value());
  const auto boolTrueKnown = primec::semantics::runSemanticsValidatorStatementKnownIterationCountStep(boolTrue, true);
  REQUIRE(boolTrueKnown.has_value());
  CHECK(*boolTrueKnown == 1u);

  primec::Expr boolFalse;
  boolFalse.kind = primec::Expr::Kind::BoolLiteral;
  boolFalse.boolValue = false;
  const auto boolFalseKnown = primec::semantics::runSemanticsValidatorStatementKnownIterationCountStep(boolFalse, true);
  REQUIRE(boolFalseKnown.has_value());
  CHECK(*boolFalseKnown == 0u);

  primec::Expr unsignedLiteral;
  unsignedLiteral.kind = primec::Expr::Kind::Literal;
  unsignedLiteral.isUnsigned = true;
  unsignedLiteral.literalValue = 7;
  const auto unsignedKnown =
      primec::semantics::runSemanticsValidatorStatementKnownIterationCountStep(unsignedLiteral, false);
  REQUIRE(unsignedKnown.has_value());
  CHECK(*unsignedKnown == 7u);

  primec::Expr negativeLiteral;
  negativeLiteral.kind = primec::Expr::Kind::Literal;
  negativeLiteral.isUnsigned = false;
  negativeLiteral.intWidth = 32;
  negativeLiteral.literalValue = static_cast<uint64_t>(static_cast<int32_t>(-1));
  CHECK(primec::semantics::runSemanticsValidatorStatementIsNegativeIntegerLiteralStep(negativeLiteral));
  const auto negativeKnown =
      primec::semantics::runSemanticsValidatorStatementKnownIterationCountStep(negativeLiteral, false);
  REQUIRE(negativeKnown.has_value());
  CHECK(*negativeKnown == 0u);

  primec::Expr positiveLiteral = negativeLiteral;
  positiveLiteral.literalValue = 1;
  CHECK_FALSE(primec::semantics::runSemanticsValidatorStatementIsNegativeIntegerLiteralStep(positiveLiteral));

  primec::Expr unsignedLiteralForNegative = unsignedLiteral;
  CHECK_FALSE(primec::semantics::runSemanticsValidatorStatementIsNegativeIntegerLiteralStep(unsignedLiteralForNegative));

  primec::Expr oneLiteral;
  oneLiteral.kind = primec::Expr::Kind::Literal;
  oneLiteral.isUnsigned = false;
  oneLiteral.intWidth = 32;
  oneLiteral.literalValue = 1;
  CHECK_FALSE(primec::semantics::runSemanticsValidatorStatementCanIterateMoreThanOnceStep(oneLiteral, false));

  primec::Expr twoLiteral = oneLiteral;
  twoLiteral.literalValue = 2;
  CHECK(primec::semantics::runSemanticsValidatorStatementCanIterateMoreThanOnceStep(twoLiteral, false));

  CHECK(primec::semantics::runSemanticsValidatorStatementCanIterateMoreThanOnceStep(nameExpr, false));
  CHECK_FALSE(primec::semantics::runSemanticsValidatorStatementCanIterateMoreThanOnceStep(boolTrue, true));
  CHECK_FALSE(primec::semantics::runSemanticsValidatorStatementCanIterateMoreThanOnceStep(boolFalse, true));
}

TEST_CASE("ir lowerer lower orchestrator stage order stays stable") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  };
  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src")) ? std::filesystem::path(".")
                                                             : std::filesystem::path("..");

  const std::filesystem::path lowererPath = repoRoot / "src" / "ir_lowerer" / "IrLowererLower.cpp";
  REQUIRE(std::filesystem::exists(lowererPath));
  const std::string lowererSource = readText(lowererPath);

  const std::vector<std::string> stageIncludes = {
      "#include \"IrLowererLowerSetupStage.h\"",
      "#include \"IrLowererLowerReturnEmitStage.h\"",
      "#include \"IrLowererLowerStatementsCallsStage.h\"",
  };

  size_t cursor = 0;
  for (const auto &stageInclude : stageIncludes) {
    const size_t pos = lowererSource.find(stageInclude);
    CAPTURE(stageInclude);
    CHECK(pos != std::string::npos);
    CHECK(pos >= cursor);
    if (pos != std::string::npos) {
      cursor = pos;
    }
  }
  CHECK(lowererSource.find("#include \"IrLowererLowerSetupStage.h\"") != std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerReturnEmitStage.h\"") != std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerStatementsCallsStage.h\"") != std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerOperators.h\"") == std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerStatementsExpr.h\"") == std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerStatementsBindings.h\"") == std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerStatementsLoops.h\"") == std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerReturnAndCalls.h\"") == std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerSetupEntryEffects.h\"") == std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerSetupImportsStructs.h\"") == std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerSetupLocals.h\"") == std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerSetupInference.h\"") == std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerStatementsCalls.h\"") == std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerExprEmitSetup.h\"") == std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerReturnCallsSetup.h\"") == std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerStatementsCallsStep.h\"") == std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerStatementsEntryExecutionStep.h\"") == std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerStatementsEntryStatementStep.h\"") == std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerStatementsFunctionTableStep.h\"") == std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerStatementsSourceMapStep.h\"") == std::string::npos);
  CHECK(lowererSource.find("runLowerSetupStage(") != std::string::npos);
  CHECK(lowererSource.find("runLowerReturnEmitStage(") != std::string::npos);
  CHECK(lowererSource.find("runLowerStatementsCallsStage(") != std::string::npos);

  const std::filesystem::path setupHeaderPath = repoRoot / "src" / "ir_lowerer" / "IrLowererLowerSetupStage.h";
  REQUIRE(std::filesystem::exists(setupHeaderPath));
  const std::string setupHeaderSource = readText(setupHeaderPath);
  CHECK(setupHeaderSource.find("struct LowerSetupStageInput {") != std::string::npos);
  CHECK(setupHeaderSource.find("struct LowerSetupStageState {") != std::string::npos);
  CHECK(setupHeaderSource.find("SetupLocalsOrchestration setupLocalsOrchestration{};") !=
        std::string::npos);
  CHECK(setupHeaderSource.find("LowerInferenceSetupBootstrapState inferenceSetupBootstrap{};") !=
        std::string::npos);
  CHECK(setupHeaderSource.find("bool runLowerSetupStage(") != std::string::npos);

  const std::filesystem::path returnEmitStageHeaderPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerReturnEmitStage.h";
  REQUIRE(std::filesystem::exists(returnEmitStageHeaderPath));
  const std::string returnEmitStageHeaderSource = readText(returnEmitStageHeaderPath);
  CHECK(returnEmitStageHeaderSource.find("struct LowerReturnEmitInlineContext {") != std::string::npos);
  CHECK(returnEmitStageHeaderSource.find("struct LowerReturnEmitStageInput {") != std::string::npos);
  CHECK(returnEmitStageHeaderSource.find("struct LowerReturnEmitStageState {") != std::string::npos);
  CHECK(returnEmitStageHeaderSource.find("bool hasMathImport = false;") != std::string::npos);
  CHECK(returnEmitStageHeaderSource.find("LowerReturnCallsEmitFileErrorWhyFn emitFileErrorWhy;") !=
        std::string::npos);
  CHECK(returnEmitStageHeaderSource.find("ResolveResultExprInfoWithLocalsFn resolveResultExprInfo;") !=
        std::string::npos);
  CHECK(returnEmitStageHeaderSource.find("GetSetupMathBuiltinNameFn getMathBuiltinName;") !=
        std::string::npos);
  CHECK(returnEmitStageHeaderSource.find("GetSetupMathConstantNameFn getMathConstantName;") !=
        std::string::npos);
  CHECK(returnEmitStageHeaderSource.find("EmitPrintArgForStatementFn emitPrintArg;") !=
        std::string::npos);
  CHECK(returnEmitStageHeaderSource.find("bool runLowerReturnEmitStage(") != std::string::npos);

  const std::filesystem::path returnEmitStageSourcePath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerReturnEmitStage.cpp";
  REQUIRE(std::filesystem::exists(returnEmitStageSourcePath));
  const std::string returnEmitStageSource = readText(returnEmitStageSourcePath);
  CHECK(returnEmitStageSource.find("#include \"IrLowererLowerReturnInfo.h\"") != std::string::npos);
  CHECK(returnEmitStageSource.find("#include \"IrLowererLowerInlineCalls.h\"") != std::string::npos);
  CHECK(returnEmitStageSource.find("#include \"IrLowererLowerEmitExpr.h\"") != std::string::npos);
  CHECK(returnEmitStageSource.find("#include \"IrLowererLowerOperators.h\"") != std::string::npos);
  CHECK(returnEmitStageSource.find("#include \"IrLowererLowerStatementsExpr.h\"") != std::string::npos);
  CHECK(returnEmitStageSource.find("#include \"IrLowererLowerStatementsBindings.h\"") != std::string::npos);
  CHECK(returnEmitStageSource.find("#include \"IrLowererLowerStatementsLoops.h\"") != std::string::npos);
  CHECK(returnEmitStageSource.find("#include \"IrLowererLowerInlineCallActiveContextStep.h\"") !=
        std::string::npos);
  CHECK(returnEmitStageSource.find("#include \"IrLowererLowerInlineCallCleanupStep.h\"") !=
        std::string::npos);
  CHECK(returnEmitStageSource.find("#include \"IrLowererLowerInlineCallContextSetupStep.h\"") !=
        std::string::npos);
  CHECK(returnEmitStageSource.find("#include \"IrLowererLowerInlineCallGpuLocalsStep.h\"") !=
        std::string::npos);
  CHECK(returnEmitStageSource.find("#include \"IrLowererLowerInlineCallReturnValueStep.h\"") !=
        std::string::npos);
  CHECK(returnEmitStageSource.find("#include \"IrLowererLowerInlineCallStatementStep.h\"") !=
        std::string::npos);
  CHECK(returnEmitStageSource.find("bool runLowerReturnEmitStage(") != std::string::npos);

  const std::filesystem::path returnInfoHeaderPath = repoRoot / "src" / "ir_lowerer" / "IrLowererLowerReturnInfo.h";
  REQUIRE(std::filesystem::exists(returnInfoHeaderPath));
  const std::string returnInfoHeaderSource = readText(returnInfoHeaderPath);
  CHECK(returnInfoHeaderSource.find("struct InlineContext {") == std::string::npos);
  CHECK(returnInfoHeaderSource.find("std::function<bool(const Expr &, const LocalMap &)> emitExpr;") ==
        std::string::npos);
  CHECK(returnInfoHeaderSource.find("analyzeDeclaredReturnTransforms(") == std::string::npos);
  CHECK(returnInfoHeaderSource.find("inferDefinitionReturnType(") == std::string::npos);

  const std::filesystem::path emitExprHeaderPath = repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExpr.h";
  REQUIRE(std::filesystem::exists(emitExprHeaderPath));
  const std::string emitExprHeaderSource = readText(emitExprHeaderPath);
  CHECK(emitExprHeaderSource.find("runLowerReturnCallsSetup(") != std::string::npos);
  CHECK(emitExprHeaderSource.find("runLowerExprEmitSetup(") != std::string::npos);
  CHECK(emitExprHeaderSource.find("LowerReturnCallsEmitFileErrorWhyFn emitFileErrorWhy;") ==
        std::string::npos);
  CHECK(emitExprHeaderSource.find("LowerExprEmitMovePassthroughCallFn emitMovePassthroughCall;") ==
        std::string::npos);
  CHECK(emitExprHeaderSource.find("runLowerExprEmitMovePassthroughStep(") != std::string::npos);
  CHECK(emitExprHeaderSource.find("runLowerExprEmitUploadReadbackPassthroughStep(") != std::string::npos);

  const std::filesystem::path statementsExprHeaderPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerStatementsExpr.h";
  REQUIRE(std::filesystem::exists(statementsExprHeaderPath));
  const std::string statementsExprHeaderSource = readText(statementsExprHeaderPath);
  CHECK(statementsExprHeaderSource.find("emitPrintArg = [&](const Expr &arg, const LocalMap &localsIn,") !=
        std::string::npos);
  CHECK(statementsExprHeaderSource.find("auto emitPrintArg = [&](const Expr &arg, const LocalMap &localsIn,") ==
        std::string::npos);

  const std::filesystem::path statementsCallsHeaderPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerStatementsCallsStage.h";
  REQUIRE(std::filesystem::exists(statementsCallsHeaderPath));
  const std::string statementsCallsHeaderSource = readText(statementsCallsHeaderPath);
  CHECK(statementsCallsHeaderSource.find("struct LowerStatementsCallsStageInput {") !=
        std::string::npos);
  CHECK(statementsCallsHeaderSource.find("InstructionSourceRange") != std::string::npos);
  CHECK(statementsCallsHeaderSource.find("std::function<void()> resetDefinitionLoweringState;") !=
        std::string::npos);
  CHECK(statementsCallsHeaderSource.find("bool runLowerStatementsCallsStage(") !=
        std::string::npos);

  const std::filesystem::path inlineCallsHeaderPath = repoRoot / "src" / "ir_lowerer" / "IrLowererLowerInlineCalls.h";
  REQUIRE(std::filesystem::exists(inlineCallsHeaderPath));
  const std::string inlineCallsHeaderSource = readText(inlineCallsHeaderPath);
  CHECK(inlineCallsHeaderSource.find("runLowerInlineCallActiveContextStep(") != std::string::npos);
  CHECK(inlineCallsHeaderSource.find("runLowerInlineCallCleanupStep(") != std::string::npos);
  CHECK(inlineCallsHeaderSource.find("runLowerInlineCallContextSetupStep(") != std::string::npos);
  CHECK(inlineCallsHeaderSource.find("runLowerInlineCallGpuLocalsStep(") != std::string::npos);
  CHECK(inlineCallsHeaderSource.find("runLowerInlineCallReturnValueStep(") != std::string::npos);
  CHECK(inlineCallsHeaderSource.find("runLowerInlineCallStatementStep(") != std::string::npos);
  CHECK(inlineCallsHeaderSource.find("const size_t startInstructionIndex = function.instructions.size();") ==
        std::string::npos);
  CHECK(inlineCallsHeaderSource.find(
            "appendInstructionSourceRange(function.name, stmt, startInstructionIndex, function.instructions.size());") ==
        std::string::npos);
  CHECK(inlineCallsHeaderSource.find("auto inheritGpuLocal =") == std::string::npos);
  CHECK(inlineCallsHeaderSource.find("inheritGpuLocal(kGpuGlobalIdXName);") == std::string::npos);
  CHECK(inlineCallsHeaderSource.find("size_t cleanupIndex = function.instructions.size();") == std::string::npos);
  CHECK(inlineCallsHeaderSource.find("function.instructions[jumpIndex].imm = static_cast<int32_t>(cleanupIndex);") ==
        std::string::npos);
  CHECK(inlineCallsHeaderSource.find("for (size_t jumpIndex : context.returnJumps)") == std::string::npos);
  CHECK(inlineCallsHeaderSource.find(
            "function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(context.returnLocal)});") ==
        std::string::npos);
  CHECK(inlineCallsHeaderSource.find("if (structDef && requireValue && context.returnsVoid)") == std::string::npos);
  CHECK(inlineCallsHeaderSource.find("context.returnsVoid = returnInfo.returnsVoid;") == std::string::npos);
  CHECK(inlineCallsHeaderSource.find("context.returnLocal = allocTempLocal();") == std::string::npos);
  CHECK(inlineCallsHeaderSource.find("IrOpcode zeroOp = IrOpcode::PushI32;") == std::string::npos);
  CHECK(inlineCallsHeaderSource.find(
            "function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(context.returnLocal)});") ==
        std::string::npos);
  CHECK(inlineCallsHeaderSource.find("if (!structDef) {") == std::string::npos);
  CHECK(inlineCallsHeaderSource.find("for (const auto &stmt : callee.statements)") == std::string::npos);
}

TEST_CASE("emitter emit setup source delegation stays stable") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  };
  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src")) ? std::filesystem::path(".")
                                                             : std::filesystem::path("..");

  const std::filesystem::path emitterSetupHeaderPath = repoRoot / "src" / "emitter" / "EmitterEmitSetup.h";
  REQUIRE(std::filesystem::exists(emitterSetupHeaderPath));
  const std::string emitterSetupHeaderSource = readText(emitterSetupHeaderPath);
  CHECK(emitterSetupHeaderSource.find("runEmitterEmitSetupMathImport(program)") != std::string::npos);
  CHECK(emitterSetupHeaderSource.find("runEmitterEmitSetupLifecycleHelperMatchStep(def.fullPath)") !=
        std::string::npos);

  const std::filesystem::path emitterEmitPath = repoRoot / "src" / "emitter" / "EmitterEmit.cpp";
  REQUIRE(std::filesystem::exists(emitterEmitPath));
  const std::string emitterEmitSource = readText(emitterEmitPath);
  CHECK(emitterEmitSource.find("#include \"EmitterEmitSetupMathImport.h\"") != std::string::npos);
  CHECK(emitterEmitSource.find("#include \"EmitterEmitSetupLifecycleHelperStep.h\"") != std::string::npos);
}

TEST_CASE("emitter builtin collection inference source stays canonical") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  };
  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src")) ? std::filesystem::path(".")
                                                             : std::filesystem::path("..");

  const std::filesystem::path emitterBuiltinCollectionsPath =
      repoRoot / "src" / "emitter" / "EmitterBuiltinCollectionInferenceHelpers.cpp";
  const std::filesystem::path astCallPathHelpersPath =
      repoRoot / "include" / "primec" / "AstCallPathHelpers.h";
  REQUIRE(std::filesystem::exists(emitterBuiltinCollectionsPath));
  REQUIRE(std::filesystem::exists(astCallPathHelpersPath));
  const std::string source = readText(emitterBuiltinCollectionsPath);
  const std::string astCallPathHelpersSource = readText(astCallPathHelpersPath);

  CHECK(astCallPathHelpersSource.find("bool isCanonicalCollectionHelperCall(") != std::string::npos);
  CHECK(astCallPathHelpersSource.find("std/collections/soa_vector/to_aos") == std::string::npos);
  CHECK(source.find("bool isCanonicalSoaToAosHelperCall(const Expr &expr)") == std::string::npos);
  CHECK(source.find("isCanonicalCollectionHelperCall(target, \"std/collections/soa_vector\", \"to_aos\", 1)") !=
        std::string::npos);
  CHECK(source.find("std/collections/soa_vector/to_aos") == std::string::npos);
  CHECK(source.find("isSimpleCallName(target, \"to_aos\")") == std::string::npos);
  CHECK(source.find("semantics::isExperimentalSoaVectorTypePath(normalized)") !=
        std::string::npos);
  CHECK(source.find("normalized == \"SoaVector\"") ==
        std::string::npos);
  CHECK(source.find("normalized == \"std/collections/experimental_soa_vector/SoaVector\"") ==
        std::string::npos);
  CHECK(source.find("normalized.rfind(\"SoaVector<\", 0) == 0") ==
        std::string::npos);
  CHECK(source.find("normalized.rfind(\"std/collections/experimental_soa_vector/SoaVector<\", 0) == 0") ==
        std::string::npos);
  CHECK(source.find("normalized.rfind(\"SoaVector__\", 0) == 0") ==
        std::string::npos);
  CHECK(source.find(
            "normalized.rfind(\"std/collections/experimental_soa_vector/SoaVector__\", 0) == 0") ==
        std::string::npos);
  CHECK(source.find("std/collections/soa_vector/get") == std::string::npos);
  CHECK(source.find("std/collections/soa_vector/ref") == std::string::npos);
  CHECK(source.find("std/collections/soa_vector/count") == std::string::npos);
  CHECK(source.find("std/collections/soa_vector/push") == std::string::npos);
  CHECK(source.find("std/collections/soa_vector/reserve") == std::string::npos);
}

TEST_CASE("soa pending diagnostics route through shared semantics helpers" * doctest::skip(true)) {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  };
  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src")) ? std::filesystem::path(".")
                                                             : std::filesystem::path("..");

  const std::filesystem::path builtinPathHelpersPath =
      repoRoot / "src" / "semantics" / "SemanticsBuiltinPathHelpers.cpp";
  const std::filesystem::path semanticsHelpersPath =
      repoRoot / "src" / "semantics" / "SemanticsHelpers.h";
  const std::filesystem::path buildInitializerInferencePath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorBuildInitializerInference.cpp";
  const std::filesystem::path buildInitializerInferenceCallsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorBuildInitializerInferenceCalls.cpp";
  const std::filesystem::path exprArgumentValidationCollectionsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprArgumentValidationCollections.cpp";
  const std::filesystem::path exprMapSoaBuiltinsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprMapSoaBuiltins.cpp";
  const std::filesystem::path exprPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExpr.cpp";
  const std::filesystem::path exprCountCapacityMapBuiltinsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprCountCapacityMapBuiltins.cpp";
  const std::filesystem::path exprMethodCompatibilitySetupPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprMethodCompatibilitySetup.cpp";
  const std::filesystem::path inferPreDispatchCallsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferPreDispatchCalls.cpp";
  const std::filesystem::path inferLateFallbackBuiltinsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferLateFallbackBuiltins.cpp";
  const std::filesystem::path inferMethodResolutionPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferMethodResolution.cpp";
  const std::filesystem::path exprMethodTargetResolutionPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprMethodTargetResolution.cpp";
  const std::filesystem::path exprCollectionAccessPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprCollectionAccess.cpp";
  const std::filesystem::path exprCollectionAccessValidationPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprCollectionAccessValidation.cpp";
  const std::filesystem::path inferDefinitionPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferDefinition.cpp";
  const std::filesystem::path inferCollectionReturnInferencePath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferCollectionReturnInference.cpp";
  const std::filesystem::path buildCallResolutionPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorBuildCallResolution.cpp";
  const std::filesystem::path exprResolvedCallArgumentsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprResolvedCallArguments.cpp";
  const std::filesystem::path exprMutationBorrowsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprMutationBorrows.cpp";
  const std::filesystem::path statementBindingsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorStatementBindings.cpp";
  const std::filesystem::path statementReturnsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorStatementReturns.cpp";
  REQUIRE(std::filesystem::exists(builtinPathHelpersPath));
  REQUIRE(std::filesystem::exists(semanticsHelpersPath));
  REQUIRE(std::filesystem::exists(buildInitializerInferencePath));
  REQUIRE(std::filesystem::exists(buildInitializerInferenceCallsPath));
  REQUIRE(std::filesystem::exists(exprArgumentValidationCollectionsPath));
  REQUIRE(std::filesystem::exists(exprMapSoaBuiltinsPath));
  REQUIRE(std::filesystem::exists(exprPath));
  REQUIRE(std::filesystem::exists(exprCountCapacityMapBuiltinsPath));
  REQUIRE(std::filesystem::exists(exprMethodCompatibilitySetupPath));
  REQUIRE(std::filesystem::exists(inferPreDispatchCallsPath));
  REQUIRE(std::filesystem::exists(inferLateFallbackBuiltinsPath));
  REQUIRE(std::filesystem::exists(inferMethodResolutionPath));
  REQUIRE(std::filesystem::exists(exprMethodTargetResolutionPath));
  REQUIRE(std::filesystem::exists(exprCollectionAccessPath));
  REQUIRE(std::filesystem::exists(exprCollectionAccessValidationPath));
  REQUIRE(std::filesystem::exists(inferDefinitionPath));
  REQUIRE(std::filesystem::exists(inferCollectionReturnInferencePath));
  REQUIRE(std::filesystem::exists(buildCallResolutionPath));
  REQUIRE(std::filesystem::exists(exprResolvedCallArgumentsPath));
  REQUIRE(std::filesystem::exists(exprMutationBorrowsPath));
  REQUIRE(std::filesystem::exists(statementBindingsPath));
  REQUIRE(std::filesystem::exists(statementReturnsPath));

  const std::string builtinPathHelpersSource = readText(builtinPathHelpersPath);
  const std::string semanticsHelpersSource = readText(semanticsHelpersPath);
  const std::string buildInitializerInferenceSource = readText(buildInitializerInferencePath);
  const std::string buildInitializerInferenceCallsSource = readText(buildInitializerInferenceCallsPath);
  const std::string exprArgumentValidationCollectionsSource =
      readText(exprArgumentValidationCollectionsPath);
  const std::string exprMapSoaBuiltinsSource = readText(exprMapSoaBuiltinsPath);
  const std::string exprSource = readText(exprPath);
  const std::string exprCountCapacityMapBuiltinsSource = readText(exprCountCapacityMapBuiltinsPath);
  const std::string exprMethodCompatibilitySetupSource =
      readText(exprMethodCompatibilitySetupPath);
  const std::string inferPreDispatchCallsSource = readText(inferPreDispatchCallsPath);
  const std::string inferLateFallbackBuiltinsSource =
      readText(inferLateFallbackBuiltinsPath);
  const std::string inferMethodResolutionSource = readText(inferMethodResolutionPath);
  const std::string exprMethodTargetResolutionSource = readText(exprMethodTargetResolutionPath);
  const std::string exprCollectionAccessSource = readText(exprCollectionAccessPath);
  const std::string exprCollectionAccessValidationSource = readText(exprCollectionAccessValidationPath);
  const std::string inferDefinitionSource = readText(inferDefinitionPath);
  const std::string inferCollectionReturnInferenceSource =
      readText(inferCollectionReturnInferencePath);
  const std::string buildCallResolutionSource = readText(buildCallResolutionPath);
  const std::string exprResolvedCallArgumentsSource = readText(exprResolvedCallArgumentsPath);
  const std::string exprMutationBorrowsSource = readText(exprMutationBorrowsPath);
  const std::string statementBindingsSource = readText(statementBindingsPath);
  const std::string statementReturnsSource = readText(statementReturnsPath);

  CHECK(builtinPathHelpersSource.find("std::string soaFieldViewHelperPath(std::string_view fieldName)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "bool splitSoaFieldViewHelperPath(std::string_view path, std::string *fieldNameOut)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "std::string canonicalizeLegacySoaToAosHelperPath(std::string_view path)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "std::string canonicalizeLegacySoaGetHelperPath(std::string_view path)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "if (canonicalPath == \"/to_aos_ref\")") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find("\"/soa_vector/to_aos_ref\"") ==
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "bool isCanonicalSoaToAosHelperPath(std::string_view path)") ==
        std::string::npos);
  CHECK(builtinPathHelpersSource.find("\"/soa_vector/field_view/\"") == std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "const size_t templateSuffix = resolvedPath.find(\"__t\");") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "std::string_view normalizedResolvedPath = resolvedPath;") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "const std::string canonicalSoaRefPath =") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "const std::string canonicalSoaGetPath =") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "canonicalizeLegacySoaRefHelperPath(normalizedResolvedPath)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "canonicalizeLegacySoaGetHelperPath(normalizedResolvedPath)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "if (normalizedResolvedPath == \"/soa_vector/count\")") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "bool isCanonicalSoaRefLikeHelperPath(std::string_view path)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "bool isExperimentalSoaBorrowedHelperPath(std::string_view path)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "bool isExperimentalSoaGetLikeHelperPath(std::string_view path)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "bool isExperimentalSoaGrowthHelperPath(std::string_view path)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "bool isExperimentalSoaFieldViewHelperPath(std::string_view path)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "bool isExperimentalSoaFieldViewReadHelperPath(std::string_view path)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "bool isExperimentalSoaFieldViewRefHelperPath(std::string_view path)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "bool isExperimentalSoaColumnSlotHelperPath(std::string_view path)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "bool isExperimentalSoaColumnFieldSchemaHelperPath(std::string_view path)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "bool isExperimentalSoaMethodRefLikeHelperPath(std::string_view path)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "bool isExperimentalSoaRefLikeHelperPath(std::string_view path)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "bool isExperimentalSoaVectorHelperFamilyPath(std::string_view path)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "bool isExperimentalSoaVectorTypePath(std::string_view path)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "bool isExperimentalSoaVectorSpecializedTypePath(std::string_view path)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "canonicalPath == \"/std/collections/experimental_soa_vector/soaVectorCountRef\"") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "canonicalPath == \"/std/collections/experimental_soa_vector/soaVectorGet\"") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "resolvedNoTemplate == "
            "\"/std/collections/experimental_soa_vector/soaVectorCountRef\"") ==
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "canonicalPath == \"/std/collections/experimental_soa_vector/soaVectorGetRef\"") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "canonicalPath == \"/std/collections/experimental_soa_vector/soaVectorRefRef\"") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "canonicalPath == \"/std/collections/experimental_soa_vector/soaVectorRef\"") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "canonicalPath == \"/std/collections/internal_soa_storage/soaColumnRef\"") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "helperName == \"push\" || helperName == \"reserve\" ||") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "helperName == \"soaVectorPush\" || helperName == \"soaVectorReserve\"") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "canonicalPath.rfind(kExperimentalSoaFieldViewPrefix, 0) == 0 ||") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "canonicalPath.rfind(kExperimentalSoaColumnFieldViewUnsafePrefix, 0) == 0") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "/std/collections/internal_soa_storage/soaFieldViewRead") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "/std/collections/internal_soa_storage/soaColumnSlotUnsafe") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "/std/collections/internal_soa_storage/soaColumnField") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "bool isExperimentalSoaVectorConversionFamilyPath(std::string_view path)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "isExperimentalSoaVectorConversionFamilyPath(canonicalPath)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "canonicalPath.starts_with(kExperimentalSoaVectorPrefix) ||") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "kExperimentalSoaVectorSpecializedPrefix") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "kExperimentalSoaVectorSpecializedPrefixNoSlash") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "kExperimentalSoaVectorSpecializedPrefixBare") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "path.starts_with(kExperimentalSoaVectorSpecializedPrefixBare)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "helperName == \"soaVectorToAos\" || helperName == \"soaVectorToAosRef\"") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "isCanonicalSoaRefLikeHelperPath(canonicalSoaRefPath)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "canonicalSoaRefPath == \"/std/collections/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "if (normalizedResolvedPath == \"/soa_vector/ref\")") ==
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "if (normalizedResolvedPath == \"/soa_vector/ref_ref\")") ==
        std::string::npos);
  CHECK(builtinPathHelpersSource.find("namespace {\n\nstd::string soaFieldViewPendingDiagnostic(") ==
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "std::string soaBorrowedViewPendingDiagnostic(std::string_view helperName)") ==
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "std::string soaDirectFieldViewPendingDiagnostic(std::string_view fieldName)") ==
        std::string::npos);
  CHECK(builtinPathHelpersSource.find("std::string soaDirectBorrowedViewPendingDiagnostic()") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "bool SemanticsValidator::hasVisibleDefinitionPathForCurrentImports(") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "bool isExperimentalSoaBorrowedHelperPath(std::string_view path);") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "bool isExperimentalSoaGetLikeHelperPath(std::string_view path);") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "bool isExperimentalSoaGrowthHelperPath(std::string_view path);") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "bool isExperimentalSoaFieldViewHelperPath(std::string_view path);") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "bool isExperimentalSoaFieldViewReadHelperPath(std::string_view path);") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "bool isExperimentalSoaFieldViewRefHelperPath(std::string_view path);") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "bool isExperimentalSoaColumnSlotHelperPath(std::string_view path);") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "bool isExperimentalSoaColumnFieldSchemaHelperPath(std::string_view path);") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "bool isExperimentalSoaMethodRefLikeHelperPath(std::string_view path);") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "bool isExperimentalSoaRefLikeHelperPath(std::string_view path);") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "bool isExperimentalSoaVectorHelperFamilyPath(std::string_view path);") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "bool isExperimentalSoaVectorTypePath(std::string_view path);") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "bool isExperimentalSoaVectorSpecializedTypePath(std::string_view path);") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "bool isExperimentalSoaVectorConversionFamilyPath(std::string_view path);") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "bool SemanticsValidator::hasVisibleSoaHelperTargetForCurrentImports(") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "std::string SemanticsValidator::preferredSoaHelperTargetForCurrentImports(") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "std::string preferredSamePathSoaHelperTarget(std::string_view helperName)") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "if (helperName == \"to_aos\" || helperName == \"to_aos_ref\")") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "return \"/\" + std::string(helperName);") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "std::string SemanticsValidator::preferredVisibleDefinitionPathForCurrentImports(") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "normalizedName == \"vector/vector\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "normalizedPrefix == \"vector\" && normalizedName == \"vector\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "resolvedCollectionPath == \"/std/collections/vector/vector\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "namespacedCollection == \"vector\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedCallPath, \"count\")") !=
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedCallPath, \"count_ref\")") !=
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "std::string preferredResolvedInitializer = "
            "hasGraphPreferredMethodResolvedInitializer") !=
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "                                             : hasGraphPreferredResolvedInitializer\n"
            "                                                 ? graphPreferredResolvedInitializer\n"
            "                                                 : preferredCollectionHelperResolvedPath(initializer);") !=
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "  std::string preferredResolvedInitializer = hasGraphPreferredMethodResolvedInitializer\n"
            "                                                 ? graphPreferredMethodResolvedInitializer\n"
            "                                             : hasGraphPreferredResolvedInitializer\n"
            "                                                 ? graphPreferredResolvedInitializer\n"
            "                                                 : preferredCollectionHelperResolvedPath(initializer);") !=
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "hasGraphPreferredMethodResolvedInitializer\n"
            "                                                 ? graphPreferredResolvedInitializer") ==
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "const std::string concretePreferredResolvedInitializer =\n"
            "        resolveExprConcreteCallPath(\n"
            "            params, locals, initializer, preferredResolvedInitializer);") !=
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "if (!concretePreferredResolvedInitializer.empty()) {\n"
            "      preferredResolvedInitializer = concretePreferredResolvedInitializer;\n"
            "    }") !=
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "resolveMethodTarget(params,\n"
            "                              locals,\n"
            "                              initializer.namespacePrefix,\n"
            "                              initializer.args.front(),\n"
            "                              initializer.name,\n"
            "                              resolvedMethodInitializer,\n"
            "                              methodBuiltin)") !=
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "const std::string concreteResolvedMethodInitializer =\n"
            "          resolveExprConcreteCallPath(\n"
              "              params, locals, initializer, resolvedMethodInitializer);") !=
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "if (!concreteResolvedMethodInitializer.empty()) {\n"
            "        resolvedMethodInitializer = concreteResolvedMethodInitializer;\n"
            "      }") !=
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "    if (hasGraphPreferredMethodResolvedInitializer) {\n"
            "      if (inferResolvedDirectCallBindingType(graphPreferredMethodResolvedInitializer, bindingOut)) {\n"
            "        return true;\n"
            "      }\n"
            "      if (inferDeclaredDirectCallBinding(graphPreferredMethodResolvedInitializer)) {\n"
            "        return true;\n"
            "      }\n"
            "    }\n"
            "    std::string resolvedMethodInitializer;\n"
            "    bool methodBuiltin = false;\n"
            "    if (resolveMethodTarget(params,\n"
            "                            locals,\n"
            "                            initializer.namespacePrefix,\n"
            "                            initializer.args.front(),\n"
            "                            initializer.name,\n"
            "                            resolvedMethodInitializer,\n"
            "                            methodBuiltin) &&\n"
            "        !resolvedMethodInitializer.empty()) {\n") !=
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "bindingExpr == nullptr\n"
            "                                                              ? "
            "preferredCollectionHelperResolvedPath(initializer)\n"
            "                                                              : std::string{}") ==
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "  std::string preferredResolvedInferencePath;\n"
            "  if (initializerExprForInference != nullptr) {\n"
            "    preferredResolvedInferencePath =\n"
            "        preferredCollectionHelperResolvedPath(*initializerExprForInference);\n"
            "    if (preferredResolvedInferencePath.empty()) {\n"
            "      preferredResolvedInferencePath = resolveCalleePath(*initializerExprForInference);\n"
            "    }\n"
            "    if (!preferredResolvedInferencePath.empty()) {\n"
            "      const std::string concreteResolvedInferencePath =\n"
            "          resolveExprConcreteCallPath(\n"
            "              params, locals, *initializerExprForInference, preferredResolvedInferencePath);\n"
            "      if (!concreteResolvedInferencePath.empty()) {\n"
            "        preferredResolvedInferencePath = concreteResolvedInferencePath;\n"
            "      }\n"
            "    }\n"
            "  }\n") !=
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "    std::string resolvedInferencePath = preferredResolvedInferencePath;\n"
            "    if (resolvedInferencePath.empty()) {\n"
            "      resolvedInferencePath = resolveCalleePath(*initializerExprForInference);\n"
            "    }\n") !=
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "      if (!resolvedInferencePath.empty() &&\n"
            "          inferResolvedDirectCallBindingType(resolvedInferencePath, resolvedCallBinding)) {\n"
            "        bindingOut = std::move(resolvedCallBinding);\n"
            "      }\n") !=
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "      const std::string fallbackPreferredResolvedInitializerPath =\n"
            "          preferredCollectionHelperResolvedPath(initializer);\n"
            "      const std::string &preferredResolvedInitializerPath =\n"
            "          !preferredResolvedInferencePath.empty()\n"
            "              ? preferredResolvedInferencePath\n"
            "          : !fallbackPreferredResolvedInitializerPath.empty()\n"
            "              ? fallbackPreferredResolvedInitializerPath\n"
            "              : resolveCalleePath(initializer);\n") !=
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "inferResolvedDirectCallBindingType(resolveCalleePath(*initializerExprForInference), bindingOut)") ==
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "inferResolvedDirectCallBindingType(resolveCalleePath(*initializerExprForInference), resolvedCallBinding)") ==
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "      const std::string &preferredResolvedInitializerPath =\n"
            "          !preferredResolvedInferencePath.empty()\n"
            "              ? preferredResolvedInferencePath\n"
            "              : resolveCalleePath(initializer);\n") ==
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "inferDeclaredDirectCallBinding(resolveCalleePath(*initializerExprForInference))") ==
        std::string::npos);
  CHECK(buildCallResolutionSource.find("auto vectorConstructorHelperPath = [&]() -> std::string {") ==
        std::string::npos);
  CHECK(buildCallResolutionSource.find("if (resolvedPath == \"/std/collections/vector/vector\") {") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "std::optional<std::string> SemanticsValidator::builtinSoaAccessHelperName(") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "    std::string resolvedCandidate = preferredCollectionHelperResolvedPath(candidate);\n"
            "    if (resolvedCandidate.empty()) {\n"
            "      resolvedCandidate = resolveCalleePath(candidate);\n"
            "    }\n"
            "    resolvedCandidate = canonicalizeResolvedPath(std::move(resolvedCandidate));\n"
            "    if (!resolvedCandidate.empty()) {\n"
            "      const std::string concreteResolvedCandidate =\n"
            "          resolveExprConcreteCallPath(params, locals, candidate, resolvedCandidate);\n"
            "      if (!concreteResolvedCandidate.empty()) {\n"
            "        resolvedCandidate = canonicalizeResolvedPath(concreteResolvedCandidate);\n"
            "      }\n"
            "    }\n") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "const std::string resolvedCandidate = canonicalizeResolvedPath(resolveCalleePath(candidate));") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "  std::string resolved = preferredCollectionHelperResolvedPath(candidate);\n"
            "  if (resolved.empty()) {\n"
            "    resolved = resolveCalleePath(candidate);\n"
            "  }\n"
            "  if (!resolved.empty()) {\n"
            "    const std::string concreteResolved =\n"
            "        resolveExprConcreteCallPath(params, locals, candidate, resolved);\n"
            "    if (!concreteResolved.empty()) {\n"
            "      resolved = concreteResolved;\n"
            "    }\n"
            "  }\n"
            "  const std::string resolvedCanonical =\n"
            "      canonicalizeLegacySoaGetHelperPath(resolved);\n") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "const std::string resolved = resolveCalleePath(candidate);") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "const auto canonicalizeSoaResolvedPath = [](std::string_view path) -> std::string {") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "canonicalizeLegacySoaGetHelperPath(resolved)") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "const std::string resolvedCanonicalRaw =") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "resolvedCanonicalRaw == \"/soa_vector/get\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "resolvedCanonicalRaw == \"/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "const std::string resolvedCanonical =") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find("resolved == \"/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find("resolved == \"/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find("resolved == \"/soa_vector/get\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find("resolved == \"/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "helper == \"to_aos\" ? \"/to_aos\" : \"/soa_vector/\" + helper") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "resolvedCanonical == \"/std/collections/soa_vector/get\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "resolvedCanonical == \"/std/collections/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedCanonical, \"get\")") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedCanonical, \"get_ref\")") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "resolvedCanonical == \"/std/collections/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "resolvedCanonical == \"/std/collections/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "const bool resolvedCanonicalIsRefLike =") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "isCanonicalSoaRefLikeHelperPath(resolvedCanonical)") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedCanonical, \"ref\")") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedCanonical, \"ref_ref\")") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "bool SemanticsValidator::isBuiltinSoaRefExpr(") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "bool SemanticsValidator::usesVisibleSamePathSoaHelper(") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "std::string SemanticsValidator::soaUnavailableMethodDiagnosticForCurrentImports(") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "bool SemanticsValidator::hasVisibleSoaRefHelper() const {") ==
        std::string::npos);
  CHECK((buildInitializerInferenceSource.find(
            "!usesSamePathSoaHelperTargetForCurrentImports(\"ref\")") !=
        std::string::npos ||
        buildInitializerInferenceSource.find(
            "!hasVisibleSoaHelperTargetForCurrentImports(\"ref\")") !=
            std::string::npos ||
        buildInitializerInferenceSource.find(
            "hasVisibleSoaHelperTargetForCurrentImports(*soaAccessHelper)") !=
            std::string::npos));
  CHECK(buildInitializerInferenceSource.find(
            "hasVisibleDefinitionPathForCurrentImports(\"/soa_vector/ref\")") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "std::optional<std::string> SemanticsValidator::builtinSoaPendingExprDiagnostic(") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "bool SemanticsValidator::reportBuiltinSoaPendingExprDiagnostic(") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find("splitSoaFieldViewHelperPath(resolved, fieldNameOut)") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "auto withPreservedError = [&](const std::function<bool()> &fn)") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "withPreservedError([&]() {\n          return const_cast<SemanticsValidator *>(this)->inferBindingTypeFromInitializer(") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "return withPreservedError([&]() {\n             return const_cast<SemanticsValidator *>(this)->inferQueryExprTypeText(") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "soaDirectFieldViewPendingDiagnostic(fieldName)") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "soaDirectBorrowedViewPendingDiagnostic()") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "return soaFieldViewHelperPath(fieldName);") !=
        std::string::npos);
  CHECK((buildInitializerInferenceSource.find(
            "return std::string(\"/std/collections/soa_vector/ref\");") !=
         std::string::npos ||
         buildInitializerInferenceSource.find(
             "return std::string(\"/std/collections/soa_vector/\") + *soaAccessHelper;") !=
             std::string::npos));
  CHECK(buildInitializerInferenceSource.find("soaFieldViewOrUnknownMethodDiagnostic(") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "return soaUnavailableMethodDiagnostic(resolvedPath,") ==
        std::string::npos);
  CHECK(statementBindingsSource.find("isBuiltinSoaRefExpr(") ==
        std::string::npos);
  CHECK(statementBindingsSource.find("builtinSoaAccessHelperName(") ==
        std::string::npos);
  CHECK(inferDefinitionSource.find("isBuiltinSoaRefExpr(") ==
        std::string::npos);
  CHECK(inferDefinitionSource.find("builtinSoaAccessHelperName(") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find("soaFieldViewPendingDiagnostic(fieldName)") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find("soaBorrowedViewPendingDiagnostic()") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find("\"/soa_vector/field_view/\"") == std::string::npos);
  CHECK(exprArgumentValidationCollectionsSource.find(
            "bool SemanticsValidator::resolveDirectSoaVectorOrExperimentalBorrowedReceiver(") !=
        std::string::npos);
  CHECK(exprArgumentValidationCollectionsSource.find(
            "isExperimentalSoaVectorSpecializedTypePath(structPath)") !=
        std::string::npos);
  CHECK(exprArgumentValidationCollectionsSource.find(
            "isExperimentalSoaVectorTypePath(normalizedBase) && !argText.empty()") !=
        std::string::npos);
  CHECK(exprArgumentValidationCollectionsSource.find(
            "normalizedBase == \"SoaVector\" ||") ==
        std::string::npos);
  CHECK(exprArgumentValidationCollectionsSource.find(
            "normalizedBase == \"std/collections/experimental_soa_vector/SoaVector\"") ==
        std::string::npos);
  CHECK(exprArgumentValidationCollectionsSource.find(
            "isExperimentalSoaVectorSpecializedTypePath(normalizedResolvedPath)") !=
        std::string::npos);
  CHECK(exprArgumentValidationCollectionsSource.find(
            "structPath.rfind(\"/std/collections/experimental_soa_vector/SoaVector__\", 0) != 0") ==
        std::string::npos);
  CHECK(exprArgumentValidationCollectionsSource.find(
            "normalizedResolvedPath.rfind(\"std/collections/experimental_soa_vector/SoaVector__\", 0) != 0") ==
        std::string::npos);
  CHECK(exprArgumentValidationCollectionsSource.find(
            "auto withPreservedError = [&](const std::function<bool()> &fn)") !=
        std::string::npos);
  CHECK(exprArgumentValidationCollectionsSource.find(
            "return withPreservedError([&]() {\n             return inferQueryExprTypeText(valueExpr, params, locals, inferredTypeText);") !=
        std::string::npos);
  CHECK(exprArgumentValidationCollectionsSource.find(
            "return withPreservedError([&]() {\n           return inferQueryExprTypeText(target, params, locals, inferredTypeText);") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find("std::string soaFieldViewPendingDiagnostic(") ==
        std::string::npos);
  CHECK(semanticsHelpersSource.find("std::string soaBorrowedViewPendingDiagnostic()") ==
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "std::string soaDirectFieldViewPendingDiagnostic(std::string_view fieldName);") ==
        std::string::npos);
  CHECK(semanticsHelpersSource.find("std::string soaDirectBorrowedViewPendingDiagnostic();") ==
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "std::string soaDirectPendingUnavailableMethodDiagnostic(") ==
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "std::string soaUnavailableMethodDiagnostic(std::string_view resolvedPath);") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "std::string canonicalizeLegacySoaToAosHelperPath(std::string_view path);") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "std::string canonicalizeLegacySoaRefHelperPath(std::string_view path);") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "std::string canonicalizeLegacySoaGetHelperPath(std::string_view path);") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "bool isLegacyOrCanonicalSoaHelperPath(std::string_view path, std::string_view helperName);") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "bool isCanonicalSoaToAosHelperPath(std::string_view path);") ==
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "bool isCanonicalSoaRefLikeHelperPath(std::string_view path);") !=
        std::string::npos);
  CHECK(exprSource.find(
            "const std::string resolvedSoaToAosCanonical =") !=
        std::string::npos);
  CHECK(exprSource.find(
            "const bool resolvedUsesCanonicalSoaNamespace =") !=
        std::string::npos);
  CHECK(exprSource.find(
            "canonicalizeLegacySoaToAosHelperPath(resolved)") !=
        std::string::npos);
  CHECK(exprSource.find(
            "resolved.rfind(\"/std/collections/soa_vector/\", 0) == 0") !=
        std::string::npos);
  CHECK(exprSource.find(
            "isLegacyOrCanonicalSoaHelperPath(\n"
            "            resolvedSoaToAosCanonical, \"to_aos\")") !=
        std::string::npos);
  CHECK(exprSource.find(
            "isCanonicalSoaToAosHelperPath(resolvedSoaToAosCanonical)") ==
        std::string::npos);
  CHECK(exprSource.find(
            "isLegacyOrCanonicalSoaHelperPath(\n"
            "            resolvedSoaToAosCanonical, \"to_aos_ref\")") !=
        std::string::npos);
  CHECK(exprSource.find(
            "resolvedSoaToAosCanonical == \"/std/collections/soa_vector/to_aos_ref\"") ==
        std::string::npos);
  CHECK(exprSource.find(
            "resolved.rfind(\"/std/collections/soa_vector/to_aos\", 0)") ==
        std::string::npos);
  CHECK(exprSource.find(
            "resolved.rfind(\"/std/collections/soa_vector/to_aos_ref\", 0)") ==
        std::string::npos);
  CHECK(exprSource.find(
            "base == \"/soa_vector/push\"") ==
        std::string::npos);
  CHECK(exprSource.find(
            "base == \"/soa_vector/reserve\"") ==
        std::string::npos);
  CHECK(exprSource.find(
            "base == \"/std/collections/soa_vector/push\"") ==
        std::string::npos);
  CHECK(exprSource.find(
            "base == \"/std/collections/soa_vector/reserve\"") ==
        std::string::npos);
  CHECK(exprSource.find(
            "isLegacyOrCanonicalSoaHelperPath(base, \"push\")") !=
        std::string::npos);
  CHECK(exprSource.find(
            "isLegacyOrCanonicalSoaHelperPath(base, \"reserve\")") !=
        std::string::npos);
  CHECK(exprSource.find(
            "isExperimentalSoaGrowthHelperPath(base)") !=
        std::string::npos);
  CHECK(exprSource.find(
            "isExperimentalSoaFieldViewHelperPath(resolvedPath)") !=
        std::string::npos);
  CHECK(exprSource.find(
            "isExperimentalSoaColumnFieldSchemaHelperPath(") !=
        std::string::npos);
  CHECK(exprSource.find(
            "const bool isExperimentalSoaPath =") ==
        std::string::npos);
  CHECK(exprSource.find(
            "return hasSuffix(\"/push\") || hasSuffix(\"/reserve\") ||") ==
        std::string::npos);
  CHECK(exprSource.find(
            "resolvedPath.rfind(\n"
            "              \"/std/collections/experimental_soa_vector/soaVectorFieldView\",") ==
        std::string::npos);
  CHECK(exprSource.find(
            "resolvedPath.rfind(\n"
            "              \"/std/collections/internal_soa_storage/soaColumnFieldViewUnsafe\",") ==
        std::string::npos);
  CHECK(exprSource.find(
            "definitionPath.rfind(\n"
            "                 \"/std/collections/internal_soa_storage/soaColumnField\", 0) == 0") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find("unknown method: /std/collections/soa_vector/field_view/") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find("soa_vector field view requires soa_vector target") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find("splitSoaFieldViewHelperPath(resolved)") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "soaDirectPendingUnavailableMethodDiagnostic(resolved)") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "soaUnavailableMethodDiagnostic(resolved)") !=
        std::string::npos);
  CHECK((exprMapSoaBuiltinsSource.find(
            "hasVisibleDefinitionPathForCurrentImports(\n"
            "              \"/soa_vector/\" + helperName)") !=
        std::string::npos ||
        exprMapSoaBuiltinsSource.find(
            "hasVisibleSoaHelperTargetForCurrentImports(helperName)") !=
            std::string::npos));
  CHECK(exprMapSoaBuiltinsSource.find("auto hasVisibleSamePathSoaAccessHelper =") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "hasDeclaredDefinitionPath(samePath) || hasImportedDefinitionPath(samePath)") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "builtinSoaAccessHelperName(expr, params, locals)") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "const auto isExplicitOldSurfaceSoaAccessCall =") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "const auto isExplicitOldSurfaceSoaConversionCall =") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "const auto canonicalizeLegacySoaHelperPath = [](std::string_view path) -> std::string {") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "canonicalizeLegacySoaGetHelperPath(resolvedNoTemplate)") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "resolvedSoaCanonicalRaw == \"/soa_vector/get\"") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "resolvedSoaCanonicalRaw == \"/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "const std::string resolvedSoaCanonical =") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "const std::string resolvedSoaToAosCanonical =") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "const bool isBorrowedToAosHelper =") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "canonicalizeLegacySoaToAosHelperPath(resolved)") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "isLegacyOrCanonicalSoaHelperPath(\n"
            "          resolvedSoaToAosCanonical, \"to_aos\")") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "isCanonicalSoaToAosHelperPath(resolvedSoaToAosCanonical)") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "isLegacyOrCanonicalSoaHelperPath(\n"
            "          resolvedSoaToAosCanonical, \"to_aos_ref\")") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "resolvedSoaToAosCanonical == \"/std/collections/soa_vector/to_aos_ref\"") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "resolvedNoTemplate == \"/soa_vector/get\"") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "resolvedNoTemplate == \"/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "resolvedNoTemplate == \"/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "resolvedNoTemplate == \"/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "resolved.rfind(\"/std/collections/soa_vector/to_aos\", 0)") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "resolved.rfind(\"/soa_vector/to_aos_ref\", 0)") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "resolved == \"/to_aos_ref\"") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "argument type mismatch for /std/collections/soa_vector/to_aos_ref parameter values") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "!hasVisibleDefinitionPathForCurrentImports(\"/\" + helperName)") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "soaUnavailableMethodDiagnostic(\"/\" + helperName)") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "isCanonicalSoaRefLikeHelperPath(resolvedSoaCanonical)") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "isExperimentalSoaBorrowedHelperPath(resolvedNoTemplate)") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "resolvedSoaCanonical == \"/std/collections/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, \"ref\")") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, \"ref_ref\")") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "resolvedSoaCanonical == \"/std/collections/soa_vector/get\"") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "resolvedSoaCanonical == \"/std/collections/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, \"get\")") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical,\n"
            "                                             \"get_ref\")") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "!hasVisibleDefinitionPathForCurrentImports(\"/soa_vector/\" + helperName)") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "soaUnavailableMethodDiagnostic(\"/soa_vector/\" + helperName)") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "resolvedNoTemplate == "
            "\"/std/collections/experimental_soa_vector/soaVectorGetRef\"") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "resolvedNoTemplate == "
            "\"/std/collections/experimental_soa_vector/soaVectorRefRef\"") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find("(helperName == \"get_ref\" &&") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "!resolvedMethod && (expr.name == \"get\" || expr.name == \"ref\") && resolvedMissing") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find("soaFieldViewPendingDiagnostic(soaFieldViewName)") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find("soaFieldViewPendingDiagnostic(expr.name)") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find("\"/soa_vector/field_view/\"") == std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "this->resolveDirectSoaVectorOrExperimentalBorrowedReceiver(\n"
            "        target, params, locals, context.resolveSoaVectorTarget, elemTypeOut)") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find("auto resolveDirectSoaReceiver =") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "this->resolveSoaVectorOrExperimentalBorrowedReceiver(\n"
            "                  expr.args.front(),\n"
            "                  params,\n"
            "                  locals,\n"
            "                  context.resolveSoaVectorTarget,\n"
            "                  elemType)") != std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find("resolveSoaVectorOrExperimentalBorrowedTarget") ==
        std::string::npos);
  CHECK(exprCountCapacityMapBuiltinsSource.find(
            "(*dispatchResolvers).resolveSoaVectorTarget(expr.args.front(),") !=
        std::string::npos);
  CHECK(exprCountCapacityMapBuiltinsSource.find(
            "auto canonicalizeSoaCountHelperPath = [](std::string canonicalPath)") !=
        std::string::npos);
  CHECK(exprCountCapacityMapBuiltinsSource.find(
            "auto isCanonicalSoaCountHelperPath = [](const std::string &candidate)") !=
        std::string::npos);
  CHECK(exprCountCapacityMapBuiltinsSource.find(
            "const std::string resolvedSoaCountCanonical =") !=
        std::string::npos);
  CHECK(exprCountCapacityMapBuiltinsSource.find(
            "const std::string logicalSoaCountCanonical =") !=
        std::string::npos);
  CHECK(exprCountCapacityMapBuiltinsSource.find(
            "const auto isExplicitOldSurfaceSoaCountCall = [&]()") !=
        std::string::npos);
  CHECK(exprCountCapacityMapBuiltinsSource.find(
            "resolved.rfind(\"/std/collections/soa_vector/count\", 0) == 0") ==
        std::string::npos);
  CHECK(exprCountCapacityMapBuiltinsSource.find(
            "logicalResolvedMethod == \"/std/collections/soa_vector/count\"") ==
        std::string::npos);
  CHECK(exprCountCapacityMapBuiltinsSource.find(
            "logicalResolvedMethod == \"/soa_vector/count\"") ==
        std::string::npos);
  CHECK(exprCountCapacityMapBuiltinsSource.find(
            "isCanonicalSoaCountHelperPath(resolvedSoaCountCanonical)") !=
        std::string::npos);
  CHECK(exprCountCapacityMapBuiltinsSource.find(
            "isLegacyOrCanonicalSoaHelperPath(\n"
             "                             logicalSoaCountCanonical,\n"
             "                             \"count\")") !=
        std::string::npos);
  CHECK(exprCountCapacityMapBuiltinsSource.find(
            "isLegacyOrCanonicalSoaHelperPath(\n"
             "                             logicalSoaCountCanonical,\n"
             "                             \"count_ref\")") !=
        std::string::npos);
  CHECK(exprCountCapacityMapBuiltinsSource.find(
            "!hasVisibleDefinitionPathForCurrentImports(\"/soa_vector/count\")") !=
        std::string::npos);
  CHECK(exprCountCapacityMapBuiltinsSource.find(
            "!hasVisibleDefinitionPathForCurrentImports(\"/soa_vector/\" +\n"
            "                                                    soaCountHelperName)") !=
        std::string::npos);
  CHECK(exprCountCapacityMapBuiltinsSource.find(
            "soaUnavailableMethodDiagnostic(\"/soa_vector/count\")") !=
        std::string::npos);
  CHECK(exprCountCapacityMapBuiltinsSource.find(
            "soaUnavailableMethodDiagnostic(\"/soa_vector/\" +\n"
            "                                           soaCountHelperName)") !=
        std::string::npos);
  CHECK((exprCountCapacityMapBuiltinsSource.find(
            "preferredSoaHelperTargetForCurrentImports(\"count\") ==\n"
            "                \"/soa_vector/count\"") !=
        std::string::npos ||
        exprCountCapacityMapBuiltinsSource.find(
            "hasVisibleSoaHelperTargetForCurrentImports(\"count\")") !=
            std::string::npos));
  CHECK(exprCountCapacityMapBuiltinsSource.find(
            "hasVisibleSoaHelperTargetForCurrentImports(soaCountHelperName)") !=
        std::string::npos);
  CHECK(exprCountCapacityMapBuiltinsSource.find("auto hasVisibleSamePathSoaCountHelper =") ==
        std::string::npos);
  CHECK(exprCountCapacityMapBuiltinsSource.find(
            "hasDeclaredDefinitionPath(\"/soa_vector/count\") ||\n"
            "           hasImportedDefinitionPath(\"/soa_vector/count\")") ==
        std::string::npos);
  CHECK(exprCountCapacityMapBuiltinsSource.find(
            "resolveSoaVectorOrExperimentalBorrowedTarget") ==
        std::string::npos);
  CHECK(exprCountCapacityMapBuiltinsSource.find(
            "this->resolveExperimentalBorrowedSoaTypeText(") ==
        std::string::npos);
  CHECK(inferMethodResolutionSource.find("resolvedOut = soaFieldViewHelperPath(normalizedMethodName);") !=
        std::string::npos);
  CHECK(inferMethodResolutionSource.find("\"/soa_vector/field_view/\"") == std::string::npos);
  CHECK(inferMethodResolutionSource.find(
            "this->resolveSoaVectorOrExperimentalBorrowedReceiver(\n"
            "            soaReceiver, params, locals, resolveDirectReceiver, elemType)") !=
        std::string::npos);
  CHECK(inferMethodResolutionSource.find("this->hasVisibleSoaRefHelper()") ==
        std::string::npos);
  CHECK(inferMethodResolutionSource.find(
            "preferredSoaHelperTargetForCurrentImports(") !=
        std::string::npos);
  CHECK(inferMethodResolutionSource.find("auto preferredSoaCountMethodTarget =") ==
        std::string::npos);
  CHECK(inferMethodResolutionSource.find(
            "hasVisibleDefinitionPathForCurrentImports(\"/soa_vector/count\")") ==
        std::string::npos);
  CHECK(inferMethodResolutionSource.find(
            "hasVisibleDefinitionPathForCurrentImports(\"/soa_vector/get\")") ==
        std::string::npos);
  CHECK(inferMethodResolutionSource.find(
            "hasVisibleDefinitionPathForCurrentImports(\"/soa_vector/ref\")") ==
        std::string::npos);
  CHECK(inferMethodResolutionSource.find(
            "normalizedMethodName == \"get\" || normalizedMethodName == \"get_ref\"") !=
        std::string::npos);
  CHECK(inferMethodResolutionSource.find("preferredBorrowedSoaAccessHelperTarget(") !=
        std::string::npos);
  CHECK(inferMethodResolutionSource.find(
            "/std/collections/experimental_soa_vector/soaVectorGetRef") !=
        std::string::npos);
  CHECK(inferMethodResolutionSource.find("hasVisibleSoaHelperTargetForCurrentImports(") !=
        std::string::npos);
  CHECK(inferMethodResolutionSource.find("auto extractBorrowedBinding =") ==
        std::string::npos);
  CHECK(inferMethodResolutionSource.find(
            "auto resolveSoaVectorOrExperimentalBorrowedReceiver =") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find("resolvedOut = soaFieldViewHelperPath(normalizedMethodName);") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find("\"/soa_vector/field_view/\"") == std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "this->resolveSoaVectorOrExperimentalBorrowedReceiver(\n"
            "          receiver, params, locals, resolveDirectReceiver, elemType)") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find("this->hasVisibleSoaRefHelper()") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "preferredSoaHelperTargetForCollectionType(") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "if (collectionTypePath == \"/soa_vector\") {\n"
            "    return samePath;\n"
            "  }") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find("auto preferredSoaCountMethodTarget =") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "hasVisibleDefinitionPathForCurrentImports(\"/soa_vector/count\")") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "hasVisibleDefinitionPathForCurrentImports(\"/soa_vector/get\")") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "hasVisibleDefinitionPathForCurrentImports(\"/soa_vector/ref\")") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "normalizedMethodName == \"get\" || normalizedMethodName == \"get_ref\"") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "const std::string resolvedSoaGetCanonical =") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "const std::string resolvedSoaCountCanonical =") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "auto isCanonicalSoaHelperPath = [](const std::string &candidate,") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "canonicalizeLegacySoaGetHelperPath(resolvedOut)") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "resolvedOut == \"/std/collections/soa_vector/count\"") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "isCanonicalSoaHelperPath(resolvedSoaCountCanonical, \"count\")") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "resolvedSoaGetCanonical == \"/std/collections/soa_vector/get\"") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "resolvedSoaGetCanonical == \"/std/collections/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "const std::string resolvedSoaToAosCanonical =") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "const bool matchesSoaToAosTarget =") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "const bool matchesBorrowedSoaToAosTarget =") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "resolvedSoaToAosCanonical, \"to_aos\")") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "resolvedSoaToAosCanonical, \"to_aos_ref\")") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "((target.isMethodCall && target.name == \"to_aos\") ||\n"
            "           (!target.isMethodCall && isSimpleCallName(target, \"to_aos\")))") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "((target.isMethodCall && target.name == \"to_aos_ref\") ||\n"
            "           (!target.isMethodCall && isSimpleCallName(target, \"to_aos_ref\")))") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "canonicalizeLegacySoaToAosHelperPath(resolveCalleePath(target))") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "isCanonicalSoaToAosHelperPath(resolvedSoaToAosCanonical)") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "resolveCalleePath(target) == \"/to_aos\"") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "resolveCalleePath(target) == \"/to_aos_ref\"") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "resolveCalleePath(target) == \"/soa_vector/to_aos_ref\"") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "const std::string resolvedSoaRefCanonical =") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "const std::string resolvedSoaToAosCanonical =") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "canonicalizeLegacySoaToAosHelperPath(resolvedOut)") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "canonicalizeLegacySoaRefHelperPath(resolvedOut)") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "hasImportedDefinitionPath(resolvedSoaGetCanonical)") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "hasImportedDefinitionPath(resolvedSoaCountCanonical)") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "defMap_.count(resolvedSoaCountCanonical) != 0") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "hasImportedDefinitionPath(resolvedSoaToAosCanonical)") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "hasImportedDefinitionPath(resolvedSoaRefCanonical)") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "isCanonicalSoaRefLikeHelperPath(resolvedSoaRefCanonical)") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "isLegacyOrCanonicalSoaHelperPath(\n"
            "            resolvedSoaToAosCanonical, \"to_aos\")") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "isLegacyOrCanonicalSoaHelperPath(\n"
            "            resolvedSoaToAosCanonical, \"to_aos_ref\")") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "isCanonicalSoaToAosHelperPath(resolvedSoaToAosCanonical)") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "resolvedOut == \"/std/collections/soa_vector/to_aos\"") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "resolvedOut == \"/std/collections/soa_vector/to_aos_ref\"") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "resolvedSoaRefCanonical == \"/std/collections/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "resolvedSoaRefCanonical == \"/std/collections/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "resolvedOut == \"/std/collections/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "resolvedSoaGetCanonical == \"/std/collections/soa_vector/get\"") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "resolvedSoaGetCanonical == \"/std/collections/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedSoaGetCanonical, \"get\")") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedSoaGetCanonical,\n"
            "                                         \"get_ref\")") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find("preferredBorrowedSoaAccessHelperTarget(") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "/std/collections/experimental_soa_vector/soaVectorGetRef") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "hasVisibleSoaHelperTargetForCurrentImports(") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find("auto extractBorrowedBinding =") ==
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "auto resolveSoaVectorOrExperimentalBorrowedReceiver =") ==
        std::string::npos);
  CHECK(exprCollectionAccessSource.find(
            "this->resolveDirectSoaVectorOrExperimentalBorrowedReceiver(\n"
            "        target, params, locals, context.resolveSoaVectorTarget, elemTypeOut)") !=
        std::string::npos);
  CHECK(exprCollectionAccessSource.find("resolveSoaVectorOrExperimentalBorrowedTarget") ==
        std::string::npos);
  CHECK(exprCollectionAccessSource.find(
            "this->resolveSoaVectorOrExperimentalBorrowedReceiver(\n"
            "            receiverCandidate,") !=
        std::string::npos);
  CHECK(exprMethodCompatibilitySetupSource.find(
            "return soaUnavailableMethodDiagnostic(resolvedPath);") !=
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find(
            "soaUnavailableMethodDiagnostic(methodResolved));") !=
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "soaUnavailableMethodDiagnostic(methodResolved));") !=
        std::string::npos);
  CHECK(exprMethodTargetResolutionSource.find(
            "usesSamePathSoaHelperTargetForCollectionType(\"count\", \"/vector\")") !=
        std::string::npos);
  CHECK((exprMethodTargetResolutionSource.find(
            "usesSamePathSoaHelperTargetForCollectionType(\"get\", \"/vector\")") !=
         std::string::npos ||
         exprMethodTargetResolutionSource.find(
             "usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, \"/vector\")") !=
             std::string::npos));
  CHECK((exprMethodTargetResolutionSource.find(
            "usesSamePathSoaHelperTargetForCollectionType(\"ref\", \"/vector\")") !=
         std::string::npos ||
         exprMethodTargetResolutionSource.find(
             "usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, \"/vector\")") !=
             std::string::npos));
  CHECK(inferMethodResolutionSource.find(
            "usesSamePathSoaHelperTargetForCollectionType(\"count\", \"/vector\")") !=
        std::string::npos);
  CHECK((inferMethodResolutionSource.find(
            "usesSamePathSoaHelperTargetForCollectionType(\"get\", \"/vector\")") !=
         std::string::npos ||
         inferMethodResolutionSource.find(
             "usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, \"/vector\")") !=
             std::string::npos));
  CHECK((inferMethodResolutionSource.find(
            "usesSamePathSoaHelperTargetForCollectionType(\"ref\", \"/vector\")") !=
         std::string::npos ||
         inferMethodResolutionSource.find(
             "usesSamePathSoaHelperTargetForCollectionType(normalizedMethodName, \"/vector\")") !=
             std::string::npos));
  CHECK(inferDefinitionSource.find("isBuiltinSoaFieldViewExpr(") == std::string::npos);
  CHECK(inferDefinitionSource.find("isBuiltinSoaRefExpr(") == std::string::npos);
  CHECK(inferDefinitionSource.find("builtinSoaAccessHelperName(") ==
        std::string::npos);
  CHECK((inferDefinitionSource.find(
            "hasVisibleDefinitionPathForCurrentImports(\"/soa_vector/\" + helperName)") !=
        std::string::npos ||
        inferDefinitionSource.find(
            "hasVisibleSoaHelperTargetForCurrentImports(helperName)") !=
            std::string::npos));
  CHECK(inferDefinitionSource.find("helperName == \"get_ref\"") !=
        std::string::npos);
  CHECK(inferDefinitionSource.find("helperName == \"ref_ref\"") !=
        std::string::npos);
  CHECK(inferDefinitionSource.find(
            "!hasDeclaredDefinitionPath(helperPath) && !hasImportedDefinitionPath(helperPath)") ==
        std::string::npos);
  CHECK(inferDefinitionSource.find("builtinSoaPendingExprDiagnostic(*valueExpr, defParams, activeLocals)") ==
        std::string::npos);
  CHECK(inferDefinitionSource.find("reportBuiltinSoaPendingExprDiagnostic(*valueExpr, defParams, activeLocals)") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "bool SemanticsValidator::reportBuiltinSoaDirectPendingExprDiagnostic(") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find("normalizedName == \"get_ref\"") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "normalizedPrefix == \"std/collections/soa_vector\"") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "normalizedName == \"std/collections/soa_vector/get\"") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "normalizedName == \"std/collections/soa_vector/get_ref\"") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "normalizedName == \"std/collections/soa_vector/ref\"") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "normalizedName == \"std/collections/soa_vector/ref_ref\"") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "normalizedName == \"count_ref\"") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "normalizedName == \"soa_vector/count_ref\"") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "normalizedName == \"std/collections/soa_vector/count_ref\"") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "normalizedName == \"get_ref\"") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "normalizedName == \"soa_vector/get_ref\"") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "normalizedName == \"std/collections/soa_vector/get_ref\"") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "normalizedName == \"ref_ref\"") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "normalizedName == \"soa_vector/ref_ref\"") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "normalizedName == \"std/collections/soa_vector/ref_ref\"") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "const bool isSoaCountOrAccessSurfaceSpelling =") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "auto explicitStdSoaHelperName = [&]() -> std::string {") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "normalizedPrefix == \"soa_vector\"") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "helperName == \"to_aos_ref\"") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "preferredSoaHelperTargetForCollectionType(helperName, "
            "\"/soa_vector\")") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "resolvedCanonical == \"/std/collections/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedCanonical, \"get_ref\")") !=
        std::string::npos);
  CHECK(inferDefinitionSource.find(
            "soaDirectPendingUnavailableMethodDiagnostic(") ==
        std::string::npos);
  CHECK(inferDefinitionSource.find(
            "soaUnavailableMethodDiagnostic(*pendingPath)") !=
        std::string::npos);
  CHECK(exprCollectionAccessValidationSource.find(
            "reportBuiltinSoaPendingExprDiagnostic(expr.args.front(), params, locals)") ==
        std::string::npos);
  CHECK(exprResolvedCallArgumentsSource.find("reportBuiltinSoaPendingExprDiagnostic(*arg, params, locals)") ==
        std::string::npos);
  CHECK(exprResolvedCallArgumentsSource.find(
            "std::string canonicalizeLegacySoaRefResolvedPath(std::string_view path)") ==
        std::string::npos);
  CHECK(exprResolvedCallArgumentsSource.find(
            "canonicalizeLegacySoaRefHelperPath(resolvedPath)") !=
        std::string::npos);
  CHECK(exprResolvedCallArgumentsSource.find(
            "const std::string resolvedPathCanonical =") !=
        std::string::npos);
  CHECK(exprResolvedCallArgumentsSource.find(
            "resolvedPath.rfind(\"/soa_vector/ref\", 0)") ==
        std::string::npos);
  CHECK(exprResolvedCallArgumentsSource.find(
            "resolvedPath.rfind(\"/soa_vector/ref_ref\", 0)") ==
        std::string::npos);
  CHECK(exprResolvedCallArgumentsSource.find(
            "resolvedPathCanonical.rfind(\"/std/collections/soa_vector/ref_ref\", 0) !=") ==
        std::string::npos);
  CHECK(exprResolvedCallArgumentsSource.find(
            "isCanonicalSoaRefLikeHelperPath(resolvedPathCanonical)") !=
        std::string::npos);
  CHECK(exprResolvedCallArgumentsSource.find(
            "isExperimentalSoaRefLikeHelperPath(resolvedPathCanonical)") !=
        std::string::npos);
  CHECK(exprResolvedCallArgumentsSource.find(
            "/std/collections/internal_soa_storage/soaColumnRef") ==
        std::string::npos);
  CHECK(exprResolvedCallArgumentsSource.find(
            "/std/collections/experimental_soa_vector/soaVectorRef\"") ==
        std::string::npos);
  CHECK(exprResolvedCallArgumentsSource.find(
            "/std/collections/experimental_soa_vector/soaVectorRefRef\"") ==
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "const auto canonicalizeLegacySoaRefPath =") ==
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "canonicalizeLegacySoaRefHelperPath(resolvedPath)") !=
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "const std::string resolvedPathCanonical =") ==
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "const std::string canonicalResolvedPath =") !=
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "isCanonicalSoaRefLikeHelperPath(canonicalResolvedPath)") !=
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "isExperimentalSoaRefLikeHelperPath(canonicalResolvedPath)") !=
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "isExperimentalSoaMethodRefLikeHelperPath(canonicalResolvedPath)") !=
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "isExperimentalSoaRefLikeHelperPath(refExpr.name)") !=
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "isExperimentalSoaFieldViewHelperPath(resolvedFieldViewPath)") !=
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "isExperimentalSoaFieldViewRefHelperPath(resolvedPointerPath)") !=
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "isExperimentalSoaFieldViewHelperPath(resolvedPath)") !=
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find("auto hasRefLikeSuffix =") ==
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "resolvedPath.rfind(\"/soa_vector/ref\", 0) == 0") ==
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "resolvedPath.rfind(\"/soa_vector/ref_ref\", 0) == 0") ==
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "resolvedFieldViewPath.rfind(\n"
            "              \"/std/collections/experimental_soa_vector/soaVectorFieldView\",") ==
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "resolvedPath.rfind(\n"
            "                  \"/std/collections/experimental_soa_vector/soaVectorFieldView\",") ==
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "resolvedPath.rfind(\n"
            "                  \"/std/collections/internal_soa_storage/soaColumnFieldViewUnsafe\",") ==
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "canonicalResolvedPath.rfind(\n"
            "                   \"/std/collections/experimental_soa_vector/\", 0) == 0 &&") ==
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "\"/std/collections/internal_soa_storage/soaFieldViewRef\"") ==
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "/std/collections/experimental_soa_vector/soaVectorRef\"") ==
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "/std/collections/experimental_soa_vector/soaVectorRefRef\"") ==
        std::string::npos);
  CHECK(exprMutationBorrowsSource.find(
            "resolvedPath.rfind(\"/std/collections/soa_vector/ref_ref\", 0) ==") ==
        std::string::npos);
  CHECK(statementBindingsSource.find(
            "reportBuiltinSoaPendingExprDiagnostic(*initializerExprForValidation, params, locals)") ==
        std::string::npos);
  CHECK(statementBindingsSource.find("builtinSoaPendingExprDiagnostic(*initializerExprForValidation, params, locals)") ==
        std::string::npos);
  CHECK(statementBindingsSource.find("isHelperReturnBuiltinSoaRefExpr") ==
        std::string::npos);
  CHECK(statementBindingsSource.find(
            "error_ == \"ref does not accept template arguments\"") ==
        std::string::npos);
  CHECK(statementBindingsSource.find(
            "reportBuiltinSoaDirectPendingExprDiagnostic(initializer, params, locals)") ==
        std::string::npos);
  CHECK(statementBindingsSource.find(
            "soaDirectPendingUnavailableMethodDiagnostic(") ==
        std::string::npos);
  CHECK(statementBindingsSource.find(
            "soaUnavailableMethodDiagnostic(*pendingPath)") !=
        std::string::npos);
  CHECK(statementBindingsSource.find(
            "builtinSoaDirectPendingHelperPath(initializer, params, locals)") !=
        std::string::npos);
  CHECK(statementBindingsSource.find("BindingInfo recoveredInitializerBinding;") !=
        std::string::npos);
  CHECK(statementBindingsSource.find(
            "inferBindingTypeFromInitializer(initializer,\n"
            "                                        params,\n"
            "                                        locals,\n"
            "                                        recoveredInitializerBinding,\n"
            "                                        &stmt)") !=
        std::string::npos);
  CHECK(statementBindingsSource.find(
            "initKind = returnKindForTypeName(\n"
            "        normalizeBindingTypeName(recoveredInitializerBinding.typeName));") !=
        std::string::npos);
  CHECK(statementBindingsSource.find(
            "const auto canonicalizeSoaRefResolvedPath = [](std::string_view path) -> std::string {") ==
        std::string::npos);
  CHECK(statementBindingsSource.find(
            "canonicalizeLegacySoaRefHelperPath(resolvedPath)") !=
        std::string::npos);
  CHECK(statementBindingsSource.find(
            "const std::string resolvedPathCanonical =") !=
        std::string::npos);
  CHECK(statementBindingsSource.find("resolvedPath.rfind(\"/soa_vector/ref\", 0)") ==
        std::string::npos);
  CHECK(statementBindingsSource.find("resolvedPath.rfind(\"/soa_vector/ref_ref\", 0)") ==
        std::string::npos);
  CHECK(statementBindingsSource.find(
            "isCanonicalSoaRefLikeHelperPath(resolvedPathCanonical)") !=
        std::string::npos);
  CHECK(statementBindingsSource.find(
            "isExperimentalSoaRefLikeHelperPath(resolvedPathCanonical)") !=
        std::string::npos);
  CHECK(statementBindingsSource.find(
            "/std/collections/internal_soa_storage/soaColumnRef") ==
        std::string::npos);
  CHECK(statementBindingsSource.find(
            "/std/collections/experimental_soa_vector/soaVectorRef\"") ==
        std::string::npos);
  CHECK(statementBindingsSource.find(
            "/std/collections/experimental_soa_vector/soaVectorRefRef\"") ==
        std::string::npos);
  CHECK(statementReturnsSource.find("reportBuiltinSoaPendingExprDiagnostic(stmt.args.front(), params, locals)") ==
        std::string::npos);
  CHECK(statementReturnsSource.find("builtinSoaPendingExprDiagnostic(stmt.args.front(), params, locals)") ==
        std::string::npos);
  CHECK(statementReturnsSource.find(
            "const auto canonicalizeLegacySoaRefHelperPath =") ==
        std::string::npos);
  CHECK(statementReturnsSource.find(
            "canonicalizeLegacySoaRefHelperPath(resolvedPathNoTemplate)") !=
        std::string::npos);
  CHECK(statementReturnsSource.find(
            "const std::string resolvedPathCanonical =") !=
        std::string::npos);
  CHECK(statementReturnsSource.find("resolvedPath == \"/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(statementReturnsSource.find("resolvedPath == \"/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(statementReturnsSource.find(
            "isCanonicalSoaRefLikeHelperPath(resolvedPathCanonical)") !=
        std::string::npos);
  CHECK(statementReturnsSource.find(
            "isExperimentalSoaRefLikeHelperPath(resolvedPathNoTemplate)") !=
        std::string::npos);
  CHECK(statementReturnsSource.find(
            "/std/collections/internal_soa_storage/soaColumnRef") ==
        std::string::npos);
  CHECK(statementReturnsSource.find(
            "isExperimentalSoaFieldViewHelperPath(resolvedPath)") !=
        std::string::npos);
  CHECK(statementReturnsSource.find("resolvedPath.rfind(\"/soa_vector/ref\", 0)") ==
        std::string::npos);
  CHECK(statementReturnsSource.find("resolvedPath.rfind(\"/soa_vector/ref_ref\", 0)") ==
        std::string::npos);
  CHECK(statementReturnsSource.find(
            "resolvedPath.rfind(\n"
            "              \"/std/collections/experimental_soa_vector/soaVectorFieldView\",") ==
        std::string::npos);
  CHECK(statementReturnsSource.find(
            "resolvedPath.rfind(\n"
            "              \"/std/collections/internal_soa_storage/soaColumnFieldViewUnsafe\",") ==
        std::string::npos);
  CHECK(statementReturnsSource.find(
            "resolvedPathNoTemplate.rfind(\n"
            "              \"/std/collections/experimental_soa_vector/soaVectorRef\",") ==
        std::string::npos);
  CHECK(statementReturnsSource.find(
            "resolvedPathNoTemplate.rfind(\n"
            "              \"/std/collections/experimental_soa_vector/soaVectorRefRef\",") ==
        std::string::npos);
  CHECK(inferDefinitionSource.find("unknown method: /std/collections/soa_vector/ref") ==
        std::string::npos);
  CHECK(inferDefinitionSource.find("soaBorrowedViewPendingDiagnostic()") ==
        std::string::npos);
  CHECK(inferDefinitionSource.find(
            "builtinSoaDirectPendingHelperPath(*valueExpr, defParams,") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "auto usesVisibleSamePathSoaAccessHelper =") ==
        std::string::npos);
  CHECK((inferCollectionReturnInferenceSource.find(
            "usesSamePathSoaHelperTargetForCurrentImports(*soaAccessHelper)") !=
        std::string::npos ||
        inferCollectionReturnInferenceSource.find(
            "hasVisibleSoaHelperTargetForCurrentImports(*soaAccessHelper)") !=
            std::string::npos));
  CHECK(inferCollectionReturnInferenceSource.find(
            "const std::string samePath = \"/soa_vector/\" + *soaAccessHelper;") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "hasVisibleDefinitionPathForCurrentImports(samePath)") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "const bool oldSurfaceCallShape =") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "const auto canonicalizeLegacySoaHelperResolvedPath = [](std::string_view path) -> std::string {") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "canonicalizeLegacySoaGetHelperPath(resolvedCandidate)") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "std::string resolvedSoaCanonical =") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find("resolvedCandidate == \"/soa_vector/get\"") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find("resolvedCandidate == \"/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "resolvedSoaCanonical == \"/std/collections/soa_vector/get\"") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "resolvedSoaCanonical == \"/std/collections/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, \"get\")") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical,\n"
            "                                             \"get_ref\")") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find("resolvedCandidate == \"/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find("resolvedCandidate == \"/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "(*soaAccessHelper == \"ref\" &&") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "isSimpleCallName(candidate, \"ref\")") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "isSimpleCallName(candidate, \"ref_ref\")") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "isSimpleCallName(candidate, *soaAccessHelper)") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "candidate.name == *soaAccessHelper") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical,") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "resolvedSoaCanonical == \"/std/collections/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "(*soaAccessHelper == \"get_ref\" &&") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "usesVisibleSamePathSoaHelper(candidate, resolvedCandidate, \"get\")") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "builtinSoaAccessHelperName(candidate, params, locals)") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "preferredCollectionHelperResolvedPath(candidate)") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "auto explicitLegacyOrCanonicalSoaHelperName = [&]() -> "
            "std::string {") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "normalizedPrefix == \"soa_vector\"") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "normalizedPrefix == \"std/collections/soa_vector\"") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "preferredSoaHelperTargetForCollectionType(helperName,\n"
            "                                                         "
            "\"/soa_vector\")") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find("const bool isBuiltinSoaGetOrRef =") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find("auto hasVisibleSamePathSoaHelper =") ==
        std::string::npos);
}

TEST_SUITE_END();
