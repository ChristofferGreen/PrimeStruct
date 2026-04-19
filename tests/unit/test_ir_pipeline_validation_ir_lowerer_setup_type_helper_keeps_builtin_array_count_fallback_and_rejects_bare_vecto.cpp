#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer setup type helper keeps builtin array count fallback and rejects bare vector method fallback") {
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "count";
  methodCall.isMethodCall = true;
  methodCall.args.push_back(receiverExpr);

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo itemsLocal;
  itemsLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  locals.emplace("items", itemsLocal);

  std::string error = "stale";
  CHECK(primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
            methodCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            {},
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &) { return std::string(); },
            {},
            error) == nullptr);
  CHECK(error == "stale");

  primec::Expr vectorReceiverExpr;
  vectorReceiverExpr.kind = primec::Expr::Kind::Name;
  vectorReceiverExpr.name = "values";

  primec::ir_lowerer::LocalMap vectorLocals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  vectorLocals.emplace("values", valuesLocal);

  methodCall.name = "count";
  methodCall.args = {vectorReceiverExpr};
  error = "stale";
  CHECK(primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
            methodCall,
            vectorLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            {},
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &) { return std::string(); },
            {},
            error) == nullptr);
  CHECK(error == "unknown method: /std/collections/vector/count");

  methodCall.name = "capacity";
  methodCall.args = {vectorReceiverExpr};
  error = "stale";
  CHECK(primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
            methodCall,
            vectorLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            {},
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &) { return std::string(); },
            {},
            error) == nullptr);
  CHECK(error == "unknown method: /std/collections/vector/capacity");

  methodCall.name = "at";
  primec::Expr indexArg;
  indexArg.kind = primec::Expr::Kind::Literal;
  indexArg.intWidth = 32;
  indexArg.literalValue = 1;
  methodCall.args = {vectorReceiverExpr, indexArg};
  error = "stale";
  CHECK(primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
            methodCall,
            vectorLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            {},
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &) { return std::string(); },
            {},
            error) == nullptr);
  CHECK(error == "unknown method: /std/collections/vector/at");

  methodCall.name = "at_unsafe";
  methodCall.args = {vectorReceiverExpr, indexArg};
  error = "stale";
  CHECK(primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
            methodCall,
            vectorLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            {},
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &) { return std::string(); },
            {},
            error) == nullptr);
  CHECK(error == "unknown method: /std/collections/vector/at_unsafe");

  auto expectUnknownVectorMutatorMethod = [&](const char *methodName, const std::vector<primec::Expr> &args) {
    methodCall.name = methodName;
    methodCall.args = args;
    error = "stale";
    CHECK(primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
              methodCall,
              vectorLocals,
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
              {},
              {},
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
              },
              [](const primec::Expr &) { return std::string(); },
              {},
              error) == nullptr);
    CHECK(error == std::string("unknown method: /std/collections/vector/") + methodName);
  };

  expectUnknownVectorMutatorMethod("push", {vectorReceiverExpr, indexArg});
  expectUnknownVectorMutatorMethod("pop", {vectorReceiverExpr});
  expectUnknownVectorMutatorMethod("reserve", {vectorReceiverExpr, indexArg});
  expectUnknownVectorMutatorMethod("clear", {vectorReceiverExpr});
  expectUnknownVectorMutatorMethod("remove_at", {vectorReceiverExpr, indexArg});
  expectUnknownVectorMutatorMethod("remove_swap", {vectorReceiverExpr, indexArg});

  methodCall.name = "at";
  methodCall.args = {receiverExpr, indexArg};
  error = "stale";
  CHECK(primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
            methodCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            {},
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &) { return std::string(); },
            {},
            error) == nullptr);
  CHECK(error == "stale");

  primec::Expr entryArgsReceiver;
  entryArgsReceiver.kind = primec::Expr::Kind::Name;
  entryArgsReceiver.name = "argv";
  methodCall.name = "at_unsafe";
  methodCall.args = {entryArgsReceiver, indexArg};
  error = "stale";
  CHECK(primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
            methodCall,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              return expr.kind == primec::Expr::Kind::Name && expr.name == "argv";
            },
            {},
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &) { return std::string(); },
            {},
            error) == nullptr);
  CHECK(error == "stale");
}

TEST_CASE("ir lowerer setup type helper reports unknown vector mutators when no override definition exists") {
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";

  primec::Expr pushValueExpr;
  pushValueExpr.kind = primec::Expr::Kind::Literal;
  pushValueExpr.intWidth = 32;
  pushValueExpr.literalValue = 7;

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "push";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverExpr, pushValueExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  locals.emplace("values", valuesLocal);

  std::string error = "stale";
  CHECK(primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
            methodCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            {},
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &) { return std::string(); },
            {},
            error) == nullptr);
  CHECK(error == "unknown method: /std/collections/vector/push");

  methodCall.name = "pop";
  methodCall.args = {receiverExpr};
  error = "stale";
  CHECK(primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
            methodCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            {},
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &) { return std::string(); },
            {},
            error) == nullptr);
  CHECK(error == "unknown method: /std/collections/vector/pop");
}

TEST_CASE("ir lowerer setup type helper reports method call definition diagnostics from expressions") {
  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "count";
  methodCall.isMethodCall = true;

  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
            methodCall,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            {},
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &) { return std::string(); },
            {},
            error) == nullptr);
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";
  methodCall.args.push_back(receiverExpr);
  error.clear();
  CHECK(primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
            methodCall,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            {},
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &) { return std::string(); },
            {},
            error) == nullptr);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper resolves return info kinds by path") {
  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo arrayInfo;
  arrayInfo.returnsVoid = false;
  arrayInfo.returnsArray = true;
  arrayInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace("/defs/array", arrayInfo);

  primec::ir_lowerer::ReturnInfo scalarInfo;
  scalarInfo.returnsVoid = false;
  scalarInfo.returnsArray = false;
  scalarInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Float64;
  infoByPath.emplace("/defs/scalar", scalarInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveReturnInfoKindForPath("/defs/array", getReturnInfo, true, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveReturnInfoKindForPath("/defs/scalar", getReturnInfo, false, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK_FALSE(primec::ir_lowerer::resolveReturnInfoKindForPath("/defs/scalar", getReturnInfo, true, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK_FALSE(primec::ir_lowerer::resolveReturnInfoKindForPath("/defs/missing", getReturnInfo, false, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper resolves method call return kinds") {
  primec::Definition methodDef;
  methodDef.fullPath = "/array/count";

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo scalarInfo;
  scalarInfo.returnsVoid = false;
  scalarInfo.returnsArray = false;
  scalarInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  infoByPath.emplace("/array/count", scalarInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "count";
  methodCall.isMethodCall = true;

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool methodResolved = false;
  CHECK(primec::ir_lowerer::resolveMethodCallReturnKind(
      methodCall,
      {},
      [&methodDef](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return &methodDef; },
      {},
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  methodResolved = false;
  CHECK_FALSE(primec::ir_lowerer::resolveMethodCallReturnKind(
      methodCall,
      {},
      [&methodDef](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return &methodDef; },
      {},
      getReturnInfo,
      true,
      kindOut,
      &methodResolved));
  CHECK(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  methodResolved = true;
  CHECK_FALSE(primec::ir_lowerer::resolveMethodCallReturnKind(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return nullptr; },
      {},
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper resolves direct definition call return kinds") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  primec::Definition directDef;
  directDef.fullPath = "/pkg/value";
  defMap.emplace("/pkg/value", &directDef);

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo scalarInfo;
  scalarInfo.returnsVoid = false;
  scalarInfo.returnsArray = false;
  scalarInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace("/pkg/value", scalarInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "value";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool definitionMatched = false;
  CHECK(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      callExpr,
      defMap,
      [](const primec::Expr &) { return std::string("/pkg/value"); },
      getReturnInfo,
      false,
      kindOut,
      &definitionMatched));
  CHECK(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  definitionMatched = false;
  CHECK_FALSE(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      callExpr,
      defMap,
      [](const primec::Expr &) { return std::string("/pkg/value"); },
      getReturnInfo,
      true,
      kindOut,
      &definitionMatched));
  CHECK(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper rejects direct definition call return kinds via removed vector aliases") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  primec::Definition canonicalCountDef;
  canonicalCountDef.fullPath = "/std/collections/vector/count";
  primec::Definition canonicalAtDef;
  canonicalAtDef.fullPath = "/std/collections/vector/at";
  primec::Definition canonicalAtUnsafeDef;
  canonicalAtUnsafeDef.fullPath = "/std/collections/vector/at_unsafe";
  defMap.emplace("/std/collections/vector/count", &canonicalCountDef);
  defMap.emplace("/std/collections/vector/at", &canonicalAtDef);
  defMap.emplace("/std/collections/vector/at_unsafe", &canonicalAtUnsafeDef);

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo scalarInfo;
  scalarInfo.returnsVoid = false;
  scalarInfo.returnsArray = false;
  scalarInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
  infoByPath.emplace("/std/collections/vector/count", scalarInfo);

  primec::ir_lowerer::ReturnInfo accessInfo;
  accessInfo.returnsVoid = false;
  accessInfo.returnsArray = false;
  accessInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace("/std/collections/vector/at", accessInfo);
  infoByPath.emplace("/std/collections/vector/at_unsafe", accessInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/vector/count";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool definitionMatched = false;
  CHECK_FALSE(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      callExpr,
      defMap,
      [](const primec::Expr &expr) { return expr.name; },
      getReturnInfo,
      false,
      kindOut,
      &definitionMatched));
  CHECK_FALSE(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  definitionMatched = false;
  CHECK_FALSE(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      callExpr,
      defMap,
      [](const primec::Expr &expr) { return expr.name; },
      getReturnInfo,
      true,
      kindOut,
      &definitionMatched));
  CHECK_FALSE(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr atCall;
  atCall.kind = primec::Expr::Kind::Call;
  atCall.name = "/vector/at";

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  definitionMatched = false;
  CHECK_FALSE(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      atCall,
      defMap,
      [](const primec::Expr &expr) { return expr.name; },
      getReturnInfo,
      false,
      kindOut,
      &definitionMatched));
  CHECK_FALSE(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr atUnsafeCall;
  atUnsafeCall.kind = primec::Expr::Kind::Call;
  atUnsafeCall.name = "/vector/at_unsafe";

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  definitionMatched = false;
  CHECK_FALSE(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      atUnsafeCall,
      defMap,
      [](const primec::Expr &expr) { return expr.name; },
      getReturnInfo,
      false,
      kindOut,
      &definitionMatched));
  CHECK_FALSE(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper rejects slashless vector alias return kinds without alias definitions") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  primec::Definition canonicalCountDef;
  canonicalCountDef.fullPath = "/std/collections/vector/count";
  defMap.emplace("/std/collections/vector/count", &canonicalCountDef);

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo scalarInfo;
  scalarInfo.returnsVoid = false;
  scalarInfo.returnsArray = false;
  scalarInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
  infoByPath.emplace("/std/collections/vector/count", scalarInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "vector/count";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool definitionMatched = false;
  CHECK_FALSE(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      callExpr,
      defMap,
      [](const primec::Expr &expr) { return expr.name; },
      getReturnInfo,
      false,
      kindOut,
      &definitionMatched));
  CHECK_FALSE(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  definitionMatched = false;
  CHECK_FALSE(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      callExpr,
      defMap,
      [](const primec::Expr &expr) { return expr.name; },
      getReturnInfo,
      true,
      kindOut,
      &definitionMatched));
  CHECK_FALSE(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}


TEST_SUITE_END();
