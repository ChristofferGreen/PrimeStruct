#include "third_party/doctest.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::filesystem::path repoRoot() {
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

std::string readText(const std::filesystem::path &path) {
  std::ifstream input(path);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::filesystem::path collectionsFile(const std::string &name) {
  return repoRoot() / "stdlib" / "std" / "collections" / name;
}

std::string relativePathString(const std::filesystem::path &path) {
  return std::filesystem::relative(path, repoRoot()).generic_string();
}

bool isSourceFile(const std::filesystem::path &path) {
  const std::string extension = path.extension().string();
  return extension == ".cpp" || extension == ".h" || extension == ".hpp";
}

std::vector<std::filesystem::path> productionMapTraceFiles() {
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

bool isAllowedMapBackingFile(const std::string &relativePath) {
  static const std::vector<std::string> files = {
      "src/ir_lowerer/IrLowererAccessTargetResolution.cpp",
      "src/ir_lowerer/IrLowererInlinePackedArgs.cpp",
      "src/ir_lowerer/IrLowererInlineParamHelpers.cpp",
      "src/ir_lowerer/IrLowererLowerEmitExprCollectionHelpers.h",
      "src/ir_lowerer/IrLowererLowerEmitExprTailDispatch.h",
      "src/ir_lowerer/IrLowererLowerInferenceDispatchSetup.cpp",
      "src/ir_lowerer/IrLowererSetupTypeDeclaredCollectionInference.cpp",
      "src/ir_lowerer/IrLowererSetupTypeMethodTargetHelpers.cpp",
      "src/ir_lowerer/IrLowererUninitializedStructInference.cpp",
  };
  return std::find(files.begin(), files.end(), relativePath) != files.end();
}

bool isAllowedExperimentalMapTrace(const std::string &relativePath,
                                   const std::string &line) {
  auto contains = [&](const std::string &needle) {
    return line.find(needle) != std::string::npos;
  };

  if (contains("std/collections/experimental_map/Map")) {
    return isAllowedMapBackingFile(relativePath);
  }
  if (contains("std/collections/experimental_map/Entry")) {
    return relativePath ==
           "src/ir_lowerer/IrLowererLowerEmitExprCollectionHelpers.h";
  }
  if (relativePath == "src/ir_lowerer/IrLowererLowerEmitExprCollectionHelpers.h" &&
      contains("receiverDef->fullPath.rfind(\"/std/collections/experimental_map/\", 0) == 0")) {
    return true;
  }
  return false;
}

} // namespace

TEST_SUITE_BEGIN("primestruct.stdlib.map_ownership");

TEST_CASE("canonical map surface owns standalone stdlib implementation") {
  const std::string mapSource = readText(collectionsFile("map.prime"));
  const std::string experimentalSource = readText(collectionsFile("experimental_map.prime"));
  const std::string internalSource = readText(collectionsFile("internal_map.prime"));
  const std::string surfacesSource = readText(collectionsFile("surfaces.psmeta"));
  const std::string registrySource = readText(repoRoot() / "src" / "StdlibSurfaceRegistry.cpp");
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
  const std::string exprCollectionDispatchSetupSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprCollectionDispatchSetup.cpp");
  const std::string exprCollectionCountCapacitySource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprCollectionCountCapacity.cpp");
  const std::string templateCoreSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphCoreUtilities.h");
  const std::string templateReceiverSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphExperimentalCollectionReceiverResolution.h");
  const std::string templateExpressionRewriteSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphExpressionRewrite.h");
  const std::string templateFallbackTypeInferenceSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphFallbackTypeInference.h");
  const std::string templateTypeResolutionSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphTypeResolution.h");
  const std::string templateCollectionCompatibilitySource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphCollectionCompatibilityPaths.h");
  const std::string templateBindingCallInferenceSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphBindingCallInference.h");
  const std::string templateConstructorRewriteSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphExperimentalCollectionConstructorRewrites.h");
  const std::string mapConstructorHelpersSource =
      readText(repoRoot() / "src" / "semantics" / "MapConstructorHelpers.h");
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
  const std::string inferDefinitionSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferDefinition.cpp");
  const std::string inferLateFallbackBuiltinsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferLateFallbackBuiltins.cpp");
  const std::string exprLateFallbackBuiltinsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprLateFallbackBuiltins.cpp");
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
  const std::string scalarPointerMemorySource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprScalarPointerMemory.cpp");
  const std::string statementSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorStatement.cpp");
  const std::string argumentValidationSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprArgumentValidation.cpp");
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
  const std::string collectionDispatchSetupSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprCollectionDispatchSetup.cpp");
  const std::string mapSoaBuiltinsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprMapSoaBuiltins.cpp");
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
  const std::string lowererCallHelpersSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererCallHelpers.cpp");
  const std::string lowerStatementsExprSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererLowerStatementsExpr.h");
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
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTailDispatch.h");
  const std::string lowerEmitExprCollectionSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererLowerEmitExprCollectionHelpers.h");
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
  REQUIRE(!experimentalSource.empty());
  REQUIRE(!internalSource.empty());
  REQUIRE(!surfacesSource.empty());
  REQUIRE(!registrySource.empty());
  REQUIRE(!publicationBuildersSource.empty());
  REQUIRE(!semanticsSource.empty());
  REQUIRE(!validatorSource.empty());
  REQUIRE(!callResolutionSource.empty());
  REQUIRE(!callPathHelpersSource.empty());
  REQUIRE(!methodTargetResolutionSource.empty());
  REQUIRE(!receiverPathsSource.empty());
  REQUIRE(!templateCoreSource.empty());
  REQUIRE(!templateReceiverSource.empty());
  REQUIRE(!templateExpressionRewriteSource.empty());
  REQUIRE(!templateFallbackTypeInferenceSource.empty());
  REQUIRE(!templateTypeResolutionSource.empty());
  REQUIRE(!templateBindingCallInferenceSource.empty());
  REQUIRE(!templateConstructorRewriteSource.empty());
  REQUIRE(!mapConstructorHelpersSource.empty());
  REQUIRE(!semanticBindingTypeHelpersSource.empty());
  REQUIRE(!inferStructReturnSource.empty());
  REQUIRE(!inferStructReturnHelpersSource.empty());
  REQUIRE(!inferMethodResolutionSource.empty());
  REQUIRE(!inferMethodResolutionHelpersSource.empty());
  REQUIRE(!inferPreDispatchCallsSource.empty());
  REQUIRE(!exprPreDispatchDirectCallsSource.empty());
  REQUIRE(!exprVectorHelpersSource.empty());
  REQUIRE(!collectionHelperRewritesSource.empty());
  REQUIRE(!effectFreeCollectionsSource.empty());
  REQUIRE(!buildParametersSource.empty());
  REQUIRE(!buildCallResolutionSource.empty());
  REQUIRE(!buildReturnKindsSource.empty());
  REQUIRE(!buildInitializerInferenceSource.empty());
  REQUIRE(!inferCollectionDispatchSource.empty());
  REQUIRE(!inferCollectionCompatibilitySource.empty());
  REQUIRE(!inferCollectionDispatchSetupSource.empty());
  REQUIRE(!inferCollectionReturnInferenceSource.empty());
  REQUIRE(!inferCollectionBufferAndMapResolversSource.empty());
  REQUIRE(!inferDefinitionSource.empty());
  REQUIRE(!inferLateFallbackBuiltinsSource.empty());
  REQUIRE(!exprLateFallbackBuiltinsSource.empty());
  REQUIRE(!lateMapAccessBuiltinsSource.empty());
  REQUIRE(!exprLateUnknownTargetFallbacksSource.empty());
  REQUIRE(!passesDiagnosticsSource.empty());
  REQUIRE(!exprTrySource.empty());
  REQUIRE(!pointerLikeSource.empty());
  REQUIRE(!statementPrintabilitySource.empty());
  REQUIRE(!statementBodyArgumentsSource.empty());
  REQUIRE(!scalarPointerMemorySource.empty());
  REQUIRE(!statementSource.empty());
  REQUIRE(!argumentValidationSource.empty());
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
  REQUIRE(!lowererCallHelpersSource.empty());
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

  CHECK(mapSource.find("import /std/collections/internal_map/*") == std::string::npos);
  CHECK(mapSource.find("import /std/collections/internal_vector/*") != std::string::npos);
  CHECK(mapSource.find("/std/collections/experimental_map/") == std::string::npos);
  CHECK(mapSource.find("/std/collections/internal_map/insertImpl") == std::string::npos);
  CHECK(mapSource.find("insert_builtin") == std::string::npos);
  CHECK(mapSource.find("Reference<Map<K, V>>") == std::string::npos);
  CHECK(mapSource.find("Reference<map<K, V>>") == std::string::npos);
  CHECK(mapSource.find("MapValue<K, V>") != std::string::npos);
  CHECK(mapSource.find("mapInsert<K, V>([MapValue<K, V> mut] entries") !=
        std::string::npos);

  CHECK(experimentalSource.find("import /std/collections/internal_map/*") != std::string::npos);
  CHECK(experimentalSource.find("namespace experimental_map") == std::string::npos);
  CHECK(experimentalSource.find("mapFindIndex") == std::string::npos);
  CHECK(experimentalSource.find("mapOverwriteSlot") == std::string::npos);
  CHECK(experimentalSource.find("[public struct]") == std::string::npos);

  CHECK(internalSource.find("mapFindIndex") != std::string::npos);
  CHECK(internalSource.find("mapOverwriteSlot") != std::string::npos);
  const auto internalNamespacePos = internalSource.find("namespace internal_map");
  CHECK(internalNamespacePos != std::string::npos);
  CHECK(internalSource.find("namespace experimental_map") != std::string::npos);
  CHECK(internalSource.find("insertImpl<K, V>") != std::string::npos);

  REQUIRE(internalNamespacePos != std::string::npos);
  const std::string internalNamespace = internalSource.substr(internalNamespacePos);
  CHECK(internalNamespace.find("findIndexImpl<K, V>") != std::string::npos);
  CHECK(internalNamespace.find("borrowedFindIndexImpl<K, V>") != std::string::npos);
  CHECK(internalNamespace.find("overwriteSlotImpl<V>") != std::string::npos);
  CHECK(internalNamespace.find("containerErrorResult<V>(containerMissingKey())") != std::string::npos);
  CHECK(internalNamespace.find("/std/collections/experimental_map/mapContains") == std::string::npos);
  CHECK(internalNamespace.find("/std/collections/experimental_map/mapTryAt") == std::string::npos);
  CHECK(internalNamespace.find("/std/collections/experimental_map/mapAt") == std::string::npos);
  CHECK(internalNamespace.find("/std/collections/experimental_map/mapInsert") == std::string::npos);

  CHECK(surfacesSource.find("id = CollectionsMapHelpers") != std::string::npos);
  CHECK(surfacesSource.find("id = CollectionsMapConstructors") != std::string::npos);
  CHECK(surfacesSource.find("member_name = at_unsafe_ref") != std::string::npos);
  CHECK(surfacesSource.find("member_alias = mapInsertRef -> insert_ref") ==
        std::string::npos);
  CHECK(surfacesSource.find("compatibility_spelling = /map/count") == std::string::npos);
  CHECK(surfacesSource.find(
            "lowering_spelling = /std/collections/experimental_map/mapInsertRef") ==
        std::string::npos);
  CHECK(registrySource.find("CollectionsMapHelperMembers") == std::string::npos);
  CHECK(registrySource.find("CollectionsMapConstructorMembers") == std::string::npos);
  CHECK(registrySource.find("resolveCollectionsMapHelperMemberName") == std::string::npos);
  CHECK(registrySource.find("\"/std/collections/map/insert\"") == std::string::npos);
  CHECK(publicationBuildersSource.find("typeName == \"/map\"") ==
        std::string::npos);
  CHECK(publicationBuildersSource.find("typeName == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(publicationBuildersSource.find(
            "StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(publicationBuildersSource.find(
            "StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(publicationBuildersSource.find("mapHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(publicationBuildersSource.find("mapConstructorSurfaceMetadataLocal()") !=
        std::string::npos);

  CHECK(semanticsSource.find("path == \"mapSingle\"") == std::string::npos);
  CHECK(semanticsSource.find("path == \"/std/collections/mapSingle\"") ==
        std::string::npos);
  CHECK(semanticsSource.find("path == \"/std/collections/mapPair\"") ==
        std::string::npos);
  CHECK(semanticsSource.find("constructorBackedBuiltinMapBindings.count(initializer.args.front().name)") !=
        std::string::npos);
  CHECK(semanticsSource.find("kBuiltinCanonicalMapInsertBuiltinPath") == std::string::npos);
  CHECK(semanticsSource.find("\"/std/collections/map/insert_builtin\"") == std::string::npos);
  CHECK(semanticsSource.find("isCanonicalBuiltinMapReadHelperName") != std::string::npos);
  CHECK(semanticsSource.find(
            "StdlibSurfaceId::CollectionsMapHelpers") == std::string::npos);
  CHECK(semanticsSource.find(
            "canonicalPrefix = \"std/collections/map/\"") == std::string::npos);
  CHECK(semanticsSource.find("aliasPrefix = \"map/\"") ==
        std::string::npos);
  CHECK(semanticsSource.find("path == \"/std/collections/map/map\"") ==
        std::string::npos);
  CHECK(semanticsSource.find(
            "expr.namespacePrefix == \"/std/collections/map\"") ==
        std::string::npos);
  CHECK(semanticsSource.find("normalized.rfind(\"/map/\", 0)") ==
        std::string::npos);
  CHECK(semanticsSource.find(
            "\"/std/collections/map/\" + helperName") ==
        std::string::npos);
  CHECK(semanticsSource.find(
            "metadataBackedMapHelperMethodName(normalizedName)") !=
        std::string::npos);
  CHECK(semanticsSource.find(
            "metadataBackedCanonicalMapHelperPath(helperName)") !=
        std::string::npos);
  CHECK(semanticsSource.find("explicitRemovedMapCompatibilityReadPath") ==
        std::string::npos);
  CHECK(semanticsSource.find("explicitRemovedKeyValueCompatibilityReadPath") !=
        std::string::npos);
  CHECK(semanticsSource.find("isResolvedMapConstructorPath(path)") !=
        std::string::npos);
  CHECK(semanticBindingTypeHelpersSource.find(
            "collectionTypePathLocal(\"map\")") == std::string::npos);
  CHECK(semanticBindingTypeHelpersSource.find(
            "std/collections/map") == std::string::npos);
  CHECK(semanticBindingTypeHelpersSource.find(
            "matchesMapCollectionRootMetadataLocal(") ==
        std::string::npos);
  CHECK(semanticBindingTypeHelpersSource.find(
            "matchesMapValueRootMetadataLocal(") ==
        std::string::npos);
  CHECK(semanticBindingTypeHelpersSource.find(
            "matchesKeyValueCollectionRootMetadataLocal(normalized)") !=
        std::string::npos);
  CHECK(semanticBindingTypeHelpersSource.find(
            "matchesKeyValueBackingRootMetadataLocal(normalized)") !=
        std::string::npos);
  CHECK(semanticBindingTypeHelpersSource.find(
            "isQualifiedExperimentalMapBackingTypeName(base)") !=
        std::string::npos);
  CHECK(validatorSource.find("name.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(validatorSource.find("name.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(validatorSource.find("path.rfind(\"/std/collections/map/count__t\"") ==
        std::string::npos);
  CHECK(validatorSource.find("path.rfind(\"/map/count__t\"") ==
        std::string::npos);
  CHECK(validatorSource.find(
            "metadataBackedMapHelperMethodName(normalizedName)") !=
        std::string::npos);
  CHECK(validatorSource.find("const std::string mapHelperName") ==
        std::string::npos);
  CHECK(validatorSource.find("const std::string keyValueHelperName") !=
        std::string::npos);
  CHECK(validatorSource.find("metadataBackedCanonicalMapHelperPath(\"count\")") !=
        std::string::npos);
  CHECK(callResolutionSource.find("\"/map/entry\"") == std::string::npos);
  CHECK(callResolutionSource.find("\"/map/entry__\"") == std::string::npos);
  CHECK(callResolutionSource.find("directExplicitCallPath == \"/map/at\"") ==
        std::string::npos);
  CHECK(callResolutionSource.find("path == \"/std/collections/map/entry\"") ==
        std::string::npos);
  CHECK(callResolutionSource.find("candidatePath == \"/std/collections/map/map\"") ==
        std::string::npos);
  CHECK(callResolutionSource.find(
            "isExperimentalCollectionConstructorPathLocal(path, \"map\", \"entry\")") ==
        std::string::npos);
  CHECK(callResolutionSource.find(
            "resolveStdlibSurfaceMemberName(*metadata, path) == \"entry\"") !=
        std::string::npos);
  CHECK(callResolutionSource.find("mapHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(callResolutionSource.find("mapConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(mapConstructorHelpersSource.find(
            "StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(mapConstructorHelpersSource.find(
            "resolveMapConstructorMemberPath(normalizedPath, memberName)") !=
        std::string::npos);
  CHECK(callPathHelpersSource.find("name.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(callPathHelpersSource.find("name.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(callPathHelpersSource.find("mapHelperName") == std::string::npos);
  CHECK(callPathHelpersSource.find("resolveMapHelperMemberName(name, keyValueHelperName)") ==
        std::string::npos);
  CHECK(callPathHelpersSource.find("resolveKeyValueHelperMemberName(name, keyValueHelperName)") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find("rawMethodName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(builtinPathHelpersSource.find(
            "rawMethodName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(builtinPathHelpersSource.find("name.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(builtinPathHelpersSource.find("name.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(builtinPathHelpersSource.find("extractHelper(\"map/\", \"map\")") ==
        std::string::npos);
  CHECK(builtinPathHelpersSource.find("resolvedMapHelperName") ==
        std::string::npos);
  CHECK(builtinPathHelpersSource.find("mapHelperName") ==
        std::string::npos);
  CHECK(builtinPathHelpersSource.find("isRemovedMapCompatibilityHelper(") ==
        std::string::npos);
  CHECK(builtinPathHelpersSource.find("mapHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find("isRemovedKeyValueCompatibilityHelper(") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find("resolveMapHelperMemberNameLocal(") ==
        std::string::npos);
  CHECK(builtinPathHelpersSource.find("resolveKeyValueHelperMemberNameLocal(") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find("resolveRootMapAliasHelperMemberNameLocal(") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find("resolvedKeyValueHelperName") !=
        std::string::npos);
  CHECK(builtinPathHelpersSource.find("keyValueHelperName") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("const std::string alias = \"/map/\" + resolvedHelperName") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("resolvedPath == \"/map/") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("candidate.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("explicitMapHelperPath.rfind(\"/map/\", 0)") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("explicitMapMethodPath") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("explicitMapHelperPath") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("borrowedMapHelperNameForReceiver") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("preferredMapMethodTarget") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("preferredMapHelper") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("getDirectMapHelperCompatibilityPath") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("removedMapCompatibilityPath") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("isRemovedMapCompatibilityHelper(") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("resolveExplicitRootMapMethodPath") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("resolvedExplicitRootMapMethod") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("explicitKeyValueMethodPath") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("explicitKeyValueHelperPath") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("borrowedKeyValueHelperNameForReceiver") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("preferredKeyValueMethodTarget") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("preferredKeyValueHelper") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("getDirectKeyValueHelperCompatibilityPath") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("removedKeyValueCompatibilityPath") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("isRemovedKeyValueCompatibilityHelper(") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("resolveExplicitRootKeyValueMethodPath") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("resolvedExplicitRootKeyValueMethod") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("normalized == \"map/count\"") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("canonicalMapHelperPathLocal(") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("canonicalMapHelperNamespaceLocal(") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("resolveCanonicalMapHelperNameFromSpelling(") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("isMapHelperImportAliasNamespaceForMethodTargets(") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("rootedMapHelperAliasPathForMethodTargets(") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("rootAliasMapHelperNameForMethodTargets(") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("isRootedMapHelperAliasPathForMethodTargets(") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("isStdNamespacedMapHelper") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("resolvedCanonicalMapHelperName") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("canonicalMapHelperName") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("isCanonicalMapAccessMethodName") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("canonicalKeyValueHelperPathLocal(") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("canonicalKeyValueHelperNamespaceLocal(") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("resolveCanonicalKeyValueHelperNameFromSpelling(") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("isKeyValueHelperImportAliasNamespaceForMethodTargets(") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("rootedKeyValueHelperAliasPathForMethodTargets(") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("rootAliasKeyValueHelperNameForMethodTargets(") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("isRootedKeyValueHelperAliasPathForMethodTargets(") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("isStdNamespacedKeyValueHelper") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("resolvedCanonicalKeyValueHelperName") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("canonicalKeyValueHelperName") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("isCanonicalKeyValueAccessMethodName") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find(
            "metadataBackedMapHelperRootAliasMethodName(candidate)") !=
        std::string::npos);
  CHECK(receiverPathsSource.find("resolvedReceiverPath == \"/map\"") ==
        std::string::npos);
  CHECK(receiverPathsSource.find(
            "isRootMapCollectionReceiverPath(resolvedReceiverPath)") !=
        std::string::npos);
  CHECK(receiverPathsSource.find("mapHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(exprMethodResolutionSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(exprMethodResolutionSource.find("resolvedMapHelperName") ==
        std::string::npos);
  CHECK(exprMethodResolutionSource.find("mapNamespacedMethodCompatibilityPath") ==
        std::string::npos);
  CHECK(exprMethodResolutionSource.find("isCanonicalMapAccessMethodName") ==
        std::string::npos);
  CHECK(exprMethodResolutionSource.find("canonicalMapAccessReturnsString") ==
        std::string::npos);
  CHECK(exprMethodResolutionSource.find(
            "rejectBuiltinStringCountShadowOnMapAccessReceiver") ==
        std::string::npos);
  CHECK(exprMethodResolutionSource.find("canonicalMapHelperName") ==
        std::string::npos);
  CHECK(exprMethodResolutionSource.find(
            "metadataBackedCanonicalMapHelperPath(helperName)") !=
        std::string::npos);
  CHECK(exprMethodResolutionSource.find("resolvedKeyValueHelperName") !=
        std::string::npos);
  CHECK(exprMethodResolutionSource.find(
            "keyValueNamespacedMethodCompatibilityPath") !=
        std::string::npos);
  CHECK(exprMethodResolutionSource.find("isCanonicalKeyValueAccessMethodName") !=
        std::string::npos);
  CHECK(exprMethodResolutionSource.find("canonicalKeyValueAccessReturnsString") !=
        std::string::npos);
  CHECK(exprMethodResolutionSource.find(
            "rejectBuiltinStringCountShadowOnKeyValueAccessReceiver") !=
        std::string::npos);
  CHECK(exprMethodResolutionSource.find("canonicalKeyValueHelperName") !=
        std::string::npos);
  CHECK(privateExprValidationSource.find(
            "isDirectStdNamespacedVectorCountWrapperMapTarget") ==
        std::string::npos);
  CHECK(privateExprValidationSource.find("getDirectMapHelperCompatibilityPath") ==
        std::string::npos);
  CHECK(privateExprValidationSource.find(
            "isDirectStdNamespacedVectorCountWrapperKeyValueTarget") !=
        std::string::npos);
  CHECK(privateExprValidationSource.find("getDirectKeyValueHelperCompatibilityPath") !=
        std::string::npos);
  CHECK(exprCollectionDispatchSetupSource.find(
            "isDirectStdNamespacedVectorCountWrapperMapTarget") ==
        std::string::npos);
  CHECK(exprCollectionDispatchSetupSource.find("directRemovedMapCompatibilityPath") ==
        std::string::npos);
  CHECK(exprCollectionDispatchSetupSource.find("directMapHelperCompatibilityPath") ==
        std::string::npos);
  CHECK(exprCollectionDispatchSetupSource.find(
            "isDirectStdNamespacedVectorCountWrapperKeyValueTarget") !=
        std::string::npos);
  CHECK(exprCollectionDispatchSetupSource.find(
            "directRemovedKeyValueCompatibilityPath") !=
        std::string::npos);
  CHECK(exprCollectionDispatchSetupSource.find(
            "directKeyValueHelperCompatibilityPath") !=
        std::string::npos);
  CHECK(exprCollectionCountCapacitySource.find(
            "isDirectStdNamespacedVectorCountWrapperMapTarget") ==
        std::string::npos);
  CHECK(exprCollectionCountCapacitySource.find(
            "isDirectStdNamespacedVectorCountWrapperKeyValueTarget") !=
        std::string::npos);
  CHECK(privateExprInferenceSource.find("preferredExperimentalMapHelperTarget") ==
        std::string::npos);
  CHECK(privateExprInferenceSource.find(
            "preferredCanonicalExperimentalMapHelperTarget") ==
        std::string::npos);
  CHECK(privateExprInferenceSource.find("canonicalExperimentalMapHelperPath") ==
        std::string::npos);
  CHECK(privateExprInferenceSource.find(
            "canonicalizeExperimentalMapHelperResolvedPath") ==
        std::string::npos);
  CHECK(privateExprInferenceSource.find(
            "shouldLogicalCanonicalizeDefinedExperimentalMapHelperPath") ==
        std::string::npos);
  CHECK(privateExprInferenceSource.find("preferredBareMapHelperTarget") ==
        std::string::npos);
  CHECK(privateExprInferenceSource.find("specializedExperimentalMapHelperTarget") ==
        std::string::npos);
  CHECK(privateExprInferenceSource.find(
            "tryRewriteCanonicalExperimentalMapHelperCall") ==
        std::string::npos);
  CHECK(privateExprInferenceSource.find(
            "explicitCanonicalExperimentalMapBorrowedHelperPath") ==
        std::string::npos);
  CHECK(privateExprInferenceSource.find("mapNamespacedMethodCompatibilityPath") ==
        std::string::npos);
  CHECK(privateExprInferenceSource.find("directMapHelperCompatibilityPath") ==
        std::string::npos);
  CHECK(privateExprInferenceSource.find(
            "preferredExperimentalKeyValueHelperTarget") !=
        std::string::npos);
  CHECK(privateExprInferenceSource.find(
            "preferredCanonicalExperimentalKeyValueHelperTarget") !=
        std::string::npos);
  CHECK(privateExprInferenceSource.find("canonicalExperimentalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(privateExprInferenceSource.find(
            "canonicalizeExperimentalKeyValueHelperResolvedPath") !=
        std::string::npos);
  CHECK(privateExprInferenceSource.find(
            "shouldLogicalCanonicalizeDefinedExperimentalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(privateExprInferenceSource.find("preferredBareKeyValueHelperTarget") !=
        std::string::npos);
  CHECK(privateExprInferenceSource.find(
            "specializedExperimentalKeyValueHelperTarget") !=
        std::string::npos);
  CHECK(privateExprInferenceSource.find(
            "tryRewriteCanonicalExperimentalKeyValueHelperCall") !=
        std::string::npos);
  CHECK(privateExprInferenceSource.find(
            "explicitCanonicalExperimentalKeyValueBorrowedHelperPath") !=
        std::string::npos);
  CHECK(privateExprInferenceSource.find(
            "keyValueNamespacedMethodCompatibilityPath") !=
        std::string::npos);
  CHECK(privateExprInferenceSource.find("directKeyValueHelperCompatibilityPath") !=
        std::string::npos);
  CHECK(templateCoreSource.find("name == \"/map\"") == std::string::npos);
  CHECK(templateCoreSource.find("name == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(templateCoreSource.find("importPath == \"/std/collections/map\"") ==
        std::string::npos);
  CHECK(templateCoreSource.find("path == \"/std/collections/map/entry\"") ==
        std::string::npos);
  CHECK(templateCoreSource.find("\"/map/entry\"") == std::string::npos);
  CHECK(templateCoreSource.find("\"/map/entry__\"") == std::string::npos);
  CHECK(templateCoreSource.find("isTemplateMonomorphMapImportAlias(name)") !=
        std::string::npos);
  CHECK(templateCoreSource.find(
            "isTemplateMonomorphMapConstructorCallPath(resolvedPath)") !=
        std::string::npos);
  CHECK(templateCoreSource.find("metadataBackedMapHelperMethodName(path)") !=
        std::string::npos);
  CHECK(templateReceiverSource.find("|| resolvedPath == \"/map/") ==
        std::string::npos);
  CHECK(templateReceiverSource.find("path.rfind(\"/map/\", 0)") ==
        std::string::npos);
  CHECK(templateReceiverSource.find(
            "isTemplateMonomorphMapImportAliasHelperPath(path)") !=
        std::string::npos);
  CHECK(templateReceiverSource.find("templateMonomorphMapHelperSurfaceMetadata") ==
        std::string::npos);
  CHECK(templateReceiverSource.find("resolveTemplateMonomorphMapHelperName") ==
        std::string::npos);
  CHECK(templateReceiverSource.find("templateMonomorphCanonicalMapHelperPath") ==
        std::string::npos);
  CHECK(templateReceiverSource.find("resolveTemplateMonomorphCanonicalMapHelperName") ==
        std::string::npos);
  CHECK(templateReceiverSource.find("isTemplateMonomorphCanonicalMapHelperPath") ==
        std::string::npos);
  CHECK(templateReceiverSource.find("isTemplateMonomorphCanonicalMapValueAccessPath") ==
        std::string::npos);
  CHECK(templateReceiverSource.find("isTemplateMonomorphCanonicalMapCountPath") ==
        std::string::npos);
  CHECK(templateReceiverSource.find("templateMonomorphPreferredMapHelperSpellingForMember") ==
        std::string::npos);
  CHECK(templateReceiverSource.find("canonicalMapHelperUnknownTargetPath") ==
        std::string::npos);
  CHECK(templateReceiverSource.find("experimentalMapHelperPathForCanonicalHelper") ==
        std::string::npos);
  CHECK(templateReceiverSource.find("experimentalMapHelperPathForWrapperHelper") ==
        std::string::npos);
  CHECK(templateReceiverSource.find("templateMonomorphKeyValueHelperSurfaceMetadata") !=
        std::string::npos);
  CHECK(templateReceiverSource.find("resolveTemplateMonomorphKeyValueHelperName") !=
        std::string::npos);
  CHECK(templateReceiverSource.find("templateMonomorphCanonicalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(templateReceiverSource.find("resolveTemplateMonomorphCanonicalKeyValueHelperName") !=
        std::string::npos);
  CHECK(templateReceiverSource.find("isTemplateMonomorphCanonicalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(templateReceiverSource.find("isTemplateMonomorphCanonicalKeyValueAccessPath") !=
        std::string::npos);
  CHECK(templateReceiverSource.find("isTemplateMonomorphCanonicalKeyValueCountPath") !=
        std::string::npos);
  CHECK(templateReceiverSource.find("templateMonomorphPreferredKeyValueHelperSpellingForMember") !=
        std::string::npos);
  CHECK(templateReceiverSource.find("canonicalKeyValueHelperUnknownTargetPath") !=
        std::string::npos);
  CHECK(templateReceiverSource.find("experimentalKeyValueHelperPathForCanonicalHelper") !=
        std::string::npos);
  CHECK(templateReceiverSource.find("experimentalKeyValueHelperPathForWrapperHelper") !=
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("templateMonomorphCanonicalMapHelperPath") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("isTemplateMonomorphCanonicalMapHelperPath") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("isTemplateMonomorphCanonicalMapValueAccessPath") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("isTemplateMonomorphCanonicalMapCountPath") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("canonicalMapHelperUnknownTargetPath") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("experimentalMapHelperPathForCanonicalHelper") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("experimentalMapHelperPathForWrapperHelper") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("experimentalWrapperMapPath") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("templateMonomorphCanonicalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("isTemplateMonomorphCanonicalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("isTemplateMonomorphCanonicalKeyValueAccessPath") !=
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("isTemplateMonomorphCanonicalKeyValueCountPath") !=
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("canonicalKeyValueHelperUnknownTargetPath") !=
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("experimentalKeyValueHelperPathForCanonicalHelper") !=
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("experimentalKeyValueHelperPathForWrapperHelper") !=
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("experimentalWrapperKeyValuePath") !=
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find(
            "resolvedPath == \"/std/collections/map/count\" || resolvedPath == \"/map/count\"") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find(
            "experimentalCollectionConstructorPathLocal(\"map\", \"mapNew\")") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("const std::string compatibilityPath = \"/map/\" +") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("resolvedPath.rfind(\"/map/\", 0) == 0) &&") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find(
            "removedMapCompatibilityPath.rfind(\"/map/\"") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("removedMapCompatibilityHelperFromPath") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("removedMapCompatibilityPath") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find(
            "const std::string removedMapCompatibilityHelper =") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("isRemovedMapCompatibilityPath") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("mapCompatibilityHelperBase(") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("keyValueCompatibilityHelperBase(") !=
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("mapConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("mapHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("forwardedEmptyConstructorPath") !=
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("removedKeyValueCompatibilityHelperFromPath") !=
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("removedKeyValueCompatibilityPath") !=
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("removedKeyValueCompatibilityHelper") !=
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("isRemovedKeyValueCompatibilityPath") !=
        std::string::npos);
  CHECK(templateTypeResolutionSource.find("isExplicitRemovedMapCompatibilityPath") ==
        std::string::npos);
  CHECK(templateTypeResolutionSource.find("isExplicitRemovedKeyValueCompatibilityPath") !=
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("borrowedCanonicalMapUnknownTarget") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("borrowedCanonicalKeyValueUnknownTarget") !=
        std::string::npos);
  CHECK(templateCollectionCompatibilitySource.find("rawMethodName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(templateCollectionCompatibilitySource.find(
            "rawMethodName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(templateCollectionCompatibilitySource.find("value == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(templateCollectionCompatibilitySource.find(
            "metadataBackedMapHelperMethodName(rawMethodName)") !=
        std::string::npos);
  CHECK(templateCollectionCompatibilitySource.find(
            "mapHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(templateCollectionCompatibilitySource.find(
            "isRemovedMapCompatibilityHelper(") == std::string::npos);
  CHECK(templateCollectionCompatibilitySource.find(
            "mapCompatibilityHelperBase(") == std::string::npos);
  CHECK(templateCollectionCompatibilitySource.find(
            "isRemovedKeyValueCompatibilityHelper(") != std::string::npos);
  CHECK(templateCollectionCompatibilitySource.find(
            "keyValueCompatibilityHelperBase(") != std::string::npos);
  CHECK(templateBindingCallInferenceSource.find("std/collections/map") ==
        std::string::npos);
  CHECK(templateBindingCallInferenceSource.find("experimental_map") ==
        std::string::npos);
  CHECK(templateBindingCallInferenceSource.find("CollectionsMap") ==
        std::string::npos);
  CHECK(templateBindingCallInferenceSource.find(
            "isExperimentalCollectionBackingTypeName(\"map\"") ==
        std::string::npos);
  CHECK(templateFallbackTypeInferenceSource.find("std/collections/map") ==
        std::string::npos);
  CHECK(templateFallbackTypeInferenceSource.find("experimental_map") ==
        std::string::npos);
  CHECK(templateFallbackTypeInferenceSource.find("CollectionsMap") ==
        std::string::npos);
  CHECK(templateFallbackTypeInferenceSource.find("base == \"map\"") ==
        std::string::npos);
  CHECK(templateFallbackTypeInferenceSource.find("builtinCollection == \"map\"") ==
        std::string::npos);
  CHECK(templateFallbackTypeInferenceSource.find("return \"map<\"") ==
        std::string::npos);
  CHECK(templateFallbackTypeInferenceSource.find(
            "isExperimentalCollectionBackingTypeName(\"map\"") ==
        std::string::npos);
  CHECK(templateFallbackTypeInferenceSource.find(
            "isUnspecializedExperimentalMapBackingTypeName(typeName)") !=
        std::string::npos);
  CHECK(templateFallbackTypeInferenceSource.find(
            "isQualifiedExperimentalMapBackingTypeName(typeName)") !=
        std::string::npos);
  CHECK(templateFallbackTypeInferenceSource.find("mapCollectionAliasToken()") !=
        std::string::npos);
  CHECK(templateConstructorRewriteSource.find("originalPath == \"/map\"") ==
        std::string::npos);
  CHECK(templateConstructorRewriteSource.find(
            "originalPath == \"/std/collections/map/map\"") ==
        std::string::npos);
  CHECK(templateConstructorRewriteSource.find(
            "experimentalCollectionConstructorPathLocal(\"map\"") ==
        std::string::npos);
  CHECK(templateConstructorRewriteSource.find(
            "isExperimentalCollectionBackingTypeName(\"map\"") ==
        std::string::npos);
  CHECK(templateConstructorRewriteSource.find(
            "isExperimentalMapConstructorMemberPathLocal(resolvedArgPath, \"entry\")") !=
        std::string::npos);
  CHECK(templateConstructorRewriteSource.find(
            "isExperimentalMapEntryBackingTypeName(normalizedArgType)") !=
        std::string::npos);
  CHECK(templateConstructorRewriteSource.find(
            "experimentalMapConstructorMemberPathLocal(\"map\")") !=
        std::string::npos);
  CHECK(templateConstructorRewriteSource.find(
            "isCanonicalMapConstructorRewriteSourcePath(originalPath)") !=
        std::string::npos);
  CHECK(templateConstructorRewriteSource.find(
            "mapConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(templateConstructorRewriteSource.find(
            "metadata->importAliasSpellings") !=
        std::string::npos);
  CHECK(inferStructReturnSource.find("\"/std/collections/map/\" + methodName, \"/map/\" + methodName") ==
        std::string::npos);
  CHECK(inferStructReturnSource.find("isExplicitMapAccessStructReturnCompatibilityCall") ==
        std::string::npos);
  CHECK(inferStructReturnSource.find("rawMethodName == \"map/at\"") ==
        std::string::npos);
  CHECK(inferStructReturnSource.find("rawMethodName == \"std/collections/map/at\"") ==
        std::string::npos);
  CHECK(inferStructReturnSource.find("\"/std/collections/map/\" + methodName") ==
        std::string::npos);
  CHECK(inferStructReturnSource.find("candidate.rfind(\"/std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(inferStructReturnSource.find("explicitMapHelperName") ==
        std::string::npos);
  CHECK(inferStructReturnSource.find("isExplicitMapAccessStructReturnMethod") ==
        std::string::npos);
  CHECK(inferStructReturnSource.find("candidateHelperName") ==
        std::string::npos);
  CHECK(inferStructReturnSource.find("sourceMethodMapHelperPath") ==
        std::string::npos);
  CHECK(inferStructReturnSource.find("sourceMapHelperPath") ==
        std::string::npos);
  CHECK(inferStructReturnSource.find(
            "resolveExplicitPublishedMapHelperExprMemberName(") ==
        std::string::npos);
  CHECK(inferStructReturnSource.find(
            "resolveCanonicalCompatibilityMapHelperNameFromResolvedPath(") ==
        std::string::npos);
  CHECK(inferStructReturnSource.find("explicitKeyValueHelperName") !=
        std::string::npos);
  CHECK(inferStructReturnSource.find(
            "isExplicitKeyValueAccessStructReturnMethod") !=
        std::string::npos);
  CHECK(inferStructReturnSource.find("candidateKeyValueHelperName") !=
        std::string::npos);
  CHECK(inferStructReturnSource.find("sourceMethodKeyValueHelperPath") !=
        std::string::npos);
  CHECK(inferStructReturnSource.find("sourceKeyValueHelperPath") !=
        std::string::npos);
  CHECK(inferStructReturnSource.find("const bool isKeyValueReceiver") !=
        std::string::npos);
  CHECK(inferStructReturnSource.find("const bool isMapReceiver") ==
        std::string::npos);
  CHECK(inferStructReturnSource.find("const bool isExperimentalKeyValueReceiver") !=
        std::string::npos);
  CHECK(inferStructReturnSource.find("const bool isExperimentalMapReceiver") ==
        std::string::npos);
  CHECK(inferStructReturnSource.find(
            "resolveExplicitPublishedKeyValueHelperExprMemberName(") !=
        std::string::npos);
  CHECK(inferStructReturnSource.find("metadataBackedCanonicalMapHelperPath(methodName)") !=
        std::string::npos);
  CHECK(inferStructReturnSource.find(
            "resolveCanonicalCompatibilityKeyValueHelperNameFromResolvedPath(") !=
        std::string::npos);
  CHECK(inferStructReturnHelpersSource.find("isExplicitMapAccessStructReturnCompatibilityCall") ==
        std::string::npos);
  CHECK(inferStructReturnHelpersSource.find("normalized == \"map/at\"") ==
        std::string::npos);
  CHECK(inferStructReturnHelpersSource.find("return \"/map\"") ==
        std::string::npos);
  CHECK(inferStructReturnHelpersSource.find("collectionTypePathLocal(\"map\"") ==
        std::string::npos);
  CHECK(inferStructReturnHelpersSource.find(
            "mapCollectionMarkerPathForInferStructReturn()") ==
        std::string::npos);
  CHECK(inferStructReturnHelpersSource.find(
            "unrootedMapHelperPrefixForInferStructReturn()") ==
        std::string::npos);
  CHECK(inferStructReturnHelpersSource.find(
            "mapValueRootForInferStructReturn()") ==
        std::string::npos);
  CHECK(inferStructReturnHelpersSource.find(
            "unrootedMapPrefix") ==
        std::string::npos);
  CHECK(inferStructReturnHelpersSource.find(
            "keyValueCollectionMarkerPathForInferStructReturn()") !=
        std::string::npos);
  CHECK(inferStructReturnHelpersSource.find(
            "unrootedKeyValueHelperPrefixForInferStructReturn()") !=
        std::string::npos);
  CHECK(inferStructReturnHelpersSource.find(
            "keyValueBackingRootForInferStructReturn()") !=
        std::string::npos);
  CHECK(inferStructReturnHelpersSource.find(
            "unrootedKeyValuePrefix") !=
        std::string::npos);
  CHECK(inferStructReturnHelpersSource.find("mapConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(inferStructReturnHelpersSource.find("mapHelperSurfaceMetadataLocal()") ==
        std::string::npos);
  CHECK(inferStructReturnHelpersSource.find(
            "keyValueHelperSurfaceMetadataForInferStructReturn()") !=
        std::string::npos);
  CHECK(lowererStructReturnPathSource.find("const std::string mapPrefix = \"map/\"") ==
        std::string::npos);
  CHECK(lowererStructReturnPathSource.find("const std::string stdMapPrefix = \"std/collections/map/\"") ==
        std::string::npos);
  CHECK(lowererStructReturnPathSource.find("collectionMemberPath(\"map\", methodName)") !=
        std::string::npos);
  CHECK(lowererStructReturnPathSource.find("return {\"/std/collections/map/\" + methodName}") ==
        std::string::npos);
  CHECK(inferMethodResolutionSource.find("joinMethodTarget(\"/std/collections/map\", helperSuffix)") ==
        std::string::npos);
  CHECK(inferMethodResolutionSource.find("joinMethodTarget(\"/map\", helperSuffix)") ==
        std::string::npos);
  CHECK(inferMethodResolutionSource.find("normalizedMethodName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(inferMethodResolutionSource.find("receiverHelperName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(inferMethodResolutionSource.find("receiverHelperName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(inferMethodResolutionSource.find(
            "metadataBackedMapHelperMethodName(normalizedMethodName)") !=
        std::string::npos);
  CHECK(inferMethodResolutionSource.find(
            "metadataBackedMapHelperMethodName(receiverHelperName)") !=
        std::string::npos);
  CHECK(inferMethodResolutionSource.find("const std::string mapHelperName") ==
        std::string::npos);
  CHECK(inferMethodResolutionSource.find("isCanonicalMapAccessMethodName") ==
        std::string::npos);
  CHECK(inferMethodResolutionSource.find("const std::string keyValueHelperName") !=
        std::string::npos);
  CHECK(inferMethodResolutionSource.find("isCanonicalKeyValueAccessMethodName") !=
        std::string::npos);
  CHECK(inferMethodResolutionHelpersSource.find("const std::string alias = \"/map/\" + selectedHelperName") ==
        std::string::npos);
  CHECK(inferMethodResolutionHelpersSource.find("\"/std/collections/map/\" + selectedHelperName") ==
        std::string::npos);
  CHECK(inferMethodResolutionHelpersSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_helpers\")") !=
        std::string::npos);
  CHECK(inferMethodResolutionHelpersSource.find(
            "mapHelperSurfaceMetadataForInferMethodResolution") ==
        std::string::npos);
  CHECK(inferMethodResolutionHelpersSource.find(
            "canonicalMapHelperPathForInferMethodResolution") ==
        std::string::npos);
  CHECK(inferMethodResolutionHelpersSource.find(
            "keyValueHelperSurfaceMetadataForInferMethodResolution") !=
        std::string::npos);
  CHECK(inferMethodResolutionHelpersSource.find(
            "canonicalKeyValueHelperPathForInferMethodResolution") !=
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find("std/collections/map/") ==
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find(
            "directRemovedKeyValueCompatibilityPath == \"/map/") ==
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find("isUnrootedMapHelperSurfacePath") ==
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find(
            "canonicalExperimentalMapHelperResolved") == std::string::npos);
  CHECK(inferPreDispatchCallsSource.find(
            "rewrittenCanonicalExperimentalMapHelperCall") ==
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find(
            "borrowedExplicitCanonicalExperimentalMapHelperPath") ==
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find(
            "tryRewriteCanonicalExperimentalMapHelperCall") ==
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find(
            "explicitCanonicalExperimentalMapBorrowedHelperPath") ==
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find(
            "canonicalizeExperimentalMapHelperResolvedPath") ==
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find(
            "shouldLogicalCanonicalizeDefinedExperimentalMapHelperPath") ==
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find("directRemovedMapCompatibilityPath") ==
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find("directMapHelperCompatibilityPath") ==
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find("isRemovedMapAccessCompatibilityPath") ==
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find(
            "isMapNamespacedAccessCompatibilityCall") == std::string::npos);
  CHECK(inferPreDispatchCallsSource.find("isUnrootedKeyValueHelperSurfacePath") !=
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find(
            "canonicalExperimentalKeyValueHelperResolved") !=
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find(
            "rewrittenCanonicalExperimentalKeyValueHelperCall") !=
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find(
            "borrowedExplicitCanonicalExperimentalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find(
            "tryRewriteCanonicalExperimentalKeyValueHelperCall") !=
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find(
            "explicitCanonicalExperimentalKeyValueBorrowedHelperPath") !=
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find(
            "canonicalizeExperimentalKeyValueHelperResolvedPath") !=
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find(
            "shouldLogicalCanonicalizeDefinedExperimentalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find(
            "directRemovedKeyValueCompatibilityPath") != std::string::npos);
  CHECK(inferPreDispatchCallsSource.find("directKeyValueHelperCompatibilityPath") !=
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find(
            "isRemovedKeyValueAccessCompatibilityPath") !=
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find(
            "isKeyValueNamespacedAccessCompatibilityCall") !=
        std::string::npos);
  CHECK(inferPreDispatchCallsSource.find("metadataBackedMapHelperRootAliasMethodName") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("std/collections/map/") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("\"/map/\" +") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_helpers\")") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "resolvePreDispatchKeyValueHelperMemberToken(") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "resolvePreDispatchMapHelperMemberToken(") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("bareKeyValueWrapperHelperPath") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("bareMapWrapperHelperPath") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "normalizedBareKeyValueWrapperHelperPath") != std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "normalizedBareMapWrapperHelperPath") == std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "resolvePreDispatchKeyValueHelperResolvedPath(") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "resolvePreDispatchMapHelperResolvedPath(") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("removedMapCompatibilityHelperFromPath") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("removedMapCompatibilityHelper") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("isRemovedMapCompatibilityPreDispatchHelperName(") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("hasExactRemovedMapAliasDefinition") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("preDispatchKeyValueHelperSurfaceMetadata()") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("preDispatchMapHelperSurfaceMetadata()") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("canonicalKeyValueHelperPathLocal(") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("canonicalMapHelperPathLocal(") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "resolveCanonicalKeyValueHelperNameFromSpelling(") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "resolveCanonicalMapHelperNameFromSpelling(") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("removedKeyValueCompatibilityHelperFromPath") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("removedKeyValueCompatibilityHelper") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("isRemovedKeyValueCompatibilityPreDispatchHelperName(") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("hasExactRemovedKeyValueAliasDefinition") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("sourceMethodKeyValueHelperName") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("sourceMethodMapHelperName") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("isKeyValueReceiver") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("const bool isMapReceiver") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("isExperimentalKeyValueReceiver") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("const bool isExperimentalMapReceiver") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("isExperimentalKeyValueReceiverExpr") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("isExperimentalMapReceiverExpr") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "isCanonicalMapAccessReturnStructHelperName") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "isSourceSpelledCanonicalMapAccessCall") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("canonicalMapAccessDiagnostic") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "hasVisibleStdlibMapAccessDefinition") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("isBareMapAccessBuiltinSurface") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("hasBareKeyValueOperands") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("hasBareMapOperands") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "isCanonicalKeyValueAccessReturnStructHelperName") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "isSourceSpelledCanonicalKeyValueAccessCall") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("canonicalKeyValueAccessDiagnostic") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "hasVisibleStdlibKeyValueAccessDefinition") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("isBareKeyValueAccessBuiltinSurface") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "canonicalExperimentalMapHelperResolved") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "rewrittenCanonicalExperimentalMapHelperCall") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "borrowedCanonicalExperimentalMapHelperPath") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "tryRewriteCanonicalExperimentalMapHelperCall") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "explicitCanonicalExperimentalMapBorrowedHelperPath") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "canonicalizeExperimentalMapHelperResolvedPath") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "shouldLogicalCanonicalizeDefinedExperimentalMapHelperPath") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "canonicalExperimentalKeyValueHelperResolved") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "rewrittenCanonicalExperimentalKeyValueHelperCall") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "borrowedCanonicalExperimentalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "tryRewriteCanonicalExperimentalKeyValueHelperCall") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "explicitCanonicalExperimentalKeyValueBorrowedHelperPath") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "canonicalizeExperimentalKeyValueHelperResolvedPath") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find(
            "shouldLogicalCanonicalizeDefinedExperimentalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("rootedMapAliasHelperPath(") ==
        std::string::npos);
  CHECK(exprPreDispatchDirectCallsSource.find("rootedKeyValueAliasHelperPath(") !=
        std::string::npos);
  CHECK(exprVectorHelpersSource.find("std/collections/map/") ==
        std::string::npos);
  CHECK(exprVectorHelpersSource.find("normalizedHelperName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(exprVectorHelpersSource.find("mapHelperName") == std::string::npos);
  CHECK(exprVectorHelpersSource.find("preferredBareMapHelperTarget") ==
        std::string::npos);
  CHECK(exprVectorHelpersSource.find("keyValueHelperName") !=
        std::string::npos);
  CHECK(exprVectorHelpersSource.find("preferredBareKeyValueHelperTarget") !=
        std::string::npos);
  CHECK(exprVectorHelpersSource.find(
            "metadataBackedMapHelperMethodName(normalizedHelperName)") !=
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("const std::string alias = \"/map/\" + std::string(helperName)") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("\"/std/collections/map/\" + std::string(helperName)") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("\"/map/\" + std::string(helperName)") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("collectionRewriteMapHelperMetadata()") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("canonicalMapHelperPathForRewrite(") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("resolveMapHelperMemberTokenForRewrite(") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("resolveExplicitMapHelperPathForRewrite(") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("preferredMapHelperLoweringPathForRewrite(") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("directExperimentalMapHelperSpelling") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("canonicalExperimentalMapHelperPath") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("preferredBareMapHelperTarget") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("isBareMapAccessHelperName(") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("specializedExperimentalMapHelperTarget") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("mapHelperReceiverIndex") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("bareMapHelperOperandIndices") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("tryRewriteBareMapHelperCall") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("hasResolvableMapHelperPath") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find(
            "tryRewriteCanonicalExperimentalMapHelperCall") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find(
            "explicitCanonicalExperimentalMapBorrowedHelperPath") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("hasVisibleMapHelperFamily") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("collectionRewriteKeyValueHelperMetadata()") !=
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("canonicalKeyValueHelperPathForRewrite(") !=
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("resolveKeyValueHelperMemberTokenForRewrite(") !=
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("resolveExplicitKeyValueHelperPathForRewrite(") !=
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("preferredKeyValueHelperLoweringPathForRewrite(") !=
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("directExperimentalKeyValueHelperSpelling") !=
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("canonicalExperimentalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("preferredBareKeyValueHelperTarget") !=
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("isBareKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find(
            "specializedExperimentalKeyValueHelperTarget") !=
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("keyValueHelperReceiverIndex") !=
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("bareKeyValueHelperOperandIndices") !=
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("tryRewriteBareKeyValueHelperCall") !=
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("hasResolvableKeyValueHelperPath") !=
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find(
            "tryRewriteCanonicalExperimentalKeyValueHelperCall") !=
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find(
            "explicitCanonicalExperimentalKeyValueBorrowedHelperPath") !=
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("hasVisibleKeyValueHelperFamily") !=
        std::string::npos);
  CHECK(effectFreeCollectionsSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(effectFreeCollectionsSource.find(
            "mapHelperSurfaceMetadataForEffectFreeCollections(") ==
        std::string::npos);
  CHECK(effectFreeCollectionsSource.find("canonicalMapHelperPathLocal(") ==
        std::string::npos);
  CHECK(effectFreeCollectionsSource.find("unrootedCanonicalMapHelperPrefixLocal(") ==
        std::string::npos);
  CHECK(effectFreeCollectionsSource.find("stdMapPrefix") ==
        std::string::npos);
  CHECK(effectFreeCollectionsSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_helpers\")") !=
        std::string::npos);
  CHECK(effectFreeCollectionsSource.find(
            "keyValueHelperSurfaceMetadataForEffectFreeCollections(") !=
        std::string::npos);
  CHECK(effectFreeCollectionsSource.find("canonicalKeyValueHelperPathLocal(") !=
        std::string::npos);
  CHECK(effectFreeCollectionsSource.find(
            "unrootedCanonicalKeyValueHelperPrefixLocal(") !=
        std::string::npos);
  CHECK(effectFreeCollectionsSource.find("stdKeyValueHelperPrefix") !=
        std::string::npos);
  CHECK(buildParametersSource.find("normalizedType == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(buildParametersSource.find("isMapCollectionTypeName(normalizedType)") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find("explicitStdKeyValueHelperName") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find("explicitStdMapHelperName") ==
        std::string::npos);
  CHECK(buildCallResolutionSource.find("\"/std/collections/map/\"") ==
        std::string::npos);
  CHECK(buildCallResolutionSource.find("\"/std/collections/map/map\"") ==
        std::string::npos);
  CHECK(buildCallResolutionSource.find("mapHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(buildCallResolutionSource.find("mapConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(buildCallResolutionSource.find("mapHelperCanonicalMemberRootPath") ==
        std::string::npos);
  CHECK(buildCallResolutionSource.find("canonicalMapHelperAliasPath") ==
        std::string::npos);
  CHECK(buildCallResolutionSource.find("mapConstructorMetadata") ==
        std::string::npos);
  CHECK(buildCallResolutionSource.find("isMapHelperNamespacePrefix") ==
        std::string::npos);
  CHECK(buildCallResolutionSource.find("isRemovedMapCompatibilityHelper(") ==
        std::string::npos);
  CHECK(buildCallResolutionSource.find(
            "keyValueHelperCanonicalMemberRootPath") !=
        std::string::npos);
  CHECK(buildCallResolutionSource.find("canonicalKeyValueHelperAliasPath") !=
        std::string::npos);
  CHECK(buildCallResolutionSource.find("keyValueConstructorMetadata") !=
        std::string::npos);
  CHECK(buildCallResolutionSource.find("isKeyValueHelperNamespacePrefix") !=
        std::string::npos);
  CHECK(buildCallResolutionSource.find("isRemovedKeyValueCompatibilityHelper(") !=
        std::string::npos);
  CHECK(buildReturnKindsSource.find("return \"/map\"") == std::string::npos);
  CHECK(buildReturnKindsSource.find(
            "mapCollectionMarkerPathForBuildReturnKinds()") ==
        std::string::npos);
  CHECK(buildReturnKindsSource.find(
            "keyValueCollectionMarkerPathForBuildReturnKinds()") !=
        std::string::npos);
  CHECK(buildReturnKindsSource.find(
            "keyValueConstructorSurfaceMetadataForBuildReturnKinds()") !=
        std::string::npos);
  CHECK(buildReturnKindsSource.find("mapConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find("const std::string alias = \"/map/\" + helperName") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find("normalizedPrefix == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find("normalizedName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find("metadataBackedCanonicalMapHelperPath(helperName)") !=
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "isExperimentalCollectionBackingTypeName(\"map\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find("collection == \"map\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find("collectionName == \"map\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "resolveCallCollectionTemplateArgs(*initializerExprForInference, \"map\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find(
            "isQualifiedExperimentalMapBackingTypeName(") !=
        std::string::npos);
  CHECK(buildInitializerInferenceCallsSource.find("mapCollectionAliasToken()") !=
        std::string::npos);
  CHECK(inferCollectionDispatchSource.find("resolvedPath.rfind(\"/map/\"") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSource.find("resolvedPath == \"/map/at\"") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSource.find("resolvedPath == \"/map/contains\"") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSource.find(
            "mapHelperSurfaceMetadataForInferCollectionDispatch(") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSource.find(
            "resolveMapHelperResolvedPathForInferCollectionDispatch(") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSource.find("resolvedMapHelperName") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_helpers\")") !=
        std::string::npos);
  CHECK(inferCollectionDispatchSource.find(
            "keyValueHelperSurfaceMetadataForInferCollectionDispatch(") !=
        std::string::npos);
  CHECK(inferCollectionDispatchSource.find(
            "resolveKeyValueHelperResolvedPathForInferCollectionDispatch(") !=
        std::string::npos);
  CHECK(inferCollectionDispatchSource.find("resolvedKeyValueHelperName") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("const std::string alias = \"/map/\" + std::string(helperName)") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("base == \"/std/collections/map\"") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("normalizedType == \"/std/collections/map\"") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("importPath == \"/std/collections/map\"") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("normalizedName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("explicitPath.rfind(\"/map/\", 0)") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("const std::string removedPath = \"/map/\" + helperName") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("canonicalMapHelperRootPathLocal(") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("canonicalMapHelperPathLocal(") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("resolvePublishedMapHelperMemberTokenLocal(") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("resolvePublishedMapHelperResolvedPathLocal(") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("isCanonicalMapHelperResolvedPathLocal(") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("resolvedBareMapHelper") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("hasKeyValueReceiver") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("hasMapReceiver") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("isWrappedKeyValueReceiverCall") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("isWrappedMapReceiverCall") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("preferredRemovedMapHelperPath") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("mapNamespacedMethodCompatibilityPath") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("directMapHelperCompatibilityPath") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("preferredExperimentalMapHelperTarget") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find(
            "preferredCanonicalExperimentalMapHelperTarget") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("canonicalExperimentalMapHelperPath") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find(
            "canonicalizeExperimentalMapHelperResolvedPath") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find(
            "shouldLogicalCanonicalizeDefinedExperimentalMapHelperPath") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("canonicalKeyValueHelperRootPathLocal(") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("canonicalKeyValueHelperPathLocal(") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("resolvePublishedKeyValueHelperMemberTokenLocal(") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("resolvePublishedKeyValueHelperResolvedPathLocal(") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("isCanonicalKeyValueHelperResolvedPathLocal(") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("resolvedBareKeyValueHelper") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("preferredRemovedKeyValueHelperPath") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find(
            "keyValueNamespacedMethodCompatibilityPath") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("directKeyValueHelperCompatibilityPath") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find(
            "preferredExperimentalKeyValueHelperTarget") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find(
            "preferredCanonicalExperimentalKeyValueHelperTarget") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("canonicalExperimentalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find(
            "canonicalizeExperimentalKeyValueHelperResolvedPath") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find(
            "shouldLogicalCanonicalizeDefinedExperimentalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("isCanonicalMapCollectionTypeRootLocal(") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("rootedMapCompatibilityHelperPath(") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("rootedKeyValueCompatibilityHelperPath(helperName)") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("metadataBackedMapHelperRootAliasMethodName(explicitPath)") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("namespacePrefix == \"map\"") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("rawMethodName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("use /std/collections/map/*") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find(
            "isPublishedMapBaseHelperName(") == std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find(
            "isPublishedBorrowedMapHelperName(") == std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find(
            "isMapHelperImportAliasNamespace(") == std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find(
            "resolveCanonicalCompatibilityMapHelperNameFromResolvedPath(") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find(
            "resolveExplicitPublishedMapHelperExprMemberName(") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find(
            "isDirectWrapperMapTarget") == std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find(
            "isPublishedKeyValueBaseHelperName(") != std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find(
            "isPublishedBorrowedKeyValueHelperName(") != std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find(
            "isKeyValueHelperImportAliasNamespace(") != std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find(
            "resolveCanonicalCompatibilityKeyValueHelperNameFromResolvedPath(") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find(
            "resolveExplicitPublishedKeyValueHelperExprMemberName(") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find(
            "isDirectWrapperKeyValueTarget") != std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("resolveMapCompatibilityMemberToken(") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("resolveMapCompatibilityResolvedPath(") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("resolveMapCompatibilityUnrootedPath(") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("rootedMapCompatibilityHelperPath(") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("canonicalMapCompatibilityPrefixOrFallback(") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("unrootedCanonicalMapCompatibilityPrefixOrFallback(") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("isCanonicalMapCompatibilityNamespace(") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("legacyExperimentalMapCompatibilityPrefix(") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("resolveKeyValueCompatibilityMemberToken(") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("resolveKeyValueCompatibilityResolvedPath(") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("resolveKeyValueCompatibilityUnrootedPath(") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("rootedKeyValueCompatibilityHelperPath(") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("canonicalKeyValueCompatibilityPrefixOrFallback(") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("unrootedCanonicalKeyValueCompatibilityPrefixOrFallback(") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("isCanonicalKeyValueCompatibilityNamespace(") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("legacyExperimentalKeyValueCompatibilityPrefix(") !=
        std::string::npos);
  CHECK(inferCollectionCompatibilityInternalSource.find("mapHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("resolvedPath == \"/map/at_ref\"") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("hasDefinitionPath(\"/map/\" +") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("namespacePrefix == \"map\"") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("expr.namespacePrefix == \"map\"") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_helpers\")") !=
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find(
            "keyValueHelperSurfaceMetadataForDispatchSetup()") !=
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find(
            "dispatchSetupMapHelperSurfaceMetadata()") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find(
            "resolveRootKeyValueHelperAliasPath(path, helperName)") !=
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("resolveRootMapHelperAliasPath(") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("resolveDispatchSetupMapHelperPath(") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("directMapHelperCompatibilityPath") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find(
            "resolveDispatchSetupKeyValueHelperPath(resolvedPath,") !=
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("directKeyValueHelperCompatibilityPath") !=
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find(
            "isCanonicalKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("isCanonicalMapAccessHelperName(") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("canonicalKeyValueHelperNamespace()") !=
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("canonicalMapHelperNamespace()") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("isNamespacedKeyValueHelperCall") !=
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("isNamespacedMapHelperCall") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find(
            "isStdNamespacedKeyValueAccessSpelling") != std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("isStdNamespacedMapAccessSpelling") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find(
            "hasStdNamespacedKeyValueAccessDefinition") != std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("hasStdNamespacedMapAccessDefinition") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find(
            "shouldInferBuiltinBareKeyValueContainsCall") != std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find(
            "shouldInferBuiltinBareMapContainsCall") == std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find(
            "shouldInferBuiltinBareKeyValueTryAtCall") != std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("shouldInferBuiltinBareMapTryAtCall") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find(
            "shouldInferBuiltinBareKeyValueAccessCall") != std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("shouldInferBuiltinBareMapAccessCall") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find(
            "shouldDeferNamespacedKeyValueAccessCall") != std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("shouldDeferNamespacedMapAccessCall") ==
        std::string::npos);
  CHECK(inferCollectionsSource.find(
            "isExperimentalCollectionBackingTypeName(\"map\"") ==
        std::string::npos);
  CHECK(inferCollectionsSource.find(
            "isUnspecializedExperimentalMapBackingTypeName(base)") !=
        std::string::npos);
  CHECK(inferCollectionsSource.find(
            "isQualifiedExperimentalMapBackingTypeName(normalizedResolvedPath)") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "\"/std/collections/map/\" + builtinAccessName") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "isExperimentalCollectionBackingTypeName(\"map\"") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find("collection == \"map\"") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find("currentTypeTextOut = \"map<\"") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "isUnspecializedExperimentalMapBackingTypeName(base)") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "isQualifiedExperimentalMapBackingTypeName(") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find("mapCollectionAliasToken()") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "metadataBackedCanonicalMapHelperPath(builtinAccessName)") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find("const bool isKeyValueReceiver") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find("const bool isMapReceiver") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "const bool isExperimentalKeyValueReceiver") != std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "const bool isExperimentalMapReceiver") == std::string::npos);
  CHECK(inferCollectionBufferAndMapResolversSource.find(
            "hasVisibleDefinitionPathForCurrentImports(\"/std/collections/map/map\")") ==
        std::string::npos);
  CHECK(inferCollectionBufferAndMapResolversSource.find(
            "mapConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(inferCollectionBufferAndMapResolversSource.find(
            "std::string(mapConstructorMetadata->canonicalPath)") !=
        std::string::npos);
  CHECK(inferCollectionBufferAndMapResolversSource.find("mapCollectionAliasToken()") !=
        std::string::npos);
  CHECK(inferCollectionBufferAndMapResolversSource.find("isRootMapConstructorAliasPath(") !=
        std::string::npos);
  CHECK(inferCollectionBufferAndMapResolversSource.find("path == \"/map\"") ==
        std::string::npos);
  CHECK(inferCollectionBufferAndMapResolversSource.find("path.rfind(\"/map__\", 0)") ==
        std::string::npos);
  CHECK(inferCollectionBufferAndMapResolversSource.find("defMap_.find(\"/map\")") ==
        std::string::npos);
  CHECK(inferCollectionBufferAndMapResolversSource.find(
            "resolveCallCollectionTemplateArgs(target, \"map\"") ==
        std::string::npos);
  CHECK(inferCollectionBufferAndMapResolversSource.find("builtinCollectionName == \"map\"") ==
        std::string::npos);
  CHECK(inferCollectionBufferAndMapResolversSource.find("collectionTypePath == \"/map\"") ==
        std::string::npos);
  CHECK(inferDefinitionSource.find("resolvedPath == \"/map/at\"") ==
        std::string::npos);
  CHECK(inferDefinitionSource.find("resolveCalleePath(candidate) == \"/std/collections/map/at_ref\"") ==
        std::string::npos);
  CHECK(inferDefinitionSource.find("resolvedPath == \"/std/collections/map/at\"") ==
        std::string::npos);
  CHECK(inferDefinitionSource.find(
            "isInferDefinitionCanonicalMapAccessHelperPath(resolvedCandidatePath)") ==
        std::string::npos);
  CHECK(inferDefinitionSource.find(
            "isInferDefinitionCanonicalKeyValueAccessHelperPath(resolvedCandidatePath)") !=
        std::string::npos);
  CHECK(inferDefinitionSource.find(
            "metadataBackedMapHelperMethodName(resolvedCandidatePath)") !=
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find("path == \"/map/at\"") ==
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find("path == \"/map/at_ref\"") ==
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find("path == \"/map/at_unsafe\"") ==
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find("path == \"/map/at_unsafe_ref\"") ==
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "lateFallbackMapHelperSurfaceMetadata(") ==
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "resolveLateFallbackMapHelperName(") ==
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "lateFallbackCanonicalMapHelperPath(") ==
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "resolveLateFallbackCanonicalMapHelperName(") ==
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "isCanonicalMapContainsHelperPath(") ==
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "isCanonicalMapTryAtHelperPath(") ==
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "isCanonicalMapAccessHelperPath(") ==
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "isLateFallbackMapAccessHelperName(") ==
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "isCanonicalMapAccessHelperName(") ==
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "isMapImportAliasAccessHelperPath(") ==
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find("rewrittenMapHelperCall") ==
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find("rewrittenMapAccessCall") ==
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "lateFallbackKeyValueHelperSurfaceMetadata(") !=
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "resolveLateFallbackKeyValueHelperName(") !=
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "lateFallbackCanonicalKeyValueHelperPath(") !=
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "resolveLateFallbackCanonicalKeyValueHelperName(") !=
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "isCanonicalKeyValueContainsHelperPath(") !=
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "isCanonicalKeyValueTryAtHelperPath(") !=
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "isCanonicalKeyValueAccessHelperPath(") !=
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "isLateFallbackKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "isCanonicalKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "isKeyValueImportAliasAccessHelperPath(methodResolved)") !=
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find("rewrittenKeyValueHelperCall") !=
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find("hasBareKeyValueOperands") !=
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find("hasBareMapOperands") ==
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find("keyValueReceiverIndex") !=
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find("mapReceiverIndex") ==
        std::string::npos);
  CHECK(inferLateFallbackBuiltinsSource.find(
            "metadata->importAliasSpellings") !=
        std::string::npos);
  CHECK(exprLateFallbackBuiltinsSource.find("rewrittenMapAccessCall") ==
        std::string::npos);
  CHECK(exprLateFallbackBuiltinsSource.find("rewrittenKeyValueAccessCall") !=
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("normalizedName == \"map/at_ref\"") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("namespacePrefix == \"map\"") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("methodResolved = \"/map/\" + helperName") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("canonicalMapHelperPathLocal(") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find(
            "resolveCanonicalMapHelperNameFromSpelling(") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("isCanonicalMapHelperResolvedPath(") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("isCanonicalMapAccessHelperPath(") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("getCanonicalMapAccessBuiltinName(") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find(
            "isSourceSpelledCanonicalMapAccessCall(") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("canonicalMapAccessDiagnostic") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("failLateMapAccessBuiltinDiagnostic") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("failLateMapAccessKeyMismatch") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("hasBareMapContainsBuiltinDefinition") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("hasVisibleStdlibMapBuiltinDefinition") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("hasBareMapOperands") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("rewrittenMapAccessCall") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("rewrittenMapHelperCall") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_helpers\")") !=
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("canonicalKeyValueHelperPathLocal(") !=
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find(
            "resolveCanonicalKeyValueHelperNameFromSpelling(") !=
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find(
            "isCanonicalKeyValueHelperResolvedPath(") !=
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find(
            "isCanonicalKeyValueAccessHelperPath(") !=
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find(
            "getCanonicalKeyValueAccessBuiltinName(") !=
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find(
            "isSourceSpelledCanonicalKeyValueAccessCall(") !=
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("canonicalKeyValueAccessDiagnostic") !=
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("failLateKeyValueAccessDiagnostic") !=
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("failLateKeyValueAccessKeyMismatch") !=
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("hasBareKeyValueContainsDefinition") !=
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find(
            "hasVisibleStdlibKeyValueAccessDefinition") !=
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("hasBareKeyValueOperands") !=
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("isExperimentalKeyValueReceiver") !=
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("isExperimentalMapReceiver") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("rewrittenKeyValueAccessCall") !=
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("rewrittenKeyValueHelperCall") !=
        std::string::npos);
  CHECK(exprTrySource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(exprTrySource.find("getDirectMapHelperCompatibilityPath") ==
        std::string::npos);
  CHECK(exprTrySource.find("removedMapCompatibilityPath") ==
        std::string::npos);
  CHECK(exprTrySource.find("getDirectKeyValueHelperCompatibilityPath") !=
        std::string::npos);
  CHECK(exprTrySource.find("removedKeyValueCompatibilityPath") !=
        std::string::npos);
  CHECK(exprTrySource.find("isBareKeyValueTryAtFallback") !=
        std::string::npos);
  CHECK(exprTrySource.find("isBareMapTryAtFallback") == std::string::npos);
  CHECK(exprTrySource.find("metadataBackedCanonicalMapHelperPath(\"tryAt\")") !=
        std::string::npos);
  CHECK(exprTrySource.find(
            "metadataBackedCanonicalMapHelperPath(\"tryAt_ref\")") !=
        std::string::npos);
  CHECK(mapSoaBuiltinsSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(mapSoaBuiltinsSource.find(
            "metadataBackedCanonicalMapHelperPath(helperName)") !=
        std::string::npos);
  CHECK(mapSoaBuiltinsSource.find("mapHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(mapSoaBuiltinsSource.find("canonicalMapHelperPathLocal(") ==
        std::string::npos);
  CHECK(mapSoaBuiltinsSource.find("isCanonicalMapHelperResolvedPath(") ==
        std::string::npos);
  CHECK(mapSoaBuiltinsSource.find("hasBareKeyValueOperands") !=
        std::string::npos);
  CHECK(mapSoaBuiltinsSource.find("hasBareMapOperands") == std::string::npos);
  CHECK(mapSoaBuiltinsSource.find("canonicalKeyValueHelperPathLocal(") !=
        std::string::npos);
  CHECK(mapSoaBuiltinsSource.find("isCanonicalKeyValueHelperResolvedPath(") !=
        std::string::npos);
  CHECK(pointerLikeSource.find("appendUnique(\"/std/collections/map/\" + suffix)") ==
        std::string::npos);
  CHECK(pointerLikeSource.find("appendUnique(\"/map/\" + suffix)") ==
        std::string::npos);
  CHECK(pointerLikeSource.find("const std::string mapPrefix = \"map/\"") ==
        std::string::npos);
  CHECK(pointerLikeSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(pointerLikeSource.find("mapHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(pointerLikeSource.find("unrootedMapImportAliasHelperPrefix()") ==
        std::string::npos);
  CHECK(pointerLikeSource.find("unrootedCanonicalMapHelperPrefix()") ==
        std::string::npos);
  CHECK(pointerLikeSource.find("const std::string mapPrefix") ==
        std::string::npos);
  CHECK(pointerLikeSource.find("const std::string stdMapPrefix") ==
        std::string::npos);
  CHECK(pointerLikeSource.find("unrootedKeyValueImportAliasHelperPrefix()") !=
        std::string::npos);
  CHECK(pointerLikeSource.find("unrootedCanonicalKeyValueHelperPrefix()") !=
        std::string::npos);
  CHECK(pointerLikeSource.find("keyValueAliasPrefix") !=
        std::string::npos);
  CHECK(pointerLikeSource.find("canonicalKeyValuePrefix") !=
        std::string::npos);
  CHECK(statementPrintabilitySource.find("resolvedPath == \"/map/at\"") ==
        std::string::npos);
  CHECK(statementPrintabilitySource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(statementPrintabilitySource.find("mapHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(statementPrintabilitySource.find("isCanonicalMapHelperResolvedPath(") ==
        std::string::npos);
  CHECK(statementPrintabilitySource.find("isCanonicalKeyValueHelperResolvedPath(") !=
        std::string::npos);
  CHECK(statementBodyArgumentsSource.find("isRemovedMapCompatibilityHelper(") ==
        std::string::npos);
  CHECK(statementBodyArgumentsSource.find("mapHelperSurfaceMetadata(") ==
        std::string::npos);
  CHECK(statementBodyArgumentsSource.find("resolveMapHelperPathMemberName(") ==
        std::string::npos);
  CHECK(statementBodyArgumentsSource.find("canonicalMapHelperPath(") ==
        std::string::npos);
  CHECK(statementBodyArgumentsSource.find("legacyMapHelperPath(") ==
        std::string::npos);
  CHECK(statementBodyArgumentsSource.find("preferredMapBodyArgumentTarget") ==
        std::string::npos);
  CHECK(statementBodyArgumentsSource.find("isMapReceiverExpr") ==
        std::string::npos);
  CHECK(statementBodyArgumentsSource.find(
            "isRemovedKeyValueCompatibilityHelper(") !=
        std::string::npos);
  CHECK(statementBodyArgumentsSource.find(
            "keyValueHelperSurfaceMetadataForBodyArguments(") !=
        std::string::npos);
  CHECK(statementBodyArgumentsSource.find(
            "resolveKeyValueHelperPathMemberName(") !=
        std::string::npos);
  CHECK(statementBodyArgumentsSource.find(
            "canonicalKeyValueHelperPathForBodyArguments(") !=
        std::string::npos);
  CHECK(statementBodyArgumentsSource.find("legacyKeyValueHelperAliasPath(") !=
        std::string::npos);
  CHECK(statementBodyArgumentsSource.find("preferredKeyValueBodyArgumentTarget") !=
        std::string::npos);
  CHECK(statementBodyArgumentsSource.find("isKeyValueReceiverExpr") !=
        std::string::npos);
  CHECK(scalarPointerMemorySource.find("isExplicitMapAccessHelperPath") ==
        std::string::npos);
  CHECK(scalarPointerMemorySource.find("resolved == \"/map/at\"") ==
        std::string::npos);
  CHECK(scalarPointerMemorySource.find("resolved == \"/std/collections/map/at_ref\"") ==
        std::string::npos);
  CHECK(scalarPointerMemorySource.find("collectionName == \"map\"") ==
        std::string::npos);
  CHECK(scalarPointerMemorySource.find("isMapCollectionTypeName(collectionName)") !=
        std::string::npos);
  CHECK(statementSource.find(
            "isExperimentalCollectionBackingTypeName(\"map\", \"Map\", typeName)") ==
        std::string::npos);
  CHECK(statementSource.find("base == \"map\"") == std::string::npos);
  CHECK(statementSource.find("isQualifiedExperimentalMapBackingTypeName(typeName)") !=
        std::string::npos);
  CHECK(statementSource.find("mapCollectionAliasTokenForStatementValidation()") !=
        std::string::npos);
  CHECK(statementSource.find("base == mapCollectionAlias") !=
        std::string::npos);
  CHECK(argumentValidationSource.find("normalizedBase == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(argumentValidationSource.find("diagnosticResolved != \"/std/collections/map/at\"") ==
        std::string::npos);
  CHECK(argumentValidationSource.find("normalizedExpectedBase == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(argumentValidationSource.find(
            "resolveCanonicalArgumentValidationMapAccessHelper(") ==
        std::string::npos);
  CHECK(argumentValidationSource.find("canonicalMapAccessHelperName") ==
        std::string::npos);
  CHECK(argumentValidationSource.find(
            "resolveCanonicalArgumentValidationKeyValueAccessHelper(") !=
        std::string::npos);
  CHECK(argumentValidationSource.find("canonicalKeyValueAccessHelperName") !=
        std::string::npos);
  CHECK(argumentValidationSource.find("mapHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find("normalizedName == \"map/at_ref\"") ==
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find("resolvedBasePath == \"/map/at\"") ==
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find(
            "mapHelperSurfaceMetadataForArgumentValidation(") ==
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find("canonicalMapHelperPathLocal(") ==
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find(
            "resolveCanonicalMapHelperNameFromSpelling(") ==
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find(
            "isCanonicalMapAccessResolvedPath(") ==
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find(
            "getCanonicalMapAccessBuiltinName(") ==
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find("resolvedMapHelperName") ==
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find(
            "methodMapAccessDefinitionReturnsString") ==
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find("isExplicitMapAccessPath") ==
        std::string::npos);
  CHECK(namedArgumentBuiltinsSource.find("isCanonicalMapAccessHelperResolvedPath(") ==
        std::string::npos);
  CHECK(namedArgumentBuiltinsSource.find(
            "isCanonicalKeyValueAccessHelperResolvedPath(") !=
        std::string::npos);
  CHECK(passesDiagnosticsSource.find("isCanonicalMapAccessHelperPath(") ==
        std::string::npos);
  CHECK(passesDiagnosticsSource.find("isVisibleCanonicalMapAccessBuiltin") ==
        std::string::npos);
  CHECK(passesDiagnosticsSource.find("isCanonicalKeyValueAccessHelperPath(") !=
        std::string::npos);
  CHECK(passesDiagnosticsSource.find("isVisibleCanonicalKeyValueAccessBuiltin") !=
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_helpers\")") !=
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find(
            "keyValueHelperSurfaceMetadataForArgumentValidation(") !=
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find(
            "canonicalKeyValueHelperPathForArgumentValidation(") !=
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find(
            "resolveCanonicalKeyValueHelperNameFromSpelling(") !=
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find(
            "isCanonicalKeyValueAccessResolvedPath(") !=
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find(
            "getCanonicalKeyValueAccessBuiltinName(") !=
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find("resolvedKeyValueHelperName") !=
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find(
            "methodKeyValueAccessDefinitionReturnsString") !=
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find("isExplicitKeyValueAccessPath") !=
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("path == \"/map/at\"") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("normalizedName == \"map/at_ref\"") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("namespacePrefix == \"map\"") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("diagnosticTarget.rfind(\"/map/\"") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("metadataBackedCanonicalMapHelperPath(") !=
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("isCanonicalMapAccessHelperName(") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("isSourceSpelledCanonicalMapAccessCall(") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find(
            "isExplicitCanonicalMapAccessCall") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("validateMethodMapAccessBuiltin") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("isExplicitMapAccessHelper") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("isCanonicalKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(collectionAccessValidationSource.find(
            "isSourceSpelledCanonicalKeyValueAccessCall(") !=
        std::string::npos);
  CHECK(exprLateUnknownTargetFallbacksSource.find("rewrittenKeyValueMethodCall") !=
        std::string::npos);
  CHECK(exprLateUnknownTargetFallbacksSource.find("rewrittenMapMethodCall") ==
        std::string::npos);
  CHECK(exprLateUnknownTargetFallbacksSource.find("const bool isKeyValueReceiver") !=
        std::string::npos);
  CHECK(exprLateUnknownTargetFallbacksSource.find("const bool isMapReceiver") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find(
            "isExplicitCanonicalKeyValueAccessCall") !=
        std::string::npos);
  CHECK(collectionAccessValidationSource.find(
            "validateMethodKeyValueAccessBuiltin") !=
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("isExplicitKeyValueAccessHelper") !=
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("canonicalMapHelperPathLocal(") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("canonicalMapHelperNamespaceLocal(") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("resolveCanonicalMapHelperNameFromSpelling(") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("rootedMapHelperAliasPathLocal(") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("resolvedMapHelperName") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("rewrittenMapHelperCall") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("canonicalKeyValueHelperPathLocal(") !=
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("canonicalKeyValueHelperNamespaceLocal(") !=
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("resolveCanonicalKeyValueHelperNameFromSpelling(") !=
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("rootedKeyValueHelperAliasPathLocal(") !=
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("resolvedKeyValueHelperName") !=
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("rewrittenKeyValueHelperCall") !=
        std::string::npos);
  CHECK(collectionDispatchSetupSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(collectionDispatchSetupSource.find("isCanonicalMapAccessHelperName(") ==
        std::string::npos);
  CHECK(collectionDispatchSetupSource.find(
            "isStdNamespacedCanonicalMapAccessPath(") ==
        std::string::npos);
  CHECK(collectionDispatchSetupSource.find("mapHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(collectionDispatchSetupSource.find(
            "isCanonicalKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(collectionDispatchSetupSource.find(
            "isStdNamespacedCanonicalKeyValueAccessPath(") !=
        std::string::npos);
  CHECK(collectionAccessSource.find("resolvedPath == \"/map/at_ref\" ||\n"
                                    "            resolvedPath == \"/std/collections/map/at_ref\"") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("normalizedName.rfind(\"map/\", 0) == 0) {\n"
                                    "          const std::string helperName =") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("expr.namespacePrefix == \"map\"") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("defMap_.find(\"/map/\" + accessHelperName)") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("resolved = \"/map/\" +") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("path == \"/map/at\"") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("methodResolved == \"/map/") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("std::string(\"/map/\")") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("auto isMapNamespacedAccessCompatibilityCall") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("isMapCanonicalAccessPath(") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("collectionAccessMapHelperMetadata()") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("canonicalMapHelperPathLocal(") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("canonicalMapHelperNamespaceLocal(") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("resolveCanonicalMapHelperNameFromSpelling(") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("resolvedMapHelperName") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("canonicalMapMethodHelperName") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("resolvedCanonicalMapHelper") ==
        std::string::npos);
  CHECK(collectionAccessSource.find(
            "getCanonicalMapAccessHelperNameForDispatch") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("isNamespacedMapAccessCall") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("explicitCanonicalMapAccessHelperName") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("explicitCanonicalMapAccessCall") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("canonicalMapAccessHelperTarget") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("resolvedCanonicalMapAccessMethod") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("resolvedDeclaredCanonicalMapHelper") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("isCanonicalMapAccessHelperName(") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("canonicalStdlibMapAccessPathForHelper(") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("isCanonicalMapContainsResolvedPath(") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("isCanonicalMapAccessResolvedPath(") ==
        std::string::npos);
  CHECK(collectionAccessSource.find(
            "canonicalStdlibMapContainsPathForResolvedMethod(") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("collectionAccessKeyValueHelperMetadata()") !=
        std::string::npos);
  CHECK(collectionAccessSource.find("canonicalKeyValueHelperPathLocal(") !=
        std::string::npos);
  CHECK(collectionAccessSource.find("canonicalKeyValueHelperNamespaceLocal(") !=
        std::string::npos);
  CHECK(collectionAccessSource.find("resolveCanonicalKeyValueHelperNameFromSpelling(") !=
        std::string::npos);
  CHECK(collectionAccessSource.find("resolvedKeyValueHelperName") !=
        std::string::npos);
  CHECK(collectionAccessSource.find("canonicalKeyValueMethodHelperName") !=
        std::string::npos);
  CHECK(collectionAccessSource.find("resolvedCanonicalKeyValueHelper") !=
        std::string::npos);
  CHECK(collectionAccessSource.find(
            "getCanonicalKeyValueAccessHelperNameForDispatch") !=
        std::string::npos);
  CHECK(collectionAccessSource.find("isNamespacedKeyValueAccessCall") !=
        std::string::npos);
  CHECK(collectionAccessSource.find(
            "explicitCanonicalKeyValueAccessHelperName") !=
        std::string::npos);
  CHECK(collectionAccessSource.find("explicitCanonicalKeyValueAccessCall") !=
        std::string::npos);
  CHECK(collectionAccessSource.find("canonicalKeyValueAccessHelperTarget") !=
        std::string::npos);
  CHECK(collectionAccessSource.find("resolvedCanonicalKeyValueAccessMethod") !=
        std::string::npos);
  CHECK(collectionAccessSource.find("resolvedDeclaredCanonicalKeyValueHelper") !=
        std::string::npos);
  CHECK(collectionAccessSource.find("isCanonicalKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(collectionAccessSource.find(
            "canonicalStdlibKeyValueAccessPathForHelper(") !=
        std::string::npos);
  CHECK(collectionAccessSource.find(
            "isCanonicalKeyValueContainsResolvedPath(") !=
        std::string::npos);
  CHECK(collectionAccessSource.find(
            "isCanonicalKeyValueAccessResolvedPath(") !=
        std::string::npos);
  CHECK(collectionAccessSource.find(
            "canonicalStdlibKeyValueContainsPathForResolvedMethod(") !=
        std::string::npos);
  CHECK(collectionAccessSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_helpers\")") !=
        std::string::npos);
  CHECK(countCapacityMapBuiltinSource.find("/std/collections/map/count") ==
        std::string::npos);
  CHECK(countCapacityMapBuiltinSource.find("/std/collections/map/at") ==
        std::string::npos);
  CHECK(countCapacityMapBuiltinSource.find("canonicalizeExperimentalMapHelperResolvedPath") ==
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("const std::string mapAlias = \"/map/\" + suffix") ==
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("preferred.rfind(\"/map/\", 0) == 0 && nameMap.count(preferred) == 0") ==
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("preferred.rfind(\"map/\", 0) == 0 || preferred.rfind(\"std/collections/map/\", 0) == 0") ==
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("resolvedPath.rfind(\"/std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("resolvedPath.rfind(\"/map/\", 0)") ==
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("scopedName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("scopedName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("KeyValueHelperSurfaceBridgeKey") !=
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("MapHelperSurfaceBridgeKey") ==
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("KeyValueConstructorSurfaceBridgeKey") !=
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("MapConstructorSurfaceBridgeKey") ==
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("mapHelperSurfaceMetadataLocal()") ==
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("keyValueConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("mapConstructorSurfaceMetadataLocal()") ==
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("keyValueConstructorAliasToken()") !=
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("mapConstructorAliasToken()") ==
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("resolveCanonicalMapHelperExprMemberName") ==
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("resolvePublishedMapHelperExprMemberName") ==
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("resolveCanonicalKeyValueHelperExprMemberName") !=
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("resolvePublishedKeyValueHelperExprMemberName") !=
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("isCanonicalMapAccessHelperName(") ==
        std::string::npos);
  CHECK(emitterCallPathHelpersSource.find("isCanonicalKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("const std::string mapAlias = \"/map/\" + suffix") ==
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("preferred.rfind(\"/map/\", 0) == 0 && defMap.count(preferred) == 0") ==
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("appendUnique(\"/map/\" + suffix)") ==
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("candidates.push_back(\"/map/\" + methodName)") ==
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("appendUnique(\"/std/collections/map/\" + suffix)") ==
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("eraseCandidate(\"/map/\" + suffix)") ==
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("eraseCandidate(\"/std/collections/map/\" + suffix)") ==
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("normalizedPath.rfind(\"map/\", 0) == 0 ||\n"
                                                     "          normalizedPath.rfind(\"std/collections/map/\", 0) == 0") ==
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("const std::string mapPrefix = \"map/\"") ==
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("const std::string stdMapPrefix = \"std/collections/map/\"") ==
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("receiverStruct == \"/map\"") ==
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("rawMethodName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("rawMethodName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("surfaceHelperPathForRawMethodName(\n"
                                                     "                  KeyValueHelperSurfaceBridgeKey") !=
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("MapHelperSurfaceBridgeKey") ==
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("return {keyValueHelperPath(methodName)}") !=
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("return {mapHelperPath(methodName)}") ==
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("keyValueMemberName") !=
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("mapMemberName") ==
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("isKeyValueHelperMethod") !=
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("isMapHelperMethod") ==
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("eraseCandidate(\"/map/\" + suffix)") ==
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("eraseCandidate(\"/std/collections/map/\" + suffix)") ==
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("normalized.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("std::string_view(\"std/collections/map/\").size()") ==
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("CollectionTypeKeyValueHelperSurfaceBridgeKey") !=
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("CollectionTypeMapHelperSurfaceBridgeKey") ==
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find(
            "collectionTypeKeyValueHelperMemberName(resolveExprPath(candidate), false)") !=
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("collectionTypeMapHelperMemberName(") ==
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("isCollectionTypeKeyValueAccessHelper(") !=
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("isCollectionTypeMapAccessHelper(") ==
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("builtinCanonicalMapAccessReceiverTypePath") ==
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("builtinCanonicalKeyValueAccessReceiverTypePath") !=
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("builtinMapAccessMethodReceiverTypePath") ==
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("builtinKeyValueAccessMethodReceiverTypePath") !=
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("isExplicitMapAccessMethod") ==
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("isExplicitKeyValueAccessMethod") !=
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("shouldProbeBuiltinMapAccessType") ==
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("shouldProbeBuiltinKeyValueAccessType") !=
        std::string::npos);
  CHECK(emitterPackedArgsSource.find("isMapAccessName") == std::string::npos);
  CHECK(emitterPackedArgsSource.find("isKeyValueAccessName") != std::string::npos);
  CHECK(emitterPackedArgsSource.find("isCanonicalMapAccessHelperName(") ==
        std::string::npos);
  CHECK(emitterPackedArgsSource.find("isCanonicalKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("} else if (normalizedPath.rfind(\"/std/collections/map/\", 0) == 0) {\n"
                                         "    const std::string suffix =") ==
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("preferCanonicalMapMethodHelperPath") ==
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("pruneMapAccessStructReturnCompatibilityCandidates") ==
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("path.rfind(\"map/\", 0) == 0 ||\n"
                                         "       path.rfind(\"std/collections/map/\", 0) == 0") ==
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("path.rfind(\"map/\", 0) == 0 || path.rfind(\"std/collections/map/\", 0) == 0") ==
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("candidate.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("candidate.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("typePath == \"/map\"") ==
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find(
            "constexpr std::string_view KeyValueHelperSurfaceBridgeKey = "
            "\"collections.map_helpers\"") !=
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("MapHelperSurfaceBridgeKey") ==
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("findKeyValueHelperSurfaceMetadata()") !=
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("findMapHelperSurfaceMetadata()") ==
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("isKeyValueHelperSurface(surfaceId)") !=
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("isMapHelperSurface(surfaceId)") ==
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("isRemovedExactPublishedKeyValueHelper(") !=
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("isRemovedExactPublishedMapHelper(") ==
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("canonicalKeyValueMemberName") !=
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("canonicalMapMemberName") ==
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("const std::string_view mapHelperName") ==
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("const std::string_view keyValueHelperName") !=
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("isCanonicalMapCountHelperName(") ==
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("isCanonicalKeyValueCountHelperName(") !=
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("normalizeMapImportAliasPath") ==
        std::string::npos);
  CHECK(emitterMethodMetadataSource.find("path.rfind(\"/map/\", 0) == 0 || path.rfind(\"/std/collections/map/\", 0) == 0") ==
        std::string::npos);
  CHECK(emitterMethodResolutionSource.find("normalizeMapImportAliasPath") ==
        std::string::npos);
  CHECK(emitterMethodResolutionSource.find("normalizedMethodName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(emitterMethodResolutionSource.find("normalizedMethodName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(emitterMethodResolutionSource.find("resolvedType == \"/map\"") ==
        std::string::npos);
  CHECK(emitterMethodResolutionSource.find("const std::string aliasPath = \"/map/\" + normalizedMethodName") ==
        std::string::npos);
  CHECK(emitterMethodResolutionSource.find("const std::string canonicalPath = \"/std/collections/map/\" + normalizedMethodName") ==
        std::string::npos);
  CHECK(emitterMethodResolutionSource.find("publishedSurfaceHelperPathForRawMethodName(\n"
                                           "            KeyValueHelperSurfaceBridgeKey") !=
        std::string::npos);
  CHECK(emitterMethodResolutionSource.find("MapHelperSurfaceBridgeKey") ==
        std::string::npos);
  CHECK(emitterMethodResolutionSource.find("isKeyValueHelperMethod") !=
        std::string::npos);
  CHECK(emitterMethodResolutionSource.find("isMapHelperMethod") ==
        std::string::npos);
  CHECK(emitterMethodResolutionSource.find("isRemovedMapSlashMethod") ==
        std::string::npos);
  CHECK(emitterMethodResolutionSource.find("isRemovedKeyValueSlashMethod") !=
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("pruneMapAccessStructReturnCompatibilityCandidates") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("isBareMapAccessMethod") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("isBareKeyValueAccessMethod") !=
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("resolveBareMapAccessMethodHelperPath") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("resolveBareKeyValueAccessMethodHelperPath") !=
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("isExplicitMapAccessCompatibilityCall") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("isExplicitKeyValueAccessCompatibilityCall") !=
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("inferExplicitMapAccessResolvedTypeName") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("inferExplicitKeyValueAccessResolvedTypeName") !=
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("inferCanonicalMapAccessTypeName") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("inferCanonicalKeyValueAccessTypeName") !=
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("explicitMapAccessType") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("explicitKeyValueAccessType") !=
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("canonicalMapAccessType") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("canonicalKeyValueAccessType") !=
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("bareMapAccessMethodPath") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("bareKeyValueAccessMethodPath") !=
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find(
            "isRemovedMapDirectCallResultCompatibility") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find(
            "isRemovedKeyValueDirectCallResultCompatibility") !=
        std::string::npos);
  CHECK(statementLowererSource.find("isPrimeKeyValueInsertBody") != std::string::npos);
  CHECK(statementLowererSource.find("rewriteKeyValueInsertHelperStatementToCanonical") != std::string::npos);
  CHECK(statementLowererSource.find("isPrimeMapInsertBody") == std::string::npos);
  CHECK(statementLowererSource.find("rewriteMapInsertHelperStatementToCanonical") == std::string::npos);
  CHECK(statementLowererSource.find("rewriteMapInsertHelperStatementToBuiltin") == std::string::npos);
  CHECK(statementLowererSource.find("/map/at(argsPack") == std::string::npos);
  CHECK(uninitializedStructInferenceSource.find("scopedCallPath == \"/map/at\"") ==
        std::string::npos);
  CHECK(uninitializedStructInferenceSource.find("scopedCallPath == \"/std/collections/map/at\"") ==
        std::string::npos);
  CHECK(uninitializedStructInferenceSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(uninitializedStructInferenceSource.find(
            "StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(uninitializedStructInferenceSource.find("constructorName == \"mapNew\"") ==
        std::string::npos);
  CHECK(uninitializedStructInferenceSource.find("publishedKeyValueHelperName == \"at\"") !=
        std::string::npos);
  CHECK(uninitializedStructInferenceSource.find(
            "keyValueHelperSurfaceMetadataForUninitializedStructs()") !=
        std::string::npos);
  CHECK(uninitializedStructInferenceSource.find(
            "keyValueConstructorSurfaceMetadataForUninitializedStructs()") !=
        std::string::npos);
  CHECK(uninitializedStructInferenceSource.find(
            "forwardedEmptyKeyValueConstructorMemberName()") !=
        std::string::npos);
  CHECK(structSlotLayoutSource.find("isBuiltinCollectionTypeName(typeName, \"map\")") !=
        std::string::npos);
  CHECK(structSlotLayoutSource.find("typeName == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(declaredCollectionInferenceSource.find("isBuiltinCollectionTypeName(base, \"map\")") !=
        std::string::npos);
  CHECK(declaredCollectionInferenceSource.find("base == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(declaredCollectionInferenceSource.find("normalizedName == \"std/collections/map/map\"") ==
        std::string::npos);
  CHECK(declaredCollectionInferenceSource.find("StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(declaredCollectionInferenceSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_constructors\")") !=
        std::string::npos);
  CHECK(declaredCollectionInferenceSource.find(
            "keyValueConstructorSurfaceMetadataForDeclaredInference()") !=
        std::string::npos);
  CHECK(declaredCollectionInferenceSource.find(
            "mapConstructorSurfaceMetadataLocal()") ==
        std::string::npos);
  CHECK(declaredCollectionInferenceSource.find("isDirectKeyValueConstructor()") !=
        std::string::npos);
  CHECK(declaredCollectionInferenceSource.find("isDirectMapConstructor()") ==
        std::string::npos);
  CHECK(bindingTypeHelpersSource.find("isBuiltinCollectionTypeName(name, \"map\")") !=
        std::string::npos);
  CHECK(bindingTypeHelpersSource.find("name == \"/map\"") ==
        std::string::npos);
  CHECK(bindingTypeHelpersSource.find("name == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(inferenceDispatchSetupSource.find(
            "isBuiltinCollectionTypeName(normalized, \"map\")") !=
        std::string::npos);
  CHECK(inferenceDispatchSetupSource.find("normalized == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(inferenceDispatchSetupSource.find(
            "\"/std/collections/map/\" + accessNameForCanonicalMapOverride") ==
        std::string::npos);
  CHECK(inferenceDispatchSetupSource.find("accessNameForCanonicalKeyValueOverride") !=
        std::string::npos);
  CHECK(inferenceDispatchSetupSource.find("accessNameForCanonicalMapOverride") ==
        std::string::npos);
  CHECK(inferenceDispatchSetupSource.find(
            "keyValueHelperSurfaceMetadataForDispatchSetup()") !=
        std::string::npos);
  CHECK(inferenceDispatchSetupSource.find(
            "mapHelperSurfaceMetadataForDispatchSetup()") ==
        std::string::npos);
  CHECK(inferenceDispatchSetupSource.find(
            "canonicalKeyValueHelperPathForDispatchSetup(") !=
        std::string::npos);
  CHECK(inferenceDispatchSetupSource.find(
            "canonicalMapHelperPathForDispatchSetup(") ==
        std::string::npos);
  CHECK(setupTypeReturnKindSource.find("canonicalMapHelperPath(") ==
        std::string::npos);
  CHECK(setupTypeReturnKindSource.find(
            "canonicalKeyValueHelperPathForSetupReturnKind(") !=
        std::string::npos);
  CHECK(inferenceDispatchSetupSource.find(
            "inferDispatchSetupKeyValueKindsFromTypeText(") !=
        std::string::npos);
  CHECK(inferenceDispatchSetupSource.find(
            "inferDispatchSetupMapKindsFromTypeText(") ==
        std::string::npos);
  CHECK(inferenceDispatchSetupSource.find(
            "inferDispatchSetupSemanticKeyValueReceiverKind(") !=
        std::string::npos);
  CHECK(inferenceDispatchSetupSource.find(
            "inferDispatchSetupSemanticMapReceiverKind(") ==
        std::string::npos);
  CHECK(inferenceDispatchSetupSource.find(
            "isDispatchSetupKeyValueFamilyText(") !=
        std::string::npos);
  CHECK(inferenceDispatchSetupSource.find(
            "isDispatchSetupMapFamilyText(") ==
        std::string::npos);
  CHECK(inferenceDispatchSetupSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_helpers\")") !=
        std::string::npos);
  CHECK(inferenceBaseKindSource.find(
            "isBuiltinCollectionTypeName(normalized, \"map\")") !=
        std::string::npos);
  CHECK(inferenceBaseKindSource.find("normalized == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(inferenceBaseKindSource.find("collectionMemberPath(\"map\", \"tryAt\", false)") !=
        std::string::npos);
  CHECK(inferenceBaseKindSource.find("normalized == \"map/tryAt\"") ==
        std::string::npos);
  CHECK(inferenceBaseKindSource.find("normalized == \"std/collections/map/tryAt\"") ==
        std::string::npos);
  CHECK(inferenceBaseKindSource.find("collectionMemberPath(\"map\", \"contains\", false)") !=
        std::string::npos);
  CHECK(inferenceBaseKindSource.find("normalized == \"map/contains\"") ==
        std::string::npos);
  CHECK(inferenceBaseKindSource.find("normalized == \"std/collections/map/contains\"") ==
        std::string::npos);
  CHECK(setupTypeMethodTargetSource.find(
            "isBuiltinCollectionTypeName(normalized, \"map\")") !=
        std::string::npos);
  CHECK(setupTypeMethodTargetSource.find("normalized == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(setupTypeMethodTargetSource.find("\"/std/collections/map\"") ==
        std::string::npos);
  CHECK(setupTypeMethodTargetSource.find("collectionTypePath(\"map\")") !=
        std::string::npos);
  CHECK(setupTypeMethodTargetSource.find("const std::string rootedKeyValuePrefix") !=
        std::string::npos);
  CHECK(setupTypeMethodTargetSource.find("const std::string rootedMapPrefix") ==
        std::string::npos);
  CHECK(setupTypeMethodTargetSource.find("const std::string canonicalKeyValuePrefix") !=
        std::string::npos);
  CHECK(setupTypeMethodTargetSource.find("const std::string canonicalMapPrefix") ==
        std::string::npos);
  CHECK(setupTypeMethodTargetSource.find("isKeyValueReceiverTarget(") !=
        std::string::npos);
  CHECK(setupTypeMethodTargetSource.find("isMapReceiverTarget(") ==
        std::string::npos);
  CHECK(setupTypeMethodTargetSource.find("shouldPreferCanonicalKeyValuePath(") !=
        std::string::npos);
  CHECK(setupTypeMethodTargetSource.find("shouldPreferCanonicalMapPath(") ==
        std::string::npos);
  CHECK(setupTypeMethodTargetSource.find("normalizedMethodName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(setupTypeMethodTargetSource.find("normalizedMethodName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(setupTypeMethodTargetSource.find("normalizedOriginalMethodName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(setupTypeMethodTargetSource.find("normalizedOriginalMethodName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("collectionMemberPath(\"map\", \"map\")") !=
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_constructors\")") !=
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("path == \"/std/collections/map/map\"") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("collectionMemberRoot(\"map\", false)") !=
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("std::string(\"std/collections/map/\").size()") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("normalizedMethodName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("collectionMemberPath(\"map\", keyValueHelperName)") !=
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("collectionMemberPath(\"map\", mapHelperName)") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("\"/std/collections/map/\" + keyValueHelperName") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("\"/std/collections/map/\" + mapHelperName") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("const std::string rootedKeyValuePrefix") !=
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("const std::string rootedMapPrefix") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("const std::string canonicalKeyValuePrefix") !=
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("const std::string canonicalMapPrefix") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("sourceKeyValueMethodHelperName(") !=
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("sourceMapMethodHelperName(") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("receiverHasKeyValueLocalInfo(") !=
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("receiverHasMapLocalInfo(") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("canonicalKeyValueHelper") !=
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("canonicalMapHelper") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("isKeyValueConstructorDirectTargetPath(") !=
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("isMapConstructorDirectTargetPath(") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("keyValueConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("mapConstructorSurfaceMetadataLocal()") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find(
            "findKeyValueConstructorBridgePathChoiceBySource(") !=
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find(
            "findMapConstructorBridgePathChoiceBySource(") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("isBareKeyValueAccessReceiverProbeExpr(") !=
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("isBareMapAccessReceiverProbeExpr(") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find(
            "blocksBareKeyValueAccessReceiverProbeKindFallback") !=
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find(
            "blocksBareMapAccessReceiverProbeKindFallback") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("isBuiltinKeyValueContainsOrTryAtCall") !=
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("isBuiltinMapContainsOrTryAtCall") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("normalized.rfind(\"/map/\", 0)") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("normalized.rfind(\"/std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("collectionMemberPath(\"map\", \"count\")") !=
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("\"/std/collections/map/count\"") ==
        std::string::npos);
  CHECK(setupTypeReceiverTargetSource.find("isBuiltinKeyValueContainsOrTryAtCall") !=
        std::string::npos);
  CHECK(setupTypeReceiverTargetSource.find("isBuiltinMapContainsOrTryAtCall") ==
        std::string::npos);
  CHECK(setupTypeReceiverTargetSource.find("isBareKeyValueAccessReceiverProbeExpr(") !=
        std::string::npos);
  CHECK(setupTypeReceiverTargetSource.find("isBareMapAccessReceiverProbeExpr(") ==
        std::string::npos);
  CHECK(setupTypeReceiverTargetSource.find("isBareKeyValueTryAtReceiverProbeExpr(") !=
        std::string::npos);
  CHECK(setupTypeReceiverTargetSource.find("isBareMapTryAtReceiverProbeExpr(") ==
        std::string::npos);
  CHECK(setupTypeReceiverTargetSource.find(
            "blocksExplicitKeyValueReceiverProbeKindFallback") !=
        std::string::npos);
  CHECK(setupTypeReceiverTargetSource.find(
            "blocksExplicitMapReceiverProbeKindFallback") ==
        std::string::npos);
  CHECK(setupTypeReceiverTargetSource.find(
            "blocksBareKeyValueAccessReceiverProbeKindFallback") !=
        std::string::npos);
  CHECK(setupTypeReceiverTargetSource.find(
            "blocksBareMapAccessReceiverProbeKindFallback") ==
        std::string::npos);
  CHECK(setupTypeCollectionSource.find("const std::string mapPrefix = \"map/\"") ==
        std::string::npos);
  CHECK(setupTypeCollectionSource.find("const std::string stdMapPrefix = \"std/collections/map/\"") ==
        std::string::npos);
  CHECK(setupTypeCollectionSource.find("collectionMemberRoot(\"map\", false)") !=
        std::string::npos);
  CHECK(setupTypeCollectionSource.find("normalizeBuiltinCollectionStructPath(\"map\").substr(1) + \"/\"") !=
        std::string::npos);
  CHECK(setupTypeCollectionSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(setupTypeCollectionSource.find("StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(setupTypeCollectionSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_helpers\")") !=
        std::string::npos);
  CHECK(setupTypeCollectionSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_constructors\")") !=
        std::string::npos);
  CHECK(setupTypeCollectionSource.find("keyValueHelperSurfaceId()") !=
        std::string::npos);
  CHECK(setupTypeCollectionSource.find("mapHelperSurfaceId()") ==
        std::string::npos);
  CHECK(setupTypeCollectionSource.find("keyValueConstructorSurfaceId()") !=
        std::string::npos);
  CHECK(setupTypeCollectionSource.find("mapConstructorSurfaceId()") ==
        std::string::npos);
  CHECK(setupTypeCollectionSource.find("isKeyValueHelperSurfaceId(") !=
        std::string::npos);
  CHECK(setupTypeCollectionSource.find("isMapHelperSurfaceId(") ==
        std::string::npos);
  CHECK(setupTypeCollectionSource.find("resolveKeyValueSurfaceMemberToken(") !=
        std::string::npos);
  CHECK(setupTypeCollectionSource.find("resolveMapSurfaceMemberToken(") ==
        std::string::npos);
  CHECK(setupTypeCollectionSource.find("isBorrowedKeyValueHelperSurface(") !=
        std::string::npos);
  CHECK(setupTypeCollectionSource.find("isBorrowedMapHelperSurface(") ==
        std::string::npos);
  CHECK(countAccessSource.find("canonicalCollectionMemberPath(\"map\", accessName)") !=
        std::string::npos);
  CHECK(countAccessSource.find("hasCanonicalCollectionMemberPrefix(text, \"map\")") !=
        std::string::npos);
  CHECK(countAccessSource.find("\"/std/collections/map/\" + std::string(accessName)") ==
        std::string::npos);
  CHECK(countAccessSource.find("\"/std/collections/map/\" + accessName") ==
        std::string::npos);
  CHECK(countAccessSource.find("text.rfind(\"/std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(countAccessSource.find("text.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(resultMetadataSource.find("collectionMemberPath(\"map\", \"map\")") !=
        std::string::npos);
  CHECK(resultMetadataSource.find("StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(resultMetadataSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_constructors\")") !=
        std::string::npos);
  CHECK(resultMetadataSource.find(
            "keyValueConstructorSurfaceMetadataForResultMetadata()") !=
        std::string::npos);
  CHECK(resultMetadataSource.find("mapConstructorSurfaceMetadataLocal()") ==
        std::string::npos);
  CHECK(resultMetadataSource.find("matchesDirectKeyValueConstructor") !=
        std::string::npos);
  CHECK(resultMetadataSource.find("matchesDirectMapConstructor") ==
        std::string::npos);
  CHECK(resultMetadataSource.find("sourceKeyValueKeyKind") !=
        std::string::npos);
  CHECK(resultMetadataSource.find("sourceMapKeyKind") ==
        std::string::npos);
  CHECK(resultMetadataSource.find("collectionKeyValueKeyKind") !=
        std::string::npos);
  CHECK(resultMetadataSource.find("collectionMapKeyKind") ==
        std::string::npos);
  CHECK(resultMetadataSource.find("\"/std/collections/map/map\"") ==
        std::string::npos);
  CHECK(resultMetadataSource.find("\"/std/collections/map/map__\"") ==
        std::string::npos);
  CHECK(resultMetadataSource.find("\"std/collections/map/map\"") ==
        std::string::npos);
  CHECK(resultMetadataSource.find("\"std/collections/map/map__\"") ==
        std::string::npos);
  CHECK(semanticsResultHelpersSource.find(
            "resolved == \"/std/collections/map/tryAt\"") ==
        std::string::npos);
  CHECK(semanticsResultHelpersSource.find(
            "resolved == \"/std/collections/map/tryAt_ref\"") ==
        std::string::npos);
  CHECK(semanticsResultHelpersSource.find("resolved == \"/map/tryAt\"") ==
        std::string::npos);
  CHECK(semanticsResultHelpersSource.find(
            "isMapTryAtResultHelperCall(resolved, expr)") !=
        std::string::npos);
  CHECK(semanticsResultHelpersSource.find(
            "metadataBackedCanonicalMapHelperPath(\"tryAt\")") !=
        std::string::npos);
  CHECK(semanticsResultHelpersSource.find(
            "metadataBackedMapHelperRootAliasMethodName(resolvedPath)") !=
        std::string::npos);
  CHECK(statementReturnsSource.find("unknown method: /map/at") ==
        std::string::npos);
  CHECK(statementReturnsSource.find("unknown method: /map/at_ref") ==
        std::string::npos);
  CHECK(statementReturnsSource.find("unknown method: /map/at_unsafe") ==
        std::string::npos);
  CHECK(statementReturnsSource.find("unknown method: /map/at_unsafe_ref") ==
        std::string::npos);
  CHECK(statementReturnsSource.find("return \"/map\"") == std::string::npos);
  CHECK(statementReturnsSource.find("typePath == \"/map\"") == std::string::npos);
  CHECK(statementReturnsSource.find("expectedType == \"/map\"") ==
        std::string::npos);
  CHECK(statementReturnsSource.find("mapHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(statementReturnsSource.find("mapConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(statementReturnsSource.find("mapCollectionMarkerPathLocal()") ==
        std::string::npos);
  CHECK(statementReturnsSource.find("isUnknownBorrowedMapAccessMethodDiagnostic") ==
        std::string::npos);
  CHECK(statementReturnsSource.find("std::array<std::string_view, 4> AccessHelpers") ==
        std::string::npos);
  CHECK(statementReturnsSource.find("mapCollectionMarker") ==
        std::string::npos);
  CHECK(statementReturnsSource.find("canonicalMapValueRoot") ==
        std::string::npos);
  CHECK(statementReturnsSource.find("keyValueCollectionMarkerPathLocal()") !=
        std::string::npos);
  CHECK(statementReturnsSource.find("isUnknownBorrowedKeyValueAccessMethodDiagnostic") !=
        std::string::npos);
  CHECK(statementReturnsSource.find("KeyValueAccessHelpers") !=
        std::string::npos);
  CHECK(statementReturnsSource.find("keyValueCollectionMarker") !=
        std::string::npos);
  CHECK(statementReturnsSource.find("canonicalKeyValueBackingRoot") !=
        std::string::npos);
  CHECK(inlineCallContextSource.find("collectionMemberRoot(\"map\") + \"map__\"") !=
        std::string::npos);
  CHECK(inlineCallContextSource.find("\"/std/collections/map/map__\"") ==
        std::string::npos);
  CHECK(accessTargetResolutionSource.find("constructorName == \"mapNew\"") ==
        std::string::npos);
  CHECK(accessTargetResolutionSource.find(
            "forwardedEmptyKeyValueConstructorMemberName()") !=
        std::string::npos);
  CHECK(accessTargetResolutionSource.find(
            "forwardedEmptyMapConstructorMemberName()") ==
        std::string::npos);
  CHECK(accessTargetResolutionSource.find(
            "resolveKeyValueConstructorPathMemberName(") !=
        std::string::npos);
  CHECK(accessTargetResolutionSource.find(
            "resolveMapConstructorPathMemberName(") ==
        std::string::npos);
  CHECK(accessTargetResolutionSource.find("isPublishedKeyValueConstructorExpr(") !=
        std::string::npos);
  CHECK(accessTargetResolutionSource.find("isPublishedMapConstructorExpr(") ==
        std::string::npos);
  CHECK(accessTargetResolutionSource.find("inferDirectKeyValueConstructorTargetInfo(") !=
        std::string::npos);
  CHECK(accessTargetResolutionSource.find("inferDirectMapConstructorTargetInfo(") ==
        std::string::npos);
  CHECK(accessTargetResolutionSource.find("isForwardedKeyValueNewConstructor(") !=
        std::string::npos);
  CHECK(accessTargetResolutionSource.find("isForwardedMapNewConstructor(") ==
        std::string::npos);
  CHECK(accessTargetResolutionSource.find(
            "resolveStdlibSurfaceMemberName(*metadata, metadata->canonicalPath)") !=
        std::string::npos);
  CHECK(statementLowererSource.find("callee->fullPath.rfind(\"/std/collections/internal_map/insertImpl__\", 0)") !=
        std::string::npos);
  CHECK(statementLowererSource.find("canonicalStatementKeyValueHelperPath(\"insert\")") !=
        std::string::npos);
  CHECK(statementLowererSource.find("canonicalStatementMapHelperPath(\"insert\")") ==
        std::string::npos);
  CHECK(statementLowererSource.find("rewrittenStmt.name = \"/std/collections/map/insert\"") ==
        std::string::npos);
  CHECK(lowererCallHelpersSource.find("const std::string unrooted = \"map/\" + std::string(helperName)") ==
        std::string::npos);
  CHECK(lowererCallHelpersSource.find("resolveMapHelperDefinitionMember(") ==
        std::string::npos);
  CHECK(lowererCallHelpersSource.find("isRemovedCountFallbackMapHelper(") ==
        std::string::npos);
  CHECK(lowererCallHelpersSource.find("isExplicitRemovedMapHelperAliasCall") ==
        std::string::npos);
  CHECK(lowererCallHelpersSource.find(
            "resolveKeyValueHelperDefinitionMember(directHelperPath, helperName)") !=
        std::string::npos);
  CHECK(lowererCallHelpersSource.find("isRemovedCountFallbackKeyValueHelper(") !=
        std::string::npos);
  CHECK(lowererCallHelpersSource.find("isExplicitRemovedKeyValueHelperAliasCall") !=
        std::string::npos);
  CHECK(inlineNativeSource.find("return helperName == \"insert\" || helperName == \"insert_ref\"") ==
        std::string::npos);
  CHECK(inlineNativeSource.find("return helperName == \"at\" || helperName == \"at_ref\"") ==
        std::string::npos);
  CHECK(inlineNativeSource.find("return helperName == \"count\" || helperName == \"count_ref\"") ==
        std::string::npos);
  CHECK(inlineNativeSource.find("rawPath == \"map/at\"") == std::string::npos);
  CHECK(inlineNativeSource.find("rawPath == \"map/at_unsafe\"") == std::string::npos);
  CHECK(inlineNativeSource.find("rawPath == \"map/at_ref\"") == std::string::npos);
  CHECK(inlineNativeSource.find("rawPath == \"map/at_unsafe_ref\"") == std::string::npos);
  CHECK(inlineNativeSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(inlineNativeSource.find("inlineKeyValueHelperMetadata()") !=
        std::string::npos);
  CHECK(inlineNativeSource.find("resolvePublishedInlineKeyValueSurfaceMemberName") !=
        std::string::npos);
  CHECK(inlineNativeSource.find("isCanonicalPublishedInlineKeyValueHelperPath") !=
        std::string::npos);
  CHECK(inlineNativeSource.find("emitCanonicalInlineDefinitionCall(expr, *callee)") != std::string::npos);
  CHECK(emitterMethodResolutionSource.find("!hasAliasHelperDefinition && !hasCanonicalHelperDefinition") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("const std::string aliasPath = \"/map/\" + candidate.name") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("const std::string canonicalPath = \"/std/collections/map/\" + candidate.name") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("normalized.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("normalized.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("resolvedExprPath.rfind(\"/std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("resolvedExprPath.rfind(\"/map/\", 0)") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find(
            "isCanonicalKeyValueHelperMemberPath(normalized, helperName)") !=
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("isCanonicalMapHelperMemberPath(") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find(
            "isKeyValueImportAliasHelperMemberPath(resolvedExprPath, helperName)") !=
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("isMapImportAliasHelperMemberPath(") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("KeyValueHelperSurfaceBridgeKey") !=
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("MapHelperSurfaceBridgeKey") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("keyValueHelperPath(") !=
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("mapHelperPath(") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("keyValueHelperMemberNameFromPath(") !=
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("mapHelperMemberNameFromPath(") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("isCanonicalMapAccessHelperName(") ==
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("isCanonicalKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(emitterHelpersSource.find("isCanonicalMapHelperName(") ==
        std::string::npos);
  CHECK(emitterHelpersSource.find("mapHelperNameFromPath(") ==
        std::string::npos);
  CHECK(emitterHelpersSource.find("isCanonicalMapHelperPath(") ==
        std::string::npos);
  CHECK(emitterHelpersSource.find("isCanonicalKeyValueHelperName(") !=
        std::string::npos);
  CHECK(emitterHelpersSource.find("isCanonicalMapCountHelperName(") ==
        std::string::npos);
  CHECK(emitterHelpersSource.find("isCanonicalKeyValueCountHelperName(") !=
        std::string::npos);
  CHECK(emitterHelpersSource.find("keyValueHelperNameFromPath(") !=
        std::string::npos);
  CHECK(emitterHelpersSource.find("isCanonicalKeyValueHelperPath(") !=
        std::string::npos);
  CHECK(emitterHelpersSource.find("isCanonicalMapAccessHelperName(") ==
        std::string::npos);
  CHECK(emitterHelpersSource.find("isCanonicalMapAccessHelperPath(") ==
        std::string::npos);
  CHECK(emitterHelpersSource.find("isCanonicalKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(emitterHelpersSource.find("isCanonicalKeyValueAccessHelperPath(") !=
        std::string::npos);
  CHECK(emitterHelpersSource.find("isRemovedMapSlashMethodMetadataHelperName(") ==
        std::string::npos);
  CHECK(emitterHelpersSource.find(
            "isRemovedMapDirectCallResultCompatibilityHelperName(") ==
        std::string::npos);
  CHECK(emitterHelpersSource.find(
            "isRemovedKeyValueSlashMethodMetadataHelperName(") !=
        std::string::npos);
  CHECK(emitterHelpersSource.find(
            "isRemovedKeyValueDirectCallResultCompatibilityHelperName(") !=
        std::string::npos);
  CHECK(emitterHelpersTypesSource.find("base += \"map/Map\"") == std::string::npos);
  CHECK(emitterHelpersTypesSource.find("experimentalCollectionTypePathLocal(\"map\", \"Map\"") !=
        std::string::npos);
  CHECK(emitterHelpersTypesSource.find("isMapCompatibilityStorageBase(") ==
        std::string::npos);
  CHECK(emitterHelpersTypesSource.find("isKeyValueCompatibilityStorageBase(") !=
        std::string::npos);
  CHECK(emitterHelpersTypesSource.find("name == \"/map\"") == std::string::npos);
  CHECK(emitterHelpersTypesSource.find("name == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(emitterHelpersTypesSource.find("name.rfind(\"/std/collections/map<\"") ==
        std::string::npos);
  CHECK(nativeTailSource.find("hasSemanticKeyValueReadHelperDefinition") !=
        std::string::npos);
  CHECK(nativeTailSource.find(
            "isKeyValueReadHelperName(directKeyValueReadHelperName)") !=
        std::string::npos);
  CHECK(nativeTailSource.find("importsMapReadHelper") == std::string::npos);
  CHECK(nativeTailSource.find("importsKeyValueReadHelper") !=
        std::string::npos);
  CHECK(nativeTailSource.find("importsMapHelpers") == std::string::npos);
  CHECK(nativeTailSource.find("importsKeyValueHelpers") != std::string::npos);
  CHECK(lowerStatementsExprSource.find("Keep direct canonical map access helpers") == std::string::npos);
  CHECK(lowerStatementsExprSource.find("keepsBuiltinCanonicalMapHelperReturn") == std::string::npos);
  CHECK(lowerStatementsExprSource.find("keyValueHelperMetadata") != std::string::npos);
  CHECK(lowerStatementsExprSource.find("mapHelperMetadata") == std::string::npos);
  CHECK(lowerStatementsExprSource.find("keyValueConstructorMetadata") != std::string::npos);
  CHECK(lowerStatementsExprSource.find("mapConstructorMetadata") == std::string::npos);
  CHECK(lowerStatementsExprSource.find("isCanonicalKeyValueHelperFamilyPath(") !=
        std::string::npos);
  CHECK(lowerStatementsExprSource.find("isCanonicalMapHelperFamilyPath(") ==
        std::string::npos);
  CHECK(lowerStatementsExprSource.find("isCanonicalKeyValueConstructorPath(") !=
        std::string::npos);
  CHECK(lowerStatementsExprSource.find("isCanonicalMapConstructorPath(") ==
        std::string::npos);
  CHECK(lowerStatementsExprSource.find("findDirectEntryKeyValueConstructorDefinition(") !=
        std::string::npos);
  CHECK(lowerStatementsExprSource.find("findDirectEntryMapConstructorDefinition(") ==
        std::string::npos);
  CHECK(lowerStatementsExprSource.find("isExplicitCanonicalKeyValueAccess") !=
        std::string::npos);
  CHECK(lowerStatementsExprSource.find("isExplicitCanonicalMapAccess") ==
        std::string::npos);
  CHECK(lowerStatementsExprSource.find("resolvedKeyValueInsertHelperName") !=
        std::string::npos);
  CHECK(lowerStatementsExprSource.find("resolvedMapInsertHelperName") ==
        std::string::npos);
  CHECK(lowerStatementsBindingsSource.find("resolvePublishedStdlibSurfaceMemberName(\n"
                                           "                  rawPath,\n"
                                           "                  metadata->id") !=
        std::string::npos);
  CHECK(lowerStatementsBindingsSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(lowerStatementsBindingsSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_helpers\")") !=
        std::string::npos);
  CHECK(lowerStatementsBindingsSource.find("canonicalizeExplicitBuiltinKeyValueHelpers") !=
        std::string::npos);
  CHECK(lowerStatementsBindingsSource.find("canonicalizeExplicitBuiltinMapHelpers") ==
        std::string::npos);
  CHECK(lowerStatementsBindingsSource.find("resolveDirectKeyValueHelperPath") !=
        std::string::npos);
  CHECK(lowerStatementsBindingsSource.find("resolveDirectMapHelperPath") ==
        std::string::npos);
  CHECK(lowerStatementsBindingsSource.find("findDirectKeyValueHelperDefinition") !=
        std::string::npos);
  CHECK(lowerStatementsBindingsSource.find("findDirectMapHelperDefinition") ==
        std::string::npos);
  CHECK(lowerStatementsBindingsSource.find("rawPath.rfind(\"/map/\", 0)") ==
        std::string::npos);
  CHECK(lowerStatementsBindingsSource.find("rawPath.rfind(\"/std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(tailDispatchSource.find("rewrittenExpr.name = ir_lowerer::collectionMemberPath(\"map\", \"insert\")") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(tailDispatchSource.find("tailDispatchKeyValueHelperSurfaceId()") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("collections.map_helpers") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("isTailDispatchKeyValueImportAliasHelperPath(rawPath, helperName)") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("rawPath.rfind(\"/\" + std::string(\"map\") + \"/\", 0)") ==
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find("StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find(
            "keyValueHelperSurfaceMetadataForLowerEmitExpr()") !=
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find(
            "mapHelperSurfaceMetadataForLowerEmitExpr()") ==
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find(
            "keyValueConstructorSurfaceMetadataForLowerEmitExpr()") !=
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find(
            "mapConstructorSurfaceMetadataForLowerEmitExpr()") ==
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find("resolvePublishedLateKeyValueMemberName(") !=
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find("resolvePublishedLateMapMemberName(") ==
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find(
            "resolvePublishedLateKeyValueConstructorName(") !=
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find("resolvePublishedLateMapConstructorName(") ==
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find("isEntryArgsPackKeyValueConstructorExpr(") !=
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find("isEntryArgsPackMapConstructorExpr(") ==
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find("rewriteExplicitBuiltinKeyValueHelperExpr(") !=
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find("rewriteExplicitBuiltinMapHelperExpr(") ==
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find("isKeyValueAccessValueCall(") !=
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find("isMapAccessValueCall(") ==
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find("rewriteBareKeyValueAccessMethodExpr(") !=
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find("rewriteBareMapAccessMethodExpr(") ==
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find("rawPath.rfind(\"/map/\", 0)") ==
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find("canonicalMapHelperRoot") ==
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find("directHelperPath.rfind(\"/std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(builtinNameHelpersSource.find("scopedName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(builtinNameHelpersSource.find("scopedName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(builtinNameHelpersSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(builtinNameHelpersSource.find("alias == \"map\"") ==
        std::string::npos);
  CHECK(builtinNameHelpersSource.find("rawName == \"map\"") ==
        std::string::npos);
  CHECK(builtinNameHelpersSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_constructors\")") !=
        std::string::npos);
  CHECK(builtinNameHelpersSource.find("keyValueConstructorAliasToken()") !=
        std::string::npos);
  CHECK(builtinNameHelpersSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_helpers\")") !=
        std::string::npos);
  CHECK(builtinNameHelpersSource.find("resolvesKeyValueHelperSurfacePath(scopedName)") !=
        std::string::npos);
  CHECK(lowererHelpersSource.find("candidate.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(lowererHelpersSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(lowererHelpersSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_helpers\")") !=
        std::string::npos);
  CHECK(lowererHelpersSource.find("resolvesKeyValueHelperSurfacePath(candidate)") !=
        std::string::npos);
  CHECK(lowererHelpersSource.find("resolvesMapHelperSurfacePath(candidate)") ==
        std::string::npos);
  CHECK(inlinePackedArgsSource.find("rewrittenExpr.name = \"/map/map\"") == std::string::npos);
  CHECK(inlinePackedArgsSource.find("rewriteBuiltinMapConstructorExpr") == std::string::npos);
  CHECK(inlinePackedArgsSource.find("StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(inlinePackedArgsSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_constructors\")") !=
        std::string::npos);
  CHECK(inlinePackedArgsSource.find(
            "keyValueConstructorSurfaceMetadataForInlinePackedArgs()") !=
        std::string::npos);
  CHECK(inlinePackedArgsSource.find(
            "mapConstructorSurfaceMetadataForInlinePackedArgs()") ==
        std::string::npos);
  CHECK(inlinePackedArgsSource.find("isPublishedKeyValueConstructorExpr(") !=
        std::string::npos);
  CHECK(inlinePackedArgsSource.find("isPublishedMapConstructorExpr(") ==
        std::string::npos);
  CHECK(inlinePackedArgsSource.find("rewritePublishedKeyValueConstructorExpr(") !=
        std::string::npos);
  CHECK(inlinePackedArgsSource.find("rewritePublishedMapConstructorExpr(") ==
        std::string::npos);
  CHECK(inlinePackedArgsSource.find("keyValueConstructorMetadata") !=
        std::string::npos);
  CHECK(inlinePackedArgsSource.find("mapConstructorMetadata") ==
        std::string::npos);
  CHECK(inlinePackedArgsSource.find("rewrittenExpr.name = collectionMemberPath(\"map\", \"map\")") !=
        std::string::npos);
  CHECK(inlineParamHelpersSource.find("rewrittenExpr.name = \"/map/map\"") == std::string::npos);
  CHECK(inlineParamHelpersSource.find("rewriteBuiltinMapConstructorExpr") == std::string::npos);
  CHECK(inlineParamHelpersSource.find("StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(inlineParamHelpersSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_constructors\")") !=
        std::string::npos);
  CHECK(inlineParamHelpersSource.find("isPublishedKeyValueConstructorExpr(") !=
        std::string::npos);
  CHECK(inlineParamHelpersSource.find("isPublishedMapConstructorExpr(") ==
        std::string::npos);
  CHECK(inlineParamHelpersSource.find("rewritePublishedKeyValueConstructorExpr(") !=
        std::string::npos);
  CHECK(inlineParamHelpersSource.find("rewritePublishedMapConstructorExpr(") ==
        std::string::npos);
  CHECK(inlineParamHelpersSource.find("keyValueConstructorMetadata") !=
        std::string::npos);
  CHECK(inlineParamHelpersSource.find("mapConstructorMetadata") ==
        std::string::npos);
  CHECK(inlineParamHelpersSource.find("rewrittenExpr.name = collectionMemberPath(\"map\", \"map\")") !=
        std::string::npos);
  CHECK(packedResultSource.find("rewrittenExpr.name = \"/map/map\"") == std::string::npos);
  CHECK(packedResultSource.find("StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(packedResultSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_constructors\")") !=
        std::string::npos);
  CHECK(packedResultSource.find("keyValueConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(packedResultSource.find("mapConstructorSurfaceMetadataLocal()") ==
        std::string::npos);
  CHECK(packedResultSource.find("rewritePackedResultKeyValueConstructorExpr(") !=
        std::string::npos);
  CHECK(packedResultSource.find("rewritePackedResultMapConstructorExpr(") ==
        std::string::npos);
  CHECK(packedResultSource.find("rewrittenDirectKeyValueExpr") !=
        std::string::npos);
  CHECK(packedResultSource.find("rewrittenDirectMapExpr") ==
        std::string::npos);
  CHECK(packedResultSource.find("rewrittenKeyValueExpr") !=
        std::string::npos);
  CHECK(packedResultSource.find("rewrittenMapExpr") ==
        std::string::npos);
  CHECK(packedResultSource.find("rewrittenExpr.name = collectionMemberPath(\"map\", \"map\")") !=
        std::string::npos);
}

TEST_CASE("experimental map production traces are classified as backing substrate") {
  std::vector<std::string> unclassified;
  for (const auto &file : productionMapTraceFiles()) {
    std::ifstream input(file);
    REQUIRE(input.is_open());
    const std::string relativePath = relativePathString(file);
    std::string line;
    size_t lineNumber = 0;
    while (std::getline(input, line)) {
      ++lineNumber;
      if (line.find("experimental_map") == std::string::npos) {
        continue;
      }
      if (!isAllowedExperimentalMapTrace(relativePath, line)) {
        unclassified.push_back(relativePath + ":" + std::to_string(lineNumber) +
                               ": " + line);
      }
    }
  }

  INFO("unclassified production experimental_map traces:");
  for (const auto &trace : unclassified) {
    INFO(trace);
  }
  CHECK(unclassified.empty());
}

TEST_SUITE_END();
