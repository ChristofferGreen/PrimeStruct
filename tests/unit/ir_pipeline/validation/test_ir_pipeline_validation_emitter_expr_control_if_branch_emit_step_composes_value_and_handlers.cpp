#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

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
