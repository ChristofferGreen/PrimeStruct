#include "primec/Parser.h"

#include "ParserHelpers.h"

#include <sstream>
#include <utility>

namespace primec {
using namespace parser;

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

bool Parser::parse(Program &program, std::string &error) {
  error_ = &error;
  while (!match(TokenKind::End)) {
    if (match(TokenKind::Semicolon)) {
      return fail("semicolon is not allowed");
    }
    if (match(TokenKind::KeywordNamespace)) {
      if (!parseNamespace(program.definitions, program.executions)) {
        return false;
      }
    } else {
      if (!parseDefinitionOrExecution(program.definitions, program.executions)) {
        return false;
      }
    }
  }
  return true;
}

bool Parser::parseNamespace(std::vector<Definition> &defs, std::vector<Execution> &execs) {
  if (!expect(TokenKind::KeywordNamespace, "expected 'namespace'")) {
    return false;
  }
  Token name = consume(TokenKind::Identifier, "expected namespace identifier");
  if (name.kind == TokenKind::End) {
    return false;
  }
  if (name.text.find('/') != std::string::npos) {
    return fail("namespace name must be a simple identifier");
  }
  std::string nameError;
  if (!validateIdentifierText(name.text, nameError)) {
    return fail(nameError);
  }
  if (!expect(TokenKind::LBrace, "expected '{' after namespace")) {
    return false;
  }
  namespaceStack_.push_back(name.text);
  while (!match(TokenKind::RBrace)) {
    if (match(TokenKind::End)) {
      return fail("unexpected end of file inside namespace block");
    }
    if (match(TokenKind::Semicolon)) {
      return fail("semicolon is not allowed");
    }
    if (match(TokenKind::KeywordNamespace)) {
      if (!parseNamespace(defs, execs)) {
        return false;
      }
    } else if (!parseDefinitionOrExecution(defs, execs)) {
      return false;
    }
  }
  expect(TokenKind::RBrace, "expected '}'");
  namespaceStack_.pop_back();
  return true;
}

bool Parser::parseDefinitionOrExecution(std::vector<Definition> &defs, std::vector<Execution> &execs) {
  std::vector<Transform> transforms;
  if (match(TokenKind::LBracket)) {
    if (!parseTransformList(transforms)) {
      return false;
    }
  }

  Token name = consume(TokenKind::Identifier, "expected identifier");
  if (name.kind == TokenKind::End) {
    return false;
  }
  std::string nameError;
  if (!validateIdentifierText(name.text, nameError)) {
    return fail(nameError);
  }

  std::vector<std::string> templateArgs;
  if (match(TokenKind::LAngle)) {
    if (!parseTemplateList(templateArgs)) {
      return false;
    }
  }

  if (!expect(TokenKind::LParen, "expected '(' after identifier")) {
    return false;
  }
  bool hasReturnTransform = false;
  bool hasStructTransform = false;
  for (const auto &transform : transforms) {
    if (transform.name == "return") {
      hasReturnTransform = true;
    }
    if (isStructTransformName(transform.name)) {
      hasStructTransform = true;
    }
  }
  bool paramsAreIdentifiers = false;
  bool isDefinition = false;
  if (hasReturnTransform) {
    isDefinition = true;
  } else if (hasStructTransform) {
    isDefinition = isDefinitionSignatureAllowNoReturn(&paramsAreIdentifiers);
  } else {
    isDefinition = isDefinitionSignature(&paramsAreIdentifiers);
  }
  if (isDefinition && !hasReturnTransform && !hasStructTransform) {
    // Definitions without return transforms are allowed as long as they are unambiguous.
  }
  if (isDefinition) {
    std::vector<Expr> parameters;
    if (!parseParameterList(parameters, currentNamespacePrefix())) {
      return false;
    }
    if (!expect(TokenKind::RParen, "expected ')' after parameters")) {
      return false;
    }
    if (!match(TokenKind::LBrace)) {
      return fail("definitions must have a body");
    }
    Definition def;
    def.name = name.text;
    def.namespacePrefix = currentNamespacePrefix();
    def.fullPath = makeFullPath(def.name, def.namespacePrefix);
    def.transforms = std::move(transforms);
    def.templateArgs = std::move(templateArgs);
    def.parameters = std::move(parameters);
    if (!parseDefinitionBody(def, hasStructTransform)) {
      return false;
    }
    defs.push_back(std::move(def));
    return true;
  }

  if (paramsAreIdentifiers && definitionHasReturnBeforeClose()) {
    return fail("definition missing return statement");
  }

  std::vector<Expr> arguments;
  std::vector<std::optional<std::string>> argumentNames;
  if (!parseCallArgumentList(arguments, argumentNames, currentNamespacePrefix())) {
    return false;
  }
  if (!validateNamedArgumentOrdering(argumentNames)) {
    return false;
  }
  if (!expect(TokenKind::RParen, "expected ')' after arguments")) {
    return false;
  }

  if (!match(TokenKind::LBrace)) {
    Execution exec;
    exec.name = name.text;
    exec.namespacePrefix = currentNamespacePrefix();
    exec.fullPath = makeFullPath(exec.name, exec.namespacePrefix);
    exec.transforms = std::move(transforms);
    exec.templateArgs = std::move(templateArgs);
    exec.arguments = std::move(arguments);
    exec.argumentNames = std::move(argumentNames);
    exec.hasBodyArguments = false;
    execs.push_back(std::move(exec));
    return true;
  }

  Execution exec;
  exec.name = name.text;
  exec.namespacePrefix = currentNamespacePrefix();
  exec.fullPath = makeFullPath(exec.name, exec.namespacePrefix);
  exec.transforms = std::move(transforms);
  exec.templateArgs = std::move(templateArgs);
  exec.arguments = std::move(arguments);
  exec.argumentNames = std::move(argumentNames);
  if (!parseBraceExprList(exec.bodyArguments, exec.namespacePrefix)) {
    return false;
  }
  exec.hasBodyArguments = true;
  execs.push_back(std::move(exec));
  return true;
}

bool Parser::definitionHasReturnBeforeClose() const {
  size_t index = pos_;
  int depth = 1;
  while (index < tokens_.size()) {
    TokenKind kind = tokens_[index].kind;
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
    }
    ++index;
  }
  if (depth != 0) {
    return false;
  }
  if (index + 1 >= tokens_.size()) {
    return false;
  }
  if (tokens_[index + 1].kind != TokenKind::LBrace) {
    return false;
  }

  size_t braceIndex = index + 1;
  int braceDepth = 0;
  while (braceIndex < tokens_.size()) {
    TokenKind kind = tokens_[braceIndex].kind;
    if (kind == TokenKind::Comment) {
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
  bool identifiersOnly = true;
  bool sawBindingSyntax = false;
  while (index < tokens_.size()) {
    TokenKind kind = tokens_[index].kind;
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
      if (kind == TokenKind::LBracket) {
        sawBindingSyntax = true;
        identifiersOnly = false;
      } else if (kind != TokenKind::Identifier && kind != TokenKind::Comma) {
        identifiersOnly = false;
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
  if (index + 1 >= tokens_.size()) {
    return false;
  }
  return tokens_[index + 1].kind == TokenKind::LBrace;
}

bool Parser::isDefinitionSignatureAllowNoReturn(bool *paramsAreIdentifiers) const {
  size_t index = pos_;
  int depth = 1;
  bool identifiersOnly = true;
  bool sawBindingSyntax = false;
  while (index < tokens_.size()) {
    TokenKind kind = tokens_[index].kind;
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
      if (kind == TokenKind::LBracket) {
        sawBindingSyntax = true;
        identifiersOnly = false;
      } else if (kind != TokenKind::Identifier && kind != TokenKind::Comma) {
        identifiersOnly = false;
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
  if (index + 1 >= tokens_.size()) {
    return false;
  }
  return tokens_[index + 1].kind == TokenKind::LBrace;
}

bool Parser::parseDefinitionBody(Definition &def, bool allowNoReturn) {
  bool returnsVoid = false;
  bool hasReturnTransform = false;
  for (const auto &transform : def.transforms) {
    if (transform.name == "return" && transform.templateArgs.size() == 1 && transform.templateArgs.front() == "void") {
      returnsVoid = true;
    }
    if (transform.name == "return") {
      hasReturnTransform = true;
    }
  }
  const bool allowImplicitVoidReturn = !hasReturnTransform && !allowNoReturn;
  bool foundReturn = false;
  struct ReturnContextGuard {
    Parser *parser;
    bool *prevTracker;
    bool prevReturnsVoid;
    bool prevAllowImplicitVoid;
    ReturnContextGuard(Parser *parserIn, bool *tracker, bool returnsVoid, bool allowImplicitVoid)
        : parser(parserIn),
          prevTracker(parserIn->returnTracker_),
          prevReturnsVoid(parserIn->returnsVoidContext_),
          prevAllowImplicitVoid(parserIn->allowImplicitVoidReturn_) {
      parser->returnTracker_ = tracker;
      parser->returnsVoidContext_ = returnsVoid;
      parser->allowImplicitVoidReturn_ = allowImplicitVoid;
    }
    ~ReturnContextGuard() {
      parser->returnTracker_ = prevTracker;
      parser->returnsVoidContext_ = prevReturnsVoid;
      parser->allowImplicitVoidReturn_ = prevAllowImplicitVoid;
    }
  } guard(this, &foundReturn, returnsVoid, allowImplicitVoidReturn);
  if (!expect(TokenKind::LBrace, "expected '{' to start body")) {
    return false;
  }
  while (!match(TokenKind::RBrace)) {
    if (match(TokenKind::End)) {
      return fail("unexpected end of file inside definition body");
    }
    if (match(TokenKind::Semicolon)) {
      return fail("semicolon is not allowed");
    }
    std::vector<Transform> statementTransforms;
    bool hasStatementTransforms = false;
    if (match(TokenKind::LBracket)) {
      if (!parseTransformList(statementTransforms)) {
        return false;
      }
      hasStatementTransforms = true;
    }
    if (match(TokenKind::Identifier) && tokens_[pos_].text == "return") {
      if (hasStatementTransforms) {
        return fail("return statement cannot have transforms");
      }
      Expr returnCall;
      if (!parseReturnStatement(returnCall, def.namespacePrefix)) {
        return false;
      }
      def.hasReturnStatement = true;
      if (!returnCall.args.empty()) {
        def.returnExpr = returnCall.args.front();
      }
      def.statements.push_back(std::move(returnCall));
      continue;
    }
    if (match(TokenKind::Identifier) && tokens_[pos_].text == "if") {
      if (hasStatementTransforms) {
        return fail("if statement cannot have transforms");
      }
      Expr ifExpr;
      bool parsedIf = false;
      if (!tryParseIfStatementSugar(ifExpr, def.namespacePrefix, parsedIf)) {
        return false;
      }
      if (parsedIf) {
        def.statements.push_back(std::move(ifExpr));
        continue;
      }
    }

    if (!statementTransforms.empty()) {
      Token name = consume(TokenKind::Identifier, "expected statement");
      if (name.kind == TokenKind::End) {
        return false;
      }
      std::string nameError;
      if (!validateIdentifierText(name.text, nameError)) {
        return fail(nameError);
      }
      if (name.text == "if") {
        return fail("if statement cannot have transforms");
      }
      if (name.text == "return") {
        return fail("return statement cannot have transforms");
      }

      Expr callExpr;
      callExpr.kind = Expr::Kind::Call;
      callExpr.name = name.text;
      callExpr.namespacePrefix = def.namespacePrefix;
      callExpr.transforms = std::move(statementTransforms);
      callExpr.isBinding = !callExpr.transforms.empty();
      if (match(TokenKind::LAngle)) {
        if (!parseTemplateList(callExpr.templateArgs)) {
          return false;
        }
      }
      if (match(TokenKind::LParen)) {
        return fail("binding initializers must use braces");
      }
      if (match(TokenKind::LBrace)) {
        if (!parseBindingInitializerList(callExpr.args, def.namespacePrefix)) {
          return false;
        }
      } else {
        return fail("binding requires initializer");
      }
      def.statements.push_back(std::move(callExpr));
      continue;
    }

    Expr statementExpr;
    if (!parseExpr(statementExpr, def.namespacePrefix)) {
      return false;
    }
    def.statements.push_back(std::move(statementExpr));
  }
  expect(TokenKind::RBrace, "expected '}' to close body");
  if (!foundReturn && !returnsVoid && !allowNoReturn) {
    if (!allowImplicitVoidReturn) {
      return fail("missing return statement in definition body");
    }
  }
  def.hasReturnStatement = foundReturn;
  return true;
}

std::string Parser::currentNamespacePrefix() const {
  if (namespaceStack_.empty()) {
    return "";
  }
  std::ostringstream out;
  for (const auto &segment : namespaceStack_) {
    out << "/" << segment;
  }
  return out.str();
}

std::string Parser::makeFullPath(const std::string &name, const std::string &prefix) const {
  if (!name.empty() && name[0] == '/') {
    return name;
  }
  if (prefix.empty()) {
    return "/" + name;
  }
  return prefix + "/" + name;
}

void Parser::skipComments() {
  while (tokens_[pos_].kind == TokenKind::Comment) {
    ++pos_;
  }
}

bool Parser::match(TokenKind kind) {
  skipComments();
  return tokens_[pos_].kind == kind;
}

bool Parser::expect(TokenKind kind, const std::string &message) {
  if (match(TokenKind::Invalid)) {
    return fail(describeInvalidToken(tokens_[pos_]));
  }
  if (!match(kind)) {
    return fail(message);
  }
  ++pos_;
  return true;
}

Token Parser::consume(TokenKind kind, const std::string &message) {
  if (match(TokenKind::Invalid)) {
    fail(describeInvalidToken(tokens_[pos_]));
    return {TokenKind::End, ""};
  }
  if (!match(kind)) {
    fail(message);
    return {TokenKind::End, ""};
  }
  return tokens_[pos_++];
}

bool Parser::fail(const std::string &message) {
  if (error_) {
    const Token &token = tokens_[pos_];
    std::ostringstream out;
    if (token.kind == TokenKind::Invalid) {
      if (token.text.size() == 1) {
        out << "invalid character: " << describeInvalidToken(token);
      } else {
        out << describeInvalidToken(token);
      }
      out << " at " << token.line << ":" << token.column;
    } else {
      out << message << " at " << token.line << ":" << token.column;
    }
    *error_ = out.str();
  }
  return false;
}

} // namespace primec
