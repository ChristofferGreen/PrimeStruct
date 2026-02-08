#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "SemanticsHelpers.h"

namespace primec::semantics {

class SemanticsValidator {
public:
  SemanticsValidator(const Program &program,
                     const std::string &entryPath,
                     std::string &error,
                     const std::vector<std::string> &defaultEffects);

  bool run();

private:
  bool buildDefinitionMaps();
  bool buildParameters();
  bool inferUnknownReturnKinds();
  bool validateDefinitions();
  bool validateExecutions();
  bool validateEntry();

  std::string resolveCalleePath(const Expr &expr) const;
  bool isParam(const std::vector<ParameterInfo> &params, const std::string &name) const;
  const BindingInfo *findParamBinding(const std::vector<ParameterInfo> &params, const std::string &name) const;
  std::string typeNameForReturnKind(ReturnKind kind) const;

  ReturnKind inferExprReturnKind(const Expr &expr,
                                const std::vector<ParameterInfo> &params,
                                const std::unordered_map<std::string, BindingInfo> &locals);
  bool inferDefinitionReturnKind(const Definition &def);

  bool validateExpr(const std::vector<ParameterInfo> &params,
                    const std::unordered_map<std::string, BindingInfo> &locals,
                    const Expr &expr);
  bool validateStatement(const std::vector<ParameterInfo> &params,
                         std::unordered_map<std::string, BindingInfo> &locals,
                         const Expr &stmt,
                         ReturnKind returnKind,
                         bool allowReturn,
                         bool allowBindings,
                         bool *sawReturn,
                         const std::string &namespacePrefix);
  bool statementAlwaysReturns(const Expr &stmt);
  bool blockAlwaysReturns(const std::vector<Expr> &statements);

  std::unordered_set<std::string> resolveEffects(const std::vector<Transform> &transforms) const;
  bool validateCapabilitiesSubset(const std::vector<Transform> &transforms, const std::string &context);

  const Program &program_;
  const std::string &entryPath_;
  std::string &error_;
  const std::vector<std::string> &defaultEffects_;

  std::unordered_set<std::string> defaultEffectSet_;
  std::unordered_map<std::string, const Definition *> defMap_;
  std::unordered_map<std::string, ReturnKind> returnKinds_;
  std::unordered_set<std::string> structNames_;
  std::unordered_map<std::string, std::vector<ParameterInfo>> paramsByDef_;
  std::unordered_set<std::string> activeEffects_;
  std::unordered_set<std::string> inferenceStack_;
};

} // namespace primec::semantics
