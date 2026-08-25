#include "primec/support/StdlibSurfaceRegistry.h"

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
  // TODO-4688: "collections.map_helpers" is generically derived (file stem +
  // surface suffix) rather than a hardcoded literal in the registry source,
  // so assert the canonical registry produces it at runtime instead of
  // scanning for the literal substring.
  const auto *mapHelpersMetadata =
      primec::findStdlibSurfaceMetadata(primec::StdlibSurfaceId::CollectionsManifestSurface2);
  REQUIRE(mapHelpersMetadata != nullptr);
  CHECK(mapHelpersMetadata->bridgeKey == "collections.map_helpers");
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
}

TEST_SUITE_END();
