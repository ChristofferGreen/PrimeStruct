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
      "src/ir_lowerer/IrLowererOperatorCollectionMutationHelpers.cpp",
      "src/ir_lowerer/IrLowererPackedResultHelpers.cpp",
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
  const std::string callResolutionSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprCallResolution.cpp");
  const std::string templateCoreSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphCoreUtilities.h");
  const std::string templateReceiverSource =
      readText(repoRoot() / "src" / "semantics" /
               "TemplateMonomorphExperimentalCollectionReceiverResolution.h");
  const std::string inferStructReturnSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferStructReturn.cpp");
  const std::string inferStructReturnHelpersSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferStructReturnHelpers.cpp");
  const std::string inferCollectionDispatchSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferCollectionDispatch.cpp");
  const std::string inferCollectionDispatchSetupSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferCollectionDispatchSetup.cpp");
  const std::string inferDefinitionSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorInferDefinition.cpp");
  const std::string lateMapAccessBuiltinsSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprLateMapAccessBuiltins.cpp");
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
  const std::string countCapacityMapBuiltinSource =
      readText(repoRoot() / "src" / "semantics" /
               "SemanticsValidatorExprCountCapacityBuiltins.cpp");
  const std::string statementLowererSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererStatementCallEmission.cpp");
  const std::string lowerStatementsExprSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererLowerStatementsExpr.h");
  const std::string inlineNativeSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererInlineNativeCallDispatch.cpp");
  const std::string nativeTailSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererNativeTailDispatch.cpp");
  const std::string tailDispatchSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTailDispatch.h");
  const std::string inlinePackedArgsSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererInlinePackedArgs.cpp");
  const std::string inlineParamHelpersSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererInlineParamHelpers.cpp");
  const std::string packedResultSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererPackedResultHelpers.cpp");

  REQUIRE(!mapSource.empty());
  REQUIRE(!experimentalSource.empty());
  REQUIRE(!internalSource.empty());
  REQUIRE(!surfacesSource.empty());
  REQUIRE(!registrySource.empty());
  REQUIRE(!semanticsSource.empty());
  REQUIRE(!callResolutionSource.empty());
  REQUIRE(!templateCoreSource.empty());
  REQUIRE(!templateReceiverSource.empty());
  REQUIRE(!inferStructReturnSource.empty());
  REQUIRE(!inferStructReturnHelpersSource.empty());
  REQUIRE(!inferCollectionDispatchSource.empty());
  REQUIRE(!inferCollectionDispatchSetupSource.empty());
  REQUIRE(!inferDefinitionSource.empty());
  REQUIRE(!lateMapAccessBuiltinsSource.empty());
  REQUIRE(!statementPrintabilitySource.empty());
  REQUIRE(!scalarPointerMemorySource.empty());
  REQUIRE(!argumentValidationCollectionsSource.empty());
  REQUIRE(!collectionAccessValidationSource.empty());
  REQUIRE(!collectionAccessSource.empty());
  REQUIRE(!countCapacityMapBuiltinSource.empty());
  REQUIRE(!statementLowererSource.empty());
  REQUIRE(!lowerStatementsExprSource.empty());
  REQUIRE(!inlineNativeSource.empty());
  REQUIRE(!nativeTailSource.empty());
  REQUIRE(!tailDispatchSource.empty());
  REQUIRE(!inlinePackedArgsSource.empty());
  REQUIRE(!inlineParamHelpersSource.empty());
  REQUIRE(!packedResultSource.empty());

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
  CHECK(callResolutionSource.find("\"/map/entry\"") == std::string::npos);
  CHECK(callResolutionSource.find("\"/map/entry__\"") == std::string::npos);
  CHECK(callResolutionSource.find("directExplicitCallPath == \"/map/at\"") ==
        std::string::npos);
  CHECK(templateCoreSource.find("\"/map/entry\"") == std::string::npos);
  CHECK(templateCoreSource.find("\"/map/entry__\"") == std::string::npos);
  CHECK(templateReceiverSource.find("|| resolvedPath == \"/map/") ==
        std::string::npos);
  CHECK(inferStructReturnSource.find("\"/std/collections/map/\" + methodName, \"/map/\" + methodName") ==
        std::string::npos);
  CHECK(inferStructReturnSource.find("isExplicitMapAccessStructReturnCompatibilityCall") ==
        std::string::npos);
  CHECK(inferStructReturnHelpersSource.find("isExplicitMapAccessStructReturnCompatibilityCall") ==
        std::string::npos);
  CHECK(inferStructReturnHelpersSource.find("normalized == \"map/at\"") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSource.find("resolvedPath.rfind(\"/map/\"") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSource.find("resolvedPath == \"/map/at\"") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSource.find("resolvedPath == \"/map/contains\"") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("resolvedPath == \"/map/at_ref\"") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("namespacePrefix == \"map\"") ==
        std::string::npos);
  CHECK(inferCollectionDispatchSetupSource.find("expr.namespacePrefix == \"map\"") ==
        std::string::npos);
  CHECK(inferDefinitionSource.find("resolvedPath == \"/map/at\"") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("normalizedName == \"map/at_ref\"") ==
        std::string::npos);
  CHECK(lateMapAccessBuiltinsSource.find("namespacePrefix == \"map\"") ==
        std::string::npos);
  CHECK(statementPrintabilitySource.find("resolvedPath == \"/map/at\"") ==
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
  CHECK(collectionAccessSource.find("resolvedPath == \"/map/at_ref\" ||\n"
                                    "            resolvedPath == \"/std/collections/map/at_ref\"") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("normalizedName.rfind(\"map/\", 0) == 0) {\n"
                                    "          const std::string helperName =") ==
        std::string::npos);
  CHECK(collectionAccessSource.find("expr.namespacePrefix == \"map\"") ==
        std::string::npos);
  CHECK(countCapacityMapBuiltinSource.find("/std/collections/map/count") ==
        std::string::npos);
  CHECK(countCapacityMapBuiltinSource.find("/std/collections/map/at") ==
        std::string::npos);
  CHECK(countCapacityMapBuiltinSource.find("canonicalizeExperimentalMapHelperResolvedPath") ==
        std::string::npos);
  CHECK(statementLowererSource.find("isPrimeMapInsertBody") != std::string::npos);
  CHECK(statementLowererSource.find("rewriteMapInsertHelperStatementToCanonical") != std::string::npos);
  CHECK(statementLowererSource.find("rewriteMapInsertHelperStatementToBuiltin") == std::string::npos);
  CHECK(statementLowererSource.find("/map/at(argsPack") == std::string::npos);
  CHECK(statementLowererSource.find("callee->fullPath.rfind(\"/std/collections/internal_map/insertImpl__\", 0)") !=
        std::string::npos);
  CHECK(statementLowererSource.find("rewrittenStmt.name = \"/std/collections/map/insert\"") != std::string::npos);
  CHECK(inlineNativeSource.find("return helperName == \"insert\" || helperName == \"insert_ref\"") ==
        std::string::npos);
  CHECK(inlineNativeSource.find("return helperName == \"at\" || helperName == \"at_ref\"") ==
        std::string::npos);
  CHECK(inlineNativeSource.find("return helperName == \"count\" || helperName == \"count_ref\"") ==
        std::string::npos);
  CHECK(inlineNativeSource.find("emitCanonicalInlineDefinitionCall(expr, *callee)") != std::string::npos);
  CHECK(nativeTailSource.find("hasSemanticMapReadHelperDefinition") != std::string::npos);
  CHECK(nativeTailSource.find("isMapReadHelperName(directMapReadHelperName)") != std::string::npos);
  CHECK(lowerStatementsExprSource.find("Keep direct canonical map access helpers") == std::string::npos);
  CHECK(lowerStatementsExprSource.find("keepsBuiltinCanonicalMapHelperReturn") == std::string::npos);
  CHECK(tailDispatchSource.find("rewrittenExpr.name = ir_lowerer::collectionMemberPath(\"map\", \"insert\")") !=
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
