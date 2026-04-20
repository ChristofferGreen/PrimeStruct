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
  CHECK(source.find("resolveMaterializedCollectionHelperName(callExpr, helperName)") !=
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

  CHECK(source.find("primec::StdlibSurfaceId::CollectionsMapConstructors") !=
        std::string::npos);
  CHECK(source.find("primec::StdlibSurfaceId::CollectionsVectorConstructors") !=
        std::string::npos);
  CHECK(source.find("resolvePublishedLateCollectionMemberName(") !=
        std::string::npos);
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
  CHECK(source.find("if ((helperName == \"count\" || helperName == \"contains\" ||") !=
        std::string::npos);
  CHECK(source.find("rawPath.rfind(\"/map/\", 0) == 0") != std::string::npos);
  CHECK(source.find("resolveDefinitionCall(callExpr) != nullptr") != std::string::npos);
  CHECK(source.find("rewrittenExpr.name = helperName;") != std::string::npos);
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
  CHECK(inferenceDispatchSource.find("resolveMapHelperAliasName(expr, canonicalMapHelperName) &&") !=
        std::string::npos);
  CHECK(inferenceDispatchSource.find(
            "(!stateInOut.resolveDefinitionCall || stateInOut.resolveDefinitionCall(expr) == nullptr) &&") !=
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

  CHECK(source.find("auto isDirectHelperDefinitionFamily = [&](const std::string &rawPath,") !=
        std::string::npos);
  CHECK(source.find("auto findDirectHelperDefinition = [&](const std::string &rawPath) -> const Definition * {") !=
        std::string::npos);
  CHECK(source.find("(helperName == \"count\" || helperName == \"contains\" ||") !=
        std::string::npos);
  CHECK(source.find("rawPath.rfind(\"/map/\", 0) == 0 ||") !=
        std::string::npos);
  CHECK(source.find("directCallee = findDirectHelperDefinition(rawPath);") !=
        std::string::npos);
  CHECK(source.find("if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {") !=
        std::string::npos);
  CHECK(source.find("rawPath.rfind(\"/std/collections/map/\", 0) == 0 &&") !=
        std::string::npos);
  CHECK(source.find("getBuiltinArrayAccessName(expr, accessName)") !=
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

TEST_SUITE_END();
