#include "third_party/doctest.h"

#include "test_stdlib_map_ownership_shared.h"

TEST_SUITE_BEGIN("primestruct.stdlib.map_ownership");

TEST_CASE("canonical map surface owns standalone stdlib implementation expr method and collection dispatch inference") {
  const MapOwnershipSources s = loadMapOwnershipSources();
  CHECK(s.exprMethodResolutionSource.find("preferredKeyValueMethodTargetForCall(") !=
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("preferredMapMethodTargetForCall(") ==
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("resolveInferredMapMethodFallback") ==
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find(
            "resolveInferredKeyValueMethodFallback") != std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("hasVisibleStdlibMapMethodDefinition") ==
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find(
            "hasVisibleStdlibKeyValueMethodDefinition") != std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("isMissingStdlibMapMethodDefinition") ==
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find(
            "isMissingStdlibKeyValueMethodDefinition") != std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("resolveIndexedArgsPackMapMethod") ==
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find(
            "resolveIndexedArgsPackKeyValueMethod") != std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("hasIndexedArgsPackMapMethodTarget") ==
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find(
            "hasIndexedArgsPackKeyValueMethodTarget") != std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("mapElemType") == std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("keyValueElemType") !=
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find(
            "keepBuiltinIndexedArgsPackKeyValueMethod") != std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("keepBuiltinIndexedArgsPackMapMethod") ==
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("resolveKeyValueTargetWithTypes") !=
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("resolveExperimentalKeyValueTarget") !=
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("const auto &resolveExperimentalMapTarget") ==
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("resolveKeyValueTarget") !=
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("resolveMapTargetWithTypes") ==
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("auto resolveMapTarget") ==
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("keyValueValueType") !=
        std::string::npos);
  CHECK(s.exprMethodResolutionSource.find("std::string mapValueType") ==
        std::string::npos);
  CHECK(s.statementBodyArgumentsSource.find("preferredKeyValueMethodTargetForCall(") !=
        std::string::npos);
  CHECK(s.statementBodyArgumentsSource.find("preferredMapMethodTargetForCall(") ==
        std::string::npos);
  CHECK(s.exprLateUnknownTargetFallbacksSource.find(
            "preferredKeyValueMethodTargetForCall(") != std::string::npos);
  CHECK(s.exprLateUnknownTargetFallbacksSource.find("preferredMapMethodTargetForCall(") ==
        std::string::npos);
  CHECK(s.privateExprInferenceSource.find(
            "isIndexedArgsPackKeyValueReceiverTarget(") != std::string::npos);
  CHECK(s.privateExprInferenceSource.find("isIndexedArgsPackMapReceiverTarget(") ==
        std::string::npos);
  CHECK(s.privateExprValidationSource.find(
            "isIndexedArgsPackKeyValueReceiverTarget") != std::string::npos);
  CHECK(s.privateExprValidationSource.find("isIndexedArgsPackMapReceiverTarget") ==
        std::string::npos);
  CHECK(s.exprCollectionPredicatesSource.find(
            "isIndexedArgsPackKeyValueReceiverTarget(") != std::string::npos);
  CHECK(s.exprCollectionPredicatesSource.find("isIndexedArgsPackMapReceiverTarget(") ==
        std::string::npos);
  CHECK(s.exprCollectionPredicatesSource.find("mapElemType") ==
        std::string::npos);
  CHECK(s.exprCollectionPredicatesSource.find("keyValueElemType") !=
        std::string::npos);
  CHECK(s.exprSource.find("isIndexedArgsPackKeyValueReceiverTarget(") !=
        std::string::npos);
  CHECK(s.exprSource.find("isIndexedArgsPackMapReceiverTarget(") ==
        std::string::npos);
  CHECK(s.exprSource.find("isIndexedArgsPackMapMethodReceiver") ==
        std::string::npos);
  CHECK(s.exprSource.find("isIndexedArgsPackKeyValueMethodReceiver") !=
        std::string::npos);
  CHECK(s.exprTrySource.find("isIndexedArgsPackKeyValueReceiverTarget") !=
        std::string::npos);
  CHECK(s.exprTrySource.find("isIndexedArgsPackMapReceiverTarget") ==
        std::string::npos);
  CHECK(s.exprLateUnknownTargetFallbacksSource.find(
            "isIndexedArgsPackKeyValueReceiverTarget") != std::string::npos);
  CHECK(s.exprLateUnknownTargetFallbacksSource.find(
            "isIndexedArgsPackMapReceiverTarget") == std::string::npos);
  CHECK(s.inferMethodResolutionHelpersSource.find("isWrappedKeyValueTypeText") !=
        std::string::npos);
  CHECK(s.inferMethodResolutionHelpersSource.find("isWrappedMapTypeText") ==
        std::string::npos);
  CHECK(s.inferMethodResolutionHelpersSource.find("isWrappedKeyValueBinding") !=
        std::string::npos);
  CHECK(s.inferMethodResolutionHelpersSource.find("isWrappedMapBinding") ==
        std::string::npos);
  CHECK(s.inferMethodResolutionHelpersSource.find("isWrappedKeyValueReceiver") !=
        std::string::npos);
  CHECK(s.inferMethodResolutionHelpersSource.find("isWrappedMapReceiver") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find("std/collections/map/") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "directRemovedKeyValueCompatibilityPath == \"/map/") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find("isUnrootedMapHelperSurfacePath") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "canonicalExperimentalMapHelperResolved") == std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "rewrittenCanonicalExperimentalMapHelperCall") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "borrowedExplicitCanonicalExperimentalMapHelperPath") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "tryRewriteCanonicalExperimentalMapHelperCall") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "explicitCanonicalExperimentalMapBorrowedHelperPath") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "canonicalizeExperimentalMapHelperResolvedPath") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "shouldLogicalCanonicalizeDefinedExperimentalMapHelperPath") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find("directRemovedMapCompatibilityPath") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find("directMapHelperCompatibilityPath") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find("isRemovedMapAccessCompatibilityPath") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "isMapNamespacedAccessCompatibilityCall") == std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find("isUnrootedKeyValueHelperSurfacePath") !=
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "canonicalExperimentalKeyValueHelperResolved") !=
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "rewrittenCanonicalExperimentalKeyValueHelperCall") !=
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "borrowedExplicitCanonicalExperimentalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "tryRewriteCanonicalExperimentalKeyValueHelperCall") !=
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "explicitCanonicalExperimentalKeyValueBorrowedHelperPath") !=
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "canonicalizeExperimentalKeyValueHelperResolvedPath") !=
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "shouldLogicalCanonicalizeDefinedExperimentalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "directRemovedKeyValueCompatibilityPath") != std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find("directKeyValueHelperCompatibilityPath") !=
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "isRemovedKeyValueAccessCompatibilityPath") !=
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "isKeyValueNamespacedAccessCompatibilityCall") !=
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find("metadataBackedKeyValueHelperRootAliasMethodName") !=
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find("builtinKeyValueKeyType") !=
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find("builtinKeyValueValueType") !=
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find("builtinMapKeyType") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find("builtinMapValueType") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find("const auto &resolveMapTarget") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find("const auto &resolveKeyValueTarget") !=
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find("hasVisibleStdlibMapMethodDefinition") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "hasVisibleStdlibKeyValueMethodDefinition") != std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find("resolveStdlibMapMethodHelperName") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "resolveStdlibKeyValueMethodHelperName") != std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find("isMapMethodWithBuiltinReturn") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find("isKeyValueMethodWithBuiltinReturn") !=
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find("isMapMethodNeedingVisibleDefinition") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "isKeyValueMethodNeedingVisibleDefinition") != std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "isVisibleStdlibMapMethodWithBuiltinReturn") == std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "isVisibleStdlibKeyValueMethodWithBuiltinReturn") !=
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find("isMissingStdlibMapMethodDefinition") ==
        std::string::npos);
  CHECK(s.inferPreDispatchCallsSource.find(
            "isMissingStdlibKeyValueMethodDefinition") != std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("std/collections/map/") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("\"/map/\" +") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "resolvePreDispatchKeyValueHelperMemberToken(") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "resolvePreDispatchMapHelperMemberToken(") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("bareKeyValueWrapperHelperPath") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("bareMapWrapperHelperPath") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "normalizedBareKeyValueWrapperHelperPath") != std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "normalizedBareMapWrapperHelperPath") == std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "resolvePreDispatchKeyValueHelperResolvedPath(") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "resolvePreDispatchMapHelperResolvedPath(") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("removedMapCompatibilityHelperFromPath") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("removedMapCompatibilityHelper") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("isRemovedMapCompatibilityPreDispatchHelperName(") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("hasExactRemovedMapAliasDefinition") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("preDispatchKeyValueHelperSurfaceMetadata()") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("preDispatchMapHelperSurfaceMetadata()") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("keyValueKeyType") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("keyValueValueType") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("std::string mapKeyType") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("std::string mapValueType") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("const std::string &mapKeyType") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("canonicalKeyValueHelperPathLocal(") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("canonicalMapHelperPathLocal(") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "resolveCanonicalKeyValueHelperNameFromSpelling(") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "resolveCanonicalMapHelperNameFromSpelling(") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("removedKeyValueCompatibilityHelperFromPath") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("removedKeyValueCompatibilityHelper") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("isRemovedKeyValueCompatibilityPreDispatchHelperName(") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("hasExactRemovedKeyValueAliasDefinition") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("sourceMethodKeyValueHelperName") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("sourceMethodMapHelperName") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("isKeyValueReceiver") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("const bool isMapReceiver") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("isExperimentalKeyValueReceiver") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("const bool isExperimentalMapReceiver") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("receiverIsExperimentalMap") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("receiverIsExperimentalKeyValue") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("isExperimentalKeyValueReceiverExpr") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("isExperimentalMapReceiverExpr") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("isExperimentalMapTypeText") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("isExperimentalKeyValueTypeText") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("isBuiltinKeyValueTarget") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("isExperimentalKeyValueTarget") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("isBuiltinMapTarget") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("isExperimentalMapTarget") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "isCanonicalMapAccessReturnStructHelperName") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "isSourceSpelledCanonicalMapAccessCall") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("canonicalMapAccessDiagnostic") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "hasVisibleStdlibMapAccessDefinition") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("isBareMapAccessBuiltinSurface") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("hasBareKeyValueOperands") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("hasBareMapOperands") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "isCanonicalKeyValueAccessReturnStructHelperName") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "isSourceSpelledCanonicalKeyValueAccessCall") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("canonicalKeyValueAccessDiagnostic") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "hasVisibleStdlibKeyValueAccessDefinition") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "failPreDispatchDirectCallMapKeyMismatch") == std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "failPreDispatchDirectCallKeyValueKeyMismatch") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "resolvesNonRootExperimentalMapValueTarget") == std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "resolvesNonRootExperimentalKeyValueTarget") != std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "isNonRootExperimentalMapReceiverExpr") == std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "isNonRootExperimentalKeyValueReceiverExpr") != std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("isBareKeyValueAccessBuiltinSurface") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "canonicalExperimentalMapHelperResolved") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "rewrittenCanonicalExperimentalMapHelperCall") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "borrowedCanonicalExperimentalMapHelperPath") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "tryRewriteCanonicalExperimentalMapHelperCall") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "explicitCanonicalExperimentalMapBorrowedHelperPath") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "canonicalizeExperimentalMapHelperResolvedPath") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "shouldLogicalCanonicalizeDefinedExperimentalMapHelperPath") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "canonicalExperimentalKeyValueHelperResolved") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "rewrittenCanonicalExperimentalKeyValueHelperCall") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "borrowedCanonicalExperimentalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "tryRewriteCanonicalExperimentalKeyValueHelperCall") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "explicitCanonicalExperimentalKeyValueBorrowedHelperPath") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "canonicalizeExperimentalKeyValueHelperResolvedPath") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find(
            "shouldLogicalCanonicalizeDefinedExperimentalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("rootedMapAliasHelperPath(") ==
        std::string::npos);
  CHECK(s.exprPreDispatchDirectCallsSource.find("rootedKeyValueAliasHelperPath(") !=
        std::string::npos);
  CHECK(s.exprVectorHelpersSource.find("std/collections/map/") ==
        std::string::npos);
  CHECK(s.exprVectorHelpersSource.find("normalizedHelperName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(s.exprVectorHelpersSource.find("mapHelperName") == std::string::npos);
  CHECK(s.exprVectorHelpersSource.find("preferredBareMapHelperTarget") ==
        std::string::npos);
  CHECK(s.exprVectorHelpersSource.find("isRootMapAliasPath") == std::string::npos);
  CHECK(s.exprVectorHelpersSource.find("isRootKeyValueAliasPath") !=
        std::string::npos);
  CHECK(s.exprVectorHelpersSource.find("isLocalRootMapAliasCall") ==
        std::string::npos);
  CHECK(s.exprVectorHelpersSource.find("isLocalRootKeyValueAliasCall") !=
        std::string::npos);
  CHECK(s.exprDispatchBootstrapSource.find("isRootMapAliasPath") ==
        std::string::npos);
  CHECK(s.exprDispatchBootstrapSource.find("isRootKeyValueAliasPath") !=
        std::string::npos);
  CHECK(s.inferCollectionCallResolutionSource.find("isRootMapAliasPath") ==
        std::string::npos);
  CHECK(s.inferCollectionCallResolutionSource.find("isRootKeyValueAliasPath") !=
        std::string::npos);
  CHECK(s.inferCollectionCallResolutionSource.find("targetIsRootMapAlias") ==
        std::string::npos);
  CHECK(s.inferCollectionCallResolutionSource.find("targetIsRootKeyValueAlias") !=
        std::string::npos);
  CHECK(s.exprVectorHelpersSource.find("keyValueHelperName") !=
        std::string::npos);
  CHECK(s.exprVectorHelpersSource.find("preferredBareKeyValueHelperTarget") !=
        std::string::npos);
  CHECK(s.exprVectorHelpersSource.find(
            "metadataBackedKeyValueHelperMethodName(normalizedHelperName)") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("const std::string alias = \"/map/\" + std::string(helperName)") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("\"/std/collections/map/\" + std::string(helperName)") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("\"/map/\" + std::string(helperName)") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("collectionRewriteMapHelperMetadata()") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("canonicalMapHelperPathForRewrite(") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("resolveMapHelperMemberTokenForRewrite(") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("resolveExplicitMapHelperPathForRewrite(") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("preferredMapHelperLoweringPathForRewrite(") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("directExperimentalMapHelperSpelling") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("canonicalExperimentalMapHelperPath") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("preferredBareMapHelperTarget") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("isBareMapAccessHelperName(") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("specializedExperimentalMapHelperTarget") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("mapHelperReceiverIndex") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("bareMapHelperOperandIndices") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("tryRewriteBareMapHelperCall") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("hasResolvableMapHelperPath") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find(
            "tryRewriteCanonicalExperimentalMapHelperCall") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find(
            "explicitCanonicalExperimentalMapBorrowedHelperPath") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("hasVisibleMapHelperFamily") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("isPublishedMapConstructorReceiver") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("isRootMapConstructorCandidate") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("resolvesBorrowedExperimentalMap") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("resolvesExperimentalMapValue") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("resolveMapValueType(") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("resolveMapKeyType(") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("resolvesExperimentalMap =") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("resolvesCanonicalMap") ==
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("collectionRewriteKeyValueHelperMetadata()") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("canonicalKeyValueHelperPathForRewrite(") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("resolveKeyValueHelperMemberTokenForRewrite(") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("resolveExplicitKeyValueHelperPathForRewrite(") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("preferredKeyValueHelperLoweringPathForRewrite(") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("directExperimentalKeyValueHelperSpelling") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("canonicalExperimentalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("preferredBareKeyValueHelperTarget") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("isBareKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find(
            "specializedExperimentalKeyValueHelperTarget") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("keyValueHelperReceiverIndex") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("bareKeyValueHelperOperandIndices") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("tryRewriteBareKeyValueHelperCall") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("hasResolvableKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find(
            "tryRewriteCanonicalExperimentalKeyValueHelperCall") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find(
            "explicitCanonicalExperimentalKeyValueBorrowedHelperPath") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("hasVisibleKeyValueHelperFamily") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("isPublishedKeyValueConstructorReceiver") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("isRootKeyValueConstructorCandidate") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("resolvesBorrowedExperimentalKeyValue") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("resolvesExperimentalKeyValueValue") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("resolveKeyValueValueType(") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("resolveKeyValueKeyType(") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("resolvesExperimentalKeyValue") !=
        std::string::npos);
  CHECK(s.collectionHelperRewritesSource.find("resolvesCanonicalKeyValue") !=
        std::string::npos);
  CHECK(s.effectFreeCollectionsSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.effectFreeCollectionsSource.find(
            "mapHelperSurfaceMetadataForEffectFreeCollections(") ==
        std::string::npos);
  CHECK(s.effectFreeCollectionsSource.find("canonicalMapHelperPathLocal(") ==
        std::string::npos);
  CHECK(s.effectFreeCollectionsSource.find("unrootedCanonicalMapHelperPrefixLocal(") ==
        std::string::npos);
  CHECK(s.effectFreeCollectionsSource.find("stdMapPrefix") ==
        std::string::npos);
  CHECK(s.effectFreeCollectionsSource.find(
            "keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.effectFreeCollectionsSource.find(
            "keyValueHelperSurfaceMetadataForEffectFreeCollections(") !=
        std::string::npos);
  CHECK(s.effectFreeCollectionsSource.find("canonicalKeyValueHelperPathLocal(") !=
        std::string::npos);
  CHECK(s.effectFreeCollectionsSource.find(
            "unrootedCanonicalKeyValueHelperPrefixLocal(") !=
        std::string::npos);
  CHECK(s.effectFreeCollectionsSource.find("stdKeyValueHelperPrefix") !=
        std::string::npos);
  CHECK(s.passesEffectFreeSource.find("bareKeyValueCallPath") !=
        std::string::npos);
  CHECK(s.passesEffectFreeSource.find("bareMapCallPath") == std::string::npos);
  CHECK(s.buildParametersSource.find("normalizedType == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(s.buildParametersSource.find("isKeyValueSurfaceTypeName(normalizedType)") !=
        std::string::npos);
  CHECK(s.buildParametersSource.find("typeTextIsExperimentalMapValue") ==
        std::string::npos);
  CHECK(s.buildParametersSource.find("typeTextIsExperimentalKeyValue") !=
        std::string::npos);
  CHECK(s.buildParametersSource.find("typeTextCarriesExperimentalMapValue") ==
        std::string::npos);
  CHECK(s.buildParametersSource.find("typeTextCarriesExperimentalKeyValue") !=
        std::string::npos);
  CHECK(s.buildParametersSource.find("bindingCarriesExperimentalMapValue") ==
        std::string::npos);
  CHECK(s.buildParametersSource.find("bindingCarriesExperimentalKeyValue") !=
        std::string::npos);
  CHECK(s.buildParametersSource.find("isResolvedExperimentalMapConstructorPath") ==
        std::string::npos);
  CHECK(s.buildParametersSource.find(
            "isResolvedExperimentalKeyValueConstructorPath") !=
        std::string::npos);
  CHECK(s.buildParametersSource.find("isAllowedExperimentalMapDefaultExpr") ==
        std::string::npos);
  CHECK(s.buildParametersSource.find("isAllowedExperimentalKeyValueDefaultExpr") !=
        std::string::npos);
  CHECK(s.buildParametersSource.find("isDirectExperimentalMapConstructor") ==
        std::string::npos);
  CHECK(s.buildParametersSource.find("isDirectExperimentalKeyValueConstructor") !=
        std::string::npos);
  CHECK(s.buildParametersSource.find("isMapConstructorExpr") ==
        std::string::npos);
  CHECK(s.buildParametersSource.find("isKeyValueConstructorExpr") !=
        std::string::npos);
  CHECK(s.buildParametersSource.find("argCarriesExperimentalMapValue") ==
        std::string::npos);
  CHECK(s.buildParametersSource.find("argCarriesExperimentalKeyValue") !=
        std::string::npos);
  CHECK(s.buildParametersSource.find("returnsExperimentalMapValue") ==
        std::string::npos);
  CHECK(s.buildParametersSource.find("returnsExperimentalKeyValue") !=
        std::string::npos);
  // The composed key-value resolver was retired for the shared spelling
  // classifier (docs/CompatPathResolutionConsolidation.md Steps 2b/2c);
  // the pin flips to guard against the legacy helper returning.
  CHECK(s.buildInitializerInferenceSource.find("explicitStdKeyValueHelperName") ==
        std::string::npos);
  CHECK(s.buildInitializerInferenceSource.find(
            "classifyCollectionHelperSpelling(") != std::string::npos);
  CHECK(s.buildInitializerInferenceSource.find("explicitStdMapHelperName") ==
        std::string::npos);
  CHECK(s.buildCallResolutionSource.find("\"/std/collections/map/\"") ==
        std::string::npos);
  CHECK(s.buildCallResolutionSource.find("\"/std/collections/map/map\"") ==
        std::string::npos);
  CHECK(s.buildCallResolutionSource.find("keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.buildCallResolutionSource.find("keyValueConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.buildCallResolutionSource.find("mapHelperCanonicalMemberRootPath") ==
        std::string::npos);
  CHECK(s.buildCallResolutionSource.find("canonicalMapHelperAliasPath") ==
        std::string::npos);
  CHECK(s.buildCallResolutionSource.find("mapConstructorMetadata") ==
        std::string::npos);
  CHECK(s.buildCallResolutionSource.find("isMapHelperNamespacePrefix") ==
        std::string::npos);
  CHECK(s.buildCallResolutionSource.find("isRemovedMapCompatibilityHelper(") ==
        std::string::npos);
  CHECK(s.buildCallResolutionSource.find(
            "keyValueHelperCanonicalMemberRootPath") !=
        std::string::npos);
  CHECK(s.buildCallResolutionSource.find("canonicalKeyValueHelperAliasPath") !=
        std::string::npos);
  CHECK(s.buildCallResolutionSource.find("keyValueConstructorMetadata") !=
        std::string::npos);
  CHECK(s.buildCallResolutionSource.find("isKeyValueHelperNamespacePrefix") !=
        std::string::npos);
  CHECK(s.buildCallResolutionSource.find("isRemovedKeyValueCompatibilityHelper(") !=
        std::string::npos);
  CHECK(s.buildReturnKindsSource.find("return \"/map\"") == std::string::npos);
  CHECK(s.buildReturnKindsSource.find(
            "mapCollectionMarkerPathForBuildReturnKinds()") ==
        std::string::npos);
  CHECK(s.buildReturnKindsSource.find(
            "keyValueCollectionMarkerPathForBuildReturnKinds()") !=
        std::string::npos);
  CHECK(s.buildReturnKindsSource.find(
            "keyValueConstructorSurfaceMetadataForBuildReturnKinds()") !=
        std::string::npos);
  CHECK(s.buildReturnKindsSource.find("keyValueConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.buildInitializerInferenceSource.find("const std::string alias = \"/map/\" + helperName") ==
        std::string::npos);
  CHECK(s.buildInitializerInferenceSource.find("normalizedPrefix == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(s.buildInitializerInferenceSource.find("normalizedName.rfind(\"std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(s.buildInitializerInferenceSource.find("isSpecializedExperimentalMapBackingPath") ==
        std::string::npos);
  CHECK(s.buildInitializerInferenceSource.find(
            "isSpecializedExperimentalKeyValueBackingPath") !=
        std::string::npos);
  // Retired with the legacy composed resolver (Steps 2b/2c); the
  // metadata-backed canonical mapping now lives behind the classifier.
  CHECK(s.buildInitializerInferenceSource.find("metadataBackedCanonicalKeyValueHelperPath(helperName)") ==
        std::string::npos);
  CHECK(s.buildInitializerInferenceCallsSource.find(
            "isExperimentalCollectionBackingTypeName(\"map\"") ==
        std::string::npos);
  CHECK(s.buildInitializerInferenceCallsSource.find("collection == \"map\"") ==
        std::string::npos);
  CHECK(s.buildInitializerInferenceCallsSource.find("collectionName == \"map\"") ==
        std::string::npos);
  CHECK(s.buildInitializerInferenceCallsSource.find(
            "resolveCallCollectionTemplateArgs(*initializerExprForInference, \"map\"") ==
        std::string::npos);
  CHECK(s.buildInitializerInferenceCallsSource.find(
            "isQualifiedExperimentalKeyValueBackingTypeName(") !=
        std::string::npos);
  CHECK(s.buildInitializerInferenceCallsSource.find("isSpecializedExperimentalMapBackingPath") ==
        std::string::npos);
  CHECK(s.buildInitializerInferenceCallsSource.find(
            "isSpecializedExperimentalKeyValueBackingPath") !=
        std::string::npos);
  CHECK(s.buildInitializerInferenceCallsSource.find("mapCollectionAliasToken()") !=
        std::string::npos);
  CHECK(s.buildInitializerInferenceCallsSource.find("const std::string mapAlias") ==
        std::string::npos);
  CHECK(s.buildInitializerInferenceCallsSource.find("const std::string keyValueAlias") !=
        std::string::npos);
  CHECK(s.buildInitializerInferenceCallsSource.find("std::vector<std::string> mapArgs") ==
        std::string::npos);
  CHECK(s.buildInitializerInferenceCallsSource.find("std::vector<std::string> keyValueArgs") !=
        std::string::npos);
  CHECK(s.buildInitializerInferenceCallsSource.find("isResolvedMapConstructorPath") ==
        std::string::npos);
  CHECK(s.buildInitializerInferenceCallsSource.find(
            "isResolvedKeyValueConstructorPath") != std::string::npos);
}

TEST_CASE("canonical map surface owns standalone stdlib implementation expr method and collection dispatch inference: dispatch and fallback paths") {
  const MapOwnershipSources s = loadMapOwnershipSources();
  CHECK(s.inferCollectionDispatchSource.find("resolvedPath.rfind(\"/map/\"") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSource.find("resolvedPath == \"/map/at\"") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSource.find("resolvedPath == \"/map/contains\"") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSource.find(
            "mapHelperSurfaceMetadataForInferCollectionDispatch(") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSource.find(
            "resolveMapHelperResolvedPathForInferCollectionDispatch(") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSource.find("resolvedMapHelperName") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSource.find(
            "keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.inferCollectionDispatchSource.find(
            "keyValueHelperSurfaceMetadataForInferCollectionDispatch(") !=
        std::string::npos);
  CHECK(s.inferCollectionDispatchSource.find(
            "resolveKeyValueHelperResolvedPathForInferCollectionDispatch(") !=
        std::string::npos);
  CHECK(s.inferCollectionDispatchSource.find("resolvedKeyValueHelperName") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("const std::string alias = \"/map/\" + std::string(helperName)") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("base == \"/std/collections/map\"") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("normalizedType == \"/std/collections/map\"") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("importPath == \"/std/collections/map\"") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("normalizedName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("explicitPath.rfind(\"/map/\", 0)") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("const std::string removedPath = \"/map/\" + helperName") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("canonicalMapHelperRootPathLocal(") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("canonicalMapHelperPathLocal(") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("resolvePublishedMapHelperMemberTokenLocal(") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("resolvePublishedMapHelperResolvedPathLocal(") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("isCanonicalMapHelperResolvedPathLocal(") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("resolvedBareMapHelper") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("hasKeyValueReceiver") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("resolveAnyKeyValueTarget") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("resolveAnyMapTarget") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("hasMapReceiver") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("isWrappedKeyValueReceiverCall") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("isWrappedMapReceiverCall") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("preferredRemovedMapHelperPath") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("mapNamespacedMethodCompatibilityPath") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("directMapHelperCompatibilityPath") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("preferredExperimentalMapHelperTarget") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find(
            "preferredCanonicalExperimentalMapHelperTarget") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("canonicalExperimentalMapHelperPath") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find(
            "canonicalizeExperimentalMapHelperResolvedPath") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find(
            "shouldLogicalCanonicalizeDefinedExperimentalMapHelperPath") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("canonicalKeyValueHelperRootPathLocal(") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("canonicalKeyValueHelperPathLocal(") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("resolvePublishedKeyValueHelperMemberTokenLocal(") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("resolvePublishedKeyValueHelperResolvedPathLocal(") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("isCanonicalKeyValueHelperResolvedPathLocal(") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("resolvedBareKeyValueHelper") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("spellsCurrentMapWrapperSurface") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find(
            "spellsCurrentKeyValueWrapperSurface") != std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("preferredRemovedKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find(
            "keyValueNamespacedMethodCompatibilityPath") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("directKeyValueHelperCompatibilityPath") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find(
            "preferredExperimentalKeyValueHelperTarget") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find(
            "preferredCanonicalExperimentalKeyValueHelperTarget") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("canonicalExperimentalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find(
            "canonicalizeExperimentalKeyValueHelperResolvedPath") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find(
            "shouldLogicalCanonicalizeDefinedExperimentalKeyValueHelperPath") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("isCanonicalMapCollectionTypeRootLocal(") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find(
            "isSpecializedExperimentalMapBackingPath") == std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find(
            "isSpecializedExperimentalKeyValueBackingPath") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("rootedMapCompatibilityHelperPath(") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("rootedKeyValueCompatibilityHelperPath(helperName)") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilitySource.find("metadataBackedKeyValueHelperRootAliasMethodName(explicitPath)") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("namespacePrefix == \"map\"") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("rawMethodName.rfind(\"map/\", 0)") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("use /std/collections/map/*") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find(
            "isPublishedMapBaseHelperName(") == std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find(
            "isPublishedBorrowedMapHelperName(") == std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find(
            "isMapHelperImportAliasNamespace(") == std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find(
            "resolveCanonicalCompatibilityMapHelperNameFromResolvedPath(") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find(
            "resolveExplicitPublishedMapHelperExprMemberName(") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find(
            "isDirectWrapperMapTarget") == std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("resolvesMapTarget") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find(
            "isPublishedKeyValueBaseHelperName(") != std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find(
            "isPublishedBorrowedKeyValueHelperName(") != std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find(
            "isKeyValueHelperImportAliasNamespace(") != std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find(
            "resolveCanonicalCompatibilityKeyValueHelperNameFromResolvedPath(") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find(
            "resolveExplicitPublishedKeyValueHelperExprMemberName(") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find(
            "isDirectWrapperKeyValueTarget") != std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("resolvesKeyValueTarget") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("resolveMapCompatibilityMemberToken(") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("resolveMapCompatibilityResolvedPath(") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("resolveMapCompatibilityUnrootedPath(") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("rootedMapCompatibilityHelperPath(") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("canonicalMapCompatibilityPrefixOrFallback(") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("unrootedCanonicalMapCompatibilityPrefixOrFallback(") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("isCanonicalMapCompatibilityNamespace(") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("legacyExperimentalMapCompatibilityPrefix(") ==
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("resolveKeyValueCompatibilityMemberToken(") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("resolveKeyValueCompatibilityResolvedPath(") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("resolveKeyValueCompatibilityUnrootedPath(") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("rootedKeyValueCompatibilityHelperPath(") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("canonicalKeyValueCompatibilityPrefixOrFallback(") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("unrootedCanonicalKeyValueCompatibilityPrefixOrFallback(") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("isCanonicalKeyValueCompatibilityNamespace(") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("legacyExperimentalKeyValueCompatibilityPrefix(") !=
        std::string::npos);
  CHECK(s.inferCollectionCompatibilityInternalSource.find("keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find("resolvedPath == \"/map/at_ref\"") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find("hasDefinitionPath(\"/map/\" +") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find("namespacePrefix == \"map\"") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find("expr.namespacePrefix == \"map\"") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find("StdlibSurfaceId::CollectionsMapHelpers") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find(
            "keyValueHelperSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find(
            "keyValueHelperSurfaceMetadataForDispatchSetup()") !=
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find(
            "dispatchSetupMapHelperSurfaceMetadata()") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find(
            "resolveRootKeyValueHelperAliasPath(path, helperName)") !=
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find("resolveRootMapHelperAliasPath(") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find("resolveDispatchSetupMapHelperPath(") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find("directMapHelperCompatibilityPath") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find(
            "resolveDispatchSetupKeyValueHelperPath(resolvedPath,") !=
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find("directKeyValueHelperCompatibilityPath") !=
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find(
            "isCanonicalKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find("isCanonicalMapAccessHelperName(") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find("canonicalKeyValueHelperNamespace()") !=
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find("canonicalMapHelperNamespace()") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find("isNamespacedKeyValueHelperCall") !=
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find("isNamespacedMapHelperCall") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find(
            "isStdNamespacedKeyValueAccessSpelling") != std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find("isStdNamespacedMapAccessSpelling") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find(
            "isIndexedArgsPackKeyValueReceiverTarget") != std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find(
            "isIndexedArgsPackMapReceiverTarget") == std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find(
            "hasStdNamespacedKeyValueAccessDefinition") != std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find("hasStdNamespacedMapAccessDefinition") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find(
            "shouldInferBuiltinBareKeyValueContainsCall") != std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find(
            "shouldInferBuiltinBareMapContainsCall") == std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find(
            "shouldInferBuiltinBareKeyValueTryAtCall") != std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find("shouldInferBuiltinBareMapTryAtCall") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find(
            "shouldInferBuiltinBareKeyValueAccessCall") != std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find("shouldInferBuiltinBareMapAccessCall") ==
        std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find(
            "shouldDeferNamespacedKeyValueAccessCall") != std::string::npos);
  CHECK(s.inferCollectionDispatchSetupSource.find("shouldDeferNamespacedMapAccessCall") ==
        std::string::npos);
  CHECK(s.inferCollectionsSource.find(
            "isExperimentalCollectionBackingTypeName(\"map\"") ==
        std::string::npos);
  CHECK(s.inferCollectionsSource.find(
            "isUnspecializedExperimentalKeyValueBackingTypeName(base)") !=
        std::string::npos);
  CHECK(s.inferCollectionsSource.find(
            "isQualifiedExperimentalKeyValueBackingTypeName(normalizedResolvedPath)") !=
        std::string::npos);
  CHECK(s.inferCollectionsSource.find("extractExperimentalMapFieldTypes =") ==
        std::string::npos);
  CHECK(s.inferCollectionsSource.find("extractExperimentalKeyValueFieldTypes =") !=
        std::string::npos);
  CHECK(s.inferCollectionsSource.find("extractAnyMapKeyValueTypes") ==
        std::string::npos);
  CHECK(s.inferCollectionsSource.find("extractAnyKeyValueTypes") !=
        std::string::npos);
  CHECK(s.inferCollectionsSource.find("resolveMapBinding") ==
        std::string::npos);
  CHECK(s.inferCollectionsSource.find("resolveKeyValueBinding") !=
        std::string::npos);
  CHECK(s.inferCollectionsSource.find("isDirectMapConstructorCall") ==
        std::string::npos);
  CHECK(s.inferCollectionsSource.find("isDirectKeyValueConstructorCall") !=
        std::string::npos);
  CHECK(s.privateCoreSource.find("extractInferExperimentalMapFieldTypes") ==
        std::string::npos);
  CHECK(s.privateCoreSource.find("resolveInferExperimentalMapTarget") ==
        std::string::npos);
  CHECK(s.privateCoreSource.find("resolveInferExperimentalMapValueTarget") ==
        std::string::npos);
  CHECK(s.privateCoreSource.find("extractInferExperimentalKeyValueFieldTypes") !=
        std::string::npos);
  CHECK(s.privateCoreSource.find("resolveInferExperimentalKeyValueTarget") !=
        std::string::npos);
  CHECK(s.privateCoreSource.find("resolveInferExperimentalKeyValueValueTarget") !=
        std::string::npos);
  CHECK(s.inferTargetResolutionSource.find("extractInferExperimentalMapFieldTypes") ==
        std::string::npos);
  CHECK(s.inferTargetResolutionSource.find("resolveInferExperimentalMapTarget") ==
        std::string::npos);
  CHECK(s.inferTargetResolutionSource.find("resolveInferExperimentalMapValueTarget") ==
        std::string::npos);
  CHECK(s.inferTargetResolutionSource.find("extractInferExperimentalKeyValueFieldTypes") !=
        std::string::npos);
  CHECK(s.inferTargetResolutionSource.find("resolveInferExperimentalKeyValueTarget") !=
        std::string::npos);
  CHECK(s.inferTargetResolutionSource.find("resolveInferExperimentalKeyValueValueTarget") !=
        std::string::npos);
  CHECK(s.inferCollectionCallResolutionSource.find("extractInferExperimentalMapFieldTypes") ==
        std::string::npos);
  CHECK(s.inferCollectionCallResolutionSource.find("extractInferExperimentalKeyValueFieldTypes") !=
        std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find("resolveMapBinding") ==
        std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find("extractExperimentalMapFieldTypes") ==
        std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find("isDirectMapConstructorCall") ==
        std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find("resolveKeyValueBinding") !=
        std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find("extractExperimentalKeyValueFieldTypes") !=
        std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find("isDirectKeyValueConstructorCall") !=
        std::string::npos);
  CHECK(s.inferCollectionReturnInferenceSource.find(
            "\"/std/collections/map/\" + builtinAccessName") ==
        std::string::npos);
  CHECK(s.inferCollectionReturnInferenceSource.find(
            "isExperimentalCollectionBackingTypeName(\"map\"") ==
        std::string::npos);
  CHECK(s.inferCollectionReturnInferenceSource.find("collection == \"map\"") ==
        std::string::npos);
  CHECK(s.inferCollectionReturnInferenceSource.find("currentTypeTextOut = \"map<\"") ==
        std::string::npos);
  CHECK(s.inferCollectionReturnInferenceSource.find(
            "isUnspecializedExperimentalKeyValueBackingTypeName(base)") !=
        std::string::npos);
  CHECK(s.inferCollectionReturnInferenceSource.find(
            "isQualifiedExperimentalKeyValueBackingTypeName(") !=
        std::string::npos);
  CHECK(s.inferCollectionReturnInferenceSource.find("mapCollectionAliasToken()") !=
        std::string::npos);
  CHECK(s.inferCollectionReturnInferenceSource.find("const std::string mapAlias") ==
        std::string::npos);
  CHECK(s.inferCollectionReturnInferenceSource.find("const std::string keyValueAlias") !=
        std::string::npos);
  CHECK(s.inferCollectionReturnInferenceSource.find(
            "sourceMethodMapResolvedCandidate") == std::string::npos);
  CHECK(s.inferCollectionReturnInferenceSource.find(
            "sourceMethodKeyValueResolvedCandidate") != std::string::npos);
  CHECK(s.inferCollectionReturnInferenceSource.find(
            "metadataBackedCanonicalKeyValueHelperPath(builtinAccessName)") !=
        std::string::npos);
  CHECK(s.inferCollectionReturnInferenceSource.find("const bool isKeyValueReceiver") !=
        std::string::npos);
  CHECK(s.inferCollectionReturnInferenceSource.find("const bool isMapReceiver") ==
        std::string::npos);
  CHECK(s.inferCollectionReturnInferenceSource.find(
            "const bool isExperimentalKeyValueReceiver") != std::string::npos);
  CHECK(s.inferCollectionReturnInferenceSource.find(
            "const bool isExperimentalMapReceiver") == std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find(
            "hasVisibleDefinitionPathForCurrentImports(\"/std/collections/map/map\")") ==
        std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find(
            "keyValueConstructorSurfaceMetadataLocal()") !=
        std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find(
            "std::string(mapConstructorMetadata->canonicalPath)") !=
        std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find("mapCollectionAliasToken()") !=
        std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find("isRootMapConstructorAliasPath(") !=
        std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find("inferMapConstructorArgTypes") ==
        std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find(
            "inferKeyValueConstructorArgTypes") != std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find("hasRootMapDefinitionFamily") ==
        std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find(
            "hasRootKeyValueDefinitionFamily") != std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find("skipRootMapAliasInference") ==
        std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find(
            "skipRootKeyValueAliasInference") != std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find(
            "hasVisibleCanonicalMapConstructor") == std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find(
            "hasVisibleCanonicalKeyValueConstructor") != std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find("allowRootMapConstructorAlias") ==
        std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find(
            "allowRootKeyValueConstructorAlias") != std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find("path == \"/map\"") ==
        std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find("path.rfind(\"/map__\", 0)") ==
        std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find("defMap_.find(\"/map\")") ==
        std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find(
            "resolveCallCollectionTemplateArgs(target, \"map\"") ==
        std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find("builtinCollectionName == \"map\"") ==
        std::string::npos);
  CHECK(s.inferCollectionBufferAndMapResolversSource.find("collectionTypePath == \"/map\"") ==
        std::string::npos);
  CHECK(s.inferDefinitionSource.find("resolvedPath == \"/map/at\"") ==
        std::string::npos);
  CHECK(s.inferDefinitionSource.find("resolveCalleePath(candidate) == \"/std/collections/map/at_ref\"") ==
        std::string::npos);
  CHECK(s.inferDefinitionSource.find("resolvedPath == \"/std/collections/map/at\"") ==
        std::string::npos);
  CHECK(s.inferDefinitionSource.find(
            "isInferDefinitionCanonicalMapAccessHelperPath(resolvedCandidatePath)") ==
        std::string::npos);
  CHECK(s.inferDefinitionSource.find(
            "isInferDefinitionCanonicalKeyValueAccessHelperPath(resolvedCandidatePath)") !=
        std::string::npos);
  CHECK(s.inferDefinitionSource.find(
            "metadataBackedKeyValueHelperMethodName(resolvedCandidatePath)") !=
        std::string::npos);
  CHECK(s.inferDefinitionSource.find("containsDeferredMapAliasInference") ==
        std::string::npos);
  CHECK(s.inferDefinitionSource.find("containsDeferredKeyValueAliasInference") !=
        std::string::npos);
  CHECK(s.inferDefinitionSource.find("shouldDeferExplicitMapAliasDiagnostic") ==
        std::string::npos);
  CHECK(s.inferDefinitionSource.find(
            "shouldDeferExplicitKeyValueAliasDiagnostic") != std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find("path == \"/map/at\"") ==
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find("path == \"/map/at_ref\"") ==
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find("path == \"/map/at_unsafe\"") ==
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find("path == \"/map/at_unsafe_ref\"") ==
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "lateFallbackMapHelperSurfaceMetadata(") ==
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "resolveLateFallbackMapHelperName(") ==
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "lateFallbackCanonicalMapHelperPath(") ==
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "resolveLateFallbackCanonicalMapHelperName(") ==
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "isCanonicalMapContainsHelperPath(") ==
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "isCanonicalMapTryAtHelperPath(") ==
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "isCanonicalMapAccessHelperPath(") ==
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "isLateFallbackMapAccessHelperName(") ==
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "isCanonicalMapAccessHelperName(") ==
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "isMapImportAliasAccessHelperPath(") ==
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find("rewrittenMapHelperCall") ==
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find("rewrittenMapAccessCall") ==
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "lateFallbackKeyValueHelperSurfaceMetadata(") !=
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "resolveLateFallbackKeyValueHelperName(") !=
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "lateFallbackCanonicalKeyValueHelperPath(") !=
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "resolveLateFallbackCanonicalKeyValueHelperName(") !=
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "isCanonicalKeyValueContainsHelperPath(") !=
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "isCanonicalKeyValueTryAtHelperPath(") !=
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "isCanonicalKeyValueAccessHelperPath(") !=
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "isLateFallbackKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "isCanonicalKeyValueAccessHelperName(") !=
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "isCollectionPairImportAliasAccessHelperPath(methodResolved)") !=
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find("rewrittenKeyValueHelperCall") !=
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find("hasBareKeyValueOperands") !=
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find("hasBareMapOperands") ==
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find("keyValueReceiverIndex") !=
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find("mapReceiverIndex") ==
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find("const auto &resolveMapTarget") ==
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "const auto &resolveExperimentalMapTarget") == std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find("const auto &resolveKeyValueTarget") !=
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "const auto &resolveExperimentalKeyValueTarget") !=
        std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "isIndexedArgsPackKeyValueReceiverTarget") != std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "isIndexedArgsPackMapReceiverTarget") == std::string::npos);
  CHECK(s.inferLateFallbackBuiltinsSource.find(
            "metadata->importAliasSpellings") !=
        std::string::npos);
  CHECK(s.exprLateFallbackBuiltinsSource.find("rewrittenMapAccessCall") ==
        std::string::npos);
  CHECK(s.exprLateFallbackBuiltinsSource.find("rewrittenKeyValueAccessCall") !=
        std::string::npos);
  CHECK(s.exprLateCallCompatibilitySource.find("keyValueKeyType") !=
        std::string::npos);
  CHECK(s.exprLateCallCompatibilitySource.find("keyValueValueType") !=
        std::string::npos);
  CHECK(s.exprLateCallCompatibilitySource.find("resolvesKeyValue") !=
        std::string::npos);
  CHECK(s.exprLateCallCompatibilitySource.find("resolvesKeyValueAfterValidation") !=
        std::string::npos);
  CHECK(s.exprLateCallCompatibilitySource.find("resolvesMap") ==
        std::string::npos);
  CHECK(s.exprLateCallCompatibilitySource.find("resolvesMapAfterValidation") ==
        std::string::npos);
  CHECK(s.exprLateCallCompatibilitySource.find("std::string mapKeyType") ==
        std::string::npos);
  CHECK(s.exprLateCallCompatibilitySource.find("std::string mapValueType") ==
        std::string::npos);
  CHECK(s.lateMapAccessBuiltinsSource.empty());
  CHECK(s.exprSource.find("prepareExprLateMapAccessBuiltinContext") ==
        std::string::npos);
  CHECK(s.exprSource.find("validateExprLateMapAccessBuiltins") == std::string::npos);
  CHECK(s.privateExprValidationSource.find("ExprLateMapAccessBuiltinContext") ==
        std::string::npos);
  CHECK(s.builtinContextSetupSource.find("prepareExprLateMapAccessBuiltinContext") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find(
            "findStdlibSurfaceMetadataByBridgeKey(\"collections.map_helpers\")") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("canonicalKeyValueHelperPathLocal(") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find(
            "resolveCanonicalKeyValueHelperNameFromSpelling(") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find(
            "isCanonicalKeyValueHelperResolvedPath(") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find(
            "isCanonicalKeyValueAccessHelperPath(") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find(
            "getCanonicalCollectionAccessBuiltinName(") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("canonicalKeyValueAccessDiagnostic") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("isKeyValueLikeReceiver(") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("resolveKeyValueKeyType(") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("failCollectionAccessKeyValueKeyMismatch") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find(
            "\"tryAt requires map key type \" + keyValueKeyType") ==
        std::string::npos);
  CHECK(s.argumentValidationSource.find("validateCanonicalKeyValueAccessKeyArgument") !=
        std::string::npos);
  CHECK(s.exprSource.find("validateResolvedCanonicalKeyValueAccessKey") !=
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("canonicalMapHelperPathLocal(") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("isCanonicalMapHelperResolvedPath(") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("receiverIsExperimentalMap") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("isMapLikeReceiver(") ==
        std::string::npos);
  CHECK(s.collectionAccessValidationSource.find("resolveMapKeyTypeWithInference") ==
        std::string::npos);
  CHECK(s.privateExprValidationSource.find(
            "shouldBuiltinValidateBareKeyValueContainsCall") !=
        std::string::npos);
  CHECK(s.privateExprValidationSource.find(
            "shouldBuiltinValidateBareMapContainsCall") == std::string::npos);
  CHECK(s.privateExprValidationSource.find(
            "shouldBuiltinValidateBareKeyValueTryAtCall") == std::string::npos);
  CHECK(s.privateExprValidationSource.find(
            "shouldBuiltinValidateBareMapTryAtCall") == std::string::npos);
  CHECK(s.privateExprValidationSource.find(
            "shouldBuiltinValidateBareKeyValueAccessCall") != std::string::npos);
  CHECK(s.privateExprValidationSource.find(
            "shouldBuiltinValidateBareMapAccessCall") == std::string::npos);
}

TEST_SUITE_END();
