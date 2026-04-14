#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer setup type helper rejects canonical map access fallback to compatibility defs") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  primec::Definition aliasAtDef;
  aliasAtDef.fullPath = "/map/at";
  primec::Definition aliasAtUnsafeDef;
  aliasAtUnsafeDef.fullPath = "/map/at_unsafe";
  defMap.emplace("/map/at", &aliasAtDef);
  defMap.emplace("/map/at_unsafe", &aliasAtUnsafeDef);

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo accessInfo;
  accessInfo.returnsVoid = false;
  accessInfo.returnsArray = false;
  accessInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace("/map/at", accessInfo);
  infoByPath.emplace("/map/at_unsafe", accessInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::Expr atCall;
  atCall.kind = primec::Expr::Kind::Call;
  atCall.name = "/std/collections/map/at";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool definitionMatched = false;
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
  atUnsafeCall.name = "/std/collections/map/at_unsafe";

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

TEST_CASE("ir lowerer setup type helper rejects canonical-only fallback for explicit map helper aliases" * doctest::skip(true)) {
  primec::Definition canonicalCountDef;
  canonicalCountDef.fullPath = "/std/collections/map/count";
  primec::Definition canonicalAtDef;
  canonicalAtDef.fullPath = "/std/collections/map/at";
  primec::Definition canonicalAtUnsafeDef;
  canonicalAtUnsafeDef.fullPath = "/std/collections/map/at_unsafe";

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo countInfo;
  countInfo.returnsVoid = false;
  countInfo.returnsArray = false;
  countInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  infoByPath.emplace(canonicalCountDef.fullPath, countInfo);

  primec::ir_lowerer::ReturnInfo accessInfo;
  accessInfo.returnsVoid = false;
  accessInfo.returnsArray = false;
  accessInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace(canonicalAtDef.fullPath, accessInfo);
  infoByPath.emplace(canonicalAtUnsafeDef.fullPath, accessInfo);

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

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool methodResolved = false;

  primec::Expr aliasCountCall;
  aliasCountCall.kind = primec::Expr::Kind::Call;
  aliasCountCall.name = "/map/count";
  aliasCountCall.args = {receiverExpr};

  int aliasCountResolveCalls = 0;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      aliasCountCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
        ++aliasCountResolveCalls;
        CHECK(methodExpr.isMethodCall);
        CHECK(methodExpr.name == "count");
        return &canonicalCountDef;
      },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(aliasCountResolveCalls == 0);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  methodResolved = false;
  primec::Expr aliasAtCall;
  aliasAtCall.kind = primec::Expr::Kind::Call;
  aliasAtCall.name = "/map/at";
  aliasAtCall.args = {receiverExpr, keyExpr};

  int aliasAtResolveCalls = 0;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      aliasAtCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
        ++aliasAtResolveCalls;
        CHECK(methodExpr.isMethodCall);
        CHECK(methodExpr.name == "at");
        return &canonicalAtDef;
      },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(aliasAtResolveCalls == 0);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  methodResolved = false;
  primec::Expr aliasAtUnsafeCall;
  aliasAtUnsafeCall.kind = primec::Expr::Kind::Call;
  aliasAtUnsafeCall.name = "/map/at_unsafe";
  aliasAtUnsafeCall.args = {receiverExpr, keyExpr};

  int aliasAtUnsafeResolveCalls = 0;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      aliasAtUnsafeCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
        ++aliasAtUnsafeResolveCalls;
        CHECK(methodExpr.isMethodCall);
        CHECK(methodExpr.name == "at_unsafe");
        return &canonicalAtUnsafeDef;
      },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(aliasAtUnsafeResolveCalls == 0);
}

TEST_CASE("ir lowerer setup type helper resolves reordered positional access call method return kinds") {
  primec::Definition atUnsafeDef;
  atUnsafeDef.fullPath = "/map/at_unsafe";

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo scalarInfo;
  scalarInfo.returnsVoid = false;
  scalarInfo.returnsArray = false;
  scalarInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  infoByPath.emplace("/map/at_unsafe", scalarInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::StringLiteral;
  keyExpr.stringValue = "only";
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::Expr atUnsafeCall;
  atUnsafeCall.kind = primec::Expr::Kind::Call;
  atUnsafeCall.name = "at_unsafe";
  atUnsafeCall.args = {keyExpr, receiverExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo mapLocal;
  mapLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  locals.emplace("items", mapLocal);

  int resolveCalls = 0;
  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool methodResolved = false;
  CHECK(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      atUnsafeCall,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
        ++resolveCalls;
        if (!methodExpr.isMethodCall || methodExpr.args.empty()) {
          return static_cast<const primec::Definition *>(nullptr);
        }
        if (methodExpr.args.front().kind == primec::Expr::Kind::Name && methodExpr.args.front().name == "items") {
          return &atUnsafeDef;
        }
        return static_cast<const primec::Definition *>(nullptr);
      },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(resolveCalls == 1);
}

TEST_CASE("ir lowerer setup type helper keeps leading collection receiver for positional access calls") {
  primec::Definition atDef;
  atDef.fullPath = "/map/at";

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo scalarInfo;
  scalarInfo.returnsVoid = false;
  scalarInfo.returnsArray = false;
  scalarInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace("/map/at", scalarInfo);

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
  primec::Expr fallbackReceiverExpr;
  fallbackReceiverExpr.kind = primec::Expr::Kind::Name;
  fallbackReceiverExpr.name = "fallback";

  primec::Expr atCall;
  atCall.kind = primec::Expr::Kind::Call;
  atCall.name = "at";
  atCall.args = {receiverExpr, fallbackReceiverExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo mapLocal;
  mapLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  locals.emplace("items", mapLocal);
  locals.emplace("fallback", mapLocal);

  int resolveCalls = 0;
  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool methodResolved = false;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      atCall,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
        ++resolveCalls;
        if (!methodExpr.isMethodCall || methodExpr.args.empty()) {
          return static_cast<const primec::Definition *>(nullptr);
        }
        if (methodExpr.args.front().kind == primec::Expr::Kind::Name &&
            methodExpr.args.front().name == "fallback") {
          return &atDef;
        }
        return static_cast<const primec::Definition *>(nullptr);
      },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(resolveCalls == 1);
}

TEST_CASE("ir lowerer setup type helper resolves reordered named access call method return kinds") {
  primec::Definition atUnsafeDef;
  atUnsafeDef.fullPath = "/map/at_unsafe";

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo scalarInfo;
  scalarInfo.returnsVoid = false;
  scalarInfo.returnsArray = false;
  scalarInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  infoByPath.emplace("/map/at_unsafe", scalarInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::StringLiteral;
  keyExpr.stringValue = "only";
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::Expr atUnsafeCall;
  atUnsafeCall.kind = primec::Expr::Kind::Call;
  atUnsafeCall.name = "at_unsafe";
  atUnsafeCall.args = {keyExpr, receiverExpr};
  atUnsafeCall.argNames = {std::string("key"), std::string("values")};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo mapLocal;
  mapLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  locals.emplace("items", mapLocal);

  int resolveCalls = 0;
  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool methodResolved = false;
  CHECK(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      atUnsafeCall,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
        ++resolveCalls;
        if (!methodExpr.isMethodCall || methodExpr.args.empty()) {
          return static_cast<const primec::Definition *>(nullptr);
        }
        if (methodExpr.args.front().kind == primec::Expr::Kind::Name && methodExpr.args.front().name == "items") {
          return &atUnsafeDef;
        }
        return static_cast<const primec::Definition *>(nullptr);
      },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(resolveCalls == 1);
}

TEST_CASE("ir lowerer setup type helper keeps labeled named access receiver leading") {
  primec::Definition atDef;
  atDef.fullPath = "/map/at";

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo scalarInfo;
  scalarInfo.returnsVoid = false;
  scalarInfo.returnsArray = false;
  scalarInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace("/map/at", scalarInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr fallbackExpr;
  fallbackExpr.kind = primec::Expr::Kind::Name;
  fallbackExpr.name = "fallback";

  primec::Expr atCall;
  atCall.kind = primec::Expr::Kind::Call;
  atCall.name = "at";
  atCall.args = {valuesExpr, fallbackExpr};
  atCall.argNames = {std::string("values"), std::string("index")};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo leadingMapInfo;
  leadingMapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  leadingMapInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  locals.emplace("values", leadingMapInfo);

  primec::ir_lowerer::LocalInfo fallbackMapInfo;
  fallbackMapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  fallbackMapInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("fallback", fallbackMapInfo);

  int resolveCalls = 0;
  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool methodResolved = false;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      atCall,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
        ++resolveCalls;
        if (!methodExpr.isMethodCall || methodExpr.args.empty()) {
          return static_cast<const primec::Definition *>(nullptr);
        }
        if (methodExpr.args.front().kind == primec::Expr::Kind::Name &&
            methodExpr.args.front().name == "fallback") {
          return &atDef;
        }
        return static_cast<const primec::Definition *>(nullptr);
      },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(resolveCalls == 1);
}

TEST_CASE("ir lowerer setup type helper resolves soa get/ref call method return kinds") {
  primec::Definition getDef;
  getDef.fullPath = "/soa_vector/get";
  primec::Definition refDef;
  refDef.fullPath = "/soa_vector/ref";

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo scalarInfo;
  scalarInfo.returnsVoid = false;
  scalarInfo.returnsArray = false;
  scalarInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace("/soa_vector/get", scalarInfo);
  infoByPath.emplace("/soa_vector/ref", scalarInfo);

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
  receiverExpr.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;

  primec::Expr getCall;
  getCall.kind = primec::Expr::Kind::Call;
  getCall.name = "get";
  getCall.args = {receiverExpr, indexExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool methodResolved = false;
  CHECK(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      getCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&getDef](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return &getDef; },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr refCall = getCall;
  refCall.name = "ref";
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  methodResolved = false;
  CHECK(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      refCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&refDef](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return &refDef; },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer setup type helper resolves soa field call method return kinds") {
  primec::Definition fieldDef;
  fieldDef.fullPath = "/soa_vector/x";

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo scalarInfo;
  scalarInfo.returnsVoid = false;
  scalarInfo.returnsArray = false;
  scalarInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  infoByPath.emplace("/soa_vector/x", scalarInfo);

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
  receiverExpr.name = "values";

  primec::Expr fieldCall;
  fieldCall.kind = primec::Expr::Kind::Call;
  fieldCall.name = "x";
  fieldCall.args = {receiverExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  valuesLocal.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  valuesLocal.isSoaVector = true;
  locals.emplace("values", valuesLocal);

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool methodResolved = false;
  CHECK(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      fieldCall,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&fieldDef](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return &fieldDef; },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  methodResolved = true;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      fieldCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return nullptr; },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}


TEST_SUITE_END();
