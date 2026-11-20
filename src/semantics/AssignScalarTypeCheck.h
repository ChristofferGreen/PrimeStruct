#pragma once

#include <optional>
#include <string>

#include "SemanticsHelpers.h"

namespace primec::semantics {

// The diagnostic for `assign(target, value)` when the target and the value are both primitive scalars
// (`i32`, `i64`, `u64`, `u8`, `f32`, `f64`, `bool`) of different types: `assign value type mismatch: expected i64,
// got f64`. Returns nothing when either kind is not a primitive scalar (a struct, a collection, a software numeric
// or an unknown kind is checked elsewhere) or when the two are the same. `targetKind` is what the validator infers for
// the target; when that is unknown (`*this.kept` through a `Reference<T>` field) `referenceFieldKind`, the element
// type of the field, stands in for it.
std::optional<std::string> assignScalarTypeMismatchMessage(ReturnKind targetKind,
                                                          ReturnKind referenceFieldKind,
                                                          ReturnKind actual);

} // namespace primec::semantics
