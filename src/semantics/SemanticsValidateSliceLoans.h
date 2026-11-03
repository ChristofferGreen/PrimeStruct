#pragma once

#include <string>

#include "primec/semantics/Semantics.h"

namespace primec::semantics {

// Lexical borrow checking for slices and text views (docs/spec/strings-and-views.md, Borrowing): a view bound from a
// local owner is a loan on that owner until the view's last use in its block.
bool validateSliceLoans(const Program &program, std::string &error);

} // namespace primec::semantics
