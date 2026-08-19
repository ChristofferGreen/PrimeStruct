#include "third_party/doctest.h"

#include "test_stdlib_map_ownership_shared.h"

TEST_SUITE_BEGIN("primestruct.stdlib.map_ownership");

TEST_CASE("canonical map surface owns standalone stdlib implementation ir lowerer inline dispatch and result helpers") {
  const MapOwnershipSources s = loadMapOwnershipSources();
  CHECK(s.setupTypeReceiverTargetSource.find("isBuiltinKeyValueContainsOrTryAtCall") !=
        std::string::npos);
  CHECK(s.setupTypeReceiverTargetSource.find("isBuiltinMapContainsOrTryAtCall") ==
        std::string::npos);
  CHECK(s.setupTypeReceiverTargetSource.find("isBareKeyValueAccessReceiverProbeExpr(") !=
        std::string::npos);
  CHECK(s.setupTypeReceiverTargetSource.find("isBareMapAccessReceiverProbeExpr(") ==
        std::string::npos);
  CHECK(s.setupTypeReceiverTargetSource.find("isBareKeyValueTryAtReceiverProbeExpr(") !=
        std::string::npos);
  CHECK(s.setupTypeReceiverTargetSource.find("isBareMapTryAtReceiverProbeExpr(") ==
        std::string::npos);
  CHECK(s.setupTypeReceiverTargetSource.find(
            "blocksExplicitKeyValueReceiverProbeKindFallback") !=
        std::string::npos);
  CHECK(s.setupTypeReceiverTargetSource.find(
            "blocksExplicitMapReceiverProbeKindFallback") ==
        std::string::npos);
  CHECK(s.setupTypeReceiverTargetSource.find(
            "blocksBareKeyValueAccessReceiverProbeKindFallback") !=
        std::string::npos);
  CHECK(s.setupTypeReceiverTargetSource.find(
            "blocksBareMapAccessReceiverProbeKindFallback") ==
        std::string::npos);
  CHECK(s.setupTypeReceiverTargetSource.find("isReferenceMap") ==
        std::string::npos);
  CHECK(s.setupTypeReceiverTargetSource.find("isPointerMap") ==
        std::string::npos);
  CHECK(s.setupTypeReceiverTargetSource.find("isReferenceKeyValue") !=
        std::string::npos);
  CHECK(s.setupTypeReceiverTargetSource.find("isPointerKeyValue") !=
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find("const std::string mapPrefix = \"map/\"") ==
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find("const std::string stdMapPrefix = \"std/collections/map/\"") ==
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find("collectionMemberRoot(\"map\", false)") !=
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find("keyValueCollectionAliasRoot(false) + \"/\"") !=
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find("StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find(
            "keyValueHelperSurfaceMetadata()") !=
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_helpers\")") ==
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find(
            "keyValueConstructorSurfaceMetadata()") !=
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find(
            "findCollectionConstructorSurfaceMetadataForHelper(") !=
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find("collectionMemberPath(\"map\", \"map\")") ==
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_constructors\")") ==
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find("keyValueHelperSurfaceId()") !=
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find("mapHelperSurfaceId()") ==
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find("keyValueConstructorSurfaceId()") !=
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find("mapConstructorSurfaceId()") ==
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find("isKeyValueHelperSurfaceId(") !=
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find("isMapHelperSurfaceId(") ==
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find("resolveKeyValueSurfaceMemberToken(") !=
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find("resolveMapSurfaceMemberToken(") ==
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find("isBorrowedKeyValueHelperSurface(") !=
        std::string::npos);
  CHECK(s.setupTypeCollectionSource.find("isBorrowedMapHelperSurface(") ==
        std::string::npos);
  CHECK(s.countAccessSource.find("canonicalKeyValueHelperPath(accessName)") !=
        std::string::npos);
  CHECK(s.countAccessSource.find("hasCanonicalCollectionMemberPrefix(text, \"map\")") !=
        std::string::npos);
  CHECK(s.countAccessSource.find("\"/std/collections/map/\" + std::string(accessName)") ==
        std::string::npos);
  CHECK(s.countAccessSource.find("\"/std/collections/map/\" + accessName") ==
        std::string::npos);
  CHECK(s.countAccessSource.find("text.rfind(\"/std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.countAccessSource.find("text.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.resultMetadataSource.find("canonicalKeyValueConstructorPath()") !=
        std::string::npos);
  CHECK(s.resultMetadataSource.find("StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(s.resultMetadataSource.find(
            "keyValueConstructorSurfaceMetadata()") !=
        std::string::npos);
  CHECK(s.resultMetadataSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_constructors\")") ==
        std::string::npos);
  CHECK(s.resultMetadataSource.find(
            "keyValueConstructorSurfaceMetadataForResultMetadata()") !=
        std::string::npos);
  CHECK(s.resultMetadataSource.find("mapConstructorSurfaceMetadataLocal()") ==
        std::string::npos);
  CHECK(s.resultMetadataSource.find("matchesDirectKeyValueConstructor") !=
        std::string::npos);
  CHECK(s.resultMetadataSource.find("matchesDirectMapConstructor") ==
        std::string::npos);
  CHECK(s.resultMetadataSource.find("sourceKeyValueKeyKind") !=
        std::string::npos);
  CHECK(s.resultMetadataSource.find("sourceMapKeyKind") ==
        std::string::npos);
  CHECK(s.resultMetadataSource.find("collectionKeyValueKeyKind") !=
        std::string::npos);
  CHECK(s.resultMetadataSource.find("collectionMapKeyKind") ==
        std::string::npos);
  CHECK(s.resultMetadataSource.find("isSpecializedExperimentalMapTypeText") ==
        std::string::npos);
  CHECK(s.resultMetadataSource.find("resolveSpecializedExperimentalMapFieldKinds") ==
        std::string::npos);
  CHECK(s.resultMetadataSource.find("resolveSpecializedExperimentalMapTypeKinds") ==
        std::string::npos);
  CHECK(s.resultMetadataSource.find("isSpecializedExperimentalKeyValueTypeText") !=
        std::string::npos);
  CHECK(s.resultMetadataSource.find(
            "resolveSpecializedExperimentalKeyValueFieldKinds") != std::string::npos);
  CHECK(s.resultMetadataSource.find(
            "resolveSpecializedExperimentalKeyValueTypeKinds") != std::string::npos);
  CHECK(s.resultMetadataSource.find("\"/std/collections/map/map\"") ==
        std::string::npos);
  CHECK(s.resultMetadataSource.find("\"/std/collections/map/map__\"") ==
        std::string::npos);
  CHECK(s.resultMetadataSource.find("\"std/collections/map/map\"") ==
        std::string::npos);
  CHECK(s.resultMetadataSource.find("\"std/collections/map/map__\"") ==
        std::string::npos);
  CHECK(s.semanticsResultHelpersSource.find(
            "resolved == \"/std/collections/map/tryAt\"") ==
        std::string::npos);
  CHECK(s.semanticsResultHelpersSource.find(
            "resolved == \"/std/collections/map/tryAt_ref\"") ==
        std::string::npos);
  CHECK(s.semanticsResultHelpersSource.find("resolved == \"/map/tryAt\"") ==
        std::string::npos);
  CHECK(s.semanticsResultHelpersSource.find(
            "isMapTryAtResultHelperCall(resolved, expr)") !=
        std::string::npos);
  CHECK(s.semanticsResultHelpersSource.find(
            "metadataBackedCanonicalKeyValueHelperPath(\"tryAt\")") !=
        std::string::npos);
  CHECK(s.semanticsResultHelpersSource.find(
            "metadataBackedKeyValueHelperRootAliasMethodName(resolvedPath)") !=
        std::string::npos);
  CHECK(s.semanticsResultHelpersSource.find("resolveKeyValueReceiverTypeText") !=
        std::string::npos);
  CHECK(s.semanticsResultHelpersSource.find("resolveMapReceiverTypeText") ==
        std::string::npos);
  CHECK(s.semanticsResultHelpersSource.find(
            "isSpecializedExperimentalMapBackingPath") == std::string::npos);
  CHECK(s.semanticsResultHelpersSource.find(
            "isSpecializedExperimentalKeyValueBackingPath") !=
        std::string::npos);
  CHECK(s.semanticsResultHelpersSource.find("mapValueRoot") ==
        std::string::npos);
  CHECK(s.semanticsResultHelpersSource.find("resolveBuiltinMapResultType") ==
        std::string::npos);
  CHECK(s.semanticsResultHelpersSource.find("leftMapValueIdentity") ==
        std::string::npos);
  CHECK(s.semanticsResultHelpersSource.find("canonicalMapValueIdentity") ==
        std::string::npos);
  CHECK(s.semanticsResultHelpersSource.find("keyValueRoot") !=
        std::string::npos);
  CHECK(s.semanticsResultHelpersSource.find("resolveBuiltinKeyValueResultType") !=
        std::string::npos);
  CHECK(s.semanticsResultHelpersSource.find("leftKeyValueIdentity") !=
        std::string::npos);
  CHECK(s.semanticsResultHelpersSource.find("canonicalKeyValueIdentity") !=
        std::string::npos);
  CHECK(s.statementReturnsSource.find("unknown method: /map/at") ==
        std::string::npos);
  CHECK(s.statementReturnsSource.find("unknown method: /map/at_ref") ==
        std::string::npos);
  CHECK(s.statementReturnsSource.find("unknown method: /map/at_unsafe") ==
        std::string::npos);
  CHECK(s.statementReturnsSource.find("unknown method: /map/at_unsafe_ref") ==
        std::string::npos);
  CHECK(s.statementReturnsSource.find("return \"/map\"") == std::string::npos);
  CHECK(s.statementReturnsSource.find("typePath == \"/map\"") == std::string::npos);
  CHECK(s.statementReturnsSource.find("expectedType == \"/map\"") ==
        std::string::npos);
  CHECK(s.statementReturnsSource.find("isSpecializedExperimentalMapBackingPath") ==
        std::string::npos);
  CHECK(s.statementReturnsSource.find(
            "isSpecializedExperimentalKeyValueBackingPath") !=
        std::string::npos);
  CHECK(s.statementReturnsSource.find("keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.statementReturnsSource.find("keyValueConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.statementReturnsSource.find("mapCollectionMarkerPathLocal()") ==
        std::string::npos);
  CHECK(s.statementReturnsSource.find("isUnknownBorrowedMapAccessMethodDiagnostic") ==
        std::string::npos);
  CHECK(s.statementReturnsSource.find("std::array<std::string_view, 4> AccessHelpers") ==
        std::string::npos);
  CHECK(s.statementReturnsSource.find("mapCollectionMarker") ==
        std::string::npos);
  CHECK(s.statementReturnsSource.find("canonicalMapValueRoot") ==
        std::string::npos);
  CHECK(s.statementReturnsSource.find("keyValueCollectionMarkerPathLocal()") !=
        std::string::npos);
  CHECK(s.statementReturnsSource.find("isUnknownBorrowedKeyValueAccessMethodDiagnostic") !=
        std::string::npos);
  CHECK(s.statementReturnsSource.find("KeyValueAccessHelpers") !=
        std::string::npos);
  CHECK(s.statementReturnsSource.find("keyValueCollectionMarker") !=
        std::string::npos);
  CHECK(s.statementReturnsSource.find("canonicalKeyValueBackingRoot") !=
        std::string::npos);
  CHECK(s.inlineCallContextSource.find("collectionMemberRoot(\"map\") + \"map__\"") !=
        std::string::npos);
  CHECK(s.inlineCallContextSource.find("\"/std/collections/map/map__\"") ==
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find("constructorName == \"mapNew\"") ==
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find(
            "forwardedEmptyKeyValueConstructorMemberName()") !=
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find(
            "forwardedEmptyMapConstructorMemberName()") ==
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find(
            "resolveKeyValueConstructorPathMemberName(") !=
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find(
            "resolveMapConstructorPathMemberName(") ==
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find("isPublishedKeyValueConstructorExpr(") !=
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find("isPublishedMapConstructorExpr(") ==
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find("inferDirectKeyValueConstructorTargetInfo(") !=
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find("inferDirectMapConstructorTargetInfo(") ==
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find("isForwardedKeyValueNewConstructor(") !=
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find("isForwardedMapNewConstructor(") ==
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find("mapKindTypeName") ==
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find("inferExperimentalMapStructPathFromKinds") ==
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find("resolvedExperimentalMapStructPath") ==
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find("experimentalMapType") ==
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find("rootedExperimentalMapType") ==
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find("isDirectMapStorage") ==
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find("preserveDirectExperimentalMapStruct") ==
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find("isDirectMap") == std::string::npos);
  CHECK(s.accessTargetResolutionSource.find("keyValueKindTypeName") ==
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find(
            "inferExperimentalKeyValueStructPathFromKinds") == std::string::npos);
  CHECK(s.accessTargetResolutionSource.find(
            "resolvedExperimentalKeyValueStructPath") == std::string::npos);
  CHECK(s.accessTargetResolutionSource.find("experimentalKeyValueType") ==
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find("rootedExperimentalKeyValueType") ==
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find("isDirectKeyValueStorage") !=
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find(
            "preserveDirectExperimentalKeyValueStruct") != std::string::npos);
  CHECK(s.accessTargetResolutionSource.find("isDirectKeyValue") !=
        std::string::npos);
  CHECK(s.accessTargetResolutionSource.find(
            "resolveStdlibSurfaceMemberName(*metadata, metadata->canonicalPath)") !=
        std::string::npos);
  CHECK(s.statementLowererSource.find("callee->fullPath.rfind(\"/std/collections/map/insertImpl__\", 0)") ==
        std::string::npos);
  CHECK(s.statementLowererSource.find("canonicalStatementKeyValueHelperPath(\"insert\")") ==
        std::string::npos);
  CHECK(s.statementLowererSource.find("canonicalStatementMapHelperPath(\"insert\")") ==
        std::string::npos);
  CHECK(s.statementLowererSource.find("rewrittenStmt.name = \"/std/collections/map/insert\"") ==
        std::string::npos);
  CHECK(s.lowererCallHelpersSource.find("const std::string unrooted = \"map/\" + std::string(helperName)") ==
        std::string::npos);
  CHECK(s.lowererCallHelpersSource.find("resolveMapHelperDefinitionMember(") ==
        std::string::npos);
  CHECK(s.lowererCallHelpersSource.find("isRemovedCountFallbackMapHelper(") ==
        std::string::npos);
  CHECK(s.lowererCallHelpersSource.find("isExplicitRemovedMapHelperAliasCall") ==
        std::string::npos);
  CHECK(s.lowererCallHelpersSource.find(
            "resolveKeyValueHelperDefinitionMember(directHelperPath, helperName)") !=
        std::string::npos);
  CHECK(s.lowererCallHelpersSource.find("isRemovedCountFallbackKeyValueHelper(") !=
        std::string::npos);
  CHECK(s.lowererCallHelpersSource.find("isExplicitRemovedKeyValueHelperAliasCall") !=
        std::string::npos);
  CHECK(s.inlineNativeSource.find("return helperName == \"insert\" || helperName == \"insert_ref\"") ==
        std::string::npos);
  CHECK(s.inlineNativeSource.find("return helperName == \"at\" || helperName == \"at_ref\"") ==
        std::string::npos);
  CHECK(s.inlineNativeSource.find("return helperName == \"count\" || helperName == \"count_ref\"") ==
        std::string::npos);
  CHECK(s.inlineNativeSource.find("rawPath == \"map/at\"") == std::string::npos);
  CHECK(s.inlineNativeSource.find("rawPath == \"map/at_unsafe\"") == std::string::npos);
  CHECK(s.inlineNativeSource.find("rawPath == \"map/at_ref\"") == std::string::npos);
  CHECK(s.inlineNativeSource.find("rawPath == \"map/at_unsafe_ref\"") == std::string::npos);
  CHECK(s.inlineNativeSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.inlineNativeSource.find("inlineKeyValueHelperMetadata()") !=
        std::string::npos);
  CHECK(s.inlineNativeSource.find("resolvePublishedInlineKeyValueSurfaceMemberName") !=
        std::string::npos);
  CHECK(s.inlineNativeSource.find("isCanonicalPublishedInlineKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.inlineNativeSource.find("emitCanonicalInlineDefinitionCall(expr, *callee)") != std::string::npos);
  CHECK(s.emitterMethodResolutionSource.find("!hasAliasHelperDefinition && !hasCanonicalHelperDefinition") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("const std::string aliasPath = \"/map/\" + candidate.name") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("const std::string canonicalPath = \"/std/collections/map/\" + candidate.name") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("normalized.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("normalized.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("resolvedExprPath.rfind(\"/std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("resolvedExprPath.rfind(\"/map/\", 0)") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find(
            "isCanonicalKeyValueHelperMemberPath(normalized, helperName)") !=
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("isCanonicalMapHelperMemberPath(") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find(
            "isCollectionPairImportAliasHelperMemberPath(resolvedExprPath, helperName)") !=
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("isMapImportAliasHelperMemberPath(") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("EmitterCollectionSurface::KeyValueHelpers") !=
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("MapHelperSurfaceBridgeKey") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("keyValueHelperPath(") !=
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("mapHelperPath(") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("keyValueHelperMemberNameFromPath(") !=
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("mapHelperMemberNameFromPath(") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("isCanonicalMapAccessHelperName(") ==
        std::string::npos);
  CHECK(s.emitterMethodTypeInferenceSource.find("isCanonicalKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(s.emitterHelpersSource.find("isCanonicalMapHelperName(") ==
        std::string::npos);
  CHECK(s.emitterHelpersSource.find("mapHelperNameFromPath(") ==
        std::string::npos);
  CHECK(s.emitterHelpersSource.find("isCanonicalMapHelperPath(") ==
        std::string::npos);
  CHECK(s.emitterHelpersSource.find("isCanonicalKeyValueHelperName(") !=
        std::string::npos);
  CHECK(s.emitterHelpersSource.find("isCanonicalMapCountHelperName(") ==
        std::string::npos);
  CHECK(s.emitterHelpersSource.find("isCanonicalKeyValueCountHelperName(") !=
        std::string::npos);
  CHECK(s.emitterHelpersSource.find("keyValueHelperNameFromPath(") !=
        std::string::npos);
  CHECK(s.emitterHelpersSource.find("isCanonicalKeyValueHelperPath(") !=
        std::string::npos);
  CHECK(s.emitterHelpersSource.find("isCanonicalMapAccessHelperName(") ==
        std::string::npos);
  CHECK(s.emitterHelpersSource.find("isCanonicalMapAccessHelperPath(") ==
        std::string::npos);
  CHECK(s.emitterHelpersSource.find("isCanonicalKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(s.emitterHelpersSource.find("isCanonicalKeyValueAccessHelperPath(") !=
        std::string::npos);
  CHECK(s.emitterHelpersSource.find("isRemovedMapSlashMethodMetadataHelperName(") ==
        std::string::npos);
  CHECK(s.emitterHelpersSource.find(
            "isRemovedMapDirectCallResultCompatibilityHelperName(") ==
        std::string::npos);
  CHECK(s.emitterHelpersSource.find(
            "isRemovedKeyValueSlashMethodMetadataHelperName(") !=
        std::string::npos);
  CHECK(s.emitterHelpersSource.find(
            "isRemovedKeyValueDirectCallResultCompatibilityHelperName(") !=
        std::string::npos);
  CHECK(s.emitterHelpersTypesSource.find("base += \"map/Map\"") == std::string::npos);
  CHECK(s.emitterHelpersTypesSource.find("experimentalCollectionTypePathLocal(\"map\", \"Map\"") !=
        std::string::npos);
  CHECK(s.emitterHelpersTypesSource.find("isMapCompatibilityStorageBase(") ==
        std::string::npos);
  CHECK(s.emitterHelpersTypesSource.find("isKeyValueCompatibilityStorageBase(") !=
        std::string::npos);
  CHECK(s.emitterHelpersTypesSource.find("name == \"/map\"") == std::string::npos);
  CHECK(s.emitterHelpersTypesSource.find("name == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(s.emitterHelpersTypesSource.find("name.rfind(\"/std/collections/map<\"") ==
        std::string::npos);
  CHECK(s.nativeTailSource.find("hasSemanticKeyValueReadHelperDefinition") ==
        std::string::npos);
  CHECK(s.nativeTailSource.find(
            "isKeyValueReadHelperName(directKeyValueReadHelperName)") !=
        std::string::npos);
  CHECK(s.nativeTailSource.find("importsMapReadHelper") == std::string::npos);
  CHECK(s.nativeTailSource.find("importsKeyValueReadHelper") ==
        std::string::npos);
  CHECK(s.nativeTailSource.find("importsMapHelpers") == std::string::npos);
  CHECK(s.nativeTailSource.find("importsKeyValueHelpers") != std::string::npos);
  CHECK(s.lowerStatementsExprSource.find("Keep direct canonical map access helpers") == std::string::npos);
  CHECK(s.lowerStatementsExprSource.find("keepsBuiltinCanonicalMapHelperReturn") == std::string::npos);
  CHECK(s.lowerStatementsExprSource.find("keyValueHelperMetadata") != std::string::npos);
  CHECK(s.lowerStatementsExprSource.find("mapHelperMetadata") == std::string::npos);
  CHECK(s.lowerStatementsExprSource.find("keyValueConstructorMetadata") != std::string::npos);
  CHECK(s.lowerStatementsExprSource.find("mapConstructorMetadata") == std::string::npos);
  CHECK(s.lowerStatementsExprSource.find("isCanonicalKeyValueHelperFamilyPath(") !=
        std::string::npos);
  CHECK(s.lowerStatementsExprSource.find("isCanonicalMapHelperFamilyPath(") ==
        std::string::npos);
  CHECK(s.lowerStatementsExprSource.find("isCanonicalKeyValueConstructorPath(") !=
        std::string::npos);
  CHECK(s.lowerStatementsExprSource.find("isCanonicalMapConstructorPath(") ==
        std::string::npos);
  CHECK(s.lowerStatementsExprSource.find("findDirectEntryKeyValueConstructorDefinition(") !=
        std::string::npos);
  CHECK(s.lowerStatementsExprSource.find("findDirectEntryMapConstructorDefinition(") ==
        std::string::npos);
  CHECK(s.lowerStatementsExprSource.find("isExplicitCanonicalKeyValueAccess") !=
        std::string::npos);
  CHECK(s.lowerStatementsExprSource.find("isExplicitCanonicalMapAccess") ==
        std::string::npos);
  CHECK(s.lowerStatementsExprSource.find("isExperimentalMapTarget") ==
        std::string::npos);
  CHECK(s.lowerStatementsExprSource.find("isExperimentalKeyValueTarget") !=
        std::string::npos);
  CHECK(s.lowerStatementsExprSource.find("resolvedKeyValueInsertHelperName") !=
        std::string::npos);
  CHECK(s.lowerStatementsExprSource.find("resolvedMapInsertHelperName") ==
        std::string::npos);
  CHECK(s.lowerStatementsBindingsSource.find("resolvePublishedStdlibSurfaceMemberName(\n"
                                           "                  rawPath,\n"
                                           "                  metadata->id") !=
        std::string::npos);
  CHECK(s.lowerStatementsBindingsSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.lowerStatementsBindingsSource.find(
            "keyValueHelperSurfaceMetadata()") !=
        std::string::npos);
  CHECK(s.lowerStatementsBindingsSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_helpers\")") ==
        std::string::npos);
  CHECK(s.lowerStatementsBindingsSource.find("canonicalizeExplicitBuiltinKeyValueHelpers") !=
        std::string::npos);
  CHECK(s.lowerStatementsBindingsSource.find("canonicalizeExplicitBuiltinMapHelpers") ==
        std::string::npos);
  CHECK(s.lowerStatementsBindingsSource.find("resolveDirectKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.lowerStatementsBindingsSource.find("resolveDirectMapHelperPath") ==
        std::string::npos);
  CHECK(s.lowerStatementsBindingsSource.find("findDirectKeyValueHelperDefinition") !=
        std::string::npos);
  CHECK(s.lowerStatementsBindingsSource.find("findDirectMapHelperDefinition") ==
        std::string::npos);
  CHECK(s.lowerStatementsBindingsSource.find("rawPath.rfind(\"/map/\", 0)") ==
        std::string::npos);
  CHECK(s.lowerStatementsBindingsSource.find("rawPath.rfind(\"/std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.tailDispatchSource.find("rewrittenExpr.name = ir_lowerer::canonicalKeyValueHelperPath(\"insert\")") !=
        std::string::npos);
  CHECK(s.tailDispatchSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.tailDispatchSource.find("tailDispatchKeyValueHelperSurfaceId()") !=
        std::string::npos);
  CHECK(s.tailDispatchSource.find("keyValueHelperSurfaceMetadata()") !=
        std::string::npos);
  CHECK(s.tailDispatchSource.find("collections.map_helpers") ==
        std::string::npos);
  CHECK(s.tailDispatchSource.find("isTailDispatchKeyValueImportAliasHelperPath(rawPath, helperName)") !=
        std::string::npos);
  CHECK(s.tailDispatchSource.find("rawPath.rfind(\"/\" + std::string(\"map\") + \"/\", 0)") ==
        std::string::npos);
  CHECK(s.tailDispatchSource.find("isSpecializedExperimentalMapStructPath") ==
        std::string::npos);
  CHECK(s.tailDispatchSource.find("isSpecializedExperimentalKeyValueStructPath") !=
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find("StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find(
            "keyValueHelperSurfaceMetadataForLowerEmitExpr()") !=
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find(
            "mapHelperSurfaceMetadataForLowerEmitExpr()") ==
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find(
            "keyValueConstructorSurfaceMetadataForLowerEmitExpr()") !=
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find(
            "mapConstructorSurfaceMetadataForLowerEmitExpr()") ==
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find("resolvePublishedLateKeyValueMemberName(") !=
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find("resolvePublishedLateMapMemberName(") ==
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find(
            "resolvePublishedLateKeyValueConstructorName(") !=
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find("resolvePublishedLateMapConstructorName(") ==
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find("isEntryArgsPackKeyValueConstructorExpr(") !=
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find("isEntryArgsPackMapConstructorExpr(") ==
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find("rewriteExplicitBuiltinKeyValueHelperExpr(") !=
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find("rewriteExplicitBuiltinMapHelperExpr(") ==
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find("isKeyValueAccessValueCall(") !=
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find("isMapAccessValueCall(") ==
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find("rewriteBareKeyValueAccessMethodExpr(") !=
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find("rewriteBareMapAccessMethodExpr(") ==
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find("rawPath.rfind(\"/map/\", 0)") ==
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find("canonicalMapHelperRoot") ==
        std::string::npos);
  CHECK(s.lowerEmitExprCollectionSource.find("directHelperPath.rfind(\"/std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.builtinNameHelpersSource.find("scopedName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(s.builtinNameHelpersSource.find("scopedName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.builtinNameHelpersSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.builtinNameHelpersSource.find("alias == \"map\"") ==
        std::string::npos);
  CHECK(s.builtinNameHelpersSource.find("rawName == \"map\"") ==
        std::string::npos);
  CHECK(s.builtinNameHelpersSource.find(
            "keyValueConstructorSurfaceMetadata()") !=
        std::string::npos);
  CHECK(s.builtinNameHelpersSource.find("keyValueConstructorAliasToken()") !=
        std::string::npos);
  CHECK(s.builtinNameHelpersSource.find(
            "keyValueHelperSurfaceMetadata()") !=
        std::string::npos);
  CHECK(s.builtinNameHelpersSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_constructors\")") ==
        std::string::npos);
  CHECK(s.builtinNameHelpersSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_helpers\")") ==
        std::string::npos);
  CHECK(s.builtinNameHelpersSource.find("resolvesKeyValueHelperSurfacePath(scopedName)") !=
        std::string::npos);
  CHECK(s.lowererHelpersSource.find("candidate.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(s.lowererHelpersSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.lowererHelpersSource.find(
            "keyValueHelperSurfaceMetadata()") !=
        std::string::npos);
  CHECK(s.lowererHelpersSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_helpers\")") ==
        std::string::npos);
  CHECK(s.lowererHelpersSource.find("resolvesKeyValueHelperSurfacePath(candidate)") !=
        std::string::npos);
  CHECK(s.lowererHelpersSource.find("resolvesMapHelperSurfacePath(candidate)") ==
        std::string::npos);
  CHECK(s.inlinePackedArgsSource.find("rewrittenExpr.name = \"/map/map\"") == std::string::npos);
  CHECK(s.inlinePackedArgsSource.find("rewriteBuiltinMapConstructorExpr") == std::string::npos);
  CHECK(s.inlinePackedArgsSource.find("StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(s.inlinePackedArgsSource.find(
            "keyValueConstructorSurfaceMetadata()") !=
        std::string::npos);
  CHECK(s.inlinePackedArgsSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_constructors\")") ==
        std::string::npos);
  CHECK(s.inlinePackedArgsSource.find(
            "keyValueConstructorSurfaceMetadataForInlinePackedArgs()") !=
        std::string::npos);
  CHECK(s.inlinePackedArgsSource.find(
            "mapConstructorSurfaceMetadataForInlinePackedArgs()") ==
        std::string::npos);
  CHECK(s.inlinePackedArgsSource.find("isPublishedKeyValueConstructorExpr(") !=
        std::string::npos);
  CHECK(s.inlinePackedArgsSource.find("isPublishedMapConstructorExpr(") ==
        std::string::npos);
  CHECK(s.inlinePackedArgsSource.find("rewritePublishedKeyValueConstructorExpr(") !=
        std::string::npos);
  CHECK(s.inlinePackedArgsSource.find("rewritePublishedMapConstructorExpr(") ==
        std::string::npos);
  CHECK(s.inlinePackedArgsSource.find("keyValueConstructorMetadata") !=
        std::string::npos);
  CHECK(s.inlinePackedArgsSource.find("mapConstructorMetadata") ==
        std::string::npos);
  CHECK(s.inlinePackedArgsSource.find("rewrittenExpr.name = canonicalKeyValueConstructorPath()") !=
        std::string::npos);
  CHECK(s.inlinePackedArgsSource.find("namesExplicitExperimentalMapType") ==
        std::string::npos);
  CHECK(s.inlinePackedArgsSource.find("experimentalMapType") ==
        std::string::npos);
  CHECK(s.inlinePackedArgsSource.find("expectsMap") == std::string::npos);
  CHECK(s.inlinePackedArgsSource.find("namesExplicitExperimentalKeyValueType") !=
        std::string::npos);
  CHECK(s.inlinePackedArgsSource.find("experimentalKeyValueType") !=
        std::string::npos);
  CHECK(s.inlinePackedArgsSource.find("expectsKeyValue") != std::string::npos);
  CHECK(s.inlineParamHelpersSource.find("rewrittenExpr.name = \"/map/map\"") == std::string::npos);
  CHECK(s.inlineParamHelpersSource.find("rewriteBuiltinMapConstructorExpr") == std::string::npos);
  CHECK(s.inlineParamHelpersSource.find("StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(s.inlineParamHelpersSource.find(
            "keyValueConstructorSurfaceMetadata()") !=
        std::string::npos);
  CHECK(s.inlineParamHelpersSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_constructors\")") ==
        std::string::npos);
  CHECK(s.inlineParamHelpersSource.find("isPublishedKeyValueConstructorExpr(") !=
        std::string::npos);
  CHECK(s.inlineParamHelpersSource.find("isPublishedMapConstructorExpr(") ==
        std::string::npos);
  CHECK(s.inlineParamHelpersSource.find("rewritePublishedKeyValueConstructorExpr(") !=
        std::string::npos);
  CHECK(s.inlineParamHelpersSource.find("rewritePublishedMapConstructorExpr(") ==
        std::string::npos);
  CHECK(s.inlineParamHelpersSource.find("keyValueConstructorMetadata") !=
        std::string::npos);
  CHECK(s.inlineParamHelpersSource.find("mapConstructorMetadata") ==
        std::string::npos);
  CHECK(s.inlineParamHelpersSource.find("rewrittenExpr.name = canonicalKeyValueConstructorPath()") !=
        std::string::npos);
  CHECK(s.inlineParamHelpersSource.find("experimentalMapType") ==
        std::string::npos);
  CHECK(s.inlineParamHelpersSource.find("experimentalKeyValueType") !=
        std::string::npos);
  CHECK(s.packedResultSource.find("rewrittenExpr.name = \"/map/map\"") == std::string::npos);
  CHECK(s.packedResultSource.find("StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(s.packedResultSource.find(
            "keyValueConstructorSurfaceMetadata()") !=
        std::string::npos);
  CHECK(s.packedResultSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_constructors\")") ==
        std::string::npos);
  CHECK(s.packedResultSource.find("keyValueConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.packedResultSource.find("mapConstructorSurfaceMetadataLocal()") ==
        std::string::npos);
  CHECK(s.packedResultSource.find("rewritePackedResultKeyValueConstructorExpr(") !=
        std::string::npos);
  CHECK(s.packedResultSource.find("rewritePackedResultMapConstructorExpr(") ==
        std::string::npos);
  CHECK(s.packedResultSource.find("rewrittenDirectKeyValueExpr") !=
        std::string::npos);
  CHECK(s.packedResultSource.find("rewrittenDirectMapExpr") ==
        std::string::npos);
  CHECK(s.packedResultSource.find("rewrittenKeyValueExpr") !=
        std::string::npos);
  CHECK(s.packedResultSource.find("rewrittenMapExpr") ==
        std::string::npos);
  CHECK(s.packedResultSource.find("validLiteralMap") ==
        std::string::npos);
  CHECK(s.packedResultSource.find("validLiteralKeyValue") !=
        std::string::npos);
  CHECK(s.packedResultSource.find("rewrittenExpr.name = canonicalKeyValueConstructorPath()") !=
        std::string::npos);
}

TEST_SUITE_END();
