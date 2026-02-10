#include "primec/Parser.h"

#include "ParserHelpers.h"

#include "primec/StringLiteral.h"

#include <utility>

namespace primec {
using namespace parser;

bool Parser::parseTransformList(std::vector<Transform> &out) {
  if (!expect(TokenKind::LBracket, "expected '['")) {
    return false;
  }
  while (!match(TokenKind::RBracket)) {
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
    return fail("parameter defaults must use braces");
  }
  if (match(TokenKind::LBrace)) {
    if (!parseBindingInitializerList(param.args, param.argNames, namespacePrefix)) {
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
  ArgumentLabelGuard labelGuard(*this);
  while (true) {
    std::optional<std::string> argName;
    if (match(TokenKind::LBracket) && pos_ + 2 < tokens_.size() && tokens_[pos_ + 1].kind == TokenKind::Identifier &&
        tokens_[pos_ + 2].kind == TokenKind::RBracket) {
      expect(TokenKind::LBracket, "expected '['");
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
      if (!expect(TokenKind::RBracket, "expected ']' after argument label")) {
        return false;
      }
      argName = name.text;
    } else if (match(TokenKind::Identifier) && pos_ + 1 < tokens_.size() &&
               tokens_[pos_ + 1].kind == TokenKind::Equal) {
      Token name = consume(TokenKind::Identifier, "expected argument name");
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

bool Parser::parseBindingInitializerList(std::vector<Expr> &out,
                                         std::vector<std::optional<std::string>> &argNames,
                                         const std::string &namespacePrefix) {
  if (!expect(TokenKind::LBrace, "expected '{' after identifier")) {
    return false;
  }
  if (match(TokenKind::RBrace)) {
    expect(TokenKind::RBrace, "expected '}'");
    return true;
  }
  ArgumentLabelGuard labelGuard(*this);
  while (true) {
    if (match(TokenKind::Semicolon)) {
      return fail("semicolon is not allowed");
    }
    std::optional<std::string> argName;
    if (match(TokenKind::LBracket) && pos_ + 2 < tokens_.size() && tokens_[pos_ + 1].kind == TokenKind::Identifier &&
        tokens_[pos_ + 2].kind == TokenKind::RBracket) {
      expect(TokenKind::LBracket, "expected '['");
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
      if (!expect(TokenKind::RBracket, "expected ']' after argument label")) {
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
      if (match(TokenKind::RBrace)) {
        return fail("trailing comma not allowed in binding initializer");
      }
      continue;
    }
    if (match(TokenKind::RBrace)) {
      break;
    }
  }
  if (!expect(TokenKind::RBrace, "expected '}' after binding initializer")) {
    return false;
  }
  return true;
}

bool Parser::validateNoBuiltinNamedArguments(const std::string &name,
                                             const std::vector<std::optional<std::string>> &argNames) {
  std::string normalized = name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  if (!isBuiltinName(normalized)) {
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

} // namespace primec
