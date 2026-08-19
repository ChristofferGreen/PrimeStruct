#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer statement call emission source delegation stays stable") {
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
  const std::filesystem::path statementCallEmissionPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererStatementCallEmission.cpp";
  REQUIRE(std::filesystem::exists(statementCallEmissionPath));

  const std::string source = readText(statementCallEmissionPath);
  CHECK(source.find("#include \"primec/support/StdlibSurfaceRegistry.h\"") ==
        std::string::npos);
  CHECK(source.find("resolvePublishedStatementCallVectorHelperName(") ==
        std::string::npos);
  CHECK(source.find("resolveStatementCallPathWithoutFallbackProbes(") !=
        std::string::npos);
  CHECK(source.find("resolvePublishedStatementKeyValueHelperName(") ==
        std::string::npos);
  CHECK(source.find("resolvePublishedStatementMapHelperName(") ==
        std::string::npos);
  CHECK(source.find("resolvePublishedStdlibSurfaceExprMemberName(") ==
        std::string::npos);
  CHECK(source.find("const std::string rawPath =\n        resolveStatementCallPathWithoutFallbackProbes(callExpr);") !=
        std::string::npos);
  CHECK(source.find("std::string normalizedName =\n        resolveStatementCallPathWithoutFallbackProbes(stmt);") ==
        std::string::npos);
  CHECK(source.find("std::string directName = resolveStatementCallPathWithoutFallbackProbes(expr);") ==
        std::string::npos);
  CHECK(source.find("std::string methodName = resolveStatementCallPathWithoutFallbackProbes(expr);") ==
        std::string::npos);
  CHECK(source.find("findStdlibSurfaceMetadataByResolvedPath(resolvedPath)") ==
        std::string::npos);
  CHECK(source.find("const auto *vectorMetadata = statementCallVectorHelperMetadata();") ==
        std::string::npos);
  CHECK(source.find("metadata->id != vectorMetadata->id") ==
        std::string::npos);
  CHECK(source.find("const auto *keyValueMetadata = statementKeyValueHelperMetadata();") ==
        std::string::npos);
  CHECK(source.find("metadata->id != keyValueMetadata->id") ==
        std::string::npos);
  CHECK(source.find("matchesRegistrySpellingSet(metadata->loweringSpellings, resolvedPath)") ==
        std::string::npos);
  CHECK(source.find("isPublishedWrapperStatementVectorMutatorAliasPath(") ==
        std::string::npos);
  CHECK(source.find("explicitVectorMutatorHelperCall && !explicitWrapperVectorMutatorHelperPath") ==
        std::string::npos);
  CHECK(source.find("explicitVectorHelperUsesBuiltinVectorReceiver") ==
        std::string::npos);
  CHECK(source.find("tryEmitVectorHelperCallFormStatement(") ==
        std::string::npos);
  CHECK(source.find("rewriteBareVectorMethodMutatorToDirectCall") ==
        std::string::npos);
  CHECK(source.find("emitExperimentalVectorHeaderSetter") == std::string::npos);
  CHECK(source.find("resolveStatementExperimentalVectorReceiverFromSemanticFacts") ==
        std::string::npos);
  CHECK(source.find("return resolvePublishedStatementCallVectorHelperName(expr.name, helperNameOut);") ==
        std::string::npos);
  CHECK(source.find("std::string helperName = callExpr.name;") ==
        std::string::npos);
  CHECK(source.find("std::string normalizedName = stmt.name;") ==
        std::string::npos);
  CHECK(source.find("std::string directName = expr.name;") ==
        std::string::npos);
  CHECK(source.find("std::string methodName = expr.name;") ==
        std::string::npos);
  CHECK(source.find("const std::string experimentalVectorPrefix = \"std/collections/experimental_vector/\"") ==
        std::string::npos);
  CHECK(source.find("expr.name == \"/map/at\" || expr.name == \"/std/collections/map/at\"") ==
        std::string::npos);
  CHECK(source.find("callee.fullPath == \"/std/collections/map/insert\"") ==
        std::string::npos);
}


TEST_SUITE_END();
