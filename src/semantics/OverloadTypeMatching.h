#pragma once

#include <string>
#include <vector>

namespace primec {

// Type-text matching for helper overload selection (docs/spec/language-core.md, Helper overloading).

// The leaf of a type spelling without its template arguments: `/lib/Box<i32>` is `Box`.
std::string overloadTypeLeafName(const std::string &typeText);

// Whether an argument of type `actual` fits a parameter declared `expected`: the same spelling, or a path and a bare
// name of the same leaf, also inside the template arguments.
bool overloadTypeTextsMatch(const std::string &expected, const std::string &actual);

// Whether `text` names one of the template parameters of the definition, as a whole identifier.
bool overloadTypeTextMentionsTemplateParam(const std::string &text, const std::vector<std::string> &templateArgs);

std::string overloadTypeTextWithoutSpaces(const std::string &text);

// Whether an argument type fits a parameter type that may spell the definition's template parameters.
bool overloadTypePatternMatches(const std::string &pattern,
                                const std::string &actual,
                                const std::vector<std::string> &templateArgs);

// overloadTypePatternMatches, and also a `Reference<T>` argument for a parameter that is not a `Reference`.
bool overloadArgumentTypeFits(const std::string &pattern,
                              const std::string &actual,
                              const std::vector<std::string> &templateArgs);

} // namespace primec
