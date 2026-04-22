#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer statement call helper validates direct-call diagnostics") {
  using EmitResult = primec::ir_lowerer::DirectCallStatementEmitResult;

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "write";
  stmt.isMethodCall = true;
  stmt.hasBodyArguments = true;

  primec::Definition methodDef;
  methodDef.fullPath = "/main/Writer.write";

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            stmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return &methodDef;
            },
            [](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return true; },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
              return true;
            },
            instructions,
            error) == EmitResult::Error);
  CHECK(error == "native backend does not support block arguments on calls");

  stmt.hasBodyArguments = false;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            stmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return true; },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
              return true;
            },
            instructions,
            error) == EmitResult::Error);
  CHECK(error.empty());

  primec::Expr defStmt;
  defStmt.kind = primec::Expr::Kind::Call;
  defStmt.name = "/main/g";
  primec::Definition def;
  def.fullPath = "/main/g";
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            defStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * { return &def; },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
              return true;
            },
            instructions,
            error) == EmitResult::Error);

  primec::Expr otherStmt;
  otherStmt.kind = primec::Expr::Kind::Call;
  otherStmt.name = "notify";
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            otherStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return true; },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
              return true;
            },
            instructions,
            error) == EmitResult::NotMatched);
  CHECK(error.empty());

  primec::Expr soaFieldStmt;
  soaFieldStmt.kind = primec::Expr::Kind::Call;
  soaFieldStmt.name = "x";
  primec::Expr soaValuesName;
  soaValuesName.kind = primec::Expr::Kind::Name;
  soaValuesName.name = "values";
  soaFieldStmt.args.push_back(soaValuesName);
  soaFieldStmt.argNames.push_back(std::nullopt);

  primec::ir_lowerer::LocalMap soaLocals;
  primec::ir_lowerer::LocalInfo soaValuesInfo;
  soaValuesInfo.isSoaVector = true;
  soaLocals.emplace("values", soaValuesInfo);

  int methodResolutionCalls = 0;
  int definitionResolutionCalls = 0;
  error = "preserve me";
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            soaFieldStmt,
            soaLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              ++methodResolutionCalls;
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++definitionResolutionCalls;
              return nullptr;
            },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return true; },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
              return true;
            },
            instructions,
            error) == EmitResult::NotMatched);
  CHECK(error == "preserve me");
  CHECK(methodResolutionCalls == 1);
  CHECK(definitionResolutionCalls == 1);
}

TEST_CASE("ir lowerer statement call helper emits assign and expression pops") {
  using EmitResult = primec::ir_lowerer::AssignOrExprStatementEmitResult;

  primec::Expr assignStmt;
  assignStmt.kind = primec::Expr::Kind::Call;
  assignStmt.name = "assign";

  std::vector<primec::IrInstruction> instructions;
  CHECK(primec::ir_lowerer::emitAssignOrExprStatementWithPop(
            assignStmt,
            {},
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              instructions.push_back({primec::IrOpcode::PushI32, 10});
              return true;
            },
            instructions) == EmitResult::Emitted);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].op == primec::IrOpcode::Pop);

  primec::Expr exprStmt;
  exprStmt.kind = primec::Expr::Kind::Call;
  exprStmt.name = "notify";
  instructions.clear();
  CHECK(primec::ir_lowerer::emitAssignOrExprStatementWithPop(
            exprStmt,
            {},
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              instructions.push_back({primec::IrOpcode::PushI64, 11});
              return true;
            },
            instructions) == EmitResult::Emitted);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].op == primec::IrOpcode::Pop);
}

TEST_CASE("ir lowerer statement call helper validates assign and expression pop errors") {
  using EmitResult = primec::ir_lowerer::AssignOrExprStatementEmitResult;

  primec::Expr assignStmt;
  assignStmt.kind = primec::Expr::Kind::Call;
  assignStmt.name = "assign";

  std::vector<primec::IrInstruction> instructions;
  CHECK(primec::ir_lowerer::emitAssignOrExprStatementWithPop(
            assignStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            instructions) == EmitResult::Error);
  CHECK(instructions.empty());

  primec::Expr exprStmt;
  exprStmt.kind = primec::Expr::Kind::Call;
  exprStmt.name = "notify";
  CHECK(primec::ir_lowerer::emitAssignOrExprStatementWithPop(
            exprStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            instructions) == EmitResult::Error);
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer statement call helper builds callable definition contexts") {
  primec::Definition def;
  def.fullPath = "/main/Kernel";
  def.namespacePrefix = "/main";
  def.transforms.push_back(primec::Transform{.name = "compute"});
  primec::Expr p0;
  p0.kind = primec::Expr::Kind::Name;
  p0.name = "x";
  primec::Expr p1;
  p1.kind = primec::Expr::Kind::Name;
  p1.name = "y";
  def.parameters = {p0, p1};

  int32_t nextLocal = 0;
  const std::unordered_set<std::string> structNames;
  primec::ir_lowerer::LocalMap locals;
  primec::Expr callExpr;
  std::string error;
  CHECK(primec::ir_lowerer::buildCallableDefinitionCallContext(
      def,
      structNames,
      nextLocal,
      locals,
      callExpr,
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo &info, std::string &) {
        info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
        info.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        return true;
      },
      error));
  CHECK(error.empty());
  CHECK(nextLocal == 5);
  CHECK(locals.count("__gpu_global_id_x") == 1);
  CHECK(locals.count("__gpu_global_id_y") == 1);
  CHECK(locals.count("__gpu_global_id_z") == 1);
  CHECK(locals.count("x") == 1);
  CHECK(locals.count("y") == 1);
  CHECK(callExpr.name == "/main/Kernel");
  REQUIRE(callExpr.args.size() == 2);
  CHECK(callExpr.args[0].name == "x");
  CHECK(callExpr.args[1].name == "y");

  nextLocal = 0;
  locals.clear();
  callExpr = {};
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::buildCallableDefinitionCallContext(
      def,
      structNames,
      nextLocal,
      locals,
      callExpr,
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo &info, std::string &) {
        info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
        info.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
        return true;
      },
      error));
  CHECK(error == "native backend requires typed parameters on /main/Kernel");
}

TEST_CASE("ir lowerer statement call helper builds callable context for struct helpers with implicit this") {
  primec::Definition structDef;
  structDef.fullPath = "/pkg/Widget";
  structDef.namespacePrefix = "/pkg";
  structDef.transforms.push_back(primec::Transform{.name = "struct"});

  primec::Definition helperDef;
  helperDef.fullPath = "/pkg/Widget/count";
  helperDef.namespacePrefix = "/pkg";
  helperDef.isNested = true;
  helperDef.transforms.push_back(primec::Transform{.name = "return", .templateArgs = {"i32"}});

  std::unordered_set<std::string> structNames = {"/pkg/Widget"};
  int32_t nextLocal = 0;
  primec::ir_lowerer::LocalMap locals;
  primec::Expr callExpr;
  std::string error;
  CHECK(primec::ir_lowerer::buildCallableDefinitionCallContext(
      helperDef,
      structNames,
      nextLocal,
      locals,
      callExpr,
      [](const std::string &structPath, primec::ir_lowerer::StructSlotLayoutInfo &layout) {
        if (structPath != "/pkg/Widget") {
          return false;
        }
        layout.structPath = structPath;
        layout.totalSlots = 1;
        return true;
      },
      [](const primec::Expr &param,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo &info,
         std::string &) {
        info.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
        info.structTypeName = "/pkg/Widget";
        if (param.name != "this") {
          info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
          info.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
          info.structTypeName.clear();
        }
        return true;
      },
      error));
  CHECK(error.empty());
  CHECK(locals.count("this") == 1u);
  CHECK(locals.at("this").kind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(locals.at("this").structTypeName == "/pkg/Widget");
  REQUIRE(callExpr.args.size() == 1u);
  CHECK(callExpr.args.front().name == "this");
}

TEST_CASE("ir lowerer statement call helper skips struct layouts for file-handle this locals") {
  primec::Definition helperDef;
  helperDef.fullPath = "/std/file/File/close";
  helperDef.namespacePrefix = "/std/file";
  helperDef.isNested = true;
  helperDef.transforms.push_back(primec::Transform{.name = "return", .templateArgs = {"void"}});

  std::unordered_set<std::string> structNames = {"/std/file/File"};
  int32_t nextLocal = 0;
  primec::ir_lowerer::LocalMap locals;
  primec::Expr callExpr;
  std::string error;
  int layoutLookups = 0;
  CHECK(primec::ir_lowerer::buildCallableDefinitionCallContext(
      helperDef,
      structNames,
      nextLocal,
      locals,
      callExpr,
      [&](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) {
        ++layoutLookups;
        return false;
      },
      [](const primec::Expr &param,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo &info,
         std::string &) {
        if (param.name == "this") {
          info.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
          info.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
          info.isFileHandle = true;
          info.structTypeName = "/std/file/File<Write>";
          return true;
        }
        return false;
      },
      error));
  CHECK(error.empty());
  CHECK(layoutLookups == 0);
  REQUIRE(locals.count("this") == 1u);
  CHECK(locals.at("this").isFileHandle);
  CHECK(locals.at("this").structTypeName.empty());
  REQUIRE(callExpr.args.size() == 1u);
  CHECK(callExpr.args.front().name == "this");
}

TEST_CASE("ir lowerer statement call helper preserves struct slot counts for variadic callable locals") {
  primec::Definition def;
  def.fullPath = "/pkg/score";

  primec::Expr valuesParam;
  valuesParam.kind = primec::Expr::Kind::Name;
  valuesParam.isBinding = true;
  valuesParam.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs = {"/pkg/MapLike"};
  valuesParam.transforms.push_back(std::move(argsTransform));
  def.parameters = {valuesParam};

  int32_t nextLocal = 0;
  const std::unordered_set<std::string> structNames;
  primec::ir_lowerer::LocalMap locals;
  primec::Expr callExpr;
  std::string error;
  CHECK(primec::ir_lowerer::buildCallableDefinitionCallContext(
      def,
      structNames,
      nextLocal,
      locals,
      callExpr,
      [](const std::string &structPath, primec::ir_lowerer::StructSlotLayoutInfo &layout) {
        if (structPath != "/pkg/MapLike") {
          return false;
        }
        layout.structPath = structPath;
        layout.totalSlots = 2;
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::LocalInfo &info, std::string &) {
        info.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        info.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
        info.structTypeName = "/pkg/MapLike";
        info.isArgsPack = true;
        return true;
      },
      error));
  CHECK(error.empty());
  REQUIRE(locals.count("values") == 1u);
  CHECK(locals.at("values").structTypeName == "/pkg/MapLike");
  CHECK(locals.at("values").structSlotCount == 2);
  CHECK(locals.at("values").argsPackElementCount == 0);
  REQUIRE(callExpr.args.size() == 1u);
  CHECK(callExpr.args.front().name == "values");
}

TEST_CASE("ir lowerer statement call helper orchestrates callable lowering") {
  using Result = primec::ir_lowerer::CallableDefinitionOrchestrationResult;

  primec::Program program;
  primec::Definition entry;
  entry.fullPath = "/main/entry";
  primec::Definition target;
  target.fullPath = "/main/target";
  program.definitions = {entry, target};

  std::unordered_set<std::string> loweredCallTargets = {"/main/target"};
  primec::IrFunction function;
  int32_t nextLocal = 123;
  std::vector<primec::IrFunction> outFunctions;
  int resetCalls = 0;
  bool sawBuildStartAtZero = false;
  bool sawExpectValueFalse = false;
  std::string error;

  CHECK(primec::ir_lowerer::lowerCallableDefinitionOrchestration(
            program,
            program.definitions.front(),
            loweredCallTargets,
            [](const primec::Definition &) { return false; },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &info) {
              info.returnsVoid = true;
              return true;
            },
            {},
            {},
            [](const primec::Expr &) { return false; },
            [&]() { ++resetCalls; },
            [&](const primec::Definition &def, int32_t &localCursor, primec::ir_lowerer::LocalMap &locals, primec::Expr &callExpr, std::string &) {
              sawBuildStartAtZero = (localCursor == 0);
              localCursor = 1;
              locals.clear();
              callExpr = {};
              callExpr.kind = primec::Expr::Kind::Call;
              callExpr.name = def.fullPath;
              return true;
            },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool expectValue) {
              sawExpectValueFalse = !expectValue;
              function.instructions.push_back({primec::IrOpcode::PushI32, 9});
              return true;
            },
            [&](const std::string &, const primec::ir_lowerer::ReturnInfo &) {
              function.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
              return true;
            },
            function,
            nextLocal,
            outFunctions,
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(resetCalls == 1);
  CHECK(sawBuildStartAtZero);
  CHECK(sawExpectValueFalse);
  REQUIRE(outFunctions.size() == 1);
  CHECK(outFunctions.front().name == "/main/target");
  REQUIRE(outFunctions.front().instructions.size() == 2);
  CHECK(outFunctions.front().instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(outFunctions.front().instructions[1].op == primec::IrOpcode::ReturnVoid);

  outFunctions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::lowerCallableDefinitionOrchestration(
            program,
            program.definitions.front(),
            loweredCallTargets,
            [](const primec::Definition &) { return false; },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; },
            {},
            {},
            [](const primec::Expr &) { return false; },
            []() {},
            [](const primec::Definition &, int32_t &, primec::ir_lowerer::LocalMap &, primec::Expr &, std::string &) {
              return true;
            },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
              return true;
            },
            [](const std::string &, const primec::ir_lowerer::ReturnInfo &) { return true; },
            function,
            nextLocal,
            outFunctions,
            error) == Result::Error);
}

TEST_CASE("ir lowerer statement call helper prefers semantic callable inventory") {
  using Result = primec::ir_lowerer::CallableDefinitionOrchestrationResult;

  primec::Program program;
  primec::Definition entry;
  entry.fullPath = "/main/entry";
  primec::Definition skipped;
  skipped.fullPath = "/main/skipped";
  primec::Definition target;
  target.fullPath = "/main/target";
  program.definitions = {entry, skipped, target};

  primec::SemanticProgram semanticProgram;
  semanticProgram.definitions.push_back(
      {.name = "entry", .fullPath = "/main/entry", .namespacePrefix = "/main"});
  semanticProgram.definitions.push_back(
      {.name = "target", .fullPath = "/main/target", .namespacePrefix = "/main"});
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 0,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main/target"),
  });

  std::unordered_set<std::string> loweredCallTargets = {"/main/skipped", "/main/target"};
  primec::IrFunction function;
  int32_t nextLocal = 0;
  std::vector<primec::IrFunction> outFunctions;
  int buildCalls = 0;
  std::string builtPath;
  std::string error;

  CHECK(primec::ir_lowerer::lowerCallableDefinitionOrchestration(
            program,
            program.definitions.front(),
            &semanticProgram,
            loweredCallTargets,
            [](const primec::Definition &) { return false; },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &info) {
              info.returnsVoid = true;
              return true;
            },
            {},
            {},
            [](const primec::Expr &) { return false; },
            []() {},
            [&](const primec::Definition &def, int32_t &, primec::ir_lowerer::LocalMap &, primec::Expr &, std::string &) {
              ++buildCalls;
              builtPath = def.fullPath;
              return true;
            },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
              return true;
            },
            [](const std::string &, const primec::ir_lowerer::ReturnInfo &) { return true; },
            function,
            nextLocal,
            outFunctions,
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(buildCalls == 1);
  CHECK(builtPath == "/main/target");
}

TEST_CASE("ir lowerer statement call helper orchestrates entry execution cleanup") {
  using Result = primec::ir_lowerer::EntryCallableExecutionResult;

  primec::Definition entry;
  entry.fullPath = "/main/entry";
  primec::Expr stmtA;
  stmtA.kind = primec::Expr::Kind::Call;
  stmtA.name = "first";
  primec::Expr stmtB;
  stmtB.kind = primec::Expr::Kind::Call;
  stmtB.name = "second";
  entry.statements = {stmtA, stmtB};

  bool sawReturn = false;
  std::optional<primec::ir_lowerer::OnErrorHandler> currentOnError;
  currentOnError = primec::ir_lowerer::OnErrorHandler{.errorType = "FileError", .handlerPath = "/main/prev"};
  const std::optional<primec::ir_lowerer::OnErrorHandler> entryOnError =
      primec::ir_lowerer::OnErrorHandler{.errorType = "FileError", .handlerPath = "/main/handler"};
  std::optional<primec::ir_lowerer::ResultReturnInfo> currentReturnResult;
  currentReturnResult = primec::ir_lowerer::ResultReturnInfo{.isResult = false, .hasValue = false};
  const std::optional<primec::ir_lowerer::ResultReturnInfo> entryResult =
      primec::ir_lowerer::ResultReturnInfo{.isResult = true, .hasValue = true};

  int emitStatementCalls = 0;
  int pushCalls = 0;
  int cleanupCalls = 0;
  int popCalls = 0;
  std::vector<primec::IrInstruction> instructions;
  std::string error;
  CHECK(primec::ir_lowerer::emitEntryCallableExecutionWithCleanup(
            entry,
            true,
            sawReturn,
            currentOnError,
            entryOnError,
            currentReturnResult,
            entryResult,
            [&](const primec::Expr &) {
              ++emitStatementCalls;
              return true;
            },
            [&]() { ++pushCalls; },
            [&]() { ++cleanupCalls; },
            [&]() { ++popCalls; },
            instructions,
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK_FALSE(sawReturn);
  CHECK(emitStatementCalls == 2);
  CHECK(pushCalls == 1);
  CHECK(cleanupCalls == 1);
  CHECK(popCalls == 1);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions.front().op == primec::IrOpcode::ReturnVoid);
  REQUIRE(currentOnError.has_value());
  CHECK(currentOnError->handlerPath == "/main/prev");
  REQUIRE(currentReturnResult.has_value());
  CHECK_FALSE(currentReturnResult->isResult);
}

TEST_CASE("ir lowerer statement call helper validates entry execution diagnostics") {
  using Result = primec::ir_lowerer::EntryCallableExecutionResult;

  primec::Definition entry;
  entry.fullPath = "/main/entry";
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "body";
  entry.statements = {stmt};

  bool sawReturn = false;
  std::optional<primec::ir_lowerer::OnErrorHandler> currentOnError;
  std::optional<primec::ir_lowerer::ResultReturnInfo> currentReturnResult;
  std::vector<primec::IrInstruction> instructions;
  std::string error;
  CHECK(primec::ir_lowerer::emitEntryCallableExecutionWithCleanup(
            entry,
            true,
            sawReturn,
            currentOnError,
            std::nullopt,
            currentReturnResult,
            std::nullopt,
            [&](const primec::Expr &) { return false; },
            []() {},
            []() {},
            []() {},
            instructions,
            error) == Result::Error);
  CHECK(error.empty());
  CHECK(instructions.empty());

  error.clear();
  CHECK(primec::ir_lowerer::emitEntryCallableExecutionWithCleanup(
            entry,
            false,
            sawReturn,
            currentOnError,
            std::nullopt,
            currentReturnResult,
            std::nullopt,
            [&](const primec::Expr &) { return true; },
            []() {},
            []() {},
            []() {},
            instructions,
            error) == Result::Error);
  CHECK(error == "native backend requires an explicit return statement");
}

TEST_CASE("ir lowerer statement call helper finalizes function table wiring") {
  using Result = primec::ir_lowerer::FunctionTableFinalizationResult;

  primec::Program program;
  primec::Definition entry;
  entry.fullPath = "/main/entry";
  primec::Definition target;
  target.fullPath = "/main/target";
  program.definitions = {entry, target};

  primec::IrFunction entryFunction;
  entryFunction.name = "/main/entry";
  entryFunction.instructions.push_back({primec::IrOpcode::PushI32, 7});

  std::unordered_set<std::string> loweredCallTargets = {"/main/target"};
  int32_t nextLocal = 17;
  std::vector<primec::IrFunction> outFunctions;
  int32_t entryIndex = -1;
  int resetCalls = 0;
  int buildCalls = 0;
  int emitInlineCalls = 0;
  bool sawBuildStartAtZero = false;
  bool sawExpectValueFalse = false;
  std::string error;
  CHECK(primec::ir_lowerer::finalizeEntryFunctionTableAndLowerCallables(
            program,
            program.definitions.front(),
            entryFunction,
            loweredCallTargets,
            [](const primec::Definition &) { return false; },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &info) {
              info.returnsVoid = true;
              return true;
            },
            {},
            {},
            [](const primec::Expr &) { return false; },
            [&]() { ++resetCalls; },
            [&](const primec::Definition &def,
                int32_t &localCursor,
                primec::ir_lowerer::LocalMap &locals,
                primec::Expr &callExpr,
                std::string &) {
              ++buildCalls;
              sawBuildStartAtZero = (localCursor == 0);
              localCursor = 1;
              locals.clear();
              callExpr = {};
              callExpr.kind = primec::Expr::Kind::Call;
              callExpr.name = def.fullPath;
              return true;
            },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool expectValue) {
              ++emitInlineCalls;
              sawExpectValueFalse = !expectValue;
              return true;
            },
            nextLocal,
            outFunctions,
            entryIndex,
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(entryIndex == 0);
  CHECK(resetCalls == 1);
  CHECK(buildCalls == 1);
  CHECK(emitInlineCalls == 1);
  CHECK(sawBuildStartAtZero);
  CHECK(sawExpectValueFalse);
  REQUIRE(outFunctions.size() == 2);
  CHECK(outFunctions[0].name == "/main/entry");
  REQUIRE(outFunctions[0].instructions.size() == 1);
  CHECK(outFunctions[0].instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(outFunctions[1].name == "/main/target");
  REQUIRE(outFunctions[1].instructions.size() == 1);
  CHECK(outFunctions[1].instructions[0].op == primec::IrOpcode::ReturnVoid);
}

TEST_SUITE_END();
