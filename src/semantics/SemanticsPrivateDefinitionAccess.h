#pragma once

#include "primec/ast/Ast.h"

#include <string>
#include <unordered_map>

namespace primec::semantics {

// True for a private definition of the standard library (a path under `/std/`): a member of a namespace
// that is not `[public]`, or a method (or other nested definition) marked `[private]`. A method without a
// visibility transform is callable wherever its struct is.
bool isPrivateStdDefinition(const std::unordered_map<std::string, const Definition *> &defMap,
                            const Definition &def);

// The namespace a definition belongs to: the path above its name, and for a method of a struct the namespace
// of the struct (`/pkg/shapes` for `/pkg/shapes/Circle/area`).
std::string definitionNamespace(const std::unordered_map<std::string, const Definition *> &defMap,
                                const std::string &definitionPath);

// The diagnostic when `callee` is a private definition of the standard library and `accessPath` (the path of
// the definition doing the call, or the namespace of an execution) is outside the callee's namespace and the
// namespaces nested in it; empty when the call is allowed.
std::string
privateDefinitionCallViolation(const std::unordered_map<std::string, const Definition *> &defMap,
                               const Definition &callee,
                               const std::string &accessPath);

// `<path> is private: only definitions in <namespace> may call it`.
std::string privateDefinitionDiagnostic(const std::string &definitionPath,
                                        const std::string &definitionNamespace);

} // namespace primec::semantics
