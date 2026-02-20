#pragma once

#include <string>

namespace primec {

enum class TokenKind {
  Identifier,
  Number,
  String,
  LBracket,
  RBracket,
  LParen,
  RParen,
  LBrace,
  RBrace,
  LAngle,
  RAngle,
  Comma,
  Dot,
  Equal,
  Star,
  Ampersand,
  Question,
  Semicolon,
  Comment,
  KeywordNamespace,
  KeywordImport,
  Invalid,
  End
};

struct Token {
  TokenKind kind = TokenKind::End;
  std::string text;
  int line = 1;
  int column = 1;
};

} // namespace primec
