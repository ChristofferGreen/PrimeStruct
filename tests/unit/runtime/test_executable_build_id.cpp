#include "primec/support/CompilerSourceId.h"
#include "primec/support/ExecutableBuildId.h"
#include "primec/support/Sha256.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

// The compiler build id of a library: the linker's identifier of the executable, the full hash as the
// fallback. Determinism is shown with byte-identical copies (equal ids) and with images that differ in code and
// identifier (different ids); nothing here measures time.
TEST_SUITE_BEGIN("primestruct.runtime.library_archive");

namespace {

void putLe(std::string &out, uint64_t value, size_t width) {
  for (size_t i = 0; i < width; ++i) {
    out.push_back(static_cast<char>((value >> (8 * i)) & 0xff));
  }
}

void putBe(std::string &out, uint64_t value, size_t width) {
  for (size_t i = 0; i < width; ++i) {
    out.push_back(static_cast<char>((value >> (8 * (width - 1 - i))) & 0xff));
  }
}

std::string uuidBytes(char seed) {
  std::string out;
  for (int i = 0; i < 16; ++i) {
    out.push_back(static_cast<char>(seed + i));
  }
  return out;
}

std::string hexOfBytes(const std::string &bytes) {
  static const char Digits[] = "0123456789abcdef";
  std::string out;
  for (const char c : bytes) {
    out.push_back(Digits[(static_cast<unsigned char>(c) >> 4) & 15]);
    out.push_back(Digits[static_cast<unsigned char>(c) & 15]);
  }
  return out;
}

// A thin 64-bit Mach-O: a segment command (to be skipped), an optional LC_UUID, then `code`.
std::string machoImage(const std::string *uuid, const std::string &code) {
  std::string commands;
  putLe(commands, 0x19, 4); // LC_SEGMENT_64, skipped by size
  putLe(commands, 24, 4);
  commands.append(16, 'x');
  if (uuid != nullptr) {
    putLe(commands, 0x1b, 4);
    putLe(commands, 24, 4);
    commands += *uuid;
  }
  std::string out;
  putLe(out, 0xfeedfacf, 4);
  putLe(out, 0x0100000c, 4);
  putLe(out, 0, 4);
  putLe(out, 2, 4);
  putLe(out, uuid != nullptr ? 2 : 1, 4);
  putLe(out, commands.size(), 4);
  putLe(out, 0, 4);
  putLe(out, 0, 4);
  return out + commands + code;
}

// An ELF image with one PT_LOAD and one PT_NOTE that holds an unrelated note and, when `buildId` is given, the GNU
// build-id note. `is64` selects the class and `little` the byte order.
std::string elfImage(bool is64, bool little, const std::string *buildId, const std::string &code) {
  auto put = [little](std::string &out, uint64_t value, size_t width) {
    if (little) {
      putLe(out, value, width);
    } else {
      putBe(out, value, width);
    }
  };
  std::string notes;
  put(notes, 4, 4); // an unrelated note first: name "ABC", type 1, 4 bytes of data
  put(notes, 4, 4);
  put(notes, 1, 4);
  notes += std::string("ABC\0", 4);
  notes += "data";
  if (buildId != nullptr) {
    put(notes, 4, 4);
    put(notes, buildId->size(), 4);
    put(notes, 3, 4);
    notes += std::string("GNU\0", 4);
    notes += *buildId;
    while (notes.size() % 4 != 0) {
      notes.push_back('\0');
    }
  }
  const size_t headerSize = is64 ? 64 : 52;
  const size_t entrySize = is64 ? 56 : 32;
  const size_t notesAt = headerSize + 2 * entrySize;
  std::string out("\x7f"
                  "ELF",
                  4);
  out.push_back(is64 ? 2 : 1);
  out.push_back(little ? 1 : 2);
  out.push_back(1);
  out.append(9, '\0');
  put(out, 2, 2);                     // e_type
  put(out, 0x3e, 2);                  // e_machine
  put(out, 1, 4);                     // e_version
  put(out, 0, is64 ? 8 : 4);          // e_entry
  put(out, headerSize, is64 ? 8 : 4); // e_phoff
  put(out, 0, is64 ? 8 : 4);          // e_shoff
  put(out, 0, 4);                     // e_flags
  put(out, headerSize, 2);            // e_ehsize
  put(out, entrySize, 2);             // e_phentsize
  put(out, 2, 2);                     // e_phnum
  put(out, 0, 2);
  put(out, 0, 2);
  put(out, 0, 2);
  auto programHeader = [&](uint32_t type, uint64_t offset, uint64_t size) {
    put(out, type, 4);
    if (is64) {
      put(out, 4, 4);      // p_flags
      put(out, offset, 8); // p_offset
      put(out, 0, 8);
      put(out, 0, 8);
      put(out, size, 8); // p_filesz
      put(out, size, 8);
      put(out, 1, 8);
    } else {
      put(out, offset, 4);
      put(out, 0, 4);
      put(out, 0, 4);
      put(out, size, 4);
      put(out, size, 4);
      put(out, 4, 4);
      put(out, 1, 4);
    }
  };
  programHeader(1, 0, 0);
  programHeader(4, notesAt, notes.size());
  return out + notes + code;
}

std::string readAll(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::filesystem::path writeImage(const std::string &name, const std::string &bytes) {
  const std::filesystem::path path = primec::testing::testScratchPath(name);
  std::ofstream out(path, std::ios::binary);
  out << bytes;
  return path;
}

std::string idOf(const std::filesystem::path &path, primec::BuildIdSource source) {
  std::string id;
  std::string error;
  REQUIRE_MESSAGE(primec::executableBuildId(path.string(), source, id, error), error);
  return id;
}

} // namespace

TEST_CASE("build id: reads LC_UUID of a Mach-O image and skips other load commands") {
  const std::string uuid = uuidBytes('a');
  CHECK(primec::linkerBuildIdOfBytes(machoImage(&uuid, "code")) == hexOfBytes(uuid));
}

TEST_CASE("build id: a Mach-O image without LC_UUID has none") {
  CHECK(primec::linkerBuildIdOfBytes(machoImage(nullptr, "code")).empty());
}

TEST_CASE("build id: reads the GNU build-id note of every ELF class and byte order") {
  const std::string id = uuidBytes('k').substr(0, 20);
  for (const bool is64 : {true, false}) {
    for (const bool little : {true, false}) {
      CAPTURE(is64);
      CAPTURE(little);
      CHECK(primec::linkerBuildIdOfBytes(elfImage(is64, little, &id, "code")) == hexOfBytes(id));
      CHECK(primec::linkerBuildIdOfBytes(elfImage(is64, little, nullptr, "code")).empty());
    }
  }
}

TEST_CASE("build id: other formats, truncated headers and damaged tables have none") {
  const std::string uuid = uuidBytes('a');
  const std::string macho = machoImage(&uuid, "");
  CHECK(primec::linkerBuildIdOfBytes("").empty());
  CHECK(primec::linkerBuildIdOfBytes("MZ and a PE image would follow").empty());
  CHECK(primec::linkerBuildIdOfBytes(std::string(200, '\0')).empty());
  CHECK(primec::linkerBuildIdOfBytes(macho.substr(0, 40)).empty());
  CHECK(primec::linkerBuildIdOfBytes(macho.substr(0, macho.size() - 1)).empty());
  const std::string id = uuidBytes('k').substr(0, 20);
  const std::string elf = elfImage(true, true, &id, "");
  CHECK(primec::linkerBuildIdOfBytes(elf.substr(0, 100)).empty());
  CHECK(primec::linkerBuildIdOfBytes(elf.substr(0, elf.size() - 5)).empty());
  std::string fat;
  putBe(fat, 0xcafebabe, 4);
  fat.append(100, '\0');
  CHECK(primec::linkerBuildIdOfBytes(fat).empty());
}

TEST_CASE("build id: the id is the linker identifier when present and the full hash otherwise") {
  const std::string uuid = uuidBytes('a');
  const std::string withUuid = machoImage(&uuid, "code");
  const std::string without = machoImage(nullptr, "code");
  const std::filesystem::path withPath = writeImage("build_id_with_uuid.bin", withUuid);
  const std::filesystem::path withoutPath = writeImage("build_id_without_uuid.bin", without);

  CHECK(idOf(withPath, primec::BuildIdSource::LinkerIdFirst) == "uuid-" + hexOfBytes(uuid));
  // The fallback: no identifier in the image, or the full hash asked for (the test hook).
  CHECK(idOf(withoutPath, primec::BuildIdSource::LinkerIdFirst) ==
        "sha256-" + primec::sha256Hex(without));
  CHECK(idOf(withPath, primec::BuildIdSource::FullHash) == "sha256-" + primec::sha256Hex(withUuid));
  // The two forms never equal each other, so an archive of one is refused under the other.
  CHECK(idOf(withPath, primec::BuildIdSource::LinkerIdFirst) !=
        idOf(withPath, primec::BuildIdSource::FullHash));
}

TEST_CASE("build id: identical builds give identical ids and a different build differs") {
  const std::string uuidA = uuidBytes('a');
  const std::string uuidB = uuidBytes('b');
  // Two byte-identical builds, and a rebuilt compiler: other code and, as a linker does, another identifier.
  const std::filesystem::path first =
      writeImage("build_id_first.bin", machoImage(&uuidA, "code one"));
  const std::filesystem::path copy =
      writeImage("build_id_copy.bin", machoImage(&uuidA, "code one"));
  const std::filesystem::path rebuilt =
      writeImage("build_id_rebuilt.bin", machoImage(&uuidB, "code two"));
  for (const primec::BuildIdSource source :
       {primec::BuildIdSource::LinkerIdFirst, primec::BuildIdSource::FullHash}) {
    CHECK(idOf(first, source) == idOf(first, source));
    CHECK(idOf(first, source) == idOf(copy, source));
    CHECK(idOf(first, source) != idOf(rebuilt, source));
  }
  // The full hash follows the code even when the identifier is the same; the linker identifier is the linker's claim.
  const std::filesystem::path sameUuid =
      writeImage("build_id_same_uuid.bin", machoImage(&uuidA, "code two"));
  CHECK(idOf(first, primec::BuildIdSource::FullHash) !=
        idOf(sameUuid, primec::BuildIdSource::FullHash));
}

TEST_CASE(
    "build id: the executables of this build have distinct, stable ids of one of the two forms") {
  const std::filesystem::path primevm = PRIMESTRUCT_TEST_PRIMEVM_PATH;
  const std::string id = idOf(primevm, primec::BuildIdSource::LinkerIdFirst);
  CHECK((id.rfind("uuid-", 0) == 0 || id.rfind("sha256-", 0) == 0));
  CHECK(id == idOf(primevm, primec::BuildIdSource::LinkerIdFirst));
  // A byte-identical copy is the same build.
  const std::filesystem::path copy = writeImage("build_id_primevm_copy.bin", readAll(primevm));
  CHECK(idOf(copy, primec::BuildIdSource::LinkerIdFirst) == id);
  CHECK(idOf(copy, primec::BuildIdSource::FullHash) ==
        idOf(primevm, primec::BuildIdSource::FullHash));
}

TEST_CASE("build id: a missing executable is an error") {
  std::string id;
  std::string error;
  CHECK_FALSE(
      primec::executableBuildId(primec::testing::testScratchPath("build_id_missing.bin").string(),
                                primec::BuildIdSource::LinkerIdFirst,
                                id,
                                error));
  CHECK(error.find("cannot read the compiler executable") != std::string::npos);
}

namespace {

// Runs tools/compiler_source_id over the files `files` (name, bytes) written under a scratch directory, in the
// order given, and returns the literal it wrote.
std::string sourceIdOf(const std::string &name,
                       const std::vector<std::pair<std::string, std::string>> &files) {
  const std::filesystem::path dir = primec::testing::testScratchPath(name);
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir / "tree");
  std::string list;
  for (const auto &[file, bytes] : files) {
    std::filesystem::create_directories((dir / "tree" / file).parent_path());
    std::ofstream(dir / "tree" / file, std::ios::binary) << bytes;
    list += file + "\n";
  }
  std::ofstream(dir / "list.txt", std::ios::binary) << list;
  const std::string command = std::string("\"") + PRIMESTRUCT_TEST_COMPILER_SOURCE_ID_TOOL +
                              "\" \"" + (dir / "tree").string() + "\" \"" +
                              (dir / "list.txt").string() + "\" \"" + (dir / "id.inc").string() +
                              "\"";
  REQUIRE(std::system(command.c_str()) == 0);
  return readAll(dir / "id.inc");
}

} // namespace

TEST_CASE("source id: the compiled-in compiler build id is src- and a SHA-256") {
  const std::string &id = primec::compilerSourceBuildId();
  REQUIRE(id.size() == 4 + 64);
  CHECK(id.rfind("src-", 0) == 0);
  CHECK(id.find_first_not_of("0123456789abcdef", 4) == std::string::npos);
  // Another form never equals the id of an executable, so an archive of one kind is refused under the other.
  CHECK(id != idOf(std::filesystem::path(PRIMESTRUCT_TEST_PRIMEVM_PATH),
                   primec::BuildIdSource::LinkerIdFirst));
}

TEST_CASE(
    "source id: a tree hashes to the same literal and every change of it changes the literal") {
  const std::vector<std::pair<std::string, std::string>> base = {{"a/one.cpp", "int one;"},
                                                                 {"b.h", "two"}};
  const std::string id = sourceIdOf("source_id_base", base);
  CHECK(id.rfind("\"src-", 0) == 0);
  CHECK(id.size() == 4 + 64 + 3);
  CHECK(sourceIdOf("source_id_again", base) == id);
  // The bytes, the name, the set and the order of the files all count; the directory the tree sits in does not.
  CHECK(sourceIdOf("source_id_bytes", {{"a/one.cpp", "int one;\n"}, {"b.h", "two"}}) != id);
  CHECK(sourceIdOf("source_id_name", {{"a/uno.cpp", "int one;"}, {"b.h", "two"}}) != id);
  CHECK(sourceIdOf("source_id_less", {{"a/one.cpp", "int one;"}}) != id);
  CHECK(sourceIdOf("source_id_order", {{"b.h", "two"}, {"a/one.cpp", "int one;"}}) != id);
  // `ab` + `c` is not `a` + `bc`: the size of a file is part of what is hashed.
  CHECK(sourceIdOf("source_id_split_one", {{"x", "ab"}, {"y", "c"}}) !=
        sourceIdOf("source_id_split_two", {{"x", "a"}, {"y", "bc"}}));
}

TEST_CASE("source id: a file the list names but the tree lacks is an error") {
  const std::filesystem::path dir = primec::testing::testScratchPath("source_id_missing");
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir / "tree");
  std::ofstream(dir / "list.txt", std::ios::binary) << "gone.cpp\n";
  const std::string command = std::string("\"") + PRIMESTRUCT_TEST_COMPILER_SOURCE_ID_TOOL +
                              "\" \"" + (dir / "tree").string() + "\" \"" +
                              (dir / "list.txt").string() + "\" \"" + (dir / "id.inc").string() +
                              "\" 2> /dev/null";
  CHECK(std::system(command.c_str()) != 0);
  CHECK_FALSE(std::filesystem::exists(dir / "id.inc"));
}

TEST_SUITE_END();
