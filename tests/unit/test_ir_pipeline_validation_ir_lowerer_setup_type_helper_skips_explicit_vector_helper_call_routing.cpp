#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer setup type helper skips explicit vector helper call routing") {
  primec::Definition countDef;
  countDef.fullPath = "/vector/count";
  primec::Definition aliasAtDef;
  aliasAtDef.fullPath = "/vector/at";
  primec::Definition aliasAtUnsafeDef;
  aliasAtUnsafeDef.fullPath = "/vector/at_unsafe";
  primec::Definition canonicalAtDef;
  canonicalAtDef.fullPath = "/std/collections/vector/at";
  primec::Definition canonicalAtUnsafeDef;
  canonicalAtUnsafeDef.fullPath = "/std/collections/vector/at_unsafe";
  primec::Definition pushDef;
  pushDef.fullPath = "/vector/push";
  primec::Definition canonicalCapacityDef;
  canonicalCapacityDef.fullPath = "/std/collections/vector/capacity";

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo countInfo;
  countInfo.returnsVoid = false;
  countInfo.returnsArray = false;
  countInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  infoByPath.emplace(countDef.fullPath, countInfo);

  primec::ir_lowerer::ReturnInfo atInfo;
  atInfo.returnsVoid = false;
  atInfo.returnsArray = false;
  atInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace(aliasAtDef.fullPath, atInfo);
  infoByPath.emplace(aliasAtUnsafeDef.fullPath, atInfo);
  infoByPath.emplace(canonicalAtDef.fullPath, atInfo);
  infoByPath.emplace(canonicalAtUnsafeDef.fullPath, atInfo);

  primec::ir_lowerer::ReturnInfo pushInfo;
  pushInfo.returnsVoid = false;
  pushInfo.returnsArray = false;
  pushInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace(pushDef.fullPath, pushInfo);

  primec::ir_lowerer::ReturnInfo capacityInfo;
  capacityInfo.returnsVoid = false;
  capacityInfo.returnsArray = false;
  capacityInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
  infoByPath.emplace(canonicalCapacityDef.fullPath, capacityInfo);

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

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool methodResolved = false;

  primec::Expr aliasCountCall;
  aliasCountCall.kind = primec::Expr::Kind::Call;
  aliasCountCall.name = "/vector/count";
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
        return &countDef;
      },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(aliasCountResolveCalls == 0);

  primec::Expr canonicalCountCall;
  canonicalCountCall.kind = primec::Expr::Kind::Call;
  canonicalCountCall.name = "/std/collections/vector/count";
  canonicalCountCall.args = {receiverExpr};
  int canonicalCountResolveCalls = 0;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      canonicalCountCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
        ++canonicalCountResolveCalls;
        return &countDef;
      },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(canonicalCountResolveCalls == 0);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  methodResolved = false;
  primec::Expr canonicalAtCall;
  canonicalAtCall.kind = primec::Expr::Kind::Call;
  canonicalAtCall.name = "/std/collections/vector/at";
  canonicalAtCall.args = {receiverExpr, indexExpr};
  int canonicalAtResolveCalls = 0;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      canonicalAtCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
        ++canonicalAtResolveCalls;
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
  CHECK(canonicalAtResolveCalls == 0);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  methodResolved = false;
  primec::Expr aliasAtCall;
  aliasAtCall.kind = primec::Expr::Kind::Call;
  aliasAtCall.name = "/vector/at";
  aliasAtCall.args = {receiverExpr, indexExpr};
  int aliasAtResolveCalls = 0;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      aliasAtCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
        ++aliasAtResolveCalls;
        CHECK(methodExpr.isMethodCall);
        CHECK(methodExpr.name == "/vector/at");
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
  aliasAtUnsafeCall.name = "/vector/at_unsafe";
  aliasAtUnsafeCall.args = {receiverExpr, indexExpr};
  int aliasAtUnsafeResolveCalls = 0;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      aliasAtUnsafeCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
        ++aliasAtUnsafeResolveCalls;
        CHECK(methodExpr.isMethodCall);
        CHECK(methodExpr.name == "/vector/at_unsafe");
        return &canonicalAtUnsafeDef;
      },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(aliasAtUnsafeResolveCalls == 0);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  methodResolved = false;
  primec::Expr aliasPushCall;
  aliasPushCall.kind = primec::Expr::Kind::Call;
  aliasPushCall.name = "/vector/push";
  aliasPushCall.args = {indexExpr, receiverExpr};
  int aliasPushResolveCalls = 0;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      aliasPushCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
        ++aliasPushResolveCalls;
        CHECK(methodExpr.isMethodCall);
        CHECK(methodExpr.name == "push");
        if (!methodExpr.args.empty() && methodExpr.args.front().kind == primec::Expr::Kind::Name &&
            methodExpr.args.front().name == "items") {
          return &pushDef;
        }
        return static_cast<const primec::Definition *>(nullptr);
      },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(aliasPushResolveCalls == 0);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  methodResolved = false;
  primec::Expr canonicalCapacityCall;
  canonicalCapacityCall.kind = primec::Expr::Kind::Call;
  canonicalCapacityCall.name = "/std/collections/vector/capacity";
  canonicalCapacityCall.args = {receiverExpr};
  int canonicalCapacityResolveCalls = 0;
  CHECK_FALSE(primec::ir_lowerer::resolveCapacityMethodCallReturnKind(
      canonicalCapacityCall,
      {},
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
        ++canonicalCapacityResolveCalls;
        CHECK(methodExpr.isMethodCall);
        CHECK(methodExpr.name == "capacity");
        return &canonicalCapacityDef;
      },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(canonicalCapacityResolveCalls == 0);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  methodResolved = false;
  primec::Expr removedArrayCapacityCall;
  removedArrayCapacityCall.kind = primec::Expr::Kind::Call;
  removedArrayCapacityCall.name = "/array/capacity";
  removedArrayCapacityCall.args = {receiverExpr};
  int removedArrayCapacityResolveCalls = 0;
  CHECK_FALSE(primec::ir_lowerer::resolveCapacityMethodCallReturnKind(
      removedArrayCapacityCall,
      {},
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
        ++removedArrayCapacityResolveCalls;
        return &canonicalCapacityDef;
      },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(removedArrayCapacityResolveCalls == 0);
}

TEST_CASE("ir lowerer setup type helper rejects direct vector access helper routing even with real defs") {
  primec::Definition atDef;
  atDef.fullPath = "/vector/at";
  primec::Definition atUnsafeDef;
  atUnsafeDef.fullPath = "/vector/at_unsafe";

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo accessInfo;
  accessInfo.returnsVoid = false;
  accessInfo.returnsArray = false;
  accessInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace(atDef.fullPath, accessInfo);
  infoByPath.emplace(atUnsafeDef.fullPath, accessInfo);

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

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool methodResolved = false;

  primec::Expr aliasAtCall;
  aliasAtCall.kind = primec::Expr::Kind::Call;
  aliasAtCall.name = "/vector/at";
  aliasAtCall.args = {receiverExpr, indexExpr};

  int aliasAtResolveCalls = 0;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      aliasAtCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
        ++aliasAtResolveCalls;
        CHECK(methodExpr.isMethodCall);
        CHECK(methodExpr.name == "/vector/at");
        return &atDef;
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
  aliasAtUnsafeCall.name = "/vector/at_unsafe";
  aliasAtUnsafeCall.args = {receiverExpr, indexExpr};

  int aliasAtUnsafeResolveCalls = 0;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      aliasAtUnsafeCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
        ++aliasAtUnsafeResolveCalls;
        CHECK(methodExpr.isMethodCall);
        CHECK(methodExpr.name == "/vector/at_unsafe");
        return &atUnsafeDef;
      },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(aliasAtUnsafeResolveCalls == 0);
}

TEST_CASE("ir lowerer setup type helper skips explicit map helper call routing" * doctest::skip(true)) {
  primec::Definition countDef;
  countDef.fullPath = "/map/count";
  primec::Definition atDef;
  atDef.fullPath = "/map/at";
  primec::Definition atUnsafeDef;
  atUnsafeDef.fullPath = "/map/at_unsafe";

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo countInfo;
  countInfo.returnsVoid = false;
  countInfo.returnsArray = false;
  countInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  infoByPath.emplace(countDef.fullPath, countInfo);

  primec::ir_lowerer::ReturnInfo accessInfo;
  accessInfo.returnsVoid = false;
  accessInfo.returnsArray = false;
  accessInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace(atDef.fullPath, accessInfo);
  infoByPath.emplace(atUnsafeDef.fullPath, accessInfo);

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
        return &countDef;
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
        return &atDef;
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
  aliasAtUnsafeCall.args = {keyExpr, receiverExpr};
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
        if (!methodExpr.args.empty() && methodExpr.args.front().kind == primec::Expr::Kind::Name &&
            methodExpr.args.front().name == "items") {
          return &atUnsafeDef;
        }
        return static_cast<const primec::Definition *>(nullptr);
      },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(aliasAtUnsafeResolveCalls == 0);
}

TEST_CASE("ir lowerer setup type helper rejects explicit slash-method map access return kinds") {
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo itemsLocal;
  itemsLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  itemsLocal.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("items", itemsLocal);

  auto expectUnknownKind = [&](const char *methodName) {
    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.name = methodName;
    methodCall.isMethodCall = true;
    methodCall.args = {receiverExpr, keyExpr};

    primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    bool methodResolved = false;
    CHECK_FALSE(primec::ir_lowerer::resolveMethodCallReturnKind(
        methodCall,
        locals,
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
          return nullptr;
        },
        {},
        {},
        false,
        kindOut,
        &methodResolved));
    CHECK_FALSE(methodResolved);
    CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  };

  expectUnknownKind("/map/at");
  expectUnknownKind("/map/at_unsafe");
  expectUnknownKind("/std/collections/map/at");
  expectUnknownKind("/std/collections/map/at_unsafe");
}

TEST_CASE("ir lowerer setup type helper rejects explicit slash-method vector access return kinds") {
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo itemsLocal;
  itemsLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  itemsLocal.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("items", itemsLocal);

  auto expectUnknownKind = [&](const char *methodName) {
    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.name = methodName;
    methodCall.isMethodCall = true;
    methodCall.args = {receiverExpr, indexExpr};

    primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    bool methodResolved = false;
    CHECK_FALSE(primec::ir_lowerer::resolveMethodCallReturnKind(
        methodCall,
        locals,
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
          return nullptr;
        },
        {},
        {},
        false,
        kindOut,
        &methodResolved));
    CHECK_FALSE(methodResolved);
    CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  };

  expectUnknownKind("/vector/at");
  expectUnknownKind("/vector/at_unsafe");
  expectUnknownKind("/std/collections/vector/at");
  expectUnknownKind("/std/collections/vector/at_unsafe");
}

TEST_CASE("ir lowerer setup type helper rejects explicit slash-method vector access return kinds for constructor receivers") {
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Call;
  receiverExpr.name = "vector";
  receiverExpr.templateArgs = {"i32"};

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;

  auto expectUnknownKind = [&](const char *methodName) {
    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.name = methodName;
    methodCall.isMethodCall = true;
    methodCall.args = {receiverExpr, indexExpr};

    primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    bool methodResolved = false;
    CHECK_FALSE(primec::ir_lowerer::resolveMethodCallReturnKind(
        methodCall,
        {},
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
          return nullptr;
        },
        {},
        {},
        false,
        kindOut,
        &methodResolved));
    CHECK_FALSE(methodResolved);
    CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  };

  expectUnknownKind("/vector/at");
  expectUnknownKind("/vector/at_unsafe");
  expectUnknownKind("/std/collections/vector/at");
  expectUnknownKind("/std/collections/vector/at_unsafe");
}

TEST_CASE("ir lowerer setup type helper rejects bare vector access method return kinds") {
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo itemsLocal;
  itemsLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  itemsLocal.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("items", itemsLocal);

  auto expectUnknownKind = [&](const char *methodName) {
    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.name = methodName;
    methodCall.isMethodCall = true;
    methodCall.args = {receiverExpr, indexExpr};

    primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    bool methodResolved = false;
    CHECK_FALSE(primec::ir_lowerer::resolveMethodCallReturnKind(
        methodCall,
        locals,
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
          return nullptr;
        },
        {},
        {},
        false,
        kindOut,
        &methodResolved));
    CHECK_FALSE(methodResolved);
    CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  };

  expectUnknownKind("at");
  expectUnknownKind("at_unsafe");
}


TEST_SUITE_END();
