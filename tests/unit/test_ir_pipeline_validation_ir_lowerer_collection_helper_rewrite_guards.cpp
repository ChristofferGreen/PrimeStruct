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
            "auto rewriteExplicitBuiltinMapHelperExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {") !=
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
  CHECK(source.find(
            "if (directHelperPath.rfind(\"/std/collections/map/\", 0) == 0 ||\n"
            "              directHelperPath.rfind(\"/std/collections/experimental_map/\", 0) == 0) {") !=
        std::string::npos);
  CHECK(source.find("resolveDirectHelperPath(callExpr).rfind(\"/std/collections/experimental_map/\", 0) == 0") !=
        std::string::npos);
  CHECK(source.find("if (resolveDefinitionCall(callExpr) != nullptr)") != std::string::npos);
  CHECK(source.find("rewrittenExpr.name = helperName;") != std::string::npos);
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
  CHECK(source.find("primec::StdlibSurfaceId::CollectionsVectorHelpers") !=
        std::string::npos);
  CHECK(source.find("if (!resolvePublishedLateCollectionMemberName(") !=
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
            "primec::StdlibSurfaceId::CollectionsVectorHelpers,\n"
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
  CHECK(source.find("primec::StdlibSurfaceId::CollectionsMapConstructors") !=
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
  CHECK(source.find("const Definition *methodCallee =\n"
                    "                  resolveMethodCallDefinition(expr, localsIn);") !=
        std::string::npos);
  CHECK(source.find("methodCallee = findDirectHelperDefinition(resolveExprPath(expr));") !=
        std::string::npos);
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
      source.find("auto isBuiltinMapInsertFamilyPath", wrapperHelper);

  REQUIRE(samePathHelper != std::string::npos);
  REQUIRE(wrapperHelper != std::string::npos);
  REQUIRE(wrapperEnd != std::string::npos);
  CHECK(samePathHelper < wrapperHelper);

  const std::string wrapperHelperSource =
      source.substr(wrapperHelper, wrapperEnd - wrapperHelper);
  CHECK(wrapperHelperSource.find("/std/collections/soa_vector/") !=
        std::string::npos);
  CHECK(wrapperHelperSource.find("/std/collections/experimental_soa_vector/soaVector") !=
        std::string::npos);
  CHECK(wrapperHelperSource.find(
            "/std/collections/experimental_soa_vector_conversions/soaVector") !=
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
  CHECK(source.find("rewrittenVectorCtor.name = \"/std/collections/experimental_vector/vector\";") !=
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
  CHECK(source.find("rewrittenVectorCtor.name = \"/std/collections/experimental_vector/vector\";") !=
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
  CHECK(statementsSource.find("path == \"/std/collections/experimental_vector/vector\"") !=
        std::string::npos);
  CHECK(collectionSource.find("path == \"/std/collections/experimental_vector/vector\"") !=
        std::string::npos);
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

  const size_t constructorSurfaceCheck = source.find(
      "if (!resolvePublishedLateCollectionMemberName(\n"
      "                  callExpr,\n"
      "                  primec::StdlibSurfaceId::CollectionsMapConstructors,\n"
      "                  constructorName) ||");
  const size_t resolveDefinitionCallPos =
      source.find("const Definition *callee = resolveDefinitionCall(callExpr);");

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
            "inferPublishedExperimentalMapStructPathFromConstructorPath(path)") !=
        std::string::npos);
  CHECK(accessTargetResolutionSource.find(
            "isPublishedStdlibSurfaceConstructorExpr(") !=
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
            "auto rewriteExplicitMapHelperBuiltinExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {") !=
        std::string::npos);
  CHECK(source.find("auto hasPublishedSemanticMapSurface = [&](const Expr &callExpr) {") !=
        std::string::npos);
  CHECK(source.find("auto resolvePublishedTailDispatchMapHelperName =") !=
        std::string::npos);
  CHECK(source.find("resolvePublishedSemanticStdlibSurfaceMemberName(") !=
        std::string::npos);
  CHECK(source.find("findSemanticProductDirectCallStdlibSurfaceId(semanticProgram, callExpr)") !=
        std::string::npos);
  CHECK(source.find("if (resolvePublishedTailDispatchMapHelperName(callExpr, helperNameOut)) {") !=
        std::string::npos);
  CHECK(source.find("if (callExpr.namespacePrefix.empty() &&\n"
                    "                    callExpr.name.find('/') == std::string::npos &&\n"
                    "                    (callExpr.name == \"count\" || callExpr.name == \"contains\" ||") !=
        std::string::npos);
  CHECK(source.find("if (candidate.isMethodCall) {\n"
                    "              std::string helperName;\n"
                    "              return resolveBuiltinMapHelperName(candidate, true, helperName) &&") !=
        std::string::npos);
  CHECK(source.find("if (!candidate.args.empty() &&\n"
                    "                candidate.namespacePrefix.empty() &&\n"
                    "                candidate.name.find('/') == std::string::npos &&\n"
                    "                (candidate.name == \"count\" || candidate.name == \"contains\" ||") !=
        std::string::npos);
  CHECK(source.find(
            "std::string helperName =\n                resolveTailDispatchDirectHelperPath(candidate);") !=
        std::string::npos);
  CHECK(source.find("if (!resolveBuiltinMapHelperName(callExpr, true, helperName) ||\n"
                    "                (helperName != \"insert\" && helperName != \"insert_ref\")) {") !=
        std::string::npos);
  CHECK(source.find(
            "if ((!ir_lowerer::resolveMapHelperAliasName(callExpr, helperName) ||\n"
            "                 (helperName != \"insert\" && helperName != \"insert_ref\")) &&") !=
        std::string::npos);
  CHECK(source.find("if (matchesPublishedMapInsertPath(callExpr)) {\n"
                    "            return false;\n"
                    "          }") ==
        std::string::npos);
  CHECK(source.find("if ((helperName == \"count\" || helperName == \"contains\" ||") !=
        std::string::npos);
  CHECK(source.find("helperName == \"tryAt\" || helperName == \"insert\" ||\n"
                    "               helperName == \"insert_ref\") &&") !=
        std::string::npos);
  CHECK(source.find("rawPath.rfind(\"/map/\", 0) == 0") != std::string::npos);
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

TEST_CASE("ir lowerer statement expr guards inline builtin map insert family") {
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

  CHECK(source.find("auto isBuiltinMapInsertFamilyPath = [&](const std::string &path) {") !=
        std::string::npos);
  CHECK(source.find("normalizedPath == \"/std/collections/map/insert_builtin\";") !=
        std::string::npos);
  CHECK(source.find("if (isBuiltinMapInsertFamilyPath(rawPath) ||\n"
                    "                isBuiltinMapInsertFamilyPath(directCallee->fullPath)) {") !=
        std::string::npos);
  CHECK(source.find("if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {") !=
        std::string::npos);
}

TEST_CASE("ir lowerer inline map insert builtin recovers typed bindings from caller and helper params") {
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
  CHECK(source.find("callerValuesIt->second.mapKeyKind != LocalInfo::ValueKind::Unknown") !=
        std::string::npos);
  CHECK(source.find("callee.parameters.size() >= 3") !=
        std::string::npos);
  CHECK(source.find("extractParameterTypeName(callee.parameters[1])") !=
        std::string::npos);
  CHECK(source.find("extractParameterTypeName(callee.parameters[2])") !=
        std::string::npos);
  CHECK(source.find("info.kind == LocalInfo::Kind::Value &&\n"
                    "                info.mapKeyKind != LocalInfo::ValueKind::Unknown &&\n"
                    "                info.mapValueKind != LocalInfo::ValueKind::Unknown") !=
        std::string::npos);
  CHECK(source.find("if (valuesIt->second.referenceToMap || valuesIt->second.pointerToMap) {") !=
        std::string::npos);
  CHECK(source.find("} else {\n"
                    "          ptrLocal = valuesIt->second.index;\n"
                    "        }") !=
        std::string::npos);
  CHECK(source.find("if (!isBuiltinCanonicalMapInsertCallee && receiverUsesExperimentalMapStruct) {") !=
        std::string::npos);
  CHECK(source.find("the builtin canonical insert path mutates the flat map") !=
        std::string::npos);
  CHECK(source.find("if (requireValue) {\n"
                    "        function.instructions.push_back({IrOpcode::PushI32, 0});\n"
                    "      }") !=
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

  CHECK(statementSource.find("if (targetInfo.isWrappedMapTarget ||") !=
        std::string::npos);
  CHECK(statementSource.find("isExperimentalMapStructPath(targetInfo.structTypeName)) {") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("if (targetInfo.isWrappedMapTarget ||") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("isExperimentalMapStructPath(targetInfo.structTypeName)) {") !=
        std::string::npos);
  CHECK(statementSource.find("structPath.rfind(\"/std/collections/experimental_map/Map__\", 0) == 0;") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("structPath.rfind(\"/std/collections/experimental_map/Map__\", 0) == 0;") !=
        std::string::npos);
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

  CHECK(emitExprSource.find("resolveMapHelperAliasName(expr, canonicalMapHelperName) &&") !=
        std::string::npos);
  CHECK(emitExprSource.find("resolveDefinitionCall(expr) == nullptr &&") != std::string::npos);
  CHECK(emitExprSource.find("canonicalMapHelperName == \"insert_ref\"") !=
        std::string::npos);
  CHECK(inferenceDispatchSource.find("resolveMapHelperAliasName(expr, canonicalMapHelperName) &&") !=
        std::string::npos);
  CHECK(inferenceDispatchSource.find(
            "(!stateInOut.resolveDefinitionCall || stateInOut.resolveDefinitionCall(expr) == nullptr) &&") !=
        std::string::npos);
  CHECK(inferenceDispatchSource.find("canonicalMapHelperName == \"insert_ref\"") !=
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

  CHECK(collectionHelpersSource.find("matchesResolvedPath(\"/std/collections/map/contains\")") !=
        std::string::npos);
  CHECK(collectionHelpersSource.find("matchesResolvedPath(\"/std/collections/map/tryAt\")") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("matchesGeneratedMapHelperPath(callExpr, \"/std/collections/map/contains\")") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("matchesGeneratedMapHelperPath(callExpr, \"/std/collections/map/tryAt\")") !=
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
  CHECK(source.find("if (directCallee == nullptr) {") !=
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
  CHECK(source.find("helperName == \"tryAt\" || helperName == \"insert\" ||\n"
                    "                 helperName == \"insert_ref\") &&") !=
        std::string::npos);
  CHECK(source.find("const size_t generatedSuffix = helperPath.find(\"__\");") ==
        std::string::npos);
  CHECK(source.find("const std::string specializedPrefix = rawPath + \"__t\";") ==
        std::string::npos);
  CHECK(source.find("const std::string overloadPrefix = rawPath + \"__ov\";") ==
        std::string::npos);
  CHECK(source.find("rawPath.rfind(\"/map/\", 0) == 0 ||") !=
        std::string::npos);
  CHECK(source.find("rawPath.rfind(\"/std/collections/experimental_map/\", 0) == 0) &&") !=
        std::string::npos);
  CHECK(source.find("directCallee = findDirectHelperDefinition(rawPath);") !=
        std::string::npos);
  CHECK(source.find("directCallee = findDirectHelperDefinition(resolveExprPath(expr));") !=
        std::string::npos);
  CHECK(source.find("if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {") !=
        std::string::npos);
  CHECK(source.find("rawPath.rfind(\"/std/collections/map/\", 0) == 0 &&") !=
        std::string::npos);
  CHECK(source.find("getBuiltinArrayAccessName(expr, accessName)") !=
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

  CHECK(source.find("const bool isExplicitCanonicalMapHelperPath =") !=
        std::string::npos);
  CHECK(source.find("rawPath.rfind(\"/std/collections/map/\", 0) == 0") !=
        std::string::npos);
  CHECK(source.find("helperName == \"contains\" || helperName == \"contains_ref\"") !=
        std::string::npos);
  CHECK(source.find("helperName == \"tryAt\" || helperName == \"tryAt_ref\"") !=
        std::string::npos);
  CHECK(source.find("expr.args.front().kind == Expr::Kind::Name") !=
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

  CHECK(source.find("auto resolveDirectMapHelperPath = [&](const Expr &exprIn) {") !=
        std::string::npos);
  CHECK(source.find("auto findDirectMapHelperDefinition = [&](const std::string &rawPath) -> const Definition * {") !=
        std::string::npos);
  CHECK(source.find("findDirectMapHelperDefinition(rawPath) != nullptr") !=
        std::string::npos);
  CHECK(source.find("exprIn.name = helperName;") !=
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
            "return resolveMapHelperAliasName(expr, helperName) &&\n"
            "         (helperName == \"count\" || helperName == \"contains\" ||\n"
            "          helperName == \"tryAt\" || helperName == \"at\" ||\n"
            "          helperName == \"at_unsafe\" || helperName == \"insert\" ||\n"
            "          helperName == \"insert_ref\");") !=
        std::string::npos);
}

TEST_SUITE_END();
