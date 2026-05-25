#include "primec/CompileTimeEvaluation.h"

#include "primec/SemanticProduct.h"
#include "primec/VmKernelBoundary.h"

#include <algorithm>
#include <charconv>
#include <iomanip>
#include <optional>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

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
  if (outcome == "cache_corrupt_or_version_mismatch") {
    return CompileTimeEvaluationResultKind::CacheCorruptOrVersionMismatch;
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
  case CompileTimeEvaluationResultKind::CacheCorruptOrVersionMismatch:
    return CompileTimeEvaluationFaultKind::CacheCorruptOrVersionMismatch;
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

bool isValuePredicatePath(std::string_view predicatePath) {
  return predicatePath == "/std/meta/value_equals" ||
         predicatePath == "/std/meta/value_not_equals" ||
         predicatePath == "/std/meta/value_less" ||
         predicatePath == "/std/meta/value_less_equal" ||
         predicatePath == "/std/meta/value_greater" ||
         predicatePath == "/std/meta/value_greater_equal";
}

std::optional<IrOpcode> valuePredicateKernelOpcode(
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

std::string_view valuePredicateOperator(std::string_view predicatePath) {
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

std::optional<std::uint64_t> parseKernelValueOperand(std::string_view text) {
  std::uint64_t value = 0;
  const char *begin = text.data();
  const char *end = text.data() + text.size();
  const auto [ptr, error] = std::from_chars(begin, end, value);
  if (error != std::errc{} || ptr != end) {
    return std::nullopt;
  }
  return value;
}

std::optional<CompileTimeEvaluationResult> evaluateValuePredicateWithKernel(
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

constexpr std::string_view DefaultEvaluatorPolicyVersion =
    "primestruct-ct-evaluator-v1";
constexpr std::string_view CompileTimeCacheMaterialVersion =
    "primestruct-ct-cache-key-v1";

std::string evaluatorPolicyText(std::string_view evaluatorPolicyVersion) {
  return evaluatorPolicyVersion.empty()
             ? std::string(DefaultEvaluatorPolicyVersion)
             : std::string(evaluatorPolicyVersion);
}

void appendCacheField(std::string &material,
                      std::string_view key,
                      std::string_view value) {
  material.append(key);
  material.push_back('=');
  material.append(std::to_string(value.size()));
  material.push_back(':');
  material.append(value);
  material.push_back('\n');
}

void appendCacheField(std::string &material,
                      std::string_view key,
                      std::uint64_t value) {
  appendCacheField(material, key, std::to_string(value));
}

std::string hex64(std::uint64_t value) {
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(16) << value;
  return out.str();
}

std::string fnv1a64Hex(std::string_view text) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const unsigned char ch : text) {
    hash ^= ch;
    hash *= 1099511628211ull;
  }
  return hex64(hash);
}

std::vector<std::string> sortedUniqueStrings(std::vector<std::string> values) {
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
  return values;
}

void appendBudgetMaterial(std::string &material,
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

std::string requirementFactCacheMaterial(
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

} // namespace

CompileTimeEvaluationCacheKey buildCompileTimeEvaluationCacheKey(
    const CompileTimeHost &host,
    const SemanticProgram *semanticProgram,
    const SemanticProgramRequirementPredicateFact &fact,
    const CompileTimeEvaluationBudget &budget,
    std::string_view evaluatorPolicyVersion) {
  std::string material;
  appendCacheField(material,
                   "cache.materialVersion",
                   CompileTimeCacheMaterialVersion);
  appendCacheField(material,
                   "language.version",
                   "primestruct-language-v1");
  appendCacheField(material,
                   "semanticProduct.contractVersion",
                   semanticProgram == nullptr
                       ? SemanticProductContractVersionCurrent
                       : semanticProgram->contractVersion);
  appendCacheField(material,
                   "evaluator.policyVersion",
                   evaluatorPolicyText(evaluatorPolicyVersion));
  appendBudgetMaterial(material, budget);

  if (semanticProgram != nullptr) {
    std::vector<std::string> visibleImports;
    visibleImports.reserve(semanticProgram->sourceImports.size() +
                           semanticProgram->imports.size());
    for (const auto &importPath : semanticProgram->sourceImports) {
      visibleImports.push_back("source:" + importPath);
    }
    for (const auto &importPath : semanticProgram->imports) {
      visibleImports.push_back("import:" + importPath);
    }
    for (const auto &importPath :
         sortedUniqueStrings(std::move(visibleImports))) {
      appendCacheField(material, "visibleImport", importPath);
    }

    std::vector<std::string> requirementFactMaterials;
    requirementFactMaterials.reserve(
        semanticProgramRequirementPredicateFactView(*semanticProgram).size());
    for (const auto *entry :
         semanticProgramRequirementPredicateFactView(*semanticProgram)) {
      if (entry != nullptr) {
        requirementFactMaterials.push_back(
            requirementFactCacheMaterial(semanticProgram, *entry, "fact"));
      }
    }
    std::sort(requirementFactMaterials.begin(),
              requirementFactMaterials.end());
    for (const auto &factMaterial : requirementFactMaterials) {
      appendCacheField(material, "semanticRequirementFact", factMaterial);
    }
  }

  material.append(requirementFactCacheMaterial(semanticProgram, fact, "request"));

  std::vector<std::string> activeEffects;
  for (std::size_t i = 0; i < fact.compileTimeEffects.size(); ++i) {
    const SymbolId effectId =
        i < fact.compileTimeEffectIds.size()
            ? fact.compileTimeEffectIds[i]
            : InvalidSymbolId;
    activeEffects.emplace_back(resolvedSemanticText(semanticProgram,
                                                    effectId,
                                                    fact.compileTimeEffects[i]));
  }
  for (const auto &effect : sortedUniqueStrings(std::move(activeEffects))) {
    appendCacheField(material, "activeCompileTimeEffect", effect);
    appendCacheField(material,
                     "hostServiceFingerprint." + effect,
                     host.hostServiceFingerprint(effect).value_or(
                         std::string("<unavailable>")));
  }

  CompileTimeEvaluationCacheKey key;
  key.material = std::move(material);
  key.digest = fnv1a64Hex(key.material);
  return key;
}

std::optional<std::string>
CompileTimeHost::describeSemanticFact(std::string_view factName) const {
  (void)factName;
  return std::nullopt;
}

std::optional<std::string>
CompileTimeHost::hostServiceFingerprint(std::string_view serviceName) const {
  (void)serviceName;
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
    CompileTimeEffectPhase phase,
    std::string_view definitionPath) const {
  if (phase != CompileTimeEffectPhase::SemanticRequirement ||
      definitionPath.empty()) {
    return false;
  }
  for (const auto *fact :
       semanticProgramRequirementPredicateFactView(semanticProgram_)) {
    if (fact == nullptr) {
      continue;
    }
    if (resolvedRequirementFactText(semanticProgram_,
                                    fact->definitionPathId,
                                    fact->definitionPath) != definitionPath) {
      continue;
    }
    for (std::size_t i = 0; i < fact->compileTimeEffects.size(); ++i) {
      const SymbolId effectId =
          i < fact->compileTimeEffectIds.size()
              ? fact->compileTimeEffectIds[i]
              : InvalidSymbolId;
      if (resolvedRequirementFactText(semanticProgram_,
                                      effectId,
                                      fact->compileTimeEffects[i]) ==
          effectName) {
        return true;
      }
    }
  }
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
                                         CompileTimeEffectPhase phase,
                                         std::string_view definitionPath) const {
  (void)effectName;
  (void)phase;
  (void)definitionPath;
  return false;
}

CompileTimeEvaluationFacade::CompileTimeEvaluationFacade(
    const CompileTimeHost &host,
    CompileTimeEvaluationBudget budget,
    CompileTimeEvaluationCache *cache)
    : host_(host),
      budget_(budget),
      cache_(cache) {}

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
CompileTimeEvaluationFacade::cacheCorruptOrVersionMismatch(
    CompileTimeEvaluationProvenance provenance,
    std::string message) const {
  return makeFault(CompileTimeEvaluationResultKind::CacheCorruptOrVersionMismatch,
                   CompileTimeEvaluationFaultKind::CacheCorruptOrVersionMismatch,
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
  if (!host_.allowEffect(effectName,
                         CompileTimeEffectPhase::SemanticRequirement,
                         provenance.definitionPath)) {
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
  const CompileTimeEvaluationFacade activeFacade(host_, activeBudget, cache_);
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

  const std::string evaluatorPolicyVersion =
      evaluatorPolicyText(request.evaluatorPolicyVersion);
  std::optional<CompileTimeEvaluationCacheKey> cacheKey;
  const bool useCache = cache_ != nullptr && cache_->enabled &&
                        request.enableCacheReuse;
  if (useCache) {
    cacheKey = buildCompileTimeEvaluationCacheKey(host_,
                                                  semanticProgram,
                                                  fact,
                                                  activeBudget,
                                                  evaluatorPolicyVersion);
    const auto cached = cache_->entries.find(cacheKey->digest);
    if (cached != cache_->entries.end()) {
      const CompileTimeEvaluationCacheEntry &entry = cached->second;
      if (entry.key.material != cacheKey->material ||
          entry.semanticProductContractVersion !=
              (semanticProgram == nullptr
                   ? SemanticProductContractVersionCurrent
                   : semanticProgram->contractVersion) ||
          entry.evaluatorPolicyVersion != evaluatorPolicyVersion) {
        ++cache_->corruptOrVersionMismatchCount;
        return activeFacade.cacheCorruptOrVersionMismatch(
            provenance,
            "compile-time evaluation cache corrupt or version mismatch");
      }
      ++cache_->hitCount;
      return entry.result;
    }
    ++cache_->missCount;
  }

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

  if (auto kernelResult =
          evaluateValuePredicateWithKernel(activeFacade,
                                           semanticProgram,
                                           fact,
                                           provenance)) {
    return *kernelResult;
  }

  const std::string_view outcome = resolvedSemanticText(
      semanticProgram, fact.evaluationOutcomeId, fact.evaluationOutcome);
  const std::string_view diagnostic = resolvedSemanticText(
      semanticProgram, fact.evaluationDiagnosticId, fact.evaluationDiagnostic);
  const CompileTimeEvaluationResultKind kind =
      resultKindFromRequirementOutcome(outcome);
  CompileTimeEvaluationResult result;
  if (kind == CompileTimeEvaluationResultKind::Success) {
    result = activeFacade.success(true,
                                  std::move(provenance),
                                  std::string(diagnostic));
  } else if (kind == CompileTimeEvaluationResultKind::UnsatisfiedPredicate) {
    result = activeFacade.unsatisfiedPredicate(std::move(provenance),
                                               std::string(diagnostic));
  } else if (kind == CompileTimeEvaluationResultKind::InvalidEvaluation) {
    result = activeFacade.invalidEvaluation(std::move(provenance),
                                            std::string(diagnostic));
  } else if (kind == CompileTimeEvaluationResultKind::DeniedEffect ||
             kind == CompileTimeEvaluationResultKind::CacheCorruptOrVersionMismatch) {
    result = makeFault(kind,
                       faultKindFromResultKind(kind),
                       std::move(provenance),
                       std::string(diagnostic));
  } else if (kind == CompileTimeEvaluationResultKind::BudgetExhausted) {
    result = activeFacade.budgetExhausted(std::move(provenance),
                                          std::string(diagnostic));
  } else {
    result = activeFacade.internalCompilerError(std::move(provenance),
                                                std::string(diagnostic));
  }
  if (useCache && cacheKey.has_value()) {
    CompileTimeEvaluationCacheEntry entry;
    entry.key = *cacheKey;
    entry.result = result;
    entry.semanticProductContractVersion =
        semanticProgram == nullptr ? SemanticProductContractVersionCurrent
                                   : semanticProgram->contractVersion;
    entry.evaluatorPolicyVersion = evaluatorPolicyVersion;
    cache_->entries[cacheKey->digest] = std::move(entry);
    ++cache_->storeCount;
  }
  return result;
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
  case CompileTimeEvaluationResultKind::CacheCorruptOrVersionMismatch:
    return "cache_corrupt_or_version_mismatch";
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
  case CompileTimeEvaluationFaultKind::CacheCorruptOrVersionMismatch:
    return "cache_corrupt_or_version_mismatch";
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
