#include "primec/Parser.h"
#include "primec/StringLiteral.h"

#include <cctype>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>

namespace primec {

namespace {
std::string describeInvalidToken(const Token &token) {
  if (token.text.size() != 1) {
    return token.text.empty() ? "<unknown>" : token.text;
  }
  const unsigned char byte = static_cast<unsigned char>(token.text[0]);
  if (byte >= 0x20 && byte <= 0x7E) {
    return std::string("'") + static_cast<char>(byte) + "'";
  }
  std::ostringstream out;
  out << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
  return out.str();
}

bool isReservedKeyword(const std::string &text) {
  return text == "mut" || text == "return" || text == "include" || text == "namespace" || text == "true" ||
         text == "false";
}

bool isAsciiAlpha(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool isAsciiDigit(char c) {
  return c >= '0' && c <= '9';
}

bool isIdentifierSegmentStart(char c) {
  return isAsciiAlpha(c) || c == '_';
}

bool isIdentifierSegmentChar(char c) {
  return isAsciiAlpha(c) || isAsciiDigit(c) || c == '_';
}

bool validateSlashPath(const std::string &text, std::string &error) {
  if (text.size() < 2 || text[0] != '/') {
    error = "invalid slash path identifier: " + text;
    return false;
  }
  size_t start = 1;
  while (start < text.size()) {
    size_t end = text.find('/', start);
    std::string segment = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (segment.empty() || !isIdentifierSegmentStart(segment[0])) {
      error = "invalid slash path identifier: " + text;
      return false;
    }
    for (size_t i = 1; i < segment.size(); ++i) {
      if (!isIdentifierSegmentChar(segment[i])) {
        error = "invalid slash path identifier: " + text;
        return false;
      }
    }
    if (isReservedKeyword(segment)) {
      error = "reserved keyword cannot be used as identifier: " + segment;
      return false;
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  if (text.back() == '/') {
    error = "invalid slash path identifier: " + text;
    return false;
  }
  return true;
}

bool validateIdentifierText(const std::string &text, std::string &error) {
  if (text.empty()) {
    error = "invalid identifier";
    return false;
  }
  if (text[0] == '/') {
    return validateSlashPath(text, error);
  }
  if (text.find('/') != std::string::npos) {
    error = "invalid slash path identifier: " + text;
    return false;
  }
  if (!isIdentifierSegmentStart(text[0])) {
    error = "invalid identifier: " + text;
    return false;
  }
  for (size_t i = 1; i < text.size(); ++i) {
    if (!isIdentifierSegmentChar(text[i])) {
      error = "invalid identifier: " + text;
      return false;
    }
  }
  if (isReservedKeyword(text)) {
    error = "reserved keyword cannot be used as identifier: " + text;
    return false;
  }
  return true;
}

bool validateTransformName(const std::string &text, std::string &error) {
  if (text == "return" || text == "mut") {
    return true;
  }
  if (text.empty()) {
    error = "invalid transform identifier";
    return false;
  }
  if (text[0] == '/' || text.find('/') != std::string::npos) {
    error = "transform identifiers cannot be slash paths";
    return false;
  }
  return validateIdentifierText(text, error);
}

bool isStructTransformName(const std::string &text) {
  return text == "struct" || text == "pod" || text == "stack" || text == "heap" || text == "buffer" ||
         text == "handle" || text == "gpu_lane";
}

bool isBuiltinName(const std::string &name) {
  return name == "assign" || name == "plus" || name == "minus" || name == "multiply" || name == "divide" ||
         name == "negate" || name == "greater_than" || name == "less_than" || name == "greater_equal" ||
         name == "less_equal" || name == "equal" || name == "not_equal" || name == "and" || name == "or" ||
         name == "not" || name == "clamp" || name == "min" || name == "max" || name == "if" || name == "then" ||
         name == "else" || name == "repeat" || name == "return" || name == "array" || name == "map" ||
         name == "count" || name == "at" || name == "at_unsafe" || name == "convert" || name == "location" ||
         name == "dereference" || name == "print" || name == "print_line" || name == "print_error" ||
         name == "print_line_error" || name == "notify" || name == "insert" || name == "take";
}

bool isHexDigitChar(char c) {
  return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

bool isValidFloatLiteral(const std::string &text) {
  if (text.empty()) {
    return false;
  }
  size_t i = 0;
  if (text[i] == '-') {
    ++i;
  }
  if (i >= text.size()) {
    return false;
  }
  bool sawDigit = false;
  bool sawDot = false;
  while (i < text.size()) {
    char c = text[i];
    if (std::isdigit(static_cast<unsigned char>(c))) {
      sawDigit = true;
      ++i;
      continue;
    }
    if (c == '.' && !sawDot) {
      sawDot = true;
      ++i;
      continue;
    }
    break;
  }
  if (!sawDigit) {
    return false;
  }
  if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
    ++i;
    if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
      ++i;
    }
    size_t expStart = i;
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
      ++i;
    }
    if (i == expStart) {
      return false;
    }
  }
  return i == text.size();
}
} // namespace

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

bool Parser::parse(Program &program, std::string &error) {
  error_ = &error;
  while (!match(TokenKind::End)) {
    if (match(TokenKind::Comment)) {
      return fail("comments are not supported");
    }
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
    if (match(TokenKind::Comment)) {
      return fail("comments are not supported");
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
  execs.push_back(std::move(exec));
  return true;
}

bool Parser::parseTransformList(std::vector<Transform> &out) {
  if (!expect(TokenKind::LBracket, "expected '['")) {
    return false;
  }
  while (!match(TokenKind::RBracket)) {
    if (match(TokenKind::Comment)) {
      return fail("comments are not supported");
    }
    Token name = consume(TokenKind::Identifier, "expected transform identifier");
    if (name.kind == TokenKind::End) {
      return false;
    }
    std::string nameError;
    if (!validateTransformName(name.text, nameError)) {
      return fail(nameError);
    }
    Transform transform;
    transform.name = name.text;
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
        if (match(TokenKind::Comment)) {
          return fail("comments are not supported");
        }
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
          transform.arguments.push_back(arg.text);
        } else {
          return fail("expected transform argument");
        }
        if (match(TokenKind::Comma)) {
          expect(TokenKind::Comma, "expected ','");
          continue;
        }
        break;
      }
      if (!expect(TokenKind::RParen, "expected ')'")) {
        return false;
      }
    }
    out.push_back(std::move(transform));
    if (match(TokenKind::Comma)) {
      expect(TokenKind::Comma, "expected ','");
      if (match(TokenKind::RBracket)) {
        return fail("trailing comma not allowed in transform list");
      }
    }
  }
  expect(TokenKind::RBracket, "expected ']'" );
  return true;
}

bool Parser::parseTemplateList(std::vector<std::string> &out) {
  if (!expect(TokenKind::LAngle, "expected '<'")) {
    return false;
  }
  if (match(TokenKind::RAngle)) {
    return fail("expected template identifier");
  }
  while (!match(TokenKind::RAngle)) {
    std::string typeName;
    if (!parseTypeName(typeName)) {
      return false;
    }
    out.push_back(typeName);
    if (match(TokenKind::Comma)) {
      expect(TokenKind::Comma, "expected ','");
      if (match(TokenKind::RAngle)) {
        return fail("trailing comma not allowed in template argument list");
      }
      continue;
    }
    if (match(TokenKind::RAngle)) {
      break;
    }
    if (match(TokenKind::Identifier)) {
      continue;
    }
    return fail("expected '>'");
  }
  if (!expect(TokenKind::RAngle, "expected '>'")) {
    return false;
  }
  return true;
}

bool Parser::parseTypeName(std::string &out) {
  Token name = consume(TokenKind::Identifier, "expected template identifier");
  if (name.kind == TokenKind::End) {
    return false;
  }
  std::string nameError;
  if (!validateIdentifierText(name.text, nameError)) {
    return fail(nameError);
  }
  out = name.text;
  if (match(TokenKind::LAngle)) {
    expect(TokenKind::LAngle, "expected '<'");
    out += "<";
    if (match(TokenKind::RAngle)) {
      return fail("expected template identifier");
    }
    bool first = true;
    while (!match(TokenKind::RAngle)) {
      std::string nested;
      if (!parseTypeName(nested)) {
        return false;
      }
      if (!first) {
        out += ", ";
      }
      out += nested;
      first = false;
      if (match(TokenKind::Comma)) {
        expect(TokenKind::Comma, "expected ','");
        if (match(TokenKind::RAngle)) {
          return fail("trailing comma not allowed in template argument list");
        }
        continue;
      }
      if (match(TokenKind::RAngle)) {
        break;
      }
      if (match(TokenKind::Identifier)) {
        continue;
      }
      return fail("expected '>'");
    }
    if (!expect(TokenKind::RAngle, "expected '>'")) {
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
    expect(TokenKind::LParen, "expected '(' after parameter identifier");
    if (!parseCallArgumentList(param.args, param.argNames, namespacePrefix)) {
      return false;
    }
    for (const auto &argName : param.argNames) {
      if (argName.has_value()) {
        return fail("parameter defaults do not accept named arguments");
      }
    }
    if (!expect(TokenKind::RParen, "expected ')' after parameter default")) {
      return false;
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
    if (match(TokenKind::Comment)) {
      return fail("comments are not supported");
    }
    Expr param;
    if (!parseParameterBinding(param, namespacePrefix)) {
      return false;
    }
    out.push_back(std::move(param));
    if (match(TokenKind::Comma)) {
      expect(TokenKind::Comma, "expected ','");
      if (match(TokenKind::RParen)) {
        return fail("trailing comma not allowed in parameter list");
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
  while (true) {
    if (match(TokenKind::Comment)) {
      return fail("comments are not supported");
    }
    std::optional<std::string> argName;
    if (match(TokenKind::Identifier) && pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].kind == TokenKind::Equal) {
      Token name = consume(TokenKind::Identifier, "expected argument name");
      if (name.kind == TokenKind::End) {
        return false;
      }
      std::string nameError;
      if (!validateIdentifierText(name.text, nameError)) {
        return fail(nameError);
      }
      if (!expect(TokenKind::Equal, "expected '=' after argument name")) {
        return false;
      }
      argName = name.text;
    }
    Expr arg;
    if (!parseExpr(arg, namespacePrefix)) {
      return false;
    }
    out.push_back(std::move(arg));
    argNames.push_back(std::move(argName));
    if (match(TokenKind::Comma)) {
      expect(TokenKind::Comma, "expected ','");
      if (match(TokenKind::RParen)) {
        return fail("trailing comma not allowed in argument list");
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

bool Parser::validateNoBuiltinNamedArguments(const std::string &name,
                                             const std::vector<std::optional<std::string>> &argNames) {
  if (!isBuiltinName(name)) {
    return true;
  }
  for (const auto &argName : argNames) {
    if (argName.has_value()) {
      return fail("named arguments not supported for builtin calls");
    }
  }
  return true;
}

bool Parser::validateNamedArgumentOrdering(const std::vector<std::optional<std::string>> &argNames) {
  bool sawNamed = false;
  for (const auto &argName : argNames) {
    if (argName.has_value()) {
      sawNamed = true;
      continue;
    }
    if (sawNamed) {
      return fail("positional argument cannot follow named arguments");
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
    if (match(TokenKind::Comment)) {
      return fail("comments are not supported");
    }
    if (match(TokenKind::Semicolon)) {
      return fail("semicolon is not allowed");
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
      if (!parseReturnStatement(arg, namespacePrefix)) {
        return false;
      }
    } else {
      if (!parseExpr(arg, namespacePrefix)) {
        return false;
      }
    }
    out.push_back(std::move(arg));
    if (match(TokenKind::Comma)) {
      expect(TokenKind::Comma, "expected ','");
      if (match(TokenKind::RBrace)) {
        return fail("trailing comma not allowed in brace list");
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

bool Parser::tryParseIfStatementSugar(Expr &out, const std::string &namespacePrefix, bool &parsed) {
  parsed = false;
  if (!match(TokenKind::Identifier) || tokens_[pos_].text != "if") {
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
  expect(TokenKind::LParen, "expected '(' after if");
  Expr condition;
  if (!parseExpr(condition, namespacePrefix)) {
    return false;
  }
  if (match(TokenKind::Comma)) {
    pos_ = savedPos;
    return true;
  }
  if (!match(TokenKind::RParen)) {
    pos_ = savedPos;
    return true;
  }
  expect(TokenKind::RParen, "expected ')' after if condition");
  if (!match(TokenKind::LBrace)) {
    pos_ = savedPos;
    return true;
  }
  std::vector<Expr> thenBody;
  if (!parseBraceExprList(thenBody, namespacePrefix)) {
    return false;
  }
  if (!match(TokenKind::Identifier) || tokens_[pos_].text != "else") {
    return fail("if statement requires else block");
  }
  consume(TokenKind::Identifier, "expected 'else'");
  std::vector<Expr> elseBody;
  if (!parseBraceExprList(elseBody, namespacePrefix)) {
    return false;
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
  ifCall.name = "if";
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
  if (!parseExpr(arg, namespacePrefix)) {
    return false;
  }
  returnCall.args.push_back(std::move(arg));
  returnCall.argNames.push_back(std::nullopt);
  if (match(TokenKind::Comma)) {
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

bool Parser::definitionHasReturnBeforeClose() const {
  size_t index = pos_;
  int depth = 1;
  while (index < tokens_.size()) {
    TokenKind kind = tokens_[index].kind;
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
    if (match(TokenKind::Comment)) {
      return fail("comments are not supported");
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
      if (!expect(TokenKind::LParen, "expected '(' after identifier")) {
        return false;
      }
      if (!parseCallArgumentList(callExpr.args, callExpr.argNames, def.namespacePrefix)) {
        return false;
      }
      if (!validateNoBuiltinNamedArguments(callExpr.name, callExpr.argNames)) {
        return false;
      }
      if (!validateNamedArgumentOrdering(callExpr.argNames)) {
        return false;
      }
      if (!expect(TokenKind::RParen, "expected ')' after call statement")) {
        return false;
      }
      if (match(TokenKind::LBrace)) {
        callExpr.hasBodyArguments = true;
        if (!parseBraceExprList(callExpr.bodyArguments, def.namespacePrefix)) {
          return false;
        }
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

bool Parser::parseExpr(Expr &expr, const std::string &namespacePrefix) {
  auto parsePrimary = [&](Expr &out) -> bool {
    if (match(TokenKind::Comment)) {
      return fail("comments are not supported");
    }
    if (match(TokenKind::Identifier) && tokens_[pos_].text == "if") {
      bool parsed = false;
      if (!tryParseIfStatementSugar(out, namespacePrefix, parsed)) {
        return false;
      }
      if (parsed) {
        return true;
      }
    }
    if (match(TokenKind::LBracket)) {
      std::vector<Transform> transforms;
      if (!parseTransformList(transforms)) {
        return false;
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
      if (!match(TokenKind::LParen)) {
        return fail("binding requires argument list");
      }
      expect(TokenKind::LParen, "expected '(' after identifier");
      Expr call;
      call.kind = Expr::Kind::Call;
      call.name = name.text;
      call.namespacePrefix = namespacePrefix;
      call.templateArgs = std::move(templateArgs);
      call.transforms = std::move(transforms);
      call.isBinding = true;
      if (!parseCallArgumentList(call.args, call.argNames, namespacePrefix)) {
        return false;
      }
      if (!validateNoBuiltinNamedArguments(call.name, call.argNames)) {
        return false;
      }
      if (!expect(TokenKind::RParen, "expected ')' to close call")) {
        return false;
      }
      out = std::move(call);
      return true;
    }
    if (match(TokenKind::Number)) {
      Token number = consume(TokenKind::Number, "expected number");
      if (number.kind == TokenKind::End) {
        return false;
      }
      out.namespacePrefix = namespacePrefix;
      std::string text = number.text;
      int intWidth = 0;
      bool isUnsigned = false;
      size_t suffixLen = 0;
      if (text.size() >= 3 && text.compare(text.size() - 3, 3, "i32") == 0) {
        intWidth = 32;
        isUnsigned = false;
        suffixLen = 3;
      } else if (text.size() >= 3 && text.compare(text.size() - 3, 3, "i64") == 0) {
        intWidth = 64;
        isUnsigned = false;
        suffixLen = 3;
      } else if (text.size() >= 3 && text.compare(text.size() - 3, 3, "u64") == 0) {
        intWidth = 64;
        isUnsigned = true;
        suffixLen = 3;
      }
      if (suffixLen == 0) {
        int floatWidth = 32;
        bool isFloat = false;
        if (text.size() >= 3 && text.compare(text.size() - 3, 3, "f64") == 0) {
          floatWidth = 64;
          isFloat = true;
          text = text.substr(0, text.size() - 3);
        } else if (text.size() >= 3 && text.compare(text.size() - 3, 3, "f32") == 0) {
          floatWidth = 32;
          isFloat = true;
          text = text.substr(0, text.size() - 3);
        } else if (!text.empty() && text.back() == 'f') {
          floatWidth = 32;
          isFloat = true;
          text.pop_back();
        } else if (text.find('.') != std::string::npos || text.find('e') != std::string::npos ||
                   text.find('E') != std::string::npos) {
          floatWidth = 32;
          isFloat = true;
        }
        if (!isFloat) {
          return fail("integer literal requires i32/i64/u64 suffix");
        }
        if (!isValidFloatLiteral(text)) {
          return fail("invalid float literal");
        }
        out.kind = Expr::Kind::FloatLiteral;
        out.floatValue = text;
        out.floatWidth = floatWidth;
        return true;
      }
      out.kind = Expr::Kind::Literal;
      text = text.substr(0, text.size() - suffixLen);
      if (text.empty()) {
        return fail("invalid integer literal");
      }
      bool negative = false;
      size_t start = 0;
      if (text[0] == '-') {
        if (isUnsigned) {
          return fail("invalid integer literal");
        }
        negative = true;
        start = 1;
        if (start >= text.size()) {
          return fail("invalid integer literal");
        }
      }
      int base = 10;
      std::string digits = text.substr(start);
      if (digits.size() >= 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
        base = 16;
        digits = digits.substr(2);
        if (digits.empty()) {
          return fail("invalid integer literal");
        }
        for (char c : digits) {
          if (!isHexDigitChar(c)) {
            return fail("invalid integer literal");
          }
        }
      } else {
        for (char c : digits) {
          if (!std::isdigit(static_cast<unsigned char>(c))) {
            return fail("invalid integer literal");
          }
        }
      }
      out.intWidth = intWidth;
      out.isUnsigned = isUnsigned;
      try {
        if (isUnsigned) {
          unsigned long long value = std::stoull(digits, nullptr, base);
          out.literalValue = static_cast<uint64_t>(value);
        } else {
          long long value = 0;
          if (negative) {
            unsigned long long magnitude = std::stoull(digits, nullptr, base);
            const unsigned long long maxMagnitude = (1ULL << 63);
            if (magnitude > maxMagnitude) {
              return fail("integer literal out of range");
            }
            if (magnitude == maxMagnitude) {
              value = std::numeric_limits<long long>::min();
            } else {
              value = -static_cast<long long>(magnitude);
            }
          } else {
            value = std::stoll(digits, nullptr, base);
          }
          if (intWidth == 32) {
            if (value < std::numeric_limits<int32_t>::min() || value > std::numeric_limits<int32_t>::max()) {
              return fail("integer literal out of range");
            }
          }
          out.literalValue = static_cast<uint64_t>(value);
        }
      } catch (const std::exception &) {
        return fail("invalid integer literal");
      }
      return true;
    }
    if (match(TokenKind::String)) {
      Token text = consume(TokenKind::String, "expected string literal");
      if (text.kind == TokenKind::End) {
        return false;
      }
      std::string literalText;
      std::string suffix;
      if (!splitStringLiteralToken(text.text, literalText, suffix)) {
        return fail("invalid string literal");
      }
      if (suffix.empty()) {
        return fail("string literal requires utf8/ascii/raw_utf8/raw_ascii suffix");
      }
      out.kind = Expr::Kind::StringLiteral;
      out.namespacePrefix = namespacePrefix;
      out.stringValue = text.text;
      return true;
    }
    if (match(TokenKind::Identifier)) {
      Token name = consume(TokenKind::Identifier, "expected identifier");
      if (name.kind == TokenKind::End) {
        return false;
      }
      if (name.text == "true" || name.text == "false") {
        out.kind = Expr::Kind::BoolLiteral;
        out.namespacePrefix = namespacePrefix;
        out.boolValue = name.text == "true";
        return true;
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
      Expr call;
      call.kind = Expr::Kind::Call;
      call.name = name.text;
      call.namespacePrefix = namespacePrefix;
      call.templateArgs = std::move(templateArgs);
      bool hasCallSyntax = false;
      if (match(TokenKind::LParen)) {
        expect(TokenKind::LParen, "expected '(' after identifier");
        hasCallSyntax = true;
        if (!parseCallArgumentList(call.args, call.argNames, namespacePrefix)) {
          return false;
        }
        if (!validateNoBuiltinNamedArguments(call.name, call.argNames)) {
          return false;
        }
        if (!validateNamedArgumentOrdering(call.argNames)) {
          return false;
        }
        if (!expect(TokenKind::RParen, "expected ')' to close call")) {
          return false;
        }
      }
      if (match(TokenKind::LBrace)) {
        hasCallSyntax = true;
        call.hasBodyArguments = true;
        if (!parseBraceExprList(call.bodyArguments, namespacePrefix)) {
          return false;
        }
      }
      if (hasCallSyntax) {
        out = std::move(call);
        return true;
      }
      if (!call.templateArgs.empty()) {
        return fail("template arguments require a call");
      }
      out.kind = Expr::Kind::Name;
      out.name = name.text;
      out.namespacePrefix = namespacePrefix;
      return true;
    }
    return fail("unexpected token in expression");
  };

  Expr current;
  if (!parsePrimary(current)) {
    return false;
  }

  if (current.isBinding) {
    expr = std::move(current);
    return true;
  }

  while (true) {
    if (match(TokenKind::Dot)) {
      expect(TokenKind::Dot, "expected '.'");
      Token member = consume(TokenKind::Identifier, "expected member identifier");
      if (member.kind == TokenKind::End) {
        return false;
      }
      std::string nameError;
      if (!validateIdentifierText(member.text, nameError)) {
        return fail(nameError);
      }
      if (!expect(TokenKind::LParen, "expected '(' after member name")) {
        return false;
      }
      Expr call;
      call.kind = Expr::Kind::Call;
      call.name = member.text;
      call.namespacePrefix = namespacePrefix;
      call.isMethodCall = true;
      if (!parseCallArgumentList(call.args, call.argNames, namespacePrefix)) {
        return false;
      }
      if (!validateNoBuiltinNamedArguments(call.name, call.argNames)) {
        return false;
      }
      if (!validateNamedArgumentOrdering(call.argNames)) {
        return false;
      }
      if (!expect(TokenKind::RParen, "expected ')' to close call")) {
        return false;
      }
      call.args.insert(call.args.begin(), current);
      call.argNames.insert(call.argNames.begin(), std::nullopt);
      if (match(TokenKind::LBrace)) {
        call.hasBodyArguments = true;
        if (!parseBraceExprList(call.bodyArguments, namespacePrefix)) {
          return false;
        }
      }
      current = std::move(call);
      continue;
    }
    if (match(TokenKind::LBracket)) {
      expect(TokenKind::LBracket, "expected '[' after expression");
      Expr indexExpr;
      if (!parseExpr(indexExpr, namespacePrefix)) {
        return false;
      }
      if (!expect(TokenKind::RBracket, "expected ']' after index expression")) {
        return false;
      }
      Expr call;
      call.kind = Expr::Kind::Call;
      call.name = "at";
      call.namespacePrefix = namespacePrefix;
      call.args.push_back(current);
      call.args.push_back(std::move(indexExpr));
      call.argNames.resize(call.args.size());
      current = std::move(call);
      continue;
    }
    break;
  }

  expr = std::move(current);
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

bool Parser::match(TokenKind kind) const {
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
