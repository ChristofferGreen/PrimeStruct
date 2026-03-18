#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <utility>
#include <vector>

#include "SemanticsHelpers.h"
#include "primec/Semantics.h"

namespace primec::semantics {

class SemanticsValidator {
public:
  SemanticsValidator(const Program &program,
                     const std::string &entryPath,
                     std::string &error,
                     const std::vector<std::string> &defaultEffects,
                     const std::vector<std::string> &entryDefaultEffects,
                     SemanticDiagnosticInfo *diagnosticInfo,
                     bool collectDiagnostics);

  bool run();

private:
  bool buildDefinitionMaps();
  bool buildParameters();
  bool inferUnknownReturnKinds();
  bool validateTraitConstraints();
  bool validateStructLayouts();
  bool validateDefinitions();
  bool validateExecutions();
  void collectDefinitionIntraBodyCallDiagnostics(const Definition &def,
                                                 std::vector<SemanticDiagnosticRecord> &out);
  void collectExecutionIntraBodyCallDiagnostics(const Execution &exec,
                                                std::vector<SemanticDiagnosticRecord> &out);
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
  std::string formatUnknownCallTarget(const Expr &expr) const;
  std::string diagnosticCallTargetPath(const std::string &path) const;
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
  bool resolveStructFieldBinding(const Definition &structDef,
                                 const Expr &fieldStmt,
                                 BindingInfo &bindingOut);
  bool validateSoaVectorElementFieldEnvelopes(const std::string &typeArg, const std::string &namespacePrefix);
  bool isDropTrivialContainerElementType(const std::string &typeName,
                                         const std::string &namespacePrefix,
                                         const std::vector<std::string> *definitionTemplateArgs,
                                         std::unordered_set<std::string> &visitingStructs);
  bool isRelocationTrivialContainerElementType(const std::string &typeName,
                                               const std::string &namespacePrefix,
                                               const std::vector<std::string> *definitionTemplateArgs,
                                               std::unordered_set<std::string> &visitingStructs);
  bool validateVectorDiscardHelperElementType(const BindingInfo &binding,
                                              const std::string &helperName,
                                              const std::string &namespacePrefix,
                                              const std::vector<std::string> *definitionTemplateArgs);
  bool validateVectorIndexedRemovalHelperElementType(const BindingInfo &binding,
                                                     const std::string &helperName,
                                                     const std::string &namespacePrefix,
                                                     const std::vector<std::string> *definitionTemplateArgs);
  bool validateVectorRelocationHelperElementType(const BindingInfo &binding,
                                                 const std::string &helperName,
                                                 const std::string &namespacePrefix,
                                                 const std::vector<std::string> *definitionTemplateArgs);
  bool allowMathBareName(const std::string &name) const;
  bool hasAnyMathImport() const;
  bool isEntryArgsName(const std::string &name) const;
  bool isEntryArgsAccess(const Expr &expr) const;
  bool isEntryArgStringBinding(const std::unordered_map<std::string, BindingInfo> &locals, const Expr &expr) const;
  bool isBuiltinBlockCall(const Expr &expr) const;
  struct ValidationContext;
  ValidationContext buildDefinitionValidationContext(const Definition &def) const;
  ValidationContext buildExecutionValidationContext(const Execution &exec) const;
  ValidationContext snapshotValidationContext() const;
  void restoreValidationContext(ValidationContext context);
  void capturePrimarySpanIfUnset(int line, int column);
  void captureRelatedSpan(int line, int column, const std::string &label);
  void captureExprContext(const Expr &expr);
  void captureDefinitionContext(const Definition &def);
  void captureExecutionContext(const Execution &exec);
  bool collectDuplicateDefinitionDiagnostics();

  ReturnKind inferExprReturnKind(const Expr &expr,
                                const std::vector<ParameterInfo> &params,
                                const std::unordered_map<std::string, BindingInfo> &locals);
  bool inferDefinitionReturnKind(const Definition &def);
  struct DefinitionReturnInferenceState {
    ReturnKind inferred = ReturnKind::Unknown;
    std::string inferredStructPath;
    bool sawReturn = false;
  };
  bool recordDefinitionInferredReturn(const Definition &def,
                                      const Expr *valueExpr,
                                      const std::vector<ParameterInfo> &defParams,
                                      const std::unordered_map<std::string, BindingInfo> &activeLocals,
                                      DefinitionReturnInferenceState &state);
  bool inferDefinitionStatementReturns(const Definition &def,
                                       const std::vector<ParameterInfo> &defParams,
                                       const Expr &stmt,
                                       std::unordered_map<std::string, BindingInfo> &activeLocals,
                                       DefinitionReturnInferenceState &state);

  bool validateExpr(const std::vector<ParameterInfo> &params,
                    const std::unordered_map<std::string, BindingInfo> &locals,
                    const Expr &expr,
                    const std::vector<Expr> *enclosingStatements = nullptr,
                    size_t statementIndex = 0);
  bool isNumericExpr(const std::vector<ParameterInfo> &params,
                     const std::unordered_map<std::string, BindingInfo> &locals,
                     const Expr &expr);
  bool isFloatExpr(const std::vector<ParameterInfo> &params,
                   const std::unordered_map<std::string, BindingInfo> &locals,
                   const Expr &expr);
  bool isComparisonOperand(const std::vector<ParameterInfo> &params,
                           const std::unordered_map<std::string, BindingInfo> &locals,
                           const Expr &expr);
  std::string inferMatrixQuaternionTypePath(const Expr &expr,
                                            const std::vector<ParameterInfo> &params,
                                            const std::unordered_map<std::string, BindingInfo> &locals);
  std::string inferVectorTypePath(const Expr &expr,
                                  const std::vector<ParameterInfo> &params,
                                  const std::unordered_map<std::string, BindingInfo> &locals);
  bool isImplicitMatrixQuaternionConversion(const std::string &expectedTypePath,
                                            const std::string &actualTypePath) const;
  std::string implicitMatrixQuaternionConversionDiagnostic(const std::string &expectedTypePath,
                                                           const std::string &actualTypePath) const;
  bool validateNumericBuiltinExpr(const std::vector<ParameterInfo> &params,
                                  const std::unordered_map<std::string, BindingInfo> &locals,
                                  const Expr &expr,
                                  bool &handled);
  bool isIfBlockEnvelope(const Expr &expr) const;
  const Expr *getIfBlockEnvelopeValueExpr(const Expr &expr) const;
  bool isStructConstructorValueExpr(const Expr &expr);
  bool validateIfExpr(const std::vector<ParameterInfo> &params,
                      const std::unordered_map<std::string, BindingInfo> &locals,
                      const Expr &expr);
  bool validateBlockExpr(const std::vector<ParameterInfo> &params,
                         const std::unordered_map<std::string, BindingInfo> &locals,
                         const Expr &expr);
  bool validateLambdaExpr(const std::vector<ParameterInfo> &params,
                          const std::unordered_map<std::string, BindingInfo> &locals,
                          const Expr &expr,
                          const std::vector<Expr> *enclosingStatements,
                          size_t statementIndex);
  ReturnKind inferControlFlowExprReturnKind(const Expr &expr,
                                            const std::vector<ParameterInfo> &params,
                                            const std::unordered_map<std::string, BindingInfo> &locals,
                                            bool &handled);
  std::string normalizeCollectionTypePath(const std::string &typePath) const;
  bool hasImportedDefinitionPath(const std::string &path) const;
  bool inferDefinitionReturnBinding(const Definition &def, BindingInfo &bindingOut);
  bool resolveCallCollectionTypePath(const Expr &target,
                                     const std::vector<ParameterInfo> &params,
                                     const std::unordered_map<std::string, BindingInfo> &locals,
                                     std::string &typePathOut);
  bool resolveCallCollectionTemplateArgs(const Expr &target,
                                         const std::string &expectedBase,
                                         const std::vector<ParameterInfo> &params,
                                         const std::unordered_map<std::string, BindingInfo> &locals,
                                         std::vector<std::string> &argsOut);
  struct BuiltinCollectionDispatchResolvers {
    std::function<bool(const Expr &, std::string &)> resolveArgsPackAccessTarget;
    std::function<bool(const Expr &, std::string &)> resolveArrayTarget;
    std::function<bool(const Expr &, std::string &)> resolveVectorTarget;
    std::function<bool(const Expr &)> resolveStringTarget;
    std::function<bool(const Expr &, std::string &, std::string &)> resolveMapTarget;
  };
  struct BuiltinCollectionCountCapacityDispatchContext {
    bool isCountLike = false;
    bool isCapacityLike = false;
    bool isUnnamespacedMapCountFallbackCall = false;
    bool shouldInferBuiltinBareMapCountCall = false;
    std::function<bool(const std::string &, std::string &)> resolveMethodCallPath;
    std::function<std::string(const std::string &)> preferVectorStdlibHelperPath;
    std::function<bool(const std::string &)> hasDeclaredDefinitionPath;
    std::function<bool(const Expr &, std::string &, std::string &)> resolveMapTarget;
    const BuiltinCollectionDispatchResolvers *dispatchResolvers = nullptr;
  };
  struct BuiltinCollectionDirectCountCapacityContext {
    bool isDirectCountCall = false;
    bool isDirectCountSingleArg = false;
    bool isDirectCapacityCall = false;
    bool isDirectCapacitySingleArg = false;
    bool shouldInferBuiltinBareMapCountCall = false;
    std::function<bool(const std::string &, std::string &)> resolveMethodCallPath;
    std::function<std::string(const std::string &)> preferVectorStdlibHelperPath;
    std::function<bool(const std::string &)> hasDeclaredDefinitionPath;
    std::function<bool(const std::string &, ReturnKind &)> inferResolvedPathReturnKind;
    std::function<bool(const std::string &, Expr &)> tryRewriteBareMapHelperCall;
    std::function<ReturnKind(const Expr &)> inferRewrittenExprReturnKind;
    std::function<bool(const Expr &, std::string &)> resolveArgsPackCountTarget;
    std::function<bool(const Expr &, std::string &)> resolveVectorTarget;
    std::function<bool(const Expr &, std::string &)> resolveArrayTarget;
    std::function<bool(const Expr &)> resolveStringTarget;
    std::function<bool(const Expr &, std::string &, std::string &)> resolveMapTarget;
    const BuiltinCollectionDispatchResolvers *dispatchResolvers = nullptr;
  };
  bool resolveBuiltinCollectionMethodReturnKind(const std::string &resolvedPath,
                                                const Expr &receiverExpr,
                                                const BuiltinCollectionDispatchResolvers &resolvers,
                                                ReturnKind &kindOut) const;
  bool resolveBuiltinCollectionAccessCallReturnKind(const Expr &callExpr,
                                                    const BuiltinCollectionDispatchResolvers &resolvers,
                                                    ReturnKind &kindOut) const;
  bool resolveBuiltinCollectionCountCapacityReturnKind(const Expr &callExpr,
                                                       const BuiltinCollectionCountCapacityDispatchContext &context,
                                                       ReturnKind &kindOut);
  ReturnKind inferBuiltinCollectionDirectCountCapacityReturnKind(
      const Expr &expr,
      const std::string &resolved,
      const BuiltinCollectionDirectCountCapacityContext &context,
      bool &handled);
  bool validateStatement(const std::vector<ParameterInfo> &params,
                         std::unordered_map<std::string, BindingInfo> &locals,
                         const Expr &stmt,
                         ReturnKind returnKind,
                         bool allowReturn,
                         bool allowBindings,
                         bool *sawReturn,
                         const std::string &namespacePrefix,
                         const std::vector<Expr> *enclosingStatements = nullptr,
                         size_t statementIndex = 0);
  bool validateBindingStatement(const std::vector<ParameterInfo> &params,
                                std::unordered_map<std::string, BindingInfo> &locals,
                                const Expr &stmt,
                                bool allowBindings,
                                const std::string &namespacePrefix,
                                bool &handled);
  bool validateControlFlowStatement(const std::vector<ParameterInfo> &params,
                                    std::unordered_map<std::string, BindingInfo> &locals,
                                    const Expr &stmt,
                                    ReturnKind returnKind,
                                    bool allowReturn,
                                    bool allowBindings,
                                    bool *sawReturn,
                                    const std::string &namespacePrefix,
                                    const std::vector<Expr> *enclosingStatements,
                                    size_t statementIndex,
                                    bool &handled);
  bool validateVectorStatementHelper(const std::vector<ParameterInfo> &params,
                                     const std::unordered_map<std::string, BindingInfo> &locals,
                                     const Expr &stmt,
                                     const std::string &namespacePrefix,
                                     const std::vector<std::string> *definitionTemplateArgs,
                                     const std::vector<Expr> *enclosingStatements,
                                     size_t statementIndex,
                                     bool &handled);
  std::string preferVectorStdlibHelperPath(const std::string &path) const;
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
  struct ValidationContext {
    std::unordered_set<std::string> activeEffects;
    std::unordered_set<std::string> movedBindings;
    std::unordered_set<std::string> endedReferenceBorrows;
    std::string definitionPath;
    bool definitionIsCompute = false;
    bool definitionIsUnsafe = false;
    std::optional<ResultTypeInfo> resultType;
    std::optional<OnErrorHandler> onError;
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
  struct ValidationContextScope {
    SemanticsValidator &validator;
    ValidationContext previous;
    ValidationContextScope(SemanticsValidator &validatorIn, ValidationContext context)
        : validator(validatorIn), previous(validatorIn.snapshotValidationContext()) {
      validator.restoreValidationContext(std::move(context));
    }
    ~ValidationContextScope() {
      validator.restoreValidationContext(std::move(previous));
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
  struct ExprContextScope {
    SemanticsValidator &validator;
    const Expr *previous;
    const Expr &current;
    ExprContextScope(SemanticsValidator &validatorIn, const Expr &exprIn)
        : validator(validatorIn), previous(validatorIn.currentExprContext_), current(exprIn) {
      validator.currentExprContext_ = &exprIn;
    }
    ~ExprContextScope() {
      if (!validator.error_.empty()) {
        validator.captureExprContext(current);
      }
      validator.currentExprContext_ = previous;
    }
  };
  struct DefinitionContextScope {
    SemanticsValidator &validator;
    const Definition *previous;
    const Definition &current;
    DefinitionContextScope(SemanticsValidator &validatorIn, const Definition &defIn)
        : validator(validatorIn), previous(validatorIn.currentDefinitionContext_), current(defIn) {
      validator.currentDefinitionContext_ = &defIn;
    }
    ~DefinitionContextScope() {
      if (!validator.error_.empty()) {
        validator.captureDefinitionContext(current);
      }
      validator.currentDefinitionContext_ = previous;
    }
  };
  struct ExecutionContextScope {
    SemanticsValidator &validator;
    const Execution *previous;
    const Execution &current;
    ExecutionContextScope(SemanticsValidator &validatorIn, const Execution &execIn)
        : validator(validatorIn), previous(validatorIn.currentExecutionContext_), current(execIn) {
      validator.currentExecutionContext_ = &execIn;
    }
    ~ExecutionContextScope() {
      if (!validator.error_.empty()) {
        validator.captureExecutionContext(current);
      }
      validator.currentExecutionContext_ = previous;
    }
  };

  const Program &program_;
  const std::string &entryPath_;
  std::string &error_;
  const std::vector<std::string> &defaultEffects_;
  const std::vector<std::string> &entryDefaultEffects_;
  SemanticDiagnosticInfo *diagnosticInfo_ = nullptr;
  bool collectDiagnostics_ = false;

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
  const Expr *currentExprContext_ = nullptr;
  const Definition *currentDefinitionContext_ = nullptr;
  const Execution *currentExecutionContext_ = nullptr;
};

} // namespace primec::semantics
