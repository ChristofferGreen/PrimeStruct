  CHECK(semanticsExprLateMapSoaBuiltinsSource.find(
            "mapSoaBuiltinContext.shouldBuiltinValidateBareMapContainsCall =") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("mapSoaBuiltinContext.bareMapHelperOperandIndices =") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("if (getBuiltinCollectionName(expr, builtinName)) {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("if (!expr.isMethodCall && isSimpleCallName(expr, \"try\")) {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("if (!expr.isMethodCall && isSimpleCallName(expr, \"File\")) {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("if (resolvedMethod && resolved == \"/result/ok\") {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("const bool allowsNamedArgsPackBuiltinLabels =") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("if (!resolveExprVectorHelperCall(params,") != std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveBareMapCallBodyArgumentTarget = [&]() -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto remapWrappedMapMethodBodyArgumentTarget = [&]() -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveLeadingNonCollectionAccessReceiverPath = [&](const Expr &receiverExpr,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveDirectCallTemporaryAccessReceiverPath = [&](const Expr &receiverExpr,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isVectorBuiltinName = [&](const Expr &candidate, const char *helper) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveVectorHelperMethodTarget = [&](const Expr &receiver,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto normalizeCollectionMethodName = [](const std::string &methodName)") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto inferPointerLikeCallReturnType = [&](const Expr &receiverExpr) -> std::string {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolvePointerLikeMethodTarget = [&](const Expr &receiverExpr,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("const std::string directRemovedMapCompatibilityPath =") ==
        std::string::npos);
  CHECK(semanticsExprCollectionDispatchSetupSource.find(
            "const std::string directRemovedMapCompatibilityPath =") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("const bool allowStdNamespacedVectorUserReceiverProbe =") ==
        std::string::npos);
  CHECK(semanticsExprCollectionDispatchSetupSource.find(
            "const bool allowStdNamespacedVectorUserReceiverProbe =") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("if (!expr.isMethodCall && setupOut.isStdNamespacedVectorCountCall &&") ==
        std::string::npos);
  CHECK(semanticsExprCollectionDispatchSetupSource.find(
            "if (!expr.isMethodCall && setupOut.isStdNamespacedVectorCountCall &&") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("return validateExprFieldAccess(params, locals, expr);") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("field access requires a receiver") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("if (resolveMapTargetWithTypes(target, keyType, valueType) ||") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("resolveExperimentalMapTarget(target, keyType, valueType)) {") ==
        std::string::npos);
  CHECK(semanticsExprDispatchBootstrapSource.find(
            "if (bootstrapOut.dispatchResolvers.resolveMapTarget(target, keyType, valueType) ||") !=
        std::string::npos);
  CHECK(semanticsExprDispatchBootstrapSource.find(
            "bootstrapOut.dispatchResolvers.resolveExperimentalMapTarget(target, keyType,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("return resolveMapTargetWithTypes(target, keyTypeOut, valueType);") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("return resolveMapTargetWithTypes(target, keyType, valueTypeOut);") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveDereferencedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveWrappedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveBuiltinAccessReceiverExprInline = [&](const Expr &accessExpr) -> const Expr * {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isNamedArgsPackMethodAccessCall = [&](const Expr &target) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isNamedArgsPackWrappedFileBuiltinAccessCall = [&](const Expr &target) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isArrayNamespacedVectorCountCompatibilityCall = [&](const Expr &candidate) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isIndexedArgsPackMapReceiverTarget = [&](const Expr &receiverExpr) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isStringExpr = [&](const Expr &arg) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto validateCollectionElementType = [&](const Expr &arg, const std::string &typeName,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isIntegerKind = [&](ReturnKind kind) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isNumericOrBoolKind = [&](ReturnKind kind) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveArrayElemType = [&](const Expr &arg, std::string &elemType) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveBufferElemType = [&](const Expr &arg, std::string &elemType) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("dispatch requires kernel and three dimension arguments") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("buffer requires exactly one template argument") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("upload requires array input") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("readback requires Buffer input") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("buffer_load requires integer index") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("buffer_store value type mismatch") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("if (getBuiltinConvertName(expr, builtinName)) {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("PrintBuiltin printBuiltin;") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("if (getPrintBuiltin(expr, printBuiltin)) {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("if (getBuiltinPointerName(expr, builtinName)) {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("if (getBuiltinMemoryName(expr, builtinName)) {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto normalizeIndexKind = [&](ReturnKind kind) -> ReturnKind {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isSupportedIndexKind = [&](ReturnKind kind) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto validateMemoryTargetType = [&](const std::string &targetType) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find(
            ".collectionAccessFallbackContext.isStdNamespacedVectorAccessCall =") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("collectionAccessValidationContext.resolveExperimentalMapTarget =") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("collectionAccessValidationContext.isMapLikeBareAccessReceiverTarget =") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto getRemovedVectorAccessBuiltinName = [&](const Expr &candidate, std::string &helperOut) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isMapLikeBareAccessReceiver = [&](const Expr &candidate) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("unknown call target: /std/collections/map/\" + builtinName") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("to_soa requires vector target") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("soa_vector field views are not implemented yet: ") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("if (resolvedMethod && resolved == \"/std/collections/map/contains\") {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "if (!resolvedMethod && !expr.isMethodCall && isSimpleCallName(expr, \"contains\") &&\n"
            "          shouldBuiltinValidateBareMapContainsCall && it == defMap_.end()) {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("resolved == \"/soa_vector/get\" || resolved == \"/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("(expr.name == \"get\" || expr.name == \"ref\") && it == defMap_.end()") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto extractWrappedPointeeType = [&](const std::string &typeText, std::string &pointeeTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto extractCollectionElementType = [&](const std::string &typeText,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveArgsPackAccessTarget = [&](const Expr &target, std::string &elemType) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveArrayTarget = [&](const Expr &target, std::string &elemType) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveVectorTarget = [&](const Expr &target, std::string &elemType) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveSoaVectorTarget = [&](const Expr &target, std::string &elemType) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("std::function<bool(const Expr &)> resolveStringTarget =") == std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveMapValueTypeForStringTarget = [&](const Expr &target, std::string &valueTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto mapHelperReceiverIndex = [&](const Expr &candidate) -> size_t {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto bareMapHelperOperandIndices = [&](const Expr &candidate,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto preferredBareMapHelperTarget = [&](std::string_view helperName)") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto specializedExperimentalMapHelperTarget = [&](std::string_view helperName,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto preferredBareVectorHelperTarget = [&](std::string_view helperName)") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto tryRewriteBareMapHelperCall = [&](const Expr &candidate,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto tryRewriteBareVectorHelperCall = [&](const Expr &candidate,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto tryRewriteCanonicalExperimentalMapHelperCall = [&](const Expr &candidate, Expr &rewrittenOut) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto explicitCanonicalExperimentalMapBorrowedHelperPath = [&](const Expr &candidate,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto extractExperimentalMapFieldTypes = [&](const BindingInfo &binding,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto extractAnyMapKeyValueTypes = [&](const BindingInfo &binding,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveExperimentalMapTarget = [&](const Expr &target,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveExperimentalMapValueTarget = [&](const Expr &target,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto argumentStructMismatchDiagnostic = [&](const std::string &paramName,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto expectedBindingTypeText = [](const BindingInfo &binding)") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isSoftwareNumericParamCompatible = [](ReturnKind expectedKind, ReturnKind actualKind) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isBuiltinCollectionLiteralExpr = [&](const Expr &candidate) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto validateArgumentTypeAgainstParam = [&](const Expr &arg,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto extractExperimentalMapFieldTypesFromStructPath = [&](const std::string &structPath,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto validateSpreadArgumentTypeAgainstParam = [&](const Expr &arg,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto validateArgumentsForParameter = [&](const ParameterInfo &param,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isTypeNamespaceMethodCall = [&](const Expr &callExpr, const std::string &resolvedPath) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto inferStructFieldBinding = [&](const Definition &def,") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveStructFieldReceiverPath =") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isTypeNamespaceFieldReceiver = [&](const Expr &receiver, std::string &structPathOut) -> bool {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto resolveStructFieldBinding =") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto isUnboundMetaReceiver = [&](const Expr &receiver)") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("auto describeMethodReflectionTarget = [&](const Expr &callExpr) -> std::string {") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("const ExprArgumentValidationContext argumentValidationContext{") ==
        std::string::npos);
  CHECK(semanticsExprResolvedCallSetupSource.find(
            "contextOut.argumentValidationContext.callExpr = &expr;") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "ExprResolvedStructConstructorContext resolvedStructConstructorContext{") ==
        std::string::npos);
  CHECK(semanticsExprResolvedCallSetupSource.find(
            "contextOut.resolvedStructConstructorContext = {") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "ExprResolvedCallArgumentContext resolvedCallArgumentContext{") ==
        std::string::npos);
  CHECK(semanticsExprResolvedCallSetupSource.find(
            "contextOut.resolvedCallArgumentContext = {") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "const std::string canonicalMapMethodTarget =") ==
        std::string::npos);
  CHECK(semanticsExprLateUnknownTargetFallbacksSource.find(
            "const std::string canonicalMapMethodTarget =") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "error_ = \"unknown call target: \" + formatUnknownCallTarget(expr);") ==
        std::string::npos);
  CHECK(semanticsExprLateUnknownTargetFallbacksSource.find(
            "error_ = \"unknown call target: \" + formatUnknownCallTarget(expr);") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("#include \"SemanticsValidatorExprCaptureSplitStep.h\"") == std::string::npos);
  CHECK(semanticsExprSource.find("#include \"SemanticsValidatorExprPredicates.h\"") == std::string::npos);
  CHECK(semanticsExprSource.find("#include \"SemanticsValidatorExprValidation.h\"") == std::string::npos);
  CHECK(semanticsExprBlockSource.find("bool SemanticsValidator::validateBlockExpr") != std::string::npos);
  CHECK(semanticsExprBlockSource.find("auto publishBlockDiagnostic = [&](const Expr &diagnosticExpr) -> bool {") !=
        std::string::npos);
  CHECK(semanticsExprBlockSource.find("captureExprContext(diagnosticExpr);") != std::string::npos);
  CHECK(semanticsExprBlockSource.find("return publishCurrentStructuredDiagnosticNow();") !=
        std::string::npos);
  CHECK(semanticsExprBlockSource.find("block expression does not accept arguments") != std::string::npos);
  CHECK(semanticsExprBlockSource.find("block expression must end with an expression") != std::string::npos);
  CHECK(semanticsExprControlFlowSource.find("bool SemanticsValidator::validateIfExpr") != std::string::npos);
  CHECK(semanticsExprControlFlowSource.find("bool SemanticsValidator::isStructConstructorValueExpr") !=
        std::string::npos);
  CHECK(semanticsExprControlFlowSource.find("if branches require block envelopes") != std::string::npos);
  CHECK(semanticsExprLambdaSource.find("bool SemanticsValidator::validateLambdaExpr") != std::string::npos);
  CHECK(semanticsExprLambdaSource.find("auto publishLambdaDiagnostic = [&](const Expr &diagnosticExpr) -> bool {") !=
        std::string::npos);
  CHECK(semanticsExprLambdaSource.find("captureExprContext(diagnosticExpr);") != std::string::npos);
  CHECK(semanticsExprLambdaSource.find("return publishCurrentStructuredDiagnosticNow();") !=
        std::string::npos);
  CHECK(semanticsExprLambdaSource.find("#include \"SemanticsValidatorExprCaptureSplitStep.h\"") !=
        std::string::npos);
  CHECK(semanticsExprLambdaSource.find("duplicate lambda capture") != std::string::npos);
  CHECK(semanticsExprNumericSource.find("bool SemanticsValidator::validateNumericBuiltinExpr") != std::string::npos);
  CHECK(semanticsExprNumericSource.find("multiply requires scalar scaling") != std::string::npos);
  CHECK(semanticsExprNumericPredicatesSource.find("implicit matrix/quaternion family conversion requires explicit helper") !=
        std::string::npos);
  CHECK(semanticsExprArgumentValidationSource.find("std::string SemanticsValidator::expectedBindingTypeText") !=
        std::string::npos);
  CHECK(semanticsExprArgumentValidationSource.find(
            "std::string SemanticsValidator::argumentStructMismatchDiagnostic") !=
        std::string::npos);
  CHECK(semanticsExprArgumentValidationCombinedSource.find(
            "bool SemanticsValidator::isBuiltinCollectionLiteralExpr") !=
        std::string::npos);
  CHECK(semanticsExprArgumentValidationCombinedSource.find(
            "bool SemanticsValidator::extractExperimentalMapFieldTypesFromStructPath") !=
        std::string::npos);
  CHECK(semanticsExprArgumentValidationSource.find(
            "bool SemanticsValidator::validateArgumentTypeAgainstParam") !=
        std::string::npos);
  CHECK(semanticsExprArgumentValidationSource.find(
            "bool SemanticsValidator::validateSpreadArgumentTypeAgainstParam") !=
        std::string::npos);
  CHECK(semanticsExprArgumentValidationSource.find(
            "bool SemanticsValidator::validateArgumentsForParameter") !=
        std::string::npos);
  CHECK(semanticsExprCollectionPredicatesSource.find(
            "const Expr *SemanticsValidator::resolveBuiltinAccessReceiverExpr") !=
        std::string::npos);
  CHECK(semanticsExprCollectionPredicatesSource.find(
            "bool SemanticsValidator::isNamedArgsPackMethodAccessCall") !=
        std::string::npos);
  CHECK(semanticsExprCollectionPredicatesSource.find(
            "bool SemanticsValidator::isNamedArgsPackWrappedFileBuiltinAccessCall") !=
        std::string::npos);
  CHECK(semanticsExprCollectionPredicatesSource.find(
            "bool SemanticsValidator::isMapLikeBareAccessReceiver") !=
        std::string::npos);
  CHECK(semanticsExprCollectionPredicatesSource.find(
            "bool SemanticsValidator::isArrayNamespacedVectorCountCompatibilityCall") !=
        std::string::npos);
  CHECK(semanticsExprCollectionPredicatesSource.find(
            "bool SemanticsValidator::isIndexedArgsPackMapReceiverTarget") !=
        std::string::npos);
  CHECK(semanticsExprCollectionPredicatesSource.find(
            "bool SemanticsValidator::validateCollectionElementType") !=
        std::string::npos);
  CHECK(semanticsExprCollectionAccessValidationSource.find(
            "bool SemanticsValidator::validateExprCollectionAccessFallbacks") !=
        std::string::npos);
  CHECK(semanticsExprCollectionAccessValidationSource.find(
            "requires array, vector, map, or string target") !=
        std::string::npos);
  CHECK(semanticsExprCollectionAccessValidationSource.find(
            "unknown call target: /std/collections/map/") !=
        std::string::npos);
  CHECK(semanticsExprCollectionLiteralsSource.find(
            "bool SemanticsValidator::validateExprCollectionLiteralBuiltins") !=
        std::string::npos);
  CHECK(semanticsExprCollectionLiteralsSource.find(
            "auto publishCollectionLiteralDiagnostic = [&]() -> bool {") !=
        std::string::npos);
  CHECK(semanticsExprCollectionLiteralsSource.find("captureExprContext(expr);") !=
        std::string::npos);
  CHECK(semanticsExprCollectionLiteralsSource.find(
            "return publishCurrentStructuredDiagnosticNow();") !=
        std::string::npos);
  CHECK(semanticsExprCollectionLiteralsSource.find("validateBuiltinMapKeyType(expr.templateArgs.front(), nullptr, error_)") !=
        std::string::npos);
  CHECK(semanticsExprCollectionLiteralsSource.find("this->validateCollectionElementType(") !=
        std::string::npos);
  CHECK(semanticsExprLateFallbackBuiltinsSource.find(
            "bool SemanticsValidator::validateExprLateFallbackBuiltins") !=
        std::string::npos);
  CHECK(semanticsExprLateFallbackBuiltinsSource.find(
            "validateExprLateCollectionAccessFallbacks(") !=
        std::string::npos);
  CHECK(semanticsExprLateFallbackBuiltinsSource.find(
            "validateExprScalarPointerMemoryBuiltins(") !=
        std::string::npos);
  CHECK(semanticsExprLateFallbackBuiltinsSource.find(
            "validateExprCollectionLiteralBuiltins(") !=
        std::string::npos);
  CHECK(semanticsExprLateFallbackBuiltinsSource.find(
            "Expr rewrittenMapAccessCall = expr;") !=
        std::string::npos);
  CHECK(semanticsExprLateFallbackBuiltinsSource.find(
            "const size_t receiverIndex =\n"
            "        this->mapHelperReceiverIndex(expr, *context.dispatchResolvers);") !=
        std::string::npos);
  CHECK(semanticsExprLateCallCompatibilitySource.find(
            "bool SemanticsValidator::validateExprLateCallCompatibility") !=
        std::string::npos);
  CHECK(semanticsExprLateCallCompatibilitySource.find(
            "math builtin requires import /std/math/* or /std/math/<name>: ") !=
        std::string::npos);
  CHECK(semanticsExprLateCallCompatibilitySource.find(
            "named arguments not supported for lambda calls") !=
        std::string::npos);
  CHECK(semanticsExprLateCallCompatibilitySource.find(
            "unknown call target: /std/collections/vector/count") !=
        std::string::npos);
  CHECK(semanticsExprLateCallCompatibilitySource.find(
            "unknown call target: /std/collections/vector/capacity") !=
        std::string::npos);
  CHECK(semanticsExprLateMapAccessBuiltinsSource.find(
            "bool SemanticsValidator::validateExprLateMapAccessBuiltins") !=
        std::string::npos);
  CHECK(semanticsExprLateMapAccessBuiltinsSource.find(
            "auto resolveMapKeyTypeWithInference =") !=
        std::string::npos);
  CHECK(semanticsExprLateMapAccessBuiltinsSource.find(
            "contains requires map target") !=
        std::string::npos);
  CHECK(semanticsExprLateMapAccessBuiltinsSource.find(
            "tryAt requires map target") !=
        std::string::npos);
  CHECK(semanticsExprLateUnknownTargetFallbacksSource.find(
            "bool SemanticsValidator::validateExprLateUnknownTargetFallbacks") !=
        std::string::npos);
  CHECK(semanticsExprLateUnknownTargetFallbacksSource.find(
            "const std::string aliasMapMethodTarget = \"/map/\" + expr.name;") !=
        std::string::npos);
  CHECK(semanticsExprStructConstructorsSource.find(
            "bool SemanticsValidator::validateExprResolvedStructConstructorCall") !=
        std::string::npos);
  CHECK(semanticsExprStructConstructorsSource.find(
            "fieldParams.reserve(context.resolvedDefinition->statements.size());") !=
        std::string::npos);
  CHECK(semanticsExprStructConstructorsSource.find(
            "struct definitions may only contain field bindings: ") !=
        std::string::npos);
  CHECK(semanticsExprStructConstructorsSource.find(
            "if (hasStructZeroArgConstructor(resolved)) {") !=
        std::string::npos);
  CHECK(semanticsExprResolvedCallSetupSource.find(
            "void SemanticsValidator::prepareExprResolvedCallSetup(") !=
        std::string::npos);
  CHECK(semanticsExprResolvedCallSetupSource.find(
            "contextOut.diagnosticResolved = diagnosticCallTargetPath(resolved);") !=
        std::string::npos);
  CHECK(semanticsExprResolvedCallArgumentsSource.find(
            "bool SemanticsValidator::validateExprResolvedCallArguments") !=
        std::string::npos);
  CHECK(semanticsExprResolvedCallArgumentsSource.find(
            "const std::vector<Expr> *orderedCallArgs = &expr.args;") !=
        std::string::npos);
  CHECK(semanticsExprResolvedCallArgumentsSource.find(
            "std::vector<const Expr *> packedArgs;") !=
        std::string::npos);
  CHECK(semanticsExprResolvedCallArgumentsSource.find(
            "validateArgumentsForParameter(") !=
        std::string::npos);
  CHECK(semanticsExprResolvedCallArgumentsSource.find(
            "unsafe reference escapes across safe boundary to ") !=
        std::string::npos);
  CHECK(semanticsExprMutationBorrowsSource.find(
            "bool SemanticsValidator::validateExprMutationBorrowBuiltins") !=
        std::string::npos);
  CHECK(semanticsExprMutationBorrowsSource.find(
            "auto hasActiveBorrowForBinding =\n"
            "      [&](const std::string &name,") !=
        std::string::npos);
  CHECK(semanticsExprMutationBorrowsSource.find("if (isSimpleCallName(expr, \"move\")) {") !=
        std::string::npos);
  CHECK(semanticsExprMutationBorrowsSource.find("if (isAssignCall(expr)) {") !=
        std::string::npos);
  CHECK(semanticsExprMutationBorrowsSource.find(
            "if (getBuiltinMutationName(expr, mutateName)) {") !=
        std::string::npos);
  CHECK(semanticsExprMutationBorrowsSource.find(
            "soa_vector field views are not implemented yet: ") ==
        std::string::npos);
  CHECK(semanticsExprMutationBorrowsSource.find(
            "isBuiltinSoaFieldViewExpr(target, params, locals,") !=
        std::string::npos);
  CHECK(semanticsExprMutationBorrowsSource.find(
            "if (!validateExpr(params, locals, target)) {") !=
        std::string::npos);
  CHECK(semanticsExprMutationBorrowsSource.find(
            "pendingExperimentalSoaFieldViewNameForAssignTarget") ==
        std::string::npos);
  CHECK(semanticsExprMutationBorrowsSource.find(
            "extractExperimentalSoaAssignFieldViewName(") ==
        std::string::npos);
  CHECK(semanticsExprMutationBorrowsSource.find(
            "soaFieldViewPendingDiagnostic(fieldTarget.name)") ==
        std::string::npos);
  CHECK(semanticsExprNamedArgumentBuiltinsSource.find(
            "bool SemanticsValidator::validateExprNamedArgumentBuiltins") !=
        std::string::npos);
  CHECK(semanticsExprNamedArgumentBuiltinsSource.find("getBuiltinGpuName(expr, builtinName)") !=
        std::string::npos);
  CHECK(semanticsExprTrySource.find("bool SemanticsValidator::validateExprTryBuiltin") !=
        std::string::npos);
  CHECK(semanticsExprTrySource.find("auto publishTryDiagnostic = [&]() -> bool {") !=
        std::string::npos);
  CHECK(semanticsExprTrySource.find("captureExprContext(expr);") !=
        std::string::npos);
  CHECK(semanticsExprTrySource.find("return publishCurrentStructuredDiagnosticNow();") !=
        std::string::npos);
  CHECK(semanticsExprTrySource.find("try requires exactly one argument") !=
        std::string::npos);
  CHECK(semanticsExprPointerLikeSource.find("std::string SemanticsValidator::normalizeCollectionMethodName") !=
        std::string::npos);
  CHECK(semanticsExprPointerLikeSource.find("std::string SemanticsValidator::inferPointerLikeCallReturnType") !=
        std::string::npos);
  CHECK(semanticsExprPointerLikeSource.find("bool SemanticsValidator::resolvePointerLikeMethodTarget") !=
        std::string::npos);
  CHECK(semanticsExprReceiverPathsSource.find(
            "bool SemanticsValidator::resolveNonCollectionAccessHelperPathFromTypeText") !=
        std::string::npos);
  CHECK(semanticsExprReceiverPathsSource.find(
            "bool SemanticsValidator::resolveLeadingNonCollectionAccessReceiverPath") !=
        std::string::npos);
  CHECK(semanticsExprReceiverPathsSource.find(
            "bool SemanticsValidator::resolveDirectCallTemporaryAccessReceiverPath") !=
        std::string::npos);
  CHECK(semanticsExprGpuBufferSource.find(
            "bool SemanticsValidator::validateExprGpuBufferBuiltins") !=
        std::string::npos);
  CHECK(semanticsExprGpuBufferSource.find(
            "auto publishGpuBufferDiagnostic = [&]() -> bool {") !=
        std::string::npos);
  CHECK(semanticsExprGpuBufferSource.find("captureExprContext(expr);") !=
        std::string::npos);
  CHECK(semanticsExprGpuBufferSource.find(
            "return publishCurrentStructuredDiagnosticNow();") !=
        std::string::npos);
  CHECK(semanticsExprGpuBufferSource.find(
            "dispatch requires kernel and three dimension arguments") !=
        std::string::npos);
  CHECK(semanticsExprScalarPointerMemorySource.find(
            "bool SemanticsValidator::validateExprScalarPointerMemoryBuiltins") !=
        std::string::npos);
  CHECK(semanticsExprScalarPointerMemorySource.find(
            "convert requires numeric or bool operand") !=
        std::string::npos);
  CHECK(semanticsExprScalarPointerMemorySource.find(
            "alloc requires exactly one template argument") !=
        std::string::npos);
  CHECK(semanticsExprMapSoaBuiltinsSource.find(
            "bool SemanticsValidator::validateExprMapSoaBuiltins") !=
        std::string::npos);
  CHECK(semanticsExprMapSoaBuiltinsSource.find(
            "context.bareMapHelperOperandIndices != nullptr") !=
        std::string::npos);
  CHECK(semanticsExprMapSoaBuiltinsSource.find(
            "to_soa requires vector target") !=
        std::string::npos);
  CHECK(semanticsExprMapSoaBuiltinsSource.find(
            "soa_vector field views are not implemented yet: ") ==
        std::string::npos);
  CHECK(semanticsExprMapSoaBuiltinsSource.find(
            "soaDirectPendingUnavailableMethodDiagnostic(resolved)") !=
        std::string::npos);
  CHECK(semanticsExprMapSoaBuiltinsSource.find(
            "soaUnavailableMethodDiagnostic(resolved, false)") ==
        std::string::npos);
  CHECK(semanticsExprMapSoaBuiltinsSource.find(
            "soaFieldViewPendingDiagnostic(soaFieldViewName)") ==
        std::string::npos);
  CHECK(semanticsExprResultFileSource.find(
            "bool SemanticsValidator::validateExprResultFileBuiltins") !=
        std::string::npos);
  CHECK(semanticsExprResultFileSource.find("Result.map2 requires exactly three arguments") !=
        std::string::npos);
  CHECK(semanticsExprFieldResolutionSource.find("bool SemanticsValidator::isTypeNamespaceFieldReceiver") !=
        std::string::npos);
  CHECK(semanticsExprFieldResolutionSource.find("bool SemanticsValidator::validateExprFieldAccess") !=
        std::string::npos);
  CHECK(semanticsExprFieldResolutionSource.find(
            "bool SemanticsValidator::resolveStructFieldBinding(const std::vector<ParameterInfo> &params,") !=
        std::string::npos);
  CHECK(semanticsExprFieldResolutionSource.find("bool SemanticsValidator::isTypeNamespaceMethodCall") !=
        std::string::npos);
  CHECK(semanticsExprFieldResolutionSource.find("std::string SemanticsValidator::describeMethodReflectionTarget") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find("bool SemanticsValidator::isVectorBuiltinName") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find("bool SemanticsValidator::resolveVectorHelperMethodTarget") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find("bool SemanticsValidator::resolveExprVectorHelperCall") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "usesSamePathSoaHelperTargetForCurrentImports(helperName)") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "preferredSoaHelperTargetForCurrentImports(") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find("auto preferredSoaMutatorHelperTarget =") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find("auto hasVisibleSoaMutatorShadow =") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "hasDeclaredDefinitionPath(samePath) || hasImportedDefinitionPath(samePath)") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "usesSamePathSoaHelperTargetForCurrentImports(\"count\")") !=
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "hasVisibleDefinitionPathForCurrentImports(canonical)") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "hasDeclaredDefinitionPath(samePath) || hasImportedDefinitionPath(samePath)") ==
        std::string::npos);
  CHECK(semanticsInferMethodResolutionSource.find(
            "usesSamePathSoaHelperTargetForCurrentImports(\"count\")") !=
        std::string::npos);
  CHECK(semanticsInferMethodResolutionSource.find(
            "hasVisibleDefinitionPathForCurrentImports(canonical)") ==
        std::string::npos);
  CHECK(semanticsInferMethodResolutionSource.find(
            "hasDeclaredDefinitionPath(samePath) || hasImportedDefinitionPath(samePath)") ==
        std::string::npos);
  CHECK(semanticsCollectionHelperRewritesSource.find("size_t SemanticsValidator::mapHelperReceiverIndex") !=
        std::string::npos);
  CHECK(semanticsCollectionHelperRewritesSource.find("bool SemanticsValidator::bareMapHelperOperandIndices") !=
        std::string::npos);
  CHECK(semanticsCollectionHelperRewritesSource.find("std::string SemanticsValidator::preferredBareMapHelperTarget") !=
        std::string::npos);
  CHECK(semanticsCollectionHelperRewritesSource.find("preferredCanonicalExperimentalMapHelperTarget(helperName)") !=
        std::string::npos);
  CHECK(semanticsCollectionHelperRewritesSource.find("canonicalExperimentalMapHelperPath(") !=
        std::string::npos);
  CHECK(semanticsCollectionHelperRewritesSource.find(
            "std::string SemanticsValidator::specializedExperimentalMapHelperTarget") !=
        std::string::npos);
  CHECK(semanticsCollectionHelperRewritesSource.find(
            "std::string SemanticsValidator::preferredBareVectorHelperTarget") != std::string::npos);
  CHECK(semanticsCollectionHelperRewritesSource.find("bool SemanticsValidator::tryRewriteBareMapHelperCall") !=
        std::string::npos);
  CHECK(semanticsCollectionHelperRewritesSource.find("bool SemanticsValidator::tryRewriteBareVectorHelperCall") !=
        std::string::npos);
  CHECK(semanticsCollectionHelperRewritesSource.find(
            "bool SemanticsValidator::tryRewriteCanonicalExperimentalMapHelperCall") !=
        std::string::npos);
  CHECK(semanticsCollectionHelperRewritesSource.find(
            "bool SemanticsValidator::explicitCanonicalExperimentalMapBorrowedHelperPath") !=
        std::string::npos);
  CHECK(semanticsCollectionHelperRewritesSource.find("dispatchResolvers.resolveExperimentalMapValueTarget") !=
        std::string::npos);
  CHECK(semanticsCollectionHelperRewritesSource.find("bool SemanticsValidator::hasResolvableMapHelperPath") !=
        std::string::npos);
  CHECK(semanticsCollectionHelperRewritesSource.find("bool SemanticsValidator::resolveMapKeyType") !=
        std::string::npos);
  CHECK(semanticsCollectionHelperRewritesSource.find("bool SemanticsValidator::resolveMapValueType") !=
        std::string::npos);

  const std::filesystem::path semanticsExprPredicatesHeaderPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprPredicates.h";
  CHECK_FALSE(std::filesystem::exists(semanticsExprPredicatesHeaderPath));
  const std::filesystem::path semanticsExprValidationHeaderPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprValidation.h";
  CHECK_FALSE(std::filesystem::exists(semanticsExprValidationHeaderPath));
