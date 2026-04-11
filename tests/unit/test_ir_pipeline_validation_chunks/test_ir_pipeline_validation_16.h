TEST_CASE("emitter expr source delegation stays stable") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  };
  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src")) ? std::filesystem::path(".")
                                                             : std::filesystem::path("..");

  const std::filesystem::path emitterExprControlHeaderPath = repoRoot / "src" / "emitter" / "EmitterExprControl.h";
  REQUIRE(std::filesystem::exists(emitterExprControlHeaderPath));
  const std::string emitterExprControlHeaderSource = readText(emitterExprControlHeaderPath);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlNameStep(") != std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlBoolLiteralStep(expr)") != std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlBuiltinBlockEarlyReturnStep(") !=
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlBuiltinBlockBindingAutoStep(") !=
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlBuiltinBlockBindingExplicitStep(") !=
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlBuiltinBlockBindingFallbackStep(") !=
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlBuiltinBlockBindingQualifiersStep(") !=
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlBuiltinBlockStatementStep(") !=
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlBuiltinBlockBindingPreludeStep(") !=
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlBuiltinBlockFinalValueStep(") !=
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlBuiltinBlockPreludeStep(") !=
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlBodyWrapperStep(") != std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlFieldAccessStep(") != std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlCallPathStep(") != std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlCountRewriteStep(") != std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlIntegerLiteralStep(expr)") != std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlFloatLiteralStep(expr)") != std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlStringLiteralStep(expr)") != std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlMethodPathStep(") != std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlIfBlockBindingAutoStep(") ==
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlIfBlockBindingExplicitStep(") ==
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlIfBlockBindingFallbackStep(") ==
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlIfBranchValueStep(") ==
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlIfBranchEmitStep(") !=
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlIfBlockBindingPreludeStep(") ==
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlIfBlockBindingQualifiersStep(") ==
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlIfBlockStatementStep(") ==
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlIfBlockFinalValueStep(") ==
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlIfBlockEarlyReturnStep(") ==
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlIfTernaryFallbackStep(") !=
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlIfTernaryStep(") !=
        std::string::npos);
  CHECK(emitterExprControlHeaderSource.find("runEmitterExprControlIfBlockEnvelopeStep(candidateExpr)") !=
        std::string::npos);

  const std::filesystem::path emitterExprPath = repoRoot / "src" / "emitter" / "EmitterExpr.cpp";
  REQUIRE(std::filesystem::exists(emitterExprPath));
  const std::string emitterExprSource = readText(emitterExprPath);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlNameStep.h\"") != std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlBuiltinBlockEarlyReturnStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlBuiltinBlockBindingAutoStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlBuiltinBlockBindingExplicitStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlBuiltinBlockBindingFallbackStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlBuiltinBlockBindingQualifiersStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlBuiltinBlockStatementStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlBuiltinBlockBindingPreludeStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlBuiltinBlockFinalValueStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlBoolLiteralStep.h\"") != std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlBuiltinBlockPreludeStep.h\"") != std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlBodyWrapperStep.h\"") != std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlCallPathStep.h\"") != std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlCountRewriteStep.h\"") != std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlFieldAccessStep.h\"") != std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIntegerLiteralStep.h\"") != std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlFloatLiteralStep.h\"") != std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIfBlockBindingAutoStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIfBlockBindingExplicitStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIfBlockBindingFallbackStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIfBranchPreludeStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIfBranchBodyStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIfBranchBodyReturnStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIfBranchBodyBindingStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIfBranchBodyHandlersStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIfBranchEmitStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIfBranchValueStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIfBranchBodyStatementStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIfBranchWrapperStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIfBlockBindingPreludeStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIfBlockBindingQualifiersStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIfBlockStatementStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIfBlockFinalValueStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIfBlockEarlyReturnStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIfTernaryFallbackStep.h\"") !=
        std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIfTernaryStep.h\"") != std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlMethodPathStep.h\"") != std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlStringLiteralStep.h\"") != std::string::npos);
  CHECK(emitterExprSource.find("#include \"EmitterExprControlIfEnvelopeStep.h\"") != std::string::npos);
}

TEST_CASE("semantics validator expr source delegation stays stable") {
#include "../test_ir_pipeline_validation_fragments/test_ir_pipeline_validation_semantics_expr_source_delegation_01.h"
#include "../test_ir_pipeline_validation_fragments/test_ir_pipeline_validation_semantics_expr_source_delegation_02.h"
#include "../test_ir_pipeline_validation_fragments/test_ir_pipeline_validation_semantics_expr_source_delegation_03.h"
}
TEST_CASE("template monomorph source delegation stays stable") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  };
  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src")) ? std::filesystem::path(".")
                                                             : std::filesystem::path("..");

  const std::filesystem::path templateMonomorphPath = repoRoot / "src" / "semantics" / "TemplateMonomorph.cpp";
  const std::filesystem::path templateMonomorphFallbackPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphFallbackTypeInference.h";
  const std::filesystem::path templateMonomorphBindingCallPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphBindingCallInference.h";
  const std::filesystem::path templateMonomorphBindingBlockPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphBindingBlockInference.h";
  const std::filesystem::path templateMonomorphMethodTargetsPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphMethodTargets.h";
  const std::filesystem::path templateMonomorphTypeResolutionPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphTypeResolution.h";
  const std::filesystem::path templateMonomorphAssignmentTargetResolutionPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphAssignmentTargetResolution.h";
  const std::filesystem::path templateMonomorphExperimentalCollectionArgumentRewritesPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExperimentalCollectionArgumentRewrites.h";
  const std::filesystem::path templateMonomorphExperimentalCollectionConstructorRewritesPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExperimentalCollectionConstructorRewrites.h";
  const std::filesystem::path templateMonomorphExperimentalCollectionTargetValueRewritesPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExperimentalCollectionTargetValueRewrites.h";
  const std::filesystem::path templateMonomorphExperimentalCollectionValueRewritesPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExperimentalCollectionValueRewrites.h";
  const std::filesystem::path templateMonomorphExperimentalCollectionReceiverResolutionPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExperimentalCollectionReceiverResolution.h";
  const std::filesystem::path templateMonomorphExperimentalCollectionConstructorPathsPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExperimentalCollectionConstructorPaths.h";
  const std::filesystem::path templateMonomorphExperimentalCollectionReturnRewritesPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExperimentalCollectionReturnRewrites.h";
  const std::filesystem::path templateMonomorphExperimentalCollectionReturnSetupPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExperimentalCollectionReturnSetup.h";
  const std::filesystem::path templateMonomorphDefinitionBindingSetupPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphDefinitionBindingSetup.h";
  const std::filesystem::path templateMonomorphDefinitionReturnOrchestrationPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphDefinitionReturnOrchestration.h";
  const std::filesystem::path templateMonomorphDefinitionExperimentalCollectionRewritesPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphDefinitionExperimentalCollectionRewrites.h";
  const std::filesystem::path templateMonomorphExecutionRewritesPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExecutionRewrites.h";
  const std::filesystem::path templateMonomorphDefinitionRewritesPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphDefinitionRewrites.h";
  const std::filesystem::path templateMonomorphTemplateSpecializationPath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphTemplateSpecialization.h";
  const std::filesystem::path templateMonomorphImplicitTemplateInferencePath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphImplicitTemplateInference.h";
  const std::filesystem::path templateMonomorphExpressionRewritePath =
      repoRoot / "src" / "semantics" / "TemplateMonomorphExpressionRewrite.h";
  REQUIRE(std::filesystem::exists(templateMonomorphPath));
  REQUIRE(std::filesystem::exists(templateMonomorphFallbackPath));
  REQUIRE(std::filesystem::exists(templateMonomorphBindingCallPath));
  REQUIRE(std::filesystem::exists(templateMonomorphBindingBlockPath));
  REQUIRE(std::filesystem::exists(templateMonomorphMethodTargetsPath));
  REQUIRE(std::filesystem::exists(templateMonomorphTypeResolutionPath));
  REQUIRE(std::filesystem::exists(templateMonomorphAssignmentTargetResolutionPath));
  REQUIRE(std::filesystem::exists(templateMonomorphExperimentalCollectionArgumentRewritesPath));
  REQUIRE(std::filesystem::exists(templateMonomorphExperimentalCollectionConstructorRewritesPath));
  REQUIRE(std::filesystem::exists(templateMonomorphExperimentalCollectionTargetValueRewritesPath));
  REQUIRE(std::filesystem::exists(templateMonomorphExperimentalCollectionValueRewritesPath));
  REQUIRE(std::filesystem::exists(templateMonomorphExperimentalCollectionReceiverResolutionPath));
  REQUIRE(std::filesystem::exists(templateMonomorphExperimentalCollectionConstructorPathsPath));
  REQUIRE(std::filesystem::exists(templateMonomorphExperimentalCollectionReturnRewritesPath));
  REQUIRE(std::filesystem::exists(templateMonomorphExperimentalCollectionReturnSetupPath));
  REQUIRE(std::filesystem::exists(templateMonomorphDefinitionBindingSetupPath));
  REQUIRE(std::filesystem::exists(templateMonomorphDefinitionReturnOrchestrationPath));
  REQUIRE(std::filesystem::exists(templateMonomorphDefinitionExperimentalCollectionRewritesPath));
  REQUIRE(std::filesystem::exists(templateMonomorphExecutionRewritesPath));
  REQUIRE(std::filesystem::exists(templateMonomorphDefinitionRewritesPath));
  REQUIRE(std::filesystem::exists(templateMonomorphTemplateSpecializationPath));
  REQUIRE(std::filesystem::exists(templateMonomorphImplicitTemplateInferencePath));
  REQUIRE(std::filesystem::exists(templateMonomorphExpressionRewritePath));
  const std::string templateMonomorphSource = readText(templateMonomorphPath);
  const std::string templateMonomorphFallbackSource = readText(templateMonomorphFallbackPath);
  const std::string templateMonomorphBindingCallSource = readText(templateMonomorphBindingCallPath);
  const std::string templateMonomorphBindingBlockSource = readText(templateMonomorphBindingBlockPath);
  const std::string templateMonomorphMethodTargetsSource = readText(templateMonomorphMethodTargetsPath);
  const std::string templateMonomorphTypeResolutionSource = readText(templateMonomorphTypeResolutionPath);
  const std::string templateMonomorphAssignmentTargetResolutionSource =
      readText(templateMonomorphAssignmentTargetResolutionPath);
  const std::string templateMonomorphExperimentalCollectionArgumentRewritesSource =
      readText(templateMonomorphExperimentalCollectionArgumentRewritesPath);
  const std::string templateMonomorphExperimentalCollectionConstructorRewritesSource =
      readText(templateMonomorphExperimentalCollectionConstructorRewritesPath);
  const std::string templateMonomorphExperimentalCollectionTargetValueRewritesSource =
      readText(templateMonomorphExperimentalCollectionTargetValueRewritesPath);
  const std::string templateMonomorphExperimentalCollectionValueRewritesSource =
      readText(templateMonomorphExperimentalCollectionValueRewritesPath);
  const std::string templateMonomorphExperimentalCollectionReceiverResolutionSource =
      readText(templateMonomorphExperimentalCollectionReceiverResolutionPath);
  const std::string templateMonomorphExperimentalCollectionConstructorPathsSource =
      readText(templateMonomorphExperimentalCollectionConstructorPathsPath);
  const std::string templateMonomorphExperimentalCollectionReturnRewritesSource =
      readText(templateMonomorphExperimentalCollectionReturnRewritesPath);
  const std::string templateMonomorphExperimentalCollectionReturnSetupSource =
      readText(templateMonomorphExperimentalCollectionReturnSetupPath);
  const std::string templateMonomorphDefinitionBindingSetupSource =
      readText(templateMonomorphDefinitionBindingSetupPath);
  const std::string templateMonomorphDefinitionReturnOrchestrationSource =
      readText(templateMonomorphDefinitionReturnOrchestrationPath);
  const std::string templateMonomorphDefinitionExperimentalCollectionRewritesSource =
      readText(templateMonomorphDefinitionExperimentalCollectionRewritesPath);
  const std::string templateMonomorphExecutionRewritesSource =
      readText(templateMonomorphExecutionRewritesPath);
  const std::string templateMonomorphDefinitionRewritesSource =
      readText(templateMonomorphDefinitionRewritesPath);
  const std::string templateMonomorphTemplateSpecializationSource =
      readText(templateMonomorphTemplateSpecializationPath);
  const std::string templateMonomorphImplicitTemplateInferenceSource =
      readText(templateMonomorphImplicitTemplateInferencePath);
  const std::string templateMonomorphExpressionRewriteSource =
      readText(templateMonomorphExpressionRewritePath);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphFallbackTypeInference.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphBindingCallInference.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphBindingBlockInference.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphMethodTargets.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphTypeResolution.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphAssignmentTargetResolution.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphExperimentalCollectionArgumentRewrites.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphExperimentalCollectionConstructorRewrites.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphExperimentalCollectionTargetValueRewrites.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphExperimentalCollectionValueRewrites.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphExperimentalCollectionReceiverResolution.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphExperimentalCollectionConstructorPaths.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphExperimentalCollectionReturnRewrites.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphExperimentalCollectionReturnSetup.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphDefinitionBindingSetup.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphDefinitionReturnOrchestration.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphDefinitionExperimentalCollectionRewrites.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphExecutionRewrites.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphDefinitionRewrites.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("#include \"TemplateMonomorphTemplateSpecialization.h\"") !=
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool isSoftwareNumericParamCompatible(ReturnKind expectedKind, ReturnKind actualKind)") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("std::string inferExprTypeTextForTemplatedVectorFallback(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool shouldPreferTemplatedVectorFallbackForTypeMismatch(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("std::string preferVectorStdlibImplicitTemplatePath(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("std::string resolveNameToPath(const std::string &name,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool inferBlockBodyBindingTypeForMonomorph(const Expr &initializer,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool inferCallBindingTypeForMonomorph(const Expr &initializer,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find(
            "ResolvedType resolveTypeStringImpl(std::string input,\n"
            "                                   const SubstMap &mapping,\n"
            "                                   const std::unordered_set<std::string> &allowedParams,\n"
            "                                   const std::string &namespacePrefix,\n"
            "                                   Context &ctx,\n"
            "                                   std::string &error,\n"
            "                                   std::unordered_set<std::string> &substitutionStack) {") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteTransforms(std::vector<Transform> &transforms,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find(
            "std::string resolveCalleePath(const Expr &expr, const std::string &namespacePrefix, const Context &ctx) {") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool inferStdlibCollectionHelperTemplateArgs(const Definition &def,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool inferCallTargetBinding(const Expr &bindingExpr,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool resolveDereferenceBindingTarget(const Expr &target,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool resolveAssignmentTargetBinding(const Expr &target,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("std::vector<ParameterInfo> buildExperimentalConstructorRewriteParams(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteExperimentalConstructorArgsForTarget(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool isExperimentalMapEntryArgument(const Expr &argExpr,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool inferExperimentalCollectionConstructorTemplateArgs(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteCanonicalExperimentalMapConstructorExpr(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteCanonicalExperimentalVectorConstructorExpr(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("auto rewriteCanonicalExperimentalVectorReturnConstructors = [&](auto &self,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("auto rewriteCanonicalExperimentalMapReturnConstructors = [&](auto &self,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("struct ExperimentalCollectionReturnRewritePlan") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("ExperimentalCollectionReturnRewritePlan inferExperimentalCollectionReturnRewritePlan(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("DefinitionReturnStatementSelection determineDefinitionReturnStatementSelection(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool tryAppendDefinitionParameterBinding(Expr &param,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteDefinitionParameters(std::vector<Expr> &parameters,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("void recordDefinitionStatementBindingLocal(Expr &stmt,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool isDefinitionReturnPathStatement(const Expr &stmt,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteDefinitionReturnConstructors(Expr &expr,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("void rewriteDefinitionExperimentalMapConstructorValue(Expr &valueExpr,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("void rewriteDefinitionExperimentalVectorConstructorValue(Expr &valueExpr,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("void rewriteDefinitionExperimentalVectorReturnConstructors(Expr &candidate,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("void rewriteDefinitionExperimentalMapReturnConstructors(Expr &candidate,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteDefinitionExperimentalReturnConstructors(Expr &expr,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteExecutionEntry(Execution &exec, Context &ctx, std::string &error)") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteDefinition(Definition &def,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("std::vector<Definition> collectTemplateSpecializationFamily(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool specializeTemplateDefinitionFamily(const std::string &basePath,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool resolveExperimentalConstructorTargetTypeText(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("void rewriteExperimentalAssignTargetValue(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("void rewriteExperimentalInitTargetValue(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool isBuiltinResultOkPayloadCall(const Expr &candidate)") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteExperimentalConstructorValueTree(Expr &candidate,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteExperimentalMapResultOkPayloadTree(Expr &candidate,") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool rewriteExperimentalConstructorBinding(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool resolvesExperimentalMapValueReceiver(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool resolvesExperimentalMapBorrowedReceiver(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool resolvesExperimentalVectorValueReceiver(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool resolveExperimentalMapValueReceiverTemplateArgs(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool resolveExperimentalVectorValueReceiverTemplateArgs(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("bool hasVisibleStdCollectionsImportForPath(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("std::string experimentalMapHelperPathForCanonicalHelper(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("std::string experimentalVectorHelperPathForCanonicalHelper(") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find("std::string experimentalMapConstructorHelperPath(size_t argumentCount)") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find(
            "std::string experimentalMapConstructorRewritePath(const std::string &resolvedPath, size_t argumentCount)") ==
        std::string::npos);
  CHECK(templateMonomorphSource.find(
            "std::string experimentalVectorConstructorRewritePath(const std::string &resolvedPath, size_t argumentCount)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "if (inferCallBindingTypeForMonomorph(initializer, params, locals, allowMathBare, ctx, infoOut,") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "std::optional<std::string> builtinSoaPendingExprDiagnosticForMonomorph(") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "builtinSoaPendingExprDiagnosticForMonomorph(*argExpr, params, locals, ctx)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "splitSoaFieldViewHelperPath(resolved, &fieldNameOut)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "splitSoaFieldViewHelperPath(resolvedPath, &fieldName)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "std::optional<std::string> soaPendingUnavailableMethodDiagnosticForMonomorph(") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaPendingUnavailableMethodDiagnostic(") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "bool hasVisibleDefinitionPathForMonomorph(const Context &ctx,") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "bool hasVisibleSoaHelperTargetForMonomorph(const Context &ctx,") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasVisibleSoaRefHelper =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const bool hasVisibleSoaRefRefHelper =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const auto canonicalizeLegacySoaResolvedPath =") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "const std::string resolvedSoaCanonical =") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, \"ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, \"ref_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "canonicalizeLegacySoaRefHelperPath(resolvedPath)") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isCanonicalSoaRefLikeHelperPath(resolvedSoaCanonical)") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "if (resolvedPath == \"/soa_vector/ref\" ||") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "if (resolvedSoaCanonical == \"/std/collections/soa_vector/ref\" ||") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "resolvedSoaCanonical == \"/std/collections/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "hasVisibleDefinitionPathForMonomorph(ctx, helperPath)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "auto hasVisibleSoaHelperTarget =") ==
        std::string::npos);
  CHECK((templateMonomorphImplicitTemplateInferenceSource.find(
            "ctx.sourceDefs.count(\"/soa_vector/ref\") > 0 ||") !=
         std::string::npos ||
         templateMonomorphImplicitTemplateInferenceSource.find(
             "hasVisibleSoaBorrowedHelper(\"ref\")") !=
             std::string::npos));
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "ctx.sourceDefs.count(ownedPath) > 0 ||") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "hasVisibleSoaHelperTarget(normalizedName)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "ctx.sourceDefs.count(ownedPath) > 0 ||\n"
            "        ctx.helperOverloads.count(ownedPath) > 0") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaPendingUnavailableMethodDiagnosticForMonomorph(\n"
            "                  ctx, resolvedPath)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaPendingUnavailableMethodDiagnostic(") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaDirectFieldViewPendingDiagnostic(normalizedName)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaDirectPendingUnavailableMethodDiagnostic(\n"
            "        soaFieldViewHelperPath(normalizedName))") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaUnavailableMethodDiagnostic(\n"
            "        soaFieldViewHelperPath(normalizedName))") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaDirectBorrowedViewPendingDiagnostic()") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaDirectPendingUnavailableMethodDiagnostic(") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "\"/std/collections/soa_vector/ref_ref\"") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "\"/std/collections/soa_vector/ref\"") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaUnavailableMethodDiagnostic(\"/soa_vector/ref\", false)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaFieldViewPendingDiagnostic(normalizedName)") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "soaBorrowedViewPendingDiagnostic()") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find("\"/soa_vector/field_view/\"") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find("hasVisibleSoaFieldHelperForMonomorph(") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isBuiltinSoaFieldViewExprForMonomorph(") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "isBuiltinSoaRefExprForMonomorph(") ==
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find("if (handledCallInference) {") !=
        std::string::npos);
  CHECK(templateMonomorphImplicitTemplateInferenceSource.find(
            "return inferBlockBodyBindingTypeForMonomorph(initializer, params, locals, allowMathBare, ctx, infoOut);") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find("bool isSoftwareNumericParamCompatible(ReturnKind expectedKind, ReturnKind actualKind)") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find("std::string resolveStructLikeExprPathForTemplatedVectorFallback(") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find("bool inferDefinitionReturnBindingForTemplatedFallback(") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find("std::string inferExprTypeTextForTemplatedVectorFallback(") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find("bool shouldPreferTemplatedVectorFallbackForTypeMismatch(") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find("std::string preferVectorStdlibImplicitTemplatePath(") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find("path == \"/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "path == \"/std/collections/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "isLegacyOrCanonicalSoaHelperPath(pathCanonical, \"get_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "auto isLegacyOrCanonicalSoaHelperPath = [](const std::string &candidate,") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "constexpr const char kLegacyPrefix[] = \"/soa_vector/\"") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "constexpr const char kCanonicalPrefix[] = \"/std/collections/soa_vector/\"") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "isCanonicalSoaRefLikeHelperPath(pathCanonical)") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "auto isCanonicalSoaRefLikePath = [](const std::string &candidate)") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "const auto canonicalizeLegacySoaRefHelperPath =") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "const std::string pathCanonical =") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "canonicalizeLegacySoaGetHelperPath(path)") !=
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find("path == \"/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find("path == \"/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "pathCanonical == \"/std/collections/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphFallbackSource.find(
            "pathCanonical == \"/std/collections/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "path == \"/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "constexpr const char *kLegacyPrefix = \"/soa_vector/\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "constexpr const char *kCanonicalPrefix = \"/std/collections/soa_vector/\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isSyntheticSamePathSoaCarryNonRefHelperPath(path)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "canonicalizeLegacySoaGetHelperPath(candidate)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isLegacyOrCanonicalSoaHelperPath(getCanonicalPath, \"get\")") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isLegacyOrCanonicalSoaHelperPath(getCanonicalPath, \"get_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "auto canonicalizeLegacySoaRefHelperPath =") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "canonicalizeLegacySoaRefHelperPath(path)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isCanonicalSoaRefLikeHelperPath(canonicalPath)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "canonicalPath == \"/std/collections/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "canonicalPath == \"/std/collections/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string resolvedPathCanonical =") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string methodPathCanonical =") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isSyntheticSamePathSoaHelperTemplateCarryPath(resolvedPath)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isSyntheticSamePathSoaHelperTemplateCarryPath(methodPath)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "resolvedReceiverFamily == \"vector\"") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "resolvedPath == \"/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "methodPath == \"/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "methodPathCanonical == \"/std/collections/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "path == \"/std/collections/soa_vector/get_ref\"") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string canonicalSoaToAosPath =") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "canonicalizeLegacySoaToAosHelperPath(samePathToAosHelper)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string samePathToAosHelper = \"/\" + helperName;") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isLegacyOrCanonicalSoaHelperPath(canonicalSoaToAosPath, \"to_aos\")") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isLegacyOrCanonicalSoaHelperPath(canonicalSoaToAosPath, \"to_aos_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isLegacyOrCanonicalSoaHelperPath(canonicalSoaToAosHelper, \"to_aos\")") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isLegacyOrCanonicalSoaHelperPath(canonicalSoaToAosHelper, \"to_aos_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isCanonicalSoaToAosHelperPath(path)") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isCanonicalSoaToAosHelperPath(canonicalSoaToAosHelper)") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "path == \"/std/collections/soa_vector/to_aos\" || path == \"/std/collections/soa_vector/to_aos_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "helperName == \"to_aos\" ? \"/to_aos\" : \"/to_aos_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "helperName == \"to_aos\" ? std::string(\"/to_aos\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "helperName == \"get\" || helperName == \"get_ref\"") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string receiverFamily = inferCollectionReceiverFamily(receiverExpr);") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string samePathGetHelper = \"/soa_vector/\" + helperName;") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "helperName == \"count\" || helperName == \"push\" || helperName == \"reserve\"") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string samePathSoaNonRefHelper = \"/soa_vector/\" + helperName;") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isLegacyOrCanonicalSoaHelperPath(samePathSoaNonRefHelper,") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "return samePathSoaNonRefHelper;") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "hasDefinitionFamilyPath(\"/soa_vector/count\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "hasDefinitionFamilyPath(\"/soa_vector/push\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "hasDefinitionFamilyPath(\"/soa_vector/reserve\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "return std::string(\"/soa_vector/count\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "return std::string(\"/soa_vector/push\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "return std::string(\"/soa_vector/reserve\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "canonicalizeLegacySoaGetHelperPath(samePathGetHelper)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isLegacyOrCanonicalSoaHelperPath(canonicalGetHelper, helperName)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "return samePathGetHelper;") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "helperName == \"get_ref\" ? \"/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "helperName == \"get_ref\" ? std::string(\"/soa_vector/get_ref\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "const std::string samePathRefHelper = \"/soa_vector/\" + helperName;") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "inferCollectionReceiverFamily(receiverExpr) == \"soa_vector\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "inferCollectionReceiverFamily(receiverExpr) == \"vector\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "canonicalizeLegacySoaRefHelperPath(samePathRefHelper)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "isCanonicalSoaRefLikeHelperPath(canonicalRefHelper)") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "return samePathRefHelper;") !=
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "hasDefinitionFamilyPath(helperName == \"ref_ref\" ? \"/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "return helperName == \"ref_ref\" ? std::string(\"/soa_vector/ref_ref\")") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "resolvedPath == \"/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphExpressionRewriteSource.find(
            "methodPath == \"/std/collections/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(templateMonomorphBindingCallSource.find("bool inferCallBindingTypeForMonomorph(const Expr &initializer,") !=
        std::string::npos);
  CHECK(templateMonomorphBindingCallSource.find("handledOut = false;") !=
        std::string::npos);
  CHECK(templateMonomorphBindingCallSource.find("inferDefinitionReturnBindingForTemplatedFallback(") !=
        std::string::npos);
  CHECK(templateMonomorphBindingBlockSource.find("bool inferBlockBodyBindingTypeForMonomorph(const Expr &initializer,") !=
        std::string::npos);
  CHECK(templateMonomorphBindingBlockSource.find("LocalTypeMap blockLocals = locals;") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find("bool resolveMethodCallTemplateTarget(const Expr &expr,") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find("std::string resolveNameToPath(const std::string &name,") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find("inferDefinitionReturnBindingForTemplatedFallback(") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "canonicalizeLegacySoaToAosHelperPath(samePath)") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "const std::string samePath = \"/\" + std::string(helperName);") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "isLegacyOrCanonicalSoaHelperPath(canonicalPath, \"to_aos\")") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "isLegacyOrCanonicalSoaHelperPath(canonicalPath, \"to_aos_ref\")") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "(matchesSoaToAosHelperPath || matchesBorrowedSoaToAosHelperPath) &&") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "if (hasDefinitionFamilyPath(samePath)) {\n"
            "      return samePath;\n"
            "    }\n"
            "    return canonicalPath;") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "helperName == \"to_aos\" ? std::string(\"/to_aos\")") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "normalizedTypeName == \"soa_vector\" &&\n"
            "      (normalizedMethodName == \"to_aos\" || normalizedMethodName == \"to_aos_ref\")") !=
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "normalizedTypeName == \"soa_vector\" && normalizedMethodName == \"to_aos\"") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "isCanonicalSoaToAosHelperPath(canonicalPath)") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "const std::string canonical = \"/std/collections/soa_vector/to_aos\"") ==
        std::string::npos);
  CHECK(templateMonomorphMethodTargetsSource.find(
            "preferredSoaToAosMethodTarget()") ==
        std::string::npos);
  CHECK(templateMonomorphTypeResolutionSource.find("ResolvedType resolveTypeStringImpl(std::string input,") !=
        std::string::npos);
  CHECK(templateMonomorphTypeResolutionSource.find("bool rewriteTransforms(std::vector<Transform> &transforms,") !=
        std::string::npos);
  CHECK(templateMonomorphTypeResolutionSource.find(
            "std::string resolveCalleePath(const Expr &expr, const std::string &namespacePrefix, const Context &ctx)") !=
        std::string::npos);
  CHECK(templateMonomorphAssignmentTargetResolutionSource.find(
            "bool inferCallTargetBinding(const Expr &bindingExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphAssignmentTargetResolutionSource.find(
            "bool resolveAssignmentTargetBinding(const Expr &target,") !=
        std::string::npos);
  CHECK(templateMonomorphAssignmentTargetResolutionSource.find(
            "bool resolveDereferenceBindingTarget(const Expr &target,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionArgumentRewritesSource.find(
            "std::vector<ParameterInfo> buildExperimentalConstructorRewriteParams(const Definition &targetDef,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionArgumentRewritesSource.find(
            "bool rewriteExperimentalConstructorArgsForTarget(Expr &callExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionConstructorRewritesSource.find(
            "bool isExperimentalMapEntryArgument(const Expr &argExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionConstructorRewritesSource.find(
            "bool inferExperimentalCollectionConstructorTemplateArgs(const std::string &originalPath,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionConstructorRewritesSource.find(
            "bool rewriteCanonicalExperimentalMapConstructorExpr(Expr &valueExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionConstructorRewritesSource.find(
            "bool rewriteCanonicalExperimentalVectorConstructorExpr(Expr &valueExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionTargetValueRewritesSource.find(
            "bool resolveExperimentalConstructorTargetTypeText(const Expr &targetExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionTargetValueRewritesSource.find(
            "void rewriteExperimentalAssignTargetValue(Expr &callExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionTargetValueRewritesSource.find(
            "void rewriteExperimentalInitTargetValue(Expr &callExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionValueRewritesSource.find(
            "bool isBuiltinResultOkPayloadCall(const Expr &candidate)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionValueRewritesSource.find(
            "bool rewriteExperimentalConstructorValueTree(Expr &candidate,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionValueRewritesSource.find(
            "bool rewriteExperimentalMapResultOkPayloadTree(Expr &candidate,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionValueRewritesSource.find(
            "bool rewriteExperimentalConstructorBinding(Expr &bindingExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "bool resolvesExperimentalMapValueReceiver(const Expr *receiverExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "bool resolveExperimentalMapValueReceiverTemplateArgs(const Expr *receiverExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "bool resolveExperimentalVectorValueReceiverTemplateArgs(const Expr *receiverExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "std::string experimentalVectorHelperPathForCanonicalHelper(const std::string &path)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "std::string experimentalSoaVectorHelperPathForCanonicalHelper(const std::string &path)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "matchesPath(\"/std/collections/soa_vector/to_aos_ref\")") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReceiverResolutionSource.find(
            "soaVectorToAosRef") ==
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionConstructorPathsSource.find(
            "std::string experimentalMapConstructorHelperPath(size_t argumentCount)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionConstructorPathsSource.find(
            "std::string experimentalMapConstructorRewritePath(const std::string &resolvedPath, size_t argumentCount)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionConstructorPathsSource.find(
            "std::string experimentalVectorConstructorRewritePath(const std::string &resolvedPath, size_t argumentCount)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReturnRewritesSource.find(
            "void rewriteExperimentalConstructorReturnTree(Expr &candidate, RewriteCurrentFn &&rewriteCurrent)") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReturnSetupSource.find(
            "struct ExperimentalCollectionReturnRewritePlan") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReturnSetupSource.find(
            "ExperimentalCollectionReturnRewritePlan inferExperimentalCollectionReturnRewritePlan(") !=
        std::string::npos);
  CHECK(templateMonomorphExperimentalCollectionReturnSetupSource.find(
            "DefinitionReturnStatementSelection determineDefinitionReturnStatementSelection(const Definition &def)") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionBindingSetupSource.find(
            "bool tryAppendDefinitionParameterBinding(Expr &param,") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionBindingSetupSource.find(
            "bool rewriteDefinitionParameters(std::vector<Expr> &parameters,") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionBindingSetupSource.find(
            "void recordDefinitionStatementBindingLocal(Expr &stmt,") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionReturnOrchestrationSource.find(
            "bool isDefinitionReturnPathStatement(const Expr &stmt,") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionReturnOrchestrationSource.find(
            "bool rewriteDefinitionReturnConstructors(Expr &expr,") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionExperimentalCollectionRewritesSource.find(
            "void rewriteDefinitionExperimentalMapConstructorValue(Expr &valueExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionExperimentalCollectionRewritesSource.find(
            "void rewriteDefinitionExperimentalVectorConstructorValue(Expr &valueExpr,") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionExperimentalCollectionRewritesSource.find(
            "void rewriteDefinitionExperimentalVectorReturnConstructors(Expr &candidate,") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionExperimentalCollectionRewritesSource.find(
            "void rewriteDefinitionExperimentalMapReturnConstructors(Expr &candidate,") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionExperimentalCollectionRewritesSource.find(
            "bool rewriteDefinitionExperimentalReturnConstructors(Expr &expr,") !=
        std::string::npos);
  CHECK(templateMonomorphExecutionRewritesSource.find(
            "bool rewriteExecutionEntry(Execution &exec, Context &ctx, std::string &error)") !=
        std::string::npos);
  CHECK(templateMonomorphDefinitionRewritesSource.find(
            "bool rewriteDefinition(Definition &def,") !=
        std::string::npos);
  CHECK(templateMonomorphTemplateSpecializationSource.find(
            "std::vector<Definition> collectTemplateSpecializationFamily(const std::string &basePath,") !=
        std::string::npos);
  CHECK(templateMonomorphTemplateSpecializationSource.find(
            "bool specializeTemplateDefinitionFamily(const std::string &basePath,") !=
        std::string::npos);
}
