#pragma once

#include <string>
#include <vector>

#include "primec/Ast.h"
#include "primec/Token.h"

namespace primec {

class Parser {
public:
  explicit Parser(std::vector<Token> tokens);

  bool parse(Program &program, std::string &error);

private:
  bool parseNamespace(std::vector<Definition> &defs, std::vector<Execution> &execs);
  bool parseDefinitionOrExecution(std::vector<Definition> &defs, std::vector<Execution> &execs);
  bool parseTransformList(std::vector<Transform> &out);
  bool parseTemplateList(std::vector<std::string> &out);
  bool parseIdentifierList(std::vector<std::string> &out);
  bool parseCallArgumentList(std::vector<Expr> &out,
                             std::vector<std::optional<std::string>> &argNames,
                             const std::string &namespacePrefix);
  bool parseBraceExprList(std::vector<Expr> &out, const std::string &namespacePrefix);
  bool validateNoBuiltinNamedArguments(const std::string &name,
                                       const std::vector<std::optional<std::string>> &argNames);
  bool parseReturnStatement(Expr &out, const std::string &namespacePrefix);
  bool definitionHasReturnBeforeClose() const;
  bool isDefinitionSignature(bool *paramsAreIdentifiers) const;
  bool parseDefinitionBody(Definition &def);
  bool parseExpr(Expr &expr, const std::string &namespacePrefix);

  std::string currentNamespacePrefix() const;
  std::string makeFullPath(const std::string &name, const std::string &prefix) const;

  bool match(TokenKind kind) const;
  bool expect(TokenKind kind, const std::string &message);
  Token consume(TokenKind kind, const std::string &message);
  bool fail(const std::string &message);

  std::vector<Token> tokens_;
  size_t pos_ = 0;
  std::vector<std::string> namespaceStack_;
  std::string *error_ = nullptr;
  bool *returnTracker_ = nullptr;
  bool returnsVoidContext_ = false;
};

} // namespace primec
