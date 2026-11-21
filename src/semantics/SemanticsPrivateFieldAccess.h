#pragma once

#include "primec/ast/Ast.h"

#include <string>
#include <unordered_map>

namespace primec::semantics {

// True when the field binding carries the `private` transform.
bool isPrivateFieldBinding(const Expr &fieldBinding);

// A private field may be named by the struct's own body and by every
// definition in the struct's namespace and the namespaces nested in it.
// `structPath` is the struct's full path (`/box/Box`, or a monomorphized
// instance such as `/box/Box__t1f`), `accessNamespace` the path of the
// definition (or the namespace of the execution) doing the access.
bool isPrivateFieldAccessible(const std::string &structPath, const std::string &accessNamespace);

// The diagnostic when `fieldName` of the struct at `structPath` (a template
// instance has its own definition) is private and
// `accessNamespace` may not name it; empty when the access is allowed or the
// field is unknown.
std::string privateFieldViolation(const std::unordered_map<std::string, const Definition *> &defMap,
                                  const std::string &structPath,
                                  const std::string &fieldName,
                                  const std::string &accessNamespace);

// `private field is not accessible: <struct>/<field>`.
std::string privateFieldDiagnostic(const std::string &structPath, const std::string &fieldName);

} // namespace primec::semantics
