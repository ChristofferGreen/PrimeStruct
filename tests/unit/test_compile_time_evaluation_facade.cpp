#include "primec/CompileTimeEvaluation.h"
#include "primec/SemanticProduct.h"

#include <optional>
#include <string>
#include <string_view>

#include "third_party/doctest.h"

TEST_SUITE_BEGIN("primestruct.compile_time.facade");

namespace {

primec::CompileTimeEvaluationProvenance sampleProvenance() {
  primec::CompileTimeEvaluationProvenance provenance;
  provenance.definitionPath = "/example/use";
  provenance.predicatePath = "/std/meta/has_trait";
  provenance.sourcePath = "example.prime";
  provenance.line = 12;
  provenance.column = 5;
  return provenance;
}

class AllowOneEffectHost final : public primec::CompileTimeHost {
public:
  bool allowEffect(std::string_view effectName,
                   primec::CompileTimeEffectPhase phase) const override {
    return phase == primec::CompileTimeEffectPhase::SemanticRequirement &&
           effectName == "meta_lookup";
  }

  std::optional<std::string>
  describeSemanticFact(std::string_view factName) const override {
    if (factName == "trait:Additive") {
      return std::string("trait Additive is available");
    }
    return std::nullopt;
  }
};

} // namespace

TEST_CASE("compile-time evaluation facade names every result and fault") {
  CHECK(primec::compileTimeEvaluationResultKindName(
            primec::CompileTimeEvaluationResultKind::Success) == "success");
  CHECK(primec::compileTimeEvaluationResultKindName(
            primec::CompileTimeEvaluationResultKind::UnsatisfiedPredicate) ==
        "unsatisfied_predicate");
  CHECK(primec::compileTimeEvaluationResultKindName(
            primec::CompileTimeEvaluationResultKind::InvalidEvaluation) ==
        "invalid_evaluation");
  CHECK(primec::compileTimeEvaluationResultKindName(
            primec::CompileTimeEvaluationResultKind::DeniedEffect) ==
        "denied_effect");
  CHECK(primec::compileTimeEvaluationResultKindName(
            primec::CompileTimeEvaluationResultKind::BudgetExhausted) ==
        "budget_exhausted");
  CHECK(primec::compileTimeEvaluationResultKindName(
            primec::CompileTimeEvaluationResultKind::InternalCompilerError) ==
        "internal_compiler_error");

  CHECK(primec::compileTimeEvaluationFaultKindName(
            primec::CompileTimeEvaluationFaultKind::None) == "none");
  CHECK(primec::compileTimeEvaluationFaultKindName(
            primec::CompileTimeEvaluationFaultKind::UnsatisfiedPredicate) ==
        "unsatisfied_predicate");
  CHECK(primec::compileTimeEvaluationFaultKindName(
            primec::CompileTimeEvaluationFaultKind::InvalidEvaluation) ==
        "invalid_evaluation");
  CHECK(primec::compileTimeEvaluationFaultKindName(
            primec::CompileTimeEvaluationFaultKind::DeniedEffect) ==
        "denied_effect");
  CHECK(primec::compileTimeEvaluationFaultKindName(
            primec::CompileTimeEvaluationFaultKind::BudgetExhausted) ==
        "budget_exhausted");
  CHECK(primec::compileTimeEvaluationFaultKindName(
            primec::CompileTimeEvaluationFaultKind::InternalCompilerError) ==
        "internal_compiler_error");
}

TEST_CASE("compile-time evaluation facade formats provenance and results") {
  primec::DenyAllCompileTimeHost host;
  const primec::CompileTimeEvaluationFacade facade(host);

  const auto unsatisfied =
      facade.unsatisfiedPredicate(sampleProvenance(), "trait not available");
  CHECK(unsatisfied.kind ==
        primec::CompileTimeEvaluationResultKind::UnsatisfiedPredicate);
  CHECK(primec::isCompileTimeEvaluationFault(unsatisfied.kind));

  const std::string formatted =
      primec::formatCompileTimeEvaluationResult(unsatisfied);
  CHECK(formatted.find("compile-time evaluation unsatisfied_predicate") !=
        std::string::npos);
  CHECK(formatted.find("fault=unsatisfied_predicate") != std::string::npos);
  CHECK(formatted.find("phase semantic_requirement_evaluation") !=
        std::string::npos);
  CHECK(formatted.find("definition /example/use") != std::string::npos);
  CHECK(formatted.find("predicate /std/meta/has_trait") != std::string::npos);
  CHECK(formatted.find("source example.prime:12:5") != std::string::npos);
  CHECK(formatted.find("trait not available") != std::string::npos);
}

TEST_CASE("compile-time evaluation facade gates effects through the CT host") {
  primec::DenyAllCompileTimeHost denyHost;
  primec::CompileTimeEvaluationFacade deniedFacade(denyHost);
  const auto denied =
      deniedFacade.requireEffect("file_read", sampleProvenance());
  CHECK(denied.kind == primec::CompileTimeEvaluationResultKind::DeniedEffect);
  CHECK(primec::formatCompileTimeEvaluationResult(denied).find(
            "denied compile-time effect: file_read") != std::string::npos);

  AllowOneEffectHost allowHost;
  primec::CompileTimeEvaluationFacade allowedFacade(
      allowHost,
      primec::CompileTimeEvaluationBudget{.maxSteps = 5,
                                          .maxFrames = 2,
                                          .maxUserPredicateCalls = 1});
  const auto allowed =
      allowedFacade.requireEffect("meta_lookup", sampleProvenance());
  CHECK(allowed.kind == primec::CompileTimeEvaluationResultKind::Success);
  CHECK_FALSE(primec::isCompileTimeEvaluationFault(allowed.kind));
  CHECK(allowed.boolValue);
  CHECK(allowedFacade.budget().maxSteps == 5);
  CHECK(allowHost.describeSemanticFact("trait:Additive") ==
        std::optional<std::string>("trait Additive is available"));
}

TEST_CASE("compile-time evaluation facade reports stable non-success categories") {
  primec::DenyAllCompileTimeHost host;
  const primec::CompileTimeEvaluationFacade facade(host);

  CHECK(facade.invalidEvaluation(sampleProvenance(), "unsupported runtime value").kind ==
        primec::CompileTimeEvaluationResultKind::InvalidEvaluation);
  CHECK(facade.budgetExhausted(sampleProvenance(), "step budget exhausted").kind ==
        primec::CompileTimeEvaluationResultKind::BudgetExhausted);
  CHECK(facade.internalCompilerError(sampleProvenance(), "missing semantic product").kind ==
        primec::CompileTimeEvaluationResultKind::InternalCompilerError);
}

TEST_CASE("compile-time evaluation facade wraps published requirement facts") {
  primec::SemanticProgramRequirementPredicateFact fact;
  fact.definitionPath = "/example/use";
  fact.predicateName = "/std/meta/has_trait";
  fact.sourceText = "require(meta.has_trait<T>(Additive))";
  fact.evaluationOutcome = "unsatisfied";
  fact.evaluationDiagnostic = "trait Additive is absent";
  fact.sourceLine = 7;
  fact.sourceColumn = 3;
  fact.semanticNodeId = 42;
  fact.provenanceHandle = 99;

  primec::DenyAllCompileTimeHost host;
  primec::CompileTimeEvaluationFacade facade(host);
  CHECK_FALSE(primec::CompileTimeEvaluationFacade::finalBackendIrAvailable());
  CHECK_FALSE(primec::CompileTimeEvaluationFacade::launchesRuntimeVm());

  primec::CompileTimeEvaluationRequest request;
  request.requirementPredicate = &fact;
  const auto result = facade.evaluateRequirementPredicate(request);
  CHECK(result.kind ==
        primec::CompileTimeEvaluationResultKind::UnsatisfiedPredicate);
  CHECK(result.provenance.semanticNodeId == 42);
  CHECK(result.provenance.provenanceHandle == 99);

  const std::string formatted =
      primec::formatCompileTimeEvaluationResult(result);
  CHECK(formatted.find("source_text \"require(meta.has_trait<T>(Additive))\"") !=
        std::string::npos);
  CHECK(formatted.find("semantic_node_id 42") != std::string::npos);
  CHECK(formatted.find("provenance_handle 99") != std::string::npos);
  CHECK(formatted.find("trait Additive is absent") != std::string::npos);
}

TEST_SUITE_END();
