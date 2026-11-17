#include "SemanticsBorrowedParameterMessages.h"

namespace primec::semantics {

std::string borrowedParameterEscapeMessage(std::string_view how, const std::string &name) {
  std::string message = "borrowed parameter escapes via ";
  message += how;
  message += ": ";
  message += name;
  message += " (a parameter without copy or move only borrows the caller's value; take it as [T "
             "copy] or [T move] to ";
  message += how == "return" ? "return it)" : "keep it)";
  return message;
}

} // namespace primec::semantics
