#include "primec/CompileTimeValue.h"

#include <sstream>
#include <utility>

namespace primec {

namespace {

std::size_t hashCombine(std::size_t seed, std::size_t value) noexcept {
  return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

std::size_t hashString(std::string_view value) noexcept {
  return std::hash<std::string_view>{}(value);
}

std::size_t hashProvenance(const CompileTimeValueProvenance &provenance) noexcept {
  std::size_t seed = hashString(provenance.sourceText);
  seed = hashCombine(seed, hashString(provenance.sourcePath));
  seed = hashCombine(seed, std::hash<std::uint32_t>{}(provenance.line));
  seed = hashCombine(seed, std::hash<std::uint32_t>{}(provenance.column));
  seed = hashCombine(seed, std::hash<std::uint64_t>{}(provenance.semanticNodeId));
  seed = hashCombine(seed, std::hash<std::uint64_t>{}(provenance.provenanceHandle));
  return seed;
}

CompileTimeValue makeStringBackedValue(CompileTimeValueKind kind,
                                       std::string value,
                                       CompileTimeValueProvenance provenance) {
  CompileTimeValue result;
  result.kind = kind;
  result.payload = std::move(value);
  result.provenance = std::move(provenance);
  return result;
}

std::string stringPayload(const CompileTimeValue &value) {
  if (const auto *payload = std::get_if<std::string>(&value.payload)) {
    return *payload;
  }
  return {};
}

} // namespace

std::size_t CompileTimeValueHash::operator()(
    const CompileTimeValue &value) const noexcept {
  std::size_t seed = std::hash<int>{}(static_cast<int>(value.kind));
  seed = hashCombine(seed, hashProvenance(value.provenance));
  switch (value.kind) {
  case CompileTimeValueKind::Bool:
    if (const auto *payload = std::get_if<bool>(&value.payload)) {
      return hashCombine(seed, std::hash<bool>{}(*payload));
    }
    break;
  case CompileTimeValueKind::SignedInteger:
    if (const auto *payload = std::get_if<std::int64_t>(&value.payload)) {
      return hashCombine(seed, std::hash<std::int64_t>{}(*payload));
    }
    break;
  case CompileTimeValueKind::UnsignedInteger:
    if (const auto *payload = std::get_if<std::uint64_t>(&value.payload)) {
      return hashCombine(seed, std::hash<std::uint64_t>{}(*payload));
    }
    break;
  case CompileTimeValueKind::StringLiteral:
  case CompileTimeValueKind::TypeFact:
  case CompileTimeValueKind::Symbol:
  case CompileTimeValueKind::UnsupportedRuntimeValue:
    if (const auto *payload = std::get_if<std::string>(&value.payload)) {
      return hashCombine(seed, hashString(*payload));
    }
    break;
  case CompileTimeValueKind::RequirementOutcome:
    if (const auto *payload =
            std::get_if<CompileTimeRequirementOutcomeValue>(&value.payload)) {
      seed = hashCombine(
          seed,
          std::hash<int>{}(static_cast<int>(payload->kind)));
      return hashCombine(seed, hashString(payload->diagnostic));
    }
    break;
  }
  return seed;
}

CompileTimeValue makeCompileTimeBool(bool value,
                                     CompileTimeValueProvenance provenance) {
  CompileTimeValue result;
  result.kind = CompileTimeValueKind::Bool;
  result.payload = value;
  result.provenance = std::move(provenance);
  return result;
}

CompileTimeValue makeCompileTimeSignedInteger(
    std::int64_t value,
    CompileTimeValueProvenance provenance) {
  CompileTimeValue result;
  result.kind = CompileTimeValueKind::SignedInteger;
  result.payload = value;
  result.provenance = std::move(provenance);
  return result;
}

CompileTimeValue makeCompileTimeUnsignedInteger(
    std::uint64_t value,
    CompileTimeValueProvenance provenance) {
  CompileTimeValue result;
  result.kind = CompileTimeValueKind::UnsignedInteger;
  result.payload = value;
  result.provenance = std::move(provenance);
  return result;
}

CompileTimeValue makeCompileTimeStringLiteral(
    std::string value,
    CompileTimeValueProvenance provenance) {
  return makeStringBackedValue(
      CompileTimeValueKind::StringLiteral, std::move(value), std::move(provenance));
}

CompileTimeValue makeCompileTimeTypeFact(
    std::string typeName,
    CompileTimeValueProvenance provenance) {
  return makeStringBackedValue(
      CompileTimeValueKind::TypeFact, std::move(typeName), std::move(provenance));
}

CompileTimeValue makeCompileTimeSymbol(
    std::string symbolName,
    CompileTimeValueProvenance provenance) {
  return makeStringBackedValue(
      CompileTimeValueKind::Symbol, std::move(symbolName), std::move(provenance));
}

CompileTimeValue makeCompileTimeRequirementOutcome(
    CompileTimeEvaluationResultKind kind,
    std::string diagnostic,
    CompileTimeValueProvenance provenance) {
  CompileTimeValue result;
  result.kind = CompileTimeValueKind::RequirementOutcome;
  result.payload = CompileTimeRequirementOutcomeValue{kind, std::move(diagnostic)};
  result.provenance = std::move(provenance);
  return result;
}

CompileTimeValue makeUnsupportedRuntimeCompileTimeValue(
    std::string description,
    CompileTimeValueProvenance provenance) {
  return makeStringBackedValue(CompileTimeValueKind::UnsupportedRuntimeValue,
                               std::move(description),
                               std::move(provenance));
}

bool operator==(const CompileTimeRequirementOutcomeValue &left,
                const CompileTimeRequirementOutcomeValue &right) {
  return left.kind == right.kind && left.diagnostic == right.diagnostic;
}

bool operator==(const CompileTimeValueProvenance &left,
                const CompileTimeValueProvenance &right) {
  return left.sourceText == right.sourceText &&
         left.sourcePath == right.sourcePath &&
         left.line == right.line &&
         left.column == right.column &&
         left.semanticNodeId == right.semanticNodeId &&
         left.provenanceHandle == right.provenanceHandle;
}

bool operator==(const CompileTimeValue &left, const CompileTimeValue &right) {
  return left.kind == right.kind &&
         left.payload == right.payload &&
         left.provenance == right.provenance;
}

bool operator!=(const CompileTimeValue &left, const CompileTimeValue &right) {
  return !(left == right);
}

std::string_view compileTimeValueKindName(CompileTimeValueKind kind) {
  switch (kind) {
  case CompileTimeValueKind::Bool:
    return "bool";
  case CompileTimeValueKind::SignedInteger:
    return "signed_integer";
  case CompileTimeValueKind::UnsignedInteger:
    return "unsigned_integer";
  case CompileTimeValueKind::StringLiteral:
    return "string_literal";
  case CompileTimeValueKind::TypeFact:
    return "type_fact";
  case CompileTimeValueKind::Symbol:
    return "symbol";
  case CompileTimeValueKind::RequirementOutcome:
    return "requirement_outcome";
  case CompileTimeValueKind::UnsupportedRuntimeValue:
    return "unsupported_runtime_value";
  }
  return "unknown";
}

std::string formatCompileTimeValueProvenance(
    const CompileTimeValueProvenance &provenance) {
  std::ostringstream out;
  if (!provenance.sourceText.empty()) {
    out << "source_text \"" << provenance.sourceText << "\"";
  }
  if (!provenance.sourcePath.empty()) {
    if (out.tellp() > 0) {
      out << ", ";
    }
    out << "source " << provenance.sourcePath;
    if (provenance.line != 0) {
      out << ':' << provenance.line;
      if (provenance.column != 0) {
        out << ':' << provenance.column;
      }
    }
  }
  if (provenance.semanticNodeId != 0) {
    if (out.tellp() > 0) {
      out << ", ";
    }
    out << "semantic_node_id " << provenance.semanticNodeId;
  }
  if (provenance.provenanceHandle != 0) {
    if (out.tellp() > 0) {
      out << ", ";
    }
    out << "provenance_handle " << provenance.provenanceHandle;
  }
  const std::string formatted = out.str();
  return formatted.empty() ? std::string("unknown provenance") : formatted;
}

std::string formatCompileTimeValue(const CompileTimeValue &value) {
  std::ostringstream out;
  out << "ct_value " << compileTimeValueKindName(value.kind) << " ";
  switch (value.kind) {
  case CompileTimeValueKind::Bool:
    out << (std::get<bool>(value.payload) ? "true" : "false");
    break;
  case CompileTimeValueKind::SignedInteger:
    out << std::get<std::int64_t>(value.payload);
    break;
  case CompileTimeValueKind::UnsignedInteger:
    out << std::get<std::uint64_t>(value.payload) << "u";
    break;
  case CompileTimeValueKind::StringLiteral:
    out << '"' << stringPayload(value) << '"';
    break;
  case CompileTimeValueKind::TypeFact:
    out << stringPayload(value);
    break;
  case CompileTimeValueKind::Symbol:
    out << stringPayload(value);
    break;
  case CompileTimeValueKind::RequirementOutcome: {
    const auto &outcome =
        std::get<CompileTimeRequirementOutcomeValue>(value.payload);
    out << compileTimeEvaluationResultKindName(outcome.kind);
    if (!outcome.diagnostic.empty()) {
      out << " diagnostic=\"" << outcome.diagnostic << "\"";
    }
    break;
  }
  case CompileTimeValueKind::UnsupportedRuntimeValue:
    out << stringPayload(value);
    break;
  }
  out << " (" << formatCompileTimeValueProvenance(value.provenance) << ')';
  return out.str();
}

std::optional<CompileTimeEvaluationResult>
compileTimeRequirementResultFromBool(const CompileTimeValue &value,
                                     CompileTimeEvaluationProvenance provenance) {
  if (value.kind != CompileTimeValueKind::Bool) {
    return std::nullopt;
  }
  const bool boolValue = std::get<bool>(value.payload);
  CompileTimeEvaluationResult result;
  result.kind = boolValue ? CompileTimeEvaluationResultKind::Success
                          : CompileTimeEvaluationResultKind::UnsatisfiedPredicate;
  result.fault = boolValue ? CompileTimeEvaluationFaultKind::None
                           : CompileTimeEvaluationFaultKind::UnsatisfiedPredicate;
  result.provenance = std::move(provenance);
  result.boolValue = boolValue;
  result.message = boolValue ? "compile-time predicate returned true"
                             : "compile-time predicate returned false";
  return result;
}

CompileTimeEvaluationResult rejectUnsupportedRuntimeValue(
    const CompileTimeValue &value,
    CompileTimeEvaluationProvenance provenance) {
  CompileTimeEvaluationResult result;
  result.kind = CompileTimeEvaluationResultKind::InvalidEvaluation;
  result.fault = CompileTimeEvaluationFaultKind::InvalidEvaluation;
  result.provenance = std::move(provenance);
  result.message = "unsupported runtime-only compile-time value";
  if (value.kind == CompileTimeValueKind::UnsupportedRuntimeValue &&
      std::holds_alternative<std::string>(value.payload)) {
    result.message += ": " + std::get<std::string>(value.payload);
  }
  return result;
}

} // namespace primec
