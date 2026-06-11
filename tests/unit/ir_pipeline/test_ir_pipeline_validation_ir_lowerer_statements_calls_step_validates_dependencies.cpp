#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer statements/calls step validates dependencies") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Name;
  stmt.name = "value";

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerStatementsCallsStep(
      {
          .inferExprKind = {},
          .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return std::string();
          },
          .emitExpr = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
          .allocTempLocal = []() { return 0; },
          .resolveExprPath = [](const primec::Expr &) { return std::string(); },
          .findDefinitionByPath = [](const std::string &) { return static_cast<const primec::Definition *>(nullptr); },
          .resolveDestroyHelperForStruct =
              [](const std::string &) { return static_cast<const primec::Definition *>(nullptr); },
          .resolveMoveHelperForStruct =
              [](const std::string &) { return static_cast<const primec::Definition *>(nullptr); },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isVectorCapacityCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .resolveMethodCallDefinition =
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                return static_cast<const primec::Definition *>(nullptr);
              },
          .resolveDefinitionCall = [](const primec::Expr &) { return static_cast<const primec::Definition *>(nullptr); },
          .getReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; },
          .emitInlineDefinitionCall =
              [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
                return false;
              },
          .emitArrayIndexOutOfBounds = []() {},
          .emitVectorCapacityExceeded = []() {},
          .emitVectorPopOnEmpty = []() {},
          .emitVectorIndexOutOfBounds = []() {},
          .emitVectorReserveNegative = []() {},
          .emitVectorReserveExceeded = []() {},
          .instructions = &instructions,
      },
      stmt,
      locals,
      error));
  CHECK(error == "native backend missing statements/calls step dependency: inferExprKind");

  CHECK_FALSE(primec::ir_lowerer::runLowerStatementsCallsStep(
      {
          .inferExprKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return std::string();
          },
          .emitExpr = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
          .allocTempLocal = []() { return 0; },
          .resolveExprPath = [](const primec::Expr &) { return std::string(); },
          .findDefinitionByPath = [](const std::string &) { return static_cast<const primec::Definition *>(nullptr); },
          .resolveDestroyHelperForStruct =
              [](const std::string &) { return static_cast<const primec::Definition *>(nullptr); },
          .resolveMoveHelperForStruct =
              [](const std::string &) { return static_cast<const primec::Definition *>(nullptr); },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isVectorCapacityCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .resolveMethodCallDefinition =
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                return static_cast<const primec::Definition *>(nullptr);
              },
          .resolveDefinitionCall = [](const primec::Expr &) { return static_cast<const primec::Definition *>(nullptr); },
          .getReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; },
          .emitInlineDefinitionCall =
              [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
                return false;
              },
          .emitArrayIndexOutOfBounds = []() {},
          .emitVectorCapacityExceeded = []() {},
          .emitVectorPopOnEmpty = []() {},
          .emitVectorIndexOutOfBounds = []() {},
          .emitVectorReserveNegative = []() {},
          .emitVectorReserveExceeded = []() {},
          .instructions = nullptr,
      },
      stmt,
      locals,
      error));
  CHECK(error == "native backend missing statements/calls step dependency: instructions");
}

TEST_CASE("ir lowerer statements entry-statement step appends source ranges") {
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, 1});

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Literal;
  stmt.literalValue = 7;
  stmt.sourceLine = 42;
  stmt.sourceColumn = 8;

  int emitCalls = 0;
  int appendCalls = 0;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerStatementsEntryStatementStep(
      {
          .function = &function,
          .emitStatement = [&](const primec::Expr &entryStmt) {
            ++emitCalls;
            CHECK(entryStmt.literalValue == 7);
            function.instructions.push_back({primec::IrOpcode::PushI32, 7});
            return true;
          },
          .appendInstructionSourceRange =
              [&](const std::string &functionName, const primec::Expr &entryStmt, size_t beginIndex, size_t endIndex) {
                ++appendCalls;
                CHECK(functionName == "/main");
                CHECK(entryStmt.sourceLine == 42);
                CHECK(entryStmt.sourceColumn == 8);
                CHECK(beginIndex == 1u);
                CHECK(endIndex == 2u);
              },
      },
      stmt,
      error));
  CHECK(error.empty());
  CHECK(emitCalls == 1);
  CHECK(appendCalls == 1);
  REQUIRE(function.instructions.size() == 2);
  CHECK(function.instructions.back().op == primec::IrOpcode::PushI32);
  CHECK(function.instructions.back().imm == 7);
}

TEST_CASE("ir lowerer statements entry-statement step validates dependencies") {
  primec::IrFunction function;
  primec::Expr stmt;
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::runLowerStatementsEntryStatementStep(
      {
          .function = nullptr,
          .emitStatement = [](const primec::Expr &) { return true; },
          .appendInstructionSourceRange =
              [](const std::string &, const primec::Expr &, size_t, size_t) {},
      },
      stmt,
      error));
  CHECK(error == "native backend missing statements entry-statement step dependency: function");

  CHECK_FALSE(primec::ir_lowerer::runLowerStatementsEntryStatementStep(
      {
          .function = &function,
          .emitStatement = {},
          .appendInstructionSourceRange =
              [](const std::string &, const primec::Expr &, size_t, size_t) {},
      },
      stmt,
      error));
  CHECK(error == "native backend missing statements entry-statement step dependency: emitStatement");

  CHECK_FALSE(primec::ir_lowerer::runLowerStatementsEntryStatementStep(
      {
          .function = &function,
          .emitStatement = [](const primec::Expr &) { return true; },
          .appendInstructionSourceRange = {},
      },
      stmt,
      error));
  CHECK(error == "native backend missing statements entry-statement step dependency: appendInstructionSourceRange");
}

TEST_CASE("ir lowerer inline-call statement step appends source ranges") {
  primec::IrFunction function;
  function.name = "/inline";
  function.instructions.push_back({primec::IrOpcode::PushI32, 1});

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Literal;
  stmt.literalValue = 9;
  stmt.sourceLine = 30;
  stmt.sourceColumn = 6;

  int emitCalls = 0;
  int appendCalls = 0;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInlineCallStatementStep(
      {
          .function = &function,
          .emitStatement = [&](const primec::Expr &inlineStmt) {
            ++emitCalls;
            CHECK(inlineStmt.literalValue == 9);
            function.instructions.push_back({primec::IrOpcode::PushI32, 9});
            return true;
          },
          .appendInstructionSourceRange =
              [&](const std::string &functionName, const primec::Expr &inlineStmt, size_t beginIndex, size_t endIndex) {
                ++appendCalls;
                CHECK(functionName == "/inline");
                CHECK(inlineStmt.sourceLine == 30);
                CHECK(inlineStmt.sourceColumn == 6);
                CHECK(beginIndex == 1u);
                CHECK(endIndex == 2u);
              },
      },
      stmt,
      error));
  CHECK(error.empty());
  CHECK(emitCalls == 1);
  CHECK(appendCalls == 1);
  REQUIRE(function.instructions.size() == 2);
  CHECK(function.instructions.back().op == primec::IrOpcode::PushI32);
  CHECK(function.instructions.back().imm == 9);
}

TEST_CASE("ir lowerer inline-call statement step validates dependencies") {
  primec::IrFunction function;
  primec::Expr stmt;
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::runLowerInlineCallStatementStep(
      {
          .function = nullptr,
          .emitStatement = [](const primec::Expr &) { return true; },
          .appendInstructionSourceRange =
              [](const std::string &, const primec::Expr &, size_t, size_t) {},
      },
      stmt,
      error));
  CHECK(error == "native backend missing inline-call statement step dependency: function");

  CHECK_FALSE(primec::ir_lowerer::runLowerInlineCallStatementStep(
      {
          .function = &function,
          .emitStatement = {},
          .appendInstructionSourceRange =
              [](const std::string &, const primec::Expr &, size_t, size_t) {},
      },
      stmt,
      error));
  CHECK(error == "native backend missing inline-call statement step dependency: emitStatement");

  CHECK_FALSE(primec::ir_lowerer::runLowerInlineCallStatementStep(
      {
          .function = &function,
          .emitStatement = [](const primec::Expr &) { return true; },
          .appendInstructionSourceRange = {},
      },
      stmt,
      error));
  CHECK(error == "native backend missing inline-call statement step dependency: appendInstructionSourceRange");
}

TEST_CASE("ir lowerer inline-call cleanup step patches return jumps") {
  primec::IrFunction function;
  function.instructions = {
      {primec::IrOpcode::PushI32, 1},
      {primec::IrOpcode::Jump, 0},
      {primec::IrOpcode::Jump, 0},
  };
  const std::vector<size_t> returnJumps = {1u, 2u};

  int cleanupCalls = 0;
  int popCalls = 0;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInlineCallCleanupStep(
      {
          .function = &function,
          .returnJumps = &returnJumps,
          .emitCurrentFileScopeCleanup = [&]() {
            ++cleanupCalls;
            function.instructions.push_back({primec::IrOpcode::Pop, 0});
          },
          .popFileScope = [&]() { ++popCalls; },
      },
      error));
  CHECK(error.empty());
  CHECK(cleanupCalls == 1);
  CHECK(popCalls == 1);
  REQUIRE(function.instructions.size() == 4);
  CHECK(function.instructions[1].imm == 3u);
  CHECK(function.instructions[2].imm == 3u);
  CHECK(function.instructions[3].op == primec::IrOpcode::Pop);
}

TEST_CASE("ir lowerer inline-call cleanup step validates dependencies") {
  primec::IrFunction function;
  std::vector<size_t> returnJumps;
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::runLowerInlineCallCleanupStep(
      {
          .function = nullptr,
          .returnJumps = &returnJumps,
          .emitCurrentFileScopeCleanup = []() {},
          .popFileScope = []() {},
      },
      error));
  CHECK(error == "native backend missing inline-call cleanup step dependency: function");

  CHECK_FALSE(primec::ir_lowerer::runLowerInlineCallCleanupStep(
      {
          .function = &function,
          .returnJumps = nullptr,
          .emitCurrentFileScopeCleanup = []() {},
          .popFileScope = []() {},
      },
      error));
  CHECK(error == "native backend missing inline-call cleanup step dependency: returnJumps");

  CHECK_FALSE(primec::ir_lowerer::runLowerInlineCallCleanupStep(
      {
          .function = &function,
          .returnJumps = &returnJumps,
          .emitCurrentFileScopeCleanup = {},
          .popFileScope = []() {},
      },
      error));
  CHECK(error == "native backend missing inline-call cleanup step dependency: emitCurrentFileScopeCleanup");

  CHECK_FALSE(primec::ir_lowerer::runLowerInlineCallCleanupStep(
      {
          .function = &function,
          .returnJumps = &returnJumps,
          .emitCurrentFileScopeCleanup = []() {},
          .popFileScope = {},
      },
      error));
  CHECK(error == "native backend missing inline-call cleanup step dependency: popFileScope");
}

TEST_CASE("ir lowerer inline-call active-context step runs statements and cleanup") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  primec::Expr firstStmt;
  firstStmt.kind = primec::Expr::Kind::Literal;
  firstStmt.literalValue = 1;
  callee.statements.push_back(firstStmt);
  primec::Expr secondStmt = firstStmt;
  secondStmt.literalValue = 2;
  callee.statements.push_back(secondStmt);

  bool active = false;
  int activateCalls = 0;
  int restoreCalls = 0;
  int emitCalls = 0;
  int cleanupCalls = 0;
  std::string error;

  CHECK(primec::ir_lowerer::runLowerInlineCallActiveContextStep(
      {
          .callee = &callee,
          .structDefinition = false,
          .activateInlineContext = [&]() {
            ++activateCalls;
            active = true;
          },
          .restoreInlineContext = [&]() {
            ++restoreCalls;
            active = false;
          },
          .emitInlineStatement = [&](const primec::Expr &) {
            CHECK(active);
            ++emitCalls;
            return true;
          },
          .runInlineCleanup = [&]() {
            CHECK(active);
            ++cleanupCalls;
            return true;
          },
      },
      error));
  CHECK(error.empty());
  CHECK_FALSE(active);
  CHECK(activateCalls == 1);
  CHECK(restoreCalls == 1);
  CHECK(emitCalls == 3);
  CHECK(cleanupCalls == 1);

  int structEmitCalls = 0;
  int structCleanupCalls = 0;
  CHECK(primec::ir_lowerer::runLowerInlineCallActiveContextStep(
      {
          .callee = &callee,
          .structDefinition = true,
          .activateInlineContext = [&]() { active = true; },
          .restoreInlineContext = [&]() { active = false; },
          .emitInlineStatement = [&](const primec::Expr &) {
            ++structEmitCalls;
            return true;
          },
          .runInlineCleanup = [&]() {
            ++structCleanupCalls;
            return true;
          },
      },
      error));
  CHECK(error.empty());
  CHECK_FALSE(active);
  CHECK(structEmitCalls == 0);
  CHECK(structCleanupCalls == 1);
}

TEST_CASE("ir lowerer inline-call active-context step restores context on failure") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Literal;
  stmt.literalValue = 1;
  callee.statements.push_back(stmt);

  bool active = false;
  int restoreCalls = 0;
  int cleanupCalls = 0;
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::runLowerInlineCallActiveContextStep(
      {
          .callee = &callee,
          .structDefinition = false,
          .activateInlineContext = [&]() { active = true; },
          .restoreInlineContext = [&]() {
            ++restoreCalls;
            active = false;
          },
          .emitInlineStatement = [&](const primec::Expr &) { return false; },
          .runInlineCleanup = [&]() {
            ++cleanupCalls;
            return true;
          },
      },
      error));
  CHECK_FALSE(active);
  CHECK(restoreCalls == 1);
  CHECK(cleanupCalls == 0);
}

TEST_CASE("ir lowerer inline-call active-context step validates dependencies") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::runLowerInlineCallActiveContextStep(
      {
          .callee = nullptr,
          .structDefinition = false,
          .activateInlineContext = []() {},
          .restoreInlineContext = []() {},
          .emitInlineStatement = [](const primec::Expr &) { return true; },
          .runInlineCleanup = []() { return true; },
      },
      error));
  CHECK(error == "native backend missing inline-call active-context step dependency: callee");

  CHECK_FALSE(primec::ir_lowerer::runLowerInlineCallActiveContextStep(
      {
          .callee = &callee,
          .structDefinition = false,
          .activateInlineContext = {},
          .restoreInlineContext = []() {},
          .emitInlineStatement = [](const primec::Expr &) { return true; },
          .runInlineCleanup = []() { return true; },
      },
      error));
  CHECK(error == "native backend missing inline-call active-context step dependency: activateInlineContext");

  CHECK_FALSE(primec::ir_lowerer::runLowerInlineCallActiveContextStep(
      {
          .callee = &callee,
          .structDefinition = false,
          .activateInlineContext = []() {},
          .restoreInlineContext = {},
          .emitInlineStatement = [](const primec::Expr &) { return true; },
          .runInlineCleanup = []() { return true; },
      },
      error));
  CHECK(error == "native backend missing inline-call active-context step dependency: restoreInlineContext");

  CHECK_FALSE(primec::ir_lowerer::runLowerInlineCallActiveContextStep(
      {
          .callee = &callee,
          .structDefinition = false,
          .activateInlineContext = []() {},
          .restoreInlineContext = []() {},
          .emitInlineStatement = {},
          .runInlineCleanup = []() { return true; },
      },
      error));
  CHECK(error == "native backend missing inline-call active-context step dependency: emitInlineStatement");

  CHECK_FALSE(primec::ir_lowerer::runLowerInlineCallActiveContextStep(
      {
          .callee = &callee,
          .structDefinition = false,
          .activateInlineContext = []() {},
          .restoreInlineContext = []() {},
          .emitInlineStatement = [](const primec::Expr &) { return true; },
          .runInlineCleanup = {},
      },
      error));
  CHECK(error == "native backend missing inline-call active-context step dependency: runInlineCleanup");
}

TEST_CASE("ir lowerer inline-call gpu-locals step copies known locals") {
  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo gidX;
  gidX.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  gidX.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  gidX.index = 3;
  callerLocals.emplace(primec::ir_lowerer::kGpuGlobalIdXName, gidX);

  primec::ir_lowerer::LocalInfo gidZ = gidX;
  gidZ.index = 9;
  callerLocals.emplace(primec::ir_lowerer::kGpuGlobalIdZName, gidZ);

  primec::ir_lowerer::LocalMap calleeLocals;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInlineCallGpuLocalsStep(
      {
          .callerLocals = &callerLocals,
          .calleeLocals = &calleeLocals,
      },
      error));
  CHECK(error.empty());
  CHECK(calleeLocals.count(primec::ir_lowerer::kGpuGlobalIdXName) == 1u);
  CHECK(calleeLocals.count(primec::ir_lowerer::kGpuGlobalIdYName) == 0u);
  CHECK(calleeLocals.count(primec::ir_lowerer::kGpuGlobalIdZName) == 1u);
  CHECK(calleeLocals.at(primec::ir_lowerer::kGpuGlobalIdXName).index == 3);
  CHECK(calleeLocals.at(primec::ir_lowerer::kGpuGlobalIdZName).index == 9);
}

TEST_CASE("ir lowerer inline-call gpu-locals step validates dependencies") {
  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::runLowerInlineCallGpuLocalsStep(
      {
          .callerLocals = nullptr,
          .calleeLocals = &calleeLocals,
      },
      error));
  CHECK(error == "native backend missing inline-call gpu-locals step dependency: callerLocals");

  CHECK_FALSE(primec::ir_lowerer::runLowerInlineCallGpuLocalsStep(
      {
          .callerLocals = &callerLocals,
          .calleeLocals = nullptr,
      },
      error));
  CHECK(error == "native backend missing inline-call gpu-locals step dependency: calleeLocals");
}

TEST_CASE("ir lowerer inline-call context-setup step initializes context and zero value") {
  primec::IrFunction function;
  std::string error;

  primec::ir_lowerer::ReturnInfo floatReturnInfo;
  floatReturnInfo.returnsVoid = false;
  floatReturnInfo.returnsArray = false;
  floatReturnInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Float64;

  primec::ir_lowerer::LowerInlineCallContextSetupStepOutput floatOutput;
  CHECK(primec::ir_lowerer::runLowerInlineCallContextSetupStep(
      {
          .function = &function,
          .returnInfo = &floatReturnInfo,
          .allocTempLocal = []() { return 11; },
      },
      floatOutput,
      error));
  CHECK(error.empty());
  CHECK_FALSE(floatOutput.returnsVoid);
  CHECK_FALSE(floatOutput.returnsArray);
  CHECK(floatOutput.returnKind == primec::ir_lowerer::LocalInfo::ValueKind::Float64);
  CHECK(floatOutput.returnLocal == 11);
  REQUIRE(function.instructions.size() == 2u);
  CHECK(function.instructions[0].op == primec::IrOpcode::PushF64);
  CHECK(function.instructions[0].imm == 0u);
  CHECK(function.instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(function.instructions[1].imm == 11u);

  primec::ir_lowerer::ReturnInfo arrayReturnInfo = floatReturnInfo;
  arrayReturnInfo.returnsArray = true;
  arrayReturnInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  primec::ir_lowerer::LowerInlineCallContextSetupStepOutput arrayOutput;
  CHECK(primec::ir_lowerer::runLowerInlineCallContextSetupStep(
      {
          .function = &function,
          .returnInfo = &arrayReturnInfo,
          .allocTempLocal = []() { return 13; },
      },
      arrayOutput,
      error));
  CHECK(error.empty());
  CHECK(arrayOutput.returnLocal == 13);
  REQUIRE(function.instructions.size() == 4u);
  CHECK(function.instructions[2].op == primec::IrOpcode::PushI64);
  CHECK(function.instructions[2].imm == 0u);
  CHECK(function.instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(function.instructions[3].imm == 13u);

  primec::ir_lowerer::ReturnInfo voidReturnInfo = floatReturnInfo;
  voidReturnInfo.returnsVoid = true;
  voidReturnInfo.returnsArray = false;
  voidReturnInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  primec::ir_lowerer::LowerInlineCallContextSetupStepOutput voidOutput;
  CHECK(primec::ir_lowerer::runLowerInlineCallContextSetupStep(
      {
          .function = &function,
          .returnInfo = &voidReturnInfo,
          .allocTempLocal = {},
      },
      voidOutput,
      error));
  CHECK(error.empty());
  CHECK(voidOutput.returnsVoid);
  CHECK(voidOutput.returnLocal == -1);
  CHECK(function.instructions.size() == 4u);
}

TEST_CASE("ir lowerer inline-call context-setup step validates dependencies") {
  primec::IrFunction function;
  primec::ir_lowerer::ReturnInfo returnInfo;
  returnInfo.returnsVoid = false;
  returnInfo.returnsArray = false;
  returnInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  primec::ir_lowerer::LowerInlineCallContextSetupStepOutput output;
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::runLowerInlineCallContextSetupStep(
      {
          .function = nullptr,
          .returnInfo = &returnInfo,
          .allocTempLocal = []() { return 1; },
      },
      output,
      error));
  CHECK(error == "native backend missing inline-call context-setup step dependency: function");

  CHECK_FALSE(primec::ir_lowerer::runLowerInlineCallContextSetupStep(
      {
          .function = &function,
          .returnInfo = nullptr,
          .allocTempLocal = []() { return 1; },
      },
      output,
      error));
  CHECK(error == "native backend missing inline-call context-setup step dependency: returnInfo");

  CHECK_FALSE(primec::ir_lowerer::runLowerInlineCallContextSetupStep(
      {
          .function = &function,
          .returnInfo = &returnInfo,
          .allocTempLocal = {},
      },
      output,
      error));
  CHECK(error == "native backend missing inline-call context-setup step dependency: allocTempLocal");
}


TEST_SUITE_END();
