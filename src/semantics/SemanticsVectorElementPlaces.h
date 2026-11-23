#pragma once

#include <string>
#include <unordered_set>

#include "primec/ast/Ast.h"

namespace primec::semantics {

// An element of a stdlib `Vector` is a place, as an element of the builtin `vector` is
// (docs/spec/value-lifecycle.md, Read-only roots): `values[i].n = x`, `values[i].bump()` and an element passed to
// a `mut` parameter change the element. The library's `at` returns a copy, so the validator notes each element
// read that a write goes through, and after validation those reads become calls of the library's
// `vectorElementRef`, which returns the element by reference.

// The element reads of a program that are written through, by the definition they are in and their source
// position.
class VectorElementPlaceSites {
public:
  void add(const std::string &definitionPath, const Expr &elementRead);
  bool contains(const std::string &definitionPath, const Expr &elementRead) const;
  // The element reads that are the whole target of an assignment (`values[i] = x`): the assignment replaces the
  // element through the reference `vectorElementRef` returns.
  void addAssignedElement(const std::string &definitionPath, const Expr &elementRead);
  bool containsAssignedElement(const std::string &definitionPath, const Expr &elementRead) const;
  bool empty() const {
    return sites_.empty() && assignedSites_.empty();
  }
  void merge(const VectorElementPlaceSites &other);

private:
  std::unordered_set<std::string> sites_;
  std::unordered_set<std::string> assignedSites_;
};

// Whether `expr` is a call of an instantiation of the stdlib vector's `at` or `at_unsafe`.
bool isStdlibVectorElementRead(const Expr &expr);

// Notes every stdlib `Vector` element read on the way from `place` (a chain of field accesses and element reads)
// to its root. With `placeIsWholeValue` the place itself may be such a read (a receiver, a `mut` argument); an
// assignment target that is the element itself is not written through a reference, so it is left out.
void noteVectorElementPlaces(const std::string &definitionPath,
                             const Expr &place,
                             bool placeIsWholeValue,
                             VectorElementPlaceSites &sites);

// Turns each noted element read into the call that returns the element by reference.
void rewriteVectorElementPlaces(Program &program, const VectorElementPlaceSites &sites);

} // namespace primec::semantics
