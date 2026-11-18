#pragma once

#include <string>

#include "primec/ast/Ast.h"

namespace primec {

// An imported definition is a declaration in the interface of a library (docs/spec/integration-and-tooling.md,
// Compiling against a library):
//   [public return<i32> imported(ui)] /ui/add([i32] a, [i32] b) {}
// It has an empty body, and its code is in the library archive: lowering turns a call of it into a call of an import
// function (primec/ir/Ir.h, IrFunction::isImport), and the link replaces that with the library's definition.
inline const Transform *importedTransform(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name == "imported") {
      return &transform;
    }
  }
  return nullptr;
}

inline bool isImportedDefinition(const Definition &def) {
  return importedTransform(def) != nullptr;
}

// The library an imported definition names, or empty when the marker has no single argument.
inline std::string importedLibraryName(const Definition &def) {
  const Transform *transform = importedTransform(def);
  return transform != nullptr && transform->arguments.size() == 1 ? transform->arguments.front()
                                                                  : std::string();
}

} // namespace primec
