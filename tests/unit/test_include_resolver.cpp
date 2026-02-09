#include "primec/IncludeResolver.h"

#include "third_party/doctest.h"

#include <filesystem>
#include <fstream>
#include <cstdlib>
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

std::string quoteShellArg(const std::string &value) {
  std::string quoted = "'";
  for (char c : value) {
    if (c == '\'') {
      quoted += "'\\''";
    } else {
      quoted += c;
    }
  }
  quoted += "'";
  return quoted;
}

bool runCommand(const std::string &command) {
  return std::system(command.c_str()) == 0;
}

bool hasZipTools() {
  return runCommand("zip -v > /dev/null 2>&1") && runCommand("unzip -v > /dev/null 2>&1");
}

bool createZip(const std::filesystem::path &zipPath, const std::filesystem::path &sourceDir) {
  std::string command = "cd " + quoteShellArg(sourceDir.string()) + " && zip -q -r " +
                        quoteShellArg(zipPath.string()) + " .";
  return runCommand(command);
}
} // namespace

TEST_SUITE_BEGIN("primestruct.includes");

TEST_CASE("expands single include") {
  const std::string libPath = writeTemp("lib_a.prime", "[return<int>]\nhelper(){ return(3i32) }\n");
  const std::string srcPath = writeTemp("main_a.prime", "include<\"" + libPath + "\">\n[return<int>]\nmain(){ return(helper()) }\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("helper") != std::string::npos);
}

TEST_CASE("expands include with whitespace") {
  const std::string marker = "LIB_WS_MARKER";
  const std::string libPath = writeTemp("lib_ws.prime", "// " + marker + "\n");
  const std::string srcPath = writeTemp("main_ws.prime", "include  <  \"" + libPath + "\"  >\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find(marker) != std::string::npos);
}

TEST_CASE("expands include with bare slash path") {
  auto baseDir = std::filesystem::temp_directory_path() / "primec_tests" / "include_bare_path_base";
  auto includeRoot = std::filesystem::temp_directory_path() / "primec_tests" / "include_bare_path_root";
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "lib.prime", "// INCLUDE_BARE_PATH\n");
  const std::string srcPath = writeFile(baseDir / "main.prime", "include</lib.prime>\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK(resolver.expandIncludes(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_BARE_PATH") != std::string::npos);
}

TEST_CASE("resolves versioned include with single quotes") {
  auto baseDir = std::filesystem::temp_directory_path() / "primec_tests" / "include_single_quote_base";
  auto includeRoot = std::filesystem::temp_directory_path() / "primec_tests" / "include_single_quote_root";
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "1.2.0" / "lib.prime", "// INCLUDE_SINGLE_QUOTE_120\n");
  writeFile(includeRoot / "1.2.3" / "lib.prime", "// INCLUDE_SINGLE_QUOTE_123\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "include < '/lib.prime', version = '1.2' >\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK(resolver.expandIncludes(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_SINGLE_QUOTE_123") != std::string::npos);
  CHECK(source.find("INCLUDE_SINGLE_QUOTE_120") == std::string::npos);
}

TEST_CASE("ignores duplicate includes") {
  const std::string marker = "LIB_B_MARKER";
  const std::string libPath = writeTemp("lib_b.prime",
                                        "// " + marker + "\n"
                                        "[return<int>]\nhelper(){ return(5i32) }\n");
  const std::string srcPath = writeTemp("main_b.prime",
                                        "include<\"" + libPath + "\">\n"
                                        "include<\"" + libPath + "\">\n"
                                        "[return<int>]\nmain(){ return(helper()) }\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.empty());
  const auto first = source.find(marker);
  const auto second = source.find(marker, first == std::string::npos ? 0 : first + 1);
  CHECK(first != std::string::npos);
  CHECK(second == std::string::npos);
}

TEST_CASE("missing include fails") {
  const std::string srcPath = writeTemp("main_c.prime", "include<\"/tmp/does_not_exist.prime\">\n");
  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("failed to read include") != std::string::npos);
}

TEST_CASE("ignores include directives inside string literals") {
  const std::string srcPath =
      writeTemp("include_in_string.prime",
                "[return<int>]\n"
                "main() {\n"
                "  print_line(\"before include<\\\"/tmp/does_not_exist.prime\\\"> after\"utf8)\n"
                "  return(0i32)\n"
                "}\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("include<\\\"/tmp/does_not_exist.prime\\\">") != std::string::npos);
}

TEST_CASE("ignores include directives inside comments") {
  const std::string srcPath =
      writeTemp("include_in_comment.prime",
                "// include<\"/tmp/does_not_exist.prime\">\n"
                "[return<int>]\n"
                "main(){ return(0i32) }\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("include<\"/tmp/does_not_exist.prime\">") != std::string::npos);
}

TEST_CASE("resolves include from include path") {
  auto baseDir = std::filesystem::temp_directory_path() / "primec_tests" / "include_search_base";
  auto includeRoot = std::filesystem::temp_directory_path() / "primec_tests" / "include_search_root";
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "lib.prime", "// INCLUDE_ROOT_MARKER\n");
  const std::string srcPath = writeFile(baseDir / "main.prime", "include<\"/lib.prime\">\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK(resolver.expandIncludes(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_ROOT_MARKER") != std::string::npos);
}

TEST_CASE("resolves relative include from include path") {
  auto baseDir = std::filesystem::temp_directory_path() / "primec_tests" / "include_relative_base";
  auto includeRoot = std::filesystem::temp_directory_path() / "primec_tests" / "include_relative_root";
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "lib.prime", "// INCLUDE_RELATIVE_MARKER\n");
  const std::string srcPath = writeFile(baseDir / "main.prime", "include<\"lib.prime\">\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK(resolver.expandIncludes(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_RELATIVE_MARKER") != std::string::npos);
}

TEST_CASE("resolves versioned absolute include from include path") {
  auto baseDir = std::filesystem::temp_directory_path() / "primec_tests" / "include_version_abs_base";
  auto includeRoot = std::filesystem::temp_directory_path() / "primec_tests" / "include_version_abs_root";
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "1.2.0" / "lib.prime", "// INCLUDE_VERSION_ABS_120\n");
  writeFile(includeRoot / "1.2.9" / "lib.prime", "// INCLUDE_VERSION_ABS_129\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "include<\"/lib.prime\", version=\"1.2\">\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK(resolver.expandIncludes(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_VERSION_ABS_129") != std::string::npos);
  CHECK(source.find("INCLUDE_VERSION_ABS_120") == std::string::npos);
}

TEST_CASE("selects newest include version across roots") {
  auto baseDir = std::filesystem::temp_directory_path() / "primec_tests" / "include_version_multi_base";
  auto includeRootA = std::filesystem::temp_directory_path() / "primec_tests" / "include_version_multi_a";
  auto includeRootB = std::filesystem::temp_directory_path() / "primec_tests" / "include_version_multi_b";
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRootA);
  std::filesystem::remove_all(includeRootB);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRootA);
  std::filesystem::create_directories(includeRootB);

  writeFile(includeRootA / "1.2.0" / "lib.prime", "// INCLUDE_VERSION_MULTI_120\n");
  writeFile(includeRootB / "1.2.9" / "lib.prime", "// INCLUDE_VERSION_MULTI_129\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "include<\"/lib.prime\", version=\"1.2\">\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK(resolver.expandIncludes(srcPath, source, error, {includeRootA.string(), includeRootB.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_VERSION_MULTI_129") != std::string::npos);
  CHECK(source.find("INCLUDE_VERSION_MULTI_120") == std::string::npos);
}

TEST_CASE("rejects include version mismatch across paths") {
  auto baseDir = std::filesystem::temp_directory_path() / "primec_tests" / "include_version_mismatch_base";
  auto includeRootA = std::filesystem::temp_directory_path() / "primec_tests" / "include_version_mismatch_a";
  auto includeRootB = std::filesystem::temp_directory_path() / "primec_tests" / "include_version_mismatch_b";
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRootA);
  std::filesystem::remove_all(includeRootB);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRootA);
  std::filesystem::create_directories(includeRootB);

  writeFile(includeRootA / "1.2.0" / "lib_a.prime", "// V120_A\n");
  writeFile(includeRootB / "1.2.9" / "lib_b.prime", "// V129_B\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "include<\"/lib_a.prime\", \"/lib_b.prime\", version=\"1.2\">\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath,
                                      source,
                                      error,
                                      {includeRootA.string(), includeRootB.string()}));
  CHECK(error.find("include version mismatch") != std::string::npos);
}

TEST_CASE("resolves versioned relative include from include path") {
  auto baseDir = std::filesystem::temp_directory_path() / "primec_tests" / "include_version_rel_base";
  auto includeRoot = std::filesystem::temp_directory_path() / "primec_tests" / "include_version_rel_root";
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "2.0.0" / "lib.prime", "// INCLUDE_VERSION_REL_200\n");
  writeFile(includeRoot / "2.1.0" / "lib.prime", "// INCLUDE_VERSION_REL_210\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "include<\"lib.prime\", version=\"2\">\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK(resolver.expandIncludes(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_VERSION_REL_210") != std::string::npos);
  CHECK(source.find("INCLUDE_VERSION_REL_200") == std::string::npos);
}

TEST_CASE("resolves versioned include from archive root") {
  if (!hasZipTools()) {
    return;
  }
  auto baseDir = std::filesystem::temp_directory_path() / "primec_tests" / "include_zip_base";
  auto includeRoot = std::filesystem::temp_directory_path() / "primec_tests" / "include_zip_root";
  auto archiveSource = includeRoot / "archive_src";
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(archiveSource);

  writeFile(archiveSource / "1.2.0" / "std" / "io" / "lib.prime", "// ZIP_MARKER\n");
  const std::filesystem::path archivePath = includeRoot / "std_io.zip";
  CHECK(createZip(archivePath, archiveSource));

  const std::string srcPath =
      writeFile(baseDir / "main.prime", "include<\"/std/io\", version=\"1.2\">\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK(resolver.expandIncludes(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("ZIP_MARKER") != std::string::npos);
}

TEST_CASE("resolves exact include version") {
  auto baseDir = std::filesystem::temp_directory_path() / "primec_tests" / "include_versions_exact";
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);

  writeFile(baseDir / "1.2.0" / "lib.prime", "// V120\n");
  writeFile(baseDir / "1.2.1" / "lib.prime", "// V121\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "include<\"/lib.prime\", version=\"1.2.0\">\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("V120") != std::string::npos);
  CHECK(source.find("V121") == std::string::npos);
}

TEST_CASE("selects newest matching include version") {
  auto baseDir = std::filesystem::temp_directory_path() / "primec_tests" / "include_versions_latest";
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);

  writeFile(baseDir / "1.2.0" / "lib.prime", "// V120\n");
  writeFile(baseDir / "1.2.5" / "lib.prime", "// V125\n");
  writeFile(baseDir / "1.3.0" / "lib.prime", "// V130\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "include<\"/lib.prime\", version=\"1.2\">\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("V125") != std::string::npos);
  CHECK(source.find("V130") == std::string::npos);
}

TEST_CASE("expands directory include contents") {
  auto baseDir = std::filesystem::temp_directory_path() / "primec_tests" / "include_dir";
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);

  writeFile(baseDir / "b.prime", "// DIR_B\n");
  writeFile(baseDir / "a.prime", "// DIR_A\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "include<\"" + baseDir.string() + "\">\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.empty());
  const auto first = source.find("DIR_A");
  const auto second = source.find("DIR_B");
  CHECK(first != std::string::npos);
  CHECK(second != std::string::npos);
  CHECK(first < second);
}

TEST_CASE("skips private subdirectories in directory include") {
  auto baseDir = std::filesystem::temp_directory_path() / "primec_tests" / "include_dir_private";
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);

  writeFile(baseDir / "_private" / "secret.prime", "// SECRET\n");
  writeFile(baseDir / "public.prime", "// PUBLIC\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "include<\"" + baseDir.string() + "\">\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("PUBLIC") != std::string::npos);
  CHECK(source.find("SECRET") == std::string::npos);
}

TEST_CASE("rejects private include root directory") {
  auto baseDir = std::filesystem::temp_directory_path() / "primec_tests" / "include_private_root";
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir / "_private");

  writeFile(baseDir / "_private" / "secret.prime", "// SECRET\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "include<\"" + (baseDir / "_private").string() + "\">\n");

  std::string source;
  std::string error;
  primec::IncludeResolver resolver;
  CHECK_FALSE(resolver.expandIncludes(srcPath, source, error));
  CHECK(error.find("private folder") != std::string::npos);
}

TEST_SUITE_END();
