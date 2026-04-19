#include "primec/Parser.h"

#include "ParserHelpers.h"

#include <algorithm>
#include <sstream>
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

bool Parser::parseParameterBinding(Expr &out,
                                   const std::string &namespacePrefix,
                                   std::vector<std::string> *implicitTemplateArgsOut) {
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
                                std::vector<std::string> *implicitTemplateArgsOut) {
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
    if (param.name.empty() && !parseParameterBinding(param, namespacePrefix, implicitTemplateArgsOut)) {
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
