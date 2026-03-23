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

struct TypeResolutionGraph;

class SemanticsValidator {
public:
  SemanticsValidator(const Program &program,
                     const std::string &entryPath,
                     std::string &error,
                     const std::vector<std::string> &defaultEffects,
                     const std::vector<std::string> &entryDefaultEffects,
                     SemanticDiagnosticInfo *diagnosticInfo,
                     bool collectDiagnostics,
                     const std::string &typeResolver);

  bool run();

private:
  bool buildDefinitionMaps();
  bool validateDefinitionBuildTransforms(const Definition &def,
                                         bool isStructHelper,
                                         bool isLifecycleHelper,
                                         std::unordered_set<std::string> &explicitStructs,
                                         std::vector<SemanticDiagnosticRecord> *transformDiagnosticRecords,
                                         bool &definitionTransformError);
  bool buildImportAliases();
  std::string resolveStructReturnPathForBuild(const std::string &typeName,
                                              const std::string &namespacePrefix) const;
  bool buildDefinitionReturnKinds(const std::unordered_set<std::string> &explicitStructs);
  bool validateLifecycleHelperDefinitions();
  bool buildParameters();
  bool inferUnknownReturnKinds();
  bool inferUnknownReturnKindsLegacy();
  bool inferUnknownReturnKindsGraph();
  void collectGraphLocalAutoBindings(const TypeResolutionGraph &graph);
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
  ReturnKind combineInferredNumericKinds(ReturnKind left, ReturnKind right) const;
  bool isInferStructTypeName(const std::string &typeName, const std::string &namespacePrefix) const;
  ReturnKind inferReferenceTargetKind(const std::string &templateArg, const std::string &namespacePrefix) const;
  ReturnKind inferUninitializedTargetKind(const std::string &templateArg,
                                          const std::string &namespacePrefix) const;
  bool resolveInferArgsPackCountTarget(const std::vector<ParameterInfo> &params,
                                       const std::unordered_map<std::string, BindingInfo> &locals,
                                       const Expr &target,
                                       std::string &elemType) const;
  bool extractInferExperimentalMapFieldTypes(const BindingInfo &binding,
                                             std::string &keyTypeOut,
                                             std::string &valueTypeOut) const;
  bool resolveInferExperimentalMapTarget(const std::vector<ParameterInfo> &params,
                                         const std::unordered_map<std::string, BindingInfo> &locals,
                                         const Expr &target,
                                         std::string &keyTypeOut,
                                         std::string &valueTypeOut);
  bool resolveInferExperimentalMapValueTarget(const std::vector<ParameterInfo> &params,
                                              const std::unordered_map<std::string, BindingInfo> &locals,
                                              const Expr &target,
                                              std::string &keyTypeOut,
                                              std::string &valueTypeOut);
  bool resolveInferMethodCallPath(const Expr &expr,
                                  const std::vector<ParameterInfo> &params,
                                  const std::unordered_map<std::string, BindingInfo> &locals,
                                  const std::string &methodName,
                                  std::string &resolvedOut);
  bool inferBindingTypeFromInitializer(const Expr &initializer,
                                       const std::vector<ParameterInfo> &params,
                                       const std::unordered_map<std::string, BindingInfo> &locals,
                                       BindingInfo &bindingOut,
                                       const Expr *bindingExpr = nullptr);
  static std::string graphLocalAutoBindingKey(const std::string &scopePath, int sourceLine, int sourceColumn);
  static std::pair<int, int> graphLocalAutoSourceLocation(const Expr &expr);
  bool lookupGraphLocalAutoBinding(const std::string &scopePath,
                                   const Expr &bindingExpr,
                                   BindingInfo &bindingOut) const;
  bool lookupGraphLocalAutoBinding(const Expr &bindingExpr, BindingInfo &bindingOut) const;
  bool inferResolvedDirectCallBindingType(const std::string &resolvedPath, BindingInfo &bindingOut) const;
  bool resolveStructFieldBinding(const Definition &structDef,
                                 const Expr &fieldStmt,
                                 BindingInfo &bindingOut);
  bool resolveStructFieldReceiverPath(const std::vector<ParameterInfo> &params,
                                      const std::unordered_map<std::string, BindingInfo> &locals,
                                      const Expr &receiver,
                                      std::string &structPathOut);
  bool isTypeNamespaceFieldReceiver(const std::vector<ParameterInfo> &params,
                                    const std::unordered_map<std::string, BindingInfo> &locals,
                                    const Expr &receiver,
                                    std::string &structPathOut);
  bool resolveStructFieldBinding(const std::vector<ParameterInfo> &params,
                                 const std::unordered_map<std::string, BindingInfo> &locals,
                                 const Expr &receiver,
                                 const std::string &fieldName,
                                 BindingInfo &bindingOut);
  bool isTypeNamespaceMethodCall(const std::vector<ParameterInfo> &params,
                                 const std::unordered_map<std::string, BindingInfo> &locals,
                                 const Expr &callExpr,
                                 const std::string &resolvedPath) const;
  std::string describeMethodReflectionTarget(const std::vector<ParameterInfo> &params,
                                             const std::unordered_map<std::string, BindingInfo> &locals,
                                             const Expr &callExpr) const;
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
  bool isStringStatementExpr(const Expr &arg,
                             const std::vector<ParameterInfo> &params,
                             const std::unordered_map<std::string, BindingInfo> &locals);
  bool isPrintableStatementExpr(const Expr &arg,
                                const std::vector<ParameterInfo> &params,
                                const std::unordered_map<std::string, BindingInfo> &locals);
  bool allowMathBareName(const std::string &name) const;
  bool hasAnyMathImport() const;
  bool isEntryArgsName(const std::string &name) const;
  bool isEntryArgsAccess(const Expr &expr) const;
  bool isEntryArgStringBinding(const std::unordered_map<std::string, BindingInfo> &locals, const Expr &expr) const;
  bool isBuiltinBlockCall(const Expr &expr) const;
  struct ValidationContext;
  ValidationContext makeDefinitionValidationContext(const Definition &def) const;
  ValidationContext makeExecutionValidationContext(const Execution &exec) const;
  const ValidationContext &buildDefinitionValidationContext(const Definition &def) const;
  const ValidationContext &buildExecutionValidationContext(const Execution &exec) const;
  void capturePrimarySpanIfUnset(int line, int column);
  void captureRelatedSpan(int line, int column, const std::string &label);
  void captureExprContext(const Expr &expr);
  void captureDefinitionContext(const Definition &def);
  void captureExecutionContext(const Execution &exec);
  bool collectDuplicateDefinitionDiagnostics();

  ReturnKind inferExprReturnKind(const Expr &expr,
                                const std::vector<ParameterInfo> &params,
                                const std::unordered_map<std::string, BindingInfo> &locals);
  bool ensureDefinitionReturnKindReady(const Definition &def);
  bool inferDefinitionReturnKind(const Definition &def);
  struct DefinitionReturnInferenceState {
    ReturnKind inferred = ReturnKind::Unknown;
    std::string inferredStructPath;
    bool sawReturn = false;
    bool sawUnresolvedReturnDependency = false;
  };
  bool inferDefinitionReturnKindGraphStep(const Definition &def, bool finalize, bool componentHasCycle, bool &changed);
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
  struct BuiltinCollectionDispatchResolverAdapters;
  struct BuiltinCollectionDispatchResolvers;

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
  bool validateExprBodyArguments(const std::vector<ParameterInfo> &params,
                                 const std::unordered_map<std::string, BindingInfo> &locals,
                                 const Expr &expr,
                                 std::string &resolved,
                                 bool &resolvedMethod,
                                 const BuiltinCollectionDispatchResolverAdapters &builtinCollectionDispatchResolverAdapters,
                                 const std::vector<Expr> *enclosingStatements,
                                 size_t statementIndex);
  bool isUnsafeReferenceExpr(const std::vector<ParameterInfo> &params,
                             const std::unordered_map<std::string, BindingInfo> &locals,
                             const Expr &expr);
  bool resolveEscapingReferenceRoot(const std::vector<ParameterInfo> &params,
                                    const std::unordered_map<std::string, BindingInfo> &locals,
                                    const Expr &expr,
                                    std::string &rootOut);
  bool reportReferenceAssignmentEscape(const std::vector<ParameterInfo> &params,
                                       const std::unordered_map<std::string, BindingInfo> &locals,
                                       const std::string &sinkName,
                                       const Expr &rhsExpr);
  struct ExprResultFileBuiltinContext {
    std::function<bool(const Expr &)> isNamedArgsPackWrappedFileBuiltinAccessCall;
    std::function<bool(const Expr &)> isStringExpr;
  };
  bool validateExprResultFileBuiltins(const std::vector<ParameterInfo> &params,
                                      const std::unordered_map<std::string, BindingInfo> &locals,
                                      const Expr &expr,
                                      const std::string &resolved,
                                      bool resolvedMethod,
                                      const ExprResultFileBuiltinContext &context,
                                      bool &handledOut);
  struct ExprTryBuiltinContext {
    std::function<std::string(const Expr &)> getDirectMapHelperCompatibilityPath;
    std::function<bool(const Expr &)> isIndexedArgsPackMapReceiverTarget;
  };
  bool validateExprTryBuiltin(const std::vector<ParameterInfo> &params,
                              const std::unordered_map<std::string, BindingInfo> &locals,
                              const Expr &expr,
                              const ExprTryBuiltinContext &context,
                              bool &handledOut);
  struct ExprLateBuiltinContext {
    ExprTryBuiltinContext tryBuiltinContext;
    ExprResultFileBuiltinContext resultFileBuiltinContext;
  };
  void prepareExprLateBuiltinContext(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const BuiltinCollectionDispatchResolverAdapters &dispatchResolverAdapters,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers,
      ExprLateBuiltinContext &contextOut);
  bool validateExprLateBuiltins(const std::vector<ParameterInfo> &params,
                                const std::unordered_map<std::string, BindingInfo> &locals,
                                const Expr &expr,
                                const std::string &resolved,
                                bool resolvedMethod,
                                const ExprLateBuiltinContext &context,
                                bool &handledOut);
  struct ExprCountCapacityMapBuiltinContext {
    bool shouldBuiltinValidateStdNamespacedVectorCountCall = false;
    bool isStdNamespacedVectorCountCall = false;
    bool shouldBuiltinValidateBareMapCountCall = false;
    bool isNamespacedMapCountCall = false;
    bool isResolvedMapCountCall = false;
    bool shouldBuiltinValidateStdNamespacedVectorCapacityCall = false;
    bool isStdNamespacedVectorCapacityCall = false;
    std::function<bool(const Expr &, std::string &)> resolveVectorTarget;
    std::function<bool(const Expr &)> resolveMapTarget;
    const BuiltinCollectionDispatchResolverAdapters *dispatchResolverAdapters = nullptr;
    const BuiltinCollectionDispatchResolvers *dispatchResolvers = nullptr;
  };
  bool validateExprCountCapacityMapBuiltins(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const std::string &resolved,
      bool resolvedMethod,
      const ExprCountCapacityMapBuiltinContext &context,
      bool &handledOut);
  void prepareExprCountCapacityMapBuiltinContext(
      bool shouldBuiltinValidateStdNamespacedVectorCountCall,
      bool isStdNamespacedVectorCountCall,
      bool shouldBuiltinValidateBareMapCountCall,
      bool isNamespacedMapCountCall,
      bool isResolvedMapCountCall,
      bool shouldBuiltinValidateStdNamespacedVectorCapacityCall,
      bool isStdNamespacedVectorCapacityCall,
      const BuiltinCollectionDispatchResolverAdapters &dispatchResolverAdapters,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers,
      ExprCountCapacityMapBuiltinContext &contextOut);
  struct ExprNamedArgumentBuiltinContext {
    bool hasVectorHelperCallResolution = false;
    std::function<bool(const Expr &)> isNamedArgsPackMethodAccessCall;
    std::function<bool(const Expr &)> isNamedArgsPackWrappedFileBuiltinAccessCall;
    std::function<bool(const Expr &)> isArrayNamespacedVectorCountCompatibilityCall;
  };
  void prepareExprNamedArgumentBuiltinContext(
      bool hasVectorHelperCallResolution,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers,
      ExprNamedArgumentBuiltinContext &contextOut);
  bool validateExprNamedArguments(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const std::string &resolved,
      bool resolvedMethod,
      const ExprNamedArgumentBuiltinContext &context);
  bool validateExprNamedArgumentBuiltins(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const std::string &resolved,
      bool resolvedMethod,
      const ExprNamedArgumentBuiltinContext &context);
  bool validateExprGpuBufferBuiltins(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      bool &handledOut);
  struct ExprMapSoaBuiltinContext {
    bool shouldBuiltinValidateBareMapContainsCall = false;
    std::function<bool(const Expr &, std::string &)> resolveMapKeyType;
    std::function<bool(const Expr &, std::string &)> resolveVectorTarget;
    std::function<bool(const Expr &, std::string &)> resolveSoaVectorTarget;
    std::function<bool(const Expr &)> resolveStringTarget;
    std::function<bool(const Expr &, size_t &, size_t &)>
        bareMapHelperOperandIndices;
    std::function<bool(const Expr &)> isNamedArgsPackMethodAccessCall;
    std::function<bool(const Expr &)> isNamedArgsPackWrappedFileBuiltinAccessCall;
  };
  bool validateExprMapSoaBuiltins(const std::vector<ParameterInfo> &params,
                                  const std::unordered_map<std::string, BindingInfo> &locals,
                                  const Expr &expr,
                                  const std::string &resolved,
                                  bool resolvedMethod,
                                  const ExprMapSoaBuiltinContext &context,
                                  bool &handledOut);
  struct ExprLateMapSoaBuiltinContext {
    bool shouldBuiltinValidateBareMapContainsCall = false;
    std::function<bool(const Expr &, std::string &)> resolveVectorTarget;
    std::function<bool(const Expr &, std::string &)> resolveSoaVectorTarget;
    const BuiltinCollectionDispatchResolvers *dispatchResolvers = nullptr;
  };
  bool validateExprLateMapSoaBuiltins(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const std::string &resolved,
      bool resolvedMethod,
      const ExprLateMapSoaBuiltinContext &context,
      bool &handledOut);
  void prepareExprLateMapSoaBuiltinContext(
      bool shouldBuiltinValidateBareMapContainsCall,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers,
      ExprLateMapSoaBuiltinContext &contextOut);
  struct ExprLateCollectionAccessFallbackContext {
    bool isStdNamespacedVectorAccessCall = false;
    bool shouldAllowStdAccessCompatibilityFallback = false;
    bool hasStdNamespacedVectorAccessDefinition = false;
    bool isStdNamespacedMapAccessCall = false;
    bool hasStdNamespacedMapAccessDefinition = false;
    bool shouldBuiltinValidateBareMapAccessCall = false;
    std::function<bool(const std::string &)> isNonCollectionStructAccessTarget;
    const BuiltinCollectionDispatchResolvers *dispatchResolvers = nullptr;
  };
  bool validateExprLateCollectionAccessFallbacks(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const std::string &resolved,
      bool resolvedMethod,
      const ExprLateCollectionAccessFallbackContext &context,
      bool &handledOut);
  struct ExprLateFallbackBuiltinContext {
    ExprLateCollectionAccessFallbackContext collectionAccessFallbackContext;
    const BuiltinCollectionDispatchResolvers *dispatchResolvers = nullptr;
  };
  bool validateExprLateFallbackBuiltins(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const std::string &resolved,
      bool resolvedMethod,
      const ExprLateFallbackBuiltinContext &context,
      bool &handledOut);
  void prepareExprLateFallbackBuiltinContext(
      bool isStdNamespacedVectorAccessCall,
      bool shouldAllowStdAccessCompatibilityFallback,
      bool hasStdNamespacedVectorAccessDefinition,
      bool isStdNamespacedMapAccessCall,
      bool hasStdNamespacedMapAccessDefinition,
      bool shouldBuiltinValidateBareMapAccessCall,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers,
      ExprLateFallbackBuiltinContext &contextOut);
  bool validateExprMutationBorrowBuiltins(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      bool &handledOut);
  struct ExprLateCallCompatibilityContext {
    const BuiltinCollectionDispatchResolvers *dispatchResolvers = nullptr;
  };
  void prepareExprLateCallCompatibilityContext(
      const BuiltinCollectionDispatchResolvers &dispatchResolvers,
      ExprLateCallCompatibilityContext &contextOut);
  bool validateExprLateCallCompatibility(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const std::string &resolved,
      const ExprLateCallCompatibilityContext &context,
      bool &handledOut);
  struct ExprMethodResolutionContext {
    bool hasVectorHelperCallResolution = false;
    std::function<void(const Expr &, std::string &, bool &, bool)> promoteCapacityToBuiltinValidation;
    std::function<std::string(const std::string &)> unavailableMethodDiagnostic;
  };
  bool validateExprMethodCallTarget(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const ExprMethodResolutionContext &context,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers,
      const BuiltinCollectionDispatchResolverAdapters &dispatchResolverAdapters,
      std::string &resolved,
      bool &resolvedMethod,
      bool &usedMethodTarget,
      bool &hasMethodReceiverIndex,
      size_t &methodReceiverIndex);
  struct ExprArgumentValidationContext;
  struct ExprLateMapAccessBuiltinContext {
    const BuiltinCollectionDispatchResolvers *dispatchResolvers = nullptr;
    bool shouldBuiltinValidateBareMapContainsCall = false;
    bool shouldBuiltinValidateBareMapTryAtCall = false;
    bool shouldBuiltinValidateBareMapAccessCall = false;
  };
  void prepareExprLateMapAccessBuiltinContext(
      const BuiltinCollectionDispatchResolvers &dispatchResolvers,
      bool shouldBuiltinValidateBareMapContainsCall,
      bool shouldBuiltinValidateBareMapTryAtCall,
      bool shouldBuiltinValidateBareMapAccessCall,
      ExprLateMapAccessBuiltinContext &contextOut);
  bool validateExprLateMapAccessBuiltins(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const std::string &resolved,
      const ExprLateMapAccessBuiltinContext &context,
      bool &handledOut);
  struct ExprLateUnknownTargetFallbackContext {
    std::function<bool(const Expr &)> resolveMapTarget;
  };
  bool validateExprLateUnknownTargetFallbacks(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const ExprLateUnknownTargetFallbackContext &context,
      bool &handledOut);
  struct ExprResolvedStructConstructorContext {
    bool isResolvedStructConstructorCall = false;
    const Definition *resolvedDefinition = nullptr;
    const ExprArgumentValidationContext *argumentValidationContext = nullptr;
    const std::string *diagnosticResolved = nullptr;
    const std::string *zeroArgDiagnostic = nullptr;
  };
  bool validateExprResolvedStructConstructorCall(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const std::string &resolved,
      const ExprResolvedStructConstructorContext &context,
      bool &handledOut);
  struct ExprResolvedCallArgumentContext {
    const Definition *resolvedDefinition = nullptr;
    const std::vector<ParameterInfo> *calleeParams = nullptr;
    const ExprArgumentValidationContext *argumentValidationContext = nullptr;
    const std::string *diagnosticResolved = nullptr;
    bool hasMethodReceiverIndex = false;
    size_t methodReceiverIndex = 0;
  };
  bool validateExprResolvedCallArguments(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const std::string &resolved,
      const ExprResolvedCallArgumentContext &context,
      bool &handledOut);
  struct ExprCollectionAccessValidationContext {
    bool isStdNamespacedVectorAccessCall = false;
    bool shouldAllowStdAccessCompatibilityFallback = false;
    bool hasStdNamespacedVectorAccessDefinition = false;
    bool isStdNamespacedMapAccessCall = false;
    bool hasStdNamespacedMapAccessDefinition = false;
    bool shouldBuiltinValidateBareMapAccessCall = false;
    std::function<bool(const Expr &, std::string &)> resolveArgsPackAccessTarget;
    std::function<bool(const Expr &, std::string &)> resolveVectorTarget;
    std::function<bool(const Expr &, std::string &)> resolveExperimentalVectorValueTarget;
    std::function<bool(const Expr &, std::string &)> resolveArrayTarget;
    std::function<bool(const Expr &)> resolveStringTarget;
    std::function<bool(const Expr &, std::string &)> resolveMapKeyType;
    std::function<bool(const Expr &, std::string &, std::string &)>
        resolveExperimentalMapTarget;
    std::function<bool(const Expr &)> isIndexedArgsPackMapReceiverTarget;
    std::function<bool(const Expr &)> isNamedArgsPackMethodAccessCall;
    std::function<bool(const Expr &)> isNamedArgsPackWrappedFileBuiltinAccessCall;
    std::function<bool(const Expr &)> isMapLikeBareAccessReceiverTarget;
    std::function<bool(const std::string &)> isNonCollectionStructAccessTarget;
    std::function<bool(const Expr &, const std::string &, Expr &)> tryRewriteBareMapHelperCall;
  };
  bool validateExprCollectionAccessFallbacks(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const std::string &resolved,
      bool resolvedMethod,
      const ExprCollectionAccessValidationContext &context,
      bool &handledOut);
  bool validateExprScalarPointerMemoryBuiltins(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      bool &handledOut);
  bool validateExprCollectionLiteralBuiltins(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      bool &handledOut);
  bool validateExprFieldAccess(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr);
  std::string normalizeCollectionMethodName(const std::string &methodName) const;
  std::string inferPointerLikeCallReturnType(
      const Expr &receiverExpr,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals) const;
  bool resolvePointerLikeMethodTarget(const std::vector<ParameterInfo> &params,
                                      const std::unordered_map<std::string, BindingInfo> &locals,
                                      const Expr &receiverExpr,
                                      const std::string &methodName,
                                      std::string &resolvedOut);
  ReturnKind inferControlFlowExprReturnKind(const Expr &expr,
                                            const std::vector<ParameterInfo> &params,
                                            const std::unordered_map<std::string, BindingInfo> &locals,
                                            bool &handled);
  std::string normalizeCollectionTypePath(const std::string &typePath) const;
  bool hasDefinitionPath(const std::string &path) const;
  bool hasImportedDefinitionPath(const std::string &path) const;
  bool isStaticHelperDefinition(const Definition &def) const;
  bool hasDeclaredDefinitionPath(const std::string &path) const;
  bool resolveReferenceEscapeSink(const std::vector<ParameterInfo> &params,
                                  const std::unordered_map<std::string, BindingInfo> &locals,
                                  const std::string &targetName,
                                  std::string &sinkOut);
  bool resolveMethodTarget(const std::vector<ParameterInfo> &params,
                           const std::unordered_map<std::string, BindingInfo> &locals,
                           const std::string &callNamespacePrefix,
                           const Expr &receiver,
                           const std::string &methodName,
                           std::string &resolvedOut,
                           bool &isBuiltinOut);
  bool isVectorBuiltinName(const Expr &candidate, const char *helper) const;
  bool getVectorStatementHelperName(const Expr &candidate, std::string &nameOut) const;
  bool resolveVectorHelperMethodTarget(const std::vector<ParameterInfo> &params,
                                       const std::unordered_map<std::string, BindingInfo> &locals,
                                       const Expr &receiver,
                                       const std::string &helperName,
                                       std::string &resolvedOut);
  std::string getDirectVectorHelperCompatibilityPath(const Expr &candidate) const;
  bool resolveExprVectorHelperCall(const std::vector<ParameterInfo> &params,
                                   const std::unordered_map<std::string, BindingInfo> &locals,
                                   const Expr &expr,
                                   bool &hasResolutionOut,
                                   std::string &resolvedPathOut,
                                   size_t &receiverIndexOut);
  struct ExprCollectionDispatchSetup {
    bool isNamespacedVectorHelperCall = false;
    bool isStdNamespacedVectorCountCall = false;
    bool shouldBuiltinValidateStdNamespacedVectorCountCall = false;
    bool isStdNamespacedMapCountCall = false;
    bool isNamespacedMapHelperCall = false;
    std::string namespacedHelper;
    bool isNamespacedVectorCountCall = false;
    bool isNamespacedMapCountCall = false;
    bool isUnnamespacedMapCountFallbackCall = false;
    bool isResolvedMapCountCall = false;
    bool prefersCanonicalVectorCountAliasDefinition = false;
    bool isStdNamespacedVectorCapacityCall = false;
    bool shouldBuiltinValidateStdNamespacedVectorCapacityCall = false;
    bool isNamespacedVectorCapacityCall = false;
    bool isDirectStdNamespacedVectorCountWrapperMapTarget = false;
    bool hasStdNamespacedVectorCountAliasDefinition = false;
    bool isStdNamespacedVectorAccessCall = false;
    bool hasStdNamespacedVectorAccessDefinition = false;
    bool isStdNamespacedMapAccessCall = false;
    bool hasStdNamespacedMapAccessDefinition = false;
    bool shouldAllowStdAccessCompatibilityFallback = false;
  };
  bool prepareExprCollectionDispatchSetup(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers,
      const BuiltinCollectionDispatchResolverAdapters &dispatchResolverAdapters,
      std::string &resolved,
      ExprCollectionDispatchSetup &setupOut);
  struct ExprDirectCollectionFallbackContext {
    bool isStdNamespacedVectorCountCall = false;
    const BuiltinCollectionDispatchResolvers *dispatchResolvers = nullptr;
  };
  bool validateExprDirectCollectionFallbacks(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const std::string &resolved,
      const ExprDirectCollectionFallbackContext &context,
      std::optional<Expr> &rewrittenExprOut);
  struct ExprCollectionAccessDispatchContext {
    bool isNamespacedVectorHelperCall = false;
    bool isNamespacedMapHelperCall = false;
    std::string namespacedHelper;
    bool shouldBuiltinValidateBareMapContainsCall = false;
    bool shouldBuiltinValidateBareMapAccessCall = false;
    std::function<bool(const Expr &, std::string &)> resolveArrayTarget;
    std::function<bool(const Expr &, std::string &)> resolveVectorTarget;
    std::function<bool(const Expr &, std::string &)> resolveSoaVectorTarget;
    std::function<bool(const Expr &)> resolveStringTarget;
    std::function<bool(const Expr &)> resolveMapTarget;
    std::function<bool(const std::string &)> hasResolvableMapHelperPath;
    std::function<bool(const Expr &)> isIndexedArgsPackMapReceiverTarget;
  };
  void prepareExprCollectionAccessDispatchContext(
      const ExprCollectionDispatchSetup &dispatchSetup,
      bool shouldBuiltinValidateBareMapContainsCall,
      bool shouldBuiltinValidateBareMapAccessCall,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers,
      const std::function<bool(const Expr &)> &resolveMapTarget,
      ExprCollectionAccessDispatchContext &contextOut);
  bool validateExprPostAccessPrechecks(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      std::string &resolved,
      bool &resolvedMethod,
      bool usedMethodTarget,
      const BuiltinCollectionDispatchResolverAdapters &dispatchResolverAdapters,
      const std::vector<Stmt> *enclosingStatements,
      size_t statementIndex,
      bool &handledOut);
  bool resolveExprCollectionAccessTarget(const std::vector<ParameterInfo> &params,
                                         const std::unordered_map<std::string, BindingInfo> &locals,
                                         const Expr &expr,
                                         const ExprCollectionAccessDispatchContext &context,
                                         bool &handledOut,
                                         std::string &resolved,
                                         bool &resolvedMethod,
                                         bool &usedMethodTarget,
                                         bool &hasMethodReceiverIndex,
                                         size_t &methodReceiverIndex);
  struct ExprCollectionCountCapacityDispatchContext {
    bool isNamespacedVectorHelperCall = false;
    std::string namespacedHelper;
    bool isStdNamespacedVectorCountCall = false;
    bool shouldBuiltinValidateStdNamespacedVectorCountCall = false;
    bool isStdNamespacedMapCountCall = false;
    bool isNamespacedVectorCountCall = false;
    bool isNamespacedMapCountCall = false;
    bool isUnnamespacedMapCountFallbackCall = false;
    bool isResolvedMapCountCall = false;
    bool prefersCanonicalVectorCountAliasDefinition = false;
    bool isStdNamespacedVectorCapacityCall = false;
    bool shouldBuiltinValidateStdNamespacedVectorCapacityCall = false;
    bool isNamespacedVectorCapacityCall = false;
    bool isDirectStdNamespacedVectorCountWrapperMapTarget = false;
    bool hasStdNamespacedVectorCountAliasDefinition = false;
    bool shouldBuiltinValidateBareMapCountCall = false;
    std::function<bool(const Expr &)> resolveMapTarget;
    std::function<bool(const Expr &)> isArrayNamespacedVectorCountCompatibilityCall;
    std::function<bool(const std::string &, Expr &)> tryRewriteBareVectorHelperCall;
    std::function<void(const Expr &, std::string &, bool &, bool)> promoteCapacityToBuiltinValidation;
    std::function<bool(const std::string &)> isNonCollectionStructCapacityTarget;
  };
  bool resolveExprCollectionCountCapacityTarget(const std::vector<ParameterInfo> &params,
                                                const std::unordered_map<std::string, BindingInfo> &locals,
                                                const Expr &expr,
                                                const ExprCollectionCountCapacityDispatchContext &context,
                                                bool &handledOut,
                                                std::optional<Expr> &rewrittenExprOut,
                                                std::string &resolved,
                                                bool &resolvedMethod,
                                                bool &usedMethodTarget,
                                                bool &hasMethodReceiverIndex,
                                                size_t &methodReceiverIndex);
  bool inferDefinitionReturnBinding(const Definition &def, BindingInfo &bindingOut);
  bool inferQueryExprTypeText(const Expr &expr,
                              const std::vector<ParameterInfo> &params,
                              const std::unordered_map<std::string, BindingInfo> &locals,
                              std::string &typeTextOut);
  bool resolveCallCollectionTypePath(const Expr &target,
                                     const std::vector<ParameterInfo> &params,
                                     const std::unordered_map<std::string, BindingInfo> &locals,
                                     std::string &typePathOut);
  bool resolveCallCollectionTemplateArgs(const Expr &target,
                                         const std::string &expectedBase,
                                         const std::vector<ParameterInfo> &params,
                                         const std::unordered_map<std::string, BindingInfo> &locals,
                                         std::vector<std::string> &argsOut);
  std::string preferredExperimentalMapHelperTarget(std::string_view helperName) const;
  std::string preferredCanonicalExperimentalMapHelperTarget(std::string_view helperName) const;
  bool canonicalExperimentalMapHelperPath(const std::string &resolvedPath,
                                          std::string &canonicalPathOut,
                                          std::string &helperNameOut) const;
  bool canonicalizeExperimentalMapHelperResolvedPath(const std::string &resolvedPath,
                                                     std::string &canonicalPathOut) const;
  bool shouldBuiltinValidateCurrentMapWrapperHelper(std::string_view helperName) const;
  std::string mapNamespacedMethodCompatibilityPath(
      const Expr &candidate,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const BuiltinCollectionDispatchResolverAdapters &adapters = {});
  std::string directMapHelperCompatibilityPath(
      const Expr &candidate,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const BuiltinCollectionDispatchResolverAdapters &adapters = {});
  std::string methodRemovedCollectionCompatibilityPath(
      const Expr &candidate,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const BuiltinCollectionDispatchResolverAdapters &adapters = {});
  std::string explicitRemovedCollectionMethodPath(std::string_view rawMethodName,
                                                  std::string_view namespacePrefix) const;
  bool shouldPreserveRemovedCollectionHelperPath(const std::string &path) const;
  bool isUnnamespacedMapCountBuiltinFallbackCall(
      const Expr &candidate,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const BuiltinCollectionDispatchResolverAdapters &adapters = {});
  bool resolveRemovedMapBodyArgumentTarget(
      const Expr &candidate,
      const std::string &resolvedPath,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const BuiltinCollectionDispatchResolverAdapters &adapters,
      std::string &targetPathOut);
  struct BuiltinCollectionDispatchResolverAdapters {
    std::function<bool(const Expr &, BindingInfo &)> resolveBindingTarget;
    std::function<bool(const Expr &, BindingInfo &)> inferCallBinding;
  };
  struct BuiltinCollectionDispatchResolvers {
    std::function<bool(const Expr &, std::string &)> resolveIndexedArgsPackElementType;
    std::function<bool(const Expr &, std::string &)> resolveDereferencedIndexedArgsPackElementType;
    std::function<bool(const Expr &, std::string &)> resolveWrappedIndexedArgsPackElementType;
    std::function<bool(const Expr &, std::string &)> resolveArgsPackAccessTarget;
    std::function<bool(const Expr &, std::string &)> resolveArrayTarget;
    std::function<bool(const Expr &, std::string &)> resolveVectorTarget;
    std::function<bool(const Expr &, std::string &)> resolveExperimentalVectorTarget;
    std::function<bool(const Expr &, std::string &)> resolveExperimentalVectorValueTarget;
    std::function<bool(const Expr &, std::string &)> resolveSoaVectorTarget;
    std::function<bool(const Expr &, std::string &)> resolveBufferTarget;
    std::function<bool(const Expr &)> resolveStringTarget;
    std::function<bool(const Expr &, std::string &, std::string &)> resolveMapTarget;
    std::function<bool(const Expr &, std::string &, std::string &)> resolveExperimentalMapTarget;
    std::function<bool(const Expr &, std::string &, std::string &)> resolveExperimentalMapValueTarget;
  };
  BuiltinCollectionDispatchResolvers makeBuiltinCollectionDispatchResolvers(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const BuiltinCollectionDispatchResolverAdapters &adapters = {});
  size_t mapHelperReceiverIndex(const Expr &candidate,
                                const BuiltinCollectionDispatchResolvers &dispatchResolvers) const;
  bool bareMapHelperOperandIndices(const Expr &candidate,
                                   const BuiltinCollectionDispatchResolvers &dispatchResolvers,
                                   size_t &receiverIndexOut,
                                   size_t &keyIndexOut) const;
  std::string preferredBareMapHelperTarget(std::string_view helperName) const;
  std::string specializedExperimentalMapHelperTarget(std::string_view helperName,
                                                     const std::string &keyType,
                                                     const std::string &valueType) const;
  std::string preferredCanonicalExperimentalVectorHelperTarget(std::string_view helperName) const;
  std::string specializedExperimentalVectorHelperTarget(std::string_view helperName,
                                                        const std::string &elemType) const;
  bool canonicalExperimentalVectorHelperPath(const std::string &resolvedPath,
                                             std::string &canonicalPathOut,
                                             std::string &helperNameOut) const;
  std::string preferredBareVectorHelperTarget(std::string_view helperName) const;
  bool tryRewriteBareMapHelperCall(const Expr &candidate,
                                   std::string_view helperName,
                                   const BuiltinCollectionDispatchResolvers &dispatchResolvers,
                                   Expr &rewrittenOut) const;
  bool tryRewriteBareVectorHelperCall(const Expr &candidate,
                                      std::string_view helperName,
                                      const BuiltinCollectionDispatchResolvers &dispatchResolvers,
                                      Expr &rewrittenOut) const;
  bool tryRewriteCanonicalExperimentalMapHelperCall(
      const Expr &candidate,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers,
      Expr &rewrittenOut) const;
  bool tryRewriteCanonicalExperimentalVectorHelperCall(
      const Expr &candidate,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers,
      Expr &rewrittenOut) const;
  bool explicitCanonicalExperimentalMapBorrowedHelperPath(
      const Expr &candidate,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers,
      std::string &resolvedPathOut) const;
  bool hasResolvableMapHelperPath(const std::string &path) const;
  bool resolveMapKeyType(const Expr &target,
                         const BuiltinCollectionDispatchResolvers &dispatchResolvers,
                         std::string &keyTypeOut) const;
  bool resolveMapValueType(const Expr &target,
                           const BuiltinCollectionDispatchResolvers &dispatchResolvers,
                           std::string &valueTypeOut) const;
  const Expr *resolveBuiltinAccessReceiverExpr(const Expr &accessExpr) const;
  bool isNamedArgsPackMethodAccessCall(
      const Expr &target,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers) const;
  bool isNamedArgsPackWrappedFileBuiltinAccessCall(
      const Expr &target,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers) const;
  bool isMapLikeBareAccessReceiver(
      const Expr &candidate,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers);
  bool isArrayNamespacedVectorCountCompatibilityCall(
      const Expr &candidate,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers) const;
  bool isIndexedArgsPackMapReceiverTarget(
      const Expr &receiverExpr,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers) const;
  bool validateCollectionElementType(
      const Expr &arg,
      const std::string &typeName,
      const std::string &errorPrefix,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers);
  bool resolveNonCollectionAccessHelperPathFromTypeText(const std::string &typeText,
                                                        const std::string &typeNamespace,
                                                        std::string_view helperName,
                                                        std::string &pathOut) const;
  bool resolveLeadingNonCollectionAccessReceiverPath(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &receiverExpr,
      std::string_view helperName,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers,
      std::string &pathOut);
  bool resolveDirectCallTemporaryAccessReceiverPath(const Expr &receiverExpr,
                                                    std::string_view helperName,
                                                    std::string &pathOut);
  struct ExprArgumentValidationContext {
    const Expr *callExpr = nullptr;
    const std::string *resolved = nullptr;
    const std::string *diagnosticResolved = nullptr;
    const std::vector<ParameterInfo> *params = nullptr;
    const std::unordered_map<std::string, BindingInfo> *locals = nullptr;
    const BuiltinCollectionDispatchResolvers *dispatchResolvers = nullptr;
  };
  struct ExprResolvedCallSetup {
    std::string diagnosticResolved;
    ExprArgumentValidationContext argumentValidationContext;
    std::string resolvedStructConstructorZeroArgDiagnostic;
    ExprResolvedStructConstructorContext resolvedStructConstructorContext;
    ExprResolvedCallArgumentContext resolvedCallArgumentContext;
  };
  void prepareExprResolvedCallSetup(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const std::string &resolved,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers,
      const Definition &resolvedDefinition,
      const std::vector<ParameterInfo> &calleeParams,
      bool hasMethodReceiverIndex,
      size_t methodReceiverIndex,
      ExprResolvedCallSetup &contextOut);
  std::string expectedBindingTypeText(const BindingInfo &binding) const;
  std::string argumentStructMismatchDiagnostic(std::string_view diagnosticResolved,
                                               std::string_view paramName,
                                               const std::string &expectedTypePath,
                                               const std::string &actualTypePath) const;
  static bool isSoftwareNumericParamCompatible(ReturnKind expectedKind, ReturnKind actualKind);
  bool isBuiltinCollectionLiteralExpr(const Expr &candidate) const;
  bool isStringExprForArgumentValidation(const Expr &arg,
                                         const BuiltinCollectionDispatchResolvers &dispatchResolvers) const;
  bool extractExperimentalVectorElementTypeFromStructPath(const std::string &structPath,
                                                          std::string &elemTypeOut) const;
  bool extractExperimentalVectorElementType(const BindingInfo &binding,
                                            std::string &elemTypeOut) const;
  bool extractExperimentalMapFieldTypesFromStructPath(const std::string &structPath,
                                                      std::string &keyTypeOut,
                                                      std::string &valueTypeOut) const;
  bool validateArgumentTypeAgainstParam(const Expr &arg,
                                        const ParameterInfo &param,
                                        const std::string &expectedTypeName,
                                        const std::string &expectedTypeText,
                                        const ExprArgumentValidationContext &context);
  bool validateSpreadArgumentTypeAgainstParam(const Expr &arg,
                                              const ParameterInfo &param,
                                              const std::string &expectedTypeName,
                                              const std::string &expectedTypeText,
                                              const ExprArgumentValidationContext &context);
  bool validateArgumentsForParameter(const ParameterInfo &param,
                                     const std::string &expectedTypeName,
                                     const std::string &expectedTypeText,
                                     const std::vector<const Expr *> &argsToValidate,
                                     const ExprArgumentValidationContext &context);
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
    std::function<bool(const Expr &, Expr &)> tryRewriteBareMapHelperCall;
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
  bool validatePathSpaceComputeBuiltinStatement(const std::vector<ParameterInfo> &params,
                                                const std::unordered_map<std::string, BindingInfo> &locals,
                                                const Expr &stmt,
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
  bool validateReturnStatement(const std::vector<ParameterInfo> &params,
                               std::unordered_map<std::string, BindingInfo> &locals,
                               const Expr &stmt,
                               ReturnKind returnKind,
                               bool allowReturn,
                               bool *sawReturn,
                               const std::string &namespacePrefix);
  bool validateStatementBodyArguments(const std::vector<ParameterInfo> &params,
                                      std::unordered_map<std::string, BindingInfo> &locals,
                                      const Expr &stmt,
                                      ReturnKind returnKind,
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
                                ResultTypeInfo &out);
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
    EffectScope(SemanticsValidator &validatorIn, std::unordered_set<std::string> nextIn)
        : validator(validatorIn), previous(std::move(validatorIn.currentValidationContext_.activeEffects)) {
      validator.currentValidationContext_.activeEffects = std::move(nextIn);
    }
    ~EffectScope() {
      validator.currentValidationContext_.activeEffects = std::move(previous);
    }
  };
  struct ValidationContextScope {
    SemanticsValidator &validator;
    ValidationContext previous;
    ValidationContextScope(SemanticsValidator &validatorIn, ValidationContext context)
        : validator(validatorIn), previous(validatorIn.currentValidationContext_) {
      validator.currentValidationContext_ = std::move(context);
    }
    ~ValidationContextScope() {
      validator.currentValidationContext_ = std::move(previous);
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
    OnErrorScope(SemanticsValidator &validatorIn, std::optional<OnErrorHandler> nextIn)
        : validator(validatorIn), previous(std::move(validatorIn.currentValidationContext_.onError)) {
      validator.currentValidationContext_.onError = std::move(nextIn);
    }
    ~OnErrorScope() {
      validator.currentValidationContext_.onError = std::move(previous);
    }
  };
  struct MovedScope {
    SemanticsValidator &validator;
    std::unordered_set<std::string> previous;
    MovedScope(SemanticsValidator &validatorIn, std::unordered_set<std::string> nextIn)
        : validator(validatorIn), previous(std::move(validatorIn.currentValidationContext_.movedBindings)) {
      validator.currentValidationContext_.movedBindings = std::move(nextIn);
    }
    ~MovedScope() {
      validator.currentValidationContext_.movedBindings = std::move(previous);
    }
  };
  struct BorrowEndScope {
    SemanticsValidator &validator;
    std::unordered_set<std::string> previous;
    BorrowEndScope(SemanticsValidator &validatorIn, std::unordered_set<std::string> nextIn)
        : validator(validatorIn), previous(std::move(validatorIn.currentValidationContext_.endedReferenceBorrows)) {
      validator.currentValidationContext_.endedReferenceBorrows = std::move(nextIn);
    }
    ~BorrowEndScope() {
      validator.currentValidationContext_.endedReferenceBorrows = std::move(previous);
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
  DiagnosticSink diagnosticSink_;
  bool collectDiagnostics_ = false;
  bool graphTypeResolverEnabled_ = false;
  bool allowRecursiveReturnInference_ = true;
  bool deferUnknownReturnInferenceErrors_ = false;

  std::unordered_set<std::string> defaultEffectSet_;
  std::unordered_set<std::string> entryDefaultEffectSet_;
  std::unordered_map<std::string, const Definition *> defMap_;
  std::unordered_map<std::string, ReturnKind> returnKinds_;
  std::unordered_map<std::string, std::string> returnStructs_;
  std::unordered_map<std::string, BindingInfo> graphLocalAutoBindings_;
  std::unordered_set<std::string> structNames_;
  std::unordered_set<std::string> publicDefinitions_;
  std::unordered_map<std::string, std::vector<ParameterInfo>> paramsByDef_;
  std::unordered_map<std::string, ValidationContext> definitionValidationContexts_;
  std::unordered_map<std::string, ValidationContext> executionValidationContexts_;
  ValidationContext currentValidationContext_;
  std::unordered_set<std::string> inferenceStack_;
  std::unordered_set<std::string> returnBindingInferenceStack_;
  std::unordered_set<std::string> queryTypeInferenceDefinitionStack_;
  std::unordered_set<const Expr *> queryTypeInferenceExprStack_;
  std::unordered_map<std::string, std::string> importAliases_;
  std::unordered_map<std::string, EffectFreeSummary> effectFreeDefCache_;
  std::unordered_set<std::string> effectFreeDefStack_;
  std::unordered_map<std::string, bool> effectFreeStructCache_;
  std::unordered_set<std::string> effectFreeStructStack_;
  bool mathImportAll_ = false;
  std::unordered_set<std::string> mathImports_;
  std::string entryArgsName_;
  bool allowEntryArgStringUse_ = false;
  const Expr *currentExprContext_ = nullptr;
  const Definition *currentDefinitionContext_ = nullptr;
  const Execution *currentExecutionContext_ = nullptr;
};

} // namespace primec::semantics
