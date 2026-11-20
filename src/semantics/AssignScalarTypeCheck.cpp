#include "AssignScalarTypeCheck.h"

namespace primec::semantics {

namespace {

const char *primitiveScalarName(ReturnKind kind) {
  switch (kind) {
  case ReturnKind::Int:
    return "i32";
  case ReturnKind::Int64:
    return "i64";
  case ReturnKind::UInt64:
    return "u64";
  case ReturnKind::UInt8:
    return "u8";
  case ReturnKind::Float32:
    return "f32";
  case ReturnKind::Float64:
    return "f64";
  case ReturnKind::Bool:
    return "bool";
  default:
    return nullptr;
  }
}

} // namespace

std::optional<std::string> assignScalarTypeMismatchMessage(ReturnKind targetKind,
                                                          ReturnKind referenceFieldKind,
                                                          ReturnKind actual) {
  const ReturnKind expected = targetKind != ReturnKind::Unknown ? targetKind : referenceFieldKind;
  const char *expectedName = primitiveScalarName(expected);
  const char *actualName = primitiveScalarName(actual);
  if (expectedName == nullptr || actualName == nullptr || expected == actual) {
    return std::nullopt;
  }
  return std::string("assign value type mismatch: expected ") + expectedName + ", got " + actualName;
}

} // namespace primec::semantics
