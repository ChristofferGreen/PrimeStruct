#pragma once

#include "primec/ast/Ast.h"

#include <functional>
#include <string>

namespace primec::semantics {

// Which values may be owned by another thread (docs/MultithreadingPrototype.md, Sendable Values). The rule is
// structural and runs on monomorphized types: scalars, `string`, enums, `Function`, and containers, tuples, sums and
// structs of sendable types are sendable; `Reference`, `Pointer`, views (`StringView`, slices) and `Task` are not.
// A `[sendable]` struct owns what its pointers point to, so only the pointee types are
// checked. Spawn arguments use it today; the `Function` conversion, `Mutex` and `Channel` sites call it too.
struct SendableTypeLookup {
  // The struct or sum definition that a type text names (after resolving it from `namespacePrefix`), or nullptr.
  std::function<const Definition *(const std::string &typeText, const std::string &namespacePrefix)> resolveDefinition;
  // The concrete type text of one field statement of a struct; false when it cannot be resolved.
  std::function<bool(const Definition &structDef, const Expr &fieldStmt, std::string &typeTextOut)> fieldType;
  // A readable spelling of a type text, template instances as `Name<Args>`.
  std::function<std::string(const std::string &typeText)> display;
};

// Empty when values of `typeText` are sendable. Otherwise the path from the type to the first value that is not,
// ready to follow a colon: `it is Pointer<u8>`, `field `data` of Buffer is Pointer<u8>`, `field `rows` of Job, field
// `bytes` of PixelRows is Pointer<u8>`, `capture `total` is Reference<i32>`, `element of vector<StringView> is
// StringView`.
std::string sendableViolation(const std::string &typeText,
                              const std::string &namespacePrefix,
                              const SendableTypeLookup &lookup);

// The note every sendable diagnostic ends with.
inline constexpr const char *SendableSharingNote = "sharing between tasks needs Mutex, Atomic or Channel";

bool hasSendableTransform(const Definition &def);

} // namespace primec::semantics
