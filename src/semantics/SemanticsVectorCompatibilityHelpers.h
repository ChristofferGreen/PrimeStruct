#pragma once

#include <string>
#include <string_view>

namespace primec::semantics {

inline bool isVectorCompatibilityHelperName(std::string_view helperName) {
  return helperName == "count" || helperName == "capacity" || helperName == "at" ||
         helperName == "at_unsafe" || helperName == "push" || helperName == "pop" ||
         helperName == "reserve" || helperName == "clear" || helperName == "remove_at" ||
         helperName == "remove_swap";
}

inline std::string vectorCompatibilityRequiresVectorTargetDiagnostic(
    std::string_view helperName) {
  return std::string(helperName) + " requires vector target";
}

} // namespace primec::semantics
