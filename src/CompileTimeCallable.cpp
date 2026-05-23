#include "primec/CompileTimeCallable.h"

#include "primec/SemanticProduct.h"

#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace primec {

namespace {

std::string_view resolvedSemanticText(
    const SemanticProgram *semanticProgram,
    SymbolId textId,
    const std::string &fallback) {
  if (semanticProgram == nullptr || textId == InvalidSymbolId) {
    return fallback;
  }
  const std::string_view resolved =
      semanticProgramResolveCallTargetString(*semanticProgram, textId);
  return resolved.empty() ? std::string_view(fallback) : resolved;
}

bool isSupportedBuiltinPredicate(std::string_view predicateName) {
  return predicateName == "/std/meta/type_equals" ||
         predicateName == "/std/meta/type_not_equals" ||
         predicateName == "/std/meta/is_type" ||
         predicateName == "/std/meta/is_struct" ||
         predicateName == "/std/meta/is_sum" ||
         predicateName == "/std/meta/has_trait" ||
         predicateName == "/std/meta/supports_call" ||
         predicateName == "/std/meta/can_construct" ||
         predicateName == "/std/meta/can_copy" ||
         predicateName == "/std/meta/can_move" ||
         predicateName == "/std/meta/has_field" ||
         predicateName == "/std/meta/has_member";
}

bool isEvaluatedUserPredicateFact(std::string_view predicateName,
                                  std::string_view outcome) {
  return !predicateName.empty() &&
         predicateName.rfind("/std/meta/", 0) != 0 &&
         (outcome == "satisfied" || outcome == "success" ||
          outcome == "unsatisfied");
}

CompileTimeValueProvenance operandProvenance(
    const SemanticProgramRequirementPredicateFact &fact,
    const SemanticProgramRequirementPredicateOperand &operand,
    std::string_view sourceText) {
  CompileTimeValueProvenance provenance;
  provenance.sourceText = std::string(sourceText);
  provenance.line = operand.sourceLine < 0 ? 0
                                           : static_cast<std::uint32_t>(
                                                 operand.sourceLine);
  provenance.column = operand.sourceColumn < 0 ? 0
                                               : static_cast<std::uint32_t>(
                                                     operand.sourceColumn);
  provenance.semanticNodeId = fact.semanticNodeId;
  provenance.provenanceHandle = fact.provenanceHandle;
  return provenance;
}

template <typename Integer>
std::optional<Integer> parseInteger(std::string_view text) {
  Integer value{};
  const char *begin = text.data();
  const char *end = text.data() + text.size();
  const auto [ptr, error] = std::from_chars(begin, end, value);
  if (error != std::errc{} || ptr != end) {
    return std::nullopt;
  }
  return value;
}

struct PreparedOperandResult {
  CompileTimeCallablePrepareStatus status =
      CompileTimeCallablePrepareStatus::Prepared;
  CompileTimeCallableOperand operand;
  std::string message;
};

std::uint64_t byteSize(std::string_view text) {
  return static_cast<std::uint64_t>(text.size());
}

std::uint64_t addBudgetBytes(std::uint64_t left, std::uint64_t right) {
  constexpr std::uint64_t Max = UINT64_MAX;
  if (Max - left < right) {
    return Max;
  }
  return left + right;
}

std::uint64_t callableHostBudgetBytes(
    const SemanticProgram *semanticProgram,
    const SemanticProgramRequirementPredicateFact &fact,
    std::string_view predicatePath,
    std::string_view sourceText,
    std::string_view outcome) {
  std::uint64_t bytes = byteSize(predicatePath);
  bytes = addBudgetBytes(bytes, byteSize(sourceText));
  bytes = addBudgetBytes(bytes, byteSize(outcome));
  bytes = addBudgetBytes(bytes,
                         byteSize(resolvedSemanticText(
                             semanticProgram, fact.definitionPathId,
                             fact.definitionPath)));
  bytes = addBudgetBytes(bytes,
                         byteSize(resolvedSemanticText(
                             semanticProgram, fact.evaluationDiagnosticId,
                             fact.evaluationDiagnostic)));
  return bytes;
}

std::uint64_t callableValueBudgetBytes(
    const SemanticProgram *semanticProgram,
    const SemanticProgramRequirementPredicateFact &fact) {
  std::uint64_t bytes = 0;
  for (const SemanticProgramRequirementPredicateOperand &operand :
       fact.operands) {
    bytes = addBudgetBytes(bytes,
                           byteSize(resolvedSemanticText(
                               semanticProgram, operand.kindId, operand.kind)));
    bytes = addBudgetBytes(bytes,
                           byteSize(resolvedSemanticText(
                               semanticProgram, operand.textId, operand.text)));
  }
  return bytes;
}

std::uint64_t callableStorageBudgetBytes(
    const SemanticProgram *semanticProgram,
    const SemanticProgramRequirementPredicateFact &fact) {
  std::uint64_t bytes = callableValueBudgetBytes(semanticProgram, fact);
  bytes = addBudgetBytes(bytes,
                         byteSize(resolvedSemanticText(
                             semanticProgram, fact.sourceTextId,
                             fact.sourceText)));
  for (const SemanticProgramRequirementPredicateOperand &operand :
       fact.operands) {
    bytes = addBudgetBytes(bytes, sizeof(operand.sourceLine));
    bytes = addBudgetBytes(bytes, sizeof(operand.sourceColumn));
    bytes = addBudgetBytes(bytes,
                           byteSize(resolvedSemanticText(
                               semanticProgram, operand.stableHandleId,
                               operand.stableHandle)));
  }
  return bytes;
}

PreparedOperandResult prepareOperand(
    const SemanticProgramRequirementPredicateFact &fact,
    const SemanticProgramRequirementPredicateOperand &operand,
    std::string_view kind,
    std::string_view text) {
  PreparedOperandResult result;
  result.operand.kind = std::string(kind);
  result.operand.sourceText = std::string(text);

  const CompileTimeValueProvenance provenance =
      operandProvenance(fact, operand, text);
  if (kind.find("runtime") != std::string_view::npos) {
    result.status = CompileTimeCallablePrepareStatus::RuntimeOnlyOperation;
    result.message =
        "runtime-only operation is not supported in compile-time callable: " +
        std::string(text);
    result.operand.value =
        makeUnsupportedRuntimeCompileTimeValue(std::string(text), provenance);
    return result;
  }
  if (kind == "type_fact" || kind == "type") {
    result.operand.value =
        makeCompileTimeTypeFact(std::string(text), provenance);
    return result;
  }
  if (kind == "symbol" || kind == "name" || kind == "compile_time_symbol") {
    result.operand.value = makeCompileTimeSymbol(std::string(text), provenance);
    return result;
  }
  if (kind == "literal_compile_time_argument") {
    if (text == "true" || text == "false") {
      result.operand.value =
          makeCompileTimeBool(text == "true", provenance);
      return result;
    }
    if (const std::optional<std::uint64_t> parsed =
            parseInteger<std::uint64_t>(text);
        parsed.has_value()) {
      result.operand.value =
          makeCompileTimeUnsignedInteger(*parsed, provenance);
      return result;
    }
    result.operand.value =
        makeCompileTimeStringLiteral(std::string(text), provenance);
    return result;
  }
  if (kind == "bool") {
    if (text == "true" || text == "1") {
      result.operand.value = makeCompileTimeBool(true, provenance);
      return result;
    }
    if (text == "false" || text == "0") {
      result.operand.value = makeCompileTimeBool(false, provenance);
      return result;
    }
    result.status = CompileTimeCallablePrepareStatus::UnsupportedOperand;
    result.message = "unsupported compile-time bool operand: " +
                     std::string(text);
    return result;
  }
  if (kind == "signed_integer" || kind == "integer") {
    const std::optional<std::int64_t> parsed =
        parseInteger<std::int64_t>(text);
    if (!parsed.has_value()) {
      result.status = CompileTimeCallablePrepareStatus::UnsupportedOperand;
      result.message = "unsupported compile-time integer operand: " +
                       std::string(text);
      return result;
    }
    result.operand.value = makeCompileTimeSignedInteger(*parsed, provenance);
    return result;
  }
  if (kind == "unsigned_integer") {
    const std::optional<std::uint64_t> parsed =
        parseInteger<std::uint64_t>(text);
    if (!parsed.has_value()) {
      result.status = CompileTimeCallablePrepareStatus::UnsupportedOperand;
      result.message = "unsupported compile-time unsigned integer operand: " +
                       std::string(text);
      return result;
    }
    result.operand.value = makeCompileTimeUnsignedInteger(*parsed, provenance);
    return result;
  }
  if (kind == "string_literal" || kind == "string") {
    result.operand.value =
        makeCompileTimeStringLiteral(std::string(text), provenance);
    return result;
  }

  result.status = CompileTimeCallablePrepareStatus::UnsupportedOperand;
  result.message =
      "unsupported compile-time callable operand kind: " + std::string(kind);
  return result;
}

CompileTimeCallablePrepareResult makeBudgetRejectedResult(
    CompileTimeEvaluationResult diagnostic) {
  CompileTimeCallablePrepareResult result;
  result.status = CompileTimeCallablePrepareStatus::BudgetExceeded;
  result.diagnostic = std::move(diagnostic);
  return result;
}

CompileTimeCallablePrepareResult makeRejectedResult(
    CompileTimeCallablePrepareStatus status,
    const CompileTimeEvaluationFacade &facade,
    const CompileTimeEvaluationProvenance &provenance,
    std::string message) {
  CompileTimeCallablePrepareResult result;
  result.status = status;
  if (status == CompileTimeCallablePrepareStatus::DeniedEffect) {
    result.diagnostic = facade.deniedEffect(message, provenance);
  } else if (status == CompileTimeCallablePrepareStatus::BudgetExceeded) {
    result.diagnostic = facade.budgetExhausted(provenance, std::move(message));
  } else {
    result.diagnostic = facade.invalidEvaluation(provenance, std::move(message));
  }
  return result;
}

} // namespace

bool CompileTimeCallablePrepareResult::prepared() const {
  return status == CompileTimeCallablePrepareStatus::Prepared;
}

std::string_view compileTimeCallablePrepareStatusName(
    CompileTimeCallablePrepareStatus status) {
  switch (status) {
  case CompileTimeCallablePrepareStatus::Prepared:
    return "prepared";
  case CompileTimeCallablePrepareStatus::UnsupportedPredicate:
    return "unsupported_predicate";
  case CompileTimeCallablePrepareStatus::UnsupportedOperand:
    return "unsupported_operand";
  case CompileTimeCallablePrepareStatus::RuntimeOnlyOperation:
    return "runtime_only_operation";
  case CompileTimeCallablePrepareStatus::MissingSemanticFact:
    return "missing_semantic_fact";
  case CompileTimeCallablePrepareStatus::DeniedEffect:
    return "denied_effect";
  case CompileTimeCallablePrepareStatus::BudgetExceeded:
    return "budget_exceeded";
  }
  return "unknown";
}

CompileTimeCallablePrepareResult prepareCompileTimeCallable(
    const CompileTimeHost &host,
    const CompileTimeCallablePrepareRequest &request) {
  const CompileTimeEvaluationFacade facade(host, request.budget);
  const SemanticProgram *semanticProgram = host.semanticProgram();
  const SemanticProgramRequirementPredicateFact *fact =
      request.requirementPredicate;
  if (fact == nullptr) {
    fact = host.findRequirementPredicateFact(request.definitionPath,
                                            request.predicateName,
                                            request.sourceText);
  }
  if (fact == nullptr) {
    const std::string predicateName =
        request.predicateName.empty() ? std::string("<unknown>")
                                      : request.predicateName;
    return makeRejectedResult(
        CompileTimeCallablePrepareStatus::MissingSemanticFact,
        facade,
        request.provenance,
        "missing semantic fact for compile-time callable: " + predicateName);
  }

  CompileTimeEvaluationProvenance provenance =
      compileTimeEvaluationProvenanceFromRequirement(*fact);
  provenance.definitionPath = std::string(resolvedSemanticText(
      semanticProgram, fact->definitionPathId, fact->definitionPath));
  provenance.predicatePath = std::string(resolvedSemanticText(
      semanticProgram, fact->predicateNameId, fact->predicateName));
  provenance.sourceText = std::string(resolvedSemanticText(
      semanticProgram, fact->sourceTextId, fact->sourceText));

  const std::string_view outcome = resolvedSemanticText(
      semanticProgram, fact->evaluationOutcomeId, fact->evaluationOutcome);
  const std::uint64_t requiredSteps =
      static_cast<std::uint64_t>(fact->operands.size()) + 1;
  if (auto overBudget = facade.requireBudget("frame",
                                             1,
                                             request.budget.maxFrames,
                                             provenance)) {
    return makeBudgetRejectedResult(std::move(*overBudget));
  }
  if (auto overBudget = facade.requireBudget("preparation step",
                                             requiredSteps,
                                             request.budget.maxPreparationSteps,
                                             provenance)) {
    return makeBudgetRejectedResult(std::move(*overBudget));
  }
  if (auto overBudget = facade.requireBudget(
          "user predicate call",
          isEvaluatedUserPredicateFact(provenance.predicatePath, outcome) ? 1
                                                                          : 0,
          request.budget.maxUserPredicateCalls,
          provenance)) {
    return makeBudgetRejectedResult(std::move(*overBudget));
  }
  if (auto overBudget = facade.requireBudget(
          "value storage",
          callableValueBudgetBytes(semanticProgram, *fact),
          request.budget.maxValueBytes,
          provenance)) {
    return makeBudgetRejectedResult(std::move(*overBudget));
  }
  if (auto overBudget = facade.requireBudget(
          "storage byte",
          callableStorageBudgetBytes(semanticProgram, *fact),
          request.budget.maxStorageBytes,
          provenance)) {
    return makeBudgetRejectedResult(std::move(*overBudget));
  }
  if (auto overBudget = facade.requireBudget(
          "host byte",
          callableHostBudgetBytes(semanticProgram,
                                  *fact,
                                  provenance.predicatePath,
                                  provenance.sourceText,
                                  outcome),
          request.budget.maxHostBytes,
          provenance)) {
    return makeBudgetRejectedResult(std::move(*overBudget));
  }

  if (!isSupportedBuiltinPredicate(provenance.predicatePath) &&
      !isEvaluatedUserPredicateFact(provenance.predicatePath, outcome)) {
    return makeRejectedResult(
        CompileTimeCallablePrepareStatus::UnsupportedPredicate,
        facade,
        provenance,
        "unsupported or unevaluated compile-time predicate: " +
            provenance.predicatePath);
  }

  CompileTimeCallablePrepareResult result;
  result.status = CompileTimeCallablePrepareStatus::Prepared;
  result.callable.definitionPath = provenance.definitionPath;
  result.callable.predicateName = provenance.predicatePath;
  result.callable.sourceText = provenance.sourceText;
  result.callable.provenance = provenance;
  result.callable.maxSteps = request.budget.maxSteps;
  result.callable.maxFrames = request.budget.maxFrames;

  result.callable.operands.reserve(fact->operands.size());
  for (const SemanticProgramRequirementPredicateOperand &operand :
       fact->operands) {
    const std::string_view kind =
        resolvedSemanticText(semanticProgram, operand.kindId, operand.kind);
    const std::string_view text =
        resolvedSemanticText(semanticProgram, operand.textId, operand.text);
    PreparedOperandResult preparedOperand =
        prepareOperand(*fact, operand, kind, text);
    if (preparedOperand.status != CompileTimeCallablePrepareStatus::Prepared) {
      return makeRejectedResult(preparedOperand.status,
                                facade,
                                provenance,
                                std::move(preparedOperand.message));
    }
    result.callable.operands.push_back(std::move(preparedOperand.operand));
  }

  result.diagnostic =
      facade.success(true,
                     provenance,
                     "prepared compile-time callable: " +
                         result.callable.predicateName);
  if (result.diagnostic.kind ==
      CompileTimeEvaluationResultKind::BudgetExhausted) {
    result.status = CompileTimeCallablePrepareStatus::BudgetExceeded;
    result.callable = {};
  }
  return result;
}

} // namespace primec
