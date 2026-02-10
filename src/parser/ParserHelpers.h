#pragma once

#include <string>

#include "primec/Token.h"

namespace primec::parser {

std::string describeInvalidToken(const Token &token);
bool validateIdentifierText(const std::string &text, std::string &error);
bool validateTransformName(const std::string &text, std::string &error);
bool isStructTransformName(const std::string &text);
bool isBuiltinName(const std::string &name, bool allowMathBare);
bool isHexDigitChar(char c);
bool isValidFloatLiteral(const std::string &text);

} // namespace primec::parser
