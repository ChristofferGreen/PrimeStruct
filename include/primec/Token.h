#pragma once

#include <string>

namespace primec {

enum class TokenKind {
  Identifier,
  Number,
  LBracket,
  RBracket,
  LParen,
  RParen,
  LBrace,
  RBrace,
  LAngle,
  RAngle,
  Comma,
  KeywordNamespace,
  End
};

struct Token {
  TokenKind kind = TokenKind::End;
  std::string text;
  int line = 1;
  int column = 1;
};

} // namespace primec
