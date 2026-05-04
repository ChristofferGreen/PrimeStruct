#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer call helpers dispatch inline calls with locals") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo marker;
  marker.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  marker.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("ctx", marker);

  primec::Definition callee;
  callee.fullPath = "/pkg/helper";

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "helper";
  methodCall.isMethodCall = true;

  std::string error;
  int emitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            methodCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &localMap) -> const primec::Definition * {
              CHECK(localMap.count("ctx") == 1);
              return &callee;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &resolvedCallee, const primec::ir_lowerer::LocalMap &localMap) {
              ++emitCalls;
              CHECK(localMap.count("ctx") == 1);
              CHECK(resolvedCallee.fullPath == "/pkg/helper");
              return true;
            },
            error) == Result::Emitted);
  CHECK(emitCalls == 1);

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            methodCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              CHECK(false);
              return false;
            },
            error) == Result::Error);

  primec::Expr methodCountCall;
  methodCountCall.kind = primec::Expr::Kind::Call;
  methodCountCall.name = "count";
  methodCountCall.isMethodCall = true;
  primec::Expr methodCountReceiver;
  methodCountReceiver.kind = primec::Expr::Kind::Name;
  methodCountReceiver.name = "ctx";
  methodCountCall.args.push_back(methodCountReceiver);
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            methodCountCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              return false;
            },
            error) == Result::NotHandled);
  CHECK(error == "stale");

  primec::ir_lowerer::LocalInfo soaCountInfo;
  soaCountInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  soaCountInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  soaCountInfo.isSoaVector = true;
  locals.emplace("soa_values", soaCountInfo);
  methodCountCall.args.front().name = "soa_values";
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            methodCountCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              CHECK(false);
              return false;
            },
            error) == Result::NotHandled);
  CHECK(error.empty());

  int soaEmitCalls = 0;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            methodCountCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return &callee;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &resolvedCallee, const primec::ir_lowerer::LocalMap &) {
              ++soaEmitCalls;
              CHECK(resolvedCallee.fullPath == "/pkg/helper");
              return true;
            },
            error) == Result::Emitted);
  CHECK(soaEmitCalls == 1);
  CHECK(error.empty());

  primec::Expr methodGetCall;
  methodGetCall.kind = primec::Expr::Kind::Call;
  methodGetCall.name = "get";
  methodGetCall.isMethodCall = true;
  methodGetCall.args.push_back(methodCountReceiver);
  primec::Expr indexArg;
  indexArg.kind = primec::Expr::Kind::Literal;
  indexArg.intWidth = 32;
  indexArg.literalValue = 0;
  methodGetCall.args.push_back(indexArg);
  methodGetCall.args.front().name = "soa_values";
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            methodGetCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              CHECK(false);
              return false;
            },
            error) == Result::Error);
  CHECK_FALSE(error.empty());

  int soaGetEmitCalls = 0;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            methodGetCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return &callee;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &resolvedCallee, const primec::ir_lowerer::LocalMap &) {
              ++soaGetEmitCalls;
              CHECK(resolvedCallee.fullPath == "/pkg/helper");
              return true;
            },
            error) == Result::Emitted);
  CHECK(soaGetEmitCalls == 1);
  CHECK(error.empty());

  primec::Expr methodRefCall = methodGetCall;
  methodRefCall.name = "ref";
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            methodRefCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              CHECK(false);
              return false;
            },
            error) == Result::Error);
  CHECK_FALSE(error.empty());

  int soaRefEmitCalls = 0;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            methodRefCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return &callee;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &resolvedCallee, const primec::ir_lowerer::LocalMap &) {
              ++soaRefEmitCalls;
              CHECK(resolvedCallee.fullPath == "/pkg/helper");
              return true;
            },
            error) == Result::Emitted);
  CHECK(soaRefEmitCalls == 1);
  CHECK(error.empty());

  primec::Expr callRefExpr;
  callRefExpr.kind = primec::Expr::Kind::Call;
  callRefExpr.name = "ref";
  callRefExpr.args.push_back(methodCountReceiver);
  callRefExpr.args.push_back(indexArg);
  callRefExpr.args.front().name = "soa_values";
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            callRefExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              CHECK(false);
              return false;
            },
            error) == Result::NotHandled);
  CHECK(error == "stale");

  int soaRefCallEmitCalls = 0;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            callRefExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return &callee;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &resolvedCallee, const primec::ir_lowerer::LocalMap &) {
              ++soaRefCallEmitCalls;
              CHECK(resolvedCallee.fullPath == "/pkg/helper");
              return true;
            },
            error) == Result::Emitted);
  CHECK(soaRefCallEmitCalls == 1);
  CHECK(error.empty());

  primec::Expr callFieldExpr;
  callFieldExpr.kind = primec::Expr::Kind::Call;
  callFieldExpr.name = "x";
  callFieldExpr.args.push_back(methodCountReceiver);
  callFieldExpr.args.front().name = "soa_values";
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            callFieldExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              CHECK(false);
              return false;
            },
            error) == Result::NotHandled);
  CHECK(error == "stale");

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            callFieldExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return &callee;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              CHECK(false);
              return false;
            },
            error) == Result::NotHandled);
  CHECK(error.empty());

  primec::Expr directCall;
  directCall.kind = primec::Expr::Kind::Call;
  directCall.name = "helper";
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            directCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * { return &callee; },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              return true;
            },
            error) == Result::Emitted);

  primec::ir_lowerer::LocalInfo mapInfo;
  mapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  mapInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  mapInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("items", mapInfo);

  primec::Expr mapReceiver;
  mapReceiver.kind = primec::Expr::Kind::Name;
  mapReceiver.name = "items";

  primec::Expr mapMethodContainsCall;
  mapMethodContainsCall.kind = primec::Expr::Kind::Call;
  mapMethodContainsCall.name = "contains";
  mapMethodContainsCall.isMethodCall = true;
  mapMethodContainsCall.args = {mapReceiver, indexArg};

  primec::Definition mapContainsCallee;
  mapContainsCallee.fullPath = "/std/collections/mapContains";

  int mapContainsResolveMethodCalls = 0;
  int mapContainsEmitCalls = 0;
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            mapMethodContainsCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              ++mapContainsResolveMethodCalls;
              return &mapContainsCallee;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              ++mapContainsEmitCalls;
              return true;
            },
            error) == Result::Emitted);
  CHECK(mapContainsResolveMethodCalls == 1);
  CHECK(error == "stale");
  CHECK(mapContainsEmitCalls == 1);

  primec::Expr mapMethodInsertCall;
  mapMethodInsertCall.kind = primec::Expr::Kind::Call;
  mapMethodInsertCall.name = "insert";
  mapMethodInsertCall.isMethodCall = true;
  mapMethodInsertCall.args = {mapReceiver, indexArg, indexArg};

  primec::Definition mapInsertRefCallee;
  mapInsertRefCallee.fullPath = "/std/collections/experimental_map/mapInsertRef";

  int mapInsertResolveMethodCalls = 0;
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            mapMethodInsertCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              ++mapInsertResolveMethodCalls;
              return &mapInsertRefCallee;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              CHECK(false);
              return false;
            },
            error) == Result::NotHandled);
  CHECK(mapInsertResolveMethodCalls == 1);
  CHECK(error == "stale");

  primec::Expr explicitVectorCountCall;
  explicitVectorCountCall.kind = primec::Expr::Kind::Call;
  explicitVectorCountCall.name = "/vector/count";
  primec::Expr explicitVectorCountArg;
  explicitVectorCountArg.kind = primec::Expr::Kind::Name;
  explicitVectorCountArg.name = "ctx";
  explicitVectorCountCall.args.push_back(explicitVectorCountArg);
  int explicitVectorCountLocalResolveMethodCalls = 0;
  int explicitVectorCountLocalResolveDefinitionCalls = 0;
  int explicitVectorCountLocalEmitCalls = 0;
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            explicitVectorCountCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              ++explicitVectorCountLocalResolveMethodCalls;
              return &callee;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++explicitVectorCountLocalResolveDefinitionCalls;
              return &callee;
            },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              ++explicitVectorCountLocalEmitCalls;
              return true;
            },
            error) == Result::Emitted);
  CHECK(error == "stale");
  CHECK(explicitVectorCountLocalResolveMethodCalls == 0);
  CHECK(explicitVectorCountLocalResolveDefinitionCalls == 1);
  CHECK(explicitVectorCountLocalEmitCalls == 1);

  primec::ir_lowerer::LocalInfo vectorInfo;
  vectorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  locals.emplace("values", vectorInfo);
  primec::Expr vectorReceiver;
  vectorReceiver.kind = primec::Expr::Kind::Name;
  vectorReceiver.name = "values";
  primec::Definition vectorCountCallee;
  vectorCountCallee.fullPath = "/vector/count";
  primec::Expr vectorCountMethodCall;
  vectorCountMethodCall.kind = primec::Expr::Kind::Call;
  vectorCountMethodCall.name = "count";
  vectorCountMethodCall.isMethodCall = true;
  vectorCountMethodCall.args.push_back(vectorReceiver);
  int vectorCountMethodResolveCalls = 0;
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            vectorCountMethodCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              ++vectorCountMethodResolveCalls;
              return &vectorCountCallee;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              CHECK(false);
              return false;
            },
            error) == Result::NotHandled);
  CHECK(vectorCountMethodResolveCalls == 0);
  CHECK(error == "stale");

  primec::Definition vectorCapacityCallee;
  vectorCapacityCallee.fullPath = "/vector/capacity";
  primec::Expr vectorCapacityMethodCall = vectorCountMethodCall;
  vectorCapacityMethodCall.name = "capacity";
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            vectorCapacityMethodCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return &vectorCapacityCallee;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              CHECK(false);
              return false;
            },
            error) == Result::NotHandled);
  CHECK(error == "stale");

  primec::Expr canonicalPushCall;
  canonicalPushCall.kind = primec::Expr::Kind::Call;
  canonicalPushCall.name = "/std/collections/vector/push";
  canonicalPushCall.args = {explicitVectorCountArg, indexArg};
  int canonicalPushLocalResolveMethodCalls = 0;
  int canonicalPushLocalResolveDefinitionCalls = 0;
  int canonicalPushLocalEmitCalls = 0;
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            canonicalPushCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              ++canonicalPushLocalResolveMethodCalls;
              return &callee;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++canonicalPushLocalResolveDefinitionCalls;
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              ++canonicalPushLocalEmitCalls;
              return true;
            },
            error) == Result::Emitted);
  CHECK(error == "stale");
  CHECK(canonicalPushLocalResolveMethodCalls == 1);
  CHECK(canonicalPushLocalResolveDefinitionCalls == 0);
  CHECK(canonicalPushLocalEmitCalls == 1);

  primec::Expr plainCall;
  plainCall.kind = primec::Expr::Kind::Call;
  plainCall.name = "plain";
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            plainCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              CHECK(false);
              return false;
            },
            error) == Result::NotHandled);
}

TEST_CASE("ir lowerer call helpers inline direct experimental map helper calls") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;
  using LocalInfo = primec::ir_lowerer::LocalInfo;

  primec::ir_lowerer::LocalMap locals;

  LocalInfo mapInfo;
  mapInfo.kind = LocalInfo::Kind::Map;
  mapInfo.mapKeyKind = LocalInfo::ValueKind::String;
  mapInfo.mapValueKind = LocalInfo::ValueKind::Int32;
  mapInfo.structTypeName = "/std/collections/experimental_map/Map__td48f7c0fb764e3c0";
  locals.emplace("values", mapInfo);

  LocalInfo keyInfo;
  keyInfo.kind = LocalInfo::Kind::Value;
  keyInfo.valueKind = LocalInfo::ValueKind::String;
  locals.emplace("key", keyInfo);

  primec::Definition callee;
  callee.fullPath = "/std/collections/experimental_map/mapAt__td48f7c0fb764e3c0";

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr keyName;
  keyName.kind = primec::Expr::Kind::Name;
  keyName.name = "key";

  primec::Expr directCall;
  directCall.kind = primec::Expr::Kind::Call;
  directCall.name = callee.fullPath;
  directCall.args = {valuesName, keyName};

  std::string error = "stale";
  int emitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            directCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &expr) -> const primec::Definition * {
              CHECK(expr.name == "/std/collections/experimental_map/mapAt__td48f7c0fb764e3c0");
              return &callee;
            },
            [&](const primec::Expr &, const primec::Definition &resolvedCallee, const primec::ir_lowerer::LocalMap &) {
              ++emitCalls;
              CHECK(resolvedCallee.fullPath ==
                    "/std/collections/experimental_map/mapAt__td48f7c0fb764e3c0");
              return true;
            },
            error) == Result::Emitted);
  CHECK(emitCalls == 1);
  CHECK(error == "stale");
}

TEST_CASE("ir lowerer call helpers keep direct canonical map count-like helpers on builtin fallback for map locals") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;
  using LocalInfo = primec::ir_lowerer::LocalInfo;

  primec::ir_lowerer::LocalMap locals;

  LocalInfo mapInfo;
  mapInfo.kind = LocalInfo::Kind::Map;
  mapInfo.mapKeyKind = LocalInfo::ValueKind::Int32;
  mapInfo.mapValueKind = LocalInfo::ValueKind::Int64;
  locals.emplace("values", mapInfo);

  LocalInfo keyInfo;
  keyInfo.kind = LocalInfo::Kind::Value;
  keyInfo.valueKind = LocalInfo::ValueKind::Int32;
  locals.emplace("key", keyInfo);

  primec::Definition canonicalCount;
  canonicalCount.fullPath = "/std/collections/map/count";

  primec::Definition canonicalContains;
  canonicalContains.fullPath = "/std/collections/map/contains";

  primec::Definition canonicalTryAt;
  canonicalTryAt.fullPath = "/std/collections/map/tryAt";

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr keyName;
  keyName.kind = primec::Expr::Kind::Name;
  keyName.name = "key";

  auto expectBuiltinFallback = [&](const primec::Expr &callExpr,
                                   const primec::Definition &resolvedCallee) {
    std::string error = "stale";
    int emitCalls = 0;
    CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
              callExpr,
              locals,
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
                CHECK(false);
                return nullptr;
              },
              [&](const primec::Expr &expr) -> const primec::Definition * {
                CHECK(expr.name == callExpr.name);
                CHECK(expr.namespacePrefix == callExpr.namespacePrefix);
                CHECK_FALSE(expr.isMethodCall);
                return &resolvedCallee;
              },
              [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
                ++emitCalls;
                return true;
              },
              error) == Result::NotHandled);
    CHECK(error == "stale");
    CHECK(emitCalls == 0);
  };

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.namespacePrefix = "/std/collections/map";
  countCall.name = "count";
  countCall.args = {valuesName};
  expectBuiltinFallback(countCall, canonicalCount);

  primec::Expr containsCall;
  containsCall.kind = primec::Expr::Kind::Call;
  containsCall.namespacePrefix = "/std/collections/map";
  containsCall.name = "contains";
  containsCall.args = {valuesName, keyName};
  expectBuiltinFallback(containsCall, canonicalContains);

  primec::Expr tryAtCall;
  tryAtCall.kind = primec::Expr::Kind::Call;
  tryAtCall.namespacePrefix = "/std/collections/map";
  tryAtCall.name = "tryAt";
  tryAtCall.args = {valuesName, keyName};
  expectBuiltinFallback(tryAtCall, canonicalTryAt);
}

TEST_CASE("ir lowerer call helpers emit unsupported native call diagnostics for stdlib-only helpers") {
  using Result = primec::ir_lowerer::UnsupportedNativeCallResult;

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  std::string error;

  callExpr.name = "count";
  CHECK(primec::ir_lowerer::emitUnsupportedNativeCallDiagnostic(
            callExpr,
            [](const primec::Expr &, std::string &) { return false; },
            error) == Result::Error);
  CHECK(error == "count requires array, vector, map, or string target (target=<none>)");

  primec::Expr scalarReceiver;
  scalarReceiver.kind = primec::Expr::Kind::Literal;
  scalarReceiver.intWidth = 32;
  scalarReceiver.literalValue = 3;
  callExpr.name = "/count";
  callExpr.args = {scalarReceiver};
  error.clear();
  CHECK(primec::ir_lowerer::emitUnsupportedNativeCallDiagnostic(
            callExpr,
            [](const primec::Expr &, std::string &) { return false; },
            error) == Result::NotHandled);
  CHECK(error.empty());
  callExpr.args.clear();

  callExpr.name = "capacity";
  error.clear();
  CHECK(primec::ir_lowerer::emitUnsupportedNativeCallDiagnostic(
            callExpr,
            [](const primec::Expr &, std::string &) { return false; },
            error) == Result::Error);
  CHECK(error == "capacity requires vector target");

  callExpr.name = "/vector/count";
  error.clear();
  CHECK(primec::ir_lowerer::emitUnsupportedNativeCallDiagnostic(
            callExpr,
            [](const primec::Expr &, std::string &) { return false; },
            error) == Result::NotHandled);
  CHECK(error.empty());

  callExpr.name = "/std/collections/vector/capacity";
  error.clear();
  CHECK(primec::ir_lowerer::emitUnsupportedNativeCallDiagnostic(
            callExpr,
            [](const primec::Expr &, std::string &) { return false; },
            error) == Result::Error);
  CHECK(error == "capacity requires vector target");

  primec::Expr helperReturnExpr;
  helperReturnExpr.kind = primec::Expr::Kind::Call;
  helperReturnExpr.name = "wrapVector";
  callExpr.args = {helperReturnExpr};
  error.clear();
  CHECK(primec::ir_lowerer::emitUnsupportedNativeCallDiagnostic(
            callExpr,
            [](const primec::Expr &, std::string &) { return false; },
            error) == Result::NotHandled);
  CHECK(error.empty());
  callExpr.args.clear();

  callExpr.name = "remove_at";
  error.clear();
  CHECK(primec::ir_lowerer::emitUnsupportedNativeCallDiagnostic(
            callExpr,
            [](const primec::Expr &, std::string &) { return false; },
            error) == Result::NotHandled);
  CHECK(error.empty());

  callExpr.name = "/std/collections/vector/remove_swap";
  error.clear();
  CHECK(primec::ir_lowerer::emitUnsupportedNativeCallDiagnostic(
            callExpr,
            [](const primec::Expr &, std::string &) { return false; },
            error) == Result::NotHandled);
  CHECK(error.empty());

  callExpr.name = "print";
  error.clear();
  CHECK(primec::ir_lowerer::emitUnsupportedNativeCallDiagnostic(
            callExpr,
            [](const primec::Expr &, std::string &builtinName) {
              builtinName = "print";
              return true;
            },
            error) == Result::Error);
  CHECK(error == "print is only supported as a statement in the native backend");

  callExpr.name = "plain";
  error.clear();
  CHECK(primec::ir_lowerer::emitUnsupportedNativeCallDiagnostic(
            callExpr,
            [](const primec::Expr &, std::string &) { return false; },
            error) == Result::NotHandled);
}

TEST_CASE("ir lowerer inline dispatch emits experimental vector element constructor aliases") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;

  primec::Expr aliasCtor;
  aliasCtor.kind = primec::Expr::Kind::Call;
  aliasCtor.namespacePrefix = "/std/collections/experimental_vector";
  aliasCtor.name = "i32";

  primec::Definition vectorCtor;
  vectorCtor.fullPath = "/std/collections/experimental_vector/vector";

  std::string error = "stale";
  int resolveDefinitionCalls = 0;
  int emitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            aliasCtor,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              ++resolveDefinitionCalls;
              CHECK(callExpr.name == "/std/collections/experimental_vector/vector");
              CHECK(callExpr.namespacePrefix.empty());
              REQUIRE(callExpr.templateArgs.size() == 1);
              CHECK(callExpr.templateArgs.front() == "i32");
              return &vectorCtor;
            },
            [&](const primec::Expr &callExpr,
                const primec::Definition &resolvedCallee,
                const primec::ir_lowerer::LocalMap &) {
              ++emitCalls;
              CHECK(callExpr.name == "/std/collections/experimental_vector/vector");
              REQUIRE(callExpr.templateArgs.size() == 1);
              CHECK(callExpr.templateArgs.front() == "i32");
              CHECK(resolvedCallee.fullPath ==
                    "/std/collections/experimental_vector/vector");
              return true;
            },
            error) == Result::Emitted);
  CHECK(resolveDefinitionCalls == 1);
  CHECK(emitCalls == 1);
  CHECK(error == "stale");
}

TEST_CASE("ir lowerer inline dispatch defers experimental vector count methods") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo experimentalVectorInfo{};
  experimentalVectorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  experimentalVectorInfo.valueKind =
      primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  experimentalVectorInfo.structTypeName =
      "/std/collections/experimental_vector/Vector__t25a78a513414c3bf";
  locals.emplace("values", experimentalVectorInfo);

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "values";

  primec::Expr methodCountCall;
  methodCountCall.kind = primec::Expr::Kind::Call;
  methodCountCall.name = "count";
  methodCountCall.isMethodCall = true;
  methodCountCall.args = {receiver};

  primec::Definition vectorCount;
  vectorCount.fullPath = "/vector/count";

  std::string error = "stale";
  int emitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            methodCountCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return &vectorCount;
            },
            [](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &,
                const primec::Definition &,
                const primec::ir_lowerer::LocalMap &) {
              ++emitCalls;
              return true;
            },
            error) == Result::NotHandled);
  CHECK(emitCalls == 0);
  CHECK(error == "stale");
}

TEST_CASE("ir lowerer inline dispatch defers experimental vector capacity methods") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo experimentalVectorInfo{};
  experimentalVectorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  experimentalVectorInfo.valueKind =
      primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  experimentalVectorInfo.structTypeName =
      "/std/collections/experimental_vector/Vector__t25a78a513414c3bf";
  locals.emplace("values", experimentalVectorInfo);

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "values";

  primec::Expr methodCapacityCall;
  methodCapacityCall.kind = primec::Expr::Kind::Call;
  methodCapacityCall.name = "capacity";
  methodCapacityCall.isMethodCall = true;
  methodCapacityCall.args = {receiver};

  primec::Definition vectorCapacity;
  vectorCapacity.fullPath = "/vector/capacity";

  std::string error = "stale";
  int emitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            methodCapacityCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return &vectorCapacity;
            },
            [](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &,
                const primec::Definition &,
                const primec::ir_lowerer::LocalMap &) {
              ++emitCalls;
              return true;
            },
            error) == Result::NotHandled);
  CHECK(emitCalls == 0);
  CHECK(error == "stale");
}

TEST_CASE("ir lowerer inline dispatch defers vector-returning temporary count methods") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;

  primec::Definition wrapVector;
  wrapVector.fullPath = "/wrapVector";
  wrapVector.transforms.push_back(
      primec::Transform{.name = "return", .templateArgs = {"vector<i32>"}});

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "wrapVector";

  primec::Expr methodCountCall;
  methodCountCall.kind = primec::Expr::Kind::Call;
  methodCountCall.name = "count";
  methodCountCall.isMethodCall = true;
  methodCountCall.args = {receiverCall};

  primec::Definition vectorCount;
  vectorCount.fullPath = "/vector/count";

  std::string error = "stale";
  int emitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            methodCountCall,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return &vectorCount;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              CHECK(callExpr.name == "wrapVector");
              return &wrapVector;
            },
            [&](const primec::Expr &,
                const primec::Definition &,
                const primec::ir_lowerer::LocalMap &) {
              ++emitCalls;
              return true;
            },
            error) == Result::NotHandled);
  CHECK(emitCalls == 0);
  CHECK(error == "stale");
}

TEST_CASE("ir lowerer inline dispatch defers vector-returning temporary capacity methods") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;

  primec::Definition wrapVector;
  wrapVector.fullPath = "/wrapVector";
  wrapVector.transforms.push_back(
      primec::Transform{.name = "return", .templateArgs = {"vector<i32>"}});

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "wrapVector";

  primec::Expr methodCapacityCall;
  methodCapacityCall.kind = primec::Expr::Kind::Call;
  methodCapacityCall.name = "capacity";
  methodCapacityCall.isMethodCall = true;
  methodCapacityCall.args = {receiverCall};

  primec::Definition vectorCapacity;
  vectorCapacity.fullPath = "/vector/capacity";

  std::string error = "stale";
  int emitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            methodCapacityCall,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return &vectorCapacity;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              CHECK(callExpr.name == "wrapVector");
              return &wrapVector;
            },
            [&](const primec::Expr &,
                const primec::Definition &,
                const primec::ir_lowerer::LocalMap &) {
              ++emitCalls;
              return true;
            },
            error) == Result::NotHandled);
  CHECK(emitCalls == 0);
  CHECK(error == "stale");
}

TEST_CASE("ir lowerer inline dispatch defers vector-returning temporary count calls") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;

  primec::Definition wrapVector;
  wrapVector.fullPath = "/wrapVector";
  wrapVector.transforms.push_back(
      primec::Transform{.name = "return", .templateArgs = {"vector<i32>"}});

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "wrapVector";

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "/std/collections/vector/count";
  countCall.args = {receiverCall};

  primec::Definition vectorCount;
  vectorCount.fullPath = "/std/collections/vector/count";

  std::string error = "stale";
  int emitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            countCall,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "wrapVector") {
                return &wrapVector;
              }
              return &vectorCount;
            },
            [&](const primec::Expr &,
                const primec::Definition &,
                const primec::ir_lowerer::LocalMap &) {
              ++emitCalls;
              return true;
            },
            error) == Result::NotHandled);
  CHECK(emitCalls == 0);
  CHECK(error == "stale");
}

TEST_CASE("ir lowerer inline dispatch defers vector-returning temporary capacity calls") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;

  primec::Definition wrapVector;
  wrapVector.fullPath = "/wrapVector";
  wrapVector.transforms.push_back(
      primec::Transform{.name = "return", .templateArgs = {"vector<i32>"}});

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "wrapVector";

  primec::Expr capacityCall;
  capacityCall.kind = primec::Expr::Kind::Call;
  capacityCall.name = "/std/collections/vector/capacity";
  capacityCall.args = {receiverCall};

  primec::Definition vectorCapacity;
  vectorCapacity.fullPath = "/std/collections/vector/capacity";

  std::string error = "stale";
  int emitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            capacityCall,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              if (callExpr.name == "wrapVector") {
                return &wrapVector;
              }
              return &vectorCapacity;
            },
            [&](const primec::Expr &,
                const primec::Definition &,
                const primec::ir_lowerer::LocalMap &) {
              ++emitCalls;
              return true;
            },
            error) == Result::NotHandled);
  CHECK(emitCalls == 0);
  CHECK(error == "stale");
}

TEST_CASE("ir lowerer inline dispatch defers vector-returning temporary access calls") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;

  primec::Definition wrapVector;
  wrapVector.fullPath = "/wrapVector";
  wrapVector.transforms.push_back(
      primec::Transform{.name = "return", .templateArgs = {"vector<i32>"}});

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "wrapVector";

  primec::Expr indexArg;
  indexArg.kind = primec::Expr::Kind::Literal;
  indexArg.intWidth = 32;
  indexArg.literalValue = 0;

  auto expectDeferred = [&](const std::string &callName,
                            const std::string &calleePath) {
    primec::Expr accessCall;
    accessCall.kind = primec::Expr::Kind::Call;
    accessCall.name = callName;
    accessCall.args = {receiverCall, indexArg};

    primec::Definition accessCallee;
    accessCallee.fullPath = calleePath;

    std::string error = "stale";
    int emitCalls = 0;
    CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
              accessCall,
              {},
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
                CHECK(false);
                return nullptr;
              },
              [&](const primec::Expr &callExpr) -> const primec::Definition * {
                if (callExpr.name == "wrapVector") {
                  return &wrapVector;
                }
                return &accessCallee;
              },
              [&](const primec::Expr &,
                  const primec::Definition &,
                  const primec::ir_lowerer::LocalMap &) {
                ++emitCalls;
                return true;
              },
              error) == Result::NotHandled);
    CHECK(emitCalls == 0);
    CHECK(error == "stale");
  };

  expectDeferred("/std/collections/vector/at", "/std/collections/vector/at");
  expectDeferred("/std/collections/vector/at_unsafe",
                 "/std/collections/vector/at_unsafe");
}

TEST_CASE("ir lowerer inline dispatch defers vector-returning temporary access methods") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;

  primec::Definition wrapVector;
  wrapVector.fullPath = "/wrapVector";
  wrapVector.transforms.push_back(
      primec::Transform{.name = "return", .templateArgs = {"vector<i32>"}});

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "wrapVector";

  primec::Expr indexArg;
  indexArg.kind = primec::Expr::Kind::Literal;
  indexArg.intWidth = 32;
  indexArg.literalValue = 0;

  auto expectDeferred = [&](const std::string &methodName,
                            const std::string &calleePath) {
    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.name = methodName;
    methodCall.isMethodCall = true;
    methodCall.args = {receiverCall, indexArg};

    primec::Definition accessCallee;
    accessCallee.fullPath = calleePath;

    std::string error = "stale";
    int emitCalls = 0;
    CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
              methodCall,
              {},
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
                return &accessCallee;
              },
              [&](const primec::Expr &callExpr) -> const primec::Definition * {
                CHECK(callExpr.name == "wrapVector");
                return &wrapVector;
              },
              [&](const primec::Expr &,
                  const primec::Definition &,
                  const primec::ir_lowerer::LocalMap &) {
                ++emitCalls;
                return true;
              },
              error) == Result::NotHandled);
    CHECK(emitCalls == 0);
    CHECK(error == "stale");
  };

  expectDeferred("at", "/std/collections/vector/at");
  expectDeferred("at_unsafe", "/std/collections/vector/at_unsafe");
}

TEST_CASE("ir lowerer inline dispatch defers vector-returning temporary mutators") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;

  primec::Definition wrapVector;
  wrapVector.fullPath = "/wrapVector";
  wrapVector.transforms.push_back(
      primec::Transform{.name = "return", .templateArgs = {"vector<i32>"}});

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "wrapVector";

  primec::Expr valueArg;
  valueArg.kind = primec::Expr::Kind::Literal;
  valueArg.intWidth = 32;
  valueArg.literalValue = 4;

  auto expectDeferred = [&](const std::string &callName,
                            std::vector<primec::Expr> args) {
    primec::Expr mutatorCall;
    mutatorCall.kind = primec::Expr::Kind::Call;
    mutatorCall.name = callName;
    mutatorCall.args = std::move(args);

    primec::Definition mutatorCallee;
    mutatorCallee.fullPath = callName;

    std::string error = "stale";
    int emitCalls = 0;
    CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
              mutatorCall,
              {},
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
                CHECK(false);
                return nullptr;
              },
              [&](const primec::Expr &callExpr) -> const primec::Definition * {
                if (callExpr.name == "wrapVector") {
                  return &wrapVector;
                }
                return &mutatorCallee;
              },
              [&](const primec::Expr &,
                  const primec::Definition &,
                  const primec::ir_lowerer::LocalMap &) {
                ++emitCalls;
                return true;
              },
              error) == Result::NotHandled);
    CHECK(emitCalls == 0);
    CHECK(error == "stale");
  };

  expectDeferred("/std/collections/vector/push", {receiverCall, valueArg});
  expectDeferred("/std/collections/vector/pop", {receiverCall});
  expectDeferred("/std/collections/vector/reserve", {receiverCall, valueArg});
  expectDeferred("/std/collections/vector/clear", {receiverCall});
  expectDeferred("/std/collections/vector/remove_at", {receiverCall, valueArg});
  expectDeferred("/std/collections/vector/remove_swap", {receiverCall, valueArg});
  expectDeferred("/std/collections/vectorPush", {receiverCall, valueArg});
  expectDeferred("/std/collections/vectorClear", {receiverCall});
}

TEST_CASE("ir lowerer inline dispatch defers vector-returning temporary mutator methods") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;

  primec::Definition wrapVector;
  wrapVector.fullPath = "/wrapVector";
  wrapVector.transforms.push_back(
      primec::Transform{.name = "return", .templateArgs = {"vector<i32>"}});

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "wrapVector";

  primec::Expr valueArg;
  valueArg.kind = primec::Expr::Kind::Literal;
  valueArg.intWidth = 32;
  valueArg.literalValue = 4;

  auto expectDeferred = [&](const std::string &methodName,
                            std::vector<primec::Expr> args) {
    primec::Expr mutatorCall;
    mutatorCall.kind = primec::Expr::Kind::Call;
    mutatorCall.name = methodName;
    mutatorCall.isMethodCall = true;
    mutatorCall.args = std::move(args);

    primec::Definition mutatorCallee;
    mutatorCallee.fullPath = "/std/collections/vector/" + methodName;

    std::string error = "stale";
    int emitCalls = 0;
    CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
              mutatorCall,
              {},
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
                CHECK(false);
                return &mutatorCallee;
              },
              [&](const primec::Expr &callExpr) -> const primec::Definition * {
                CHECK(callExpr.name == "wrapVector");
                return &wrapVector;
              },
              [&](const primec::Expr &,
                  const primec::Definition &,
                  const primec::ir_lowerer::LocalMap &) {
                ++emitCalls;
                return true;
              },
              error) == Result::NotHandled);
    CHECK(emitCalls == 0);
    CHECK(error == "stale");
  };

  expectDeferred("push", {receiverCall, valueArg});
  expectDeferred("pop", {receiverCall});
  expectDeferred("reserve", {receiverCall, valueArg});
  expectDeferred("clear", {receiverCall});
  expectDeferred("remove_at", {receiverCall, valueArg});
  expectDeferred("remove_swap", {receiverCall, valueArg});
  expectDeferred("/std/collections/vector/push", {receiverCall, valueArg});
  expectDeferred("/std/collections/vectorPush", {receiverCall, valueArg});
}

TEST_SUITE_END();
