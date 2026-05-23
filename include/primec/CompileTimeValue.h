#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "primec/CompileTimeEvaluation.h"

namespace primec {

enum class CompileTimeValueKind {
  Bool,
  SignedInteger,
  UnsignedInteger,
  StringLiteral,
  TypeFact,
  Symbol,
  RequirementOutcome,
  UnsupportedRuntimeValue,
};

struct CompileTimeValueProvenance {
  std::string sourceText;
  std::string sourcePath;
  std::uint32_t line = 0;
  std::uint32_t column = 0;
  std::uint64_t semanticNodeId = 0;
  std::uint64_t provenanceHandle = 0;
};

struct CompileTimeRequirementOutcomeValue {
  CompileTimeEvaluationResultKind kind =
      CompileTimeEvaluationResultKind::InvalidEvaluation;
  std::string diagnostic;
};

struct CompileTimeValue {
  CompileTimeValueKind kind = CompileTimeValueKind::UnsupportedRuntimeValue;
  std::variant<bool,
               std::int64_t,
               std::uint64_t,
               std::string,
               CompileTimeRequirementOutcomeValue>
      payload = false;
  CompileTimeValueProvenance provenance;
};

struct CompileTimeValueHash {
  std::size_t operator()(const CompileTimeValue &value) const noexcept;
};

CompileTimeValue makeCompileTimeBool(bool value,
                                     CompileTimeValueProvenance provenance = {});
CompileTimeValue makeCompileTimeSignedInteger(
    std::int64_t value,
    CompileTimeValueProvenance provenance = {});
CompileTimeValue makeCompileTimeUnsignedInteger(
    std::uint64_t value,
    CompileTimeValueProvenance provenance = {});
CompileTimeValue makeCompileTimeStringLiteral(
    std::string value,
    CompileTimeValueProvenance provenance = {});
CompileTimeValue makeCompileTimeTypeFact(
    std::string typeName,
    CompileTimeValueProvenance provenance = {});
CompileTimeValue makeCompileTimeSymbol(
    std::string symbolName,
    CompileTimeValueProvenance provenance = {});
CompileTimeValue makeCompileTimeRequirementOutcome(
    CompileTimeEvaluationResultKind kind,
    std::string diagnostic = {},
    CompileTimeValueProvenance provenance = {});
CompileTimeValue makeUnsupportedRuntimeCompileTimeValue(
    std::string description,
    CompileTimeValueProvenance provenance = {});

bool operator==(const CompileTimeRequirementOutcomeValue &left,
                const CompileTimeRequirementOutcomeValue &right);
bool operator==(const CompileTimeValueProvenance &left,
                const CompileTimeValueProvenance &right);
bool operator==(const CompileTimeValue &left, const CompileTimeValue &right);
bool operator!=(const CompileTimeValue &left, const CompileTimeValue &right);

std::string_view compileTimeValueKindName(CompileTimeValueKind kind);
std::string formatCompileTimeValueProvenance(
    const CompileTimeValueProvenance &provenance);
std::string formatCompileTimeValue(const CompileTimeValue &value);

std::optional<CompileTimeEvaluationResult>
compileTimeRequirementResultFromBool(const CompileTimeValue &value,
                                     CompileTimeEvaluationProvenance provenance);
CompileTimeEvaluationResult rejectUnsupportedRuntimeValue(
    const CompileTimeValue &value,
    CompileTimeEvaluationProvenance provenance);

} // namespace primec
