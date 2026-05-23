#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "primec/CompileTimeEvaluation.h"
#include "primec/CompileTimeValue.h"

namespace primec {

enum class CompileTimeCallablePrepareStatus {
  Prepared,
  UnsupportedPredicate,
  UnsupportedOperand,
  RuntimeOnlyOperation,
  MissingSemanticFact,
  DeniedEffect,
  BudgetExceeded,
};

struct CompileTimeCallableOperand {
  CompileTimeValue value;
  std::string sourceText;
  std::string kind;
};

struct CompileTimePreparedCallable {
  std::string definitionPath;
  std::string predicateName;
  std::string sourceText;
  std::vector<CompileTimeCallableOperand> operands;
  CompileTimeEvaluationProvenance provenance;
  std::uint64_t maxSteps = 0;
  std::uint64_t maxFrames = 0;

  static constexpr bool requiresFinalBackendIr() { return false; }
  static constexpr bool launchesRuntimeVm() { return false; }
};

struct CompileTimeCallablePrepareResult {
  CompileTimeCallablePrepareStatus status =
      CompileTimeCallablePrepareStatus::MissingSemanticFact;
  CompileTimePreparedCallable callable;
  CompileTimeEvaluationResult diagnostic;

  bool prepared() const;
};

struct CompileTimeCallablePrepareRequest {
  std::string definitionPath;
  std::string predicateName;
  std::string sourceText;
  const SemanticProgramRequirementPredicateFact *requirementPredicate = nullptr;
  CompileTimeEvaluationProvenance provenance;
  CompileTimeEvaluationBudget budget;
};

std::string_view compileTimeCallablePrepareStatusName(
    CompileTimeCallablePrepareStatus status);

CompileTimeCallablePrepareResult prepareCompileTimeCallable(
    const CompileTimeHost &host,
    const CompileTimeCallablePrepareRequest &request);

} // namespace primec
