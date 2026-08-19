#include "third_party/doctest.h"

#include "test_stdlib_map_ownership_shared.h"

TEST_SUITE_BEGIN("primestruct.stdlib.map_ownership");

TEST_CASE("canonical map surface owns standalone stdlib implementation collection access and emitter lowering") {
  const MapOwnershipSources s = loadMapOwnershipSources();
  CHECK(s.exprSource.find("shouldBuiltinValidateBareKeyValueContainsCall") !=
        std::string::npos);
  CHECK(s.exprSource.find("shouldBuiltinValidateBareMapContainsCall") ==
        std::string::npos);
  CHECK(s.exprSource.find("handledMapSoaBuiltin") == std::string::npos);
  CHECK(s.exprSource.find("handledKeyValueSoaBuiltin") != std::string::npos);
  CHECK(s.exprSource.find("handledLateMapAccessBuiltin") == std::string::npos);
  CHECK(s.exprSource.find("handledLateKeyValueAccessBuiltin") ==
        std::string::npos);
  CHECK(s.builtinContextSetupSource.find(
            "shouldBuiltinValidateBareKeyValueAccessCall") != std::string::npos);
  CHECK(s.builtinContextSetupSource.find("shouldBuiltinValidateBareMapAccessCall") ==
        std::string::npos);
  CHECK(s.collectionAccessSetupSource.find(
            "shouldBuiltinValidateBareKeyValueContainsCall") !=
        std::string::npos);
  CHECK(s.collectionAccessSetupSource.find(
            "shouldBuiltinValidateBareMapContainsCall") == std::string::npos);
  CHECK(s.collectionAccessSource.find("shouldBuiltinValidateBareKeyValueAccessCall") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find("shouldBuiltinValidateBareMapAccessCall") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find(
            "shouldBuiltinValidateBareKeyValueAccessCall") != std::string::npos);
  CHECK(s.collectionAccessValidationSource.find(
            "shouldBuiltinValidateBareMapAccessCall") == std::string::npos);
  CHECK(s.lateCollectionAccessFallbacksSource.find(
            "shouldBuiltinValidateBareKeyValueAccessCall") != std::string::npos);
  CHECK(s.lateCollectionAccessFallbacksSource.find(
            "shouldBuiltinValidateBareMapAccessCall") == std::string::npos);
  CHECK(s.lateCollectionAccessFallbacksSource.find("keyValueKeyTypeOut") !=
        std::string::npos);
  CHECK(s.lateCollectionAccessFallbacksSource.find("mapKeyTypeOut") ==
        std::string::npos);
  CHECK(s.lateMapAccessBuiltinsSource.find(
            "shouldBuiltinValidateBareKeyValueTryAtCall") == std::string::npos);
  CHECK(s.lateMapAccessBuiltinsSource.find("shouldBuiltinValidateBareMapTryAtCall") ==
        std::string::npos);
  CHECK(s.mapSoaBuiltinsSource.find(
            "shouldBuiltinValidateBareKeyValueContainsCall") !=
        std::string::npos);
  CHECK(s.mapSoaBuiltinsSource.find("shouldBuiltinValidateBareMapContainsCall") ==
        std::string::npos);
  CHECK(s.mapSoaBuiltinsSource.find("keyValueKeyType") != std::string::npos);
  CHECK(s.lateMapSoaBuiltinsSource.find("keyValueKeyTypeOut") !=
        std::string::npos);
  CHECK(s.lateMapSoaBuiltinsSource.find("mapKeyTypeOut") == std::string::npos);
  CHECK(s.mapSoaBuiltinsSource.find("std::string mapKeyType") ==
        std::string::npos);
  CHECK(s.mapSoaBuiltinsSource.find("const std::string &mapKeyType") ==
        std::string::npos);
  CHECK(s.collectionAccessSetupSource.find(
            "isIndexedArgsPackKeyValueReceiverTarget") != std::string::npos);
  CHECK(s.collectionAccessSetupSource.find("isIndexedArgsPackMapReceiverTarget") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("isIndexedArgsPackKeyValueReceiverTarget") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find("isIndexedArgsPackMapReceiverTarget") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("isLocalRootMapAliasReceiverCall") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("isLocalRootKeyValueAliasReceiverCall") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find(
            "isIndexedArgsPackKeyValueReceiverTarget") != std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("isIndexedArgsPackMapReceiverTarget") ==
        std::string::npos);
  CHECK(s.privateExprValidationSource.find(
            "isKeyValueLikeBareAccessReceiverTarget") != std::string::npos);
  CHECK(s.privateExprValidationSource.find("isMapLikeBareAccessReceiverTarget") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find(
            "isKeyValueLikeBareAccessReceiverTarget") == std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("isMapLikeBareAccessReceiverTarget") ==
        std::string::npos);
  CHECK(s.lateCollectionAccessFallbacksSource.find(
            "isIndexedArgsPackKeyValueReceiverTarget") != std::string::npos);
  CHECK(s.lateCollectionAccessFallbacksSource.find(
            "isIndexedArgsPackMapReceiverTarget") == std::string::npos);
  CHECK(s.lateCollectionAccessFallbacksSource.find(
            "isKeyValueLikeBareAccessReceiverTarget") != std::string::npos);
  CHECK(s.lateCollectionAccessFallbacksSource.find(
            "isMapLikeBareAccessReceiverTarget") == std::string::npos);
  CHECK(s.lateMapAccessBuiltinsSource.find(
            "isIndexedArgsPackKeyValueReceiverTarget(") == std::string::npos);
  CHECK(s.lateMapAccessBuiltinsSource.find("isIndexedArgsPackMapReceiverTarget(") ==
        std::string::npos);
  CHECK(s.lateMapAccessBuiltinsSource.find("isExperimentalKeyValueReceiver") ==
        std::string::npos);
  CHECK(s.lateMapAccessBuiltinsSource.find("isExperimentalMapReceiver") ==
        std::string::npos);
  CHECK(s.lateMapAccessBuiltinsSource.find("rewrittenKeyValueAccessCall") ==
        std::string::npos);
  CHECK(s.lateMapAccessBuiltinsSource.find("rewrittenKeyValueHelperCall") ==
        std::string::npos);
  CHECK(s.exprTrySource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.exprTrySource.find("getDirectMapHelperCompatibilityPath") ==
        std::string::npos);
  CHECK(s.exprTrySource.find("removedMapCompatibilityPath") ==
        std::string::npos);
  CHECK(s.exprTrySource.find("getDirectKeyValueHelperCompatibilityPath") !=
        std::string::npos);
  CHECK(s.exprTrySource.find("removedKeyValueCompatibilityPath") !=
        std::string::npos);
  CHECK(s.exprTrySource.find("isBareKeyValueTryAtFallback") !=
        std::string::npos);
  CHECK(s.exprTrySource.find("isBareMapTryAtFallback") == std::string::npos);
  CHECK(s.exprTrySource.find("allowCurrentMapWrapperTryAt") ==
        std::string::npos);
  CHECK(s.exprTrySource.find("allowCurrentKeyValueWrapperTryAt") !=
        std::string::npos);
  CHECK(s.exprTrySource.find("metadataBackedCanonicalKeyValueHelperPath(\"tryAt\")") !=
        std::string::npos);
  CHECK(s.exprTrySource.find(
            "metadataBackedCanonicalKeyValueHelperPath(\"tryAt_ref\")") !=
        std::string::npos);
  CHECK(s.mapSoaBuiltinsSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.mapSoaBuiltinsSource.find(
            "metadataBackedCanonicalKeyValueHelperPath(helperName)") !=
        std::string::npos);
  CHECK(s.mapSoaBuiltinsSource.find("keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.mapSoaBuiltinsSource.find("canonicalMapHelperPathLocal(") ==
        std::string::npos);
  CHECK(s.mapSoaBuiltinsSource.find("isCanonicalMapHelperResolvedPath(") ==
        std::string::npos);
  CHECK(s.mapSoaBuiltinsSource.find("hasBareKeyValueOperands") !=
        std::string::npos);
  CHECK(s.mapSoaBuiltinsSource.find("hasBareMapOperands") == std::string::npos);
  CHECK(s.mapSoaBuiltinsSource.find("failKeyValueSoaBuiltinDiagnostic") !=
        std::string::npos);
  CHECK(s.mapSoaBuiltinsSource.find("failMapSoaBuiltinDiagnostic") ==
        std::string::npos);
  CHECK(s.mapSoaBuiltinsSource.find("hasBareKeyValueContainsDefinition") !=
        std::string::npos);
  CHECK(s.mapSoaBuiltinsSource.find("hasBareMapContainsBuiltinDefinition") ==
        std::string::npos);
  CHECK(s.mapSoaBuiltinsSource.find("validateMapContainsKeyExpr") ==
        std::string::npos);
  CHECK(s.mapSoaBuiltinsSource.find("validateKeyValueContainsKeyExpr") !=
        std::string::npos);
  CHECK(s.mapSoaBuiltinsSource.find("canonicalKeyValueHelperPathLocal(") !=
        std::string::npos);
  CHECK(s.mapSoaBuiltinsSource.find("isCanonicalKeyValueHelperResolvedPath(") !=
        std::string::npos);
  CHECK(s.pointerLikeSource.find("appendUnique(\"/std/collections/map/\" + suffix)") ==
        std::string::npos);
  CHECK(s.pointerLikeSource.find("appendUnique(\"/map/\" + suffix)") ==
        std::string::npos);
  CHECK(s.pointerLikeSource.find("const std::string mapPrefix = \"map/\"") ==
        std::string::npos);
  CHECK(s.pointerLikeSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.pointerLikeSource.find("keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.pointerLikeSource.find("unrootedMapImportAliasHelperPrefix()") ==
        std::string::npos);
  CHECK(s.pointerLikeSource.find("unrootedCanonicalMapHelperPrefix()") ==
        std::string::npos);
  CHECK(s.pointerLikeSource.find("const std::string mapPrefix") ==
        std::string::npos);
  CHECK(s.pointerLikeSource.find("const std::string stdMapPrefix") ==
        std::string::npos);
  CHECK(s.pointerLikeSource.find("unrootedKeyValueImportAliasHelperPrefix()") !=
        std::string::npos);
  CHECK(s.pointerLikeSource.find("unrootedCanonicalKeyValueHelperPrefix()") !=
        std::string::npos);
  CHECK(s.pointerLikeSource.find("keyValueAliasPrefix") !=
        std::string::npos);
  CHECK(s.pointerLikeSource.find("canonicalKeyValuePrefix") !=
        std::string::npos);
  CHECK(s.statementPrintabilitySource.find("resolvedPath == \"/map/at\"") ==
        std::string::npos);
  CHECK(s.statementPrintabilitySource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.statementPrintabilitySource.find("keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.statementPrintabilitySource.find("isCanonicalMapHelperResolvedPath(") ==
        std::string::npos);
  CHECK(s.statementPrintabilitySource.find("isCanonicalKeyValueHelperResolvedPath(") !=
        std::string::npos);
  CHECK(s.statementPrintabilitySource.find(
            "isUnspecializedExperimentalMapBackingBaseForPrintability") ==
        std::string::npos);
  CHECK(s.statementPrintabilitySource.find(
            "isUnspecializedExperimentalKeyValueBackingBaseForPrintability") !=
        std::string::npos);
  CHECK(s.statementPrintabilitySource.find("keyValueValueType") !=
        std::string::npos);
  CHECK(s.statementPrintabilitySource.find("std::string mapValueType") ==
        std::string::npos);
  CHECK(s.statementPrintabilitySource.find("resolveMapValueType") ==
        std::string::npos);
  CHECK(s.statementPrintabilitySource.find("resolveKeyValueValueType") !=
        std::string::npos);
  CHECK(s.statementPrintabilitySource.find("extractMapValueTypeFromReturn") ==
        std::string::npos);
  CHECK(s.statementPrintabilitySource.find("extractKeyValueValueTypeFromReturn") !=
        std::string::npos);
  CHECK(s.statementBodyArgumentsSource.find("isRemovedMapCompatibilityHelper(") ==
        std::string::npos);
  CHECK(s.statementBodyArgumentsSource.find("mapHelperSurfaceMetadata(") ==
        std::string::npos);
  CHECK(s.statementBodyArgumentsSource.find("resolveMapHelperPathMemberName(") ==
        std::string::npos);
  CHECK(s.statementBodyArgumentsSource.find("canonicalMapHelperPath(") ==
        std::string::npos);
  CHECK(s.statementBodyArgumentsSource.find("legacyMapHelperPath(") ==
        std::string::npos);
  CHECK(s.statementBodyArgumentsSource.find("preferredMapBodyArgumentTarget") ==
        std::string::npos);
  CHECK(s.statementBodyArgumentsSource.find("isMapReceiverExpr") ==
        std::string::npos);
  CHECK(s.exprBodyArgumentsSource.find("remappedRemovedMapTarget") ==
        std::string::npos);
  CHECK(s.exprBodyArgumentsSource.find("remappedRemovedKeyValueTarget") !=
        std::string::npos);
  CHECK(s.statementBodyArgumentsSource.find(
            "isRemovedKeyValueCompatibilityHelper(") !=
        std::string::npos);
  CHECK(s.statementBodyArgumentsSource.find(
            "keyValueHelperSurfaceMetadataForBodyArguments(") !=
        std::string::npos);
  CHECK(s.statementBodyArgumentsSource.find(
            "resolveKeyValueHelperPathMemberName(") !=
        std::string::npos);
  CHECK(s.statementBodyArgumentsSource.find(
            "canonicalKeyValueHelperPathForBodyArguments(") !=
        std::string::npos);
  CHECK(s.statementBodyArgumentsSource.find("legacyKeyValueHelperAliasPath(") !=
        std::string::npos);
  CHECK(s.statementBodyArgumentsSource.find("preferredKeyValueBodyArgumentTarget") !=
        std::string::npos);
  CHECK(s.statementBodyArgumentsSource.find("isKeyValueReceiverExpr") !=
        std::string::npos);
  CHECK(s.scalarPointerMemorySource.find("isExplicitMapAccessHelperPath") ==
        std::string::npos);
  CHECK(s.scalarPointerMemorySource.find("resolved == \"/map/at\"") ==
        std::string::npos);
  CHECK(s.scalarPointerMemorySource.find("resolved == \"/std/collections/map/at_ref\"") ==
        std::string::npos);
  CHECK(s.scalarPointerMemorySource.find("collectionName == \"map\"") ==
        std::string::npos);
  CHECK(s.scalarPointerMemorySource.find("isKeyValueCollectionTypeName(collectionName)") !=
        std::string::npos);
  CHECK(s.scalarPointerMemorySource.find("isMapLikeCollectionExpr") ==
        std::string::npos);
  CHECK(s.scalarPointerMemorySource.find("isKeyValueLikeCollectionExpr") !=
        std::string::npos);
  CHECK(s.statementSource.find(
            "isExperimentalCollectionBackingTypeName(\"map\", \"Map\", typeName)") ==
        std::string::npos);
  CHECK(s.statementSource.find("base == \"map\"") == std::string::npos);
  CHECK(s.statementSource.find("isQualifiedExperimentalKeyValueBackingTypeName(typeName)") !=
        std::string::npos);
  CHECK(s.statementSource.find("isSpecializedExperimentalMapBackingPath") ==
        std::string::npos);
  CHECK(s.statementSource.find("isSpecializedExperimentalKeyValueBackingPath") !=
        std::string::npos);
  CHECK(s.statementSource.find("mapCollectionAliasTokenForStatementValidation()") !=
        std::string::npos);
  CHECK(s.statementSource.find("base == mapCollectionAlias") ==
        std::string::npos);
  CHECK(s.statementSource.find("base == keyValueCollectionAlias") !=
        std::string::npos);
  CHECK(s.statementSource.find("extractMapArgsFromAnyType") ==
        std::string::npos);
  CHECK(s.statementSource.find("extractKeyValueArgsFromAnyType") !=
        std::string::npos);
  CHECK(s.statementSource.find("expectedMapKeyType") ==
        std::string::npos);
  CHECK(s.statementSource.find("expectedMapValueType") ==
        std::string::npos);
  CHECK(s.statementSource.find("expectedKeyValueKeyType") !=
        std::string::npos);
  CHECK(s.statementSource.find("expectedKeyValueValueType") !=
        std::string::npos);
  CHECK(s.statementSource.find("actualKeyValueKeyType") !=
        std::string::npos);
  CHECK(s.statementSource.find("actualKeyValueValueType") !=
        std::string::npos);
  CHECK(s.statementSource.find("actualMapKeyType") ==
        std::string::npos);
  CHECK(s.statementSource.find("actualMapValueType") ==
        std::string::npos);
  CHECK(s.statementContainerHelpersSource.find("isExperimentalMapBackingStructPath") ==
        std::string::npos);
  CHECK(s.statementContainerHelpersSource.find(
            "isExperimentalKeyValueBackingStructPath") != std::string::npos);
  CHECK(s.buildDirectCallBindingSource.find("isExperimentalMapBackingReturnStruct") ==
        std::string::npos);
  CHECK(s.buildDirectCallBindingSource.find(
            "isExperimentalKeyValueBackingReturnStruct") != std::string::npos);
  CHECK(s.argumentValidationSource.find("normalizedBase == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(s.argumentValidationSource.find("diagnosticResolved != \"/std/collections/map/at\"") ==
        std::string::npos);
  CHECK(s.argumentValidationSource.find("normalizedExpectedBase == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(s.argumentValidationSource.find(
            "resolveCanonicalArgumentValidationMapAccessHelper(") ==
        std::string::npos);
  CHECK(s.argumentValidationSource.find("canonicalMapAccessHelperName") ==
        std::string::npos);
  CHECK(s.argumentValidationSource.find(
            "resolveCanonicalArgumentValidationKeyValueAccessHelper(") !=
        std::string::npos);
  CHECK(s.argumentValidationSource.find("canonicalKeyValueAccessHelperName") !=
        std::string::npos);
  CHECK(s.argumentValidationSource.find("keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.argumentValidationSource.find("actualKeyValueKeyType") !=
        std::string::npos);
  CHECK(s.argumentValidationSource.find("actualKeyValueValueType") !=
        std::string::npos);
  CHECK(s.argumentValidationSource.find("actualMapKeyType") ==
        std::string::npos);
  CHECK(s.argumentValidationSource.find("actualMapValueType") ==
        std::string::npos);
  CHECK(s.argumentValidationSource.find("expectedMapKeyType") ==
        std::string::npos);
  CHECK(s.argumentValidationSource.find("expectedMapValueType") ==
        std::string::npos);
  CHECK(s.argumentValidationSource.find("expectedKeyValueKeyType") !=
        std::string::npos);
  CHECK(s.argumentValidationSource.find("expectedKeyValueValueType") !=
        std::string::npos);
  CHECK(s.argumentValidationSource.find("resolveMapValueType(") ==
        std::string::npos);
  CHECK(s.argumentValidationSource.find("resolveKeyValueValueType(") !=
        std::string::npos);
  CHECK(s.argumentValidationSource.find("resolveMapKeyType(") ==
        std::string::npos);
  CHECK(s.argumentValidationSource.find("resolveKeyValueKeyType(") !=
        std::string::npos);
  CHECK(s.argumentValidationSource.find("actualMapBase") == std::string::npos);
  CHECK(s.argumentValidationSource.find("actualMapTemplateArgs") ==
        std::string::npos);
  CHECK(s.argumentValidationSource.find("actualKeyValueBase") !=
        std::string::npos);
  CHECK(s.argumentValidationSource.find("actualKeyValueTemplateArgs") !=
        std::string::npos);
  CHECK(s.argumentValidationSource.find("isCompatibleExperimentalMapReceiver") ==
        std::string::npos);
  CHECK(s.argumentValidationSource.find(
            "isCompatibleExperimentalKeyValueReceiver") != std::string::npos);
  CHECK(s.argumentValidationSource.find(
            "maybePreferExplicitCanonicalMapKeyDiagnostic") ==
        std::string::npos);
  CHECK(s.argumentValidationSource.find(
            "maybePreferExplicitCanonicalKeyValueKeyDiagnostic") !=
        std::string::npos);
  CHECK(s.argumentValidationSource.find("const bool mapConstructorCall") ==
        std::string::npos);
  CHECK(s.argumentValidationSource.find("const bool keyValueConstructorCall") !=
        std::string::npos);
  CHECK(s.collectionLiteralValidationSource.find("collectMapLiteralEntries") ==
        std::string::npos);
  CHECK(s.collectionLiteralValidationSource.find("isMapLiteralAssignPair") ==
        std::string::npos);
  CHECK(s.collectionLiteralValidationSource.find("map literal requires") ==
        std::string::npos);
  CHECK(s.operatorCollectionMutationSource.find("collectMapLiteralEntries") ==
        std::string::npos);
  CHECK(s.operatorCollectionMutationSource.find("isMapLiteralAssignPair") ==
        std::string::npos);
  CHECK(s.operatorCollectionMutationSource.find("map literal key type mismatch") ==
        std::string::npos);
  CHECK(s.operatorCollectionMutationSource.find("map literal value type mismatch") ==
        std::string::npos);
  CHECK(s.operatorCollectionMutationSource.find(
            "native backend requires collection literal key/value types") ==
        std::string::npos);
  CHECK(s.textFilterPipelinePassSource.find("name == \"map\"") ==
        std::string::npos);
  CHECK(s.textFilterPipelinePassSource.find(
            "const std::string names[] = {\"array\", \"vector\", publicSoaName}") !=
        std::string::npos);
  CHECK(s.resolvedCallArgumentsSource.find("mapConstructorArgumentMatchesExactType") ==
        std::string::npos);
  CHECK(s.resolvedCallArgumentsSource.find(
            "keyValueConstructorArgumentMatchesExactType") !=
        std::string::npos);
  CHECK(s.resolvedCallArgumentsSource.find(
            "validateExplicitCanonicalMapConstructorArguments") ==
        std::string::npos);
  CHECK(s.resolvedCallArgumentsSource.find(
            "validateExplicitCanonicalKeyValueConstructorArguments") !=
        std::string::npos);
  CHECK(s.resolvedCallArgumentsSource.find("isCanonicalMapConstructorResolvedPath(") !=
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find("normalizedName == \"map/at_ref\"") ==
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find("resolvedBasePath == \"/map/at\"") ==
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find(
            "mapHelperSurfaceMetadataForArgumentValidation(") ==
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find("canonicalMapHelperPathLocal(") ==
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find(
            "resolveCanonicalMapHelperNameFromSpelling(") ==
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find(
            "isCanonicalMapAccessResolvedPath(") ==
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find(
            "getCanonicalMapAccessBuiltinName(") ==
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find("resolvedMapHelperName") ==
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find("receiverIsMap") ==
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find("receiverIsKeyValue") !=
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find("keyValueValueType") !=
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find("std::string mapValueType") ==
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find("experimentalMapKeyType") ==
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find("experimentalMapValueType") ==
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find("experimentalKeyValueKeyType") !=
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find("experimentalKeyValueValueType") !=
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find("resolveMapValueType(") ==
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find("resolveKeyValueValueType(") !=
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find(
            "extractExperimentalMapFieldTypesFromStructPath") ==
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find(
            "extractExperimentalKeyValueFieldTypesFromStructPath") !=
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find(
            "methodMapAccessDefinitionReturnsString") ==
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find("isExplicitMapAccessPath") ==
        std::string::npos);
  CHECK(s.namedArgumentBuiltinsSource.find("isCanonicalMapAccessHelperResolvedPath(") ==
        std::string::npos);
  CHECK(s.namedArgumentBuiltinsSource.find(
            "isCanonicalKeyValueAccessHelperResolvedPath(") !=
        std::string::npos);
  CHECK(s.passesDiagnosticsSource.find("isCanonicalMapAccessHelperPath(") ==
        std::string::npos);
  CHECK(s.passesDiagnosticsSource.find("isVisibleCanonicalMapAccessBuiltin") ==
        std::string::npos);
  CHECK(s.passesDiagnosticsSource.find("isCanonicalKeyValueAccessHelperPath(") !=
        std::string::npos);
  CHECK(s.passesDiagnosticsSource.find("isVisibleCanonicalKeyValueAccessBuiltin") !=
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find(
            "keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find(
            "keyValueHelperSurfaceMetadataForArgumentValidation(") !=
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find(
            "canonicalKeyValueHelperPathForArgumentValidation(") !=
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find(
            "resolveCanonicalKeyValueHelperNameFromSpelling(") !=
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find(
            "isCanonicalKeyValueAccessResolvedPath(") !=
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find(
            "getCanonicalKeyValueAccessBuiltinName(") !=
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find("resolvedKeyValueHelperName") !=
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find(
            "methodKeyValueAccessDefinitionReturnsString") !=
        std::string::npos);
  CHECK(s.argumentValidationCollectionsSource.find("isExplicitKeyValueAccessPath") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("path == \"/map/at\"") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("normalizedName == \"map/at_ref\"") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("namespacePrefix == \"map\"") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("diagnosticTarget.rfind(\"/map/\"") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("metadataBackedCanonicalKeyValueHelperPath(") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("isCanonicalMapAccessHelperName(") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("isSourceSpelledCanonicalMapAccessCall(") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find(
            "isExplicitCanonicalMapAccessCall") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("validateMethodMapAccessBuiltin") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("isExplicitMapAccessHelper") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("keyValueKeyType") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("keyValueKeyTypeOut") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("keyValueValueType") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("reorderedKeyValueKeyType") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("reorderedKeyValueValueType") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("reorderedKeyValue") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("isKeyValue") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("isExperimentalKeyValue") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("failCollectionAccessMapKeyMismatch") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find(
            "failCollectionAccessKeyValueKeyMismatch") != std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("validateMapKeyExpr") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("validateKeyValueKeyExpr") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("reorderedMapKeyType") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("reorderedMapValueType") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("reorderedMap") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("reorderedExperimentalMap") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("bool isMap") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("bool isExperimentalMap =") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("std::string mapValueType") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("std::string mapKeyType") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("const std::string &mapKeyType") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("mapKeyTypeOut") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("isRootMapAliasPath") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("isRootKeyValueAliasPath") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("isRootMapAliasExpr") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("isRootKeyValueAliasExpr") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("isExperimentalMapTypeText") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("isExperimentalKeyValueTypeText") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("isExperimentalMapTypeReceiver") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find(
            "isExperimentalKeyValueTypeReceiver") != std::string::npos);
  CHECK(s.collectionAccessValidationSource.find(
            "resolvesNonRootExperimentalMapTarget") == std::string::npos);
  CHECK(s.collectionAccessValidationSource.find(
            "resolvesNonRootExperimentalKeyValueTarget") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("experimentalMapKeyType") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("experimentalKeyValueKeyType") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("isCanonicalKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find(
            "isSourceSpelledCanonicalKeyValueAccessCall(") !=
        std::string::npos);
  CHECK(s.exprLateUnknownTargetFallbacksSource.find("rewrittenKeyValueMethodCall") !=
        std::string::npos);
  CHECK(s.exprLateUnknownTargetFallbacksSource.find("rewrittenMapMethodCall") ==
        std::string::npos);
  CHECK(s.exprLateUnknownTargetFallbacksSource.find(
            "isCanonicalKeyValueMethodHelper(") != std::string::npos);
  CHECK(s.exprLateUnknownTargetFallbacksSource.find("isCanonicalMapMethodHelper(") ==
        std::string::npos);
  CHECK(s.exprLateUnknownTargetFallbacksSource.find(
            "canonicalKeyValueMethodHelperTarget(") != std::string::npos);
  CHECK(s.exprLateUnknownTargetFallbacksSource.find("canonicalMapMethodHelperTarget(") ==
        std::string::npos);
  CHECK(s.exprLateUnknownTargetFallbacksSource.find("const bool isKeyValueReceiver") !=
        std::string::npos);
  CHECK(s.exprLateUnknownTargetFallbacksSource.find("const bool isMapReceiver") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find(
            "isExplicitCanonicalKeyValueAccessCall") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find(
            "validateMethodKeyValueAccessBuiltin") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("isExplicitKeyValueAccessHelper") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("canonicalMapHelperPathLocal(") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("canonicalMapHelperNamespaceLocal(") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("resolveCanonicalMapHelperNameFromSpelling(") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("rootedMapHelperAliasPathLocal(") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("resolvedMapHelperName") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("rewrittenMapHelperCall") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("canonicalKeyValueHelperPathLocal(") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("canonicalKeyValueHelperNamespaceLocal(") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("resolveCanonicalKeyValueHelperNameFromSpelling(") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("rootedKeyValueHelperAliasPathLocal(") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("resolvedKeyValueHelperName") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("rewrittenKeyValueHelperCall") !=
        std::string::npos);
  CHECK(s.collectionDispatchSetupSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.collectionDispatchSetupSource.find("isCanonicalMapAccessHelperName(") ==
        std::string::npos);
  CHECK(s.collectionDispatchSetupSource.find(
            "isStdNamespacedCanonicalMapAccessPath(") ==
        std::string::npos);
  CHECK(s.collectionDispatchSetupSource.find("keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.collectionDispatchSetupSource.find(
            "isCanonicalKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(s.collectionDispatchSetupSource.find(
            "isStdNamespacedCanonicalKeyValueAccessPath(") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find("resolvedPath == \"/map/at_ref\" ||\n"
                                    "            resolvedPath == \"/std/collections/map/at_ref\"") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("normalizedName.rfind(\"map/\", 0) == 0) {\n"
                                    "          const std::string helperName =") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("expr.namespacePrefix == \"map\"") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("defMap_.find(\"/map/\" + accessHelperName)") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("resolved = \"/map/\" +") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("path == \"/map/at\"") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("methodResolved == \"/map/") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("std::string(\"/map/\")") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("auto isMapNamespacedAccessCompatibilityCall") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("isMapCanonicalAccessPath(") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("collectionAccessMapHelperMetadata()") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("canonicalMapHelperPathLocal(") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("canonicalMapHelperNamespaceLocal(") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("resolveCanonicalMapHelperNameFromSpelling(") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("resolvedMapHelperName") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("canonicalMapMethodHelperName") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("resolvedCanonicalMapHelper") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find(
            "getCanonicalMapAccessHelperNameForDispatch") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("isNamespacedMapAccessCall") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("explicitCanonicalMapAccessHelperName") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("explicitCanonicalMapAccessCall") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("canonicalMapAccessHelperTarget") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("resolvedCanonicalMapAccessMethod") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("resolvedDeclaredCanonicalMapHelper") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("isCanonicalMapAccessHelperName(") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("canonicalStdlibMapAccessPathForHelper(") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("isCanonicalMapContainsResolvedPath(") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("isCanonicalMapAccessResolvedPath(") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find(
            "canonicalStdlibMapContainsPathForResolvedMethod(") ==
        std::string::npos);
  CHECK(s.collectionAccessSource.find("collectionAccessKeyValueHelperMetadata()") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find("canonicalKeyValueHelperPathLocal(") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find("canonicalKeyValueHelperNamespaceLocal(") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find("resolveCanonicalKeyValueHelperNameFromSpelling(") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find("resolvedKeyValueHelperName") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find("canonicalKeyValueMethodHelperName") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find("resolvedCanonicalKeyValueHelper") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find(
            "getCanonicalKeyValueAccessHelperNameForDispatch") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find("isNamespacedKeyValueAccessCall") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find(
            "explicitCanonicalKeyValueAccessHelperName") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find("explicitCanonicalKeyValueAccessCall") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find("canonicalKeyValueAccessHelperTarget") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find("resolvedCanonicalKeyValueAccessMethod") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find("resolvedDeclaredCanonicalKeyValueHelper") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find("isCanonicalKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find(
            "canonicalStdlibKeyValueAccessPathForHelper(") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find(
            "isCanonicalKeyValueContainsResolvedPath(") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find(
            "isCanonicalKeyValueAccessResolvedPath(") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find(
            "canonicalStdlibKeyValueContainsPathForResolvedMethod(") !=
        std::string::npos);
  CHECK(s.collectionAccessSource.find(
            "keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.countCapacityMapBuiltinSource.find("/std/collections/map/count") ==
        std::string::npos);
  CHECK(s.countCapacityMapBuiltinSource.find("/std/collections/map/at") ==
        std::string::npos);
  CHECK(s.countCapacityMapBuiltinSource.find("canonicalizeExperimentalMapHelperResolvedPath") ==
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("const std::string mapAlias = \"/map/\" + suffix") ==
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("preferred.rfind(\"/map/\", 0) == 0 && nameMap.count(preferred) == 0") ==
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("preferred.rfind(\"map/\", 0) == 0 || preferred.rfind(\"std/collections/map/\", 0) == 0") ==
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("resolvedPath.rfind(\"/std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("resolvedPath.rfind(\"/map/\", 0)") ==
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("scopedName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("scopedName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find(
            "emitterCollectionSurfaceMetadata(EmitterCollectionSurface::KeyValueHelpers)") !=
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("MapHelperSurfaceBridgeKey") ==
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find(
            "emitterCollectionSurfaceMetadata(EmitterCollectionSurface::KeyValueConstructors)") !=
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("MapConstructorSurfaceBridgeKey") ==
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("mapHelperSurfaceMetadataLocal()") ==
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("keyValueConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("mapConstructorSurfaceMetadataLocal()") ==
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("keyValueConstructorAliasToken()") !=
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("mapConstructorAliasToken()") ==
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("resolveCanonicalMapHelperExprMemberName") ==
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("resolvePublishedMapHelperExprMemberName") ==
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("resolveCanonicalKeyValueHelperExprMemberName") !=
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("resolvePublishedKeyValueHelperExprMemberName") !=
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("isCanonicalMapAccessHelperName(") ==
        std::string::npos);
  CHECK(s.emitterCallPathHelpersSource.find("isCanonicalKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("const std::string mapAlias = \"/map/\" + suffix") ==
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("preferred.rfind(\"/map/\", 0) == 0 && defMap.count(preferred) == 0") ==
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("appendUnique(\"/map/\" + suffix)") ==
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("candidates.push_back(\"/map/\" + methodName)") ==
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("appendUnique(\"/std/collections/map/\" + suffix)") ==
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("eraseCandidate(\"/map/\" + suffix)") ==
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("eraseCandidate(\"/std/collections/map/\" + suffix)") ==
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("normalizedPath.rfind(\"map/\", 0) == 0 ||\n"
                                                     "          normalizedPath.rfind(\"std/collections/map/\", 0) == 0") ==
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("const std::string mapPrefix = \"map/\"") ==
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("const std::string stdMapPrefix = \"std/collections/map/\"") ==
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("receiverStruct == \"/map\"") ==
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("rawMethodName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("rawMethodName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("surfaceHelperPathForRawMethodName(\n"
                                                     "                    *keyValueHelperMetadata") !=
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("MapHelperSurfaceBridgeKey") ==
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("return {keyValueHelperPath(methodName)}") !=
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("return {mapHelperPath(methodName)}") ==
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("keyValueMemberName") !=
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("mapMemberName") ==
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("isCollectionPairHelperMethod") !=
        std::string::npos);
  CHECK(s.emitterReturnInferenceCollectionsSource.find("isMapHelperMethod") ==
        std::string::npos);
  CHECK(s.emitterCollectionTypeHelpersSource.find("eraseCandidate(\"/map/\" + suffix)") ==
        std::string::npos);
  CHECK(s.emitterCollectionTypeHelpersSource.find("eraseCandidate(\"/std/collections/map/\" + suffix)") ==
        std::string::npos);
  CHECK(s.emitterCollectionTypeHelpersSource.find("normalized.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.emitterCollectionTypeHelpersSource.find("std::string_view(\"std/collections/map/\").size()") ==
        std::string::npos);
  CHECK(s.emitterCollectionTypeHelpersSource.find("collectionTypeKeyValueHelperMetadata") !=
        std::string::npos);
  CHECK(s.emitterCollectionTypeHelpersSource.find("CollectionTypeMapHelperSurfaceBridgeKey") ==
        std::string::npos);
  CHECK(s.emitterCollectionTypeHelpersSource.find(
            "collectionTypeKeyValueHelperMemberName(resolveExprPath(candidate), false)") !=
        std::string::npos);
  CHECK(s.emitterCollectionTypeHelpersSource.find("collectionTypeMapHelperMemberName(") ==
        std::string::npos);
  CHECK(s.emitterCollectionTypeHelpersSource.find("isCollectionTypeKeyValueAccessHelper(") !=
        std::string::npos);
  CHECK(s.emitterCollectionTypeHelpersSource.find("isCollectionTypeMapAccessHelper(") ==
        std::string::npos);
  CHECK(s.emitterCollectionTypeHelpersSource.find("builtinCanonicalMapAccessReceiverTypePath") ==
        std::string::npos);
  CHECK(s.emitterCollectionTypeHelpersSource.find("builtinCanonicalKeyValueAccessReceiverTypePath") !=
        std::string::npos);
  CHECK(s.emitterCollectionTypeHelpersSource.find("builtinMapAccessMethodReceiverTypePath") ==
        std::string::npos);
  CHECK(s.emitterCollectionTypeHelpersSource.find("builtinKeyValueAccessMethodReceiverTypePath") !=
        std::string::npos);
  CHECK(s.emitterCollectionTypeHelpersSource.find("isExplicitMapAccessMethod") ==
        std::string::npos);
  CHECK(s.emitterCollectionTypeHelpersSource.find("isExplicitKeyValueAccessMethod") !=
        std::string::npos);
  CHECK(s.emitterCollectionTypeHelpersSource.find("shouldProbeBuiltinMapAccessType") ==
        std::string::npos);
  CHECK(s.emitterCollectionTypeHelpersSource.find("shouldProbeBuiltinKeyValueAccessType") !=
        std::string::npos);
  CHECK(s.emitterPackedArgsSource.find("isMapAccessName") == std::string::npos);
  CHECK(s.emitterPackedArgsSource.find("isCollectionPairAccessName") != std::string::npos);
  CHECK(s.emitterPackedArgsSource.find("isCanonicalMapAccessHelperName(") ==
        std::string::npos);
  CHECK(s.emitterPackedArgsSource.find("isCanonicalKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("} else if (normalizedPath.rfind(\"/std/collections/map/\", 0) == 0) {\n"
                                         "    const std::string suffix =") ==
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("preferCanonicalMapMethodHelperPath") ==
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("pruneMapAccessStructReturnCompatibilityCandidates") ==
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("path.rfind(\"map/\", 0) == 0 ||\n"
                                         "       path.rfind(\"std/collections/map/\", 0) == 0") ==
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("path.rfind(\"map/\", 0) == 0 || path.rfind(\"std/collections/map/\", 0) == 0") ==
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("candidate.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("candidate.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("typePath == \"/map\"") ==
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find(
            "emitterCollectionSurfaceMetadata(EmitterCollectionSurface::KeyValueHelpers)") !=
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("MapHelperSurfaceBridgeKey") ==
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("findKeyValueHelperSurfaceMetadata()") !=
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("findMapHelperSurfaceMetadata()") ==
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("isKeyValueHelperSurface(surfaceId)") !=
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("isMapHelperSurface(surfaceId)") ==
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("isRemovedExactPublishedKeyValueHelper(") !=
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("isRemovedExactPublishedMapHelper(") ==
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("canonicalKeyValueMemberName") !=
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("canonicalMapMemberName") ==
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("const std::string_view mapHelperName") ==
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("const std::string_view keyValueHelperName") !=
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("isCanonicalMapCountHelperName(") ==
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("isCanonicalKeyValueCountHelperName(") !=
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("normalizeMapImportAliasPath") ==
        std::string::npos);
  CHECK(s.emitterMethodMetadataSource.find("path.rfind(\"/map/\", 0) == 0 || path.rfind(\"/std/collections/map/\", 0) == 0") ==
        std::string::npos);
  CHECK(s.emitterMethodResolutionSource.find("normalizeMapImportAliasPath") ==
        std::string::npos);
  CHECK(s.emitterMethodResolutionSource.find("normalizedMethodName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(s.emitterMethodResolutionSource.find("normalizedMethodName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.emitterMethodResolutionSource.find("resolvedType == \"/map\"") ==
        std::string::npos);
  CHECK(s.emitterMethodResolutionSource.find("const std::string aliasPath = \"/map/\" + normalizedMethodName") ==
        std::string::npos);
  CHECK(s.emitterMethodResolutionSource.find("const std::string canonicalPath = \"/std/collections/map/\" + normalizedMethodName") ==
        std::string::npos);
  CHECK(s.emitterMethodResolutionSource.find(
            "publishedSurfaceHelperPathForRawMethodName(*keyValueHelperMetadata") !=
        std::string::npos);
  CHECK(s.emitterMethodResolutionSource.find("MapHelperSurfaceBridgeKey") ==
        std::string::npos);
  CHECK(s.emitterMethodResolutionSource.find("isCollectionPairHelperMethod") !=
        std::string::npos);
  CHECK(s.emitterMethodResolutionSource.find("isMapHelperMethod") ==
        std::string::npos);
  CHECK(s.emitterMethodResolutionSource.find("isRemovedMapSlashMethod") ==
        std::string::npos);
  CHECK(s.emitterMethodResolutionSource.find("isRemovedKeyValueSlashMethod") !=
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("pruneMapAccessStructReturnCompatibilityCandidates") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("isBareMapAccessMethod") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("isBareKeyValueAccessMethod") !=
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("resolveBareMapAccessMethodHelperPath") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("resolveBareKeyValueAccessMethodHelperPath") !=
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("isExplicitMapAccessCompatibilityCall") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("isExplicitKeyValueAccessCompatibilityCall") !=
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("inferExplicitMapAccessResolvedTypeName") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("inferExplicitKeyValueAccessResolvedTypeName") !=
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("inferCanonicalMapAccessTypeName") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("inferCanonicalKeyValueAccessTypeName") !=
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("explicitMapAccessType") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("explicitKeyValueAccessType") !=
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("canonicalMapAccessType") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("canonicalKeyValueAccessType") !=
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("bareMapAccessMethodPath") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("bareKeyValueAccessMethodPath") !=
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find(
            "isRemovedMapDirectCallResultCompatibility") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find(
            "isRemovedKeyValueDirectCallResultCompatibility") !=
        std::string::npos);
  CHECK(s.statementLowererSource.find("isPrimeKeyValueInsertBody") == std::string::npos);
  CHECK(s.statementLowererSource.find("rewriteKeyValueInsertHelperStatementToCanonical") == std::string::npos);
  CHECK(s.statementLowererSource.find("isPrimeMapInsertBody") == std::string::npos);
  CHECK(s.statementLowererSource.find("rewriteMapInsertHelperStatementToCanonical") == std::string::npos);
  CHECK(s.statementLowererSource.find("rewriteMapInsertHelperStatementToBuiltin") == std::string::npos);
  CHECK(s.statementLowererSource.find("/map/at(argsPack") == std::string::npos);
  CHECK(s.statementLowererSource.find("isMapBase") == std::string::npos);
  CHECK(s.statementLowererSource.find("mapArgs") == std::string::npos);
  CHECK(s.statementLowererSource.find("inferMapStructPathFromTypeText") ==
        std::string::npos);
  CHECK(s.statementLowererSource.find("experimentalMapType") ==
        std::string::npos);
  CHECK(s.statementLowererSource.find("slashlessExperimentalMapType") ==
        std::string::npos);
  CHECK(s.statementLowererSource.find("hasSemanticMapReceiverFact") ==
        std::string::npos);
  CHECK(s.statementLowererSource.find("directMap") == std::string::npos);
  CHECK(s.statementLowererSource.find("wrappedMap") == std::string::npos);
  CHECK(s.statementLowererSource.find("argsPackMap") == std::string::npos);
  CHECK(s.statementLowererSource.find("isExperimentalMapStructPath") ==
        std::string::npos);
  CHECK(s.statementLowererSource.find("isKeyValueBase") == std::string::npos);
  CHECK(s.statementLowererSource.find("keyValueArgs") == std::string::npos);
  CHECK(s.statementLowererSource.find("inferKeyValueStructPathFromTypeText") ==
        std::string::npos);
  CHECK(s.statementLowererSource.find("experimentalKeyValueType") ==
        std::string::npos);
  CHECK(s.statementLowererSource.find("slashlessExperimentalKeyValueType") ==
        std::string::npos);
  CHECK(s.statementLowererSource.find("hasSemanticKeyValueReceiverFact") ==
        std::string::npos);
  CHECK(s.statementLowererSource.find("directKeyValue") == std::string::npos);
  CHECK(s.statementLowererSource.find("wrappedKeyValue") == std::string::npos);
  CHECK(s.statementLowererSource.find("argsPackKeyValue") == std::string::npos);
  CHECK(s.statementLowererSource.find("isExperimentalKeyValueStructPath") ==
        std::string::npos);
  CHECK(s.uninitializedStructInferenceSource.find("scopedCallPath == \"/map/at\"") ==
        std::string::npos);
  CHECK(s.uninitializedStructInferenceSource.find("scopedCallPath == \"/std/collections/map/at\"") ==
        std::string::npos);
  CHECK(s.uninitializedStructInferenceSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.uninitializedStructInferenceSource.find(
            "StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(s.uninitializedStructInferenceSource.find("constructorName == \"mapNew\"") ==
        std::string::npos);
  CHECK(s.uninitializedStructInferenceSource.find("publishedKeyValueHelperName == \"at\"") !=
        std::string::npos);
  CHECK(s.uninitializedStructInferenceSource.find(
            "keyValueHelperSurfaceMetadataForUninitializedStructs()") !=
        std::string::npos);
  CHECK(s.uninitializedStructInferenceSource.find(
            "keyValueConstructorSurfaceMetadataForUninitializedStructs()") !=
        std::string::npos);
  CHECK(s.uninitializedStructInferenceSource.find(
            "forwardedEmptyKeyValueConstructorMemberName()") !=
        std::string::npos);
  CHECK(s.uninitializedStructInferenceSource.find("const std::string mapType") ==
        std::string::npos);
  CHECK(s.uninitializedStructInferenceSource.find("inferredMapStruct") ==
        std::string::npos);
  CHECK(s.uninitializedStructInferenceSource.find(
            "isSpecializedExperimentalMapStructPath") == std::string::npos);
  CHECK(s.uninitializedStructInferenceSource.find("const std::string keyValueType") !=
        std::string::npos);
  CHECK(s.uninitializedStructInferenceSource.find("inferredKeyValueStruct") !=
        std::string::npos);
  CHECK(s.uninitializedStructInferenceSource.find(
            "isSpecializedExperimentalKeyValueStructPath") != std::string::npos);
  CHECK(s.structSlotLayoutSource.find("isBuiltinCollectionTypeName(typeName, \"map\")") ==
        std::string::npos);
  CHECK(s.structSlotLayoutSource.find("experimentalMapType") == std::string::npos);
  CHECK(s.structSlotLayoutSource.find("rootedExperimentalMapType") ==
        std::string::npos);
  CHECK(s.structSlotLayoutSource.find("mapValueRoot") == std::string::npos);
  CHECK(s.structSlotLayoutSource.find("mapValueRootNoSlash") == std::string::npos);
  CHECK(s.structSlotLayoutSource.find("resolveExperimentalMapConstructorStructPath") ==
        std::string::npos);
  CHECK(s.structSlotLayoutSource.find("experimentalKeyValueType") ==
        std::string::npos);
  CHECK(s.structSlotLayoutSource.find("rootedExperimentalKeyValueType") ==
        std::string::npos);
  CHECK(s.structSlotLayoutSource.find("keyValueRoot") == std::string::npos);
  CHECK(s.structSlotLayoutSource.find("keyValueRootNoSlash") ==
        std::string::npos);
  CHECK(s.structSlotLayoutSource.find(
            "resolveExperimentalKeyValueConstructorStructPath") != std::string::npos);
  CHECK(s.lowererStructTypeHelpersSource.find("inferredMapStruct") ==
        std::string::npos);
  CHECK(s.lowererStructTypeHelpersSource.find("mapKindTypeName") ==
        std::string::npos);
  CHECK(s.lowererStructTypeHelpersSource.find(
            "inferExperimentalMapStructPathFromKinds") == std::string::npos);
  CHECK(s.lowererStructTypeHelpersSource.find("inferredKeyValueStruct") ==
        std::string::npos);
  CHECK(s.lowererStructTypeHelpersSource.find("keyValueKindTypeName") ==
        std::string::npos);
  CHECK(s.lowererStructTypeHelpersSource.find(
            "inferExperimentalKeyValueStructPathFromKinds") == std::string::npos);
  CHECK(s.structSlotLayoutSource.find("typeName == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(s.declaredCollectionInferenceSource.find("isBuiltinCollectionTypeName(base, \"map\")") !=
        std::string::npos);
  CHECK(s.declaredCollectionInferenceSource.find("base == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(s.declaredCollectionInferenceSource.find("normalizedName == \"std/collections/map/map\"") ==
        std::string::npos);
  CHECK(s.declaredCollectionInferenceSource.find("StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(s.declaredCollectionInferenceSource.find(
            "keyValueConstructorSurfaceMetadata()") !=
        std::string::npos);
  CHECK(s.declaredCollectionInferenceSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_constructors\")") ==
        std::string::npos);
  CHECK(s.declaredCollectionInferenceSource.find(
            "keyValueConstructorSurfaceMetadataForDeclaredInference()") !=
        std::string::npos);
  CHECK(s.declaredCollectionInferenceSource.find(
            "keyValueConstructorSurfaceMetadataLocal()") ==
        std::string::npos);
  CHECK(s.declaredCollectionInferenceSource.find("isDirectKeyValueConstructor()") !=
        std::string::npos);
  CHECK(s.declaredCollectionInferenceSource.find("isDirectMapConstructor()") ==
        std::string::npos);
  CHECK(s.bindingTypeHelpersSource.find("isBuiltinCollectionTypeName(name, \"map\")") !=
        std::string::npos);
  CHECK(s.bindingTypeHelpersSource.find("name == \"/map\"") ==
        std::string::npos);
  CHECK(s.bindingTypeHelpersSource.find("name == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(s.inferenceDispatchSetupSource.find(
            "isBuiltinCollectionTypeName(normalized, \"map\")") !=
        std::string::npos);
  CHECK(s.inferenceDispatchSetupSource.find("normalized == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(s.inferenceDispatchSetupSource.find(
            "\"/std/collections/map/\" + accessNameForCanonicalMapOverride") ==
        std::string::npos);
  CHECK(s.inferenceDispatchSetupSource.find("accessNameForCanonicalKeyValueOverride") !=
        std::string::npos);
  CHECK(s.inferenceDispatchSetupSource.find("accessNameForCanonicalMapOverride") ==
        std::string::npos);
  CHECK(s.inferenceDispatchSetupSource.find(
            "keyValueHelperSurfaceMetadataForDispatchSetup()") !=
        std::string::npos);
  CHECK(s.inferenceDispatchSetupSource.find(
            "mapHelperSurfaceMetadataForDispatchSetup()") ==
        std::string::npos);
  CHECK(s.inferenceDispatchSetupSource.find(
            "canonicalKeyValueHelperPathForDispatchSetup(") !=
        std::string::npos);
  CHECK(s.inferenceDispatchSetupSource.find(
            "canonicalMapHelperPathForDispatchSetup(") ==
        std::string::npos);
  CHECK(s.setupTypeReturnKindSource.find("canonicalMapHelperPath(") ==
        std::string::npos);
  CHECK(s.setupTypeReturnKindSource.find(
            "canonicalKeyValueHelperPathForSetupReturnKind(") !=
        std::string::npos);
  CHECK(s.inferenceDispatchSetupSource.find(
            "inferDispatchSetupKeyValueKindsFromTypeText(") !=
        std::string::npos);
  CHECK(s.inferenceDispatchSetupSource.find(
            "inferDispatchSetupMapKindsFromTypeText(") ==
        std::string::npos);
  CHECK(s.inferenceDispatchSetupSource.find(
            "inferDispatchSetupSemanticKeyValueReceiverKind(") !=
        std::string::npos);
  CHECK(s.inferenceDispatchSetupSource.find(
            "inferDispatchSetupSemanticMapReceiverKind(") ==
        std::string::npos);
  CHECK(s.inferenceDispatchSetupSource.find(
            "isDispatchSetupKeyValueFamilyText(") !=
        std::string::npos);
  CHECK(s.inferenceDispatchSetupSource.find(
            "isDispatchSetupMapFamilyText(") ==
        std::string::npos);
  CHECK(s.inferenceDispatchSetupSource.find(
            "keyValueHelperSurfaceMetadata()") !=
        std::string::npos);
  CHECK(s.inferenceDispatchSetupSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_helpers\")") ==
        std::string::npos);
  CHECK(s.inferenceBaseKindSource.find(
            "isBuiltinCollectionTypeName(normalized, \"map\")") !=
        std::string::npos);
  CHECK(s.inferenceBaseKindSource.find("normalized == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(s.inferenceBaseKindSource.find("canonicalKeyValueHelperPath(\"tryAt\", false)") !=
        std::string::npos);
  CHECK(s.inferenceBaseKindSource.find("normalized == \"map/tryAt\"") ==
        std::string::npos);
  CHECK(s.inferenceBaseKindSource.find("normalized == \"std/collections/map/tryAt\"") ==
        std::string::npos);
  CHECK(s.inferenceBaseKindSource.find("canonicalKeyValueHelperPath(\"contains\", false)") !=
        std::string::npos);
  CHECK(s.inferenceBaseKindSource.find("normalized == \"map/contains\"") ==
        std::string::npos);
  CHECK(s.inferenceBaseKindSource.find("normalized == \"std/collections/map/contains\"") ==
        std::string::npos);
  CHECK(s.setupTypeMethodTargetSource.find(
            "isBuiltinCollectionTypeName(normalized, \"map\")") !=
        std::string::npos);
  CHECK(s.setupTypeMethodTargetSource.find("normalized == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(s.setupTypeMethodTargetSource.find("\"/std/collections/map\"") ==
        std::string::npos);
  CHECK(s.setupTypeMethodTargetSource.find("collectionTypePath(\"map\")") !=
        std::string::npos);
  CHECK(s.setupTypeMethodTargetSource.find("const std::string rootedKeyValuePrefix") !=
        std::string::npos);
  CHECK(s.setupTypeMethodTargetSource.find("const std::string rootedMapPrefix") ==
        std::string::npos);
  CHECK(s.setupTypeMethodTargetSource.find("const std::string canonicalKeyValuePrefix") !=
        std::string::npos);
  CHECK(s.setupTypeMethodTargetSource.find("const std::string canonicalMapPrefix") ==
        std::string::npos);
  CHECK(s.setupTypeMethodTargetSource.find("isKeyValueReceiverTarget(") !=
        std::string::npos);
  CHECK(s.setupTypeMethodTargetSource.find("isMapReceiverTarget(") ==
        std::string::npos);
  CHECK(s.setupTypeMethodTargetSource.find("shouldPreferCanonicalKeyValuePath(") !=
        std::string::npos);
  CHECK(s.setupTypeMethodTargetSource.find("shouldPreferCanonicalMapPath(") ==
        std::string::npos);
  CHECK(s.setupTypeMethodTargetSource.find("normalizedMethodName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(s.setupTypeMethodTargetSource.find("normalizedMethodName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.setupTypeMethodTargetSource.find("normalizedOriginalMethodName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(s.setupTypeMethodTargetSource.find("normalizedOriginalMethodName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("canonicalKeyValueConstructorPath()") !=
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find(
            "keyValueConstructorSurfaceMetadata()") !=
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_constructors\")") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("path == \"/std/collections/map/map\"") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("collectionMemberRoot(\"map\", false)") !=
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("std::string(\"std/collections/map/\").size()") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("normalizedMethodName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("canonicalKeyValueHelperPath(keyValueHelperName)") !=
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("collectionMemberPath(\"map\", mapHelperName)") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("\"/std/collections/map/\" + keyValueHelperName") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("\"/std/collections/map/\" + mapHelperName") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("const std::string rootedKeyValuePrefix") !=
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("const std::string rootedMapPrefix") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("const std::string canonicalKeyValuePrefix") !=
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("const std::string canonicalMapPrefix") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("sourceKeyValueMethodHelperName(") !=
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("sourceMapMethodHelperName(") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("receiverHasKeyValueLocalInfo(") !=
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("receiverHasMapLocalInfo(") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("canonicalKeyValueHelper") !=
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("canonicalMapHelper") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("isKeyValueConstructorDirectTargetPath(") !=
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("isMapConstructorDirectTargetPath(") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("keyValueConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("mapConstructorSurfaceMetadataLocal()") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find(
            "findKeyValueConstructorBridgePathChoiceBySource(") !=
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find(
            "findMapConstructorBridgePathChoiceBySource(") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("isBareKeyValueAccessReceiverProbeExpr(") !=
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("isBareMapAccessReceiverProbeExpr(") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find(
            "blocksBareKeyValueAccessReceiverProbeKindFallback") !=
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find(
            "blocksBareMapAccessReceiverProbeKindFallback") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("isBuiltinKeyValueContainsOrTryAtCall") !=
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("isBuiltinMapContainsOrTryAtCall") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("normalized.rfind(\"/map/\", 0)") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("normalized.rfind(\"/std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("canonicalKeyValueHelperPath(\"count\")") !=
        std::string::npos);
  CHECK(s.setupTypeMethodCallSource.find("\"/std/collections/map/count\"") ==
        std::string::npos);
}

TEST_SUITE_END();
