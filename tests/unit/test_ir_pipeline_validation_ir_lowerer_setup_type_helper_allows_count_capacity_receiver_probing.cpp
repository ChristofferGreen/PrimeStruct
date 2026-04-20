#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer setup type helper allows count/capacity receiver probing") {
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "count";
  methodCall.isMethodCall = true;
  methodCall.args.push_back(receiverExpr);

  const primec::Expr *receiverOut = nullptr;
  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodCallReceiverExpr(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      receiverOut,
      error));
  REQUIRE(receiverOut != nullptr);
  CHECK(receiverOut->name == "items");
  CHECK(error.empty());

  methodCall.name = "capacity";
  receiverOut = nullptr;
  error.clear();
  CHECK(primec::ir_lowerer::resolveMethodCallReceiverExpr(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      receiverOut,
      error));
  REQUIRE(receiverOut != nullptr);
  CHECK(receiverOut->name == "items");
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper allows access receiver fallback probing") {
  primec::Expr entryArgsReceiver;
  entryArgsReceiver.kind = primec::Expr::Kind::Name;
  entryArgsReceiver.name = "argv";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "at";
  methodCall.isMethodCall = true;
  methodCall.args = {entryArgsReceiver, indexExpr};

  const primec::Expr *receiverOut = &methodCall;
  std::string error = "stale";
  CHECK_FALSE(primec::ir_lowerer::resolveMethodCallReceiverExpr(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        return expr.kind == primec::Expr::Kind::Name && expr.name == "argv";
      },
      receiverOut,
      error));
  CHECK(receiverOut == nullptr);
  CHECK(error == "stale");

  methodCall.name = "at_unsafe";
  receiverOut = &methodCall;
  error = "stale";
  CHECK_FALSE(primec::ir_lowerer::resolveMethodCallReceiverExpr(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        return expr.kind == primec::Expr::Kind::Name && expr.name == "argv";
      },
      receiverOut,
      error));
  CHECK(receiverOut == nullptr);
  CHECK(error == "stale");
}

TEST_CASE("ir lowerer setup type helper resolves method call definitions from expressions") {
  primec::Definition arrayCountDef;
  arrayCountDef.fullPath = "/array/count";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/array/count", &arrayCountDef},
  };

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

  std::string error;
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
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
      defMap,
      error);
  CHECK(resolved == &arrayCountDef);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper prefers canonical bare vector count and capacity methods") {
  primec::Definition vectorCountDef;
  vectorCountDef.fullPath = "/vector/count";
  primec::Definition stdCountDef;
  stdCountDef.fullPath = "/std/collections/vector/count";
  primec::Definition vectorCapacityDef;
  vectorCapacityDef.fullPath = "/vector/capacity";
  primec::Definition stdCapacityDef;
  stdCapacityDef.fullPath = "/std/collections/vector/capacity";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {vectorCountDef.fullPath, &vectorCountDef},
      {stdCountDef.fullPath, &stdCountDef},
      {vectorCapacityDef.fullPath, &vectorCapacityDef},
      {stdCapacityDef.fullPath, &stdCapacityDef},
  };

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo itemsLocal;
  itemsLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  locals.emplace("items", itemsLocal);

  auto expectResolvedMethod = [&](const char *methodName, const primec::Definition *expectedDef) {
    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.name = methodName;
    methodCall.isMethodCall = true;
    methodCall.args = {receiverExpr};

    std::string error;
    const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
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
        defMap,
        error);
    CHECK(resolved == expectedDef);
    CHECK(error.empty());
  };

  expectResolvedMethod("count", &stdCountDef);
  expectResolvedMethod("capacity", &stdCapacityDef);
}

TEST_CASE("ir lowerer setup type helper prefers canonical bare vector access methods") {
  primec::Definition vectorAtDef;
  vectorAtDef.fullPath = "/vector/at";
  primec::Definition stdAtDef;
  stdAtDef.fullPath = "/std/collections/vector/at";
  primec::Definition vectorAtUnsafeDef;
  vectorAtUnsafeDef.fullPath = "/vector/at_unsafe";
  primec::Definition stdAtUnsafeDef;
  stdAtUnsafeDef.fullPath = "/std/collections/vector/at_unsafe";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {vectorAtDef.fullPath, &vectorAtDef},
      {stdAtDef.fullPath, &stdAtDef},
      {vectorAtUnsafeDef.fullPath, &vectorAtUnsafeDef},
      {stdAtUnsafeDef.fullPath, &stdAtUnsafeDef},
  };

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 0;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo itemsLocal;
  itemsLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  locals.emplace("items", itemsLocal);

  auto expectResolvedMethod = [&](const char *methodName, const primec::Definition *expectedDef) {
    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.name = methodName;
    methodCall.isMethodCall = true;
    methodCall.args = {receiverExpr, indexExpr};

    std::string error;
    const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
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
        defMap,
        error);
    CHECK(resolved == expectedDef);
    CHECK(error.empty());
  };

  expectResolvedMethod("at", &stdAtDef);
  expectResolvedMethod("at_unsafe", &stdAtUnsafeDef);
}

TEST_CASE("ir lowerer setup type helper prefers canonical bare vector mutator methods") {
  primec::Definition vectorPushDef;
  vectorPushDef.fullPath = "/vector/push";
  primec::Definition stdPushDef;
  stdPushDef.fullPath = "/std/collections/vector/push";
  primec::Definition vectorPopDef;
  vectorPopDef.fullPath = "/vector/pop";
  primec::Definition stdPopDef;
  stdPopDef.fullPath = "/std/collections/vector/pop";
  primec::Definition vectorReserveDef;
  vectorReserveDef.fullPath = "/vector/reserve";
  primec::Definition stdReserveDef;
  stdReserveDef.fullPath = "/std/collections/vector/reserve";
  primec::Definition vectorClearDef;
  vectorClearDef.fullPath = "/vector/clear";
  primec::Definition stdClearDef;
  stdClearDef.fullPath = "/std/collections/vector/clear";
  primec::Definition vectorRemoveAtDef;
  vectorRemoveAtDef.fullPath = "/vector/remove_at";
  primec::Definition stdRemoveAtDef;
  stdRemoveAtDef.fullPath = "/std/collections/vector/remove_at";
  primec::Definition vectorRemoveSwapDef;
  vectorRemoveSwapDef.fullPath = "/vector/remove_swap";
  primec::Definition stdRemoveSwapDef;
  stdRemoveSwapDef.fullPath = "/std/collections/vector/remove_swap";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {vectorPushDef.fullPath, &vectorPushDef},
      {stdPushDef.fullPath, &stdPushDef},
      {vectorPopDef.fullPath, &vectorPopDef},
      {stdPopDef.fullPath, &stdPopDef},
      {vectorReserveDef.fullPath, &vectorReserveDef},
      {stdReserveDef.fullPath, &stdReserveDef},
      {vectorClearDef.fullPath, &vectorClearDef},
      {stdClearDef.fullPath, &stdClearDef},
      {vectorRemoveAtDef.fullPath, &vectorRemoveAtDef},
      {stdRemoveAtDef.fullPath, &stdRemoveAtDef},
      {vectorRemoveSwapDef.fullPath, &vectorRemoveSwapDef},
      {stdRemoveSwapDef.fullPath, &stdRemoveSwapDef},
  };

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Literal;
  valueExpr.intWidth = 32;
  valueExpr.literalValue = 1;

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 0;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo itemsLocal;
  itemsLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  locals.emplace("items", itemsLocal);

  auto expectResolvedMethod = [&](const char *methodName,
                                  const std::vector<primec::Expr> &args,
                                  const primec::Definition *expectedDef) {
    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.name = methodName;
    methodCall.isMethodCall = true;
    methodCall.args = args;

    std::string error;
    const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
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
        defMap,
        error);
    CHECK(resolved == expectedDef);
    CHECK(error.empty());
  };

  expectResolvedMethod("push", {receiverExpr, valueExpr}, &stdPushDef);
  expectResolvedMethod("pop", {receiverExpr}, &stdPopDef);
  expectResolvedMethod("reserve", {receiverExpr, valueExpr}, &stdReserveDef);
  expectResolvedMethod("clear", {receiverExpr}, &stdClearDef);
  expectResolvedMethod("remove_at", {receiverExpr, indexExpr}, &stdRemoveAtDef);
  expectResolvedMethod("remove_swap", {receiverExpr, indexExpr}, &stdRemoveSwapDef);
}

TEST_CASE("ir lowerer setup type helper rejects explicit rooted vector slash methods while honoring /array/count") {
  primec::Definition arrayCountDef;
  arrayCountDef.fullPath = "/array/count";
  primec::Definition vectorCountDef;
  vectorCountDef.fullPath = "/vector/count";
  primec::Definition stdCountDef;
  stdCountDef.fullPath = "/std/collections/vector/count";
  primec::Definition vectorCapacityDef;
  vectorCapacityDef.fullPath = "/vector/capacity";
  primec::Definition stdCapacityDef;
  stdCapacityDef.fullPath = "/std/collections/vector/capacity";
  primec::Definition vectorAtDef;
  vectorAtDef.fullPath = "/vector/at";
  primec::Definition stdAtDef;
  stdAtDef.fullPath = "/std/collections/vector/at";
  primec::Definition vectorAtUnsafeDef;
  vectorAtUnsafeDef.fullPath = "/vector/at_unsafe";
  primec::Definition stdAtUnsafeDef;
  stdAtUnsafeDef.fullPath = "/std/collections/vector/at_unsafe";
  primec::Definition vectorPushDef;
  vectorPushDef.fullPath = "/vector/push";
  primec::Definition stdPushDef;
  stdPushDef.fullPath = "/std/collections/vector/push";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/array/count", &arrayCountDef},
      {"/vector/count", &vectorCountDef},
      {"/std/collections/vector/count", &stdCountDef},
      {"/vector/capacity", &vectorCapacityDef},
      {"/std/collections/vector/capacity", &stdCapacityDef},
      {"/vector/at", &vectorAtDef},
      {"/std/collections/vector/at", &stdAtDef},
      {"/vector/at_unsafe", &vectorAtUnsafeDef},
      {"/std/collections/vector/at_unsafe", &stdAtUnsafeDef},
      {"/vector/push", &vectorPushDef},
      {"/std/collections/vector/push", &stdPushDef},
  };

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
  locals.emplace("items", itemsLocal);

  auto expectResolvedMethod = [&](const char *methodName,
                                  const std::vector<primec::Expr> &args,
                                  const primec::Definition *expectedDef) {
    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.name = methodName;
    methodCall.isMethodCall = true;
    methodCall.args = args;

    std::string error;
    const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
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
        defMap,
        error);
    CHECK(resolved == expectedDef);
    CHECK(error.empty());
  };
  auto expectRejectedMethod = [&](const char *methodName, const std::vector<primec::Expr> &args) {
    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.name = methodName;
    methodCall.isMethodCall = true;
    methodCall.args = args;

    std::string error;
    const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
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
        defMap,
        error);
    CHECK(resolved == nullptr);
    CHECK(error == std::string("unknown method: ") + methodName);
  };

  primec::Expr arrayCountCall;
  arrayCountCall.kind = primec::Expr::Kind::Call;
  arrayCountCall.name = "/array/count";
  arrayCountCall.isMethodCall = true;
  arrayCountCall.args = {receiverExpr};

  std::string error;
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      arrayCountCall,
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
      defMap,
      error);
  CHECK(resolved == &arrayCountDef);
  CHECK(error.empty());

  expectRejectedMethod("/vector/count", {receiverExpr});
  expectResolvedMethod("/std/collections/vector/count", {receiverExpr}, &stdCountDef);
  expectRejectedMethod("/vector/capacity", {receiverExpr});
  expectResolvedMethod("/std/collections/vector/capacity", {receiverExpr}, &stdCapacityDef);
  expectRejectedMethod("/vector/at", {receiverExpr, indexExpr});
  expectResolvedMethod("/std/collections/vector/at", {receiverExpr, indexExpr}, &stdAtDef);
  expectRejectedMethod("/vector/at_unsafe", {receiverExpr, indexExpr});
  expectResolvedMethod("/std/collections/vector/at_unsafe", {receiverExpr, indexExpr}, &stdAtUnsafeDef);
  expectRejectedMethod("/vector/push", {receiverExpr, indexExpr});
  expectResolvedMethod("/std/collections/vector/push", {receiverExpr, indexExpr}, &stdPushDef);
}

TEST_CASE("ir lowerer setup type helper rejects slash-path map methods from expressions") {
  primec::Definition mapCountDef;
  mapCountDef.fullPath = "/map/count";
  primec::Definition stdMapCountDef;
  stdMapCountDef.fullPath = "/std/collections/map/count";
  primec::Definition mapAtDef;
  mapAtDef.fullPath = "/map/at";
  primec::Definition mapAtUnsafeDef;
  mapAtUnsafeDef.fullPath = "/map/at_unsafe";
  primec::Definition stdMapAtDef;
  stdMapAtDef.fullPath = "/std/collections/map/at";
  primec::Definition stdMapAtUnsafeDef;
  stdMapAtUnsafeDef.fullPath = "/std/collections/map/at_unsafe";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/map/count", &mapCountDef},
      {"/std/collections/map/count", &stdMapCountDef},
      {"/map/at", &mapAtDef},
      {"/map/at_unsafe", &mapAtUnsafeDef},
      {"/std/collections/map/at", &stdMapAtDef},
      {"/std/collections/map/at_unsafe", &stdMapAtUnsafeDef},
  };

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  locals.emplace("values", valuesLocal);

  auto expectUnknownMethod = [&](const char *methodName, const std::vector<primec::Expr> &args,
                                 const char *expectedError) {
    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.name = methodName;
    methodCall.isMethodCall = true;
    methodCall.args = args;

    std::string error;
    const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
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
        defMap,
        error);
    CHECK(resolved == nullptr);
    CHECK(error == std::string(expectedError));
  };

  expectUnknownMethod("/map/count", {receiverExpr}, "unknown method: /map/count");
  expectUnknownMethod(
      "/std/collections/map/count", {receiverExpr}, "unknown method: /std/collections/map/count");
  expectUnknownMethod("/map/contains", {receiverExpr, keyExpr}, "unknown method: /map/contains");
  expectUnknownMethod("/std/collections/map/contains",
                      {receiverExpr, keyExpr},
                      "unknown method: /std/collections/map/contains");
  expectUnknownMethod("/map/tryAt", {receiverExpr, keyExpr}, "unknown method: /map/tryAt");
  expectUnknownMethod("/std/collections/map/tryAt",
                      {receiverExpr, keyExpr},
                      "unknown method: /std/collections/map/tryAt");
  expectUnknownMethod("/map/at", {receiverExpr, keyExpr}, "unknown method: /map/at");
  expectUnknownMethod(
      "/std/collections/map/at", {receiverExpr, keyExpr}, "unknown method: /std/collections/map/at");
  expectUnknownMethod("/map/at_unsafe", {receiverExpr, keyExpr}, "unknown method: /map/at_unsafe");
  expectUnknownMethod("/std/collections/map/at_unsafe",
                      {receiverExpr, keyExpr},
                      "unknown method: /std/collections/map/at_unsafe");
}

TEST_CASE("ir lowerer setup type helper rejects canonical fallback for explicit map contains and tryAt methods") {
  primec::Definition canonicalContainsDef;
  canonicalContainsDef.fullPath = "/std/collections/map/contains";
  primec::Definition canonicalTryAtDef;
  canonicalTryAtDef.fullPath = "/std/collections/map/tryAt";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalContainsDef.fullPath, &canonicalContainsDef},
      {canonicalTryAtDef.fullPath, &canonicalTryAtDef},
  };

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  locals.emplace("values", valuesLocal);

  auto expectUnknownMethod = [&](const char *methodName, const char *expectedError) {
    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.name = methodName;
    methodCall.isMethodCall = true;
    methodCall.args = {receiverExpr, keyExpr};

    std::string error;
    const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
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
        defMap,
        error);
    CHECK(resolved == nullptr);
    CHECK(error == std::string(expectedError));
  };

  expectUnknownMethod("/map/contains", "unknown method: /map/contains");
  expectUnknownMethod("/map/tryAt", "unknown method: /map/tryAt");
  expectUnknownMethod("/std/collections/map/contains", "unknown method: /std/collections/map/contains");
  expectUnknownMethod("/std/collections/map/tryAt", "unknown method: /std/collections/map/tryAt");
}

TEST_CASE("ir lowerer setup type helper prefers canonical bare map contains and tryAt methods") {
  primec::Definition aliasContainsDef;
  aliasContainsDef.fullPath = "/map/contains";
  primec::Definition canonicalContainsDef;
  canonicalContainsDef.fullPath = "/std/collections/map/contains";
  primec::Definition aliasTryAtDef;
  aliasTryAtDef.fullPath = "/map/tryAt";
  primec::Definition canonicalTryAtDef;
  canonicalTryAtDef.fullPath = "/std/collections/map/tryAt";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {aliasContainsDef.fullPath, &aliasContainsDef},
      {canonicalContainsDef.fullPath, &canonicalContainsDef},
      {aliasTryAtDef.fullPath, &aliasTryAtDef},
      {canonicalTryAtDef.fullPath, &canonicalTryAtDef},
  };

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  locals.emplace("values", valuesLocal);

  auto expectResolvedMethod = [&](const char *methodName, const primec::Definition *expected) {
    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.name = methodName;
    methodCall.isMethodCall = true;
    methodCall.args = {receiverExpr, keyExpr};

    std::string error;
    const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
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
        defMap,
        error);
    CHECK(resolved == expected);
    CHECK(error.empty());
  };

  expectResolvedMethod("contains", &canonicalContainsDef);
  expectResolvedMethod("tryAt", &canonicalTryAtDef);
}

TEST_CASE("ir lowerer setup type helper rejects explicit map contains and tryAt slash methods even when definitions exist") {
  primec::Definition aliasContainsDef;
  aliasContainsDef.fullPath = "/map/contains";
  primec::Definition canonicalContainsDef;
  canonicalContainsDef.fullPath = "/std/collections/map/contains";
  primec::Definition aliasTryAtDef;
  aliasTryAtDef.fullPath = "/map/tryAt";
  primec::Definition canonicalTryAtDef;
  canonicalTryAtDef.fullPath = "/std/collections/map/tryAt";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {aliasContainsDef.fullPath, &aliasContainsDef},
      {canonicalContainsDef.fullPath, &canonicalContainsDef},
      {aliasTryAtDef.fullPath, &aliasTryAtDef},
      {canonicalTryAtDef.fullPath, &canonicalTryAtDef},
  };

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  locals.emplace("values", valuesLocal);

  auto expectUnknownMethod = [&](const char *methodName, const char *expectedError) {
    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.name = methodName;
    methodCall.isMethodCall = true;
    methodCall.args = {receiverExpr, keyExpr};

    std::string error;
    const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
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
        defMap,
        error);
    CHECK(resolved == nullptr);
    CHECK(error == std::string(expectedError));
  };

  expectUnknownMethod("/map/contains", "unknown method: /map/contains");
  expectUnknownMethod("/map/tryAt", "unknown method: /map/tryAt");
  expectUnknownMethod("/std/collections/map/contains", "unknown method: /std/collections/map/contains");
  expectUnknownMethod("/std/collections/map/tryAt", "unknown method: /std/collections/map/tryAt");
}

TEST_CASE("ir lowerer setup type helper resolves declared receiver aliases through slashless map imports") {
  primec::Definition wrapMapDef;
  wrapMapDef.fullPath = "/pkg/wrapMap";
  wrapMapDef.namespacePrefix = "/pkg";
  primec::Transform returnAlias;
  returnAlias.name = "return";
  returnAlias.templateArgs = {"MapAtAlias"};
  wrapMapDef.transforms.push_back(returnAlias);

  primec::Definition mapTagDef;
  mapTagDef.fullPath = "/map/at/tag";

  const std::unordered_map<std::string, std::string> importAliases = {
      {"MapAtAlias", "std/collections/map/at"},
  };

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "wrapMap";
  receiverCall.namespacePrefix = "/pkg";

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

  const auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.kind == primec::Expr::Kind::Call && expr.name == "wrapMap") {
      return std::string("/pkg/wrapMap");
    }
    if (!expr.name.empty() && expr.name.front() == '/') {
      return expr.name;
    }
    return std::string("/") + expr.name;
  };

  std::string error;
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {wrapMapDef.fullPath, &wrapMapDef},
      {mapTagDef.fullPath, &mapTagDef},
  };
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      importAliases,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      resolveExprPath,
      defMap,
      error);
  CHECK(resolved == &mapTagDef);
  CHECK(error.empty());

  const std::unordered_map<std::string, const primec::Definition *> missingMethodDefMap = {
      {wrapMapDef.fullPath, &wrapMapDef},
  };
  error.clear();
  CHECK(primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
            methodCall,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            importAliases,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            resolveExprPath,
            missingMethodDefMap,
            error) == nullptr);
  CHECK(error == "unknown method: /std/collections/map/at/tag");
}

TEST_CASE("ir lowerer setup type helper resolves canonical map methods from slashless receiver call paths") {
  primec::Definition makeValuesDef;
  makeValuesDef.fullPath = "/makeValues";
  primec::Transform returnMap;
  returnMap.name = "return";
  returnMap.templateArgs = {"/std/collections/map<i32, i32>"};
  makeValuesDef.transforms.push_back(returnMap);

  primec::Definition canonicalCountDef;
  canonicalCountDef.fullPath = "/std/collections/map/count";

  primec::Definition canonicalContainsDef;
  canonicalContainsDef.fullPath = "/std/collections/map/contains";
  primec::Transform returnMarker;
  returnMarker.name = "return";
  returnMarker.templateArgs = {"Marker"};
  canonicalContainsDef.transforms.push_back(returnMarker);

  primec::Definition markerTagDef;
  markerTagDef.fullPath = "/Marker/tag";

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {makeValuesDef.fullPath, &makeValuesDef},
      {canonicalCountDef.fullPath, &canonicalCountDef},
      {canonicalContainsDef.fullPath, &canonicalContainsDef},
      {markerTagDef.fullPath, &markerTagDef},
  };

  const auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.kind != primec::Expr::Kind::Call) {
      return expr.name;
    }
    if (expr.name == "count" || expr.name == "contains" || expr.name == "tag") {
      return std::string();
    }
    return expr.name;
  };

  primec::Expr makeValuesCall;
  makeValuesCall.kind = primec::Expr::Kind::Call;
  makeValuesCall.name = "makeValues";

  primec::Expr countMethod;
  countMethod.kind = primec::Expr::Kind::Call;
  countMethod.name = "count";
  countMethod.isMethodCall = true;
  countMethod.args = {makeValuesCall};

  std::string error;
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      countMethod,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      {},
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      resolveExprPath,
      defMap,
      error);
  CHECK(resolved == &canonicalCountDef);
  CHECK(error.empty());

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::Expr containsMethod;
  containsMethod.kind = primec::Expr::Kind::Call;
  containsMethod.name = "contains";
  containsMethod.isMethodCall = true;
  containsMethod.args = {makeValuesCall, keyExpr};

  primec::Expr tagMethod;
  tagMethod.kind = primec::Expr::Kind::Call;
  tagMethod.name = "tag";
  tagMethod.isMethodCall = true;
  tagMethod.args = {containsMethod};

  error.clear();
  resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      tagMethod,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      {},
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      resolveExprPath,
      defMap,
      error);
  CHECK(resolved == &markerTagDef);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper resolves canonical map methods from generated receiver overload paths") {
  primec::Definition makeValuesDef;
  makeValuesDef.fullPath = "/makeValues__ov0";
  primec::Transform returnMap;
  returnMap.name = "return";
  returnMap.templateArgs = {"/std/collections/map<i32, i32>"};
  makeValuesDef.transforms.push_back(returnMap);

  primec::Definition canonicalCountDef;
  canonicalCountDef.fullPath = "/std/collections/map/count";

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {makeValuesDef.fullPath, &makeValuesDef},
      {canonicalCountDef.fullPath, &canonicalCountDef},
  };

  primec::Expr makeValuesCall;
  makeValuesCall.kind = primec::Expr::Kind::Call;
  makeValuesCall.name = "makeValues";

  primec::Expr countMethod;
  countMethod.kind = primec::Expr::Kind::Call;
  countMethod.name = "count";
  countMethod.isMethodCall = true;
  countMethod.args = {makeValuesCall};

  std::string error;
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      countMethod,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      {},
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &expr) {
        if (expr.kind == primec::Expr::Kind::Call && expr.name == "makeValues") {
          return std::string("makeValues");
        }
        return std::string();
      },
      defMap,
      error);
  CHECK(resolved == &canonicalCountDef);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper resolves struct receiver method definitions from expressions") {
  primec::Definition structMethodDef;
  structMethodDef.fullPath = "/pkg/Ctor/length";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Ctor/length", &structMethodDef},
  };

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "Ctor";

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "length";
  methodCall.isMethodCall = true;
  methodCall.args.push_back(receiverCall);

  std::string error;
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      {},
      {"/pkg/Ctor"},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return std::string("/pkg/Ctor"); },
      defMap,
      error);
  CHECK(resolved == &structMethodDef);
  CHECK(error.empty());
}

TEST_SUITE_END();
