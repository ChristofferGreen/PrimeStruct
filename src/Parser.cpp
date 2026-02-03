#include "primec/Parser.h"

#include <limits>
#include <sstream>

namespace primec {

namespace {
bool isReservedKeyword(const std::string &text) {
  return text == "mut" || text == "return" || text == "include" || text == "namespace" || text == "true" ||
         text == "false" || text == "null";
}
} // namespace

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

bool Parser::parse(Program &program, std::string &error) {
  error_ = &error;
  while (!match(TokenKind::End)) {
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
  if (isReservedKeyword(name.text)) {
    return fail("reserved keyword cannot be used as identifier: " + name.text);
  }
  if (!expect(TokenKind::LBrace, "expected '{' after namespace")) {
    return false;
  }
  namespaceStack_.push_back(name.text);
  while (!match(TokenKind::RBrace)) {
    if (match(TokenKind::End)) {
      return fail("unexpected end of file inside namespace block");
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
  if (isReservedKeyword(name.text)) {
    return fail("reserved keyword cannot be used as identifier: " + name.text);
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
  bool isDefinition = isDefinitionSignature();
  if (isDefinition) {
    std::vector<std::string> parameters;
    if (!parseIdentifierList(parameters)) {
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
    if (!parseDefinitionBody(def)) {
      return false;
    }
    defs.push_back(std::move(def));
    return true;
  }

  std::vector<Expr> arguments;
  if (!parseExprList(arguments, currentNamespacePrefix())) {
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
  if (!parseBraceExprList(exec.bodyArguments, exec.namespacePrefix)) {
    return false;
  }
  execs.push_back(std::move(exec));
  return true;
}

bool Parser::parseTransformList(std::vector<Transform> &out) {
  if (!expect(TokenKind::LBracket, "expected '['")) {
    return false;
  }
  while (!match(TokenKind::RBracket)) {
    Token name = consume(TokenKind::Identifier, "expected transform identifier");
    if (name.kind == TokenKind::End) {
      return false;
    }
    Transform transform;
    transform.name = name.text;
    if (match(TokenKind::LAngle)) {
      expect(TokenKind::LAngle, "expected '<'");
      Token arg = consume(TokenKind::Identifier, "expected template argument");
      if (arg.kind == TokenKind::End) {
        return false;
      }
      transform.templateArg = arg.text;
      if (!expect(TokenKind::RAngle, "expected '>'")) {
        return false;
      }
    }
    out.push_back(std::move(transform));
    if (match(TokenKind::Comma)) {
      expect(TokenKind::Comma, "expected ','");
    }
  }
  expect(TokenKind::RBracket, "expected ']'" );
  return true;
}

bool Parser::parseTemplateList(std::vector<std::string> &out) {
  if (!expect(TokenKind::LAngle, "expected '<'")) {
    return false;
  }
  while (!match(TokenKind::RAngle)) {
    Token name = consume(TokenKind::Identifier, "expected template identifier");
    if (name.kind == TokenKind::End) {
      return false;
    }
    out.push_back(name.text);
    if (match(TokenKind::Comma)) {
      expect(TokenKind::Comma, "expected ','");
    } else {
      break;
    }
  }
  if (!expect(TokenKind::RAngle, "expected '>'")) {
    return false;
  }
  return true;
}

bool Parser::parseIdentifierList(std::vector<std::string> &out) {
  if (match(TokenKind::RParen)) {
    return true;
  }
  Token first = consume(TokenKind::Identifier, "expected parameter identifier");
  if (first.kind == TokenKind::End) {
    return false;
  }
  if (isReservedKeyword(first.text)) {
    return fail("reserved keyword cannot be used as identifier: " + first.text);
  }
  out.push_back(first.text);
  if (!match(TokenKind::RParen)) {
    while (match(TokenKind::Comma)) {
      expect(TokenKind::Comma, "expected ','");
      Token next = consume(TokenKind::Identifier, "expected parameter identifier");
      if (next.kind == TokenKind::End) {
        return false;
      }
      if (isReservedKeyword(next.text)) {
        return fail("reserved keyword cannot be used as identifier: " + next.text);
      }
      out.push_back(next.text);
    }
  }
  return true;
}

bool Parser::parseExprList(std::vector<Expr> &out, const std::string &namespacePrefix) {
  if (match(TokenKind::RParen)) {
    return true;
  }
  while (true) {
    Expr arg;
    if (!parseExpr(arg, namespacePrefix)) {
      return false;
    }
    out.push_back(std::move(arg));
    if (match(TokenKind::Comma)) {
      expect(TokenKind::Comma, "expected ','");
    } else {
      break;
    }
  }
  return true;
}

bool Parser::parseBraceExprList(std::vector<Expr> &out, const std::string &namespacePrefix) {
  if (!expect(TokenKind::LBrace, "expected '{'")) {
    return false;
  }
  if (match(TokenKind::RBrace)) {
    expect(TokenKind::RBrace, "expected '}'");
    return true;
  }
  while (true) {
    Expr arg;
    if (!parseExpr(arg, namespacePrefix)) {
      return false;
    }
    out.push_back(std::move(arg));
    if (match(TokenKind::Comma)) {
      expect(TokenKind::Comma, "expected ','");
    } else {
      break;
    }
  }
  if (!expect(TokenKind::RBrace, "expected '}'")) {
    return false;
  }
  return true;
}

bool Parser::isDefinitionSignature() const {
  size_t index = pos_;
  int depth = 1;
  bool paramsAreIdentifiers = true;
  while (index < tokens_.size()) {
    TokenKind kind = tokens_[index].kind;
    if (kind == TokenKind::LParen) {
      ++depth;
    } else if (kind == TokenKind::RParen) {
      --depth;
      if (depth == 0) {
        break;
      }
    } else if (depth == 1) {
      if (kind != TokenKind::Identifier && kind != TokenKind::Comma) {
        paramsAreIdentifiers = false;
      }
    }
    ++index;
  }
  if (depth != 0) {
    return false;
  }
  if (!paramsAreIdentifiers) {
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

bool Parser::parseDefinitionBody(Definition &def) {
  if (!expect(TokenKind::LBrace, "expected '{' to start body")) {
    return false;
  }
  while (!match(TokenKind::RBrace)) {
    if (match(TokenKind::End)) {
      return fail("unexpected end of file inside definition body");
    }
    Token name = consume(TokenKind::Identifier, "expected statement");
    if (name.kind == TokenKind::End) {
      return false;
    }
    if (name.text == "return") {
      if (def.returnExpr) {
        return fail("multiple return statements are not supported");
      }
      Expr returnCall = {};
      returnCall.kind = Expr::Kind::Call;
      returnCall.name = name.text;
      returnCall.namespacePrefix = def.namespacePrefix;
      if (!expect(TokenKind::LParen, "expected '(' after return")) {
        return false;
      }
      if (match(TokenKind::RParen)) {
        return fail("return requires exactly one argument");
      }
      Expr arg;
      if (!parseExpr(arg, def.namespacePrefix)) {
        return false;
      }
      returnCall.args.push_back(std::move(arg));
      if (match(TokenKind::Comma)) {
        return fail("return requires exactly one argument");
      }
      if (!expect(TokenKind::RParen, "expected ')' after return argument")) {
        return false;
      }
      def.returnExpr = returnCall.args.front();
      if (!expect(TokenKind::RBrace, "expected '}' after return statement")) {
        return false;
      }
      return true;
    }
    if (isReservedKeyword(name.text)) {
      return fail("reserved keyword cannot be used as identifier: " + name.text);
    }

    Expr callExpr;
    callExpr.kind = Expr::Kind::Call;
    callExpr.name = name.text;
    callExpr.namespacePrefix = def.namespacePrefix;
    if (!expect(TokenKind::LParen, "expected '(' after identifier")) {
      return false;
    }
    if (!match(TokenKind::RParen)) {
      while (true) {
        Expr arg;
        if (!parseExpr(arg, def.namespacePrefix)) {
          return false;
        }
        callExpr.args.push_back(std::move(arg));
        if (match(TokenKind::Comma)) {
          expect(TokenKind::Comma, "expected ','");
        } else {
          break;
        }
      }
    }
    if (!expect(TokenKind::RParen, "expected ')' after call statement")) {
      return false;
    }
    def.statements.push_back(std::move(callExpr));
  }
  expect(TokenKind::RBrace, "expected '}' to close body");
  if (!def.returnExpr) {
    return fail("missing return statement in definition body");
  }
  return true;
}

bool Parser::parseExpr(Expr &expr, const std::string &namespacePrefix) {
  if (match(TokenKind::Number)) {
    Token number = consume(TokenKind::Number, "expected number");
    if (number.kind == TokenKind::End) {
      return false;
    }
    expr.kind = Expr::Kind::Literal;
    expr.namespacePrefix = namespacePrefix;
    std::string text = number.text;
    if (text.size() >= 3 && text.compare(text.size() - 3, 3, "i32") == 0) {
      text = text.substr(0, text.size() - 3);
    }
    try {
      long long value = std::stoll(text);
      if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
        return fail("integer literal out of range");
      }
      expr.literalValue = static_cast<int>(value);
    } catch (const std::exception &) {
      return fail("invalid integer literal");
    }
    return true;
  }
  if (match(TokenKind::Identifier)) {
    Token name = consume(TokenKind::Identifier, "expected identifier");
    if (name.kind == TokenKind::End) {
      return false;
    }
    if (isReservedKeyword(name.text)) {
      return fail("reserved keyword cannot be used as identifier: " + name.text);
    }
    std::vector<std::string> templateArgs;
    if (match(TokenKind::LAngle)) {
      if (!parseTemplateList(templateArgs)) {
        return false;
      }
    }
    if (match(TokenKind::LParen)) {
      expect(TokenKind::LParen, "expected '(' after identifier");
      Expr call;
      call.kind = Expr::Kind::Call;
      call.name = name.text;
      call.namespacePrefix = namespacePrefix;
      call.templateArgs = std::move(templateArgs);
      if (!match(TokenKind::RParen)) {
        while (true) {
          Expr arg;
          if (!parseExpr(arg, namespacePrefix)) {
            return false;
          }
          call.args.push_back(std::move(arg));
          if (match(TokenKind::Comma)) {
            expect(TokenKind::Comma, "expected ','");
          } else {
            break;
          }
        }
      }
      if (!expect(TokenKind::RParen, "expected ')' to close call")) {
        return false;
      }
      expr = std::move(call);
      return true;
    }

    if (!templateArgs.empty()) {
      return fail("template arguments require a call");
    }
    expr.kind = Expr::Kind::Name;
    expr.name = name.text;
    expr.namespacePrefix = namespacePrefix;
    return true;
  }
  return fail("unexpected token in expression");
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

bool Parser::match(TokenKind kind) const {
  return tokens_[pos_].kind == kind;
}

bool Parser::expect(TokenKind kind, const std::string &message) {
  if (!match(kind)) {
    return fail(message);
  }
  ++pos_;
  return true;
}

Token Parser::consume(TokenKind kind, const std::string &message) {
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
    out << message << " at " << token.line << ":" << token.column;
    *error_ = out.str();
  }
  return false;
}

} // namespace primec
