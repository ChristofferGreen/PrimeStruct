#include "primec/CompileTimeEvaluation.h"

#include "primec/SemanticProduct.h"

#include <sstream>
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

std::string_view resolvedRequirementFactText(
    const SemanticProgram &semanticProgram,
    SymbolId textId,
    const std::string &fallback) {
  return resolvedSemanticText(&semanticProgram, textId, fallback);
}

CompileTimeEvaluationResultKind resultKindFromRequirementOutcome(
    std::string_view outcome) {
  if (outcome == "satisfied" || outcome == "success") {
    return CompileTimeEvaluationResultKind::Success;
  }
  if (outcome == "unsatisfied") {
    return CompileTimeEvaluationResultKind::UnsatisfiedPredicate;
  }
  if (outcome == "invalid_evaluation") {
    return CompileTimeEvaluationResultKind::InvalidEvaluation;
  }
  if (outcome == "denied_effect") {
    return CompileTimeEvaluationResultKind::DeniedEffect;
  }
  if (outcome == "budget_exhausted") {
    return CompileTimeEvaluationResultKind::BudgetExhausted;
  }
  if (outcome == "internal_compiler_error") {
    return CompileTimeEvaluationResultKind::InternalCompilerError;
  }
  return CompileTimeEvaluationResultKind::InvalidEvaluation;
}

CompileTimeEvaluationFaultKind faultKindFromResultKind(
    CompileTimeEvaluationResultKind kind) {
  switch (kind) {
  case CompileTimeEvaluationResultKind::Success:
    return CompileTimeEvaluationFaultKind::None;
  case CompileTimeEvaluationResultKind::UnsatisfiedPredicate:
    return CompileTimeEvaluationFaultKind::UnsatisfiedPredicate;
  case CompileTimeEvaluationResultKind::InvalidEvaluation:
    return CompileTimeEvaluationFaultKind::InvalidEvaluation;
  case CompileTimeEvaluationResultKind::DeniedEffect:
    return CompileTimeEvaluationFaultKind::DeniedEffect;
  case CompileTimeEvaluationResultKind::BudgetExhausted:
    return CompileTimeEvaluationFaultKind::BudgetExhausted;
  case CompileTimeEvaluationResultKind::InternalCompilerError:
    return CompileTimeEvaluationFaultKind::InternalCompilerError;
  }
  return CompileTimeEvaluationFaultKind::InternalCompilerError;
}

CompileTimeEvaluationResult makeFault(CompileTimeEvaluationResultKind kind,
                                      CompileTimeEvaluationFaultKind fault,
                                      CompileTimeEvaluationProvenance provenance,
                                      std::string message) {
  CompileTimeEvaluationResult result;
  result.kind = kind;
  result.fault = fault;
  result.provenance = std::move(provenance);
  result.message = std::move(message);
  result.boolValue = false;
  return result;
}

} // namespace

std::optional<std::string>
CompileTimeHost::describeSemanticFact(std::string_view factName) const {
  (void)factName;
  return std::nullopt;
}

const SemanticProgramRequirementPredicateFact *
CompileTimeHost::findRequirementPredicateFact(std::string_view definitionPath,
                                              std::string_view predicateName,
                                              std::string_view sourceText) const {
  (void)definitionPath;
  (void)predicateName;
  (void)sourceText;
  return nullptr;
}

const SemanticProgram *CompileTimeHost::semanticProgram() const {
  return nullptr;
}

SemanticProgramCompileTimeHost::SemanticProgramCompileTimeHost(
    const SemanticProgram &semanticProgram)
    : semanticProgram_(semanticProgram) {}

bool SemanticProgramCompileTimeHost::allowEffect(
    std::string_view effectName,
    CompileTimeEffectPhase phase) const {
  (void)effectName;
  (void)phase;
  return false;
}

std::optional<std::string>
SemanticProgramCompileTimeHost::describeSemanticFact(
    std::string_view factName) const {
  if (factName == "requirement_predicate_count") {
    return std::to_string(semanticProgramRequirementPredicateFactView(
                              semanticProgram_)
                              .size());
  }
  if (factName.rfind("requirement_predicate:", 0) == 0) {
    const std::string_view predicateName =
        factName.substr(std::string_view("requirement_predicate:").size());
    std::size_t count = 0;
    for (const auto *fact :
         semanticProgramRequirementPredicateFactView(semanticProgram_)) {
      if (fact == nullptr) {
        continue;
      }
      if (resolvedRequirementFactText(semanticProgram_,
                                      fact->predicateNameId,
                                      fact->predicateName) == predicateName) {
        ++count;
      }
    }
    return std::to_string(count);
  }
  return std::nullopt;
}

const SemanticProgramRequirementPredicateFact *
SemanticProgramCompileTimeHost::findRequirementPredicateFact(
    std::string_view definitionPath,
    std::string_view predicateName,
    std::string_view sourceText) const {
  for (const auto *fact :
       semanticProgramRequirementPredicateFactView(semanticProgram_)) {
    if (fact == nullptr) {
      continue;
    }
    if (!definitionPath.empty() &&
        resolvedRequirementFactText(semanticProgram_,
                                    fact->definitionPathId,
                                    fact->definitionPath) != definitionPath) {
      continue;
    }
    if (!predicateName.empty() &&
        resolvedRequirementFactText(semanticProgram_,
                                    fact->predicateNameId,
                                    fact->predicateName) != predicateName) {
      continue;
    }
    if (!sourceText.empty() &&
        resolvedRequirementFactText(semanticProgram_,
                                    fact->sourceTextId,
                                    fact->sourceText) != sourceText) {
      continue;
    }
    return fact;
  }
  return nullptr;
}

const SemanticProgram *SemanticProgramCompileTimeHost::semanticProgram() const {
  return &semanticProgram_;
}

bool DenyAllCompileTimeHost::allowEffect(std::string_view effectName,
                                         CompileTimeEffectPhase phase) const {
  (void)effectName;
  (void)phase;
  return false;
}

CompileTimeEvaluationFacade::CompileTimeEvaluationFacade(
    const CompileTimeHost &host,
    CompileTimeEvaluationBudget budget)
    : host_(host),
      budget_(budget) {}

const CompileTimeEvaluationBudget &CompileTimeEvaluationFacade::budget() const {
  return budget_;
}

CompileTimeEvaluationResult CompileTimeEvaluationFacade::success(
    bool value,
    CompileTimeEvaluationProvenance provenance,
    std::string message) const {
  CompileTimeEvaluationResult result;
  result.kind = CompileTimeEvaluationResultKind::Success;
  result.fault = CompileTimeEvaluationFaultKind::None;
  result.provenance = std::move(provenance);
  result.message = std::move(message);
  result.boolValue = value;
  return result;
}

CompileTimeEvaluationResult
CompileTimeEvaluationFacade::unsatisfiedPredicate(
    CompileTimeEvaluationProvenance provenance,
    std::string message) const {
  return makeFault(CompileTimeEvaluationResultKind::UnsatisfiedPredicate,
                   CompileTimeEvaluationFaultKind::UnsatisfiedPredicate,
                   std::move(provenance),
                   std::move(message));
}

CompileTimeEvaluationResult CompileTimeEvaluationFacade::invalidEvaluation(
    CompileTimeEvaluationProvenance provenance,
    std::string message) const {
  return makeFault(CompileTimeEvaluationResultKind::InvalidEvaluation,
                   CompileTimeEvaluationFaultKind::InvalidEvaluation,
                   std::move(provenance),
                   std::move(message));
}

CompileTimeEvaluationResult
CompileTimeEvaluationFacade::deniedEffect(
    std::string_view effectName,
    CompileTimeEvaluationProvenance provenance) const {
  return makeFault(CompileTimeEvaluationResultKind::DeniedEffect,
                   CompileTimeEvaluationFaultKind::DeniedEffect,
                   std::move(provenance),
                   "denied compile-time effect: " + std::string(effectName));
}

CompileTimeEvaluationResult CompileTimeEvaluationFacade::budgetExhausted(
    CompileTimeEvaluationProvenance provenance,
    std::string message) const {
  return makeFault(CompileTimeEvaluationResultKind::BudgetExhausted,
                   CompileTimeEvaluationFaultKind::BudgetExhausted,
                   std::move(provenance),
                   std::move(message));
}

CompileTimeEvaluationResult
CompileTimeEvaluationFacade::internalCompilerError(
    CompileTimeEvaluationProvenance provenance,
    std::string message) const {
  return makeFault(CompileTimeEvaluationResultKind::InternalCompilerError,
                   CompileTimeEvaluationFaultKind::InternalCompilerError,
                   std::move(provenance),
                   std::move(message));
}

CompileTimeEvaluationResult CompileTimeEvaluationFacade::requireEffect(
    std::string_view effectName,
    CompileTimeEvaluationProvenance provenance) const {
  if (!host_.allowEffect(effectName, CompileTimeEffectPhase::SemanticRequirement)) {
    return deniedEffect(effectName, std::move(provenance));
  }
  return success(true,
                 std::move(provenance),
                 "allowed compile-time effect: " + std::string(effectName));
}

CompileTimeEvaluationResult
CompileTimeEvaluationFacade::evaluateRequirementPredicate(
    const CompileTimeEvaluationRequest &request) const {
  const SemanticProgram *semanticProgram =
      request.semanticProgram != nullptr ? request.semanticProgram
                                         : host_.semanticProgram();
  const SemanticProgramRequirementPredicateFact *requirementPredicate =
      request.requirementPredicate;
  if (requirementPredicate == nullptr) {
    requirementPredicate =
        host_.findRequirementPredicateFact(request.definitionPath,
                                           request.predicateName,
                                           request.sourceText);
  }
  if (requirementPredicate == nullptr) {
    const std::string predicateName =
        request.predicateName.empty() ? std::string("<unknown>")
                                      : request.predicateName;
    return internalCompilerError(
        request.provenance,
        "compile-time requirement evaluation requires a published predicate fact "
        "for " +
            predicateName);
  }

  const SemanticProgramRequirementPredicateFact &fact =
      *requirementPredicate;
  CompileTimeEvaluationProvenance provenance =
      compileTimeEvaluationProvenanceFromRequirement(fact);
  provenance.definitionPath = std::string(resolvedSemanticText(
      semanticProgram, fact.definitionPathId, fact.definitionPath));
  provenance.predicatePath = std::string(resolvedSemanticText(
      semanticProgram, fact.predicateNameId, fact.predicateName));
  provenance.sourceText = std::string(resolvedSemanticText(
      semanticProgram, fact.sourceTextId, fact.sourceText));

  const std::string_view outcome = resolvedSemanticText(
      semanticProgram, fact.evaluationOutcomeId, fact.evaluationOutcome);
  const std::string_view diagnostic = resolvedSemanticText(
      semanticProgram, fact.evaluationDiagnosticId, fact.evaluationDiagnostic);
  const CompileTimeEvaluationResultKind kind =
      resultKindFromRequirementOutcome(outcome);
  if (kind == CompileTimeEvaluationResultKind::Success) {
    return success(true, std::move(provenance), std::string(diagnostic));
  }
  return makeFault(kind,
                   faultKindFromResultKind(kind),
                   std::move(provenance),
                   std::string(diagnostic));
}

bool isCompileTimeEvaluationFault(CompileTimeEvaluationResultKind kind) {
  return kind != CompileTimeEvaluationResultKind::Success;
}

std::string_view
compileTimeEvaluationPhaseName(CompileTimeEvaluationPhase phase) {
  switch (phase) {
  case CompileTimeEvaluationPhase::SemanticRequirementEvaluation:
    return "semantic_requirement_evaluation";
  }
  return "unknown";
}

std::string_view
compileTimeEvaluationResultKindName(CompileTimeEvaluationResultKind kind) {
  switch (kind) {
  case CompileTimeEvaluationResultKind::Success:
    return "success";
  case CompileTimeEvaluationResultKind::UnsatisfiedPredicate:
    return "unsatisfied_predicate";
  case CompileTimeEvaluationResultKind::InvalidEvaluation:
    return "invalid_evaluation";
  case CompileTimeEvaluationResultKind::DeniedEffect:
    return "denied_effect";
  case CompileTimeEvaluationResultKind::BudgetExhausted:
    return "budget_exhausted";
  case CompileTimeEvaluationResultKind::InternalCompilerError:
    return "internal_compiler_error";
  }
  return "unknown";
}

std::string_view
compileTimeEvaluationFaultKindName(CompileTimeEvaluationFaultKind kind) {
  switch (kind) {
  case CompileTimeEvaluationFaultKind::None:
    return "none";
  case CompileTimeEvaluationFaultKind::UnsatisfiedPredicate:
    return "unsatisfied_predicate";
  case CompileTimeEvaluationFaultKind::InvalidEvaluation:
    return "invalid_evaluation";
  case CompileTimeEvaluationFaultKind::DeniedEffect:
    return "denied_effect";
  case CompileTimeEvaluationFaultKind::BudgetExhausted:
    return "budget_exhausted";
  case CompileTimeEvaluationFaultKind::InternalCompilerError:
    return "internal_compiler_error";
  }
  return "unknown";
}

std::string formatCompileTimeEvaluationProvenance(
    const CompileTimeEvaluationProvenance &provenance) {
  std::ostringstream out;
  out << "phase " << compileTimeEvaluationPhaseName(provenance.phase);
  if (!provenance.definitionPath.empty()) {
    out << ", ";
    out << "definition " << provenance.definitionPath;
  }
  if (!provenance.predicatePath.empty()) {
    out << ", ";
    out << "predicate " << provenance.predicatePath;
  }
  if (!provenance.sourceText.empty()) {
    out << ", ";
    out << "source_text \"" << provenance.sourceText << "\"";
  }
  if (!provenance.sourcePath.empty()) {
    out << ", ";
    out << "source " << provenance.sourcePath;
    if (provenance.line != 0) {
      out << ':' << provenance.line;
      if (provenance.column != 0) {
        out << ':' << provenance.column;
      }
    }
  }
  if (provenance.semanticNodeId != 0) {
    out << ", semantic_node_id " << provenance.semanticNodeId;
  }
  if (provenance.provenanceHandle != 0) {
    out << ", provenance_handle " << provenance.provenanceHandle;
  }
  const std::string formatted = out.str();
  return formatted.empty() ? std::string("unknown provenance") : formatted;
}

std::string
formatCompileTimeEvaluationResult(const CompileTimeEvaluationResult &result) {
  std::ostringstream out;
  out << "compile-time evaluation "
      << compileTimeEvaluationResultKindName(result.kind);
  if (result.fault != CompileTimeEvaluationFaultKind::None) {
    out << " fault=" << compileTimeEvaluationFaultKindName(result.fault);
  } else {
    out << " value=" << (result.boolValue ? "true" : "false");
  }
  out << " (" << formatCompileTimeEvaluationProvenance(result.provenance) << ')';
  if (!result.message.empty()) {
    out << ": " << result.message;
  }
  return out.str();
}

CompileTimeEvaluationProvenance compileTimeEvaluationProvenanceFromRequirement(
    const SemanticProgramRequirementPredicateFact &fact) {
  CompileTimeEvaluationProvenance provenance;
  provenance.phase = CompileTimeEvaluationPhase::SemanticRequirementEvaluation;
  provenance.definitionPath = fact.definitionPath;
  provenance.predicatePath = fact.predicateName;
  provenance.sourceText = fact.sourceText;
  provenance.line = fact.sourceLine < 0 ? 0 : static_cast<std::uint32_t>(fact.sourceLine);
  provenance.column = fact.sourceColumn < 0 ? 0 : static_cast<std::uint32_t>(fact.sourceColumn);
  provenance.semanticNodeId = fact.semanticNodeId;
  provenance.provenanceHandle = fact.provenanceHandle;
  return provenance;
}

} // namespace primec
