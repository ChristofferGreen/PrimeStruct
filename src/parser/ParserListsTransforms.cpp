#include "primec/Parser.h"

#include "ParserHelpers.h"

#include "primec/StringLiteral.h"
#include "primec/TransformRegistry.h"

#include <sstream>
#include <utility>

namespace primec {
using namespace parser;

namespace {

void printTransforms(std::ostringstream &out, const std::vector<Transform> &transforms) {
  if (transforms.empty()) {
    return;
  }
  out << "[";
  for (size_t i = 0; i < transforms.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << transforms[i].name;
    if (!transforms[i].templateArgs.empty()) {
      out << "<";
      for (size_t t = 0; t < transforms[i].templateArgs.size(); ++t) {
        if (t > 0) {
          out << ", ";
        }
        out << transforms[i].templateArgs[t];
      }
      out << ">";
    }
    if (!transforms[i].arguments.empty()) {
      out << "(";
      for (size_t argIndex = 0; argIndex < transforms[i].arguments.size(); ++argIndex) {
        if (argIndex > 0) {
          out << ", ";
        }
        out << transforms[i].arguments[argIndex];
      }
      out << ")";
    }
  }
  out << "] ";
}

void printTemplateArgs(std::ostringstream &out, const std::vector<std::string> &args) {
  if (args.empty()) {
    return;
  }
  out << "<";
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << args[i];
  }
  out << ">";
}

void printExpr(std::ostringstream &out, const Expr &expr) {
  if (expr.isSpread) {
    out << "[spread] ";
  }
  switch (expr.kind) {
    case Expr::Kind::Literal:
      if (expr.isUnsigned) {
        out << expr.literalValue << (expr.intWidth == 64 ? "u64" : "u32");
      } else {
        out << static_cast<int64_t>(expr.literalValue) << (expr.intWidth == 64 ? "i64" : "i32");
      }
      break;
    case Expr::Kind::BoolLiteral:
      out << (expr.boolValue ? "true" : "false");
      break;
    case Expr::Kind::FloatLiteral:
      out << expr.floatValue << (expr.floatWidth == 64 ? "f64" : "f32");
      break;
    case Expr::Kind::StringLiteral:
      out << expr.stringValue;
      break;
    case Expr::Kind::Name:
      if (!expr.transforms.empty()) {
        printTransforms(out, expr.transforms);
      }
      out << expr.name;
      break;
    case Expr::Kind::Call:
      if (expr.isMethodCall && !expr.args.empty()) {
        printExpr(out, expr.args.front());
        out << "." << expr.name;
        printTemplateArgs(out, expr.templateArgs);
        out << "(";
        for (size_t i = 1; i < expr.args.size(); ++i) {
          if (i > 1) {
            out << ", ";
          }
          if (i < expr.argNames.size() && expr.argNames[i].has_value()) {
            out << "[" << *expr.argNames[i] << "] ";
          }
          printExpr(out, expr.args[i]);
        }
        out << ")";
      } else {
        if (!expr.transforms.empty()) {
          printTransforms(out, expr.transforms);
        }
        out << expr.name;
        printTemplateArgs(out, expr.templateArgs);
        const bool useBraces = expr.isBinding;
        out << (useBraces ? "{" : "(");
        for (size_t i = 0; i < expr.args.size(); ++i) {
          if (i > 0) {
            out << ", ";
          }
          if (i < expr.argNames.size() && expr.argNames[i].has_value()) {
            out << "[" << *expr.argNames[i] << "] ";
          }
          printExpr(out, expr.args[i]);
        }
        out << (useBraces ? "}" : ")");
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        out << " { ";
        for (size_t i = 0; i < expr.bodyArguments.size(); ++i) {
          if (i > 0) {
            out << ", ";
          }
          printExpr(out, expr.bodyArguments[i]);
        }
        out << " }";
      }
      break;
  }
}

} // namespace

bool Parser::parseTransformList(std::vector<Transform> &out) {
  auto skipSeparators = [&]() {
    while (match(TokenKind::Comma) || match(TokenKind::Semicolon)) {
      if (match(TokenKind::Comma)) {
        expect(TokenKind::Comma, "expected ','");
      } else {
        expect(TokenKind::Semicolon, "expected ';'");
      }
    }
  };
  if (!expect(TokenKind::LBracket, "expected '['")) {
    return false;
  }
  skipSeparators();
  if (match(TokenKind::RBracket)) {
    return fail("transform list cannot be empty");
  }
  auto isBoolLiteral = [](const std::string &text) {
    return text == "true" || text == "false";
  };
  auto parseSimpleTransformArgument = [&](std::vector<std::string> &args) -> bool {
    if (match(TokenKind::Identifier)) {
      Token arg = consume(TokenKind::Identifier, "expected transform argument");
      if (arg.kind == TokenKind::End) {
        return false;
      }
      if (!isBoolLiteral(arg.text)) {
        std::string argError;
        if (!validateIdentifierText(arg.text, argError)) {
          return fail(argError);
        }
      }
      args.push_back(arg.text);
      return true;
    }
    if (match(TokenKind::Number)) {
      Token arg = consume(TokenKind::Number, "expected transform argument");
      if (arg.kind == TokenKind::End) {
        return false;
      }
      args.push_back(arg.text);
      return true;
    }
    if (match(TokenKind::String)) {
      Token arg = consume(TokenKind::String, "expected transform argument");
      if (arg.kind == TokenKind::End) {
        return false;
      }
      std::string normalized;
      std::string parseError;
      if (!normalizeStringLiteralToken(arg.text, normalized, parseError)) {
        return fail(parseError);
      }
      args.push_back(std::move(normalized));
      return true;
    }
    return fail("expected transform argument");
  };
  auto parseFullTransformArgument = [&](std::vector<std::string> &args) -> bool {
    Expr arg;
    if (!parseExpr(arg, currentNamespacePrefix())) {
      return false;
    }
    std::ostringstream out;
    printExpr(out, arg);
    std::string argText = out.str();
    if (argText.empty()) {
      return fail("expected transform argument");
    }
    args.push_back(std::move(argText));
    return true;
  };
  auto allowFullTransformArguments = [&](TransformPhase phase, const std::string &name) {
    if (phase == TransformPhase::Text) {
      return false;
    }
    if (phase == TransformPhase::Semantic) {
      return true;
    }
    return primec::isSemanticTransformName(name);
  };
  auto parseTransformArguments = [&](Transform &transform, TransformPhase phase) -> bool {
    if (!match(TokenKind::LParen)) {
      return true;
    }
    const bool allowFullForms = allowFullTransformArguments(phase, transform.name);
    auto failTextTransformArgument = [&]() -> bool {
      return fail("text transform arguments must be identifiers or literals");
    };
    expect(TokenKind::LParen, "expected '('");
    skipSeparators();
    if (match(TokenKind::RParen)) {
      return fail("transform argument list cannot be empty");
    }
    while (true) {
      if (allowFullForms) {
        if (!parseFullTransformArgument(transform.arguments)) {
          return false;
        }
      } else {
        if (match(TokenKind::LBracket) || match(TokenKind::LParen) || match(TokenKind::LBrace) ||
            match(TokenKind::LAngle)) {
          return failTextTransformArgument();
        }
        if (!parseSimpleTransformArgument(transform.arguments)) {
          return false;
        }
        if (match(TokenKind::LBracket) || match(TokenKind::LParen) || match(TokenKind::LBrace) ||
            match(TokenKind::LAngle)) {
          return failTextTransformArgument();
        }
      }
      skipSeparators();
      if (match(TokenKind::RParen)) {
        break;
      }
      if (allowFullForms) {
        if (match(TokenKind::Identifier) || match(TokenKind::Number) || match(TokenKind::String) ||
            match(TokenKind::LBracket)) {
          continue;
        }
      } else {
        if (match(TokenKind::Identifier) || match(TokenKind::Number) || match(TokenKind::String)) {
          continue;
        }
      }
      break;
    }
    if (!expect(TokenKind::RParen, "expected ')'")) {
      return false;
    }
    return true;
  };
  auto parseTransformItem = [&](TransformPhase phase, Transform &transform) -> bool {
    Token name = consume(TokenKind::Identifier, "expected transform identifier");
    if (name.kind == TokenKind::End) {
      return false;
    }
    std::string nameError;
    if (!validateTransformName(name.text, nameError)) {
      return fail(nameError);
    }
    transform.name = name.text;
    transform.phase = phase;
    if (match(TokenKind::LAngle)) {
      if (!parseTemplateList(transform.templateArgs)) {
        return false;
      }
    }
    if (!parseTransformArguments(transform, phase)) {
      return false;
    }
    return true;
  };
  auto parseTransformGroup = [&](const std::string &groupName) -> bool {
    if (!expect(TokenKind::LParen, "expected '('")) {
      return false;
    }
    skipSeparators();
    if (match(TokenKind::RParen)) {
      return fail("transform " + groupName + " group cannot be empty");
    }
    while (!match(TokenKind::RParen)) {
      Transform nested;
      if (!parseTransformItem(groupName == "text" ? TransformPhase::Text : TransformPhase::Semantic, nested)) {
        return false;
      }
      out.push_back(std::move(nested));
      skipSeparators();
      if (match(TokenKind::RParen)) {
        break;
      }
    }
    if (!expect(TokenKind::RParen, "expected ')'")) {
      return false;
    }
    return true;
  };
  while (!match(TokenKind::RBracket)) {
    Token name = consume(TokenKind::Identifier, "expected transform identifier");
    if (name.kind == TokenKind::End) {
      return false;
    }
    std::string nameError;
    if (!validateTransformName(name.text, nameError)) {
      return fail(nameError);
    }
    if (name.text == "text" || name.text == "semantic") {
      if (match(TokenKind::LParen)) {
        if (!parseTransformGroup(name.text)) {
          return false;
        }
        continue;
      }
      return fail(name.text + " transform group requires parentheses");
    }
    Transform transform;
    transform.name = name.text;
    transform.phase = TransformPhase::Auto;
    if (match(TokenKind::LAngle)) {
      if (!parseTemplateList(transform.templateArgs)) {
        return false;
      }
    }
    if (!parseTransformArguments(transform, transform.phase)) {
      return false;
    }
    out.push_back(std::move(transform));
    skipSeparators();
  }
  expect(TokenKind::RBracket, "expected ']'");
  return true;
}

} // namespace primec
