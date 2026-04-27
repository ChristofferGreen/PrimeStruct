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
  return kind == TokenKind::Comment;
}

bool isControlKeyword(const std::string &name) {
  return name == "if" || name == "else" || name == "loop" || name == "while" || name == "for" || name == "match";
}

bool isLoopFormKeyword(const std::string &name) {
  return name == "loop" || name == "while" || name == "for";
}

bool isSurfaceControlFlowBody(const std::string &name) {
  return name == "if" || name == "match" || isLoopFormKeyword(name);
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

bool Parser::parseExpr(Expr &expr, const std::string &namespacePrefix) {
  SeparatorWhitespaceGuard separatorGuard(*this, false);
  skipComments();
  const Token &startToken = tokens_[pos_];
  const int startLine = startToken.line;
  const int startColumn = startToken.column;

  auto assignSourceIfMissing = [&](Expr &target) {
    if (target.sourceLine == 0 && startLine > 0) {
      target.sourceLine = startLine;
    }
    if (target.sourceColumn == 0 && startColumn > 0) {
      target.sourceColumn = startColumn;
    }
  };

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
  auto isPrimitiveBraceType = [&](const std::string &name) -> bool {
    if (name.empty() || name.find('/') != std::string::npos) {
      return false;
    }
    return name == "int" || name == "i32" || name == "i64" || name == "u64" || name == "bool" ||
           name == "float" || name == "f32" || name == "f64";
  };
  auto parseBraceConstructor = [&](Expr &call, Expr &out) -> bool {
    call.isBraceConstructor = true;
    if (call.templateArgs.empty() && isPrimitiveBraceType(call.name)) {
      call.templateArgs.push_back(call.name);
      call.name = "convert";
    }
    if (tryParseBraceConstructorArgumentList(call.args, call.argNames, namespacePrefix)) {
      out = std::move(call);
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
    call.args.push_back(std::move(block));
    call.argNames.push_back(std::nullopt);
    out = std::move(call);
    return true;
  };
  auto parsePrimary = [&](Expr &out) -> bool {
    if (match(TokenKind::LParen)) {
      expect(TokenKind::LParen, "expected '('");
      Expr inner;
      {
        BareBindingGuard bindingGuard(*this, false);
        if (!parseExpr(inner, namespacePrefix)) {
          return false;
        }
      }
      if (!expect(TokenKind::RParen, "expected ')' to close expression")) {
        return false;
      }
      out = std::move(inner);
      return true;
    }
    if (allowSurfaceSyntax_ && match(TokenKind::Identifier) &&
        (tokens_[pos_].text == "if" || tokens_[pos_].text == "match")) {
      bool parsed = false;
      if (!tryParseIfStatementSugar(out, namespacePrefix, parsed, false)) {
        return false;
      }
      if (parsed) {
        return true;
      }
    }
    if (match(TokenKind::LBracket)) {
      bool parsedLambda = false;
      if (!tryParseLambdaExpr(out, namespacePrefix, parsedLambda)) {
        return false;
      }
      if (parsedLambda) {
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
      if (allowSurfaceSyntax_ && isLoopFormKeyword(name.text)) {
        bool parsedLoop = false;
        if (!tryParseLoopFormAfterName(out, namespacePrefix, name.text, transforms, parsedLoop)) {
          return false;
        }
        if (parsedLoop) {
          return true;
        }
      }
      std::string nameError;
      if (!validateIdentifierText(name.text, nameError)) {
        if (!isControlKeyword(name.text) || (!match(TokenKind::LParen) && !match(TokenKind::LAngle))) {
          return fail(nameError);
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
          if (call.name == "block") {
            if (!parseValueBlock(call.bodyArguments, namespacePrefix)) {
              return false;
            }
          } else {
            if (!parseBraceExprList(call.bodyArguments, namespacePrefix, allowSingleBranchIfStatementBlocks_)) {
              return false;
            }
          }
        }
        out = std::move(call);
        return true;
      }
      if (match(TokenKind::LBrace)) {
        if (!bindingTransforms && (!allowBareBindings_ || isPrimitiveBraceType(call.name))) {
          return parseBraceConstructor(call, out);
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
        call.isBinding = true;
        out = std::move(call);
        return true;
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
        bool hasFloatWidthSuffix = false;
        if (text.size() >= 3 && text.compare(text.size() - 3, 3, "f64") == 0) {
          floatWidth = 64;
          isFloat = true;
          hasFloatWidthSuffix = true;
          text = text.substr(0, text.size() - 3);
        } else if (text.size() >= 3 && text.compare(text.size() - 3, 3, "f32") == 0) {
          floatWidth = 32;
          isFloat = true;
          hasFloatWidthSuffix = true;
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
        if (!allowSurfaceSyntax_ && !hasFloatWidthSuffix) {
          return fail("float literal requires f32/f64 suffix in canonical mode");
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
      if (!allowSurfaceSyntax_) {
        std::string literalText;
        std::string suffix;
        if (!splitStringLiteralToken(text.text, literalText, suffix)) {
          return fail("invalid string literal");
        }
        if (literalText.empty() || literalText.front() != '"') {
          return fail("canonical string literal requires double quotes");
        }
        if (suffix != "utf8" && suffix != "ascii") {
          return fail("canonical string literal requires utf8/ascii suffix");
        }
      }
      std::string normalized;
      std::string parseError;
      if (!normalizeStringLiteralToken(text.text, normalized, parseError)) {
        return fail(parseError);
      }
      if (!allowSurfaceSyntax_ && normalized != text.text) {
        return fail("canonical string literal must use normalized escapes");
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
      std::string nameError;
      if (!validateIdentifierText(name.text, nameError)) {
        if (!isControlKeyword(name.text) || (!match(TokenKind::LParen) && !match(TokenKind::LAngle))) {
          return fail(nameError);
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
          if (!allowBareBindings_ || isPrimitiveBraceType(call.name)) {
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
          if (call.name == "block") {
            if (!parseValueBlock(call.bodyArguments, namespacePrefix)) {
              return false;
            }
          } else {
            if (!parseBraceExprList(call.bodyArguments, namespacePrefix, allowSingleBranchIfStatementBlocks_)) {
              return false;
            }
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
  assignSourceIfMissing(current);

  if (current.isBinding) {
    assignSourceIfMissing(current);
    expr = std::move(current);
    return true;
  }

  auto looksLikeBindingAfterExpr = [&]() -> bool {
    if (!allowBareBindings_) {
      return false;
    }
    if (!match(TokenKind::LBracket)) {
      return false;
    }
    size_t scan = pos_;
    int depth = 0;
    while (scan < tokens_.size()) {
      TokenKind kind = tokens_[scan].kind;
      if (kind == TokenKind::LBracket) {
        ++depth;
      } else if (kind == TokenKind::RBracket) {
        --depth;
        if (depth == 0) {
          ++scan;
          break;
        }
      }
      ++scan;
    }
    if (depth != 0) {
      return false;
    }
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
    return scan < tokens_.size() && tokens_[scan].kind == TokenKind::LBrace;
  };

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
      if (match(TokenKind::LParen)) {
        expect(TokenKind::LParen, "expected '(' after member name");
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
          if (!parseBraceExprList(call.bodyArguments, namespacePrefix, allowSingleBranchIfStatementBlocks_)) {
            return false;
          }
        }
        current = std::move(call);
        continue;
      }
      if (!templateArgs.empty()) {
        return fail("template arguments require a call");
      }
      Expr access;
      access.kind = Expr::Kind::Call;
      access.name = member.text;
      access.namespacePrefix = namespacePrefix;
      access.isMethodCall = true;
      access.isFieldAccess = true;
      access.args.push_back(current);
      access.argNames.push_back(std::nullopt);
      current = std::move(access);
      continue;
    }
    if (match(TokenKind::LBracket)) {
      if (looksLikeBindingAfterExpr()) {
        break;
      }
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
    if (match(TokenKind::Question)) {
      expect(TokenKind::Question, "expected '?'");
      Expr call;
      call.kind = Expr::Kind::Call;
      call.name = "try";
      call.namespacePrefix = namespacePrefix;
      call.args.push_back(current);
      call.argNames.push_back(std::nullopt);
      current = std::move(call);
      continue;
    }
    break;
  }

  assignSourceIfMissing(current);
  expr = std::move(current);
  return true;
}

} // namespace primec
