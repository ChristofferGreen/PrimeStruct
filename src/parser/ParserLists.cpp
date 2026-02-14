#include "primec/Parser.h"

#include "ParserHelpers.h"

#include "primec/StringLiteral.h"

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
  return kind == TokenKind::Comment || kind == TokenKind::Comma || kind == TokenKind::Semicolon;
}
} // namespace

bool Parser::parseTransformList(std::vector<Transform> &out) {
  if (!expect(TokenKind::LBracket, "expected '['")) {
    return false;
  }
  if (match(TokenKind::RBracket)) {
    return fail("transform list cannot be empty");
  }
  auto parseTransformItem = [&](TransformPhase phase, Transform &transform) -> bool {
    Token name = consume(TokenKind::Identifier, "expected transform identifier");
    if (name.kind == TokenKind::End) {
      return false;
    }
    std::string nameError;
    if (!validateTransformName(name.text, nameError)) {
      return fail(nameError);
    }
    transform.name = name.text;
    transform.phase = phase;
    if (match(TokenKind::LAngle)) {
      if (!parseTemplateList(transform.templateArgs)) {
        return false;
      }
    }
    if (match(TokenKind::LParen)) {
      expect(TokenKind::LParen, "expected '('");
      if (match(TokenKind::RParen)) {
        return fail("transform argument list cannot be empty");
      }
      while (true) {
        if (match(TokenKind::Identifier)) {
          Token arg = consume(TokenKind::Identifier, "expected transform argument");
          if (arg.kind == TokenKind::End) {
            return false;
          }
          std::string argError;
          if (!validateIdentifierText(arg.text, argError)) {
            return fail(argError);
          }
          transform.arguments.push_back(arg.text);
        } else if (match(TokenKind::Number)) {
          Token arg = consume(TokenKind::Number, "expected transform argument");
          if (arg.kind == TokenKind::End) {
            return false;
          }
          transform.arguments.push_back(arg.text);
        } else if (match(TokenKind::String)) {
          Token arg = consume(TokenKind::String, "expected transform argument");
          if (arg.kind == TokenKind::End) {
            return false;
          }
          std::string normalized;
          std::string parseError;
          if (!normalizeStringLiteralToken(arg.text, normalized, parseError)) {
            return fail(parseError);
          }
          transform.arguments.push_back(std::move(normalized));
        } else {
          return fail("expected transform argument");
        }
        if (match(TokenKind::RParen)) {
          break;
        }
        if (match(TokenKind::Identifier) || match(TokenKind::Number) || match(TokenKind::String)) {
          continue;
        }
        break;
      }
      if (!expect(TokenKind::RParen, "expected ')'")) {
        return false;
      }
    }
    return true;
  };
  auto parseTransformGroup = [&](const std::string &groupName) -> bool {
    if (!expect(TokenKind::LParen, "expected '('")) {
      return false;
    }
    if (match(TokenKind::RParen)) {
      return fail("transform " + groupName + " group cannot be empty");
    }
    while (!match(TokenKind::RParen)) {
      Transform nested;
      if (!parseTransformItem(groupName == "text" ? TransformPhase::Text : TransformPhase::Semantic, nested)) {
        return false;
      }
      out.push_back(std::move(nested));
      if (match(TokenKind::RParen)) {
        break;
      }
    }
    if (!expect(TokenKind::RParen, "expected ')'")) {
      return false;
    }
    return true;
  };
  while (!match(TokenKind::RBracket)) {
    Token name = consume(TokenKind::Identifier, "expected transform identifier");
    if (name.kind == TokenKind::End) {
      return false;
    }
    std::string nameError;
    if (!validateTransformName(name.text, nameError)) {
      return fail(nameError);
    }
    if ((name.text == "text" || name.text == "semantic") && match(TokenKind::LParen)) {
      if (!parseTransformGroup(name.text)) {
        return false;
      }
      continue;
    }
    Transform transform;
    transform.name = name.text;
    transform.phase = TransformPhase::Auto;
    if (match(TokenKind::LAngle)) {
      if (!parseTemplateList(transform.templateArgs)) {
        return false;
      }
    }
    if (match(TokenKind::LParen)) {
      expect(TokenKind::LParen, "expected '('");
      if (match(TokenKind::RParen)) {
        return fail("transform argument list cannot be empty");
      }
      while (true) {
        if (match(TokenKind::Identifier)) {
          Token arg = consume(TokenKind::Identifier, "expected transform argument");
          if (arg.kind == TokenKind::End) {
            return false;
          }
          std::string argError;
          if (!validateIdentifierText(arg.text, argError)) {
            return fail(argError);
          }
          transform.arguments.push_back(arg.text);
        } else if (match(TokenKind::Number)) {
          Token arg = consume(TokenKind::Number, "expected transform argument");
          if (arg.kind == TokenKind::End) {
            return false;
          }
          transform.arguments.push_back(arg.text);
        } else if (match(TokenKind::String)) {
          Token arg = consume(TokenKind::String, "expected transform argument");
          if (arg.kind == TokenKind::End) {
            return false;
          }
          std::string normalized;
          std::string parseError;
          if (!normalizeStringLiteralToken(arg.text, normalized, parseError)) {
            return fail(parseError);
          }
          transform.arguments.push_back(std::move(normalized));
        } else {
          return fail("expected transform argument");
        }
        if (match(TokenKind::RParen)) {
          break;
        }
        if (match(TokenKind::Identifier) || match(TokenKind::Number) || match(TokenKind::String)) {
          continue;
        }
        break;
      }
      if (!expect(TokenKind::RParen, "expected ')'")) {
        return false;
      }
    }
    out.push_back(std::move(transform));
  }
  expect(TokenKind::RBracket, "expected ']'");
  return true;
}

bool Parser::parseLambdaCaptureList(std::vector<std::string> &captures) {
  captures.clear();
  if (!expect(TokenKind::LBracket, "expected '[' to start lambda capture list")) {
    return false;
  }
  if (match(TokenKind::RBracket)) {
    return expect(TokenKind::RBracket, "expected ']' to close lambda capture list");
  }
  while (true) {
    if (match(TokenKind::Comma) || match(TokenKind::Semicolon)) {
      return fail("expected lambda capture");
    }
    std::string entry;
    while (true) {
      if (match(TokenKind::Identifier)) {
        Token name = consume(TokenKind::Identifier, "expected lambda capture");
        if (name.kind == TokenKind::End) {
          return false;
        }
        if (!entry.empty()) {
          entry.push_back(' ');
        }
        entry += name.text;
        continue;
      }
      if (match(TokenKind::Equal)) {
        consume(TokenKind::Equal, "expected lambda capture");
        if (!entry.empty()) {
          entry.push_back(' ');
        }
        entry += "=";
        continue;
      }
      break;
    }
    if (entry.empty()) {
      return fail("expected lambda capture");
    }
    captures.push_back(std::move(entry));
    if (match(TokenKind::Comma)) {
      expect(TokenKind::Comma, "expected ','");
      if (match(TokenKind::RBracket)) {
        break;
      }
      continue;
    }
    if (match(TokenKind::Semicolon)) {
      expect(TokenKind::Semicolon, "expected ';'");
      if (match(TokenKind::RBracket)) {
        break;
      }
      continue;
    }
    if (match(TokenKind::RBracket)) {
      break;
    }
    return fail("expected ',' or ']' after lambda capture");
  }
  return expect(TokenKind::RBracket, "expected ']' to close lambda capture list");
}

bool Parser::parseTemplateList(std::vector<std::string> &out) {
  auto skipCommentsOnly = [&]() {
    while (tokens_[pos_].kind == TokenKind::Comment) {
      ++pos_;
    }
  };
  auto matchRaw = [&](TokenKind kind) -> bool {
    skipCommentsOnly();
    return tokens_[pos_].kind == kind;
  };
  auto expectRaw = [&](TokenKind kind, const std::string &message) -> bool {
    if (matchRaw(TokenKind::Invalid)) {
      return fail(describeInvalidToken(tokens_[pos_]));
    }
    if (!matchRaw(kind)) {
      return fail(message);
    }
    ++pos_;
    return true;
  };
  auto skipSeparators = [&]() {
    skipCommentsOnly();
    while (tokens_[pos_].kind == TokenKind::Comma || tokens_[pos_].kind == TokenKind::Semicolon) {
      ++pos_;
      skipCommentsOnly();
    }
  };

  if (!expectRaw(TokenKind::LAngle, "expected '<'")) {
    return false;
  }
  skipSeparators();
  if (matchRaw(TokenKind::RAngle)) {
    return fail("expected template identifier");
  }
  while (true) {
    std::string typeName;
    if (!parseTypeName(typeName)) {
      return false;
    }
    out.push_back(typeName);
    skipSeparators();
    if (matchRaw(TokenKind::RAngle)) {
      break;
    }
    if (matchRaw(TokenKind::Identifier)) {
      continue;
    }
    return fail("expected '>'");
  }
  if (!expectRaw(TokenKind::RAngle, "expected '>'")) {
    return false;
  }
  return true;
}

bool Parser::parseTypeName(std::string &out) {
  auto skipCommentsOnly = [&]() {
    while (tokens_[pos_].kind == TokenKind::Comment) {
      ++pos_;
    }
  };
  auto matchRaw = [&](TokenKind kind) -> bool {
    skipCommentsOnly();
    return tokens_[pos_].kind == kind;
  };
  auto consumeRaw = [&](TokenKind kind, const std::string &message) -> Token {
    if (matchRaw(TokenKind::Invalid)) {
      fail(describeInvalidToken(tokens_[pos_]));
      return {TokenKind::End, ""};
    }
    if (!matchRaw(kind)) {
      fail(message);
      return {TokenKind::End, ""};
    }
    return tokens_[pos_++];
  };
  auto expectRaw = [&](TokenKind kind, const std::string &message) -> bool {
    if (matchRaw(TokenKind::Invalid)) {
      return fail(describeInvalidToken(tokens_[pos_]));
    }
    if (!matchRaw(kind)) {
      return fail(message);
    }
    ++pos_;
    return true;
  };
  auto skipSeparators = [&]() {
    skipCommentsOnly();
    while (tokens_[pos_].kind == TokenKind::Comma || tokens_[pos_].kind == TokenKind::Semicolon) {
      ++pos_;
      skipCommentsOnly();
    }
  };

  Token name = consumeRaw(TokenKind::Identifier, "expected template identifier");
  if (name.kind == TokenKind::End) {
    return false;
  }
  std::string nameError;
  if (!validateIdentifierText(name.text, nameError)) {
    return fail(nameError);
  }
  out = name.text;
  if (matchRaw(TokenKind::LAngle)) {
    expectRaw(TokenKind::LAngle, "expected '<'");
    out += "<";
    skipSeparators();
    if (matchRaw(TokenKind::RAngle)) {
      return fail("expected template identifier");
    }
    bool first = true;
    while (true) {
      std::string nested;
      if (!parseTypeName(nested)) {
        return false;
      }
      if (!first) {
        out += ", ";
      }
      out += nested;
      first = false;
      skipSeparators();
      if (matchRaw(TokenKind::RAngle)) {
        break;
      }
      if (matchRaw(TokenKind::Identifier)) {
        continue;
      }
      return fail("expected '>'");
    }
    if (!expectRaw(TokenKind::RAngle, "expected '>'")) {
      return false;
    }
    out += ">";
  }
  return true;
}

bool Parser::parseParameterBinding(Expr &out, const std::string &namespacePrefix) {
  if (!match(TokenKind::LBracket)) {
    return fail("expected '[' to start parameter");
  }
  std::vector<Transform> transforms;
  if (!parseTransformList(transforms)) {
    return false;
  }
  Token name = consume(TokenKind::Identifier, "expected parameter identifier");
  if (name.kind == TokenKind::End) {
    return false;
  }
  std::string nameError;
  if (!validateIdentifierText(name.text, nameError)) {
    return fail(nameError);
  }
  if (match(TokenKind::LAngle)) {
    return fail("parameter identifiers do not accept template arguments");
  }
  Expr param;
  param.kind = Expr::Kind::Call;
  param.name = name.text;
  param.namespacePrefix = namespacePrefix;
  param.transforms = std::move(transforms);
  param.isBinding = true;
  if (match(TokenKind::LParen)) {
    return fail("parameter defaults must use braces");
  }
  if (match(TokenKind::LBrace)) {
    if (!parseBindingInitializerList(param.args, param.argNames, namespacePrefix)) {
      return false;
    }
    for (const auto &name : param.argNames) {
      if (name.has_value()) {
        return fail("parameter defaults do not accept named arguments");
      }
    }
  }
  out = std::move(param);
  return true;
}

bool Parser::parseParameterList(std::vector<Expr> &out, const std::string &namespacePrefix) {
  if (match(TokenKind::RParen)) {
    return true;
  }
  while (true) {
    if (match(TokenKind::Semicolon)) {
      expect(TokenKind::Semicolon, "expected ';'");
      if (match(TokenKind::RParen)) {
        break;
      }
      continue;
    }
    Expr param;
    if (!parseParameterBinding(param, namespacePrefix)) {
      return false;
    }
    out.push_back(std::move(param));
    if (match(TokenKind::Comma) || match(TokenKind::Semicolon)) {
      if (match(TokenKind::Comma)) {
        expect(TokenKind::Comma, "expected ','");
      } else {
        expect(TokenKind::Semicolon, "expected ';'");
      }
      while (match(TokenKind::Comma) || match(TokenKind::Semicolon)) {
        if (match(TokenKind::Comma)) {
          expect(TokenKind::Comma, "expected ','");
        } else {
          expect(TokenKind::Semicolon, "expected ';'");
        }
      }
      if (match(TokenKind::RParen)) {
        break;
      }
      continue;
    }
    if (match(TokenKind::RParen)) {
      break;
    }
    continue;
  }
  return true;
}

bool Parser::parseCallArgumentList(std::vector<Expr> &out,
                                   std::vector<std::optional<std::string>> &argNames,
                                   const std::string &namespacePrefix) {
  if (match(TokenKind::RParen)) {
    return true;
  }
  ArgumentLabelGuard labelGuard(*this);
  while (true) {
    if (match(TokenKind::Semicolon)) {
      expect(TokenKind::Semicolon, "expected ';'");
      if (match(TokenKind::RParen)) {
        break;
      }
      continue;
    }
    std::optional<std::string> argName;
    if (match(TokenKind::LBracket)) {
      size_t savedPos = pos_;
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
            argName = name.text;
          } else {
            pos_ = savedPos;
          }
        } else {
          pos_ = savedPos;
        }
      } else {
        pos_ = savedPos;
      }
    } else if (match(TokenKind::Identifier) && pos_ + 1 < tokens_.size() &&
               tokens_[pos_ + 1].kind == TokenKind::Equal) {
      return fail("named arguments must use [name] syntax");
    } else if (match(TokenKind::Identifier)) {
      size_t scan = pos_ + 1;
      while (scan < tokens_.size() && isIgnorableToken(tokens_[scan].kind)) {
        ++scan;
      }
      if (scan < tokens_.size() && tokens_[scan].kind == TokenKind::Equal) {
        return fail("named arguments must use [name] syntax");
      }
    }
    Expr arg;
    if (!parseExpr(arg, namespacePrefix)) {
      return false;
    }
    out.push_back(std::move(arg));
    argNames.push_back(std::move(argName));
    if (match(TokenKind::Comma) || match(TokenKind::Semicolon)) {
      if (match(TokenKind::Comma)) {
        expect(TokenKind::Comma, "expected ','");
      } else {
        expect(TokenKind::Semicolon, "expected ';'");
      }
      while (match(TokenKind::Comma) || match(TokenKind::Semicolon)) {
        if (match(TokenKind::Comma)) {
          expect(TokenKind::Comma, "expected ','");
        } else {
          expect(TokenKind::Semicolon, "expected ';'");
        }
      }
      if (match(TokenKind::RParen)) {
        break;
      }
    } else {
      if (match(TokenKind::RParen)) {
        break;
      }
      continue;
    }
  }
  return true;
}

bool Parser::parseValueBlock(std::vector<Expr> &out, const std::string &namespacePrefix) {
  out.clear();
  struct ReturnContextGuard {
    explicit ReturnContextGuard(Parser &parser)
        : parser_(parser),
          previousTracker_(parser.returnTracker_),
          previousReturnsVoid_(parser.returnsVoidContext_),
          previousAllowImplicitVoid_(parser.allowImplicitVoidReturn_) {
      parser_.returnTracker_ = nullptr;
      parser_.returnsVoidContext_ = false;
      parser_.allowImplicitVoidReturn_ = false;
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
  if (!parseBraceExprList(out, namespacePrefix)) {
    return false;
  }
  return normalizeValueBlock(out);
}

bool Parser::normalizeValueBlock(std::vector<Expr> &body) {
  for (size_t i = 0; i < body.size(); ++i) {
    Expr &expr = body[i];
    if (expr.kind != Expr::Kind::Call || expr.name != "return") {
      continue;
    }
    if (expr.args.size() != 1) {
      return fail("return requires exactly one argument");
    }
    Expr value = std::move(expr.args.front());
    body[i] = std::move(value);
    body.resize(i + 1);
    break;
  }
  return true;
}

bool Parser::parseBindingInitializerList(std::vector<Expr> &out,
                                         std::vector<std::optional<std::string>> &argNames,
                                         const std::string &namespacePrefix) {
  Expr block;
  block.kind = Expr::Kind::Call;
  block.name = "block";
  block.namespacePrefix = namespacePrefix;
  block.hasBodyArguments = true;
  if (!parseValueBlock(block.bodyArguments, namespacePrefix)) {
    return false;
  }
  out.clear();
  argNames.clear();
  if (block.bodyArguments.size() == 1 && !block.bodyArguments.front().isBinding) {
    out.push_back(std::move(block.bodyArguments.front()));
    argNames.push_back(std::nullopt);
  } else {
    out.push_back(std::move(block));
    argNames.push_back(std::nullopt);
  }
  return true;
}

bool Parser::finalizeBindingInitializer(Expr &binding) {
  bool hasNamed = false;
  for (const auto &name : binding.argNames) {
    if (name.has_value()) {
      hasNamed = true;
      break;
    }
  }
  if (!hasNamed && binding.args.size() <= 1) {
    return true;
  }

  std::string typeName;
  std::vector<std::string> templateArgs;
  for (const auto &transform : binding.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities" || transform.name == "return") {
      continue;
    }
    if (isBindingAuxTransformName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    typeName = transform.name;
    templateArgs = transform.templateArgs;
    break;
  }
  if (typeName.empty()) {
    return fail("binding initializer requires explicit type");
  }
  if (hasNamed) {
    std::string normalized = typeName;
    if (!normalized.empty() && normalized[0] == '/') {
      normalized.erase(0, 1);
    }
    if (isBuiltinNameForArguments(normalized)) {
      return fail("named arguments not supported for builtin calls");
    }
  }

  Expr constructorCall;
  constructorCall.kind = Expr::Kind::Call;
  constructorCall.name = typeName;
  constructorCall.namespacePrefix = binding.namespacePrefix;
  constructorCall.templateArgs = std::move(templateArgs);
  constructorCall.args = std::move(binding.args);
  constructorCall.argNames = std::move(binding.argNames);
  binding.args.clear();
  binding.argNames.clear();
  binding.args.push_back(std::move(constructorCall));
  binding.argNames.push_back(std::nullopt);
  return true;
}

bool Parser::validateNamedArgumentOrdering(const std::vector<std::optional<std::string>> &argNames) {
  (void)argNames;
  return true;
}

bool Parser::parseBraceExprList(std::vector<Expr> &out, const std::string &namespacePrefix) {
  if (!expect(TokenKind::LBrace, "expected '{'")) {
    return false;
  }
  BareBindingGuard bindingGuard(*this, allowBraceBindings_);
  if (match(TokenKind::RBrace)) {
    expect(TokenKind::RBrace, "expected '}'");
    return true;
  }
  while (true) {
    if (match(TokenKind::Semicolon)) {
      expect(TokenKind::Semicolon, "expected ';'");
      if (match(TokenKind::RBrace)) {
        break;
      }
      continue;
    }
    if (!allowBraceBindings_ && match(TokenKind::LBracket)) {
      return fail("execution body arguments cannot be bindings");
    }
    Expr arg;
    if (match(TokenKind::Identifier) && tokens_[pos_].text == "if") {
      bool parsedIf = false;
      if (!tryParseIfStatementSugar(arg, namespacePrefix, parsedIf)) {
        return false;
      }
      if (parsedIf) {
        out.push_back(std::move(arg));
        if (match(TokenKind::Comma)) {
          expect(TokenKind::Comma, "expected ','");
          continue;
        }
        if (match(TokenKind::RBrace)) {
          break;
        }
        continue;
      }
    }
    if (match(TokenKind::Identifier) && tokens_[pos_].text == "return") {
      if (!allowBraceReturn_) {
        return fail("return not allowed in execution body");
      }
      if (!parseReturnStatement(arg, namespacePrefix)) {
        return false;
      }
    } else {
      if (!parseExpr(arg, namespacePrefix)) {
        return false;
      }
    }
    if (!allowBraceBindings_) {
      if (arg.kind != Expr::Kind::Call) {
        return fail("execution body arguments must be calls");
      }
      if (arg.isBinding) {
        return fail("execution body arguments cannot be bindings");
      }
    }
    out.push_back(std::move(arg));
    if (match(TokenKind::Comma) || match(TokenKind::Semicolon)) {
      if (match(TokenKind::Comma)) {
        expect(TokenKind::Comma, "expected ','");
      } else {
        expect(TokenKind::Semicolon, "expected ';'");
      }
      while (match(TokenKind::Comma) || match(TokenKind::Semicolon)) {
        if (match(TokenKind::Comma)) {
          expect(TokenKind::Comma, "expected ','");
        } else {
          expect(TokenKind::Semicolon, "expected ';'");
        }
      }
      if (match(TokenKind::RBrace)) {
        break;
      }
      continue;
    }
    if (match(TokenKind::RBrace)) {
      break;
    }
  }
  if (!expect(TokenKind::RBrace, "expected '}'")) {
    return false;
  }
  return true;
}

} // namespace primec
