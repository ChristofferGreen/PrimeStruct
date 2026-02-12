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

TEST_CASE("unterminated include with whitespace fails") {
  const std::string srcPath = writeTemp("main_bad_include_ws.prime", "include < \"/tmp/missing.prime\"\n");
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
  CHECK(error.find("requires at least one path") != std::string::npos);
}

TEST_CASE("include path suffix fails") {
  const std::string srcPath = writeTemp("main_include_suffix.prime", "include<\"/lib.prime\"utf8>\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("suffix") != std::string::npos);
}

TEST_CASE("include path suffix with single quotes fails") {
  const std::string srcPath = writeTemp("main_include_suffix_single.prime", "include<'/lib.prime'utf8>\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("suffix") != std::string::npos);
}

TEST_CASE("include entry missing comma fails") {
  const std::string srcPath =
      writeTemp("main_include_missing_comma.prime", "include<\"/lib_a.prime\" \"/lib_b.prime\">\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("expected ',' between include entries") != std::string::npos);
}

TEST_CASE("include entry with semicolon fails") {
  const std::string srcPath =
      writeTemp("main_include_semicolon.prime", "include<\"/lib_a.prime\"; \"/lib_b.prime\">\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("semicolon") != std::string::npos);
}

TEST_CASE("include version with semicolon fails") {
  const std::string srcPath =
      writeTemp("main_include_semicolon_version.prime", "include<\"/lib.prime\", version=\"1.2\";>\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("semicolon") != std::string::npos);
}

TEST_CASE("include version with semicolon and single quotes fails") {
  const std::string srcPath =
      writeTemp("main_include_semicolon_version_single.prime", "include<'/lib.prime', version='1.2';>\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("semicolon") != std::string::npos);
}

TEST_CASE("include version with trailing junk fails") {
  const std::string srcPath =
      writeTemp("main_include_version_trailing.prime", "include<\"/lib.prime\", version=\"1.2\"x>\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("unexpected characters after include version") != std::string::npos);
}

TEST_CASE("include version with trailing junk and single quotes fails") {
  const std::string srcPath =
      writeTemp("main_include_version_trailing_single.prime", "include<'/lib.prime', version='1.2'x>\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("unexpected characters after include version") != std::string::npos);
}

TEST_CASE("duplicate include version attribute fails") {
  const std::string srcPath =
      writeTemp("main_include_duplicate_version.prime", "include<\"/lib.prime\", version=\"1.2\", version=\"1.3\">\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("duplicate version attribute in include<...>") != std::string::npos);
}

TEST_CASE("duplicate include version attribute with single quotes fails") {
  const std::string srcPath =
      writeTemp("main_include_duplicate_version_single.prime", "include<'/lib.prime', version='1.2', version='1.3'>\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("duplicate version attribute in include<...>") != std::string::npos);
}

TEST_CASE("unquoted non-slash include path fails") {
  const std::string srcPath = writeTemp("main_include_bare_relative.prime", "include<lib.prime>\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("unquoted include paths must be slash paths") != std::string::npos);
}

TEST_CASE("unquoted include path with invalid segment fails") {
  const std::string srcPath = writeTemp("main_include_bad_segment.prime", "include</std//io>\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("invalid slash path identifier") != std::string::npos);
}

TEST_CASE("unquoted include path with dot fails") {
  const std::string srcPath = writeTemp("main_include_dot_segment.prime", "include</lib.prime>\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("invalid slash path identifier") != std::string::npos);
}

TEST_CASE("unquoted include path with reserved keyword fails") {
  const std::string srcPath = writeTemp("main_include_reserved_segment.prime", "include</return/io>\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("reserved keyword") != std::string::npos);
}

TEST_CASE("unquoted include path with import keyword fails") {
  const std::string srcPath = writeTemp("main_include_import_keyword.prime", "include</import/io>\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("reserved keyword") != std::string::npos);
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

TEST_CASE("empty include version fails") {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests" / "include_empty_version";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string srcPath =
      writeFile(dir / "main_empty_version.prime",
                "include<\"/lib.prime\", version=\"\">\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("include version cannot be empty") != std::string::npos);
}

TEST_CASE("invalid include version with single quotes fails") {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests" / "include_bad_version_single";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string srcPath =
      writeFile(dir / "main_bad_version_single.prime",
                "include<'/lib.prime', version='1.x'>\n");
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

TEST_CASE("missing include minor version fails") {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests" / "include_missing_version_minor";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  writeFile(dir / "1.1.9" / "lib.prime", "// LIB\n");
  writeFile(dir / "1.3.0" / "lib.prime", "// LIB\n");
  const std::string srcPath =
      writeFile(dir / "main_missing_version_minor.prime",
                "include<\"/lib.prime\", version=\"1.2\">\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("include version not found") != std::string::npos);
}

TEST_CASE("missing include minor version with single quotes fails") {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests" / "include_missing_version_minor_single";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  writeFile(dir / "1.1.9" / "lib.prime", "// LIB\n");
  writeFile(dir / "1.3.0" / "lib.prime", "// LIB\n");
  const std::string srcPath =
      writeFile(dir / "main_missing_version_minor_single.prime",
                "include<'/lib.prime', version='1.2'>\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("include version not found") != std::string::npos);
}

TEST_CASE("missing include major version fails") {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests" / "include_missing_version_major";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string srcPath =
      writeFile(dir / "main_missing_version_major.prime",
                "include<\"/lib.prime\", version=\"1\">\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("include version not found") != std::string::npos);
}

TEST_CASE("missing include major version with single quotes fails") {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests" / "include_missing_version_major_single";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string srcPath =
      writeFile(dir / "main_missing_version_major_single.prime",
                "include<'/lib.prime', version='1'>\n");
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
