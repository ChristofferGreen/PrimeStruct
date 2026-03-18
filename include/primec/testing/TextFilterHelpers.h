#pragma once

#include <string>

#include "primec/TextFilterPipeline.h"

namespace primec::text_filter {

bool isSeparator(char c);
bool isTokenChar(char c);
bool isStringSuffixStart(char c);
bool isStringSuffixBody(char c);
bool isOperatorTokenChar(char c);
bool isLeftOperandEndChar(char c);
bool isDigitChar(char c);
bool isHexDigitChar(char c);
bool isCommentStart(const std::string &input, size_t index);
bool isCommentEnd(const std::string &input, size_t index);
bool isUnaryPrefixPosition(const std::string &input, size_t index);
bool isRightOperandStartChar(const std::string &input, size_t index);
bool isExponentSign(const std::string &text, size_t index);
bool isEscaped(const std::string &text, size_t index);
size_t findQuotedStart(const std::string &text, size_t endQuote);
size_t skipQuotedForward(const std::string &text, size_t start);
size_t findMatchingClose(const std::string &text, size_t openIndex, char openChar, char closeChar);
size_t findMatchingOpen(const std::string &text, size_t closeIndex, char openChar, char closeChar);
size_t findLeftTokenStart(const std::string &text, size_t end);
size_t findRightTokenEnd(const std::string &text, size_t start);
std::string stripOuterParens(const std::string &text);
std::string normalizeUnaryOperand(const std::string &operand);
bool looksLikeTemplateList(const std::string &input, size_t index);
bool looksLikeTemplateListClose(const std::string &input, size_t index);
std::string maybeAppendI32(const std::string &token);
std::string maybeAppendUtf8(const std::string &token);
bool rewriteUnaryNot(const std::string &input,
                     std::string &output,
                     size_t &index,
                     const TextFilterOptions &options);
bool rewriteUnaryAddressOf(const std::string &input,
                           std::string &output,
                           size_t &index,
                           const TextFilterOptions &options);
bool rewriteUnaryDeref(const std::string &input,
                       std::string &output,
                       size_t &index,
                       const TextFilterOptions &options);
bool rewriteUnaryIncrement(const std::string &input,
                           std::string &output,
                           size_t &index,
                           const TextFilterOptions &options);
bool rewriteUnaryDecrement(const std::string &input,
                           std::string &output,
                           size_t &index,
                           const TextFilterOptions &options);
bool rewriteUnaryMinus(const std::string &input,
                       std::string &output,
                       size_t &index,
                       const TextFilterOptions &options);

} // namespace primec::text_filter
