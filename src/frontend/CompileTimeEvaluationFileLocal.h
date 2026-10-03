#pragma once

// Helpers shared by the CompileTimeEvaluation*.cpp units (split out of
// CompileTimeEvaluation.cpp without changes).
#include "primec/frontend/CompileTimeEvaluation.h"
#include "primec/frontend/SemanticProduct.h"
#include "primec/runtime/VmKernelBoundary.h"
#include <algorithm>
#include <charconv>
#include <iomanip>
#include <optional>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

namespace primec {

namespace compile_time_evaluation_file_local {

inline std::string_view resolvedSemanticText(
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

std::vector<const SemanticProgramRequirementPredicateFact *>
strictRequirementPredicateFactView(const SemanticProgram &semanticProgram) {
  std::vector<const SemanticProgramRequirementPredicateFact *> entries;
  if (!semanticProgram.moduleResolvedArtifacts.empty()) {
    size_t moduleEntryCount = 0;
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      moduleEntryCount += module.requirementPredicateFactIndices.size();
    }
    entries.reserve(moduleEntryCount);
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      for (const std::size_t entryIndex :
           module.requirementPredicateFactIndices) {
        if (entryIndex < semanticProgram.requirementPredicateFacts.size()) {
          entries.push_back(
              &semanticProgram.requirementPredicateFacts[entryIndex]);
        }
      }
    }
    return entries;
  }

  entries.reserve(semanticProgram.requirementPredicateFacts.size());
  for (const auto &entry : semanticProgram.requirementPredicateFacts) {
    entries.push_back(&entry);
  }
  return entries;
}

inline bool isStrictRequirementPredicateFact(
    const SemanticProgram &semanticProgram,
    const SemanticProgramRequirementPredicateFact &fact) {
  for (const auto *publishedFact :
       strictRequirementPredicateFactView(semanticProgram)) {
    if (publishedFact == &fact) {
      return true;
    }
  }
  return false;
}

inline CompileTimeEvaluationResultKind resultKindFromRequirementOutcome(
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
  if (outcome == "cache_corrupt_or_version_mismatch") {
    return CompileTimeEvaluationResultKind::CacheCorruptOrVersionMismatch;
  }
  if (outcome == "internal_compiler_error") {
    return CompileTimeEvaluationResultKind::InternalCompilerError;
  }
  return CompileTimeEvaluationResultKind::InvalidEvaluation;
}

inline CompileTimeEvaluationFaultKind faultKindFromResultKind(
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
  case CompileTimeEvaluationResultKind::CacheCorruptOrVersionMismatch:
    return CompileTimeEvaluationFaultKind::CacheCorruptOrVersionMismatch;
  case CompileTimeEvaluationResultKind::InternalCompilerError:
    return CompileTimeEvaluationFaultKind::InternalCompilerError;
  }
  return CompileTimeEvaluationFaultKind::InternalCompilerError;
}

inline CompileTimeEvaluationResult makeFault(CompileTimeEvaluationResultKind kind,
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

inline std::string formatBudgetExhaustedMessage(std::string_view budgetName,
                                         std::uint64_t used,
                                         std::uint64_t limit) {
  std::ostringstream out;
  out << "compile-time " << budgetName << " budget exceeded";
  out << " (used " << used << ", limit " << limit << ')';
  return out.str();
}

inline std::uint64_t byteSize(std::string_view text) {
  return static_cast<std::uint64_t>(text.size());
}

inline std::uint64_t addBudgetBytes(std::uint64_t left, std::uint64_t right) {
  constexpr std::uint64_t Max = UINT64_MAX;
  if (Max - left < right) {
    return Max;
  }
  return left + right;
}

inline std::uint64_t provenanceBudgetBytes(
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

inline std::uint64_t requirementValueBudgetBytes(
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

inline std::uint64_t requirementStorageBudgetBytes(
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

inline std::uint64_t requirementHostBudgetBytes(
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

inline bool isUserPredicatePath(std::string_view predicatePath) {
  return !predicatePath.empty() &&
         predicatePath.rfind("/std/meta/", 0) != 0;
}

inline bool isValuePredicatePath(std::string_view predicatePath) {
  return predicatePath == "/std/meta/value_equals" ||
         predicatePath == "/std/meta/value_not_equals" ||
         predicatePath == "/std/meta/value_less" ||
         predicatePath == "/std/meta/value_less_equal" ||
         predicatePath == "/std/meta/value_greater" ||
         predicatePath == "/std/meta/value_greater_equal";
}

inline std::optional<IrOpcode> valuePredicateKernelOpcode(
    std::string_view predicatePath) {
  if (predicatePath == "/std/meta/value_equals") {
    return IrOpcode::CmpEqI64;
  }
  if (predicatePath == "/std/meta/value_not_equals") {
    return IrOpcode::CmpNeI64;
  }
  if (predicatePath == "/std/meta/value_less") {
    return IrOpcode::CmpLtU64;
  }
  if (predicatePath == "/std/meta/value_less_equal") {
    return IrOpcode::CmpLeU64;
  }
  if (predicatePath == "/std/meta/value_greater") {
    return IrOpcode::CmpGtU64;
  }
  if (predicatePath == "/std/meta/value_greater_equal") {
    return IrOpcode::CmpGeU64;
  }
  return std::nullopt;
}

inline std::string_view valuePredicateOperator(std::string_view predicatePath) {
  if (predicatePath == "/std/meta/value_equals") {
    return "==";
  }
  if (predicatePath == "/std/meta/value_not_equals") {
    return "!=";
  }
  if (predicatePath == "/std/meta/value_less") {
    return "<";
  }
  if (predicatePath == "/std/meta/value_less_equal") {
    return "<=";
  }
  if (predicatePath == "/std/meta/value_greater") {
    return ">";
  }
  if (predicatePath == "/std/meta/value_greater_equal") {
    return ">=";
  }
  return "?";
}

inline std::optional<std::uint64_t> parseKernelValueOperand(std::string_view text) {
  std::uint64_t value = 0;
  const char *begin = text.data();
  const char *end = text.data() + text.size();
  const auto [ptr, error] = std::from_chars(begin, end, value);
  if (error != std::errc{} || ptr != end) {
    return std::nullopt;
  }
  return value;
}

inline std::optional<CompileTimeEvaluationResult> evaluateValuePredicateWithKernel(
    const CompileTimeEvaluationFacade &facade,
    const SemanticProgram *semanticProgram,
    const SemanticProgramRequirementPredicateFact &fact,
    const CompileTimeEvaluationProvenance &provenance) {
  if (!isValuePredicatePath(provenance.predicatePath)) {
    return std::nullopt;
  }
  const std::optional<IrOpcode> opcode =
      valuePredicateKernelOpcode(provenance.predicatePath);
  if (!opcode.has_value()) {
    return facade.internalCompilerError(
        provenance,
        "missing VM kernel opcode for value predicate: " +
            provenance.predicatePath);
  }
  if (fact.operands.size() != 2) {
    return facade.invalidEvaluation(
        provenance,
        "requirement predicate " + provenance.predicatePath +
            " expects two value operands");
  }

  std::vector<std::uint64_t> stack;
  stack.reserve(2);
  std::vector<std::uint64_t> parsedOperands;
  parsedOperands.reserve(2);
  for (const SemanticProgramRequirementPredicateOperand &operand :
       fact.operands) {
    const std::string_view kind =
        resolvedSemanticText(semanticProgram, operand.kindId, operand.kind);
    const std::string_view text =
        resolvedSemanticText(semanticProgram, operand.textId, operand.text);
    if (kind != "literal_compile_time_argument") {
      return facade.invalidEvaluation(
          provenance,
          "non-constant value operand for requirement predicate " +
              provenance.predicatePath + ": " + std::string(text));
    }
    const std::optional<std::uint64_t> parsed =
        parseKernelValueOperand(text);
    if (!parsed.has_value()) {
      return facade.invalidEvaluation(
          provenance,
          "unsupported value operand for requirement predicate " +
              provenance.predicatePath + ": " + std::string(text));
    }
    stack.push_back(*parsed);
    parsedOperands.push_back(*parsed);
  }

  IrInstruction inst;
  inst.op = *opcode;
  std::string error;
  const vm_kernel::PureOpcodeResult kernelResult =
      vm_kernel::executePureNumericOpcode(inst, stack, error);
  if (kernelResult == vm_kernel::PureOpcodeResult::NotHandled) {
    return facade.internalCompilerError(
        provenance,
        "VM kernel did not handle value predicate opcode");
  }
  if (kernelResult == vm_kernel::PureOpcodeResult::Fault) {
    return facade.invalidEvaluation(
        provenance,
        error.empty() ? "VM kernel value predicate evaluation failed"
                      : std::move(error));
  }
  if (stack.empty()) {
    return facade.internalCompilerError(
        provenance,
        "VM kernel value predicate produced no result");
  }

  const bool satisfied = stack.back() != 0;
  std::string message =
      std::string("value predicate ") +
      (satisfied ? "satisfied: " : "failed: ") +
      std::to_string(parsedOperands[0]) + " " +
      std::string(valuePredicateOperator(provenance.predicatePath)) + " " +
      std::to_string(parsedOperands[1]);
  if (satisfied) {
    return facade.success(true, provenance, std::move(message));
  }
  return facade.unsatisfiedPredicate(provenance, std::move(message));
}

inline bool budgetEquals(const CompileTimeEvaluationBudget &left,
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

constexpr std::string_view DefaultEvaluatorPolicyVersion =
    "primestruct-ct-evaluator-v1";
constexpr std::string_view CompileTimeCacheMaterialVersion =
    "primestruct-ct-cache-key-v1";

inline std::string evaluatorPolicyText(std::string_view evaluatorPolicyVersion) {
  return evaluatorPolicyVersion.empty()
             ? std::string(DefaultEvaluatorPolicyVersion)
             : std::string(evaluatorPolicyVersion);
}

inline void appendCacheField(std::string &material,
                      std::string_view key,
                      std::string_view value) {
  material.append(key);
  material.push_back('=');
  material.append(std::to_string(value.size()));
  material.push_back(':');
  material.append(value);
  material.push_back('\n');
}

inline void appendCacheField(std::string &material,
                      std::string_view key,
                      std::uint64_t value) {
  appendCacheField(material, key, std::to_string(value));
}

inline std::string hex64(std::uint64_t value) {
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(16) << value;
  return out.str();
}

inline std::string fnv1a64Hex(std::string_view text) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const unsigned char ch : text) {
    hash ^= ch;
    hash *= 1099511628211ull;
  }
  return hex64(hash);
}

inline std::vector<std::string> sortedUniqueStrings(std::vector<std::string> values) {
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
  return values;
}

inline void appendBudgetMaterial(std::string &material,
                          const CompileTimeEvaluationBudget &budget) {
  appendCacheField(material, "budget.maxPreparationSteps",
                   budget.maxPreparationSteps);
  appendCacheField(material, "budget.maxSteps", budget.maxSteps);
  appendCacheField(material, "budget.maxFrames", budget.maxFrames);
  appendCacheField(material, "budget.maxUserPredicateCalls",
                   budget.maxUserPredicateCalls);
  appendCacheField(material, "budget.maxValueBytes", budget.maxValueBytes);
  appendCacheField(material, "budget.maxStorageBytes", budget.maxStorageBytes);
  appendCacheField(material, "budget.maxHostBytes", budget.maxHostBytes);
  appendCacheField(material, "budget.maxDiagnosticBytes",
                   budget.maxDiagnosticBytes);
  appendCacheField(material, "budget.maxProvenanceBytes",
                   budget.maxProvenanceBytes);
}

inline std::string requirementFactCacheMaterial(
    const SemanticProgram *semanticProgram,
    const SemanticProgramRequirementPredicateFact &fact,
    std::string_view prefix) {
  std::string material;
  appendCacheField(material,
                   std::string(prefix) + ".definitionPath",
                   resolvedSemanticText(semanticProgram,
                                        fact.definitionPathId,
                                        fact.definitionPath));
  appendCacheField(material,
                   std::string(prefix) + ".predicateKind",
                   resolvedSemanticText(semanticProgram,
                                        fact.predicateKindId,
                                        fact.predicateKind));
  appendCacheField(material,
                   std::string(prefix) + ".predicateName",
                   resolvedSemanticText(semanticProgram,
                                        fact.predicateNameId,
                                        fact.predicateName));
  appendCacheField(material,
                   std::string(prefix) + ".relationOperator",
                   resolvedSemanticText(semanticProgram,
                                        fact.relationOperatorId,
                                        fact.relationOperator));
  appendCacheField(material,
                   std::string(prefix) + ".sourceText",
                   resolvedSemanticText(semanticProgram,
                                        fact.sourceTextId,
                                        fact.sourceText));
  appendCacheField(material,
                   std::string(prefix) + ".evaluationOutcome",
                   resolvedSemanticText(semanticProgram,
                                        fact.evaluationOutcomeId,
                                        fact.evaluationOutcome));
  appendCacheField(material,
                   std::string(prefix) + ".evaluationDiagnostic",
                   resolvedSemanticText(semanticProgram,
                                        fact.evaluationDiagnosticId,
                                        fact.evaluationDiagnostic));
  appendCacheField(material,
                   std::string(prefix) + ".semanticNodeId",
                   fact.semanticNodeId);
  appendCacheField(material,
                   std::string(prefix) + ".provenanceHandle",
                   fact.provenanceHandle);
  std::vector<std::string> effects;
  for (std::size_t i = 0; i < fact.compileTimeEffects.size(); ++i) {
    const SymbolId effectId =
        i < fact.compileTimeEffectIds.size()
            ? fact.compileTimeEffectIds[i]
            : InvalidSymbolId;
    effects.emplace_back(resolvedSemanticText(semanticProgram,
                                              effectId,
                                              fact.compileTimeEffects[i]));
  }
  for (const auto &effect : sortedUniqueStrings(std::move(effects))) {
    appendCacheField(material, std::string(prefix) + ".compileTimeEffect",
                     effect);
  }
  for (std::size_t i = 0; i < fact.operands.size(); ++i) {
    const SemanticProgramRequirementPredicateOperand &operand =
        fact.operands[i];
    const std::string operandPrefix =
        std::string(prefix) + ".operand." + std::to_string(i);
    appendCacheField(material,
                     operandPrefix + ".kind",
                     resolvedSemanticText(semanticProgram,
                                          operand.kindId,
                                          operand.kind));
    appendCacheField(material,
                     operandPrefix + ".text",
                     resolvedSemanticText(semanticProgram,
                                          operand.textId,
                                          operand.text));
    appendCacheField(material,
                     operandPrefix + ".stableHandle",
                     resolvedSemanticText(semanticProgram,
                                          operand.stableHandleId,
                                          operand.stableHandle));
  }
  return material;
}

} // namespace compile_time_evaluation_file_local
} // namespace primec
