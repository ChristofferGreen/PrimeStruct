#include "primec/Parser.h"

#include "ParserHelpers.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace primec {
using namespace parser;

namespace {

bool isNonTypeTransformName(const std::string &name) {
  return name == "return" || name == "effects" || name == "capabilities" || name == "mut" || name == "copy" ||
         name == "restrict" || name == "align_bytes" || name == "align_kbytes" || name == "struct" ||
         name == "pod" || name == "handle" || name == "gpu_lane" || name == "public" || name == "private" ||
         name == "package" || name == "static" || name == "single_type_to_return" || name == "stack" ||
         name == "heap" || name == "buffer";
}

bool isSingleTypeReturnCandidate(const Transform &transform) {
  if (!transform.arguments.empty()) {
    return false;
  }
  if (isNonTypeTransformName(transform.name)) {
    return false;
  }
  return true;
}

std::string formatTemplateArgs(const std::vector<std::string> &args) {
  std::string out;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      out += ", ";
    }
    out += args[i];
  }
  return out;
}

std::string formatTransformType(const Transform &transform) {
  if (transform.templateArgs.empty()) {
    return transform.name;
  }
  return transform.name + "<" + formatTemplateArgs(transform.templateArgs) + ">";
}

bool isBindingParamBracket(const std::vector<Token> &tokens, size_t index) {
  if (index >= tokens.size() || tokens[index].kind != TokenKind::LBracket) {
    return false;
  }
  auto skipComments = [&](size_t &pos) {
    while (pos < tokens.size() && tokens[pos].kind == TokenKind::Comment) {
      ++pos;
    }
  };
  size_t scan = index + 1;
  skipComments(scan);
  if (scan >= tokens.size() || tokens[scan].kind != TokenKind::Identifier) {
    return true;
  }
  ++scan;
  skipComments(scan);
  if (scan >= tokens.size() || tokens[scan].kind != TokenKind::RBracket) {
    return true;
  }
  ++scan;
  skipComments(scan);
  if (scan >= tokens.size()) {
    return true;
  }
  const TokenKind nextKind = tokens[scan].kind;
  if (nextKind == TokenKind::Number || nextKind == TokenKind::String) {
    return false;
  }
  if (nextKind == TokenKind::Identifier) {
    if (tokens[scan].text == "true" || tokens[scan].text == "false") {
      return false;
    }
    size_t follow = scan + 1;
    skipComments(follow);
    if (follow < tokens.size()) {
      const TokenKind followKind = tokens[follow].kind;
      if (followKind == TokenKind::LParen || followKind == TokenKind::Dot ||
          followKind == TokenKind::LBracket || followKind == TokenKind::LAngle) {
        return false;
      }
      if (followKind == TokenKind::LBrace) {
        return true;
      }
    }
    return true;
  }
  return true;
}

} // namespace

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

bool Parser::parse(Program &program, std::string &error) {
  error_ = &error;
  mathImportAll_ = false;
  mathImports_.clear();
  for (size_t scan = 0; scan < tokens_.size(); ++scan) {
    if (tokens_[scan].kind != TokenKind::KeywordImport) {
      continue;
    }
    ++scan;
    while (scan < tokens_.size()) {
      if (tokens_[scan].kind == TokenKind::Comment) {
        ++scan;
        continue;
      }
      if (tokens_[scan].kind == TokenKind::Identifier) {
        std::string path = tokens_[scan].text;
        if (!path.empty() && path.back() == '/' && scan + 1 < tokens_.size() &&
            tokens_[scan + 1].kind == TokenKind::Star) {
          path.pop_back();
          path += "/*";
          ++scan;
        }
        if (path == "/math/*") {
          mathImportAll_ = true;
        } else if (path.rfind("/math/", 0) == 0 && path.size() > 6) {
          std::string name = path.substr(6);
          if (name.find('/') == std::string::npos && name != "*") {
            mathImports_.insert(std::move(name));
          }
        }
        ++scan;
        continue;
      }
      if (tokens_[scan].kind == TokenKind::Comma) {
        ++scan;
        continue;
      }
      break;
    }
  }
  while (!match(TokenKind::End)) {
    if (match(TokenKind::Semicolon)) {
      return fail("semicolon is not allowed");
    }
    if (match(TokenKind::KeywordImport)) {
      if (!parseImport(program)) {
        return false;
      }
      continue;
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

bool Parser::parseImport(Program &program) {
  if (!expect(TokenKind::KeywordImport, "expected 'import'")) {
    return false;
  }
  while (true) {
    Token path = consume(TokenKind::Identifier, "expected import path");
    if (path.kind == TokenKind::End) {
      return false;
    }
    if (path.text.empty() || path.text[0] != '/') {
      return fail("import path must be a slash path");
    }
    std::string pathText = path.text;
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
    if (pathText == "/math") {
      return fail("import /math is not supported; use import /math/* or /math/<name>");
    }
    if (pathText.find('/', 1) == std::string::npos) {
      pathText += "/*";
    }
    if (std::find(program.imports.begin(), program.imports.end(), pathText) == program.imports.end()) {
      program.imports.push_back(pathText);
    }
    if (pathText == "/math/*") {
      mathImportAll_ = true;
    } else if (pathText.rfind("/math/", 0) == 0 && pathText.size() > 6) {
      std::string name = pathText.substr(6);
      if (name.find('/') == std::string::npos && name != "*") {
        mathImports_.insert(std::move(name));
      }
    }
    if (match(TokenKind::Comma)) {
      expect(TokenKind::Comma, "expected ','");
      if (match(TokenKind::End)) {
        return fail("expected import path");
      }
      continue;
    }
    skipComments();
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
      return fail("semicolon is not allowed");
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
  if (!applySingleTypeToReturn(transforms)) {
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

  if (match(TokenKind::LBrace)) {
    return fail("expected '(' after identifier; bindings are only allowed inside definition bodies or parameter lists");
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
    exec.hasBodyArguments = false;
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
  {
    BraceListGuard braceGuard(*this, true, true);
    if (!parseBraceExprList(exec.bodyArguments, exec.namespacePrefix)) {
      return false;
    }
  }
  exec.hasBodyArguments = true;
  execs.push_back(std::move(exec));
  return true;
}

bool Parser::applySingleTypeToReturn(std::vector<Transform> &transforms) {
  size_t markerIndex = transforms.size();
  for (size_t i = 0; i < transforms.size(); ++i) {
    if (transforms[i].name != "single_type_to_return") {
      continue;
    }
    if (markerIndex != transforms.size()) {
      return fail("duplicate single_type_to_return transform");
    }
    if (!transforms[i].templateArgs.empty()) {
      return fail("single_type_to_return does not accept template arguments");
    }
    if (!transforms[i].arguments.empty()) {
      return fail("single_type_to_return does not accept arguments");
    }
    markerIndex = i;
  }
  const bool hasExplicitMarker = markerIndex != transforms.size();
  if (!hasExplicitMarker && !forceSingleTypeToReturn_) {
    return true;
  }
  for (const auto &transform : transforms) {
    if (transform.name == "return") {
      if (hasExplicitMarker) {
        return fail("single_type_to_return cannot be combined with return transform");
      }
      return true;
    }
  }
  size_t typeIndex = transforms.size();
  for (size_t i = 0; i < transforms.size(); ++i) {
    if (i == markerIndex) {
      continue;
    }
    if (!isSingleTypeReturnCandidate(transforms[i])) {
      continue;
    }
    if (typeIndex != transforms.size()) {
      if (hasExplicitMarker) {
        return fail("single_type_to_return requires a single type transform");
      }
      return true;
    }
    typeIndex = i;
  }
  if (typeIndex == transforms.size()) {
    if (hasExplicitMarker) {
      return fail("single_type_to_return requires a type transform");
    }
    return true;
  }
  Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back(formatTransformType(transforms[typeIndex]));
  if (!hasExplicitMarker) {
    transforms[typeIndex] = std::move(returnTransform);
    return true;
  }
  std::vector<Transform> rewritten;
  rewritten.reserve(transforms.size() - 1);
  for (size_t i = 0; i < transforms.size(); ++i) {
    if (i == markerIndex) {
      continue;
    }
    if (i == typeIndex) {
      rewritten.push_back(std::move(returnTransform));
      continue;
    }
    rewritten.push_back(std::move(transforms[i]));
  }
  transforms = std::move(rewritten);
  return true;
}

bool Parser::definitionHasReturnBeforeClose() const {
  size_t index = pos_;
  int depth = 1;
  while (index < tokens_.size()) {
    TokenKind kind = tokens_[index].kind;
    if (kind == TokenKind::Comment) {
      ++index;
      continue;
    }
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
    if (kind == TokenKind::Comment) {
      ++braceIndex;
      continue;
    }
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
  int braceDepth = 0;
  bool identifiersOnly = true;
  bool sawBindingSyntax = false;
  bool sawIdentifier = false;
  TokenKind prevAtDepth1 = TokenKind::LParen;
  while (index < tokens_.size()) {
    TokenKind kind = tokens_[index].kind;
    if (kind == TokenKind::Comment) {
      ++index;
      continue;
    }
    if (kind == TokenKind::LParen) {
      ++depth;
    } else if (kind == TokenKind::RParen) {
      --depth;
      if (depth == 0) {
        break;
      }
    } else if (depth == 1) {
      if (kind == TokenKind::LBrace) {
        ++braceDepth;
      } else if (kind == TokenKind::RBrace && braceDepth > 0) {
        --braceDepth;
      }
      if (braceDepth == 0) {
        if (kind == TokenKind::LBracket) {
          identifiersOnly = false;
          const bool atArgStart = (prevAtDepth1 == TokenKind::LParen || prevAtDepth1 == TokenKind::Comma);
          if (atArgStart && isBindingParamBracket(tokens_, index)) {
            sawBindingSyntax = true;
          }
        } else if (kind == TokenKind::Identifier) {
          sawIdentifier = true;
        } else if (kind != TokenKind::Identifier && kind != TokenKind::Comma) {
          identifiersOnly = false;
        }
        prevAtDepth1 = kind;
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
  if (!sawBindingSyntax && sawIdentifier) {
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
  int braceDepth = 0;
  bool identifiersOnly = true;
  bool sawBindingSyntax = false;
  bool sawIdentifier = false;
  TokenKind prevAtDepth1 = TokenKind::LParen;
  while (index < tokens_.size()) {
    TokenKind kind = tokens_[index].kind;
    if (kind == TokenKind::Comment) {
      ++index;
      continue;
    }
    if (kind == TokenKind::LParen) {
      ++depth;
    } else if (kind == TokenKind::RParen) {
      --depth;
      if (depth == 0) {
        break;
      }
    } else if (depth == 1) {
      if (kind == TokenKind::LBrace) {
        ++braceDepth;
      } else if (kind == TokenKind::RBrace && braceDepth > 0) {
        --braceDepth;
      }
      if (braceDepth == 0) {
        if (kind == TokenKind::LBracket) {
          identifiersOnly = false;
          const bool atArgStart = (prevAtDepth1 == TokenKind::LParen || prevAtDepth1 == TokenKind::Comma);
          if (atArgStart && isBindingParamBracket(tokens_, index)) {
            sawBindingSyntax = true;
          }
        } else if (kind == TokenKind::Identifier) {
          sawIdentifier = true;
        } else if (kind != TokenKind::Identifier && kind != TokenKind::Comma) {
          identifiersOnly = false;
        }
        prevAtDepth1 = kind;
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
  if (!sawBindingSyntax && sawIdentifier) {
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
      if (match(TokenKind::LParen)) {
        return fail("binding initializers must use braces");
      }
      if (match(TokenKind::LBrace)) {
        if (!parseBindingInitializerList(callExpr.args, callExpr.argNames, def.namespacePrefix)) {
          return false;
        }
        if (!finalizeBindingInitializer(callExpr)) {
          return false;
        }
      } else {
        return fail("binding requires initializer");
      }
      def.statements.push_back(std::move(callExpr));
      continue;
    }

    Expr statementExpr;
    {
      BareBindingGuard bindingGuard(*this, true);
      if (!parseExpr(statementExpr, def.namespacePrefix)) {
        return false;
      }
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
  while (tokens_[pos_].kind == TokenKind::Comment) {
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
