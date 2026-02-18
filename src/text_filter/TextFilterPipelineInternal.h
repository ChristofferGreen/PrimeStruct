#pragma once

#include <string>
#include <vector>

namespace primec {

struct TextFilterOptions;
struct TextTransformRule;

struct TransformListScan {
  size_t end = std::string::npos;
  std::vector<std::string> textTransforms;
};

bool applyPass(const std::string &input,
               std::string &output,
               std::string &error,
               const TextFilterOptions &options);

bool applyPerEnvelope(const std::string &input,
                      std::string &output,
                      std::string &error,
                      const std::vector<std::string> &filters,
                      const std::vector<TextTransformRule> &rules,
                      bool suppressLeadingOverride,
                      const std::string &baseNamespacePrefix,
                      const std::string &baseDefinitionPrefix,
                      const std::vector<std::string> &baseFilters);

bool isIdentifierStart(char c);
bool isIdentifierBody(char c);
bool isTransformListBoundary(const std::string &input, size_t index);
void skipWhitespaceAndComments(const std::string &input, size_t &pos);
size_t findMatchingCloseWithComments(const std::string &input, size_t openIndex, char openChar, char closeChar);
bool parseIdentifier(const std::string &input, size_t &pos, std::string &out);
bool scanTransformList(const std::string &input, size_t start, TransformListScan &out);
bool scanLambdaEnvelope(const std::string &input, size_t listStart, size_t &listEnd, size_t &envelopeEnd);
bool scanEnvelopeAfterList(const std::string &input, size_t start, size_t &envelopeEnd, std::string &nameOut);
size_t findNextTransformListStart(const std::string &input, size_t start);
bool appendOperatorsTransform(const std::string &input, std::string &output, std::string &error);

} // namespace primec
