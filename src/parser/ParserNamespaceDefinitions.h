#pragma once

#include "primec/ast/Ast.h"

#include <string>
#include <vector>

namespace primec::parser {

struct NamespaceDefinitionError {
  std::string message;
  int line = 0;
  int column = 0;
};

// True when the transform list carries the `namespace` definition transform.
bool isNamespaceDefinition(const Definition &def);

// Shape rules of a `[namespace]` definition: no parameters, template list or `return`, and a body of
// definitions only. Returns an empty string when the definition is well formed.
std::string namespaceDefinitionShapeError(const Definition &def);

// Folds the `[namespace]` definitions of one path into the first one (transform lists joined, source
// order) and drops a namespace definition whose path is shared with another kind of definition when its
// list is `namespace` alone. Any other list on a shared path is an error.
bool foldNamespaceDefinitions(std::vector<Definition> &definitions, NamespaceDefinitionError &error);

} // namespace primec::parser
