// /std/dir (docs/spec/stdlib-reference.md): listDirectory on the VM and through optexe over a fixture
// directory. Modification times are set explicitly with utimes, never read from the clock, so the expected listing is
// the same on every run.
#include "primec/embed/Script.h"
#include "primec/embed/ScriptEngine.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.backend.file_listing");

namespace {
const std::string Primec = PRIMESTRUCT_PRIMEC_PATH;

std::string scratch(const std::string &name) {
  const std::filesystem::path dir = primec::testing::testScratchDir("file_listing");
  std::filesystem::create_directories(dir);
  return (dir / name).string();
}

std::string readFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::string shellQuote(const std::string &text) { return "'" + text + "'"; }

int run(const std::string &command) {
  const int status = std::system(command.c_str());
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

void makeFile(const std::filesystem::path &path, const std::string &content, time_t seconds) {
  std::ofstream(path, std::ios::binary) << content;
  const timeval times[2] = {{seconds, 0}, {seconds, 0}};
  REQUIRE(utimes(path.c_str(), times) == 0);
}

// A directory with a plain file, a file whose name sorts before the others bytewise (uppercase, dot), an empty file,
// a subdirectory and a link to nothing. Names that would sort differently under a case-insensitive or locale order
// are the point: "Zed" is before "alpha" and ".hidden" before both.
std::filesystem::path makeFixture(const std::string &name) {
  const std::filesystem::path root = scratch(name);
  std::filesystem::create_directories(root / "sub");
  makeFile(root / "alpha.prime", "hello", 1000000000);
  makeFile(root / "beta.txt", "", 1500000000);
  makeFile(root / "Zed", "abc", 1600000000);
  makeFile(root / ".hidden", "0123456789", 1700000000);
  makeFile(root / "sub" / "inner.prime", "x", 1800000000);
  std::filesystem::create_symlink(root / "nowhere", root / "dangling");
  const timeval directoryTimes[2] = {{1900000000, 0}, {1900000000, 0}};
  REQUIRE(utimes((root / "sub").c_str(), directoryTimes) == 0);
  return root;
}

struct ExpectedEntry {
  int kind;
  long long size;
  long long seconds;
  std::string name;
};

// print_line takes no String, so the program prints a name as its byte codes, each followed by a comma.
std::string expectedListing() {
  const std::vector<ExpectedEntry> entries = {{0, 10, 1700000000, ".hidden"},      {0, 3, 1600000000, "Zed"},
                                              {0, 5, 1000000000, "alpha.prime"},   {0, 0, 1500000000, "beta.txt"},
                                              {2, 0, 0, "dangling"},               {1, 0, 1900000000, "sub"}};
  std::string out = "ok\n";
  for (const ExpectedEntry &entry : entries) {
    out += std::to_string(entry.kind) + " " + std::to_string(entry.size) + " " + std::to_string(entry.seconds) + " ";
    for (const unsigned char ch : entry.name) {
      out += std::to_string(ch) + ",";
    }
    out += "\n";
  }
  return out;
}

// Prints "ok" and one "kind size seconds name" line per entry, or the error text of a failed listing.
std::string program(const std::filesystem::path &directory) {
  return "import /std/dir/*\n"
         "import /std/file/*\n"
         "import /std/collections/vector/*\n"
         "import /std/text/*\n"
         "import /std/text/view/*\n\n"
         "[return<void> effects(io_out, file_read, heap_alloc)]\n"
         "show([String] path) {\n"
         "  [vector<DirEntry> mut] entries{vector<DirEntry>{}}\n"
         "  [Result<FileError>] listed{listDirectory(path.view(), entries)}\n"
         "  if(Result.error(listed)) {\n"
         "    print_line(Result.why(listed))\n"
         "    return()\n"
         "  }\n"
         "  print_line(\"ok\")\n"
         "  for([i32 mut] i{0i32}, i < entries.count(), i++) {\n"
         "    print(entries[i].kind)\n    print(\" \")\n    print(entries[i].size)\n    print(\" \")\n"
         "    print(entries[i].modifiedSeconds)\n    print(\" \")\n"
         "    for([i32 mut] at{0i32}, at < entries[i].name.count(), at++) {\n"
         "      print(entries[i].name.byteAt(at))\n      print(\",\")\n    }\n"
         "    print_line(\"\")\n"
         "  }\n"
         "}\n\n"
         "[return<int> effects(io_out, file_read, heap_alloc)]\n"
         "main() {\n"
         "  show(stringFrom(\"" +
         directory.string() +
         "\"))\n"
         "  return(0i32)\n"
         "}\n";
}

std::string vmOutput(const std::string &source) {
  ScriptEngine engine;
  const auto script = engine.compileSource("/file_listing.prime", source);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  std::string out;
  primec::embed::RunOptions options;
  options.output = [&out](int fd, std::string_view chunk) {
    if (fd == 1) {
      out += chunk;
    }
  };
  const auto result = script.run({}, options);
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.exitCode == 0);
  return out;
}

std::string optexeOutput(const std::string &name, const std::string &source) {
  const std::string file = scratch(name + ".prime");
  std::ofstream(file, std::ios::binary) << source;
  const std::string exe = scratch(name + "_optexe");
  const std::string log = exe + ".log";
  const int status = run(shellQuote(Primec) + " --emit=optexe " + shellQuote(file) + " -o " + shellQuote(exe) + " > " +
                         shellQuote(log) + " 2>&1");
  REQUIRE_MESSAGE(status == 0, readFile(log));
  const std::string out = exe + ".out";
  CHECK(run(shellQuote(exe) + " > " + shellQuote(out)) == 0);
  return readFile(out);
}
} // namespace

TEST_CASE("listDirectory lists a fixture directory sorted bytewise with kind, size and time on the VM") {
  CHECK(vmOutput(program(makeFixture("vm_fixture"))) == expectedListing());
}

TEST_CASE("listDirectory lists a fixture directory sorted bytewise with kind, size and time through optexe") {
  CHECK(optexeOutput("listing", program(makeFixture("optexe_fixture"))) == expectedListing());
}

TEST_CASE("listDirectory lists an empty directory as no entries on the VM") {
  const std::filesystem::path empty = scratch("vm_empty");
  std::filesystem::create_directories(empty);
  CHECK(vmOutput(program(empty)) == "ok\n");
}

TEST_CASE("listDirectory returns the error value for a missing directory on the VM") {
  const std::filesystem::path root = makeFixture("vm_missing");
  CHECK(vmOutput(program(root / "no_such_directory")) == "ENOENT\n");
}

TEST_CASE("listDirectory returns the error value for a missing directory through optexe") {
  const std::filesystem::path root = makeFixture("optexe_missing");
  CHECK(optexeOutput("listing_missing", program(root / "no_such_directory")) == "ENOENT\n");
}

TEST_CASE("listDirectory returns the error value for a path that is a file on the VM") {
  const std::filesystem::path root = makeFixture("vm_notdir");
  CHECK(vmOutput(program(root / "Zed")) == "ENOTDIR\n");
}

TEST_CASE("a program that does not call listDirectory has no directory host import") {
  // The host function is declared by /std/dir alone; importing /std/file does not pull it in.
  ScriptEngine engine;
  const auto script = engine.compileSource("/file_no_listing.prime", R"(
import /std/file/*

[return<int>]
main() {
  return(0i32)
}
)");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  CHECK(result.ok);
  CHECK(result.exitCode == 0);
}

TEST_SUITE_END();
