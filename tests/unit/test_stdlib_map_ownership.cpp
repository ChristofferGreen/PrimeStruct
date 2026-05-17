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
      "src/emitter/EmitterBuiltinCallPathHelpers.cpp",
      "src/emitter/EmitterHelpersTypes.cpp",
      "src/ir_lowerer/IrLowererAccessTargetResolution.cpp",
      "src/ir_lowerer/IrLowererBindingTypeHelpers.cpp",
      "src/ir_lowerer/IrLowererCallHelpers.cpp",
      "src/ir_lowerer/IrLowererInlineCallContextHelpers.cpp",
      "src/ir_lowerer/IrLowererInlinePackedArgs.cpp",
      "src/ir_lowerer/IrLowererInlineParamHelpers.cpp",
      "src/ir_lowerer/IrLowererLowerEmitExprCollectionHelpers.h",
      "src/ir_lowerer/IrLowererLowerEmitExprTailDispatch.h",
      "src/ir_lowerer/IrLowererLowerEmitExprTryHelpers.h",
      "src/ir_lowerer/IrLowererLowerInferenceBaseKindHelpers.cpp",
      "src/ir_lowerer/IrLowererLowerInferenceDispatchSetup.cpp",
      "src/ir_lowerer/IrLowererLowerInferenceFallbackSetup.cpp",
      "src/ir_lowerer/IrLowererLowerInlineCalls.h",
      "src/ir_lowerer/IrLowererLowerStatementsExpr.h",
      "src/ir_lowerer/IrLowererResultMetadataHelpers.cpp",
      "src/ir_lowerer/IrLowererSetupTypeCollectionHelpers.cpp",
      "src/ir_lowerer/IrLowererSetupTypeDeclaredCollectionInference.cpp",
      "src/ir_lowerer/IrLowererSetupTypeMethodTargetHelpers.cpp",
      "src/ir_lowerer/IrLowererStatementBindingHelpers.cpp",
      "src/ir_lowerer/IrLowererStatementBindingTypeMetadata.cpp",
      "src/ir_lowerer/IrLowererStatementCallEmission.cpp",
      "src/ir_lowerer/IrLowererStructFieldBindingHelpers.cpp",
      "src/ir_lowerer/IrLowererStructLayoutHelpers.cpp",
      "src/ir_lowerer/IrLowererStructSlotLayoutHelpers.cpp",
      "src/ir_lowerer/IrLowererStructTypeHelpers.cpp",
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
  if (relativePath == "src/ir_lowerer/IrLowererSetupTypeMethodCallResolution.cpp" &&
      contains("normalized.rfind(\"/std/collections/experimental_map/\", 0) == 0")) {
    return true;
  }
  if (relativePath == "src/ir_lowerer/IrLowererLowerEmitExprCollectionHelpers.h" &&
      contains("receiverDef->fullPath.rfind(\"/std/collections/experimental_map/\", 0) == 0")) {
    return true;
  }
  if (relativePath == "src/ir_lowerer/IrLowererInlineCallContextHelpers.cpp" &&
      contains("isSinglePathSegmentWithPrefix(path, \"/std/collections/experimental_map/map__\")")) {
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
  const std::string semanticsSource = readText(repoRoot() / "src" / "semantics" / "SemanticsValidate.cpp");
  const std::string validatorSource =
      readText(repoRoot() / "src" / "semantics" / "SemanticsValidator.cpp");
  const std::string callResolutionSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprCallResolution.cpp");
  const std::string callPathHelpersSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsCallPathHelpers.cpp");
  const std::string methodTargetResolutionSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprMethodTargetResolution.cpp");
  const std::string exprMethodResolutionSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprMethodResolution.cpp");
  const std::string templateCoreSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphCoreUtilities.h");
  const std::string templateReceiverSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphExperimentalCollectionReceiverResolution.h");
  const std::string templateExpressionRewriteSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphExpressionRewrite.h");
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
  const std::string collectionHelperRewritesSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorCollectionHelperRewrites.cpp");
  const std::string buildInitializerInferenceSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorBuildInitializerInference.cpp");
  const std::string inferCollectionDispatchSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferCollectionDispatch.cpp");
  const std::string inferCollectionCompatibilitySource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferCollectionCompatibility.cpp");
  const std::string inferCollectionDispatchSetupSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferCollectionDispatchSetup.cpp");
  const std::string inferCollectionReturnInferenceSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferCollectionReturnInference.cpp");
  const std::string inferDefinitionSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferDefinition.cpp");
  const std::string lateMapAccessBuiltinsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprLateMapAccessBuiltins.cpp");
  const std::string exprTrySource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprTry.cpp");
  const std::string pointerLikeSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprPointerLike.cpp");
  const std::string statementPrintabilitySource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorStatementPrintability.cpp");
  const std::string scalarPointerMemorySource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprScalarPointerMemory.cpp");
  const std::string argumentValidationCollectionsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprArgumentValidationCollections.cpp");
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
  const std::string lowererStructReturnPathSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererStructReturnPathHelpers.cpp");
  const std::string setupTypeCollectionSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererSetupTypeCollectionHelpers.cpp");
  const std::string countAccessSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererCountAccessHelpers.cpp");
  const std::string resultMetadataSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererResultMetadataHelpers.cpp");
  const std::string inlineCallContextSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererInlineCallContextHelpers.cpp");

  REQUIRE(!mapSource.empty());
  REQUIRE(!experimentalSource.empty());
  REQUIRE(!internalSource.empty());
  REQUIRE(!surfacesSource.empty());
  REQUIRE(!registrySource.empty());
  REQUIRE(!semanticsSource.empty());
  REQUIRE(!validatorSource.empty());
  REQUIRE(!callResolutionSource.empty());
  REQUIRE(!callPathHelpersSource.empty());
  REQUIRE(!methodTargetResolutionSource.empty());
  REQUIRE(!templateCoreSource.empty());
  REQUIRE(!templateReceiverSource.empty());
  REQUIRE(!templateExpressionRewriteSource.empty());
  REQUIRE(!inferStructReturnSource.empty());
  REQUIRE(!inferStructReturnHelpersSource.empty());
  REQUIRE(!inferMethodResolutionSource.empty());
  REQUIRE(!inferMethodResolutionHelpersSource.empty());
  REQUIRE(!collectionHelperRewritesSource.empty());
  REQUIRE(!buildInitializerInferenceSource.empty());
  REQUIRE(!inferCollectionDispatchSource.empty());
  REQUIRE(!inferCollectionCompatibilitySource.empty());
  REQUIRE(!inferCollectionDispatchSetupSource.empty());
  REQUIRE(!inferCollectionReturnInferenceSource.empty());
  REQUIRE(!inferDefinitionSource.empty());
  REQUIRE(!lateMapAccessBuiltinsSource.empty());
  REQUIRE(!exprTrySource.empty());
  REQUIRE(!pointerLikeSource.empty());
  REQUIRE(!statementPrintabilitySource.empty());
  REQUIRE(!scalarPointerMemorySource.empty());
  REQUIRE(!argumentValidationCollectionsSource.empty());
  REQUIRE(!collectionAccessValidationSource.empty());
  REQUIRE(!collectionAccessSource.empty());
  REQUIRE(!countCapacityMapBuiltinSource.empty());
  REQUIRE(!emitterCallPathHelpersSource.empty());
  REQUIRE(!emitterReturnInferenceCollectionsSource.empty());
  REQUIRE(!emitterCollectionTypeHelpersSource.empty());
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
  REQUIRE(!lowererStructReturnPathSource.empty());
  REQUIRE(!setupTypeCollectionSource.empty());
  REQUIRE(!countAccessSource.empty());
  REQUIRE(!resultMetadataSource.empty());
  REQUIRE(!inlineCallContextSource.empty());

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
  CHECK(semanticsSource.find("helperName = \"/std/collections/map/\" + helperName") !=
        std::string::npos);
  CHECK(validatorSource.find("path.rfind(\"/map/count__t\"") ==
        std::string::npos);
  CHECK(callResolutionSource.find("\"/map/entry\"") == std::string::npos);
  CHECK(callResolutionSource.find("\"/map/entry__\"") == std::string::npos);
  CHECK(callResolutionSource.find("directExplicitCallPath == \"/map/at\"") ==
        std::string::npos);
  CHECK(callResolutionSource.find("path == \"/std/collections/map/entry\"") ==
        std::string::npos);
  CHECK(callResolutionSource.find("candidatePath == \"/std/collections/map/map\"") ==
        std::string::npos);
  CHECK(callResolutionSource.find("mapHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(callResolutionSource.find("mapConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(callPathHelpersSource.find("name.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(callPathHelpersSource.find("name.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(callPathHelpersSource.find("resolveMapHelperMemberName(name, mapHelperName)") !=
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("const std::string alias = \"/map/\" + resolvedHelperName") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("resolvedPath == \"/map/") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find("candidate.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(methodTargetResolutionSource.find(
            "metadataBackedMapHelperRootAliasMethodName(candidate)") !=
        std::string::npos);
  CHECK(exprMethodResolutionSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(exprMethodResolutionSource.find(
            "metadataBackedCanonicalMapHelperPath(helperName)") !=
        std::string::npos);
  CHECK(templateCoreSource.find("\"/map/entry\"") == std::string::npos);
  CHECK(templateCoreSource.find("\"/map/entry__\"") == std::string::npos);
  CHECK(templateReceiverSource.find("|| resolvedPath == \"/map/") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find(
            "resolvedPath == \"/std/collections/map/count\" || resolvedPath == \"/map/count\"") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("const std::string compatibilityPath = \"/map/\" +") ==
        std::string::npos);
  CHECK(templateExpressionRewriteSource.find("resolvedPath.rfind(\"/map/\", 0) == 0) &&") ==
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
  CHECK(inferStructReturnSource.find(
            "resolveExplicitPublishedMapHelperExprMemberName(rawMethodName") !=
        std::string::npos);
  CHECK(inferStructReturnSource.find("metadataBackedCanonicalMapHelperPath(methodName)") !=
        std::string::npos);
  CHECK(inferStructReturnSource.find(
            "resolveCanonicalCompatibilityMapHelperNameFromResolvedPath(") !=
        std::string::npos);
  CHECK(inferStructReturnHelpersSource.find("isExplicitMapAccessStructReturnCompatibilityCall") ==
        std::string::npos);
  CHECK(inferStructReturnHelpersSource.find("normalized == \"map/at\"") ==
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
  CHECK(inferMethodResolutionHelpersSource.find("const std::string alias = \"/map/\" + selectedHelperName") ==
        std::string::npos);
  CHECK(collectionHelperRewritesSource.find("const std::string alias = \"/map/\" + std::string(helperName)") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find("const std::string alias = \"/map/\" + helperName") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find("normalizedPrefix == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find("normalizedName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(buildInitializerInferenceSource.find("metadataBackedCanonicalMapHelperPath(helperName)") !=
        std::string::npos);
  CHECK(inferCollectionDispatchSource.find("resolvedPath.rfind(\"/map/\"") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSource.find("resolvedPath == \"/map/at\"") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSource.find("resolvedPath == \"/map/contains\"") ==
        std::string::npos);
  CHECK(inferCollectionCompatibilitySource.find("const std::string alias = \"/map/\" + std::string(helperName)") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("resolvedPath == \"/map/at_ref\"") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("hasDefinitionPath(\"/map/\" +") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("namespacePrefix == \"map\"") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("expr.namespacePrefix == \"map\"") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("resolveRootMapHelperAliasPath(path, helperName)") !=
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "\"/std/collections/map/\" + builtinAccessName") ==
        std::string::npos);
  CHECK(inferCollectionReturnInferenceSource.find(
            "metadataBackedCanonicalMapHelperPath(builtinAccessName)") !=
        std::string::npos);
  CHECK(inferDefinitionSource.find("resolvedPath == \"/map/at\"") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("normalizedName == \"map/at_ref\"") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("namespacePrefix == \"map\"") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("methodResolved = \"/map/\" + helperName") ==
        std::string::npos);
  CHECK(exprTrySource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
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
  CHECK(pointerLikeSource.find("appendUnique(\"/std/collections/map/\" + suffix)") ==
        std::string::npos);
  CHECK(pointerLikeSource.find("appendUnique(\"/map/\" + suffix)") ==
        std::string::npos);
  CHECK(pointerLikeSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(pointerLikeSource.find("mapHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(statementPrintabilitySource.find("resolvedPath == \"/map/at\"") ==
        std::string::npos);
  CHECK(statementPrintabilitySource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(statementPrintabilitySource.find("mapHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(scalarPointerMemorySource.find("isExplicitMapAccessHelperPath") ==
        std::string::npos);
  CHECK(scalarPointerMemorySource.find("resolved == \"/map/at\"") ==
        std::string::npos);
  CHECK(scalarPointerMemorySource.find("resolved == \"/std/collections/map/at_ref\"") ==
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find("normalizedName == \"map/at_ref\"") ==
        std::string::npos);
  CHECK(argumentValidationCollectionsSource.find("resolvedBasePath == \"/map/at\"") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("path == \"/map/at\"") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("normalizedName == \"map/at_ref\"") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("namespacePrefix == \"map\"") ==
        std::string::npos);
  CHECK(collectionAccessValidationSource.find("diagnosticTarget.rfind(\"/map/\"") ==
        std::string::npos);
  CHECK(collectionDispatchSetupSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(collectionDispatchSetupSource.find("mapHelperSurfaceMetadataLocal()") !=
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
  CHECK(emitterCallPathHelpersSource.find("resolveCanonicalStdlibSurfaceExprMemberName(\n"
                                          "          expr, StdlibSurfaceId::CollectionsMapHelpers") !=
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
                                                     "                  MapHelperSurfaceBridgeKey") !=
        std::string::npos);
  CHECK(emitterReturnInferenceCollectionsSource.find("return {mapHelperPath(methodName)}") !=
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("eraseCandidate(\"/map/\" + suffix)") ==
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("eraseCandidate(\"/std/collections/map/\" + suffix)") ==
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("normalized.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("std::string_view(\"std/collections/map/\").size()") ==
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("CollectionTypeMapHelperSurfaceBridgeKey") !=
        std::string::npos);
  CHECK(emitterCollectionTypeHelpersSource.find("collectionTypeMapHelperMemberName(resolveExprPath(candidate), false)") !=
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
  CHECK(emitterMethodMetadataSource.find("resolvePublishedCollectionSurfaceMemberName(\n"
                                         "          normalizeCollectionHelperPath(candidate),\n"
                                         "          StdlibSurfaceId::CollectionsMapHelpers") !=
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
                                           "            MapHelperSurfaceBridgeKey") !=
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("pruneMapAccessStructReturnCompatibilityCandidates") ==
        std::string::npos);
  CHECK(statementLowererSource.find("isPrimeMapInsertBody") != std::string::npos);
  CHECK(statementLowererSource.find("rewriteMapInsertHelperStatementToCanonical") != std::string::npos);
  CHECK(statementLowererSource.find("rewriteMapInsertHelperStatementToBuiltin") == std::string::npos);
  CHECK(statementLowererSource.find("/map/at(argsPack") == std::string::npos);
  CHECK(uninitializedStructInferenceSource.find("scopedCallPath == \"/map/at\"") ==
        std::string::npos);
  CHECK(uninitializedStructInferenceSource.find("scopedCallPath == \"/std/collections/map/at\"") ==
        std::string::npos);
  CHECK(uninitializedStructInferenceSource.find("publishedMapHelperName == \"at\"") !=
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
  CHECK(setupTypeMethodTargetSource.find("const std::string rootedMapPrefix") !=
        std::string::npos);
  CHECK(setupTypeMethodTargetSource.find("const std::string canonicalMapPrefix") !=
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
  CHECK(setupTypeMethodCallSource.find("path == \"/std/collections/map/map\"") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("collectionMemberRoot(\"map\", false)") !=
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("std::string(\"std/collections/map/\").size()") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("normalizedMethodName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("collectionMemberPath(\"map\", mapHelperName)") !=
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("\"/std/collections/map/\" + mapHelperName") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("normalized.rfind(\"/map/\", 0)") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("normalized.rfind(\"/std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("collectionMemberPath(\"map\", \"count\")") !=
        std::string::npos);
  CHECK(setupTypeMethodCallSource.find("\"/std/collections/map/count\"") ==
        std::string::npos);
  CHECK(setupTypeCollectionSource.find("const std::string mapPrefix = \"map/\"") ==
        std::string::npos);
  CHECK(setupTypeCollectionSource.find("const std::string stdMapPrefix = \"std/collections/map/\"") ==
        std::string::npos);
  CHECK(setupTypeCollectionSource.find("collectionMemberRoot(\"map\", false)") !=
        std::string::npos);
  CHECK(setupTypeCollectionSource.find("normalizeBuiltinCollectionStructPath(\"map\").substr(1) + \"/\"") !=
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
  CHECK(resultMetadataSource.find("\"/std/collections/map/map\"") ==
        std::string::npos);
  CHECK(resultMetadataSource.find("\"/std/collections/map/map__\"") ==
        std::string::npos);
  CHECK(resultMetadataSource.find("\"std/collections/map/map\"") ==
        std::string::npos);
  CHECK(resultMetadataSource.find("\"std/collections/map/map__\"") ==
        std::string::npos);
  CHECK(inlineCallContextSource.find("collectionMemberRoot(\"map\") + \"map__\"") !=
        std::string::npos);
  CHECK(inlineCallContextSource.find("\"/std/collections/map/map__\"") ==
        std::string::npos);
  CHECK(statementLowererSource.find("callee->fullPath.rfind(\"/std/collections/internal_map/insertImpl__\", 0)") !=
        std::string::npos);
  CHECK(statementLowererSource.find("canonicalStatementMapHelperPath(\"insert\")") !=
        std::string::npos);
  CHECK(statementLowererSource.find("rewrittenStmt.name = \"/std/collections/map/insert\"") ==
        std::string::npos);
  CHECK(lowererCallHelpersSource.find("const std::string unrooted = \"map/\" + std::string(helperName)") ==
        std::string::npos);
  CHECK(lowererCallHelpersSource.find("resolveMapHelperDefinitionMember(directHelperPath, helperName)") !=
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
  CHECK(emitterMethodTypeInferenceSource.find("isCanonicalMapHelperMemberPath(normalized, helperName)") !=
        std::string::npos);
  CHECK(emitterMethodTypeInferenceSource.find("isMapImportAliasHelperMemberPath(resolvedExprPath, helperName)") !=
        std::string::npos);
  CHECK(emitterHelpersTypesSource.find("base += \"map/Map\"") == std::string::npos);
  CHECK(emitterHelpersTypesSource.find("experimentalCollectionTypePathLocal(\"map\", \"Map\"") !=
        std::string::npos);
  CHECK(emitterHelpersTypesSource.find("name == \"/map\"") == std::string::npos);
  CHECK(emitterHelpersTypesSource.find("name == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(emitterHelpersTypesSource.find("name.rfind(\"/std/collections/map<\"") ==
        std::string::npos);
  CHECK(nativeTailSource.find("hasSemanticMapReadHelperDefinition") != std::string::npos);
  CHECK(nativeTailSource.find("isMapReadHelperName(directMapReadHelperName)") != std::string::npos);
  CHECK(lowerStatementsExprSource.find("Keep direct canonical map access helpers") == std::string::npos);
  CHECK(lowerStatementsExprSource.find("keepsBuiltinCanonicalMapHelperReturn") == std::string::npos);
  CHECK(lowerStatementsBindingsSource.find("resolvePublishedStdlibSurfaceMemberName(\n"
                                           "                  rawPath,\n"
                                           "                  StdlibSurfaceId::CollectionsMapHelpers") !=
        std::string::npos);
  CHECK(lowerStatementsBindingsSource.find("rawPath.rfind(\"/map/\", 0)") ==
        std::string::npos);
  CHECK(lowerStatementsBindingsSource.find("rawPath.rfind(\"/std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(tailDispatchSource.find("rewrittenExpr.name = ir_lowerer::collectionMemberPath(\"map\", \"insert\")") !=
        std::string::npos);
  CHECK(lowerEmitExprCollectionSource.find("resolvePublishedStdlibSurfaceMemberName(\n"
                                           "                    rawPath,\n"
                                           "                    MapHelperSurfaceId") !=
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
  CHECK(builtinNameHelpersSource.find("resolvesMapHelperSurfacePath(scopedName)") !=
        std::string::npos);
  CHECK(lowererHelpersSource.find("candidate.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(lowererHelpersSource.find("resolvesMapHelperSurfacePath(candidate)") !=
        std::string::npos);
  CHECK(inlinePackedArgsSource.find("rewrittenExpr.name = \"/map/map\"") == std::string::npos);
  CHECK(inlinePackedArgsSource.find("rewriteBuiltinMapConstructorExpr") == std::string::npos);
  CHECK(inlinePackedArgsSource.find("rewrittenExpr.name = collectionMemberPath(\"map\", \"map\")") !=
        std::string::npos);
  CHECK(inlineParamHelpersSource.find("rewrittenExpr.name = \"/map/map\"") == std::string::npos);
  CHECK(inlineParamHelpersSource.find("rewriteBuiltinMapConstructorExpr") == std::string::npos);
  CHECK(inlineParamHelpersSource.find("rewrittenExpr.name = collectionMemberPath(\"map\", \"map\")") !=
        std::string::npos);
  CHECK(packedResultSource.find("rewrittenExpr.name = \"/map/map\"") == std::string::npos);
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
