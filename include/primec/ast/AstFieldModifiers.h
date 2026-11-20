#pragma once

#include "primec/ast/Ast.h"

namespace primec {

// A struct field marked `ctor_only` takes its value from the brace construction (`Type{...}`) and is visible
// to the defaults of the fields after it, but the struct does not store it: it has no layout slot, no
// metadata, no implicit destroy or copy, and no field access (docs/spec/type-system.md, construction-only
// fields). The one predicate every struct-field walk asks, in semantics and in the lowerer alike.
inline bool isUnstoredConstructionField(const Expr &field) {
  for (const auto &transform : field.transforms) {
    if (transform.name == "ctor_only") {
      return true;
    }
  }
  return false;
}

} // namespace primec
