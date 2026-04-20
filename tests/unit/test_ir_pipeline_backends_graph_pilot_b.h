  CHECK(validatorHeader.find("struct GraphLocalAutoKey {") != std::string::npos);
  CHECK(validatorHeader.find("std::unordered_map<GraphLocalAutoKey, GraphLocalAutoFacts, GraphLocalAutoKeyHash> graphLocalAutoFacts_;") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool ensureDefinitionReturnKindReady(const Definition &def);") != std::string::npos);
  CHECK(validatorHeader.find("bool inferDefinitionReturnKindGraphStep") != std::string::npos);
  CHECK(validatorHeader.find("graphTypeResolverEnabled_") == std::string::npos);
  CHECK(validatorHeader.find("bool inferDefinitionReturnBinding(const Definition &def, BindingInfo &bindingOut);") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool inferQueryExprTypeText(const Expr &expr,") != std::string::npos);
  CHECK(validatorHeader.find("std::string preferredExperimentalMapHelperTarget(std::string_view helperName) const;") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::string preferredCanonicalExperimentalMapHelperTarget(std::string_view helperName) const;") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool canonicalExperimentalMapHelperPath(const std::string &resolvedPath,") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool canonicalizeExperimentalMapHelperResolvedPath(const std::string &resolvedPath,") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool shouldBuiltinValidateCurrentMapWrapperHelper(std::string_view helperName) const;") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::string mapNamespacedMethodCompatibilityPath(") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::string directMapHelperCompatibilityPath(") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::string explicitRemovedCollectionMethodPath(std::string_view rawMethodName,") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool shouldPreserveRemovedCollectionHelperPath(const std::string &path) const;") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool isUnnamespacedMapCountBuiltinFallbackCall(") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool resolveRemovedMapBodyArgumentTarget(") !=
        std::string::npos);
  CHECK(validatorHeader.find("struct BuiltinCollectionDispatchResolverAdapters {") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::function<bool(const Expr &, BindingInfo &)> resolveBindingTarget;") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::function<bool(const Expr &, BindingInfo &)> inferCallBinding;") !=
        std::string::npos);
  CHECK(validatorHeader.find("BuiltinCollectionDispatchResolvers makeBuiltinCollectionDispatchResolvers(") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::function<bool(const Expr &, std::string &)> resolveIndexedArgsPackElementType;") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::function<bool(const Expr &, std::string &)> resolveDereferencedIndexedArgsPackElementType;") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::function<bool(const Expr &, std::string &)> resolveWrappedIndexedArgsPackElementType;") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::function<bool(const Expr &, std::string &)> resolveSoaVectorTarget;") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::function<bool(const Expr &, std::string &)> resolveBufferTarget;") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::function<bool(const Expr &, std::string &, std::string &)> resolveExperimentalMapTarget;") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::function<bool(const Expr &, std::string &, std::string &)> resolveExperimentalMapValueTarget;") !=
        std::string::npos);
  CHECK(validatorHeader.find("struct InferCollectionDispatchSetup") != std::string::npos);
  CHECK(validatorHeader.find("void prepareInferCollectionDispatchSetup(") != std::string::npos);
  CHECK(validatorHeader.find("struct InferLateFallbackBuiltinContext") != std::string::npos);
  CHECK(validatorHeader.find("ReturnKind inferLateFallbackReturnKind(") !=
        std::string::npos);
  CHECK(validatorHeader.find("struct InferPreDispatchCallContext") != std::string::npos);
  CHECK(validatorHeader.find("ReturnKind inferPreDispatchCallReturnKind(") !=
        std::string::npos);
  CHECK(validatorHeader.find("ReturnKind inferScalarBuiltinReturnKind(") !=
        std::string::npos);
  CHECK(validatorHeader.find("struct InferResolvedCallContext") != std::string::npos);
  CHECK(validatorHeader.find("ReturnKind inferResolvedCallReturnKind(") !=
        std::string::npos);
  CHECK(validatorBuild.find("bool SemanticsValidator::lookupGraphLocalAutoBinding(const std::string &scopePath,") !=
        std::string::npos);
  CHECK(validatorBuild.find("bool SemanticsValidator::lookupGraphLocalAutoBinding(const Expr &bindingExpr, BindingInfo &bindingOut) const {") !=
        std::string::npos);
  CHECK(validatorCollections.find("ValidationStateScope validationContextScope(*this, buildDefinitionValidationState(def));") !=
        std::string::npos);
  CHECK(validatorCollections.find("bool SemanticsValidator::inferQueryExprTypeText(const Expr &expr,") !=
        std::string::npos);
  CHECK(validatorCollections.find("SemanticsValidator::makeBuiltinCollectionDispatchResolvers(") !=
        std::string::npos);
  CHECK(validatorCollections.find("const BuiltinCollectionDispatchResolverAdapters &adapters") !=
        std::string::npos);
  CHECK(validatorCollections.find("auto resolveBindingTarget = [") !=
        std::string::npos);
  CHECK(validatorCollections.find("auto inferCallBinding = [") !=
        std::string::npos);
  CHECK(validatorCollections.find("auto isDirectMapConstructorCall = [") !=
        std::string::npos);
  CHECK(validatorCollections.find("state->resolveSoaVectorTarget = [") !=
        std::string::npos);
  CHECK(validatorCollections.find("state->resolveBufferTarget = [") !=
        std::string::npos);
  CHECK(validatorCollections.find("state->resolveExperimentalMapTarget =") != std::string::npos);
  CHECK(validatorCollections.find("state->resolveExperimentalMapValueTarget =") != std::string::npos);
  CHECK(validatorCollections.find("resolvePublishedCollectionHelperMemberToken(") !=
        std::string::npos);
  CHECK(validatorCollections.find(
            "inline bool isStdNamespacedVectorCompatibilityHelperPath(") !=
        std::string::npos);
  CHECK(validatorCollections.find(
            "inline std::string classifyStdNamespacedVectorCountDiagnosticMessage(") !=
        std::string::npos);
  CHECK(validatorCollections.find("SemanticsVectorCompatibilityHelpers.h") ==
        std::string::npos);
  CHECK(validatorCollections.find("preferredPublishedCollectionLoweringPath(") !=
        std::string::npos);
  CHECK(validatorCollections.find("resolveCanonicalCompatibilityMapHelperNameFromResolvedPath(") !=
        std::string::npos);
  CHECK(validatorCollections.find("std::string SemanticsValidator::preferredExperimentalMapHelperTarget(std::string_view helperName) const {") !=
        std::string::npos);
  CHECK(validatorCollections.find("std::string SemanticsValidator::preferredCanonicalExperimentalMapHelperTarget(std::string_view helperName) const {") !=
        std::string::npos);
  CHECK(validatorCollections.find("bool SemanticsValidator::canonicalExperimentalMapHelperPath(const std::string &resolvedPath,") !=
        std::string::npos);
  CHECK(validatorCollections.find("bool SemanticsValidator::canonicalizeExperimentalMapHelperResolvedPath(const std::string &resolvedPath,") !=
        std::string::npos);
  CHECK(validatorCollections.find("bool SemanticsValidator::shouldBuiltinValidateCurrentMapWrapperHelper(std::string_view helperName) const {") !=
        std::string::npos);
  CHECK(validatorCollections.find("std::string SemanticsValidator::mapNamespacedMethodCompatibilityPath(") !=
        std::string::npos);
  CHECK(validatorCollections.find("std::string SemanticsValidator::directMapHelperCompatibilityPath(") !=
        std::string::npos);
  CHECK(validatorCollections.find("enum class RemovedCollectionHelperFamily {") != std::string::npos);
  CHECK(validatorCollections.find("std::string SemanticsValidator::explicitRemovedCollectionMethodPath(std::string_view rawMethodName,") !=
        std::string::npos);
  CHECK(validatorCollections.find("bool SemanticsValidator::shouldPreserveRemovedCollectionHelperPath(const std::string &path) const {") !=
        std::string::npos);
  CHECK(validatorCollections.find("bool SemanticsValidator::isUnnamespacedMapCountBuiltinFallbackCall(") !=
        std::string::npos);
  CHECK(validatorCollections.find("struct ExperimentalMapHelperDescriptor {") ==
        std::string::npos);
  CHECK(validatorCollections.find("constexpr ExperimentalMapHelperDescriptor kExperimentalMapHelperDescriptors[] = {") ==
        std::string::npos);
  CHECK(validatorCollections.find("struct RemovedCollectionHelperDescriptor {") ==
        std::string::npos);
  CHECK(validatorCollections.find("constexpr RemovedCollectionHelperDescriptor kRemovedCollectionHelperDescriptors[] = {") ==
        std::string::npos);
  CHECK(validatorCollections.find("findRemovedCollectionHelperReference(") ==
        std::string::npos);
  CHECK(validatorCollections.find("bool SemanticsValidator::resolveRemovedMapBodyArgumentTarget(") !=
        std::string::npos);
  CHECK(validatorCollections.find("if (inferQueryExprTypeText(target, params, locals, targetTypeText))") !=
        std::string::npos);
  CHECK(validatorBuild.find("bool SemanticsValidator::inferResolvedDirectCallBindingType(") !=
        std::string::npos);
  CHECK(validatorCore.find("return inferQueryExprTypeText(receiverExpr, params, locals, typeTextOut);") !=
        std::string::npos);
  CHECK(validatorCore.find("auto isEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> bool {") ==
        std::string::npos);
  CHECK(validatorCore.find("inferExprTypeText(stmt.args.front(), defParams, defLocals, inferredLocalType)") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveCallCollectionTypePath = [&](const Expr &target, std::string &typePathOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveCallCollectionTemplateArgs =") == std::string::npos);
  CHECK(validatorExprMain.find("prepareExprDispatchBootstrap(params, locals, dispatchBootstrap);") !=
        std::string::npos);
  CHECK(validatorExprMain.find("validateExprPreDispatchDirectCalls(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("prepareExprMethodCompatibilitySetup(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("const BuiltinCollectionDispatchResolverAdapters builtinCollectionDispatchResolverAdapters{") ==
        std::string::npos);
  CHECK(validatorExprMain.find("const BuiltinCollectionDispatchResolvers builtinCollectionDispatchResolvers =") ==
        std::string::npos);
  CHECK(validatorExprMain.find("makeBuiltinCollectionDispatchResolvers(params, locals, builtinCollectionDispatchResolverAdapters)") ==
        std::string::npos);
  CHECK(validatorExpr.find("void SemanticsValidator::prepareExprDispatchBootstrap(") !=
        std::string::npos);
  CHECK(validatorExpr.find("bootstrapOut.dispatchResolverAdapters = {") !=
        std::string::npos);
  CHECK(validatorExpr.find("bootstrapOut.dispatchResolvers = makeBuiltinCollectionDispatchResolvers(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("auto isDeclaredPointerLikeCall = [&](const Expr &candidate) -> bool {") ==
        std::string::npos);
  CHECK(validatorExpr.find("bootstrapOut.isDeclaredPointerLikeCall = [this](const Expr &candidate) -> bool {") !=
        std::string::npos);
  CHECK(validatorExpr.find("bool SemanticsValidator::validateExprPreDispatchDirectCalls(") !=
        std::string::npos);
  CHECK(validatorExpr.find("bool SemanticsValidator::prepareExprMethodCompatibilitySetup(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("std::string canonicalExperimentalMapHelperResolved;") ==
        std::string::npos);
  CHECK(validatorExpr.find("std::string canonicalExperimentalMapHelperResolved;") !=
        std::string::npos);
  CHECK(validatorExprMain.find("tryRewriteCanonicalExperimentalVectorHelperCall(") ==
        std::string::npos);
  CHECK(validatorExpr.find("tryRewriteCanonicalExperimentalVectorHelperCall(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("explicitCanonicalExperimentalMapBorrowedHelperPath(") ==
        std::string::npos);
  CHECK(validatorExpr.find("explicitCanonicalExperimentalMapBorrowedHelperPath(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("auto setCanonicalMapKeyMismatch =") ==
        std::string::npos);
  CHECK(validatorExpr.find("auto setCanonicalMapKeyMismatch =") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto isExperimentalMapReceiverExpr = [&](const Expr &candidate)") ==
        std::string::npos);
  CHECK(validatorExpr.find("auto isExperimentalMapReceiverExpr = [&](const Expr &candidate)") !=
        std::string::npos);
  CHECK(validatorExprMain.find("const std::string methodReflectionTarget =") ==
        std::string::npos);
  CHECK(validatorExpr.find("const std::string methodReflectionTarget =") !=
        std::string::npos);
  CHECK(validatorExprMain.find("const std::string removedVectorCompatibilityPath =") ==
        std::string::npos);
  CHECK(validatorExpr.find("const std::string removedVectorCompatibilityPath =") ==
        std::string::npos);
  CHECK(validatorExprMain.find("const std::string removedMapCompatibilityPath =") ==
        std::string::npos);
  CHECK(validatorExpr.find("const std::string removedMapCompatibilityPath =") !=
        std::string::npos);
  CHECK(validatorExprMain.find("auto isKnownCollectionTarget = [&](const Expr &targetExpr) -> bool {") ==
        std::string::npos);
  CHECK(validatorExpr.find("auto isKnownCollectionTarget = [this, &dispatchBootstrap](") !=
        std::string::npos);
  CHECK(validatorExpr.find("auto resolveIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") !=
        std::string::npos);
  CHECK(validatorExpr.find("auto resolveDereferencedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") !=
        std::string::npos);
  CHECK(validatorExpr.find("auto resolveWrappedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") !=
        std::string::npos);
  CHECK(validatorExprMain.find("const auto &resolveArgsPackAccessTarget = builtinCollectionDispatchResolvers.resolveArgsPackAccessTarget;") ==
        std::string::npos);
  CHECK(validatorExprMain.find("const auto &resolveArrayTarget = builtinCollectionDispatchResolvers.resolveArrayTarget;") ==
        std::string::npos);
  CHECK(validatorExprMain.find("const auto &resolveVectorTarget = builtinCollectionDispatchResolvers.resolveVectorTarget;") ==
        std::string::npos);
  CHECK(validatorExprMain.find("const auto &resolveSoaVectorTarget = builtinCollectionDispatchResolvers.resolveSoaVectorTarget;") ==
        std::string::npos);
  CHECK(validatorExprMain.find("const auto &resolveStringTarget = builtinCollectionDispatchResolvers.resolveStringTarget;") ==
        std::string::npos);
  CHECK(validatorExprMain.find("const auto &resolveMapTargetWithTypes = builtinCollectionDispatchResolvers.resolveMapTarget;") ==
        std::string::npos);
  CHECK(validatorExprMain.find("const auto &resolveExperimentalMapTarget =") == std::string::npos);
  CHECK(validatorExprMain.find("builtinCollectionDispatchResolvers.resolveExperimentalMapTarget;") ==
        std::string::npos);
  CHECK(validatorExpr.find("bootstrapOut.resolveMapTarget = [this, paramsPtr, localsPtr, &bootstrapOut](const Expr &target) -> bool {") !=
        std::string::npos);
  CHECK(validatorExpr.find("bootstrapOut.dispatchResolvers.resolveExperimentalMapTarget(target, keyType,") !=
        std::string::npos);
  CHECK(validatorExpr.find("dispatchResolvers.resolveExperimentalMapValueTarget == nullptr") != std::string::npos);
  CHECK(validatorExpr.find("dispatchResolvers.resolveExperimentalMapValueTarget(receiverExpr, keyType, valueType)") !=
        std::string::npos);
  CHECK(validatorExprMain.find("auto preferredExperimentalMapHelperTarget = [&](std::string_view helperName) {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto preferredCanonicalExperimentalMapHelperTarget = [&](std::string_view helperName) {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto preferredCanonicalExperimentalMapReferenceHelperTarget = [&](std::string_view helperName) {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto canonicalExperimentalMapHelperPath = [&](const std::string &resolvedPath, std::string &canonicalPathOut, std::string &helperNameOut) {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto canonicalizeExperimentalMapHelperResolvedPath = [&](const std::string &resolvedPath,") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto shouldBuiltinValidateCurrentMapWrapperHelper = [&](std::string_view helperName) {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto isMapNamespacedCountCompatibilityCall = [&](const Expr &candidate) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto isMapNamespacedAccessCompatibilityCall = [&](const Expr &candidate) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto getMapNamespacedMethodCompatibilityPath = [&](const Expr &candidate) -> std::string {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto getDirectMapHelperCompatibilityPath = [&](const Expr &candidate) -> std::string {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto isUnnamespacedMapCountBuiltinFallbackCall = [&](const Expr &candidate) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto explicitRemovedCollectionMethodPath = [&](const std::string &rawMethodName) -> std::string {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto shouldPreserveBodyArgumentTarget = [&](const std::string &path) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto isRemovedVectorCompatibilityHelper = [](std::string_view helperName) {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto isRemovedMapCompatibilityHelper = [](std::string_view helperName) {") ==
        std::string::npos);
  CHECK(validatorExpr.find("preferredCanonicalExperimentalMapHelperTarget(helperName)") != std::string::npos);
  CHECK(validatorExpr.find("this->preferredCanonicalExperimentalMapHelperTarget(") !=
        std::string::npos);
  CHECK(validatorExpr.find("canonicalExperimentalMapHelperPath(") != std::string::npos);
  CHECK(validatorExpr.find("canonicalizeExperimentalMapHelperResolvedPath(") != std::string::npos);
  CHECK(validatorExpr.find("shouldBuiltinValidateCurrentMapWrapperHelper(") != std::string::npos);
  CHECK(validatorHeader.find("bool validateExprMethodCallTarget(") != std::string::npos);
  CHECK(validatorExprMain.find("ExprMethodResolutionContext methodResolutionContext;") !=
        std::string::npos);
  CHECK(validatorExprMain.find("validateExprMethodCallTarget(") != std::string::npos);
  CHECK(validatorExprMain.find("mapNamespacedMethodCompatibilityPath(expr, params, locals, builtinCollectionDispatchResolverAdapters)") ==
        std::string::npos);
  CHECK(validatorExpr.find("mapNamespacedMethodCompatibilityPath(expr, params, locals, dispatchResolverAdapters)") !=
        std::string::npos);
  CHECK(validatorExprMain.find("failMethodResolutionDiagnostic(\"method call missing receiver\")") ==
        std::string::npos);
  CHECK(validatorExpr.find("auto failMethodResolutionDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(validatorExpr.find("failMethodResolutionDiagnostic(\"method call missing receiver\")") !=
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveInferredMapMethodFallback = [&]() -> bool {") ==
        std::string::npos);
  CHECK(validatorExpr.find("auto resolveInferredMapMethodFallback = [&]() -> bool {") !=
        std::string::npos);
  CHECK(validatorExpr.find("directMapHelperCompatibilityPath(") != std::string::npos);
  CHECK(validatorExpr.find("explicitRemovedCollectionMethodPath(methodName)") !=
        std::string::npos);
  CHECK(validatorExpr.find("shouldPreserveRemovedCollectionHelperPath(resolved)") ==
        std::string::npos);
  CHECK(validatorExprBodyArguments.find("shouldPreserveRemovedCollectionHelperPath(resolved)") !=
        std::string::npos);
  CHECK(validatorExprMain.find("isUnnamespacedMapCountBuiltinFallbackCall(expr, params, locals, dispatchBootstrap.dispatchResolverAdapters)") ==
        std::string::npos);
  CHECK(validatorExpr.find("isUnnamespacedMapCountBuiltinFallbackCall(expr, params, locals, dispatchResolverAdapters)") !=
        std::string::npos);
  CHECK(validatorExpr.find("resolveRemovedMapBodyArgumentTarget(") ==
        std::string::npos);
  CHECK(validatorExprBodyArguments.find("resolveRemovedMapBodyArgumentTarget(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveBareMapCallBodyArgumentTarget = [&]() -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto remapWrappedMapMethodBodyArgumentTarget = [&]() -> bool {") ==
        std::string::npos);
  CHECK(validatorHeader.find("struct ExprDispatchBootstrap") != std::string::npos);
  CHECK(validatorHeader.find("void prepareExprDispatchBootstrap(") != std::string::npos);
  CHECK(validatorHeader.find("struct ExprPreDispatchDirectCallContext") != std::string::npos);
  CHECK(validatorHeader.find("bool validateExprPreDispatchDirectCalls(") !=
        std::string::npos);
  CHECK(validatorHeader.find("struct ExprMethodCompatibilitySetup") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool prepareExprMethodCompatibilitySetup(") !=
        std::string::npos);
  CHECK(validatorHeader.find("struct ExprCollectionDispatchSetup") != std::string::npos);
  CHECK(validatorHeader.find("bool prepareExprCollectionDispatchSetup(") != std::string::npos);
  CHECK(validatorHeader.find("void prepareExprCollectionAccessDispatchContext(") !=
        std::string::npos);
  CHECK(validatorHeader.find("struct ExprDirectCollectionFallbackContext") != std::string::npos);
  CHECK(validatorHeader.find("bool validateExprDirectCollectionFallbacks(") != std::string::npos);
  CHECK(validatorHeader.find("bool validateExprPostAccessPrechecks(") !=
        std::string::npos);
  CHECK(validatorHeader.find("void prepareExprNamedArgumentBuiltinContext(") !=
        std::string::npos);
  CHECK(validatorHeader.find("void prepareExprLateBuiltinContext(") !=
        std::string::npos);
  CHECK(validatorHeader.find("void prepareExprResolvedCallSetup(") !=
        std::string::npos);
  CHECK(validatorHeader.find("struct ExprLateUnknownTargetFallbackContext") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool validateExprLateUnknownTargetFallbacks(") !=
        std::string::npos);
  CHECK(validatorHeader.find("void prepareExprCountCapacityMapBuiltinContext(") !=
        std::string::npos);
  CHECK(validatorHeader.find("void prepareExprLateMapSoaBuiltinContext(") !=
        std::string::npos);
  CHECK(validatorHeader.find("void prepareExprLateFallbackBuiltinContext(") !=
        std::string::npos);
  CHECK(validatorHeader.find("void prepareExprLateCallCompatibilityContext(") !=
        std::string::npos);
  CHECK(validatorHeader.find("void prepareExprLateMapAccessBuiltinContext(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("const std::string directRemovedMapCompatibilityPath =") ==
        std::string::npos);
  CHECK(validatorExpr.find("const std::string directRemovedMapCompatibilityPath =") !=
        std::string::npos);
  CHECK(validatorExprMain.find("prepareExprDispatchBootstrap(") != std::string::npos);
