#include "ir_link_test_support.h"

#include "primec/ir/Ir.h"
#include "primec/ir/IrSerializer.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <sys/wait.h>
#include <vector>

using namespace primec;
using namespace ir_link_test;

// `primevm <program> --link <module>` and the schema version 38 of the serialized IR (TODO-5922).
TEST_SUITE_BEGIN("primestruct.runtime.ir_link_cli");

namespace {

void setVersion(std::vector<uint8_t> &data, uint32_t version) {
  for (size_t i = 0; i < 4; ++i) {
    data[4 + i] = static_cast<uint8_t>((version >> (8 * i)) & 0xFF);
  }
}

std::vector<uint8_t> bytesOf(const IrModule &module) {
  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE_MESSAGE(serializeIr(module, bytes, error), error);
  return bytes;
}

std::filesystem::path writeModule(const std::filesystem::path &path, const IrModule &module) {
  const std::vector<uint8_t> bytes = bytesOf(module);
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  return path;
}

struct CliRun {
  int exitCode = -1;
  std::string output;
};

CliRun runPrimevm(const std::filesystem::path &dir, const std::string &arguments) {
  const std::filesystem::path out = dir / "out.txt";
  const std::string command =
      "\"" PRIMESTRUCT_TEST_PRIMEVM_PATH "\" " + arguments + " > \"" + out.string() + "\" 2>&1";
  const int status = std::system(command.c_str());
  CliRun result;
  result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  std::ifstream in(out);
  result.output.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return result;
}

std::string quoted(const std::filesystem::path &path) {
  return "\"" + path.string() + "\"";
}

} // namespace

TEST_CASE("primevm --link runs a serialized program with a serialized library") {
  const auto dir = primec::testing::testScratchDir("ir_link_cli_run");
  const auto program = writeModule(dir / "program.psir", consumerModule());
  const auto library = writeModule(dir / "library.psir", libraryModule());
  const CliRun linked = runPrimevm(dir, quoted(program) + " --link " + quoted(library));
  CHECK(linked.exitCode == 21);
  CHECK(linked.output.empty());
  // `--link=<path>` is the same flag.
  CHECK(runPrimevm(dir, quoted(program) + " --link=" + quoted(library)).exitCode == 21);
}

TEST_CASE("primevm --link reports an import no module defines") {
  const auto dir = primec::testing::testScratchDir("ir_link_cli_undefined");
  const auto program = writeModule(dir / "program.psir", consumerModule());
  const auto other = writeModule(
      dir / "other.psir", moduleOf({functionOf("/lib/other", 0, {{IrOpcode::ReturnVoid, 0}})}, -1));
  const CliRun linked = runPrimevm(dir, quoted(program) + " --link " + quoted(other));
  CHECK(linked.exitCode == 3);
  CHECK(linked.output.find("undefined symbol /lib/add_one (library lib)") != std::string::npos);
  // The program alone is not runnable either.
  const CliRun alone = runPrimevm(dir, quoted(program));
  CHECK(alone.exitCode == 3);
  CHECK(alone.output.find("/lib/add_one") != std::string::npos);
}

TEST_CASE("primevm --link reports a duplicate definition and a missing file") {
  const auto dir = primec::testing::testScratchDir("ir_link_cli_errors");
  const auto program = writeModule(dir / "program.psir", libraryModule());
  // libraryModule has no entry; give the program one that defines /lib/twice as well.
  IrModule twice =
      moduleOf({functionOf("/main", 0, {{IrOpcode::PushI32, 3}, {IrOpcode::ReturnI32, 0}}),
                functionOf("/lib/twice", 1, {{IrOpcode::ReturnI32, 0}})});
  const auto both = writeModule(dir / "both.psir", twice);
  const CliRun duplicate = runPrimevm(dir, quoted(both) + " --link " + quoted(program));
  CHECK(duplicate.exitCode == 3);
  CHECK(duplicate.output.find("duplicate definition of /lib/twice") != std::string::npos);
  const CliRun missing = runPrimevm(dir, quoted(both) + " --link " + quoted(dir / "absent.psir"));
  CHECK(missing.exitCode == 3);
  CHECK(missing.output.find("failed to read IR module") != std::string::npos);
}

TEST_CASE("primevm --link prints strings of both modules") {
  const auto dir = primec::testing::testScratchDir("ir_link_cli_strings");
  IrModule program =
      moduleOf({functionOf("/main",
                           0,
                           {{IrOpcode::PrintString, encodePrintStringImm(1, PrintFlagNewline)},
                            {IrOpcode::CallVoid, 1},
                            {IrOpcode::PushI32, 0},
                            {IrOpcode::ReturnI32, 0}}),
                importOf("/lib/greet", 0, "lib")});
  program.stringTable = {"unused", "from the program"};
  IrModule library =
      moduleOf({functionOf("/lib/greet",
                           0,
                           {{IrOpcode::PrintString, encodePrintStringImm(1, PrintFlagNewline)},
                            {IrOpcode::ReturnVoid, 0}})},
               -1);
  library.stringTable = {"unused too", "from the library"};
  const CliRun linked = runPrimevm(dir,
                                   quoted(writeModule(dir / "program.psir", program)) + " --link " +
                                       quoted(writeModule(dir / "library.psir", library)));
  CHECK(linked.exitCode == 0);
  CHECK(linked.output == "from the program\nfrom the library\n");
}

TEST_CASE("schema 38: a module with imports and no entry round trips") {
  const IrModule library = libraryModule();
  IrModule withImport = consumerModule();
  withImport.entryIndex = -1;
  std::string error;
  IrModule back;
  REQUIRE_MESSAGE(deserializeIr(bytesOf(withImport), back, error), error);
  CHECK(back.schemaVersion == IrSchemaVersion);
  CHECK(IrSchemaVersion >= 38u);
  CHECK(back.entryIndex == -1);
  REQUIRE(back.functions.size() == 2);
  CHECK_FALSE(back.functions[0].isImport);
  CHECK(back.functions[1].isImport);
  CHECK(back.functions[1].importLibrary == "lib");
  CHECK(back.functions[1].parameterCount == 1);
  CHECK(back.functions[1].instructions.empty());
  REQUIRE_MESSAGE(deserializeIr(bytesOf(library), back, error), error);
  CHECK(back.entryIndex == -1);
}

TEST_CASE(
    "schema 38: the writer rejects an import with a body and the loader a bad import record") {
  IrModule broken = consumerModule();
  broken.functions[1].instructions.push_back({IrOpcode::ReturnVoid, 0});
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_FALSE(serializeIr(broken, bytes, error));
  CHECK(error == "IR import /lib/add_one has instructions");
  IrModule outOfRange = consumerModule();
  outOfRange.entryIndex = 5;
  CHECK_FALSE(serializeIr(outOfRange, bytes, error));
  CHECK(error == "invalid IR entry index");

  // The import record names function 1; pointing it at function 7 is refused.
  std::vector<uint8_t> good = bytesOf(consumerModule());
  const std::string library = "lib";
  const size_t recordOffset = good.size() - (4 + 4 + library.size());
  good[recordOffset] = 7;
  IrModule loaded;
  CHECK_FALSE(deserializeIr(good, loaded, error));
  CHECK(error == "invalid IR import function index");
  good.resize(good.size() - 2);
  CHECK_FALSE(deserializeIr(good, loaded, error));
}

TEST_CASE(
    "schema 38: a version 37 module loads, has no imports and is written back as version 38") {
  const IrModule plain =
      moduleOf({functionOf("/main", 0, {{IrOpcode::PushI32, 12}, {IrOpcode::ReturnI32, 0}})});
  std::vector<uint8_t> old = bytesOf(plain);
  // A version 37 file ends after the host imports, where version 38 adds the import count (zero here).
  REQUIRE(old.size() > 4);
  for (size_t i = old.size() - 4; i < old.size(); ++i) {
    CHECK(old[i] == 0);
  }
  old.resize(old.size() - 4);
  setVersion(old, 37u);
  IrModule loaded;
  std::string error;
  REQUIRE_MESSAGE(deserializeIr(old, loaded, error), error);
  CHECK(loaded.schemaVersion == 37u);
  CHECK_FALSE(loaded.functions[0].isImport);
  CHECK(loaded.entryIndex == 0);
  CHECK(run(loaded).value == 12);
  std::vector<uint8_t> rewritten;
  REQUIRE_MESSAGE(serializeIr(loaded, rewritten, error), error);
  CHECK(rewritten[4] == static_cast<uint8_t>(IrSchemaVersion));
  CHECK(rewritten == bytesOf(plain));
  // A version 37 module has no way to say "no entry": the same bytes as an entry index of -1 are out of range.
  std::vector<uint8_t> noEntry = old;
  for (size_t i = 0; i < 4; ++i) {
    noEntry[12 + i] = 0xFF;
  }
  CHECK_FALSE(deserializeIr(noEntry, loaded, error));
  CHECK(error == "invalid IR entry index");
}

TEST_SUITE_END();
