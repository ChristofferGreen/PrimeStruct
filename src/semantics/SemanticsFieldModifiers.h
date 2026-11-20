#pragma once

#include <optional>
#include <string>

#include "SemanticsHelpers.h"

namespace primec::semantics {

// What a field does after the struct is constructed. Every field is `Assignable` unless its type or a
// modifier says otherwise; `ConstructionOnly` fields take their value from the brace construction
// (`Type{...}`) and are never the target of an assignment (docs/spec/type-system.md, Reference fields).
// `ConstructionOnlyUnstored` (the `ctor_only` modifier) is the same for the argument, which the field
// defaults after it can read, but the struct does not keep it: no layout slot, no implicit destroy or copy.
enum class FieldBinding { Assignable, ConstructionOnly, ConstructionOnlyUnstored };

// The one place that says which fields are set only by brace construction. A field of type `Reference<T>`
// is construction-only, and so is one marked `ctor_only`, which also stays out of the stored layout. A new
// field modifier with a related meaning adds its case here, so every check that asks sees it.
FieldBinding fieldBindingFor(const BindingInfo &field);

// The diagnostic for assigning to `fieldName`, or nothing when the field may be assigned. It starts with
// `Reference field cannot be reassigned: <field>` for a `Reference` field and ends with the way out.
std::optional<std::string> fieldAssignmentDiagnostic(const BindingInfo &field, const std::string &fieldName);

// The diagnostic for `*this.field = value` (a write through a `Reference` field to its referent), or
// nothing when the write is allowed: the field must be declared `mut` (`[Reference<T> mut] field`). The write never
// rebinds the field; `fieldAssignmentDiagnostic` still rejects `this.field = value`.
std::optional<std::string> referenceFieldWriteDiagnostic(const BindingInfo &field,
                                                         const std::string &fieldName);

// Whether `fullPath` names a lifecycle helper (`Create`, `Destroy`, `Copy`, `Move` and their storage variants), which
// may write the fields of `this` that are not `mut`.
bool isLifecycleHelperPath(const std::string &fullPath);

// The error for a `ctor_only` that is not on a field of an explicit `[struct]` (a local, a parameter, a
// field of an implicit struct) or that is combined with `static`, or empty when every use is allowed.
std::string constructionOnlyPlacementError(const Program &program);

} // namespace primec::semantics
