#include "primec/Parser.h"

#include "ParserHelpers.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace primec {
using namespace parser;

namespace {

bool isIgnorableToken(TokenKind kind) {
  return kind == TokenKind::Comment || kind == TokenKind::Comma || kind == TokenKind::Semicolon;
}

} // namespace

Parser::Parser(std::vector<Token> tokens, bool allowSurfaceSyntax)
    : tokens_(std::move(tokens)), allowSurfaceSyntax_(allowSurfaceSyntax) {}

bool Parser::parse(Program &program,
                   std::string &error,
                   ErrorInfo *errorInfo,
                   std::vector<ErrorInfo> *errorInfos) {
  error_ = &error;
  error.clear();
  lastErrorInfo_ = {};
  errorInfos_ = errorInfos;
  if (errorInfos_ != nullptr) {
    errorInfos_->clear();
  }
  mathImportAll_ = false;
  mathImports_.clear();
  for (size_t scan = 0; scan < tokens_.size(); ++scan) {
    if (tokens_[scan].kind != TokenKind::KeywordImport) {
      continue;
    }
    ++scan;
    while (scan < tokens_.size()) {
      if (isIgnorableToken(tokens_[scan].kind)) {
        ++scan;
        continue;
      }
      if (tokens_[scan].kind == TokenKind::Identifier) {
        std::string path = tokens_[scan].text;
        size_t next = scan + 1;
        while (next < tokens_.size() && isIgnorableToken(tokens_[next].kind)) {
          ++next;
        }
        if (!path.empty() && path.back() == '/' && next < tokens_.size() &&
            tokens_[next].kind == TokenKind::Star) {
          path.pop_back();
          path += "/*";
          scan = next;
        }
        if (path == "/std/math/*") {
          mathImportAll_ = true;
        } else if (path.rfind("/std/math/", 0) == 0 && path.size() > 10) {
          std::string name = path.substr(10);
          if (name.find('/') == std::string::npos && name != "*") {
            mathImports_.insert(std::move(name));
          }
        }
        ++scan;
        continue;
      }
      break;
    }
  }
  bool hadError = false;
  auto handleParseError = [&](size_t startPos) {
    hadError = true;
    if (errorInfos_ == nullptr) {
      if (errorInfo != nullptr) {
        *errorInfo = lastErrorInfo_;
      }
      errorInfos_ = nullptr;
      return false;
    }
    recoverToTopLevel(startPos);
    return true;
  };

  while (!match(TokenKind::End)) {
    const size_t startPos = pos_;
    if (match(TokenKind::Semicolon)) {
      expect(TokenKind::Semicolon, "expected ';'");
      continue;
    }
    if (match(TokenKind::KeywordImport)) {
      if (!parseImport(program)) {
        if (!handleParseError(startPos)) {
          return false;
        }
        continue;
      }
      continue;
    }
    if (match(TokenKind::KeywordNamespace)) {
      if (!parseNamespace(program.definitions, program.executions)) {
        if (!handleParseError(startPos)) {
          return false;
        }
        continue;
      }
    } else {
      if (!parseDefinitionOrExecution(program.definitions, program.executions)) {
        if (!handleParseError(startPos)) {
          return false;
        }
        continue;
      }
    }
  }
  if (hadError) {
    if (errorInfo != nullptr) {
      if (errorInfos_ != nullptr && !errorInfos_->empty()) {
        *errorInfo = errorInfos_->front();
      } else {
        *errorInfo = lastErrorInfo_;
      }
    }
    errorInfos_ = nullptr;
    return false;
  }
  if (errorInfo != nullptr) {
    *errorInfo = {};
  }
  errorInfos_ = nullptr;
  return true;
}

bool Parser::parseExpression(Expr &expr, const std::string &namespacePrefix, std::string &error, ErrorInfo *errorInfo) {
  error_ = &error;
  lastErrorInfo_ = {};
  errorInfos_ = nullptr;
  error.clear();
  if (!parseExpr(expr, namespacePrefix)) {
    if (errorInfo != nullptr) {
      *errorInfo = lastErrorInfo_;
    }
    return false;
  }
  if (!match(TokenKind::End)) {
    const bool ok = fail("unexpected token after expression");
    if (errorInfo != nullptr) {
      *errorInfo = lastErrorInfo_;
    }
    return ok;
  }
  if (errorInfo != nullptr) {
    *errorInfo = {};
  }
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

bool Parser::allowMathBareName(const std::string &name) const {
  if (name.empty() || name.find('/') != std::string::npos) {
    return false;
  }
  const std::string prefix = currentNamespacePrefix();
  if (prefix == "/math" || prefix.rfind("/math/", 0) == 0) {
    return true;
  }
  return mathImportAll_ || mathImports_.count(name) > 0;
}

bool Parser::isBuiltinNameForArguments(const std::string &name) const {
  std::string normalized = name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  return isBuiltinName(normalized, allowMathBareName(normalized));
}

void Parser::skipComments() {
  while (tokens_[pos_].kind == TokenKind::Comment ||
         (allowSeparatorWhitespace_ &&
          (tokens_[pos_].kind == TokenKind::Comma || tokens_[pos_].kind == TokenKind::Semicolon))) {
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
    lastErrorInfo_.line = token.line;
    lastErrorInfo_.column = token.column;
    std::ostringstream out;
    if (token.kind == TokenKind::Invalid) {
      if (token.text.size() == 1) {
        lastErrorInfo_.message = "invalid character: " + describeInvalidToken(token);
      } else {
        lastErrorInfo_.message = describeInvalidToken(token);
      }
    } else {
      lastErrorInfo_.message = message;
    }
    out << lastErrorInfo_.message << " at " << token.line << ":" << token.column;
    if (errorInfos_ != nullptr) {
      errorInfos_->push_back(lastErrorInfo_);
      if (error_->empty()) {
        *error_ = out.str();
      }
    } else {
      *error_ = out.str();
    }
  }
  return false;
}

void Parser::recoverToTopLevel(size_t startPos) {
  if (tokens_.empty()) {
    return;
  }
  if (pos_ <= startPos && pos_ < tokens_.size()) {
    ++pos_;
  }
  while (pos_ < tokens_.size()) {
    const TokenKind kind = tokens_[pos_].kind;
    if (kind == TokenKind::End) {
      return;
    }
    if (kind == TokenKind::Semicolon) {
      ++pos_;
      return;
    }
    if (kind == TokenKind::KeywordImport || kind == TokenKind::KeywordNamespace || kind == TokenKind::LBracket ||
        kind == TokenKind::Identifier) {
      return;
    }
    ++pos_;
  }
}

} // namespace primec
