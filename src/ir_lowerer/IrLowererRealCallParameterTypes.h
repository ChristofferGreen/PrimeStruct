#pragma once

#include "primec/ast/Ast.h"

#include <string>
#include <unordered_set>

namespace primec::ir_lowerer {

// A builtin `vector<T>` type (not the stdlib struct of the capitalized spelling) whose element the lowerer knows how
// to copy and destroy.
bool isBuiltinVectorType(const Transform &transform,
                         const std::string &namespacePrefix,
                         const std::unordered_set<std::string> &structNames);

// `Reference<vector<T>>` of a builtin vector the lowerer can copy and destroy elements of: the argument is the
// vector's record address, as for a `mut` vector parameter.
bool isReferenceToBuiltinVector(const Transform &transform,
                                const std::string &namespacePrefix,
                                const std::unordered_set<std::string> &structNames);

// An instantiated `ReadSlice<T>` or `WriteSlice<T>`: a borrowed view that is a data address and an element count
// (docs/spec/integration-and-tooling.md, library function shapes). The record is the caller's own and the callee
// reads its two slots through the address it is passed.
bool isBorrowedSliceType(const Transform &transform,
                         const std::unordered_set<std::string> &structNames);

} // namespace primec::ir_lowerer
