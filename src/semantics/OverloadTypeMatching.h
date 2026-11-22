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

// The referent of a `Reference<T>` type text. A `Reference<T>` argument reads as its referent in a value position
// (docs/spec/pointers-and-references.md), so it also fits a parameter declared `T`.
bool overloadReferentTypeText(const std::string &typeText, std::string &referentOut);

} // namespace primec
