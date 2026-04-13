#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer statement binding helper validates print statement builtin diagnostics") {
  using EmitResult = primec::ir_lowerer::StatementPrintPathSpaceEmitResult;

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "print";
  stmt.hasBodyArguments = true;

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitPrintPathSpaceStatementBuiltin(
            stmt,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, const primec::ir_lowerer::PrintBuiltin &) {
              return true;
            },
            [](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            instructions,
            error) == EmitResult::Error);
  CHECK(error == "print does not support body arguments");

  stmt.hasBodyArguments = false;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitPrintPathSpaceStatementBuiltin(
            stmt,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, const primec::ir_lowerer::PrintBuiltin &) {
              return true;
            },
            [](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            instructions,
            error) == EmitResult::Error);
  CHECK(error == "print requires exactly one argument");
}

TEST_CASE("ir lowerer statement binding helper emits pathspace statement builtins") {
  using EmitResult = primec::ir_lowerer::StatementPrintPathSpaceEmitResult;

  primec::Expr stringArg;
  stringArg.kind = primec::Expr::Kind::StringLiteral;
  stringArg.stringValue = "\"id\"utf8";
  primec::Expr dynamicArg;
  dynamicArg.kind = primec::Expr::Kind::Name;
  dynamicArg.name = "value";

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "notify";
  stmt.args = {stringArg, dynamicArg};

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::IrInstruction> instructions;
  int emitExprCalls = 0;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitPrintPathSpaceStatementBuiltin(
            stmt,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, const primec::ir_lowerer::PrintBuiltin &) {
              return true;
            },
            [](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI32, 42});
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].op == primec::IrOpcode::Pop);

  primec::Definition def;
  error.clear();
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitPrintPathSpaceStatementBuiltin(
            stmt,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, const primec::ir_lowerer::PrintBuiltin &) {
              return true;
            },
            [&](const primec::Expr &) -> const primec::Definition * { return &def; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            instructions,
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer statement binding helper emits inline return statements") {
  using EmitResult = primec::ir_lowerer::ReturnStatementEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Literal;
  valueExpr.intWidth = 32;
  valueExpr.literalValue = 5;

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "return";
  stmt.args = {valueExpr};

  std::vector<primec::IrInstruction> instructions;
  std::vector<size_t> jumps;
  bool sawReturn = false;
  int cleanupCalls = 0;
  std::string error;
  const std::optional<primec::ir_lowerer::ReturnStatementInlineContext> inlineContext =
      primec::ir_lowerer::ReturnStatementInlineContext{false, false, ValueKind::Unknown, 9, &jumps};

  CHECK(primec::ir_lowerer::tryEmitReturnStatement(
            stmt,
            {},
            instructions,
            inlineContext,
            std::nullopt,
            false,
            sawReturn,
            [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              instructions.push_back({primec::IrOpcode::PushI32, expr.literalValue});
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::ResultExprInfo &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Unknown; },
            [&]() { ++cleanupCalls; },
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK_FALSE(sawReturn);
  CHECK(cleanupCalls == 0);
  REQUIRE(instructions.size() == 3);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 9);
  CHECK(instructions[2].op == primec::IrOpcode::Jump);
  REQUIRE(jumps.size() == 1);
  CHECK(jumps[0] == 2);
}

TEST_CASE("ir lowerer statement binding helper emits non-inline void return statements") {
  using EmitResult = primec::ir_lowerer::ReturnStatementEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "return";

  std::vector<primec::IrInstruction> instructions;
  bool sawReturn = false;
  int cleanupCalls = 0;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitReturnStatement(
            stmt,
            {},
            instructions,
            std::nullopt,
            std::nullopt,
            true,
            sawReturn,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Unknown; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::ResultExprInfo &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Unknown; },
            [&]() { ++cleanupCalls; },
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(sawReturn);
  CHECK(cleanupCalls == 1);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::ReturnVoid);
}

TEST_CASE("ir lowerer statement binding helper validates return diagnostics") {
  using EmitResult = primec::ir_lowerer::ReturnStatementEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Literal;
  valueExpr.intWidth = 32;
  valueExpr.literalValue = 1;

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "return";
  stmt.args = {valueExpr};

  std::vector<primec::IrInstruction> instructions;
  bool sawReturn = false;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitReturnStatement(
            stmt,
            {},
            instructions,
            std::nullopt,
            std::nullopt,
            true,
            sawReturn,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::ResultExprInfo &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Unknown; },
            []() {},
            error) == EmitResult::Error);
  CHECK(error == "return value not allowed for void definition");

  std::vector<size_t> jumps;
  const std::optional<primec::ir_lowerer::ReturnStatementInlineContext> inlineContext =
      primec::ir_lowerer::ReturnStatementInlineContext{false, true, ValueKind::Unknown, 3, &jumps};
  error.clear();
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitReturnStatement(
            stmt,
            {},
            instructions,
            inlineContext,
            std::nullopt,
            false,
            sawReturn,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::ResultExprInfo &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Unknown; },
            []() {},
            error) == EmitResult::Error);
  CHECK(error.find("native backend only supports returning array values") != std::string::npos);
}

TEST_CASE("ir lowerer statement binding helper emits if statements") {
  using EmitResult = primec::ir_lowerer::StatementMatchIfEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr condExpr;
  condExpr.kind = primec::Expr::Kind::Name;
  condExpr.name = "cond";

  primec::Expr thenValue;
  thenValue.kind = primec::Expr::Kind::Literal;
  thenValue.intWidth = 32;
  thenValue.literalValue = 1;
  primec::Expr elseValue;
  elseValue.kind = primec::Expr::Kind::Literal;
  elseValue.intWidth = 32;
  elseValue.literalValue = 2;

  primec::Expr thenBlock;
  thenBlock.kind = primec::Expr::Kind::Call;
  thenBlock.name = "then";
  thenBlock.hasBodyArguments = true;
  thenBlock.bodyArguments = {thenValue};
  primec::Expr elseBlock;
  elseBlock.kind = primec::Expr::Kind::Call;
  elseBlock.name = "else";
  elseBlock.hasBodyArguments = true;
  elseBlock.bodyArguments = {elseValue};

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "if";
  stmt.args = {condExpr, thenBlock, elseBlock};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo condInfo;
  condInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  condInfo.valueKind = ValueKind::Bool;
  locals.emplace("cond", condInfo);

  std::vector<primec::IrInstruction> instructions;
  int emitBlockCalls = 0;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitMatchIfStatement(
            stmt,
            locals,
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              instructions.push_back({primec::IrOpcode::PushI32, 1});
              return true;
            },
            [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              return expr.kind == primec::Expr::Kind::Name ? ValueKind::Bool : ValueKind::Int32;
            },
            [&](const primec::Expr &branchExpr, primec::ir_lowerer::LocalMap &) {
              ++emitBlockCalls;
              const uint64_t marker = branchExpr.bodyArguments.front().literalValue == 1 ? 111 : 222;
              instructions.push_back({primec::IrOpcode::PushI32, marker});
              return true;
            },
            [](const primec::Expr &, primec::ir_lowerer::LocalMap &) { return true; },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(emitBlockCalls == 2);
  REQUIRE(instructions.size() == 5);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[1].imm == 4);
  CHECK(instructions[2].op == primec::IrOpcode::PushI32);
  CHECK(instructions[2].imm == 111);
  CHECK(instructions[3].op == primec::IrOpcode::Jump);
  CHECK(instructions[3].imm == 5);
  CHECK(instructions[4].op == primec::IrOpcode::PushI32);
  CHECK(instructions[4].imm == 222);
}

TEST_CASE("ir lowerer statement binding helper lowers match via statement callback") {
  using EmitResult = primec::ir_lowerer::StatementMatchIfEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr condExpr;
  condExpr.kind = primec::Expr::Kind::Name;
  condExpr.name = "cond";

  primec::Expr thenValue;
  thenValue.kind = primec::Expr::Kind::Literal;
  thenValue.intWidth = 32;
  thenValue.literalValue = 1;
  primec::Expr elseValue;
  elseValue.kind = primec::Expr::Kind::Literal;
  elseValue.intWidth = 32;
  elseValue.literalValue = 0;

  primec::Expr thenBlock;
  thenBlock.kind = primec::Expr::Kind::Call;
  thenBlock.name = "case";
  thenBlock.hasBodyArguments = true;
  thenBlock.bodyArguments = {thenValue};
  primec::Expr elseBlock;
  elseBlock.kind = primec::Expr::Kind::Call;
  elseBlock.name = "else";
  elseBlock.hasBodyArguments = true;
  elseBlock.bodyArguments = {elseValue};

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "match";
  stmt.args = {condExpr, thenBlock, elseBlock};

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::IrInstruction> instructions;
  int emitStatementCalls = 0;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitMatchIfStatement(
            stmt,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Bool; },
            [](const primec::Expr &, primec::ir_lowerer::LocalMap &) { return true; },
            [&](const primec::Expr &loweredStmt, primec::ir_lowerer::LocalMap &) {
              ++emitStatementCalls;
              return loweredStmt.kind == primec::Expr::Kind::Call && loweredStmt.name == "if";
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(emitStatementCalls == 1);
}

TEST_CASE("ir lowerer statement binding helper validates if and match diagnostics") {
  using EmitResult = primec::ir_lowerer::StatementMatchIfEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr condExpr;
  condExpr.kind = primec::Expr::Kind::Name;
  condExpr.name = "cond";
  primec::Expr scalarExpr;
  scalarExpr.kind = primec::Expr::Kind::Literal;
  scalarExpr.intWidth = 32;
  scalarExpr.literalValue = 7;

  primec::Expr blockExpr;
  blockExpr.kind = primec::Expr::Kind::Call;
  blockExpr.name = "branch";
  blockExpr.hasBodyArguments = true;
  blockExpr.bodyArguments = {scalarExpr};

  primec::Expr ifStmt;
  ifStmt.kind = primec::Expr::Kind::Call;
  ifStmt.name = "if";
  ifStmt.args = {condExpr, blockExpr};

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitMatchIfStatement(
            ifStmt,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Bool; },
            [](const primec::Expr &, primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, primec::ir_lowerer::LocalMap &) { return true; },
            instructions,
            error) == EmitResult::Error);
  CHECK(error == "if requires condition, then, else");

  ifStmt.args = {condExpr, blockExpr, scalarExpr};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitMatchIfStatement(
            ifStmt,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Bool; },
            [](const primec::Expr &, primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, primec::ir_lowerer::LocalMap &) { return true; },
            instructions,
            error) == EmitResult::Error);
  CHECK(error == "if branches require block envelopes");

  primec::Expr matchStmt;
  matchStmt.kind = primec::Expr::Kind::Call;
  matchStmt.name = "match";
  matchStmt.args = {condExpr, blockExpr};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitMatchIfStatement(
            matchStmt,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Bool; },
            [](const primec::Expr &, primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, primec::ir_lowerer::LocalMap &) { return true; },
            instructions,
            error) == EmitResult::Error);
  CHECK(error == "match requires value, cases, else");
}

TEST_CASE("ir lowerer statement call helper emits buffer_store") {
  using EmitResult = primec::ir_lowerer::BufferStoreStatementEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr bufferExpr;
  bufferExpr.kind = primec::Expr::Kind::Name;
  bufferExpr.name = "bufferValue";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 2;

  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Literal;
  valueExpr.intWidth = 32;
  valueExpr.literalValue = 77;

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "buffer_store";
  stmt.args = {bufferExpr, indexExpr, valueExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo bufferInfo;
  bufferInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Buffer;
  bufferInfo.valueKind = ValueKind::Int32;
  locals.emplace("bufferValue", bufferInfo);

  std::vector<primec::IrInstruction> instructions;
  int nextLocal = 10;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitBufferStoreStatement(
            stmt,
            locals,
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(expr.literalValue)});
              return true;
            },
            [&]() { return nextLocal++; },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 16);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[5].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[6].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[14].op == primec::IrOpcode::StoreIndirect);
  CHECK(instructions[15].op == primec::IrOpcode::Pop);
}

TEST_CASE("ir lowerer statement call helper emits buffer_store for variadic Buffer receivers") {
  using EmitResult = primec::ir_lowerer::BufferStoreStatementEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  auto makeIndexExpr = [](int64_t value) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Literal;
    expr.intWidth = 32;
    expr.literalValue = value;
    return expr;
  };

  const primec::Expr indexExpr = makeIndexExpr(0);
  const primec::Expr valueExpr = makeIndexExpr(77);

  auto makePackAccess = [&](const std::string &packName) {
    primec::Expr packExpr;
    packExpr.kind = primec::Expr::Kind::Name;
    packExpr.name = packName;

    primec::Expr accessExpr;
    accessExpr.kind = primec::Expr::Kind::Call;
    accessExpr.name = "at";
    accessExpr.args = {packExpr, indexExpr};
    return accessExpr;
  };

  auto makeDereference = [&](primec::Expr targetExpr) {
    primec::Expr derefExpr;
    derefExpr.kind = primec::Expr::Kind::Call;
    derefExpr.name = "dereference";
    derefExpr.args = {std::move(targetExpr)};
    return derefExpr;
  };

  auto makeBufferStoreStmt = [&](primec::Expr bufferExpr) {
    primec::Expr stmt;
    stmt.kind = primec::Expr::Kind::Call;
    stmt.name = "buffer_store";
    stmt.args = {std::move(bufferExpr), indexExpr, valueExpr};
    return stmt;
  };

  primec::ir_lowerer::LocalMap locals;

  primec::ir_lowerer::LocalInfo valuePackInfo;
  valuePackInfo.isArgsPack = true;
  valuePackInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Buffer;
  valuePackInfo.valueKind = ValueKind::Int32;
  locals.emplace("buffers", valuePackInfo);

  primec::ir_lowerer::LocalInfo borrowedPackInfo;
  borrowedPackInfo.isArgsPack = true;
  borrowedPackInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  borrowedPackInfo.referenceToBuffer = true;
  borrowedPackInfo.valueKind = ValueKind::Int32;
  locals.emplace("bufferRefs", borrowedPackInfo);

  primec::ir_lowerer::LocalInfo pointerPackInfo;
  pointerPackInfo.isArgsPack = true;
  pointerPackInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerPackInfo.pointerToBuffer = true;
  pointerPackInfo.valueKind = ValueKind::Int32;
  locals.emplace("bufferPtrs", pointerPackInfo);

  auto runStore = [&](const primec::Expr &stmtExpr) {
    std::vector<primec::IrInstruction> instructions;
    int nextLocal = 20;
    std::string error;
    const auto result = primec::ir_lowerer::tryEmitBufferStoreStatement(
        stmtExpr,
        locals,
        [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
        [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          instructions.push_back({primec::IrOpcode::PushI32, 0});
          return true;
        },
        [&]() { return nextLocal++; },
        instructions,
        error);
    CHECK(result == EmitResult::Emitted);
    CHECK(error.empty());
    REQUIRE(instructions.size() == 16);
    CHECK(instructions[14].op == primec::IrOpcode::StoreIndirect);
    CHECK(instructions[15].op == primec::IrOpcode::Pop);
  };

  runStore(makeBufferStoreStmt(makePackAccess("buffers")));
  runStore(makeBufferStoreStmt(makeDereference(makePackAccess("bufferRefs"))));
  runStore(makeBufferStoreStmt(makeDereference(makePackAccess("bufferPtrs"))));
}


TEST_SUITE_END();
