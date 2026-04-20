#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer call helpers dispatch inline call with count fallbacks") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;

  primec::Definition callee;
  callee.fullPath = "/pkg/helper";

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "count";
  primec::Expr countArg;
  countArg.kind = primec::Expr::Kind::Name;
  countArg.name = "items";
  countCall.args.push_back(countArg);

  std::string error;

  int firstResolveMethodCalls = 0;
  int firstResolveDefinitionCalls = 0;
  int firstEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            countCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++firstResolveMethodCalls;
              return &callee;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++firstResolveDefinitionCalls;
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              ++firstEmitCalls;
              return true;
            },
            error) == Result::Emitted);
  CHECK(firstResolveMethodCalls == 1);
  CHECK(firstResolveDefinitionCalls == 1);
  CHECK(firstEmitCalls == 1);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "helper";
  methodCall.isMethodCall = true;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            methodCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              CHECK(false);
              return false;
            },
            error) == Result::Error);

  primec::Expr methodCountCall;
  methodCountCall.kind = primec::Expr::Kind::Call;
  methodCountCall.name = "count";
  methodCountCall.isMethodCall = true;
  methodCountCall.args.push_back(countArg);
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            methodCountCall,
            [](const primec::Expr &) { return true; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &, const primec::Definition &) { return false; },
            error) == Result::NotHandled);
  CHECK(error.empty());

  primec::Expr indexArg;
  indexArg.kind = primec::Expr::Kind::Literal;
  indexArg.intWidth = 32;
  indexArg.literalValue = 1;
  primec::Expr methodAccessCall;
  methodAccessCall.kind = primec::Expr::Kind::Call;
  methodAccessCall.name = "at";
  methodAccessCall.isMethodCall = true;
  methodAccessCall.args = {countArg, indexArg};
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            methodAccessCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &, const primec::Definition &) { return false; },
            error) == Result::NotHandled);
  CHECK(error.empty());

  primec::Expr directCall;
  directCall.kind = primec::Expr::Kind::Call;
  directCall.name = "helper";
  int directEmitCalls = 0;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            directCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * { return &callee; },
            [&](const primec::Expr &, const primec::Definition &) {
              ++directEmitCalls;
              return true;
            },
            error) == Result::Emitted);
  CHECK(directEmitCalls == 1);

  primec::Expr canonicalMapCountCall = countCall;
  canonicalMapCountCall.name = "/std/collections/map/count";
  int canonicalMapCountResolveMethodCalls = 0;
  int canonicalMapCountResolveDefinitionCalls = 0;
  int canonicalMapCountEmitCalls = 0;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            canonicalMapCountCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++canonicalMapCountResolveMethodCalls;
              return &callee;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              ++canonicalMapCountResolveDefinitionCalls;
              CHECK(callExpr.name == "/std/collections/map/count");
              CHECK_FALSE(callExpr.isMethodCall);
              return &callee;
            },
            [&](const primec::Expr &callExpr, const primec::Definition &) {
              ++canonicalMapCountEmitCalls;
              CHECK(callExpr.name == "count");
              CHECK(callExpr.isMethodCall);
              return true;
            },
            error) == Result::Emitted);
  CHECK(canonicalMapCountResolveMethodCalls == 1);
  CHECK(canonicalMapCountResolveDefinitionCalls == 1);
  CHECK(canonicalMapCountEmitCalls == 1);

  primec::Expr explicitVectorCountCall = countCall;
  explicitVectorCountCall.name = "/vector/count";
  int explicitVectorCountResolveMethodCalls = 0;
  int explicitVectorCountResolveDefinitionCalls = 0;
  int explicitVectorCountEmitCalls = 0;
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            explicitVectorCountCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++explicitVectorCountResolveMethodCalls;
              return &callee;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++explicitVectorCountResolveDefinitionCalls;
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              ++explicitVectorCountEmitCalls;
              return true;
            },
            error) == Result::NotHandled);
  CHECK(error == "stale");
  CHECK(explicitVectorCountResolveMethodCalls == 0);
  CHECK(explicitVectorCountResolveDefinitionCalls == 2);
  CHECK(explicitVectorCountEmitCalls == 0);

  primec::Definition vectorAccessDef;
  vectorAccessDef.fullPath = "/std/collections/vector/at";
  primec::Expr explicitVectorAtCall = methodAccessCall;
  explicitVectorAtCall.isMethodCall = false;
  explicitVectorAtCall.name = "/std/collections/vector/at";
  int explicitVectorAtResolveMethodCalls = 0;
  int explicitVectorAtResolveDefinitionCalls = 0;
  int explicitVectorAtEmitCalls = 0;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            explicitVectorAtCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++explicitVectorAtResolveMethodCalls;
              return &callee;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              ++explicitVectorAtResolveDefinitionCalls;
              CHECK(callExpr.name == "/std/collections/vector/at");
              CHECK_FALSE(callExpr.isMethodCall);
              return &vectorAccessDef;
            },
            [&](const primec::Expr &callExpr, const primec::Definition &resolvedCallee) {
              ++explicitVectorAtEmitCalls;
              CHECK(callExpr.name == "at");
              CHECK(callExpr.isMethodCall);
              CHECK(resolvedCallee.fullPath == "/pkg/helper");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(explicitVectorAtResolveMethodCalls == 1);
  CHECK(explicitVectorAtResolveDefinitionCalls == 1);
  CHECK(explicitVectorAtEmitCalls == 1);

  primec::Expr canonicalPushCall = countCall;
  canonicalPushCall.name = "/std/collections/vector/push";
  canonicalPushCall.args.push_back(indexArg);
  int canonicalPushResolveMethodCalls = 0;
  int canonicalPushResolveDefinitionCalls = 0;
  int canonicalPushEmitCalls = 0;
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            canonicalPushCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++canonicalPushResolveMethodCalls;
              return &callee;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++canonicalPushResolveDefinitionCalls;
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              ++canonicalPushEmitCalls;
              return true;
            },
            error) == Result::Emitted);
  CHECK(error == "stale");
  CHECK(canonicalPushResolveMethodCalls == 1);
  CHECK(canonicalPushResolveDefinitionCalls == 1);
  CHECK(canonicalPushEmitCalls == 1);

  primec::Definition vectorPushDef;
  vectorPushDef.fullPath = "/std/collections/vector/push";
  int canonicalPushDirectResolveMethodCalls = 0;
  int canonicalPushDirectResolveDefinitionCalls = 0;
  int canonicalPushDirectEmitCalls = 0;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            canonicalPushCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++canonicalPushDirectResolveMethodCalls;
              return &callee;
            },
            [&](const primec::Expr &callExpr) -> const primec::Definition * {
              ++canonicalPushDirectResolveDefinitionCalls;
              CHECK(callExpr.name == "/std/collections/vector/push");
              CHECK_FALSE(callExpr.isMethodCall);
              return &vectorPushDef;
            },
            [&](const primec::Expr &callExpr, const primec::Definition &resolvedCallee) {
              ++canonicalPushDirectEmitCalls;
              CHECK(callExpr.name == "push");
              CHECK(callExpr.isMethodCall);
              CHECK(resolvedCallee.fullPath == "/pkg/helper");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(canonicalPushDirectResolveMethodCalls == 1);
  CHECK(canonicalPushDirectResolveDefinitionCalls == 1);
  CHECK(canonicalPushDirectEmitCalls == 1);

  primec::Expr experimentalMapCountCall = countCall;
  experimentalMapCountCall.name = "/std/collections/experimental_map/mapCount";
  int experimentalMapCountResolveMethodCalls = 0;
  int experimentalMapCountResolveDefinitionCalls = 0;
  int experimentalMapCountEmitCalls = 0;
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            experimentalMapCountCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++experimentalMapCountResolveMethodCalls;
              return &callee;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++experimentalMapCountResolveDefinitionCalls;
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              ++experimentalMapCountEmitCalls;
              return true;
            },
            error) == Result::NotHandled);
  CHECK(error == "stale");
  CHECK(experimentalMapCountResolveMethodCalls == 0);
  CHECK(experimentalMapCountResolveDefinitionCalls == 2);
  CHECK(experimentalMapCountEmitCalls == 0);

  int secondResolveMethodCalls = 0;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            countCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++secondResolveMethodCalls;
              return secondResolveMethodCalls == 1 ? nullptr : &callee;
            },
            [&](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &, const primec::Definition &) { return true; },
            error) == Result::Emitted);
  CHECK(secondResolveMethodCalls == 2);

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            directCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * { return &callee; },
            [&](const primec::Expr &, const primec::Definition &) {
              error = "emit failed";
              return false;
            },
            error) == Result::Error);
  CHECK(error == "emit failed");

  primec::Expr plainCall;
  plainCall.kind = primec::Expr::Kind::Call;
  plainCall.name = "plain";
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            plainCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &, const primec::Definition &) {
              CHECK(false);
              return false;
            },
            error) == Result::NotHandled);
}

TEST_CASE("ir lowerer call helpers preserve canonical map helper method return chains") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;

  auto isMapReceiver = [](const primec::Expr &expr) {
    return expr.kind == primec::Expr::Kind::Name && expr.name == "values";
  };
  auto makeReturnTransform = [](const std::string &typeText) {
    primec::Transform transform;
    transform.name = "return";
    transform.templateArgs = {typeText};
    return transform;
  };
  auto makeMapHelper = [&](const std::string &path, const std::string &returnType) {
    primec::Definition def;
    def.fullPath = path;
    const size_t slash = path.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      def.namespacePrefix = path.substr(0, slash);
    }
    def.transforms.push_back(makeReturnTransform(returnType));
    return def;
  };

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  primec::Expr containsCall;
  containsCall.kind = primec::Expr::Kind::Call;
  containsCall.name = "contains";
  containsCall.isMethodCall = true;
  containsCall.args = {valuesName, keyLiteral};

  const primec::Definition markerContains =
      makeMapHelper("/std/collections/map/contains", "Marker");
  int markerContainsResolveCalls = 0;
  int markerContainsEmitCalls = 0;
  std::string error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            containsCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            isMapReceiver,
            [&](const primec::Expr &) -> const primec::Definition * {
              ++markerContainsResolveCalls;
              return &markerContains;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &callExpr, const primec::Definition &resolvedCallee) {
              ++markerContainsEmitCalls;
              CHECK(callExpr.name == "contains");
              CHECK(callExpr.isMethodCall);
              CHECK(resolvedCallee.fullPath == "/std/collections/map/contains");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error == "stale");
  CHECK(markerContainsResolveCalls == 1);
  CHECK(markerContainsEmitCalls == 1);

  const primec::Definition builtinContains =
      makeMapHelper("/std/collections/map/contains", "bool");
  int builtinContainsEmitCalls = 0;
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            containsCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            isMapReceiver,
            [&](const primec::Expr &) -> const primec::Definition * { return &builtinContains; },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              ++builtinContainsEmitCalls;
              return true;
            },
            error) == Result::NotHandled);
  CHECK(error == "stale");
  CHECK(builtinContainsEmitCalls == 0);

  primec::Expr tryAtCall;
  tryAtCall.kind = primec::Expr::Kind::Call;
  tryAtCall.name = "tryAt";
  tryAtCall.isMethodCall = true;
  tryAtCall.args = {valuesName, keyLiteral};

  const primec::Definition markerTryAt =
      makeMapHelper("/std/collections/map/tryAt", "Marker");
  int markerTryAtResolveCalls = 0;
  int markerTryAtEmitCalls = 0;
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            tryAtCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            isMapReceiver,
            [&](const primec::Expr &) -> const primec::Definition * {
              ++markerTryAtResolveCalls;
              return &markerTryAt;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &callExpr, const primec::Definition &resolvedCallee) {
              ++markerTryAtEmitCalls;
              CHECK(callExpr.name == "tryAt");
              CHECK(callExpr.isMethodCall);
              CHECK(resolvedCallee.fullPath == "/std/collections/map/tryAt");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error == "stale");
  CHECK(markerTryAtResolveCalls == 1);
  CHECK(markerTryAtEmitCalls == 1);

  const primec::Definition builtinTryAt =
      makeMapHelper("/std/collections/map/tryAt", "Result<i32, ContainerError>");
  int builtinTryAtEmitCalls = 0;
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            tryAtCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            isMapReceiver,
            [&](const primec::Expr &) -> const primec::Definition * { return &builtinTryAt; },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              ++builtinTryAtEmitCalls;
              return true;
            },
            error) == Result::NotHandled);
  CHECK(error == "stale");
  CHECK(builtinTryAtEmitCalls == 0);
}

TEST_CASE("ir lowerer call helpers prefer direct same-path map count-like defs") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;

  auto makeDirectCall = [](const std::string &name, std::vector<primec::Expr> args) {
    primec::Expr callExpr;
    callExpr.kind = primec::Expr::Kind::Call;
    callExpr.name = name;
    callExpr.args = std::move(args);
    return callExpr;
  };
  auto makeNamespacedDirectCall = [](const std::string &namespacePrefix,
                                     const std::string &name,
                                     std::vector<primec::Expr> args) {
    primec::Expr callExpr;
    callExpr.kind = primec::Expr::Kind::Call;
    callExpr.namespacePrefix = namespacePrefix;
    callExpr.name = name;
    callExpr.args = std::move(args);
    return callExpr;
  };

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  primec::Definition rootedCount;
  rootedCount.fullPath = "/map/count";

  primec::Definition rootedContains;
  rootedContains.fullPath = "/map/contains";

  primec::Definition rootedTryAt;
  rootedTryAt.fullPath = "/map/tryAt";

  auto expectDirectDefWins = [&](primec::Expr callExpr, const primec::Definition &expectedCallee) {
    int resolveMethodCalls = 0;
    int resolveDefinitionCalls = 0;
    int emitCalls = 0;
    std::string error = "stale";
    CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
              callExpr,
              [](const primec::Expr &) { return false; },
              [](const primec::Expr &) { return false; },
              [](const primec::Expr &) { return false; },
              [&](const primec::Expr &) -> const primec::Definition * {
                ++resolveMethodCalls;
                return &expectedCallee;
              },
              [&](const primec::Expr &candidate) -> const primec::Definition * {
                ++resolveDefinitionCalls;
                CHECK(candidate.name == callExpr.name);
                CHECK(candidate.namespacePrefix == callExpr.namespacePrefix);
                CHECK_FALSE(candidate.isMethodCall);
                return &expectedCallee;
              },
              [&](const primec::Expr &emittedExpr, const primec::Definition &resolvedCallee) {
                ++emitCalls;
                CHECK(emittedExpr.name == callExpr.name);
                CHECK(emittedExpr.namespacePrefix == callExpr.namespacePrefix);
                CHECK_FALSE(emittedExpr.isMethodCall);
                CHECK(resolvedCallee.fullPath == expectedCallee.fullPath);
                return true;
              },
              error) == Result::Emitted);
    CHECK(error == "stale");
    CHECK(resolveMethodCalls == 0);
    CHECK(resolveDefinitionCalls == 1);
    CHECK(emitCalls == 1);
  };

  expectDirectDefWins(makeDirectCall("/map/count", {valuesName}), rootedCount);
  expectDirectDefWins(makeDirectCall("/map/contains", {valuesName, keyLiteral}), rootedContains);
  expectDirectDefWins(makeDirectCall("/map/tryAt", {valuesName, keyLiteral}), rootedTryAt);
  expectDirectDefWins(
      makeNamespacedDirectCall("/std/collections/map", "contains", {valuesName, keyLiteral}),
      rootedContains);
  expectDirectDefWins(
      makeNamespacedDirectCall("/std/collections/map", "tryAt", {valuesName, keyLiteral}),
      rootedTryAt);
}

TEST_CASE("ir lowerer call helpers keep map count and local access same-path defs") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;

  auto isMapReceiver = [](const primec::Expr &receiverExpr) {
    return receiverExpr.kind == primec::Expr::Kind::Name ||
           receiverExpr.kind == primec::Expr::Kind::Call;
  };

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  primec::Definition canonicalCount;
  canonicalCount.fullPath = "/std/collections/map/count";
  canonicalCount.transforms.push_back(primec::Transform{.name = "return", .arguments = {"i32"}});

  primec::Expr countMethod;
  countMethod.kind = primec::Expr::Kind::Call;
  countMethod.name = "count";
  countMethod.isMethodCall = true;
  countMethod.args = {valuesName};

  int countEmitCalls = 0;
  std::string error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            countMethod,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            isMapReceiver,
            [&](const primec::Expr &) -> const primec::Definition * { return &canonicalCount; },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &callExpr, const primec::Definition &resolvedCallee) {
              ++countEmitCalls;
              CHECK(callExpr.name == "count");
              CHECK(callExpr.isMethodCall);
              CHECK(resolvedCallee.fullPath == "/std/collections/map/count");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error == "stale");
  CHECK(countEmitCalls == 1);

  primec::Definition canonicalAt;
  canonicalAt.fullPath = "/std/collections/map/at";
  canonicalAt.transforms.push_back(primec::Transform{.name = "return", .arguments = {"i32"}});

  primec::Expr atMethod;
  atMethod.kind = primec::Expr::Kind::Call;
  atMethod.name = "at";
  atMethod.isMethodCall = true;
  atMethod.args = {valuesName, keyLiteral};

  int atEmitCalls = 0;
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            atMethod,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            isMapReceiver,
            [&](const primec::Expr &) -> const primec::Definition * { return &canonicalAt; },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &callExpr, const primec::Definition &resolvedCallee) {
              ++atEmitCalls;
              CHECK(callExpr.name == "at");
              CHECK(callExpr.isMethodCall);
              CHECK(resolvedCallee.fullPath == "/std/collections/map/at");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error == "stale");
  CHECK(atEmitCalls == 1);

  primec::Expr wrapCall;
  wrapCall.kind = primec::Expr::Kind::Call;
  wrapCall.name = "wrapValues";

  primec::Expr wrapperAtMethod = atMethod;
  wrapperAtMethod.args.front() = wrapCall;

  int wrapperAtEmitCalls = 0;
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            wrapperAtMethod,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            isMapReceiver,
            [&](const primec::Expr &) -> const primec::Definition * { return &canonicalAt; },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              ++wrapperAtEmitCalls;
              return true;
            },
            error) == Result::NotHandled);
  CHECK(error == "stale");
  CHECK(wrapperAtEmitCalls == 0);
}

TEST_CASE("ir lowerer call helpers dispatch bare semantic map sugar inline") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;
  using LocalInfo = primec::ir_lowerer::LocalInfo;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo mapInfo;
  mapInfo.kind = LocalInfo::Kind::Map;
  mapInfo.mapKeyKind = LocalInfo::ValueKind::Int32;
  mapInfo.mapValueKind = LocalInfo::ValueKind::Int32;
  locals.emplace("values", mapInfo);

  primec::Definition canonicalCount;
  canonicalCount.fullPath = "/std/collections/map/count";
  primec::Definition canonicalAt;
  canonicalAt.fullPath = "/std/collections/map/at";
  primec::Definition canonicalAtUnsafe;
  canonicalAtUnsafe.fullPath = "/std/collections/map/at_unsafe";

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  auto expectEmitted = [&](primec::Expr callExpr, const primec::Definition &expectedCallee) {
    int emitCalls = 0;
    std::string error = "stale";
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
                if (expr.name == "count") {
                  return &canonicalCount;
                }
                if (expr.name == "at") {
                  return &canonicalAt;
                }
                if (expr.name == "at_unsafe") {
                  return &canonicalAtUnsafe;
                }
                return nullptr;
              },
              [&](const primec::Expr &emittedExpr,
                  const primec::Definition &resolvedCallee,
                  const primec::ir_lowerer::LocalMap &) {
                ++emitCalls;
                CHECK(emittedExpr.name == callExpr.name);
                CHECK(resolvedCallee.fullPath == expectedCallee.fullPath);
                return true;
              },
              error) == Result::Emitted);
    CHECK(error == "stale");
    CHECK(emitCalls == 1);
  };

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "count";
  countCall.semanticNodeId = 101;
  countCall.args = {valuesName};
  expectEmitted(countCall, canonicalCount);

  primec::Expr atCall;
  atCall.kind = primec::Expr::Kind::Call;
  atCall.name = "at";
  atCall.semanticNodeId = 102;
  atCall.args = {valuesName, keyLiteral};
  expectEmitted(atCall, canonicalAt);

  primec::Expr atUnsafeCall = atCall;
  atUnsafeCall.name = "at_unsafe";
  atUnsafeCall.semanticNodeId = 103;
  expectEmitted(atUnsafeCall, canonicalAtUnsafe);
}

TEST_CASE("ir lowerer call helpers preserve direct wrapper map access defs") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;

  auto isMapReceiver = [](const primec::Expr &expr) {
    return (expr.kind == primec::Expr::Kind::Name && expr.name == "values") ||
           (expr.kind == primec::Expr::Kind::Call && expr.name == "wrapValues");
  };

  primec::Definition canonicalAt;
  canonicalAt.fullPath = "/std/collections/map/at";
  canonicalAt.transforms.push_back(primec::Transform{.name = "return", .arguments = {"i32"}});

  primec::Definition canonicalAtUnsafe;
  canonicalAtUnsafe.fullPath = "/std/collections/map/at_unsafe";
  canonicalAtUnsafe.transforms.push_back(primec::Transform{.name = "return", .arguments = {"i32"}});

  primec::Expr wrapCall;
  wrapCall.kind = primec::Expr::Kind::Call;
  wrapCall.name = "wrapValues";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  auto expectEmitted = [&](primec::Expr callExpr, const primec::Definition &expectedCallee) {
    int emitCalls = 0;
    std::string error = "stale";
    CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
              callExpr,
              [](const primec::Expr &) { return false; },
              [](const primec::Expr &) { return false; },
              [](const primec::Expr &) { return false; },
              isMapReceiver,
              [&](const primec::Expr &) -> const primec::Definition * {
                CHECK(false);
                return nullptr;
              },
              [&](const primec::Expr &expr) -> const primec::Definition * {
                if (expr.name == "at") {
                  return &canonicalAt;
                }
                if (expr.name == "at_unsafe") {
                  return &canonicalAtUnsafe;
                }
                return nullptr;
              },
              [&](const primec::Expr &emittedExpr, const primec::Definition &resolvedCallee) {
                ++emitCalls;
                CHECK(emittedExpr.name == callExpr.name);
                CHECK_FALSE(emittedExpr.isMethodCall);
                CHECK(resolvedCallee.fullPath == expectedCallee.fullPath);
                return true;
              },
              error) == Result::Emitted);
    CHECK(error == "stale");
    CHECK(emitCalls == 1);
  };

  primec::Expr atCall;
  atCall.kind = primec::Expr::Kind::Call;
  atCall.name = "at";
  atCall.args = {wrapCall, keyLiteral};
  expectEmitted(atCall, canonicalAt);

  primec::Expr atUnsafeCall = atCall;
  atUnsafeCall.name = "at_unsafe";
  expectEmitted(atUnsafeCall, canonicalAtUnsafe);
}


TEST_SUITE_END();
