#pragma once

#include "primec/ast/Ast.h"

#include <string>
#include <vector>

namespace primec::semantics {

// `summary("text")` carries a text for tools and has no effect on generated code. Returns an empty string
// when `transform` is a well-formed summary; `first` is the earlier summary of the same list, or null.
std::string summaryTransformError(const Transform &transform, const Transform *first, const std::string &context);

// The text of the summary in `transforms`, or an empty string when there is none.
std::string definitionSummaryText(const std::vector<Transform> &transforms);

} // namespace primec::semantics
