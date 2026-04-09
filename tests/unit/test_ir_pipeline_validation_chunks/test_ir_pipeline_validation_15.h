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
      "#include \"IrLowererLowerSetupEntryEffects.h\"",
      "#include \"IrLowererLowerSetupImportsStructs.h\"",
      "#include \"IrLowererLowerSetupLocals.h\"",
      "#include \"IrLowererLowerSetupInference.h\"",
      "#include \"IrLowererLowerReturnAndCalls.h\"",
      "#include \"IrLowererLowerOperators.h\"",
      "#include \"IrLowererLowerStatementsExpr.h\"",
      "#include \"IrLowererLowerStatementsBindings.h\"",
      "#include \"IrLowererLowerStatementsLoops.h\"",
      "#include \"IrLowererLowerStatementsCalls.h\"",
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
  CHECK(lowererSource.find("#include \"IrLowererLowerStatementsCallsStep.h\"") != std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerStatementsEntryExecutionStep.h\"") != std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerStatementsEntryStatementStep.h\"") != std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerStatementsFunctionTableStep.h\"") != std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerStatementsSourceMapStep.h\"") != std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerInlineCallActiveContextStep.h\"") != std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerInlineCallCleanupStep.h\"") != std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerInlineCallContextSetupStep.h\"") != std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerInlineCallGpuLocalsStep.h\"") != std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerInlineCallReturnValueStep.h\"") != std::string::npos);
  CHECK(lowererSource.find("#include \"IrLowererLowerInlineCallStatementStep.h\"") != std::string::npos);

  const std::filesystem::path setupHeaderPath = repoRoot / "src" / "ir_lowerer" / "IrLowererLowerSetupEntryEffects.h";
  REQUIRE(std::filesystem::exists(setupHeaderPath));
  const std::string setupHeaderSource = readText(setupHeaderPath);
  CHECK(setupHeaderSource.find("runLowerEntrySetup(") != std::string::npos);

  const std::filesystem::path importsHeaderPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerSetupImportsStructs.h";
  REQUIRE(std::filesystem::exists(importsHeaderPath));
  const std::string importsHeaderSource = readText(importsHeaderPath);
  CHECK(importsHeaderSource.find("runLowerImportsStructsSetup(") != std::string::npos);

  const std::filesystem::path localsHeaderPath = repoRoot / "src" / "ir_lowerer" / "IrLowererLowerSetupLocals.h";
  REQUIRE(std::filesystem::exists(localsHeaderPath));
  const std::string localsHeaderSource = readText(localsHeaderPath);
  CHECK(localsHeaderSource.find("runLowerLocalsSetup(") != std::string::npos);

  const std::filesystem::path inferenceHeaderPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerSetupInference.h";
  REQUIRE(std::filesystem::exists(inferenceHeaderPath));
  const std::string inferenceHeaderSource = readText(inferenceHeaderPath);
  CHECK(inferenceHeaderSource.find("runLowerInferenceSetup(") != std::string::npos);
  CHECK(inferenceHeaderSource.find("runLowerInferenceSetupBootstrap(") == std::string::npos);
  CHECK(inferenceHeaderSource.find("runLowerInferenceArrayKindSetup(") == std::string::npos);
  CHECK(inferenceHeaderSource.find("runLowerInferenceExprKindBaseSetup(") == std::string::npos);
  CHECK(inferenceHeaderSource.find("runLowerInferenceExprKindCallBaseSetup(") == std::string::npos);
  CHECK(inferenceHeaderSource.find("runLowerInferenceExprKindCallReturnSetup(") == std::string::npos);
  CHECK(inferenceHeaderSource.find("runLowerInferenceExprKindCallFallbackSetup(") == std::string::npos);
  CHECK(inferenceHeaderSource.find("runLowerInferenceExprKindCallOperatorFallbackSetup(") == std::string::npos);
  CHECK(inferenceHeaderSource.find("runLowerInferenceExprKindCallControlFlowFallbackSetup(") == std::string::npos);
  CHECK(inferenceHeaderSource.find("runLowerInferenceExprKindCallPointerFallbackSetup(") == std::string::npos);
  CHECK(inferenceHeaderSource.find("runLowerInferenceExprKindDispatchSetup(") == std::string::npos);
  CHECK(inferenceHeaderSource.find("runLowerInferenceReturnInfoSetup(") == std::string::npos);
  CHECK(inferenceHeaderSource.find("runLowerInferenceGetReturnInfoSetup(") == std::string::npos);
  CHECK(inferenceHeaderSource.find("runLowerInferenceGetReturnInfoCallbackSetup(") == std::string::npos);
  CHECK(inferenceHeaderSource.find("runLowerInferenceGetReturnInfoStep(") == std::string::npos);
  CHECK(inferenceHeaderSource.find("inferCallExprControlFlowFallbackKind(expr, localsIn, error, controlFlowKind)") ==
        std::string::npos);
  CHECK(inferenceHeaderSource.find("returnInfoCache.find(path)") == std::string::npos);

  const std::filesystem::path returnInfoHeaderPath = repoRoot / "src" / "ir_lowerer" / "IrLowererLowerReturnInfo.h";
  REQUIRE(std::filesystem::exists(returnInfoHeaderPath));
  const std::string returnInfoHeaderSource = readText(returnInfoHeaderPath);
  CHECK(returnInfoHeaderSource.find("analyzeDeclaredReturnTransforms(") == std::string::npos);
  CHECK(returnInfoHeaderSource.find("inferDefinitionReturnType(") == std::string::npos);

  const std::filesystem::path emitExprHeaderPath = repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExpr.h";
  REQUIRE(std::filesystem::exists(emitExprHeaderPath));
  const std::string emitExprHeaderSource = readText(emitExprHeaderPath);
  CHECK(emitExprHeaderSource.find("runLowerReturnCallsSetup(") != std::string::npos);
  CHECK(emitExprHeaderSource.find("runLowerExprEmitSetup(") != std::string::npos);
  CHECK(emitExprHeaderSource.find("runLowerExprEmitMovePassthroughStep(") != std::string::npos);
  CHECK(emitExprHeaderSource.find("runLowerExprEmitUploadReadbackPassthroughStep(") != std::string::npos);

  const std::filesystem::path statementsCallsHeaderPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerStatementsCalls.h";
  REQUIRE(std::filesystem::exists(statementsCallsHeaderPath));
  const std::string statementsCallsHeaderSource = readText(statementsCallsHeaderPath);
  CHECK(statementsCallsHeaderSource.find("runLowerStatementsCallsStep(") != std::string::npos);
  CHECK(statementsCallsHeaderSource.find("runLowerStatementsEntryStatementStep(") != std::string::npos);
  CHECK(statementsCallsHeaderSource.find("runLowerStatementsEntryExecutionStep(") != std::string::npos);
  CHECK(statementsCallsHeaderSource.find("runLowerStatementsFunctionTableStep(") != std::string::npos);
  CHECK(statementsCallsHeaderSource.find("runLowerStatementsSourceMapStep(") != std::string::npos);
  CHECK(statementsCallsHeaderSource.find("const size_t startInstructionIndex = function.instructions.size();") ==
        std::string::npos);
  CHECK(statementsCallsHeaderSource.find(
            "appendInstructionSourceRange(function.name, stmt, startInstructionIndex, function.instructions.size());") ==
        std::string::npos);
  CHECK(statementsCallsHeaderSource.find("tryEmitBufferStoreStatement(") == std::string::npos);
  CHECK(statementsCallsHeaderSource.find("tryEmitDispatchStatement(") == std::string::npos);
  CHECK(statementsCallsHeaderSource.find("tryEmitDirectCallStatement(") == std::string::npos);
  CHECK(statementsCallsHeaderSource.find("emitAssignOrExprStatementWithPop(") == std::string::npos);
  CHECK(statementsCallsHeaderSource.find("emitEntryCallableExecutionWithCleanup(") == std::string::npos);
  CHECK(statementsCallsHeaderSource.find("finalizeEntryFunctionTableAndLowerCallables(") == std::string::npos);
  CHECK(statementsCallsHeaderSource.find("instruction.debugId = static_cast<uint32_t>(nextInstructionDebugId)") ==
        std::string::npos);
  CHECK(statementsCallsHeaderSource.find("out.instructionSourceMap.push_back(") == std::string::npos);
  CHECK(statementsCallsHeaderSource.find("out.stringTable = std::move(stringTable);") == std::string::npos);

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
  CHECK(source.find("std/collections/soa_vector/get") == std::string::npos);
  CHECK(source.find("std/collections/soa_vector/ref") == std::string::npos);
  CHECK(source.find("std/collections/soa_vector/count") == std::string::npos);
  CHECK(source.find("std/collections/soa_vector/push") == std::string::npos);
  CHECK(source.find("std/collections/soa_vector/reserve") == std::string::npos);
}

TEST_CASE("soa pending diagnostics route through shared semantics helpers") {
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
  const std::filesystem::path exprArgumentValidationCollectionsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprArgumentValidationCollections.cpp";
  const std::filesystem::path exprMapSoaBuiltinsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprMapSoaBuiltins.cpp";
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
  const std::filesystem::path exprResolvedCallArgumentsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprResolvedCallArguments.cpp";
  const std::filesystem::path statementBindingsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorStatementBindings.cpp";
  const std::filesystem::path statementReturnsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorStatementReturns.cpp";
  REQUIRE(std::filesystem::exists(builtinPathHelpersPath));
  REQUIRE(std::filesystem::exists(semanticsHelpersPath));
  REQUIRE(std::filesystem::exists(buildInitializerInferencePath));
  REQUIRE(std::filesystem::exists(exprArgumentValidationCollectionsPath));
  REQUIRE(std::filesystem::exists(exprMapSoaBuiltinsPath));
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
  REQUIRE(std::filesystem::exists(exprResolvedCallArgumentsPath));
  REQUIRE(std::filesystem::exists(statementBindingsPath));
  REQUIRE(std::filesystem::exists(statementReturnsPath));

  const std::string builtinPathHelpersSource = readText(builtinPathHelpersPath);
  const std::string semanticsHelpersSource = readText(semanticsHelpersPath);
  const std::string buildInitializerInferenceSource = readText(buildInitializerInferencePath);
  const std::string exprArgumentValidationCollectionsSource =
      readText(exprArgumentValidationCollectionsPath);
  const std::string exprMapSoaBuiltinsSource = readText(exprMapSoaBuiltinsPath);
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
  const std::string exprResolvedCallArgumentsSource = readText(exprResolvedCallArgumentsPath);
  const std::string statementBindingsSource = readText(statementBindingsPath);
  const std::string statementReturnsSource = readText(statementReturnsPath);

  CHECK(builtinPathHelpersSource.find("std::string soaFieldViewHelperPath(std::string_view fieldName)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "bool splitSoaFieldViewHelperPath(std::string_view path, std::string *fieldNameOut)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find("namespace {\n\nstd::string soaFieldViewPendingDiagnostic(") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "std::string soaBorrowedViewPendingDiagnostic(std::string_view helperName)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "std::string soaDirectFieldViewPendingDiagnostic(std::string_view fieldName)") ==
        std::string::npos);
  CHECK(builtinPathHelpersSource.find("std::string soaDirectBorrowedViewPendingDiagnostic()") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "bool SemanticsValidator::hasVisibleDefinitionPathForCurrentImports(") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "bool SemanticsValidator::hasVisibleSoaHelperTargetForCurrentImports(") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "std::string SemanticsValidator::preferredSoaHelperTargetForCurrentImports(") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "std::string SemanticsValidator::preferredVisibleDefinitionPathForCurrentImports(") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "std::optional<std::string> SemanticsValidator::builtinSoaAccessHelperName(") !=
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
            "soaDirectFieldViewPendingDiagnostic(fieldName)") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "soaDirectBorrowedViewPendingDiagnostic()") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find(
            "return soaFieldViewHelperPath(fieldName);") !=
        std::string::npos);
  CHECK((buildInitializerInferenceSource.find(
            "return std::string(\"/soa_vector/ref\");") !=
         std::string::npos ||
         buildInitializerInferenceSource.find(
             "return std::string(\"/soa_vector/\") + *soaAccessHelper;") !=
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
            "std::string soaDirectPendingUnavailableMethodDiagnostic(") !=
        std::string::npos);
  CHECK(semanticsHelpersSource.find(
            "std::string soaDirectPendingUnavailableMethodDiagnostic(") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find("soa_vector field views are not implemented yet: ") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find("soa_vector field view requires soa_vector target") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find("splitSoaFieldViewHelperPath(resolved)") !=
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "soaDirectPendingUnavailableMethodDiagnostic(resolved)") !=
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
  CHECK(exprMapSoaBuiltinsSource.find("soaUnavailableMethodDiagnostic(resolved, false)") ==
        std::string::npos);
  CHECK(exprMapSoaBuiltinsSource.find(
            "builtinSoaAccessHelperName(expr, params, locals)") !=
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
  CHECK((exprCountCapacityMapBuiltinsSource.find(
            "preferredSoaHelperTargetForCurrentImports(\"count\") ==\n"
            "                \"/soa_vector/count\"") !=
        std::string::npos ||
        exprCountCapacityMapBuiltinsSource.find(
            "hasVisibleSoaHelperTargetForCurrentImports(\"count\")") !=
            std::string::npos));
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
            "resolvedOut == \"/std/collections/soa_vector/get_ref\"") !=
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
  CHECK((exprMethodCompatibilitySetupSource.find(
            "soaUnavailableMethodDiagnostic(\n"
            "        resolvedPath,\n"
            "        this->usesSamePathSoaHelperTargetForCurrentImports(\"ref\"))") !=
        std::string::npos ||
        exprMethodCompatibilitySetupSource.find(
            "soaUnavailableMethodDiagnostic(\n"
            "        resolvedPath,\n"
            "        this->hasVisibleSoaHelperTargetForCurrentImports(\"ref\"))") !=
            std::string::npos ||
        exprMethodCompatibilitySetupSource.find(
            "this->hasVisibleSoaHelperTargetForCurrentImports(helperName)") !=
            std::string::npos));
  CHECK((inferPreDispatchCallsSource.find(
            "usesSamePathSoaHelperTargetForCurrentImports(\"ref\")))") !=
        std::string::npos ||
        inferPreDispatchCallsSource.find(
            "hasVisibleSoaHelperTargetForCurrentImports(\"ref\")))") !=
            std::string::npos ||
        inferPreDispatchCallsSource.find(
            "hasVisibleSoaBorrowedHelperForPath(methodResolved))") !=
            std::string::npos));
  CHECK((inferLateFallbackBuiltinsSource.find(
            "usesSamePathSoaHelperTargetForCurrentImports(\"ref\")))") !=
        std::string::npos ||
        inferLateFallbackBuiltinsSource.find(
            "hasVisibleSoaHelperTargetForCurrentImports(\"ref\")))") !=
            std::string::npos ||
        inferLateFallbackBuiltinsSource.find(
            "hasVisibleSoaBorrowedHelperForPath(methodResolved))") !=
            std::string::npos));
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
  CHECK(buildInitializerInferenceSource.find("resolved == \"/soa_vector/get_ref\"") !=
        std::string::npos);
  CHECK(inferDefinitionSource.find(
            "soaDirectPendingUnavailableMethodDiagnostic(") !=
        std::string::npos);
  CHECK(exprCollectionAccessValidationSource.find(
            "reportBuiltinSoaPendingExprDiagnostic(expr.args.front(), params, locals)") ==
        std::string::npos);
  CHECK(exprResolvedCallArgumentsSource.find("reportBuiltinSoaPendingExprDiagnostic(*arg, params, locals)") ==
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
            "soaDirectPendingUnavailableMethodDiagnostic(") !=
        std::string::npos);
  CHECK(statementBindingsSource.find(
            "builtinSoaDirectPendingHelperPath(initializer, params, locals)") !=
        std::string::npos);
  CHECK(statementReturnsSource.find("reportBuiltinSoaPendingExprDiagnostic(stmt.args.front(), params, locals)") ==
        std::string::npos);
  CHECK(statementReturnsSource.find("builtinSoaPendingExprDiagnostic(stmt.args.front(), params, locals)") ==
        std::string::npos);
  CHECK(inferDefinitionSource.find("soa_vector borrowed views are not implemented yet: ref") ==
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
            "(*soaAccessHelper == \"get_ref\" &&") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "usesVisibleSamePathSoaHelper(candidate, resolvedCandidate, \"get\")") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "builtinSoaAccessHelperName(candidate, params, locals)") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find("const bool isBuiltinSoaGetOrRef =") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find("auto hasVisibleSamePathSoaHelper =") ==
        std::string::npos);
}
