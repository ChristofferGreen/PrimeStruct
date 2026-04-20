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
  CHECK(source.find("if (resolveDefinitionCall(callExpr) != nullptr)") != std::string::npos);
  CHECK(source.find("rewrittenExpr.name = helperName;") != std::string::npos);
}

TEST_SUITE_END();
