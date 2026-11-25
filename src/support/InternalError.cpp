#include "primec/support/InternalError.h"

#include <string_view>

namespace primec {

std::string markInternalCompilerError(const std::string &error) {
  constexpr std::string_view InternalPrefix = "internal compiler error (please report it): ";
  if (error.find("missing semantic-product") == std::string::npos ||
      error.compare(0, InternalPrefix.size(), InternalPrefix) == 0) {
    return error;
  }
  return std::string(InternalPrefix) + error;
}

} // namespace primec
