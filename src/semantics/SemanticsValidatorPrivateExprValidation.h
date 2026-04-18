#pragma once

  ReturnKind inferExprReturnKind(const Expr &expr,
                                const std::vector<ParameterInfo> &params,
                                const std::unordered_map<std::string, BindingInfo> &locals);
  ReturnKind inferExprReturnKindImpl(
      const Expr &expr,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals);
  bool ensureDefinitionReturnKindReady(const Definition &def);
  bool inferDefinitionReturnKind(const Definition &def);
  struct DefinitionReturnInferenceState {
    ReturnKind inferred = ReturnKind::Unknown;
    std::string inferredStructPath;
    BindingInfo inferredBinding;
    bool hasInferredBinding = false;
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
    std::shared_ptr<void> resolverStateKeepAlive;
  };

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
  bool resolveExprVectorHelperCall(const std::vector<ParameterInfo> &params,
                                   const std::unordered_map<std::string, BindingInfo> &locals,
                                   const Expr &expr,
                                   bool &hasResolutionOut,
                                   std::string &resolvedPathOut,
                                   size_t &receiverIndexOut);
  struct ExprDispatchBootstrap {
    BuiltinCollectionDispatchResolverAdapters dispatchResolverAdapters;
    BuiltinCollectionDispatchResolvers dispatchResolvers;
    std::function<bool(const Expr &)> isDeclaredPointerLikeCall;
    std::function<bool(const Expr &)> resolveMapTarget;
  };
  bool validateExprEarlyPointerBuiltin(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const ExprDispatchBootstrap &dispatchBootstrap,
      bool &handledOut);
  std::string resolveExprConcreteCallPath(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const std::string &candidatePath) const;
  void prepareExprDispatchBootstrap(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      ExprDispatchBootstrap &bootstrapOut);
  struct ExprPreDispatchDirectCallContext {
    const ExprDispatchBootstrap *dispatchBootstrap = nullptr;
  };
  bool validateExprPreDispatchDirectCalls(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const ExprPreDispatchDirectCallContext &context,
      std::string &resolvedOut,
      std::optional<Expr> &rewrittenExprOut,
      bool &handledOut);
  struct ExprMethodCompatibilitySetup {
    bool resolvedMethod = false;
    bool usedMethodTarget = false;
    bool hasMethodReceiverIndex = false;
    size_t methodReceiverIndex = 0;
    std::function<void(const Expr &, std::string &, bool &, bool)> promoteCapacityToBuiltinValidation;
    std::function<bool(const std::string &)> isNonCollectionStructCapacityTarget;
    std::function<std::string(const std::string &)> unavailableMethodDiagnostic;
  };
  bool prepareExprMethodCompatibilitySetup(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const ExprDispatchBootstrap &dispatchBootstrap,
      bool hasVectorHelperCallResolution,
      const std::string &vectorHelperCallResolvedPath,
      size_t vectorHelperCallReceiverIndex,
      std::string &resolved,
      ExprMethodCompatibilitySetup &setupOut);
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
      const std::vector<Expr> *enclosingStatements,
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
