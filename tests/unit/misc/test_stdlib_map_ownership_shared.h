#pragma once

#include "third_party/doctest.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

inline std::filesystem::path repoRoot() {
  const auto cwd = std::filesystem::current_path();
  const auto marker = std::filesystem::path("stdlib") / "std" / "collections" / "map.prime";
  if (std::filesystem::exists(cwd / marker)) {
    return cwd;
  }
  if (std::filesystem::exists(cwd.parent_path() / marker)) {
    return cwd.parent_path();
  }
  return cwd;
}

inline std::string readText(const std::filesystem::path &path) {
  std::ifstream input(path);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

inline std::filesystem::path collectionsFile(const std::string &name) {
  return repoRoot() / "stdlib" / "std" / "collections" / name;
}

inline std::string relativePathString(const std::filesystem::path &path) {
  return std::filesystem::relative(path, repoRoot()).generic_string();
}

inline bool isSourceFile(const std::filesystem::path &path) {
  const std::string extension = path.extension().string();
  return extension == ".cpp" || extension == ".h" || extension == ".hpp";
}

inline std::vector<std::filesystem::path> productionMapTraceFiles() {
  std::vector<std::filesystem::path> files;
  for (const auto &root :
       {repoRoot() / "src" / "ir_lowerer", repoRoot() / "src" / "emitter"}) {
    for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
      if (entry.is_regular_file() && isSourceFile(entry.path())) {
        files.push_back(entry.path());
      }
    }
  }
  std::sort(files.begin(), files.end());
  return files;
}

inline bool isAllowedMapBackingFile(const std::string &relativePath) {
  static const std::vector<std::string> files = {
      "src/ir_lowerer/IrLowererAccessTargetResolution.cpp",
      "src/ir_lowerer/IrLowererInlinePackedArgs.cpp",
      "src/ir_lowerer/IrLowererInlineParamHelpers.cpp",
      "src/ir_lowerer/IrLowererLowerEmitExprCollectionHelpers.cpp",
      "src/ir_lowerer/IrLowererLowerEmitExprTailDispatch.h",
      "src/ir_lowerer/IrLowererLowerInferenceDispatchSetup.cpp",
      "src/ir_lowerer/IrLowererSetupTypeDeclaredCollectionInference.cpp",
      "src/ir_lowerer/IrLowererSetupTypeMethodTargetHelpers.cpp",
      "src/ir_lowerer/IrLowererUninitializedStructInference.cpp",
  };
  return std::find(files.begin(), files.end(), relativePath) != files.end();
}

inline bool isAllowedExperimentalMapTrace(const std::string &relativePath,
                                   const std::string &line) {
  auto contains = [&](const std::string &needle) {
    return line.find(needle) != std::string::npos;
  };

  if (contains("std/collections/experimental_map/Map")) {
    return isAllowedMapBackingFile(relativePath);
  }
  if (contains("std/collections/experimental_map/Entry")) {
    return relativePath ==
           "src/ir_lowerer/IrLowererLowerEmitExprCollectionHelpers.cpp";
  }
  if (relativePath == "src/ir_lowerer/IrLowererLowerEmitExprCollectionHelpers.cpp" &&
      contains("receiverDef->fullPath.rfind(\"/std/collections/experimental_map/\", 0) == 0")) {
    return true;
  }
  return false;
}

} // namespace

struct MapOwnershipSources {
  std::string mapSource;
  std::string registrySource;
  std::string publicationBuildersSource;
  std::string semanticsSource;
  std::string semanticBindingTypeHelpersSource;
  std::string validatorSource;
  std::string callResolutionSource;
  std::string callPathHelpersSource;
  std::string builtinPathHelpersSource;
  std::string methodTargetResolutionSource;
  std::string receiverPathsSource;
  std::string exprMethodResolutionSource;
  std::string privateExprValidationSource;
  std::string privateExprInferenceSource;
  std::string privateCoreSource;
  std::string exprCollectionDispatchSetupSource;
  std::string exprDispatchBootstrapSource;
  std::string exprCollectionPredicatesSource;
  std::string exprCollectionCountCapacitySource;
  std::string templateCoreSource;
  std::string templateReceiverSource;
  std::string templateExpressionRewriteSource;
  std::string templateFallbackTypeInferenceSource;
  std::string templateReturnSetupSource;
  std::string templateReturnOrchestrationSource;
  std::string templateDefinitionRewriteSource;
  std::string templateTypeResolutionSource;
  std::string templateCollectionCompatibilitySource;
  std::string templateBindingCallInferenceSource;
  std::string templateConstructorRewriteSource;
  std::string templateValueRewriteSource;
  std::string collectionSurfaceHelpersSource;
  std::string inferStructReturnSource;
  std::string inferStructReturnHelpersSource;
  std::string inferMethodResolutionSource;
  std::string inferMethodResolutionHelpersSource;
  std::string inferPreDispatchCallsSource;
  std::string exprPreDispatchDirectCallsSource;
  std::string exprVectorHelpersSource;
  std::string collectionHelperRewritesSource;
  std::string effectFreeCollectionsSource;
  std::string passesEffectFreeSource;
  std::string buildParametersSource;
  std::string buildCallResolutionSource;
  std::string buildReturnKindsSource;
  std::string buildInitializerInferenceSource;
  std::string buildInitializerInferenceCallsSource;
  std::string inferCollectionDispatchSource;
  std::string inferCollectionCallResolutionSource;
  std::string inferCollectionCompatibilitySource;
  std::string inferCollectionCompatibilityInternalSource;
  std::string inferCollectionDispatchSetupSource;
  std::string inferCollectionsSource;
  std::string inferCollectionReturnInferenceSource;
  std::string inferCollectionBufferAndMapResolversSource;
  std::string inferTargetResolutionSource;
  std::string inferDefinitionSource;
  std::string inferLateFallbackBuiltinsSource;
  std::string exprLateFallbackBuiltinsSource;
  std::string exprLateCallCompatibilitySource;
  std::string lateMapAccessBuiltinsSource;
  std::string exprLateUnknownTargetFallbacksSource;
  std::string passesDiagnosticsSource;
  std::string exprTrySource;
  std::string pointerLikeSource;
  std::string statementPrintabilitySource;
  std::string statementBodyArgumentsSource;
  std::string exprBodyArgumentsSource;
  std::string scalarPointerMemorySource;
  std::string statementSource;
  std::string statementContainerHelpersSource;
  std::string buildDirectCallBindingSource;
  std::string argumentValidationSource;
  std::string collectionLiteralValidationSource;
  std::string resolvedCallArgumentsSource;
  std::string argumentValidationCollectionsSource;
  std::string namedArgumentBuiltinsSource;
  std::string collectionAccessValidationSource;
  std::string collectionAccessSource;
  std::string exprSource;
  std::string builtinContextSetupSource;
  std::string collectionAccessSetupSource;
  std::string lateCollectionAccessFallbacksSource;
  std::string collectionDispatchSetupSource;
  std::string mapSoaBuiltinsSource;
  std::string lateMapSoaBuiltinsSource;
  std::string countCapacityMapBuiltinSource;
  std::string emitterCallPathHelpersSource;
  std::string emitterReturnInferenceCollectionsSource;
  std::string emitterCollectionTypeHelpersSource;
  std::string emitterPackedArgsSource;
  std::string emitterHelpersSource;
  std::string emitterMethodMetadataSource;
  std::string statementLowererSource;
  std::string operatorCollectionMutationSource;
  std::string lowererCallHelpersSource;
  std::string textFilterPipelinePassSource;
  std::string lowerStatementsExprSource;
  std::string lowerStatementsBindingsSource;
  std::string inlineNativeSource;
  std::string emitterMethodResolutionSource;
  std::string emitterMethodTypeInferenceSource;
  std::string emitterHelpersTypesSource;
  std::string nativeTailSource;
  std::string tailDispatchSource;
  std::string lowerEmitExprCollectionSource;
  std::string builtinNameHelpersSource;
  std::string lowererHelpersSource;
  std::string inlinePackedArgsSource;
  std::string inlineParamHelpersSource;
  std::string packedResultSource;
  std::string uninitializedStructInferenceSource;
  std::string structSlotLayoutSource;
  std::string lowererStructTypeHelpersSource;
  std::string declaredCollectionInferenceSource;
  std::string bindingTypeHelpersSource;
  std::string inferenceDispatchSetupSource;
  std::string inferenceBaseKindSource;
  std::string setupTypeMethodTargetSource;
  std::string setupTypeMethodCallSource;
  std::string setupTypeReceiverTargetSource;
  std::string setupTypeReturnKindSource;
  std::string lowererStructReturnPathSource;
  std::string setupTypeCollectionSource;
  std::string countAccessSource;
  std::string resultMetadataSource;
  std::string semanticsResultHelpersSource;
  std::string statementReturnsSource;
  std::string inlineCallContextSource;
  std::string accessTargetResolutionSource;
};

inline MapOwnershipSources loadMapOwnershipSources() {
  MapOwnershipSources result;
  const std::string mapSource = readText(collectionsFile("map.prime"));
  const std::string registrySource = readText(repoRoot() / "src" / "support" / "StdlibSurfaceRegistry.cpp");
  const std::string publicationBuildersSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticPublicationBuilders.cpp");
  const std::string semanticsSource = readText(repoRoot() / "src" / "semantics" / "SemanticsValidate.cpp");
  const std::string semanticBindingTypeHelpersSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsBindingTypeHelpers.cpp");
  const std::string validatorSource =
      readText(repoRoot() / "src" / "semantics" / "SemanticsValidator.cpp");
  const std::string callResolutionSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprCallResolution.cpp");
  const std::string callPathHelpersSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsCallPathHelpers.cpp");
  const std::string builtinPathHelpersSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsBuiltinPathHelpers.cpp");
  const std::string methodTargetResolutionSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprMethodTargetResolution.cpp");
  const std::string receiverPathsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprReceiverPaths.cpp");
  const std::string exprMethodResolutionSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprMethodResolution.cpp");
  const std::string privateExprValidationSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorPrivateExprValidation.h");
  const std::string privateExprInferenceSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorPrivateExprInference.h");
  const std::string privateCoreSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorPrivateCore.h");
  const std::string exprCollectionDispatchSetupSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprCollectionDispatchSetup.cpp");
  const std::string exprDispatchBootstrapSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprDispatchBootstrap.cpp");
  const std::string exprCollectionPredicatesSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprCollectionPredicates.cpp");
  const std::string exprCollectionCountCapacitySource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprCollectionCountCapacity.cpp");
  const std::string templateCoreSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphCoreUtilities.h") +
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphCoreUtilities.cpp");
  const std::string templateReceiverSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphExperimentalCollectionReceiverResolution.h") +
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphExperimentalCollectionReceiverResolution.cpp");
  const std::string templateExpressionRewriteSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphExpressionRewrite.h") +
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphExpressionRewrite.cpp");
  const std::string templateFallbackTypeInferenceSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphFallbackTypeInference.h") +
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphFallbackTypeInference.cpp");
  const std::string templateReturnSetupSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphExperimentalCollectionReturnSetup.h") +
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphExperimentalCollectionReturnSetup.cpp");
  const std::string templateReturnOrchestrationSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphDefinitionReturnOrchestration.h") +
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphDefinitionReturnOrchestration.cpp");
  const std::string templateDefinitionRewriteSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphDefinitionExperimentalCollectionRewrites.h") +
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphDefinitionExperimentalCollectionRewrites.cpp");
  const std::string templateTypeResolutionSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphTypeResolution.h") +
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphTypeResolution.cpp");
  const std::string templateCollectionCompatibilitySource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphCollectionCompatibilityPaths.h") +
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphCollectionCompatibilityPaths.cpp");
  const std::string templateBindingCallInferenceSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphBindingCallInference.h") +
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphBindingCallInference.cpp");
  const std::string templateConstructorRewriteSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphExperimentalCollectionConstructorRewrites.h") +
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphExperimentalCollectionConstructorRewrites.cpp");
  const std::string templateValueRewriteSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphExperimentalCollectionValueRewrites.h") +
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphExperimentalCollectionValueRewrites.cpp");
  const std::filesystem::path oldMapConstructorHelpersPath =
      repoRoot() / "src" / "semantics" / "MapConstructorHelpers.h";
  const std::filesystem::path collectionSurfaceHelpersPath =
      repoRoot() / "src" / "semantics" / "StdlibCollectionSurfaceHelpers.h";
  CHECK_FALSE(std::filesystem::exists(oldMapConstructorHelpersPath));
  REQUIRE(std::filesystem::exists(collectionSurfaceHelpersPath));
  const std::string collectionSurfaceHelpersSource =
      readText(collectionSurfaceHelpersPath);
  const std::string inferStructReturnSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferStructReturn.cpp");
  const std::string inferStructReturnHelpersSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferStructReturnHelpers.cpp");
  const std::string inferMethodResolutionSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferMethodResolution.cpp");
  const std::string inferMethodResolutionHelpersSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferMethodResolutionHelpers.cpp");
  const std::string inferPreDispatchCallsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferPreDispatchCalls.cpp");
  const std::string exprPreDispatchDirectCallsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprPreDispatchDirectCalls.cpp");
  const std::string exprVectorHelpersSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprVectorHelpers.cpp");
  const std::string collectionHelperRewritesSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorCollectionHelperRewrites.cpp");
  const std::string effectFreeCollectionsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorEffectFreeCollections.cpp");
  const std::string passesEffectFreeSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorPassesEffectFree.cpp");
  const std::string buildParametersSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorBuildParameters.cpp");
  const std::string buildCallResolutionSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorBuildCallResolution.cpp");
  const std::string buildReturnKindsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorBuildReturnKinds.cpp");
  const std::string buildInitializerInferenceSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorBuildInitializerInference.cpp");
  const std::string buildInitializerInferenceCallsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorBuildInitializerInferenceCalls.cpp");
  const std::string inferCollectionDispatchSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferCollectionDispatch.cpp");
  const std::string inferCollectionCallResolutionSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferCollectionCallResolution.cpp");
  const std::string inferCollectionCompatibilitySource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferCollectionCompatibility.cpp");
  const std::string inferCollectionCompatibilityInternalSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferCollectionCompatibilityInternal.h");
  const std::string inferCollectionDispatchSetupSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferCollectionDispatchSetup.cpp");
  const std::string inferCollectionsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferCollections.cpp");
  const std::string inferCollectionReturnInferenceSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferCollectionReturnInference.cpp");
  const std::string inferCollectionBufferAndMapResolversSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferCollectionBufferAndMapResolvers.cpp");
  const std::string inferTargetResolutionSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferTargetResolution.cpp");
  const std::string inferDefinitionSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferDefinition.cpp");
  const std::string inferLateFallbackBuiltinsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferLateFallbackBuiltins.cpp");
  const std::string exprLateFallbackBuiltinsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprLateFallbackBuiltins.cpp");
  const std::string exprLateCallCompatibilitySource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprLateCallCompatibility.cpp");
  const std::string lateMapAccessBuiltinsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprLateMapAccessBuiltins.cpp");
  const std::string exprLateUnknownTargetFallbacksSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprLateUnknownTargetFallbacks.cpp");
  const std::string passesDiagnosticsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorPassesDiagnostics.cpp");
  const std::string exprTrySource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprTry.cpp");
  const std::string pointerLikeSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprPointerLike.cpp");
  const std::string statementPrintabilitySource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorStatementPrintability.cpp");
  const std::string statementBodyArgumentsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorStatementBodyArguments.cpp");
  const std::string exprBodyArgumentsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprBodyArguments.cpp");
  const std::string scalarPointerMemorySource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprScalarPointerMemory.cpp");
  const std::string statementSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorStatement.cpp");
  const std::string statementContainerHelpersSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorStatementContainerHelpers.cpp");
  const std::string buildDirectCallBindingSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorBuildDirectCallBinding.cpp");
  const std::string argumentValidationSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprArgumentValidation.cpp");
  const std::string collectionLiteralValidationSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprCollectionLiterals.cpp");
  const std::string resolvedCallArgumentsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprResolvedCallArguments.cpp");
  const std::string argumentValidationCollectionsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprArgumentValidationCollections.cpp");
  const std::string namedArgumentBuiltinsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprNamedArgumentBuiltins.cpp");
  const std::string collectionAccessValidationSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprCollectionAccessValidation.cpp");
  const std::string collectionAccessSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprCollectionAccess.cpp");
  const std::string exprSource =
      readText(repoRoot() / "src" / "semantics" / "SemanticsValidatorExpr.cpp");
  const std::string builtinContextSetupSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprBuiltinContextSetup.cpp");
  const std::string collectionAccessSetupSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprCollectionAccessSetup.cpp");
  const std::string lateCollectionAccessFallbacksSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprLateCollectionAccessFallbacks.cpp");
  const std::string collectionDispatchSetupSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprCollectionDispatchSetup.cpp");
  const std::string mapSoaBuiltinsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprMapSoaBuiltins.cpp");
  const std::string lateMapSoaBuiltinsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprLateMapSoaBuiltins.cpp");
  const std::string countCapacityMapBuiltinSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprCountCapacityBuiltins.cpp");
  const std::string emitterCallPathHelpersSource =
      readText(repoRoot() / "src" / "emitter" /
               "EmitterBuiltinCallPathHelpers.cpp");
  const std::string emitterReturnInferenceCollectionsSource =
      readText(repoRoot() / "src" / "emitter" /
               "EmitterEmitSetupReturnInferenceCollections.h");
  const std::string emitterCollectionTypeHelpersSource =
      readText(repoRoot() / "src" / "emitter" /
               "EmitterExprCollectionTypeHelpers.h");
  const std::string emitterPackedArgsSource =
      readText(repoRoot() / "src" / "emitter" / "EmitterExprPackedArgs.h");
  const std::string emitterHelpersSource =
      readText(repoRoot() / "src" / "emitter" / "EmitterHelpers.h");
  const std::string emitterMethodMetadataSource =
      readText(repoRoot() / "src" / "emitter" /
               "EmitterBuiltinMethodResolutionMetadataHelpers.cpp");
  const std::string statementLowererSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererStatementCallEmission.cpp");
  const std::string operatorCollectionMutationSource =
      readText(repoRoot() / "src" / "ir_lowerer" /
               "IrLowererOperatorCollectionMutationHelpers.cpp");
  const std::string lowererCallHelpersSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererCallHelpers.cpp");
  const std::string textFilterPipelinePassSource =
      readText(repoRoot() / "src" / "text_filter" /
               "TextFilterPipelinePass.cpp");
  const std::string lowerStatementsExprSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererLowerStatementsExpr.h") +
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererLowerStatementsExprHelpers.cpp");
  const std::string lowerStatementsBindingsSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererLowerStatementsBindings.h");
  const std::string inlineNativeSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererInlineNativeCallDispatch.cpp");
  const std::string emitterMethodResolutionSource =
      readText(repoRoot() / "src" / "emitter" /
               "EmitterBuiltinMethodResolutionHelpers.cpp");
  const std::string emitterMethodTypeInferenceSource =
      readText(repoRoot() / "src" / "emitter" /
               "EmitterBuiltinMethodResolutionTypeInferenceHelpers.cpp");
  const std::string emitterHelpersTypesSource =
      readText(repoRoot() / "src" / "emitter" / "EmitterHelpersTypes.cpp");
  const std::string nativeTailSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererNativeTailDispatch.cpp");
  const std::string tailDispatchSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTailDispatch.h") +
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTailDispatchHelpers.cpp");
  const std::string lowerEmitExprCollectionSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererLowerEmitExprCollectionHelpers.cpp");
  const std::string builtinNameHelpersSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererBuiltinNameHelpers.cpp");
  const std::string lowererHelpersSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererHelpers.cpp");
  const std::string inlinePackedArgsSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererInlinePackedArgs.cpp");
  const std::string inlineParamHelpersSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererInlineParamHelpers.cpp");
  const std::string packedResultSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererPackedResultHelpers.cpp");
  const std::string uninitializedStructInferenceSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererUninitializedStructInference.cpp");
  const std::string structSlotLayoutSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererStructSlotLayoutHelpers.cpp");
  const std::string lowererStructTypeHelpersSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererStructTypeHelpers.cpp");
  const std::string declaredCollectionInferenceSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererSetupTypeDeclaredCollectionInference.cpp");
  const std::string bindingTypeHelpersSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererBindingTypeHelpers.cpp");
  const std::string inferenceDispatchSetupSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererLowerInferenceDispatchSetup.cpp");
  const std::string inferenceBaseKindSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererLowerInferenceBaseKindHelpers.cpp");
  const std::string setupTypeMethodTargetSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererSetupTypeMethodTargetHelpers.cpp");
  const std::string setupTypeMethodCallSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererSetupTypeMethodCallResolution.cpp");
  const std::string setupTypeReceiverTargetSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererSetupTypeReceiverTargetHelpers.cpp");
  const std::string setupTypeReturnKindSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererSetupTypeReturnKindHelpers.cpp");
  const std::string lowererStructReturnPathSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererStructReturnPathHelpers.cpp");
  const std::string setupTypeCollectionSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererSetupTypeCollectionHelpers.cpp");
  const std::string countAccessSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererCountAccessHelpers.cpp");
  const std::string resultMetadataSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererResultMetadataHelpers.cpp");
  const std::string semanticsResultHelpersSource =
      readText(repoRoot() / "src" / "semantics" / "SemanticsValidatorResultHelpers.cpp");
  const std::string statementReturnsSource =
      readText(repoRoot() / "src" / "semantics" / "SemanticsValidatorStatementReturns.cpp");
  const std::string inlineCallContextSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererInlineCallContextHelpers.cpp");
  const std::string accessTargetResolutionSource =
      readText(repoRoot() / "src" / "ir_lowerer" /
               "IrLowererAccessTargetResolution.cpp");


  REQUIRE(!mapSource.empty());
  CHECK_FALSE(std::filesystem::exists(collectionsFile("map2.prime")));
  CHECK_FALSE(std::filesystem::exists(collectionsFile("experimental_map.prime")));
  CHECK_FALSE(std::filesystem::exists(collectionsFile("internal_map.prime")));
  REQUIRE(!registrySource.empty());
  REQUIRE(!publicationBuildersSource.empty());
  REQUIRE(!semanticsSource.empty());
  REQUIRE(!validatorSource.empty());
  REQUIRE(!callResolutionSource.empty());
  REQUIRE(!callPathHelpersSource.empty());
  REQUIRE(!methodTargetResolutionSource.empty());
  REQUIRE(!receiverPathsSource.empty());
  REQUIRE(!privateCoreSource.empty());
  REQUIRE(!templateCoreSource.empty());
  REQUIRE(!templateReceiverSource.empty());
  REQUIRE(!templateExpressionRewriteSource.empty());
  REQUIRE(!templateValueRewriteSource.empty());
  REQUIRE(!templateFallbackTypeInferenceSource.empty());
  REQUIRE(!templateReturnSetupSource.empty());
  REQUIRE(!templateReturnOrchestrationSource.empty());
  REQUIRE(!templateDefinitionRewriteSource.empty());
  REQUIRE(!templateTypeResolutionSource.empty());
  REQUIRE(!templateBindingCallInferenceSource.empty());
  REQUIRE(!templateConstructorRewriteSource.empty());
  REQUIRE(!collectionSurfaceHelpersSource.empty());
  REQUIRE(!semanticBindingTypeHelpersSource.empty());
  REQUIRE(!inferStructReturnSource.empty());
  REQUIRE(!inferStructReturnHelpersSource.empty());
  REQUIRE(!inferMethodResolutionSource.empty());
  REQUIRE(!inferMethodResolutionHelpersSource.empty());
  REQUIRE(!inferPreDispatchCallsSource.empty());
  REQUIRE(!exprPreDispatchDirectCallsSource.empty());
  REQUIRE(!exprVectorHelpersSource.empty());
  REQUIRE(!exprDispatchBootstrapSource.empty());
  REQUIRE(!collectionHelperRewritesSource.empty());
  REQUIRE(!effectFreeCollectionsSource.empty());
  REQUIRE(!exprCollectionPredicatesSource.empty());
  REQUIRE(!passesEffectFreeSource.empty());
  REQUIRE(!buildParametersSource.empty());
  REQUIRE(!buildCallResolutionSource.empty());
  REQUIRE(!buildReturnKindsSource.empty());
  REQUIRE(!buildInitializerInferenceSource.empty());
  REQUIRE(!inferCollectionDispatchSource.empty());
  REQUIRE(!inferCollectionCallResolutionSource.empty());
  REQUIRE(!inferCollectionCompatibilitySource.empty());
  REQUIRE(!inferCollectionDispatchSetupSource.empty());
  REQUIRE(!inferCollectionReturnInferenceSource.empty());
  REQUIRE(!inferCollectionBufferAndMapResolversSource.empty());
  REQUIRE(!inferTargetResolutionSource.empty());
  REQUIRE(!inferDefinitionSource.empty());
  REQUIRE(!inferLateFallbackBuiltinsSource.empty());
  REQUIRE(!exprLateFallbackBuiltinsSource.empty());
  REQUIRE(!exprLateCallCompatibilitySource.empty());
  REQUIRE(lateMapAccessBuiltinsSource.empty());
  REQUIRE(!lateMapSoaBuiltinsSource.empty());
  REQUIRE(!exprLateUnknownTargetFallbacksSource.empty());
  REQUIRE(!passesDiagnosticsSource.empty());
  REQUIRE(!exprTrySource.empty());
  REQUIRE(!pointerLikeSource.empty());
  REQUIRE(!statementPrintabilitySource.empty());
  REQUIRE(!statementBodyArgumentsSource.empty());
  REQUIRE(!exprBodyArgumentsSource.empty());
  REQUIRE(!scalarPointerMemorySource.empty());
  REQUIRE(!statementSource.empty());
  REQUIRE(!statementContainerHelpersSource.empty());
  REQUIRE(!buildDirectCallBindingSource.empty());
  REQUIRE(!argumentValidationSource.empty());
  REQUIRE(!collectionLiteralValidationSource.empty());
  REQUIRE(!resolvedCallArgumentsSource.empty());
  REQUIRE(!argumentValidationCollectionsSource.empty());
  REQUIRE(!namedArgumentBuiltinsSource.empty());
  REQUIRE(!collectionAccessValidationSource.empty());
  REQUIRE(!collectionAccessSource.empty());
  REQUIRE(!countCapacityMapBuiltinSource.empty());
  REQUIRE(!emitterCallPathHelpersSource.empty());
  REQUIRE(!emitterReturnInferenceCollectionsSource.empty());
  REQUIRE(!emitterCollectionTypeHelpersSource.empty());
  REQUIRE(!emitterPackedArgsSource.empty());
  REQUIRE(!emitterMethodMetadataSource.empty());
  REQUIRE(!statementLowererSource.empty());
  REQUIRE(!operatorCollectionMutationSource.empty());
  REQUIRE(!lowererCallHelpersSource.empty());
  REQUIRE(!textFilterPipelinePassSource.empty());
  REQUIRE(!lowerStatementsExprSource.empty());
  REQUIRE(!lowerStatementsBindingsSource.empty());
  REQUIRE(!inlineNativeSource.empty());
  REQUIRE(!emitterMethodResolutionSource.empty());
  REQUIRE(!emitterMethodTypeInferenceSource.empty());
  REQUIRE(!emitterHelpersTypesSource.empty());
  REQUIRE(!nativeTailSource.empty());
  REQUIRE(!tailDispatchSource.empty());
  REQUIRE(!lowerEmitExprCollectionSource.empty());
  REQUIRE(!builtinNameHelpersSource.empty());
  REQUIRE(!lowererHelpersSource.empty());
  REQUIRE(!inlinePackedArgsSource.empty());
  REQUIRE(!inlineParamHelpersSource.empty());
  REQUIRE(!packedResultSource.empty());
  REQUIRE(!uninitializedStructInferenceSource.empty());
  REQUIRE(!structSlotLayoutSource.empty());
  REQUIRE(!lowererStructTypeHelpersSource.empty());
  REQUIRE(!declaredCollectionInferenceSource.empty());
  REQUIRE(!bindingTypeHelpersSource.empty());
  REQUIRE(!inferenceDispatchSetupSource.empty());
  REQUIRE(!inferenceBaseKindSource.empty());
  REQUIRE(!setupTypeMethodTargetSource.empty());
  REQUIRE(!setupTypeMethodCallSource.empty());
  REQUIRE(!setupTypeReceiverTargetSource.empty());
  REQUIRE(!setupTypeReturnKindSource.empty());
  REQUIRE(!lowererStructReturnPathSource.empty());
  REQUIRE(!setupTypeCollectionSource.empty());
  REQUIRE(!countAccessSource.empty());
  REQUIRE(!resultMetadataSource.empty());
  REQUIRE(!semanticsResultHelpersSource.empty());
  REQUIRE(!statementReturnsSource.empty());
  REQUIRE(!inlineCallContextSource.empty());
  REQUIRE(!accessTargetResolutionSource.empty());

  result.mapSource = mapSource;
  result.registrySource = registrySource;
  result.publicationBuildersSource = publicationBuildersSource;
  result.semanticsSource = semanticsSource;
  result.semanticBindingTypeHelpersSource = semanticBindingTypeHelpersSource;
  result.validatorSource = validatorSource;
  result.callResolutionSource = callResolutionSource;
  result.callPathHelpersSource = callPathHelpersSource;
  result.builtinPathHelpersSource = builtinPathHelpersSource;
  result.methodTargetResolutionSource = methodTargetResolutionSource;
  result.receiverPathsSource = receiverPathsSource;
  result.exprMethodResolutionSource = exprMethodResolutionSource;
  result.privateExprValidationSource = privateExprValidationSource;
  result.privateExprInferenceSource = privateExprInferenceSource;
  result.privateCoreSource = privateCoreSource;
  result.exprCollectionDispatchSetupSource = exprCollectionDispatchSetupSource;
  result.exprDispatchBootstrapSource = exprDispatchBootstrapSource;
  result.exprCollectionPredicatesSource = exprCollectionPredicatesSource;
  result.exprCollectionCountCapacitySource = exprCollectionCountCapacitySource;
  result.templateCoreSource = templateCoreSource;
  result.templateReceiverSource = templateReceiverSource;
  result.templateExpressionRewriteSource = templateExpressionRewriteSource;
  result.templateFallbackTypeInferenceSource = templateFallbackTypeInferenceSource;
  result.templateReturnSetupSource = templateReturnSetupSource;
  result.templateReturnOrchestrationSource = templateReturnOrchestrationSource;
  result.templateDefinitionRewriteSource = templateDefinitionRewriteSource;
  result.templateTypeResolutionSource = templateTypeResolutionSource;
  result.templateCollectionCompatibilitySource = templateCollectionCompatibilitySource;
  result.templateBindingCallInferenceSource = templateBindingCallInferenceSource;
  result.templateConstructorRewriteSource = templateConstructorRewriteSource;
  result.templateValueRewriteSource = templateValueRewriteSource;
  result.collectionSurfaceHelpersSource = collectionSurfaceHelpersSource;
  result.inferStructReturnSource = inferStructReturnSource;
  result.inferStructReturnHelpersSource = inferStructReturnHelpersSource;
  result.inferMethodResolutionSource = inferMethodResolutionSource;
  result.inferMethodResolutionHelpersSource = inferMethodResolutionHelpersSource;
  result.inferPreDispatchCallsSource = inferPreDispatchCallsSource;
  result.exprPreDispatchDirectCallsSource = exprPreDispatchDirectCallsSource;
  result.exprVectorHelpersSource = exprVectorHelpersSource;
  result.collectionHelperRewritesSource = collectionHelperRewritesSource;
  result.effectFreeCollectionsSource = effectFreeCollectionsSource;
  result.passesEffectFreeSource = passesEffectFreeSource;
  result.buildParametersSource = buildParametersSource;
  result.buildCallResolutionSource = buildCallResolutionSource;
  result.buildReturnKindsSource = buildReturnKindsSource;
  result.buildInitializerInferenceSource = buildInitializerInferenceSource;
  result.buildInitializerInferenceCallsSource = buildInitializerInferenceCallsSource;
  result.inferCollectionDispatchSource = inferCollectionDispatchSource;
  result.inferCollectionCallResolutionSource = inferCollectionCallResolutionSource;
  result.inferCollectionCompatibilitySource = inferCollectionCompatibilitySource;
  result.inferCollectionCompatibilityInternalSource = inferCollectionCompatibilityInternalSource;
  result.inferCollectionDispatchSetupSource = inferCollectionDispatchSetupSource;
  result.inferCollectionsSource = inferCollectionsSource;
  result.inferCollectionReturnInferenceSource = inferCollectionReturnInferenceSource;
  result.inferCollectionBufferAndMapResolversSource = inferCollectionBufferAndMapResolversSource;
  result.inferTargetResolutionSource = inferTargetResolutionSource;
  result.inferDefinitionSource = inferDefinitionSource;
  result.inferLateFallbackBuiltinsSource = inferLateFallbackBuiltinsSource;
  result.exprLateFallbackBuiltinsSource = exprLateFallbackBuiltinsSource;
  result.exprLateCallCompatibilitySource = exprLateCallCompatibilitySource;
  result.lateMapAccessBuiltinsSource = lateMapAccessBuiltinsSource;
  result.exprLateUnknownTargetFallbacksSource = exprLateUnknownTargetFallbacksSource;
  result.passesDiagnosticsSource = passesDiagnosticsSource;
  result.exprTrySource = exprTrySource;
  result.pointerLikeSource = pointerLikeSource;
  result.statementPrintabilitySource = statementPrintabilitySource;
  result.statementBodyArgumentsSource = statementBodyArgumentsSource;
  result.exprBodyArgumentsSource = exprBodyArgumentsSource;
  result.scalarPointerMemorySource = scalarPointerMemorySource;
  result.statementSource = statementSource;
  result.statementContainerHelpersSource = statementContainerHelpersSource;
  result.buildDirectCallBindingSource = buildDirectCallBindingSource;
  result.argumentValidationSource = argumentValidationSource;
  result.collectionLiteralValidationSource = collectionLiteralValidationSource;
  result.resolvedCallArgumentsSource = resolvedCallArgumentsSource;
  result.argumentValidationCollectionsSource = argumentValidationCollectionsSource;
  result.namedArgumentBuiltinsSource = namedArgumentBuiltinsSource;
  result.collectionAccessValidationSource = collectionAccessValidationSource;
  result.collectionAccessSource = collectionAccessSource;
  result.exprSource = exprSource;
  result.builtinContextSetupSource = builtinContextSetupSource;
  result.collectionAccessSetupSource = collectionAccessSetupSource;
  result.lateCollectionAccessFallbacksSource = lateCollectionAccessFallbacksSource;
  result.collectionDispatchSetupSource = collectionDispatchSetupSource;
  result.mapSoaBuiltinsSource = mapSoaBuiltinsSource;
  result.lateMapSoaBuiltinsSource = lateMapSoaBuiltinsSource;
  result.countCapacityMapBuiltinSource = countCapacityMapBuiltinSource;
  result.emitterCallPathHelpersSource = emitterCallPathHelpersSource;
  result.emitterReturnInferenceCollectionsSource = emitterReturnInferenceCollectionsSource;
  result.emitterCollectionTypeHelpersSource = emitterCollectionTypeHelpersSource;
  result.emitterPackedArgsSource = emitterPackedArgsSource;
  result.emitterHelpersSource = emitterHelpersSource;
  result.emitterMethodMetadataSource = emitterMethodMetadataSource;
  result.statementLowererSource = statementLowererSource;
  result.operatorCollectionMutationSource = operatorCollectionMutationSource;
  result.lowererCallHelpersSource = lowererCallHelpersSource;
  result.textFilterPipelinePassSource = textFilterPipelinePassSource;
  result.lowerStatementsExprSource = lowerStatementsExprSource;
  result.lowerStatementsBindingsSource = lowerStatementsBindingsSource;
  result.inlineNativeSource = inlineNativeSource;
  result.emitterMethodResolutionSource = emitterMethodResolutionSource;
  result.emitterMethodTypeInferenceSource = emitterMethodTypeInferenceSource;
  result.emitterHelpersTypesSource = emitterHelpersTypesSource;
  result.nativeTailSource = nativeTailSource;
  result.tailDispatchSource = tailDispatchSource;
  result.lowerEmitExprCollectionSource = lowerEmitExprCollectionSource;
  result.builtinNameHelpersSource = builtinNameHelpersSource;
  result.lowererHelpersSource = lowererHelpersSource;
  result.inlinePackedArgsSource = inlinePackedArgsSource;
  result.inlineParamHelpersSource = inlineParamHelpersSource;
  result.packedResultSource = packedResultSource;
  result.uninitializedStructInferenceSource = uninitializedStructInferenceSource;
  result.structSlotLayoutSource = structSlotLayoutSource;
  result.lowererStructTypeHelpersSource = lowererStructTypeHelpersSource;
  result.declaredCollectionInferenceSource = declaredCollectionInferenceSource;
  result.bindingTypeHelpersSource = bindingTypeHelpersSource;
  result.inferenceDispatchSetupSource = inferenceDispatchSetupSource;
  result.inferenceBaseKindSource = inferenceBaseKindSource;
  result.setupTypeMethodTargetSource = setupTypeMethodTargetSource;
  result.setupTypeMethodCallSource = setupTypeMethodCallSource;
  result.setupTypeReceiverTargetSource = setupTypeReceiverTargetSource;
  result.setupTypeReturnKindSource = setupTypeReturnKindSource;
  result.lowererStructReturnPathSource = lowererStructReturnPathSource;
  result.setupTypeCollectionSource = setupTypeCollectionSource;
  result.countAccessSource = countAccessSource;
  result.resultMetadataSource = resultMetadataSource;
  result.semanticsResultHelpersSource = semanticsResultHelpersSource;
  result.statementReturnsSource = statementReturnsSource;
  result.inlineCallContextSource = inlineCallContextSource;
  result.accessTargetResolutionSource = accessTargetResolutionSource;
  return result;
}
