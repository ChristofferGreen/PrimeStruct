#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("emitter expr control count-rewrite step only rewrites bare collection calls") {
  primec::Expr methodExpr;
  methodExpr.kind = primec::Expr::Kind::Call;
  methodExpr.name = "count";
  methodExpr.isMethodCall = true;
  CHECK_FALSE(primec::emitter::runEmitterExprControlCountRewriteStep(
      methodExpr,
      "count",
      {},
      {},
      {},
      {},
      {},
      [&](const primec::Expr &, std::string &) { return true; }).has_value());

  primec::Expr wrongNameExpr = methodExpr;
  wrongNameExpr.isMethodCall = false;
  wrongNameExpr.name = "size";
  CHECK_FALSE(primec::emitter::runEmitterExprControlCountRewriteStep(
      wrongNameExpr,
      "size",
      {},
      {},
      {},
      {},
      {},
      [&](const primec::Expr &, std::string &) { return true; }).has_value());

  primec::Expr wrongArityExpr = wrongNameExpr;
  wrongArityExpr.name = "count";
  wrongArityExpr.args.clear();
  CHECK_FALSE(primec::emitter::runEmitterExprControlCountRewriteStep(
      wrongArityExpr,
      "count",
      {},
      {},
      {},
      {},
      {},
      [&](const primec::Expr &, std::string &) { return true; }).has_value());

  primec::Expr countExpr = wrongArityExpr;
  countExpr.args.push_back(primec::Expr{});
  CHECK_FALSE(primec::emitter::runEmitterExprControlCountRewriteStep(
      countExpr,
      "count",
      {{"count", "/already/resolved"}},
      {},
      {},
      {},
      {},
      [&](const primec::Expr &, std::string &) { return true; }).has_value());

  bool resolverCalled = false;
  auto countLikeResolvedPath = primec::emitter::runEmitterExprControlCountRewriteStep(
      countExpr,
      "count",
      {},
      {},
      [&](const primec::Expr &, const std::unordered_map<std::string, primec::Emitter::BindingInfo> &) {
        return true;
      },
      {},
      {},
      [&](const primec::Expr &, std::string &pathOut) {
        resolverCalled = true;
        pathOut = "/Vec3/count";
        return true;
      });
  REQUIRE(countLikeResolvedPath.has_value());
  CHECK(*countLikeResolvedPath == "/Vec3/count");
  CHECK(resolverCalled);

  primec::Expr aliasCountExpr = countExpr;
  aliasCountExpr.name = "/std/collections/vector/count";
  int aliasCountResolveCalls = 0;
  CHECK_FALSE(primec::emitter::runEmitterExprControlCountRewriteStep(
                  aliasCountExpr,
                  "/std/collections/vector/count",
                  {},
                  {},
                  {},
                  {},
                  {},
                  [&](const primec::Expr &, std::string &) {
                    ++aliasCountResolveCalls;
                    return true;
                  })
                  .has_value());
  CHECK(aliasCountResolveCalls == 0);

  primec::Expr aliasMapCountExpr = countExpr;
  aliasMapCountExpr.name = "/std/collections/map/count";
  int aliasMapCountResolveCalls = 0;
  CHECK_FALSE(primec::emitter::runEmitterExprControlCountRewriteStep(
                  aliasMapCountExpr,
                  "/std/collections/map/count",
                  {},
                  {},
                  {},
                  {},
                  {},
                  [&](const primec::Expr &, std::string &) {
                    ++aliasMapCountResolveCalls;
                    return true;
                  })
                  .has_value());
  CHECK(aliasMapCountResolveCalls == 0);

  std::unordered_map<std::string, std::string> aliasOnlyMapCountNameMap{
      {"/map/count", "ps_map_count"}};
  CHECK_FALSE(primec::emitter::runEmitterExprControlCountRewriteStep(
                  aliasMapCountExpr,
                  "/std/collections/map/count",
                  aliasOnlyMapCountNameMap,
                  {},
                  {},
                  {},
                  {},
                  [&](const primec::Expr &, std::string &) { return true; })
                  .has_value());

  primec::Expr aliasCapacityExpr = countExpr;
  aliasCapacityExpr.name = "/vector/capacity";
  int aliasCapacityResolveCalls = 0;
  auto aliasCapacityResolvedPath = primec::emitter::runEmitterExprControlCountRewriteStep(
      aliasCapacityExpr,
      "/vector/capacity",
      {},
      {},
      {},
      {},
      {},
      [&](const primec::Expr &methodCandidate, std::string &pathOut) {
        ++aliasCapacityResolveCalls;
        CHECK(methodCandidate.isMethodCall);
        CHECK(methodCandidate.name == "capacity");
        pathOut = "/vector/capacity";
        return true;
      });
  CHECK_FALSE(aliasCapacityResolvedPath.has_value());
  CHECK(aliasCapacityResolveCalls == 0);

  primec::Expr accessExpr = wrongArityExpr;
  accessExpr.name = "at";
  accessExpr.args = {primec::Expr{}, primec::Expr{}};
  accessExpr.args[0].kind = primec::Expr::Kind::Literal;
  accessExpr.args[0].literalValue = 1;
  accessExpr.args[1].kind = primec::Expr::Kind::Name;
  accessExpr.args[1].name = "values";
  accessExpr.argNames = {std::string("index"), std::string("values")};
  int accessResolveCalls = 0;
  bool sawReorderedReceiver = false;
  auto accessResolvedPath = primec::emitter::runEmitterExprControlCountRewriteStep(
      accessExpr,
      "at",
      {},
      {},
      {},
      {},
      {},
      [&](const primec::Expr &methodCandidate, std::string &pathOut) {
        ++accessResolveCalls;
        if (!methodCandidate.isMethodCall || methodCandidate.argNames.size() < 2 ||
            !methodCandidate.argNames[0].has_value() || !methodCandidate.argNames[1].has_value()) {
          return false;
        }
        if (*methodCandidate.argNames[0] != "values" || *methodCandidate.argNames[1] != "index") {
          return false;
        }
        sawReorderedReceiver = true;
        pathOut = "/vector/at";
        return true;
      });
  REQUIRE(accessResolvedPath.has_value());
  CHECK(*accessResolvedPath == "/vector/at");
  CHECK(sawReorderedReceiver);
  CHECK(accessResolveCalls == 1);

  primec::Expr aliasAccessExpr = accessExpr;
  aliasAccessExpr.name = "/std/collections/vector/at";
  int aliasAccessResolveCalls = 0;
  CHECK_FALSE(primec::emitter::runEmitterExprControlCountRewriteStep(
                  aliasAccessExpr,
                  "/std/collections/vector/at",
                  {},
                  {},
                  {},
                  {},
                  {},
                  [&](const primec::Expr &, std::string &) {
                    ++aliasAccessResolveCalls;
                    return true;
                  })
                  .has_value());
  CHECK(aliasAccessResolveCalls == 0);

  primec::Expr aliasMapAccessExpr = accessExpr;
  aliasMapAccessExpr.name = "/map/at";
  int aliasMapAccessResolveCalls = 0;
  CHECK_FALSE(primec::emitter::runEmitterExprControlCountRewriteStep(
                  aliasMapAccessExpr,
                  "/map/at",
                  {},
                  {},
                  {},
                  {},
                  {},
                  [&](const primec::Expr &, std::string &) {
                    ++aliasMapAccessResolveCalls;
                    return true;
                  })
                  .has_value());
  CHECK(aliasMapAccessResolveCalls == 0);

  primec::Expr canonicalMapAccessExpr = accessExpr;
  canonicalMapAccessExpr.name = "/std/collections/map/at";
  int canonicalMapAccessResolveCalls = 0;
  CHECK_FALSE(primec::emitter::runEmitterExprControlCountRewriteStep(
                  canonicalMapAccessExpr,
                  "/std/collections/map/at",
                  {},
                  {},
                  {},
                  {},
                  {},
                  [&](const primec::Expr &, std::string &) {
                    ++canonicalMapAccessResolveCalls;
                    return true;
                  })
                  .has_value());
  CHECK(canonicalMapAccessResolveCalls == 0);

  std::unordered_map<std::string, std::string> aliasOnlyMapAccessNameMap{
      {"/map/at", "ps_map_at"}};
  CHECK_FALSE(primec::emitter::runEmitterExprControlCountRewriteStep(
                  canonicalMapAccessExpr,
                  "/std/collections/map/at",
                  aliasOnlyMapAccessNameMap,
                  {},
                  {},
                  {},
                  {},
                  [&](const primec::Expr &, std::string &) { return true; })
                  .has_value());

  int labeledAccessResolveCalls = 0;
  CHECK_FALSE(primec::emitter::runEmitterExprControlCountRewriteStep(
                  accessExpr,
                  "at",
                  {},
                  {},
                  {},
                  {},
                  {},
                  [&](const primec::Expr &methodCandidate, std::string &pathOut) {
                    ++labeledAccessResolveCalls;
                    if (!methodCandidate.isMethodCall || methodCandidate.argNames.size() < 2 ||
                        !methodCandidate.argNames[0].has_value()) {
                      return false;
                    }
                    if (*methodCandidate.argNames[0] == "index") {
                      pathOut = "/i32/at";
                      return true;
                    }
                    return false;
                  })
                  .has_value());
  CHECK(labeledAccessResolveCalls == 1);

  primec::Expr positionalAccessExpr = accessExpr;
  positionalAccessExpr.argNames.clear();
  int positionalResolveCalls = 0;
  auto positionalAccessResolvedPath = primec::emitter::runEmitterExprControlCountRewriteStep(
      positionalAccessExpr,
      "at",
      {},
      {},
      {},
      {},
      {},
      [&](const primec::Expr &methodCandidate, std::string &pathOut) {
        ++positionalResolveCalls;
        if (!methodCandidate.isMethodCall || methodCandidate.args.empty()) {
          return false;
        }
        if (methodCandidate.args.front().kind == primec::Expr::Kind::Name && methodCandidate.args.front().name == "values") {
          pathOut = "/vector/at";
          return true;
        }
        return false;
      });
  REQUIRE(positionalAccessResolvedPath.has_value());
  CHECK(*positionalAccessResolvedPath == "/vector/at");
  CHECK(positionalResolveCalls == 2);

  primec::Expr positionalStringAccessExpr = positionalAccessExpr;
  positionalStringAccessExpr.args[0].kind = primec::Expr::Kind::StringLiteral;
  positionalStringAccessExpr.args[0].stringValue = "\"only\"raw_utf8";
  int positionalStringResolveCalls = 0;
  auto positionalStringAccessResolvedPath = primec::emitter::runEmitterExprControlCountRewriteStep(
      positionalStringAccessExpr,
      "at",
      {},
      {},
      {},
      {},
      {},
      [&](const primec::Expr &methodCandidate, std::string &pathOut) {
        ++positionalStringResolveCalls;
        if (!methodCandidate.isMethodCall || methodCandidate.args.empty()) {
          return false;
        }
        if (methodCandidate.args.front().kind == primec::Expr::Kind::Name && methodCandidate.args.front().name == "values") {
          pathOut = "/map/at";
          return true;
        }
        return false;
      });
  REQUIRE(positionalStringAccessResolvedPath.has_value());
  CHECK(*positionalStringAccessResolvedPath == "/map/at");
  CHECK(positionalStringResolveCalls == 2);

  primec::Expr positionalUnsafeStringAccessExpr = positionalStringAccessExpr;
  positionalUnsafeStringAccessExpr.name = "at_unsafe";
  int positionalUnsafeStringResolveCalls = 0;
  auto positionalUnsafeStringAccessResolvedPath = primec::emitter::runEmitterExprControlCountRewriteStep(
      positionalUnsafeStringAccessExpr,
      "at_unsafe",
      {},
      {},
      {},
      {},
      {},
      [&](const primec::Expr &methodCandidate, std::string &pathOut) {
        ++positionalUnsafeStringResolveCalls;
        if (!methodCandidate.isMethodCall || methodCandidate.args.empty()) {
          return false;
        }
        if (methodCandidate.args.front().kind == primec::Expr::Kind::Name && methodCandidate.args.front().name == "values") {
          pathOut = "/map/at_unsafe";
          return true;
        }
        return false;
      });
  REQUIRE(positionalUnsafeStringAccessResolvedPath.has_value());
  CHECK(*positionalUnsafeStringAccessResolvedPath == "/map/at_unsafe");
  CHECK(positionalUnsafeStringResolveCalls == 2);

  primec::Expr canonicalUnsafeStringAccessExpr = positionalStringAccessExpr;
  canonicalUnsafeStringAccessExpr.name = "/std/collections/map/at_unsafe";
  int canonicalUnsafeStringResolveCalls = 0;
  CHECK_FALSE(primec::emitter::runEmitterExprControlCountRewriteStep(
                  canonicalUnsafeStringAccessExpr,
                  "/std/collections/map/at_unsafe",
                  {},
                  {},
                  {},
                  {},
                  {},
                  [&](const primec::Expr &, std::string &) {
                    ++canonicalUnsafeStringResolveCalls;
                    return true;
                  })
                  .has_value());
  CHECK(canonicalUnsafeStringResolveCalls == 0);

  primec::Expr positionalNameAccessExpr = positionalAccessExpr;
  positionalNameAccessExpr.args[0].kind = primec::Expr::Kind::Name;
  positionalNameAccessExpr.args[0].name = "indexName";
  int positionalNameResolveCalls = 0;
  auto positionalNameAccessResolvedPath = primec::emitter::runEmitterExprControlCountRewriteStep(
      positionalNameAccessExpr,
      "at",
      {},
      {},
      {},
      {},
      {},
      [&](const primec::Expr &methodCandidate, std::string &pathOut) {
        ++positionalNameResolveCalls;
        if (!methodCandidate.isMethodCall || methodCandidate.args.empty()) {
          return false;
        }
        if (methodCandidate.args.front().kind == primec::Expr::Kind::Name && methodCandidate.args.front().name == "values") {
          pathOut = "/vector/at";
          return true;
        }
        return false;
      },
      [&](const primec::Expr &receiverExpr) {
        return receiverExpr.kind == primec::Expr::Kind::Name && receiverExpr.name == "values";
      });
  REQUIRE(positionalNameAccessResolvedPath.has_value());
  CHECK(*positionalNameAccessResolvedPath == "/vector/at");
  CHECK(positionalNameResolveCalls == 2);

  primec::Expr knownReceiverAccessExpr = positionalAccessExpr;
  knownReceiverAccessExpr.args[0].kind = primec::Expr::Kind::Name;
  knownReceiverAccessExpr.args[0].name = "values";
  knownReceiverAccessExpr.args[1].kind = primec::Expr::Kind::Name;
  knownReceiverAccessExpr.args[1].name = "indexName";
  int knownReceiverResolveCalls = 0;
  CHECK_FALSE(primec::emitter::runEmitterExprControlCountRewriteStep(
                  knownReceiverAccessExpr,
                  "at",
                  {},
                  {},
                  {},
                  {},
                  {},
                  [&](const primec::Expr &methodCandidate, std::string &pathOut) {
                    ++knownReceiverResolveCalls;
                    if (!methodCandidate.isMethodCall || methodCandidate.args.empty()) {
                      return false;
                    }
                    if (methodCandidate.args.front().kind == primec::Expr::Kind::Name &&
                        methodCandidate.args.front().name == "indexName") {
                      pathOut = "/i32/at";
                      return true;
                    }
                    return false;
                  },
                  [&](const primec::Expr &receiverExpr) {
                    return receiverExpr.kind == primec::Expr::Kind::Name && receiverExpr.name == "values";
                  })
                  .has_value());
  CHECK(knownReceiverResolveCalls == 1);

  CHECK_FALSE(primec::emitter::runEmitterExprControlCountRewriteStep(
      countExpr,
      "count",
      {},
      {},
      {},
      {},
      {},
      [&](const primec::Expr &, std::string &) { return false; }).has_value());

  CHECK_FALSE(primec::emitter::runEmitterExprControlCountRewriteStep(
      countExpr,
      "count",
      {},
      {},
      {},
      {},
      {},
      {}).has_value());

  auto resolvedPath = primec::emitter::runEmitterExprControlCountRewriteStep(
      countExpr,
      "count",
      {},
      {},
      {},
      {},
      {},
      [&](const primec::Expr &methodCandidate, std::string &pathOut) {
        CHECK(methodCandidate.isMethodCall);
        pathOut = "/Vec3/count";
        return true;
      });
  REQUIRE(resolvedPath.has_value());
  CHECK(*resolvedPath == "/Vec3/count");
}

TEST_CASE("emitter expr control builtin-block prelude step gates block handling") {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "block";
  expr.hasBodyArguments = true;
  expr.bodyArguments.push_back(primec::Expr{});

  const auto noBuiltinMatch = primec::emitter::runEmitterExprControlBuiltinBlockPreludeStep(
      expr,
      {},
      [&](const primec::Expr &, const std::unordered_map<std::string, std::string> &) { return false; },
      [&](const std::vector<std::optional<std::string>> &) { return false; });
  CHECK_FALSE(noBuiltinMatch.matched);
  CHECK_FALSE(noBuiltinMatch.earlyReturnExpr.has_value());

  const auto noPredicate = primec::emitter::runEmitterExprControlBuiltinBlockPreludeStep(
      expr,
      {},
      {},
      [&](const std::vector<std::optional<std::string>> &) { return false; });
  CHECK_FALSE(noPredicate.matched);
  CHECK_FALSE(noPredicate.earlyReturnExpr.has_value());

  primec::Expr noBodyFlag = expr;
  noBodyFlag.hasBodyArguments = false;
  const auto noBody = primec::emitter::runEmitterExprControlBuiltinBlockPreludeStep(
      noBodyFlag,
      {},
      [&](const primec::Expr &, const std::unordered_map<std::string, std::string> &) { return true; },
      [&](const std::vector<std::optional<std::string>> &) { return false; });
  CHECK_FALSE(noBody.matched);
  CHECK_FALSE(noBody.earlyReturnExpr.has_value());

  primec::Expr invalidArgs = expr;
  invalidArgs.args.push_back(primec::Expr{});
  const auto invalidArgsResult = primec::emitter::runEmitterExprControlBuiltinBlockPreludeStep(
      invalidArgs,
      {},
      [&](const primec::Expr &, const std::unordered_map<std::string, std::string> &) { return true; },
      [&](const std::vector<std::optional<std::string>> &) { return false; });
  REQUIRE(invalidArgsResult.matched);
  REQUIRE(invalidArgsResult.earlyReturnExpr.has_value());
  CHECK(*invalidArgsResult.earlyReturnExpr == "0");

  primec::Expr invalidNamedArgs = expr;
  invalidNamedArgs.argNames.push_back("value");
  const auto invalidNamedArgsResult = primec::emitter::runEmitterExprControlBuiltinBlockPreludeStep(
      invalidNamedArgs,
      {},
      [&](const primec::Expr &, const std::unordered_map<std::string, std::string> &) { return true; },
      [&](const std::vector<std::optional<std::string>> &) { return true; });
  REQUIRE(invalidNamedArgsResult.matched);
  REQUIRE(invalidNamedArgsResult.earlyReturnExpr.has_value());
  CHECK(*invalidNamedArgsResult.earlyReturnExpr == "0");

  primec::Expr emptyBody = expr;
  emptyBody.bodyArguments.clear();
  const auto emptyBodyResult = primec::emitter::runEmitterExprControlBuiltinBlockPreludeStep(
      emptyBody,
      {},
      [&](const primec::Expr &, const std::unordered_map<std::string, std::string> &) { return true; },
      [&](const std::vector<std::optional<std::string>> &) { return false; });
  REQUIRE(emptyBodyResult.matched);
  REQUIRE(emptyBodyResult.earlyReturnExpr.has_value());
  CHECK(*emptyBodyResult.earlyReturnExpr == "0");

  const auto validResult = primec::emitter::runEmitterExprControlBuiltinBlockPreludeStep(
      expr,
      {},
      [&](const primec::Expr &, const std::unordered_map<std::string, std::string> &) { return true; },
      [&](const std::vector<std::optional<std::string>> &) { return false; });
  CHECK(validResult.matched);
  CHECK_FALSE(validResult.earlyReturnExpr.has_value());
}

TEST_CASE("emitter expr control builtin-block early-return step handles non-final returns") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "value";

  const auto notReturn = primec::emitter::runEmitterExprControlBuiltinBlockEarlyReturnStep(
      stmt,
      false,
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &) { return "unused"; });
  CHECK_FALSE(notReturn.handled);
  CHECK(notReturn.emittedStatement.empty());

  const auto lastReturn = primec::emitter::runEmitterExprControlBuiltinBlockEarlyReturnStep(
      stmt,
      true,
      [&](const primec::Expr &) { return true; },
      [&](const primec::Expr &) { return "unused"; });
  CHECK_FALSE(lastReturn.handled);
  CHECK(lastReturn.emittedStatement.empty());

  const auto noPredicate = primec::emitter::runEmitterExprControlBuiltinBlockEarlyReturnStep(
      stmt,
      false,
      {},
      [&](const primec::Expr &) { return "unused"; });
  CHECK_FALSE(noPredicate.handled);
  CHECK(noPredicate.emittedStatement.empty());

  primec::Expr returnStmt = stmt;
  returnStmt.name = "return";
  returnStmt.args.push_back(primec::Expr{});

  const auto validReturn = primec::emitter::runEmitterExprControlBuiltinBlockEarlyReturnStep(
      returnStmt,
      false,
      [&](const primec::Expr &) { return true; },
      [&](const primec::Expr &) { return "emit_value"; });
  CHECK(validReturn.handled);
  CHECK(validReturn.emittedStatement == "return emit_value; ");

  const auto invalidReturn = primec::emitter::runEmitterExprControlBuiltinBlockEarlyReturnStep(
      returnStmt,
      false,
      [&](const primec::Expr &) { return true; },
      {});
  CHECK(invalidReturn.handled);
  CHECK(invalidReturn.emittedStatement == "return 0; ");

  returnStmt.args.push_back(primec::Expr{});
  const auto invalidArity = primec::emitter::runEmitterExprControlBuiltinBlockEarlyReturnStep(
      returnStmt,
      false,
      [&](const primec::Expr &) { return true; },
      [&](const primec::Expr &) { return "emit_value"; });
  CHECK(invalidArity.handled);
  CHECK(invalidArity.emittedStatement == "return 0; ");
}

TEST_SUITE_END();
