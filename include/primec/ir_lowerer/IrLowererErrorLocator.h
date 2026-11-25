#pragma once

#include <optional>
#include <string>

#include "primec/ast/Ast.h"
#include "primec/frontend/ExpandedSource.h"
#include "primec/support/Diagnostics.h"

namespace primec::ir_lowerer {

// Finds the place of a lowering error. Lowering reports a failure as a message only; the locator remembers the
// expression whose lowering failed, so the failure can be shown at the code the user wrote.
//
// Failures propagate outward: the innermost expression fails first, then every expression around it. For one
// message the locator keeps the innermost expression that sits in a file of the user (the program or a file it
// imports); an expression of the standard library that was inlined into the user's call is passed over for the call
// that led to it, and kept only when no user code surrounds it. A failure that lowering drops (a fallback tried
// instead) leaves a record behind; the next failure with another message replaces it, so a dropped failure does not
// locate a later one with another message.
class LoweringErrorLocator {
public:
  // `expandedSource` maps the positions of expressions to the files they were written in; null keeps them as they are.
  explicit LoweringErrorLocator(const ExpandedSource *expandedSource = nullptr)
      : expandedSource_(expandedSource) {
  }

  // Lowering `expr` failed with `error`.
  void noteFailure(const Expr &expr, const std::string &error);

  // The place of the failure `error` in the file the user wrote; nothing when no expression was recorded for it.
  std::optional<DiagnosticSpan> spanOf(const std::string &error) const;

private:
  const ExpandedSource *expandedSource_ = nullptr;
  std::string error_;
  DiagnosticSpan span_;
  bool hasSpan_ = false;
  bool spanInUserCode_ = false;
};

// `error` with the generated names of `program`'s template instances spelled the way the program writes them
// (`Result<i32, Oops>` for `/std/result/Result__arity2__ta77c...`).
std::string userFacingLoweringError(const std::string &error, const Program &program);

// A message that reports a violated compiler invariant (a `missing semantic-product ...` fact the semantic pass should
// have published) is not a mistake in the user's program: `error` with the prefix
// `internal compiler error (please report it): `. Any other message, and one already marked, comes back unchanged.
std::string markInternalLoweringError(const std::string &error);

} // namespace primec::ir_lowerer
