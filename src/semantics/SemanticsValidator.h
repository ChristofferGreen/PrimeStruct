#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <utility>
#include <vector>

#include "SemanticsHelpers.h"

namespace primec::semantics {

class SemanticsValidator {
public:
  SemanticsValidator(const Program &program,
                     const std::string &entryPath,
                     std::string &error,
                     const std::vector<std::string> &defaultEffects,
                     const std::vector<std::string> &entryDefaultEffects);

  bool run();

private:
  bool buildDefinitionMaps();
  bool buildParameters();
  bool inferUnknownReturnKinds();
  bool validateTraitConstraints();
  bool validateStructLayouts();
  bool validateDefinitions();
  bool validateExecutions();
  bool validateEntry();
  std::optional<std::string> validateUninitializedDefiniteState(
      const std::vector<ParameterInfo> &params,
      const std::vector<Expr> &statements);
  bool validateOmittedBindingInitializer(const Expr &binding,
                                         const BindingInfo &info,
                                         const std::string &namespacePrefix);
  bool hasStructZeroArgConstructor(const std::string &structPath) const;
  bool isOutsideEffectFreeStructConstructor(const std::string &structPath);

  std::string resolveCalleePath(const Expr &expr) const;
  bool isParam(const std::vector<ParameterInfo> &params, const std::string &name) const;
  const BindingInfo *findParamBinding(const std::vector<ParameterInfo> &params, const std::string &name) const;
  std::string typeNameForReturnKind(ReturnKind kind) const;
  std::string inferStructReturnPath(const Expr &expr,
                                    const std::vector<ParameterInfo> &params,
                                    const std::unordered_map<std::string, BindingInfo> &locals);
  bool inferBindingTypeFromInitializer(const Expr &initializer,
                                       const std::vector<ParameterInfo> &params,
                                       const std::unordered_map<std::string, BindingInfo> &locals,
                                       BindingInfo &bindingOut);
  bool allowMathBareName(const std::string &name) const;
  bool hasAnyMathImport() const;
  bool isEntryArgsName(const std::string &name) const;
  bool isEntryArgsAccess(const Expr &expr) const;
  bool isEntryArgStringBinding(const std::unordered_map<std::string, BindingInfo> &locals, const Expr &expr) const;
  bool isBuiltinBlockCall(const Expr &expr) const;

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

  std::unordered_set<std::string> resolveEffects(const std::vector<Transform> &transforms, bool isEntry) const;
  bool validateCapabilitiesSubset(const std::vector<Transform> &transforms, const std::string &context);
  bool resolveExecutionEffects(const Expr &expr, std::unordered_set<std::string> &effectsOut);
  bool exprUsesName(const Expr &expr, const std::string &name) const;
  bool statementsUseNameFrom(const std::vector<Expr> &statements, size_t startIndex, const std::string &name) const;
  void expireReferenceBorrowsForRemainder(const std::vector<ParameterInfo> &params,
                                          const std::unordered_map<std::string, BindingInfo> &locals,
                                          const std::vector<Expr> &statements,
                                          size_t nextIndex);

  struct ResultTypeInfo {
    bool isResult = false;
    bool hasValue = false;
    std::string valueType;
    std::string errorType;
  };
  struct OnErrorHandler {
    std::string errorType;
    std::string handlerPath;
    std::vector<Expr> boundArgs;
  };

  bool parseTransformArgumentExpr(const std::string &text, const std::string &namespacePrefix, Expr &out);
  bool resolveResultTypeFromTypeName(const std::string &typeName, ResultTypeInfo &out) const;
  bool resolveResultTypeFromTemplateArg(const std::string &templateArg, ResultTypeInfo &out) const;
  bool resolveResultTypeForExpr(const Expr &expr,
                                const std::vector<ParameterInfo> &params,
                                const std::unordered_map<std::string, BindingInfo> &locals,
                                ResultTypeInfo &out) const;
  bool parseOnErrorTransform(const std::vector<Transform> &transforms,
                             const std::string &namespacePrefix,
                             const std::string &context,
                             std::optional<OnErrorHandler> &out);
  bool errorTypesMatch(const std::string &left, const std::string &right, const std::string &namespacePrefix) const;
  bool resolveUninitializedStorageBinding(const std::vector<ParameterInfo> &params,
                                          const std::unordered_map<std::string, BindingInfo> &locals,
                                          const Expr &storage,
                                          BindingInfo &bindingOut,
                                          bool &resolvedOut);

  struct EffectFreeSummary {
    bool effectFree = false;
    bool writesThis = false;
  };
  struct EffectFreeContext {
    const std::vector<ParameterInfo> *params = nullptr;
    std::unordered_map<std::string, BindingInfo> locals;
    std::unordered_set<std::string> paramNames;
    std::string thisName;
  };

  EffectFreeSummary effectFreeSummaryForDefinition(const Definition &def);
  bool isOutsideEffectFreeStatement(const Expr &stmt, EffectFreeContext &ctx, bool &writesThis);
  bool isOutsideEffectFreeExpr(const Expr &expr, EffectFreeContext &ctx, bool &writesThis);

  struct EffectScope {
    SemanticsValidator &validator;
    std::unordered_set<std::string> previous;
    EffectScope(SemanticsValidator &validatorIn, std::unordered_set<std::string> next)
        : validator(validatorIn), previous(std::move(validatorIn.activeEffects_)) {
      validator.activeEffects_ = std::move(next);
    }
    ~EffectScope() {
      validator.activeEffects_ = std::move(previous);
    }
  };
  struct EntryArgStringScope {
    SemanticsValidator &validator;
    bool previous = false;
    EntryArgStringScope(SemanticsValidator &validatorIn, bool allow)
        : validator(validatorIn), previous(validatorIn.allowEntryArgStringUse_) {
      validator.allowEntryArgStringUse_ = allow;
    }
    ~EntryArgStringScope() {
      validator.allowEntryArgStringUse_ = previous;
    }
  };
  struct OnErrorScope {
    SemanticsValidator &validator;
    std::optional<OnErrorHandler> previous;
    OnErrorScope(SemanticsValidator &validatorIn, std::optional<OnErrorHandler> next)
        : validator(validatorIn), previous(std::move(validatorIn.currentOnError_)) {
      validator.currentOnError_ = std::move(next);
    }
    ~OnErrorScope() {
      validator.currentOnError_ = std::move(previous);
    }
  };
  struct MovedScope {
    SemanticsValidator &validator;
    std::unordered_set<std::string> previous;
    MovedScope(SemanticsValidator &validatorIn, std::unordered_set<std::string> next)
        : validator(validatorIn), previous(std::move(validatorIn.movedBindings_)) {
      validator.movedBindings_ = std::move(next);
    }
    ~MovedScope() {
      validator.movedBindings_ = std::move(previous);
    }
  };
  struct BorrowEndScope {
    SemanticsValidator &validator;
    std::unordered_set<std::string> previous;
    BorrowEndScope(SemanticsValidator &validatorIn, std::unordered_set<std::string> next)
        : validator(validatorIn), previous(std::move(validatorIn.endedReferenceBorrows_)) {
      validator.endedReferenceBorrows_ = std::move(next);
    }
    ~BorrowEndScope() {
      validator.endedReferenceBorrows_ = std::move(previous);
    }
  };

  const Program &program_;
  const std::string &entryPath_;
  std::string &error_;
  const std::vector<std::string> &defaultEffects_;
  const std::vector<std::string> &entryDefaultEffects_;

  std::unordered_set<std::string> defaultEffectSet_;
  std::unordered_set<std::string> entryDefaultEffectSet_;
  std::unordered_map<std::string, const Definition *> defMap_;
  std::unordered_map<std::string, ReturnKind> returnKinds_;
  std::unordered_map<std::string, std::string> returnStructs_;
  std::unordered_set<std::string> structNames_;
  std::unordered_set<std::string> publicDefinitions_;
  std::unordered_map<std::string, std::vector<ParameterInfo>> paramsByDef_;
  std::unordered_set<std::string> activeEffects_;
  std::unordered_set<std::string> movedBindings_;
  std::unordered_set<std::string> endedReferenceBorrows_;
  std::unordered_set<std::string> inferenceStack_;
  std::unordered_map<std::string, std::string> importAliases_;
  std::unordered_map<std::string, EffectFreeSummary> effectFreeDefCache_;
  std::unordered_set<std::string> effectFreeDefStack_;
  std::unordered_map<std::string, bool> effectFreeStructCache_;
  std::unordered_set<std::string> effectFreeStructStack_;
  bool mathImportAll_ = false;
  std::unordered_set<std::string> mathImports_;
  std::string entryArgsName_;
  std::string currentDefinitionPath_;
  bool currentDefinitionIsCompute_ = false;
  bool currentDefinitionIsUnsafe_ = false;
  bool allowEntryArgStringUse_ = false;
  std::optional<ResultTypeInfo> currentResultType_;
  std::optional<OnErrorHandler> currentOnError_;
};

} // namespace primec::semantics
