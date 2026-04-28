#pragma once

  void forEachLocalAwareSnapshotCall(
      const std::function<void(const Definition &,
                               const std::vector<ParameterInfo> &,
                               const Expr &,
                               const std::unordered_map<std::string, BindingInfo> &)> &visitor);
  void forEachInferredQuerySnapshot(
      const std::function<void(const Definition &, const Expr &, QuerySnapshotData &&)> &visitor);
  void ensureQuerySnapshotFactCaches();
  void ensureCallAndTrySnapshotFactCaches(bool includeTryValues,
                                          bool includeCallBindings);
  void collectPilotRoutingSemanticProductFacts();
  void collectCallableSummaryEntriesForStableRange(
      std::size_t stableOrderOffset,
      std::size_t stableOrderCount,
      std::vector<CollectedCallableSummaryEntry> &out) const;
  void collectDefinitionPublicationFactsForStableRange(
      std::size_t stableOrderOffset,
      std::size_t stableOrderCount,
      SemanticPublicationSurface &out);
  void rebindMergedWorkerPublicationFactSemanticNodeIds();
  void collectExecutionCallableSummaryEntries(
      std::vector<CollectedCallableSummaryEntry> &out) const;
  void rebindCollectedCallableSummarySemanticNodeIds(
      std::vector<CollectedCallableSummaryEntry> &entries) const;
  void rebindCollectedOnErrorSemanticNodeIds(
      std::vector<OnErrorSnapshotEntry> &entries) const;
  static void sortCollectedCallableSummaries(
      std::vector<CollectedCallableSummaryEntry> &entries);
  void collectOnErrorSnapshotEntriesForStableRange(
      std::size_t stableOrderOffset,
      std::size_t stableOrderCount,
      std::vector<OnErrorSnapshotEntry> &out) const;
  static void sortCollectedOnErrorSnapshots(
      std::vector<OnErrorSnapshotEntry> &entries);
  void ensureOnErrorSnapshotFactCache() const;
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
  void rebuildCallResolutionFamilyIndexes();
  bool inferUnknownReturnKinds();
  bool inferUnknownReturnKindsGraph();
  void collectGraphLocalAutoBindings(const TypeResolutionGraph &graph);
  bool validateTraitConstraints();
  bool validateStructLayouts();
  bool validateDefinitions();
  bool validateDefinitionsFromStableIndexResolver(
      std::size_t stableCount,
      const std::function<std::size_t(std::size_t)> &resolveStableIndex);
  bool validateDefinitionsForStableIndices(const std::vector<std::size_t> &stableIndices);
  bool validateDefinitionsForStableRange(std::size_t stableOrderOffset,
                                         std::size_t stableOrderCount);
  bool runDefinitionValidationWorkerChunk(std::size_t stableOrderOffset,
                                          std::size_t stableOrderCount);
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
  bool isSumDefinition(const Definition &def) const;
  const Definition *resolveSumDefinitionForTypeText(
      const std::string &typeText,
      const std::string &namespacePrefix) const;
  bool validateExplicitSumConstructorExpr(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      bool &handledOut);
  bool validateTargetTypedSumInitializer(
      const std::string &targetTypeText,
      const Expr &initializer,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const std::string &namespacePrefix,
      bool &handledOut);
  bool inferExplicitSumConstructorBinding(
      const Expr &initializer,
      BindingInfo &bindingOut) const;

  std::string resolveCalleePath(const Expr &expr) const;
  std::string formatUnknownCallTarget(const Expr &expr) const;
  std::string diagnosticCallTargetPath(const std::string &path) const;
  bool isParam(const std::vector<ParameterInfo> &params, const std::string &name) const;
  const BindingInfo *findParamBinding(const std::vector<ParameterInfo> &params, const std::string &name) const;
  std::string typeNameForReturnKind(ReturnKind kind) const;
  std::string inferStructReturnPath(const Expr &expr,
                                    const std::vector<ParameterInfo> &params,
                                    const std::unordered_map<std::string, BindingInfo> &locals);
  std::string inferStructReturnPathImpl(const Expr &expr,
                                        const std::vector<ParameterInfo> &params,
                                        const std::unordered_map<std::string, BindingInfo> &locals);
  bool resolveReflectedStructEqualityHelperPath(
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals,
      const Expr &expr,
      const std::string &builtinName,
      std::string &helperPathOut) const;
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
  bool inferQuerySnapshotData(const std::vector<ParameterInfo> &defParams,
                              const std::unordered_map<std::string, BindingInfo> &activeLocals,
                              const Expr &expr,
                              QuerySnapshotData &out);
  bool inferCallSnapshotData(const std::vector<ParameterInfo> &defParams,
                             const std::unordered_map<std::string, BindingInfo> &activeLocals,
                             const Expr &expr,
                             CallSnapshotData &out);
  bool inferTrySnapshotData(const Definition &def,
                            const std::vector<ParameterInfo> &defParams,
                            const std::unordered_map<std::string, BindingInfo> &activeLocals,
                            const Expr &expr,
                            LocalAutoTrySnapshotData &out);
  bool inferBindingTypeFromInitializer(const Expr &initializer,
                                       const std::vector<ParameterInfo> &params,
                                       const std::unordered_map<std::string, BindingInfo> &locals,
                                       BindingInfo &bindingOut,
                                       const Expr *bindingExpr = nullptr);
  std::optional<std::string> builtinSoaAccessHelperName(
      const Expr &expr,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals) const;
  bool isBuiltinSoaFieldViewExpr(const Expr &expr,
                                 const std::vector<ParameterInfo> &params,
                                 const std::unordered_map<std::string, BindingInfo> &locals,
                                 std::string *fieldNameOut = nullptr) const;
  std::optional<std::string> builtinSoaDirectPendingHelperPath(
      const Expr &expr,
      const std::vector<ParameterInfo> &params,
      const std::unordered_map<std::string, BindingInfo> &locals) const;
  bool hasVisibleDefinitionPathForCurrentImports(
      std::string_view path) const;
  std::string preferredSoaHelperTargetForCurrentImports(
      std::string_view helperName) const;
  bool usesSamePathSoaHelperTargetForCurrentImports(
      std::string_view helperName) const;
  bool hasVisibleSoaHelperTargetForCurrentImports(
      std::string_view helperName) const;
  std::string preferredSoaHelperTargetForCollectionType(
      std::string_view helperName,
      std::string_view collectionTypePath) const;
  bool usesSamePathSoaHelperTargetForCollectionType(
      std::string_view helperName,
      std::string_view collectionTypePath) const;
  bool graphBindingIsUsable(const BindingInfo &binding) const;
  bool shouldBypassGraphBindingLookup(const Expr &candidate) const;
  bool hasDirectExperimentalVectorImport() const;
  bool canonicalizeInferredCollectionBinding(const Expr *sourceExpr,
                                             const std::vector<ParameterInfo> &params,
                                             const std::unordered_map<std::string, BindingInfo> &locals,
                                             BindingInfo &bindingOut);
  bool inferCollectionBindingFromExpr(const Expr &expr,
                                      const std::vector<ParameterInfo> &params,
                                      const std::unordered_map<std::string, BindingInfo> &locals,
                                      BindingInfo &bindingOut);
  bool inferBuiltinCollectionValueBinding(const Expr &expr,
                                          const std::vector<ParameterInfo> &params,
                                          const std::unordered_map<std::string, BindingInfo> &locals,
                                          BindingInfo &bindingOut);
  bool inferBuiltinPointerBinding(const Expr &expr,
                                  const std::vector<ParameterInfo> &params,
                                  const std::unordered_map<std::string, BindingInfo> &locals,
                                  BindingInfo &bindingOut);
  bool inferCallInitializerBinding(const Expr &initializer,
                                   const std::vector<ParameterInfo> &params,
                                   const std::unordered_map<std::string, BindingInfo> &locals,
                                   BindingInfo &bindingOut,
                                   const Expr *bindingExpr = nullptr);
  GraphLocalAutoKey graphLocalAutoBindingKey(std::string_view scopePath,
                                             int sourceLine,
                                             int sourceColumn) const;
  static std::pair<int, int> graphLocalAutoSourceLocation(const Expr &expr);
  bool lookupGraphLocalAutoBinding(const std::string &scopePath,
                                   const Expr &bindingExpr,
                                   BindingInfo &bindingOut) const;
  bool lookupGraphLocalAutoBinding(const Expr &bindingExpr, BindingInfo &bindingOut) const;
  bool lookupGraphLocalAutoDirectCallFact(const std::string &scopePath,
                                          const Expr &bindingExpr,
                                          std::string &resolvedPathOut,
                                          ReturnKind &returnKindOut) const;
  bool lookupGraphLocalAutoDirectCallFact(const Expr &bindingExpr,
                                          std::string &resolvedPathOut,
                                          ReturnKind &returnKindOut) const;
  bool lookupGraphLocalAutoMethodCallFact(const std::string &scopePath,
                                          const Expr &bindingExpr,
                                          std::string &resolvedPathOut,
                                          ReturnKind &returnKindOut) const;
  bool lookupGraphLocalAutoMethodCallFact(const Expr &bindingExpr,
                                          std::string &resolvedPathOut,
                                          ReturnKind &returnKindOut) const;
  std::string preferredCollectionHelperResolvedPath(const Expr &initializerCall) const;
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
  bool isEnvelopeValueExpr(const Expr &expr, bool allowAnyName) const;
  bool isSyntheticBlockValueBinding(const Expr &expr) const;
  const Expr *getEnvelopeValueExpr(const Expr &expr, bool allowAnyName) const;
  struct ValidationContext;
  struct ValidationState;
  bool makeDefinitionValidationContext(const Definition &def, ValidationContext &out);
  ValidationContext makeExecutionValidationContext(const Execution &exec) const;
  ValidationContext buildDefinitionValidationContext(const Definition &def) const;
  ValidationContext buildExecutionValidationContext(const Execution &exec) const;
  bool makeDefinitionValidationState(const Definition &def, ValidationState &out);
  ValidationState makeExecutionValidationState(const Execution &exec) const;
  ValidationState buildDefinitionValidationState(const Definition &def) const;
  ValidationState buildExecutionValidationState(const Execution &exec) const;
  void capturePrimarySpanIfUnset(int line, int column);
  void captureRelatedSpan(int line, int column, const std::string &label);
  void captureExprContext(const Expr &expr);
  void captureDefinitionContext(const Definition &def);
  void captureExecutionContext(const Execution &exec);
  bool failExprDiagnostic(const Expr &expr, std::string message);
  bool failDefinitionDiagnostic(const Definition &def, std::string message);
  bool failExecutionDiagnostic(const Execution &exec, std::string message);
  static bool definitionDiagnosticOrderLess(const Definition *left,
                                            const Definition *right);
  static void sortDefinitionsForDiagnosticOrder(std::vector<const Definition *> &definitions);
  static bool typeResolutionNodeDiagnosticOrderLess(const TypeResolutionGraphNode &left,
                                                    const TypeResolutionGraphNode &right);
  static void sortTypeResolutionNodesForDiagnosticOrder(
      std::vector<const TypeResolutionGraphNode *> &nodes);
  bool collectDuplicateDefinitionDiagnostics();
  bool shouldCollectStructuredDiagnostics() const;
  void clearStructuredDiagnosticContext();
  void moveCurrentStructuredDiagnosticTo(std::vector<SemanticDiagnosticRecord> &out);
  void rememberFirstCollectedDiagnosticMessage(const std::string &message);
  bool publishPassesDefinitionsDiagnostic(const Expr *expr = nullptr);
  bool failUncontextualizedDiagnostic(std::string message);
  bool publishCurrentStructuredDiagnosticNow();
  bool finalizeCollectedStructuredDiagnostics(std::vector<SemanticDiagnosticRecord> &records);
