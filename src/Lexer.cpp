#include "primec/Lexer.h"

#include <cctype>

namespace primec {

Lexer::Lexer(const std::string &source) : source_(source) {}

std::vector<Token> Lexer::tokenize() {
  std::vector<Token> tokens;
  while (true) {
    skipWhitespace();
    if (pos_ >= source_.size()) {
      tokens.push_back({TokenKind::End, "", line_, column_});
      break;
    }
    char c = source_[pos_];
    if (isIdentifierStart(c)) {
      tokens.push_back(readIdentifier());
    } else if (std::isdigit(static_cast<unsigned char>(c)) ||
               (c == '-' && pos_ + 1 < source_.size() &&
                std::isdigit(static_cast<unsigned char>(source_[pos_ + 1])))) {
      tokens.push_back(readNumber());
    } else {
      tokens.push_back(readPunct());
    }
  }
  return tokens;
}

bool Lexer::isIdentifierStart(char c) const {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '/';
}

bool Lexer::isIdentifierBody(char c) const {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '/';
}

void Lexer::skipWhitespace() {
  while (pos_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[pos_]))) {
    advance();
  }
}

void Lexer::advance() {
  if (pos_ >= source_.size()) {
    return;
  }
  if (source_[pos_] == '\n') {
    ++line_;
    column_ = 1;
  } else {
    ++column_;
  }
  ++pos_;
}

Token Lexer::readIdentifier() {
  int startLine = line_;
  int startColumn = column_;
  size_t start = pos_;
  while (pos_ < source_.size() && isIdentifierBody(source_[pos_])) {
    advance();
  }
  std::string text = source_.substr(start, pos_ - start);
  if (text == "namespace") {
    return {TokenKind::KeywordNamespace, text, startLine, startColumn};
  }
  return {TokenKind::Identifier, text, startLine, startColumn};
}

Token Lexer::readNumber() {
  int startLine = line_;
  int startColumn = column_;
  size_t start = pos_;
  if (source_[pos_] == '-') {
    advance();
  }
  while (pos_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_]))) {
    advance();
  }
  if (source_.compare(pos_, 3, "i32") == 0) {
    advance();
    advance();
    advance();
  }
  return {TokenKind::Number, source_.substr(start, pos_ - start), startLine, startColumn};
}

Token Lexer::readPunct() {
  int startLine = line_;
  int startColumn = column_;
  char c = source_[pos_];
  advance();
  switch (c) {
  case '[':
    return {TokenKind::LBracket, "[", startLine, startColumn};
  case ']':
    return {TokenKind::RBracket, "]", startLine, startColumn};
  case '(':
    return {TokenKind::LParen, "(", startLine, startColumn};
  case ')':
    return {TokenKind::RParen, ")", startLine, startColumn};
  case '{':
    return {TokenKind::LBrace, "{", startLine, startColumn};
  case '}':
    return {TokenKind::RBrace, "}", startLine, startColumn};
  case '<':
    return {TokenKind::LAngle, "<", startLine, startColumn};
  case '>':
    return {TokenKind::RAngle, ">", startLine, startColumn};
  case ',':
    return {TokenKind::Comma, ",", startLine, startColumn};
  default:
    return {TokenKind::End, "", startLine, startColumn};
  }
}

} // namespace primec
