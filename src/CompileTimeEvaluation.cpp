#include "primec/CompileTimeEvaluation.h"

#include "primec/SemanticProduct.h"

#include <optional>
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

std::string formatBudgetExhaustedMessage(std::string_view budgetName,
                                         std::uint64_t used,
                                         std::uint64_t limit) {
  std::ostringstream out;
  out << "compile-time " << budgetName << " budget exceeded";
  out << " (used " << used << ", limit " << limit << ')';
  return out.str();
}

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

std::uint64_t provenanceBudgetBytes(
    const CompileTimeEvaluationProvenance &provenance) {
  std::uint64_t bytes = byteSize(provenance.definitionPath);
  bytes = addBudgetBytes(bytes, byteSize(provenance.predicatePath));
  bytes = addBudgetBytes(bytes, byteSize(provenance.sourcePath));
  bytes = addBudgetBytes(bytes, byteSize(provenance.sourceText));
  bytes = addBudgetBytes(bytes, sizeof(provenance.line));
  bytes = addBudgetBytes(bytes, sizeof(provenance.column));
  bytes = addBudgetBytes(bytes, sizeof(provenance.semanticNodeId));
  bytes = addBudgetBytes(bytes, sizeof(provenance.provenanceHandle));
  return bytes;
}

std::uint64_t requirementValueBudgetBytes(
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

std::uint64_t requirementStorageBudgetBytes(
    const SemanticProgram *semanticProgram,
    const SemanticProgramRequirementPredicateFact &fact) {
  std::uint64_t bytes = requirementValueBudgetBytes(semanticProgram, fact);
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

std::uint64_t requirementHostBudgetBytes(
    const SemanticProgram *semanticProgram,
    const SemanticProgramRequirementPredicateFact &fact) {
  std::uint64_t bytes = byteSize(resolvedSemanticText(
      semanticProgram, fact.definitionPathId, fact.definitionPath));
  bytes = addBudgetBytes(bytes,
                         byteSize(resolvedSemanticText(
                             semanticProgram, fact.predicateNameId,
                             fact.predicateName)));
  bytes = addBudgetBytes(bytes,
                         byteSize(resolvedSemanticText(
                             semanticProgram, fact.sourceTextId,
                             fact.sourceText)));
  bytes = addBudgetBytes(bytes,
                         byteSize(resolvedSemanticText(
                             semanticProgram, fact.evaluationOutcomeId,
                             fact.evaluationOutcome)));
  bytes = addBudgetBytes(bytes,
                         byteSize(resolvedSemanticText(
                             semanticProgram, fact.evaluationDiagnosticId,
                             fact.evaluationDiagnostic)));
  return bytes;
}

bool isUserPredicatePath(std::string_view predicatePath) {
  return !predicatePath.empty() &&
         predicatePath.rfind("/std/meta/", 0) != 0;
}

bool budgetEquals(const CompileTimeEvaluationBudget &left,
                  const CompileTimeEvaluationBudget &right) {
  return left.maxPreparationSteps == right.maxPreparationSteps &&
         left.maxSteps == right.maxSteps &&
         left.maxFrames == right.maxFrames &&
         left.maxUserPredicateCalls == right.maxUserPredicateCalls &&
         left.maxValueBytes == right.maxValueBytes &&
         left.maxStorageBytes == right.maxStorageBytes &&
         left.maxHostBytes == right.maxHostBytes &&
         left.maxDiagnosticBytes == right.maxDiagnosticBytes &&
         left.maxProvenanceBytes == right.maxProvenanceBytes;
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
  if (auto overBudget = requireBudget("diagnostic payload",
                                      byteSize(message),
                                      budget_.maxDiagnosticBytes,
                                      provenance)) {
    return *overBudget;
  }
  if (auto overBudget = requireBudget("provenance payload",
                                      provenanceBudgetBytes(provenance),
                                      budget_.maxProvenanceBytes,
                                      provenance)) {
    return *overBudget;
  }
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
  if (auto overBudget = requireBudget("diagnostic payload",
                                      byteSize(message),
                                      budget_.maxDiagnosticBytes,
                                      provenance)) {
    return *overBudget;
  }
  if (auto overBudget = requireBudget("provenance payload",
                                      provenanceBudgetBytes(provenance),
                                      budget_.maxProvenanceBytes,
                                      provenance)) {
    return *overBudget;
  }
  return makeFault(CompileTimeEvaluationResultKind::UnsatisfiedPredicate,
                   CompileTimeEvaluationFaultKind::UnsatisfiedPredicate,
                   std::move(provenance),
                   std::move(message));
}

CompileTimeEvaluationResult CompileTimeEvaluationFacade::invalidEvaluation(
    CompileTimeEvaluationProvenance provenance,
    std::string message) const {
  if (auto overBudget = requireBudget("diagnostic payload",
                                      byteSize(message),
                                      budget_.maxDiagnosticBytes,
                                      provenance)) {
    return *overBudget;
  }
  if (auto overBudget = requireBudget("provenance payload",
                                      provenanceBudgetBytes(provenance),
                                      budget_.maxProvenanceBytes,
                                      provenance)) {
    return *overBudget;
  }
  return makeFault(CompileTimeEvaluationResultKind::InvalidEvaluation,
                   CompileTimeEvaluationFaultKind::InvalidEvaluation,
                   std::move(provenance),
                   std::move(message));
}

CompileTimeEvaluationResult
CompileTimeEvaluationFacade::deniedEffect(
    std::string_view effectName,
    CompileTimeEvaluationProvenance provenance) const {
  std::string message = "denied compile-time effect: " + std::string(effectName);
  if (auto overBudget = requireBudget("diagnostic payload",
                                      byteSize(message),
                                      budget_.maxDiagnosticBytes,
                                      provenance)) {
    return *overBudget;
  }
  if (auto overBudget = requireBudget("provenance payload",
                                      provenanceBudgetBytes(provenance),
                                      budget_.maxProvenanceBytes,
                                      provenance)) {
    return *overBudget;
  }
  return makeFault(CompileTimeEvaluationResultKind::DeniedEffect,
                   CompileTimeEvaluationFaultKind::DeniedEffect,
                   std::move(provenance),
                   std::move(message));
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
  if (auto overBudget = requireBudget("diagnostic payload",
                                      byteSize(message),
                                      budget_.maxDiagnosticBytes,
                                      provenance)) {
    return *overBudget;
  }
  if (auto overBudget = requireBudget("provenance payload",
                                      provenanceBudgetBytes(provenance),
                                      budget_.maxProvenanceBytes,
                                      provenance)) {
    return *overBudget;
  }
  return makeFault(CompileTimeEvaluationResultKind::InternalCompilerError,
                   CompileTimeEvaluationFaultKind::InternalCompilerError,
                   std::move(provenance),
                   std::move(message));
}

std::optional<CompileTimeEvaluationResult>
CompileTimeEvaluationFacade::requireBudget(
    std::string_view budgetName,
    std::uint64_t used,
    std::uint64_t limit,
    CompileTimeEvaluationProvenance provenance) const {
  if (used <= limit) {
    return std::nullopt;
  }
  return budgetExhausted(std::move(provenance),
                         formatBudgetExhaustedMessage(budgetName, used, limit));
}

CompileTimeEvaluationResult CompileTimeEvaluationFacade::requireEffect(
    std::string_view effectName,
    CompileTimeEvaluationProvenance provenance) const {
  if (auto overBudget = requireBudget("frame",
                                      1,
                                      budget_.maxFrames,
                                      provenance)) {
    return *overBudget;
  }
  if (auto overBudget = requireBudget("evaluator step",
                                      1,
                                      budget_.maxSteps,
                                      provenance)) {
    return *overBudget;
  }
  if (auto overBudget = requireBudget("host byte",
                                      byteSize(effectName),
                                      budget_.maxHostBytes,
                                      provenance)) {
    return *overBudget;
  }
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
  const CompileTimeEvaluationBudget defaultBudget;
  const CompileTimeEvaluationBudget &activeBudget =
      budgetEquals(request.budget, defaultBudget) ? budget_ : request.budget;
  const CompileTimeEvaluationFacade activeFacade(host_, activeBudget);
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
    return activeFacade.internalCompilerError(
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

  const std::uint64_t requiredSteps =
      static_cast<std::uint64_t>(fact.operands.size()) + 1;
  if (auto overBudget = activeFacade.requireBudget("frame",
                                                   1,
                                                   activeBudget.maxFrames,
                                                   provenance)) {
    return *overBudget;
  }
  if (auto overBudget = activeFacade.requireBudget("evaluator step",
                                                   requiredSteps,
                                                   activeBudget.maxSteps,
                                                   provenance)) {
    return *overBudget;
  }
  if (auto overBudget = activeFacade.requireBudget(
          "user predicate call",
          isUserPredicatePath(provenance.predicatePath) ? 1 : 0,
          activeBudget.maxUserPredicateCalls,
          provenance)) {
    return *overBudget;
  }
  if (auto overBudget = activeFacade.requireBudget(
          "value storage",
          requirementValueBudgetBytes(semanticProgram, fact),
          activeBudget.maxValueBytes,
          provenance)) {
    return *overBudget;
  }
  if (auto overBudget = activeFacade.requireBudget(
          "storage byte",
          requirementStorageBudgetBytes(semanticProgram, fact),
          activeBudget.maxStorageBytes,
          provenance)) {
    return *overBudget;
  }
  if (auto overBudget = activeFacade.requireBudget(
          "host byte",
          requirementHostBudgetBytes(semanticProgram, fact),
          activeBudget.maxHostBytes,
          provenance)) {
    return *overBudget;
  }

  const std::string_view outcome = resolvedSemanticText(
      semanticProgram, fact.evaluationOutcomeId, fact.evaluationOutcome);
  const std::string_view diagnostic = resolvedSemanticText(
      semanticProgram, fact.evaluationDiagnosticId, fact.evaluationDiagnostic);
  const CompileTimeEvaluationResultKind kind =
      resultKindFromRequirementOutcome(outcome);
  if (kind == CompileTimeEvaluationResultKind::Success) {
    return activeFacade.success(true,
                                std::move(provenance),
                                std::string(diagnostic));
  }
  if (kind == CompileTimeEvaluationResultKind::UnsatisfiedPredicate) {
    return activeFacade.unsatisfiedPredicate(std::move(provenance),
                                             std::string(diagnostic));
  }
  if (kind == CompileTimeEvaluationResultKind::InvalidEvaluation) {
    return activeFacade.invalidEvaluation(std::move(provenance),
                                          std::string(diagnostic));
  }
  if (kind == CompileTimeEvaluationResultKind::DeniedEffect) {
    return makeFault(kind,
                     faultKindFromResultKind(kind),
                     std::move(provenance),
                     std::string(diagnostic));
  }
  if (kind == CompileTimeEvaluationResultKind::BudgetExhausted) {
    return activeFacade.budgetExhausted(std::move(provenance),
                                        std::string(diagnostic));
  }
  return activeFacade.internalCompilerError(std::move(provenance),
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
