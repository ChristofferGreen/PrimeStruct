#include "primec/Parser.h"

#include "ParserHelpers.h"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace primec {
using namespace parser;

namespace {

std::string renderTransformAsType(const Transform &transform) {
  std::ostringstream out;
  out << transform.name;
  if (!transform.templateArgs.empty()) {
    out << "<";
    for (size_t i = 0; i < transform.templateArgs.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }
      out << transform.templateArgs[i];
    }
    out << ">";
  }
  return out.str();
}

std::string nextVariadicPackTemplateName(const std::vector<std::string> &existing) {
  std::string candidate = "__pack_T";
  int suffix = 0;
  auto exists = [&](const std::string &name) {
    return std::find(existing.begin(), existing.end(), name) != existing.end();
  };
  while (exists(candidate)) {
    ++suffix;
    candidate = "__pack_T" + std::to_string(suffix);
  }
  return candidate;
}

} // namespace

bool Parser::parseLambdaCaptureList(std::vector<std::string> &captures) {
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

  captures.clear();
  if (!expectRaw(TokenKind::LBracket, "expected '[' to start lambda capture list")) {
    return false;
  }
  skipSeparators();
  if (matchRaw(TokenKind::RBracket)) {
    return expectRaw(TokenKind::RBracket, "expected ']' to close lambda capture list");
  }
  while (true) {
    std::string entry;
    while (true) {
      if (matchRaw(TokenKind::Identifier)) {
        Token name = consumeRaw(TokenKind::Identifier, "expected lambda capture");
        if (name.kind == TokenKind::End) {
          return false;
        }
        if (!entry.empty()) {
          entry.push_back(' ');
        }
        entry += name.text;
        continue;
      }
      if (matchRaw(TokenKind::Equal)) {
        consumeRaw(TokenKind::Equal, "expected lambda capture");
        if (!entry.empty()) {
          entry.push_back(' ');
        }
        entry += "=";
        continue;
      }
      if (matchRaw(TokenKind::Ampersand)) {
        consumeRaw(TokenKind::Ampersand, "expected lambda capture");
        if (!entry.empty()) {
          entry.push_back(' ');
        }
        entry += "&";
        continue;
      }
      break;
    }
    if (entry.empty()) {
      return fail("expected lambda capture");
    }
    captures.push_back(std::move(entry));
    skipSeparators();
    if (matchRaw(TokenKind::RBracket)) {
      break;
    }
    if (matchRaw(TokenKind::Identifier) || matchRaw(TokenKind::Equal) || matchRaw(TokenKind::Ampersand)) {
      continue;
    }
    return fail("expected ',' or ']' after lambda capture");
  }
  return expectRaw(TokenKind::RBracket, "expected ']' to close lambda capture list");
}

bool Parser::parseTemplateList(std::vector<std::string> &out,
                               std::vector<TemplateArgument> *detailsOut) {
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
    std::string argText;
    TemplateArgument argDetail;
    if (!parseTemplateArgument(argText, &argDetail)) {
      return false;
    }
    if (matchRaw(TokenKind::Ellipsis)) {
      return fail("type-pack expansion is only valid in template parameter lists");
    }
    out.push_back(argText);
    if (detailsOut != nullptr) {
      detailsOut->push_back(std::move(argDetail));
    }
    skipSeparators();
    if (matchRaw(TokenKind::RAngle)) {
      break;
    }
    if (matchRaw(TokenKind::Identifier) || matchRaw(TokenKind::Number) ||
        matchRaw(TokenKind::String)) {
      continue;
    }
    return fail("expected '>'");
  }
  if (!expectRaw(TokenKind::RAngle, "expected '>'")) {
    return false;
  }
  return true;
}

bool Parser::parseTemplateArgument(std::string &out, TemplateArgument *detailOut) {
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
  auto parseIntegerText = [](const std::string &text,
                             std::string &normalizedOut,
                             uint64_t &valueOut,
                             std::string &errorOut) {
    if (text.empty()) {
      errorOut = "template integer arguments must be unsigned integer literals";
      return false;
    }
    if (text.front() == '-') {
      errorOut = "negative integer template arguments are not supported";
      return false;
    }
    if (text.find('.') != std::string::npos || text.find('e') != std::string::npos ||
        text.find('E') != std::string::npos || text.find('f') != std::string::npos ||
        text.find('F') != std::string::npos || text.find("i32") != std::string::npos ||
        text.find("i64") != std::string::npos || text.find("u64") != std::string::npos) {
      errorOut = "template integer arguments must be unsigned integer literals";
      return false;
    }
    std::string normalized;
    normalized.reserve(text.size());
    for (char c : text) {
      if (c != ',') {
        normalized.push_back(c);
      }
    }
    if (normalized.size() > 2 && normalized[0] == '0' &&
        (normalized[1] == 'x' || normalized[1] == 'X')) {
      uint64_t value = 0;
      for (size_t i = 2; i < normalized.size(); ++i) {
        const char c = normalized[i];
        uint64_t digit = 0;
        if (c >= '0' && c <= '9') {
          digit = static_cast<uint64_t>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
          digit = static_cast<uint64_t>(10 + c - 'a');
        } else if (c >= 'A' && c <= 'F') {
          digit = static_cast<uint64_t>(10 + c - 'A');
        } else {
          errorOut = "template integer arguments must be unsigned integer literals";
          return false;
        }
        if (value > (UINT64_MAX - digit) / 16) {
          errorOut = "template integer argument is too large";
          return false;
        }
        value = value * 16 + digit;
      }
      normalizedOut = normalized;
      valueOut = value;
      return true;
    }
    uint64_t value = 0;
    for (char c : normalized) {
      if (c < '0' || c > '9') {
        errorOut = "template integer arguments must be unsigned integer literals";
        return false;
      }
      const uint64_t digit = static_cast<uint64_t>(c - '0');
      if (value > (UINT64_MAX - digit) / 10) {
        errorOut = "template integer argument is too large";
        return false;
      }
      value = value * 10 + digit;
    }
    normalizedOut = normalized;
    valueOut = value;
    return true;
  };

  if (matchRaw(TokenKind::Number)) {
    Token number = consumeRaw(TokenKind::Number, "expected template argument");
    if (number.kind == TokenKind::End) {
      return false;
    }
    std::string normalized;
    uint64_t value = 0;
    std::string error;
    if (!parseIntegerText(number.text, normalized, value, error)) {
      return fail(error);
    }
    out = normalized;
    if (detailOut != nullptr) {
      *detailOut = TemplateArgument::integer(normalized, value);
    }
    return true;
  }
  if (matchRaw(TokenKind::String)) {
    return fail("template arguments do not accept string literals");
  }

  Token name = consumeRaw(TokenKind::Identifier, "expected template identifier");
  if (name.kind == TokenKind::End) {
    return false;
  }
  if (name.text == "true" || name.text == "false") {
    return fail("template arguments do not accept bool literals");
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
      if (!parseTemplateArgument(nested, nullptr)) {
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
      if (matchRaw(TokenKind::Identifier) || matchRaw(TokenKind::Number) ||
          matchRaw(TokenKind::String)) {
        continue;
      }
      return fail("expected '>'");
    }
    if (!expectRaw(TokenKind::RAngle, "expected '>'")) {
      return false;
    }
    out += ">";
  }
  if (matchRaw(TokenKind::LBracket)) {
    if (!expectRaw(TokenKind::LBracket, "expected '['")) {
      return false;
    }
    std::string indexArg;
    if (!parseTemplateArgument(indexArg, nullptr)) {
      return false;
    }
    if (!expectRaw(TokenKind::RBracket, "expected ']'")) {
      return false;
    }
    out += "[" + indexArg + "]";
  }
  if (matchRaw(TokenKind::Ellipsis)) {
    if (!expectRaw(TokenKind::Ellipsis, "expected '...'")) {
      return false;
    }
    out += "...";
  }
  if (detailOut != nullptr) {
    *detailOut = TemplateArgument::type(out);
  }
  return true;
}

bool Parser::parseTemplateParameterList(std::vector<std::string> &out,
                                        std::vector<bool> &outIsPack) {
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

  if (!expectRaw(TokenKind::LAngle, "expected '<'")) {
    return false;
  }
  skipSeparators();
  if (matchRaw(TokenKind::RAngle)) {
    return fail("expected template parameter identifier");
  }

  std::unordered_set<std::string> seen;
  bool sawPack = false;
  while (true) {
    if (sawPack && matchRaw(TokenKind::Identifier)) {
      size_t scan = pos_ + 1;
      while (scan < tokens_.size() && tokens_[scan].kind == TokenKind::Comment) {
        ++scan;
      }
      if (scan < tokens_.size() && tokens_[scan].kind == TokenKind::Ellipsis) {
        return fail("only one type pack parameter is supported");
      }
      return fail("type pack parameter must be last");
    }
    Token name = consumeRaw(TokenKind::Identifier, "expected template parameter identifier");
    if (name.kind == TokenKind::End) {
      return false;
    }
    std::string nameError;
    if (!validateIdentifierText(name.text, nameError)) {
      return fail(nameError);
    }
    if (name.text == "args" && matchRaw(TokenKind::LAngle)) {
      return fail("args<T> is variadic value-pack syntax, not a heterogeneous type pack; use Ts...");
    }
    if (matchRaw(TokenKind::LAngle)) {
      return fail("template parameter declarations accept identifiers only; use Ts... for a type pack");
    }
    const bool isPack = matchRaw(TokenKind::Ellipsis);
    if (isPack) {
      if (sawPack) {
        return fail("only one type pack parameter is supported");
      }
      if (!expectRaw(TokenKind::Ellipsis, "expected '...'")) {
        return false;
      }
      sawPack = true;
    }
    if (!seen.insert(name.text).second) {
      return fail("duplicate template parameter: " + name.text);
    }
    out.push_back(name.text);
    outIsPack.push_back(isPack);

    skipSeparators();
    if (matchRaw(TokenKind::RAngle)) {
      break;
    }
    if (matchRaw(TokenKind::Identifier)) {
      continue;
    }
    return fail("expected '>'");
  }
  return expectRaw(TokenKind::RAngle, "expected '>'");
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
      if (!parseTemplateArgument(nested, nullptr)) {
        return false;
      }
      if (!first) {
        out += ", ";
      }
      out += nested;
      first = false;
      skipSeparators();
      if (matchRaw(TokenKind::Ellipsis)) {
        return fail("type-pack expansion is only valid in template parameter lists");
      }
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

bool Parser::parseParameterBinding(Expr &out,
                                   const std::string &namespacePrefix,
                                   std::vector<std::string> *implicitTemplateArgsOut,
                                   std::vector<bool> *implicitTemplateArgIsPackOut) {
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
  param.sourceLine = name.line;
  param.sourceColumn = name.column;
  param.transforms = std::move(transforms);
  param.isBinding = true;
  bool isVariadic = false;
  if (match(TokenKind::Ellipsis)) {
    expect(TokenKind::Ellipsis, "expected '...'");
    isVariadic = true;
    std::optional<Transform> explicitType;
    for (const auto &transform : param.transforms) {
      if (isBindingAuxTransformName(transform.name) || !transform.arguments.empty()) {
        return fail("variadic parameter does not accept binding modifiers");
      }
      if (explicitType.has_value()) {
        return fail("variadic parameter requires exactly one envelope");
      }
      explicitType = transform;
    }
    std::string packType;
    if (!explicitType.has_value() || explicitType->name == "auto") {
      if (implicitTemplateArgsOut == nullptr) {
        return fail("variadic parameter sugar requires definition template context");
      }
      packType = nextVariadicPackTemplateName(*implicitTemplateArgsOut);
      implicitTemplateArgsOut->push_back(packType);
      if (implicitTemplateArgIsPackOut != nullptr) {
        implicitTemplateArgIsPackOut->push_back(false);
      }
    } else {
      if (explicitType->name == "args") {
        return fail("variadic parameter cannot combine ... with args envelope");
      }
      packType = renderTransformAsType(*explicitType);
    }
    Transform packTransform;
    packTransform.name = "args";
    packTransform.templateArgs.push_back(std::move(packType));
    param.transforms.clear();
    param.transforms.push_back(std::move(packTransform));
  }
  if (match(TokenKind::LParen)) {
    return fail("parameter defaults must use braces");
  }
  if (match(TokenKind::LBrace)) {
    if (isVariadic) {
      return fail("variadic parameter does not accept default initializer");
    }
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

bool Parser::parseParameterList(std::vector<Expr> &out,
                                const std::string &namespacePrefix,
                                std::vector<std::string> *implicitTemplateArgsOut,
                                std::vector<bool> *implicitTemplateArgIsPackOut) {
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
  bool sawVariadicParam = false;
  while (true) {
    if (matchRaw(TokenKind::Semicolon)) {
      expectRaw(TokenKind::Semicolon, "expected ';'");
      if (matchRaw(TokenKind::RParen)) {
        break;
      }
      continue;
    }
    Expr param;
    if (matchRaw(TokenKind::Identifier)) {
      size_t next = pos_ + 1;
      while (next < tokens_.size() && tokens_[next].kind == TokenKind::Comment) {
        ++next;
      }
      if (next < tokens_.size() && tokens_[next].kind == TokenKind::Ellipsis) {
        Token name = consumeRaw(TokenKind::Identifier, "expected parameter identifier");
        if (name.kind == TokenKind::End) {
          return false;
        }
        std::string nameError;
        if (!validateIdentifierText(name.text, nameError)) {
          return fail(nameError);
        }
        if (implicitTemplateArgsOut == nullptr) {
          return fail("variadic parameter sugar requires definition template context");
        }
        if (!expectRaw(TokenKind::Ellipsis, "expected '...' after variadic parameter")) {
          return false;
        }
        Expr packParam;
        packParam.kind = Expr::Kind::Call;
        packParam.name = name.text;
        packParam.namespacePrefix = namespacePrefix;
        packParam.sourceLine = name.line;
        packParam.sourceColumn = name.column;
        packParam.isBinding = true;
        Transform packTransform;
        packTransform.name = "args";
        packTransform.templateArgs.push_back(nextVariadicPackTemplateName(*implicitTemplateArgsOut));
        implicitTemplateArgsOut->push_back(packTransform.templateArgs.front());
        packParam.transforms.push_back(std::move(packTransform));
        param = std::move(packParam);
      } else if (next < tokens_.size() &&
                 (tokens_[next].kind == TokenKind::LBrace ||
                  tokens_[next].kind == TokenKind::Comma ||
                  tokens_[next].kind == TokenKind::Semicolon ||
                  tokens_[next].kind == TokenKind::RParen)) {
        Token name = consumeRaw(TokenKind::Identifier, "expected parameter identifier");
        if (name.kind == TokenKind::End) {
          return false;
        }
        std::string nameError;
        if (!validateIdentifierText(name.text, nameError)) {
          return fail(nameError);
        }
        if (matchRaw(TokenKind::LAngle)) {
          return fail("parameter identifiers do not accept template arguments");
        }
        Expr bareParam;
        bareParam.kind = Expr::Kind::Call;
        bareParam.name = name.text;
        bareParam.namespacePrefix = namespacePrefix;
        bareParam.sourceLine = name.line;
        bareParam.sourceColumn = name.column;
        bareParam.isBinding = true;
        if (matchRaw(TokenKind::LBrace)) {
          if (!parseBindingInitializerList(bareParam.args, bareParam.argNames, namespacePrefix)) {
            return false;
          }
          for (const auto &nameLabel : bareParam.argNames) {
            if (nameLabel.has_value()) {
              return fail("parameter defaults do not accept named arguments");
            }
          }
        }
        param = std::move(bareParam);
      } else if (!matchRaw(TokenKind::LBracket)) {
        return fail("expected '[' to start parameter");
      }
    }
    if (param.name.empty() &&
        !parseParameterBinding(param, namespacePrefix, implicitTemplateArgsOut, implicitTemplateArgIsPackOut)) {
      return false;
    }
    const bool isVariadic =
        !param.transforms.empty() && param.transforms.front().name == "args" && param.args.empty();
    if (sawVariadicParam) {
      return fail("variadic parameter must be final");
    }
    if (isVariadic) {
      sawVariadicParam = true;
    }
    out.push_back(std::move(param));
    if (matchRaw(TokenKind::Comma) || matchRaw(TokenKind::Semicolon)) {
      if (sawVariadicParam) {
        return fail("variadic parameter must be final");
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
      continue;
    }
    if (matchRaw(TokenKind::RParen)) {
      break;
    }
    continue;
  }
  return true;
}

} // namespace primec
