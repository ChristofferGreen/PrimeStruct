#include "TextFilterPipelineInternal.h"

#include "TextFilterHelpers.h"
#include "primec/support/TransformRegistry.h"

#include <cctype>

namespace primec {
using namespace text_filter;

enum class ListPhase { Auto, Text, Semantic };

bool isIdentifierStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '/';
}

bool isIdentifierBody(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '/';
}

bool isTransformListBoundary(const std::string &input, size_t index) {
  if (index == 0) {
    return true;
  }
  char prev = input[index - 1];
  if (std::isspace(static_cast<unsigned char>(prev))) {
    return true;
  }
  switch (prev) {
  case '(':
  case '{':
  case ';':
  case ',':
    return true;
  default:
    return false;
  }
}

void skipWhitespaceAndComments(const std::string &input, size_t &pos) {
  while (pos < input.size()) {
    char c = input[pos];
    if (std::isspace(static_cast<unsigned char>(c))) {
      ++pos;
      continue;
    }
    if (c == '/' && pos + 1 < input.size()) {
      if (input[pos + 1] == '/') {
        pos += 2;
        while (pos < input.size() && input[pos] != '\n') {
          ++pos;
        }
        continue;
      }
      if (input[pos + 1] == '*') {
        size_t end = input.find("*/", pos + 2);
        if (end == std::string::npos) {
          pos = input.size();
          return;
        }
        pos = end + 2;
        continue;
      }
    }
    return;
  }
}

size_t findMatchingCloseWithComments(const std::string &input,
                                     size_t openIndex,
                                     char openChar,
                                     char closeChar) {
  int depth = 1;
  size_t pos = openIndex + 1;
  while (pos < input.size()) {
    char c = input[pos];
    if (c == '"' || c == '\'') {
      size_t end = skipQuotedForward(input, pos);
      if (end == std::string::npos) {
        return std::string::npos;
      }
      pos = end;
      continue;
    }
    if (c == 'R' && pos + 2 < input.size() && input[pos + 1] == '"' && input[pos + 2] == '(') {
      size_t end = input.find(")\"", pos + 3);
      if (end == std::string::npos) {
        return std::string::npos;
      }
      pos = end + 2;
      continue;
    }
    if (c == '/' && pos + 1 < input.size()) {
      if (input[pos + 1] == '/') {
        pos += 2;
        while (pos < input.size() && input[pos] != '\n') {
          ++pos;
        }
        continue;
      }
      if (input[pos + 1] == '*') {
        size_t end = input.find("*/", pos + 2);
        if (end == std::string::npos) {
          return std::string::npos;
        }
        pos = end + 2;
        continue;
      }
    }
    if (c == openChar) {
      ++depth;
      ++pos;
      continue;
    }
    if (c == closeChar) {
      --depth;
      if (depth == 0) {
        return pos;
      }
      ++pos;
      continue;
    }
    ++pos;
  }
  return std::string::npos;
}

bool parseIdentifier(const std::string &input, size_t &pos, std::string &out) {
  if (pos >= input.size() || !isIdentifierStart(input[pos])) {
    return false;
  }
  size_t start = pos;
  ++pos;
  while (pos < input.size() && isIdentifierBody(input[pos])) {
    ++pos;
  }
  out = input.substr(start, pos - start);
  return true;
}

void appendTextTransform(const std::string &name, ListPhase phase, std::vector<std::string> &out) {
  if (phase == ListPhase::Semantic) {
    return;
  }
  if (phase == ListPhase::Text) {
    if (primec::isTextTransformName(name)) {
      out.push_back(name);
    }
    return;
  }
  if (primec::isTextTransformName(name) && !primec::isSemanticTransformName(name)) {
    out.push_back(name);
  }
}

void scanTransformEntries(const std::string &input,
                          size_t &pos,
                          size_t end,
                          ListPhase phase,
                          std::vector<std::string> &textTransforms) {
  while (pos < end) {
    skipWhitespaceAndComments(input, pos);
    while (pos < end && (input[pos] == ',' || input[pos] == ';')) {
      ++pos;
      skipWhitespaceAndComments(input, pos);
    }
    if (pos >= end) {
      return;
    }
    if (!isIdentifierStart(input[pos])) {
      ++pos;
      continue;
    }
    std::string name;
    if (!parseIdentifier(input, pos, name)) {
      ++pos;
      continue;
    }
    size_t scan = pos;
    skipWhitespaceAndComments(input, scan);
    if ((name == "text" || name == "semantic") && scan < end && input[scan] == '(') {
      size_t groupClose = findMatchingCloseWithComments(input, scan, '(', ')');
      if (groupClose != std::string::npos && groupClose <= end) {
        size_t groupPos = scan + 1;
        scanTransformEntries(input, groupPos, groupClose, name == "text" ? ListPhase::Text : ListPhase::Semantic,
                             textTransforms);
        pos = groupClose + 1;
        continue;
      }
    }
    appendTextTransform(name, phase, textTransforms);
    scan = pos;
    skipWhitespaceAndComments(input, scan);
    if (scan < end && input[scan] == '<') {
      size_t close = findMatchingCloseWithComments(input, scan, '<', '>');
      if (close != std::string::npos && close <= end) {
        scan = close + 1;
      }
    }
    pos = scan;
    skipWhitespaceAndComments(input, pos);
    if (pos < end && input[pos] == '(') {
      size_t close = findMatchingCloseWithComments(input, pos, '(', ')');
      if (close != std::string::npos && close <= end) {
        pos = close + 1;
      }
    }
  }
}

bool scanTransformList(const std::string &input, size_t start, TransformListScan &out) {
  size_t close = findMatchingCloseWithComments(input, start, '[', ']');
  if (close == std::string::npos) {
    return false;
  }
  out.end = close;
  out.textTransforms.clear();
  size_t pos = start + 1;
  scanTransformEntries(input, pos, close, ListPhase::Auto, out.textTransforms);
  return true;
}

bool scanLambdaEnvelope(const std::string &input, size_t listStart, size_t &listEnd, size_t &envelopeEnd) {
  listEnd = findMatchingCloseWithComments(input, listStart, '[', ']');
  if (listEnd == std::string::npos) {
    return false;
  }
  size_t pos = listEnd + 1;
  skipWhitespaceAndComments(input, pos);
  if (pos >= input.size()) {
    return false;
  }
  if (input[pos] == '<') {
    size_t close = findMatchingCloseWithComments(input, pos, '<', '>');
    if (close == std::string::npos) {
      return false;
    }
    pos = close + 1;
    skipWhitespaceAndComments(input, pos);
  }
  if (pos >= input.size() || input[pos] != '(') {
    return false;
  }
  size_t closeParen = findMatchingCloseWithComments(input, pos, '(', ')');
  if (closeParen == std::string::npos) {
    return false;
  }
  pos = closeParen + 1;
  skipWhitespaceAndComments(input, pos);
  if (pos >= input.size() || input[pos] != '{') {
    return false;
  }
  size_t closeBrace = findMatchingCloseWithComments(input, pos, '{', '}');
  if (closeBrace == std::string::npos) {
    return false;
  }
  envelopeEnd = closeBrace;
  return true;
}

bool scanEnvelopeAfterList(const std::string &input, size_t start, size_t &envelopeEnd, std::string &nameOut) {
  size_t pos = start;
  skipWhitespaceAndComments(input, pos);
  std::string name;
  if (!parseIdentifier(input, pos, name)) {
    return false;
  }
  nameOut = name;
  skipWhitespaceAndComments(input, pos);
  if (pos < input.size() && input[pos] == '<') {
    size_t close = findMatchingCloseWithComments(input, pos, '<', '>');
    if (close == std::string::npos) {
      return false;
    }
    pos = close + 1;
    skipWhitespaceAndComments(input, pos);
  }
  if (pos >= input.size()) {
    return false;
  }
  if (input[pos] == '{') {
    size_t closeBrace = findMatchingCloseWithComments(input, pos, '{', '}');
    if (closeBrace == std::string::npos) {
      return false;
    }
    envelopeEnd = closeBrace;
    return true;
  }
  if (input[pos] != '(') {
    return false;
  }
  size_t closeParen = findMatchingCloseWithComments(input, pos, '(', ')');
  if (closeParen == std::string::npos) {
    return false;
  }
  size_t afterParams = closeParen + 1;
  skipWhitespaceAndComments(input, afterParams);
  if (afterParams < input.size() && input[afterParams] == '{') {
    size_t closeBrace = findMatchingCloseWithComments(input, afterParams, '{', '}');
    if (closeBrace == std::string::npos) {
      return false;
    }
    envelopeEnd = closeBrace;
    return true;
  }
  envelopeEnd = closeParen;
  return true;
}

// A `(` opens call parentheses when it follows a name or a template list
// directly and its group is not a definition's parameter list or a control
// body header (`name(...) {`).
bool isCallParenAt(const std::string &input, size_t openIndex) {
  if (openIndex == 0) {
    return false;
  }
  const char prev = input[openIndex - 1];
  if (!isIdentifierBody(prev) && prev != '>') {
    return false;
  }
  size_t close = findMatchingCloseWithComments(input, openIndex, '(', ')');
  if (close == std::string::npos) {
    return false;
  }
  size_t after = close + 1;
  skipWhitespaceAndComments(input, after);
  return after >= input.size() || input[after] != '{';
}

// A `{` opens the braces of a value construction (`Type{...}`,
// `Type<T>{...}`) when it follows a type name directly; `else {` and
// `namespace name {` open blocks.
static bool isConstructionBraceAt(const std::string &input, size_t openIndex) {
  if (openIndex == 0) {
    return false;
  }
  const char prev = input[openIndex - 1];
  if (prev == '>') {
    return true;
  }
  if (!isIdentifierBody(prev)) {
    return false;
  }
  size_t wordStart = openIndex;
  while (wordStart > 0 && isIdentifierBody(input[wordStart - 1])) {
    --wordStart;
  }
  if (input.compare(wordStart, openIndex - wordStart, "else") == 0) {
    return false;
  }
  size_t before = wordStart;
  while (before > 0 && std::isspace(static_cast<unsigned char>(input[before - 1]))) {
    --before;
  }
  return !(before >= 9 && before < wordStart && input.compare(before - 9, 9, "namespace") == 0);
}

// `[name]` with a single plain identifier that is not a text or semantic transform (`[operators] add(...)` is a
// transform list on an execution): the label of an argument.
static bool isArgumentLabelList(const std::string &input, size_t listStart, size_t listEnd) {
  size_t pos = listStart + 1;
  while (pos < listEnd && std::isspace(static_cast<unsigned char>(input[pos]))) {
    ++pos;
  }
  if (pos >= listEnd || !isIdentifierStart(input[pos])) {
    return false;
  }
  const size_t nameStart = pos;
  while (pos < listEnd && isIdentifierBody(input[pos])) {
    ++pos;
  }
  const std::string name = input.substr(nameStart, pos - nameStart);
  while (pos < listEnd && std::isspace(static_cast<unsigned char>(input[pos]))) {
    ++pos;
  }
  return pos == listEnd && !primec::isTextTransformName(name) &&
         !primec::isSemanticTransformName(name);
}

size_t findNextTransformListStart(const std::string &input, size_t start, bool lambdasJoinCalls) {
  size_t pos = start;
  // The `(` and `{` open since `start`. The scan starts in statement
  // position, so an empty stack is a statement context.
  struct Frame {
    char open;
    size_t index;
  };
  std::vector<Frame> frames;
  while (pos < input.size()) {
    char c = input[pos];
    if (c == '"' || c == '\'') {
      size_t end = skipQuotedForward(input, pos);
      if (end == std::string::npos) {
        return std::string::npos;
      }
      pos = end;
      continue;
    }
    if (c == 'R' && pos + 2 < input.size() && input[pos + 1] == '"' && input[pos + 2] == '(') {
      size_t end = input.find(")\"", pos + 3);
      if (end == std::string::npos) {
        return std::string::npos;
      }
      pos = end + 2;
      continue;
    }
    if (c == '/' && pos + 1 < input.size()) {
      if (input[pos + 1] == '/') {
        pos += 2;
        while (pos < input.size() && input[pos] != '\n') {
          ++pos;
        }
        continue;
      }
      if (input[pos + 1] == '*') {
        size_t end = input.find("*/", pos + 2);
        if (end == std::string::npos) {
          return std::string::npos;
        }
        pos = end + 2;
        continue;
      }
    }
    if (c == '(' || c == '{') {
      frames.push_back({c, pos});
      ++pos;
      continue;
    }
    if (c == ')' || c == '}') {
      if (!frames.empty()) {
        frames.pop_back();
      }
      ++pos;
      continue;
    }
    if (c == '[' && isTransformListBoundary(input, pos)) {
      size_t listEnd = std::string::npos;
      size_t lambdaEnd = std::string::npos;
      const bool inCallParens =
          !frames.empty() && frames.back().open == '(' && isCallParenAt(input, frames.back().index);
      if (scanLambdaEnvelope(input, pos, listEnd, lambdaEnd)) {
        pos = inCallParens && lambdasJoinCalls ? lambdaEnd + 1 : listEnd + 1;
        continue;
      }
      if (!frames.empty() &&
          (inCallParens ||
           (frames.back().open == '{' && isConstructionBraceAt(input, frames.back().index)))) {
        const size_t labelEnd = findMatchingCloseWithComments(input, pos, '[', ']');
        if (labelEnd != std::string::npos && isArgumentLabelList(input, pos, labelEnd)) {
          pos = labelEnd + 1;
          continue;
        }
      }
      return pos;
    }
    ++pos;
  }
  return std::string::npos;
}

bool appendOperatorsTransform(const std::string &input, std::string &output, std::string &error) {
  output = input;
  size_t scanPos = 0;
  while (scanPos < output.size()) {
    size_t listStart = findNextTransformListStart(output, scanPos);
    if (listStart == std::string::npos) {
      break;
    }
    TransformListScan listScan;
    if (!scanTransformList(output, listStart, listScan)) {
      error = "unterminated transform list";
      return false;
    }
    bool hasAppendOperators = false;
    bool hasOperators = false;
    for (const auto &name : listScan.textTransforms) {
      if (name == "append_operators") {
        hasAppendOperators = true;
      } else if (name == "operators") {
        hasOperators = true;
      }
    }
    if (hasAppendOperators && !hasOperators) {
      bool hasContent = false;
      for (size_t pos = listStart + 1; pos < listScan.end; ++pos) {
        if (!std::isspace(static_cast<unsigned char>(output[pos]))) {
          hasContent = true;
          break;
        }
      }
      std::string insertText = "operators";
      if (hasContent) {
        insertText = " " + insertText;
      }
      output.insert(listScan.end, insertText);
      listScan.end += insertText.size();
    }
    scanPos = listScan.end + 1;
  }
  return true;
}


} // namespace primec
