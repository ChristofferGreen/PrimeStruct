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

TEST_SUITE_END();
