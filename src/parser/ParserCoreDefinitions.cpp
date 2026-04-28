#include "primec/Parser.h"

#include "ParserHelpers.h"

#include <algorithm>
#include <utility>

namespace primec {
using namespace parser;

namespace {

bool isBindingOnlyTransformName(const std::string &name) {
  return name == "mut" || name == "copy" || name == "restrict" || name == "align_bytes" ||
         name == "align_kbytes";
}

bool isNoReturnDefinitionTransform(const std::string &name) {
  return (isStructTransformName(name) || name == "sum") && !isBindingAuxTransformName(name);
}

size_t skipCommentTokens(const std::vector<Token> &tokens, size_t index) {
  while (index < tokens.size()) {
    TokenKind kind = tokens[index].kind;
    if (kind != TokenKind::Comment && kind != TokenKind::Comma && kind != TokenKind::Semicolon) {
      break;
    }
    ++index;
  }
  return index;
}

size_t skipDefinitionTailTransforms(const std::vector<Token> &tokens, size_t index, bool allowSurfaceSyntax) {
  size_t scan = skipCommentTokens(tokens, index);
  if (!allowSurfaceSyntax || scan >= tokens.size() || tokens[scan].kind != TokenKind::LBracket) {
    return scan;
  }
  int bracketDepth = 0;
  while (scan < tokens.size()) {
    TokenKind kind = tokens[scan].kind;
    if (kind == TokenKind::LBracket) {
      ++bracketDepth;
    } else if (kind == TokenKind::RBracket) {
      --bracketDepth;
      if (bracketDepth == 0) {
        ++scan;
        break;
      }
    }
    ++scan;
  }
  return skipCommentTokens(tokens, scan);
}

} // namespace

bool Parser::parseImport(Program &program) {
  if (!expect(TokenKind::KeywordImport, "expected 'import'")) {
    return false;
  }
  while (true) {
    skipComments();
    Token path = consume(TokenKind::Identifier, "expected import path");
    if (path.kind == TokenKind::End) {
      return false;
    }
    if (path.text.empty() || path.text[0] != '/') {
      return fail("import path must be a slash path");
    }
    std::string pathText = path.text;
    skipComments();
    if (!pathText.empty() && pathText.back() == '/' && match(TokenKind::Star)) {
      expect(TokenKind::Star, "expected '*'");
      pathText.pop_back();
      std::string nameError;
      if (!validateIdentifierText(pathText, nameError)) {
        return fail(nameError);
      }
      pathText += "/*";
    } else {
      std::string nameError;
      if (!validateIdentifierText(pathText, nameError)) {
        return fail(nameError);
      }
    }
    if (pathText == "/std/math") {
      return fail("import /std/math is not supported; use import /std/math/* or /std/math/<name>");
    }
    if (pathText.find('/', 1) == std::string::npos) {
      pathText += "/*";
    }
    if (std::find(program.imports.begin(), program.imports.end(), pathText) == program.imports.end()) {
      program.imports.push_back(pathText);
    }
    if (pathText == "/std/math/*") {
      mathImportAll_ = true;
    } else if (pathText.rfind("/std/math/", 0) == 0 && pathText.size() > 10) {
      std::string name = pathText.substr(10);
      if (name.find('/') == std::string::npos && name != "*") {
        mathImports_.insert(std::move(name));
      }
    }
    skipComments();
    while (match(TokenKind::Comma) || match(TokenKind::Semicolon)) {
      if (match(TokenKind::Comma)) {
        expect(TokenKind::Comma, "expected ','");
      } else {
        expect(TokenKind::Semicolon, "expected ';'");
      }
      skipComments();
    }
    if (tokens_[pos_].kind == TokenKind::Identifier && !tokens_[pos_].text.empty() &&
        tokens_[pos_].text[0] == '/') {
      continue;
    }
    break;
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
    if (match(TokenKind::Semicolon)) {
      expect(TokenKind::Semicolon, "expected ';'");
      continue;
    }
    if (match(TokenKind::KeywordImport)) {
      return fail("import statements must appear at the top level");
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
  if (allowSurfaceSyntax_ && match(TokenKind::LBracket)) {
    return fail("definition transform tail sugar requires parameter list before transform list");
  }
  bool hasReturnTransform = false;
  bool hasNoReturnDefinitionTransform = false;
  for (const auto &transform : transforms) {
    if (transform.name == "return") {
      hasReturnTransform = true;
    }
    if (isNoReturnDefinitionTransform(transform.name)) {
      hasNoReturnDefinitionTransform = true;
    }
  }
  bool hasDefinitionOnlyNoReturnTransform = false;
  for (const auto &transform : transforms) {
    if (isNoReturnDefinitionTransform(transform.name)) {
      hasDefinitionOnlyNoReturnTransform = true;
      break;
    }
  }
  const bool hasBindingTypeTransform = hasExplicitBindingTypeTransform(transforms);
  bool hasBindingOnlyTransform = false;
  for (const auto &transform : transforms) {
    if (isBindingOnlyTransformName(transform.name)) {
      hasBindingOnlyTransform = true;
      break;
    }
  }
  if (match(TokenKind::LBrace)) {
    if ((hasBindingTypeTransform || hasBindingOnlyTransform) && !hasReturnTransform &&
        !hasDefinitionOnlyNoReturnTransform) {
      return fail(
          "expected '(' after identifier; bindings are only allowed inside definition bodies or parameter lists");
    }
    if (!allowSurfaceSyntax_ && !hasReturnTransform && !hasNoReturnDefinitionTransform) {
      return fail("definition requires explicit return transform in canonical mode");
    }
    Definition def;
    def.name = name.text;
    def.namespacePrefix = currentNamespacePrefix();
    def.fullPath = makeFullPath(def.name, def.namespacePrefix);
    def.sourceLine = name.line;
    def.sourceColumn = name.column;
    def.transforms = std::move(transforms);
    def.templateArgs = std::move(templateArgs);
    if (!parseDefinitionBody(def, hasNoReturnDefinitionTransform, defs)) {
      return false;
    }
    defs.push_back(std::move(def));
    return true;
  }
  if (!expect(TokenKind::LParen, "expected '(' after identifier")) {
    return false;
  }
  bool isDefinition = false;
  const bool copyShorthand =
      allowSurfaceSyntax_ && name.text == "Copy" && isCopyConstructorShorthandSignature();
  if (hasReturnTransform) {
    isDefinition = true;
  } else if (hasNoReturnDefinitionTransform) {
    isDefinition = isDefinitionSignatureAllowNoReturn(nullptr);
  } else {
    isDefinition = isDefinitionSignature(nullptr);
  }
  if (copyShorthand) {
    isDefinition = true;
  }
  if (!allowSurfaceSyntax_ && isDefinition && !hasReturnTransform && !hasNoReturnDefinitionTransform) {
    return fail("definition requires explicit return transform in canonical mode");
  }
  if (isDefinition) {
    std::vector<Expr> parameters;
    if (copyShorthand) {
      if (!parseCopyConstructorShorthand(parameters, currentNamespacePrefix())) {
        return false;
      }
    } else {
      if (!parseParameterList(parameters, currentNamespacePrefix(), &templateArgs)) {
        return false;
      }
    }
    if (!expect(TokenKind::RParen, "expected ')' after parameters")) {
      return false;
    }
    if (allowSurfaceSyntax_ && match(TokenKind::LBracket)) {
      std::vector<Transform> tailTransforms;
      if (!parseTransformList(tailTransforms)) {
        return false;
      }
      for (auto &transform : tailTransforms) {
        if (transform.name == "return") {
          hasReturnTransform = true;
        }
        if (isNoReturnDefinitionTransform(transform.name)) {
          hasNoReturnDefinitionTransform = true;
        }
        transforms.push_back(std::move(transform));
      }
    }
    if (!match(TokenKind::LBrace)) {
      return fail("definitions must have a body");
    }
    Definition def;
    def.name = name.text;
    def.namespacePrefix = currentNamespacePrefix();
    def.fullPath = makeFullPath(def.name, def.namespacePrefix);
    def.sourceLine = name.line;
    def.sourceColumn = name.column;
    def.transforms = std::move(transforms);
    def.templateArgs = std::move(templateArgs);
    def.parameters = std::move(parameters);
    if (!parseDefinitionBody(def, hasNoReturnDefinitionTransform, defs)) {
      return false;
    }
    defs.push_back(std::move(def));
    return true;
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
  if (allowSurfaceSyntax_ && match(TokenKind::LBracket)) {
    const size_t afterTailTransforms = skipDefinitionTailTransforms(tokens_, pos_, true);
    if (afterTailTransforms >= tokens_.size() || tokens_[afterTailTransforms].kind != TokenKind::Identifier) {
      return fail("post-parameter transforms are only valid on definitions");
    }
  }
  if (match(TokenKind::LBrace)) {
    return fail("executions do not accept body blocks");
  }

  Execution exec;
  exec.name = name.text;
  exec.namespacePrefix = currentNamespacePrefix();
  exec.fullPath = makeFullPath(exec.name, exec.namespacePrefix);
  exec.sourceLine = name.line;
  exec.sourceColumn = name.column;
  exec.transforms = std::move(transforms);
  exec.templateArgs = std::move(templateArgs);
  exec.arguments = std::move(arguments);
  exec.argumentNames = std::move(argumentNames);
  execs.push_back(std::move(exec));
  return true;
}

} // namespace primec
