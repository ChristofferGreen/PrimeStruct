#pragma once

#include <functional>
#include <optional>
#include <string>

#include "SemanticsHelpers.h"

namespace primec::semantics {

// A binding a struct is reached through that is read-only: a non-`mut` local, a `[T]` or `[T copy]` parameter
// (a lambda's included), or the `this` of a method that is not `[mut]`. A `Reference<T>` or `Pointer<T>`
// binding is not read-only here: it carries its own write access (docs/spec/pointers-and-references.md).
struct ReadOnlyRoot {
  enum class Kind { Local, Parameter, Receiver };
  std::string name;
  Kind kind = Kind::Local;
};

// What the walk needs to know about a name and a field; the validator supplies both.
struct ReadOnlyRootLookup {
  // The parameter or local binding named `name`, or nullptr when neither exists.
  std::function<const BindingInfo *(const std::string &name, bool &isParameter)> bindingOf;
  // The binding of field `fieldName` of `receiver`, or false when it cannot be resolved.
  std::function<bool(const Expr &receiver, const std::string &fieldName, BindingInfo &fieldOut)> fieldOf;
};

// The read-only root of `place` (a name, or a chain of field accesses ending in a name), or nothing when the
// place may be written: its root is `mut`, a `Reference` or `Pointer`, a field on the way is a `Reference` or
// `Pointer` (the write goes through it to its referent), the root is not a binding, or the place is no chain.
// `placeIsReceiver` says the place itself is what a method is called on, so its own last field counts as a field
// on the way (a `Reference` field's method call changes its referent); for an assignment target it does not.
std::optional<ReadOnlyRoot> findReadOnlyRoot(const Expr &place, const ReadOnlyRootLookup &lookup,
                                             bool placeIsReceiver);

// Whether `path` is the `call` of a `Function` box or of a closure struct a lambda or a named definition becomes. Its
// receiver is not held to this rule: whether a `Function` may be called through a read-only place is a separate
// decision (docs/todo.md, TODO-6116).
bool isClosureCallPath(const std::string &path);

// `assign target must be a mutable binding: c.n (c is read-only; declare it with mut)`: the write to a field of a
// read-only root, with the way out for the root's kind.
std::string readOnlyFieldWriteDiagnostic(const Expr &target, const ReadOnlyRoot &root);

// `mutating method needs a mutable receiver: c.bump() (c is read-only; declare it with mut)`.
std::string readOnlyReceiverDiagnostic(const Expr &receiver, const std::string &methodName, const ReadOnlyRoot &root);

} // namespace primec::semantics
