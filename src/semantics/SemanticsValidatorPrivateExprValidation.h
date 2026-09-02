#pragma once

  ReturnKind inferExprReturnKind(const Expr &expr,
                                const std::vector<ParameterInfo> &params,
                                const std::unordered_map<std::string, BindingInfo> &locals);
  ReturnKind inferExprReturnKindImpl(
      const Expr &expr,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals);
  ReturnKind inferPickExprReturnKind(
      const Expr &expr,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals);
  bool inferPickExprTypeText(
      const Expr &expr,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      std::string &typeTextOut);
  std::string inferPickExprStructReturnPath(
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
    std::function<bool(const Expr &, std::string &)> resolveCollectionVectorTarget;
    std::function<bool(const Expr &, std::string &)> resolveCollectionVectorValueTarget;
    std::function<bool(const Expr &, std::string &)> resolveSoaVectorTarget;
    std::function<bool(const Expr &, std::string &)> resolveBufferTarget;
    std::function<bool(const Expr &)> resolveStringTarget;
    std::function<bool(const Expr &, std::string &, std::string &)> resolveMapTarget;
    std::function<bool(const Expr &, std::string &, std::string &)> resolveKeyValueTarget;
    std::function<bool(const Expr &, std::string &, std::string &)> resolveDirectKeyValueTarget;
    std::shared_ptr<void> resolverStateKeepAlive;
  };

  bool validateExpr(const std::vector<ParameterInfo> &params,
                    const std::unordered_map<std::string, BindingInfo> &locals,
                    const Expr &expr,
                    const std::vector<Expr> *enclosingStatements = nullptr,
                    size_t statementIndex = 0,
                    bool expressionIsStatementContext = false);
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
  bool validatePickExpr(const std::vector<ParameterInfo> &params,
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
  bool resolveEscapingArraySliceRoot(const std::vector<ParameterInfo> &params,
                                     const std::unordered_map<std::string, BindingInfo> &locals,
                                     const Expr &expr,
                                     std::string &rootOut);
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
    std::function<std::string(const Expr &)> getDirectKeyValueHelperCompatibilityPath;
    std::function<bool(const Expr &)> isIndexedArgsPackKeyValueReceiverTarget;
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
  struct ExprCountCapacityBuiltinContext {
    std::function<bool(const Expr &, std::string &)> resolveVectorTarget;
    const BuiltinCollectionDispatchResolvers *dispatchResolvers = nullptr;
  };
  bool validateExprCountCapacityBuiltins(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const std::string &resolved,
      bool resolvedMethod,
      const ExprCountCapacityBuiltinContext &context,
      bool &handledOut);
  void prepareExprCountCapacityBuiltinContext(
      const BuiltinCollectionDispatchResolvers &dispatchResolvers,
      ExprCountCapacityBuiltinContext &contextOut);
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
    bool shouldBuiltinValidateBareKeyValueContainsCall = false;
    std::function<bool(const Expr &, std::string &)> resolveKeyValueKeyType;
    std::function<bool(const Expr &, std::string &)> resolveVectorTarget;
    std::function<bool(const Expr &, std::string &)> resolveSoaVectorTarget;
    std::function<bool(const Expr &)> resolveStringTarget;
    std::function<bool(const Expr &, size_t &, size_t &)>
        bareKeyValueHelperOperandIndices;
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
    bool shouldBuiltinValidateBareKeyValueContainsCall = false;
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
      bool shouldBuiltinValidateBareKeyValueContainsCall,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers,
      ExprLateMapSoaBuiltinContext &contextOut);
  struct ExprLateCollectionAccessFallbackContext {
    bool isStdNamespacedVectorAccessCall = false;
    bool shouldAllowStdAccessCompatibilityFallback = false;
    bool hasStdNamespacedVectorAccessDefinition = false;
    bool isStdNamespacedMapAccessCall = false;
    bool hasStdNamespacedKeyValueAccessDefinition = false;
    bool shouldBuiltinValidateBareKeyValueAccessCall = false;
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
      bool hasStdNamespacedKeyValueAccessDefinition,
      bool shouldBuiltinValidateBareKeyValueAccessCall,
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
  struct ExprLateUnknownTargetFallbackContext {
    std::function<bool(const Expr &)> resolveMapTarget;
    std::function<bool(const Expr &)> isIndexedArgsPackKeyValueReceiverTarget;
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
    bool hasStdNamespacedKeyValueAccessDefinition = false;
    bool shouldBuiltinValidateBareKeyValueAccessCall = false;
    std::function<bool(const Expr &, std::string &)> resolveArgsPackAccessTarget;
    std::function<bool(const Expr &, std::string &)> resolveVectorTarget;
    std::function<bool(const Expr &, std::string &)> resolveCollectionVectorValueTarget;
    std::function<bool(const Expr &, std::string &)> resolveArrayTarget;
    std::function<bool(const Expr &)> resolveStringTarget;
    std::function<bool(const Expr &, std::string &)> resolveKeyValueKeyType;
    std::function<bool(const Expr &, std::string &, std::string &)>
        resolveKeyValueTarget;
    std::function<bool(const Expr &)> isIndexedArgsPackKeyValueReceiverTarget;
    std::function<bool(const Expr &)> isNamedArgsPackMethodAccessCall;
    std::function<bool(const Expr &)> isNamedArgsPackWrappedFileBuiltinAccessCall;
    std::function<bool(const Expr &)> isKeyValueLikeBareAccessReceiverTarget;
    std::function<bool(const std::string &)> isNonCollectionStructAccessTarget;
    std::function<bool(const Expr &, const std::string &, Expr &)> tryRewriteBareKeyValueHelperCall;
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
  bool typeHasCollectionCategoryTrait(const std::string &typeName,
                                      const std::string &namespacePrefix,
                                      std::string_view traitName) const;
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
  // TODO-4724: extracted from resolveMethodTarget's body - resolves a
  // method call on a declared sum type by trying the sum's canonical
  // path, its as-spelled path, and its bare leaf name, each joined with
  // the method name, in that order.
  bool resolveDeclaredSumMethodTarget(const std::string &sumPath,
                                      const std::string &normalizedMethodName,
                                      std::string &resolvedOut,
                                      bool &isBuiltinOut) const;
  // TODO-4724: extracted from resolveMethodTarget's body (was the local
  // lambda `setCollectionMethodTarget`, seam (2)'s hub-lambda precursor -
  // see this task's own implementation_notes) - the "explicit removed/
  // compat collection helper path vs. canonical stdlib path" resolution
  // hub, called from every branch that resolves a vector/soa/array/
  // string/key-value method-call target to a concrete path.
  bool resolveExplicitOrCanonicalCollectionMethodTarget(
      const std::string &path,
      const std::string &explicitRemovedMethodPath,
      const std::string &normalizedMethodName,
      const Expr &receiver,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      std::string &resolvedOut,
      bool &isBuiltinOut);
  // TODO-4724 seam (3): extracted from resolveMethodTarget's body - the
  // "explicit vector-namespaced helper path vs. receiver family"
  // classification cluster. These 6 mutually-calling helpers classify a
  // receiver's collection family (vector/soa/array/string/map) and check
  // whether an explicitly-spelled vector-compat helper path's own
  // parameter type is compatible with a given receiver - used throughout
  // resolveMethodTarget wherever an explicit `/std/collections/vector/...`
  // (or soa-surface) spelling needs validating against the actual
  // receiver shape.
  std::string classifyVectorCompatHelperParamFamily(const BindingInfo &binding) const;
  bool explicitVectorCompatHelperFamilyHasCompatibleReceiver(
      std::string_view path, std::string_view receiverFamily) const;
  std::string classifyExplicitVectorHelperReceiver(
      const Expr &receiverExpr, const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals);
  bool hasReceiverCompatibleExplicitVectorHelperPath(
      const std::string &path, const Expr &receiverExpr,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals);
  bool preferExplicitCanonicalVectorHelperForReceiver(
      const Expr &receiverExpr, const std::string &explicitVectorHelperPath,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals);
  std::optional<bool> tryResolveExplicitCanonicalVectorCountMethodTarget(
      const Expr &receiverExpr,
      const std::string &explicitVectorHelperPath,
      const std::string &normalizedMethodName,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      std::string &resolvedOut,
      bool &isBuiltinOut);
  // TODO-4724 seam (4a): extracted from resolveMethodTarget's body - runs
  // error_ under a saved/restored snapshot while probing a speculative
  // inference call whose own failure diagnostic must not leak into the
  // caller's own diagnostic if the probe doesn't pan out.
  bool withPreservedError(const std::function<bool()> &fn);
  // TODO-4724 seam (4a): extracted from resolveMethodTarget's body - the
  // first, narrower half of the "primitive/struct/sum-type generic
  // fallback" tail (see this task's own implementation_notes for why the
  // full tail's capture surface was too large to extract in one step).
  // Infers a method-call receiver's type name/template argument from
  // scratch via direct param/local binding lookup, then (in order)
  // initializer inference, definition-return-type inference, struct-return
  // inference, and a general ReturnKind fallback - stopping at the first
  // one that produces a non-empty type name.
  void inferMethodTargetReceiverType(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &receiver,
      std::string &typeNameOut,
      std::string &typeTemplateArgOut);
  // TODO-4724 seam (4b), step 1: promoted from resolveMethodTarget's own
  // local lambda (used at 4 call sites across the function's tail, not
  // just by maybeFailRetiredMaybeMutableHelperForType below) - resolves a
  // (possibly templated) type text to a declared sum type's definition
  // path, preferring a matching template specialization's own mangled
  // path over the generic base path when one exists and is registered.
  std::string resolveSumTypePath(const std::string &typeText,
                                  const std::string &namespacePrefix) const;
  // TODO-4724 seam (4b), step 1: extracted from resolveMethodTarget's body
  // - rejects a call to one of the retired Maybe<T> in-place mutation
  // helpers (set/clear/etc.) on a sum-backed Maybe<T> receiver with a
  // diagnostic naming the non-mutating replacement API, leaving handledOut
  // false (and returning false) for every other receiver/method-name
  // combination so callers can fall through to the rest of resolveMethodTarget's
  // own dispatch.
  bool maybeFailRetiredMaybeMutableHelperForType(const std::string &typeName,
                                                  const std::string &typeTemplateArg,
                                                  const std::string &normalizedMethodName,
                                                  const Expr &receiver,
                                                  bool &handledOut);
  // TODO-4724 seam (4b), step 2: promoted from resolveMethodTarget's own
  // local lambda (used at 11 call sites throughout the function) - walks a
  // type name up through progressively shorter namespace-prefix scopes
  // looking for a declared struct with that scoped path, then falls back
  // to an import alias. Deliberately NOT named resolveStructTypePath (the
  // exact name of both the local lambda it replaces and an unrelated
  // free function overload declared in SemanticsHelpers.h taking a third
  // structTypes-set parameter, called unqualified from dozens of other
  // SemanticsValidator member function bodies elsewhere in this class) -
  // a same-named 2-arg member function would hide that free function via
  // C++ member-name-hiding rules and break every one of those unqualified
  // 3-arg call sites at compile time. The local lambda in
  // resolveMethodTarget keeps its own name/signature and forwards here.
  std::string resolveMethodTargetStructTypePath(
      const std::string &typeName, const std::string &namespacePrefix) const;
  // TODO-4724 seam (4b), step 3: promoted from resolveMethodTarget's own
  // local lambda (single call site) - resolves a canonical SoA collection
  // helper name to its borrowed-receiver variant (registry-backed lookup)
  // before composing the preferred internal SoA helper target path for it.
  std::string preferredBorrowedSoaHelperTargetForCollectionMethod(
      std::string helperName) const;
  // TODO-4724 seam (4c): the "preferred key-value method target" hub chain
  // - setPreferredKeyValueMethodTarget's own multi-lambda dependency web,
  // extracted bottom-up (each function below only calls the ones already
  // promoted above it, plus real members/free functions - verified by
  // reading every dependency's own definition, not assumed). All names
  // reused verbatim from resolveMethodTarget's own local lambdas after
  // confirming (by grepping every .cpp/.h in src/semantics/) that none of
  // them collide with an unrelated free function or struct-field name
  // called unqualified from another SemanticsValidator member body - see
  // resolveMethodTargetStructTypePath's own comment above for why that
  // check matters. bindingTypeText was deliberately NOT promoted (it
  // collides with an unrelated free function of the same name used
  // elsewhere in this class) - its trivial two-line body is inlined
  // directly into isWrappedKeyValueReceiver below instead.
  bool resolveFieldBindingTarget(const std::vector<ParameterInfo> &params,
                                 const std::unordered_map<std::string, BindingInfo> &locals,
                                 const Expr &target, BindingInfo &bindingOut);
  bool extractWrappedPointeeType(const std::string &typeText,
                                 std::string &pointeeTypeOut) const;
  bool extractExperimentalKeyValueFieldTypes(const BindingInfo &binding,
                                             std::string &keyTypeOut,
                                             std::string &valueTypeOut) const;
  bool isWrappedKeyValueTypeText(const std::string &typeText) const;
  bool extractAnyKeyValueTypes(const BindingInfo &binding, std::string &keyTypeOut,
                               std::string &valueTypeOut) const;
  // TODO-5280: promoted from a local lambda in resolveMethodTarget's
  // body - fully self-contained (captures no resolveMethodTarget locals,
  // only member state: currentValidationState_/paramsByDef_/defMap_/
  // structNames_/importAliases_/sumNames_).
  bool resolveCurrentDefinitionParamBinding(const std::string &name,
                                             BindingInfo &bindingOut) const;
  // TODO-5280: promoted from local lambdas that depended only on
  // params/locals plus resolveCurrentDefinitionParamBinding above.
  bool resolveArgsPackCountTarget(
      const Expr &target, std::string &elemType,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals) const;
  bool resolveArgsPackAccessTarget(
      const Expr &target, std::string &elemType,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals) const;
  bool resolveIndexedArgsPackElementType(
      const Expr &target, std::string &elemTypeOut,
      const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget) const;
  bool resolveDereferencedIndexedArgsPackElementType(
      const Expr &target, std::string &elemTypeOut,
      const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget) const;
  bool resolveWrappedIndexedArgsPackElementType(
      const Expr &target, std::string &elemTypeOut,
      const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget) const;
  bool isWrappedKeyValueReceiver(
      const Expr &receiverExpr, const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget);
  bool isCanonicalKeyValueReceiver(
      const Expr &receiverExpr, const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals);
  bool resolveExperimentalKeyValueTarget(
      const Expr &target, std::string &keyTypeOut, std::string &valueTypeOut,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals);
  std::string borrowedKeyValueHelperNameForReceiver(
      const Expr &receiverExpr, const std::string &helperName,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget);
  std::string preferredKeyValueMethodTarget(
      const Expr &receiverExpr, const std::string &helperName,
      const std::string &explicitKeyValueHelperPath,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget);
  bool setPreferredKeyValueMethodTarget(
      const Expr &receiverExpr, const std::string &helperName,
      const std::string &explicitKeyValueHelperPath, const Expr &receiver,
      const std::string &explicitRemovedMethodPath, const std::string &normalizedMethodName,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      std::string &resolvedOut, bool &isBuiltinOut);
  // TODO-4724 seam (4d): a batch of the largest remaining still-full-bodied
  // local lambdas in resolveMethodTarget, promoted after `setPreferredKeyValueMethodTarget`
  // (seam (4c)) removed most of their entangled dependencies. Each was
  // independently verified (by reading its own dependency chain, not
  // assumed) to route only through real members, free functions, or
  // other already-promoted seam (1)-(4c) members.
  bool extractCollectionElementType(const std::string &typeText,
                                    const std::string &expectedBase,
                                    std::string &elemTypeOut) const;
  bool resolveArrayTarget(
      const Expr &target, std::string &elemType,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget);
  // TODO-4724 seam (6): self-contained recursive lambda promoted verbatim.
  bool resolveBorrowedVectorReceiver(
      const Expr &candidate, std::string &elemTypeOut,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals);
  // TODO-4724 seam (7): mutually-recursive pair promoted verbatim.
  bool resolveVectorTarget(
      const Expr &target, std::string &elemType,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget);
  bool resolveSoaVectorTarget(
      const Expr &target, std::string &elemType,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget);
  // TODO-4724 seam (8): self-contained recursive lambda promoted verbatim.
  bool resolveStringTarget(
      const Expr &target, const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget);
  // TODO-4724 seam (9): straight-line dispatch block promoted verbatim.
  bool resolveArgsPackElementMethodTarget(
      const std::string &elementTypeText, const Expr &receiverExpr,
      const std::string &normalizedMethodName,
      const std::function<bool(const std::string &)> &setCollectionMethodTarget,
      const std::function<bool(const Expr &, const std::string &)>
          &setPreferredKeyValueMethodTarget,
      std::string &resolvedOut, bool &isBuiltinOut);
  // TODO-4724 seam (10)/TODO-5277: promoted from the local lambda
  // `explicitRemovedCollectionMethodPathLocal`, itself renamed during
  // TODO-4724 seam (4d) after the original name
  // `explicitRemovedCollectionMethodPath` was found to silently collide
  // (as a legal C++ overload, not a build error) with a pre-existing,
  // unrelated member of that exact name in
  // SemanticsValidatorInferCollectionCompatibility.cpp - see that seam's
  // progress note in docs/todo_finished.md for the full incident. This
  // name was checked with an anchored `SemanticsValidator::<name>(`
  // definition grep across all of src/semantics/ before being chosen;
  // do not rename this back to `explicitRemovedCollectionMethodPath` or
  // any other spelling without repeating that check.
  std::string explicitRemovedCollectionMethodPathForCallNamespace(
      const std::string &rawMethodName, const std::string &callNamespacePrefix) const;
  bool resolveCollectionVectorValueTarget(
      const Expr &target, std::string &elemTypeOut,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals);
  bool resolveKeyValueTarget(
      const Expr &target, const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget);
  bool resolveMethodTargetKeyValueValueType(
      const Expr &target, std::string &valueTypeOut,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget);
  std::string preferredBorrowedSoaAccessHelperTarget(std::string_view helperName) const;
  bool tryRedirectConcreteExperimentalSoaMethodTarget(
      const std::string &resolvedType, const std::string &canonicalCollectionHelperName,
      const Expr &receiver, const std::string &explicitRemovedMethodPath,
      const std::string &normalizedMethodName, const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      std::string &resolvedOut, bool &isBuiltinOut);
  bool resolveExplicitDirectCallReturnMethodTarget(
      const Expr &receiverExpr, const std::string &canonicalCollectionHelperName,
      const std::string &normalizedMethodName, const Expr &receiver,
      const std::string &explicitRemovedMethodPath,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals, std::string &resolvedOut,
      bool &isBuiltinOut);
  // TODO-4724 seam (4e): the last big batch - resolveCollectionMethodFromTypePath
  // (129 lines, the single biggest remaining local lambda) plus its own
  // small dependencies not yet promoted. Names checked with the updated
  // methodology from seam (4d)'s regression: grepped for
  // "SemanticsValidator::<name>(" as an anchored definition pattern
  // across the whole src/semantics/ tree, not just call sites - all
  // clean, no collisions found.
  bool isValueSurfaceAccessMethodName(std::string_view helperName) const;
  bool isCanonicalKeyValueAccessMethodName(std::string_view helperName) const;
  std::string preferredBufferMethodTarget(const std::string &helperName) const;
  bool resolveCollectionMethodFromTypePath(
      const std::string &collectionTypePath, const std::string &normalizedMethodName,
      const Expr &receiver, const std::string &explicitVectorHelperPath,
      const std::string &explicitKeyValueHelperPath,
      const std::string &explicitRemovedMethodPath,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      std::string &resolvedOut,
      bool &isBuiltinOut);
  // TODO-4724 seam (4f): another batch of the next-largest remaining
  // still-full-bodied local lambdas, same methodology as seam (4e) -
  // every name checked via an anchored "SemanticsValidator::<name>("
  // definition grep across the whole src/semantics/ tree before
  // writing any code.
  std::string getDirectKeyValueHelperCompatibilityPath(
      const Expr &candidate, const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget);
  bool resolveCollectionVectorMetadataMethodTarget(
      const std::string &normalizedMethodName, const Expr &receiver,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals, std::string &resolvedOut,
      bool &isBuiltinOut);
  std::string explicitVectorMethodPath(
      const std::string &rawMethodName, const std::string &callNamespacePrefix) const;
  std::string explicitKeyValueMethodPath(
      const std::string &rawMethodName, const std::string &callNamespacePrefix) const;
  bool setIndexedArgsPackKeyValueMethodTarget(
      const Expr &receiverExpr, const std::string &helperName,
      const std::string &explicitKeyValueHelperPath, const Expr &receiver,
      const std::string &explicitRemovedMethodPath, const std::string &normalizedMethodName,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      std::string &resolvedOut, bool &isBuiltinOut);
  // TODO-4724 seam (5): structural split of the "generic fallback" tail -
  // the straight-line dispatch-on-typeName body that was never wrapped in
  // a local lambda to begin with (see this task's own implementation_notes
  // on why seams (1)-(4f) couldn't reach it: those extracted pre-existing
  // self-contained lambdas, but this tail had no such boundary until now).
  // Small helpers this tail needs that weren't already members:
  static const char *exprKindName(Expr::Kind kind);
  bool resolveExplicitRootKeyValueMethodPath(
      const std::string &explicitKeyValueHelperPath, const Expr &receiver,
      std::string &resolvedOut, bool &isBuiltinOut);
  // The tail itself, starting from receiver-type inference through the
  // function's final generic resolvedType + "/" + normalizedMethodName
  // fallback. failMethodTargetResolutionDiagnostic and
  // stampFileErrorResultFailure are threaded through as callbacks rather
  // than promoted, since both close over resolveMethodTarget's own
  // error_/rememberedMethodTargetTraceFailure diagnostic-tracing state.
  bool resolveMethodTargetGenericFallback(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const std::string &callNamespacePrefix, const Expr &receiver,
      const std::string &normalizedMethodName,
      const std::string &canonicalCollectionHelperName,
      const std::string &explicitVectorHelperPath,
      const std::string &explicitKeyValueHelperPath,
      const std::string &explicitRemovedMethodPath, bool traceFileErrorResult,
      const std::function<bool(std::string)> &failMethodTargetResolutionDiagnostic,
      const std::function<void(std::string_view, std::string_view, std::string_view)>
          &stampFileErrorResultFailure,
      std::string &resolvedOut, bool &isBuiltinOut);
  bool isUnqualifiedCollectionBuiltinName(const Expr &candidate, const char *helper) const;
  bool getVectorMutatorHelperName(const Expr &candidate, std::string &nameOut) const;
  bool resolveVectorHelperMethodTarget(const std::vector<ParameterInfo> &params,
                                       const std::unordered_map<std::string, BindingInfo> &locals,
                                       const Expr &receiver,
                                       const std::string &helperName,
                                       std::string &resolvedOut);
  bool resolveExprVectorHelperCall(const std::vector<ParameterInfo> &params,
                                   const std::unordered_map<std::string, BindingInfo> &locals,
                                   const Expr &expr,
                                   bool allowStatementOnlyMutator,
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
    bool isNamespacedMapHelperCall = false;
    std::string namespacedHelper;
    bool isNamespacedVectorCapacityCall = false;
    bool isDirectStdNamespacedVectorCountWrapperKeyValueTarget = false;
    bool isStdNamespacedVectorAccessCall = false;
    bool hasStdNamespacedVectorAccessDefinition = false;
    bool isStdNamespacedMapAccessCall = false;
    bool hasStdNamespacedKeyValueAccessDefinition = false;
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
  bool validateExprDirectCollectionFallbacks(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const std::string &resolved,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers,
      std::optional<Expr> &rewrittenExprOut);
  struct ExprCollectionAccessDispatchContext {
    bool isNamespacedVectorHelperCall = false;
    bool isNamespacedMapHelperCall = false;
    std::string namespacedHelper;
    bool shouldBuiltinValidateBareKeyValueContainsCall = false;
    bool shouldBuiltinValidateBareKeyValueAccessCall = false;
    std::function<bool(const Expr &, std::string &)> resolveArrayTarget;
    std::function<bool(const Expr &, std::string &)> resolveVectorTarget;
    std::function<bool(const Expr &, std::string &)> resolveSoaVectorTarget;
    std::function<bool(const Expr &)> resolveStringTarget;
    std::function<bool(const Expr &)> resolveMapTarget;
    std::function<bool(const std::string &)> hasResolvableKeyValueHelperPath;
    std::function<bool(const Expr &)> isIndexedArgsPackKeyValueReceiverTarget;
  };
  void prepareExprCollectionAccessDispatchContext(
      const ExprCollectionDispatchSetup &dispatchSetup,
      bool shouldBuiltinValidateBareKeyValueContainsCall,
      bool shouldBuiltinValidateBareKeyValueAccessCall,
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
    bool isNamespacedVectorCapacityCall = false;
    bool isDirectStdNamespacedVectorCountWrapperKeyValueTarget = false;
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
