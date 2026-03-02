#include "primec/ImportResolver.h"

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

TEST_CASE("unterminated import fails") {
  const std::string srcPath = writeTemp("main_bad_include.prime", "import<\"/tmp/missing.prime\"\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("unterminated import") != std::string::npos);
}

TEST_CASE("unterminated import with whitespace fails") {
  const std::string srcPath = writeTemp("main_bad_include_ws.prime", "import < \"/tmp/missing.prime\"\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("unterminated import") != std::string::npos);
}

TEST_CASE("missing import path fails") {
  const std::string srcPath = writeTemp("main_missing_path.prime", "import<version=\"1.2.3\">\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("requires at least one path") != std::string::npos);
}

TEST_CASE("import path suffix fails") {
  const std::string srcPath = writeTemp("main_include_suffix.prime", "import<\"/lib.prime\"utf8>\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("suffix") != std::string::npos);
}

TEST_CASE("import path suffix before version fails") {
  const std::string srcPath =
      writeTemp("main_include_suffix_before_version.prime",
                "import<\"/lib.prime\"version=\"1.2\">\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("import path cannot have suffix") != std::string::npos);
}

TEST_CASE("import path suffix with single quotes fails") {
  const std::string srcPath = writeTemp("main_include_suffix_single.prime", "import<'/lib.prime'utf8>\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("suffix") != std::string::npos);
}

TEST_CASE("import version with trailing junk fails") {
  const std::string srcPath =
      writeTemp("main_include_version_trailing.prime", "import<\"/lib.prime\", version=\"1.2\"x>\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("unexpected characters after import version") != std::string::npos);
}

TEST_CASE("import version missing equals fails") {
  const std::string srcPath =
      writeTemp("main_include_version_missing_equals.prime", "import<\"/lib.prime\", version \"1.2\">\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("expected '=' after import version") != std::string::npos);
}

TEST_CASE("import version requires quoted string") {
  const std::string srcPath =
      writeTemp("main_include_version_unquoted.prime", "import<\"/lib.prime\", version=1.2>\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("expected quoted string in import<...>") != std::string::npos);
}

TEST_CASE("unterminated import string literal fails") {
  const std::string srcPath =
      writeTemp("main_include_unterminated_string.prime", "import<\"/tmp/missing.prime>\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("unterminated import string literal") != std::string::npos);
}

TEST_CASE("import version with trailing junk and single quotes fails") {
  const std::string srcPath =
      writeTemp("main_include_version_trailing_single.prime", "import<'/lib.prime', version='1.2'x>\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("unexpected characters after import version") != std::string::npos);
}

TEST_CASE("duplicate import version attribute fails") {
  const std::string srcPath =
      writeTemp("main_include_duplicate_version.prime", "import<\"/lib.prime\", version=\"1.2\", version=\"1.3\">\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("duplicate version attribute in import<...>") != std::string::npos);
}

TEST_CASE("duplicate import version attribute with single quotes fails") {
  const std::string srcPath =
      writeTemp("main_include_duplicate_version_single.prime", "import<'/lib.prime', version='1.2', version='1.3'>\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("duplicate version attribute in import<...>") != std::string::npos);
}

TEST_CASE("unquoted non-slash import path fails") {
  const std::string srcPath = writeTemp("main_include_bare_relative.prime", "import<lib.prime>\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("unquoted import paths must be slash paths") != std::string::npos);
}

TEST_CASE("unquoted import path with invalid segment fails") {
  const std::string srcPath = writeTemp("main_include_bad_segment.prime", "import</std//io>\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("invalid slash path identifier") != std::string::npos);
}

TEST_CASE("unquoted import path with trailing slash fails") {
  const std::string srcPath = writeTemp("main_include_trailing_slash.prime", "import</std/io/>\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("invalid slash path identifier") != std::string::npos);
}

TEST_CASE("unquoted import path with digit segment fails") {
  const std::string srcPath = writeTemp("main_include_digit_segment.prime", "import</1std/io>\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("invalid slash path identifier") != std::string::npos);
}

TEST_CASE("unquoted import path with dot fails") {
  const std::string srcPath = writeTemp("main_include_dot_segment.prime", "import</lib.prime>\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("invalid slash path identifier") != std::string::npos);
}

TEST_CASE("import version mismatch across paths fails") {
  auto tempRoot = std::filesystem::temp_directory_path() / "primec_tests";
  auto rootA = tempRoot / "include_versions_a";
  auto rootB = tempRoot / "include_versions_b";
  writeFile(rootA / "1.2.0" / "lib" / "a" / "module.prime", "// LIB_A\n");
  writeFile(rootB / "1.2.1" / "lib" / "b" / "module.prime", "// LIB_B\n");

  const std::string srcPath =
      writeTemp("main_include_version_mismatch.prime",
                "import</lib/a, /lib/b, version=\"1.2\">\n[return<int>]\nmain(){ return(1i32) }\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error, {rootA.string(), rootB.string()}));
  CHECK(error.find("import version mismatch") != std::string::npos);
}

TEST_CASE("unquoted import path with reserved keyword fails") {
  const std::string srcPath = writeTemp("main_include_reserved_segment.prime", "import</if/io>\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("reserved keyword") != std::string::npos);
}

TEST_CASE("unquoted import path with import keyword fails") {
  const std::string srcPath = writeTemp("main_include_import_keyword.prime", "import</import/io>\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("reserved keyword") != std::string::npos);
}

TEST_CASE("invalid import version fails") {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests" / "include_bad_version";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string srcPath =
      writeFile(dir / "main_bad_version.prime",
                "import<\"/lib.prime\", version=\"1.x\">\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("invalid import version") != std::string::npos);
}

TEST_CASE("import version with too many parts fails") {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests" / "include_version_too_many_parts";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string srcPath =
      writeFile(dir / "main_version_too_many_parts.prime",
                "import<\"/lib.prime\", version=\"1.2.3.4\">\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("import version must have 1 to 3 numeric parts") != std::string::npos);
}

TEST_CASE("empty import version fails") {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests" / "include_empty_version";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string srcPath =
      writeFile(dir / "main_empty_version.prime",
                "import<\"/lib.prime\", version=\"\">\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("import version cannot be empty") != std::string::npos);
}

TEST_CASE("invalid import version with single quotes fails") {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests" / "include_bad_version_single";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string srcPath =
      writeFile(dir / "main_bad_version_single.prime",
                "import<'/lib.prime', version='1.x'>\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("invalid import version") != std::string::npos);
}

TEST_CASE("missing import version directory fails") {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests" / "include_missing_version";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string srcPath =
      writeFile(dir / "main_missing_version.prime",
                "import<\"/lib.prime\", version=\"2.0.0\">\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("import version not found") != std::string::npos);
}

TEST_CASE("missing import minor version fails") {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests" / "include_missing_version_minor";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  writeFile(dir / "1.1.9" / "lib.prime", "// LIB\n");
  writeFile(dir / "1.3.0" / "lib.prime", "// LIB\n");
  const std::string srcPath =
      writeFile(dir / "main_missing_version_minor.prime",
                "import<\"/lib.prime\", version=\"1.2\">\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("import version not found") != std::string::npos);
}

TEST_CASE("missing import minor version with single quotes fails") {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests" / "include_missing_version_minor_single";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  writeFile(dir / "1.1.9" / "lib.prime", "// LIB\n");
  writeFile(dir / "1.3.0" / "lib.prime", "// LIB\n");
  const std::string srcPath =
      writeFile(dir / "main_missing_version_minor_single.prime",
                "import<'/lib.prime', version='1.2'>\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("import version not found") != std::string::npos);
}

TEST_CASE("missing import major version fails") {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests" / "include_missing_version_major";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string srcPath =
      writeFile(dir / "main_missing_version_major.prime",
                "import<\"/lib.prime\", version=\"1\">\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("import version not found") != std::string::npos);
}

TEST_CASE("missing import major version with single quotes fails") {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests" / "include_missing_version_major_single";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string srcPath =
      writeFile(dir / "main_missing_version_major_single.prime",
                "import<'/lib.prime', version='1'>\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("import version not found") != std::string::npos);
}

TEST_CASE("absolute versioned import without roots fails") {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests" / "include_version_abs_no_root";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string srcPath =
      writeFile(dir / "main_abs_no_root.prime", "import<\"/tmp/lib.prime\", version=\"1.2\">\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("import version not found") != std::string::npos);
}

TEST_CASE("import version mismatch fails") {
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
                "import<\"/lib_a.prime\", \"/lib_b.prime\", version=\"1.2\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error, {rootA.string(), rootB.string()}));
  CHECK(error.find("import version mismatch") != std::string::npos);
}

TEST_CASE("private import path fails") {
  auto dir = std::filesystem::temp_directory_path() / "primec_tests" / "include_private_dir";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::filesystem::path privateFile = dir / "_private" / "lib.prime";
  writeFile(privateFile, "// PRIVATE\n");
  const std::string srcPath =
      writeFile(dir / "main_private.prime", "import<\"" + privateFile.string() + "\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("private folder") != std::string::npos);
}

TEST_CASE("private import path fails from import root") {
  auto root = std::filesystem::temp_directory_path() / "primec_tests" / "include_private_root";
  auto baseDir = std::filesystem::temp_directory_path() / "primec_tests" / "include_private_base";
  std::filesystem::remove_all(root);
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(root);
  std::filesystem::create_directories(baseDir);

  const std::filesystem::path privateFile = root / "_private" / "lib.prime";
  writeFile(privateFile, "// PRIVATE\n");
  const std::string srcPath = writeFile(baseDir / "main.prime", "import<\"_private/lib.prime\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error, {root.string()}));
  CHECK(error.find("private folder") != std::string::npos);
}

TEST_CASE("import directory without prime files fails") {
  auto baseDir = std::filesystem::temp_directory_path() / "primec_tests" / "include_empty_base";
  auto includeDir = std::filesystem::temp_directory_path() / "primec_tests" / "include_empty_dir";
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeDir);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeDir);
  writeFile(includeDir / "readme.txt", "noop\n");

  const std::string srcPath =
      writeFile(baseDir / "main.prime", "import<\"" + includeDir.string() + "\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("import directory contains no .prime files") != std::string::npos);
}

TEST_CASE("logical import version requires import roots") {
  auto baseDir = std::filesystem::temp_directory_path() / "primec_tests" / "include_logical_version_empty";
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "import</lib, version=\"1\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("import version not found") != std::string::npos);
}

TEST_CASE("versioned import fails when file missing") {
  auto baseDir = std::filesystem::temp_directory_path() / "primec_tests" / "include_version_missing_file_base";
  auto includeRoot = std::filesystem::temp_directory_path() / "primec_tests" / "include_version_missing_file_root";
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot / "1.2.0");

  const std::string srcPath =
      writeFile(baseDir / "main.prime", "import<\"/lib.prime\", version=\"1.2\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.find("failed to read import") != std::string::npos);
}

TEST_SUITE_END();
