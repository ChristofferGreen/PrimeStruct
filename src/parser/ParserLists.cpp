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

bool isSimpleSpreadMarker(const std::vector<Token> &tokens, size_t index) {
  return index + 2 < tokens.size() && tokens[index].kind == TokenKind::LBracket &&
         tokens[index + 1].kind == TokenKind::Identifier && tokens[index + 1].text == "spread" &&
         tokens[index + 2].kind == TokenKind::RBracket;
}

bool isIgnorableToken(TokenKind kind) {
  return kind == TokenKind::Comment;
}

bool isVectorBindingTypeName(std::string name) {
  if (!name.empty() && name.front() == '/') {
    name.erase(0, 1);
  }
  return name == "vector" || name == "std/collections/vector" ||
         name == "std/collections/vector/vector";
}
} // namespace

bool Parser::parseCallArgumentList(std::vector<Expr> &out,
                                   std::vector<std::optional<std::string>> &argNames,
                                   const std::string &namespacePrefix) {
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

  if (matchRaw(TokenKind::RParen)) {
    return true;
  }
  ArgumentLabelGuard labelGuard(*this);
  bool sawSpreadArg = false;
  while (true) {
    if (matchRaw(TokenKind::Comma) || matchRaw(TokenKind::Semicolon)) {
      if (matchRaw(TokenKind::Comma)) {
        expectRaw(TokenKind::Comma, "expected ','");
      } else {
        expectRaw(TokenKind::Semicolon, "expected ';'");
      }
      while (matchRaw(TokenKind::Comma) || matchRaw(TokenKind::Semicolon)) {
        if (matchRaw(TokenKind::Comma)) {
          expectRaw(TokenKind::Comma, "expected ','");
        } else {
          expectRaw(TokenKind::Semicolon, "expected ';'");
        }
      }
      if (matchRaw(TokenKind::RParen)) {
        break;
      }
      continue;
    }
    std::optional<std::string> argName;
    if (matchRaw(TokenKind::LBracket) && !isSimpleSpreadMarker(tokens_, pos_)) {
      size_t savedPos = pos_;
      expectRaw(TokenKind::LBracket, "expected '['");
      skipComments();
      if (matchRaw(TokenKind::Identifier)) {
        Token name = consumeRaw(TokenKind::Identifier, "expected argument label");
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
        if (matchRaw(TokenKind::RBracket)) {
          expectRaw(TokenKind::RBracket, "expected ']' after argument label");
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
    } else if (matchRaw(TokenKind::Identifier) && pos_ + 1 < tokens_.size() &&
               tokens_[pos_ + 1].kind == TokenKind::Equal) {
      return fail("named arguments must use [name] syntax");
    } else if (matchRaw(TokenKind::Identifier)) {
      size_t scan = pos_ + 1;
      while (scan < tokens_.size() && isIgnorableToken(tokens_[scan].kind)) {
        ++scan;
      }
      if (scan < tokens_.size() && tokens_[scan].kind == TokenKind::Equal) {
        return fail("named arguments must use [name] syntax");
      }
    }
    bool prefixedSpread = false;
    if (isSimpleSpreadMarker(tokens_, pos_)) {
      prefixedSpread = true;
      expectRaw(TokenKind::LBracket, "expected '['");
      Token spread = consumeRaw(TokenKind::Identifier, "expected spread marker");
      if (spread.kind == TokenKind::End) {
        return false;
      }
      if (spread.text != "spread") {
        return fail("expected spread marker");
      }
      if (!expectRaw(TokenKind::RBracket, "expected ']' after spread marker")) {
        return false;
      }
    }
    Expr arg;
    {
      BareBindingGuard bindingGuard(*this, false);
      if (!parseExpr(arg, namespacePrefix)) {
        return false;
      }
    }
    bool suffixedSpread = false;
    if (matchRaw(TokenKind::Ellipsis)) {
      expectRaw(TokenKind::Ellipsis, "expected '...'");
      suffixedSpread = true;
    }
    if (prefixedSpread && suffixedSpread) {
      return fail("spread argument cannot use both [spread] and ...");
    }
    if (prefixedSpread || suffixedSpread) {
      if (argName.has_value()) {
        return fail("spread argument does not accept named label");
      }
      if (sawSpreadArg) {
        return fail("call accepts at most one spread argument");
      }
      arg.isSpread = true;
      sawSpreadArg = true;
    }
    out.push_back(std::move(arg));
    argNames.push_back(std::move(argName));
    if (matchRaw(TokenKind::Comma) || matchRaw(TokenKind::Semicolon)) {
      if (sawSpreadArg) {
        return fail("spread argument must be final");
      }
      if (matchRaw(TokenKind::Comma)) {
        expectRaw(TokenKind::Comma, "expected ','");
      } else {
        expectRaw(TokenKind::Semicolon, "expected ';'");
      }
      while (matchRaw(TokenKind::Comma) || matchRaw(TokenKind::Semicolon)) {
        if (matchRaw(TokenKind::Comma)) {
          expectRaw(TokenKind::Comma, "expected ','");
        } else {
          expectRaw(TokenKind::Semicolon, "expected ';'");
        }
      }
      if (matchRaw(TokenKind::RParen)) {
        break;
      }
    } else {
      if (matchRaw(TokenKind::RParen)) {
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
  if (!parseBraceExprList(out, namespacePrefix, false)) {
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
  auto tryParseLabeledInitializerList = [&]() -> bool {
    const size_t savedPos = pos_;
    std::string ignoredError;
    std::string *savedError = error_;
    auto *savedErrorInfos = errorInfos_;
    const ErrorInfo savedErrorInfo = lastErrorInfo_;
    error_ = &ignoredError;
    errorInfos_ = nullptr;

    auto restoreState = [&](bool rewind) {
      if (rewind) {
        pos_ = savedPos;
      }
      error_ = savedError;
      errorInfos_ = savedErrorInfos;
      lastErrorInfo_ = savedErrorInfo;
    };
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

    if (!expectRaw(TokenKind::LBrace, "expected '{'")) {
      restoreState(true);
      return false;
    }
    if (matchRaw(TokenKind::RBrace)) {
      restoreState(true);
      return false;
    }

    ArgumentLabelGuard labelGuard(*this);
    std::vector<Expr> labeledArgs;
    std::vector<std::optional<std::string>> labeledArgNames;
    while (true) {
      if (!expectRaw(TokenKind::LBracket, "expected '['")) {
        restoreState(true);
        return false;
      }
      Token label = consumeRaw(TokenKind::Identifier, "expected argument label");
      if (label.kind == TokenKind::End) {
        restoreState(true);
        return false;
      }
      if (label.text.find('/') != std::string::npos) {
        restoreState(true);
        return false;
      }
      std::string labelError;
      if (!validateIdentifierText(label.text, labelError)) {
        restoreState(true);
        return false;
      }
      if (!expectRaw(TokenKind::RBracket, "expected ']' after argument label")) {
        restoreState(true);
        return false;
      }
      size_t nextIndex = pos_;
      while (nextIndex < tokens_.size() && isIgnorableToken(tokens_[nextIndex].kind)) {
        ++nextIndex;
      }
      if (nextIndex >= tokens_.size() ||
          !isArgumentLabelValueStart(tokens_[nextIndex].kind)) {
        restoreState(true);
        return false;
      }

      Expr arg;
      {
        BareBindingGuard bindingGuard(*this, false);
        if (!parseExpr(arg, namespacePrefix)) {
          restoreState(true);
          return false;
        }
      }
      labeledArgs.push_back(std::move(arg));
      labeledArgNames.push_back(label.text);

      if (matchRaw(TokenKind::Comma) || matchRaw(TokenKind::Semicolon)) {
        while (matchRaw(TokenKind::Comma) || matchRaw(TokenKind::Semicolon)) {
          if (matchRaw(TokenKind::Comma)) {
            expectRaw(TokenKind::Comma, "expected ','");
          } else {
            expectRaw(TokenKind::Semicolon, "expected ';'");
          }
        }
        if (matchRaw(TokenKind::RBrace)) {
          break;
        }
        continue;
      }
      if (matchRaw(TokenKind::RBrace)) {
        break;
      }
      restoreState(true);
      return false;
    }

    if (!expectRaw(TokenKind::RBrace, "expected '}'")) {
      restoreState(true);
      return false;
    }

    out = std::move(labeledArgs);
    argNames = std::move(labeledArgNames);
    restoreState(false);
    return true;
  };

  if (tryParseLabeledInitializerList()) {
    return true;
  }

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
  const bool hasSingleInitializer = !hasNamed && binding.args.size() <= 1;
  const bool hasBlockInitializer = binding.args.size() == 1 &&
                                   binding.args.front().kind == Expr::Kind::Call &&
                                   binding.args.front().name == "block" &&
                                   binding.args.front().hasBodyArguments;

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
  if (hasSingleInitializer && (!hasBlockInitializer || typeName.empty() ||
                               !isVectorBindingTypeName(typeName))) {
    return true;
  }

  if (typeName.empty()) {
    return fail("binding initializer requires explicit type");
  }
  const bool conciseVectorInitializer = hasSingleInitializer && hasBlockInitializer &&
                                        isVectorBindingTypeName(typeName);
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
  if (conciseVectorInitializer) {
    Expr block = std::move(binding.args.front());
    constructorCall.args = std::move(block.bodyArguments);
    constructorCall.argNames.assign(constructorCall.args.size(), std::nullopt);
  } else {
    constructorCall.args = std::move(binding.args);
    constructorCall.argNames = std::move(binding.argNames);
  }
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

bool Parser::parseBraceExprList(std::vector<Expr> &out,
                                const std::string &namespacePrefix,
                                bool allowSingleBranchIfStatement) {
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
    Expr arg;
    if (match(TokenKind::Identifier) &&
        (tokens_[pos_].text == "if" || tokens_[pos_].text == "match")) {
      bool parsedIf = false;
      if (!tryParseIfStatementSugar(arg, namespacePrefix, parsedIf, allowSingleBranchIfStatement)) {
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
      SingleBranchIfGuard singleBranchGuard(*this, allowSingleBranchIfStatement);
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
