#pragma once

#include <optional>
#include <string>

#include "SemanticsHelpers.h"

namespace primec::semantics {

// What a field does after the struct is constructed. Every field is `Assignable` unless its type or a
// modifier says otherwise; `ConstructionOnly` fields take their value from the brace construction
// (`Type{...}`) and are never the target of an assignment (docs/spec/type-system.md, Reference fields).
enum class FieldBinding { Assignable, ConstructionOnly };

// The one place that says which fields are set only by brace construction. A field of type `Reference<T>`
// is construction-only. A new field modifier that has the same meaning (one that also keeps the field out
// of the stored layout, for instance) adds its case here, so every check that asks sees it.
FieldBinding fieldBindingFor(const BindingInfo &field);

// The diagnostic for assigning to `fieldName`, or nothing when the field may be assigned. It starts with
// `Reference field cannot be reassigned: <field>` for a `Reference` field and ends with the way out.
std::optional<std::string> fieldAssignmentDiagnostic(const BindingInfo &field, const std::string &fieldName);

} // namespace primec::semantics
