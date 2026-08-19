#include "third_party/doctest.h"

#include "test_stdlib_map_ownership_shared.h"

TEST_SUITE_BEGIN("primestruct.stdlib.map_ownership");

TEST_CASE("canonical map surface owns standalone stdlib implementation map surface registry and template monomorph") {
  const MapOwnershipSources s = loadMapOwnershipSources();
  CHECK(s.mapSource.find("import /std/collections/map/*") == std::string::npos);
  CHECK(s.mapSource.find("import /std/collections/vector/*") != std::string::npos);
  CHECK(s.mapSource.find("import /std/collections/map2") == std::string::npos);
  CHECK(s.mapSource.find("/std/collections/map2/") == std::string::npos);
  CHECK(s.mapSource.find("map2") == std::string::npos);
  CHECK(s.mapSource.find("/std/collections/experimental_map/") == std::string::npos);
  CHECK(s.mapSource.find("/std/collections/map/insertImpl") == std::string::npos);
  CHECK(s.mapSource.find("insert_builtin") == std::string::npos);
  CHECK(s.mapSource.find("Reference<Map<K, V>>") == std::string::npos);
  CHECK(s.mapSource.find("Reference<map<K, V>>") == std::string::npos);
  CHECK(s.mapSource.find("MapValue<K, V>") != std::string::npos);
  CHECK(s.mapSource.find("mapInsert<K, V>([MapValue<K, V> mut] entries") !=
        std::string::npos);
  CHECK(s.mapSource.find("[args<Entry<K, V>>] entries") != std::string::npos);
  CHECK(s.mapSource.find("entries[index]") == std::string::npos);
  CHECK(s.mapSource.find("[K] eighthKey, [V] eighthValue") != std::string::npos);
  CHECK(s.registrySource.find("\"collections.map_helpers\"") != std::string::npos);
  CHECK(s.registrySource.find("\"at_unsafe_ref\"") == std::string::npos);
  CHECK(s.registrySource.find("CollectionsMapHelperMembers") == std::string::npos);
  CHECK(s.registrySource.find("CollectionsMapConstructorMembers") == std::string::npos);
  CHECK(s.registrySource.find("resolveCollectionsMapHelperMemberName") == std::string::npos);
  CHECK(s.registrySource.find("\"/std/collections/map/insert\"") == std::string::npos);
  CHECK(s.publicationBuildersSource.find("typeName == \"/map\"") ==
        std::string::npos);
  CHECK(s.publicationBuildersSource.find("typeName == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(s.publicationBuildersSource.find(
            "StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.publicationBuildersSource.find(
            "StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(s.publicationBuildersSource.find(
            "isUnspecializedExperimentalMapBackingTypeForPublication") ==
        std::string::npos);
  CHECK(s.publicationBuildersSource.find(
            "isUnspecializedExperimentalKeyValueBackingTypeForPublication") !=
        std::string::npos);
  CHECK(s.publicationBuildersSource.find("keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.publicationBuildersSource.find("keyValueConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("path == \"mapSingle\"") == std::string::npos);
  CHECK(s.semanticsSource.find("path == \"/std/collections/mapSingle\"") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("path == \"/std/collections/mapPair\"") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("constructorBackedBuiltinMapBindings") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("isConstructorBackedMapInitializer") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("constructorBackedBuiltinKeyValueBindings") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("isConstructorBackedKeyValueInitializer") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("kBuiltinCanonicalMapInsertBuiltinPath") == std::string::npos);
  CHECK(s.semanticsSource.find("\"/std/collections/map/insert_builtin\"") == std::string::npos);
  CHECK(s.semanticsSource.find("resolveBuiltinMapInsertSurfaceMemberName") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("canonicalBuiltinMapInsertSurfacePath") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("resolveBuiltinMapReadSurfaceMemberName") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("isBuiltinMapReadHelperName") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("isCanonicalBuiltinMapReadHelperName") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("isBuiltinMapInsertValueHelperName") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("isBuiltinMapInsertReferenceHelperName") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("isBuiltinMapInsertHelperName") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("resolveBuiltinKeyValueInsertSurfaceMemberName") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("canonicalBuiltinKeyValueInsertSurfacePath") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("resolveBuiltinKeyValueReadSurfaceMemberName") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("isBuiltinKeyValueReadHelperName") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("isCanonicalBuiltinKeyValueReadHelperName") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("isCanonicalMapReadHelper") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("isCanonicalKeyValueReadHelper") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("isBuiltinKeyValueInsertValueHelperName") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("isBuiltinKeyValueInsertReferenceHelperName") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("isBuiltinKeyValueInsertHelperName") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("resolveBuiltinMapInsertReceiverBinding") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("resolveBuiltinKeyValueInsertReceiverBinding") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("rewriteBuiltinMapInsertExpr") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("rewriteBuiltinMapInsertStatements") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("rewriteBuiltinMapInsertMethods") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("rewriteBuiltinKeyValueInsertExpr") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("rewriteBuiltinKeyValueInsertStatements") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("rewriteBuiltinKeyValueInsertMethods") !=
        std::string::npos);
  CHECK(s.semanticsSource.find(
            "StdlibSurfaceId::CollectionsMapHelpers") == std::string::npos);
  CHECK(s.semanticsSource.find(
            "canonicalPrefix = \"std/collections/map/\"") == std::string::npos);
  CHECK(s.semanticsSource.find("aliasPrefix = \"map/\"") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("path == \"/std/collections/map/map\"") ==
        std::string::npos);
  CHECK(s.semanticsSource.find(
            "expr.namespacePrefix == \"/std/collections/map\"") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("normalized.rfind(\"/map/\", 0)") ==
        std::string::npos);
  CHECK(s.semanticsSource.find(
            "\"/std/collections/map/\" + helperName") ==
        std::string::npos);
  CHECK(s.semanticsSource.find(
            "metadataBackedKeyValueHelperMethodName(normalizedName)") !=
        std::string::npos);
  CHECK(s.semanticsSource.find(
            "metadataBackedCanonicalKeyValueHelperPath(helperName)") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("explicitRemovedMapCompatibilityReadPath") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("explicitRemovedKeyValueCompatibilityReadPath") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("isExperimentalMapTypeText") == std::string::npos);
  CHECK(s.semanticsSource.find("isExperimentalKeyValueTypeText") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("extractBorrowedExperimentalMapBinding") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("isBorrowedExperimentalMapBinding") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("extractBorrowedExperimentalMapReturnBinding") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("borrowedExperimentalMapHelperName") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("rewriteBorrowedExperimentalMapMethodExpr") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("rewriteBorrowedExperimentalMapMethodStatements") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("rewriteBorrowedExperimentalMapMethods") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("extractBorrowedExperimentalKeyValueBinding") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("isBorrowedExperimentalKeyValueBinding") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("extractBorrowedExperimentalKeyValueReturnBinding") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("borrowedExperimentalKeyValueHelperName") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("rewriteBorrowedExperimentalKeyValueMethodExpr") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("rewriteBorrowedExperimentalKeyValueMethodStatements") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("rewriteBorrowedExperimentalKeyValueMethods") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("isExperimentalMapValueBinding") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("extractExperimentalMapBinding") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("extractExperimentalMapReturnBinding") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("experimentalMapValueHelperName") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("rewriteExperimentalMapValueMethodExpr") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("rewriteExperimentalMapValueMethodStatements") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("rewriteExperimentalMapValueMethods") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("isExperimentalKeyValueValueBinding") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("extractExperimentalKeyValueValueBinding") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("extractExperimentalKeyValueValueReturnBinding") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("experimentalKeyValueValueHelperName") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("rewriteExperimentalKeyValueValueMethodExpr") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("rewriteExperimentalKeyValueValueMethodStatements") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("rewriteExperimentalKeyValueValueMethods") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("isBuiltinMapMutationBinding") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("isBuiltinMapReferenceBinding") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("isBuiltinKeyValueMutationBinding") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("isBuiltinKeyValueReferenceBinding") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("isBuiltinCanonicalMapConstructorExpr") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("isBuiltinCanonicalKeyValueConstructorExpr") !=
        std::string::npos);
  CHECK(s.semanticsSource.find("isResolvedMapConstructorPath(path)") ==
        std::string::npos);
  CHECK(s.semanticsSource.find("isResolvedKeyValueConstructorPath(path)") !=
        std::string::npos);
  CHECK(s.semanticBindingTypeHelpersSource.find(
            "collectionTypePathLocal(\"map\")") == std::string::npos);
  CHECK(s.semanticBindingTypeHelpersSource.find(
            "std/collections/map") == std::string::npos);
  CHECK(s.semanticBindingTypeHelpersSource.find(
            "matchesMapCollectionRootMetadataLocal(") ==
        std::string::npos);
  CHECK(s.semanticBindingTypeHelpersSource.find(
            "matchesMapValueRootMetadataLocal(") ==
        std::string::npos);
  CHECK(s.semanticBindingTypeHelpersSource.find(
            "matchesKeyValueCollectionRootMetadataLocal(normalized)") !=
        std::string::npos);
  CHECK(s.semanticBindingTypeHelpersSource.find(
            "matchesKeyValueBackingRootMetadataLocal(normalized)") !=
        std::string::npos);
  CHECK(s.semanticBindingTypeHelpersSource.find(
            "isQualifiedExperimentalKeyValueBackingTypeName(base)") !=
        std::string::npos);
  CHECK(s.semanticBindingTypeHelpersSource.find("const bool isMapLike") ==
        std::string::npos);
  CHECK(s.semanticBindingTypeHelpersSource.find("const bool isKeyValueLike") !=
        std::string::npos);
  CHECK(s.validatorSource.find("name.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(s.validatorSource.find("name.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.validatorSource.find("path.rfind(\"/std/collections/map/count__t\"") ==
        std::string::npos);
  CHECK(s.validatorSource.find("path.rfind(\"/map/count__t\"") ==
        std::string::npos);
  CHECK(s.validatorSource.find(
            "metadataBackedKeyValueHelperMethodName(normalizedName)") !=
        std::string::npos);
  CHECK(s.validatorSource.find("isSlashlessMapHelperName") ==
        std::string::npos);
  CHECK(s.validatorSource.find("isSlashlessKeyValueHelperName") !=
        std::string::npos);
  CHECK(s.validatorSource.find("const std::string mapHelperName") ==
        std::string::npos);
  CHECK(s.validatorSource.find("const std::string keyValueHelperName") !=
        std::string::npos);
  CHECK(s.validatorSource.find("mapTemplateSuffix") == std::string::npos);
  CHECK(s.validatorSource.find("keyValueTemplateSuffix") != std::string::npos);
  CHECK(s.validatorSource.find("canonicalMapCount") == std::string::npos);
  CHECK(s.validatorSource.find("canonicalKeyValueCount") != std::string::npos);
  CHECK(s.validatorSource.find("metadataBackedCanonicalKeyValueHelperPath(\"count\")") !=
        std::string::npos);
  CHECK(s.callResolutionSource.find("\"/map/entry\"") == std::string::npos);
  CHECK(s.callResolutionSource.find("\"/map/entry__\"") == std::string::npos);
  CHECK(s.callResolutionSource.find("directExplicitCallPath == \"/map/at\"") ==
        std::string::npos);
  CHECK(s.callResolutionSource.find("path == \"/std/collections/map/entry\"") ==
        std::string::npos);
  CHECK(s.callResolutionSource.find("candidatePath == \"/std/collections/map/map\"") ==
        std::string::npos);
  CHECK(s.callResolutionSource.find("isMapEntryConstructorPath") ==
        std::string::npos);
  CHECK(s.callResolutionSource.find("isKeyValueEntryConstructorPath") !=
        std::string::npos);
  CHECK(s.callResolutionSource.find("isMapEntryConstructorExpr") ==
        std::string::npos);
  CHECK(s.callResolutionSource.find("isKeyValueEntryConstructorExpr") !=
        std::string::npos);
  CHECK(s.callResolutionSource.find("mapConstructorBasePath") ==
        std::string::npos);
  CHECK(s.callResolutionSource.find("keyValueConstructorBasePath") !=
        std::string::npos);
  CHECK(s.callResolutionSource.find("preferDirectMapConstructorCandidate") ==
        std::string::npos);
  CHECK(s.callResolutionSource.find("preferDirectKeyValueConstructorCandidate") !=
        std::string::npos);
  CHECK(s.callResolutionSource.find(
            "isExperimentalCollectionConstructorPathLocal(path, \"map\", \"entry\")") ==
        std::string::npos);
  CHECK(s.callResolutionSource.find(
            "resolveStdlibSurfaceMemberName(*metadata, path) == \"entry\"") !=
        std::string::npos);
  CHECK(s.callResolutionSource.find("keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.callResolutionSource.find("keyValueConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.collectionSurfaceHelpersSource.find(
            "StdlibSurfaceId::CollectionsMapConstructors") ==
        std::string::npos);
  CHECK(s.collectionSurfaceHelpersSource.find("resolveMapConstructorMemberPath") ==
        std::string::npos);
  CHECK(s.collectionSurfaceHelpersSource.find(
            "resolveKeyValueConstructorMemberPath(normalizedPath, memberName)") !=
        std::string::npos);
  CHECK(s.collectionSurfaceHelpersSource.find(
            "isResolvedCanonicalMapConstructorPath") ==
        std::string::npos);
  CHECK(s.collectionSurfaceHelpersSource.find(
            "isResolvedCanonicalKeyValueConstructorPath") !=
        std::string::npos);
  CHECK(s.collectionSurfaceHelpersSource.find(
            "isResolvedPublishedMapConstructorPath") ==
        std::string::npos);
  CHECK(s.collectionSurfaceHelpersSource.find(
            "isResolvedPublishedKeyValueConstructorPath") !=
        std::string::npos);
  CHECK(s.callPathHelpersSource.find("name.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(s.callPathHelpersSource.find("name.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.callPathHelpersSource.find("mapHelperName") == std::string::npos);
  CHECK(s.callPathHelpersSource.find("resolveMapHelperMemberName(name, keyValueHelperName)") ==
        std::string::npos);
  CHECK(s.callPathHelpersSource.find("resolveKeyValueHelperMemberName(name, keyValueHelperName)") !=
        std::string::npos);
  CHECK(s.builtinPathHelpersSource.find("rawMethodName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(s.builtinPathHelpersSource.find(
            "rawMethodName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.builtinPathHelpersSource.find("name.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(s.builtinPathHelpersSource.find("name.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.builtinPathHelpersSource.find("extractHelper(\"map/\", \"map\")") ==
        std::string::npos);
  CHECK(s.builtinPathHelpersSource.find("resolvedMapHelperName") ==
        std::string::npos);
  CHECK(s.builtinPathHelpersSource.find("mapHelperName") ==
        std::string::npos);
  CHECK(s.builtinPathHelpersSource.find("isRemovedMapCompatibilityHelper(") ==
        std::string::npos);
  CHECK(s.builtinPathHelpersSource.find("keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.builtinPathHelpersSource.find("isRemovedKeyValueCompatibilityHelper(") !=
        std::string::npos);
  CHECK(s.builtinPathHelpersSource.find("resolveMapHelperMemberNameLocal(") ==
        std::string::npos);
  CHECK(s.builtinPathHelpersSource.find("resolveKeyValueHelperMemberNameLocal(") !=
        std::string::npos);
  CHECK(s.builtinPathHelpersSource.find("resolveRootMapAliasHelperMemberNameLocal(") !=
        std::string::npos);
  CHECK(s.builtinPathHelpersSource.find("resolvedKeyValueHelperName") !=
        std::string::npos);
  CHECK(s.builtinPathHelpersSource.find("keyValueHelperName") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("const std::string alias = \"/map/\" + resolvedHelperName") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("resolvedPath == \"/map/") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("candidate.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("explicitMapHelperPath.rfind(\"/map/\", 0)") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("explicitMapMethodPath") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("explicitMapHelperPath") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("borrowedMapHelperNameForReceiver") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("preferredMapMethodTarget") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("preferredMapHelper") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("getDirectMapHelperCompatibilityPath") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("removedMapCompatibilityPath") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("isRemovedMapCompatibilityHelper(") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("resolveExplicitRootMapMethodPath") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("resolvedExplicitRootMapMethod") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("explicitKeyValueMethodPath") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("explicitKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("borrowedKeyValueHelperNameForReceiver") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("preferredKeyValueMethodTarget") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("preferredKeyValueHelper") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("getDirectKeyValueHelperCompatibilityPath") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("removedKeyValueCompatibilityPath") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("isRemovedKeyValueCompatibilityHelper(") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("resolveExplicitRootKeyValueMethodPath") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("resolvedExplicitRootKeyValueMethod") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("normalized == \"map/count\"") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("canonicalMapHelperPathLocal(") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("canonicalMapHelperNamespaceLocal(") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("resolveCanonicalMapHelperNameFromSpelling(") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("isMapHelperImportAliasNamespaceForMethodTargets(") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("rootedMapHelperAliasPathForMethodTargets(") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("rootAliasMapHelperNameForMethodTargets(") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("isRootedMapHelperAliasPathForMethodTargets(") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("isStdNamespacedMapHelper") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("resolvedCanonicalMapHelperName") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("canonicalMapHelperName") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("isCanonicalMapAccessMethodName") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("canonicalKeyValueHelperPathLocal(") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("canonicalKeyValueHelperNamespaceLocal(") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("resolveCanonicalKeyValueHelperNameFromSpelling(") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("isKeyValueHelperImportAliasNamespaceForMethodTargets(") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("rootedKeyValueHelperAliasPathForMethodTargets(") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("rootAliasKeyValueHelperNameForMethodTargets(") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("isRootedKeyValueHelperAliasPathForMethodTargets(") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("isStdNamespacedKeyValueHelper") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("resolvedCanonicalKeyValueHelperName") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("canonicalKeyValueHelperName") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("isCanonicalKeyValueAccessMethodName") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("auto resolveMapTarget") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("auto resolveExperimentalMapTarget") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("auto resolveKeyValueTarget") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("auto resolveExperimentalKeyValueTarget") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("setIndexedArgsPackMapMethodTarget") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find(
            "setIndexedArgsPackKeyValueMethodTarget") != std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("isMapElementType") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("isKeyValueElementType") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("mapTypeText") == std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("keyValueTypeText") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("resolvedIndexedMapType") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("resolvedIndexedKeyValueType") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find(
            "metadataBackedKeyValueHelperRootAliasMethodName(candidate)") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("keyValueValueType") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("std::string mapValueType") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("extractExperimentalMapFieldTypes =") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find(
            "extractExperimentalKeyValueFieldTypes =") != std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("extractAnyMapKeyValueTypes") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("extractAnyKeyValueTypes") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find(
            "experimentalMapBackingLeafForMethodTargets") == std::string::npos);
  CHECK(s.methodTargetResolutionSource.find(
            "experimentalKeyValueBackingLeafForMethodTargets") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find(
            "isUnspecializedExperimentalMapBackingTypeForMethodTargets") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find(
            "isSpecializedExperimentalMapBackingTypeForMethodTargets") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find(
            "isUnspecializedExperimentalKeyValueBackingTypeForMethodTargets") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find(
            "isSpecializedExperimentalKeyValueBackingTypeForMethodTargets") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("resolveMapValueType") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("resolveKeyValueValueType") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("isWrappedMapTypeText") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("isWrappedKeyValueTypeText") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("isWrappedMapReceiver") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("isWrappedKeyValueReceiver") !=
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find("isDirectMapConstructorReceiverCall") ==
        std::string::npos);
  CHECK(s.methodTargetResolutionSource.find(
            "isDirectKeyValueConstructorReceiverCall") != std::string::npos);
  CHECK(s.receiverPathsSource.find("resolvedReceiverPath == \"/map\"") ==
        std::string::npos);
  CHECK(s.receiverPathsSource.find(
            "isRootMapCollectionReceiverPath(resolvedReceiverPath)") !=
        std::string::npos);
  CHECK(s.receiverPathsSource.find("isSpecializedExperimentalMapBackingPath") ==
        std::string::npos);
  CHECK(s.receiverPathsSource.find(
            "isSpecializedExperimentalKeyValueBackingPath") !=
        std::string::npos);
  CHECK(s.receiverPathsSource.find("keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("resolvedMapHelperName") ==
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("mapNamespacedMethodCompatibilityPath") ==
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("removedMapMethodPath") ==
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("removedKeyValueMethodPath") !=
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("isCanonicalMapAccessMethodName") ==
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("canonicalMapAccessReturnsString") ==
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find(
            "rejectBuiltinStringCountShadowOnMapAccessReceiver") ==
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("canonicalMapHelperName") ==
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find(
            "metadataBackedCanonicalKeyValueHelperPath(helperName)") !=
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("resolvedKeyValueHelperName") !=
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find(
            "keyValueNamespacedMethodCompatibilityPath") !=
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("isCanonicalKeyValueAccessMethodName") !=
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("canonicalKeyValueAccessReturnsString") !=
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find(
            "rejectBuiltinStringCountShadowOnKeyValueAccessReceiver") !=
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("canonicalKeyValueHelperName") !=
        std::string::npos);
  CHECK(s.privateExprValidationSource.find(
            "isDirectStdNamespacedVectorCountWrapperMapTarget") ==
        std::string::npos);
  CHECK(s.privateExprValidationSource.find("getDirectMapHelperCompatibilityPath") ==
        std::string::npos);
  CHECK(s.privateExprValidationSource.find(
            "isDirectStdNamespacedVectorCountWrapperKeyValueTarget") !=
        std::string::npos);
  CHECK(s.privateExprValidationSource.find("getDirectKeyValueHelperCompatibilityPath") !=
        std::string::npos);
  CHECK(s.exprCollectionDispatchSetupSource.find(
            "isDirectStdNamespacedVectorCountWrapperMapTarget") ==
        std::string::npos);
  CHECK(s.exprCollectionDispatchSetupSource.find("directRemovedMapCompatibilityPath") ==
        std::string::npos);
  CHECK(s.exprCollectionDispatchSetupSource.find("directMapHelperCompatibilityPath") ==
        std::string::npos);
  CHECK(s.exprCollectionDispatchSetupSource.find("resolveKeyValueTarget") !=
        std::string::npos);
  CHECK(s.exprCollectionDispatchSetupSource.find("auto resolveMapTarget") ==
        std::string::npos);
  CHECK(s.exprCollectionDispatchSetupSource.find(
            "isDirectStdNamespacedVectorCountWrapperKeyValueTarget") !=
        std::string::npos);
  CHECK(s.exprCollectionDispatchSetupSource.find(
            "directRemovedKeyValueCompatibilityPath") !=
        std::string::npos);
  CHECK(s.exprCollectionDispatchSetupSource.find(
            "directKeyValueHelperCompatibilityPath") !=
        std::string::npos);
  CHECK(s.exprCollectionCountCapacitySource.find(
            "isDirectStdNamespacedVectorCountWrapperMapTarget") ==
        std::string::npos);
  CHECK(s.exprCollectionCountCapacitySource.find(
            "isDirectStdNamespacedVectorCountWrapperKeyValueTarget") !=
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("preferredExperimentalMapHelperTarget") ==
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find(
            "preferredCanonicalExperimentalMapHelperTarget") ==
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("canonicalExperimentalMapHelperPath") ==
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find(
            "canonicalizeExperimentalMapHelperResolvedPath") ==
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find(
            "shouldLogicalCanonicalizeDefinedExperimentalMapHelperPath") ==
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("preferredBareMapHelperTarget") ==
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("specializedExperimentalMapHelperTarget") ==
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find(
            "tryRewriteCanonicalExperimentalMapHelperCall") ==
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find(
            "explicitCanonicalExperimentalMapBorrowedHelperPath") ==
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("mapNamespacedMethodCompatibilityPath") ==
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("directMapHelperCompatibilityPath") ==
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find(
            "preferredExperimentalKeyValueHelperTarget") !=
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find(
            "preferredCanonicalExperimentalKeyValueHelperTarget") !=
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("canonicalExperimentalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find(
            "canonicalizeExperimentalKeyValueHelperResolvedPath") !=
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find(
            "shouldLogicalCanonicalizeDefinedExperimentalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("preferredBareKeyValueHelperTarget") !=
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find(
            "specializedExperimentalKeyValueHelperTarget") !=
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find(
            "tryRewriteCanonicalExperimentalKeyValueHelperCall") !=
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find(
            "explicitCanonicalExperimentalKeyValueBorrowedHelperPath") !=
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find(
            "keyValueNamespacedMethodCompatibilityPath") !=
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("directKeyValueHelperCompatibilityPath") !=
        std::string::npos);
  CHECK(s.templateCoreSource.find("name == \"/map\"") == std::string::npos);
  CHECK(s.templateCoreSource.find("name == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(s.templateCoreSource.find("importPath == \"/std/collections/map\"") ==
        std::string::npos);
  CHECK(s.templateCoreSource.find("path == \"/std/collections/map/entry\"") ==
        std::string::npos);
  CHECK(s.templateCoreSource.find("\"/map/entry\"") == std::string::npos);
  CHECK(s.templateCoreSource.find("\"/map/entry__\"") == std::string::npos);
  CHECK(s.templateCoreSource.find("isTemplateMonomorphMapImportAlias(name)") !=
        std::string::npos);
  CHECK(s.templateCoreSource.find(
            "isTemplateMonomorphMapConstructorCallPath(resolvedPath)") !=
        std::string::npos);
  CHECK(s.templateCoreSource.find("isMapEntryConstructorExpr") ==
        std::string::npos);
  CHECK(s.templateCoreSource.find("isKeyValueEntryConstructorExpr") !=
        std::string::npos);
  CHECK(s.templateCoreSource.find("preferMapEntryArgsPackOverload") ==
        std::string::npos);
  CHECK(s.templateCoreSource.find("preferKeyValueEntryArgsPackOverload") !=
        std::string::npos);
  CHECK(s.templateCoreSource.find("metadataBackedKeyValueHelperMethodName(path)") !=
        std::string::npos);
  CHECK(s.templateReceiverSource.find("|| resolvedPath == \"/map/") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find("path.rfind(\"/map/\", 0)") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find(
            "isTemplateMonomorphMapImportAliasHelperPath(path)") !=
        std::string::npos);
  CHECK(s.templateReceiverSource.find("templateMonomorphMapHelperSurfaceMetadata") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find("resolveTemplateMonomorphMapHelperName") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find("templateMonomorphCanonicalMapHelperPath") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find("resolveTemplateMonomorphCanonicalMapHelperName") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find("isTemplateMonomorphCanonicalMapHelperPath") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find("isTemplateMonomorphCanonicalMapValueAccessPath") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find("isTemplateMonomorphCanonicalMapCountPath") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find("templateMonomorphPreferredMapHelperSpellingForMember") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find("canonicalMapHelperUnknownTargetPath") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find("experimentalMapHelperPathForCanonicalHelper") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find("experimentalMapHelperPathForWrapperHelper") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find(
            "experimentalMapBackingLeafForReceiverResolution") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find(
            "experimentalKeyValueBackingLeafForReceiverResolution") !=
        std::string::npos);
  CHECK(s.templateReceiverSource.find(
            "isUnspecializedExperimentalMapBackingTypeForReceiverResolution") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find(
            "isSpecializedExperimentalMapBackingTypeForReceiverResolution") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find(
            "isUnspecializedExperimentalKeyValueBackingTypeForReceiverResolution") !=
        std::string::npos);
  CHECK(s.templateReceiverSource.find(
            "isSpecializedExperimentalKeyValueBackingTypeForReceiverResolution") !=
        std::string::npos);
  CHECK(s.templateReceiverSource.find("templateMonomorphKeyValueHelperSurfaceMetadata") !=
        std::string::npos);
  CHECK(s.templateReceiverSource.find("resolveTemplateMonomorphKeyValueHelperName") !=
        std::string::npos);
  CHECK(s.templateReceiverSource.find("templateMonomorphCanonicalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.templateReceiverSource.find("resolveTemplateMonomorphCanonicalKeyValueHelperName") !=
        std::string::npos);
  CHECK(s.templateReceiverSource.find("isTemplateMonomorphCanonicalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.templateReceiverSource.find("isTemplateMonomorphCanonicalKeyValueAccessPath") !=
        std::string::npos);
  CHECK(s.templateReceiverSource.find("isTemplateMonomorphCanonicalKeyValueCountPath") !=
        std::string::npos);
  CHECK(s.templateReceiverSource.find("templateMonomorphPreferredKeyValueHelperSpellingForMember") !=
        std::string::npos);
  CHECK(s.templateReceiverSource.find("canonicalKeyValueHelperUnknownTargetPath") !=
        std::string::npos);
  CHECK(s.templateReceiverSource.find("experimentalKeyValueHelperPathForCanonicalHelper") !=
        std::string::npos);
  CHECK(s.templateReceiverSource.find("experimentalKeyValueHelperPathForWrapperHelper") !=
        std::string::npos);
  CHECK(s.templateReceiverSource.find("mapsToBorrowedSoaHelper") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find("resolvesToBorrowedSoaHelper") !=
        std::string::npos);
  CHECK(s.templateReceiverSource.find("resolvesExperimentalMapValueReceiver") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find("resolvesExperimentalKeyValueReceiver") !=
        std::string::npos);
  CHECK(s.templateReceiverSource.find("resolvesExperimentalMapBorrowedReceiver") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find("resolvesExperimentalKeyValueBorrowedReceiver") !=
        std::string::npos);
  CHECK(s.templateReceiverSource.find(
            "resolveExperimentalMapValueReceiverTemplateArgs") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find(
            "resolveExperimentalKeyValueReceiverTemplateArgs") !=
        std::string::npos);
  CHECK(s.templateReceiverSource.find(
            "extractExperimentalMapValueReceiverTemplateArgsFromTypeText") ==
        std::string::npos);
  CHECK(s.templateReceiverSource.find(
            "extractExperimentalKeyValueReceiverTemplateArgsFromTypeText") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("templateMonomorphCanonicalMapHelperPath") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("isTemplateMonomorphCanonicalMapHelperPath") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("isTemplateMonomorphCanonicalMapValueAccessPath") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("isTemplateMonomorphCanonicalMapCountPath") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("canonicalMapHelperUnknownTargetPath") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("experimentalMapHelperPathForCanonicalHelper") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("experimentalMapHelperPathForWrapperHelper") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("experimentalWrapperMapPath") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("templateMonomorphCanonicalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("isTemplateMonomorphCanonicalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("isTemplateMonomorphCanonicalKeyValueAccessPath") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("isTemplateMonomorphCanonicalKeyValueCountPath") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("canonicalKeyValueHelperUnknownTargetPath") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("experimentalKeyValueHelperPathForCanonicalHelper") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("experimentalKeyValueHelperPathForWrapperHelper") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("experimentalWrapperKeyValuePath") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("resolvesExperimentalMapValueReceiver") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("resolvesExperimentalKeyValueReceiver") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("resolvesExperimentalMapBorrowedReceiver") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("resolvesExperimentalKeyValueBorrowedReceiver") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find(
            "resolveExperimentalMapValueReceiverTemplateArgs") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find(
            "resolveExperimentalKeyValueReceiverTemplateArgs") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find(
            "resolvedPath == \"/std/collections/map/count\" || resolvedPath == \"/map/count\"") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find(
            "experimentalCollectionConstructorPathLocal(\"map\", \"mapNew\")") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("const std::string compatibilityPath = \"/map/\" +") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("resolvedPath.rfind(\"/map/\", 0) == 0) &&") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find(
            "removedMapCompatibilityPath.rfind(\"/map/\"") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("removedMapCompatibilityHelperFromPath") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("removedMapCompatibilityPath") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find(
            "const std::string removedMapCompatibilityHelper =") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("isRemovedMapCompatibilityPath") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("mapCompatibilityHelperBase(") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("keyValueCompatibilityHelperBase(") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("keyValueConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("forwardedEmptyConstructorPath") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("removedKeyValueCompatibilityHelperFromPath") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("removedKeyValueCompatibilityPath") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("removedKeyValueCompatibilityHelper") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("isRemovedKeyValueCompatibilityPath") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("mapHelperReceiverExpr") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("mutableMapHelperReceiverExpr") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("resolvesBuiltinMapReceiver") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("rewriteCanonicalExperimentalMapConstructorBinding") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("rewriteCanonicalExperimentalMapConstructorExpr") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("resolvesBorrowedExperimentalMapReceiver") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("resolvesExperimentalMapBorrowedReceiver") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("experimentalMapPath") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("experimentalMapReceiverExpr") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("rejectsWrapperReturnedExperimentalMapAccess") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("inferredCanonicalMapReceiverTemplateArgs") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("isBuiltinMapCountPath") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("isCanonicalBuiltinMapHelperPath") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("rewriteNestedExperimentalMapConstructorValue") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("rewriteNestedExperimentalMapResultOkPayloadValue") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("rewriteMapTargetValueForResolvedType") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("collectionHelperReceiverExpr") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("mutableCollectionHelperReceiverExpr") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("resolvesBuiltinKeyValueReceiver") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("rewriteCanonicalKeyValueConstructorBinding") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("rewriteCanonicalExperimentalKeyValueConstructorExpr") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("resolvesBorrowedExperimentalKeyValueReceiver") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("experimentalKeyValuePath") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("experimentalKeyValueReceiverExpr") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("rejectsWrapperReturnedKeyValueAccess") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("inferredCanonicalKeyValueReceiverTemplateArgs") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("isBuiltinKeyValueCountPath") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("isCanonicalBuiltinKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("rewriteNestedExperimentalKeyValueConstructorValue") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("rewriteNestedExperimentalKeyValueResultOkPayloadValue") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("rewriteKeyValueTargetValueForResolvedType") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find(
            "rewriteExperimentalMapTargetValueForType") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find(
            "rewriteExperimentalMapResultOkPayloadTree") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find(
            "rewriteExperimentalKeyValueTargetValueForType") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find(
            "rewriteExperimentalKeyValueResultOkPayloadTree") !=
        std::string::npos);
  CHECK(s.templateValueRewriteSource.find("RewriteMapValueFn") ==
        std::string::npos);
  CHECK(s.templateValueRewriteSource.find("RewriteNestedMapValueFn") ==
        std::string::npos);
  CHECK(s.templateValueRewriteSource.find("RewriteMapPayloadFn") ==
        std::string::npos);
  CHECK(s.templateValueRewriteSource.find(
            "rewriteExperimentalMapTargetValueForType") ==
        std::string::npos);
  CHECK(s.templateValueRewriteSource.find(
            "rewriteExperimentalMapResultOkPayloadTree") ==
        std::string::npos);
  CHECK(s.templateValueRewriteSource.find("RewriteKeyValueValueFn") !=
        std::string::npos);
  CHECK(s.templateValueRewriteSource.find("RewriteNestedKeyValueValueFn") !=
        std::string::npos);
  CHECK(s.templateValueRewriteSource.find("RewriteKeyValuePayloadFn") !=
        std::string::npos);
  CHECK(s.templateValueRewriteSource.find(
            "rewriteExperimentalKeyValueTargetValueForType") !=
        std::string::npos);
  CHECK(s.templateValueRewriteSource.find(
            "rewriteExperimentalKeyValueResultOkPayloadTree") !=
        std::string::npos);
  CHECK(s.templateTypeResolutionSource.find("isExplicitRemovedMapCompatibilityPath") ==
        std::string::npos);
  CHECK(s.templateTypeResolutionSource.find("isExplicitRemovedKeyValueCompatibilityPath") !=
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("borrowedCanonicalMapUnknownTarget") ==
        std::string::npos);
  CHECK(s.templateExpressionRewriteSource.find("borrowedCanonicalKeyValueUnknownTarget") !=
        std::string::npos);
  CHECK(s.templateCollectionCompatibilitySource.find("rawMethodName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(s.templateCollectionCompatibilitySource.find(
            "rawMethodName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.templateCollectionCompatibilitySource.find("value == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(s.templateCollectionCompatibilitySource.find(
            "metadataBackedKeyValueHelperMethodName(rawMethodName)") !=
        std::string::npos);
  CHECK(s.templateCollectionCompatibilitySource.find(
            "keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.templateCollectionCompatibilitySource.find(
            "isRemovedMapCompatibilityHelper(") == std::string::npos);
  CHECK(s.templateCollectionCompatibilitySource.find(
            "mapCompatibilityHelperBase(") == std::string::npos);
  CHECK(s.templateCollectionCompatibilitySource.find("mapBackingName") ==
        std::string::npos);
  CHECK(s.templateCollectionCompatibilitySource.find("bareGeneratedMapBacking") ==
        std::string::npos);
  CHECK(s.templateCollectionCompatibilitySource.find(
            "isRemovedKeyValueCompatibilityHelper(") != std::string::npos);
  CHECK(s.templateCollectionCompatibilitySource.find(
            "keyValueCompatibilityHelperBase(") != std::string::npos);
  CHECK(s.templateCollectionCompatibilitySource.find("keyValueBackingName") !=
        std::string::npos);
  CHECK(s.templateCollectionCompatibilitySource.find(
            "bareGeneratedKeyValueBacking") != std::string::npos);
  CHECK(s.templateBindingCallInferenceSource.find("std/collections/map") ==
        std::string::npos);
  CHECK(s.templateBindingCallInferenceSource.find("experimental_map") ==
        std::string::npos);
  CHECK(s.templateBindingCallInferenceSource.find("CollectionsMap") ==
        std::string::npos);
  CHECK(s.templateBindingCallInferenceSource.find(
            "isExperimentalCollectionBackingTypeName(\"map\"") ==
        std::string::npos);
  CHECK(s.templateFallbackTypeInferenceSource.find("std/collections/map") ==
        std::string::npos);
  CHECK(s.templateFallbackTypeInferenceSource.find("experimental_map") ==
        std::string::npos);
  CHECK(s.templateFallbackTypeInferenceSource.find("CollectionsMap") ==
        std::string::npos);
  CHECK(s.templateFallbackTypeInferenceSource.find("base == \"map\"") ==
        std::string::npos);
  CHECK(s.templateFallbackTypeInferenceSource.find("builtinCollection == \"map\"") ==
        std::string::npos);
  CHECK(s.templateFallbackTypeInferenceSource.find("return \"map<\"") ==
        std::string::npos);
  CHECK(s.templateFallbackTypeInferenceSource.find(
            "isExperimentalCollectionBackingTypeName(\"map\"") ==
        std::string::npos);
  CHECK(s.templateFallbackTypeInferenceSource.find(
            "isUnspecializedExperimentalKeyValueBackingTypeName(typeName)") !=
        std::string::npos);
  CHECK(s.templateFallbackTypeInferenceSource.find(
            "isQualifiedExperimentalKeyValueBackingTypeName(typeName)") !=
        std::string::npos);
  CHECK(s.templateFallbackTypeInferenceSource.find(
            "isUnspecializedExperimentalMapBackingTypeForFallbackInference") ==
        std::string::npos);
  CHECK(s.templateFallbackTypeInferenceSource.find(
            "isSpecializedExperimentalMapBackingTypeForFallbackInference") ==
        std::string::npos);
  CHECK(s.templateFallbackTypeInferenceSource.find(
            "isUnspecializedExperimentalKeyValueBackingTypeForFallbackInference") !=
        std::string::npos);
  CHECK(s.templateFallbackTypeInferenceSource.find(
            "isSpecializedExperimentalKeyValueBackingTypeForFallbackInference") !=
        std::string::npos);
  CHECK(s.templateFallbackTypeInferenceSource.find(
            "resolvesExperimentalMapValueTypeText") == std::string::npos);
  CHECK(s.templateFallbackTypeInferenceSource.find(
            "resolvesExperimentalKeyValueTypeText") != std::string::npos);
  CHECK(s.templateFallbackTypeInferenceSource.find("mapCollectionAliasToken()") !=
        std::string::npos);
  CHECK(s.templateReturnSetupSource.find("expectedExperimentalMapReturn") ==
        std::string::npos);
  CHECK(s.templateReturnSetupSource.find("expectedExperimentalKeyValueReturn") !=
        std::string::npos);
  CHECK(s.templateReturnOrchestrationSource.find("RewriteMapFn") ==
        std::string::npos);
  CHECK(s.templateReturnOrchestrationSource.find("rewriteMapReturn") ==
        std::string::npos);
  CHECK(s.templateReturnOrchestrationSource.find("expectedExperimentalMapReturn") ==
        std::string::npos);
  CHECK(s.templateReturnOrchestrationSource.find("RewriteKeyValueFn") !=
        std::string::npos);
  CHECK(s.templateReturnOrchestrationSource.find("rewriteKeyValueReturn") !=
        std::string::npos);
  CHECK(s.templateReturnOrchestrationSource.find("expectedExperimentalKeyValueReturn") !=
        std::string::npos);
  CHECK(s.templateDefinitionRewriteSource.find(
            "rewriteDefinitionExperimentalMapConstructorValue") ==
        std::string::npos);
  CHECK(s.templateDefinitionRewriteSource.find(
            "rewriteDefinitionExperimentalMapReturnConstructors") ==
        std::string::npos);
  CHECK(s.templateDefinitionRewriteSource.find(
            "rewriteCanonicalExperimentalMapConstructorExpr") ==
        std::string::npos);
  CHECK(s.templateDefinitionRewriteSource.find(
            "rewriteDefinitionExperimentalKeyValueConstructorValue") !=
        std::string::npos);
  CHECK(s.templateDefinitionRewriteSource.find(
            "rewriteDefinitionExperimentalKeyValueReturnConstructors") !=
        std::string::npos);
  CHECK(s.templateDefinitionRewriteSource.find(
            "rewriteCanonicalExperimentalKeyValueConstructorExpr") !=
        std::string::npos);
  CHECK(s.templateFallbackTypeInferenceSource.find(
            "const std::string mapCollectionAlias") == std::string::npos);
  CHECK(s.templateFallbackTypeInferenceSource.find(
            "const std::string keyValueCollectionAlias") !=
        std::string::npos);
  CHECK(s.templateConstructorRewriteSource.find("originalPath == \"/map\"") ==
        std::string::npos);
  CHECK(s.templateConstructorRewriteSource.find(
            "originalPath == \"/std/collections/map/map\"") ==
        std::string::npos);
  CHECK(s.templateConstructorRewriteSource.find(
            "experimentalCollectionConstructorPathLocal(\"map\"") ==
        std::string::npos);
  CHECK(s.templateConstructorRewriteSource.find(
            "isExperimentalCollectionBackingTypeName(\"map\"") ==
        std::string::npos);
  CHECK(s.templateConstructorRewriteSource.find(
            "isExperimentalKeyValueConstructorMemberPathLocal(resolvedArgPath, \"entry\")") !=
        std::string::npos);
  CHECK(s.templateConstructorRewriteSource.find(
            "isExperimentalKeyValueEntryBackingTypeName(normalizedArgType)") !=
        std::string::npos);
  CHECK(s.templateConstructorRewriteSource.find(
            "experimentalKeyValueConstructorMemberPathLocal(\"map\")") !=
        std::string::npos);
  CHECK(s.templateConstructorRewriteSource.find(
            "isCanonicalMapConstructorRewriteSourcePath(originalPath)") !=
        std::string::npos);
  CHECK(s.templateConstructorRewriteSource.find(
            "rewriteCanonicalExperimentalMapConstructorExpr") ==
        std::string::npos);
  CHECK(s.templateConstructorRewriteSource.find(
            "rewriteCanonicalExperimentalKeyValueConstructorExpr") !=
        std::string::npos);
  CHECK(s.templateConstructorRewriteSource.find(
            "keyValueConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.templateConstructorRewriteSource.find(
            "metadata->importAliasSpellings") !=
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("\"/std/collections/map/\" + methodName, \"/map/\" + methodName") ==
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("isExplicitMapAccessStructReturnCompatibilityCall") ==
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("rawMethodName == \"map/at\"") ==
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("rawMethodName == \"std/collections/map/at\"") ==
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("\"/std/collections/map/\" + methodName") ==
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("candidate.rfind(\"/std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("explicitMapHelperName") ==
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("isExplicitMapAccessStructReturnMethod") ==
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("candidateHelperName") ==
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("sourceMethodMapHelperPath") ==
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("sourceMapHelperPath") ==
        std::string::npos);
  CHECK(s.inferStructReturnSource.find(
            "resolveExplicitPublishedMapHelperExprMemberName(") ==
        std::string::npos);
  CHECK(s.inferStructReturnSource.find(
            "resolveCanonicalCompatibilityMapHelperNameFromResolvedPath(") ==
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("explicitKeyValueHelperName") !=
        std::string::npos);
  CHECK(s.inferStructReturnSource.find(
            "isExplicitKeyValueAccessStructReturnMethod") !=
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("candidateKeyValueHelperName") !=
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("sourceMethodKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("sourceKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("const bool isKeyValueReceiver") !=
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("const bool isMapReceiver") ==
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("const bool isExperimentalKeyValueReceiver") !=
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("const bool isExperimentalMapReceiver") ==
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("isSpecializedExperimentalMapBackingPath") ==
        std::string::npos);
  CHECK(s.inferStructReturnSource.find(
            "isSpecializedExperimentalKeyValueBackingPath") != std::string::npos);
  CHECK(s.inferStructReturnSource.find(
            "resolveExplicitPublishedKeyValueHelperExprMemberName(") !=
        std::string::npos);
  CHECK(s.inferStructReturnSource.find("metadataBackedCanonicalKeyValueHelperPath(methodName)") !=
        std::string::npos);
  CHECK(s.inferStructReturnSource.find(
            "resolveCanonicalCompatibilityKeyValueHelperNameFromResolvedPath(") !=
        std::string::npos);
  CHECK(s.inferStructReturnHelpersSource.find("isExplicitMapAccessStructReturnCompatibilityCall") ==
        std::string::npos);
  CHECK(s.inferStructReturnHelpersSource.find("normalized == \"map/at\"") ==
        std::string::npos);
  CHECK(s.inferStructReturnHelpersSource.find("return \"/map\"") ==
        std::string::npos);
  CHECK(s.inferStructReturnHelpersSource.find("collectionTypePathLocal(\"map\"") ==
        std::string::npos);
  CHECK(s.inferStructReturnHelpersSource.find(
            "mapCollectionMarkerPathForInferStructReturn()") ==
        std::string::npos);
  CHECK(s.inferStructReturnHelpersSource.find(
            "unrootedMapHelperPrefixForInferStructReturn()") ==
        std::string::npos);
  CHECK(s.inferStructReturnHelpersSource.find(
            "mapValueRootForInferStructReturn()") ==
        std::string::npos);
  CHECK(s.inferStructReturnHelpersSource.find(
            "specializedExperimentalMapStructReturnPath") ==
        std::string::npos);
  CHECK(s.inferStructReturnHelpersSource.find(
            "unrootedMapPrefix") ==
        std::string::npos);
  CHECK(s.inferStructReturnHelpersSource.find(
            "keyValueCollectionMarkerPathForInferStructReturn()") !=
        std::string::npos);
  CHECK(s.inferStructReturnHelpersSource.find(
            "unrootedKeyValueHelperPrefixForInferStructReturn()") !=
        std::string::npos);
  CHECK(s.inferStructReturnHelpersSource.find(
            "keyValueBackingRootForInferStructReturn()") !=
        std::string::npos);
  CHECK(s.inferStructReturnHelpersSource.find(
            "specializedExperimentalKeyValueStructReturnPath") !=
        std::string::npos);
  CHECK(s.inferStructReturnHelpersSource.find(
            "unrootedKeyValuePrefix") !=
        std::string::npos);
  CHECK(s.inferStructReturnHelpersSource.find("keyValueConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.inferStructReturnHelpersSource.find("mapHelperSurfaceMetadataLocal()") ==
        std::string::npos);
  CHECK(s.inferStructReturnHelpersSource.find(
            "keyValueHelperSurfaceMetadataForInferStructReturn()") !=
        std::string::npos);
  CHECK(s.lowererStructReturnPathSource.find("const std::string mapPrefix = \"map/\"") ==
        std::string::npos);
  CHECK(s.lowererStructReturnPathSource.find("const std::string stdMapPrefix = \"std/collections/map/\"") ==
        std::string::npos);
  CHECK(s.lowererStructReturnPathSource.find("keyValueCollectionAliasRoot() + \"/\" + methodName") !=
        std::string::npos);
  CHECK(s.lowererStructReturnPathSource.find("return {\"/std/collections/map/\" + methodName}") ==
        std::string::npos);
  CHECK(s.inferMethodResolutionSource.find("joinMethodTarget(\"/std/collections/map\", helperSuffix)") ==
        std::string::npos);
  CHECK(s.inferMethodResolutionSource.find("joinMethodTarget(\"/map\", helperSuffix)") ==
        std::string::npos);
  CHECK(s.inferMethodResolutionSource.find("normalizedMethodName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.inferMethodResolutionSource.find("receiverHelperName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(s.inferMethodResolutionSource.find("receiverHelperName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.inferMethodResolutionSource.find(
            "metadataBackedKeyValueHelperMethodName(normalizedMethodName)") !=
        std::string::npos);
  CHECK(s.inferMethodResolutionSource.find(
            "metadataBackedKeyValueHelperMethodName(receiverHelperName)") !=
        std::string::npos);
  CHECK(s.inferMethodResolutionSource.find("const std::string mapHelperName") ==
        std::string::npos);
  CHECK(s.inferMethodResolutionSource.find("isCanonicalMapAccessMethodName") ==
        std::string::npos);
  CHECK(s.inferMethodResolutionSource.find("const std::string keyValueHelperName") !=
        std::string::npos);
  CHECK(s.inferMethodResolutionSource.find("isCanonicalKeyValueAccessMethodName") !=
        std::string::npos);
  CHECK(s.inferMethodResolutionHelpersSource.find("const std::string alias = \"/map/\" + selectedHelperName") ==
        std::string::npos);
  CHECK(s.inferMethodResolutionHelpersSource.find("\"/std/collections/map/\" + selectedHelperName") ==
        std::string::npos);
  CHECK(s.inferMethodResolutionHelpersSource.find(
            "keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.inferMethodResolutionHelpersSource.find(
            "mapHelperSurfaceMetadataForInferMethodResolution") ==
        std::string::npos);
  CHECK(s.inferMethodResolutionHelpersSource.find(
            "canonicalMapHelperPathForInferMethodResolution") ==
        std::string::npos);
  CHECK(s.inferMethodResolutionHelpersSource.find(
            "keyValueHelperSurfaceMetadataForInferMethodResolution") !=
        std::string::npos);
  CHECK(s.inferMethodResolutionHelpersSource.find(
            "canonicalKeyValueHelperPathForInferMethodResolution") !=
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("preferredKeyValueMethodTargetForCall(") !=
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("preferredMapMethodTargetForCall(") ==
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("resolveMapValueType(") ==
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("resolveKeyValueValueType(") !=
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("resolveMapKeyType(") ==
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("resolveKeyValueKeyType(") !=
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("resolveMapBinding") ==
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("extractExperimentalMapFieldTypes") ==
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("isDirectMapConstructorCall") ==
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("resolveKeyValueBinding") !=
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("extractExperimentalKeyValueFieldTypes") !=
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find("isDirectKeyValueConstructorCall") !=
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find(
            "extractExperimentalMapFieldTypesFromStructPath") ==
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find(
            "extractExperimentalKeyValueFieldTypesFromStructPath") !=
        std::string::npos);
  CHECK(s.privateExprValidationSource.find("resolveMapKeyType") ==
        std::string::npos);
  CHECK(s.privateExprValidationSource.find("resolveKeyValueKeyType") !=
        std::string::npos);
  CHECK(s.inferMethodResolutionHelpersSource.find(
            "preferredKeyValueMethodTargetForCall(") != std::string::npos);
  CHECK(s.inferMethodResolutionHelpersSource.find("preferredMapMethodTargetForCall(") ==
        std::string::npos);
  CHECK(s.inferMethodResolutionSource.find("preferredKeyValueMethodTargetForCall(") !=
        std::string::npos);
  CHECK(s.inferMethodResolutionSource.find("preferredMapMethodTargetForCall(") ==
        std::string::npos);
  CHECK(s.inferMethodResolutionSource.find("const auto &resolveMapTarget") ==
        std::string::npos);
  CHECK(s.inferMethodResolutionSource.find("const auto &resolveKeyValueTarget") !=
        std::string::npos);
  CHECK(s.inferMethodResolutionSource.find("setIndexedArgsPackMapMethodTarget") ==
        std::string::npos);
  CHECK(s.inferMethodResolutionSource.find(
            "setIndexedArgsPackKeyValueMethodTarget") != std::string::npos);
  CHECK(s.inferMethodResolutionSource.find("indexedMapTypeText") ==
        std::string::npos);
  CHECK(s.inferMethodResolutionSource.find("indexedKeyValueTypeText") !=
        std::string::npos);
}

TEST_SUITE_END();
