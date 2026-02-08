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
    if (c == '"' || c == '\'') {
      tokens.push_back(readString(c));
    } else if (c == '/' && pos_ + 1 < source_.size() && (source_[pos_ + 1] == '/' || source_[pos_ + 1] == '*')) {
      Token comment = readComment();
      if (comment.kind == TokenKind::Invalid) {
        tokens.push_back(std::move(comment));
      }
    } else if (isIdentifierStart(c)) {
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

static bool isHexDigitChar(char c) {
  return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

static bool isAsciiAlpha(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static bool isAsciiDigit(char c) {
  return c >= '0' && c <= '9';
}

static bool isStringSuffixStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

static bool isStringSuffixBody(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool Lexer::isIdentifierStart(char c) const {
  return isAsciiAlpha(c) || c == '_' || c == '/';
}

bool Lexer::isIdentifierBody(char c) const {
  return isAsciiAlpha(c) || isAsciiDigit(c) || c == '_' || c == '/';
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
  bool isHex = false;
  if (pos_ + 1 < source_.size() && source_[pos_] == '0' && (source_[pos_ + 1] == 'x' || source_[pos_ + 1] == 'X')) {
    isHex = true;
    advance();
    advance();
    while (pos_ < source_.size() && isHexDigitChar(source_[pos_])) {
      advance();
    }
  } else {
    while (pos_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_]))) {
      advance();
    }
    if (pos_ < source_.size() && source_[pos_] == '.') {
      advance();
      while (pos_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[pos_]))) {
        advance();
      }
    }
    if (pos_ < source_.size() && (source_[pos_] == 'e' || source_[pos_] == 'E')) {
      size_t expPos = pos_;
      size_t scan = expPos + 1;
      if (scan < source_.size() && (source_[scan] == '+' || source_[scan] == '-')) {
        ++scan;
      }
      size_t digitsStart = scan;
      while (scan < source_.size() && std::isdigit(static_cast<unsigned char>(source_[scan]))) {
        ++scan;
      }
      if (scan > digitsStart) {
        pos_ = scan;
      }
    }
  }
  if (!isHex) {
    if (source_.compare(pos_, 3, "f64") == 0) {
      advance();
      advance();
      advance();
    } else if (source_.compare(pos_, 3, "f32") == 0) {
      advance();
      advance();
      advance();
    } else if (pos_ < source_.size() && source_[pos_] == 'f') {
      advance();
    }
  }
  if (source_.compare(pos_, 3, "i64") == 0 || source_.compare(pos_, 3, "u64") == 0 ||
      source_.compare(pos_, 3, "i32") == 0) {
    advance();
    advance();
    advance();
  }
  return {TokenKind::Number, source_.substr(start, pos_ - start), startLine, startColumn};
}

Token Lexer::readString(char quote) {
  int startLine = line_;
  int startColumn = column_;
  size_t start = pos_;
  advance();
  while (pos_ < source_.size()) {
    char c = source_[pos_];
    if (c == '\\') {
      advance();
      if (pos_ < source_.size()) {
        advance();
      }
      continue;
    }
    if (c == quote) {
      advance();
      if (pos_ < source_.size() && isStringSuffixStart(source_[pos_])) {
        while (pos_ < source_.size() && isStringSuffixBody(source_[pos_])) {
          advance();
        }
      }
      return {TokenKind::String, source_.substr(start, pos_ - start), startLine, startColumn};
    }
    advance();
  }
  return {TokenKind::Invalid, "unterminated string literal", startLine, startColumn};
}

Token Lexer::readComment() {
  int startLine = line_;
  int startColumn = column_;
  size_t start = pos_;
  if (pos_ + 1 >= source_.size() || source_[pos_] != '/') {
    advance();
    return {TokenKind::End, "", startLine, startColumn};
  }
  char next = source_[pos_ + 1];
  if (next == '/') {
    advance();
    advance();
    while (pos_ < source_.size() && source_[pos_] != '\n') {
      advance();
    }
    return {TokenKind::Comment, source_.substr(start, pos_ - start), startLine, startColumn};
  }
  if (next == '*') {
    advance();
    advance();
    bool closed = false;
    while (pos_ < source_.size()) {
      if (source_[pos_] == '*' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '/') {
        advance();
        advance();
        closed = true;
        break;
      }
      advance();
    }
    if (!closed) {
      return {TokenKind::Invalid, "unterminated block comment", startLine, startColumn};
    }
    return {TokenKind::Comment, source_.substr(start, pos_ - start), startLine, startColumn};
  }
  advance();
  return {TokenKind::End, "", startLine, startColumn};
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
  case '.':
    return {TokenKind::Dot, ".", startLine, startColumn};
  case '=':
    return {TokenKind::Equal, "=", startLine, startColumn};
  case ';':
    return {TokenKind::Semicolon, ";", startLine, startColumn};
  default:
    return {TokenKind::Invalid, std::string(1, c), startLine, startColumn};
  }
}

} // namespace primec
