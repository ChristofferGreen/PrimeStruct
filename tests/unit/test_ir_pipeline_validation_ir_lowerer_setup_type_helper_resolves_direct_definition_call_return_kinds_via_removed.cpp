#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer setup type helper resolves direct definition call return kinds via removed map aliases") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  primec::Definition canonicalCountDef;
  canonicalCountDef.fullPath = "/std/collections/map/count";
  primec::Definition canonicalContainsDef;
  canonicalContainsDef.fullPath = "/std/collections/map/contains";
  primec::Definition canonicalTryAtDef;
  canonicalTryAtDef.fullPath = "/std/collections/map/tryAt";
  primec::Definition canonicalAtDef;
  canonicalAtDef.fullPath = "/std/collections/map/at";
  defMap.emplace("/std/collections/map/count", &canonicalCountDef);
  defMap.emplace("/std/collections/map/contains", &canonicalContainsDef);
  defMap.emplace("/std/collections/map/tryAt", &canonicalTryAtDef);
  defMap.emplace("/std/collections/map/at", &canonicalAtDef);

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo countInfo;
  countInfo.returnsVoid = false;
  countInfo.returnsArray = false;
  countInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
  infoByPath.emplace("/std/collections/map/count", countInfo);

  primec::ir_lowerer::ReturnInfo containsInfo;
  containsInfo.returnsVoid = false;
  containsInfo.returnsArray = false;
  containsInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Bool;
  infoByPath.emplace("/std/collections/map/contains", containsInfo);

  primec::ir_lowerer::ReturnInfo tryAtInfo;
  tryAtInfo.returnsVoid = false;
  tryAtInfo.returnsArray = false;
  tryAtInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  infoByPath.emplace("/std/collections/map/tryAt", tryAtInfo);

  primec::ir_lowerer::ReturnInfo accessInfo;
  accessInfo.returnsVoid = false;
  accessInfo.returnsArray = false;
  accessInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace("/std/collections/map/at", accessInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "/map/count";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool definitionMatched = false;
  CHECK(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      countCall,
      defMap,
      [](const primec::Expr &expr) { return expr.name; },
      getReturnInfo,
      false,
      kindOut,
      &definitionMatched));
  CHECK(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);

  primec::Expr containsCall;
  containsCall.kind = primec::Expr::Kind::Call;
  containsCall.name = "/map/contains";

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  definitionMatched = false;
  CHECK(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      containsCall,
      defMap,
      [](const primec::Expr &expr) { return expr.name; },
      getReturnInfo,
      false,
      kindOut,
      &definitionMatched));
  CHECK(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Bool);

  primec::Expr tryAtCall;
  tryAtCall.kind = primec::Expr::Kind::Call;
  tryAtCall.name = "/map/tryAt";

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  definitionMatched = false;
  CHECK(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      tryAtCall,
      defMap,
      [](const primec::Expr &expr) { return expr.name; },
      getReturnInfo,
      false,
      kindOut,
      &definitionMatched));
  CHECK(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::Expr atCall;
  atCall.kind = primec::Expr::Kind::Call;
  atCall.name = "/map/at";

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  definitionMatched = false;
  CHECK(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      atCall,
      defMap,
      [](const primec::Expr &expr) { return expr.name; },
      getReturnInfo,
      false,
      kindOut,
      &definitionMatched));
  CHECK(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer setup type helper rejects canonical map count fallback while keeping direct access defs") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  primec::Definition aliasCountDef;
  aliasCountDef.fullPath = "/map/count";
  primec::Definition aliasAtUnsafeDef;
  aliasAtUnsafeDef.fullPath = "/map/at_unsafe";
  defMap.emplace("/map/count", &aliasCountDef);
  defMap.emplace("/map/at_unsafe", &aliasAtUnsafeDef);

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo countInfo;
  countInfo.returnsVoid = false;
  countInfo.returnsArray = false;
  countInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  infoByPath.emplace("/map/count", countInfo);

  primec::ir_lowerer::ReturnInfo accessInfo;
  accessInfo.returnsVoid = false;
  accessInfo.returnsArray = false;
  accessInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace("/map/at_unsafe", accessInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "/std/collections/map/count";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool definitionMatched = false;
  CHECK_FALSE(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      countCall,
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
  atUnsafeCall.name = "/map/at_unsafe";

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  definitionMatched = false;
  CHECK(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      atUnsafeCall,
      defMap,
      [](const primec::Expr &expr) { return expr.name; },
      getReturnInfo,
      false,
      kindOut,
      &definitionMatched));
  CHECK(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer setup type helper rejects canonical map contains fallback while keeping direct access defs") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  primec::Definition aliasContainsDef;
  aliasContainsDef.fullPath = "/map/contains";
  primec::Definition aliasAtUnsafeDef;
  aliasAtUnsafeDef.fullPath = "/map/at_unsafe";
  defMap.emplace("/map/contains", &aliasContainsDef);
  defMap.emplace("/map/at_unsafe", &aliasAtUnsafeDef);

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo containsInfo;
  containsInfo.returnsVoid = false;
  containsInfo.returnsArray = false;
  containsInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Bool;
  infoByPath.emplace("/map/contains", containsInfo);

  primec::ir_lowerer::ReturnInfo accessInfo;
  accessInfo.returnsVoid = false;
  accessInfo.returnsArray = false;
  accessInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace("/map/at_unsafe", accessInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::Expr containsCall;
  containsCall.kind = primec::Expr::Kind::Call;
  containsCall.name = "/std/collections/map/contains";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool definitionMatched = false;
  CHECK_FALSE(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      containsCall,
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
  atUnsafeCall.name = "/map/at_unsafe";

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  definitionMatched = false;
  CHECK(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      atUnsafeCall,
      defMap,
      [](const primec::Expr &expr) { return expr.name; },
      getReturnInfo,
      false,
      kindOut,
      &definitionMatched));
  CHECK(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer setup type helper rejects canonical map tryAt fallback while keeping direct access defs") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  primec::Definition aliasTryAtDef;
  aliasTryAtDef.fullPath = "/map/tryAt";
  primec::Definition aliasAtUnsafeDef;
  aliasAtUnsafeDef.fullPath = "/map/at_unsafe";
  defMap.emplace("/map/tryAt", &aliasTryAtDef);
  defMap.emplace("/map/at_unsafe", &aliasAtUnsafeDef);

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo tryAtInfo;
  tryAtInfo.returnsVoid = false;
  tryAtInfo.returnsArray = false;
  tryAtInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  infoByPath.emplace("/map/tryAt", tryAtInfo);

  primec::ir_lowerer::ReturnInfo accessInfo;
  accessInfo.returnsVoid = false;
  accessInfo.returnsArray = false;
  accessInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace("/map/at_unsafe", accessInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::Expr tryAtCall;
  tryAtCall.kind = primec::Expr::Kind::Call;
  tryAtCall.name = "/std/collections/map/tryAt";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool definitionMatched = false;
  CHECK_FALSE(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      tryAtCall,
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
  atUnsafeCall.name = "/map/at_unsafe";

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  definitionMatched = false;
  CHECK(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      atUnsafeCall,
      defMap,
      [](const primec::Expr &expr) { return expr.name; },
      getReturnInfo,
      false,
      kindOut,
      &definitionMatched));
  CHECK(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer setup type helper rejects canonical map constructor fallback to compatibility defs") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  primec::Definition aliasCtorDef;
  aliasCtorDef.fullPath = "/map/map";
  defMap.emplace("/map/map", &aliasCtorDef);

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo ctorInfo;
  ctorInfo.returnsVoid = false;
  ctorInfo.returnsArray = false;
  ctorInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  infoByPath.emplace("/map/map", ctorInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::Expr ctorCall;
  ctorCall.kind = primec::Expr::Kind::Call;
  ctorCall.name = "/std/collections/map/map";
  ctorCall.templateArgs = {"i32", "i32"};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool definitionMatched = false;
  CHECK_FALSE(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      ctorCall,
      defMap,
      [](const primec::Expr &expr) { return expr.name; },
      getReturnInfo,
      false,
      kindOut,
      &definitionMatched));
  CHECK_FALSE(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper rejects canonical return-info forwarding when compatibility defs lack return info") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  primec::Definition aliasCountDef;
  aliasCountDef.fullPath = "/vector/count";
  primec::Definition canonicalCountDef;
  canonicalCountDef.fullPath = "/std/collections/vector/count";
  defMap.emplace("/vector/count", &aliasCountDef);
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
  CHECK(definitionMatched);
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
  CHECK(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper skips unmatched direct definition call return kinds") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  primec::Definition directDef;
  directDef.fullPath = "/pkg/value";
  defMap.emplace("/pkg/value", &directDef);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "value";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  bool definitionMatched = true;
  CHECK_FALSE(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      callExpr,
      defMap,
      [](const primec::Expr &) { return std::string("/pkg/missing"); },
      {},
      false,
      kindOut,
      &definitionMatched));
  CHECK_FALSE(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  callExpr.isMethodCall = true;
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  definitionMatched = true;
  CHECK_FALSE(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      callExpr,
      defMap,
      [](const primec::Expr &) { return std::string("/pkg/value"); },
      {},
      false,
      kindOut,
      &definitionMatched));
  CHECK_FALSE(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper resolves count call method return kinds") {
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

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "count";
  countCall.args.push_back(receiverExpr);

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool methodResolved = false;
  CHECK(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      countCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&methodDef](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return &methodDef; },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  methodResolved = false;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      countCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&methodDef](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return &methodDef; },
      getReturnInfo,
      true,
      kindOut,
      &methodResolved));
  CHECK(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper resolves access call method return kinds") {
  primec::Definition atDef;
  atDef.fullPath = "/array/at";
  primec::Definition atUnsafeDef;
  atUnsafeDef.fullPath = "/array/at_unsafe";

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo scalarInfo;
  scalarInfo.returnsVoid = false;
  scalarInfo.returnsArray = false;
  scalarInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace("/array/at", scalarInfo);
  infoByPath.emplace("/array/at_unsafe", scalarInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;

  primec::Expr atCall;
  atCall.kind = primec::Expr::Kind::Call;
  atCall.name = "at";
  atCall.args = {receiverExpr, indexExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool methodResolved = false;
  CHECK(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      atCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&atDef](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return &atDef; },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr atUnsafeCall = atCall;
  atUnsafeCall.name = "at_unsafe";
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  methodResolved = false;
  CHECK(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      atUnsafeCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&atUnsafeDef](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return &atUnsafeDef; },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_SUITE_END();
