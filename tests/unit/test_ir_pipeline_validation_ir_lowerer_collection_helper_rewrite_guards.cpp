#include <filesystem>
#include <fstream>
#include <iterator>

#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer collection helper rewrite guards explicit map defs") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path collectionHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprCollectionHelpers.h";

  REQUIRE(std::filesystem::exists(collectionHelpersPath));
  const std::string source = readText(collectionHelpersPath);

  CHECK(source.find(
            "auto rewriteExplicitBuiltinKeyValueHelperExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {") !=
        std::string::npos);
  CHECK(source.find("auto resolvePublishedLateCollectionMemberName =") !=
        std::string::npos);
  CHECK(source.find("resolvePublishedSemanticStdlibSurfaceMemberName(") !=
        std::string::npos);
  CHECK(source.find("resolvePublishedStdlibSurfaceExprMemberName(") !=
        std::string::npos);
  CHECK(source.find("auto isRootedAliasSamePathCountLikeCall = [&](const Expr &candidate) {") !=
        std::string::npos);
  CHECK(source.find("if (helperName != \"insert\" && isRootedAliasSamePathCountLikeCall(callExpr)) {") !=
        std::string::npos);
  CHECK(source.find(
            "if (callExpr.namespacePrefix.empty() &&\n"
            "              callExpr.name == helperName) {") !=
        std::string::npos);
  CHECK(source.find("keyValueHelperSurfaceMetadataForLowerEmitExpr()") !=
        std::string::npos);
  CHECK(source.find(
            "ir_lowerer::isCanonicalPublishedStdlibSurfaceHelperPath(\n"
            "                  directHelperPath,\n"
            "                  metadata->id)") !=
        std::string::npos);
  CHECK(source.find("directHelperPath.rfind(\"/std/collections/map/\", 0)") ==
        std::string::npos);
  CHECK(source.find("directHelperPath.rfind(\"/std/collections/experimental_map/\", 0)") ==
        std::string::npos);
  CHECK(source.find("if (resolveDefinitionCall(callExpr) != nullptr)") != std::string::npos);
  CHECK(source.find("rewrittenExpr.name = helperName;") != std::string::npos);
  CHECK(source.find("auto isEntryArgsPackKeyValueConstructorExpr =") !=
        std::string::npos);
  CHECK(source.find(
            "if (!hasSpreadArg) {\n"
            "            return false;\n"
            "          }\n"
            "          return true;") != std::string::npos);
  CHECK(source.find("native backend does not support variadic entry map constructors") !=
        std::string::npos);
}

TEST_CASE("ir lowerer materialized collection receivers use published helper queries") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path collectionHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprCollectionHelpers.h";

  REQUIRE(std::filesystem::exists(collectionHelpersPath));
  const std::string source = readText(collectionHelpersPath);

  CHECK(source.find("auto resolveMaterializedCollectionHelperName =") !=
        std::string::npos);
  CHECK(source.find("callResolutionAdapters.semanticProgram") !=
        std::string::npos);
  CHECK(source.find("resolvePublishedStdlibSurfaceExprMemberName(") !=
        std::string::npos);
  CHECK(source.find("findStdlibSurfaceMetadataByBridgeKey(\"collections.vector_helpers\")") !=
        std::string::npos);
  CHECK(source.find("primec::StdlibSurfaceId::CollectionsVectorHelperSurface") ==
        std::string::npos);
  CHECK(source.find("auto resolvePublishedLateVectorMemberName =") !=
        std::string::npos);
  CHECK(source.find("resolvePublishedLateKeyValueMemberName(candidate,") !=
        std::string::npos);
  CHECK(source.find("resolvePublishedLateMapMemberName(") ==
        std::string::npos);
  CHECK(source.find("if (!resolveMaterializedCollectionHelperName(callExpr, helperName)) {") !=
        std::string::npos);
  CHECK(source.find("helperName = resolveCollectionExprDirectPath(callExpr);") !=
        std::string::npos);
  CHECK(source.find("resolveMaterializedCollectionHelperName(callExpr, helperName)") !=
        std::string::npos);
  CHECK(source.find("helperName = callExpr.name;") !=
        std::string::npos);
  CHECK(source.find(
            "primec::StdlibSurfaceId::CollectionsVectorHelperSurface,\n"
            "                    helperName)) {\n"
            "              helperName = callExpr.name;") ==
        std::string::npos);
  CHECK(source.find(
            "if (!resolveMaterializedCollectionHelperName(callExpr, helperName)) {\n"
            "              helperName = callExpr.name;") ==
        std::string::npos);
}

TEST_CASE("ir lowerer late collection constructor guards use published constructor queries") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path collectionHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprCollectionHelpers.h";

  REQUIRE(std::filesystem::exists(collectionHelpersPath));
  const std::string source = readText(collectionHelpersPath);

  CHECK(source.find("auto resolvePublishedLateCollectionConstructorName =") !=
        std::string::npos);
  CHECK(source.find("keyValueConstructorSurfaceMetadataForLowerEmitExpr()") !=
        std::string::npos);
  CHECK(source.find("primec::StdlibSurfaceId::CollectionsVectorConstructors") !=
        std::string::npos);
  CHECK(source.find("resolvePublishedStdlibSurfaceConstructorExprMemberName(") !=
        std::string::npos);
  CHECK(source.find("resolvePublishedStdlibSurfaceConstructorMemberName(") !=
        std::string::npos);
  CHECK(source.find("resolvePublishedLateCollectionConstructorName(") !=
        std::string::npos);
}

TEST_CASE("ir lowerer temp receiver method access skips builtin array lowering") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path lowerStatementsExprPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerStatementsExpr.h";

  REQUIRE(std::filesystem::exists(lowerStatementsExprPath));
  const std::string source = readText(lowerStatementsExprPath);

  CHECK(source.find("const bool isMethodCallTempReceiver =") !=
        std::string::npos);
  CHECK(source.find("expr.args.front().kind == Expr::Kind::Call") !=
        std::string::npos);
  CHECK(source.find("(accessName == \"at\" || accessName == \"at_unsafe\")") !=
        std::string::npos);
  CHECK(source.find("bool tempReceiverSupportsBuiltinAccess = false;") !=
        std::string::npos);
  CHECK(source.find("resolveHelperReturnedArrayVectorAccessTargetInfo(") !=
        std::string::npos);
  CHECK(source.find("if (isMethodCallTempReceiver && !tempReceiverSupportsBuiltinAccess)") !=
        std::string::npos);
  CHECK(source.find(
            "Let normal helper lowering handle method calls on constructor- or\n"
            "            // helper-backed temporaries instead of forcing builtin raw access.") !=
        std::string::npos);
  CHECK(source.find("if (expr.isMethodCall) {") != std::string::npos);
  const size_t methodCalleeDecl =
      source.find("const Definition *methodCallee =");
  REQUIRE(methodCalleeDecl != std::string::npos);
  const size_t methodCalleeResolve =
      source.find("resolveMethodCallDefinition(expr, localsIn);", methodCalleeDecl);
  REQUIRE(methodCalleeResolve != std::string::npos);
  const size_t methodCalleeDirectFallback =
      source.find("methodCallee = findDirectHelperDefinition(resolveExprPath(expr));",
                  methodCalleeResolve);
  CHECK(methodCalleeDirectFallback !=
        std::string::npos);
  CHECK(methodCalleeResolve < methodCalleeDirectFallback);
  CHECK(source.find("emitInlineDefinitionCall(expr, *methodCallee, localsIn, true)") !=
        std::string::npos);
}

TEST_CASE("ir lowerer direct soa wrapper dispatch uses canonical wrapper probes only") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path lowerStatementsExprPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerStatementsExpr.h";

  REQUIRE(std::filesystem::exists(lowerStatementsExprPath));
  const std::string source = readText(lowerStatementsExprPath);

  const size_t samePathHelper =
      source.find("auto isSamePathSoaHelperPath = [&](const std::string &path) {");
  const size_t wrapperHelper =
      source.find("auto isSoaWrapperHelperFamilyPath = [&](const std::string &path) {");
  const size_t wrapperEnd =
      source.find("auto findDirectSoaWrapperDefinition", wrapperHelper);

  REQUIRE(samePathHelper != std::string::npos);
  REQUIRE(wrapperHelper != std::string::npos);
  REQUIRE(wrapperEnd != std::string::npos);
  CHECK(samePathHelper < wrapperHelper);

  const std::string wrapperHelperSource =
      source.substr(wrapperHelper, wrapperEnd - wrapperHelper);
  CHECK(wrapperHelperSource.find("\"/std/collections/\" \"soa\" \"_vector/\"") !=
        std::string::npos);
  CHECK(wrapperHelperSource.find("\"/std/collections/experimental\" \"_soa\" "
                                 "\"_vector/soa\" \"Vector\"") !=
        std::string::npos);
  CHECK(wrapperHelperSource.find("\"/std/collections/experimental\" \"_soa\" "
                                 "\"_vector_conversions/soa\" \"Vector\"") !=
        std::string::npos);
  CHECK(wrapperHelperSource.find("\"/soa_vector/") == std::string::npos);
  CHECK(wrapperHelperSource.find("\"/to_aos\"") == std::string::npos);
  CHECK(wrapperHelperSource.find("\"/to_aos_ref\"") == std::string::npos);

  CHECK(source.find("directCallee == nullptr &&\n"
                    "              isSamePathSoaHelperPath(rawPath)") ==
        std::string::npos);
  CHECK(source.find("const bool isVisibleSamePathSoaHelper =\n"
                    "                isSamePathSoaHelperPath(rawPath) &&\n"
                    "                isDirectHelperDefinitionFamily(expr, *directCallee);") !=
        std::string::npos);
  CHECK(source.find("const bool isResolvedSoaWrapperHelper =\n"
                    "                isSoaWrapperHelperFamilyPath(directCallee->fullPath);") !=
        std::string::npos);
  CHECK(source.find("if (isResolvedSoaWrapperHelper || isVisibleSamePathSoaHelper)") !=
        std::string::npos);
}

TEST_CASE("ir lowerer rewrites experimental vector constructor aliases before direct resolution") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path lowerStatementsExprPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerStatementsExpr.h";

  REQUIRE(std::filesystem::exists(lowerStatementsExprPath));
  const std::string source = readText(lowerStatementsExprPath);

  const size_t aliasRewrite = source.find(
      "const bool isExperimentalVectorConstructorAlias =\n"
      "              getExperimentalVectorConstructorElementTypeAlias(");
  const size_t resolvedAliasRewrite = source.find(
      "getExperimentalVectorConstructorElementTypeAliasFromPath(\n"
      "                  resolveExprPath(expr), experimentalVectorElementType)");
  const size_t directResolution =
      source.find("const Definition *directCallee = resolveDefinitionCall(expr);");
  const size_t structFallback =
      source.find("directCallee = findDirectStructDefinition(expr);");

  REQUIRE(aliasRewrite != std::string::npos);
  REQUIRE(resolvedAliasRewrite != std::string::npos);
  REQUIRE(directResolution != std::string::npos);
  REQUIRE(structFallback != std::string::npos);
  CHECK(aliasRewrite < directResolution);
  CHECK(resolvedAliasRewrite < directResolution);
  CHECK(aliasRewrite < structFallback);
  CHECK(resolvedAliasRewrite < structFallback);
  CHECK(source.find(
            "rewrittenVectorCtor.name =\n"
            "                experimentalCollectionMemberPath(\"vector\", \"vector\");") !=
        std::string::npos);
  CHECK(source.find("rewrittenVectorCtor.templateArgs = {experimentalVectorElementType};") !=
        std::string::npos);
}

TEST_CASE("ir lowerer collection helper resolves vector aliases before direct definitions") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path collectionHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprCollectionHelpers.h";

  REQUIRE(std::filesystem::exists(collectionHelpersPath));
  const std::string source = readText(collectionHelpersPath);

  const size_t aliasRewrite = source.find(
      "if (getExperimentalVectorConstructorElementTypeAlias(\n"
      "                  candidate, experimentalVectorElementType)) {");
  const size_t directResolution =
      source.find("const Definition *callee = resolveDefinitionCall(candidate);");
  const size_t receiverResolution = source.find(
      "resolveCollectionExprDirectDefinition(callExpr.args.front())");

  REQUIRE(aliasRewrite != std::string::npos);
  REQUIRE(directResolution != std::string::npos);
  REQUIRE(receiverResolution != std::string::npos);
  CHECK(aliasRewrite < directResolution);
  CHECK(source.find(
            "rewrittenVectorCtor.name =\n"
            "                experimentalCollectionMemberPath(\"vector\", \"vector\");") !=
        std::string::npos);
  CHECK(source.find("rewrittenVectorCtor.templateArgs = {experimentalVectorElementType};") !=
        std::string::npos);
}

TEST_CASE("ir lowerer prefers explicit experimental vector helper before struct constructor defs") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path statementsExprPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerStatementsExpr.h";
  const std::filesystem::path collectionHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprCollectionHelpers.h";

  REQUIRE(std::filesystem::exists(statementsExprPath));
  REQUIRE(std::filesystem::exists(collectionHelpersPath));
  const std::string statementsSource = readText(statementsExprPath);
  const std::string collectionSource = readText(collectionHelpersPath);

  const size_t statementsVectorHelperCheck =
      statementsSource.find("isExplicitExperimentalVectorConstructorHelper(rawPath)");
  const size_t statementsDirectResolution =
      statementsSource.find("const Definition *callee = resolveDefinitionCall(targetExpr);");
  const size_t collectionVectorHelperCheck =
      collectionSource.find("isExplicitExperimentalVectorConstructorHelper(rawPath)");
  const size_t collectionDirectResolution =
      collectionSource.find("const Definition *callee = resolveDefinitionCall(candidate);");

  REQUIRE(statementsVectorHelperCheck != std::string::npos);
  REQUIRE(statementsDirectResolution != std::string::npos);
  REQUIRE(collectionVectorHelperCheck != std::string::npos);
  REQUIRE(collectionDirectResolution != std::string::npos);
  CHECK(statementsVectorHelperCheck < statementsDirectResolution);
  CHECK(collectionVectorHelperCheck < collectionDirectResolution);
  CHECK(statementsSource.find(
            "experimentalCollectionMemberPath(\"vector\", \"vector\")") !=
        std::string::npos);
  CHECK(collectionSource.find(
            "const std::string slashPath =\n"
            "                    experimentalCollectionMemberPath(\"vector\", \"vector\");") !=
        std::string::npos);
}

TEST_CASE("ir lowerer bare vector helper rewrites prefer semantic receiver facts") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path collectionHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprCollectionHelpers.h";

  REQUIRE(std::filesystem::exists(collectionHelpersPath));
  const std::string source = readText(collectionHelpersPath);

  CHECK(source.find("auto tryPopulateLateArrayVectorTargetFromSemanticFacts =") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductCollectionSpecialization(\n"
                    "                      callResolutionAdapters.semanticProductTargets.semanticIndex,") !=
        std::string::npos);
  CHECK(source.find("collectionFact->elementTypeTextId") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductQueryFact(\n"
                    "                          callResolutionAdapters.semanticProgram,") !=
        std::string::npos);
  CHECK(source.find("queryFact->bindingTypeTextId") !=
        std::string::npos);
  CHECK(source.find("queryFact->receiverBindingTypeTextId") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductBindingFact(\n"
                    "                          semanticIndex, targetExpr)") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductLocalAutoFact(\n"
                    "                          callResolutionAdapters.semanticProgram,") !=
        std::string::npos);

  const size_t callbackSemanticFactUse =
      source.find("if (tryPopulateLateArrayVectorTargetFromSemanticFacts(\n"
                  "                        targetCallExpr, targetInfoOut, nullptr)) {");
  REQUIRE(callbackSemanticFactUse != std::string::npos);
  const size_t callbackDirectReturnInference =
      source.find("const Definition *callee =\n"
                  "                    resolveCollectionExprDirectDefinition(targetCallExpr);",
                  callbackSemanticFactUse);
  REQUIRE(callbackDirectReturnInference != std::string::npos);
  CHECK(callbackSemanticFactUse < callbackDirectReturnInference);

  const size_t methodSemanticFactUse =
      source.find("tryPopulateLateArrayVectorTargetFromSemanticFacts(\n"
                  "                    receiverCallExpr, semanticTargetInfo, &receiverCollectionArgs)");
  REQUIRE(methodSemanticFactUse != std::string::npos);
  const size_t methodDirectReturnInference =
      source.find("resolveCollectionExprDirectDefinition(receiverCallExpr)",
                  methodSemanticFactUse);
  REQUIRE(methodDirectReturnInference != std::string::npos);
  CHECK(methodSemanticFactUse < methodDirectReturnInference);
}

TEST_CASE("ir lowerer materialized collection receivers prefer semantic target facts") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path collectionHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprCollectionHelpers.h";

  REQUIRE(std::filesystem::exists(collectionHelpersPath));
  const std::string source = readText(collectionHelpersPath);

  const size_t rewritePos =
      source.find("auto emitMaterializedCollectionReceiverExpr =");
  REQUIRE(rewritePos != std::string::npos);
  const size_t semanticIndexPos =
      source.find("lateCollectionSemanticIndex", rewritePos);
  const size_t semanticMapResolverPos = source.find(
      "ir_lowerer::resolveCollectionPairTypeInfo(\n"
      "                    *receiverExpr,\n"
      "                    localsIn,\n"
      "                    inferCallKeyValueTargetInfo,\n"
      "                    lateCollectionSemanticProgram,\n"
      "                    lateCollectionSemanticIndex)",
      rewritePos);
  const size_t semanticArrayVectorResolverPos = source.find(
      "ir_lowerer::resolveArrayVectorAccessTargetInfo(\n"
      "                    *receiverExpr,\n"
      "                    localsIn,\n"
      "                    {},\n"
      "                    lateCollectionSemanticProgram,\n"
      "                    lateCollectionSemanticIndex)",
      rewritePos);
  const size_t wrappedMapGatePos = source.find(
      "if (keyValueTargetInfo.isKeyValueTarget && arrayVectorTargetInfo.isWrappedKeyValueTarget)",
      rewritePos);
  const size_t emitReceiverValuePos =
      source.find("auto emitCollectionReceiverValue =", rewritePos);
  REQUIRE(emitReceiverValuePos != std::string::npos);
  const size_t nestedArrayVectorResolverPos = source.find(
      "ir_lowerer::resolveArrayVectorAccessTargetInfo(\n"
      "                      valueExpr.args.front(),\n"
      "                      localsIn,\n"
      "                      {},\n"
      "                      lateCollectionSemanticProgram,\n"
      "                      lateCollectionSemanticIndex)",
      emitReceiverValuePos);
  const size_t argsPackGatePos =
      source.find("const bool isStructArgsPackAccess =", emitReceiverValuePos);

  REQUIRE(semanticIndexPos != std::string::npos);
  REQUIRE(semanticMapResolverPos != std::string::npos);
  REQUIRE(semanticArrayVectorResolverPos != std::string::npos);
  REQUIRE(wrappedMapGatePos != std::string::npos);
  REQUIRE(nestedArrayVectorResolverPos != std::string::npos);
  REQUIRE(argsPackGatePos != std::string::npos);
  CHECK(semanticIndexPos < semanticMapResolverPos);
  CHECK(semanticMapResolverPos < wrappedMapGatePos);
  CHECK(semanticArrayVectorResolverPos < wrappedMapGatePos);
  CHECK(nestedArrayVectorResolverPos < argsPackGatePos);
}

TEST_CASE("ir lowerer map constructor rewrite checks constructor surface before resolving defs") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path collectionHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprCollectionHelpers.h";

  REQUIRE(std::filesystem::exists(collectionHelpersPath));
  const std::string source = readText(collectionHelpersPath);

  const size_t constructorSurfaceCheck =
      source.find("resolvePublishedLateKeyValueConstructorName(callExpr,");
  const size_t resolveDefinitionCallPos =
      source.find("resolveDefinitionCall(callExpr)", constructorSurfaceCheck);

  REQUIRE(constructorSurfaceCheck != std::string::npos);
  REQUIRE(resolveDefinitionCallPos != std::string::npos);
  CHECK(constructorSurfaceCheck < resolveDefinitionCallPos);
}

TEST_CASE("ir lowerer constructor metadata helpers retire duplicated constructor tables") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path setupHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererSetupTypeCollectionHelpers.cpp";
  const std::filesystem::path accessTargetResolutionPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererAccessTargetResolution.cpp";
  const std::filesystem::path inlineParamHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererInlineParamHelpers.cpp";
  const std::filesystem::path declaredInferencePath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererSetupTypeDeclaredCollectionInference.cpp";

  REQUIRE(std::filesystem::exists(setupHelpersPath));
  REQUIRE(std::filesystem::exists(accessTargetResolutionPath));
  REQUIRE(std::filesystem::exists(inlineParamHelpersPath));
  REQUIRE(std::filesystem::exists(declaredInferencePath));

  const std::string setupHelpersSource = readText(setupHelpersPath);
  const std::string accessTargetResolutionSource =
      readText(accessTargetResolutionPath);
  const std::string inlineParamHelpersSource =
      readText(inlineParamHelpersPath);
  const std::string declaredInferenceSource =
      readText(declaredInferencePath);

  CHECK(setupHelpersSource.find(
            "bool resolvePublishedStdlibSurfaceConstructorMemberName(") !=
        std::string::npos);
  CHECK(setupHelpersSource.find(
            "bool resolvePublishedStdlibSurfaceConstructorExprMemberName(") !=
        std::string::npos);
  CHECK(setupHelpersSource.find(
            "bool isResolvedCanonicalPublishedStdlibSurfaceConstructorPath(") !=
        std::string::npos);
  CHECK(setupHelpersSource.find(
            "std::string inferPublishedExperimentalMapStructPathFromConstructorPath(") !=
        std::string::npos);

  CHECK(accessTargetResolutionSource.find(
            "resolveKeyValueConstructorPathMemberName(normalizedName, constructorName)") !=
        std::string::npos);
  CHECK(accessTargetResolutionSource.find(
            "isPublishedKeyValueConstructorExpr(target)") !=
        std::string::npos);
  CHECK(accessTargetResolutionSource.find("matchesPath(\"std/collections/mapSingle\")") ==
        std::string::npos);

  CHECK(inlineParamHelpersSource.find(
            "isResolvedCanonicalPublishedStdlibSurfaceConstructorPath(") !=
        std::string::npos);
  CHECK(inlineParamHelpersSource.find(
            "isPublishedStdlibSurfaceConstructorExpr(") !=
        std::string::npos);
  CHECK(inlineParamHelpersSource.find("isSimpleCallName(callExpr, \"mapSingle\")") ==
        std::string::npos);

  CHECK(declaredInferenceSource.find(
            "isPublishedStdlibSurfaceConstructorExpr(") !=
        std::string::npos);
  CHECK(declaredInferenceSource.find("isSimpleCallName(candidate, \"mapSingle\")") ==
        std::string::npos);
  CHECK(declaredInferenceSource.find(
            "std::function<bool(const std::string &)> inferCollectionFromNamedValue;") !=
        std::string::npos);
  CHECK(declaredInferenceSource.find(
            "if (candidate.kind == Expr::Kind::Name &&\n"
            "        inferCollectionFromNamedValue(candidate.name)) {") !=
        std::string::npos);
  CHECK(declaredInferenceSource.find(
            "if (isDirectVectorConstructor() && !candidate.args.empty()) {") !=
        std::string::npos);
  CHECK(declaredInferenceSource.find(
            "return inferCollectionFromType(trimTemplateTypeText(declaredType),") !=
        std::string::npos);
}

TEST_CASE("ir lowerer tail dispatch rewrite guards explicit map defs") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path tailDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTailDispatch.h";

  REQUIRE(std::filesystem::exists(tailDispatchPath));
  const std::string source = readText(tailDispatchPath);

  CHECK(source.find(
            "auto rewriteExplicitKeyValueHelperBuiltinExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {") !=
        std::string::npos);
  CHECK(source.find("auto hasPublishedSemanticKeyValueSurface = [&](const Expr &callExpr) {") !=
        std::string::npos);
  CHECK(source.find("auto resolvePublishedTailDispatchKeyValueHelperName =") !=
        std::string::npos);
  CHECK(source.find("resolvePublishedSemanticStdlibSurfaceMemberName(") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductDirectCallStdlibSurfaceId(\n"
                    "                         semanticProgram, callExpr)") !=
        std::string::npos);
  CHECK(source.find("if (resolvePublishedTailDispatchKeyValueHelperName(callExpr, helperNameOut)) {") !=
        std::string::npos);
  CHECK(source.find("if (callExpr.namespacePrefix.empty() &&\n"
                    "                    callExpr.name.find('/') == std::string::npos &&\n"
                    "                    (callExpr.name == \"count\" || callExpr.name == \"contains\" ||") !=
        std::string::npos);
  CHECK(source.find("if (candidate.isMethodCall) {\n"
                    "              std::string helperName;\n"
                    "              return resolveBuiltinKeyValueHelperName(candidate, true, helperName) &&") !=
        std::string::npos);
  CHECK(source.find("if (!candidate.args.empty() &&\n"
                    "                candidate.namespacePrefix.empty() &&\n"
                    "                candidate.name.find('/') == std::string::npos &&\n"
                    "                (candidate.name == \"count\" || candidate.name == \"contains\" ||") !=
        std::string::npos);
  CHECK(source.find(
            "std::string helperName =\n                resolveTailDispatchDirectHelperPath(candidate);") !=
        std::string::npos);
  CHECK(source.find("if (!resolveBuiltinKeyValueHelperName(callExpr, true, helperName) ||\n"
                    "                (helperName != \"insert\" && helperName != \"insert_ref\")) {") !=
        std::string::npos);
  CHECK(source.find(
            "if ((!ir_lowerer::resolveKeyValueHelperAliasName(callExpr, helperName) ||\n"
            "                 (helperName != \"insert\" && helperName != \"insert_ref\")) &&") !=
        std::string::npos);
  CHECK(source.find("if (matchesPublishedMapInsertPath(callExpr)) {\n"
                    "            return false;\n"
                    "          }") ==
        std::string::npos);
  CHECK(source.find("if ((helperName == \"count\" || helperName == \"contains\" ||") !=
        std::string::npos);
  CHECK(source.find("helperName == \"at_unsafe\" || helperName == \"insert\" ||\n"
                    "               helperName == \"insert_ref\") &&") !=
        std::string::npos);
  CHECK(source.find("isTailDispatchKeyValueImportAliasHelperPath(rawPath, helperName)") !=
        std::string::npos);
  CHECK(source.find("rawPath.rfind(\"/\" + std::string(\"map\") + \"/\", 0) == 0") ==
        std::string::npos);
  CHECK(source.find("resolveDefinitionCall(callExpr) != nullptr") != std::string::npos);
  CHECK(source.find("rewrittenExpr.name = helperName;") != std::string::npos);
  CHECK(source.find("std::string helperName = candidate.name;") ==
        std::string::npos);
  CHECK(source.find("std::string normalizedMethodName = callExpr.name;") ==
        std::string::npos);
  CHECK(source.find("const bool isMethodInsertStem =") ==
        std::string::npos);
  CHECK(source.find("if (callExpr.name == \"count\" || callExpr.name == \"contains\" ||") ==
        std::string::npos);
  CHECK(source.find("if (candidate.isMethodCall) {\n"
                    "              return candidate.name == \"count\" || candidate.name == \"contains\" ||") ==
        std::string::npos);
  CHECK(source.find("if (!candidate.args.empty() &&\n"
                    "                (candidate.name == \"count\" || candidate.name == \"contains\" ||") ==
        std::string::npos);
}

TEST_CASE("ir lowerer tail dispatch semantic query targets resolve interned type ids") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path tailDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTailDispatch.h";

  REQUIRE(std::filesystem::exists(tailDispatchPath));
  const std::string source = readText(tailDispatchPath);

  CHECK(source.find("auto resolveSemanticQueryFactTypeText =") !=
        std::string::npos);
  CHECK(source.find("SymbolId typeTextId") != std::string::npos);
  CHECK(source.find("semanticProgramResolveCallTargetString(*semanticProgram, typeTextId)") !=
        std::string::npos);
  CHECK(source.find("resolveFactTypeText(queryFact.bindingTypeText, queryFact.bindingTypeTextId)") !=
        std::string::npos);
  CHECK(source.find("resolveFactTypeText(queryFact.queryTypeText, queryFact.queryTypeTextId)") !=
        std::string::npos);
  CHECK(source.find("const std::string bindingType = resolveSemanticQueryFactTypeText(*queryFact);") !=
        std::string::npos);
  CHECK(source.find("std::string bindingType = resolveSemanticQueryFactTypeText(*queryFact);") !=
        std::string::npos);
  CHECK(source.find("std::string bindingType = ir_lowerer::trimTemplateTypeText(queryFact->bindingTypeText);") ==
        std::string::npos);
  CHECK(source.find("bindingType = ir_lowerer::trimTemplateTypeText(queryFact->queryTypeText);") ==
        std::string::npos);
}

TEST_CASE("ir lowerer internal soa metadata receivers resolve interned ids") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path tailDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTailDispatch.h";

  REQUIRE(std::filesystem::exists(tailDispatchPath));
  const std::string source = readText(tailDispatchPath);

  const size_t resolverPos =
      source.find("auto resolveSemanticReceiverTypeText =");
  const size_t idCheckPos =
      source.find("typeTextId != InvalidSymbolId", resolverPos);
  const size_t internedResolvePos =
      source.find("semanticProgramResolveCallTargetString(*semanticProgram, typeTextId)",
                  idCheckPos);
  const size_t beforeInlinePos =
      source.find("auto emitInternalSoaMetadataBeforeInline", internedResolvePos);
  const size_t beforeInlineReceiverPos =
      source.find("resolveSemanticReceiverTypeText(\n"
                  "                  queryFact->receiverBindingTypeText,\n"
                  "                  queryFact->receiverBindingTypeTextId)",
                  beforeInlinePos);
  const size_t beforeInlineLocalFallbackPos =
      source.find("auto localIt = localsIn.find(receiverExpr.name);",
                  beforeInlineReceiverPos);
  const size_t nativeReceiverPos =
      source.find("auto isInternalSoaMetadataReceiver", beforeInlineReceiverPos);
  const size_t nativeReceiverResolvePos =
      source.find("resolveSemanticReceiverTypeText(",
                  nativeReceiverPos);
  const size_t nativeReceiverLocalFallbackPos =
      source.find("auto localIt = localsIn.find(receiverExpr.name);",
                  nativeReceiverResolvePos);

  REQUIRE(resolverPos != std::string::npos);
  REQUIRE(idCheckPos != std::string::npos);
  REQUIRE(internedResolvePos != std::string::npos);
  REQUIRE(beforeInlinePos != std::string::npos);
  REQUIRE(beforeInlineReceiverPos != std::string::npos);
  REQUIRE(beforeInlineLocalFallbackPos != std::string::npos);
  REQUIRE(nativeReceiverPos != std::string::npos);
  REQUIRE(nativeReceiverResolvePos != std::string::npos);
  REQUIRE(nativeReceiverLocalFallbackPos != std::string::npos);
  CHECK(resolverPos < idCheckPos);
  CHECK(idCheckPos < internedResolvePos);
  CHECK(internedResolvePos < beforeInlinePos);
  CHECK(beforeInlinePos < beforeInlineReceiverPos);
  CHECK(beforeInlineReceiverPos < beforeInlineLocalFallbackPos);
  CHECK(beforeInlineReceiverPos < nativeReceiverPos);
  CHECK(nativeReceiverPos < nativeReceiverResolvePos);
  CHECK(nativeReceiverResolvePos < nativeReceiverLocalFallbackPos);
  CHECK(source.find("auto classifyInternalReceiverFromSemanticFacts =") !=
        std::string::npos);
  CHECK(source.find("semanticInternalReceiver.has_value()") !=
        std::string::npos);
  CHECK(source.find("auto resolveReceiverTypeText") == std::string::npos);
}

TEST_CASE("ir lowerer statement expr has no inline builtin map insert family") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path statementsExprPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerStatementsExpr.h";

  REQUIRE(std::filesystem::exists(statementsExprPath));
  const std::string source = readText(statementsExprPath);

  CHECK(source.find("isBuiltinMapInsertFamilyPath") == std::string::npos);
  CHECK(source.find("/std/collections/map/insert_builtin") == std::string::npos);
}

TEST_CASE("ir lowerer inline map insert helper prefers semantic receiver facts") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path inlineCallsPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerInlineCalls.h";

  REQUIRE(std::filesystem::exists(inlineCallsPath));
  const std::string source = readText(inlineCallsPath);

  CHECK(source.find("auto inferValueKindFromTypeText = [&](std::string typeText,") !=
        std::string::npos);
  CHECK(source.find("std::function<bool(const std::string &,") !=
        std::string::npos);
  CHECK(source.find("auto tryPopulateMapKindsFromSemanticReceiver =") !=
        std::string::npos);
  CHECK(source.find("semanticProgramResolveCallTargetString(") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductCollectionSpecialization(semanticTargets.semanticIndex,") !=
        std::string::npos);
  CHECK(source.find("isBuiltinCollectionTypeName(normalizedBase, \"map\")") !=
        std::string::npos);
  CHECK(source.find("isExperimentalCollectionTypeName(normalizedBase, \"map\", \"Map\")") !=
        std::string::npos);
  CHECK(source.find("isBuiltinCollectionTypeName(collectionFamily, \"map\")") !=
        std::string::npos);
  CHECK(source.find("normalizedBase == \"std/collections/map\"") ==
        std::string::npos);
  CHECK(source.find("collectionFamily != \"map\" && collectionFamily != \"/map\"") ==
        std::string::npos);
  CHECK(source.find("collectionFact->keyTypeTextId") !=
        std::string::npos);
  CHECK(source.find("collectionFact->valueTypeTextId") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductQueryFact(callResolutionAdapters.semanticProgram,") !=
        std::string::npos);
  CHECK(source.find("tryApplySemanticMapTypeText(queryFact->bindingTypeTextId,") !=
        std::string::npos);
  CHECK(source.find("tryApplySemanticMapTypeText(queryFact->queryTypeTextId,") !=
        std::string::npos);
  CHECK(source.find("tryPopulateMapKindsFromSemanticReceiver(*originalValuesArg, valuesIt->second)") <
        source.find("callerValuesIt->second.keyValueKeyKind != LocalInfo::ValueKind::Unknown"));
  CHECK(source.find("valuesIt->second.keyValueKeyKind == LocalInfo::ValueKind::Unknown &&") <
        source.find("callerValuesIt->second.keyValueKeyKind != LocalInfo::ValueKind::Unknown"));
  CHECK(source.find("callerValuesIt->second.keyValueKeyKind != LocalInfo::ValueKind::Unknown") !=
        std::string::npos);
  CHECK(source.find("callee.parameters.size() >= 3") !=
        std::string::npos);
  CHECK(source.find("extractParameterTypeName(callee.parameters[1])") !=
        std::string::npos);
  CHECK(source.find("extractParameterTypeName(callee.parameters[2])") !=
        std::string::npos);
  CHECK(source.find("tryPopulateMapKindsFromSemanticReceiver(*originalValuesArg, valuesIt->second)") <
        source.find("extractParameterTypeName(callee.parameters[1])"));
  CHECK(source.find("return info.kind == LocalInfo::Kind::Value && hasKeyValueKinds(info);") !=
        std::string::npos);
  CHECK(source.find("if (hasKeyValueKinds(valuesIt->second)) {") !=
        std::string::npos);
  CHECK(source.find("ptrLocal = valuesIt->second.index;") !=
        std::string::npos);
  CHECK(source.find("if (receiverUsesExperimentalMapStruct) {") !=
        std::string::npos);
  CHECK(source.find("this helper path mutates the flat map") !=
        std::string::npos);
  CHECK(source.find("if (requireValue) {\n"
                    "        function.instructions.push_back({IrOpcode::PushI32, 0});\n"
                    "      }") !=
        std::string::npos);
}

TEST_CASE("ir lowerer vector metadata inline helpers stay in prime definitions") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path inlineCallsPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerInlineCalls.h";

  REQUIRE(std::filesystem::exists(inlineCallsPath));
  const std::string source = readText(inlineCallsPath);

  CHECK(source.find("isExperimentalVectorMetadataInlineHelper") ==
        std::string::npos);
  CHECK(source.find("isExperimentalVectorMetadataOwner") ==
        std::string::npos);
  CHECK(source.find("isInternalSoaMetadataInlineHelper") !=
        std::string::npos);
  CHECK(source.find("isInternalSoaMetadataOwner") != std::string::npos);
  CHECK(source.find(
            "isInternalSoaMetadataInlineHelper(callee.fullPath, \"set_field_count\")") !=
        std::string::npos);
}

TEST_CASE("ir lowerer skips builtin map insert rewrite for direct experimental map locals") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path statementCallPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererStatementCallEmission.cpp";
  const std::filesystem::path tailDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTailDispatch.h";

  REQUIRE(std::filesystem::exists(statementCallPath));
  REQUIRE(std::filesystem::exists(tailDispatchPath));
  const std::string statementSource = readText(statementCallPath);
  const std::string tailDispatchSource = readText(tailDispatchPath);

  CHECK(statementSource.find("isExperimentalMapStructPath") ==
        std::string::npos);
  CHECK(statementSource.find("targetInfo.isWrappedKeyValueTarget") ==
        std::string::npos);
  CHECK(tailDispatchSource.find("if (targetInfo.isWrappedKeyValueTarget ||") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("ir_lowerer::isExperimentalMapStructTypePath(targetInfo.structTypeName)) {") !=
        std::string::npos);
  CHECK(statementSource.find("experimentalMapType") ==
        std::string::npos);
  CHECK(tailDispatchSource.find("auto isSpecializedExperimentalKeyValueStructPath =") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("ir_lowerer::isExperimentalMapStructTypePath(structPath)") !=
        std::string::npos);
}

TEST_CASE("ir lowerer statement collection receiver gates use semantic product ids first") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path statementCallPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererStatementCallEmission.cpp";
  const std::filesystem::path callsStepPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerStatementsCallsStep.cpp";

  REQUIRE(std::filesystem::exists(statementCallPath));
  REQUIRE(std::filesystem::exists(callsStepPath));
  const std::string statementSource = readText(statementCallPath);
  const std::string callsStepSource = readText(callsStepPath);

  CHECK(statementSource.find("resolveStatementCallSemanticTypeText(") !=
        std::string::npos);
  CHECK(statementSource.find("semanticProgramResolveCallTargetString(*semanticProgram, typeTextId)") !=
        std::string::npos);
  CHECK(statementSource.find("findSemanticProductCollectionSpecialization(*semanticIndex, receiverExpr)") !=
        std::string::npos);
  CHECK(statementSource.find("collectionFact->collectionFamilyId") !=
        std::string::npos);
  CHECK(statementSource.find("collectionFact->elementTypeTextId") !=
        std::string::npos);
  CHECK(statementSource.find("collectionFact->keyTypeTextId") ==
        std::string::npos);
  CHECK(statementSource.find("collectionFact->valueTypeTextId") ==
        std::string::npos);
  CHECK(statementSource.find("findSemanticProductQueryFact(semanticProgram, *semanticIndex, receiverExpr)") !=
        std::string::npos);
  CHECK(statementSource.find("queryFact->bindingTypeTextId") <
        statementSource.find("queryFact->bindingTypeText,"));
  CHECK(statementSource.find("queryFact->queryTypeTextId") <
        statementSource.find("queryFact->queryTypeText,"));
  CHECK(statementSource.find("resolveStatementVectorReceiverTargetInfoFromSemanticFacts(") !=
        std::string::npos);
  const size_t semanticReceiverGate = statementSource.find(
      "resolveStatementVectorReceiverTargetInfoFromSemanticFacts(");
  const size_t canonicalResolver = statementSource.find(
      "resolveArrayVectorAccessTargetInfo(callExpr.args[receiverIndex]",
      semanticReceiverGate);
  const size_t keyValueTargetGate =
      statementSource.find("if (hasSemanticVectorFact) {",
                           semanticReceiverGate);
  REQUIRE(semanticReceiverGate != std::string::npos);
  REQUIRE(canonicalResolver != std::string::npos);
  REQUIRE(keyValueTargetGate != std::string::npos);
  CHECK(semanticReceiverGate < keyValueTargetGate);
  CHECK(keyValueTargetGate < canonicalResolver);
  CHECK(callsStepSource.find("input.semanticProgram,\n"
                             "      input.semanticIndex);") !=
        std::string::npos);
}

TEST_CASE("ir lowerer statement calls step receives semantic product adapters") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path callsLoweringPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerStatementsCalls.h";
  const std::filesystem::path callsStepPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerStatementsCallsStep.cpp";

  REQUIRE(std::filesystem::exists(callsLoweringPath));
  REQUIRE(std::filesystem::exists(callsStepPath));
  const std::string loweringSource = readText(callsLoweringPath);
  const std::string callsStepSource = readText(callsStepPath);

  const size_t stepCall =
      loweringSource.find("return ir_lowerer::runLowerStatementsCallsStep(");
  const size_t semanticProgram =
      loweringSource.find(".semanticProgram = callResolutionAdapters.semanticProgram",
                          stepCall);
  const size_t semanticIndex = loweringSource.find(
      ".semanticIndex = &callResolutionAdapters.semanticProductTargets.semanticIndex",
      semanticProgram);
  const size_t firstDependency =
      loweringSource.find(".inferExprKind =", semanticIndex);

  REQUIRE(stepCall != std::string::npos);
  REQUIRE(semanticProgram != std::string::npos);
  REQUIRE(semanticIndex != std::string::npos);
  REQUIRE(firstDependency != std::string::npos);
  CHECK(stepCall < semanticProgram);
  CHECK(semanticProgram < semanticIndex);
  CHECK(semanticIndex < firstDependency);

  const size_t vectorHelperCall =
      callsStepSource.find("const auto vectorHelperResult = tryEmitVectorStatementHelper(");
  const size_t receiverGate = callsStepSource.find(
      "classifyCallsStepVectorHelperReceiverFromSemanticFacts(\n"
      "                  candidate.args.front(),\n"
      "                  input.semanticProgram,\n"
      "                  input.semanticIndex)",
      vectorHelperCall);
  REQUIRE(vectorHelperCall != std::string::npos);
  REQUIRE(receiverGate != std::string::npos);
  CHECK(vectorHelperCall < receiverGate);
}

TEST_CASE("ir lowerer statement vector method gate uses semantic receiver facts first") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path callsStepPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerStatementsCallsStep.cpp";

  REQUIRE(std::filesystem::exists(callsStepPath));
  const std::string source = readText(callsStepPath);

  CHECK(source.find("#include \"IrLowererSemanticProductTargetAdapters.h\"") !=
        std::string::npos);
  CHECK(source.find("enum class CallsStepVectorHelperReceiverFact") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductCollectionSpecialization(*semanticIndex, receiverExpr)") !=
        std::string::npos);
  CHECK(source.find("collectionFact->collectionFamilyId") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductQueryFact(semanticProgram, *semanticIndex, receiverExpr)") !=
        std::string::npos);
  CHECK(source.find("queryFact->receiverBindingTypeTextId") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductBindingFact(*semanticIndex, receiverExpr)") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductLocalAutoFact(semanticProgram, *semanticIndex, receiverExpr)") !=
        std::string::npos);

  const size_t semanticFactUse =
      source.find("const CallsStepVectorHelperReceiverFact receiverFact =\n"
                  "              classifyCallsStepVectorHelperReceiverFromSemanticFacts(");
  const size_t staleLocalFallback =
      source.find("auto localIt = localsIn.find(candidate.args.front().name);");
  REQUIRE(semanticFactUse != std::string::npos);
  REQUIRE(staleLocalFallback != std::string::npos);
  CHECK(semanticFactUse < staleLocalFallback);

  const size_t nonVectorGate =
      source.find("if (receiverFact == CallsStepVectorHelperReceiverFact::NonVector) {",
                  semanticFactUse);
  const size_t methodResolution =
      source.find("return input.resolveMethodCallDefinition(candidate, localsIn) != nullptr;",
                  nonVectorGate);
  REQUIRE(nonVectorGate != std::string::npos);
  REQUIRE(methodResolution != std::string::npos);
  CHECK(nonVectorGate < methodResolution);
}

TEST_CASE("ir lowerer statement direct vector receiver fallbacks use semantic target facts") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path statementCallPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererStatementCallEmission.cpp";

  REQUIRE(std::filesystem::exists(statementCallPath));
  const std::string source = readText(statementCallPath);

  const size_t explicitHelperFallback = source.find(
      "resolveArrayVectorAccessTargetInfo(callExpr.args[receiverIndex],\n"
      "                                           localsIn,\n"
      "                                           {},\n"
      "                                           semanticProgram,\n"
      "                                           semanticIndex)");
  const size_t builtinReceiverFallback = source.find(
      "resolveArrayVectorAccessTargetInfo(candidate,\n"
      "                                           localsIn,\n"
      "                                           {},\n"
      "                                           semanticProgram,\n"
      "                                           semanticIndex)");
  const size_t explicitSemanticGate = source.find(
      "resolveStatementVectorReceiverTargetInfoFromSemanticFacts(\n"
      "            callExpr.args[receiverIndex],");
  const size_t builtinSemanticGate = source.find(
      "resolveStatementVectorReceiverTargetInfoFromSemanticFacts(candidate,");
  const size_t explicitTargetGate =
      source.find("return targetInfo.isVectorTarget;", explicitHelperFallback);
  const size_t builtinTargetGate =
      source.find("if (targetInfo.isVectorTarget) {", builtinReceiverFallback);

  REQUIRE(explicitHelperFallback != std::string::npos);
  REQUIRE(builtinReceiverFallback != std::string::npos);
  REQUIRE(explicitSemanticGate != std::string::npos);
  REQUIRE(builtinSemanticGate != std::string::npos);
  REQUIRE(explicitTargetGate != std::string::npos);
  REQUIRE(builtinTargetGate != std::string::npos);
  CHECK(explicitSemanticGate < explicitHelperFallback);
  CHECK(explicitHelperFallback < explicitTargetGate);
  CHECK(builtinSemanticGate < builtinReceiverFallback);
  CHECK(builtinReceiverFallback < builtinTargetGate);
  CHECK(source.find("resolveArrayVectorAccessTargetInfo(callExpr.args[receiverIndex], localsIn)") ==
        std::string::npos);
  CHECK(source.find("resolveArrayVectorAccessTargetInfo(candidate, localsIn)") ==
        std::string::npos);
}

TEST_CASE("ir lowerer inline dispatch map helper deferral uses semantic receiver facts first") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path inlineDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererInlineNativeCallDispatch.cpp";
  const std::filesystem::path tailDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTailDispatch.h";

  REQUIRE(std::filesystem::exists(inlineDispatchPath));
  REQUIRE(std::filesystem::exists(tailDispatchPath));
  const std::string inlineDispatchSource = readText(inlineDispatchPath);
  const std::string tailDispatchSource = readText(tailDispatchPath);

  CHECK(inlineDispatchSource.find("const SemanticProductIndex semanticIndex = buildSemanticProductIndex(semanticProgram);") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("auto resolveInlineSemanticTypeText =") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("semanticProgramResolveCallTargetString(*semanticProgram,") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("collectionFact->keyTypeTextId") ==
        std::string::npos);
  CHECK(inlineDispatchSource.find("collectionFact->valueTypeTextId") ==
        std::string::npos);
  CHECK(inlineDispatchSource.find("tryPopulateMapKindsFromSemanticTypeText(") ==
        std::string::npos);
  CHECK(inlineDispatchSource.find("tryPopulateKeyValueTargetInfoFromSemanticFacts(") ==
        std::string::npos);
  CHECK(inlineDispatchSource.find("resolveCollectionPairTypeInfo(receiverExpr,") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("error,\n"
                                "            semanticProgram);") !=
        std::string::npos);
}

TEST_CASE("ir lowerer inline dispatch vector helper deferral uses semantic receiver facts first") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path inlineDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererInlineNativeCallDispatch.cpp";

  REQUIRE(std::filesystem::exists(inlineDispatchPath));
  const std::string source = readText(inlineDispatchPath);

  CHECK(source.find("enum class InlineVectorTargetFact") !=
        std::string::npos);
  CHECK(source.find("auto classifyInlineVectorTargetFromSemanticFacts =") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductCollectionSpecialization(*semanticIndexPtr, targetExpr)") !=
        std::string::npos);
  CHECK(source.find("collectionFact->collectionFamilyId") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductQueryFact(semanticProgram, *semanticIndexPtr, targetExpr)") !=
        std::string::npos);
  CHECK(source.find("queryFact->receiverBindingTypeTextId") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductBindingFact(*semanticIndexPtr, targetExpr)") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductLocalAutoFact(semanticProgram, *semanticIndexPtr, targetExpr)") !=
        std::string::npos);

  const size_t semanticGate =
      source.find("return isVectorTarget(targetExpr, localsIn);");
  const size_t firstVectorHelperDeferral =
      source.find("if (expr.args.size() == 1 &&\n"
                  "        isSemanticOrLegacyVectorTarget(expr.args.front())) {");
  REQUIRE(semanticGate != std::string::npos);
  REQUIRE(firstVectorHelperDeferral != std::string::npos);
  CHECK(semanticGate < firstVectorHelperDeferral);
  CHECK(source.find("if (expr.args.size() == 1 && isVectorTarget(expr.args.front(), localsIn))") ==
        std::string::npos);
  CHECK(source.find("if (expr.args.size() == 2 && isVectorTarget(expr.args.front(), localsIn))") ==
        std::string::npos);

  CHECK(source.find("if (semanticFact == InlineVectorTargetFact::NonVector) {\n"
                    "      return false;\n"
                    "    }\n"
                    "    return isVectorTarget(targetExpr, localsIn);") !=
        std::string::npos);

  const size_t vectorReturningTarget =
      source.find("auto isVectorReturningCallTarget = [&](const Expr &receiverExpr) {");
  const size_t vectorReturningSemanticFact =
      source.find("const InlineVectorTargetFact semanticFact =\n"
                  "        classifyInlineVectorTargetFromSemanticFacts(receiverExpr);",
                  vectorReturningTarget);
  const size_t vectorReturningLegacyReturn =
      source.find("const Definition *receiverDef = resolveDefinitionCallFn(receiverExpr);",
                  vectorReturningTarget);
  REQUIRE(vectorReturningTarget != std::string::npos);
  REQUIRE(vectorReturningSemanticFact != std::string::npos);
  REQUIRE(vectorReturningLegacyReturn != std::string::npos);
  CHECK(vectorReturningSemanticFact < vectorReturningLegacyReturn);
  CHECK(source.find("if (semanticFact == InlineVectorTargetFact::NonVector) {\n"
                    "      return false;\n"
                    "    }\n"
                    "    const Definition *receiverDef = resolveDefinitionCallFn(receiverExpr);") !=
        std::string::npos);
}

TEST_CASE("ir lowerer inline dispatch SoA vector fallback uses semantic receiver facts first") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path inlineDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererInlineNativeCallDispatch.cpp";

  REQUIRE(std::filesystem::exists(inlineDispatchPath));
  const std::string source = readText(inlineDispatchPath);

  CHECK(source.find("auto isInlineRawBuiltinSoaVectorTypeText =") !=
        std::string::npos);
  CHECK(source.find("isRawBuiltinSoaVectorTarget = [&](const Expr &targetExpr)") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductCollectionSpecialization(*semanticIndexPtr, targetExpr)") !=
        std::string::npos);
  CHECK(source.find("collectionFact->collectionFamilyId") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductQueryFact(semanticProgram, *semanticIndexPtr, targetExpr)") !=
        std::string::npos);
  CHECK(source.find("queryFact->receiverBindingTypeTextId") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductBindingFact(*semanticIndexPtr, targetExpr)") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductLocalAutoFact(semanticProgram, *semanticIndexPtr, targetExpr)") !=
        std::string::npos);

  const size_t semanticGate = source.find(
      "if (semanticProgram != nullptr && semanticIndexPtr != nullptr &&\n"
      "        targetExpr.semanticNodeId != 0) {");
  const size_t localFallback =
      source.find("if (targetExpr.kind == Expr::Kind::Name)", semanticGate);
  const size_t fallbackCallback =
      source.find("[&](const Expr &receiverExpr) { return isRawBuiltinSoaVectorTarget(receiverExpr); }");
  REQUIRE(semanticGate != std::string::npos);
  REQUIRE(localFallback != std::string::npos);
  REQUIRE(fallbackCallback != std::string::npos);
  CHECK(semanticGate < localFallback);
  CHECK(localFallback < fallbackCallback);
  CHECK(source.find("[&](const Expr &receiverExpr) { return isSoaVectorTarget(receiverExpr, localsIn); }") ==
        std::string::npos);

  CHECK(source.find("isInlineRawBuiltinSoaVectorTypeText(queryFact->bindingTypeTextId,") !=
        std::string::npos);
  CHECK(source.find("return isSoaVectorTarget(targetExpr, localsIn);") ==
        std::string::npos);
}

TEST_CASE("ir lowerer inline dispatch collection access fallback uses semantic receiver facts first") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path inlineDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererInlineNativeCallDispatch.cpp";

  REQUIRE(std::filesystem::exists(inlineDispatchPath));
  const std::string source = readText(inlineDispatchPath);

  CHECK(source.find("enum class InlineCollectionAccessTargetFact") !=
        std::string::npos);
  CHECK(source.find("auto isInlineExperimentalMapTypeName =") ==
        std::string::npos);
  CHECK(source.find("auto classifyInlineCollectionAccessTargetFromSemanticFacts =") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductCollectionSpecialization(*semanticIndexPtr, targetExpr)") !=
        std::string::npos);
  CHECK(source.find("collectionFact->collectionFamilyId") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductQueryFact(semanticProgram, *semanticIndexPtr, targetExpr)") !=
        std::string::npos);
  CHECK(source.find("queryFact->receiverBindingTypeTextId") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductBindingFact(*semanticIndexPtr, targetExpr)") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductLocalAutoFact(semanticProgram, *semanticIndexPtr, targetExpr)") !=
        std::string::npos);

  const size_t semanticFactUse =
      source.find("const InlineCollectionAccessTargetFact semanticFact =\n"
                  "              classifyInlineCollectionAccessTargetFromSemanticFacts(receiverExpr);");
  const size_t nameReceiverBranch =
      source.rfind("if (receiverExpr.kind == Expr::Kind::Name) {",
                   semanticFactUse);
  const size_t staleLocalFallback =
      source.find("auto it = localsIn.find(receiverExpr.name);");
  const size_t legacyInferFallback =
      source.find("const LocalInfo::ValueKind inferredReceiverKind =",
                  semanticFactUse);
  const size_t nonCollectionGate = source.find(
      "if (semanticFact == InlineCollectionAccessTargetFact::NonCollectionAccess) {",
      semanticFactUse);
  REQUIRE(semanticFactUse != std::string::npos);
  REQUIRE(nameReceiverBranch != std::string::npos);
  REQUIRE(nonCollectionGate != std::string::npos);
  REQUIRE(staleLocalFallback != std::string::npos);
  REQUIRE(legacyInferFallback != std::string::npos);
  CHECK(semanticFactUse < legacyInferFallback);
  CHECK(semanticFactUse < staleLocalFallback);
  CHECK(nonCollectionGate < legacyInferFallback);
  CHECK(nonCollectionGate < staleLocalFallback);
  CHECK(source.find("isInlineExperimentalMapTypeName(collectionFamily)") ==
        std::string::npos);
  const size_t semanticArrayVectorFallback = source.find(
      "resolveArrayVectorAccessTargetInfo(receiverExpr,\n"
      "                                               localsIn,\n"
      "                                               {},\n"
      "                                               semanticProgram,\n"
      "                                               semanticIndexPtr)",
      semanticFactUse);
  const size_t nameSemanticMapFallback =
      source.find("resolveCollectionPairTypeInfo(receiverExpr,",
                  nameReceiverBranch);
  const size_t callReceiverBranch =
      source.find("if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding) {",
                  semanticFactUse);
  REQUIRE(callReceiverBranch != std::string::npos);
  const size_t callSemanticMapFallback =
      source.find("resolveCollectionPairTypeInfo(receiverExpr,",
                  callReceiverBranch);
  const size_t callSemanticFactUse =
      source.find("const InlineCollectionAccessTargetFact semanticFact =\n"
                  "            classifyInlineCollectionAccessTargetFromSemanticFacts(receiverExpr);",
                  callReceiverBranch);
  const size_t builtinCollectionFallback =
      source.find("getBuiltinCollectionName(receiverExpr, collectionName)",
                  semanticFactUse);
  REQUIRE(semanticArrayVectorFallback != std::string::npos);
  REQUIRE(nameSemanticMapFallback != std::string::npos);
  REQUIRE(callSemanticMapFallback != std::string::npos);
  REQUIRE(callSemanticFactUse != std::string::npos);
  REQUIRE(builtinCollectionFallback != std::string::npos);
  CHECK(nonCollectionGate < semanticArrayVectorFallback);
  CHECK(nameSemanticMapFallback < semanticFactUse);
  CHECK(callSemanticMapFallback < callSemanticFactUse);
  CHECK(semanticArrayVectorFallback < builtinCollectionFallback);
  CHECK(callSemanticMapFallback < builtinCollectionFallback);
  CHECK(source.find("resolveCollectionPairTypeInfo(receiverExpr, localsIn, inferCallKeyValueTargetInfo).isKeyValueTarget") ==
        std::string::npos);
}

TEST_CASE("ir lowerer tail map insert rewrite uses semantic receiver facts first") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path tailDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTailDispatch.h";

  REQUIRE(std::filesystem::exists(tailDispatchPath));
  const std::string tailDispatchSource = readText(tailDispatchPath);

  CHECK(tailDispatchSource.find("const SemanticProductIndex tailDispatchKeyValueSemanticIndex =") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("tailDispatchKeyValueSemanticIndexPtr") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("populateTailDispatchKeyValueStructPathFromKinds") ==
        std::string::npos);
  CHECK(tailDispatchSource.find("resolveSpecializedExperimentalMapStructPathForBindingType(\n"
                                "                      typeText, structPath)") ==
        std::string::npos);
  CHECK(tailDispatchSource.find("collectionTypePath(\"map\", false) + \"<\"") ==
        std::string::npos);
  CHECK(tailDispatchSource.find("resolveCollectionPairTypeInfo(\n"
                                "                  callExpr.args[receiverIndex],\n"
                                "                  localsIn,\n"
                                "                  inferCallKeyValueTargetInfo,\n"
                                "                  semanticProgram,\n"
                                "                  tailDispatchKeyValueSemanticIndexPtr)") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("auto resolveTailDispatchSemanticTypeText =") ==
        std::string::npos);
  CHECK(tailDispatchSource.find("tryPopulateTailDispatchMapKindsFromSemanticTypeText") ==
        std::string::npos);
  CHECK(tailDispatchSource.find("tryPopulateTailDispatchKeyValueTargetInfoFromSemanticFacts") ==
        std::string::npos);
  CHECK(tailDispatchSource.find("inferTailDispatchMapStructPathFromTypeText") ==
        std::string::npos);
  CHECK(tailDispatchSource.find("ir_lowerer::isExperimentalMapStructTypePath(targetInfo.structTypeName)") !=
        std::string::npos);
}

TEST_CASE("ir lowerer no longer synthesizes generated map value paths") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path lowererFiles[] = {
      repoRoot / "src" / "ir_lowerer" / "IrLowererAccessTargetResolution.cpp",
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTailDispatch.h",
      repoRoot / "src" / "ir_lowerer" / "IrLowererStatementBindingHelpers.cpp",
      repoRoot / "src" / "ir_lowerer" / "IrLowererStatementBindingTypeMetadata.cpp",
      repoRoot / "src" / "ir_lowerer" / "IrLowererStructSlotLayoutHelpers.cpp",
      repoRoot / "src" / "ir_lowerer" / "IrLowererStructTypeHelpers.cpp",
      repoRoot / "src" / "ir_lowerer" / "IrLowererUninitializedStructInference.cpp",
  };

  for (const auto &path : lowererFiles) {
    INFO(path.string());
    REQUIRE(std::filesystem::exists(path));
    const std::string source = readText(path);
    CHECK(source.find("inferExperimentalKeyValueStructPathFromKinds") ==
          std::string::npos);
    CHECK(source.find("collectionTypePath(\"map\") + \"/MapValue\"") ==
          std::string::npos);
    CHECK(source.find("collectionTypePath(\"map\") << \"/MapValue\"") ==
          std::string::npos);
    CHECK(source.find("mangleTemplateTypeArgsSuffix({keyType, valueType})") ==
          std::string::npos);
    CHECK(source.find("collectionTypePath(\"map\", false) + \"<\"") ==
          std::string::npos);
  }
}

TEST_CASE("ir lowerer tail explicit map helper rewrite uses semantic receiver facts first") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path tailDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTailDispatch.h";

  REQUIRE(std::filesystem::exists(tailDispatchPath));
  const std::string tailDispatchSource = readText(tailDispatchPath);

  const size_t rewritePos =
      tailDispatchSource.find("auto rewriteExplicitKeyValueHelperBuiltinExpr =");
  REQUIRE(rewritePos != std::string::npos);
  const size_t semanticIndexPos =
      tailDispatchSource.find("explicitKeyValueHelperSemanticIndex", rewritePos);
  const size_t semanticMapResolverPos = tailDispatchSource.find(
      "ir_lowerer::resolveCollectionPairTypeInfo(\n"
      "                  callExpr.args.front(),\n"
      "                  localsIn,\n"
      "                  inferCallKeyValueTargetInfo,\n"
      "                  semanticProgram,\n"
      "                  explicitKeyValueHelperSemanticIndexPtr)",
      rewritePos);
  const size_t semanticReceiverResolverPos = tailDispatchSource.find(
      "ir_lowerer::resolveArrayVectorAccessTargetInfo(\n"
      "                  callExpr.args.front(),\n"
      "                  localsIn,\n"
      "                  {},\n"
      "                  semanticProgram,\n"
      "                  explicitKeyValueHelperSemanticIndexPtr)",
      rewritePos);
  const size_t staleLocalGatePos =
      tailDispatchSource.find("if (!keyValueTargetInfo.isKeyValueTarget) {", rewritePos);
  const size_t argsPackGatePos =
      tailDispatchSource.find("receiverTargetInfo.isArgsPackTarget", rewritePos);

  REQUIRE(semanticIndexPos != std::string::npos);
  REQUIRE(semanticMapResolverPos != std::string::npos);
  REQUIRE(semanticReceiverResolverPos != std::string::npos);
  REQUIRE(staleLocalGatePos != std::string::npos);
  REQUIRE(argsPackGatePos != std::string::npos);
  CHECK(semanticIndexPos < semanticMapResolverPos);
  CHECK(semanticMapResolverPos < staleLocalGatePos);
  CHECK(semanticReceiverResolverPos < argsPackGatePos);
}

TEST_CASE("ir lowerer tail canonical experimental map helper rewrite uses semantic receiver facts first") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path tailDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTailDispatch.h";

  REQUIRE(std::filesystem::exists(tailDispatchPath));
  const std::string tailDispatchSource = readText(tailDispatchPath);

  const size_t rewritePos = tailDispatchSource.find(
      "auto rewriteCanonicalKeyValueHelperForExperimentalReceiverExpr =");
  REQUIRE(rewritePos != std::string::npos);
  const size_t semanticIndexPos =
      tailDispatchSource.find("canonicalKeyValueHelperSemanticIndex", rewritePos);
  const size_t semanticFactGatePos = tailDispatchSource.find(
      "hasCanonicalKeyValueHelperSemanticReceiverFact(receiverExpr)",
      rewritePos);
  const size_t localFallbackPos =
      tailDispatchSource.find("auto it = localsIn.find(receiverExpr.name);",
                              rewritePos);
  const size_t semanticMapResolverPos = tailDispatchSource.find(
      "ir_lowerer::resolveCollectionPairTypeInfo(\n"
      "                    receiverExpr,\n"
      "                    localsIn,\n"
      "                    inferCallKeyValueTargetInfo,\n"
      "                    semanticProgram,\n"
      "                    canonicalKeyValueHelperSemanticIndexPtr)",
      rewritePos);
  const size_t semanticReceiverResolverPos = tailDispatchSource.find(
      "ir_lowerer::resolveArrayVectorAccessTargetInfo(\n"
      "                    receiverExpr,\n"
      "                    localsIn,\n"
      "                    {},\n"
      "                    semanticProgram,\n"
      "                    canonicalKeyValueHelperSemanticIndexPtr)",
      rewritePos);

  REQUIRE(semanticIndexPos != std::string::npos);
  REQUIRE(semanticFactGatePos != std::string::npos);
  REQUIRE(localFallbackPos != std::string::npos);
  REQUIRE(semanticMapResolverPos != std::string::npos);
  REQUIRE(semanticReceiverResolverPos != std::string::npos);
  CHECK(semanticIndexPos < semanticMapResolverPos);
  CHECK(semanticMapResolverPos < semanticFactGatePos);
  CHECK(semanticFactGatePos < localFallbackPos);
  CHECK(localFallbackPos < semanticReceiverResolverPos);
}

TEST_CASE("ir lowerer tail borrowed key/value receiver rewrite uses semantic receiver facts first") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path tailDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTailDispatch.h";

  REQUIRE(std::filesystem::exists(tailDispatchPath));
  const std::string tailDispatchSource = readText(tailDispatchPath);

  const size_t rewritePos =
      tailDispatchSource.find("auto rewriteImplicitBorrowedKeyValueReceiverExpr =");
  REQUIRE(rewritePos != std::string::npos);
  const size_t semanticIndexPos =
      tailDispatchSource.find("borrowedKeyValueReceiverSemanticIndex", rewritePos);
  const size_t semanticMapResolverPos = tailDispatchSource.find(
      "ir_lowerer::resolveCollectionPairTypeInfo(\n"
      "                    receiverExpr,\n"
      "                    localsIn,\n"
      "                    inferCallKeyValueTargetInfo,\n"
      "                    semanticProgram,\n"
      "                    borrowedKeyValueReceiverSemanticIndexPtr)",
      rewritePos);
  const size_t wrappedMapGatePos = tailDispatchSource.find(
      "return keyValueTargetInfo.isKeyValueTarget && keyValueTargetInfo.isWrappedKeyValueTarget;",
      rewritePos);
  const size_t rewriteEndPos =
      tailDispatchSource.find("Expr rewrittenExplicitKeyValueHelperExpr;", rewritePos);
  const size_t oldNameLocalProbePos =
      tailDispatchSource.find("localsIn.find(receiverExpr.name)", rewritePos);
  const size_t oldArgsPackLocalProbePos =
      tailDispatchSource.find("localsIn.find(receiverExpr.args.front().name)",
                              rewritePos);

  REQUIRE(semanticIndexPos != std::string::npos);
  REQUIRE(semanticMapResolverPos != std::string::npos);
  REQUIRE(wrappedMapGatePos != std::string::npos);
  REQUIRE(rewriteEndPos != std::string::npos);
  CHECK(semanticIndexPos < semanticMapResolverPos);
  CHECK(semanticMapResolverPos < wrappedMapGatePos);
  const bool noOldNameLocalProbeInKeyValueRewrite =
      oldNameLocalProbePos == std::string::npos ||
      oldNameLocalProbePos > rewriteEndPos;
  const bool noOldArgsPackLocalProbeInKeyValueRewrite =
      oldArgsPackLocalProbePos == std::string::npos ||
      oldArgsPackLocalProbePos > rewriteEndPos;
  CHECK(noOldNameLocalProbeInKeyValueRewrite);
  CHECK(noOldArgsPackLocalProbeInKeyValueRewrite);
}

TEST_CASE("ir lowerer native tail map access inference uses semantic receiver facts first") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path tailDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTailDispatch.h";

  REQUIRE(std::filesystem::exists(tailDispatchPath));
  const std::string tailDispatchSource = readText(tailDispatchPath);

  CHECK(tailDispatchSource.find("resolveCollectionPairTypeInfo(\n"
                                "                  targetCallExpr,\n"
                                "                  localsIn,\n"
                                "                  inferCallKeyValueTargetInfo,\n"
                                "                  semanticProgram,\n"
                                "                  tailDispatchKeyValueSemanticIndexPtr)") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("populateTailDispatchKeyValueStructPathFromKinds(targetInfoOut);") ==
        std::string::npos);
  CHECK(tailDispatchSource.find("return targetInfoOut.isKeyValueTarget;") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("inferNativeTailMapKindsFromTypeText =") ==
        std::string::npos);
  CHECK(tailDispatchSource.find("tryPopulateMapFromSemanticReceiverFacts") ==
        std::string::npos);
  CHECK(tailDispatchSource.find("inferExperimentalMapStructPathFromTypeTexts") ==
        std::string::npos);
}

TEST_CASE("ir lowerer native tail vector access inference uses semantic receiver facts first") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path tailDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTailDispatch.h";

  REQUIRE(std::filesystem::exists(tailDispatchPath));
  const std::string tailDispatchSource = readText(tailDispatchPath);

  CHECK(tailDispatchSource.find("inferNativeTailArrayVectorTargetFromTypeText =") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("tryPopulateArrayVectorFromSemanticReceiverFacts") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("tryPopulateNativeTailArrayVectorTargetFromSemanticCollection") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("findSemanticProductCollectionSpecialization(\n"
                                "                            semanticIndex, targetCallExpr)") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("collectionFact->elementTypeTextId") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("findSemanticProductQueryFact(semanticProgram, semanticIndex, targetCallExpr)") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("queryFact->bindingTypeTextId") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("queryFact->receiverBindingTypeTextId") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("findSemanticProductBindingFact(\n"
                                "                            semanticIndex, targetCallExpr)") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("findSemanticProductLocalAutoFact(\n"
                                "                            semanticProgram, semanticIndex, targetCallExpr)") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("if (tryPopulateArrayVectorFromSemanticReceiverFacts()) {\n"
                                "                return true;\n"
                                "              }") <
        tailDispatchSource.find("if (targetCallExpr.isFieldAccess && targetCallExpr.args.size() == 1)"));
}

TEST_CASE("ir lowerer direct expr and inference rewrites guard explicit map defs") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path emitExprPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExpr.h";
  const std::filesystem::path inferenceDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerInferenceDispatchSetup.cpp";

  REQUIRE(std::filesystem::exists(emitExprPath));
  REQUIRE(std::filesystem::exists(inferenceDispatchPath));

  const std::string emitExprSource = readText(emitExprPath);
  const std::string inferenceDispatchSource = readText(inferenceDispatchPath);

  CHECK(emitExprSource.find("resolveKeyValueHelperAliasName(expr, canonicalKeyValueHelperName) &&") !=
        std::string::npos);
  CHECK(emitExprSource.find("resolveDefinitionCall(expr) == nullptr &&") != std::string::npos);
  CHECK(emitExprSource.find("canonicalKeyValueHelperName == \"insert_ref\"") !=
        std::string::npos);
  CHECK(inferenceDispatchSource.find("resolveKeyValueHelperAliasName(expr, canonicalKeyValueHelperName) &&") !=
        std::string::npos);
  CHECK(inferenceDispatchSource.find(
            "(!stateInOut.resolveDefinitionCall || stateInOut.resolveDefinitionCall(expr) == nullptr) &&") !=
        std::string::npos);
  CHECK(inferenceDispatchSource.find("canonicalKeyValueHelperName == \"insert_ref\"") !=
        std::string::npos);
}

TEST_CASE("ir lowerer canonical map contains and tryAt rewrites stay recognized late") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path collectionHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprCollectionHelpers.h";
  const std::filesystem::path tailDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTailDispatch.h";

  REQUIRE(std::filesystem::exists(collectionHelpersPath));
  REQUIRE(std::filesystem::exists(tailDispatchPath));

  const std::string collectionHelpersSource = readText(collectionHelpersPath);
  const std::string tailDispatchSource = readText(tailDispatchPath);

  CHECK(collectionHelpersSource.find("keyValueHelperSurfaceMetadataForLowerEmitExpr()") !=
        std::string::npos);
  CHECK(collectionHelpersSource.find("helperName != \"count\" && helperName != \"contains\" &&") !=
        std::string::npos);
  CHECK(collectionHelpersSource.find("helperName != \"tryAt\" && helperName != \"insert\"") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("ir_lowerer::collectionMemberPath(\"map\", helperName)") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("helperName != \"count\" && helperName != \"contains\" &&") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("helperName != \"tryAt\" && helperName != \"at\" &&") !=
        std::string::npos);
}

TEST_CASE("ir lowerer late expression fallback guards explicit map helper defs") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path statementsExprPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerStatementsExpr.h";

  REQUIRE(std::filesystem::exists(statementsExprPath));
  const std::string source = readText(statementsExprPath);

  CHECK(source.find("auto matchesDirectHelperDefinitionFamilyPath =") !=
        std::string::npos);
  CHECK(source.find("auto isDirectHelperDefinitionFamily = [&](const Expr &callExpr,") !=
        std::string::npos);
  CHECK(source.find("const std::string resolvedPath = resolveExprPath(callExpr);") !=
        std::string::npos);
  CHECK(source.find("matchesDirectHelperDefinitionFamilyPath(resolvedPath, callee);") !=
        std::string::npos);
  CHECK(source.find("auto findDirectHelperDefinition = [&](const std::string &rawPath) -> const Definition * {") !=
        std::string::npos);
  CHECK(source.find("auto matchesGeneratedLeafDefinition = [&](const std::string &path,") !=
        std::string::npos);
  CHECK(source.find("path.compare(rawPath.size(), markerSize, marker) == 0 &&") !=
        std::string::npos);
  CHECK(source.find("path.find('/', rawPath.size() + markerSize) ==") !=
        std::string::npos);
  CHECK(source.find("auto stripGeneratedHelperSuffix = [](std::string helperPath) {") !=
        std::string::npos);
  CHECK(source.find("const size_t leafStart = helperPath.find_last_of('/');") !=
        std::string::npos);
  CHECK(source.find(
            "helperPath.find(\"__\", leafStart == std::string::npos ? 0 : leafStart + 1);") !=
        std::string::npos);
  CHECK(source.find("auto extractHelperTail = [&](std::string helperPath) {") !=
        std::string::npos);
  CHECK(source.find("auto isInternalSoaHelperFamilyName = [&](const std::string &helperName) {") !=
        std::string::npos);
  CHECK(source.find("helperName.rfind(\"soaColumn\", 0) == 0 ||") !=
        std::string::npos);
  CHECK(source.find("helperName.rfind(\"SoaColumns\", 0) == 0;") !=
        std::string::npos);
  CHECK(source.find("auto findDirectInternalSoaDefinition = [&](const std::string &rawPath)") !=
        std::string::npos);
  CHECK(source.find("path.rfind(\"/std/collections/internal_soa_storage/\", 0) != 0") !=
        std::string::npos);
  CHECK(source.find("auto findDirectStructDefinition = [&](const Expr &callExpr) -> const Definition * {") !=
        std::string::npos);
  CHECK(source.find("resolveStructTypeName(callExpr.name, callExpr.namespacePrefix, directStructPath)") !=
        std::string::npos);
  CHECK(source.find("auto isInternalSoaHelperFamilyPath = [&](const std::string &path) {") !=
        std::string::npos);
  CHECK(source.find("extractHelperTail(normalizeCollectionHelperPath(path))") !=
        std::string::npos);
  CHECK(source.find("if (directCallee == nullptr &&") !=
        std::string::npos);
  CHECK(source.find("isInternalSoaHelperFamilyPath(rawPath)) {") !=
        std::string::npos);
  CHECK(source.find("directCallee = findDirectInternalSoaDefinition(rawPath);") !=
        std::string::npos);
  CHECK(source.find("if (directCallee == nullptr && !expr.isMethodCall) {") !=
        std::string::npos);
  CHECK(source.find("directCallee = findDirectStructDefinition(expr);") !=
        std::string::npos);
  CHECK(source.find("if (ir_lowerer::isStructDefinition(*directCallee)) {") !=
        std::string::npos);
  CHECK(source.find("directCallee->fullPath.rfind(\"/std/collections/internal_soa_storage/\", 0) == 0 &&") !=
        std::string::npos);
  CHECK(source.find("isInternalSoaHelperFamilyPath(directCallee->fullPath)) {") !=
        std::string::npos);
  CHECK(source.find("(helperName == \"count\" || helperName == \"contains\" ||") !=
        std::string::npos);
  CHECK(source.find("helperName == \"tryAt\" || helperName == \"at\" ||\n"
                    "                 helperName == \"at_unsafe\" || helperName == \"insert\" ||\n"
                    "                 helperName == \"insert_ref\") &&") !=
        std::string::npos);
  CHECK(source.find("const size_t generatedSuffix = helperPath.find(\"__\");") ==
        std::string::npos);
  CHECK(source.find("const std::string specializedPrefix = rawPath + \"__t\";") ==
        std::string::npos);
  CHECK(source.find("const std::string overloadPrefix = rawPath + \"__ov\";") ==
        std::string::npos);
  CHECK(source.find("rawPath.rfind(\"/map/\", 0) == 0 ||") ==
        std::string::npos);
  CHECK(source.find("rawPath.rfind(\"/std/collections/experimental_map/\", 0) == 0) &&") ==
        std::string::npos);
  CHECK(source.find("isDirectCollectionHelperPath(rawPath)") !=
        std::string::npos);
  CHECK(source.find("isCanonicalKeyValueHelperFamilyPath(rawPath)") !=
        std::string::npos);
  CHECK(source.find("directCallee = findDirectHelperDefinition(rawPath);") !=
        std::string::npos);
  CHECK(source.find("directCallee = findDirectHelperDefinition(resolvedExprPath);") !=
        std::string::npos);
  CHECK(source.find("if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {") !=
        std::string::npos);
  CHECK(source.find("rawPath.rfind(\"/std/collections/map/\", 0) == 0 &&") ==
        std::string::npos);
  CHECK(source.find("getBuiltinArrayAccessName(expr, accessName)") !=
        std::string::npos);
}

TEST_CASE("ir lowerer late expression canonical map helpers use path-family gates") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path statementsExprPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerStatementsExpr.h";

  REQUIRE(std::filesystem::exists(statementsExprPath));
  const std::string source = readText(statementsExprPath);

  const size_t canonicalMapGate =
      source.find("auto isCanonicalKeyValueHelperFamilyPath =");
  const size_t explicitAccessGate = source.find(
      "const bool isExplicitCanonicalKeyValueAccess =",
      canonicalMapGate);
  const size_t builtinDeferral =
      source.find("if (isExplicitCanonicalKeyValueAccess &&",
                  explicitAccessGate);

  REQUIRE(canonicalMapGate != std::string::npos);
  REQUIRE(explicitAccessGate != std::string::npos);
  REQUIRE(builtinDeferral != std::string::npos);
  CHECK(canonicalMapGate < explicitAccessGate);
  CHECK(explicitAccessGate < builtinDeferral);
  CHECK(source.find("resolveCollectionPairTypeInfo(expr.args.front(), localsIn)") ==
        std::string::npos);
}

TEST_CASE("ir lowerer inline native dispatch prefers published canonical map access helpers") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path inlineDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererInlineNativeCallDispatch.cpp";

  REQUIRE(std::filesystem::exists(inlineDispatchPath));
  const std::string source = readText(inlineDispatchPath);

  CHECK(source.find("const bool isCanonicalStdKeyValueHelperCall =") !=
        std::string::npos);
  CHECK(source.find("isCanonicalPublishedStdlibSurfaceHelperPath(") !=
        std::string::npos);
  CHECK(source.find("inlineKeyValueHelperMetadata()") !=
        std::string::npos);
  CHECK(source.find("StdlibSurfaceId::CollectionsKeyValueHelpers") ==
        std::string::npos);
  CHECK(source.find("rawPath.rfind(\"/std/collections/map/\", 0) == 0") ==
        std::string::npos);
  CHECK(source.find("helperName == \"contains\" || helperName == \"contains_ref\"") !=
        std::string::npos);
  CHECK(source.find("helperName == \"tryAt\" || helperName == \"tryAt_ref\"") !=
        std::string::npos);
  CHECK(source.find("expr.args.front().kind == Expr::Kind::Name") !=
        std::string::npos);
}

TEST_CASE("ir lowerer synthetic method probes clear direct helper semantic targets") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path callHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererCallHelpers.cpp";
  const std::filesystem::path inlineDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererInlineNativeCallDispatch.cpp";
  const std::filesystem::path returnKindHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererSetupTypeReturnKindHelpers.cpp";
  const std::filesystem::path collectionHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererSetupTypeCollectionHelpers.cpp";

  REQUIRE(std::filesystem::exists(callHelpersPath));
  REQUIRE(std::filesystem::exists(inlineDispatchPath));
  REQUIRE(std::filesystem::exists(returnKindHelpersPath));
  REQUIRE(std::filesystem::exists(collectionHelpersPath));

  const std::string callHelpersSource = readText(callHelpersPath);
  const std::string inlineDispatchSource = readText(inlineDispatchPath);
  const std::string returnKindHelpersSource = readText(returnKindHelpersPath);
  const std::string collectionHelpersSource = readText(collectionHelpersPath);

  CHECK(callHelpersSource.find("methodExpr.semanticNodeId = 0;") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("methodExpr.semanticNodeId = 0;") !=
        std::string::npos);
  CHECK(returnKindHelpersSource.find("methodExpr.semanticNodeId = 0;") !=
        std::string::npos);
  CHECK(collectionHelpersSource.find("helperNameOut == \"get\"") !=
        std::string::npos);
  CHECK(collectionHelpersSource.find("helperNameOut == \"get_ref\"") !=
        std::string::npos);
  CHECK(collectionHelpersSource.find("helperNameOut == \"ref\"") !=
        std::string::npos);
  CHECK(collectionHelpersSource.find("helperNameOut == \"ref_ref\"") !=
        std::string::npos);
}

TEST_CASE("ir lowerer binding normalization guards explicit map helper defs") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path bindingsPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerStatementsBindings.h";

  REQUIRE(std::filesystem::exists(bindingsPath));
  const std::string source = readText(bindingsPath);

  CHECK(source.find("auto resolveDirectKeyValueHelperPath = [&](const Expr &exprIn) {") !=
        std::string::npos);
  CHECK(source.find("auto findDirectKeyValueHelperDefinition = [&](const std::string &rawPath) -> const Definition * {") !=
        std::string::npos);
  CHECK(source.find("findDirectKeyValueHelperDefinition(rawPath) != nullptr") !=
        std::string::npos);
  CHECK(source.find("exprIn.name = helperName;") !=
        std::string::npos);
}

TEST_CASE("ir lowerer statement binding args-pack initializer uses semantic target facts") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path bindingsPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerStatementsBindings.h";

  REQUIRE(std::filesystem::exists(bindingsPath));
  const std::string source = readText(bindingsPath);

  const size_t semanticResolver = source.find(
      "ir_lowerer::resolveArrayVectorAccessTargetInfo(\n"
      "                  init.args.front(),\n"
      "                  localsIn,\n"
      "                  {},\n"
      "                  callResolutionAdapters.semanticProgram,\n"
      "                  &callResolutionAdapters.semanticProductTargets.semanticIndex)");
  const size_t argsPackGate =
      source.find("const bool isVectorArgsPackAccess =", semanticResolver);
  const size_t emitAccess =
      source.find("ir_lowerer::emitArrayVectorIndexedAccess(",
                  semanticResolver);

  REQUIRE(semanticResolver != std::string::npos);
  REQUIRE(argsPackGate != std::string::npos);
  REQUIRE(emitAccess != std::string::npos);
  CHECK(semanticResolver < argsPackGate);
  CHECK(argsPackGate < emitAccess);
  CHECK(source.find(
            "resolveArrayVectorAccessTargetInfo(init.args.front(), localsIn)") ==
        std::string::npos);
}

TEST_CASE("ir lowerer packed result buffer helpers normalize scoped gfx calls") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path emitExprPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExpr.h";

  REQUIRE(std::filesystem::exists(emitExprPath));
  const std::string source = readText(emitExprPath);

  CHECK(source.find("const std::string scopedName = resolveExprPath(candidate);") !=
        std::string::npos);
  CHECK(source.find("std::string normalized = resolveExprPath(valueExpr);") !=
        std::string::npos);
  CHECK(source.find("candidate.name == \"/std/gfx/Buffer\"") == std::string::npos);
  CHECK(source.find("std::string normalized = valueExpr.name;") == std::string::npos);
}

TEST_CASE("ir lowerer rooted map alias definition fallback covers insert helpers") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path callResolutionPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererCallResolution.cpp";

  REQUIRE(std::filesystem::exists(callResolutionPath));
  const std::string source = readText(callResolutionPath);

  CHECK(source.find(
            "return resolveKeyValueHelperAliasName(expr, helperName) &&\n"
            "         (helperName == \"count\" || helperName == \"contains\" ||\n"
            "          helperName == \"tryAt\" || helperName == \"at\" ||\n"
            "          helperName == \"at_unsafe\" || helperName == \"insert\" ||\n"
            "          helperName == \"insert_ref\");") !=
        std::string::npos);
}

TEST_SUITE_END();
