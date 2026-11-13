#pragma once

#include <string>
#include <string_view>

namespace primec {

// Spellings of call names and scalar type names that every layer reads the same way (semantics, the lowerer, the IR
// printer and the AST printer). What a layer reads beyond them (collection member spellings, namespace prefixes,
// SoA aliases) stays in that layer.

// `path` without one leading `/`.
std::string_view trimLeadingSlash(std::string_view path);

// Reads a single-segment name: one optional leading `/`, then a name without another `/`. Returns false (and leaves
// `out` empty) for a name with more segments, so `/foo/return` is not `return`.
bool readSingleSegmentName(std::string_view spelling, std::string &out);

// `readSingleSegmentName` followed by a comparison with `target`.
bool isSingleSegmentName(std::string_view spelling, std::string_view target);

// The scalar types every layer knows by spelling. `float` and `f32` are Float32, `int` and `i32` are Int32.
enum class ScalarTypeKind {
  None,
  Int32,
  Int64,
  UInt64,
  UInt8,
  Bool,
  String,
  Float32,
  Float64,
  Void
};

// The scalar kind of a type spelling, None for anything else (`never`, `Result<..>`, error types and the like are
// each layer's own business).
ScalarTypeKind scalarTypeKindForName(std::string_view typeName);

} // namespace primec
