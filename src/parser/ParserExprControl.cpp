#include "primec/Parser.h"

#include "ParserHelpers.h"

#include <utility>

namespace primec {
using namespace parser;

namespace {
bool isArgumentLabelValueStart(TokenKind kind) {
  switch (kind) {
  case TokenKind::Identifier:
  case TokenKind::Number:
  case TokenKind::String:
  case TokenKind::LBracket:
    return true;
  default:
    return false;
  }
}

bool isIgnorableToken(TokenKind kind) {
  return kind == TokenKind::Comment;
}
} // namespace

bool Parser::tryParseIfStatementSugar(Expr &out,
                                      const std::string &namespacePrefix,
                                      bool &parsed,
                                      bool allowSingleBranchIfStatement) {
  parsed = false;
  if (!match(TokenKind::Identifier)) {
    return true;
  }
  const std::string keyword = tokens_[pos_].text;
  if (keyword != "if" && keyword != "match") {
    return true;
  }
  size_t savedPos = pos_;
  Token name = consume(TokenKind::Identifier, "expected statement");
  if (name.kind == TokenKind::End) {
    return false;
  }
  if (!match(TokenKind::LParen)) {
    pos_ = savedPos;
    return true;
  }
  expect(TokenKind::LParen, "expected '(' after " + keyword);
  if (match(TokenKind::LBracket)) {
    size_t scan = pos_ + 1;
    while (scan < tokens_.size() && isIgnorableToken(tokens_[scan].kind)) {
      ++scan;
    }
    if (scan < tokens_.size() && tokens_[scan].kind == TokenKind::Identifier) {
      ++scan;
      while (scan < tokens_.size() && isIgnorableToken(tokens_[scan].kind)) {
        ++scan;
      }
      if (scan < tokens_.size() && tokens_[scan].kind == TokenKind::RBracket) {
        pos_ = savedPos;
        return true;
      }
    }
  }
  Expr condition;
  {
    BareBindingGuard bindingGuard(*this, false);
    if (!parseExpr(condition, namespacePrefix)) {
      return false;
    }
  }
  if (!match(TokenKind::RParen)) {
    pos_ = savedPos;
    return true;
  }
  expect(TokenKind::RParen, "expected ')' after " + keyword + " condition");
  if (!match(TokenKind::LBrace)) {
    pos_ = savedPos;
    return true;
  }
  std::vector<Expr> thenBody;
  {
    BraceListGuard braceGuard(*this, true, true);
    if (!parseBraceExprList(thenBody, namespacePrefix, true)) {
      return false;
    }
  }
  std::vector<Expr> elseBody;
  if (match(TokenKind::Identifier) && tokens_[pos_].text == "else") {
    consume(TokenKind::Identifier, "expected 'else'");
    {
      BraceListGuard braceGuard(*this, true, true);
      if (!parseBraceExprList(elseBody, namespacePrefix, true)) {
        return false;
      }
    }
  } else {
    if (keyword == "if" && allowSingleBranchIfStatement) {
      elseBody.clear();
    } else if (keyword == "if") {
      return fail("single-branch if is only allowed in statement position");
    } else {
      return fail(keyword + " statement requires else block");
    }
  }
  Expr thenCall;
  thenCall.kind = Expr::Kind::Call;
  thenCall.name = "then";
  thenCall.namespacePrefix = namespacePrefix;
  thenCall.hasBodyArguments = true;
  thenCall.bodyArguments = std::move(thenBody);
  Expr elseCall;
  elseCall.kind = Expr::Kind::Call;
  elseCall.name = "else";
  elseCall.namespacePrefix = namespacePrefix;
  elseCall.hasBodyArguments = true;
  elseCall.bodyArguments = std::move(elseBody);
  Expr ifCall;
  ifCall.kind = Expr::Kind::Call;
  ifCall.name = name.text;
  ifCall.namespacePrefix = namespacePrefix;
  ifCall.args.push_back(std::move(condition));
  ifCall.argNames.push_back(std::nullopt);
  ifCall.args.push_back(std::move(thenCall));
  ifCall.argNames.push_back(std::nullopt);
  ifCall.args.push_back(std::move(elseCall));
  ifCall.argNames.push_back(std::nullopt);
  out = std::move(ifCall);
  parsed = true;
  return true;
}

bool Parser::tryParseLoopFormAfterName(Expr &out,
                                       const std::string &namespacePrefix,
                                       const std::string &keyword,
                                       const std::vector<Transform> &transforms,
                                       bool &parsed) {
  parsed = false;
  const size_t savedPos = pos_;
  if (!match(TokenKind::LParen)) {
    return true;
  }
  expect(TokenKind::LParen, "expected '(' after " + keyword);
  std::vector<Expr> slots;
  std::vector<std::optional<std::string>> slotNames;
  auto parseSlot = [&](Expr &slot) -> bool {
    std::optional<std::string> argName;
    if (match(TokenKind::LBracket)) {
      size_t savedLabelPos = pos_;
      expect(TokenKind::LBracket, "expected '['");
      skipComments();
      if (match(TokenKind::Identifier)) {
        Token name = consume(TokenKind::Identifier, "expected argument label");
        if (name.kind == TokenKind::End) {
          return false;
        }
        if (name.text.find('/') != std::string::npos) {
          return fail("named argument must be a simple identifier");
        }
        std::string nameError;
        if (!validateIdentifierText(name.text, nameError)) {
          return fail(nameError);
        }
        skipComments();
        if (match(TokenKind::RBracket)) {
          expect(TokenKind::RBracket, "expected ']' after argument label");
          size_t nextIndex = pos_;
          while (nextIndex < tokens_.size() && isIgnorableToken(tokens_[nextIndex].kind)) {
            ++nextIndex;
          }
          if (nextIndex < tokens_.size() && isArgumentLabelValueStart(tokens_[nextIndex].kind)) {
            bool looksLikeBinding = false;
            if (allowBareBindings_ && tokens_[nextIndex].kind == TokenKind::Identifier) {
              size_t afterName = nextIndex + 1;
              while (afterName < tokens_.size() && isIgnorableToken(tokens_[afterName].kind)) {
                ++afterName;
              }
              if (afterName < tokens_.size() && tokens_[afterName].kind == TokenKind::LBrace) {
                looksLikeBinding = true;
              }
            }
            if (!looksLikeBinding) {
              argName = name.text;
            } else {
              pos_ = savedLabelPos;
            }
          } else {
            pos_ = savedLabelPos;
          }
        } else {
          pos_ = savedLabelPos;
        }
      } else {
        pos_ = savedLabelPos;
      }
    }
    if (!parseExpr(slot, namespacePrefix)) {
      return false;
    }
    slotNames.push_back(argName);
    return true;
  };
  auto consumeSlotSeparator = [&]() {
    skipComments();
    if (match(TokenKind::Comma)) {
      expect(TokenKind::Comma, "expected ','");
      return;
    }
    if (match(TokenKind::Semicolon)) {
      expect(TokenKind::Semicolon, "expected ';'");
    }
  };
  if (keyword == "for") {
    BareBindingGuard bindingGuard(*this, true);
    ArgumentLabelGuard labelGuard(*this);
    for (int i = 0; i < 3; ++i) {
      Expr slot;
      if (!parseSlot(slot)) {
        return false;
      }
      slots.push_back(std::move(slot));
      if (i < 2) {
        consumeSlotSeparator();
      }
    }
  } else {
    Expr slot;
    {
      BareBindingGuard bindingGuard(*this, false);
      if (!parseSlot(slot)) {
        return false;
      }
    }
    slots.push_back(std::move(slot));
  }
  if (!match(TokenKind::RParen)) {
    pos_ = savedPos;
    return true;
  }
  expect(TokenKind::RParen, "expected ')' after " + keyword);
  if (!match(TokenKind::LBrace)) {
    pos_ = savedPos;
    return true;
  }
  std::vector<Expr> body;
  {
    BraceListGuard braceGuard(*this, true, true);
    if (!parseBraceExprList(body, namespacePrefix, true)) {
      return false;
    }
  }
  Expr bodyCall;
  bodyCall.kind = Expr::Kind::Call;
  bodyCall.name = "do";
  bodyCall.namespacePrefix = namespacePrefix;
  bodyCall.hasBodyArguments = true;
  bodyCall.bodyArguments = std::move(body);

  Expr loopCall;
  loopCall.kind = Expr::Kind::Call;
  loopCall.name = keyword;
  loopCall.namespacePrefix = namespacePrefix;
  loopCall.transforms = transforms;
  for (size_t i = 0; i < slots.size(); ++i) {
    auto &slot = slots[i];
    loopCall.args.push_back(std::move(slot));
    if (i < slotNames.size()) {
      loopCall.argNames.push_back(slotNames[i]);
    } else {
      loopCall.argNames.push_back(std::nullopt);
    }
  }
  loopCall.args.push_back(std::move(bodyCall));
  loopCall.argNames.push_back(std::nullopt);
  out = std::move(loopCall);
  parsed = true;
  return true;
}

bool Parser::tryParseLambdaExpr(Expr &out, const std::string &namespacePrefix, bool &parsed) {
  parsed = false;
  if (!match(TokenKind::LBracket)) {
    return true;
  }
  const size_t savedPos = pos_;
  size_t scan = pos_ + 1;
  while (scan < tokens_.size() && tokens_[scan].kind != TokenKind::RBracket) {
    ++scan;
  }
  if (scan >= tokens_.size() || tokens_[scan].kind != TokenKind::RBracket) {
    pos_ = savedPos;
    return true;
  }
  size_t after = scan + 1;
  while (after < tokens_.size() && isIgnorableToken(tokens_[after].kind)) {
    ++after;
  }
  if (after >= tokens_.size() ||
      (tokens_[after].kind != TokenKind::LAngle && tokens_[after].kind != TokenKind::LParen)) {
    pos_ = savedPos;
    return true;
  }

  std::vector<std::string> captures;
  if (!parseLambdaCaptureList(captures)) {
    return false;
  }
  std::vector<std::string> templateArgs;
  if (match(TokenKind::LAngle)) {
    if (!parseTemplateList(templateArgs)) {
      return false;
    }
  }
  if (!expect(TokenKind::LParen, "expected '(' after lambda capture list")) {
    return false;
  }
  std::vector<Expr> params;
  if (!parseParameterList(params, namespacePrefix, nullptr)) {
    return false;
  }
  if (!expect(TokenKind::RParen, "expected ')' after lambda parameters")) {
    return false;
  }
  if (!match(TokenKind::LBrace)) {
    return fail("lambda requires a body");
  }
  std::vector<Expr> body;
  {
    struct ReturnContextGuard {
      explicit ReturnContextGuard(Parser &parser)
          : parser_(parser),
            previousTracker_(parser.returnTracker_),
            previousReturnsVoid_(parser.returnsVoidContext_),
            previousAllowImplicitVoid_(parser.allowImplicitVoidReturn_) {
        parser_.returnTracker_ = nullptr;
        parser_.returnsVoidContext_ = false;
        parser_.allowImplicitVoidReturn_ = true;
      }
      ~ReturnContextGuard() {
        parser_.returnTracker_ = previousTracker_;
        parser_.returnsVoidContext_ = previousReturnsVoid_;
        parser_.allowImplicitVoidReturn_ = previousAllowImplicitVoid_;
      }
      ReturnContextGuard(const ReturnContextGuard &) = delete;
      ReturnContextGuard &operator=(const ReturnContextGuard &) = delete;

    private:
      Parser &parser_;
      bool *previousTracker_;
      bool previousReturnsVoid_;
      bool previousAllowImplicitVoid_;
    } returnGuard(*this);

    BraceListGuard braceGuard(*this, true, true);
    if (!parseBraceExprList(body, namespacePrefix, true)) {
      return false;
    }
  }
  Expr lambda;
  lambda.kind = Expr::Kind::Call;
  lambda.name = "lambda";
  lambda.namespacePrefix = namespacePrefix;
  lambda.templateArgs = std::move(templateArgs);
  lambda.args = std::move(params);
  lambda.argNames.resize(lambda.args.size());
  lambda.hasBodyArguments = true;
  lambda.bodyArguments = std::move(body);
  lambda.lambdaCaptures = std::move(captures);
  lambda.isLambda = true;
  out = std::move(lambda);
  parsed = true;
  return true;
}

bool Parser::parseReturnStatement(Expr &out, const std::string &namespacePrefix) {
  Token name = consume(TokenKind::Identifier, "expected return statement");
  if (name.kind == TokenKind::End) {
    return false;
  }
  if (name.text != "return") {
    return fail("expected return statement");
  }
  Expr returnCall = {};
  returnCall.kind = Expr::Kind::Call;
  returnCall.name = name.text;
  returnCall.namespacePrefix = namespacePrefix;
  returnCall.sourceLine = name.line;
  returnCall.sourceColumn = name.column;
  if (!expect(TokenKind::LParen, "expected '(' after return")) {
    return false;
  }
  if (match(TokenKind::RParen)) {
    if (!returnsVoidContext_ && !allowImplicitVoidReturn_) {
      return fail("return requires exactly one argument");
    }
    expect(TokenKind::RParen, "expected ')' after return");
    out = std::move(returnCall);
    if (returnTracker_) {
      *returnTracker_ = true;
    }
    return true;
  }
  if (returnsVoidContext_) {
    return fail("return value not allowed for void definition");
  }
  Expr arg;
  {
    BareBindingGuard bindingGuard(*this, false);
    if (!parseExpr(arg, namespacePrefix)) {
      return false;
    }
  }
  returnCall.args.push_back(std::move(arg));
  returnCall.argNames.push_back(std::nullopt);
  if (!match(TokenKind::RParen)) {
    return fail("return requires exactly one argument");
  }
  if (!expect(TokenKind::RParen, "expected ')' after return argument")) {
    return false;
  }
  out = std::move(returnCall);
  if (returnTracker_) {
    *returnTracker_ = true;
  }
  return true;
}

} // namespace primec
