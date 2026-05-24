#include "primec/CompileTimeValue.h"

#include <optional>
#include <string>
#include <unordered_set>

#include "third_party/doctest.h"

TEST_SUITE_BEGIN("primestruct.compile_time.value");

namespace {

primec::CompileTimeValueProvenance valueProvenance() {
  primec::CompileTimeValueProvenance provenance;
  provenance.sourceText = "N > 0";
  provenance.sourcePath = "generic.prime";
  provenance.line = 9;
  provenance.column = 11;
  provenance.semanticNodeId = 101;
  provenance.provenanceHandle = 202;
  return provenance;
}

primec::CompileTimeEvaluationProvenance evaluationProvenance() {
  primec::CompileTimeEvaluationProvenance provenance;
  provenance.definitionPath = "/generic/use";
  provenance.predicatePath = "/std/meta/value_greater";
  provenance.sourceText = "N > 0";
  provenance.line = 9;
  provenance.column = 11;
  provenance.semanticNodeId = 101;
  provenance.provenanceHandle = 202;
  return provenance;
}

} // namespace

TEST_CASE("compile-time values preserve kinded equality and hashing") {
  const auto provenance = valueProvenance();
  const auto left = primec::makeCompileTimeSignedInteger(-7, provenance);
  const auto same = primec::makeCompileTimeSignedInteger(-7, provenance);
  const auto differentKind = primec::makeCompileTimeUnsignedInteger(7, provenance);
  const auto differentValue = primec::makeCompileTimeSignedInteger(8, provenance);

  CHECK(left == same);
  CHECK(left != differentKind);
  CHECK(left != differentValue);

  std::unordered_set<primec::CompileTimeValue, primec::CompileTimeValueHash> values;
  values.insert(left);
  values.insert(same);
  values.insert(differentKind);
  CHECK(values.size() == 2);
}

TEST_CASE("compile-time values format supported typed facts") {
  const auto provenance = valueProvenance();

  CHECK(primec::formatCompileTimeValue(
            primec::makeCompileTimeBool(true, provenance))
            .find("ct_value bool true") != std::string::npos);
  CHECK(primec::formatCompileTimeValue(
            primec::makeCompileTimeBool(false, provenance))
            .find("ct_value bool false") != std::string::npos);
  CHECK(primec::formatCompileTimeValue(
            primec::makeCompileTimeSignedInteger(-7, provenance))
            .find("ct_value signed_integer -7") != std::string::npos);
  CHECK(primec::formatCompileTimeValue(
            primec::makeCompileTimeUnsignedInteger(7, provenance))
            .find("ct_value unsigned_integer 7u") != std::string::npos);
  CHECK(primec::formatCompileTimeValue(
            primec::makeCompileTimeStringLiteral("hello", provenance))
            .find("ct_value string_literal \"hello\"") != std::string::npos);
  CHECK(primec::formatCompileTimeValue(
            primec::makeCompileTimeTypeFact("i32", provenance))
            .find("ct_value type_fact i32") != std::string::npos);
  CHECK(primec::formatCompileTimeValue(
            primec::makeCompileTimeSymbol("Additive", provenance))
            .find("ct_value symbol Additive") != std::string::npos);

  const std::string formatted = primec::formatCompileTimeValue(
      primec::makeCompileTimeRequirementOutcome(
          primec::CompileTimeEvaluationResultKind::UnsatisfiedPredicate,
          "trait missing",
          provenance));
  CHECK(formatted.find("ct_value requirement_outcome unsatisfied_predicate") !=
        std::string::npos);
  CHECK(formatted.find("diagnostic=\"trait missing\"") != std::string::npos);
  CHECK(formatted.find("source_text \"N > 0\"") != std::string::npos);
  CHECK(formatted.find("semantic_node_id 101") != std::string::npos);
  CHECK(formatted.find("provenance_handle 202") != std::string::npos);

  CHECK(primec::formatCompileTimeValue(
            primec::makeUnsupportedRuntimeCompileTimeValue("runtime file IO",
                                                           provenance))
            .find("ct_value unsupported_runtime_value runtime file IO") !=
        std::string::npos);
}

TEST_CASE("compile-time bool values wrap requirement outcomes") {
  const auto satisfied = primec::compileTimeRequirementResultFromBool(
      primec::makeCompileTimeBool(true, valueProvenance()),
      evaluationProvenance());
  REQUIRE(satisfied.has_value());
  CHECK(satisfied->kind == primec::CompileTimeEvaluationResultKind::Success);
  CHECK(satisfied->fault == primec::CompileTimeEvaluationFaultKind::None);
  CHECK(satisfied->boolValue);

  const auto unsatisfied = primec::compileTimeRequirementResultFromBool(
      primec::makeCompileTimeBool(false, valueProvenance()),
      evaluationProvenance());
  REQUIRE(unsatisfied.has_value());
  CHECK(unsatisfied->kind ==
        primec::CompileTimeEvaluationResultKind::UnsatisfiedPredicate);
  CHECK(unsatisfied->fault ==
        primec::CompileTimeEvaluationFaultKind::UnsatisfiedPredicate);
  CHECK_FALSE(unsatisfied->boolValue);

  CHECK_FALSE(primec::compileTimeRequirementResultFromBool(
                  primec::makeCompileTimeTypeFact("i32", valueProvenance()),
                  evaluationProvenance())
                  .has_value());
}

TEST_CASE("unsupported runtime-only values reject as invalid evaluation") {
  const auto unsupported = primec::makeUnsupportedRuntimeCompileTimeValue(
      "runtime buffer slot", valueProvenance());
  const auto rejected =
      primec::rejectUnsupportedRuntimeValue(unsupported, evaluationProvenance());

  CHECK(rejected.kind ==
        primec::CompileTimeEvaluationResultKind::InvalidEvaluation);
  CHECK(rejected.fault ==
        primec::CompileTimeEvaluationFaultKind::InvalidEvaluation);
  CHECK(rejected.message.find("runtime buffer slot") != std::string::npos);
  CHECK(rejected.provenance.provenanceHandle == 202);
}

TEST_SUITE_END();
