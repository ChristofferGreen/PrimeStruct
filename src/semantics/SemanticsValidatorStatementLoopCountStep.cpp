#include "SemanticsValidatorStatementLoopCountStep.h"

#include "primec/testing/SemanticsControlFlowProbes.h"

namespace primec::semantics {

std::optional<uint64_t> runSemanticsValidatorStatementKnownIterationCountStep(const Expr &countExpr,
                                                                               bool allowBoolCount) {
  if (allowBoolCount && countExpr.kind == Expr::Kind::BoolLiteral) {
    return countExpr.boolValue ? 1u : 0u;
  }
  if (countExpr.kind != Expr::Kind::Literal) {
    return std::nullopt;
  }
  if (countExpr.isUnsigned) {
    return countExpr.literalValue;
  }
  const int64_t signedCount = countExpr.intWidth == 32 ? static_cast<int32_t>(countExpr.literalValue)
                                                       : static_cast<int64_t>(countExpr.literalValue);
  if (signedCount <= 0) {
    return 0u;
  }
  return static_cast<uint64_t>(signedCount);
}

bool runSemanticsValidatorStatementCanIterateMoreThanOnceStep(const Expr &countExpr, bool allowBoolCount) {
  const std::optional<uint64_t> knownCount =
      runSemanticsValidatorStatementKnownIterationCountStep(countExpr, allowBoolCount);
  if (!knownCount.has_value()) {
    return true;
  }
  return *knownCount > 1u;
}

bool runSemanticsValidatorStatementIsNegativeIntegerLiteralStep(const Expr &expr) {
  if (expr.kind != Expr::Kind::Literal || expr.isUnsigned) {
    return false;
  }
  if (expr.intWidth == 32) {
    return static_cast<int32_t>(expr.literalValue) < 0;
  }
  return static_cast<int64_t>(expr.literalValue) < 0;
}

LoopCountProbeSnapshotForTesting probeLoopCountForTesting(const Expr &countExpr, bool allowBoolCount) {
  return LoopCountProbeSnapshotForTesting{
      .knownIterationCount = runSemanticsValidatorStatementKnownIterationCountStep(countExpr, allowBoolCount),
      .canIterateMoreThanOnce = runSemanticsValidatorStatementCanIterateMoreThanOnceStep(countExpr, allowBoolCount),
      .isNegativeIntegerLiteral = runSemanticsValidatorStatementIsNegativeIntegerLiteralStep(countExpr),
  };
}

} // namespace primec::semantics
