#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer statement call helper validates buffer_store diagnostics") {
  using EmitResult = primec::ir_lowerer::BufferStoreStatementEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr bufferExpr;
  bufferExpr.kind = primec::Expr::Kind::Name;
  bufferExpr.name = "bufferValue";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;

  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Literal;
  valueExpr.intWidth = 32;
  valueExpr.literalValue = 2;

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "buffer_store";
  stmt.args = {bufferExpr, indexExpr};

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitBufferStoreStatement(
            stmt,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            instructions,
            error) == EmitResult::Error);
  CHECK(error == "buffer_store requires buffer, index, and value");

  stmt.args = {bufferExpr, indexExpr, valueExpr};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitBufferStoreStatement(
            stmt,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            instructions,
            error) == EmitResult::Error);
  CHECK(error == "buffer_store requires numeric/bool buffer");

  primec::ir_lowerer::LocalInfo bufferInfo;
  bufferInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Buffer;
  bufferInfo.valueKind = ValueKind::Int32;
  locals.emplace("bufferValue", bufferInfo);

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitBufferStoreStatement(
            stmt,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Float32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            instructions,
            error) == EmitResult::Error);
  CHECK(error == "buffer_store requires integer index");

  primec::SemanticProgram semanticProgram;
  primec::SemanticProgramQueryFact queryFact;
  queryFact.semanticNodeId = 7101;
  queryFact.queryTypeText = "u64";
  queryFact.bindingTypeText = "f32";
  semanticProgram.queryFacts.push_back(queryFact);
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(7101, 0);
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  stmt.args[1].semanticNodeId = 7101;
  int inferCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitBufferStoreStatement(
            stmt,
            locals,
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++inferCalls;
              return ValueKind::Float32;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            instructions,
            error,
            &semanticProgram,
            &semanticIndex) == EmitResult::Emitted);
  CHECK(inferCalls == 0);
  CHECK(error.empty());

  primec::Expr otherStmt;
  otherStmt.kind = primec::Expr::Kind::Call;
  otherStmt.name = "notify";
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitBufferStoreStatement(
            otherStmt,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            instructions,
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer buffer_store target kind uses semantic facts before stale locals") {
  using EmitResult = primec::ir_lowerer::BufferStoreStatementEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo staleStringBuffer;
  staleStringBuffer.kind = primec::ir_lowerer::LocalInfo::Kind::Buffer;
  staleStringBuffer.valueKind = ValueKind::String;
  locals.emplace("bufferValue", staleStringBuffer);

  primec::ir_lowerer::LocalInfo staleNumericBuffer;
  staleNumericBuffer.kind = primec::ir_lowerer::LocalInfo::Kind::Buffer;
  staleNumericBuffer.valueKind = ValueKind::Int32;
  locals.emplace("notBuffer", staleNumericBuffer);
  locals.emplace("wrappedBuffer", staleNumericBuffer);

  primec::SemanticProgram semanticProgram;
  auto internType = [&](const std::string &typeText) {
    return primec::semanticProgramInternCallTargetString(semanticProgram, typeText);
  };
  auto addBindingFact = [&](uint64_t semanticNodeId, std::string name, std::string typeText) {
    const std::size_t factIndex = semanticProgram.bindingFacts.size();
    semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
        .name = name,
        .bindingTypeText = "Buffer<string>",
        .semanticNodeId = semanticNodeId,
        .bindingTypeTextId = internType(typeText),
    });
    semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr[semanticNodeId] = factIndex;
  };
  auto addLocalAutoFact = [&](uint64_t semanticNodeId, std::string name, std::string typeText) {
    const std::size_t factIndex = semanticProgram.localAutoFacts.size();
    semanticProgram.localAutoFacts.push_back(primec::SemanticProgramLocalAutoFact{
        .bindingName = name,
        .bindingTypeText = "Buffer<string>",
        .semanticNodeId = semanticNodeId,
        .bindingTypeTextId = internType(typeText),
    });
    semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr[semanticNodeId] = factIndex;
  };
  auto addQueryFact = [&](uint64_t semanticNodeId, std::string typeText) {
    const std::size_t factIndex = semanticProgram.queryFacts.size();
    semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
        .callName = "makeBuffer",
        .queryTypeText = "Buffer<string>",
        .bindingTypeText = "Buffer<string>",
        .semanticNodeId = semanticNodeId,
        .queryTypeTextId = internType(typeText),
        .bindingTypeTextId = internType(typeText),
    });
    semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr[semanticNodeId] = factIndex;
  };
  addBindingFact(7201, "bufferValue", "Buffer<f32>");
  addBindingFact(7202, "notBuffer", "i32");
  addBindingFact(7205, "wrappedBuffer", "Reference<Buffer<i32>>");
  addLocalAutoFact(7203, "autoBuffer", "Reference<Buffer<i32>>");
  addQueryFact(7204, "Pointer<Buffer<u64>>");
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;

  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Literal;
  valueExpr.intWidth = 32;
  valueExpr.literalValue = 2;

  auto makeName = [](std::string name, uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };
  auto makeDereference = [](primec::Expr target) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "dereference";
    expr.args = {target};
    return expr;
  };
  auto makeQueryCall = [](uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "makeBuffer";
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };
  auto emitBufferStore = [&](primec::Expr bufferExpr, std::string &errorOut) {
    primec::Expr stmt;
    stmt.kind = primec::Expr::Kind::Call;
    stmt.name = "buffer_store";
    stmt.args = {bufferExpr, indexExpr, valueExpr};

    std::vector<primec::IrInstruction> instructions;
    int32_t nextLocal = 0;
    errorOut.clear();
    return primec::ir_lowerer::tryEmitBufferStoreStatement(
        stmt,
        locals,
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
        [&]() { return nextLocal++; },
        instructions,
        errorOut,
        &semanticProgram,
        &semanticIndex);
  };

  std::string error;
  CHECK(emitBufferStore(makeName("bufferValue", 7201), error) == EmitResult::Emitted);
  CHECK(error.empty());

  CHECK(emitBufferStore(makeDereference(makeName("autoBuffer", 7203)), error) == EmitResult::Emitted);
  CHECK(error.empty());

  CHECK(emitBufferStore(makeDereference(makeQueryCall(7204)), error) == EmitResult::Emitted);
  CHECK(error.empty());

  CHECK(emitBufferStore(makeName("notBuffer", 7202), error) == EmitResult::Error);
  CHECK(error == "buffer_store requires numeric/bool buffer");

  CHECK(emitBufferStore(makeName("wrappedBuffer", 7205), error) == EmitResult::Error);
  CHECK(error == "buffer_store requires numeric/bool buffer");

  CHECK(emitBufferStore(makeName("notBuffer", 0), error) == EmitResult::Emitted);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer statement call helper emits dispatch statements") {
  using EmitResult = primec::ir_lowerer::DispatchStatementEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr kernelName;
  kernelName.kind = primec::Expr::Kind::Name;
  kernelName.name = "Kernel";
  kernelName.namespacePrefix = "/main";

  primec::Expr gx;
  gx.kind = primec::Expr::Kind::Literal;
  gx.intWidth = 32;
  gx.literalValue = 2;
  primec::Expr gy = gx;
  gy.literalValue = 3;
  primec::Expr gz = gx;
  gz.literalValue = 4;
  primec::Expr payload = gx;
  payload.literalValue = 9;

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "dispatch";
  stmt.args = {kernelName, gx, gy, gz, payload};

  primec::Definition kernelDef;
  kernelDef.fullPath = "/main/Kernel";
  kernelDef.transforms.push_back(primec::Transform{.name = "compute"});
  kernelDef.parameters.resize(1);

  std::vector<primec::IrInstruction> instructions;
  int nextLocal = 20;
  int inlineCallCount = 0;
  primec::Expr observedCall;
  primec::ir_lowerer::LocalMap observedLocals;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitDispatchStatement(
            stmt,
            {},
            [](const primec::Expr &) { return std::string("/main/Kernel"); },
            [&](const std::string &path) -> const primec::Definition * {
              return path == "/main/Kernel" ? &kernelDef : nullptr;
            },
            [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              if (&expr == &stmt.args[1] || &expr == &stmt.args[2] || &expr == &stmt.args[3]) {
                return ValueKind::Int32;
              }
              return ValueKind::Unknown;
            },
            [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              instructions.push_back({primec::IrOpcode::PushI32, expr.literalValue});
              return true;
            },
            [&]() { return nextLocal++; },
            [&](const primec::Expr &callExpr,
                const primec::Definition &,
                const primec::ir_lowerer::LocalMap &callLocals,
                bool expectValue) {
              ++inlineCallCount;
              observedCall = callExpr;
              observedLocals = callLocals;
              CHECK_FALSE(expectValue);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCallCount == 1);
  CHECK(observedCall.name == "/main/Kernel");
  CHECK(observedCall.args.size() == 1);
  CHECK(observedLocals.count(primec::ir_lowerer::kGpuGlobalIdXName) == 1);
  CHECK(observedLocals.count(primec::ir_lowerer::kGpuGlobalIdYName) == 1);
  CHECK(observedLocals.count(primec::ir_lowerer::kGpuGlobalIdZName) == 1);
  int jumpIfZeroCount = 0;
  for (const auto &inst : instructions) {
    if (inst.op == primec::IrOpcode::JumpIfZero) {
      ++jumpIfZeroCount;
      CHECK(inst.imm > 0);
    }
  }
  CHECK(jumpIfZeroCount == 3);
}

TEST_CASE("ir lowerer dispatch dimensions use semantic facts before expression inference") {
  using EmitResult = primec::ir_lowerer::DispatchStatementEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr kernelName;
  kernelName.kind = primec::Expr::Kind::Name;
  kernelName.name = "Kernel";
  kernelName.namespacePrefix = "/main";

  auto makeDimension = [](std::string name, uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };
  auto makeQueryDimension = [](uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "size";
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };

  primec::SemanticProgram semanticProgram;
  auto internType = [&](const std::string &typeText) {
    return primec::semanticProgramInternCallTargetString(semanticProgram, typeText);
  };
  auto addBindingFact = [&](uint64_t semanticNodeId, std::string name, std::string typeText) {
    const std::size_t factIndex = semanticProgram.bindingFacts.size();
    semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
        .name = name,
        .bindingTypeText = "f32",
        .semanticNodeId = semanticNodeId,
        .bindingTypeTextId = internType(typeText),
    });
    semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr[semanticNodeId] = factIndex;
  };
  auto addLocalAutoFact = [&](uint64_t semanticNodeId, std::string name, std::string typeText) {
    const std::size_t factIndex = semanticProgram.localAutoFacts.size();
    semanticProgram.localAutoFacts.push_back(primec::SemanticProgramLocalAutoFact{
        .bindingName = name,
        .bindingTypeText = "f32",
        .semanticNodeId = semanticNodeId,
        .bindingTypeTextId = internType(typeText),
    });
    semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr[semanticNodeId] = factIndex;
  };
  auto addQueryFact = [&](uint64_t semanticNodeId, std::string typeText) {
    const std::size_t factIndex = semanticProgram.queryFacts.size();
    semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
        .callName = "size",
        .queryTypeText = "f32",
        .bindingTypeText = "f32",
        .semanticNodeId = semanticNodeId,
        .queryTypeTextId = internType(typeText),
        .bindingTypeTextId = internType(typeText),
    });
    semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr[semanticNodeId] = factIndex;
  };
  addBindingFact(7301, "gx", "i32");
  addLocalAutoFact(7302, "gy", "i32");
  addQueryFact(7303, "i32");
  addBindingFact(7304, "bad", "u64");
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::Definition kernelDef;
  kernelDef.fullPath = "/main/Kernel";
  kernelDef.transforms.push_back(primec::Transform{.name = "compute"});

  auto emitDispatch = [&](std::vector<primec::Expr> dimensions,
                          ValueKind fallbackKind,
                          int &inferCalls,
                          std::string &errorOut) {
    primec::Expr stmt;
    stmt.kind = primec::Expr::Kind::Call;
    stmt.name = "dispatch";
    stmt.args = {kernelName, dimensions[0], dimensions[1], dimensions[2]};

    std::vector<primec::IrInstruction> instructions;
    int32_t nextLocal = 0;
    inferCalls = 0;
    errorOut.clear();
    return primec::ir_lowerer::tryEmitDispatchStatement(
        stmt,
        {},
        [](const primec::Expr &) { return std::string("/main/Kernel"); },
        [&](const std::string &path) -> const primec::Definition * {
          return path == "/main/Kernel" ? &kernelDef : nullptr;
        },
        [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          ++inferCalls;
          return fallbackKind;
        },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
        [&]() { return nextLocal++; },
        [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
          return true;
        },
        instructions,
        errorOut,
        &semanticProgram,
        &semanticIndex);
  };

  int inferCalls = 0;
  std::string error;
  CHECK(emitDispatch(
            {makeDimension("gx", 7301), makeDimension("gy", 7302), makeQueryDimension(7303)},
            ValueKind::Float32,
            inferCalls,
            error) == EmitResult::Emitted);
  CHECK(inferCalls == 0);
  CHECK(error.empty());

  CHECK(emitDispatch(
            {makeDimension("gx", 7301), makeDimension("bad", 7304), makeQueryDimension(7303)},
            ValueKind::Int32,
            inferCalls,
            error) == EmitResult::Error);
  CHECK(inferCalls == 0);
  CHECK(error == "dispatch requires i32 dimensions");

  CHECK(emitDispatch(
            {makeDimension("gx", 0), makeDimension("gy", 0), makeQueryDimension(0)},
            ValueKind::Int32,
            inferCalls,
            error) == EmitResult::Emitted);
  CHECK(inferCalls == 3);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer statement call helper validates dispatch diagnostics") {
  using EmitResult = primec::ir_lowerer::DispatchStatementEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr kernelName;
  kernelName.kind = primec::Expr::Kind::Name;
  kernelName.name = "Kernel";

  primec::Expr dim;
  dim.kind = primec::Expr::Kind::Literal;
  dim.intWidth = 32;
  dim.literalValue = 1;

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "dispatch";
  stmt.args = {kernelName, dim, dim};

  primec::Definition kernelDef;
  kernelDef.fullPath = "/main/Kernel";
  kernelDef.parameters.resize(1);

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitDispatchStatement(
            stmt,
            {},
            [](const primec::Expr &) { return std::string("/main/Kernel"); },
            [&](const std::string &) -> const primec::Definition * { return &kernelDef; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
              return true;
            },
            instructions,
            error) == EmitResult::Error);
  CHECK(error == "dispatch requires kernel and three dimension arguments");

  primec::Expr badKernelArg;
  badKernelArg.kind = primec::Expr::Kind::Literal;
  badKernelArg.intWidth = 32;
  badKernelArg.literalValue = 0;
  stmt.args = {badKernelArg, dim, dim, dim};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDispatchStatement(
            stmt,
            {},
            [](const primec::Expr &) { return std::string("/main/Kernel"); },
            [&](const std::string &) -> const primec::Definition * { return &kernelDef; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
              return true;
            },
            instructions,
            error) == EmitResult::Error);
  CHECK(error == "dispatch requires kernel name as first argument");

  stmt.args = {kernelName, dim, dim, dim, dim};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDispatchStatement(
            stmt,
            {},
            [](const primec::Expr &) { return std::string("/main/MissingKernel"); },
            [](const std::string &) -> const primec::Definition * { return nullptr; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
              return true;
            },
            instructions,
            error) == EmitResult::Error);
  CHECK(error == "dispatch requires known kernel: /main/MissingKernel");

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDispatchStatement(
            stmt,
            {},
            [](const primec::Expr &) { return std::string("/main/Kernel"); },
            [&](const std::string &) -> const primec::Definition * { return &kernelDef; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
              return true;
            },
            instructions,
            error) == EmitResult::Error);
  CHECK(error == "dispatch requires compute definition: /main/Kernel");

  kernelDef.transforms.push_back(primec::Transform{.name = "compute"});
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDispatchStatement(
            stmt,
            {},
            [](const primec::Expr &) { return std::string("/main/Kernel"); },
            [&](const std::string &) -> const primec::Definition * { return &kernelDef; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Float32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
              return true;
            },
            instructions,
            error) == EmitResult::Error);
  CHECK(error == "dispatch requires i32 dimensions");

  kernelDef.parameters.resize(2);
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDispatchStatement(
            stmt,
            {},
            [](const primec::Expr &) { return std::string("/main/Kernel"); },
            [&](const std::string &) -> const primec::Definition * { return &kernelDef; },
            [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              if (&expr == &stmt.args[1] || &expr == &stmt.args[2] || &expr == &stmt.args[3]) {
                return ValueKind::Int32;
              }
              return ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
              return true;
            },
            instructions,
            error) == EmitResult::Error);
  CHECK(error == "dispatch argument count mismatch for /main/Kernel");

  primec::Expr otherStmt;
  otherStmt.kind = primec::Expr::Kind::Call;
  otherStmt.name = "notify";
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDispatchStatement(
            otherStmt,
            {},
            [](const primec::Expr &) { return std::string("/main/Kernel"); },
            [&](const std::string &) -> const primec::Definition * { return &kernelDef; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
              return true;
            },
            instructions,
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer map insert rewrite uses semantic receiver facts before stale locals") {
  using EmitResult = primec::ir_lowerer::DirectCallStatementEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr keyArg;
  keyArg.kind = primec::Expr::Kind::Literal;
  keyArg.literalValue = 1;
  keyArg.intWidth = 32;
  primec::Expr valueArg;
  valueArg.kind = primec::Expr::Kind::Literal;
  valueArg.literalValue = 4;
  valueArg.intWidth = 32;

  auto makeReceiver = [](std::string name, uint64_t semanticNodeId) {
    primec::Expr receiver;
    receiver.kind = primec::Expr::Kind::Name;
    receiver.name = name;
    receiver.semanticNodeId = semanticNodeId;
    return receiver;
  };

  auto makeMapInsertStmt = [&](const primec::Expr &receiver) {
    primec::Expr stmt;
    stmt.kind = primec::Expr::Kind::Call;
    stmt.name = "/std/collections/map/insert";
    stmt.args = {receiver, keyArg, valueArg};
    stmt.argNames = {std::nullopt, std::nullopt, std::nullopt};
    return stmt;
  };

  primec::SemanticProgram semanticProgram;
  auto internType = [&](const std::string &typeText) {
    return primec::semanticProgramInternCallTargetString(semanticProgram, typeText);
  };
  auto addBindingFact = [&](uint64_t semanticNodeId, std::string name, std::string typeText) {
    const std::size_t factIndex = semanticProgram.bindingFacts.size();
    semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
        .name = name,
        .bindingTypeText = "map<i64, i64>",
        .semanticNodeId = semanticNodeId,
        .bindingTypeTextId = internType(typeText),
    });
    semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr[semanticNodeId] = factIndex;
  };
  auto addLocalAutoFact = [&](uint64_t semanticNodeId, std::string name, std::string typeText) {
    const std::size_t factIndex = semanticProgram.localAutoFacts.size();
    semanticProgram.localAutoFacts.push_back(primec::SemanticProgramLocalAutoFact{
        .bindingName = name,
        .bindingTypeText = "map<i64, i64>",
        .semanticNodeId = semanticNodeId,
        .bindingTypeTextId = internType(typeText),
    });
    semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr[semanticNodeId] = factIndex;
  };
  auto addQueryFact = [&](uint64_t semanticNodeId, std::string typeText) {
    const std::size_t factIndex = semanticProgram.queryFacts.size();
    semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
        .callName = "values",
        .queryTypeText = "map<i64, i64>",
        .bindingTypeText = "map<i64, i64>",
        .semanticNodeId = semanticNodeId,
        .queryTypeTextId = internType(typeText),
        .bindingTypeTextId = internType(typeText),
    });
    semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr[semanticNodeId] = factIndex;
  };
  addBindingFact(7401, "bindingValues", "map<i32, i32>");
  addLocalAutoFact(7402, "autoValues", "map<i32, i32>");
  addQueryFact(7403, "map<i32, i32>");
  addBindingFact(7404, "notAMap", "i32");
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LocalMap staleLocals;
  auto addStaleLocalMap = [&](const std::string &name) {
    primec::ir_lowerer::LocalInfo local;
    local.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
    local.mapKeyKind = ValueKind::Int64;
    local.mapValueKind = ValueKind::Int64;
    local.valueKind = ValueKind::Int64;
    local.index = 7;
    staleLocals[name] = local;
  };
  addStaleLocalMap("bindingValues");
  addStaleLocalMap("autoValues");
  addStaleLocalMap("queryValues");
  addStaleLocalMap("notAMap");

  primec::Definition mapInsertBuiltinDef;
  mapInsertBuiltinDef.fullPath = "/std/collections/map/insert_builtin";

  auto emitMapInsert = [&](const primec::Expr &stmt,
                           std::vector<std::string> &templateArgsOut,
                           int &inlineCalls,
                           std::string &errorOut) {
    std::vector<primec::IrInstruction> instructions;
    inlineCalls = 0;
    templateArgsOut.clear();
    errorOut.clear();
    return primec::ir_lowerer::tryEmitDirectCallStatement(
        stmt,
        staleLocals,
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
          return nullptr;
        },
        [&](const primec::Expr &callExpr) -> const primec::Definition * {
          return callExpr.name == "/std/collections/map/insert_builtin" ? &mapInsertBuiltinDef : nullptr;
        },
        [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
          if (path == "/std/collections/map/insert_builtin") {
            info.returnsVoid = true;
            return true;
          }
          return false;
        },
        [&](const primec::Expr &callExpr,
            const primec::Definition &callee,
            const primec::ir_lowerer::LocalMap &,
            bool expectValue) {
          ++inlineCalls;
          CHECK(callExpr.name == "/std/collections/map/insert_builtin");
          CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
          CHECK_FALSE(expectValue);
          templateArgsOut = callExpr.templateArgs;
          return true;
        },
        instructions,
        errorOut,
        &semanticProgram,
        &semanticIndex);
  };

  std::vector<std::string> templateArgs;
  const std::vector<std::string> expectedMapTypes = {"i32", "i32"};
  int inlineCalls = 0;
  std::string error;
  CHECK(emitMapInsert(makeMapInsertStmt(makeReceiver("bindingValues", 7401)),
                      templateArgs,
                      inlineCalls,
                      error) == EmitResult::Emitted);
  CHECK(templateArgs == expectedMapTypes);
  CHECK(inlineCalls == 1);
  CHECK(error.empty());

  CHECK(emitMapInsert(makeMapInsertStmt(makeReceiver("autoValues", 7402)),
                      templateArgs,
                      inlineCalls,
                      error) == EmitResult::Emitted);
  CHECK(templateArgs == expectedMapTypes);
  CHECK(inlineCalls == 1);
  CHECK(error.empty());

  CHECK(emitMapInsert(makeMapInsertStmt(makeReceiver("queryValues", 7403)),
                      templateArgs,
                      inlineCalls,
                      error) == EmitResult::Emitted);
  CHECK(templateArgs == expectedMapTypes);
  CHECK(inlineCalls == 1);
  CHECK(error.empty());

  CHECK(emitMapInsert(makeMapInsertStmt(makeReceiver("notAMap", 7404)),
                      templateArgs,
                      inlineCalls,
                      error) == EmitResult::NotMatched);
  CHECK(templateArgs.empty());
  CHECK(inlineCalls == 0);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer vector mutator rewrite uses semantic receiver facts before stale locals") {
  using EmitResult = primec::ir_lowerer::DirectCallStatementEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr valueArg;
  valueArg.kind = primec::Expr::Kind::Literal;
  valueArg.literalValue = 4;
  valueArg.intWidth = 32;

  auto makeReceiver = [](std::string name, uint64_t semanticNodeId) {
    primec::Expr receiver;
    receiver.kind = primec::Expr::Kind::Name;
    receiver.name = name;
    receiver.semanticNodeId = semanticNodeId;
    return receiver;
  };

  auto makePushMethodStmt = [&](const primec::Expr &receiver) {
    primec::Expr stmt;
    stmt.kind = primec::Expr::Kind::Call;
    stmt.name = "push";
    stmt.isMethodCall = true;
    stmt.args = {receiver, valueArg};
    stmt.argNames = {std::nullopt, std::nullopt};
    return stmt;
  };

  auto makeExplicitPushStmt = [&](const primec::Expr &receiver) {
    primec::Expr stmt;
    stmt.kind = primec::Expr::Kind::Call;
    stmt.name = "/std/collections/vector/push";
    stmt.args = {receiver, valueArg};
    stmt.argNames = {std::nullopt, std::nullopt};
    return stmt;
  };

  primec::SemanticProgram semanticProgram;
  auto internType = [&](const std::string &typeText) {
    return primec::semanticProgramInternCallTargetString(semanticProgram, typeText);
  };
  auto addBindingFact = [&](uint64_t semanticNodeId, std::string name, std::string typeText) {
    const std::size_t factIndex = semanticProgram.bindingFacts.size();
    semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
        .name = name,
        .bindingTypeText = "i32",
        .semanticNodeId = semanticNodeId,
        .bindingTypeTextId = internType(typeText),
    });
    semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr[semanticNodeId] = factIndex;
  };
  auto addLocalAutoFact = [&](uint64_t semanticNodeId, std::string name, std::string typeText) {
    const std::size_t factIndex = semanticProgram.localAutoFacts.size();
    semanticProgram.localAutoFacts.push_back(primec::SemanticProgramLocalAutoFact{
        .bindingName = name,
        .bindingTypeText = "i32",
        .semanticNodeId = semanticNodeId,
        .bindingTypeTextId = internType(typeText),
    });
    semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr[semanticNodeId] = factIndex;
  };
  auto addQueryFact = [&](uint64_t semanticNodeId, std::string typeText) {
    const std::size_t factIndex = semanticProgram.queryFacts.size();
    semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
        .callName = "values",
        .queryTypeText = "i32",
        .bindingTypeText = "i32",
        .semanticNodeId = semanticNodeId,
        .queryTypeTextId = internType(typeText),
        .bindingTypeTextId = internType(typeText),
    });
    semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr[semanticNodeId] = factIndex;
  };
  addBindingFact(7501, "bindingValues", "vector<i32>");
  addLocalAutoFact(7502, "autoValues", "vector<i32>");
  addQueryFact(7503, "vector<i32>");
  addBindingFact(7504, "notAVector", "i32");
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LocalMap staleLocals;
  auto addStaleLocalVector = [&](const std::string &name) {
    primec::ir_lowerer::LocalInfo local;
    local.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
    local.valueKind = ValueKind::Int64;
    local.index = 9;
    staleLocals[name] = local;
  };
  addStaleLocalVector("bindingValues");
  addStaleLocalVector("autoValues");
  addStaleLocalVector("queryValues");
  addStaleLocalVector("notAVector");

  primec::Definition fallbackPushDef;
  fallbackPushDef.fullPath = "/main/Vector.push";

  auto emitVectorPush = [&](const primec::Expr &stmt,
                            std::string &forwardedName,
                            bool &forwardedWasMethod,
                            int &inlineCalls,
                            std::string &errorOut) {
    std::vector<primec::IrInstruction> instructions;
    forwardedName.clear();
    forwardedWasMethod = false;
    inlineCalls = 0;
    errorOut.clear();
    return primec::ir_lowerer::tryEmitDirectCallStatement(
        stmt,
        staleLocals,
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [&](const primec::Expr &forwardedExpr, const primec::ir_lowerer::LocalMap &) {
          forwardedName = forwardedExpr.name;
          forwardedWasMethod = forwardedExpr.isMethodCall;
          return true;
        },
        [&](const primec::Expr &callExpr,
            const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
          return callExpr.isMethodCall && callExpr.name == "push" ? &fallbackPushDef : nullptr;
        },
        [](const primec::Expr &) -> const primec::Definition * { return nullptr; },
        [](const std::string &, primec::ir_lowerer::ReturnInfo &info) {
          info.returnsVoid = true;
          return true;
        },
        [&](const primec::Expr &callExpr,
            const primec::Definition &callee,
            const primec::ir_lowerer::LocalMap &,
            bool expectValue) {
          ++inlineCalls;
          CHECK(callExpr.isMethodCall);
          CHECK(callExpr.name == "push");
          CHECK(callee.fullPath == "/main/Vector.push");
          CHECK_FALSE(expectValue);
          return true;
        },
        instructions,
        errorOut,
        &semanticProgram,
        &semanticIndex);
  };

  std::string forwardedName;
  bool forwardedWasMethod = true;
  int inlineCalls = 0;
  std::string error;
  CHECK(emitVectorPush(makePushMethodStmt(makeReceiver("bindingValues", 7501)),
                       forwardedName,
                       forwardedWasMethod,
                       inlineCalls,
                       error) == EmitResult::Emitted);
  CHECK(forwardedName == "push");
  CHECK_FALSE(forwardedWasMethod);
  CHECK(inlineCalls == 0);
  CHECK(error.empty());

  CHECK(emitVectorPush(makePushMethodStmt(makeReceiver("autoValues", 7502)),
                       forwardedName,
                       forwardedWasMethod,
                       inlineCalls,
                       error) == EmitResult::Emitted);
  CHECK(forwardedName == "push");
  CHECK_FALSE(forwardedWasMethod);
  CHECK(inlineCalls == 0);
  CHECK(error.empty());

  CHECK(emitVectorPush(makeExplicitPushStmt(makeReceiver("queryValues", 7503)),
                       forwardedName,
                       forwardedWasMethod,
                       inlineCalls,
                       error) == EmitResult::Emitted);
  CHECK(forwardedName == "push");
  CHECK_FALSE(forwardedWasMethod);
  CHECK(inlineCalls == 0);
  CHECK(error.empty());

  CHECK(emitVectorPush(makePushMethodStmt(makeReceiver("notAVector", 7504)),
                       forwardedName,
                       forwardedWasMethod,
                       inlineCalls,
                       error) == EmitResult::Emitted);
  CHECK(forwardedName.empty());
  CHECK(inlineCalls == 1);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer experimental vector setters use semantic receiver facts before stale locals") {
  using EmitResult = primec::ir_lowerer::DirectCallStatementEmitResult;

  primec::Expr valueArg;
  valueArg.kind = primec::Expr::Kind::Literal;
  valueArg.literalValue = 2;
  valueArg.intWidth = 32;

  auto makeReceiver = [](std::string name, uint64_t semanticNodeId) {
    primec::Expr receiver;
    receiver.kind = primec::Expr::Kind::Name;
    receiver.name = name;
    receiver.semanticNodeId = semanticNodeId;
    return receiver;
  };

  auto makeSetCountStmt = [&](const primec::Expr &receiver) {
    primec::Expr stmt;
    stmt.kind = primec::Expr::Kind::Call;
    stmt.name = "set_field_count";
    stmt.isMethodCall = true;
    stmt.args = {receiver, valueArg};
    stmt.argNames = {std::nullopt, std::nullopt};
    return stmt;
  };

  primec::SemanticProgram semanticProgram;
  auto internType = [&](const std::string &typeText) {
    return primec::semanticProgramInternCallTargetString(semanticProgram, typeText);
  };
  auto addCollectionFact =
      [&](uint64_t semanticNodeId, std::string name, std::string typeText) {
    const std::size_t factIndex = semanticProgram.collectionSpecializations.size();
    semanticProgram.collectionSpecializations.push_back(primec::SemanticProgramCollectionSpecialization{
        .name = name,
        .collectionFamily = "vector",
        .bindingTypeText = "vector<i32>",
        .elementTypeText = "i32",
        .semanticNodeId = semanticNodeId,
        .collectionFamilyId = internType("vector"),
        .bindingTypeTextId = internType(typeText),
        .elementTypeTextId = internType("i32"),
    });
    semanticProgram.publishedRoutingLookups
        .collectionSpecializationIndicesByExpr[semanticNodeId] = factIndex;
  };
  auto addBindingFact = [&](uint64_t semanticNodeId,
                            std::string name,
                            std::string staleTypeText,
                            std::string typeText) {
    const std::size_t factIndex = semanticProgram.bindingFacts.size();
    semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
        .name = name,
        .bindingTypeText = staleTypeText,
        .semanticNodeId = semanticNodeId,
        .bindingTypeTextId = internType(typeText),
    });
    semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr[semanticNodeId] = factIndex;
  };
  auto addLocalAutoFact = [&](uint64_t semanticNodeId, std::string name, std::string typeText) {
    const std::size_t factIndex = semanticProgram.localAutoFacts.size();
    semanticProgram.localAutoFacts.push_back(primec::SemanticProgramLocalAutoFact{
        .bindingName = name,
        .bindingTypeText = "i32",
        .semanticNodeId = semanticNodeId,
        .bindingTypeTextId = internType(typeText),
    });
    semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr[semanticNodeId] = factIndex;
  };
  auto addQueryFact = [&](uint64_t semanticNodeId, std::string typeText) {
    const std::size_t factIndex = semanticProgram.queryFacts.size();
    semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
        .callName = "values",
        .queryTypeText = "i32",
        .bindingTypeText = "i32",
        .semanticNodeId = semanticNodeId,
        .queryTypeTextId = internType(typeText),
        .bindingTypeTextId = internType(typeText),
    });
    semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr[semanticNodeId] = factIndex;
  };

  const std::string experimentalVectorType =
      "/std/collections/experimental_vector/Vector__t25a78a513414c3bf";
  addCollectionFact(7701, "collectionValues", experimentalVectorType);
  addBindingFact(7702, "bindingValues", "i32", experimentalVectorType);
  addLocalAutoFact(7703, "autoValues", "Reference<" + experimentalVectorType + ">");
  addQueryFact(7704, "Pointer<" + experimentalVectorType + ">");
  addBindingFact(7705, "notAVector", experimentalVectorType, "i32");
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LocalMap locals;
  auto addLocal = [&](const std::string &name, std::string structTypeName) {
    primec::ir_lowerer::LocalInfo local;
    local.structTypeName = structTypeName;
    local.index = 13;
    locals[name] = local;
  };
  addLocal("collectionValues", "/main/NotVector");
  addLocal("bindingValues", "/main/NotVector");
  addLocal("autoValues", "/main/NotVector");
  addLocal("queryValues", "/main/NotVector");
  addLocal("notAVector", experimentalVectorType);

  primec::Definition fallbackSetterDef;
  fallbackSetterDef.fullPath = "/main/Vector.set_field_count";

  auto emitSetter = [&](const primec::Expr &stmt,
                        int &inlineCalls,
                        std::vector<primec::IrInstruction> &instructions,
                        std::string &errorOut) {
    instructions.clear();
    inlineCalls = 0;
    errorOut.clear();
    return primec::ir_lowerer::tryEmitDirectCallStatement(
        stmt,
        locals,
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
        [&](const primec::Expr &callExpr,
            const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
          return callExpr.isMethodCall && callExpr.name == "set_field_count"
                     ? &fallbackSetterDef
                     : nullptr;
        },
        [](const primec::Expr &) -> const primec::Definition * { return nullptr; },
        [](const std::string &, primec::ir_lowerer::ReturnInfo &info) {
          info.returnsVoid = true;
          return true;
        },
        [&](const primec::Expr &callExpr,
            const primec::Definition &callee,
            const primec::ir_lowerer::LocalMap &,
            bool expectValue) {
          ++inlineCalls;
          CHECK(callExpr.isMethodCall);
          CHECK(callExpr.name == "set_field_count");
          CHECK(callee.fullPath == "/main/Vector.set_field_count");
          CHECK_FALSE(expectValue);
          return true;
        },
        instructions,
        errorOut,
        &semanticProgram,
        &semanticIndex);
  };

  auto hasHeaderStore = [](const std::vector<primec::IrInstruction> &instructions) {
    return std::any_of(instructions.begin(), instructions.end(), [](const auto &instruction) {
      return instruction.op == primec::IrOpcode::StoreIndirect;
    });
  };

  int inlineCalls = 0;
  std::vector<primec::IrInstruction> instructions;
  std::string error;
  CHECK(emitSetter(makeSetCountStmt(makeReceiver("collectionValues", 7701)),
                   inlineCalls,
                   instructions,
                   error) == EmitResult::Emitted);
  CHECK(inlineCalls == 0);
  CHECK(hasHeaderStore(instructions));
  CHECK(error.empty());

  CHECK(emitSetter(makeSetCountStmt(makeReceiver("bindingValues", 7702)),
                   inlineCalls,
                   instructions,
                   error) == EmitResult::Emitted);
  CHECK(inlineCalls == 0);
  CHECK(hasHeaderStore(instructions));
  CHECK(error.empty());

  CHECK(emitSetter(makeSetCountStmt(makeReceiver("autoValues", 7703)),
                   inlineCalls,
                   instructions,
                   error) == EmitResult::Emitted);
  CHECK(inlineCalls == 0);
  CHECK(hasHeaderStore(instructions));
  CHECK(error.empty());

  CHECK(emitSetter(makeSetCountStmt(makeReceiver("queryValues", 7704)),
                   inlineCalls,
                   instructions,
                   error) == EmitResult::Emitted);
  CHECK(inlineCalls == 0);
  CHECK(hasHeaderStore(instructions));
  CHECK(error.empty());

  CHECK(emitSetter(makeSetCountStmt(makeReceiver("notAVector", 7705)),
                   inlineCalls,
                   instructions,
                   error) == EmitResult::Emitted);
  CHECK(inlineCalls == 1);
  CHECK_FALSE(hasHeaderStore(instructions));
  CHECK(error.empty());
}

TEST_CASE("ir lowerer SoA helper dispatch uses semantic receiver facts before stale locals") {
  using EmitResult = primec::ir_lowerer::DirectCallStatementEmitResult;

  auto makeReceiver = [](std::string name, uint64_t semanticNodeId) {
    primec::Expr receiver;
    receiver.kind = primec::Expr::Kind::Name;
    receiver.name = name;
    receiver.semanticNodeId = semanticNodeId;
    return receiver;
  };

  auto makeCountStmt = [](const primec::Expr &receiver) {
    primec::Expr stmt;
    stmt.kind = primec::Expr::Kind::Call;
    stmt.name = "count";
    stmt.args = {receiver};
    stmt.argNames = {std::nullopt};
    return stmt;
  };

  primec::SemanticProgram semanticProgram;
  auto internType = [&](const std::string &typeText) {
    return primec::semanticProgramInternCallTargetString(semanticProgram, typeText);
  };
  auto addCollectionFact =
      [&](uint64_t semanticNodeId, std::string name, std::string collectionFamily) {
    const std::size_t factIndex = semanticProgram.collectionSpecializations.size();
    semanticProgram.collectionSpecializations.push_back(primec::SemanticProgramCollectionSpecialization{
        .name = name,
        .collectionFamily = "vector",
        .bindingTypeText = "vector<i32>",
        .elementTypeText = "Particle",
        .semanticNodeId = semanticNodeId,
        .collectionFamilyId = internType(collectionFamily),
        .bindingTypeTextId = internType("soa_vector<Particle>"),
        .elementTypeTextId = internType("Particle"),
    });
    semanticProgram.publishedRoutingLookups
        .collectionSpecializationIndicesByExpr[semanticNodeId] = factIndex;
  };
  auto addBindingFact = [&](uint64_t semanticNodeId, std::string name, std::string typeText) {
    const std::size_t factIndex = semanticProgram.bindingFacts.size();
    semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
        .name = name,
        .bindingTypeText = "i32",
        .semanticNodeId = semanticNodeId,
        .bindingTypeTextId = internType(typeText),
    });
    semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr[semanticNodeId] = factIndex;
  };
  auto addLocalAutoFact = [&](uint64_t semanticNodeId, std::string name, std::string typeText) {
    const std::size_t factIndex = semanticProgram.localAutoFacts.size();
    semanticProgram.localAutoFacts.push_back(primec::SemanticProgramLocalAutoFact{
        .bindingName = name,
        .bindingTypeText = "i32",
        .semanticNodeId = semanticNodeId,
        .bindingTypeTextId = internType(typeText),
    });
    semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr[semanticNodeId] = factIndex;
  };
  auto addQueryFact = [&](uint64_t semanticNodeId, std::string typeText) {
    const std::size_t factIndex = semanticProgram.queryFacts.size();
    semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
        .callName = "values",
        .queryTypeText = "i32",
        .bindingTypeText = "i32",
        .semanticNodeId = semanticNodeId,
        .queryTypeTextId = internType(typeText),
        .bindingTypeTextId = internType(typeText),
    });
    semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr[semanticNodeId] = factIndex;
  };
  addCollectionFact(7601, "collectionValues", "soa_vector");
  addBindingFact(7602, "bindingValues", "Reference<soa_vector<Particle>>");
  addLocalAutoFact(7603, "autoValues", "Pointer<soa_vector<Particle>>");
  addQueryFact(7604, "/std/collections/experimental_soa_vector/SoaVector__t8Particle");
  addBindingFact(7605, "notASoa", "i32");
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LocalMap staleLocals;
  auto addStaleLocalSoa = [&](const std::string &name) {
    primec::ir_lowerer::LocalInfo local;
    local.isSoaVector = true;
    local.index = 11;
    staleLocals[name] = local;
  };
  addStaleLocalSoa("collectionValues");
  addStaleLocalSoa("bindingValues");
  addStaleLocalSoa("autoValues");
  addStaleLocalSoa("queryValues");
  addStaleLocalSoa("notASoa");

  primec::Definition countDef;
  countDef.fullPath = "/std/collections/soa_vector/count";

  auto emitSoaCount = [&](const primec::Expr &stmt,
                          bool &forwardedWasMethod,
                          int &inlineCalls,
                          std::string &errorOut) {
    std::vector<primec::IrInstruction> instructions;
    forwardedWasMethod = false;
    inlineCalls = 0;
    errorOut.clear();
    return primec::ir_lowerer::tryEmitDirectCallStatement(
        stmt,
        staleLocals,
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
        [&](const primec::Expr &callExpr,
            const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
          forwardedWasMethod = callExpr.isMethodCall;
          return callExpr.isMethodCall && callExpr.name == "count" ? &countDef : nullptr;
        },
        [](const primec::Expr &) -> const primec::Definition * { return nullptr; },
        [](const std::string &, primec::ir_lowerer::ReturnInfo &info) {
          info.returnsVoid = true;
          return true;
        },
        [&](const primec::Expr &callExpr,
            const primec::Definition &callee,
            const primec::ir_lowerer::LocalMap &,
            bool expectValue) {
          ++inlineCalls;
          CHECK(callExpr.isMethodCall);
          CHECK(callExpr.name == "count");
          CHECK(callee.fullPath == "/std/collections/soa_vector/count");
          CHECK_FALSE(expectValue);
          return true;
        },
        instructions,
        errorOut,
        &semanticProgram,
        &semanticIndex);
  };

  bool forwardedWasMethod = false;
  int inlineCalls = 0;
  std::string error;
  CHECK(emitSoaCount(makeCountStmt(makeReceiver("collectionValues", 7601)),
                     forwardedWasMethod,
                     inlineCalls,
                     error) == EmitResult::Emitted);
  CHECK(forwardedWasMethod);
  CHECK(inlineCalls == 1);
  CHECK(error.empty());

  CHECK(emitSoaCount(makeCountStmt(makeReceiver("bindingValues", 7602)),
                     forwardedWasMethod,
                     inlineCalls,
                     error) == EmitResult::Emitted);
  CHECK(forwardedWasMethod);
  CHECK(inlineCalls == 1);
  CHECK(error.empty());

  CHECK(emitSoaCount(makeCountStmt(makeReceiver("autoValues", 7603)),
                     forwardedWasMethod,
                     inlineCalls,
                     error) == EmitResult::Emitted);
  CHECK(forwardedWasMethod);
  CHECK(inlineCalls == 1);
  CHECK(error.empty());

  CHECK(emitSoaCount(makeCountStmt(makeReceiver("queryValues", 7604)),
                     forwardedWasMethod,
                     inlineCalls,
                     error) == EmitResult::Emitted);
  CHECK(forwardedWasMethod);
  CHECK(inlineCalls == 1);
  CHECK(error.empty());

  CHECK(emitSoaCount(makeCountStmt(makeReceiver("notASoa", 7605)),
                     forwardedWasMethod,
                     inlineCalls,
                     error) == EmitResult::NotMatched);
  CHECK_FALSE(forwardedWasMethod);
  CHECK(inlineCalls == 0);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer statement call helper emits direct calls") {
  using EmitResult = primec::ir_lowerer::DirectCallStatementEmitResult;

  primec::Expr methodStmt;
  methodStmt.kind = primec::Expr::Kind::Call;
  methodStmt.name = "write";
  methodStmt.isMethodCall = true;

  primec::Definition methodDef;
  methodDef.fullPath = "/main/Writer.write";

  std::vector<primec::IrInstruction> instructions;
  int inlineCalls = 0;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            methodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return &methodDef;
            },
            [](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return true; },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool expectValue) {
              ++inlineCalls;
              CHECK_FALSE(expectValue);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr defStmt;
  defStmt.kind = primec::Expr::Kind::Call;
  defStmt.name = "/main/f";

  primec::Definition def;
  def.fullPath = "/main/f";
  inlineCalls = 0;
  instructions.clear();
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
            [](const std::string &, primec::ir_lowerer::ReturnInfo &info) {
              info.returnsVoid = false;
              return true;
            },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool expectValue) {
              ++inlineCalls;
              CHECK_FALSE(expectValue);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions.front().op == primec::IrOpcode::Pop);

  primec::Expr valuesFactoryCall;
  valuesFactoryCall.kind = primec::Expr::Kind::Call;
  valuesFactoryCall.name = "/main/makeValues";

  primec::Expr keyArg;
  keyArg.kind = primec::Expr::Kind::Literal;
  keyArg.literalValue = 1;
  keyArg.intWidth = 32;
  primec::Expr valueArg;
  valueArg.kind = primec::Expr::Kind::Literal;
  valueArg.literalValue = 4;
  valueArg.intWidth = 32;

  primec::Expr mapInsertStmt;
  mapInsertStmt.kind = primec::Expr::Kind::Call;
  mapInsertStmt.name = "/std/collections/map/insert";
  mapInsertStmt.args = {valuesFactoryCall, keyArg, valueArg};
  mapInsertStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};
  mapInsertStmt.templateArgs = {"i32", "i32"};

  primec::Definition mapValuesFactoryDef;
  mapValuesFactoryDef.fullPath = "/main/makeValues";
  primec::Transform mapValuesFactoryReturn;
  mapValuesFactoryReturn.name = "return";
  mapValuesFactoryReturn.templateArgs = {"map<i32, i32>"};
  mapValuesFactoryDef.transforms.push_back(mapValuesFactoryReturn);
  primec::Definition mapValuesRefFactoryDef;
  mapValuesRefFactoryDef.fullPath = "/main/makeValuesRef";
  primec::Transform mapValuesRefFactoryReturn;
  mapValuesRefFactoryReturn.name = "return";
  mapValuesRefFactoryReturn.templateArgs = {"Reference<map<i32, i32>>"};
  mapValuesRefFactoryDef.transforms.push_back(mapValuesRefFactoryReturn);

  primec::Definition mapInsertBuiltinDef;
  mapInsertBuiltinDef.fullPath = "/std/collections/map/insert_builtin";
  primec::Definition mapInsertMethodDef;
  mapInsertMethodDef.fullPath = "/std/collections/map/insert";
  primec::Definition mapInsertAliasDef;
  mapInsertAliasDef.fullPath = "/std/collections/mapInsert";
  primec::Definition mapInsertGeneratedBareDef;
  mapInsertGeneratedBareDef.fullPath = "/insert__generated";
  primec::Definition mapInsertGeneratedAliasBareDef;
  mapInsertGeneratedAliasBareDef.fullPath = "/mapInsert__generated";
  primec::Definition mapInsertGeneratedPascalAliasBareDef;
  mapInsertGeneratedPascalAliasBareDef.fullPath = "/MapInsert__generated";
  primec::Definition mapInsertGeneratedPascalRefAliasBareDef;
  mapInsertGeneratedPascalRefAliasBareDef.fullPath = "/MapInsertRef__generated";
  primec::Definition mapAtArgsPackDef;
  mapAtArgsPackDef.fullPath = "/map/at";
  primec::Definition mapAtArgsPackRefDef;
  mapAtArgsPackRefDef.fullPath = "/map/at_ref";
  primec::Definition mapAtArgsPackUnsafeDef;
  mapAtArgsPackUnsafeDef.fullPath = "/map/at_unsafe";
  primec::Definition mapAtArgsPackUnsafeRefDef;
  mapAtArgsPackUnsafeRefDef.fullPath = "/map/at_unsafe_ref";
  primec::Definition mapAtAliasArgsPackDef;
  mapAtAliasArgsPackDef.fullPath = "/std/collections/mapAt";
  primec::Definition mapAtUnsafeAliasArgsPackDef;
  mapAtUnsafeAliasArgsPackDef.fullPath = "/std/collections/mapAtUnsafe";
  primec::Definition mapAtRefAliasArgsPackDef;
  mapAtRefAliasArgsPackDef.fullPath = "/std/collections/mapAtRef";
  primec::Definition mapAtUnsafeRefAliasArgsPackDef;
  mapAtUnsafeRefAliasArgsPackDef.fullPath = "/std/collections/mapAtUnsafeRef";
  primec::Expr mapInsertMethodValuesParam;
  mapInsertMethodValuesParam.kind = primec::Expr::Kind::Name;
  mapInsertMethodValuesParam.name = "values";
  primec::Transform mapInsertMethodMutTransform;
  mapInsertMethodMutTransform.name = "mut";
  primec::Transform mapInsertMethodTypeTransform;
  mapInsertMethodTypeTransform.name = "map";
  mapInsertMethodTypeTransform.templateArgs = {"i32", "i32"};
  mapInsertMethodValuesParam.transforms = {mapInsertMethodMutTransform, mapInsertMethodTypeTransform};
  mapInsertMethodDef.parameters = {mapInsertMethodValuesParam};
  mapInsertAliasDef.parameters = {mapInsertMethodValuesParam};
  mapInsertGeneratedBareDef.parameters = {mapInsertMethodValuesParam};
  mapInsertGeneratedAliasBareDef.parameters = {mapInsertMethodValuesParam};
  mapInsertGeneratedPascalAliasBareDef.parameters = {mapInsertMethodValuesParam};
  mapInsertGeneratedPascalRefAliasBareDef.parameters = {mapInsertMethodValuesParam};
  primec::Expr mapAtArgsPackValuesParam;
  mapAtArgsPackValuesParam.kind = primec::Expr::Kind::Name;
  mapAtArgsPackValuesParam.name = "values";
  primec::Transform mapAtArgsPackValuesTypeTransform;
  mapAtArgsPackValuesTypeTransform.name = "args";
  mapAtArgsPackValuesTypeTransform.templateArgs = {"Reference<map<i32, i32>>"};
  mapAtArgsPackValuesParam.transforms = {mapAtArgsPackValuesTypeTransform};
  mapAtArgsPackDef.parameters = {mapAtArgsPackValuesParam};
  mapAtArgsPackRefDef.parameters = {mapAtArgsPackValuesParam};
  mapAtArgsPackUnsafeDef.parameters = {mapAtArgsPackValuesParam};
  mapAtArgsPackUnsafeRefDef.parameters = {mapAtArgsPackValuesParam};
  mapAtAliasArgsPackDef.parameters = {mapAtArgsPackValuesParam};
  mapAtUnsafeAliasArgsPackDef.parameters = {mapAtArgsPackValuesParam};
  mapAtRefAliasArgsPackDef.parameters = {mapAtArgsPackValuesParam};
  mapAtUnsafeRefAliasArgsPackDef.parameters = {mapAtArgsPackValuesParam};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/main/makeValues") {
                return &mapValuesFactoryDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr namespacedMapInsertStmt = mapInsertStmt;
  namespacedMapInsertStmt.name = "insert";
  namespacedMapInsertStmt.namespacePrefix = "/std/collections/map";

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            namespacedMapInsertStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/main/makeValues") {
                return &mapValuesFactoryDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertMethodStmt = mapInsertStmt;
  mapInsertMethodStmt.name = "insert";
  mapInsertMethodStmt.isMethodCall = true;
  mapInsertMethodStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/main/makeValues") {
                return &mapValuesFactoryDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr holderName;
  holderName.kind = primec::Expr::Kind::Name;
  holderName.name = "holder";

  primec::Expr holderValuesFieldExpr;
  holderValuesFieldExpr.kind = primec::Expr::Kind::Call;
  holderValuesFieldExpr.name = "values";
  holderValuesFieldExpr.isFieldAccess = true;
  holderValuesFieldExpr.args = {holderName};
  holderValuesFieldExpr.argNames = {std::nullopt};

  primec::Expr mapInsertFieldAccessStmt = mapInsertStmt;
  mapInsertFieldAccessStmt.args = {holderValuesFieldExpr, keyArg, valueArg};
  mapInsertFieldAccessStmt.templateArgs = {"i32", "i32"};
  mapInsertFieldAccessStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertFieldAccessStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert") {
                return &mapInsertMethodDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertFieldAccessInferredStmt = mapInsertFieldAccessStmt;
  mapInsertFieldAccessInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertFieldAccessInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert") {
                return &mapInsertMethodDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertFieldAccessAliasInferredStmt = mapInsertFieldAccessStmt;
  mapInsertFieldAccessAliasInferredStmt.name = "/std/collections/mapInsert";
  mapInsertFieldAccessAliasInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertFieldAccessAliasInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/mapInsert") {
                return &mapInsertAliasDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertFieldAccessGeneratedPascalAliasPathInferredStmt = mapInsertFieldAccessStmt;
  mapInsertFieldAccessGeneratedPascalAliasPathInferredStmt.name = "/std/collections/MapInsert__generated";
  mapInsertFieldAccessGeneratedPascalAliasPathInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertFieldAccessGeneratedPascalAliasPathInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/MapInsert__generated") {
                return &mapInsertGeneratedPascalAliasBareDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertFieldAccessGeneratedPascalRefAliasPathInferredStmt = mapInsertFieldAccessStmt;
  mapInsertFieldAccessGeneratedPascalRefAliasPathInferredStmt.name =
      "/std/collections/MapInsertRef__generated";
  mapInsertFieldAccessGeneratedPascalRefAliasPathInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertFieldAccessGeneratedPascalRefAliasPathInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/MapInsertRef__generated") {
                return &mapInsertGeneratedPascalRefAliasBareDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Error);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  primec::Expr mapInsertFieldAccessGeneratedBareInferredStmt = mapInsertFieldAccessStmt;
  mapInsertFieldAccessGeneratedBareInferredStmt.name = "insert__generated";
  mapInsertFieldAccessGeneratedBareInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertFieldAccessGeneratedBareInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "insert__generated") {
                return &mapInsertGeneratedBareDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertFieldAccessGeneratedAliasBareInferredStmt = mapInsertFieldAccessStmt;
  mapInsertFieldAccessGeneratedAliasBareInferredStmt.name = "mapInsert__generated";
  mapInsertFieldAccessGeneratedAliasBareInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertFieldAccessGeneratedAliasBareInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "mapInsert__generated") {
                return &mapInsertGeneratedAliasBareDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertFieldAccessGeneratedPascalAliasBareInferredStmt = mapInsertFieldAccessStmt;
  mapInsertFieldAccessGeneratedPascalAliasBareInferredStmt.name = "MapInsert__generated";
  mapInsertFieldAccessGeneratedPascalAliasBareInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertFieldAccessGeneratedPascalAliasBareInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "MapInsert__generated") {
                return &mapInsertGeneratedPascalAliasBareDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertFieldAccessGeneratedPascalRefAliasBareInferredStmt = mapInsertFieldAccessStmt;
  mapInsertFieldAccessGeneratedPascalRefAliasBareInferredStmt.name = "MapInsertRef__generated";
  mapInsertFieldAccessGeneratedPascalRefAliasBareInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertFieldAccessGeneratedPascalRefAliasBareInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "MapInsertRef__generated") {
                return &mapInsertGeneratedPascalRefAliasBareDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertFieldAccessMethodStmt;
  mapInsertFieldAccessMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertFieldAccessMethodStmt.name = "insert";
  mapInsertFieldAccessMethodStmt.isMethodCall = true;
  mapInsertFieldAccessMethodStmt.args = {holderValuesFieldExpr, keyArg, valueArg};
  mapInsertFieldAccessMethodStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertFieldAccessMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertMethodDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertFieldAccessMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertFieldAccessMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertGeneratedBareDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertFieldAccessMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertGeneratedPascalAliasBareDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertFieldAccessGeneratedAliasMethodStmt = mapInsertFieldAccessMethodStmt;
  mapInsertFieldAccessGeneratedAliasMethodStmt.name = "mapInsert__generated";

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertFieldAccessGeneratedAliasMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapInsert__generated" &&
                  callExpr.args.size() == 3) {
                return &mapInsertGeneratedAliasBareDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertFieldAccessGeneratedPascalRefAliasMethodStmt = mapInsertFieldAccessMethodStmt;
  mapInsertFieldAccessGeneratedPascalRefAliasMethodStmt.name = "MapInsertRef__generated";

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertFieldAccessGeneratedPascalRefAliasMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "MapInsertRef__generated" &&
                  callExpr.args.size() == 3) {
                return &mapInsertGeneratedPascalRefAliasBareDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertFieldAccessGeneratedAliasSlashMethodStmt = mapInsertFieldAccessMethodStmt;
  mapInsertFieldAccessGeneratedAliasSlashMethodStmt.name = "/std/collections/mapInsert__generated";

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertFieldAccessGeneratedAliasSlashMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall &&
                  callExpr.name == "/std/collections/mapInsert__generated" &&
                  callExpr.args.size() == 3) {
                return &mapInsertGeneratedAliasBareDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertFieldAccessGeneratedPascalRefAliasSlashMethodStmt = mapInsertFieldAccessMethodStmt;
  mapInsertFieldAccessGeneratedPascalRefAliasSlashMethodStmt.name =
      "/std/collections/MapInsertRef__generated";

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertFieldAccessGeneratedPascalRefAliasSlashMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall &&
                  callExpr.name == "/std/collections/MapInsertRef__generated" &&
                  callExpr.args.size() == 3) {
                return &mapInsertGeneratedPascalRefAliasBareDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr localMapValuesName;
  localMapValuesName.kind = primec::Expr::Kind::Name;
  localMapValuesName.name = "valuesLocal";
  primec::Expr localMapValuesLocationExpr;
  localMapValuesLocationExpr.kind = primec::Expr::Kind::Call;
  localMapValuesLocationExpr.name = "location";
  localMapValuesLocationExpr.args = {localMapValuesName};
  localMapValuesLocationExpr.argNames = {std::nullopt};
  primec::ir_lowerer::LocalMap localMapLocals;
  primec::ir_lowerer::LocalInfo localMapInfo;
  localMapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  localMapInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  localMapInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  localMapInfo.index = 4;
  localMapLocals.emplace("valuesLocal", localMapInfo);

  primec::Expr mapInsertLocationLocalMapInferredStmt = mapInsertStmt;
  mapInsertLocationLocalMapInferredStmt.args = {localMapValuesLocationExpr, keyArg, valueArg};
  mapInsertLocationLocalMapInferredStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};
  mapInsertLocationLocalMapInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertLocationLocalMapInferredStmt,
            localMapLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &localsIn,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              CHECK(localsIn.find("valuesLocal") != localsIn.end());
              return true;
            },
            instructions,
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  primec::Expr mapInsertLocationLocalMapMethodStmt;
  mapInsertLocationLocalMapMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertLocationLocalMapMethodStmt.name = "insert";
  mapInsertLocationLocalMapMethodStmt.isMethodCall = true;
  mapInsertLocationLocalMapMethodStmt.args = {localMapValuesLocationExpr, keyArg, valueArg};
  mapInsertLocationLocalMapMethodStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertLocationLocalMapMethodStmt,
            localMapLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &localsIn,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "insert");
              CHECK(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/mapInsert");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs.empty());
              CHECK(localsIn.find("valuesLocal") != localsIn.end());
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackName;
  mapsPackName.kind = primec::Expr::Kind::Name;
  mapsPackName.name = "mapsPack";
  primec::Expr mapsPackSlotIndex;
  mapsPackSlotIndex.kind = primec::Expr::Kind::Literal;
  mapsPackSlotIndex.literalValue = 0;
  mapsPackSlotIndex.intWidth = 32;

  primec::Expr mapsPackAtExpr;
  mapsPackAtExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtExpr.name = "/map/at";
  mapsPackAtExpr.args = {mapsPackName, mapsPackSlotIndex};
  mapsPackAtExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapsPackAtLocationExpr;
  mapsPackAtLocationExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtLocationExpr.name = "location";
  mapsPackAtLocationExpr.args = {mapsPackAtExpr};
  mapsPackAtLocationExpr.argNames = {std::nullopt};

  primec::Expr mapsPackAtLocationDerefExpr;
  mapsPackAtLocationDerefExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtLocationDerefExpr.name = "dereference";
  mapsPackAtLocationDerefExpr.args = {mapsPackAtLocationExpr};
  mapsPackAtLocationDerefExpr.argNames = {std::nullopt};

  primec::Expr mapsPackAtNestedLocationDerefExpr;
  mapsPackAtNestedLocationDerefExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtNestedLocationDerefExpr.name = "location";
  mapsPackAtNestedLocationDerefExpr.args = {mapsPackAtLocationDerefExpr};
  mapsPackAtNestedLocationDerefExpr.argNames = {std::nullopt};

  primec::ir_lowerer::LocalMap mapsPackLocals;
  primec::ir_lowerer::LocalInfo mapsPackInfo;
  mapsPackInfo.isArgsPack = true;
  mapsPackInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  mapsPackInfo.referenceToMap = true;
  mapsPackInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  mapsPackInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  mapsPackLocals.emplace("mapsPack", mapsPackInfo);

  primec::Expr mapInsertArgsPackWrappedInferredStmt = mapInsertStmt;
  mapInsertArgsPackWrappedInferredStmt.args = {mapsPackAtNestedLocationDerefExpr, keyArg, valueArg};
  mapInsertArgsPackWrappedInferredStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};
  mapInsertArgsPackWrappedInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackWrappedInferredStmt,
            mapsPackLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &localsIn,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              CHECK(localsIn.find("mapsPack") != localsIn.end());
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackWrappedMethodStmt;
  mapInsertArgsPackWrappedMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackWrappedMethodStmt.name = "insert";
  mapInsertArgsPackWrappedMethodStmt.isMethodCall = true;
  mapInsertArgsPackWrappedMethodStmt.args = {mapsPackAtNestedLocationDerefExpr, keyArg, valueArg};
  mapInsertArgsPackWrappedMethodStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackWrappedMethodStmt,
            mapsPackLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &localsIn,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              CHECK(localsIn.find("mapsPack") != localsIn.end());
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackReceiverLocationExpr;
  mapsPackReceiverLocationExpr.kind = primec::Expr::Kind::Call;
  mapsPackReceiverLocationExpr.name = "location";
  mapsPackReceiverLocationExpr.args = {mapsPackName};
  mapsPackReceiverLocationExpr.argNames = {std::nullopt};

  primec::Expr mapsPackAtWrappedReceiverExpr;
  mapsPackAtWrappedReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtWrappedReceiverExpr.name = "/map/at";
  mapsPackAtWrappedReceiverExpr.args = {mapsPackReceiverLocationExpr, mapsPackSlotIndex};
  mapsPackAtWrappedReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackReceiverLocationInferredStmt = mapInsertStmt;
  mapInsertArgsPackReceiverLocationInferredStmt.args = {mapsPackAtWrappedReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackReceiverLocationInferredStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};
  mapInsertArgsPackReceiverLocationInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackReceiverLocationInferredStmt,
            mapsPackLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &localsIn,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              CHECK(localsIn.find("mapsPack") != localsIn.end());
              return true;
            },
            instructions,
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackReceiverLocationMethodStmt;
  mapInsertArgsPackReceiverLocationMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackReceiverLocationMethodStmt.name = "insert";
  mapInsertArgsPackReceiverLocationMethodStmt.isMethodCall = true;
  mapInsertArgsPackReceiverLocationMethodStmt.args = {mapsPackAtWrappedReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackReceiverLocationMethodStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackReceiverLocationMethodStmt,
            mapsPackLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &localsIn,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "insert");
              CHECK(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/mapInsert");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs.empty());
              CHECK(localsIn.find("mapsPack") != localsIn.end());
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr holderMapsPackFieldExpr;
  holderMapsPackFieldExpr.kind = primec::Expr::Kind::Call;
  holderMapsPackFieldExpr.name = "mapsPack";
  holderMapsPackFieldExpr.isFieldAccess = true;
  holderMapsPackFieldExpr.args = {holderName};
  holderMapsPackFieldExpr.argNames = {std::nullopt};

  primec::Expr holderMapsPackFieldLocationExpr;
  holderMapsPackFieldLocationExpr.kind = primec::Expr::Kind::Call;
  holderMapsPackFieldLocationExpr.name = "location";
  holderMapsPackFieldLocationExpr.args = {holderMapsPackFieldExpr};
  holderMapsPackFieldLocationExpr.argNames = {std::nullopt};

  primec::Expr mapsPackAtNonLocalReceiverExpr;
  mapsPackAtNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtNonLocalReceiverExpr.name = "/map/at";
  mapsPackAtNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackAtNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackNonLocalReceiverInferredStmt.args = {mapsPackAtNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackNonLocalReceiverInferredStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};
  mapInsertArgsPackNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/map/at") {
                return &mapAtArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackNonLocalReceiverMethodStmt;
  mapInsertArgsPackNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackNonLocalReceiverMethodStmt.args = {mapsPackAtNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackNonLocalReceiverMethodStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/map/at") {
                return &mapAtArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackAtUnsafeNonLocalReceiverExpr;
  mapsPackAtUnsafeNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtUnsafeNonLocalReceiverExpr.name = "/map/at_unsafe";
  mapsPackAtUnsafeNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackAtUnsafeNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackUnsafeNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackUnsafeNonLocalReceiverInferredStmt.args = {mapsPackAtUnsafeNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackUnsafeNonLocalReceiverInferredStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};
  mapInsertArgsPackUnsafeNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackUnsafeNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/map/at_unsafe") {
                return &mapAtArgsPackUnsafeDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackUnsafeNonLocalReceiverMethodStmt;
  mapInsertArgsPackUnsafeNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackUnsafeNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackUnsafeNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackUnsafeNonLocalReceiverMethodStmt.args = {mapsPackAtUnsafeNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackUnsafeNonLocalReceiverMethodStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackUnsafeNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/map/at_unsafe") {
                return &mapAtArgsPackUnsafeDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackAtRefDirectNonLocalReceiverExpr;
  mapsPackAtRefDirectNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtRefDirectNonLocalReceiverExpr.name = "/map/at_ref";
  mapsPackAtRefDirectNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackAtRefDirectNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackAtRefDirectNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackAtRefDirectNonLocalReceiverInferredStmt.args = {mapsPackAtRefDirectNonLocalReceiverExpr,
                                                                   keyArg,
                                                                   valueArg};
  mapInsertArgsPackAtRefDirectNonLocalReceiverInferredStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};
  mapInsertArgsPackAtRefDirectNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtRefDirectNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/map/at_ref") {
                return &mapAtArgsPackRefDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackAtRefDirectNonLocalReceiverMethodStmt;
  mapInsertArgsPackAtRefDirectNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackAtRefDirectNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackAtRefDirectNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackAtRefDirectNonLocalReceiverMethodStmt.args = {mapsPackAtRefDirectNonLocalReceiverExpr,
                                                                  keyArg,
                                                                  valueArg};
  mapInsertArgsPackAtRefDirectNonLocalReceiverMethodStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtRefDirectNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/map/at_ref") {
                return &mapAtArgsPackRefDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackAtUnsafeRefDirectNonLocalReceiverExpr;
  mapsPackAtUnsafeRefDirectNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtUnsafeRefDirectNonLocalReceiverExpr.name = "/map/at_unsafe_ref";
  mapsPackAtUnsafeRefDirectNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackAtUnsafeRefDirectNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackAtUnsafeRefDirectNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackAtUnsafeRefDirectNonLocalReceiverInferredStmt.args = {
      mapsPackAtUnsafeRefDirectNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackAtUnsafeRefDirectNonLocalReceiverInferredStmt.argNames = {std::nullopt, std::nullopt,
                                                                              std::nullopt};
  mapInsertArgsPackAtUnsafeRefDirectNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtUnsafeRefDirectNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/map/at_unsafe_ref") {
                return &mapAtArgsPackUnsafeRefDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackAtUnsafeRefDirectNonLocalReceiverMethodStmt;
  mapInsertArgsPackAtUnsafeRefDirectNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackAtUnsafeRefDirectNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackAtUnsafeRefDirectNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackAtUnsafeRefDirectNonLocalReceiverMethodStmt.args = {
      mapsPackAtUnsafeRefDirectNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackAtUnsafeRefDirectNonLocalReceiverMethodStmt.argNames = {std::nullopt,
                                                                            std::nullopt,
                                                                            std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtUnsafeRefDirectNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/map/at_unsafe_ref") {
                return &mapAtArgsPackUnsafeRefDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackAtUnsafeRefGeneratedAliasNonLocalReceiverExpr;
  mapsPackAtUnsafeRefGeneratedAliasNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtUnsafeRefGeneratedAliasNonLocalReceiverExpr.name = "/std/collections/mapAtUnsafeRef__generated";
  mapsPackAtUnsafeRefGeneratedAliasNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackAtUnsafeRefGeneratedAliasNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackAtUnsafeRefGeneratedAliasNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackAtUnsafeRefGeneratedAliasNonLocalReceiverInferredStmt.args = {
      mapsPackAtUnsafeRefGeneratedAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackAtUnsafeRefGeneratedAliasNonLocalReceiverInferredStmt.argNames = {std::nullopt, std::nullopt,
                                                                                      std::nullopt};
  mapInsertArgsPackAtUnsafeRefGeneratedAliasNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtUnsafeRefGeneratedAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/mapAtUnsafeRef__generated") {
                return &mapAtUnsafeRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(inlineCalls == 0);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackAtUnsafeRefGeneratedAliasNonLocalReceiverMethodStmt;
  mapInsertArgsPackAtUnsafeRefGeneratedAliasNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackAtUnsafeRefGeneratedAliasNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackAtUnsafeRefGeneratedAliasNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackAtUnsafeRefGeneratedAliasNonLocalReceiverMethodStmt.args = {
      mapsPackAtUnsafeRefGeneratedAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackAtUnsafeRefGeneratedAliasNonLocalReceiverMethodStmt.argNames = {std::nullopt,
                                                                                    std::nullopt,
                                                                                    std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtUnsafeRefGeneratedAliasNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/mapAtUnsafeRef__generated") {
                return &mapAtUnsafeRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.name == "insert");
              CHECK(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/mapInsert");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs.empty());
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackAtRefBareNonLocalReceiverExpr;
  mapsPackAtRefBareNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtRefBareNonLocalReceiverExpr.name = "at_ref";
  mapsPackAtRefBareNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackAtRefBareNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackAtRefBareNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackAtRefBareNonLocalReceiverInferredStmt.args = {mapsPackAtRefBareNonLocalReceiverExpr,
                                                                 keyArg,
                                                                 valueArg};
  mapInsertArgsPackAtRefBareNonLocalReceiverInferredStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};
  mapInsertArgsPackAtRefBareNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtRefBareNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "at_ref") {
                return &mapAtRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackAtRefBareNonLocalReceiverMethodStmt;
  mapInsertArgsPackAtRefBareNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackAtRefBareNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackAtRefBareNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackAtRefBareNonLocalReceiverMethodStmt.args = {mapsPackAtRefBareNonLocalReceiverExpr,
                                                               keyArg,
                                                               valueArg};
  mapInsertArgsPackAtRefBareNonLocalReceiverMethodStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtRefBareNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "at_ref") {
                return &mapAtRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverExpr;
  mapsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverExpr.name = "mapAtUnsafeRef__generated";
  mapsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverInferredStmt.args = {
      mapsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverInferredStmt.argNames = {
      std::nullopt, std::nullopt, std::nullopt};
  mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "mapAtUnsafeRef__generated") {
                return &mapAtUnsafeRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverMethodStmt;
  mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverMethodStmt.args = {
      mapsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverMethodStmt.argNames = {
      std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "mapAtUnsafeRef__generated") {
                return &mapAtUnsafeRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverGeneratedInsertStmt;
  mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverGeneratedInsertStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverGeneratedInsertStmt.name = "insert__generated";
  mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverGeneratedInsertStmt.isMethodCall = false;
  mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverGeneratedInsertStmt.args = {
      mapsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverGeneratedInsertStmt.argNames = {
      std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverGeneratedInsertStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "mapAtUnsafeRef__generated") {
                return &mapAtUnsafeRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverGeneratedMapInsertStmt;
  mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverGeneratedMapInsertStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverGeneratedMapInsertStmt.name = "mapInsert__generated";
  mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverGeneratedMapInsertStmt.isMethodCall = false;
  mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverGeneratedMapInsertStmt.args = {
      mapsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverGeneratedMapInsertStmt.argNames = {
      std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackMapAtUnsafeRefGeneratedBareNonLocalReceiverGeneratedMapInsertStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "mapAtUnsafeRef__generated") {
                return &mapAtUnsafeRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackAtAliasNonLocalReceiverExpr;
  mapsPackAtAliasNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtAliasNonLocalReceiverExpr.name = "/std/collections/mapAt";
  mapsPackAtAliasNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackAtAliasNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackAliasNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackAliasNonLocalReceiverInferredStmt.args = {mapsPackAtAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackAliasNonLocalReceiverInferredStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};
  mapInsertArgsPackAliasNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/mapAt") {
                return &mapAtAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackAliasNonLocalReceiverMethodStmt;
  mapInsertArgsPackAliasNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackAliasNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackAliasNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackAliasNonLocalReceiverMethodStmt.args = {mapsPackAtAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackAliasNonLocalReceiverMethodStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAliasNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/mapAt") {
                return &mapAtAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackAtUnsafeAliasNonLocalReceiverExpr;
  mapsPackAtUnsafeAliasNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtUnsafeAliasNonLocalReceiverExpr.name = "/std/collections/mapAtUnsafe";
  mapsPackAtUnsafeAliasNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackAtUnsafeAliasNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackUnsafeAliasNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackUnsafeAliasNonLocalReceiverInferredStmt.args = {mapsPackAtUnsafeAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackUnsafeAliasNonLocalReceiverInferredStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};
  mapInsertArgsPackUnsafeAliasNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackUnsafeAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/mapAtUnsafe") {
                return &mapAtUnsafeAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackUnsafeAliasNonLocalReceiverMethodStmt;
  mapInsertArgsPackUnsafeAliasNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackUnsafeAliasNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackUnsafeAliasNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackUnsafeAliasNonLocalReceiverMethodStmt.args = {mapsPackAtUnsafeAliasNonLocalReceiverExpr, keyArg,
                                                                 valueArg};
  mapInsertArgsPackUnsafeAliasNonLocalReceiverMethodStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackUnsafeAliasNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/mapAtUnsafe") {
                return &mapAtUnsafeAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackAtMethodAliasNonLocalReceiverExpr;
  mapsPackAtMethodAliasNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtMethodAliasNonLocalReceiverExpr.name = "at";
  mapsPackAtMethodAliasNonLocalReceiverExpr.isMethodCall = true;
  mapsPackAtMethodAliasNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackAtMethodAliasNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackMethodAliasNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackMethodAliasNonLocalReceiverInferredStmt.args = {mapsPackAtMethodAliasNonLocalReceiverExpr, keyArg,
                                                                   valueArg};
  mapInsertArgsPackMethodAliasNonLocalReceiverInferredStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};
  mapInsertArgsPackMethodAliasNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "at" && callExpr.args.size() == 2) {
                return &mapAtAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackMethodAliasNonLocalReceiverMethodStmt;
  mapInsertArgsPackMethodAliasNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackMethodAliasNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackMethodAliasNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackMethodAliasNonLocalReceiverMethodStmt.args = {mapsPackAtMethodAliasNonLocalReceiverExpr, keyArg,
                                                                 valueArg};
  mapInsertArgsPackMethodAliasNonLocalReceiverMethodStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackMethodAliasNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "at" && callExpr.args.size() == 2) {
                return &mapAtAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackAtUnsafeMethodAliasNonLocalReceiverExpr;
  mapsPackAtUnsafeMethodAliasNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtUnsafeMethodAliasNonLocalReceiverExpr.name = "at_unsafe";
  mapsPackAtUnsafeMethodAliasNonLocalReceiverExpr.isMethodCall = true;
  mapsPackAtUnsafeMethodAliasNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackAtUnsafeMethodAliasNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackUnsafeMethodAliasNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackUnsafeMethodAliasNonLocalReceiverInferredStmt.args = {mapsPackAtUnsafeMethodAliasNonLocalReceiverExpr,
                                                                         keyArg,
                                                                         valueArg};
  mapInsertArgsPackUnsafeMethodAliasNonLocalReceiverInferredStmt.argNames = {std::nullopt, std::nullopt,
                                                                              std::nullopt};
  mapInsertArgsPackUnsafeMethodAliasNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackUnsafeMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "at_unsafe" && callExpr.args.size() == 2) {
                return &mapAtUnsafeAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackUnsafeMethodAliasNonLocalReceiverMethodStmt;
  mapInsertArgsPackUnsafeMethodAliasNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackUnsafeMethodAliasNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackUnsafeMethodAliasNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackUnsafeMethodAliasNonLocalReceiverMethodStmt.args = {
      mapsPackAtUnsafeMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackUnsafeMethodAliasNonLocalReceiverMethodStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackUnsafeMethodAliasNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "at_unsafe" && callExpr.args.size() == 2) {
                return &mapAtUnsafeAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackAtPascalMethodAliasNonLocalReceiverExpr;
  mapsPackAtPascalMethodAliasNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtPascalMethodAliasNonLocalReceiverExpr.name = "At";
  mapsPackAtPascalMethodAliasNonLocalReceiverExpr.isMethodCall = true;
  mapsPackAtPascalMethodAliasNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackAtPascalMethodAliasNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackAtPascalMethodAliasNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackAtPascalMethodAliasNonLocalReceiverInferredStmt.args = {mapsPackAtPascalMethodAliasNonLocalReceiverExpr,
                                                                           keyArg,
                                                                           valueArg};
  mapInsertArgsPackAtPascalMethodAliasNonLocalReceiverInferredStmt.argNames = {std::nullopt, std::nullopt,
                                                                                std::nullopt};
  mapInsertArgsPackAtPascalMethodAliasNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtPascalMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "At" && callExpr.args.size() == 2) {
                return &mapAtAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackAtPascalMethodAliasNonLocalReceiverMethodStmt;
  mapInsertArgsPackAtPascalMethodAliasNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackAtPascalMethodAliasNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackAtPascalMethodAliasNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackAtPascalMethodAliasNonLocalReceiverMethodStmt.args = {
      mapsPackAtPascalMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackAtPascalMethodAliasNonLocalReceiverMethodStmt.argNames = {std::nullopt,
                                                                              std::nullopt,
                                                                              std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtPascalMethodAliasNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "At" && callExpr.args.size() == 2) {
                return &mapAtAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackAtUnsafePascalMethodAliasNonLocalReceiverExpr;
  mapsPackAtUnsafePascalMethodAliasNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtUnsafePascalMethodAliasNonLocalReceiverExpr.name = "AtUnsafe";
  mapsPackAtUnsafePascalMethodAliasNonLocalReceiverExpr.isMethodCall = true;
  mapsPackAtUnsafePascalMethodAliasNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackAtUnsafePascalMethodAliasNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackAtUnsafePascalMethodAliasNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackAtUnsafePascalMethodAliasNonLocalReceiverInferredStmt.args = {
      mapsPackAtUnsafePascalMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackAtUnsafePascalMethodAliasNonLocalReceiverInferredStmt.argNames = {std::nullopt,
                                                                                      std::nullopt,
                                                                                      std::nullopt};
  mapInsertArgsPackAtUnsafePascalMethodAliasNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtUnsafePascalMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "AtUnsafe" && callExpr.args.size() == 2) {
                return &mapAtUnsafeAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackAtUnsafePascalMethodAliasNonLocalReceiverMethodStmt;
  mapInsertArgsPackAtUnsafePascalMethodAliasNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackAtUnsafePascalMethodAliasNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackAtUnsafePascalMethodAliasNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackAtUnsafePascalMethodAliasNonLocalReceiverMethodStmt.args = {
      mapsPackAtUnsafePascalMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackAtUnsafePascalMethodAliasNonLocalReceiverMethodStmt.argNames = {std::nullopt,
                                                                                    std::nullopt,
                                                                                    std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtUnsafePascalMethodAliasNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "AtUnsafe" && callExpr.args.size() == 2) {
                return &mapAtUnsafeAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackAtRefPascalMethodAliasNonLocalReceiverExpr;
  mapsPackAtRefPascalMethodAliasNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtRefPascalMethodAliasNonLocalReceiverExpr.name = "AtRef";
  mapsPackAtRefPascalMethodAliasNonLocalReceiverExpr.isMethodCall = true;
  mapsPackAtRefPascalMethodAliasNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackAtRefPascalMethodAliasNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackAtRefPascalMethodAliasNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackAtRefPascalMethodAliasNonLocalReceiverInferredStmt.args = {
      mapsPackAtRefPascalMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackAtRefPascalMethodAliasNonLocalReceiverInferredStmt.argNames = {std::nullopt, std::nullopt,
                                                                                   std::nullopt};
  mapInsertArgsPackAtRefPascalMethodAliasNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtRefPascalMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "AtRef" && callExpr.args.size() == 2) {
                return &mapAtRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackAtRefPascalMethodAliasNonLocalReceiverMethodStmt;
  mapInsertArgsPackAtRefPascalMethodAliasNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackAtRefPascalMethodAliasNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackAtRefPascalMethodAliasNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackAtRefPascalMethodAliasNonLocalReceiverMethodStmt.args = {
      mapsPackAtRefPascalMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackAtRefPascalMethodAliasNonLocalReceiverMethodStmt.argNames = {std::nullopt,
                                                                                 std::nullopt,
                                                                                 std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtRefPascalMethodAliasNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "AtRef" && callExpr.args.size() == 2) {
                return &mapAtRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverExpr;
  mapsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverExpr.name = "AtUnsafeRef";
  mapsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverExpr.isMethodCall = true;
  mapsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverInferredStmt.args = {
      mapsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverInferredStmt.argNames = {std::nullopt,
                                                                                         std::nullopt,
                                                                                         std::nullopt};
  mapInsertArgsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "AtUnsafeRef" && callExpr.args.size() == 2) {
                return &mapAtUnsafeRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverMethodStmt;
  mapInsertArgsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverMethodStmt.args = {
      mapsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverMethodStmt.argNames = {std::nullopt,
                                                                                       std::nullopt,
                                                                                       std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtUnsafeRefPascalMethodAliasNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "AtUnsafeRef" && callExpr.args.size() == 2) {
                return &mapAtUnsafeRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackMapAtGeneratedMethodAliasNonLocalReceiverExpr;
  mapsPackMapAtGeneratedMethodAliasNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackMapAtGeneratedMethodAliasNonLocalReceiverExpr.name = "mapAt__generated";
  mapsPackMapAtGeneratedMethodAliasNonLocalReceiverExpr.isMethodCall = true;
  mapsPackMapAtGeneratedMethodAliasNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackMapAtGeneratedMethodAliasNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverInferredStmt.args = {
      mapsPackMapAtGeneratedMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverInferredStmt.argNames = {std::nullopt, std::nullopt,
                                                                                      std::nullopt};
  mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAt__generated" && callExpr.args.size() == 2) {
                return &mapAtAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverMethodStmt;
  mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverMethodStmt.args = {
      mapsPackMapAtGeneratedMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverMethodStmt.argNames = {std::nullopt,
                                                                                    std::nullopt,
                                                                                    std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAt__generated" && callExpr.args.size() == 2) {
                return &mapAtAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverGeneratedInsertMethodStmt;
  mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverGeneratedInsertMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverGeneratedInsertMethodStmt.name = "insert__generated";
  mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverGeneratedInsertMethodStmt.isMethodCall = true;
  mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverGeneratedInsertMethodStmt.args = {
      mapsPackMapAtGeneratedMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverGeneratedInsertMethodStmt.argNames = {
      std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackMapAtGeneratedMethodAliasNonLocalReceiverGeneratedInsertMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert__generated" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAt__generated" && callExpr.args.size() == 2) {
                return &mapAtAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverExpr;
  mapsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverExpr.name = "AtUnsafeRef__generated";
  mapsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverExpr.isMethodCall = true;
  mapsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr,
                                                                      mapsPackSlotIndex};
  mapsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverInferredStmt.args = {
      mapsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverInferredStmt.argNames = {std::nullopt,
                                                                                            std::nullopt,
                                                                                            std::nullopt};
  mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "AtUnsafeRef__generated" && callExpr.args.size() == 2) {
                return &mapAtUnsafeRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverMethodStmt;
  mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverMethodStmt.args = {
      mapsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverMethodStmt.argNames = {std::nullopt,
                                                                                          std::nullopt,
                                                                                          std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "AtUnsafeRef__generated" &&
                  callExpr.args.size() == 2) {
                return &mapAtUnsafeRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverGeneratedInsertMethodStmt;
  mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverGeneratedInsertMethodStmt.kind =
      primec::Expr::Kind::Call;
  mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverGeneratedInsertMethodStmt.name = "insert__generated";
  mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverGeneratedInsertMethodStmt.isMethodCall = true;
  mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverGeneratedInsertMethodStmt.args = {
      mapsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverGeneratedInsertMethodStmt.argNames = {
      std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtUnsafeRefGeneratedMethodAliasNonLocalReceiverGeneratedInsertMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert__generated" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "AtUnsafeRef__generated" &&
                  callExpr.args.size() == 2) {
                return &mapAtUnsafeRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackMapAtMethodAliasNonLocalReceiverExpr;
  mapsPackMapAtMethodAliasNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackMapAtMethodAliasNonLocalReceiverExpr.name = "mapAt";
  mapsPackMapAtMethodAliasNonLocalReceiverExpr.isMethodCall = true;
  mapsPackMapAtMethodAliasNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackMapAtMethodAliasNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackMapAtMethodAliasNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackMapAtMethodAliasNonLocalReceiverInferredStmt.args = {mapsPackMapAtMethodAliasNonLocalReceiverExpr,
                                                                        keyArg,
                                                                        valueArg};
  mapInsertArgsPackMapAtMethodAliasNonLocalReceiverInferredStmt.argNames = {std::nullopt, std::nullopt,
                                                                             std::nullopt};
  mapInsertArgsPackMapAtMethodAliasNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackMapAtMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAt" && callExpr.args.size() == 2) {
                return &mapAtAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackMapAtMethodAliasNonLocalReceiverMethodStmt;
  mapInsertArgsPackMapAtMethodAliasNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackMapAtMethodAliasNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackMapAtMethodAliasNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackMapAtMethodAliasNonLocalReceiverMethodStmt.args = {
      mapsPackMapAtMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackMapAtMethodAliasNonLocalReceiverMethodStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackMapAtMethodAliasNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAt" && callExpr.args.size() == 2) {
                return &mapAtAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackMapAtUnsafeMethodAliasNonLocalReceiverExpr;
  mapsPackMapAtUnsafeMethodAliasNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackMapAtUnsafeMethodAliasNonLocalReceiverExpr.name = "mapAtUnsafe";
  mapsPackMapAtUnsafeMethodAliasNonLocalReceiverExpr.isMethodCall = true;
  mapsPackMapAtUnsafeMethodAliasNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackMapAtUnsafeMethodAliasNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackMapAtUnsafeMethodAliasNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackMapAtUnsafeMethodAliasNonLocalReceiverInferredStmt.args = {
      mapsPackMapAtUnsafeMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackMapAtUnsafeMethodAliasNonLocalReceiverInferredStmt.argNames = {std::nullopt, std::nullopt,
                                                                                   std::nullopt};
  mapInsertArgsPackMapAtUnsafeMethodAliasNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackMapAtUnsafeMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAtUnsafe" && callExpr.args.size() == 2) {
                return &mapAtUnsafeAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackMapAtUnsafeMethodAliasNonLocalReceiverMethodStmt;
  mapInsertArgsPackMapAtUnsafeMethodAliasNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackMapAtUnsafeMethodAliasNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackMapAtUnsafeMethodAliasNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackMapAtUnsafeMethodAliasNonLocalReceiverMethodStmt.args = {
      mapsPackMapAtUnsafeMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackMapAtUnsafeMethodAliasNonLocalReceiverMethodStmt.argNames = {std::nullopt, std::nullopt,
                                                                                 std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackMapAtUnsafeMethodAliasNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAtUnsafe" && callExpr.args.size() == 2) {
                return &mapAtUnsafeAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackAtRefMethodAliasNonLocalReceiverExpr;
  mapsPackAtRefMethodAliasNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtRefMethodAliasNonLocalReceiverExpr.name = "at_ref";
  mapsPackAtRefMethodAliasNonLocalReceiverExpr.isMethodCall = true;
  mapsPackAtRefMethodAliasNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackAtRefMethodAliasNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackAtRefMethodAliasNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackAtRefMethodAliasNonLocalReceiverInferredStmt.args = {mapsPackAtRefMethodAliasNonLocalReceiverExpr,
                                                                        keyArg,
                                                                        valueArg};
  mapInsertArgsPackAtRefMethodAliasNonLocalReceiverInferredStmt.argNames = {std::nullopt, std::nullopt,
                                                                             std::nullopt};
  mapInsertArgsPackAtRefMethodAliasNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtRefMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "at_ref" && callExpr.args.size() == 2) {
                return &mapAtRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackAtRefMethodAliasNonLocalReceiverMethodStmt;
  mapInsertArgsPackAtRefMethodAliasNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackAtRefMethodAliasNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackAtRefMethodAliasNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackAtRefMethodAliasNonLocalReceiverMethodStmt.args = {
      mapsPackAtRefMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackAtRefMethodAliasNonLocalReceiverMethodStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtRefMethodAliasNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "at_ref" && callExpr.args.size() == 2) {
                return &mapAtRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackAtUnsafeRefMethodAliasNonLocalReceiverExpr;
  mapsPackAtUnsafeRefMethodAliasNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackAtUnsafeRefMethodAliasNonLocalReceiverExpr.name = "at_unsafe_ref";
  mapsPackAtUnsafeRefMethodAliasNonLocalReceiverExpr.isMethodCall = true;
  mapsPackAtUnsafeRefMethodAliasNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackAtUnsafeRefMethodAliasNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackAtUnsafeRefMethodAliasNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackAtUnsafeRefMethodAliasNonLocalReceiverInferredStmt.args = {
      mapsPackAtUnsafeRefMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackAtUnsafeRefMethodAliasNonLocalReceiverInferredStmt.argNames = {std::nullopt, std::nullopt,
                                                                                   std::nullopt};
  mapInsertArgsPackAtUnsafeRefMethodAliasNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtUnsafeRefMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "at_unsafe_ref" && callExpr.args.size() == 2) {
                return &mapAtUnsafeRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackAtUnsafeRefMethodAliasNonLocalReceiverMethodStmt;
  mapInsertArgsPackAtUnsafeRefMethodAliasNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackAtUnsafeRefMethodAliasNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackAtUnsafeRefMethodAliasNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackAtUnsafeRefMethodAliasNonLocalReceiverMethodStmt.args = {
      mapsPackAtUnsafeRefMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackAtUnsafeRefMethodAliasNonLocalReceiverMethodStmt.argNames = {std::nullopt,
                                                                                 std::nullopt,
                                                                                 std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackAtUnsafeRefMethodAliasNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "at_unsafe_ref" && callExpr.args.size() == 2) {
                return &mapAtUnsafeRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackMapAtRefMethodAliasNonLocalReceiverExpr;
  mapsPackMapAtRefMethodAliasNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackMapAtRefMethodAliasNonLocalReceiverExpr.name = "mapAtRef";
  mapsPackMapAtRefMethodAliasNonLocalReceiverExpr.isMethodCall = true;
  mapsPackMapAtRefMethodAliasNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackMapAtRefMethodAliasNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackMapAtRefMethodAliasNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackMapAtRefMethodAliasNonLocalReceiverInferredStmt.args = {
      mapsPackMapAtRefMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackMapAtRefMethodAliasNonLocalReceiverInferredStmt.argNames = {std::nullopt, std::nullopt,
                                                                                std::nullopt};
  mapInsertArgsPackMapAtRefMethodAliasNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackMapAtRefMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAtRef" && callExpr.args.size() == 2) {
                return &mapAtRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackMapAtRefMethodAliasNonLocalReceiverMethodStmt;
  mapInsertArgsPackMapAtRefMethodAliasNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackMapAtRefMethodAliasNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackMapAtRefMethodAliasNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackMapAtRefMethodAliasNonLocalReceiverMethodStmt.args = {
      mapsPackMapAtRefMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackMapAtRefMethodAliasNonLocalReceiverMethodStmt.argNames = {std::nullopt, std::nullopt,
                                                                              std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackMapAtRefMethodAliasNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAtRef" && callExpr.args.size() == 2) {
                return &mapAtRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapsPackMapAtUnsafeRefMethodAliasNonLocalReceiverExpr;
  mapsPackMapAtUnsafeRefMethodAliasNonLocalReceiverExpr.kind = primec::Expr::Kind::Call;
  mapsPackMapAtUnsafeRefMethodAliasNonLocalReceiverExpr.name = "mapAtUnsafeRef";
  mapsPackMapAtUnsafeRefMethodAliasNonLocalReceiverExpr.isMethodCall = true;
  mapsPackMapAtUnsafeRefMethodAliasNonLocalReceiverExpr.args = {holderMapsPackFieldLocationExpr, mapsPackSlotIndex};
  mapsPackMapAtUnsafeRefMethodAliasNonLocalReceiverExpr.argNames = {std::nullopt, std::nullopt};

  primec::Expr mapInsertArgsPackMapAtUnsafeRefMethodAliasNonLocalReceiverInferredStmt = mapInsertStmt;
  mapInsertArgsPackMapAtUnsafeRefMethodAliasNonLocalReceiverInferredStmt.args = {
      mapsPackMapAtUnsafeRefMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackMapAtUnsafeRefMethodAliasNonLocalReceiverInferredStmt.argNames = {std::nullopt, std::nullopt,
                                                                                      std::nullopt};
  mapInsertArgsPackMapAtUnsafeRefMethodAliasNonLocalReceiverInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackMapAtUnsafeRefMethodAliasNonLocalReceiverInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAtUnsafeRef" && callExpr.args.size() == 2) {
                return &mapAtUnsafeRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertArgsPackMapAtUnsafeRefMethodAliasNonLocalReceiverMethodStmt;
  mapInsertArgsPackMapAtUnsafeRefMethodAliasNonLocalReceiverMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertArgsPackMapAtUnsafeRefMethodAliasNonLocalReceiverMethodStmt.name = "insert";
  mapInsertArgsPackMapAtUnsafeRefMethodAliasNonLocalReceiverMethodStmt.isMethodCall = true;
  mapInsertArgsPackMapAtUnsafeRefMethodAliasNonLocalReceiverMethodStmt.args = {
      mapsPackMapAtUnsafeRefMethodAliasNonLocalReceiverExpr, keyArg, valueArg};
  mapInsertArgsPackMapAtUnsafeRefMethodAliasNonLocalReceiverMethodStmt.argNames = {std::nullopt,
                                                                                    std::nullopt,
                                                                                    std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertArgsPackMapAtUnsafeRefMethodAliasNonLocalReceiverMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "mapAtUnsafeRef" && callExpr.args.size() == 2) {
                return &mapAtUnsafeRefAliasArgsPackDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr valuesFactoryLocationExpr;
  valuesFactoryLocationExpr.kind = primec::Expr::Kind::Call;
  valuesFactoryLocationExpr.name = "location";
  valuesFactoryLocationExpr.args = {valuesFactoryCall};
  valuesFactoryLocationExpr.argNames = {std::nullopt};

  primec::Expr mapInsertLocationHelperInferredStmt = mapInsertStmt;
  mapInsertLocationHelperInferredStmt.args = {valuesFactoryLocationExpr, keyArg, valueArg};
  mapInsertLocationHelperInferredStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};
  mapInsertLocationHelperInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertLocationHelperInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/main/makeValues") {
                return &mapValuesFactoryDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertLocationHelperMethodStmt;
  mapInsertLocationHelperMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertLocationHelperMethodStmt.name = "insert";
  mapInsertLocationHelperMethodStmt.isMethodCall = true;
  mapInsertLocationHelperMethodStmt.args = {valuesFactoryLocationExpr, keyArg, valueArg};
  mapInsertLocationHelperMethodStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertLocationHelperMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/main/makeValues") {
                return &mapValuesFactoryDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr holderValuesLocationExpr;
  holderValuesLocationExpr.kind = primec::Expr::Kind::Call;
  holderValuesLocationExpr.name = "location";
  holderValuesLocationExpr.args = {holderValuesFieldExpr};
  holderValuesLocationExpr.argNames = {std::nullopt};

  primec::Expr mapInsertLocationFieldAccessInferredStmt = mapInsertStmt;
  mapInsertLocationFieldAccessInferredStmt.args = {holderValuesLocationExpr, keyArg, valueArg};
  mapInsertLocationFieldAccessInferredStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};
  mapInsertLocationFieldAccessInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertLocationFieldAccessInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert") {
                return &mapInsertMethodDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertLocationFieldAccessMethodStmt;
  mapInsertLocationFieldAccessMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertLocationFieldAccessMethodStmt.name = "insert";
  mapInsertLocationFieldAccessMethodStmt.isMethodCall = true;
  mapInsertLocationFieldAccessMethodStmt.args = {holderValuesLocationExpr, keyArg, valueArg};
  mapInsertLocationFieldAccessMethodStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertLocationFieldAccessMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr valuesRefFactoryCall;
  valuesRefFactoryCall.kind = primec::Expr::Kind::Call;
  valuesRefFactoryCall.name = "/main/makeValuesRef";
  primec::Expr valuesRefDerefExpr;
  valuesRefDerefExpr.kind = primec::Expr::Kind::Call;
  valuesRefDerefExpr.name = "dereference";
  valuesRefDerefExpr.args = {valuesRefFactoryCall};
  valuesRefDerefExpr.argNames = {std::nullopt};
  primec::Expr valuesRefLocationExpr;
  valuesRefLocationExpr.kind = primec::Expr::Kind::Call;
  valuesRefLocationExpr.name = "location";
  valuesRefLocationExpr.args = {valuesRefFactoryCall};
  valuesRefLocationExpr.argNames = {std::nullopt};
  primec::Expr valuesRefLocationDerefExpr;
  valuesRefLocationDerefExpr.kind = primec::Expr::Kind::Call;
  valuesRefLocationDerefExpr.name = "dereference";
  valuesRefLocationDerefExpr.args = {valuesRefLocationExpr};
  valuesRefLocationDerefExpr.argNames = {std::nullopt};
  primec::Expr valuesRefNestedLocationDerefExpr;
  valuesRefNestedLocationDerefExpr.kind = primec::Expr::Kind::Call;
  valuesRefNestedLocationDerefExpr.name = "location";
  valuesRefNestedLocationDerefExpr.args = {valuesRefLocationDerefExpr};
  valuesRefNestedLocationDerefExpr.argNames = {std::nullopt};

  primec::Expr mapInsertNestedLocationDerefHelperStmt = mapInsertStmt;
  mapInsertNestedLocationDerefHelperStmt.args = {valuesRefNestedLocationDerefExpr, keyArg, valueArg};
  mapInsertNestedLocationDerefHelperStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};
  mapInsertNestedLocationDerefHelperStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertNestedLocationDerefHelperStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/main/makeValuesRef") {
                return &mapValuesRefFactoryDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertNestedLocationDerefHelperMethodStmt;
  mapInsertNestedLocationDerefHelperMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertNestedLocationDerefHelperMethodStmt.name = "insert";
  mapInsertNestedLocationDerefHelperMethodStmt.isMethodCall = true;
  mapInsertNestedLocationDerefHelperMethodStmt.args = {valuesRefNestedLocationDerefExpr, keyArg, valueArg};
  mapInsertNestedLocationDerefHelperMethodStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertNestedLocationDerefHelperMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/main/makeValuesRef") {
                return &mapValuesRefFactoryDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertDerefHelperStmt = mapInsertStmt;
  mapInsertDerefHelperStmt.args = {valuesRefDerefExpr, keyArg, valueArg};
  mapInsertDerefHelperStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};
  mapInsertDerefHelperStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertDerefHelperStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/main/makeValuesRef") {
                return &mapValuesRefFactoryDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertDerefHelperMethodStmt;
  mapInsertDerefHelperMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertDerefHelperMethodStmt.name = "insert";
  mapInsertDerefHelperMethodStmt.isMethodCall = true;
  mapInsertDerefHelperMethodStmt.args = {valuesRefDerefExpr, keyArg, valueArg};
  mapInsertDerefHelperMethodStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertDerefHelperMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/main/makeValuesRef") {
                return &mapValuesRefFactoryDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr holderValuesRefFieldExpr;
  holderValuesRefFieldExpr.kind = primec::Expr::Kind::Call;
  holderValuesRefFieldExpr.name = "valuesRef";
  holderValuesRefFieldExpr.isFieldAccess = true;
  holderValuesRefFieldExpr.args = {holderName};
  holderValuesRefFieldExpr.argNames = {std::nullopt};

  primec::Expr holderValuesRefDerefExpr;
  holderValuesRefDerefExpr.kind = primec::Expr::Kind::Call;
  holderValuesRefDerefExpr.name = "dereference";
  holderValuesRefDerefExpr.args = {holderValuesRefFieldExpr};
  holderValuesRefDerefExpr.argNames = {std::nullopt};
  primec::Expr holderValuesRefLocationExpr;
  holderValuesRefLocationExpr.kind = primec::Expr::Kind::Call;
  holderValuesRefLocationExpr.name = "location";
  holderValuesRefLocationExpr.args = {holderValuesRefFieldExpr};
  holderValuesRefLocationExpr.argNames = {std::nullopt};
  primec::Expr holderValuesRefLocationDerefExpr;
  holderValuesRefLocationDerefExpr.kind = primec::Expr::Kind::Call;
  holderValuesRefLocationDerefExpr.name = "dereference";
  holderValuesRefLocationDerefExpr.args = {holderValuesRefLocationExpr};
  holderValuesRefLocationDerefExpr.argNames = {std::nullopt};
  primec::Expr holderValuesRefNestedLocationDerefExpr;
  holderValuesRefNestedLocationDerefExpr.kind = primec::Expr::Kind::Call;
  holderValuesRefNestedLocationDerefExpr.name = "location";
  holderValuesRefNestedLocationDerefExpr.args = {holderValuesRefLocationDerefExpr};
  holderValuesRefNestedLocationDerefExpr.argNames = {std::nullopt};

  primec::Expr mapInsertNestedLocationDerefFieldAccessStmt = mapInsertStmt;
  mapInsertNestedLocationDerefFieldAccessStmt.args = {holderValuesRefNestedLocationDerefExpr, keyArg, valueArg};
  mapInsertNestedLocationDerefFieldAccessStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};
  mapInsertNestedLocationDerefFieldAccessStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertNestedLocationDerefFieldAccessStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert") {
                return &mapInsertMethodDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertNestedLocationDerefFieldAccessMethodStmt;
  mapInsertNestedLocationDerefFieldAccessMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertNestedLocationDerefFieldAccessMethodStmt.name = "insert";
  mapInsertNestedLocationDerefFieldAccessMethodStmt.isMethodCall = true;
  mapInsertNestedLocationDerefFieldAccessMethodStmt.args = {holderValuesRefNestedLocationDerefExpr, keyArg, valueArg};
  mapInsertNestedLocationDerefFieldAccessMethodStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertNestedLocationDerefFieldAccessMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertDerefFieldAccessInferredStmt = mapInsertStmt;
  mapInsertDerefFieldAccessInferredStmt.args = {holderValuesRefDerefExpr, keyArg, valueArg};
  mapInsertDerefFieldAccessInferredStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};
  mapInsertDerefFieldAccessInferredStmt.templateArgs.clear();

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertDerefFieldAccessInferredStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert") {
                return &mapInsertMethodDef;
              }
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  primec::Expr mapInsertDerefFieldAccessMethodStmt;
  mapInsertDerefFieldAccessMethodStmt.kind = primec::Expr::Kind::Call;
  mapInsertDerefFieldAccessMethodStmt.name = "insert";
  mapInsertDerefFieldAccessMethodStmt.isMethodCall = true;
  mapInsertDerefFieldAccessMethodStmt.args = {holderValuesRefDerefExpr, keyArg, valueArg};
  mapInsertDerefFieldAccessMethodStmt.argNames = {std::nullopt, std::nullopt, std::nullopt};

  inlineCalls = 0;
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            mapInsertDerefFieldAccessMethodStmt,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              if (callExpr.isMethodCall && callExpr.name == "insert" && callExpr.args.size() == 3) {
                return &mapInsertAliasDef;
              }
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "/std/collections/map/insert_builtin") {
                return &mapInsertBuiltinDef;
              }
              return nullptr;
            },
            [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
              if (path == "/std/collections/map/insert_builtin") {
                info.returnsVoid = true;
                return true;
              }
              return false;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &,
                bool expectValue) {
              ++inlineCalls;
              const std::vector<std::string> expectedTemplateArgs{"i32", "i32"};
              CHECK(callExpr.name == "/std/collections/map/insert_builtin");
              CHECK_FALSE(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/std/collections/map/insert_builtin");
              CHECK_FALSE(expectValue);
              CHECK(callExpr.templateArgs == expectedTemplateArgs);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

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

  primec::Definition soaFieldDef;
  soaFieldDef.fullPath = "/soa_vector/x";
  int methodResolutionCalls = 0;
  int definitionResolutionCalls = 0;
  inlineCalls = 0;
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            soaFieldStmt,
            soaLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &callExpr,
                const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              ++methodResolutionCalls;
              CHECK(callExpr.isMethodCall);
              CHECK(callExpr.name == "x");
              return &soaFieldDef;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++definitionResolutionCalls;
              return nullptr;
            },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return true; },
            [&](const primec::Expr &callExpr,
                const primec::Definition &callee,
                const primec::ir_lowerer::LocalMap &localsIn,
                bool expectValue) {
              ++inlineCalls;
              CHECK(callExpr.isMethodCall);
              CHECK(callee.fullPath == "/soa_vector/x");
              CHECK(localsIn.find("values") != localsIn.end());
              CHECK_FALSE(expectValue);
              return true;
            },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(methodResolutionCalls == 1);
  CHECK(definitionResolutionCalls == 0);
  CHECK(inlineCalls == 1);
  CHECK(instructions.empty());

  const auto runVectorMutatorAliasNotHandledCase =
      [&](const std::string &aliasName, const std::vector<primec::Expr> &args) {
    primec::Expr aliasStmt;
    aliasStmt.kind = primec::Expr::Kind::Call;
    aliasStmt.name = aliasName;
    aliasStmt.args = args;
    aliasStmt.argNames.resize(args.size(), std::nullopt);

    primec::ir_lowerer::LocalMap locals;
    primec::ir_lowerer::LocalInfo valuesInfo;
    valuesInfo.isSoaVector = true;
    locals.emplace("values", valuesInfo);

    int aliasMethodResolutionCalls = 0;
    int aliasDefinitionResolutionCalls = 0;
    int aliasInlineCalls = 0;
    error.clear();
    instructions.clear();
    CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
              aliasStmt,
              locals,
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              [&](const primec::Expr &callExpr,
                  const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
                ++aliasMethodResolutionCalls;
                CHECK(callExpr.isMethodCall);
                return nullptr;
              },
              [&](const primec::Expr &) -> const primec::Definition * {
                ++aliasDefinitionResolutionCalls;
                return nullptr;
              },
              [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return true; },
              [&](const primec::Expr &,
                  const primec::Definition &,
                  const primec::ir_lowerer::LocalMap &,
                  bool) {
                ++aliasInlineCalls;
                return true;
              },
              instructions,
              error) == EmitResult::NotMatched);
    CHECK(error.empty());
    CHECK(aliasMethodResolutionCalls >= 0);
    CHECK(aliasMethodResolutionCalls <= 2);
    CHECK(aliasDefinitionResolutionCalls == 1);
    CHECK(aliasInlineCalls == 0);
    CHECK(instructions.empty());
  };

  const auto makeValuesArg = [] {
    primec::Expr valuesArg;
    valuesArg.kind = primec::Expr::Kind::Name;
    valuesArg.name = "values";
    return valuesArg;
  };
  const auto makeValueArg = [] {
    primec::Expr valueArg;
    valueArg.kind = primec::Expr::Kind::BoolLiteral;
    valueArg.boolValue = true;
    return valueArg;
  };

  runVectorMutatorAliasNotHandledCase("/vector/push", {makeValueArg(), makeValuesArg()});
  runVectorMutatorAliasNotHandledCase("/std/collections/vector/push", {makeValueArg(), makeValuesArg()});
  runVectorMutatorAliasNotHandledCase(
      "/std/collections/vectorPush",
      {makeValuesArg(), makeValueArg()});
  runVectorMutatorAliasNotHandledCase(
      "/std/collections/vectorPop",
      {makeValuesArg()});
  runVectorMutatorAliasNotHandledCase(
      "/std/collections/vectorReserve",
      {makeValuesArg(), makeValueArg()});
  runVectorMutatorAliasNotHandledCase(
      "/std/collections/vectorClear",
      {makeValuesArg()});
  runVectorMutatorAliasNotHandledCase(
      "/std/collections/vectorRemoveAt",
      {makeValuesArg(), makeValueArg()});
  runVectorMutatorAliasNotHandledCase(
      "/std/collections/vectorRemoveSwap",
      {makeValuesArg(), makeValueArg()});
  runVectorMutatorAliasNotHandledCase(
      "/std/collections/experimental_vector/vectorPush",
      {makeValueArg(), makeValuesArg()});
  runVectorMutatorAliasNotHandledCase("/vector/clear", {makeValuesArg()});
  runVectorMutatorAliasNotHandledCase("/std/collections/vector/clear", {makeValuesArg()});
  runVectorMutatorAliasNotHandledCase(
      "/std/collections/experimental_vector/vectorClear",
      {makeValuesArg()});

  const auto runExplicitVectorMutatorDirectDefinitionCase =
      [&](const std::string &helperName, const std::vector<primec::Expr> &args) {
        primec::Expr helperStmt;
        helperStmt.kind = primec::Expr::Kind::Call;
        helperStmt.name = helperName;
        helperStmt.args = args;
        helperStmt.argNames.resize(args.size(), std::nullopt);

        primec::ir_lowerer::LocalMap locals;
        primec::ir_lowerer::LocalInfo valuesInfo;
        valuesInfo.isSoaVector = true;
        locals.emplace("values", valuesInfo);

        primec::Definition helperDef;
        helperDef.fullPath = helperName;

        int helperMethodResolutionCalls = 0;
        int helperDefinitionResolutionCalls = 0;
        int helperInlineCalls = 0;
        error.clear();
        instructions.clear();
        CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
                  helperStmt,
                  locals,
                  [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
                  [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
                  [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
                  [&](const primec::Expr &callExpr,
                      const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
                    ++helperMethodResolutionCalls;
                    CHECK(callExpr.isMethodCall);
                    return nullptr;
                  },
                  [&](const primec::Expr &callExpr) -> const primec::Definition * {
                    ++helperDefinitionResolutionCalls;
                    CHECK(callExpr.name == helperName);
                    CHECK_FALSE(callExpr.isMethodCall);
                    return &helperDef;
                  },
                  [&](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
                    CHECK(path == helperName);
                    info.returnsVoid = true;
                    return true;
                  },
                  [&](const primec::Expr &callExpr,
                      const primec::Definition &callee,
                      const primec::ir_lowerer::LocalMap &localsIn,
                      bool expectValue) {
                    ++helperInlineCalls;
                    CHECK(callExpr.name == helperName);
                    CHECK_FALSE(callExpr.isMethodCall);
                    CHECK(callee.fullPath == helperName);
                    CHECK(localsIn.find("values") != localsIn.end());
                    CHECK_FALSE(expectValue);
                    return true;
                  },
                  instructions,
                  error) == EmitResult::Emitted);
        CHECK(error.empty());
        CHECK(helperMethodResolutionCalls == 0);
        CHECK(helperDefinitionResolutionCalls == 1);
        CHECK(helperInlineCalls == 1);
        CHECK(instructions.empty());
      };

  runExplicitVectorMutatorDirectDefinitionCase("/vector/push", {makeValueArg(), makeValuesArg()});
  runExplicitVectorMutatorDirectDefinitionCase(
      "/std/collections/vector/clear", {makeValuesArg()});
  runExplicitVectorMutatorDirectDefinitionCase(
      "/std/collections/experimental_vector/vectorPush",
      {makeValueArg(), makeValuesArg()});

  primec::Expr wrapperAliasPushStmt;
  wrapperAliasPushStmt.kind = primec::Expr::Kind::Call;
  wrapperAliasPushStmt.name = "/std/collections/vectorPush";
  wrapperAliasPushStmt.args = {makeValuesArg(), makeValueArg()};
  wrapperAliasPushStmt.argNames = {std::nullopt, std::nullopt};

  primec::ir_lowerer::LocalMap wrapperBuiltinVectorLocals;
  primec::ir_lowerer::LocalInfo wrapperBuiltinVectorInfo;
  wrapperBuiltinVectorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  wrapperBuiltinVectorLocals.emplace("values", wrapperBuiltinVectorInfo);

  int wrapperBuiltinVectorEmitCalls = 0;
  int wrapperBuiltinVectorMethodResolutionCalls = 0;
  int wrapperBuiltinVectorDefinitionResolutionCalls = 0;
  primec::Expr observedWrapperBuiltinVectorExpr;
  error.clear();
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            wrapperAliasPushStmt,
            wrapperBuiltinVectorLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              ++wrapperBuiltinVectorEmitCalls;
              observedWrapperBuiltinVectorExpr = expr;
              return true;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              ++wrapperBuiltinVectorMethodResolutionCalls;
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++wrapperBuiltinVectorDefinitionResolutionCalls;
              return nullptr;
            },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return true; },
            [&](const primec::Expr &,
                const primec::Definition &,
                const primec::ir_lowerer::LocalMap &,
                bool) {
              return true;
            },
            instructions,
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(wrapperBuiltinVectorEmitCalls == 0);
  CHECK(wrapperBuiltinVectorMethodResolutionCalls >= 1);
  CHECK(wrapperBuiltinVectorMethodResolutionCalls <= 2);
  CHECK(wrapperBuiltinVectorDefinitionResolutionCalls == 1);
  CHECK(observedWrapperBuiltinVectorExpr.name.empty());
  CHECK(observedWrapperBuiltinVectorExpr.namespacePrefix.empty());
  CHECK(observedWrapperBuiltinVectorExpr.args.empty());

  primec::Expr namespacedCanonicalPushStmt;
  namespacedCanonicalPushStmt.kind = primec::Expr::Kind::Call;
  namespacedCanonicalPushStmt.name = "push";
  namespacedCanonicalPushStmt.namespacePrefix = "/std/collections/vector";
  namespacedCanonicalPushStmt.args = {makeValueArg(), makeValuesArg()};
  namespacedCanonicalPushStmt.argNames = {std::nullopt, std::nullopt};

  primec::ir_lowerer::LocalMap builtinVectorLocals;
  primec::ir_lowerer::LocalInfo builtinVectorInfo;
  builtinVectorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  builtinVectorLocals.emplace("values", builtinVectorInfo);

  int builtinVectorEmitCalls = 0;
  int builtinVectorMethodResolutionCalls = 0;
  int builtinVectorDefinitionResolutionCalls = 0;
  primec::Expr observedBuiltinVectorExpr;
  error.clear();
  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitDirectCallStatement(
            namespacedCanonicalPushStmt,
            builtinVectorLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              ++builtinVectorEmitCalls;
              observedBuiltinVectorExpr = expr;
              return true;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              ++builtinVectorMethodResolutionCalls;
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++builtinVectorDefinitionResolutionCalls;
              return nullptr;
            },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return true; },
            [&](const primec::Expr &,
                const primec::Definition &,
                const primec::ir_lowerer::LocalMap &,
                bool) {
              return true;
            },
            instructions,
            error) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(builtinVectorEmitCalls == 0);
  CHECK(builtinVectorMethodResolutionCalls == 0);
  CHECK(builtinVectorDefinitionResolutionCalls == 1);
  CHECK(observedBuiltinVectorExpr.name.empty());
  CHECK(observedBuiltinVectorExpr.namespacePrefix.empty());
  CHECK(observedBuiltinVectorExpr.args.empty());
}

TEST_CASE("ir lowerer statement call emission source delegation stays stable") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };
  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path statementCallEmissionPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererStatementCallEmission.cpp";
  REQUIRE(std::filesystem::exists(statementCallEmissionPath));

  const std::string source = readText(statementCallEmissionPath);
  CHECK(source.find("#include \"primec/StdlibSurfaceRegistry.h\"") !=
        std::string::npos);
  CHECK(source.find("resolvePublishedStatementVectorHelperName(") !=
        std::string::npos);
  CHECK(source.find("resolveStatementCallPathWithoutFallbackProbes(") !=
        std::string::npos);
  CHECK(source.find("resolvePublishedStatementMapHelperName(") !=
        std::string::npos);
  CHECK(source.find("resolvePublishedStdlibSurfaceExprMemberName(") !=
        std::string::npos);
  CHECK(source.find("std::string helperName =\n        resolveStatementCallPathWithoutFallbackProbes(callExpr);") !=
        std::string::npos);
  CHECK(source.find("std::string normalizedName =\n        resolveStatementCallPathWithoutFallbackProbes(stmt);") !=
        std::string::npos);
  CHECK(source.find("std::string directName = resolveStatementCallPathWithoutFallbackProbes(expr);") !=
        std::string::npos);
  CHECK(source.find("std::string methodName = resolveStatementCallPathWithoutFallbackProbes(expr);") !=
        std::string::npos);
  CHECK(source.find("findStdlibSurfaceMetadataByResolvedPath(resolvedPath)") !=
        std::string::npos);
  CHECK(source.find("metadata->id != StdlibSurfaceId::CollectionsVectorHelpers") !=
        std::string::npos);
  CHECK(source.find("metadata->id != StdlibSurfaceId::CollectionsMapHelpers") !=
        std::string::npos);
  CHECK(source.find("matchesRegistrySpellingSet(metadata->loweringSpellings, resolvedPath)") !=
        std::string::npos);
  CHECK(source.find("isPublishedWrapperStatementVectorMutatorAliasPath(") !=
        std::string::npos);
  CHECK(source.find("explicitVectorMutatorHelperCall && !explicitWrapperVectorMutatorHelperPath") !=
        std::string::npos);
  CHECK(source.find("return resolvePublishedStatementVectorHelperName(expr.name, helperNameOut);") ==
        std::string::npos);
  CHECK(source.find("std::string helperName = callExpr.name;") !=
        std::string::npos);
  CHECK(source.find("std::string normalizedName = stmt.name;") ==
        std::string::npos);
  CHECK(source.find("std::string directName = expr.name;") ==
        std::string::npos);
  CHECK(source.find("std::string methodName = expr.name;") ==
        std::string::npos);
  CHECK(source.find("const std::string experimentalVectorPrefix = \"std/collections/experimental_vector/\"") ==
        std::string::npos);
  CHECK(source.find("expr.name == \"/map/at\" || expr.name == \"/std/collections/map/at\"") ==
        std::string::npos);
  CHECK(source.find("callee.fullPath == \"/std/collections/map/insert\"") ==
        std::string::npos);
}

TEST_SUITE_END();
