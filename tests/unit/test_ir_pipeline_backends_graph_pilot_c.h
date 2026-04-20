  CHECK(validatorExpr.find("void SemanticsValidator::prepareExprDispatchBootstrap(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("prepareExprCollectionDispatchSetup(") != std::string::npos);
  CHECK(validatorExpr.find("bool SemanticsValidator::prepareExprCollectionDispatchSetup(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("prepareExprCollectionAccessDispatchContext(") !=
        std::string::npos);
  CHECK(validatorExpr.find("void SemanticsValidator::prepareExprCollectionAccessDispatchContext(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("validateExprDirectCollectionFallbacks(") != std::string::npos);
  CHECK(validatorExpr.find("bool SemanticsValidator::validateExprDirectCollectionFallbacks(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("validateExprPostAccessPrechecks(") !=
        std::string::npos);
  CHECK(validatorExpr.find("bool SemanticsValidator::validateExprPostAccessPrechecks(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("prepareExprNamedArgumentBuiltinContext(") !=
        std::string::npos);
  CHECK(validatorExpr.find("void SemanticsValidator::prepareExprNamedArgumentBuiltinContext(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("prepareExprLateBuiltinContext(") !=
        std::string::npos);
  CHECK(validatorExpr.find("void SemanticsValidator::prepareExprLateBuiltinContext(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("prepareExprResolvedCallSetup(") !=
        std::string::npos);
  CHECK(validatorExpr.find("void SemanticsValidator::prepareExprResolvedCallSetup(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("validateExprLateUnknownTargetFallbacks(") !=
        std::string::npos);
  CHECK(validatorExpr.find("bool SemanticsValidator::validateExprLateUnknownTargetFallbacks(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("prepareExprCountCapacityMapBuiltinContext(") !=
        std::string::npos);
  CHECK(validatorExpr.find("void SemanticsValidator::prepareExprCountCapacityMapBuiltinContext(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("prepareExprLateMapSoaBuiltinContext(") !=
        std::string::npos);
  CHECK(validatorExpr.find("void SemanticsValidator::prepareExprLateMapSoaBuiltinContext(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("prepareExprLateFallbackBuiltinContext(") !=
        std::string::npos);
  CHECK(validatorExpr.find("void SemanticsValidator::prepareExprLateFallbackBuiltinContext(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("prepareExprLateCallCompatibilityContext(") !=
        std::string::npos);
  CHECK(validatorExpr.find("void SemanticsValidator::prepareExprLateCallCompatibilityContext(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("prepareExprLateMapAccessBuiltinContext(") !=
        std::string::npos);
  CHECK(validatorExpr.find("void SemanticsValidator::prepareExprLateMapAccessBuiltinContext(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("const bool allowStdNamespacedVectorUserReceiverProbe =") ==
        std::string::npos);
  CHECK(validatorExpr.find("const bool allowStdNamespacedVectorUserReceiverProbe =") !=
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolvesExperimentalVectorValueReceiverForBareAccess =") ==
        std::string::npos);
  CHECK(validatorExpr.find("auto resolvesExperimentalVectorValueReceiverForBareAccess =") !=
        std::string::npos);
  CHECK(validatorExprMain.find("collectionAccessDispatchContext.resolveArrayTarget =") ==
        std::string::npos);
  CHECK(validatorExpr.find("contextOut.resolveArrayTarget = dispatchResolvers.resolveArrayTarget;") !=
        std::string::npos);
  CHECK(validatorExprMain.find("std::string gpuBuiltin;") == std::string::npos);
  CHECK(validatorExpr.find("std::string gpuBuiltin;") != std::string::npos);
  CHECK(validatorExprMain.find("PathSpaceBuiltin pathSpaceBuiltin;") ==
        std::string::npos);
  CHECK(validatorExpr.find("PathSpaceBuiltin pathSpaceBuiltin;") !=
        std::string::npos);
  CHECK(validatorExprMain.find("namedArgumentBuiltinContext.hasVectorHelperCallResolution =") ==
        std::string::npos);
  CHECK(validatorExpr.find("contextOut.hasVectorHelperCallResolution = hasVectorHelperCallResolution;") !=
        std::string::npos);
  CHECK(validatorExprMain.find(
            "lateBuiltinContext.tryBuiltinContext.getDirectMapHelperCompatibilityPath =") ==
        std::string::npos);
  CHECK(validatorExpr.find(
            "contextOut.tryBuiltinContext.getDirectMapHelperCompatibilityPath =") !=
        std::string::npos);
  CHECK(validatorExprMain.find(
            "countCapacityMapBuiltinContext.shouldBuiltinValidateStdNamespacedVectorCountCall =") ==
        std::string::npos);
  CHECK(validatorExpr.find(
            "contextOut.shouldBuiltinValidateStdNamespacedVectorCountCall =") !=
        std::string::npos);
  CHECK(validatorExprMain.find(
            "lateFallbackBuiltinContext.collectionAccessFallbackContext.isStdNamespacedVectorAccessCall =") ==
        std::string::npos);
  CHECK(validatorExpr.find(
            "contextOut.collectionAccessFallbackContext.isStdNamespacedVectorAccessCall =") !=
        std::string::npos);
  CHECK(validatorExprMain.find(
            "lateMapAccessBuiltinContext.shouldBuiltinValidateBareMapTryAtCall =") ==
        std::string::npos);
  CHECK(validatorExpr.find(
            "contextOut.shouldBuiltinValidateBareMapTryAtCall =") !=
        std::string::npos);
  CHECK(validatorExprMain.find("const ExprArgumentValidationContext argumentValidationContext{") ==
        std::string::npos);
  CHECK(validatorExpr.find("contextOut.argumentValidationContext.callExpr = &expr;") !=
        std::string::npos);
  CHECK(validatorExprMain.find("ExprResolvedStructConstructorContext resolvedStructConstructorContext{") ==
        std::string::npos);
  CHECK(validatorExpr.find("contextOut.resolvedStructConstructorContext = {") !=
        std::string::npos);
  CHECK(validatorExprMain.find("ExprResolvedCallArgumentContext resolvedCallArgumentContext{") ==
        std::string::npos);
  CHECK(validatorExpr.find("contextOut.resolvedCallArgumentContext = {") !=
        std::string::npos);
  CHECK(validatorExprMain.find("const std::string canonicalMapMethodTarget =") ==
        std::string::npos);
  CHECK(validatorExpr.find("const std::string canonicalMapMethodTarget =") !=
        std::string::npos);
  CHECK(validatorExprMain.find("return failLateUnknownTargetDiagnostic(\"unknown call target: \" +") ==
        std::string::npos);
  CHECK(validatorExpr.find("auto failLateUnknownTargetDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(validatorExpr.find("return failLateUnknownTargetDiagnostic(\"unknown call target: \" +") !=
        std::string::npos);
  CHECK(validatorExpr.find("if (resolveMapTargetWithTypes(target, keyType, valueType) ||") !=
        std::string::npos);
  CHECK(validatorExpr.find("resolveExperimentalMapTarget(target, keyType, valueType)) {") !=
        std::string::npos);
  CHECK(validatorExpr.find("auto resolveMapValueType = [&](const Expr &target, std::string &valueTypeOut) -> bool {") !=
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveDereferencedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveWrappedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto extractWrappedPointeeType = [&](const std::string &typeText, std::string &pointeeTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto extractCollectionElementType = [&](const std::string &typeText,") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveArgsPackAccessTarget = [&](const Expr &target, std::string &elemType) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveArrayTarget = [&](const Expr &target, std::string &elemType) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveVectorTarget = [&](const Expr &target, std::string &elemType) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveSoaVectorTarget = [&](const Expr &target, std::string &elemType) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("std::function<bool(const Expr &)> resolveStringTarget =") == std::string::npos);
  CHECK(validatorExprMain.find("auto resolveMapValueTypeForStringTarget = [&](const Expr &target, std::string &valueTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto extractExperimentalMapFieldTypes = [&](const BindingInfo &binding,") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto extractAnyMapKeyValueTypes = [&](const BindingInfo &binding,") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveExperimentalMapTarget = [&](const Expr &target,") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveExperimentalMapValueTarget = [&](const Expr &target,") ==
        std::string::npos);
  CHECK(validatorInfer.find("buildTypeResolutionGraph(program_)") != std::string::npos);
  CHECK(validatorInfer.find("collectGraphLocalAutoBindings(graph);") != std::string::npos);
  CHECK(validatorInfer.find("dependencyCountByBindingKey") != std::string::npos);
  CHECK(validatorInfer.find("return true;") != std::string::npos);
  CHECK(validatorInfer.find("isIfCall(initializer) || isMatchCall(initializer) || isBuiltinBlockCall(initializer)") !=
        std::string::npos);
  CHECK(validatorInfer.find("auto [factIt, inserted] = graphLocalAutoFacts_.try_emplace(bindingKey);") !=
        std::string::npos);
  CHECK(validatorInfer.find("computeTypeResolutionDependencyDag(graph)") != std::string::npos);
  CHECK(validatorInfer.find("sortTypeResolutionNodesForDiagnosticOrder(unresolvedNodes);") !=
        std::string::npos);
  CHECK(validatorInfer.find("makeBuiltinCollectionDispatchResolvers(params, locals, ") != std::string::npos);
  CHECK(validatorInfer.find("const auto &resolveIndexedArgsPackElementType =") != std::string::npos);
  CHECK(validatorInfer.find("builtinCollectionDispatchResolvers.resolveIndexedArgsPackElementType;") !=
        std::string::npos);
  CHECK(validatorInfer.find("const auto &resolveDereferencedIndexedArgsPackElementType =") !=
        std::string::npos);
  CHECK(validatorInfer.find("resolveDereferencedIndexedArgsPackElementType =") !=
        std::string::npos);
  CHECK(validatorInfer.find("const auto &resolveWrappedIndexedArgsPackElementType =") != std::string::npos);
  CHECK(validatorInfer.find("builtinCollectionDispatchResolvers.resolveWrappedIndexedArgsPackElementType;") !=
        std::string::npos);
  CHECK(validatorInfer.find("const auto &resolveArgsPackAccessTarget = builtinCollectionDispatchResolvers.resolveArgsPackAccessTarget;") !=
        std::string::npos);
  CHECK(validatorInfer.find("const auto &resolveArrayTarget = builtinCollectionDispatchResolvers.resolveArrayTarget;") !=
        std::string::npos);
  CHECK(validatorInfer.find("const auto &resolveVectorTarget = builtinCollectionDispatchResolvers.resolveVectorTarget;") !=
        std::string::npos);
  CHECK(validatorInfer.find("const auto &resolveSoaVectorTarget = builtinCollectionDispatchResolvers.resolveSoaVectorTarget;") !=
        std::string::npos);
  CHECK(validatorInfer.find("const auto &resolveBufferTarget = builtinCollectionDispatchResolvers.resolveBufferTarget;") !=
        std::string::npos);
  CHECK(validatorInfer.find("const auto &resolveStringTarget = builtinCollectionDispatchResolvers.resolveStringTarget;") !=
        std::string::npos);
  CHECK(validatorInfer.find("const auto &resolveMapTarget = builtinCollectionDispatchResolvers.resolveMapTarget;") !=
        std::string::npos);
  CHECK(validatorInfer.find("prepareInferCollectionDispatchSetup(") != std::string::npos);
  CHECK(validatorInfer.find("InferCollectionDispatchSetup inferCollectionDispatchSetup;") !=
        std::string::npos);
  CHECK(validatorInfer.find("inferLateFallbackReturnKind(") != std::string::npos);
  CHECK(validatorInfer.find("inferPreDispatchCallReturnKind(") != std::string::npos);
  CHECK(validatorInfer.find("inferScalarBuiltinReturnKind(") != std::string::npos);
  CHECK(validatorInfer.find("inferResolvedCallReturnKind(") != std::string::npos);
  CHECK(validatorInferMain.find("auto resolveIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto resolveDereferencedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto resolveWrappedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto resolveArgsPackAccessTarget = [&](const Expr &target, std::string &elemType)") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto resolveArrayTarget = [&](const Expr &target, std::string &elemType)") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto resolveVectorTarget = [&](const Expr &target, std::string &elemType)") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto resolveSoaVectorTarget = [&](const Expr &target, std::string &elemType) -> bool {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto resolveBufferTarget = [&](const Expr &target, std::string &elemType) -> bool {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("std::function<bool(const Expr &)> resolveStringTarget =") == std::string::npos);
  CHECK(validatorInferMain.find("std::function<bool(const Expr &, std::string &, std::string &)> resolveMapTarget =") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto resolveBuiltinAccessReceiverExprInline = [&](const Expr &accessExpr) -> const Expr * {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto extractWrappedPointeeType = [&](const std::string &typeText, std::string &pointeeTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto extractCollectionElementType = [&](const std::string &typeText,") ==
        std::string::npos);
  CHECK(validatorInferMain.find("std::string namespacedCollection;") == std::string::npos);
  CHECK(validatorInferMain.find("const bool isStdNamespacedVectorCountCall =") == std::string::npos);
  CHECK(validatorInferMain.find("BuiltinCollectionCountCapacityDispatchContext builtinCollectionCountCapacityDispatchContext;") ==
        std::string::npos);
  CHECK(validatorInferMain.find("BuiltinCollectionDirectCountCapacityContext builtinCollectionDirectCountCapacityContext;") ==
        std::string::npos);
  CHECK(validatorInferMain.find("const bool shouldDeferResolvedNamespacedCollectionHelperReturn =") ==
        std::string::npos);
  CHECK(validatorInferMain.find("const bool isBuiltinGet = isSimpleCallName(expr, \"get\");") ==
        std::string::npos);
  CHECK(validatorInferMain.find("if (!expr.isMethodCall && (isSimpleCallName(expr, \"to_soa\") || isSimpleCallName(expr, \"to_aos\")) &&") ==
        std::string::npos);
  CHECK(validatorInferMain.find("if (getBuiltinGpuName(expr, builtinName)) {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("if (getBuiltinPointerName(expr, builtinName) && expr.args.size() == 1) {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("std::function<ReturnKind(const Expr &)> pointerTargetKind =") ==
        std::string::npos);
  CHECK(validatorInferMain.find("const std::string directRemovedMapCompatibilityPath =") ==
        std::string::npos);
  CHECK(validatorInferMain.find("if (getVectorStatementHelperName(expr, vectorHelper) && !expr.args.empty()) {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("if (getBuiltinMathName(expr, builtinName, allowMathBareName(expr.name))) {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("if (getBuiltinOperatorName(expr, builtinName)) {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("if (getBuiltinMutationName(expr, builtinName)) {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("if (isAssignCall(expr)) {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto isTypeNamespaceMethodCall = [&](const Expr &callExpr, const std::string &resolvedPath) -> bool {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("if (!validateNamedArgumentsAgainstParams(calleeParams, *orderedCallArgNames, orderedArgError)) {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("if (!ensureDefinitionReturnKindReady(*defIt->second)) {") ==
        std::string::npos);
  CHECK(validatorInfer.find("void SemanticsValidator::prepareInferCollectionDispatchSetup(") !=
        std::string::npos);
  CHECK(validatorInfer.find("setupOut.builtinCollectionCountCapacityDispatchContext.isCountLike =") !=
        std::string::npos);
  CHECK(validatorInfer.find("setupOut.builtinCollectionDirectCountCapacityContext.isDirectCountCall =") !=
        std::string::npos);
  CHECK(validatorInfer.find("ReturnKind SemanticsValidator::inferLateFallbackReturnKind(") !=
        std::string::npos);
  CHECK(validatorInfer.find("auto failInferLateFallbackDiagnostic = [&](std::string message) -> ReturnKind {") !=
        std::string::npos);
  CHECK(validatorInfer.find("const bool isBuiltinGet = isSimpleCallName(expr, \"get\");") !=
        std::string::npos);
  CHECK(validatorInfer.find("isSimpleCallName(expr, \"to_soa\") || isSimpleCallName(expr, \"to_aos\")") !=
        std::string::npos);
  CHECK(validatorInfer.find("if (getBuiltinGpuName(expr, builtinName)) {") !=
        std::string::npos);
  CHECK(validatorInfer.find("ReturnKind SemanticsValidator::inferPreDispatchCallReturnKind(") !=
        std::string::npos);
  CHECK(validatorInfer.find("auto failInferPreDispatchDiagnostic = [&](std::string message) -> ReturnKind {") !=
        std::string::npos);
  CHECK(validatorInfer.find("std::function<ReturnKind(const Expr &)> pointerTargetKind =") !=
        std::string::npos);
  CHECK(validatorInfer.find("const std::string directRemovedMapCompatibilityPath =") !=
        std::string::npos);
  CHECK(validatorInfer.find("if (getVectorStatementHelperName(expr, vectorHelper) && !expr.args.empty()) {") !=
        std::string::npos);
  CHECK(validatorInfer.find("ReturnKind SemanticsValidator::inferScalarBuiltinReturnKind(") !=
        std::string::npos);
  CHECK(validatorInfer.find("if (getBuiltinPointerName(expr, builtinName) && expr.args.size() == 1) {") !=
        std::string::npos);
  CHECK(validatorInfer.find("if (getBuiltinMathName(expr, builtinName, allowMathBareName(expr.name))) {") !=
        std::string::npos);
  CHECK(validatorInfer.find("if (getBuiltinOperatorName(expr, builtinName)) {") !=
        std::string::npos);
  CHECK(validatorInfer.find("if (getBuiltinMutationName(expr, builtinName)) {") !=
        std::string::npos);
  CHECK(validatorInfer.find("if (isAssignCall(expr)) {") !=
        std::string::npos);
  CHECK(validatorInfer.find("ReturnKind SemanticsValidator::inferResolvedCallReturnKind(") !=
        std::string::npos);
  CHECK(validatorInfer.find("auto failInferResolvedCallDiagnostic = [&](std::string message) -> ReturnKind {") !=
        std::string::npos);
  CHECK(validatorInfer.find("auto isTypeNamespaceMethodCall = [&](const Expr &callExpr,") !=
        std::string::npos);
  CHECK(validatorInfer.find("validateNamedArgumentsAgainstParams(") !=
        std::string::npos);
  CHECK(validatorInfer.find("if (!ensureDefinitionReturnKindReady(*defIt->second)) {") !=
        std::string::npos);
  CHECK(validatorCollections.find("bool SemanticsValidator::resolveCallCollectionTypePath(") !=
        std::string::npos);
  CHECK(validatorCollections.find("bool SemanticsValidator::resolveCallCollectionTemplateArgs(") !=
        std::string::npos);
  CHECK(validatorInferMain.find("auto inferTargetTypeText = [&](const Expr &candidate, std::string &typeTextOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto extractAnyMapKeyValueTypes = [&](const BindingInfo &binding,") ==
        std::string::npos);
  CHECK(validatorInfer.find("formatReturnInferenceCycleDiagnostic(") != std::string::npos);
  CHECK(validatorInfer.find("auto failInferDefinitionDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(validatorInfer.find("auto failInferGraphDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(validatorInfer.find("rememberFirstCollectedDiagnosticMessage(message);") !=
        std::string::npos);
  CHECK(validatorInfer.find("return failUncontextualizedDiagnostic(std::move(message));") !=
        std::string::npos);
  CHECK(validatorInfer.find("cycle member: ") != std::string::npos);
  CHECK(validatorInfer.find("allowRecursiveReturnInference_ = false;") != std::string::npos);
  CHECK(validatorInfer.find("ensureDefinitionReturnKindReady(*defIt->second)") != std::string::npos);
  CHECK(pipeline.find("options.typeResolver") == std::string::npos);
  CHECK(primecMain.find("[--type-resolver legacy|graph]") == std::string::npos);
  CHECK(primevmMain.find("[--type-resolver legacy|graph]") == std::string::npos);
}
