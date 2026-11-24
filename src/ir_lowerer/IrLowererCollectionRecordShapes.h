#pragma once

#include "primec/ast/Ast.h"

#include <string>
#include <unordered_set>

namespace primec::ir_lowerer {

// A struct type name as written inside `namespacePrefix`, resolved to its full path, or empty.
std::string resolveStructTypeName(const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  const std::unordered_set<std::string> &structNames);

// Whether a parameter or the result of `def` is a collection record: a builtin `vector<T>`, `map<K, V>` or
// `array<T>`, or a struct of the canonical collections (`/std/collections/...`), each as written or behind a
// `Reference` or a `Pointer`. A helper under the collections root that takes or returns one is expanded inline; a helper
// of that root with scalar parameters and result may be a function of a library.
bool takesOrReturnsCollectionRecord(const Definition &def,
                                    const std::unordered_set<std::string> &structNames);

} // namespace primec::ir_lowerer
