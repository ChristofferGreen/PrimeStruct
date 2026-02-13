#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "primec/Ast.h"
#include "primec/Token.h"

namespace primec {

class Parser {
public:
  explicit Parser(std::vector<Token> tokens, bool allowSurfaceSyntax = true);

  bool parse(Program &program, std::string &error);

private:
  bool parseImport(Program &program);
  bool parseNamespace(std::vector<Definition> &defs, std::vector<Execution> &execs);
  bool parseDefinitionOrExecution(std::vector<Definition> &defs, std::vector<Execution> &execs);
  bool parseTransformList(std::vector<Transform> &out);
  bool parseTemplateList(std::vector<std::string> &out);
  bool parseTypeName(std::string &out);
  bool parseParameterList(std::vector<Expr> &out, const std::string &namespacePrefix);
  bool parseParameterBinding(Expr &out, const std::string &namespacePrefix);
  bool parseCallArgumentList(std::vector<Expr> &out,
                             std::vector<std::optional<std::string>> &argNames,
                             const std::string &namespacePrefix);
  bool parseBindingInitializerList(std::vector<Expr> &out,
                                   std::vector<std::optional<std::string>> &argNames,
                                   const std::string &namespacePrefix);
  bool parseBraceExprList(std::vector<Expr> &out, const std::string &namespacePrefix);
  bool validateNamedArgumentOrdering(const std::vector<std::optional<std::string>> &argNames);
  bool parseReturnStatement(Expr &out, const std::string &namespacePrefix);
  bool tryParseIfStatementSugar(Expr &out, const std::string &namespacePrefix, bool &parsed);
  bool definitionHasReturnBeforeClose() const;
  bool isDefinitionSignature(bool *paramsAreIdentifiers) const;
  bool isDefinitionSignatureAllowNoReturn(bool *paramsAreIdentifiers) const;
  bool parseDefinitionBody(Definition &def, bool allowNoReturn);
  bool parseExpr(Expr &expr, const std::string &namespacePrefix);
  bool finalizeBindingInitializer(Expr &binding);
  bool allowMathBareName(const std::string &name) const;
  bool isBuiltinNameForArguments(const std::string &name) const;

  struct ArgumentLabelGuard {
    explicit ArgumentLabelGuard(Parser &parser) : parser_(parser), previous_(parser_.allowArgumentLabels_) {
      parser_.allowArgumentLabels_ = true;
    }
    ~ArgumentLabelGuard() { parser_.allowArgumentLabels_ = previous_; }
    ArgumentLabelGuard(const ArgumentLabelGuard &) = delete;
    ArgumentLabelGuard &operator=(const ArgumentLabelGuard &) = delete;

  private:
    Parser &parser_;
    bool previous_;
  };

  struct BareBindingGuard {
    explicit BareBindingGuard(Parser &parser, bool enabled)
        : parser_(parser), previous_(parser_.allowBareBindings_) {
      parser_.allowBareBindings_ = enabled;
    }
    ~BareBindingGuard() { parser_.allowBareBindings_ = previous_; }
    BareBindingGuard(const BareBindingGuard &) = delete;
    BareBindingGuard &operator=(const BareBindingGuard &) = delete;

  private:
    Parser &parser_;
    bool previous_;
  };

  bool tryParseLoopFormAfterName(Expr &out,
                                 const std::string &namespacePrefix,
                                 const std::string &keyword,
                                 const std::vector<Transform> &transforms,
                                 bool &parsed);

  struct BraceListGuard {
    BraceListGuard(Parser &parser, bool allowBindings, bool allowReturn)
        : parser_(parser),
          prevAllowBindings_(parser_.allowBraceBindings_),
          prevAllowReturn_(parser_.allowBraceReturn_) {
      parser_.allowBraceBindings_ = allowBindings;
      parser_.allowBraceReturn_ = allowReturn;
    }
    ~BraceListGuard() {
      parser_.allowBraceBindings_ = prevAllowBindings_;
      parser_.allowBraceReturn_ = prevAllowReturn_;
    }
    BraceListGuard(const BraceListGuard &) = delete;
    BraceListGuard &operator=(const BraceListGuard &) = delete;

  private:
    Parser &parser_;
    bool prevAllowBindings_;
    bool prevAllowReturn_;
  };

  std::string currentNamespacePrefix() const;
  std::string makeFullPath(const std::string &name, const std::string &prefix) const;

  void skipComments();
  bool match(TokenKind kind);
  bool expect(TokenKind kind, const std::string &message);
  Token consume(TokenKind kind, const std::string &message);
  bool fail(const std::string &message);

  std::vector<Token> tokens_;
  size_t pos_ = 0;
  std::vector<std::string> namespaceStack_;
  std::string *error_ = nullptr;
  bool *returnTracker_ = nullptr;
  bool returnsVoidContext_ = false;
  bool allowImplicitVoidReturn_ = false;
  bool allowArgumentLabels_ = false;
  bool allowBareBindings_ = false;
  bool allowBraceBindings_ = true;
  bool allowBraceReturn_ = true;
  bool mathImportAll_ = false;
  std::unordered_set<std::string> mathImports_;
  bool allowSurfaceSyntax_ = true;
};

} // namespace primec
