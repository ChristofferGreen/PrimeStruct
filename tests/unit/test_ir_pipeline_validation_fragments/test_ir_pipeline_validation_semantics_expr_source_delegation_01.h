  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  };
  auto readTexts = [&](const std::vector<std::filesystem::path> &paths) {
    std::string combined;
    for (const auto &path : paths) {
      combined += readText(path);
      combined.push_back('\n');
    }
    return combined;
  };
  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src")) ? std::filesystem::path(".")
                                                             : std::filesystem::path("..");

  const std::filesystem::path semanticsExprPath = repoRoot / "src" / "semantics" / "SemanticsValidatorExpr.cpp";
  const std::filesystem::path semanticsExprDispatchBootstrapPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprDispatchBootstrap.cpp";
  const std::filesystem::path semanticsExprPreDispatchDirectCallsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprPreDispatchDirectCalls.cpp";
  const std::filesystem::path semanticsExprMethodCompatibilitySetupPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprMethodCompatibilitySetup.cpp";
  const std::filesystem::path semanticsExprBlockPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprBlock.cpp";
  const std::filesystem::path semanticsExprControlFlowPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprControlFlow.cpp";
  const std::filesystem::path semanticsExprLambdaPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprLambda.cpp";
  const std::filesystem::path semanticsExprNumericPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprNumeric.cpp";
  const std::filesystem::path semanticsExprCallResolutionPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprCallResolution.cpp";
  const std::filesystem::path semanticsExprNumericPredicatesPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprNumericPredicates.cpp";
  const std::filesystem::path semanticsExprFieldResolutionPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprFieldResolution.cpp";
  const std::filesystem::path semanticsExprArgumentValidationPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprArgumentValidation.cpp";
  const std::filesystem::path semanticsExprArgumentValidationCollectionsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprArgumentValidationCollections.cpp";
  const std::filesystem::path semanticsExprCollectionPredicatesPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprCollectionPredicates.cpp";
  const std::filesystem::path semanticsExprCollectionCountCapacityPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprCollectionCountCapacity.cpp";
  const std::filesystem::path semanticsExprCountCapacityMapBuiltinsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprCountCapacityMapBuiltins.cpp";
  const std::filesystem::path semanticsExprCollectionAccessPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprCollectionAccess.cpp";
  const std::filesystem::path semanticsExprCollectionAccessValidationPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprCollectionAccessValidation.cpp";
  const std::filesystem::path semanticsExprCollectionDispatchSetupPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprCollectionDispatchSetup.cpp";
  const std::filesystem::path semanticsExprCollectionAccessSetupPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprCollectionAccessSetup.cpp";
  const std::filesystem::path semanticsExprDirectCollectionFallbacksPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprDirectCollectionFallbacks.cpp";
  const std::filesystem::path semanticsExprPostAccessPrechecksPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprPostAccessPrechecks.cpp";
  const std::filesystem::path semanticsExprBuiltinContextSetupPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprBuiltinContextSetup.cpp";
  const std::filesystem::path semanticsExprCollectionLiteralsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprCollectionLiterals.cpp";
  const std::filesystem::path semanticsExprBodyArgumentsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprBodyArguments.cpp";
  const std::filesystem::path semanticsExprLateFallbackBuiltinsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprLateFallbackBuiltins.cpp";
  const std::filesystem::path semanticsExprLateCallCompatibilityPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprLateCallCompatibility.cpp";
  const std::filesystem::path semanticsExprLateMapSoaBuiltinsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprLateMapSoaBuiltins.cpp";
  const std::filesystem::path semanticsInferMethodResolutionPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferMethodResolution.cpp";
  const std::filesystem::path semanticsExprMethodResolutionPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprMethodResolution.cpp";
  const std::filesystem::path semanticsExprMethodTargetResolutionPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprMethodTargetResolution.cpp";
  const std::filesystem::path semanticsExprLateMapAccessBuiltinsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprLateMapAccessBuiltins.cpp";
  const std::filesystem::path semanticsExprLateUnknownTargetFallbacksPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprLateUnknownTargetFallbacks.cpp";
  const std::filesystem::path semanticsExprMutationBorrowsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprMutationBorrows.cpp";
  const std::filesystem::path semanticsExprNamedArgumentBuiltinsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprNamedArgumentBuiltins.cpp";
  const std::filesystem::path semanticsExprPointerLikePath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprPointerLike.cpp";
  const std::filesystem::path semanticsExprReferenceEscapesPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprReferenceEscapes.cpp";
  const std::filesystem::path semanticsExprReceiverPathsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprReceiverPaths.cpp";
  const std::filesystem::path semanticsExprTryPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprTry.cpp";
  const std::filesystem::path semanticsCollectionHelperRewritesPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorCollectionHelperRewrites.cpp";
  const std::filesystem::path semanticsExprGpuBufferPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprGpuBuffer.cpp";
  const std::filesystem::path semanticsExprScalarPointerMemoryPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprScalarPointerMemory.cpp";
  const std::filesystem::path semanticsExprResolvedCallSetupPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprResolvedCallSetup.cpp";
  const std::filesystem::path semanticsExprResolvedCallArgumentsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprResolvedCallArguments.cpp";
  const std::filesystem::path semanticsExprStructConstructorsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprStructConstructors.cpp";
  const std::filesystem::path semanticsExprMapSoaBuiltinsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprMapSoaBuiltins.cpp";
  const std::filesystem::path semanticsExprResultFilePath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprResultFile.cpp";
  const std::filesystem::path semanticsExprVectorHelpersPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExprVectorHelpers.cpp";
  REQUIRE(std::filesystem::exists(semanticsExprPath));
  REQUIRE(std::filesystem::exists(semanticsExprDispatchBootstrapPath));
  REQUIRE(std::filesystem::exists(semanticsExprPreDispatchDirectCallsPath));
  REQUIRE(std::filesystem::exists(semanticsExprMethodCompatibilitySetupPath));
  REQUIRE(std::filesystem::exists(semanticsExprBlockPath));
  REQUIRE(std::filesystem::exists(semanticsExprControlFlowPath));
  REQUIRE(std::filesystem::exists(semanticsExprLambdaPath));
  REQUIRE(std::filesystem::exists(semanticsExprNumericPath));
  REQUIRE(std::filesystem::exists(semanticsExprCallResolutionPath));
  REQUIRE(std::filesystem::exists(semanticsExprNumericPredicatesPath));
  REQUIRE(std::filesystem::exists(semanticsExprFieldResolutionPath));
  REQUIRE(std::filesystem::exists(semanticsExprArgumentValidationPath));
  REQUIRE(std::filesystem::exists(semanticsExprArgumentValidationCollectionsPath));
  REQUIRE(std::filesystem::exists(semanticsExprCollectionPredicatesPath));
  REQUIRE(std::filesystem::exists(semanticsExprCollectionCountCapacityPath));
  REQUIRE(std::filesystem::exists(semanticsExprCountCapacityMapBuiltinsPath));
  REQUIRE(std::filesystem::exists(semanticsExprCollectionAccessPath));
  REQUIRE(std::filesystem::exists(semanticsExprCollectionAccessValidationPath));
  REQUIRE(std::filesystem::exists(semanticsExprCollectionDispatchSetupPath));
  REQUIRE(std::filesystem::exists(semanticsExprCollectionAccessSetupPath));
  REQUIRE(std::filesystem::exists(semanticsExprDirectCollectionFallbacksPath));
  REQUIRE(std::filesystem::exists(semanticsExprPostAccessPrechecksPath));
  REQUIRE(std::filesystem::exists(semanticsExprBuiltinContextSetupPath));
  REQUIRE(std::filesystem::exists(semanticsExprCollectionLiteralsPath));
  REQUIRE(std::filesystem::exists(semanticsExprBodyArgumentsPath));
  REQUIRE(std::filesystem::exists(semanticsExprLateFallbackBuiltinsPath));
  REQUIRE(std::filesystem::exists(semanticsExprLateCallCompatibilityPath));
  REQUIRE(std::filesystem::exists(semanticsExprLateMapSoaBuiltinsPath));
  REQUIRE(std::filesystem::exists(semanticsInferMethodResolutionPath));
  REQUIRE(std::filesystem::exists(semanticsExprMethodResolutionPath));
  REQUIRE(std::filesystem::exists(semanticsExprMethodTargetResolutionPath));
  REQUIRE(std::filesystem::exists(semanticsExprLateMapAccessBuiltinsPath));
  REQUIRE(std::filesystem::exists(semanticsExprLateUnknownTargetFallbacksPath));
  REQUIRE(std::filesystem::exists(semanticsExprMutationBorrowsPath));
  REQUIRE(std::filesystem::exists(semanticsExprNamedArgumentBuiltinsPath));
  REQUIRE(std::filesystem::exists(semanticsExprPointerLikePath));
  REQUIRE(std::filesystem::exists(semanticsExprReferenceEscapesPath));
  REQUIRE(std::filesystem::exists(semanticsExprReceiverPathsPath));
  REQUIRE(std::filesystem::exists(semanticsExprTryPath));
  REQUIRE(std::filesystem::exists(semanticsCollectionHelperRewritesPath));
  REQUIRE(std::filesystem::exists(semanticsExprGpuBufferPath));
  REQUIRE(std::filesystem::exists(semanticsExprScalarPointerMemoryPath));
  REQUIRE(std::filesystem::exists(semanticsExprResolvedCallSetupPath));
  REQUIRE(std::filesystem::exists(semanticsExprResolvedCallArgumentsPath));
  REQUIRE(std::filesystem::exists(semanticsExprStructConstructorsPath));
  REQUIRE(std::filesystem::exists(semanticsExprMapSoaBuiltinsPath));
  REQUIRE(std::filesystem::exists(semanticsExprResultFilePath));
  REQUIRE(std::filesystem::exists(semanticsExprVectorHelpersPath));
  const std::string semanticsExprSource = readText(semanticsExprPath);
  const std::string semanticsExprDispatchBootstrapSource =
      readText(semanticsExprDispatchBootstrapPath);
  const std::string semanticsExprPreDispatchDirectCallsSource =
      readText(semanticsExprPreDispatchDirectCallsPath);
  const std::string semanticsExprMethodCompatibilitySetupSource =
      readText(semanticsExprMethodCompatibilitySetupPath);
  const std::string semanticsExprBlockSource = readText(semanticsExprBlockPath);
  const std::string semanticsExprControlFlowSource = readText(semanticsExprControlFlowPath);
  const std::string semanticsExprLambdaSource = readText(semanticsExprLambdaPath);
  const std::string semanticsExprNumericSource = readText(semanticsExprNumericPath);
  const std::string semanticsExprCallResolutionSource = readText(semanticsExprCallResolutionPath);
  const std::string semanticsExprNumericPredicatesSource = readText(semanticsExprNumericPredicatesPath);
  const std::string semanticsExprFieldResolutionSource = readText(semanticsExprFieldResolutionPath);
  const std::string semanticsExprArgumentValidationSource = readText(semanticsExprArgumentValidationPath);
  const std::string semanticsExprArgumentValidationCombinedSource = readTexts({
      semanticsExprArgumentValidationPath,
      semanticsExprArgumentValidationCollectionsPath,
  });
  const std::string semanticsExprCollectionPredicatesSource = readText(semanticsExprCollectionPredicatesPath);
  const std::string semanticsExprCollectionCountCapacitySource =
      readText(semanticsExprCollectionCountCapacityPath);
  const std::string semanticsExprCountCapacityMapBuiltinsSource =
      readText(semanticsExprCountCapacityMapBuiltinsPath);
  const std::string semanticsExprCollectionAccessSource =
      readText(semanticsExprCollectionAccessPath);
  const std::string semanticsExprCollectionAccessValidationSource =
      readText(semanticsExprCollectionAccessValidationPath);
  const std::string semanticsExprCollectionDispatchSetupSource =
      readText(semanticsExprCollectionDispatchSetupPath);
  const std::string semanticsExprCollectionAccessSetupSource =
      readText(semanticsExprCollectionAccessSetupPath);
  const std::string semanticsExprDirectCollectionFallbacksSource =
      readText(semanticsExprDirectCollectionFallbacksPath);
  const std::string semanticsExprPostAccessPrechecksSource =
      readText(semanticsExprPostAccessPrechecksPath);
  const std::string semanticsExprBuiltinContextSetupSource =
      readText(semanticsExprBuiltinContextSetupPath);
  const std::string semanticsExprCollectionLiteralsSource = readText(semanticsExprCollectionLiteralsPath);
  const std::string semanticsExprBodyArgumentsSource =
      readText(semanticsExprBodyArgumentsPath);
  const std::string semanticsExprLateFallbackBuiltinsSource =
      readText(semanticsExprLateFallbackBuiltinsPath);
  const std::string semanticsExprLateCallCompatibilitySource =
      readText(semanticsExprLateCallCompatibilityPath);
  const std::string semanticsExprLateMapSoaBuiltinsSource =
      readText(semanticsExprLateMapSoaBuiltinsPath);
  const std::string semanticsInferMethodResolutionSource =
      readText(semanticsInferMethodResolutionPath);
  const std::string semanticsExprMethodResolutionSource =
      readText(semanticsExprMethodResolutionPath);
  const std::string semanticsExprMethodTargetResolutionSource =
      readText(semanticsExprMethodTargetResolutionPath);
  const std::string semanticsExprLateMapAccessBuiltinsSource =
      readText(semanticsExprLateMapAccessBuiltinsPath);
  const std::string semanticsExprLateUnknownTargetFallbacksSource =
      readText(semanticsExprLateUnknownTargetFallbacksPath);
  const std::string semanticsExprMutationBorrowsSource =
      readText(semanticsExprMutationBorrowsPath);
  const std::string semanticsExprNamedArgumentBuiltinsSource = readText(semanticsExprNamedArgumentBuiltinsPath);
  const std::string semanticsExprPointerLikeSource = readText(semanticsExprPointerLikePath);
  const std::string semanticsExprReferenceEscapesSource =
      readText(semanticsExprReferenceEscapesPath);
  const std::string semanticsExprReceiverPathsSource = readText(semanticsExprReceiverPathsPath);
  const std::string semanticsExprTrySource = readText(semanticsExprTryPath);
  const std::string semanticsCollectionHelperRewritesSource = readText(semanticsCollectionHelperRewritesPath);
  const std::string semanticsExprGpuBufferSource = readText(semanticsExprGpuBufferPath);
  const std::string semanticsExprScalarPointerMemorySource = readText(semanticsExprScalarPointerMemoryPath);
  const std::string semanticsExprResolvedCallSetupSource =
      readText(semanticsExprResolvedCallSetupPath);
  const std::string semanticsExprResolvedCallArgumentsSource =
      readText(semanticsExprResolvedCallArgumentsPath);
  const std::string semanticsExprStructConstructorsSource = readText(semanticsExprStructConstructorsPath);
  const std::string semanticsExprMapSoaBuiltinsSource = readText(semanticsExprMapSoaBuiltinsPath);
  const std::string semanticsExprResultFileSource = readText(semanticsExprResultFilePath);
  const std::string semanticsExprVectorHelpersSource = readText(semanticsExprVectorHelpersPath);
  CHECK(semanticsExprSource.find("bool SemanticsValidator::validateExpr") != std::string::npos);
  CHECK(semanticsExprSource.find("return validateLambdaExpr(params, locals, expr, enclosingStatements, statementIndex);") !=
        std::string::npos);
  CHECK(semanticsExprSource.find("return validateBlockExpr(params, locals, expr);") != std::string::npos);
  CHECK(semanticsExprSource.find("return validateIfExpr(params, locals, expr);") != std::string::npos);
  CHECK(semanticsExprSource.find(
            "validateExprLateFallbackBuiltins(\n"
            "              params, locals, expr, resolved, resolvedMethod,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "validateExprMutationBorrowBuiltins(\n"
            "              params, locals, expr, handledMutationBorrowBuiltin)") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "validateExprLateCallCompatibility(\n"
            "              params, locals, expr, resolved,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "validateExprMethodCallTarget(\n"
            "            params,\n"
            "            locals,\n"
            "            expr,\n"
            "            methodResolutionContext,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "matchesCtorPath(\"/std/collections/vector/vector\")") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("prepareExprDispatchBootstrap(params, locals, dispatchBootstrap);") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "validateExprPreDispatchDirectCalls(\n"
            "            params,\n"
            "            locals,\n"
            "            expr,\n"
            "            preDispatchDirectCallContext,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "prepareExprMethodCompatibilitySetup(\n"
            "            params,\n"
            "            locals,\n"
            "            expr,\n"
            "            dispatchBootstrap,") !=
        std::string::npos);
  CHECK(semanticsExprMethodCompatibilitySetupSource.find("splitSoaFieldViewHelperPath(") ==
        std::string::npos);
  CHECK(semanticsExprMethodCompatibilitySetupSource.find("soaBorrowedViewPendingDiagnostic()") ==
        std::string::npos);
  CHECK(semanticsExprMethodCompatibilitySetupSource.find(
            "auto failMethodCompatibilityDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(semanticsExprMethodCompatibilitySetupSource.find(
            "return failExprDiagnostic(expr, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "auto failMethodTargetResolutionDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "return failExprDiagnostic(receiver, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "std::optional<std::string> rememberedMethodTargetTraceFailure;") !=
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "rememberedMethodTargetTraceFailure = std::move(message);") !=
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "if (resolvedOut == \"/std/collections/vector/count\" ||\n"
            "        resolvedOut == \"/std/collections/vector/capacity\" ||") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "if (resolvedOut == \"/std/collections/vector/at\" ||\n"
            "        resolvedOut == \"/std/collections/vector/at_unsafe\")") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "if ((resolvedOut == \"/std/collections/vector/at\" ||\n"
            "         resolvedOut == \"/std/collections/vector/at_unsafe\") &&") !=
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "if ((resolvedOut == \"/std/collections/vector/at\" ||\n"
            "         resolvedOut == \"/std/collections/vector/at_unsafe\") &&\n"
            "        explicitVectorHelperPath.empty() &&") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "explicitVectorHelperPath == \"/vector/count\" &&\n"
            "      (explicitVectorReceiverFamily == \"string\" ||") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "if (explicitVectorHelperPath.rfind(\"/vector/\", 0) == 0 &&\n"
            "      (normalizedMethodName == \"count\" || normalizedMethodName == \"capacity\" ||\n"
            "       normalizedMethodName == \"at\" || normalizedMethodName == \"at_unsafe\") &&\n"
            "      (explicitVectorReceiverFamily == \"vector\" ||\n"
            "       explicitVectorReceiverFamily == \"experimental_vector\" ||\n"
            "       explicitVectorReceiverFamily == \"soa_vector\"))") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "if (explicitVectorHelperPath == \"/vector/count\" &&\n"
            "      (explicitVectorReceiverFamily == \"vector\" ||\n"
            "       explicitVectorReceiverFamily == \"experimental_vector\" ||\n"
            "       explicitVectorReceiverFamily == \"soa_vector\"))") !=
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "if (explicitVectorHelperPath == \"/vector/capacity\" &&\n"
            "      (explicitVectorReceiverFamily == \"vector\" ||\n"
            "       explicitVectorReceiverFamily == \"experimental_vector\" ||\n"
            "       explicitVectorReceiverFamily == \"soa_vector\"))") !=
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "if (explicitVectorHelperPath == \"/vector/at\" &&\n"
            "      (explicitVectorReceiverFamily == \"vector\" ||\n"
            "       explicitVectorReceiverFamily == \"experimental_vector\" ||\n"
            "       explicitVectorReceiverFamily == \"soa_vector\"))") !=
        std::string::npos);
  CHECK(semanticsExprCollectionCountCapacitySource.find(
            "auto rejectsRootedVectorBuiltinAlias =") ==
        std::string::npos);
  CHECK(semanticsExprCollectionCountCapacitySource.find(
            "auto rejectsRemovedRootedVectorDirectCall =") ==
        std::string::npos);
  CHECK(semanticsExprCollectionCountCapacitySource.find(
            "const std::string removedRootedVectorDirectCallDiagnostic =\n"
            "      getRemovedRootedVectorDirectCallDiagnostic(expr);") !=
        std::string::npos);
  CHECK(semanticsExprCollectionCountCapacitySource.find(
            "context.isNamespacedVectorCountCall &&\n"
            "        expr.args.size() == 1 &&\n"
            "        expr.args.front().kind == Expr::Kind::Call &&") ==
        std::string::npos);
  CHECK(semanticsExprCollectionCountCapacitySource.find(
            "context.isNamespacedVectorCountCall &&\n"
            "        expr.args.size() == 1 &&\n"
            "        !hasDeclaredDefinitionPath(\"/vector/count\") &&") ==
        std::string::npos);
  CHECK(semanticsExprCollectionCountCapacitySource.find(
            "context.isNamespacedVectorCapacityCall &&\n"
            "        expr.args.size() == 1 &&\n"
            "        !hasDeclaredDefinitionPath(\"/vector/capacity\") &&") ==
        std::string::npos);
  CHECK(semanticsExprCollectionCountCapacitySource.find(
            "if (!removedRootedVectorDirectCallDiagnostic.empty()) {\n"
            "      return failCollectionCountCapacityDiagnostic(\n"
            "          removedRootedVectorDirectCallDiagnostic);") !=
        std::string::npos);
  CHECK(semanticsExprCollectionCountCapacitySource.find(
            "getRemovedRootedVectorDirectCallDiagnostic(expr)") !=
        std::string::npos);
  CHECK(semanticsExprDirectCollectionFallbacksSource.find(
            "const std::string removedRootedVectorDirectCallDiagnostic =\n"
            "      getRemovedRootedVectorDirectCallDiagnostic(expr);") !=
        std::string::npos);
  CHECK(semanticsExprDirectCollectionFallbacksSource.find(
            "explicitPath == \"/vector/at\" || explicitPath == \"/vector/at_unsafe\"") ==
        std::string::npos);
  CHECK(semanticsExprDirectCollectionFallbacksSource.find(
            "if (!removedRootedVectorDirectCallDiagnostic.empty()) {\n"
            "    return failDirectCollectionFallbackDiagnostic(\n"
            "        removedRootedVectorDirectCallDiagnostic);\n"
            "  }") !=
        std::string::npos);
  CHECK(semanticsExprDirectCollectionFallbacksSource.find(
            "getRemovedRootedVectorDirectCallDiagnostic(expr)") !=
        std::string::npos);
  CHECK(semanticsExprCollectionCountCapacitySource.find(
            "getRemovedRootedVectorDirectCallDiagnostic(expr, \"count\", \"capacity\")") ==
        std::string::npos);
  CHECK(semanticsExprDirectCollectionFallbacksSource.find(
            "getRemovedRootedVectorDirectCallDiagnostic(expr, \"at\", \"at_unsafe\")") ==
        std::string::npos);
  CHECK(semanticsInferMethodResolutionSource.find(
            "getRemovedRootedVectorDirectCallPath(") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "(normalizedMethodName == \"count\" || normalizedMethodName == \"capacity\") &&\n"
            "      (explicitVectorReceiverFamily == \"string\" ||") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "explicitVectorHelperPath.rfind(\"/std/collections/vector/\", 0) == 0 &&\n"
            "      normalizedMethodName == \"count\" &&") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "(normalizedMethodName == \"count\" &&\n"
            "          explicitVectorHelperPath.rfind(\"/std/collections/vector/\", 0) == 0 &&\n"
            "          receiverCollectionTypePath == \"/map\")") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "(normalizedMethodName == \"capacity\" &&\n"
            "          explicitVectorHelperPath.rfind(\"/std/collections/vector/\", 0) == 0 &&\n"
            "          receiverCollectionTypePath != \"/vector\" && receiverCollectionTypePath != \"/soa_vector\")") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "(normalizedMethodName == \"count\" || normalizedMethodName == \"capacity\") &&\n"
            "      (normalizedTypeName == \"string\" || normalizedTypeName == \"array\" ||\n"
            "       isMapCollectionTypeName(normalizedTypeName))") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "normalizedMethodName == \"count\" &&\n"
            "      (normalizedTypeName == \"string\" || normalizedTypeName == \"array\" ||\n"
            "       isMapCollectionTypeName(normalizedTypeName))") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "if ((removedVectorMethodCompatibilityPath == \"/std/collections/vector/count\" ||\n"
            "         removedVectorMethodCompatibilityPath == \"/std/collections/vector/capacity\") &&") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "if (removedVectorMethodCompatibilityPath == \"/std/collections/vector/capacity\" &&") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "explicitVectorReceiverFamily + \"/count\"") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "if (explicitVectorHelperPath.rfind(\"/vector/\", 0) == 0 &&\n"
            "      normalizedMethodName == \"at_unsafe\" &&\n"
            "      (explicitVectorReceiverFamily == \"vector\" ||\n"
            "       explicitVectorReceiverFamily == \"experimental_vector\" ||\n"
            "       explicitVectorReceiverFamily == \"soa_vector\"))") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "const bool isExplicitRootedVectorMethod =") !=
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "const bool isExplicitVectorFamilyReceiver =") !=
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "if (isExplicitRootedVectorMethod && isExplicitVectorFamilyReceiver)") !=
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "if (explicitVectorHelperPath == \"/vector/count\" &&\n"
            "      (explicitVectorReceiverFamily == \"vector\" ||\n"
            "       explicitVectorReceiverFamily == \"experimental_vector\" ||\n"
            "       explicitVectorReceiverFamily == \"soa_vector\"))") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "if (explicitVectorHelperPath == \"/vector/capacity\" &&\n"
            "      (explicitVectorReceiverFamily == \"vector\" ||\n"
            "       explicitVectorReceiverFamily == \"experimental_vector\" ||\n"
            "       explicitVectorReceiverFamily == \"soa_vector\"))") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "if (explicitVectorHelperPath == \"/vector/at\" &&\n"
            "      (explicitVectorReceiverFamily == \"vector\" ||\n"
            "       explicitVectorReceiverFamily == \"experimental_vector\" ||\n"
            "       explicitVectorReceiverFamily == \"soa_vector\"))") ==
        std::string::npos);
  CHECK(semanticsExprMethodTargetResolutionSource.find(
            "if (explicitVectorHelperPath == \"/vector/at_unsafe\" &&\n"
            "      (explicitVectorReceiverFamily == \"vector\" ||\n"
            "       explicitVectorReceiverFamily == \"experimental_vector\" ||\n"
            "       explicitVectorReceiverFamily == \"soa_vector\"))") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "auto isExplicitVectorCompatibilityMethodWithTemplateArgs = [&]() {") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "if (resolved == \"/std/collections/vector/count\" &&") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "auto explicitVectorCountMethodMissingSamePathHelper = [&]() -> std::string {") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "auto explicitVectorCountCapacityMethodMissingSamePathHelper = [&]() -> std::string {") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "normalizedName == \"std/collections/vector/capacity\"") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "if (normalizedMethodNamespace == \"vector\" &&\n"
            "      (expr.name == \"count\" || expr.name == \"capacity\" ||\n"
            "       expr.name == \"at\" || expr.name == \"at_unsafe\") &&\n"
            "      !expr.args.empty())") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "const std::string preferredVisibleVectorMethodTarget =") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "const bool hasVisiblePreferredVectorMethodTarget =") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "const bool hasExplicitVectorCompatibilityNamespace =") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "const bool isVectorCompatibilityMethodName =") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "bool hasExplicitVectorCompatibilityNamespace(std::string_view normalizedMethodNamespace)") !=
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "bool isVectorCompatibilityMethodName(std::string_view helperName)") !=
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "bool isExperimentalVectorCompatibilityMethodTarget(std::string_view methodTarget)") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "bool isExperimentalVectorCompatibilityResolvedMethodTarget(std::string_view methodTarget)") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "const std::string canonicalVectorMethodTarget =\n"
            "      \"/std/collections/vector/\" + expr.name;") !=
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "const bool hasVisibleCanonicalVectorMethodTarget =\n"
            "      hasImportedDefinitionPath(canonicalVectorMethodTarget) ||\n"
            "      defMap_.count(canonicalVectorMethodTarget) > 0;") !=
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "auto preferVisibleCanonicalVectorMethodTarget =") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "auto preferResolvedVectorCompatibilityMethodTarget =") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "const bool isExperimentalVectorMethodTarget =") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "bool requireCurrentTargetMissing") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "if (vectorMethodTarget.rfind(\"/std/collections/experimental_vector/\", 0) == 0 &&") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "if (!hasImportedDefinitionPath(vectorMethodTarget) &&\n"
            "        defMap_.count(vectorMethodTarget) == 0 &&") !=
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "if (!hasImportedDefinitionPath(vectorMethodTarget) &&\n"
            "        defMap_.count(vectorMethodTarget) == 0 &&\n"
            "        vectorMethodTarget.rfind(\"/std/collections/experimental_vector/\", 0) == 0 &&\n"
            "        hasVisibleCanonicalVectorMethodTarget) {\n"
            "      vectorMethodTarget = canonicalVectorMethodTarget;") !=
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "preferVisibleCanonicalVectorMethodTarget(resolved)") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "preferVisibleCanonicalVectorMethodTarget(vectorMethodTarget)") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "if (!isBuiltinMethod && isVectorCompatibilityMethodName(expr.name) &&\n"
            "      resolved.rfind(\"/std/collections/experimental_vector/Vector__\", 0) == 0 &&\n"
            "      hasVisibleCanonicalVectorMethodTarget) {\n"
            "    resolved = preferVectorStdlibHelperPath(canonicalVectorMethodTarget);") !=
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "if (!isBuiltinMethod && isVectorCompatibilityMethodName(expr.name) &&\n"
            "      resolved.rfind(\"/std/collections/experimental_vector/Vector__\", 0) == 0) {") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "methodTarget.rfind(\"/std/collections/experimental_vector/Vector__\", 0) != 0") ==
        std::string::npos);
  CHECK(semanticsExprMethodResolutionSource.find(
            "expr.args.front().kind == Expr::Kind::Call &&\n"
            "      !expr.args.front().isBinding &&\n"
            "      !expr.args.front().isMethodCall &&\n"
            "      resolveCallCollectionTypePath(expr.args.front(), params, locals, receiverCollectionTypePath) &&\n"
            "      receiverCollectionTypePath == \"/vector\"") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "resolved == \"/std/collections/vector/count\"") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "resolved == \"/std/collections/vector/capacity\"") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "resolved == \"/std/collections/vector/at\"") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "resolved == \"/std/collections/vector/at_unsafe\"") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "const bool isStdNamespacedVectorCanonicalCompatibilityHelper =") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "bool isStdNamespacedVectorCanonicalCompatibilityHelperPath(") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "bool isStdNamespacedVectorCanonicalCompatibilityDirectCall(") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "bool isStdNamespacedVectorCanonicalCompatibilityMethodCall(") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "bool isStdNamespacedVectorCanonicalDirectCallReceiverCompatible(") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "bool shouldProbePositionalReorderedVectorHelperReceiver(") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "const bool isStdNamespacedVectorCanonicalCompatibilityDirectCallSite =") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "const bool isStdNamespacedVectorCanonicalCountCapacityNamedArgException =") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "const bool resolvedVectorHelperDefinitionMissing =") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "size_t namedValuesReceiverIndex = expr.args.size();") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "const bool hasNamedValuesReceiver = [&]() {") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "auto tryResolveRemainingReceivers = [&](size_t startIndex) {") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "auto markResolvedReceiver = [&](bool didResolve) {") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "auto tryResolvePrimaryReceiver = [&]() {") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "auto tryResolveNamedValuesReceiver = [&]() {") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "auto tryResolveNamedFallbackReceivers = [&]() {") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "auto tryResolveNamedInitialReceivers = [&]() {") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "auto tryResolvePositionalInitialReceiver = [&]() {") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "auto tryResolveInitialReceiverForCallShape = [&]() {") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "auto tryResolveReorderedReceiver = [&]() {") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "expr.isMethodCall &&\n"
            "      isStdNamespacedVectorCanonicalCompatibilityHelperPath(") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "isStdNamespacedVectorCanonicalCompatibilityDirectCallSite &&\n"
            "        (namespacedHelper == \"count\" || namespacedHelper == \"capacity\") &&\n"
            "        hasNamedArgs") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "receiverFamily == \"vector\" || receiverFamily == \"experimental_vector\"") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "defMap_.find(resolved) == defMap_.end()") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "bool hasValuesNamedReceiver = false;") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "if (!hasNamedValuesReceiver && !resolvedReceiver)") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "} else {\n      resolvedReceiver = tryResolveReceiverIndex(0);") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "resolvedReceiver = true;") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "resolvedReceiver = tryResolveReceiverIndex(0);") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "if (hasNamedValuesReceiver) {\n"
            "        resolvedReceiver = tryResolveReceiverIndex(namedValuesReceiverIndex);") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "if (hasNamedArgs) {\n"
            "      tryResolveNamedInitialReceivers();") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "auto tryResolveInitialReceiver = [&]() {") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "} else {\n      tryResolvePrimaryReceiver();") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "if (hasNamedArgs) {\n"
            "      tryResolveNamedInitialReceivers();\n"
            "    } else {\n"
            "      tryResolvePositionalInitialReceiver();\n"
            "    }") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "if (shouldProbePositionalReorderedVectorHelperReceiver(\n"
            "            expr, isStdNamespacedVectorCanonicalCompatibilityDirectCallSite,\n"
            "            hasNamedArgs, params, locals) &&\n"
            "        !resolvedReceiver) {") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "for (size_t i = 1; i < expr.args.size(); ++i) {\n"
            "          if (tryResolveReceiverIndex(i)) {") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "const bool probePositionalReorderedReceiver =") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "const bool isExplicitStdNamespacedVectorCompatibilityMethod =") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "const bool allowStdNamespacedUserReceiverProbe =") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "const bool shouldProbeVectorHelperReceiver =") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "const bool isStdNamespacedVectorCanonicalHelperCall =") ==
        std::string::npos);
  const auto vectorHelpersHasNamedArgsPos = semanticsExprVectorHelpersSource.find(
      "const bool hasNamedArgs = hasNamedArguments(expr.argNames);");
  CHECK(vectorHelpersHasNamedArgsPos != std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.rfind(
            "const bool hasNamedArgs = hasNamedArguments(expr.argNames);") ==
        vectorHelpersHasNamedArgsPos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "const bool hasVisibleStdNamespacedVectorCanonicalHelper =") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "const bool builtinCompatibleReceiver =") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "const bool hasCompatibleDeclaredHelper =") ==
        std::string::npos);
  CHECK(semanticsExprLateCallCompatibilitySource.find(
            "if (resolved == \"/std/collections/vector/count\" &&\n"
            "        hasImportedDefinitionPath(\"/std/collections/vector/count\") &&\n"
            "        (resolvesVector || resolvesExperimentalVector))") ==
        std::string::npos);
  CHECK(semanticsExprLateCallCompatibilitySource.find(
            "if (resolved == \"/std/collections/vector/capacity\" &&\n"
            "        hasImportedDefinitionPath(\"/std/collections/vector/capacity\"))") ==
        std::string::npos);
  CHECK(semanticsExprMethodCompatibilitySetupSource.find(
            "return soaUnavailableMethodDiagnostic(resolvedPath);") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "prepareExprCollectionDispatchSetup(\n"
            "            params,\n"
            "            locals,\n"
            "            expr,\n"
            "            dispatchBootstrap.dispatchResolvers,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "validateExprDirectCollectionFallbacks(\n"
            "            params,\n"
            "            locals,\n"
            "            expr,\n"
            "            resolved,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "prepareExprCollectionAccessDispatchContext(\n"
            "        collectionDispatchSetup,\n"
            "        shouldBuiltinValidateBareMapContainsCall,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "validateExprPostAccessPrechecks(\n"
            "            params,\n"
            "            locals,\n"
            "            expr,\n"
            "            resolved,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "prepareExprNamedArgumentBuiltinContext(\n"
            "        hasVectorHelperCallResolution,\n"
            "        dispatchBootstrap.dispatchResolvers,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "prepareExprLateBuiltinContext(\n"
            "        params,\n"
            "        locals,\n"
            "        dispatchBootstrap.dispatchResolverAdapters,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "prepareExprCountCapacityMapBuiltinContext(\n"
            "        collectionDispatchSetup.shouldBuiltinValidateStdNamespacedVectorCountCall,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "prepareExprLateMapSoaBuiltinContext(\n"
            "          shouldBuiltinValidateBareMapContainsCall,\n"
            "          dispatchBootstrap.dispatchResolvers,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "prepareExprLateFallbackBuiltinContext(\n"
            "          collectionDispatchSetup.isStdNamespacedVectorAccessCall,\n"
            "          collectionDispatchSetup.shouldAllowStdAccessCompatibilityFallback,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "prepareExprLateCallCompatibilityContext(\n"
            "          dispatchBootstrap.dispatchResolvers,\n"
            "          lateCallCompatibilityContext);") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "prepareExprLateMapAccessBuiltinContext(\n"
            "          dispatchBootstrap.dispatchResolvers,\n"
            "          shouldBuiltinValidateBareMapContainsCall,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "validateExprLateMapAccessBuiltins(\n"
            "              params, locals, expr, resolved,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "validateExprLateUnknownTargetFallbacks(\n"
            "              params, locals, expr, lateUnknownTargetFallbackContext,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "prepareExprResolvedCallSetup(\n"
            "        params,\n"
            "        locals,\n"
            "        expr,\n"
            "        resolved,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "validateExprResolvedStructConstructorCall(\n"
            "            params, locals, expr, resolved,\n"
            "            resolvedCallSetup.resolvedStructConstructorContext,") !=
        std::string::npos);
  CHECK(semanticsExprSource.find(
            "validateExprResolvedCallArguments(\n"
            "            params, locals, expr, resolved,\n"
            "            resolvedCallSetup.resolvedCallArgumentContext,") !=
        std::string::npos);
  CHECK(semanticsExprNamedArgumentBuiltinsSource.find(
            "auto tryResolveReceiverIndex = [&](size_t index)") !=
        std::string::npos);
  CHECK(semanticsExprNamedArgumentBuiltinsSource.find(
            "return resolveMethodTarget(params, locals, expr.namespacePrefix, receiverCandidate,") !=
        std::string::npos);
  CHECK(semanticsExprNamedArgumentBuiltinsSource.find("std::vector<size_t> receiverIndices;") ==
        std::string::npos);
  CHECK(semanticsExprNamedArgumentBuiltinsSource.find("appendUniqueReceiverIndex(") ==
        std::string::npos);
  CHECK(semanticsExprCollectionAccessSource.find(
            "auto tryResolveReceiverIndex = [&](size_t receiverIndex)") !=
        std::string::npos);
  CHECK(semanticsExprCollectionAccessSource.find(
            "for (size_t receiverIndex : receiverIndices) {\n"
            "      const Expr &receiverCandidate = expr.args[receiverIndex];\n"
            "      std::string methodTarget;\n"
            "      if (resolveVectorHelperMethodTarget(params, locals, receiverCandidate, expr.name, methodTarget)) {") ==
        std::string::npos);
  CHECK(semanticsExprCollectionAccessSource.find(
            "if (failedReceiverProbe) {\n"
            "      return false;\n"
            "    }") !=
        std::string::npos);
  CHECK(semanticsExprCollectionAccessSource.find(
            "std::any_of(expr.args.begin() + 1, expr.args.end(),") !=
        std::string::npos);
  CHECK(semanticsExprCollectionAccessSource.find(
            "tryResolveReceiverIndex(0, hasAlternativeCollectionReceiver)") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "matchesVectorCtorPath(\"/std/collections/vector/vector\")") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "matchesVectorCtorPath(\"/std/collections/experimental_vector/vector\")") ==
        std::string::npos);
  CHECK(semanticsExprCollectionAccessSource.find("std::vector<size_t> receiverIndices;") ==
        std::string::npos);
  CHECK(semanticsExprCollectionAccessSource.find("appendUniqueReceiverIndex(") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find(
            "auto tryResolveReceiverIndex = [&](size_t receiverIndex)") !=
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find("std::vector<size_t> receiverIndices;") ==
        std::string::npos);
  CHECK(semanticsExprVectorHelpersSource.find("appendUniqueIndex(") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("const BuiltinCollectionDispatchResolverAdapters builtinCollectionDispatchResolverAdapters{") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("const BuiltinCollectionDispatchResolvers builtinCollectionDispatchResolvers =") ==
        std::string::npos);
  CHECK(semanticsExprSource.find("makeBuiltinCollectionDispatchResolvers(params, locals, builtinCollectionDispatchResolverAdapters)") ==
        std::string::npos);
