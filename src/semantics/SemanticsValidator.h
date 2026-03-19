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
  bool inferBindingTypeFromInitializer(const Expr &initializer,
                                       const std::vector<ParameterInfo> &params,
                                       const std::unordered_map<std::string, BindingInfo> &locals,
                                       BindingInfo &bindingOut,
                                       const Expr *bindingExpr = nullptr);
  static std::string graphLocalAutoBindingKey(const std::string &scopePath, int sourceLine, int sourceColumn);
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
                                 const std::vector<Expr> *enclosingStatements,
                                 size_t statementIndex);
  struct ExprResultFileBuiltinContext {
    std::function<bool(const Expr &)> isNamedArgsPackWrappedFileBuiltinAccessCall;
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
  struct ExprNamedArgumentBuiltinContext {
    std::function<bool(const Expr &)> isNamedArgsPackMethodAccessCall;
    std::function<bool(const Expr &)> isNamedArgsPackWrappedFileBuiltinAccessCall;
    std::function<bool(const Expr &)> isArrayNamespacedVectorCountCompatibilityCall;
  };
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
  struct ExprCollectionAccessValidationContext {
    bool isStdNamespacedVectorAccessCall = false;
    bool shouldAllowStdAccessCompatibilityFallback = false;
    bool hasStdNamespacedVectorAccessDefinition = false;
    bool isStdNamespacedMapAccessCall = false;
    bool hasStdNamespacedMapAccessDefinition = false;
    bool shouldBuiltinValidateBareMapAccessCall = false;
    std::function<bool(const Expr &, std::string &)> resolveArgsPackAccessTarget;
    std::function<bool(const Expr &, std::string &)> resolveArrayTarget;
    std::function<bool(const Expr &)> resolveStringTarget;
    std::function<bool(const Expr &, std::string &)> resolveMapKeyType;
    std::function<bool(const Expr &)> isIndexedArgsPackMapReceiverTarget;
    std::function<bool(const Expr &)> isNamedArgsPackMethodAccessCall;
    std::function<bool(const Expr &)> isNamedArgsPackWrappedFileBuiltinAccessCall;
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
  bool hasImportedDefinitionPath(const std::string &path) const;
  bool isStaticHelperDefinition(const Definition &def) const;
  bool hasDeclaredDefinitionPath(const std::string &path) const;
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
  std::string mapNamespacedMethodCompatibilityPath(const Expr &candidate) const;
  std::string directMapHelperCompatibilityPath(const Expr &candidate) const;
  std::string explicitRemovedCollectionMethodPath(std::string_view rawMethodName,
                                                  std::string_view namespacePrefix) const;
  bool shouldPreserveRemovedCollectionHelperPath(const std::string &path) const;
  bool isUnnamespacedMapCountBuiltinFallbackCall(const Expr &candidate) const;
  bool resolveRemovedMapBodyArgumentTarget(const Expr &candidate,
                                           const std::string &resolvedPath,
                                           std::string &targetPathOut) const;
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
