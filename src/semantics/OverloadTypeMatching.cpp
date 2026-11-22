#include "OverloadTypeMatching.h"

#include <cctype>

#include "primec/support/TypeText.h"

namespace primec {

// The leaf of a type spelling without its template arguments: `/lib/Box<i32>` is `Box`.
std::string overloadTypeLeafName(const std::string &typeText) {
  std::string base = typeText;
  const size_t angle = base.find('<');
  if (angle != std::string::npos) {
    base.erase(angle);
  }
  const size_t slash = base.find_last_of('/');
  return slash == std::string::npos ? base : base.substr(slash + 1);
}


// Whether an argument of type `actual` fits a parameter declared `expected`. The same spelling does; so does a path
// and a bare name of the same leaf (`/lib/Tag` and `Tag`), since a parameter and a local may name a struct of a
// namespace differently.
bool overloadTypeTextsMatch(const std::string &expected, const std::string &actual) {
  if (expected == actual) {
    return true;
  }
  const bool expectedPath = expected.find('/') != std::string::npos;
  const bool actualPath = actual.find('/') != std::string::npos;
  if (!expectedPath && !actualPath) {
    return false;
  }
  if (overloadTypeLeafName(expected) != overloadTypeLeafName(actual)) {
    return false;
  }
  const size_t expectedAngle = expected.find('<');
  const size_t actualAngle = actual.find('<');
  if (expectedAngle == std::string::npos || actualAngle == std::string::npos) {
    return expectedAngle == actualAngle;
  }
  if (expected.back() != '>' || actual.back() != '>') {
    return false;
  }
  // The arguments match one by one, so `Reference<Response>` fits `Reference</std/http/Response>`.
  std::vector<std::string> expectedArgs;
  std::vector<std::string> actualArgs;
  if (!splitTopLevelTemplateArgs(expected.substr(expectedAngle + 1, expected.size() - expectedAngle - 2), expectedArgs) ||
      !splitTopLevelTemplateArgs(actual.substr(actualAngle + 1, actual.size() - actualAngle - 2), actualArgs) ||
      expectedArgs.size() != actualArgs.size()) {
    return false;
  }
  for (size_t i = 0; i < expectedArgs.size(); ++i) {
    if (!overloadTypeTextsMatch(expectedArgs[i], actualArgs[i])) {
      return false;
    }
  }
  return true;
}

// Whether `text` names one of the template parameters of the definition, as a whole identifier.
bool overloadTypeTextMentionsTemplateParam(const std::string &text,
                                           const std::vector<std::string> &templateArgs) {
  size_t i = 0;
  while (i < text.size()) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    if (std::isalnum(c) != 0 || c == '_') {
      size_t end = i;
      while (end < text.size() &&
             (std::isalnum(static_cast<unsigned char>(text[end])) != 0 || text[end] == '_')) {
        ++end;
      }
      if (std::find(templateArgs.begin(), templateArgs.end(), text.substr(i, end - i)) !=
          templateArgs.end()) {
        return true;
      }
      i = end;
    } else {
      ++i;
    }
  }
  return false;
}

std::string overloadTypeTextWithoutSpaces(const std::string &text) {
  std::string out;
  for (const char c : text) {
    if (std::isspace(static_cast<unsigned char>(c)) == 0) {
      out.push_back(c);
    }
  }
  return out;
}

// Whether an argument type `actual` fits a parameter type `pattern` that may spell the definition's template
// parameters inside its arguments (`App<S>`, `Pair<S, i32>`). A template parameter matches any argument type, and
// every other part must match as overloadTypeTextsMatch does; the arguments of a nested type are compared the same
// way. This is what makes `mode<S>([App<S>])` viable for an `App<i32>` and not for an `i32`.
bool overloadTypePatternMatches(const std::string &pattern,
                                const std::string &actual,
                                const std::vector<std::string> &templateArgs) {
  const std::string patternText = overloadTypeTextWithoutSpaces(pattern);
  const std::string actualText = overloadTypeTextWithoutSpaces(actual);
  if (std::find(templateArgs.begin(), templateArgs.end(), patternText) != templateArgs.end()) {
    return true;
  }
  const size_t patternAngle = patternText.find('<');
  if (patternAngle == std::string::npos ||
      !overloadTypeTextMentionsTemplateParam(patternText, templateArgs)) {
    return overloadTypeTextsMatch(patternText, actualText);
  }
  const size_t actualAngle = actualText.find('<');
  if (actualAngle == std::string::npos || patternText.back() != '>' || actualText.back() != '>' ||
      overloadTypeLeafName(patternText) != overloadTypeLeafName(actualText)) {
    return false;
  }
  std::vector<std::string> patternArgs;
  std::vector<std::string> actualArgs;
  if (!splitTopLevelTemplateArgs(
          patternText.substr(patternAngle + 1, patternText.size() - patternAngle - 2),
          patternArgs) ||
      !splitTopLevelTemplateArgs(
          actualText.substr(actualAngle + 1, actualText.size() - actualAngle - 2), actualArgs) ||
      patternArgs.size() != actualArgs.size()) {
    return false;
  }
  for (size_t i = 0; i < patternArgs.size(); ++i) {
    if (!overloadTypePatternMatches(patternArgs[i], actualArgs[i], templateArgs)) {
      return false;
    }
  }
  return true;
}


bool overloadArgumentTypeFits(const std::string &pattern,
                              const std::string &actual,
                              const std::vector<std::string> &templateArgs) {
  if (overloadTypePatternMatches(pattern, actual, templateArgs)) {
    return true;
  }
  // A `Reference<T>` argument reads as its referent in a value position (docs/spec/pointers-and-references.md), so it
  // also fits a parameter declared `T`.
  const std::string actualText = overloadTypeTextWithoutSpaces(actual);
  const std::string patternText = overloadTypeTextWithoutSpaces(pattern);
  if (overloadTypeLeafName(actualText) != "Reference" || overloadTypeLeafName(patternText) == "Reference") {
    return false;
  }
  const size_t angle = actualText.find('<');
  if (angle == std::string::npos || actualText.back() != '>') {
    return false;
  }
  return overloadTypePatternMatches(
      patternText, actualText.substr(angle + 1, actualText.size() - angle - 2), templateArgs);
}


} // namespace primec
