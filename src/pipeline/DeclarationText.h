#pragma once

// Canonical PrimeStruct text of the pieces of a definition (docs/ModulesAndLibrariesPlan.md, section 5). This is the
// declaration printer of `--emit=interface`; it is not `--dump-stage ast`, which is a debug rendering. Every function is
// a pure function of its inputs and prints one line (no newline) unless it says otherwise.

#include "primec/ast/Ast.h"

#include <string>
#include <vector>

namespace primec {

// `name<arg, ...>...(argument, ...)` of one transform, without the enclosing brackets.
std::string declarationTransformText(const Transform &transform);

// `[a, b(c)] ` for a list of transforms, with the trailing space; empty for an empty list. The separator is a comma
// because an absolute type path after a space (`[static /ui/Color]`) would be rewritten as a division.
std::string declarationTransformListText(const std::vector<Transform> &transforms);

// An expression in canonical call syntax (`f(a, [label] b)`, `x.f(a)`, `Type{...}`, `[type] name{init}`).
std::string declarationExprText(const Expr &expr);

// A parameter: its transforms, its name and `{default}` when it has one.
std::string declarationParameterText(const Expr &parameter);

// A field or other binding statement of a struct: `[public i32] x{1i32}`.
std::string declarationBindingText(const Expr &binding);

// The statements of a body, one per line, each indented by two spaces and ended by a newline.
std::string declarationBodyText(const std::vector<Expr> &statements);

// A string as a `"..."` literal with `\\`, `\"` and line breaks escaped.
std::string declarationQuotedString(const std::string &text);

} // namespace primec
