#pragma once

#include "primec/frontend/Token.h"

namespace primec {

// Tokens a parser step skips without looking at them: comments.
inline bool isIgnorableToken(TokenKind kind) {
  return kind == TokenKind::Comment;
}

// Comments and the separators between list items, for the steps that skip all of them.
inline bool isIgnorableOrSeparatorToken(TokenKind kind) {
  return kind == TokenKind::Comment || kind == TokenKind::Comma || kind == TokenKind::Semicolon;
}

// Tokens that can begin the value of a labeled argument.
inline bool isArgumentLabelValueStart(TokenKind kind) {
  return kind == TokenKind::Identifier || kind == TokenKind::Number || kind == TokenKind::String ||
         kind == TokenKind::LBracket;
}

} // namespace primec
