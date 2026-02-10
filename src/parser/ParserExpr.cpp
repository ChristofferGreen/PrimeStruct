#include "primec/Parser.h"
#include "primec/StringLiteral.h"

#include "ParserHelpers.h"

#include <cctype>
#include <exception>
#include <limits>
#include <utility>

namespace primec {
using namespace parser;

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

bool Parser::parseExpr(Expr &expr, const std::string &namespacePrefix) {
  auto parsePrimary = [&](Expr &out) -> bool {
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
      Expr call;
      call.kind = Expr::Kind::Call;
      call.name = name.text;
      call.namespacePrefix = namespacePrefix;
      call.templateArgs = std::move(templateArgs);
      call.transforms = std::move(transforms);
      call.isBinding = true;
      if (match(TokenKind::LParen)) {
        return fail("binding initializers must use braces");
      }
      if (match(TokenKind::LBrace)) {
        if (!parseBindingInitializerList(call.args, namespacePrefix)) {
          return false;
        }
      } else {
        return fail("binding requires initializer");
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
      std::string normalized;
      std::string parseError;
      if (!normalizeStringLiteralToken(text.text, normalized, parseError)) {
        return fail(parseError);
      }
      out.kind = Expr::Kind::StringLiteral;
      out.namespacePrefix = namespacePrefix;
      out.stringValue = std::move(normalized);
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
      bool sawParen = false;
      if (match(TokenKind::LParen)) {
        expect(TokenKind::LParen, "expected '(' after identifier");
        hasCallSyntax = true;
        sawParen = true;
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
        if (!sawParen) {
          return fail("call body requires parameter list");
        }
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
      std::vector<std::string> templateArgs;
      if (match(TokenKind::LAngle)) {
        if (!parseTemplateList(templateArgs)) {
          return false;
        }
      }
      if (!expect(TokenKind::LParen, "expected '(' after member name")) {
        return false;
      }
      Expr call;
      call.kind = Expr::Kind::Call;
      call.name = member.text;
      call.namespacePrefix = namespacePrefix;
      call.isMethodCall = true;
      call.templateArgs = std::move(templateArgs);
      if (!parseCallArgumentList(call.args, call.argNames, namespacePrefix)) {
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

} // namespace primec
