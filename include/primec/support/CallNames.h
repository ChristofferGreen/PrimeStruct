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

// One rule decides whether a call spelling is the simple call `target`: take the spelling (optionally joined with
// its namespace prefix), drop one leading `/` (and `std/gpu/` when the policy says so), let the policy's hook decide
// collection-spelled names, reject any remaining multi-segment name, drop a generated `__t` template suffix when the
// policy says so, and compare. What differs between layers is only the policy row: the strict row (the default) is
// the IR and AST printers' single-segment test, semantics and the lowerer add their own flags and hook.
struct SimpleCallPolicy {
  // Lowerer: a bare name inside a namespace reads as `/<prefix>/<name>`.
  bool joinNamespacePrefix = false;
  // Semantics and lowerer: `std/gpu/<name>` reads as `<name>`.
  bool stripGpuPrefix = false;
  // Semantics: `vector__t<hash>` style generated suffixes are not part of the name.
  bool stripTemplateSuffix = false;
  // Decides names the layer reads as collection members or storage aliases. Receives the name with the leading `/`
  // (and the gpu prefix) already dropped. Returns true when it decided, with the answer in `matches`; false lets the
  // shared rule continue.
  bool (*decide)(const std::string &name, std::string_view target, bool &matches) = nullptr;
};

bool matchSimpleCallName(std::string_view spelling,
                         std::string_view namespacePrefix,
                         std::string_view target,
                         const SimpleCallPolicy &policy);

// The spellings of the control-flow and statement builtins, each written once.
namespace call_names {
inline constexpr std::string_view Assign = "assign";
inline constexpr std::string_view Block = "block";
inline constexpr std::string_view For = "for";
inline constexpr std::string_view If = "if";
inline constexpr std::string_view Loop = "loop";
inline constexpr std::string_view Match = "match";
inline constexpr std::string_view Pick = "pick";
inline constexpr std::string_view Repeat = "repeat";
inline constexpr std::string_view Return = "return";
inline constexpr std::string_view While = "while";
} // namespace call_names

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
