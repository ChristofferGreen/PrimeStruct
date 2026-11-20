#include "ir_link_test_support.h"

#include "primec/pipeline/LibraryArchive.h"
#include "primec/support/Sha256.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

using namespace primec;
using namespace ir_link_test;

// The container of the library archive (docs/spec/integration-and-tooling.md, Library Archive) with a
// hand-built module: the layout, the determinism and every refusal of the reader.
TEST_SUITE_BEGIN("primestruct.runtime.library_archive");

namespace {

const char *const BuildId = "4f2a0c9d11ab7e53c04d8e9f6a1b2c3d4e5f60718293a4b5c6d7e8f9a0b1c2d3";
const char *const InterfaceHash =
    "8151fb12e46e25142da92e8c22165c4128b5965b08bb1240a3155eae35bbe67f";

LibraryArchiveContent sampleContent() {
  LibraryArchiveContent content;
  content.manifest.name = "ui";
  content.manifest.roots = {"/ui", "/base"};
  content.manifest.compilerBuildId = BuildId;
  content.manifest.interfaceHash = InterfaceHash;
  content.manifest.libraries = {{"core", "aa"}, {"base", "bb"}};
  content.interfaceText = "[namespace] ui {\n  [public, return<i32>] one() {}\n}\n";
  content.symbolsText =
      std::string("{\"interfaceHash\":\"") + InterfaceHash + "\",\"symbols\":[]}\n";
  content.code =
      moduleOf({functionOf("/ui/one", 0, {{IrOpcode::PushI32, 1}, {IrOpcode::ReturnI32, 0}})}, -1);
  return content;
}

LibraryArchiveExpectations expectations() {
  LibraryArchiveExpectations expected;
  expected.compilerBuildId = BuildId;
  return expected;
}

std::vector<uint8_t> bytesOf(const LibraryArchiveContent &content) {
  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE_MESSAGE(serializeLibraryArchive(content, bytes, error), error);
  return bytes;
}

// The offset of `needle` in the archive bytes.
size_t find(const std::vector<uint8_t> &bytes, const std::string &needle) {
  const std::string text(bytes.begin(), bytes.end());
  const size_t at = text.find(needle);
  REQUIRE(at != std::string::npos);
  return at;
}

std::string readError(const std::vector<uint8_t> &bytes) {
  LibraryArchiveContent out;
  std::string error;
  CHECK_FALSE(deserializeLibraryArchive(bytes, expectations(), out, error));
  return error;
}

bool mentionsRebuild(const std::string &message) {
  return message.find("rebuild the library") != std::string::npos;
}

} // namespace

TEST_CASE("library archive: a written archive reads back with every section") {
  LibraryArchiveContent content = sampleContent();
  content.hasSources = true;
  content.sources = {{"z/last.prime", "last\n"}, {"a/first.prime", "first\n"}};
  const std::vector<uint8_t> bytes = bytesOf(content);

  LibraryArchiveContent back;
  std::string error;
  REQUIRE_MESSAGE(deserializeLibraryArchive(bytes, expectations(), back, error), error);
  CHECK(back.manifest.name == "ui");
  // Roots, linked libraries and sources are stored sorted.
  CHECK(back.manifest.roots == std::vector<std::string>{"/base", "/ui"});
  REQUIRE(back.manifest.libraries.size() == 2);
  CHECK(back.manifest.libraries[0].name == "base");
  CHECK(back.manifest.libraries[1].interfaceHash == "aa");
  CHECK(back.manifest.compilerBuildId == BuildId);
  CHECK(back.manifest.irSchemaVersion == IrSchemaVersion);
  CHECK(back.manifest.hashScheme == "sha256");
  CHECK(back.manifest.interfaceHash == InterfaceHash);
  CHECK(back.interfaceText == content.interfaceText);
  CHECK(back.symbolsText == content.symbolsText);
  REQUIRE(back.code.functions.size() == 1);
  CHECK(back.code.functions[0].name == "/ui/one");
  CHECK(back.code.entryIndex == -1);
  REQUIRE(back.hasSources);
  REQUIRE(back.sources.size() == 2);
  CHECK(back.sources[0].path == "a/first.prime");
  CHECK(back.sources[1].text == "last\n");
}

TEST_CASE("library archive: sections come in name order and the sources are optional") {
  const std::vector<uint8_t> without = bytesOf(sampleContent());
  const size_t code = find(without, "code");
  const size_t interfaceSection = find(without, "interface");
  const size_t manifest = find(without, "manifest");
  const size_t symbols = find(without, "symbols");
  CHECK(code < interfaceSection);
  CHECK(interfaceSection < manifest);
  CHECK(manifest < symbols);
  LibraryArchiveContent back;
  std::string error;
  REQUIRE(deserializeLibraryArchive(without, expectations(), back, error));
  CHECK_FALSE(back.hasSources);

  LibraryArchiveContent withSources = sampleContent();
  withSources.hasSources = true;
  const std::vector<uint8_t> bytes = bytesOf(withSources);
  CHECK(find(bytes, "manifest") < find(bytes, "sources"));
  CHECK(find(bytes, "sources") < find(bytes, "symbols"));
}

TEST_CASE("library archive: the bytes depend only on the content") {
  LibraryArchiveContent content = sampleContent();
  const std::vector<uint8_t> first = bytesOf(content);
  const std::vector<uint8_t> second = bytesOf(content);
  CHECK(first == second);
  // The order a caller listed roots and libraries in does not reach the file.
  LibraryArchiveContent shuffled = sampleContent();
  std::swap(shuffled.manifest.roots[0], shuffled.manifest.roots[1]);
  std::swap(shuffled.manifest.libraries[0], shuffled.manifest.libraries[1]);
  CHECK(bytesOf(shuffled) == first);
  // No path and no time: the file does not hold the scratch directory it was made in.
  const std::string text(first.begin(), first.end());
  CHECK(text.find(primec::testing::testScratchPath("").string()) == std::string::npos);
}

TEST_CASE("library archive: a changed section is refused and the message says to rebuild") {
  for (const char *section : {"interface", "symbols", "manifest"}) {
    CAPTURE(section);
    std::vector<uint8_t> bytes = bytesOf(sampleContent());
    // The payloads follow the table; flip a byte of the section's own text.
    const std::string probe = std::string(section) == "interface" ? "one() {}"
                              : std::string(section) == "symbols" ? "\"symbols\""
                                                                  : "name ui";
    bytes[find(bytes, probe)] ^= 0x01;
    const std::string error = readError(bytes);
    CHECK(error.find(std::string("library section `") + section + "` changed") !=
          std::string::npos);
    CHECK(mentionsRebuild(error));
  }
  // The last byte of the file is in the last section.
  std::vector<uint8_t> bytes = bytesOf(sampleContent());
  bytes.back() ^= 0xFF;
  const std::string error = readError(bytes);
  CHECK(error.find("changed since the library was built") != std::string::npos);
  CHECK(mentionsRebuild(error));
}

TEST_CASE("library archive: another compiler build is refused with its own message") {
  const std::vector<uint8_t> bytes = bytesOf(sampleContent());
  LibraryArchiveExpectations other = expectations();
  other.compilerBuildId = "0123456789abcdef0123456789abcdef";
  LibraryArchiveContent out;
  std::string error;
  CHECK_FALSE(deserializeLibraryArchive(bytes, other, out, error));
  CHECK(error.find("another compiler build") != std::string::npos);
  CHECK(error.find("4f2a0c9d11ab") != std::string::npos);
  CHECK(mentionsRebuild(error));
}

TEST_CASE("library archive: another IR schema version is refused with its own message") {
  LibraryArchiveContent content = sampleContent();
  content.manifest.irSchemaVersion = IrSchemaVersion - 1u;
  const std::vector<uint8_t> bytes = bytesOf(content);
  const std::string error = readError(bytes);
  CHECK(error.find("IR schema version " + std::to_string(IrSchemaVersion - 1u)) !=
        std::string::npos);
  CHECK(error.find("another compiler build") == std::string::npos);
  CHECK(mentionsRebuild(error));
}

TEST_CASE(
    "library archive: a file that is not an archive, an old container and a cut file are refused") {
  CHECK(readError({'h', 'e', 'l', 'l', 'o'}).find("not a library archive") != std::string::npos);

  std::vector<uint8_t> oldFormat = bytesOf(sampleContent());
  oldFormat[8] = 99;
  const std::string formatError = readError(oldFormat);
  CHECK(formatError.find("format version 99") != std::string::npos);
  CHECK(mentionsRebuild(formatError));

  std::vector<uint8_t> cut = bytesOf(sampleContent());
  cut.resize(cut.size() - 3);
  const std::string cutError = readError(cut);
  CHECK(cutError.find("damaged") != std::string::npos);
  CHECK(mentionsRebuild(cutError));

  std::vector<uint8_t> extra = bytesOf(sampleContent());
  extra.push_back(0);
  CHECK(readError(extra).find("bytes follow the last section") != std::string::npos);
}

TEST_CASE("library archive: a manifest and a symbol table that disagree are refused") {
  LibraryArchiveContent content = sampleContent();
  content.symbolsText = "{\"interfaceHash\":\"" + std::string(64, '0') + "\",\"symbols\":[]}\n";
  const std::string error = readError(bytesOf(content));
  CHECK(error.find("different interface hashes") != std::string::npos);
  CHECK(mentionsRebuild(error));
}

TEST_CASE(
    "library archive: a code section with an entry point and a name with white space are refused") {
  LibraryArchiveContent withEntry = sampleContent();
  withEntry.code.entryIndex = 0;
  CHECK(readError(bytesOf(withEntry)).find("entry point") != std::string::npos);

  LibraryArchiveContent badName = sampleContent();
  badName.manifest.name = "two words";
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_FALSE(serializeLibraryArchive(badName, bytes, error));
  CHECK(error.find("white space") != std::string::npos);
}

TEST_CASE("library archive: a file written to disk reads back") {
  const std::filesystem::path path = primec::testing::testScratchPath("library_archive_file.psa");
  std::string error;
  REQUIRE_MESSAGE(writeLibraryArchiveFile(path.string(), sampleContent(), error), error);
  LibraryArchiveContent back;
  REQUIRE_MESSAGE(readLibraryArchiveFile(path.string(), expectations(), back, error), error);
  CHECK(back.manifest.interfaceHash == InterfaceHash);
  CHECK_FALSE(readLibraryArchiveFile(
      primec::testing::testScratchPath("library_archive_missing.psa").string(),
      expectations(),
      back,
      error));
  CHECK(error.find("cannot read") != std::string::npos);
}

TEST_CASE("sha256: published digests, whatever the update sizes") {
  CHECK(primec::sha256Hex("") ==
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  CHECK(primec::sha256Hex("abc") ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  const std::string twoBlocks = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  CHECK(primec::sha256Hex(twoBlocks) ==
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  const std::string million(1000000, 'a');
  const std::string expected = "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0";
  CHECK(primec::sha256Hex(million) == expected);
  primec::Sha256 pieces;
  size_t at = 0;
  size_t step = 1;
  while (at < million.size()) {
    const size_t take = std::min(step, million.size() - at);
    pieces.update(million.data() + at, take);
    at += take;
    step = step * 3 % 211 + 1;
  }
  CHECK(pieces.finishHex() == expected);
}

TEST_SUITE_END();
