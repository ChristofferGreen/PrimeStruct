#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("emitter expr control if branch emit step composes value and handlers") {
  primec::Expr nonEnvelope;
  nonEnvelope.kind = primec::Expr::Kind::Name;
  nonEnvelope.name = "direct";

  std::unordered_map<std::string, primec::Emitter::BindingInfo> branchTypes;
  std::unordered_map<std::string, primec::Emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;

  int directEmitCalls = 0;
  const auto directResult = primec::emitter::runEmitterExprControlIfBranchEmitStep(
      nonEnvelope,
      branchTypes,
      returnKinds,
      false,
      importAliases,
      structTypeMap,
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &) {
        CHECK_FALSE(true);
        return false;
      },
      [&](const primec::Expr &) {
        CHECK_FALSE(true);
        return primec::Emitter::BindingInfo{};
      },
      [&](const primec::Expr &) {
        CHECK_FALSE(true);
        return false;
      },
      [&](const primec::Expr &, const auto &, const auto &, bool) {
        CHECK_FALSE(true);
        return primec::Emitter::ReturnKind::Unknown;
      },
      [&](primec::Emitter::ReturnKind) {
        CHECK_FALSE(true);
        return std::string{};
      },
      [&](const primec::Emitter::BindingInfo &) {
        CHECK_FALSE(true);
        return false;
      },
      [&](const primec::Expr &candidate) {
        ++directEmitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Name);
        CHECK(candidate.name == "direct");
        return std::string("emit_direct");
      });
  CHECK(directResult.handled);
  CHECK(directResult.emittedExpr == "emit_direct");
  CHECK(directEmitCalls == 1);

  primec::Expr returnArg;
  returnArg.kind = primec::Expr::Kind::Name;
  returnArg.name = "value";

  primec::Expr returnStmt;
  returnStmt.kind = primec::Expr::Kind::Call;
  returnStmt.name = "return";
  returnStmt.args = {returnArg};

  primec::Expr envelope;
  envelope.kind = primec::Expr::Kind::Call;
  envelope.bodyArguments = {returnStmt};

  int returnValueEmitCalls = 0;
  const auto wrapped = primec::emitter::runEmitterExprControlIfBranchEmitStep(
      envelope,
      branchTypes,
      returnKinds,
      false,
      importAliases,
      structTypeMap,
      [&](const primec::Expr &) { return true; },
      [&](const primec::Expr &candidate) {
        return candidate.kind == primec::Expr::Kind::Call && candidate.name == "return";
      },
      [&](const primec::Expr &) {
        CHECK_FALSE(true);
        return primec::Emitter::BindingInfo{};
      },
      [&](const primec::Expr &) {
        CHECK_FALSE(true);
        return false;
      },
      [&](const primec::Expr &, const auto &, const auto &, bool) {
        CHECK_FALSE(true);
        return primec::Emitter::ReturnKind::Unknown;
      },
      [&](primec::Emitter::ReturnKind) {
        CHECK_FALSE(true);
        return std::string{};
      },
      [&](const primec::Emitter::BindingInfo &) {
        CHECK_FALSE(true);
        return false;
      },
      [&](const primec::Expr &candidate) {
        ++returnValueEmitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Name);
        CHECK(candidate.name == "value");
        return std::string("emit_value");
      });
  CHECK(wrapped.handled);
  CHECK(wrapped.emittedExpr == "([&]() { return emit_value; }())");
  CHECK(returnValueEmitCalls == 1);

  const auto missingEmit = primec::emitter::runEmitterExprControlIfBranchEmitStep(
      nonEnvelope,
      branchTypes,
      returnKinds,
      false,
      importAliases,
      structTypeMap,
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &) { return primec::Emitter::BindingInfo{}; },
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &, const auto &, const auto &, bool) {
        return primec::Emitter::ReturnKind::Unknown;
      },
      [&](primec::Emitter::ReturnKind) { return std::string{}; },
      [&](const primec::Emitter::BindingInfo &) { return false; },
      {});
  CHECK_FALSE(missingEmit.handled);
  CHECK(missingEmit.emittedExpr.empty());
}

TEST_CASE("emitter expr control if branch body statement step dispatches statements") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Name;
  stmt.name = "value";

  int emitCalls = 0;
  const auto emitted = primec::emitter::runEmitterExprControlIfBranchBodyStatementStep(
      stmt,
      [&](const primec::Expr &candidate) {
        ++emitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Name);
        CHECK(candidate.name == "value");
        return std::string("emit_value");
      });
  CHECK(emitted.handled);
  CHECK(emitted.emitted.handled);
  CHECK(emitCalls == 1);
  CHECK(emitted.emitted.emittedStatement == "(void)emit_value; ");
  CHECK_FALSE(emitted.emitted.shouldBreak);

  const auto missingEmit = primec::emitter::runEmitterExprControlIfBranchBodyStatementStep(
      stmt,
      {});
  CHECK_FALSE(missingEmit.handled);
  CHECK_FALSE(missingEmit.emitted.handled);
  CHECK(missingEmit.emitted.emittedStatement.empty());
  CHECK_FALSE(missingEmit.emitted.shouldBreak);
}

TEST_CASE("emitter expr control if-block statement step emits void statements") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Name;
  stmt.name = "value";

  int emitCalls = 0;
  const auto emitted = primec::emitter::runEmitterExprControlIfBlockStatementStep(
      stmt,
      [&](const primec::Expr &candidate) {
        ++emitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Name);
        CHECK(candidate.name == "value");
        return std::string("emit_value");
      });
  CHECK(emitted.handled);
  CHECK(emitCalls == 1);
  CHECK(emitted.emittedStatement == "(void)emit_value; ");

  const auto missingEmit = primec::emitter::runEmitterExprControlIfBlockStatementStep(
      stmt,
      {});
  CHECK_FALSE(missingEmit.handled);
  CHECK(missingEmit.emittedStatement.empty());
}

TEST_CASE("emitter expr control if ternary step emits conditional expression") {
  int conditionCalls = 0;
  int thenCalls = 0;
  int elseCalls = 0;
  const auto emitted = primec::emitter::runEmitterExprControlIfTernaryStep(
      [&]() {
        ++conditionCalls;
        return std::string("cond_expr");
      },
      [&]() {
        ++thenCalls;
        return std::string("then_expr");
      },
      [&]() {
        ++elseCalls;
        return std::string("else_expr");
      });
  CHECK(emitted.handled);
  CHECK(conditionCalls == 1);
  CHECK(thenCalls == 1);
  CHECK(elseCalls == 1);
  CHECK(emitted.emittedExpr == "(cond_expr ? then_expr : else_expr)");

  const auto missingCondition =
      primec::emitter::runEmitterExprControlIfTernaryStep({}, [] { return std::string("then"); }, [] {
        return std::string("else");
      });
  CHECK_FALSE(missingCondition.handled);
  CHECK(missingCondition.emittedExpr.empty());

  const auto missingThen =
      primec::emitter::runEmitterExprControlIfTernaryStep([] { return std::string("cond"); }, {}, [] {
        return std::string("else");
      });
  CHECK_FALSE(missingThen.handled);
  CHECK(missingThen.emittedExpr.empty());

  const auto missingElse =
      primec::emitter::runEmitterExprControlIfTernaryStep([] { return std::string("cond"); }, [] {
        return std::string("then");
      }, {});
  CHECK_FALSE(missingElse.handled);
  CHECK(missingElse.emittedExpr.empty());
}

TEST_CASE("semantics validator expr capture split step tokenizes captures") {
  CHECK(primec::semantics::probeExprCaptureSplitForTesting("").tokens.empty());
  CHECK(primec::semantics::probeExprCaptureSplitForTesting(" \t \n").tokens.empty());

  const auto single = primec::semantics::probeExprCaptureSplitForTesting("value");
  REQUIRE(single.tokens.size() == 1);
  CHECK(single.tokens[0] == "value");

  const auto pair = primec::semantics::probeExprCaptureSplitForTesting("ref   item");
  REQUIRE(pair.tokens.size() == 2);
  CHECK(pair.tokens[0] == "ref");
  CHECK(pair.tokens[1] == "item");

  const auto mixed = primec::semantics::probeExprCaptureSplitForTesting("  =   ref\tname  ");
  REQUIRE(mixed.tokens.size() == 3);
  CHECK(mixed.tokens[0] == "=");
  CHECK(mixed.tokens[1] == "ref");
  CHECK(mixed.tokens[2] == "name");
}

TEST_CASE("semantics validator statement loop-count step resolves iteration bounds") {
  primec::Expr nameExpr;
  nameExpr.kind = primec::Expr::Kind::Name;
  nameExpr.name = "count";
  const auto nameProbe = primec::semantics::probeLoopCountForTesting(nameExpr, false);
  CHECK_FALSE(nameProbe.knownIterationCount.has_value());

  primec::Expr boolTrue;
  boolTrue.kind = primec::Expr::Kind::BoolLiteral;
  boolTrue.boolValue = true;
  const auto boolTrueNoBoolProbe = primec::semantics::probeLoopCountForTesting(boolTrue, false);
  CHECK_FALSE(boolTrueNoBoolProbe.knownIterationCount.has_value());
  const auto boolTrueProbe = primec::semantics::probeLoopCountForTesting(boolTrue, true);
  REQUIRE(boolTrueProbe.knownIterationCount.has_value());
  CHECK(*boolTrueProbe.knownIterationCount == 1u);

  primec::Expr boolFalse;
  boolFalse.kind = primec::Expr::Kind::BoolLiteral;
  boolFalse.boolValue = false;
  const auto boolFalseProbe = primec::semantics::probeLoopCountForTesting(boolFalse, true);
  REQUIRE(boolFalseProbe.knownIterationCount.has_value());
  CHECK(*boolFalseProbe.knownIterationCount == 0u);

  primec::Expr unsignedLiteral;
  unsignedLiteral.kind = primec::Expr::Kind::Literal;
  unsignedLiteral.isUnsigned = true;
  unsignedLiteral.literalValue = 7;
  const auto unsignedProbe = primec::semantics::probeLoopCountForTesting(unsignedLiteral, false);
  REQUIRE(unsignedProbe.knownIterationCount.has_value());
  CHECK(*unsignedProbe.knownIterationCount == 7u);

  primec::Expr negativeLiteral;
  negativeLiteral.kind = primec::Expr::Kind::Literal;
  negativeLiteral.isUnsigned = false;
  negativeLiteral.intWidth = 32;
  negativeLiteral.literalValue = static_cast<uint64_t>(static_cast<int32_t>(-1));
  const auto negativeProbe = primec::semantics::probeLoopCountForTesting(negativeLiteral, false);
  CHECK(negativeProbe.isNegativeIntegerLiteral);
  REQUIRE(negativeProbe.knownIterationCount.has_value());
  CHECK(*negativeProbe.knownIterationCount == 0u);

  primec::Expr positiveLiteral = negativeLiteral;
  positiveLiteral.literalValue = 1;
  CHECK_FALSE(primec::semantics::probeLoopCountForTesting(positiveLiteral, false).isNegativeIntegerLiteral);

  primec::Expr unsignedLiteralForNegative = unsignedLiteral;
  CHECK_FALSE(primec::semantics::probeLoopCountForTesting(unsignedLiteralForNegative, false).isNegativeIntegerLiteral);

  primec::Expr oneLiteral;
  oneLiteral.kind = primec::Expr::Kind::Literal;
  oneLiteral.isUnsigned = false;
  oneLiteral.intWidth = 32;
  oneLiteral.literalValue = 1;
  CHECK_FALSE(primec::semantics::probeLoopCountForTesting(oneLiteral, false).canIterateMoreThanOnce);

  primec::Expr twoLiteral = oneLiteral;
  twoLiteral.literalValue = 2;
  CHECK(primec::semantics::probeLoopCountForTesting(twoLiteral, false).canIterateMoreThanOnce);

  CHECK(nameProbe.canIterateMoreThanOnce);
  CHECK_FALSE(boolTrueProbe.canIterateMoreThanOnce);
  CHECK_FALSE(boolFalseProbe.canIterateMoreThanOnce);
}

TEST_SUITE_END();
