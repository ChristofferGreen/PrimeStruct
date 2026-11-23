#pragma once

#include <functional>
#include <optional>
#include <string>

#include "SemanticsHelpers.h"

namespace primec::semantics {

// A binding a struct is reached through that is read-only: a non-`mut` local, a `[T]` or `[T copy]` parameter
// (a lambda's included), the `this` of a method that is not `[mut]`, or a capture of a lambda whose `call` is not
// `mut` (the body reads it as `this.name`; `name` is the capture). A `Reference<T>` or `Pointer<T>` binding is not
// read-only here: it carries its own write access (docs/spec/pointers-and-references.md).
struct ReadOnlyRoot {
  enum class Kind { Local, Parameter, Receiver, Capture };
  std::string name;
  Kind kind = Kind::Local;
  // `copy` or `move` for a parameter of that mode: its fix is `[T copy mut]` or `[T move mut]`.
  std::string parameterMode;
};

// What the walk needs to know about a name and a field; the validator supplies both.
struct ReadOnlyRootLookup {
  // The parameter or local binding named `name`, or nullptr when neither exists.
  std::function<const BindingInfo *(const std::string &name, bool &isParameter)> bindingOf;
  // The binding of field `fieldName` of `receiver`, or false when it cannot be resolved.
  std::function<bool(const Expr &receiver, const std::string &fieldName, BindingInfo &fieldOut)> fieldOf;
  // The type of the value `expr` yields, or false when it cannot be inferred.
  std::function<bool(const Expr &expr, BindingInfo &typeOut)> typeOf;
  // The place is in the body of a lambda (the `call` of its closure struct), where `this.name` is a capture.
  bool inClosureBody = false;
};

// The read-only root of `place` (a name, or a chain of field accesses and element reads ending in a name), or
// nothing when the place may be written: its root is `mut`, a `Reference` or `Pointer`, a field or element on the
// way is a `Reference` or `Pointer` (the write goes through it to its referent), the root is not a binding, or the
// place is no chain. An element of a collection (`bag.items[i]`) is part of the collection's owner, so its root is
// the root of the collection place; the result of any other call is a temporary and ends the chain.
// `placeIsReceiver` says the place itself is what a method is called on, so its own last field or element counts
// as one on the way (a `Reference` field's method call changes its referent); for an assignment target it does not.
std::optional<ReadOnlyRoot> findReadOnlyRoot(const Expr &place, const ReadOnlyRootLookup &lookup,
                                             bool placeIsReceiver);

// Whether `path` is the `call` of a `Function` or of a closure struct a lambda or a named definition becomes
// (docs/spec/value-lifecycle.md, Function values). The `call` of a `Function` is always `mut`; a closure's is `mut`
// when it has a `mut` value capture.
bool isClosureCallPath(const std::string &path);

// Whether `path` is the `call` of a `Function`.
bool isFunctionCallPath(const std::string &path);

// `assign target must be a mutable binding: c.n (c is read-only; declare it with mut)`: the write to a field of a
// read-only root, with the way out for the root's kind.
std::string readOnlyFieldWriteDiagnostic(const Expr &target, const ReadOnlyRoot &root);

// `mut parameter requires a mutable place: c.n (c is read-only; declare it with mut: [T mut] c{...})`: a field or an
// element of a read-only root handed to a `mut` parameter.
std::string readOnlyArgumentDiagnostic(const Expr &argument, const ReadOnlyRoot &root);

// `mutating method needs a mutable receiver: c.bump() (c is read-only; declare it with mut)`.
std::string readOnlyReceiverDiagnostic(const Expr &receiver, const std::string &methodName, const ReadOnlyRoot &root);

// `mutating method needs a mutable receiver: b.f.call() (b is a read-only parameter; take it as a [T mut] parameter,
// or call a mut copy: [Function<...> mut] h{b.f})`: the call of a `Function` (`isFunction`) or of a closure with
// `mut` state through a read-only root. `f(x)` means `f.call(x)`, and a copy has its own state.
std::string
readOnlyCallableDiagnostic(const Expr &callable, const ReadOnlyRoot &root, bool isFunction);

} // namespace primec::semantics
