#include "primec/CompileTimeEvaluation.h"
#include "primec/SemanticProduct.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

primec::SemanticProgramRequirementPredicateFact makeRequirementFact(
    std::string definitionPath,
    std::string predicateName,
    std::string sourceText,
    std::string outcome,
    std::string diagnostic) {
  primec::SemanticProgramRequirementPredicateFact fact;
  fact.definitionPath = std::move(definitionPath);
  fact.predicateName = std::move(predicateName);
  fact.sourceText = std::move(sourceText);
  fact.evaluationOutcome = std::move(outcome);
  fact.evaluationDiagnostic = std::move(diagnostic);
  fact.sourceLine = 17;
  fact.sourceColumn = 9;
  fact.semanticNodeId = 77;
  fact.provenanceHandle = 88;
  return fact;
}

primec::SemanticProgramRequirementPredicateOperand makeOperand(
    std::string kind,
    std::string text) {
  primec::SemanticProgramRequirementPredicateOperand operand;
  operand.kind = std::move(kind);
  operand.text = std::move(text);
  operand.sourceLine = 17;
  operand.sourceColumn = 9;
  return operand;
}

primec::SemanticProgram makeRequirementProgram() {
  primec::SemanticProgram program;
  program.requirementPredicateFacts.push_back(makeRequirementFact(
      "/generic/same",
      "/std/meta/type_equals",
      "/std/meta/type_equals<i32, i32>()",
      "satisfied",
      "types are equal: i32"));
  program.requirementPredicateFacts.push_back(makeRequirementFact(
      "/generic/add",
      "/std/meta/has_trait",
      "/std/meta/has_trait<i32, Additive>()",
      "satisfied",
      "type supports Additive: i32"));
  program.requirementPredicateFacts.push_back(makeRequirementFact(
      "/generic/field",
      "/std/meta/has_field",
      "/std/meta/has_field</Record, secret>()",
      "unsatisfied",
      "no visible field named secret on /Record"));
  program.requirementPredicateFacts.push_back(makeRequirementFact(
      "/generic/bad",
      "/std/meta/unknown_predicate",
      "/std/meta/unknown_predicate<i32>()",
      "invalid_evaluation",
      "unknown requirement predicate: /std/meta/unknown_predicate"));
  return program;
}

primec::SemanticProgram makeBudgetProgram() {
  primec::SemanticProgram program;
  primec::SemanticProgramRequirementPredicateFact fact = makeRequirementFact(
      "/generic/user",
      "/project/is_small",
      "/project/is_small<N>()",
      "satisfied",
      "user predicate returned true");
  fact.operands.push_back(
      makeOperand("literal_compile_time_argument", "12345"));
  program.requirementPredicateFacts.push_back(std::move(fact));
  return program;
}

class AllowOneEffectHost final : public primec::CompileTimeHost {
public:
  bool allowEffect(std::string_view effectName,
                   primec::CompileTimeEffectPhase phase,
                   std::string_view definitionPath = {}) const override {
    (void)definitionPath;
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

TEST_CASE("semantic CT host gates effects on phase-qualified metadata") {
  primec::SemanticProgram program;
  program.requirementPredicateFacts.push_back(makeRequirementFact(
      "/generic/runtime",
      "/project/effectful",
      "/project/effectful()",
      "satisfied",
      "runtime effect metadata only"));

  auto compileTimeFact = makeRequirementFact("/generic/compiletime",
                                             "/project/effectful",
                                             "/project/effectful()",
                                             "satisfied",
                                             "compile-time effect allowed");
  compileTimeFact.compileTimeEffects.push_back("file_read");
  program.requirementPredicateFacts.push_back(std::move(compileTimeFact));

  const primec::SemanticProgramCompileTimeHost host(program);
  const primec::CompileTimeEvaluationFacade facade(host);

  auto provenance = sampleProvenance();
  provenance.definitionPath = "/generic/runtime";
  const auto runtimeOnly = facade.requireEffect("file_read", provenance);
  CHECK(runtimeOnly.kind ==
        primec::CompileTimeEvaluationResultKind::DeniedEffect);

  provenance.definitionPath = "/generic/missing";
  const auto wrongDefinition = facade.requireEffect("file_read", provenance);
  CHECK(wrongDefinition.kind ==
        primec::CompileTimeEvaluationResultKind::DeniedEffect);

  provenance.definitionPath = "/generic/compiletime";
  const auto allowed = facade.requireEffect("file_read", provenance);
  CHECK(allowed.kind == primec::CompileTimeEvaluationResultKind::Success);
  CHECK(allowed.boolValue);
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

TEST_CASE("compile-time evaluation facade enforces deterministic budgets") {
  const primec::SemanticProgram program = makeBudgetProgram();
  const primec::SemanticProgramCompileTimeHost host(program);
  const primec::CompileTimeEvaluationFacade facade(host);

  auto makeRequest = [] {
    primec::CompileTimeEvaluationRequest request;
    request.definitionPath = "/generic/user";
    request.predicateName = "/project/is_small";
    return request;
  };

  CHECK(facade.evaluateRequirementPredicate(makeRequest()).kind ==
        primec::CompileTimeEvaluationResultKind::Success);

  auto stepRequest = makeRequest();
  stepRequest.budget.maxSteps = 1;
  const auto stepResult = facade.evaluateRequirementPredicate(stepRequest);
  CHECK(stepResult.kind ==
        primec::CompileTimeEvaluationResultKind::BudgetExhausted);
  CHECK(stepResult.message.find("evaluator step budget exceeded") !=
        std::string::npos);

  auto userRequest = makeRequest();
  userRequest.budget.maxUserPredicateCalls = 0;
  const auto userResult = facade.evaluateRequirementPredicate(userRequest);
  CHECK(userResult.kind ==
        primec::CompileTimeEvaluationResultKind::BudgetExhausted);
  CHECK(userResult.message.find("user predicate call budget exceeded") !=
        std::string::npos);

  auto valueRequest = makeRequest();
  valueRequest.budget.maxValueBytes = 4;
  const auto valueResult = facade.evaluateRequirementPredicate(valueRequest);
  CHECK(valueResult.kind ==
        primec::CompileTimeEvaluationResultKind::BudgetExhausted);
  CHECK(valueResult.message.find("value storage budget exceeded") !=
        std::string::npos);

  auto storageRequest = makeRequest();
  storageRequest.budget.maxStorageBytes = 4;
  const auto storageResult =
      facade.evaluateRequirementPredicate(storageRequest);
  CHECK(storageResult.kind ==
        primec::CompileTimeEvaluationResultKind::BudgetExhausted);
  CHECK(storageResult.message.find("storage byte budget exceeded") !=
        std::string::npos);

  auto hostRequest = makeRequest();
  hostRequest.budget.maxHostBytes = 4;
  const auto hostResult = facade.evaluateRequirementPredicate(hostRequest);
  CHECK(hostResult.kind ==
        primec::CompileTimeEvaluationResultKind::BudgetExhausted);
  CHECK(hostResult.message.find("host byte budget exceeded") !=
        std::string::npos);

  auto diagnosticRequest = makeRequest();
  diagnosticRequest.budget.maxDiagnosticBytes = 4;
  const auto diagnosticResult =
      facade.evaluateRequirementPredicate(diagnosticRequest);
  CHECK(diagnosticResult.kind ==
        primec::CompileTimeEvaluationResultKind::BudgetExhausted);
  CHECK(diagnosticResult.message.find("diagnostic payload budget exceeded") !=
        std::string::npos);

  auto provenanceRequest = makeRequest();
  provenanceRequest.budget.maxProvenanceBytes = 4;
  const auto provenanceResult =
      facade.evaluateRequirementPredicate(provenanceRequest);
  CHECK(provenanceResult.kind ==
        primec::CompileTimeEvaluationResultKind::BudgetExhausted);
  CHECK(provenanceResult.message.find("provenance payload budget exceeded") !=
        std::string::npos);
}

TEST_CASE("semantic CT host answers canonical builtin meta predicate facts") {
  const primec::SemanticProgram program = makeRequirementProgram();
  const primec::SemanticProgramCompileTimeHost host(program);
  const primec::CompileTimeEvaluationFacade facade(host);

  CHECK(host.describeSemanticFact("requirement_predicate_count") ==
        std::optional<std::string>("4"));
  CHECK(host.describeSemanticFact(
            "requirement_predicate:/std/meta/has_trait") ==
        std::optional<std::string>("1"));

  primec::CompileTimeEvaluationRequest typeRequest;
  typeRequest.definitionPath = "/generic/same";
  typeRequest.predicateName = "/std/meta/type_equals";
  typeRequest.sourceText = "/std/meta/type_equals<i32, i32>()";
  const auto typeResult = facade.evaluateRequirementPredicate(typeRequest);
  CHECK(typeResult.kind == primec::CompileTimeEvaluationResultKind::Success);
  CHECK(typeResult.boolValue);
  CHECK(typeResult.provenance.predicatePath == "/std/meta/type_equals");
  CHECK(typeResult.message == "types are equal: i32");

  primec::CompileTimeEvaluationRequest traitRequest;
  traitRequest.definitionPath = "/generic/add";
  traitRequest.predicateName = "/std/meta/has_trait";
  const auto traitResult = facade.evaluateRequirementPredicate(traitRequest);
  CHECK(traitResult.kind == primec::CompileTimeEvaluationResultKind::Success);
  CHECK(traitResult.message.find("Additive") != std::string::npos);
}

TEST_CASE("semantic CT host preserves visibility and invalid meta diagnostics") {
  const primec::SemanticProgram program = makeRequirementProgram();
  const primec::SemanticProgramCompileTimeHost host(program);
  const primec::CompileTimeEvaluationFacade facade(host);

  primec::CompileTimeEvaluationRequest fieldRequest;
  fieldRequest.definitionPath = "/generic/field";
  fieldRequest.predicateName = "/std/meta/has_field";
  const auto fieldResult = facade.evaluateRequirementPredicate(fieldRequest);
  CHECK(fieldResult.kind ==
        primec::CompileTimeEvaluationResultKind::UnsatisfiedPredicate);
  CHECK_FALSE(fieldResult.boolValue);
  CHECK(fieldResult.message.find("no visible field named secret") !=
        std::string::npos);

  primec::CompileTimeEvaluationRequest invalidRequest;
  invalidRequest.definitionPath = "/generic/bad";
  invalidRequest.predicateName = "/std/meta/unknown_predicate";
  const auto invalidResult = facade.evaluateRequirementPredicate(invalidRequest);
  CHECK(invalidResult.kind ==
        primec::CompileTimeEvaluationResultKind::InvalidEvaluation);
  CHECK(invalidResult.message.find("unknown requirement predicate") !=
        std::string::npos);

  primec::CompileTimeEvaluationRequest missingRequest;
  missingRequest.definitionPath = "/generic/missing";
  missingRequest.predicateName = "/std/meta/type_equals";
  missingRequest.provenance = sampleProvenance();
  const auto missingResult = facade.evaluateRequirementPredicate(missingRequest);
  CHECK(missingResult.kind ==
        primec::CompileTimeEvaluationResultKind::InternalCompilerError);
  CHECK(missingResult.message.find(
            "requires a published predicate fact for /std/meta/type_equals") !=
        std::string::npos);
}

TEST_SUITE_END();
