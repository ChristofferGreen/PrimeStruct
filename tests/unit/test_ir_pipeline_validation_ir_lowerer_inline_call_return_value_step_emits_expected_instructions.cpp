#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer inline-call return-value step emits expected instructions") {
  primec::IrFunction function;
  std::string error;

  CHECK(primec::ir_lowerer::runLowerInlineCallReturnValueStep(
      {
          .function = &function,
          .returnsVoid = false,
          .returnLocal = 17,
          .structDefinition = false,
          .requireValue = true,
      },
      error));
  CHECK(error.empty());
  REQUIRE(function.instructions.size() == 1u);
  CHECK(function.instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(function.instructions[0].imm == 17u);

  CHECK(primec::ir_lowerer::runLowerInlineCallReturnValueStep(
      {
          .function = &function,
          .returnsVoid = true,
          .returnLocal = -1,
          .structDefinition = true,
          .requireValue = true,
      },
      error));
  CHECK(error.empty());
  REQUIRE(function.instructions.size() == 2u);
  CHECK(function.instructions[1].op == primec::IrOpcode::PushI32);
  CHECK(function.instructions[1].imm == 0u);

  CHECK(primec::ir_lowerer::runLowerInlineCallReturnValueStep(
      {
          .function = &function,
          .returnsVoid = true,
          .returnLocal = -1,
          .structDefinition = true,
          .requireValue = false,
      },
      error));
  CHECK(error.empty());
  CHECK(function.instructions.size() == 2u);
}

TEST_CASE("ir lowerer inline-call return-value step validates dependencies") {
  primec::IrFunction function;
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::runLowerInlineCallReturnValueStep(
      {
          .function = nullptr,
          .returnsVoid = true,
          .returnLocal = -1,
          .structDefinition = false,
          .requireValue = false,
      },
      error));
  CHECK(error == "native backend missing inline-call return-value step dependency: function");

  CHECK_FALSE(primec::ir_lowerer::runLowerInlineCallReturnValueStep(
      {
          .function = &function,
          .returnsVoid = false,
          .returnLocal = -1,
          .structDefinition = false,
          .requireValue = false,
      },
      error));
  CHECK(error == "native backend missing inline-call return-value step dependency: returnLocal");
}

TEST_CASE("ir lowerer statements entry-execution step wires context and cleanup") {
  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  primec::Expr statementExpr;
  statementExpr.kind = primec::Expr::Kind::Literal;
  statementExpr.literalValue = 1;
  entryDef.statements.push_back(statementExpr);

  bool sawReturn = false;
  std::unordered_map<std::string, std::optional<primec::ir_lowerer::OnErrorHandler>> onErrorByDef;
  primec::ir_lowerer::OnErrorHandler entryOnError;
  entryOnError.handlerPath = "/handler";
  onErrorByDef.emplace("/main", entryOnError);

  std::optional<primec::ir_lowerer::OnErrorHandler> currentOnError;
  primec::ir_lowerer::OnErrorHandler previousOnError;
  previousOnError.handlerPath = "/previous";
  currentOnError = previousOnError;

  std::optional<primec::ir_lowerer::ResultReturnInfo> currentReturnResult;
  currentReturnResult = primec::ir_lowerer::ResultReturnInfo{.isResult = false, .hasValue = false};
  const primec::ir_lowerer::ResultReturnInfo entryResultInfo{.isResult = true, .hasValue = true};

  int emitStatementCalls = 0;
  int pushScopeCalls = 0;
  int cleanupCalls = 0;
  int popScopeCalls = 0;
  std::vector<primec::IrInstruction> instructions;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerStatementsEntryExecutionStep(
      {
          .entryDef = &entryDef,
          .returnsVoid = true,
          .sawReturn = &sawReturn,
          .onErrorByDef = &onErrorByDef,
          .currentOnError = &currentOnError,
          .currentReturnResult = &currentReturnResult,
          .entryHasResultInfo = true,
          .entryResultInfo = &entryResultInfo,
          .emitEntryStatement = [&](const primec::Expr &) {
            ++emitStatementCalls;
            REQUIRE(currentOnError.has_value());
            CHECK(currentOnError->handlerPath == "/handler");
            REQUIRE(currentReturnResult.has_value());
            CHECK(currentReturnResult->isResult);
            CHECK(currentReturnResult->hasValue);
            return true;
          },
          .pushFileScope = [&]() { ++pushScopeCalls; },
          .emitCurrentFileScopeCleanup = [&]() { ++cleanupCalls; },
          .popFileScope = [&]() { ++popScopeCalls; },
          .instructions = &instructions,
      },
      error));
  CHECK(error.empty());
  CHECK_FALSE(sawReturn);
  CHECK(emitStatementCalls == 1);
  CHECK(pushScopeCalls == 1);
  CHECK(cleanupCalls == 1);
  CHECK(popScopeCalls == 1);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions.front().op == primec::IrOpcode::ReturnVoid);
  REQUIRE(currentOnError.has_value());
  CHECK(currentOnError->handlerPath == "/previous");
  REQUIRE(currentReturnResult.has_value());
  CHECK_FALSE(currentReturnResult->isResult);
  CHECK_FALSE(currentReturnResult->hasValue);
}

TEST_CASE("ir lowerer statements entry-execution step validates dependencies") {
  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  bool sawReturn = false;
  std::unordered_map<std::string, std::optional<primec::ir_lowerer::OnErrorHandler>> onErrorByDef;
  std::optional<primec::ir_lowerer::OnErrorHandler> currentOnError;
  std::optional<primec::ir_lowerer::ResultReturnInfo> currentReturnResult;
  std::vector<primec::IrInstruction> instructions;
  const primec::ir_lowerer::ResultReturnInfo entryResultInfo{.isResult = true, .hasValue = true};
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::runLowerStatementsEntryExecutionStep(
      {
          .entryDef = nullptr,
          .returnsVoid = true,
          .sawReturn = &sawReturn,
          .onErrorByDef = &onErrorByDef,
          .currentOnError = &currentOnError,
          .currentReturnResult = &currentReturnResult,
          .entryHasResultInfo = false,
          .entryResultInfo = &entryResultInfo,
          .emitEntryStatement = [](const primec::Expr &) { return true; },
          .pushFileScope = []() {},
          .emitCurrentFileScopeCleanup = []() {},
          .popFileScope = []() {},
          .instructions = &instructions,
      },
      error));
  CHECK(error == "native backend missing statements entry-execution step dependency: entryDef");

  CHECK_FALSE(primec::ir_lowerer::runLowerStatementsEntryExecutionStep(
      {
          .entryDef = &entryDef,
          .returnsVoid = true,
          .sawReturn = &sawReturn,
          .onErrorByDef = &onErrorByDef,
          .currentOnError = &currentOnError,
          .currentReturnResult = &currentReturnResult,
          .entryHasResultInfo = true,
          .entryResultInfo = nullptr,
          .emitEntryStatement = [](const primec::Expr &) { return true; },
          .pushFileScope = []() {},
          .emitCurrentFileScopeCleanup = []() {},
          .popFileScope = []() {},
          .instructions = &instructions,
      },
      error));
  CHECK(error == "native backend missing statements entry-execution step dependency: entryResultInfo");
}

TEST_CASE("ir lowerer statements function-table step finalizes entry function") {
  primec::Program program;
  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  program.definitions.push_back(entryDef);

  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});

  std::unordered_set<std::string> loweredCallTargets;
  std::vector<std::string> defaultEffects;
  std::vector<std::string> entryDefaultEffects;
  int resetCalls = 0;
  int buildContextCalls = 0;
  int emitInlineCalls = 0;
  int32_t nextLocal = 5;
  std::vector<primec::IrFunction> outFunctions;
  int32_t entryIndex = -1;
  std::string error;

  CHECK(primec::ir_lowerer::runLowerStatementsFunctionTableStep(
      {
          .program = &program,
          .entryDef = &program.definitions.front(),
          .function = &function,
          .loweredCallTargets = &loweredCallTargets,
          .isStructDefinition = [](const primec::Definition &) { return false; },
          .getReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; },
          .defaultEffects = &defaultEffects,
          .entryDefaultEffects = &entryDefaultEffects,
          .isTailCallCandidate = [](const primec::Expr &) { return false; },
          .resetDefinitionLoweringState = [&]() { ++resetCalls; },
          .buildDefinitionCallContext =
              [&](const primec::Definition &,
                  int32_t &,
                  primec::ir_lowerer::LocalMap &,
                  primec::Expr &,
                  std::string &) {
                ++buildContextCalls;
                return true;
              },
          .emitInlineDefinitionCall =
              [&](const primec::Expr &,
                  const primec::Definition &,
                  const primec::ir_lowerer::LocalMap &,
                  bool) {
                ++emitInlineCalls;
                return true;
              },
          .nextLocal = &nextLocal,
          .outFunctions = &outFunctions,
          .entryIndex = &entryIndex,
      },
      error));
  CHECK(error.empty());
  CHECK(resetCalls == 0);
  CHECK(buildContextCalls == 0);
  CHECK(emitInlineCalls == 0);
  CHECK(nextLocal == 5);
  CHECK(entryIndex == 0);
  REQUIRE(outFunctions.size() == 1);
  CHECK(outFunctions.front().name == "/main");
}

TEST_CASE("ir lowerer statements function-table step validates dependencies") {
  primec::Program program;
  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  program.definitions.push_back(entryDef);

  primec::IrFunction function;
  std::unordered_set<std::string> loweredCallTargets;
  std::vector<std::string> defaultEffects;
  std::vector<std::string> entryDefaultEffects;
  int32_t nextLocal = 0;
  std::vector<primec::IrFunction> outFunctions;
  int32_t entryIndex = -1;
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::runLowerStatementsFunctionTableStep(
      {
          .program = nullptr,
          .entryDef = &program.definitions.front(),
          .function = &function,
          .loweredCallTargets = &loweredCallTargets,
          .isStructDefinition = [](const primec::Definition &) { return false; },
          .getReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; },
          .defaultEffects = &defaultEffects,
          .entryDefaultEffects = &entryDefaultEffects,
          .isTailCallCandidate = [](const primec::Expr &) { return false; },
          .resetDefinitionLoweringState = []() {},
          .buildDefinitionCallContext =
              [](const primec::Definition &, int32_t &, primec::ir_lowerer::LocalMap &, primec::Expr &, std::string &) {
                return true;
              },
          .emitInlineDefinitionCall =
              [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
                return true;
              },
          .nextLocal = &nextLocal,
          .outFunctions = &outFunctions,
          .entryIndex = &entryIndex,
      },
      error));
  CHECK(error == "native backend missing statements function-table step dependency: program");

  CHECK_FALSE(primec::ir_lowerer::runLowerStatementsFunctionTableStep(
      {
          .program = &program,
          .entryDef = &program.definitions.front(),
          .function = &function,
          .loweredCallTargets = &loweredCallTargets,
          .isStructDefinition = [](const primec::Definition &) { return false; },
          .getReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; },
          .defaultEffects = &defaultEffects,
          .entryDefaultEffects = &entryDefaultEffects,
          .isTailCallCandidate = [](const primec::Expr &) { return false; },
          .resetDefinitionLoweringState = []() {},
          .buildDefinitionCallContext =
              [](const primec::Definition &, int32_t &, primec::ir_lowerer::LocalMap &, primec::Expr &, std::string &) {
                return true;
              },
          .emitInlineDefinitionCall =
              [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
                return true;
              },
          .nextLocal = &nextLocal,
          .outFunctions = nullptr,
          .entryIndex = &entryIndex,
      },
      error));
  CHECK(error == "native backend missing statements function-table step dependency: outFunctions");
}

TEST_CASE("ir lowerer statements source-map step finalizes instruction metadata") {
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.sourceLine = 12;
  mainDef.sourceColumn = 4;

  primec::Definition helperDef;
  helperDef.fullPath = "/helper";
  helperDef.sourceLine = 25;
  helperDef.sourceColumn = 3;

  std::unordered_map<std::string, const primec::Definition *> defMap;
  defMap.emplace(mainDef.fullPath, &mainDef);
  defMap.emplace(helperDef.fullPath, &helperDef);

  std::unordered_map<std::string, std::vector<primec::ir_lowerer::InstructionSourceRange>>
      instructionSourceRangesByFunction;
  instructionSourceRangesByFunction["/main"].push_back({
      .beginIndex = 1,
      .endIndex = 2,
      .line = 77,
      .column = 9,
      .provenance = primec::IrSourceMapProvenance::SyntheticIr,
  });
  instructionSourceRangesByFunction["/main"].push_back({
      .beginIndex = 0,
      .endIndex = 2,
      .line = 100,
      .column = 2,
      .provenance = primec::IrSourceMapProvenance::CanonicalAst,
  });

  primec::IrModule module;
  primec::IrFunction mainFunction;
  mainFunction.name = "/main";
  mainFunction.instructions = {
      {primec::IrOpcode::PushI32, 1},
      {primec::IrOpcode::PushI32, 2},
      {primec::IrOpcode::AddI32, 0},
  };
  module.functions.push_back(mainFunction);

  primec::IrFunction helperFunction;
  helperFunction.name = "/helper";
  helperFunction.instructions = {
      {primec::IrOpcode::ReturnVoid, 0},
  };
  module.functions.push_back(helperFunction);

  primec::IrFunction missingFunction;
  missingFunction.name = "/missing";
  missingFunction.instructions = {
      {primec::IrOpcode::ReturnVoid, 0},
  };
  module.functions.push_back(missingFunction);

  std::vector<std::string> stringTable = {"alpha", "beta"};
  std::string error;
  CHECK(primec::ir_lowerer::runLowerStatementsSourceMapStep(
      {
          .defMap = &defMap,
          .instructionSourceRangesByFunction = &instructionSourceRangesByFunction,
          .stringTable = &stringTable,
          .outModule = &module,
      },
      error));
  CHECK(error.empty());

  uint32_t expectedDebugId = 1;
  for (const auto &function : module.functions) {
    for (const auto &instruction : function.instructions) {
      CHECK(instruction.debugId == expectedDebugId);
      ++expectedDebugId;
    }
  }

  REQUIRE(module.instructionSourceMap.size() == 5);
  CHECK(module.instructionSourceMap[0].line == 100);
  CHECK(module.instructionSourceMap[0].column == 2);
  CHECK(module.instructionSourceMap[0].provenance == primec::IrSourceMapProvenance::CanonicalAst);

  CHECK(module.instructionSourceMap[1].line == 77);
  CHECK(module.instructionSourceMap[1].column == 9);
  CHECK(module.instructionSourceMap[1].provenance == primec::IrSourceMapProvenance::SyntheticIr);

  CHECK(module.instructionSourceMap[2].line == 12);
  CHECK(module.instructionSourceMap[2].column == 4);
  CHECK(module.instructionSourceMap[2].provenance == primec::IrSourceMapProvenance::SyntheticIr);

  CHECK(module.instructionSourceMap[3].line == 25);
  CHECK(module.instructionSourceMap[3].column == 3);
  CHECK(module.instructionSourceMap[3].provenance == primec::IrSourceMapProvenance::SyntheticIr);

  CHECK(module.instructionSourceMap[4].line == 0);
  CHECK(module.instructionSourceMap[4].column == 0);
  CHECK(module.instructionSourceMap[4].provenance == primec::IrSourceMapProvenance::SyntheticIr);

  REQUIRE(module.stringTable.size() == 2);
  CHECK(module.stringTable[0] == "alpha");
  CHECK(module.stringTable[1] == "beta");
}

TEST_CASE("ir lowerer statements source-map step validates dependencies") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::vector<primec::ir_lowerer::InstructionSourceRange>>
      instructionSourceRangesByFunction;
  std::vector<std::string> stringTable;
  primec::IrModule module;
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::runLowerStatementsSourceMapStep(
      {
          .defMap = nullptr,
          .instructionSourceRangesByFunction = &instructionSourceRangesByFunction,
          .stringTable = &stringTable,
          .outModule = &module,
      },
      error));
  CHECK(error == "native backend missing statements source-map step dependency: defMap");

  CHECK_FALSE(primec::ir_lowerer::runLowerStatementsSourceMapStep(
      {
          .defMap = &defMap,
          .instructionSourceRangesByFunction = &instructionSourceRangesByFunction,
          .stringTable = &stringTable,
          .outModule = nullptr,
      },
      error));
  CHECK(error == "native backend missing statements source-map step dependency: outModule");
}

TEST_CASE("ir lowerer expr emit setup wires unary passthrough callbacks") {
  primec::ir_lowerer::LowerExprEmitMovePassthroughCallFn emitMovePassthroughCall;
  primec::ir_lowerer::LowerExprEmitUploadPassthroughCallFn emitUploadPassthroughCall;
  primec::ir_lowerer::LowerExprEmitReadbackPassthroughCallFn emitReadbackPassthroughCall;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerExprEmitSetup(
      {},
      emitMovePassthroughCall,
      emitUploadPassthroughCall,
      emitReadbackPassthroughCall,
      error));
  CHECK(error.empty());
  CHECK(static_cast<bool>(emitMovePassthroughCall));
  CHECK(static_cast<bool>(emitUploadPassthroughCall));
  CHECK(static_cast<bool>(emitReadbackPassthroughCall));

  primec::Expr literalExpr;
  literalExpr.kind = primec::Expr::Kind::Literal;
  literalExpr.literalValue = 1;

  auto assertUnaryPassthrough = [&](const primec::ir_lowerer::LowerExprEmitUnaryPassthroughCallFn &emitPassthrough,
                                    const std::string &callName) {
    primec::Expr callExpr;
    callExpr.kind = primec::Expr::Kind::Call;
    callExpr.name = callName;
    callExpr.args.push_back(literalExpr);

    int emitExprCalls = 0;
    std::string passError;
    const auto result = emitPassthrough(
        callExpr,
        primec::ir_lowerer::LocalMap{},
        [&](const primec::Expr &argExpr, const primec::ir_lowerer::LocalMap &) {
          ++emitExprCalls;
          return argExpr.kind == primec::Expr::Kind::Literal;
        },
        passError);
    CHECK(result == primec::ir_lowerer::UnaryPassthroughCallResult::Emitted);
    CHECK(passError.empty());
    CHECK(emitExprCalls == 1);
  };
  assertUnaryPassthrough(emitMovePassthroughCall, "move");
  assertUnaryPassthrough(emitUploadPassthroughCall, "upload");
  assertUnaryPassthrough(emitReadbackPassthroughCall, "readback");

  primec::Expr unknownCallExpr;
  unknownCallExpr.kind = primec::Expr::Kind::Call;
  unknownCallExpr.name = "unknown";
  unknownCallExpr.args.push_back(literalExpr);
  CHECK(emitUploadPassthroughCall(
            unknownCallExpr,
            primec::ir_lowerer::LocalMap{},
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::NotMatched);
}

TEST_CASE("ir lowerer expr emit setup validates unary callback dependencies") {
  primec::ir_lowerer::LowerExprEmitMovePassthroughCallFn emitMovePassthroughCall;
  primec::ir_lowerer::LowerExprEmitUploadPassthroughCallFn emitUploadPassthroughCall;
  primec::ir_lowerer::LowerExprEmitReadbackPassthroughCallFn emitReadbackPassthroughCall;
  std::string error;
  REQUIRE(primec::ir_lowerer::runLowerExprEmitSetup(
      {},
      emitMovePassthroughCall,
      emitUploadPassthroughCall,
      emitReadbackPassthroughCall,
      error));
  REQUIRE(error.empty());

  primec::Expr moveExpr;
  moveExpr.kind = primec::Expr::Kind::Call;
  moveExpr.name = "move";
  primec::Expr literalExpr;
  literalExpr.kind = primec::Expr::Kind::Literal;
  literalExpr.literalValue = 2;
  moveExpr.args.push_back(literalExpr);

  const auto result = emitMovePassthroughCall(moveExpr, primec::ir_lowerer::LocalMap{}, {}, error);
  CHECK(result == primec::ir_lowerer::UnaryPassthroughCallResult::Error);
  CHECK(error == "native backend missing expr emit setup dependency: emitExpr");

  primec::Expr uploadExpr = moveExpr;
  uploadExpr.name = "upload";
  const auto uploadResult = emitUploadPassthroughCall(uploadExpr, primec::ir_lowerer::LocalMap{}, {}, error);
  CHECK(uploadResult == primec::ir_lowerer::UnaryPassthroughCallResult::Error);
  CHECK(error == "native backend missing expr emit setup dependency: emitExpr");

  primec::Expr readbackExpr = moveExpr;
  readbackExpr.name = "readback";
  const auto readbackResult = emitReadbackPassthroughCall(readbackExpr, primec::ir_lowerer::LocalMap{}, {}, error);
  CHECK(readbackResult == primec::ir_lowerer::UnaryPassthroughCallResult::Error);
  CHECK(error == "native backend missing expr emit setup dependency: emitExpr");
}

TEST_CASE("ir lowerer expr emit setup dispatches move passthrough step") {
  primec::ir_lowerer::LowerExprEmitMovePassthroughCallFn emitMovePassthroughCall;
  primec::ir_lowerer::LowerExprEmitUploadPassthroughCallFn emitUploadPassthroughCall;
  primec::ir_lowerer::LowerExprEmitReadbackPassthroughCallFn emitReadbackPassthroughCall;
  std::string error;
  REQUIRE(primec::ir_lowerer::runLowerExprEmitSetup(
      {},
      emitMovePassthroughCall,
      emitUploadPassthroughCall,
      emitReadbackPassthroughCall,
      error));
  REQUIRE(error.empty());

  primec::Expr literalExpr;
  literalExpr.kind = primec::Expr::Kind::Literal;
  literalExpr.literalValue = 4;

  int emitExprCalls = 0;
  auto emitExpr = [&](const primec::Expr &argExpr, const primec::ir_lowerer::LocalMap &) {
    ++emitExprCalls;
    return argExpr.kind == primec::Expr::Kind::Literal;
  };

  primec::Expr moveExpr;
  moveExpr.kind = primec::Expr::Kind::Call;
  moveExpr.name = "move";
  moveExpr.args.push_back(literalExpr);
  CHECK(primec::ir_lowerer::runLowerExprEmitMovePassthroughStep(
            moveExpr,
            primec::ir_lowerer::LocalMap{},
            emitMovePassthroughCall,
            emitExpr,
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);

  primec::Expr unknownExpr = moveExpr;
  unknownExpr.name = "unknown";
  emitExprCalls = 0;
  error.clear();
  CHECK(primec::ir_lowerer::runLowerExprEmitMovePassthroughStep(
            unknownExpr,
            primec::ir_lowerer::LocalMap{},
            emitMovePassthroughCall,
            emitExpr,
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::NotMatched);
  CHECK(error.empty());
  CHECK(emitExprCalls == 0);
}

TEST_CASE("ir lowerer expr emit setup validates move dispatch dependencies") {
  primec::ir_lowerer::LowerExprEmitMovePassthroughCallFn emitMovePassthroughCall;
  primec::ir_lowerer::LowerExprEmitUploadPassthroughCallFn emitUploadPassthroughCall;
  primec::ir_lowerer::LowerExprEmitReadbackPassthroughCallFn emitReadbackPassthroughCall;
  std::string error;
  REQUIRE(primec::ir_lowerer::runLowerExprEmitSetup(
      {},
      emitMovePassthroughCall,
      emitUploadPassthroughCall,
      emitReadbackPassthroughCall,
      error));
  REQUIRE(error.empty());

  primec::Expr moveExpr;
  moveExpr.kind = primec::Expr::Kind::Call;
  moveExpr.name = "move";
  primec::Expr literalExpr;
  literalExpr.kind = primec::Expr::Kind::Literal;
  literalExpr.literalValue = 1;
  moveExpr.args.push_back(literalExpr);

  CHECK(primec::ir_lowerer::runLowerExprEmitMovePassthroughStep(
            moveExpr,
            primec::ir_lowerer::LocalMap{},
            {},
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::Error);
  CHECK(error == "native backend missing expr emit setup dependency: emitMovePassthroughCall");

  CHECK(primec::ir_lowerer::runLowerExprEmitMovePassthroughStep(
            moveExpr,
            primec::ir_lowerer::LocalMap{},
            emitMovePassthroughCall,
            {},
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::Error);
  CHECK(error == "native backend missing expr emit setup dependency: emitExpr");
}


TEST_SUITE_END();
