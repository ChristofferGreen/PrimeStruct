#include "SemanticsSummaryTransform.h"

#include "primec/frontend/StringLiteral.h"

namespace primec::semantics {
namespace {

std::string positionText(const Transform &transform) {
  return std::to_string(transform.sourceLine) + ":" + std::to_string(transform.sourceColumn);
}

} // namespace

std::string summaryTransformError(const Transform &transform, const Transform *first, const std::string &context) {
  if (first != nullptr) {
    return "duplicate summary transform on " + context + " (first at " + positionText(*first) + ", again at " +
           positionText(transform) + ")";
  }
  if (!transform.templateArgs.empty()) {
    return "summary transform does not accept template arguments on " + context;
  }
  if (transform.arguments.size() != 1) {
    return "summary transform requires exactly one string literal argument on " + context;
  }
  ParsedStringLiteral literal;
  std::string literalError;
  if (!parseStringLiteralToken(transform.arguments.front(), literal, literalError)) {
    return "summary transform argument must be a string literal on " + context;
  }
  return {};
}

std::string definitionSummaryText(const std::vector<Transform> &transforms) {
  for (const auto &transform : transforms) {
    if (transform.name == "summary" && transform.arguments.size() == 1) {
      ParsedStringLiteral literal;
      std::string literalError;
      if (parseStringLiteralToken(transform.arguments.front(), literal, literalError)) {
        return literal.decoded;
      }
    }
  }
  return {};
}

} // namespace primec::semantics
