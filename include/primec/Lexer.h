#pragma once

#include <string>
#include <vector>

#include "primec/Token.h"

namespace primec {

class Lexer {
public:
  explicit Lexer(const std::string &source);

  std::vector<Token> tokenize();

private:
  bool isIdentifierStart(char c) const;
  bool isIdentifierBody(char c) const;
  void skipWhitespace();
  void advance();

  Token readIdentifier();
  Token readNumber();
  Token readString(char quote);
  Token readRawString();
  Token readPunct();

  const std::string &source_;
  size_t pos_ = 0;
  int line_ = 1;
  int column_ = 1;
};

} // namespace primec
