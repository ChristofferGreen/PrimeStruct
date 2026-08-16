#include "primec/frontend/StdlibSymbolManifest.h"

#include "third_party/doctest.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

TEST_SUITE_BEGIN("primestruct.stdlib_symbol_manifest");

namespace {

std::filesystem::path repoRoot() {
  std::filesystem::path candidate = std::filesystem::path("..") / "stdlib";
  if (std::filesystem::exists(candidate)) {
    return std::filesystem::path("..");
  }
  return std::filesystem::current_path();
}

void writeWholeFile(const std::filesystem::path &path, const std::string &content) {
  std::ofstream out(path, std::ios::binary);
  REQUIRE(static_cast<bool>(out));
  out << content;
}

} // namespace

TEST_CASE("real stdlib manifests load and every entry extracts and hash-verifies") {
  const std::filesystem::path root = repoRoot();
  const struct {
    std::filesystem::path manifestPath;
    std::filesystem::path sourcePath;
  } modules[] = {
      {root / "stdlib" / "std" / "image" / "image.psmeta",
       root / "stdlib" / "std" / "image" / "image.prime"},
      {root / "stdlib" / "std" / "gfx" / "experimental.psmeta",
       root / "stdlib" / "std" / "gfx" / "experimental.prime"},
  };

  for (const auto &module : modules) {
    INFO("manifest: " << module.manifestPath.string());
    REQUIRE(std::filesystem::exists(module.manifestPath));
    REQUIRE(std::filesystem::exists(module.sourcePath));

    std::vector<primec::StdlibSymbolManifestEntry> entries;
    std::string error;
    REQUIRE(primec::readStdlibSymbolManifest(module.manifestPath.string(), entries, error));
    CHECK_FALSE(entries.empty());

    for (const auto &entry : entries) {
      INFO("symbol: " << entry.path);
      std::string sourceText;
      const bool ok = primec::extractAndVerifyManifestedSymbolSource(
          entry, module.sourcePath.string(), sourceText, error);
      CHECK(ok);
      CHECK_FALSE(sourceText.empty());
      if (!ok) {
        INFO("error: " << error);
        CHECK(ok);
      }
    }
  }
}

TEST_CASE("a manifest entry with a tampered content hash fails extraction loudly") {
  const std::filesystem::path root = repoRoot();
  const std::filesystem::path manifestPath = root / "stdlib" / "std" / "image" / "image.psmeta";
  const std::filesystem::path sourcePath = root / "stdlib" / "std" / "image" / "image.prime";
  REQUIRE(std::filesystem::exists(manifestPath));

  std::vector<primec::StdlibSymbolManifestEntry> entries;
  std::string error;
  REQUIRE(primec::readStdlibSymbolManifest(manifestPath.string(), entries, error));
  REQUIRE_FALSE(entries.empty());

  primec::StdlibSymbolManifestEntry tampered = entries.front();
  tampered.contentHash ^= 0xdeadbeefULL;

  std::string sourceText;
  const bool ok = primec::extractAndVerifyManifestedSymbolSource(
      tampered, sourcePath.string(), sourceText, error);
  CHECK_FALSE(ok);
  CHECK(sourceText.empty());
  CHECK(error.find(tampered.path) != std::string::npos);
}

TEST_CASE("readStdlibSymbolManifest rejects malformed manifests") {
  const std::filesystem::path tempPath =
      std::filesystem::temp_directory_path() / "primestruct_bad_manifest_test.psmeta";

  writeWholeFile(tempPath,
                 "[symbol]\n"
                 "path = /std/example/thing\n"
                 "start_line = 4\n"
                 "end_line = 2\n"
                 "content_hash = ab\n");
  std::vector<primec::StdlibSymbolManifestEntry> entries;
  std::string error;
  CHECK_FALSE(primec::readStdlibSymbolManifest(tempPath.string(), entries, error));
  CHECK_FALSE(error.empty());

  writeWholeFile(tempPath, "path = /std/example/thing\n");
  entries.clear();
  error.clear();
  CHECK_FALSE(primec::readStdlibSymbolManifest(tempPath.string(), entries, error));

  std::filesystem::remove(tempPath);
}

TEST_SUITE_END();
