#include "primec/Parser.h"

#include "ParserHelpers.h"

#include <utility>

namespace primec {
using namespace parser;

namespace {

bool isIgnorableToken(TokenKind kind) {
  return kind == TokenKind::Comment || kind == TokenKind::Comma || kind == TokenKind::Semicolon;
}

bool isControlKeyword(const std::string &name) {
  return name == "if" || name == "else" || name == "loop" || name == "while" || name == "for" ||
         name == "match";
}

bool isNoReturnDefinitionTransform(const std::string &name) {
  return isStructTransformName(name) || name == "sum";
}

size_t skipCommentTokens(const std::vector<Token> &tokens, size_t index) {
  while (index < tokens.size() && isIgnorableToken(tokens[index].kind)) {
    ++index;
  }
  return index;
}

size_t skipDefinitionTailTransforms(const std::vector<Token> &tokens, size_t index, bool allowSurfaceSyntax) {
  size_t scan = skipCommentTokens(tokens, index);
  if (!allowSurfaceSyntax || scan >= tokens.size() || tokens[scan].kind != TokenKind::LBracket) {
    return scan;
  }
  int bracketDepth = 0;
  while (scan < tokens.size()) {
    TokenKind kind = tokens[scan].kind;
    if (kind == TokenKind::LBracket) {
      ++bracketDepth;
    } else if (kind == TokenKind::RBracket) {
      --bracketDepth;
      if (bracketDepth == 0) {
        ++scan;
        break;
      }
    }
    ++scan;
  }
  return skipCommentTokens(tokens, scan);
}

bool isBindingParamBracket(const std::vector<Token> &tokens, size_t index) {
  if (index >= tokens.size() || tokens[index].kind != TokenKind::LBracket) {
    return false;
  }
  auto skipComments = [&](size_t &pos) {
    while (pos < tokens.size() && tokens[pos].kind == TokenKind::Comment) {
      ++pos;
    }
  };
  size_t scan = index + 1;
  skipComments(scan);
  if (scan >= tokens.size() || tokens[scan].kind != TokenKind::Identifier) {
    return true;
  }
  ++scan;
  skipComments(scan);
  if (scan >= tokens.size() || tokens[scan].kind != TokenKind::RBracket) {
    return true;
  }
  ++scan;
  skipComments(scan);
  if (scan >= tokens.size()) {
    return true;
  }
  const TokenKind nextKind = tokens[scan].kind;
  if (nextKind == TokenKind::Number || nextKind == TokenKind::String) {
    return false;
  }
  if (nextKind == TokenKind::Identifier) {
    if (tokens[scan].text == "true" || tokens[scan].text == "false") {
      return false;
    }
    size_t follow = scan + 1;
    skipComments(follow);
    if (follow < tokens.size()) {
      const TokenKind followKind = tokens[follow].kind;
      if (followKind == TokenKind::LParen || followKind == TokenKind::Dot ||
          followKind == TokenKind::LBracket || followKind == TokenKind::LAngle) {
        return false;
      }
      if (followKind == TokenKind::LBrace) {
        return true;
      }
    }
    return true;
  }
  return true;
}

bool isVariadicIdentifierParam(const std::vector<Token> &tokens, size_t index) {
  if (index >= tokens.size() || tokens[index].kind != TokenKind::Identifier) {
    return false;
  }
  size_t scan = skipCommentTokens(tokens, index + 1);
  return scan < tokens.size() && tokens[scan].kind == TokenKind::Ellipsis;
}

} // namespace

bool Parser::definitionHasReturnBeforeClose() const {
  size_t index = pos_;
  int depth = 1;
  while (index < tokens_.size()) {
    TokenKind kind = tokens_[index].kind;
    if (isIgnorableToken(kind)) {
      ++index;
      continue;
    }
    if (kind == TokenKind::LParen) {
      ++depth;
    } else if (kind == TokenKind::RParen) {
      --depth;
      if (depth == 0) {
        break;
      }
    }
    ++index;
  }
  if (depth != 0) {
    return false;
  }
  size_t braceIndex = skipCommentTokens(tokens_, index + 1);
  braceIndex = skipDefinitionTailTransforms(tokens_, braceIndex, allowSurfaceSyntax_);
  if (braceIndex >= tokens_.size()) {
    return false;
  }
  if (tokens_[braceIndex].kind != TokenKind::LBrace) {
    return false;
  }
  int braceDepth = 0;
  while (braceIndex < tokens_.size()) {
    TokenKind kind = tokens_[braceIndex].kind;
    if (isIgnorableToken(kind)) {
      ++braceIndex;
      continue;
    }
    if (kind == TokenKind::LBrace) {
      ++braceDepth;
    } else if (kind == TokenKind::RBrace) {
      --braceDepth;
      if (braceDepth == 0) {
        break;
      }
    } else if (kind == TokenKind::Identifier && tokens_[braceIndex].text == "return") {
      return true;
    }
    ++braceIndex;
  }
  return false;
}

bool Parser::isDefinitionSignature(bool *paramsAreIdentifiers) const {
  size_t index = pos_;
  int depth = 1;
  int braceDepth = 0;
  bool identifiersOnly = true;
  bool sawBindingSyntax = false;
  TokenKind prevAtDepth1 = TokenKind::LParen;
  while (index < tokens_.size()) {
    TokenKind kind = tokens_[index].kind;
    if (kind == TokenKind::Comma || kind == TokenKind::Semicolon) {
      if (depth == 1 && braceDepth == 0) {
        prevAtDepth1 = TokenKind::Comma;
      }
      ++index;
      continue;
    }
    if (kind == TokenKind::Comment) {
      ++index;
      continue;
    }
    if (kind == TokenKind::LParen) {
      ++depth;
    } else if (kind == TokenKind::RParen) {
      --depth;
      if (depth == 0) {
        break;
      }
    } else if (depth == 1) {
      if (kind == TokenKind::LBrace) {
        ++braceDepth;
      } else if (kind == TokenKind::RBrace && braceDepth > 0) {
        --braceDepth;
      }
      if (braceDepth == 0) {
        if (kind == TokenKind::LBracket) {
          identifiersOnly = false;
          const bool atArgStart = (prevAtDepth1 == TokenKind::LParen || prevAtDepth1 == TokenKind::Comma);
          if (atArgStart && isBindingParamBracket(tokens_, index)) {
            sawBindingSyntax = true;
          }
        } else if (kind == TokenKind::Identifier) {
          const bool atArgStart = (prevAtDepth1 == TokenKind::LParen || prevAtDepth1 == TokenKind::Comma);
          if (atArgStart && isVariadicIdentifierParam(tokens_, index)) {
            identifiersOnly = false;
            sawBindingSyntax = true;
          }
        } else if (kind == TokenKind::Ellipsis) {
          if (prevAtDepth1 != TokenKind::Identifier) {
            identifiersOnly = false;
          }
        } else if (kind != TokenKind::Identifier && kind != TokenKind::Comma) {
          identifiersOnly = false;
        }
        prevAtDepth1 = kind;
      }
    }
    ++index;
  }
  if (paramsAreIdentifiers) {
    *paramsAreIdentifiers = identifiersOnly;
  }
  if (depth != 0) {
    return false;
  }
  if (!identifiersOnly && !sawBindingSyntax) {
    return false;
  }
  size_t braceIndex = skipCommentTokens(tokens_, index + 1);
  braceIndex = skipDefinitionTailTransforms(tokens_, braceIndex, allowSurfaceSyntax_);
  if (braceIndex >= tokens_.size()) {
    return false;
  }
  return tokens_[braceIndex].kind == TokenKind::LBrace;
}

bool Parser::isDefinitionSignatureAllowNoReturn(bool *paramsAreIdentifiers) const {
  size_t index = pos_;
  int depth = 1;
  int braceDepth = 0;
  bool identifiersOnly = true;
  bool sawBindingSyntax = false;
  TokenKind prevAtDepth1 = TokenKind::LParen;
  while (index < tokens_.size()) {
    TokenKind kind = tokens_[index].kind;
    if (kind == TokenKind::Comma || kind == TokenKind::Semicolon) {
      if (depth == 1 && braceDepth == 0) {
        prevAtDepth1 = TokenKind::Comma;
      }
      ++index;
      continue;
    }
    if (kind == TokenKind::Comment) {
      ++index;
      continue;
    }
    if (kind == TokenKind::LParen) {
      ++depth;
    } else if (kind == TokenKind::RParen) {
      --depth;
      if (depth == 0) {
        break;
      }
    } else if (depth == 1) {
      if (kind == TokenKind::LBrace) {
        ++braceDepth;
      } else if (kind == TokenKind::RBrace && braceDepth > 0) {
        --braceDepth;
      }
      if (braceDepth == 0) {
        if (kind == TokenKind::LBracket) {
          identifiersOnly = false;
          const bool atArgStart = (prevAtDepth1 == TokenKind::LParen || prevAtDepth1 == TokenKind::Comma);
          if (atArgStart && isBindingParamBracket(tokens_, index)) {
            sawBindingSyntax = true;
          }
        } else if (kind == TokenKind::Identifier) {
          const bool atArgStart = (prevAtDepth1 == TokenKind::LParen || prevAtDepth1 == TokenKind::Comma);
          if (atArgStart && isVariadicIdentifierParam(tokens_, index)) {
            identifiersOnly = false;
            sawBindingSyntax = true;
          }
        } else if (kind == TokenKind::Ellipsis) {
          if (prevAtDepth1 != TokenKind::Identifier) {
            identifiersOnly = false;
          }
        } else if (kind != TokenKind::Identifier && kind != TokenKind::Comma) {
          identifiersOnly = false;
        }
        prevAtDepth1 = kind;
      }
    }
    ++index;
  }
  if (paramsAreIdentifiers) {
    *paramsAreIdentifiers = identifiersOnly;
  }
  if (depth != 0 || (!identifiersOnly && !sawBindingSyntax)) {
    return false;
  }
  size_t braceIndex = skipCommentTokens(tokens_, index + 1);
  braceIndex = skipDefinitionTailTransforms(tokens_, braceIndex, allowSurfaceSyntax_);
  if (braceIndex >= tokens_.size()) {
    return false;
  }
  return tokens_[braceIndex].kind == TokenKind::LBrace;
}

bool Parser::isCopyConstructorShorthandSignature() const {
  size_t index = pos_;
  auto skipCommentsOnly = [&]() {
    while (index < tokens_.size() && tokens_[index].kind == TokenKind::Comment) {
      ++index;
    }
  };
  skipCommentsOnly();
  if (index >= tokens_.size() || tokens_[index].kind != TokenKind::Identifier) {
    return false;
  }
  ++index;
  skipCommentsOnly();
  if (index >= tokens_.size() || tokens_[index].kind != TokenKind::RParen) {
    return false;
  }
  size_t braceIndex = skipCommentTokens(tokens_, index + 1);
  braceIndex = skipDefinitionTailTransforms(tokens_, braceIndex, allowSurfaceSyntax_);
  if (braceIndex >= tokens_.size()) {
    return false;
  }
  return tokens_[braceIndex].kind == TokenKind::LBrace;
}

bool Parser::parseCopyConstructorShorthand(std::vector<Expr> &out, const std::string &namespacePrefix) {
  Token name = consume(TokenKind::Identifier, "expected parameter identifier");
  if (name.kind == TokenKind::End) {
    return false;
  }
  std::string nameError;
  if (!validateIdentifierText(name.text, nameError)) {
    return fail(nameError);
  }
  Expr param;
  param.kind = Expr::Kind::Call;
  param.name = name.text;
  param.namespacePrefix = namespacePrefix;
  param.sourceLine = name.line;
  param.sourceColumn = name.column;
  param.isBinding = true;
  Transform typeTransform;
  typeTransform.name = "Reference";
  typeTransform.templateArgs.push_back("Self");
  param.transforms.push_back(std::move(typeTransform));
  out.push_back(std::move(param));
  return true;
}

bool Parser::tryParseNestedDefinition(std::vector<Definition> &defs,
                                      const std::vector<Transform> &transforms,
                                      const std::string &parentPath,
                                      bool &parsed) {
  parsed = false;
  if (!match(TokenKind::Identifier)) {
    return true;
  }
  const size_t savedPos = pos_;
  Token name = consume(TokenKind::Identifier, "expected identifier");
  if (name.kind == TokenKind::End) {
    return false;
  }
  std::string nameError;
  if (!validateIdentifierText(name.text, nameError)) {
    if (!isControlKeyword(name.text) || (!match(TokenKind::LParen) && !match(TokenKind::LAngle))) {
      return fail(nameError);
    }
    pos_ = savedPos;
    return true;
  }

  std::vector<std::string> templateArgs;
  if (match(TokenKind::LAngle)) {
    if (!parseTemplateList(templateArgs)) {
      return false;
    }
  }

  bool hasReturnTransform = false;
  bool hasNoReturnDefinitionTransform = false;
  for (const auto &transform : transforms) {
    if (transform.name == "return") {
      hasReturnTransform = true;
    }
    if (isNoReturnDefinitionTransform(transform.name)) {
      hasNoReturnDefinitionTransform = true;
    }
  }

  if (name.text == "repeat" && !hasReturnTransform && !hasNoReturnDefinitionTransform) {
    pos_ = savedPos;
    return true;
  }

  if (!match(TokenKind::LParen)) {
    pos_ = savedPos;
    return true;
  }
  if (!expect(TokenKind::LParen, "expected '(' after identifier")) {
    return false;
  }

  bool isDefinition = false;
  const bool copyShorthand =
      allowSurfaceSyntax_ && name.text == "Copy" && isCopyConstructorShorthandSignature();
  if (hasReturnTransform) {
    isDefinition = true;
  } else if (hasNoReturnDefinitionTransform) {
    isDefinition = isDefinitionSignatureAllowNoReturn(nullptr);
  } else {
    isDefinition = isDefinitionSignature(nullptr);
  }
  if (copyShorthand) {
    isDefinition = true;
  }

  if (!isDefinition) {
    pos_ = savedPos;
    return true;
  }
  if (!allowSurfaceSyntax_ && !hasReturnTransform && !hasNoReturnDefinitionTransform) {
    return fail("definition requires explicit return transform in canonical mode");
  }

  std::vector<Expr> parameters;
  if (copyShorthand) {
    if (!parseCopyConstructorShorthand(parameters, parentPath)) {
      return false;
    }
  } else {
    if (!parseParameterList(parameters, parentPath, &templateArgs)) {
      return false;
    }
  }
  if (!expect(TokenKind::RParen, "expected ')' after parameters")) {
    return false;
  }
  std::vector<Transform> combinedTransforms = transforms;
  if (allowSurfaceSyntax_ && match(TokenKind::LBracket)) {
    std::vector<Transform> tailTransforms;
    if (!parseTransformList(tailTransforms)) {
      return false;
    }
    for (auto &transform : tailTransforms) {
      if (transform.name == "return") {
        hasReturnTransform = true;
      }
      if (isNoReturnDefinitionTransform(transform.name)) {
        hasNoReturnDefinitionTransform = true;
      }
      combinedTransforms.push_back(std::move(transform));
    }
  }
  if (!match(TokenKind::LBrace)) {
    return fail("definitions must have a body");
  }

  Definition def;
  def.name = name.text;
  def.namespacePrefix = parentPath;
  def.fullPath = makeFullPath(def.name, def.namespacePrefix);
  def.sourceLine = name.line;
  def.sourceColumn = name.column;
  def.transforms = std::move(combinedTransforms);
  def.templateArgs = std::move(templateArgs);
  def.parameters = std::move(parameters);
  def.isNested = true;
  if (!parseDefinitionBody(def, hasNoReturnDefinitionTransform, defs)) {
    return false;
  }
  defs.push_back(std::move(def));
  parsed = true;
  return true;
}

} // namespace primec
