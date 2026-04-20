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
  CHECK(source.find("auto isRootedAliasSamePathCountLikeCall = [&](const Expr &candidate) {") !=
        std::string::npos);
  CHECK(source.find("if (helperName != \"insert\" && isRootedAliasSamePathCountLikeCall(callExpr)) {") !=
        std::string::npos);
  CHECK(source.find("resolveDirectHelperPath(callExpr).rfind(\"/std/collections/experimental_map/\", 0) == 0") !=
        std::string::npos);
  CHECK(source.find("if (resolveDefinitionCall(callExpr) != nullptr)") != std::string::npos);
  CHECK(source.find("rewrittenExpr.name = helperName;") != std::string::npos);
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

TEST_SUITE_END();
