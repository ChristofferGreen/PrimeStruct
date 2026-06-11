#include "primec/CompileTimeCallable.h"
#include "primec/SemanticProduct.h"

#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "third_party/doctest.h"

TEST_SUITE_BEGIN("primestruct.compile_time.callable");

namespace {

primec::SemanticProgramRequirementPredicateOperand makeOperand(
    std::string kind,
    std::string text,
    int line = 17,
    int column = 9) {
  primec::SemanticProgramRequirementPredicateOperand operand;
  operand.kind = std::move(kind);
  operand.text = std::move(text);
  operand.stableHandle = operand.kind + ":" + operand.text;
  operand.sourceLine = line;
  operand.sourceColumn = column;
  return operand;
}

primec::SemanticProgramRequirementPredicateFact makeRequirementFact(
    std::string definitionPath,
    std::string predicateName,
    std::string sourceText,
    std::vector<primec::SemanticProgramRequirementPredicateOperand> operands,
    std::string outcome = "satisfied",
    std::string diagnostic = "prepared") {
  primec::SemanticProgramRequirementPredicateFact fact;
  fact.definitionPath = std::move(definitionPath);
  fact.predicateKind = "predicate_call";
  fact.predicateName = std::move(predicateName);
  fact.sourceText = std::move(sourceText);
  fact.operands = std::move(operands);
  fact.evaluationOutcome = std::move(outcome);
  fact.evaluationDiagnostic = std::move(diagnostic);
  fact.sourceLine = 23;
  fact.sourceColumn = 5;
  fact.semanticNodeId = 77;
  fact.provenanceHandle = 88;
  return fact;
}

primec::SemanticProgram makeProgramWith(
    primec::SemanticProgramRequirementPredicateFact fact) {
  primec::SemanticProgram program;
  fact.definitionPathId =
      primec::semanticProgramInternCallTargetString(program, fact.definitionPath);
  fact.predicateKindId =
      primec::semanticProgramInternCallTargetString(program, fact.predicateKind);
  fact.predicateNameId =
      primec::semanticProgramInternCallTargetString(program, fact.predicateName);
  fact.relationOperatorId =
      primec::semanticProgramInternCallTargetString(program, fact.relationOperator);
  fact.sourceTextId =
      primec::semanticProgramInternCallTargetString(program, fact.sourceText);
  fact.compileTimeEffectIds.reserve(fact.compileTimeEffects.size());
  for (const auto &effect : fact.compileTimeEffects) {
    fact.compileTimeEffectIds.push_back(
        primec::semanticProgramInternCallTargetString(program, effect));
  }
  fact.evaluationOutcomeId =
      primec::semanticProgramInternCallTargetString(program, fact.evaluationOutcome);
  fact.evaluationDiagnosticId =
      primec::semanticProgramInternCallTargetString(program, fact.evaluationDiagnostic);
  for (auto &operand : fact.operands) {
    operand.kindId =
        primec::semanticProgramInternCallTargetString(program, operand.kind);
    operand.textId =
        primec::semanticProgramInternCallTargetString(program, operand.text);
    operand.stableHandleId =
        primec::semanticProgramInternCallTargetString(program, operand.stableHandle);
  }
  program.requirementPredicateFacts.push_back(std::move(fact));
  return program;
}

primec::CompileTimeCallablePrepareRequest makeRequest(
    std::string definitionPath,
    std::string predicateName,
    std::string sourceText) {
  primec::CompileTimeCallablePrepareRequest request;
  request.definitionPath = std::move(definitionPath);
  request.predicateName = std::move(predicateName);
  request.sourceText = std::move(sourceText);
  request.provenance.definitionPath = request.definitionPath;
  request.provenance.predicatePath = request.predicateName;
  request.provenance.sourceText = request.sourceText;
  request.provenance.line = 1;
  request.provenance.column = 1;
  return request;
}

} // namespace

TEST_CASE("builtin meta predicate facts prepare restricted callables") {
  primec::SemanticProgram program = makeProgramWith(makeRequirementFact(
      "/generic/same",
      "/std/meta/type_equals",
      "/std/meta/type_equals<i32, i32>()",
      {makeOperand("type_fact", "i32"), makeOperand("type_fact", "i32")}));
  const primec::SemanticProgramCompileTimeHost host(program);

  const primec::CompileTimeCallablePrepareResult result =
      primec::prepareCompileTimeCallable(
          host,
          makeRequest("/generic/same",
                      "/std/meta/type_equals",
                      "/std/meta/type_equals<i32, i32>()"));

  REQUIRE(result.prepared());
  CHECK(result.status == primec::CompileTimeCallablePrepareStatus::Prepared);
  CHECK(primec::compileTimeCallablePrepareStatusName(result.status) ==
        "prepared");
  CHECK(result.callable.definitionPath == "/generic/same");
  CHECK(result.callable.predicateName == "/std/meta/type_equals");
  CHECK(result.callable.sourceText == "/std/meta/type_equals<i32, i32>()");
  CHECK(result.callable.provenance.line == 23);
  CHECK(result.callable.provenance.column == 5);
  CHECK(result.callable.provenance.semanticNodeId == 77);
  CHECK(result.callable.provenance.provenanceHandle == 88);
  CHECK(result.callable.maxSteps == 10000);
  CHECK(result.callable.maxFrames == 128);
  REQUIRE(result.callable.operands.size() == 2);
  CHECK(result.callable.operands[0].kind == "type_fact");
  CHECK(result.callable.operands[0].sourceText == "i32");
  CHECK(result.callable.operands[0].value.kind ==
        primec::CompileTimeValueKind::TypeFact);
  CHECK(std::get<std::string>(result.callable.operands[0].value.payload) ==
        "i32");
  CHECK(result.callable.operands[0].value.provenance.line == 17);
  CHECK(result.diagnostic.kind ==
        primec::CompileTimeEvaluationResultKind::Success);
  CHECK(result.diagnostic.boolValue);

  CHECK(!primec::CompileTimePreparedCallable::requiresFinalBackendIr());
  CHECK(!primec::CompileTimePreparedCallable::launchesRuntimeVm());
}

TEST_CASE("runtime-only operands fail before compile-time execution") {
  primec::SemanticProgram program = makeProgramWith(makeRequirementFact(
      "/generic/runtime",
      "/std/meta/type_equals",
      "/std/meta/type_equals<read(), i32>()",
      {makeOperand("runtime_expression", "file.read()"),
       makeOperand("type_fact", "i32")}));
  const primec::SemanticProgramCompileTimeHost host(program);

  const primec::CompileTimeCallablePrepareResult result =
      primec::prepareCompileTimeCallable(
          host,
          makeRequest("/generic/runtime",
                      "/std/meta/type_equals",
                      "/std/meta/type_equals<read(), i32>()"));

  CHECK_FALSE(result.prepared());
  CHECK(result.status ==
        primec::CompileTimeCallablePrepareStatus::RuntimeOnlyOperation);
  CHECK(result.diagnostic.kind ==
        primec::CompileTimeEvaluationResultKind::InvalidEvaluation);
  CHECK(result.diagnostic.message.find("runtime-only operation") !=
        std::string::npos);
  CHECK(result.callable.operands.empty());
}

TEST_CASE("evaluated user predicates prepare with compile-time arguments") {
  primec::SemanticProgram program = makeProgramWith(makeRequirementFact(
      "/generic/user",
      "/project/is_small",
      "/project/is_small<N>()",
      {makeOperand("literal_compile_time_argument", "4")}));
  const primec::SemanticProgramCompileTimeHost host(program);

  const primec::CompileTimeCallablePrepareResult result =
      primec::prepareCompileTimeCallable(
          host,
          makeRequest("/generic/user", "/project/is_small", {}));

  REQUIRE(result.prepared());
  CHECK(result.status == primec::CompileTimeCallablePrepareStatus::Prepared);
  CHECK(result.callable.predicateName == "/project/is_small");
  REQUIRE(result.callable.operands.size() == 1);
  CHECK(result.callable.operands[0].kind == "literal_compile_time_argument");
  CHECK(result.callable.operands[0].value.kind ==
        primec::CompileTimeValueKind::UnsignedInteger);
  CHECK(std::get<std::uint64_t>(result.callable.operands[0].value.payload) ==
        4);
}

TEST_CASE("denied compile-time effect facts reject before callable execution") {
  primec::SemanticProgram program = makeProgramWith(makeRequirementFact(
      "/generic/effect",
      "/project/needs_file",
      "/project/needs_file()",
      {},
      "denied_effect",
      "denied compile-time effect in user requirement predicate "
      "/project/needs_file: file_read "
      "(missing effects<compiletime>(file_read))"));
  const primec::SemanticProgramCompileTimeHost host(program);

  const primec::CompileTimeCallablePrepareResult result =
      primec::prepareCompileTimeCallable(
          host,
          makeRequest("/generic/effect", "/project/needs_file", {}));

  CHECK_FALSE(result.prepared());
  CHECK(result.status == primec::CompileTimeCallablePrepareStatus::DeniedEffect);
  CHECK(result.diagnostic.kind ==
        primec::CompileTimeEvaluationResultKind::DeniedEffect);
  CHECK(result.diagnostic.message.find("missing effects<compiletime>") !=
        std::string::npos);
  CHECK(result.callable.operands.empty());
}

TEST_CASE("compile-time callable preparation enforces deterministic budgets") {
  auto prepareWithBudget = [](primec::CompileTimeEvaluationBudget budget) {
    primec::SemanticProgram program = makeProgramWith(makeRequirementFact(
        "/generic/user",
        "/project/is_small",
        "/project/is_small<N>()",
        {makeOperand("literal_compile_time_argument", "12345")}));
    const primec::SemanticProgramCompileTimeHost host(program);
    primec::CompileTimeCallablePrepareRequest request =
        makeRequest("/generic/user", "/project/is_small", {});
    request.budget = budget;
    return primec::prepareCompileTimeCallable(host, request);
  };

  CHECK(prepareWithBudget({}).status ==
        primec::CompileTimeCallablePrepareStatus::Prepared);

  auto frameBudget = primec::CompileTimeEvaluationBudget{};
  frameBudget.maxFrames = 0;
  const auto frame = prepareWithBudget(frameBudget);
  CHECK(frame.status ==
        primec::CompileTimeCallablePrepareStatus::BudgetExceeded);
  CHECK(frame.diagnostic.message.find("frame budget exceeded") !=
        std::string::npos);

  auto userBudget = primec::CompileTimeEvaluationBudget{};
  userBudget.maxUserPredicateCalls = 0;
  const auto user = prepareWithBudget(userBudget);
  CHECK(user.status ==
        primec::CompileTimeCallablePrepareStatus::BudgetExceeded);
  CHECK(user.diagnostic.message.find("user predicate call budget exceeded") !=
        std::string::npos);

  auto valueBudget = primec::CompileTimeEvaluationBudget{};
  valueBudget.maxValueBytes = 4;
  const auto value = prepareWithBudget(valueBudget);
  CHECK(value.status ==
        primec::CompileTimeCallablePrepareStatus::BudgetExceeded);
  CHECK(value.diagnostic.message.find("value storage budget exceeded") !=
        std::string::npos);

  auto storageBudget = primec::CompileTimeEvaluationBudget{};
  storageBudget.maxStorageBytes = 4;
  const auto storage = prepareWithBudget(storageBudget);
  CHECK(storage.status ==
        primec::CompileTimeCallablePrepareStatus::BudgetExceeded);
  CHECK(storage.diagnostic.message.find("storage byte budget exceeded") !=
        std::string::npos);

  auto hostBudget = primec::CompileTimeEvaluationBudget{};
  hostBudget.maxHostBytes = 4;
  const auto host = prepareWithBudget(hostBudget);
  CHECK(host.status ==
        primec::CompileTimeCallablePrepareStatus::BudgetExceeded);
  CHECK(host.diagnostic.message.find("host byte budget exceeded") !=
        std::string::npos);

  auto diagnosticBudget = primec::CompileTimeEvaluationBudget{};
  diagnosticBudget.maxDiagnosticBytes = 4;
  const auto diagnostic = prepareWithBudget(diagnosticBudget);
  CHECK(diagnostic.status ==
        primec::CompileTimeCallablePrepareStatus::BudgetExceeded);
  CHECK(diagnostic.diagnostic.message.find(
            "diagnostic payload budget exceeded") != std::string::npos);
}

TEST_CASE("unsupported user predicates and operands reject deterministically") {
  primec::SemanticProgram program = makeProgramWith(makeRequirementFact(
      "/generic/user",
      "/project/is_small",
      "/project/is_small<N>()",
      {makeOperand("unsigned_integer", "4")},
      "invalid_evaluation",
      "unsupported pure user predicate body"));
  const primec::SemanticProgramCompileTimeHost host(program);

  const primec::CompileTimeCallablePrepareResult result =
      primec::prepareCompileTimeCallable(
          host,
          makeRequest("/generic/user", "/project/is_small", {}));

  CHECK_FALSE(result.prepared());
  CHECK(result.status ==
        primec::CompileTimeCallablePrepareStatus::UnsupportedPredicate);
  CHECK(result.diagnostic.kind ==
        primec::CompileTimeEvaluationResultKind::InvalidEvaluation);
  CHECK(result.diagnostic.message.find("unsupported or unevaluated") !=
        std::string::npos);

  primec::SemanticProgram unsupportedOperandProgram =
      makeProgramWith(makeRequirementFact("/generic/bad",
                                          "/std/meta/has_trait",
                                          "/std/meta/has_trait<i32, Additive>()",
                                          {makeOperand("opaque_handle", "x")}));
  const primec::SemanticProgramCompileTimeHost unsupportedOperandHost(
      unsupportedOperandProgram);

  const primec::CompileTimeCallablePrepareResult unsupportedOperand =
      primec::prepareCompileTimeCallable(
          unsupportedOperandHost,
          makeRequest("/generic/bad",
                      "/std/meta/has_trait",
                      "/std/meta/has_trait<i32, Additive>()"));

  CHECK_FALSE(unsupportedOperand.prepared());
  CHECK(unsupportedOperand.status ==
        primec::CompileTimeCallablePrepareStatus::UnsupportedOperand);
  CHECK(unsupportedOperand.diagnostic.message.find("opaque_handle") !=
        std::string::npos);
}

TEST_CASE("missing facts and preparation budgets fail before execution") {
  primec::SemanticProgram emptyProgram;
  const primec::SemanticProgramCompileTimeHost emptyHost(emptyProgram);

  const primec::CompileTimeCallablePrepareResult missing =
      primec::prepareCompileTimeCallable(
          emptyHost,
          makeRequest("/generic/missing",
                      "/std/meta/has_trait",
                      "/std/meta/has_trait<i32, Additive>()"));

  CHECK_FALSE(missing.prepared());
  CHECK(missing.status ==
        primec::CompileTimeCallablePrepareStatus::MissingSemanticFact);
  CHECK(missing.diagnostic.kind ==
        primec::CompileTimeEvaluationResultKind::InvalidEvaluation);
  CHECK(missing.diagnostic.message.find(
            "missing semantic-product requirementPredicateFacts fact") !=
        std::string::npos);

  primec::SemanticProgram budgetProgram = makeProgramWith(makeRequirementFact(
      "/generic/budget",
      "/std/meta/type_equals",
      "/std/meta/type_equals<i32, i64>()",
      {makeOperand("type_fact", "i32"), makeOperand("type_fact", "i64")}));
  const primec::SemanticProgramCompileTimeHost budgetHost(budgetProgram);
  primec::CompileTimeCallablePrepareRequest budgetRequest =
      makeRequest("/generic/budget",
                  "/std/meta/type_equals",
                  "/std/meta/type_equals<i32, i64>()");
  budgetRequest.budget.maxPreparationSteps = 2;
  budgetRequest.budget.maxSteps = 2;

  const primec::CompileTimeCallablePrepareResult budget =
      primec::prepareCompileTimeCallable(budgetHost, budgetRequest);

  CHECK_FALSE(budget.prepared());
  CHECK(budget.status ==
        primec::CompileTimeCallablePrepareStatus::BudgetExceeded);
  CHECK(budget.diagnostic.kind ==
        primec::CompileTimeEvaluationResultKind::BudgetExhausted);
  CHECK(budget.diagnostic.message.find("budget exceeded") !=
        std::string::npos);
}

TEST_CASE("compile-time callable preparation rejects stale requirementPredicateFacts") {
  primec::SemanticProgram program = makeProgramWith(makeRequirementFact(
      "/generic/user",
      "/project/is_small",
      "/project/is_small<N>()",
      {makeOperand("literal_compile_time_argument", "4")}));
  const primec::SemanticProgramCompileTimeHost host(program);

  primec::CompileTimeCallablePrepareRequest request =
      makeRequest("/generic/user",
                  "/project/is_small",
                  "/project/is_small<Other>()");
  request.requirementPredicate = &program.requirementPredicateFacts.front();

  const primec::CompileTimeCallablePrepareResult result =
      primec::prepareCompileTimeCallable(host, request);

  CHECK_FALSE(result.prepared());
  CHECK(result.status ==
        primec::CompileTimeCallablePrepareStatus::MissingSemanticFact);
  CHECK(result.diagnostic.kind ==
        primec::CompileTimeEvaluationResultKind::InvalidEvaluation);
  CHECK(result.diagnostic.message.find(
            "stale semantic-product requirementPredicateFacts argument "
            "identity") != std::string::npos);
}

TEST_SUITE_END();
