#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace primec {

// A generated definition is the instance of a template: `/pkg/Box__t4f2a9c1d` is `Box<i32>`.
// The path carries a hash of the arguments, so it cannot be read back; the instance records what it came from.
struct TemplateInstanceName {
  // The template as the definition was written (`/std/result/Result`; a sum type carries its arity, `Result__arity2`).
  std::string templatePath;
  // The resolved template arguments, as written in a type (`i32`, `/pkg/Oops`, `Box<i32>`).
  std::vector<std::string> arguments;
};

// Tells whether `path` is a generated instance and what it instantiates. Returns nothing for any other path.
using TemplateInstanceLookup =
    std::function<std::optional<TemplateInstanceName>(const std::string &path)>;

// `Result<i32, Oops>` for an instance of `/std/result/Result__arity2` with the arguments `i32` and `/pkg/Oops`: the
// leaf of the template without its arity, the arguments with their namespaces dropped and the generated names inside
// them spelled out in turn.
std::string templateInstanceDisplayName(const TemplateInstanceName &instance,
                                        const TemplateInstanceLookup &lookup);

// Rewrites each generated path in `text` (`/std/result/Result__arity2__ta77c/Error` becomes `Result<i32, Oops>/Error`)
// as the program spells it. Text without a generated path, and a generated path `lookup` does not know, stay as they
// are.
std::string userFacingDiagnosticText(const std::string &text, const TemplateInstanceLookup &lookup);

} // namespace primec
