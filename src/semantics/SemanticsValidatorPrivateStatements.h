#pragma once

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
  void insertLocalBinding(std::unordered_map<std::string, BindingInfo> &locals,
                          const std::string &name,
                          BindingInfo info);
  uint64_t currentLocalBindingMemoRevision(const void *localsIdentity) const;
  void bumpLocalBindingMemoRevision(const void *localsIdentity);
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
  bool validatePickStatement(const std::vector<ParameterInfo> &params,
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
  bool isVectorStatementIntegerExpr(const std::vector<ParameterInfo> &params,
                                    const std::unordered_map<std::string, BindingInfo> &locals,
                                    const Expr &arg);
  bool resolveVectorStatementBinding(const std::vector<ParameterInfo> &params,
                                     const std::unordered_map<std::string, BindingInfo> &locals,
                                     const Expr &target,
                                     BindingInfo &bindingOut);
  bool validateVectorStatementElementType(const std::vector<ParameterInfo> &params,
                                          const std::unordered_map<std::string, BindingInfo> &locals,
                                          const Expr &arg,
                                          const std::string &typeName);
  bool validateVectorStatementHelperTarget(const std::vector<ParameterInfo> &params,
                                           const std::unordered_map<std::string, BindingInfo> &locals,
                                           const Expr &target,
                                           const char *helperName,
                                           BindingInfo &bindingOut);
  bool resolveVectorStatementHelperTargetPath(const std::vector<ParameterInfo> &params,
                                              const std::unordered_map<std::string, BindingInfo> &locals,
                                              const Expr &receiver,
                                              const std::string &helperName,
                                              std::string &resolvedOut);
  bool isNonCollectionStructVectorHelperTarget(const std::string &resolvedPath) const;
  bool validateVectorStatementHelperReceiver(const std::vector<ParameterInfo> &params,
                                             const std::unordered_map<std::string, BindingInfo> &locals,
                                             const Expr &receiver,
                                             const std::string &helperName);
  std::string preferVectorStdlibHelperPath(const std::string &path) const;
  bool statementAlwaysReturns(const Expr &stmt);
  bool blockAlwaysReturns(const std::vector<Expr> &statements);

  std::unordered_set<std::string> resolveEffects(const std::vector<Transform> &transforms, bool isEntry) const;
  bool validateCapabilitiesSubset(const std::vector<Transform> &transforms, const std::string &context);
  bool resolveExecutionEffects(const Expr &expr, std::unordered_set<std::string> &effectsOut);
  bool exprUsesName(const Expr &expr, const std::string &name) const;
  bool statementsUseNameFrom(const std::vector<Expr> &statements, size_t startIndex, const std::string &name) const;
  struct BorrowLivenessRange {
    const std::vector<Expr> *statements = nullptr;
    size_t startIndex = 0;
  };
  void expireReferenceBorrowsForRanges(const std::vector<ParameterInfo> &params,
                                       const std::unordered_map<std::string, BindingInfo> &locals,
                                       const std::vector<BorrowLivenessRange> &ranges);
  void expireReferenceBorrowsForRemainder(const std::vector<ParameterInfo> &params,
                                          const std::unordered_map<std::string, BindingInfo> &locals,
                                          const std::vector<Expr> &statements,
                                          size_t nextIndex);

  struct OnErrorHandler {
    std::string errorType;
    std::string handlerPath;
    std::vector<Expr> boundArgs;
  };
  struct ValidationContext {
    std::unordered_set<std::string> activeEffects;
    std::string definitionPath;
    bool definitionIsCompute = false;
    bool definitionIsUnsafe = false;
    std::optional<ResultTypeInfo> resultType;
    std::optional<OnErrorHandler> onError;
  };
  struct ValidationState {
    ValidationContext context;
    std::unordered_set<std::string> movedBindings;
    std::unordered_set<std::string> endedReferenceBorrows;
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
  std::string normalizeEffectFreeCollectionMethodName(const std::string &receiverPath,
                                                      std::string methodName) const;
  std::vector<std::string> effectFreeMethodPathCandidatesForReceiver(const std::string &receiverPath,
                                                                     const std::string &methodName) const;
  std::string preferEffectFreeCollectionHelperPath(const std::string &path) const;
  std::vector<std::string> effectFreeCollectionHelperPathCandidates(const std::string &path) const;
  std::string effectFreeCollectionPathFromType(const std::string &typeName,
                                               const std::string &typeTemplateArg) const;
  std::string effectFreeCollectionPathFromBinding(const BindingInfo &binding) const;
  std::string effectFreeCollectionPathFromCallExpr(const Expr &callExpr) const;
  std::string resolveEffectFreeBareMapCallPath(const Expr &callExpr, const EffectFreeContext &ctx) const;
  bool isOutsideEffectFreeStatement(const Expr &stmt, EffectFreeContext &ctx, bool &writesThis);
  bool isOutsideEffectFreeExpr(const Expr &expr, EffectFreeContext &ctx, bool &writesThis);
