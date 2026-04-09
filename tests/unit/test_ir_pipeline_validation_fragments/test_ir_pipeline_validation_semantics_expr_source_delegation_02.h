  CHECK(semanticsExprDispatchBootstrapSource.find("void SemanticsValidator::prepareExprDispatchBootstrap(") !=
        std::string::npos);
  CHECK(semanticsExprDispatchBootstrapSource.find("bootstrapOut.dispatchResolverAdapters = {") !=
        std::string::npos);
  CHECK(semanticsExprDispatchBootstrapSource.find("bootstrapOut.dispatchResolvers = makeBuiltinCollectionDispatchResolvers(") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isDeclaredPointerLikeCall = [&](const Expr &candidate) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprDispatchBootstrapSource.find(
            "bootstrapOut.isDeclaredPointerLikeCall = [this](const Expr &candidate) -> bool {") !=
        std::string::npos);
  CHECK(semanticsExprPreDispatchDirectCallsSource.find(
            "bool SemanticsValidator::validateExprPreDispatchDirectCalls(") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("std::string canonicalExperimentalMapHelperResolved;") ==
        std::string::npos);
  CHECK(semanticsExprPreDispatchDirectCallsSource.find(
            "std::string canonicalExperimentalMapHelperResolved;") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("tryRewriteCanonicalExperimentalVectorHelperCall(") ==
        std::string::npos);
  CHECK(semanticsExprPreDispatchDirectCallsSource.find(
            "tryRewriteCanonicalExperimentalVectorHelperCall(") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("explicitCanonicalExperimentalMapBorrowedHelperPath(") ==
        std::string::npos);
  CHECK(semanticsExprPreDispatchDirectCallsSource.find(
            "explicitCanonicalExperimentalMapBorrowedHelperPath(") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("auto failPreDispatchDirectCallMapKeyMismatch =") ==
        std::string::npos);
  CHECK(semanticsExprPreDispatchDirectCallsSource.find(
            "auto failPreDispatchDirectCallMapKeyMismatch =") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isExperimentalMapReceiverExpr = [&](const Expr &candidate)") ==
        std::string::npos);
  CHECK(semanticsExprPreDispatchDirectCallsSource.find(
            "auto isExperimentalMapReceiverExpr = [&](const Expr &candidate)") !=
        std::string::npos);
  CHECK(semanticsExprMethodCompatibilitySetupSource.find(
            "bool SemanticsValidator::prepareExprMethodCompatibilitySetup(") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("const std::string methodReflectionTarget =") ==
        std::string::npos);
  CHECK(semanticsExprMethodCompatibilitySetupSource.find(
            "const std::string methodReflectionTarget =") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("const std::string removedVectorCompatibilityPath =") ==
        std::string::npos);
  CHECK(semanticsExprMethodCompatibilitySetupSource.find(
            "const std::string removedVectorCompatibilityPath =") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("const std::string removedMapCompatibilityPath =") ==
        std::string::npos);
  CHECK(semanticsExprMethodCompatibilitySetupSource.find(
            "const std::string removedMapCompatibilityPath =") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isKnownCollectionTarget = [&](const Expr &targetExpr) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprMethodCompatibilitySetupSource.find(
            "auto isKnownCollectionTarget = [this, &dispatchBootstrap](") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("const auto &resolveIndexedArgsPackElementType =") == std::string::npos);
  CHECK(semanticsExprSource.find("builtinCollectionDispatchResolvers.resolveIndexedArgsPackElementType;") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("const auto &resolveDereferencedIndexedArgsPackElementType =") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("builtinCollectionDispatchResolvers.resolveDereferencedIndexedArgsPackElementType;") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("const auto &resolveWrappedIndexedArgsPackElementType =") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("builtinCollectionDispatchResolvers.resolveWrappedIndexedArgsPackElementType;") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("const auto &resolveArgsPackAccessTarget = builtinCollectionDispatchResolvers.resolveArgsPackAccessTarget;") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("const auto &resolveArrayTarget = builtinCollectionDispatchResolvers.resolveArrayTarget;") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("const auto &resolveVectorTarget = builtinCollectionDispatchResolvers.resolveVectorTarget;") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("const auto &resolveSoaVectorTarget = builtinCollectionDispatchResolvers.resolveSoaVectorTarget;") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("const auto &resolveStringTarget = builtinCollectionDispatchResolvers.resolveStringTarget;") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("const auto &resolveMapTargetWithTypes = builtinCollectionDispatchResolvers.resolveMapTarget;") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("const auto &resolveExperimentalMapTarget =") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("builtinCollectionDispatchResolvers.resolveExperimentalMapTarget;") ==
        std::string::npos);
  CHECK(semanticsExprDispatchBootstrapSource.find(
            "bootstrapOut.resolveMapTarget = [this, paramsPtr, localsPtr, &bootstrapOut](const Expr &target) -> bool {") !=
        std::string::npos);
  CHECK(semanticsExprDispatchBootstrapSource.find("bootstrapOut.dispatchResolvers.resolveExperimentalMapTarget(target, keyType,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("const auto &resolveExperimentalMapValueTarget =") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("builtinCollectionDispatchResolvers.resolveExperimentalMapValueTarget;") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto preferredExperimentalMapHelperTarget = [&](std::string_view helperName) {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto preferredCanonicalExperimentalMapHelperTarget = [&](std::string_view helperName) {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto canonicalExperimentalMapHelperPath = [&](const std::string &resolvedPath, std::string &canonicalPathOut, std::string &helperNameOut) {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto canonicalizeExperimentalMapHelperResolvedPath = [&](const std::string &resolvedPath,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto shouldBuiltinValidateCurrentMapWrapperHelper = [&](std::string_view helperName) {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isMapNamespacedCountCompatibilityCall = [&](const Expr &candidate) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isMapNamespacedAccessCompatibilityCall = [&](const Expr &candidate) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto getMapNamespacedMethodCompatibilityPath = [&](const Expr &candidate) -> std::string {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto getDirectMapHelperCompatibilityPath = [&](const Expr &candidate) -> std::string {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isUnnamespacedMapCountBuiltinFallbackCall = [&](const Expr &candidate) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto explicitRemovedCollectionMethodPath = [&](const std::string &rawMethodName) -> std::string {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto shouldPreserveBodyArgumentTarget = [&](const std::string &path) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isRemovedVectorCompatibilityHelper = [](std::string_view helperName) {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isRemovedMapCompatibilityHelper = [](std::string_view helperName) {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("std::function<bool(const Expr &)> isUnsafeReferenceExpr;") ==
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "this->mapNamespacedMethodCompatibilityPath(expr, params, locals, builtinCollectionDispatchResolverAdapters)") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "this->mapNamespacedMethodCompatibilityPath(expr, params, locals, dispatchResolverAdapters)") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("failMethodResolutionDiagnostic(\"method call missing receiver\")") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "auto failMethodResolutionDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "failMethodResolutionDiagnostic(\"method call missing receiver\")") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveInferredMapMethodFallback = [&]() -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find("auto resolveInferredMapMethodFallback = [&]() -> bool {") !=
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find("setCollectionMethodTarget(\"/soa_vector/count\")") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "preferredSoaHelperTargetForCollectionType(\"count\", \"/vector\")") !=
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "preferredSoaHelperTargetForCollectionType(") != std::string::npos);
  CHECK((semanticsExprMethodTargetResolutionSource.find(
            "\"ref\", \"/soa_vector\"") != std::string::npos ||
         semanticsExprMethodTargetResolutionSource.find(
             "normalizedMethodName, \"/soa_vector\"") != std::string::npos));
  CHECK((semanticsExprMethodTargetResolutionSource.find(
            "\"to_aos\", \"/vector\"") != std::string::npos ||
         semanticsExprMethodTargetResolutionSource.find(
             "normalizedMethodName, \"/vector\"") != std::string::npos));
  CHECK(semanticsExprMethodTargetResolutionSource.find("setCollectionMethodTarget(\"/to_aos\")") ==
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "auto resolveVectorMutatorName = [&](const std::string &name, std::string &helperOut) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "validateNamedArguments(expr.args, expr.argNames, resolved, error_)") ==
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "resolveEscapingReferenceRoot = [&](const Expr &argExpr, std::string &rootOut) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "auto reportReferenceAssignmentEscape = [&](const std::string &sinkName, const Expr &rhsExpr) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "auto resolveReferenceEscapeSink = [&](const std::string &targetName, std::string &sinkOut) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("this->preferredCanonicalExperimentalMapHelperTarget(helperName)") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("this->canonicalExperimentalMapHelperPath(") == std::string::npos);
  CHECK(semanticsExprSource.find("this->canonicalizeExperimentalMapHelperResolvedPath(") ==
        std::string::npos);
  CHECK(semanticsExprPreDispatchDirectCallsSource.find(
            "this->canonicalizeExperimentalMapHelperResolvedPath(") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("this->directMapHelperCompatibilityPath(") ==
        std::string::npos);
  CHECK(semanticsExprBuiltinContextSetupSource.find(
            "this->directMapHelperCompatibilityPath(") != std::string::npos);
  CHECK(semanticsExprSource.find("this->shouldPreserveRemovedCollectionHelperPath(resolved)") ==
        std::string::npos);
  CHECK(semanticsExprBodyArgumentsSource.find(
            "this->shouldPreserveRemovedCollectionHelperPath(resolved)") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "this->isUnnamespacedMapCountBuiltinFallbackCall(expr, params, locals, dispatchBootstrap.dispatchResolverAdapters)") ==
        std::string::npos);
  CHECK(semanticsExprCollectionDispatchSetupSource.find(
            "this->isUnnamespacedMapCountBuiltinFallbackCall(expr, params, locals, dispatchResolverAdapters)") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolvesExperimentalVectorValueReceiverForBareAccess =") ==
        std::string::npos);
  CHECK(semanticsExprDirectCollectionFallbacksSource.find(
            "auto resolvesExperimentalVectorValueReceiverForBareAccess =") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("Expr rewrittenVectorHelperCall;") == std::string::npos);
  CHECK(semanticsExprDirectCollectionFallbacksSource.find("Expr rewrittenVectorHelperCall;") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "if (!expr.isMethodCall && context.isStdNamespacedVectorCountCall &&") ==
        std::string::npos);
  CHECK(semanticsExprDirectCollectionFallbacksSource.find(
            "if (!expr.isMethodCall && context.isStdNamespacedVectorCountCall &&") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("collectionAccessDispatchContext.resolveArrayTarget =") ==
        std::string::npos);
  CHECK(semanticsExprCollectionAccessSetupSource.find(
            "contextOut.resolveArrayTarget = dispatchResolvers.resolveArrayTarget;") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "collectionAccessDispatchContext.isIndexedArgsPackMapReceiverTarget =") ==
        std::string::npos);
  CHECK(semanticsExprCollectionAccessSetupSource.find(
            "contextOut.isIndexedArgsPackMapReceiverTarget = [&](const Expr &target)") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("this->resolveRemovedMapBodyArgumentTarget(") ==
        std::string::npos);
  CHECK(semanticsExprBodyArgumentsSource.find("this->resolveRemovedMapBodyArgumentTarget(") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "validateExprBodyArguments(params, locals, expr, resolved, resolvedMethod,") ==
        std::string::npos);
  CHECK(semanticsExprPostAccessPrechecksSource.find(
            "validateExprBodyArguments(params, locals, expr, resolved, resolvedMethod,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "validateExprNamedArguments(params, locals, expr, resolved,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "namedArgumentBuiltinContext.hasVectorHelperCallResolution =") ==
        std::string::npos);
  CHECK(semanticsExprBuiltinContextSetupSource.find(
            "contextOut.hasVectorHelperCallResolution = hasVectorHelperCallResolution;") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "lateBuiltinContext.tryBuiltinContext.getDirectMapHelperCompatibilityPath =") ==
        std::string::npos);
  CHECK(semanticsExprBuiltinContextSetupSource.find(
            "contextOut.tryBuiltinContext.getDirectMapHelperCompatibilityPath =") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "countCapacityMapBuiltinContext.shouldBuiltinValidateStdNamespacedVectorCountCall =") ==
        std::string::npos);
  CHECK(semanticsExprBuiltinContextSetupSource.find(
            "contextOut.shouldBuiltinValidateStdNamespacedVectorCountCall =") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "lateFallbackBuiltinContext.collectionAccessFallbackContext.isStdNamespacedVectorAccessCall =") ==
        std::string::npos);
  CHECK(semanticsExprBuiltinContextSetupSource.find(
            "contextOut.collectionAccessFallbackContext.isStdNamespacedVectorAccessCall =") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "lateMapAccessBuiltinContext.shouldBuiltinValidateBareMapTryAtCall =") ==
        std::string::npos);
  CHECK(semanticsExprBuiltinContextSetupSource.find(
            "contextOut.shouldBuiltinValidateBareMapTryAtCall =") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("std::string gpuBuiltin;") == std::string::npos);
  CHECK(semanticsExprPostAccessPrechecksSource.find("std::string gpuBuiltin;") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("PathSpaceBuiltin pathSpaceBuiltin;") ==
        std::string::npos);
  CHECK(semanticsExprPostAccessPrechecksSource.find("PathSpaceBuiltin pathSpaceBuiltin;") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "if (!resolvedMethod && resolved == \"/file_error/why\" &&") ==
        std::string::npos);
  CHECK(semanticsExprPostAccessPrechecksSource.find(
            "if (!resolvedMethod && resolved == \"/file_error/why\" &&") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("isUnsafeReferenceExpr(params, locals, expr.args[1])") ==
        std::string::npos);
  CHECK(semanticsExprMutationBorrowsSource.find(
            "isUnsafeReferenceExpr(params, locals, expr.args[1])") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "reportReferenceAssignmentEscape(params, locals, escapeSink,") ==
        std::string::npos);
  CHECK(semanticsExprMutationBorrowsSource.find(
            "reportReferenceAssignmentEscape(params, locals, escapeSink,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("validateExprCollectionLiteralBuiltins(params, locals, expr,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("validateExprLateBuiltins(params, locals, expr, resolved,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("validateExprCountCapacityMapBuiltins(") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("validateExprTryBuiltin(params, locals, expr,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("validateExprResultFileBuiltins(params, locals, expr, resolved,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("std::string logicalResolvedMethod = resolved;") ==
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "auto usesCanonicalMapReceiver = [&](const Expr &receiverExpr)") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("validateExprNamedArgumentBuiltins(") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("validateExprGpuBufferBuiltins(params, locals, expr,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("validateExprScalarPointerMemoryBuiltins(") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("validateExprLateMapSoaBuiltins(") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("validateExprMapSoaBuiltins(") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("validateExprLateCollectionAccessFallbacks(") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("validateExprCollectionAccessFallbacks(") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto hasActiveBorrowForBinding = [&](const std::string &name,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("if (isSimpleCallName(expr, \"move\")) {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("if (isAssignCall(expr)) {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("if (getBuiltinMutationName(expr, mutateName)) {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "const BindingInfo *callableBinding = findParamBinding(params, expr.name);") ==
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "if (callableBinding != nullptr && callableBinding->typeName == \"lambda\") {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveMapKeyTypeWithInference =") ==
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "isSimpleCallName(expr, \"contains\") && !shouldBuiltinValidateBareMapContainsCall") ==
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "isSimpleCallName(expr, \"tryAt\") && !shouldBuiltinValidateBareMapTryAtCall") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("fieldParams.reserve(it->second->statements.size());") ==
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "auto isStaticField = [](const Expr &stmt) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "struct definitions may only contain field bindings: \" + resolved") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("if (hasStructZeroArgConstructor(resolved)) {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("const std::vector<Expr> *orderedCallArgs = &expr.args;") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("std::vector<const Expr *> packedArgs;") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("unsafe reference escapes across safe boundary to ") ==
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "lateMapSoaBuiltinContext.shouldBuiltinValidateBareMapContainsCall =") ==
        std::string::npos);
