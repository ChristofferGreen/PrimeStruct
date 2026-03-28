#include "test_import_resolver_helpers.h"

namespace {
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

TEST_SUITE_BEGIN("primestruct.imports.resolver");

TEST_CASE("allows comments around version attribute") {
  auto baseDir = importResolverPath("include_version_comment_base");
  auto includeRoot = importResolverPath("include_version_comment_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "1.2.0" / "lib.prime", "// INCLUDE_COMMENT_VERSION\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime",
                "import<\"/lib.prime\" /* note */, version = \"1.2\" >\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_COMMENT_VERSION") != std::string::npos);
}

TEST_CASE("allows comments containing > in import list") {
  auto baseDir = importResolverPath("include_comment_bracket_base");
  auto includeRoot = importResolverPath("include_comment_bracket_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "1.2.0" / "lib.prime", "// INCLUDE_COMMENT_BRACKET\n");
  const std::string srcPath =
      writeFile(baseDir / "main.prime",
                "import<\"/lib.prime\" /* ignore > here */, version=\"1.2\" >\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_COMMENT_BRACKET") != std::string::npos);
}

TEST_CASE("resolves highest matching version directory") {
  auto baseDir = importResolverPath("include_version_highest_base");
  auto includeRoot = importResolverPath("include_version_highest_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "1.2.0" / "lib.prime", "// INCLUDE_VERSION_120\n");
  writeFile(includeRoot / "1.2.7" / "lib.prime", "// INCLUDE_VERSION_127\n");
  writeFile(includeRoot / "1.3.0" / "lib.prime", "// INCLUDE_VERSION_130\n");
  const std::string srcPath = writeFile(baseDir / "main.prime", "import<\"/lib.prime\", version=\"1.2\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_VERSION_127") != std::string::npos);
  CHECK(source.find("INCLUDE_VERSION_120") == std::string::npos);
  CHECK(source.find("INCLUDE_VERSION_130") == std::string::npos);
}

TEST_CASE("rejects missing version match") {
  auto baseDir = importResolverPath("include_version_missing_base");
  auto includeRoot = importResolverPath("include_version_missing_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot);

  writeFile(includeRoot / "2.0.0" / "lib.prime", "// INCLUDE_VERSION_200\n");
  const std::string srcPath = writeFile(baseDir / "main.prime", "import<\"/lib.prime\", version=\"1.2\">\n");

  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK_FALSE(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.find("no version matching") != std::string::npos);
}

TEST_CASE("versioned archives expand through import resolver") {
  if (!hasZipTools()) {
    CHECK(true);
    return;
  }

  auto baseDir = importResolverPath("include_version_archive_base");
  auto includeRoot = importResolverPath("include_version_archive_root");
  auto packageDir = importResolverPath("include_version_archive_pkg");
  auto archivePath = importResolverPath("include_version_archive_root/1.4.0/lib.zip");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::remove_all(packageDir);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot / "1.4.0");
  std::filesystem::create_directories(packageDir);

  writeFile(packageDir / "lib.prime", "// INCLUDE_VERSION_ARCHIVE\n");
  REQUIRE(createZip(archivePath, packageDir));

  const std::string srcPath = writeFile(baseDir / "main.prime", "import<\"/lib.zip\", version=\"1.4\">\n");
  std::string source;
  std::string error;
  primec::ImportResolver resolver;
  CHECK(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK(error.empty());
  CHECK(source.find("INCLUDE_VERSION_ARCHIVE") != std::string::npos);
}

TEST_CASE("versioned import delegates archive extraction to injected runner") {
  auto baseDir = importResolverPath("include_version_runner_base");
  auto includeRoot = importResolverPath("include_version_runner_root");
  std::filesystem::remove_all(baseDir);
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(baseDir);
  std::filesystem::create_directories(includeRoot / "1.8.0");
  writeFile(includeRoot / "1.8.0" / "lib.zip", "not_a_real_zip");
  const std::string srcPath = writeFile(baseDir / "main.prime", "import<\"/lib.zip\", version=\"1.8\">\n");

  RecordingProcessRunner runner([](const std::vector<std::string> &) { return 0; });
  std::string source;
  std::string error;
  primec::ImportResolver resolver(&runner);
  CHECK_FALSE(resolver.expandImports(srcPath, source, error, {includeRoot.string()}));
  CHECK_FALSE(runner.commands.empty());
}

TEST_SUITE_END();
