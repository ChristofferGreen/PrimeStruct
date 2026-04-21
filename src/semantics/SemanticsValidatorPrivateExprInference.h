#pragma once

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
  std::string inferStructReturnCollectionPath(const std::string &typeName,
                                              const std::string &typeTemplateArg) const;
  std::string resolveInferStructTypePath(const std::string &typeName,
                                         const std::string &namespacePrefix) const;
  bool isInferStructReturnNumericScalarExpr(
      const Expr &expr,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals);
  std::string inferStructReturnPointerTargetTypeText(
      const Expr &pointerExpr,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals);
  std::vector<std::string> inferStructReturnCollectionHelperPathCandidates(
      const std::string &path) const;
  void pruneInferStructReturnMapAccessCompatibilityCandidates(
      const std::string &path,
      std::vector<std::string> &candidates) const;
  void pruneInferStructReturnBuiltinVectorAccessCandidates(
      const Expr &candidate,
      const std::string &path,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers,
      std::vector<std::string> &candidates) const;
  bool isExplicitMapAccessStructReturnCompatibilityCall(
      const Expr &candidate,
      const BuiltinCollectionDispatchResolvers &dispatchResolvers) const;
  std::string specializedExperimentalMapStructReturnPath(
      const std::vector<std::string> &templateArgs) const;
  std::string inferMethodCollectionTypePathFromTypeText(const std::string &typeText) const;
  std::string resolveMethodStructTypePath(const std::string &typeName,
                                          const std::string &namespacePrefix) const;
  bool hasDefinitionFamilyPath(std::string_view path) const;
  std::string preferredFileHelperTarget(std::string_view helperName,
                                        std::string_view currentDefinitionPath) const;
  std::string preferredFileErrorHelperTarget(std::string_view helperName) const;
  std::string preferredImageErrorHelperTarget(std::string_view helperName) const;
  std::string preferredContainerErrorHelperTarget(std::string_view helperName) const;
  std::string preferredGfxErrorHelperTarget(std::string_view helperName,
                                            const std::string &resolvedStructPath) const;
  std::string preferredMapMethodTargetForCall(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &receiverExpr,
      const std::string &helperName);
  std::string preferredBufferMethodTargetForCall(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &receiverExpr,
      const std::string &helperName);
  std::string preferredExperimentalMapHelperTarget(std::string_view helperName) const;
  std::string preferredCanonicalExperimentalMapHelperTarget(std::string_view helperName) const;
  bool canonicalExperimentalMapHelperPath(const std::string &resolvedPath,
                                          std::string &canonicalPathOut,
                                          std::string &helperNameOut) const;
  bool canonicalizeExperimentalMapHelperResolvedPath(const std::string &resolvedPath,
                                                     std::string &canonicalPathOut) const;
  bool shouldLogicalCanonicalizeDefinedExperimentalMapHelperPath(
      const std::string &resolvedPath) const;
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
  std::string getRemovedRootedVectorDirectCallPath(
      const Expr &candidate) const;
  std::string getRemovedRootedVectorDirectCallDiagnostic(
      const Expr &candidate) const;
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
  BuiltinCollectionDispatchResolvers makeBuiltinCollectionDispatchResolvers(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const BuiltinCollectionDispatchResolverAdapters &adapters = {});
  void populateBuiltinCollectionDispatchBufferAndMapResolvers(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const std::shared_ptr<BuiltinCollectionDispatchResolvers> &state,
      const std::function<bool(const Expr &, BindingInfo &)> &resolveBindingTarget,
      const std::function<bool(const Expr &, BindingInfo &)> &inferCallBinding,
      const std::function<bool(const BindingInfo &, std::string &, std::string &)> &resolveMapBinding,
      const std::function<bool(const BindingInfo &, std::string &, std::string &)>
          &extractExperimentalMapFieldTypes,
      const std::function<bool(const Expr &)> &isDirectMapConstructorCall);
  void populateBuiltinCollectionDispatchStringResolver(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const std::shared_ptr<BuiltinCollectionDispatchResolvers> &state,
      const std::function<bool(const Expr &, BindingInfo &)> &resolveBindingTarget,
      const std::function<bool(const Expr &, size_t &)>
          &isDirectCanonicalVectorAccessCallOnBuiltinReceiver);
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
  bool isArrayNamespacedVectorAccessCompatibilityCall(
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
  bool extractExperimentalSoaColumnElementTypeFromStructPath(const std::string &structPath,
                                                             std::string &elemTypeOut) const;
  bool extractExperimentalSoaVectorElementTypeFromStructPath(const std::string &structPath,
                                                             std::string &elemTypeOut) const;
  bool extractExperimentalSoaVectorElementType(const BindingInfo &binding,
                                               std::string &elemTypeOut) const;
  bool resolveExperimentalBorrowedSoaTypeText(const std::string &typeText,
                                              std::string &elemTypeOut) const;
  bool resolveDirectSoaVectorOrExperimentalBorrowedReceiver(
      const Expr &target,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const std::function<bool(const Expr &, std::string &)> &resolveDirectReceiver,
      std::string &elemTypeOut);
  bool resolveSoaVectorOrExperimentalBorrowedReceiver(
      const Expr &target,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const std::function<bool(const Expr &, std::string &)> &resolveDirectReceiver,
      std::string &elemTypeOut);
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
    std::string countHelperName = "count";
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
    std::string countHelperName = "count";
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
  struct InferCollectionDispatchSetup {
    std::string builtinAccessName;
    bool hasBuiltinAccessSpelling = false;
    bool isBuiltinAccess = false;
    bool shouldDeferNamespacedVectorAccessCall = false;
    bool shouldDeferNamespacedMapAccessCall = false;
    bool shouldDeferResolvedNamespacedCollectionHelperReturn = false;
    bool hasPreferredBuiltinAccessKind = false;
    ReturnKind preferredBuiltinAccessKind = ReturnKind::Unknown;
    bool isStdNamespacedVectorAccessSpelling = false;
    bool shouldAllowStdAccessCompatibilityFallback = false;
    bool isStdNamespacedMapAccessSpelling = false;
    bool hasStdNamespacedMapAccessDefinition = false;
    bool shouldInferBuiltinBareMapContainsCall = true;
    bool shouldInferBuiltinBareMapTryAtCall = true;
    bool shouldInferBuiltinBareMapAccessCall = true;
    std::function<bool(const Expr &)> isIndexedArgsPackMapReceiverTarget;
    BuiltinCollectionCountCapacityDispatchContext builtinCollectionCountCapacityDispatchContext;
    BuiltinCollectionDirectCountCapacityContext builtinCollectionDirectCountCapacityContext;
  };
  struct InferLateFallbackBuiltinContext {
    std::string resolved;
    const InferCollectionDispatchSetup *collectionDispatchSetup = nullptr;
    const BuiltinCollectionDispatchResolvers *dispatchResolvers = nullptr;
    std::function<bool(const std::string &, std::string &)> resolveMethodCallPath;
  };
  struct InferResolvedCallContext {
    std::string resolved;
    const InferCollectionDispatchSetup *collectionDispatchSetup = nullptr;
    const BuiltinCollectionDispatchResolvers *dispatchResolvers = nullptr;
  };
  struct InferPreDispatchCallContext {
    std::string resolved;
    const BuiltinCollectionDispatchResolverAdapters *dispatchResolverAdapters =
        nullptr;
    const BuiltinCollectionDispatchResolvers *dispatchResolvers = nullptr;
  };
  void prepareInferCollectionDispatchSetup(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const std::string &resolved,
      const std::function<bool(const std::string &, std::string &)> &resolveMethodCallPath,
      const std::function<bool(const Expr &, std::string &)> &resolveArgsPackCountTarget,
      const std::function<bool(const std::string &, ReturnKind &)> &inferResolvedPathReturnKind,
      const BuiltinCollectionDispatchResolvers &builtinCollectionDispatchResolvers,
      InferCollectionDispatchSetup &setupOut);
  ReturnKind inferLateFallbackReturnKind(
      const Expr &expr,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const InferLateFallbackBuiltinContext &context,
      bool &handled);
  ReturnKind inferResolvedCallReturnKind(
      const Expr &expr,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const InferResolvedCallContext &context,
      bool &handled);
  ReturnKind inferPreDispatchCallReturnKind(
      const Expr &expr,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      InferPreDispatchCallContext &context,
      bool &handled);
  ReturnKind inferScalarBuiltinReturnKind(
      const Expr &expr,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      bool &handled);
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
