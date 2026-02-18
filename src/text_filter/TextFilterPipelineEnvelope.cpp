#include "TextFilterPipelineInternal.h"

#include "TextFilterHelpers.h"
#include "primec/TextFilterPipeline.h"
#include "primec/TransformRules.h"

#include <unordered_set>

namespace primec {
using namespace text_filter;

bool applyPerEnvelope(const std::string &input,
                      std::string &output,
                      std::string &error,
                      const std::vector<std::string> &filters,
                      const std::vector<TextTransformRule> &rules,
                      bool suppressLeadingOverride,
                      const std::string &baseNamespacePrefix,
                      const std::string &baseDefinitionPrefix,
                      const std::vector<std::string> &baseFilters);

namespace {

struct NamespaceBlock {
  std::string name;
  size_t end = std::string::npos;
};

struct DefinitionBlock {
  std::string name;
  size_t end = std::string::npos;
};

bool isReservedKeywordName(const std::string &name) {
  return name == "mut" || name == "return" || name == "include" || name == "import" || name == "namespace" ||
         name == "true" || name == "false" || name == "if" || name == "else" || name == "loop" || name == "while" ||
         name == "for";
}

bool isNamespaceKeywordAt(const std::string &input, size_t pos) {
  constexpr const char *kNamespace = "namespace";
  constexpr size_t kNamespaceLen = 9;
  if (pos + kNamespaceLen > input.size()) {
    return false;
  }
  if (input.compare(pos, kNamespaceLen, kNamespace) != 0) {
    return false;
  }
  if (pos > 0 && isIdentifierBody(input[pos - 1])) {
    return false;
  }
  size_t after = pos + kNamespaceLen;
  if (after < input.size() && isIdentifierBody(input[after])) {
    return false;
  }
  return true;
}

bool parseNamespaceBlock(const std::string &input, size_t start, size_t &afterPos, NamespaceBlock &block) {
  if (!isNamespaceKeywordAt(input, start)) {
    return false;
  }
  size_t pos = start + 9;
  skipWhitespaceAndComments(input, pos);
  std::string name;
  if (!parseIdentifier(input, pos, name)) {
    return false;
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  skipWhitespaceAndComments(input, pos);
  if (pos >= input.size() || input[pos] != '{') {
    return false;
  }
  size_t close = findMatchingCloseWithComments(input, pos, '{', '}');
  if (close == std::string::npos) {
    return false;
  }
  block.name = name;
  block.end = close;
  afterPos = pos + 1;
  return true;
}

bool isRawStringStart(const std::string &input, size_t pos, size_t limit) {
  return pos + 2 < limit && input[pos] == 'R' && input[pos + 1] == '"' && input[pos + 2] == '(';
}

bool isBindingParamBracket(const std::string &input, size_t index, size_t limit) {
  if (index >= limit || input[index] != '[') {
    return false;
  }
  size_t scan = index + 1;
  skipWhitespaceAndComments(input, scan);
  if (scan >= limit || !isIdentifierStart(input[scan])) {
    return true;
  }
  std::string name;
  parseIdentifier(input, scan, name);
  skipWhitespaceAndComments(input, scan);
  if (scan >= limit || input[scan] != ']') {
    return true;
  }
  ++scan;
  skipWhitespaceAndComments(input, scan);
  if (scan >= limit) {
    return true;
  }
  char c = input[scan];
  if (isDigitChar(c) || c == '"' || c == '\'') {
    return false;
  }
  if (isRawStringStart(input, scan, limit)) {
    return false;
  }
  if (isIdentifierStart(c)) {
    std::string nextName;
    parseIdentifier(input, scan, nextName);
    if (nextName == "true" || nextName == "false") {
      return false;
    }
    size_t follow = scan;
    skipWhitespaceAndComments(input, follow);
    if (follow < limit) {
      char f = input[follow];
      if (f == '(' || f == '.' || f == '[' || f == '<') {
        return false;
      }
      if (f == '{') {
        return true;
      }
    }
    return true;
  }
  return true;
}

bool isDefinitionParamList(const std::string &input, size_t openParen, size_t closeParen) {
  size_t pos = openParen + 1;
  bool sawContent = false;
  bool sawBinding = false;
  enum class DepthToken { LParen, Comma, Other };
  DepthToken prevAtDepth1 = DepthToken::LParen;
  int parenDepth = 1;
  int braceDepth = 0;
  while (pos < closeParen) {
    skipWhitespaceAndComments(input, pos);
    if (pos >= closeParen) {
      break;
    }
    char c = input[pos];
    if (c == '"' || c == '\'') {
      size_t end = skipQuotedForward(input, pos);
      if (end == std::string::npos) {
        return true;
      }
      pos = end;
      sawContent = true;
      prevAtDepth1 = DepthToken::Other;
      continue;
    }
    if (isRawStringStart(input, pos, closeParen)) {
      size_t end = input.find(")\"", pos + 3);
      if (end == std::string::npos) {
        return true;
      }
      pos = end + 2;
      sawContent = true;
      prevAtDepth1 = DepthToken::Other;
      continue;
    }
    if (c == '(') {
      ++parenDepth;
      ++pos;
      sawContent = true;
      prevAtDepth1 = DepthToken::Other;
      continue;
    }
    if (c == ')') {
      --parenDepth;
      ++pos;
      prevAtDepth1 = DepthToken::Other;
      continue;
    }
    if (parenDepth != 1) {
      ++pos;
      continue;
    }
    if (c == '{') {
      ++braceDepth;
      ++pos;
      sawContent = true;
      prevAtDepth1 = DepthToken::Other;
      continue;
    }
    if (c == '}') {
      if (braceDepth > 0) {
        --braceDepth;
      }
      ++pos;
      prevAtDepth1 = DepthToken::Other;
      continue;
    }
    if (braceDepth > 0) {
      ++pos;
      continue;
    }
    if (c == ',' || c == ';') {
      prevAtDepth1 = DepthToken::Comma;
      ++pos;
      continue;
    }
    if (c == '[') {
      sawContent = true;
      const bool atArgStart = (prevAtDepth1 == DepthToken::LParen || prevAtDepth1 == DepthToken::Comma);
      if (atArgStart && isBindingParamBracket(input, pos, closeParen)) {
        sawBinding = true;
      }
      size_t close = findMatchingCloseWithComments(input, pos, '[', ']');
      if (close == std::string::npos || close > closeParen) {
        return true;
      }
      pos = close + 1;
      prevAtDepth1 = DepthToken::Other;
      continue;
    }
    if (isIdentifierStart(c)) {
      sawContent = true;
      std::string name;
      parseIdentifier(input, pos, name);
      prevAtDepth1 = DepthToken::Other;
      continue;
    }
    sawContent = true;
    prevAtDepth1 = DepthToken::Other;
    ++pos;
  }
  if (sawBinding) {
    return true;
  }
  if (!sawContent) {
    return true;
  }
  return false;
}

bool parseDefinitionBlock(const std::string &input, size_t start, size_t &afterPos, DefinitionBlock &block) {
  size_t pos = start;
  skipWhitespaceAndComments(input, pos);
  std::string name;
  if (!parseIdentifier(input, pos, name)) {
    return false;
  }
  if (isReservedKeywordName(name)) {
    return false;
  }
  skipWhitespaceAndComments(input, pos);
  if (pos < input.size() && input[pos] == '<') {
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
  if (!isDefinitionParamList(input, pos, closeParen)) {
    return false;
  }
  size_t afterParams = closeParen + 1;
  skipWhitespaceAndComments(input, afterParams);
  if (afterParams >= input.size() || input[afterParams] != '{') {
    return false;
  }
  size_t closeBrace = findMatchingCloseWithComments(input, afterParams, '{', '}');
  if (closeBrace == std::string::npos) {
    return false;
  }
  block.name = name;
  block.end = closeBrace;
  afterPos = afterParams + 1;
  return true;
}

void advanceNamespaceScan(const std::string &input,
                          size_t &scanPos,
                          size_t targetPos,
                          std::vector<NamespaceBlock> &stack) {
  while (scanPos < targetPos) {
    char c = input[scanPos];
    if (c == '"' || c == '\'') {
      size_t end = skipQuotedForward(input, scanPos);
      if (end == std::string::npos) {
        scanPos = targetPos;
        break;
      }
      scanPos = end;
      continue;
    }
    if (c == 'R' && scanPos + 2 < input.size() && input[scanPos + 1] == '"' && input[scanPos + 2] == '(') {
      size_t end = input.find(")\"", scanPos + 3);
      if (end == std::string::npos) {
        scanPos = targetPos;
        break;
      }
      scanPos = end + 2;
      continue;
    }
    if (c == '/' && scanPos + 1 < input.size()) {
      if (input[scanPos + 1] == '/') {
        scanPos += 2;
        while (scanPos < targetPos && input[scanPos] != '\n') {
          ++scanPos;
        }
        continue;
      }
      if (input[scanPos + 1] == '*') {
        size_t end = input.find("*/", scanPos + 2);
        if (end == std::string::npos) {
          scanPos = targetPos;
          break;
        }
        scanPos = end + 2;
        continue;
      }
    }
    NamespaceBlock block;
    size_t afterPos = scanPos;
    if (parseNamespaceBlock(input, scanPos, afterPos, block)) {
      stack.push_back(std::move(block));
      scanPos = afterPos;
      continue;
    }
    ++scanPos;
  }
  while (!stack.empty() && stack.back().end < targetPos) {
    stack.pop_back();
  }
}

void advanceDefinitionScan(const std::string &input,
                           size_t &scanPos,
                           size_t targetPos,
                           std::vector<DefinitionBlock> &stack,
                           int targetBodyDepth) {
  int parenDepth = 0;
  int bodyDepth = 0;
  while (scanPos < targetPos) {
    char c = input[scanPos];
    if (c == '"' || c == '\'') {
      size_t end = skipQuotedForward(input, scanPos);
      if (end == std::string::npos) {
        scanPos = targetPos;
        break;
      }
      scanPos = end;
      continue;
    }
    if (c == 'R' && scanPos + 2 < input.size() && input[scanPos + 1] == '"' && input[scanPos + 2] == '(') {
      size_t end = input.find(")\"", scanPos + 3);
      if (end == std::string::npos) {
        scanPos = targetPos;
        break;
      }
      scanPos = end + 2;
      continue;
    }
    if (c == '/' && scanPos + 1 < input.size()) {
      if (input[scanPos + 1] == '/') {
        scanPos += 2;
        while (scanPos < targetPos && input[scanPos] != '\n') {
          ++scanPos;
        }
        continue;
      }
      if (input[scanPos + 1] == '*') {
        size_t end = input.find("*/", scanPos + 2);
        if (end == std::string::npos) {
          scanPos = targetPos;
          break;
        }
        scanPos = end + 2;
        continue;
      }
    }
    if (c == '[' && isTransformListBoundary(input, scanPos)) {
      size_t close = findMatchingCloseWithComments(input, scanPos, '[', ']');
      if (close == std::string::npos) {
        scanPos = targetPos;
        break;
      }
      scanPos = close + 1;
      continue;
    }
    if (c == '(') {
      ++parenDepth;
      ++scanPos;
      continue;
    }
    if (c == ')') {
      if (parenDepth > 0) {
        --parenDepth;
      }
      ++scanPos;
      continue;
    }
    if (c == '{') {
      ++bodyDepth;
      ++scanPos;
      continue;
    }
    if (c == '}') {
      if (bodyDepth > 0) {
        --bodyDepth;
      }
      ++scanPos;
      continue;
    }
    if (bodyDepth == targetBodyDepth && parenDepth == 0 && isIdentifierStart(c)) {
      if (scanPos > 0 && isIdentifierBody(input[scanPos - 1])) {
        ++scanPos;
        continue;
      }
      DefinitionBlock block;
      size_t afterPos = scanPos;
      if (parseDefinitionBlock(input, scanPos, afterPos, block)) {
        stack.push_back(std::move(block));
        scanPos = afterPos;
        continue;
      }
    }
    ++scanPos;
  }
  while (!stack.empty() && stack.back().end < targetPos) {
    stack.pop_back();
  }
}

size_t findNextEnvelopeStart(const std::string &input, size_t start, int targetBodyDepth) {
  size_t pos = start;
  int parenDepth = 0;
  int bodyDepth = 0;
  std::vector<NamespaceBlock> namespaceStack;
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
    if (c == '[') {
      if (bodyDepth == targetBodyDepth) {
        size_t listEnd = std::string::npos;
        size_t lambdaEnd = std::string::npos;
        if (scanLambdaEnvelope(input, pos, listEnd, lambdaEnd)) {
          return pos;
        }
      }
      size_t close = findMatchingCloseWithComments(input, pos, '[', ']');
      if (close == std::string::npos) {
        return std::string::npos;
      }
      pos = close + 1;
      continue;
    }
    if (!namespaceStack.empty() && pos == namespaceStack.back().end && input[pos] == '}') {
      namespaceStack.pop_back();
      ++pos;
      continue;
    }
    NamespaceBlock block;
    size_t afterPos = pos;
    if (parseNamespaceBlock(input, pos, afterPos, block)) {
      namespaceStack.push_back(std::move(block));
      pos = afterPos;
      continue;
    }
    if (c == '(') {
      ++parenDepth;
      ++pos;
      continue;
    }
    if (c == ')') {
      if (parenDepth > 0) {
        --parenDepth;
      }
      ++pos;
      continue;
    }
    if (c == '{') {
      ++bodyDepth;
      ++pos;
      continue;
    }
    if (c == '}') {
      if (bodyDepth > 0) {
        --bodyDepth;
      }
      ++pos;
      continue;
    }
    if (bodyDepth == targetBodyDepth && parenDepth == 0 && isIdentifierStart(c)) {
      if (pos > 0 && isIdentifierBody(input[pos - 1])) {
        ++pos;
        continue;
      }
      if (targetBodyDepth > 0) {
        DefinitionBlock block;
        size_t afterPos = pos;
        if (parseDefinitionBlock(input, pos, afterPos, block)) {
          return pos;
        }
        size_t namePos = pos;
        std::string name;
        if (parseIdentifier(input, namePos, name)) {
          pos = namePos;
          continue;
        }
        ++pos;
        continue;
      }
      size_t namePos = pos;
      std::string name;
      if (!parseIdentifier(input, namePos, name)) {
        ++pos;
        continue;
      }
      if (name == "namespace" || name == "import" || name == "include") {
        pos = namePos;
        continue;
      }
      size_t probe = pos;
      size_t envelopeEnd = std::string::npos;
      std::string envelopeName;
      if (scanEnvelopeAfterList(input, probe, envelopeEnd, envelopeName)) {
        return pos;
      }
      pos = namePos;
      continue;
    }
    ++pos;
  }
  return std::string::npos;
}

std::string buildNamespacePrefix(const std::string &basePrefix, const std::vector<NamespaceBlock> &stack) {
  std::string prefix = basePrefix;
  for (const auto &entry : stack) {
    if (prefix.empty()) {
      prefix = "/" + entry.name;
    } else {
      prefix.append("/");
      prefix.append(entry.name);
    }
  }
  return prefix;
}

std::string buildDefinitionPrefix(const std::string &basePrefix, const std::vector<DefinitionBlock> &stack) {
  std::string prefix = basePrefix;
  for (const auto &entry : stack) {
    if (prefix.empty()) {
      prefix = "/" + entry.name;
    } else {
      prefix.append("/");
      prefix.append(entry.name);
    }
  }
  return prefix;
}

std::string makeFullPath(const std::string &name, const std::string &prefix) {
  if (!name.empty() && name[0] == '/') {
    return name;
  }
  if (prefix.empty()) {
    return "/" + name;
  }
  return prefix + "/" + name;
}

bool containsFilterName(const std::vector<std::string> &filters, const std::string &name) {
  for (const auto &filter : filters) {
    if (filter == name) {
      return true;
    }
  }
  return false;
}

bool filtersEqual(const std::vector<std::string> &left, const std::vector<std::string> &right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (size_t i = 0; i < left.size(); ++i) {
    if (left[i] != right[i]) {
      return false;
    }
  }
  return true;
}

bool scanLeadingTransformList(const std::string &input, std::vector<std::string> &out) {
  size_t pos = 0;
  skipWhitespaceAndComments(input, pos);
  if (pos >= input.size() || input[pos] != '[') {
    return false;
  }
  if (!isTransformListBoundary(input, pos)) {
    return false;
  }
  size_t listEnd = std::string::npos;
  size_t lambdaEnd = std::string::npos;
  if (scanLambdaEnvelope(input, pos, listEnd, lambdaEnd)) {
    return false;
  }
  TransformListScan listScan;
  if (!scanTransformList(input, pos, listScan)) {
    return false;
  }
  out = listScan.textTransforms;
  return true;
}

bool applyFilterToChunk(const std::string &input,
                        std::string &output,
                        std::string &error,
                        const std::string *filter) {
  if (filter == nullptr || filter->empty()) {
    output = input;
    return true;
  }
  TextFilterOptions passOptions;
  passOptions.enabledFilters = {*filter};
  return applyPass(input, output, error, passOptions);
}

bool applyPerEnvelopeFilterPass(const std::string &input,
                                std::string &output,
                                std::string &error,
                                const std::string &filter,
                                const std::vector<std::string> &activeFilters,
                                const std::vector<TextTransformRule> &rules,
                                bool suppressLeadingOverride,
                                const std::string &baseNamespacePrefix,
                                const std::string &baseDefinitionPrefix,
                                bool allowExplicitRecursion,
                                const std::vector<std::string> &baseFilters) {
  output.clear();
  size_t pos = 0;
  size_t scanPos = 0;
  bool suppress = suppressLeadingOverride;
  const int targetBodyDepth = suppressLeadingOverride ? 1 : 0;
  std::vector<NamespaceBlock> namespaceStack;
  std::vector<DefinitionBlock> definitionStack;
  const bool filterActive = containsFilterName(activeFilters, filter);
  while (scanPos < input.size()) {
    size_t listStart = findNextTransformListStart(input, scanPos);
    size_t envelopeStart = findNextEnvelopeStart(input, scanPos, targetBodyDepth);
    size_t nextStart = std::string::npos;
    bool isTransformList = false;
    if (listStart != std::string::npos &&
        (envelopeStart == std::string::npos || listStart < envelopeStart)) {
      nextStart = listStart;
      isTransformList = true;
    } else if (envelopeStart != std::string::npos) {
      nextStart = envelopeStart;
      isTransformList = false;
    }
    if (nextStart == std::string::npos) {
      break;
    }
    size_t contextPos = scanPos;
    advanceNamespaceScan(input, contextPos, nextStart, namespaceStack);
    contextPos = scanPos;
    advanceDefinitionScan(input, contextPos, nextStart, definitionStack, targetBodyDepth);
    scanPos = nextStart;
    std::string namespacePrefix = buildNamespacePrefix(baseNamespacePrefix, namespaceStack);
    std::string definitionBase = baseDefinitionPrefix.empty() ? namespacePrefix : baseDefinitionPrefix;
    std::string definitionPrefix = buildDefinitionPrefix(definitionBase, definitionStack);
    size_t envelopeStartPos = nextStart;
    size_t envelopeEnd = std::string::npos;
  std::string envelopeName;
  std::vector<std::string> explicitTransforms;
  bool envelopeIsDefinition = false;
  bool envelopeIsLambda = false;
  std::string definitionFullPath;
  if (isTransformList) {
    TransformListScan listScan;
    if (!scanTransformList(input, listStart, listScan)) {
        break;
      }
      if (suppress && listStart == 0) {
        scanPos = listScan.end + 1;
        suppress = false;
        continue;
      }
      if (!scanEnvelopeAfterList(input, listScan.end + 1, envelopeEnd, envelopeName)) {
        scanPos = listScan.end + 1;
        suppress = false;
        continue;
      }
      DefinitionBlock definitionBlock;
      size_t afterPos = listScan.end + 1;
      if (parseDefinitionBlock(input, listScan.end + 1, afterPos, definitionBlock)) {
        envelopeIsDefinition = true;
        definitionFullPath = makeFullPath(definitionBlock.name, definitionPrefix);
      }
      explicitTransforms = std::move(listScan.textTransforms);
    } else {
      if (suppress && envelopeStart == 0) {
        scanPos = envelopeStart + 1;
        suppress = false;
        continue;
      }
      if (input[envelopeStart] == '[') {
        size_t listEnd = std::string::npos;
        if (!scanLambdaEnvelope(input, envelopeStart, listEnd, envelopeEnd)) {
          scanPos = envelopeStart + 1;
          suppress = false;
          continue;
        }
        envelopeIsLambda = true;
        envelopeName.clear();
      } else {
        size_t probe = envelopeStart;
        if (!scanEnvelopeAfterList(input, probe, envelopeEnd, envelopeName)) {
          scanPos = envelopeStart + 1;
          suppress = false;
          continue;
        }
        DefinitionBlock definitionBlock;
        size_t afterPos = envelopeStart;
        if (parseDefinitionBlock(input, envelopeStart, afterPos, definitionBlock)) {
          envelopeIsDefinition = true;
          definitionFullPath = makeFullPath(definitionBlock.name, definitionPrefix);
        }
      }
    }
    std::string chunk = input.substr(pos, envelopeStartPos - pos);
    std::string filteredChunk;
    if (!applyFilterToChunk(chunk, filteredChunk, error, filterActive ? &filter : nullptr)) {
      return false;
    }
    output.append(filteredChunk);
    std::string envelope = input.substr(envelopeStartPos, envelopeEnd - envelopeStartPos + 1);
    const std::vector<std::string> *autoFilters = &activeFilters;
    if (envelopeIsDefinition || envelopeIsLambda) {
      autoFilters = &baseFilters;
    }
    if (!rules.empty() && !envelopeIsLambda) {
      std::string fullPath = makeFullPath(envelopeName, envelopeIsDefinition ? definitionPrefix : namespacePrefix);
      if (const auto *ruleFilters = selectRuleTransforms(rules, fullPath)) {
        autoFilters = ruleFilters;
      }
    }
    const std::vector<std::string> &envelopeFilters =
        explicitTransforms.empty() ? *autoFilters : explicitTransforms;
    const bool inheritsFilters = filtersEqual(envelopeFilters, activeFilters);
    std::string filteredEnvelope;
    const std::string &nextDefinitionPrefix = envelopeIsDefinition ? definitionFullPath : baseDefinitionPrefix;
    if (inheritsFilters) {
      if (!applyPerEnvelopeFilterPass(envelope,
                                      filteredEnvelope,
                                      error,
                                      filter,
                                      envelopeFilters,
                                      rules,
                                      true,
                                      namespacePrefix,
                                      nextDefinitionPrefix,
                                      allowExplicitRecursion,
                                      baseFilters)) {
        return false;
      }
      output.append(filteredEnvelope);
    } else if (allowExplicitRecursion) {
      if (!applyPerEnvelope(envelope,
                            filteredEnvelope,
                            error,
                            envelopeFilters,
                            rules,
                            true,
                            namespacePrefix,
                            nextDefinitionPrefix,
                            baseFilters)) {
        return false;
      }
      output.append(filteredEnvelope);
    } else {
      output.append(envelope);
    }
    pos = envelopeEnd + 1;
    scanPos = pos;
    suppress = false;
  }
  std::string tail = input.substr(pos);
  std::string filteredTail;
  if (!applyFilterToChunk(tail, filteredTail, error, filterActive ? &filter : nullptr)) {
    return false;
  }
  output.append(filteredTail);
  return true;
}

} // namespace

bool applyPerEnvelope(const std::string &input,
                      std::string &output,
                      std::string &error,
                      const std::vector<std::string> &filters,
                      const std::vector<TextTransformRule> &rules,
                      bool suppressLeadingOverride,
                      const std::string &baseNamespacePrefix,
                      const std::string &baseDefinitionPrefix,
                      const std::vector<std::string> &baseFilters) {
  std::vector<std::string> activeFilters = filters;
  if (activeFilters.empty()) {
    return applyPerEnvelopeFilterPass(input,
                                      output,
                                      error,
                                      std::string(),
                                      activeFilters,
                                      rules,
                                      suppressLeadingOverride,
                                      baseNamespacePrefix,
                                      baseDefinitionPrefix,
                                      true,
                                      baseFilters);
  }
  std::unordered_set<std::string> applied;
  std::string current = input;
  bool allowExplicitRecursion = true;
  for (size_t i = 0; i < activeFilters.size(); ++i) {
    const std::string &filter = activeFilters[i];
    if (!applied.insert(filter).second) {
      continue;
    }
    std::string next;
    if (!applyPerEnvelopeFilterPass(current,
                                    next,
                                    error,
                                    filter,
                                    activeFilters,
                                    rules,
                                    suppressLeadingOverride,
                                    baseNamespacePrefix,
                                    baseDefinitionPrefix,
                                    allowExplicitRecursion,
                                    baseFilters)) {
      return false;
    }
    current.swap(next);
    allowExplicitRecursion = false;
    std::vector<std::string> listTransforms;
    if (scanLeadingTransformList(current, listTransforms)) {
      for (const auto &name : listTransforms) {
        if (applied.find(name) != applied.end()) {
          continue;
        }
        if (!containsFilterName(activeFilters, name)) {
          activeFilters.push_back(name);
        }
      }
    }
  }
  output = current;
  return true;
}

} // namespace primec
