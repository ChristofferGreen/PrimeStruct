#pragma once

#include <string>
#include <vector>

#include "primec/Ast.h"
#include "primec/Token.h"

namespace primec::parser {

std::string describeInvalidToken(const Token &token);
bool validateIdentifierText(const std::string &text, std::string &error);
bool validateTransformName(const std::string &text, std::string &error);
bool isStructTransformName(const std::string &text);
bool isBindingAuxTransformName(const std::string &name);
bool hasExplicitBindingTypeTransform(const std::vector<Transform> &transforms);
bool isBindingTransformList(const std::vector<Transform> &transforms);
bool isBuiltinName(const std::string &name, bool allowMathBare);
bool isHexDigitChar(char c);
bool isValidFloatLiteral(const std::string &text);

} // namespace primec::parser
