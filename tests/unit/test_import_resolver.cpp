#include "primec/ImportResolver.h"
#include "primec/ProcessRunner.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <functional>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
std::filesystem::path importResolverPath(std::string_view relativePath) {
  return primec::testing::testScratchPath("imports_resolver/" + std::string(relativePath));
}

std::string writeTemp(const std::string &name, const std::string &contents) {
  const auto path = importResolverPath(name);
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

class RecordingProcessRunner final : public primec::ProcessRunner {
public:
  explicit RecordingProcessRunner(std::function<int(const std::vector<std::string> &)> handler = {})
      : handler_(std::move(handler)) {}

  int run(const std::vector<std::string> &args) const override {
    commands.push_back(args);
    if (handler_) {
      return handler_(args);
    }
    return 1;
  }

  mutable std::vector<std::vector<std::string>> commands;

private:
  std::function<int(const std::vector<std::string> &)> handler_;
};
} // namespace

TEST_SUITE_BEGIN("primestruct.imports.resolver");

TEST_CASE("expands single import") {
  const std::string libPath = writeTemp("lib_a.prime", "[return<int>]\nhelper(){ return(3i32) }\n");
  const std::string srcPath = writeTemp("main_a.prime", "import<\"" + libPath + "\">\n[return<int>]\nmain(){ return(helper()) }\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("helper") != std::string::npos);
}

TEST_CASE("rejects single legacy include alias") {
  const std::string srcPath = writeTemp("main_legacy_alias.prime", "include<\"/std/io\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error == "legacy include<...> is no longer supported; use import<...>");
}

TEST_CASE("expands import with whitespace") {
  const std::string marker = "LIB_WS_MARKER";
  const std::string libPath = writeTemp("lib_ws.prime", "// " + marker + "\n");
  const std::string srcPath = writeTemp("main_ws_single.prime", "import<  \"" + libPath + "\"  >\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find(marker) != std::string::npos);
}

TEST_CASE("expands import with tight comment") {
  const std::string marker = "LIB_TIGHT_COMMENT";
  const std::string libPath = writeTemp("lib_tight_comment.prime", "// " + marker + "\n");
  const std::string srcPath = writeTemp("main_tight_comment.prime", "import/* note */<\"" + libPath + "\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find(marker) != std::string::npos);
}

TEST_CASE("expands import with bare slash path") {
  auto baseDir = importResolverPath("include_bare_path_base");
  auto includeRoot = importResolverPath("include_bare_path_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "lib" / "lib.prime", "// INCLUDE_BARE_PATH\n");
  const std::string srcPath = writeFile(baseDir / "main.prime", "import</lib>\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_BARE_PATH") != std::string::npos);
}

TEST_CASE("expands bare slash imports with semicolons") {
  auto baseDir = importResolverPath("include_bare_semicolon_base");
  auto includeRoot = importResolverPath("include_bare_semicolon_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "lib_a" / "lib.prime", "// INCLUDE_BARE_SEMI_A\n");
  writeFile(includeRoot / "lib_b" / "lib.prime", "// INCLUDE_BARE_SEMI_B\n");
  const std::string srcPath = writeFile(baseDir / "main.prime", "import</lib_a;/lib_b>\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_BARE_SEMI_A") != std::string::npos);
  CHECK(source.find("INCLUDE_BARE_SEMI_B") != std::string::npos);
}

TEST_CASE("expands bare slash imports with whitespace separators") {
  auto baseDir = importResolverPath("include_bare_ws_base");
  auto includeRoot = importResolverPath("include_bare_ws_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "lib_a" / "lib.prime", "// INCLUDE_BARE_WS_A\n");
  writeFile(includeRoot / "lib_b" / "lib.prime", "// INCLUDE_BARE_WS_B\n");
  const std::string srcPath = writeFile(baseDir / "main.prime", "import</lib_a /lib_b>\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_BARE_WS_A") != std::string::npos);
  CHECK(source.find("INCLUDE_BARE_WS_B") != std::string::npos);
}

TEST_CASE("bare slash import does not use absolute filesystem path") {
  auto absRoot = importResolverPath("include_bare_absolute");
  auto baseDir = importResolverPath("include_bare_absolute_base");
  std::filesystem::remove_all(absRoot);
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(absRoot);
  std::filesystem::create_directories(baseDir);

  writeFile(absRoot / "lib.prime", "// INCLUDE_BARE_ABS\n");
  const std::string srcPath = writeFile(baseDir / "main.prime", "import<" + absRoot.string() + ">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(!resolver.expandImports(srcPath, source, error));
  CHECK(error.find("invalid slash path identifier") != std::string::npos);
}

TEST_CASE("resolves versioned import with single quotes") {
  auto baseDir = importResolverPath("include_single_quote_base");
  auto includeRoot = importResolverPath("include_single_quote_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "1.2.0" / "lib.prime", "// INCLUDE_SINGLE_QUOTE_120\n");
  writeFile(includeRoot / "1.2.3" / "lib.prime", "// INCLUDE_SINGLE_QUOTE_123\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime", "import<'/lib.prime', version = '1.2'>\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_SINGLE_QUOTE_123") != std::string::npos);
  CHECK(source.find("INCLUDE_SINGLE_QUOTE_120") == std::string::npos);
}

TEST_CASE("rejects versioned legacy include alias") {
  const std::string srcPath = writeTemp("main_legacy_version_alias.prime", "include<'/lib.prime', version='1.2'>\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error == "legacy include<...> is no longer supported; use import<...>");
}

TEST_CASE("rejects version-first legacy include alias") {
  const std::string srcPath = writeTemp("main_legacy_version_first_alias.prime", "include<version='1.2', '/lib.prime'>\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error == "legacy include<...> is no longer supported; use import<...>");
}

TEST_CASE("ignores duplicate imports") {
  const std::string marker = "LIB_B_MARKER";
  const std::string libPath = writeTemp("lib_b.prime",
                                        "// " + marker + "\n"
                                        "[return<int>]\nhelper(){ return(5i32) }\n");
  const std::string srcPath = writeTemp("main_b.prime",
                                        "import<\"" + libPath + "\">\n"
                                        "import<\"" + libPath + "\">\n"
                                        "[return<int>]\nmain(){ return(helper()) }\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  const auto first = source.find(marker);
  const auto second = source.find(marker, first == std::string::npos ? 0 : first + 1);
  CHECK(first != std::string::npos);
  CHECK(second == std::string::npos);
}

TEST_CASE("ignores duplicate imports with equivalent relative paths") {
  auto baseDir = importResolverPath("include_duplicate_relative");
  std::filesystem::remove_all(baseDir);
  std::filesystem::create_directories(baseDir);

  writeFile(baseDir / "lib.prime", "// INCLUDE_DUPLICATE_RELATIVE\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime",
                "import<\"lib.prime\">\n"
                "import<\"./lib.prime\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  const auto first = source.find("INCLUDE_DUPLICATE_RELATIVE");
  const auto second = source.find("INCLUDE_DUPLICATE_RELATIVE", first == std::string::npos ? 0 : first + 1);
  CHECK(first != std::string::npos);
  CHECK(second == std::string::npos);
}

TEST_CASE("missing import fails") {
  const std::string srcPath = writeTemp("main_c.prime", "import<\"/tmp/does_not_exist.prime\">\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error));
  CHECK(error.find("failed to read import") != std::string::npos);
}

TEST_CASE("ignores import directives inside string literals") {
  const std::string srcPath =
      writeTemp("include_in_string.prime",
                "[return<int>]\n"
                "main() {\n"
                "  print_line(\"before import<\\\"/tmp/does_not_exist.prime\\\"> after\"utf8)\n"
                "  return(0i32)\n"
                "}\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("import<\\\"/tmp/does_not_exist.prime\\\">") != std::string::npos);
}

TEST_CASE("ignores import directives inside comments") {
  const std::string srcPath =
      writeTemp("include_in_comment.prime",
                "// import<\"/tmp/does_not_exist.prime\">\n"
                "[return<int>]\n"
                "main(){ return(0i32) }\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("import<\"/tmp/does_not_exist.prime\">") != std::string::npos);
}

TEST_CASE("ignores import keyword within identifiers") {
  const std::string srcPath =
      writeTemp("include_identifier.prime",
                "ximport<\"/tmp/does_not_exist.prime\">\n"
                "importX<\"/tmp/does_not_exist.prime\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("ximport<\"/tmp/does_not_exist.prime\">") != std::string::npos);
  CHECK(source.find("importX<\"/tmp/does_not_exist.prime\">") != std::string::npos);
}

TEST_CASE("ignores import keyword after path separator") {
  const std::string srcPath =
      writeTemp("include_after_slash.prime",
                "path/import<\"/tmp/does_not_exist.prime\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find("path/import<\"/tmp/does_not_exist.prime\">") != std::string::npos);
}

TEST_CASE("resolves import from import path") {
  auto baseDir = importResolverPath("include_search_base");
  auto includeRoot = importResolverPath("include_search_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "lib.prime", "// INCLUDE_ROOT_MARKER\n");
  const std::string srcPath = writeFile(baseDir / "main.prime", "import<\"/lib.prime\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_ROOT_MARKER") != std::string::npos);
}

TEST_CASE("resolves relative import from import path") {
  auto baseDir = importResolverPath("include_relative_base");
  auto includeRoot = importResolverPath("include_relative_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "lib.prime", "// INCLUDE_RELATIVE_MARKER\n");
  const std::string srcPath = writeFile(baseDir / "main.prime", "import<\"lib.prime\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_RELATIVE_MARKER") != std::string::npos);
}

TEST_CASE("supports semicolon separated imports") {
  const std::string markerA = "SEMICOLON_A";
  const std::string markerB = "SEMICOLON_B";
  const std::string libA = writeTemp("lib_semicolon_a.prime", "// " + markerA + "\n");
  const std::string libB = writeTemp("lib_semicolon_b.prime", "// " + markerB + "\n");
  const std::string srcPath =
      writeTemp("main_semicolon.prime", "import<\"" + libA + "\"; \"" + libB + "\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error));
  CHECK(error.empty());
  CHECK(source.find(markerA) != std::string::npos);
  CHECK(source.find(markerB) != std::string::npos);
}

#include "test_import_resolver_versions.h"
