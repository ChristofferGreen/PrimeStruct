#pragma once

#include <string>
#include <string_view>

#include "primec/semantics/Semantics.h"

namespace primec::semantics {

// Turns every lambda into a closure struct (docs/spec/value-lifecycle.md, "Lambdas and closures"):
// one field per capture, a `call` method holding the body, and a generated factory whose implicit template
// arguments are the capture types. The lambda expression becomes a call of that factory, so template
// monomorphization infers the capture types from the enclosing locals.
bool rewriteLambdaClosures(Program &program, std::string &error);

// Marker in the name of every closure struct the pass generates (`/ns/main__lambda3`, after its enclosing definition).
inline constexpr const char *LambdaClosureStructPrefix = "__lambda";

// Whether `path` is a closure struct the pass generates for a lambda (`/ns/main__lambda3`).
inline bool isLambdaClosureStructPath(const std::string &path) {
  const size_t slash = path.rfind('/');
  return path.find(LambdaClosureStructPrefix, slash == std::string::npos ? 0 : slash) !=
         std::string::npos;
}

// Whether `path` is the generated factory of a lambda (`/ns/main__lambda3_make`): its leading template parameters are
// the enclosing definition's type parameters the lambda names, written explicitly at the call; the capture types after
// them are inferred.
inline bool isLambdaClosureFactoryPath(const std::string &path) {
  constexpr std::string_view FactorySuffix = "_make";
  const size_t slash = path.rfind('/');
  const std::string_view leaf =
      std::string_view(path).substr(slash == std::string::npos ? 0 : slash + 1);
  return leaf.size() > FactorySuffix.size() &&
         leaf.compare(leaf.size() - FactorySuffix.size(), FactorySuffix.size(), FactorySuffix) ==
             0 &&
         leaf.find(LambdaClosureStructPrefix) != std::string_view::npos;
}

// Suffix of the closure struct the pass generates for a named definition used as a `Function` value
// (`/double__fnref`): a struct without fields whose `call` forwards to the definition.
inline constexpr const char *FunctionForwarderSuffix = "__fnref";

// The user-facing spelling of a type-name leaf (no namespace, no `__t` instance suffix) that the pass generates, for
// diagnostics: the `Function<R, A...>` box struct `__FunctionN` reads `Function`, a closure struct reads `closure`.
// Any other leaf is returned unchanged.
inline std::string userFacingLoweredTypeLeaf(const std::string &leaf) {
  constexpr std::string_view FunctionBoxPrefix = "__Function";
  if (leaf.size() > FunctionBoxPrefix.size() &&
      leaf.compare(0, FunctionBoxPrefix.size(), FunctionBoxPrefix) == 0 &&
      leaf.find_first_not_of("0123456789", FunctionBoxPrefix.size()) == std::string::npos) {
    return "Function";
  }
  if (leaf.find(LambdaClosureStructPrefix) != std::string::npos) {
    return "closure";
  }
  return leaf;
}

} // namespace primec::semantics
