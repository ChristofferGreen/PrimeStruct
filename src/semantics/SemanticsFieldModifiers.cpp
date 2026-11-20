#include "SemanticsFieldModifiers.h"

namespace primec::semantics {

FieldBinding fieldBindingFor(const BindingInfo &field) {
  if (field.typeName == "Reference") {
    return FieldBinding::ConstructionOnly;
  }
  return FieldBinding::Assignable;
}

std::optional<std::string> fieldAssignmentDiagnostic(const BindingInfo &field, const std::string &fieldName) {
  if (fieldBindingFor(field) == FieldBinding::Assignable) {
    return std::nullopt;
  }
  return "Reference field cannot be reassigned: " + fieldName +
         " (a Reference field is bound once by brace construction; to write through it, bind a Reference "
         "local to location(this." + fieldName + ") in a method and assign that)";
}

} // namespace primec::semantics
