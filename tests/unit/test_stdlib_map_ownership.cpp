#include "third_party/doctest.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::filesystem::path repoRoot() {
  const auto cwd = std::filesystem::current_path();
  const auto marker = std::filesystem::path("stdlib") / "std" / "collections" / "map.prime";
  if (std::filesystem::exists(cwd / marker)) {
    return cwd;
  }
  if (std::filesystem::exists(cwd.parent_path() / marker)) {
    return cwd.parent_path();
  }
  return cwd;
}

std::string readText(const std::filesystem::path &path) {
  std::ifstream input(path);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::filesystem::path collectionsFile(const std::string &name) {
  return repoRoot() / "stdlib" / "std" / "collections" / name;
}

} // namespace

TEST_SUITE_BEGIN("primestruct.stdlib.map_ownership");

TEST_CASE("canonical map surface owns implementation through internal map module") {
  const std::string mapSource = readText(collectionsFile("map.prime"));
  const std::string experimentalSource = readText(collectionsFile("experimental_map.prime"));
  const std::string internalSource = readText(collectionsFile("internal_map.prime"));
  const std::string semanticsSource = readText(repoRoot() / "src" / "semantics" / "SemanticsValidate.cpp");
  const std::string statementLowererSource =
      readText(repoRoot() / "src" / "ir_lowerer" / "IrLowererStatementCallEmission.cpp");

  REQUIRE(!mapSource.empty());
  REQUIRE(!experimentalSource.empty());
  REQUIRE(!internalSource.empty());
  REQUIRE(!semanticsSource.empty());
  REQUIRE(!statementLowererSource.empty());

  CHECK(mapSource.find("import /std/collections/internal_map/*") != std::string::npos);
  CHECK(mapSource.find("/std/collections/experimental_map/") == std::string::npos);
  CHECK(mapSource.find("/std/collections/internal_map/insertImpl") != std::string::npos);
  CHECK(mapSource.find("Reference<Map<K, V>>") == std::string::npos);
  CHECK(mapSource.find("Reference<map<K, V>>") != std::string::npos);

  CHECK(experimentalSource.find("import /std/collections/internal_map/*") != std::string::npos);
  CHECK(experimentalSource.find("mapFindIndex") == std::string::npos);
  CHECK(experimentalSource.find("mapOverwriteSlot") == std::string::npos);
  CHECK(experimentalSource.find("[public struct]") == std::string::npos);

  CHECK(internalSource.find("mapFindIndex") != std::string::npos);
  CHECK(internalSource.find("mapOverwriteSlot") != std::string::npos);
  const auto internalNamespacePos = internalSource.find("namespace internal_map");
  CHECK(internalNamespacePos != std::string::npos);
  CHECK(internalSource.find("namespace experimental_map") != std::string::npos);
  CHECK(internalSource.find("insertImpl<K, V>") != std::string::npos);

  REQUIRE(internalNamespacePos != std::string::npos);
  const std::string internalNamespace = internalSource.substr(internalNamespacePos);
  CHECK(internalNamespace.find("findIndexImpl<K, V>") != std::string::npos);
  CHECK(internalNamespace.find("borrowedFindIndexImpl<K, V>") != std::string::npos);
  CHECK(internalNamespace.find("overwriteSlotImpl<V>") != std::string::npos);
  CHECK(internalNamespace.find("containerErrorResult<V>(containerMissingKey())") != std::string::npos);
  CHECK(internalNamespace.find("/std/collections/experimental_map/mapContains") == std::string::npos);
  CHECK(internalNamespace.find("/std/collections/experimental_map/mapTryAt") == std::string::npos);
  CHECK(internalNamespace.find("/std/collections/experimental_map/mapAt") == std::string::npos);
  CHECK(internalNamespace.find("/std/collections/experimental_map/mapInsert") == std::string::npos);

  CHECK(semanticsSource.find("path == \"mapSingle\"") != std::string::npos);
  CHECK(semanticsSource.find("constructorBackedBuiltinMapBindings.count(initializer.args.front().name)") !=
        std::string::npos);
  CHECK(statementLowererSource.find("isPrimeMapInsertBody") != std::string::npos);
  CHECK(statementLowererSource.find("callee->fullPath.rfind(\"/std/collections/internal_map/insertImpl__\", 0)") !=
        std::string::npos);
}

TEST_SUITE_END();
