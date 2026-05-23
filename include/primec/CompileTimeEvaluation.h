#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace primec {

struct SemanticProgram;
struct SemanticProgramRequirementPredicateFact;

enum class CompileTimeEvaluationPhase {
  SemanticRequirementEvaluation,
};

enum class CompileTimeEvaluationResultKind {
  Success,
  UnsatisfiedPredicate,
  InvalidEvaluation,
  DeniedEffect,
  BudgetExhausted,
  CacheCorruptOrVersionMismatch,
  InternalCompilerError,
};

enum class CompileTimeEvaluationFaultKind {
  None,
  UnsatisfiedPredicate,
  InvalidEvaluation,
  DeniedEffect,
  BudgetExhausted,
  CacheCorruptOrVersionMismatch,
  InternalCompilerError,
};

enum class CompileTimeEffectPhase {
  SemanticRequirement,
};

struct CompileTimeEvaluationProvenance {
  CompileTimeEvaluationPhase phase =
      CompileTimeEvaluationPhase::SemanticRequirementEvaluation;
  std::string definitionPath;
  std::string predicatePath;
  std::string sourcePath;
  std::string sourceText;
  std::uint32_t line = 0;
  std::uint32_t column = 0;
  std::uint64_t semanticNodeId = 0;
  std::uint64_t provenanceHandle = 0;
};

struct CompileTimeEvaluationBudget {
  std::uint64_t maxPreparationSteps = 10000;
  std::uint64_t maxSteps = 10000;
  std::uint64_t maxFrames = 128;
  std::uint64_t maxUserPredicateCalls = 128;
  std::uint64_t maxValueBytes = 1048576;
  std::uint64_t maxStorageBytes = 1048576;
  std::uint64_t maxHostBytes = 1048576;
  std::uint64_t maxDiagnosticBytes = 65536;
  std::uint64_t maxProvenanceBytes = 65536;
};

struct CompileTimeEvaluationRequest {
  const SemanticProgram *semanticProgram = nullptr;
  const SemanticProgramRequirementPredicateFact *requirementPredicate = nullptr;
  std::string definitionPath;
  std::string predicateName;
  std::string sourceText;
  std::string evaluatorPolicyVersion;
  bool enableCacheReuse = true;
  CompileTimeEvaluationProvenance provenance;
  CompileTimeEvaluationBudget budget;
};

class CompileTimeHost {
public:
  virtual ~CompileTimeHost() = default;

  virtual bool allowEffect(std::string_view effectName,
                           CompileTimeEffectPhase phase,
                           std::string_view definitionPath = {}) const = 0;

  virtual std::optional<std::string>
  describeSemanticFact(std::string_view factName) const;

  virtual std::optional<std::string>
  hostServiceFingerprint(std::string_view serviceName) const;

  virtual const SemanticProgramRequirementPredicateFact *
  findRequirementPredicateFact(std::string_view definitionPath,
                               std::string_view predicateName,
                               std::string_view sourceText = {}) const;

  virtual const SemanticProgram *semanticProgram() const;
};

class SemanticProgramCompileTimeHost final : public CompileTimeHost {
public:
  explicit SemanticProgramCompileTimeHost(const SemanticProgram &semanticProgram);

  bool allowEffect(std::string_view effectName,
                   CompileTimeEffectPhase phase,
                   std::string_view definitionPath = {}) const override;

  std::optional<std::string>
  describeSemanticFact(std::string_view factName) const override;

  const SemanticProgramRequirementPredicateFact *
  findRequirementPredicateFact(std::string_view definitionPath,
                               std::string_view predicateName,
                               std::string_view sourceText = {}) const override;

  const SemanticProgram *semanticProgram() const override;

private:
  const SemanticProgram &semanticProgram_;
};

class DenyAllCompileTimeHost final : public CompileTimeHost {
public:
  bool allowEffect(std::string_view effectName,
                   CompileTimeEffectPhase phase,
                   std::string_view definitionPath = {}) const override;
};

struct CompileTimeEvaluationResult {
  CompileTimeEvaluationResultKind kind =
      CompileTimeEvaluationResultKind::InvalidEvaluation;
  CompileTimeEvaluationFaultKind fault = CompileTimeEvaluationFaultKind::InvalidEvaluation;
  CompileTimeEvaluationProvenance provenance;
  std::string message;
  bool boolValue = false;
};

struct CompileTimeEvaluationCacheKey {
  std::string digest;
  std::string material;
};

struct CompileTimeEvaluationCacheEntry {
  CompileTimeEvaluationCacheKey key;
  CompileTimeEvaluationResult result;
  uint32_t semanticProductContractVersion = 0;
  std::string evaluatorPolicyVersion;
};

struct CompileTimeEvaluationCache {
  std::unordered_map<std::string, CompileTimeEvaluationCacheEntry> entries;
  bool enabled = true;
  std::uint64_t hitCount = 0;
  std::uint64_t missCount = 0;
  std::uint64_t storeCount = 0;
  std::uint64_t corruptOrVersionMismatchCount = 0;

  void clear() {
    entries.clear();
    hitCount = 0;
    missCount = 0;
    storeCount = 0;
    corruptOrVersionMismatchCount = 0;
  }
};

CompileTimeEvaluationCacheKey buildCompileTimeEvaluationCacheKey(
    const CompileTimeHost &host,
    const SemanticProgram *semanticProgram,
    const SemanticProgramRequirementPredicateFact &fact,
    const CompileTimeEvaluationBudget &budget,
    std::string_view evaluatorPolicyVersion);

class CompileTimeEvaluationFacade {
public:
  CompileTimeEvaluationFacade(const CompileTimeHost &host,
                              CompileTimeEvaluationBudget budget = {},
                              CompileTimeEvaluationCache *cache = nullptr);

  static constexpr bool finalBackendIrAvailable() { return false; }
  static constexpr bool launchesRuntimeVm() { return false; }

  const CompileTimeEvaluationBudget &budget() const;

  CompileTimeEvaluationResult success(bool value,
                                      CompileTimeEvaluationProvenance provenance,
                                      std::string message = {}) const;
  CompileTimeEvaluationResult unsatisfiedPredicate(
      CompileTimeEvaluationProvenance provenance,
      std::string message) const;
  CompileTimeEvaluationResult invalidEvaluation(
      CompileTimeEvaluationProvenance provenance,
      std::string message) const;
  CompileTimeEvaluationResult deniedEffect(std::string_view effectName,
                                           CompileTimeEvaluationProvenance provenance) const;
  CompileTimeEvaluationResult budgetExhausted(
      CompileTimeEvaluationProvenance provenance,
      std::string message) const;
  CompileTimeEvaluationResult cacheCorruptOrVersionMismatch(
      CompileTimeEvaluationProvenance provenance,
      std::string message) const;
  CompileTimeEvaluationResult internalCompilerError(
      CompileTimeEvaluationProvenance provenance,
      std::string message) const;
  std::optional<CompileTimeEvaluationResult> requireBudget(
      std::string_view budgetName,
      std::uint64_t used,
      std::uint64_t limit,
      CompileTimeEvaluationProvenance provenance) const;

  CompileTimeEvaluationResult requireEffect(
      std::string_view effectName,
      CompileTimeEvaluationProvenance provenance) const;
  CompileTimeEvaluationResult evaluateRequirementPredicate(
      const CompileTimeEvaluationRequest &request) const;

private:
  const CompileTimeHost &host_;
  CompileTimeEvaluationBudget budget_;
  CompileTimeEvaluationCache *cache_ = nullptr;
};

bool isCompileTimeEvaluationFault(CompileTimeEvaluationResultKind kind);
std::string_view compileTimeEvaluationPhaseName(
    CompileTimeEvaluationPhase phase);
std::string_view
compileTimeEvaluationResultKindName(CompileTimeEvaluationResultKind kind);
std::string_view
compileTimeEvaluationFaultKindName(CompileTimeEvaluationFaultKind kind);
std::string formatCompileTimeEvaluationProvenance(
    const CompileTimeEvaluationProvenance &provenance);
std::string formatCompileTimeEvaluationResult(
    const CompileTimeEvaluationResult &result);
CompileTimeEvaluationProvenance compileTimeEvaluationProvenanceFromRequirement(
    const SemanticProgramRequirementPredicateFact &fact);

} // namespace primec
