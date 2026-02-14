#include "primec/Parser.h"
#include "primec/StringLiteral.h"

#include "ParserHelpers.h"

#include <cctype>
#include <exception>
#include <limits>
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

bool isControlKeyword(const std::string &name) {
  return name == "if" || name == "else" || name == "loop" || name == "while" || name == "for";
}

bool isLoopFormKeyword(const std::string &name) {
  return name == "loop" || name == "while" || name == "for";
}

bool isSurfaceControlFlowBody(const std::string &name) {
  return name == "if" || isLoopFormKeyword(name);
}

std::string stripNumericSeparators(const std::string &text) {
  if (text.find(',') == std::string::npos) {
    return text;
  }
  std::string cleaned;
  cleaned.reserve(text.size());
  for (char c : text) {
    if (c != ',') {
      cleaned.push_back(c);
    }
  }
  return cleaned;
}
} // namespace

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
  expect(TokenKind::RParen, "expected ')' after if condition");
  if (!match(TokenKind::LBrace)) {
    pos_ = savedPos;
    return true;
  }
  std::vector<Expr> thenBody;
  {
    BraceListGuard braceGuard(*this, true, true);
    if (!parseBraceExprList(thenBody, namespacePrefix)) {
      return false;
    }
  }
  if (!match(TokenKind::Identifier) || tokens_[pos_].text != "else") {
    return fail("if statement requires else block");
  }
  consume(TokenKind::Identifier, "expected 'else'");
  std::vector<Expr> elseBody;
  {
    BraceListGuard braceGuard(*this, true, true);
    if (!parseBraceExprList(elseBody, namespacePrefix)) {
      return false;
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
  if (keyword == "for") {
    BareBindingGuard bindingGuard(*this, true);
    for (int i = 0; i < 3; ++i) {
      Expr slot;
      if (!parseExpr(slot, namespacePrefix)) {
        return false;
      }
      slots.push_back(std::move(slot));
    }
  } else {
    Expr slot;
    {
      BareBindingGuard bindingGuard(*this, false);
      if (!parseExpr(slot, namespacePrefix)) {
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
    if (!parseBraceExprList(body, namespacePrefix)) {
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
  for (auto &slot : slots) {
    loopCall.args.push_back(std::move(slot));
    loopCall.argNames.push_back(std::nullopt);
  }
  loopCall.args.push_back(std::move(bodyCall));
  loopCall.argNames.push_back(std::nullopt);
  out = std::move(loopCall);
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

bool Parser::parseExpr(Expr &expr, const std::string &namespacePrefix) {
  auto looksLikeArgumentLabel = [&](size_t start, size_t &afterLabel) -> bool {
    if (start >= tokens_.size() || tokens_[start].kind != TokenKind::LBracket) {
      return false;
    }
    size_t scan = start + 1;
    while (scan < tokens_.size() && isIgnorableToken(tokens_[scan].kind)) {
      ++scan;
    }
    if (scan >= tokens_.size() || tokens_[scan].kind != TokenKind::Identifier) {
      return false;
    }
    ++scan;
    while (scan < tokens_.size() && isIgnorableToken(tokens_[scan].kind)) {
      ++scan;
    }
    if (scan >= tokens_.size() || tokens_[scan].kind != TokenKind::RBracket) {
      return false;
    }
    afterLabel = scan + 1;
    return true;
  };
  auto parseBraceConstructor = [&](Expr &call, Expr &out) -> bool {
    if (!allowSurfaceSyntax_) {
      return fail("brace constructors require canonical call form");
    }
    Expr block;
    block.kind = Expr::Kind::Call;
    block.name = "block";
    block.namespacePrefix = namespacePrefix;
    block.hasBodyArguments = true;
    {
      BraceListGuard braceGuard(*this, true, true);
      if (!parseBraceExprList(block.bodyArguments, namespacePrefix)) {
        return false;
      }
    }
    call.args.push_back(std::move(block));
    call.argNames.push_back(std::nullopt);
    out = std::move(call);
    return true;
  };
  auto parsePrimary = [&](Expr &out) -> bool {
    if (allowSurfaceSyntax_ && match(TokenKind::Identifier) && tokens_[pos_].text == "if") {
      bool parsed = false;
      if (!tryParseIfStatementSugar(out, namespacePrefix, parsed)) {
        return false;
      }
      if (parsed) {
        return true;
      }
    }
    if (match(TokenKind::KeywordImport)) {
      return fail("import statements must appear at the top level");
    }
    if (match(TokenKind::LBracket)) {
      std::vector<Transform> transforms;
      if (!parseTransformList(transforms)) {
        return false;
      }
      const bool bindingTransforms = isBindingTransformList(transforms);
      Token name = consume(TokenKind::Identifier, "expected identifier");
      if (name.kind == TokenKind::End) {
        return false;
      }
      std::string nameError;
      if (!validateIdentifierText(name.text, nameError)) {
        if (!isControlKeyword(name.text) || (!match(TokenKind::LParen) && !match(TokenKind::LAngle))) {
          return fail(nameError);
        }
      }
      if (allowSurfaceSyntax_ && isLoopFormKeyword(name.text)) {
        bool parsedLoop = false;
        if (!tryParseLoopFormAfterName(out, namespacePrefix, name.text, transforms, parsedLoop)) {
          return false;
        }
        if (parsedLoop) {
          return true;
        }
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
      if (match(TokenKind::LParen)) {
        expect(TokenKind::LParen, "expected '(' after identifier");
        if (!parseCallArgumentList(call.args, call.argNames, namespacePrefix)) {
          return false;
        }
        if (!validateNamedArgumentOrdering(call.argNames)) {
          return false;
        }
        if (!expect(TokenKind::RParen, "expected ')' to close call")) {
          return false;
        }
        if (match(TokenKind::LBrace)) {
          if (!allowSurfaceSyntax_ && isSurfaceControlFlowBody(call.name)) {
            return fail("control-flow body sugar requires canonical call form");
          }
          call.hasBodyArguments = true;
          if (!parseBraceExprList(call.bodyArguments, namespacePrefix)) {
            return false;
          }
        }
        out = std::move(call);
        return true;
      }
      if (match(TokenKind::LBrace)) {
        if (!bindingTransforms) {
          if (call.name == "block") {
            call.hasBodyArguments = true;
            if (!parseBraceExprList(call.bodyArguments, namespacePrefix)) {
              return false;
            }
            out = std::move(call);
            return true;
          }
          if (!allowBareBindings_) {
            return parseBraceConstructor(call, out);
          }
          return fail("call body requires parameter list");
        }
        if (!call.templateArgs.empty()) {
          return fail("template arguments require a call");
        }
        call.isBinding = true;
        if (!parseBindingInitializerList(call.args, call.argNames, namespacePrefix)) {
          return false;
        }
        if (!finalizeBindingInitializer(call)) {
          return false;
        }
        out = std::move(call);
        return true;
      }
      if (bindingTransforms) {
        return fail("binding requires initializer");
      }
      return fail("transform-prefixed execution requires arguments");
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
        text = stripNumericSeparators(text);
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
      text = stripNumericSeparators(text);
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
        if (!isControlKeyword(name.text) || (!match(TokenKind::LParen) && !match(TokenKind::LAngle))) {
          return fail(nameError);
        }
      }
      if (allowSurfaceSyntax_ && isLoopFormKeyword(name.text)) {
        bool parsedLoop = false;
        const std::vector<Transform> noTransforms;
        if (!tryParseLoopFormAfterName(out, namespacePrefix, name.text, noTransforms, parsedLoop)) {
          return false;
        }
        if (parsedLoop) {
          return true;
        }
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
        if (!validateNamedArgumentOrdering(call.argNames)) {
          return false;
        }
        if (!expect(TokenKind::RParen, "expected ')' to close call")) {
          return false;
        }
      }
      if (match(TokenKind::LBrace)) {
        if (!sawParen) {
          if (call.name == "block") {
            hasCallSyntax = true;
            call.hasBodyArguments = true;
            if (!parseBraceExprList(call.bodyArguments, namespacePrefix)) {
              return false;
            }
          } else if (!allowBareBindings_) {
            return parseBraceConstructor(call, out);
          } else if (allowBareBindings_) {
            if (!call.templateArgs.empty()) {
              return fail("template arguments require a call");
            }
            call.isBinding = true;
            if (!parseBindingInitializerList(call.args, call.argNames, namespacePrefix)) {
              return false;
            }
            if (!finalizeBindingInitializer(call)) {
              return false;
            }
            out = std::move(call);
            return true;
          } else {
            return fail("call body requires parameter list");
          }
        } else {
          if (!allowSurfaceSyntax_ && isSurfaceControlFlowBody(call.name)) {
            return fail("control-flow body sugar requires canonical call form");
          }
          hasCallSyntax = true;
          call.hasBodyArguments = true;
          if (!parseBraceExprList(call.bodyArguments, namespacePrefix)) {
            return false;
          }
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
      if (allowArgumentLabels_) {
        size_t afterLabel = 0;
        if (looksLikeArgumentLabel(pos_, afterLabel)) {
          size_t nextIndex = afterLabel;
          while (nextIndex < tokens_.size() && isIgnorableToken(tokens_[nextIndex].kind)) {
            ++nextIndex;
          }
          if (nextIndex < tokens_.size() && isArgumentLabelValueStart(tokens_[nextIndex].kind)) {
            break;
          }
        }
      }
      if (!allowSurfaceSyntax_) {
        return fail("indexing sugar requires canonical at(value, index)");
      }
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
