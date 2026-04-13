#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer flow helpers emit vector statement helpers through variadic vector receivers") {
  using EmitResult = primec::ir_lowerer::VectorStatementHelperEmitResult;
  using Kind = primec::ir_lowerer::LocalInfo::Kind;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  auto makeName = [](const std::string &name) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    return expr;
  };
  auto makeI32Literal = [](uint64_t value) {
    primec::Expr literal;
    literal.kind = primec::Expr::Kind::Literal;
    literal.intWidth = 32;
    literal.literalValue = value;
    return literal;
  };
  auto makeAccess = [&](const std::string &receiverName, uint64_t indexValue) {
    primec::Expr access;
    access.kind = primec::Expr::Kind::Call;
    access.name = "at";
    access.args = {makeName(receiverName), makeI32Literal(indexValue)};
    return access;
  };
  auto makeDeref = [&](primec::Expr target) {
    primec::Expr deref;
    deref.kind = primec::Expr::Kind::Call;
    deref.name = "dereference";
    deref.args = {std::move(target)};
    return deref;
  };
  auto makeCall = [&](const std::string &name, std::vector<primec::Expr> args) {
    primec::Expr call;
    call.kind = primec::Expr::Kind::Call;
    call.name = name;
    call.args = std::move(args);
    return call;
  };

  primec::ir_lowerer::LocalMap locals;

  primec::ir_lowerer::LocalInfo vectorPackInfo;
  vectorPackInfo.kind = Kind::Array;
  vectorPackInfo.isArgsPack = true;
  vectorPackInfo.argsPackElementKind = Kind::Vector;
  vectorPackInfo.valueKind = ValueKind::Int32;
  vectorPackInfo.index = 4;
  locals.emplace("values", vectorPackInfo);

  primec::ir_lowerer::LocalInfo borrowedVectorPackInfo = vectorPackInfo;
  borrowedVectorPackInfo.argsPackElementKind = Kind::Reference;
  borrowedVectorPackInfo.referenceToVector = true;
  borrowedVectorPackInfo.index = 8;
  locals.emplace("refs", borrowedVectorPackInfo);

  primec::ir_lowerer::LocalInfo pointerVectorPackInfo = vectorPackInfo;
  pointerVectorPackInfo.argsPackElementKind = Kind::Pointer;
  pointerVectorPackInfo.pointerToVector = true;
  pointerVectorPackInfo.index = 12;
  locals.emplace("ptrs", pointerVectorPackInfo);

  primec::ir_lowerer::LocalInfo scalarRefPackInfo = vectorPackInfo;
  scalarRefPackInfo.argsPackElementKind = Kind::Reference;
  scalarRefPackInfo.referenceToVector = false;
  scalarRefPackInfo.valueKind = ValueKind::Int32;
  scalarRefPackInfo.index = 16;
  locals.emplace("scalarRefs", scalarRefPackInfo);

  primec::ir_lowerer::LocalInfo soaRefPackInfo = borrowedVectorPackInfo;
  soaRefPackInfo.isSoaVector = true;
  soaRefPackInfo.index = 20;
  locals.emplace("soaRefs", soaRefPackInfo);

  auto inferExprKind = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
    if (expr.kind == primec::Expr::Kind::Literal) {
      return ValueKind::Int32;
    }
    return ValueKind::Unknown;
  };

  auto runHelper = [&](const primec::Expr &stmt,
                       int &emitExprCalls,
                       std::string &error,
                       std::vector<primec::IrInstruction> &instructions) {
    instructions.clear();
    int32_t nextTempLocal = 40;
    emitExprCalls = 0;
    return primec::ir_lowerer::tryEmitVectorStatementHelper(
        stmt,
        locals,
        instructions,
        [&]() { return nextTempLocal++; },
        inferExprKind,
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
        [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          ++emitExprCalls;
          instructions.push_back({primec::IrOpcode::PushI64, 77});
          return true;
        },
        [](const primec::Expr &) { return false; },
        [] {},
        [] {},
        [] {},
        [] {},
        [] {},
        error);
  };

  int emitExprCalls = 0;
  std::string error;
  std::vector<primec::IrInstruction> instructions;

  CHECK(runHelper(
            makeCall("push", {makeAccess("values", 0), makeI32Literal(7)}),
            emitExprCalls,
            error,
            instructions) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls >= 2);
  CHECK_FALSE(instructions.empty());

  error.clear();
  CHECK(runHelper(
            makeCall("clear", {makeDeref(makeAccess("refs", 1))}),
            emitExprCalls,
            error,
            instructions) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls >= 1);
  CHECK_FALSE(instructions.empty());

  error.clear();
  CHECK(runHelper(
            makeCall("remove_swap", {makeDeref(makeAccess("ptrs", 2)), makeI32Literal(0)}),
            emitExprCalls,
            error,
            instructions) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls >= 2);
  CHECK_FALSE(instructions.empty());

  error.clear();
  CHECK(runHelper(
            makeCall("push", {makeDeref(makeAccess("scalarRefs", 0)), makeI32Literal(7)}),
            emitExprCalls,
            error,
            instructions) == EmitResult::Error);
  CHECK(error == "push requires mutable vector binding");

  error.clear();
  CHECK(runHelper(
            makeCall("push", {makeDeref(makeAccess("soaRefs", 0)), makeI32Literal(7)}),
            emitExprCalls,
            error,
            instructions) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls >= 2);
  CHECK_FALSE(instructions.empty());
}

TEST_CASE("ir lowerer flow helpers validate vector statement helper diagnostics") {
  using EmitResult = primec::ir_lowerer::VectorStatementHelperEmitResult;
  using Kind = primec::ir_lowerer::LocalInfo::Kind;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  auto makeName = [](const std::string &name) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    return expr;
  };

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo vectorInfo;
  vectorInfo.kind = Kind::Vector;
  vectorInfo.valueKind = ValueKind::Int32;
  vectorInfo.isMutable = false;
  vectorInfo.index = 4;
  locals.emplace("v", vectorInfo);

  std::vector<primec::IrInstruction> instructions;
  int32_t nextTempLocal = 30;
  std::string error;

  primec::Expr nonHelperCall;
  nonHelperCall.kind = primec::Expr::Kind::Call;
  nonHelperCall.name = "count";
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            nonHelperCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &) { return false; },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::NotMatched);
  CHECK(error.empty());

  primec::Expr pushCall;
  pushCall.kind = primec::Expr::Kind::Call;
  pushCall.name = "push";
  pushCall.args = {makeName("v"), makeName("value")};
  primec::ir_lowerer::LocalInfo arrayInfo;
  arrayInfo.kind = Kind::Array;
  arrayInfo.valueKind = ValueKind::Int32;
  arrayInfo.isMutable = true;
  arrayInfo.index = 10;
  locals.emplace("a", arrayInfo);

  primec::Expr pushArrayCall = pushCall;
  pushArrayCall.args = {makeName("a"), makeName("value")};
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            pushArrayCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &) { return false; },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::Error);
  CHECK(error == "push requires mutable vector binding");

  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            pushCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &) { return false; },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::Error);
  CHECK(error == "push requires mutable vector binding");

  primec::Expr stdlibPushCall = pushCall;
  stdlibPushCall.name = "/std/collections/vector/push";
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            stdlibPushCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &) { return false; },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::Error);
  CHECK(error == "push requires mutable vector binding");

  primec::ir_lowerer::LocalInfo soaInfo;
  soaInfo.kind = Kind::Value;
  soaInfo.valueKind = ValueKind::Unknown;
  soaInfo.isMutable = true;
  soaInfo.index = 8;
  soaInfo.isSoaVector = true;
  locals.emplace("soa", soaInfo);

  primec::Expr soaPushCall;
  soaPushCall.kind = primec::Expr::Kind::Call;
  soaPushCall.name = "push";
  soaPushCall.args = {makeName("soa"), makeName("value")};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            soaPushCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &) { return false; },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::Error);
  CHECK(error == "push requires mutable vector binding");

  primec::Expr explicitSoaPushCall = soaPushCall;
  explicitSoaPushCall.name = "/soa_vector/push";
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            explicitSoaPushCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &) { return false; },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::Error);
  CHECK(error == "push requires mutable vector binding");

  primec::Expr canonicalSoaPushCall = soaPushCall;
  canonicalSoaPushCall.name = "/std/collections/soa_vector/push";
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            canonicalSoaPushCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &) { return false; },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::Error);
  CHECK(error == "push requires mutable vector binding");

  locals.at("v").isMutable = true;
  pushCall.templateArgs = {"i32"};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            pushCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &) { return false; },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::Error);
  CHECK(error == "push does not accept template arguments");

  primec::Expr reserveCall;
  reserveCall.kind = primec::Expr::Kind::Call;
  reserveCall.name = "reserve";
  reserveCall.args = {makeName("v"), makeName("cap")};
  primec::Expr reserveArrayCall = reserveCall;
  reserveArrayCall.args = {makeName("a"), makeName("cap")};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            reserveArrayCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &) { return false; },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::Error);
  CHECK(error == "reserve requires mutable vector binding");

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            reserveCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Float32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &) { return false; },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::Error);
  CHECK(error == "reserve requires integer capacity");

  primec::Expr explicitSoaReserveCall = reserveCall;
  explicitSoaReserveCall.name = "/soa_vector/reserve";
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            explicitSoaReserveCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &) { return false; },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::Emitted);
  CHECK(error.empty());

  primec::Expr removeAtCall;
  removeAtCall.kind = primec::Expr::Kind::Call;
  removeAtCall.name = "remove_at";
  removeAtCall.args = {makeName("v"), makeName("index")};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            removeAtCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Float32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &) { return false; },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::Error);
  CHECK(error == "remove_at requires integer index");

  primec::Expr removeSwapCall;
  removeSwapCall.kind = primec::Expr::Kind::Call;
  removeSwapCall.name = "remove_swap";
  removeSwapCall.args = {makeName("v"), makeName("index")};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitVectorStatementHelper(
            removeSwapCall,
            locals,
            instructions,
            [&]() { return nextTempLocal++; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return ValueKind::Float32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &) { return false; },
            [] {},
            [] {},
            [] {},
            [] {},
            [] {},
            error) == EmitResult::Error);
  CHECK(error == "remove_swap requires integer index");
}

TEST_SUITE_END();
