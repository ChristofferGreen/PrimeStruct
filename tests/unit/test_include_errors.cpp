#include "primec/IncludeResolver.h"

#include "third_party/doctest.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {
std::string writeTemp(const std::string &name, const std::string &contents) {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests";
  std::filesystem::create_directories(dir);
  auto path = dir / name;
  std::ofstream file(path);
  CHECK(file.good());
  file << contents;
  CHECK(file.good());
  return path.string();
}

std::string writeFile(const std::filesystem::path &path, const std::string &contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path);
  CHECK(file.good());
  file << contents;
  CHECK(file.good());
  return path.string();
}
} // namespace

TEST_SUITE_BEGIN("primestruct.includes.errors");

TEST_CASE("unterminated include fails") {
  const std::string srcPath = writeTemp("main_bad_include.prime", "include<\"/tmp/missing.prime\"\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("unterminated include") != std::string::npos);
}

TEST_CASE("missing include path fails") {
  const std::string srcPath = writeTemp("main_missing_path.prime", "include<version=\"1.2.3\">\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("requires at least one quoted path") != std::string::npos);
}

TEST_CASE("invalid include version fails") {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests" / "include_bad_version";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string srcPath =
      writeFile(dir / "main_bad_version.prime",
                "include<\"/lib.prime\", version=\"1.x\">\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("invalid include version") != std::string::npos);
}

TEST_CASE("missing include version directory fails") {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests" / "include_missing_version";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string srcPath =
      writeFile(dir / "main_missing_version.prime",
                "include<\"/lib.prime\", version=\"2.0.0\">\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("include version not found") != std::string::npos);
}

TEST_CASE("include version mismatch fails") {
  auto rootA = std::filesystem::temp_directory_path() / "primec_tests" / "include_version_root_a";
  auto rootB = std::filesystem::temp_directory_path() / "primec_tests" / "include_version_root_b";
  std::filesystem::remove_all(rootA);
  std::filesystem::remove_all(rootB);
  std::filesystem::create_directories(rootA);
  std::filesystem::create_directories(rootB);

  writeFile(rootA / "1.2.1" / "lib_a.prime", "// LIB_A\n");
  writeFile(rootB / "1.2.0" / "lib_b.prime", "// LIB_B\n");
  const std::string srcPath =
      writeFile(rootA / "main_version_mismatch.prime",
                "include<\"/lib_a.prime\", \"/lib_b.prime\", version=\"1.2\">\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error, {rootA.string(), rootB.string()}));
  CHECK(error.find("include version mismatch") != std::string::npos);
}

TEST_CASE("private include path fails") {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests" / "include_private_dir";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::filesystem::path privateFile = dir / "_private" / "lib.prime";
  writeFile(privateFile, "// PRIVATE\n");
  const std::string srcPath =
      writeFile(dir / "main_private.prime", "include<\"" + privateFile.string() + "\">\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("private folder") != std::string::npos);
}

TEST_CASE("private include path fails from include root") {
  auto root = std::filesystem::temp_directory_path() / "primec_tests" / "include_private_root";
  auto baseDir = std::filesystem::temp_directory_path() / "primec_tests" / "include_private_base";
  std::filesystem::remove_all(root);
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(root);
  std::filesystem::create_directories(baseDir);

  const std::filesystem::path privateFile = root / "_private" / "lib.prime";
  writeFile(privateFile, "// PRIVATE\n");
  const std::string srcPath = writeFile(baseDir / "main.prime", "include<\"_private/lib.prime\">\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error, {root.string()}));
  CHECK(error.find("private folder") != std::string::npos);
}

TEST_SUITE_END();
