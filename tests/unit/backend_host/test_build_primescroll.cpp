// examples/apps/text_editor/make.prime (TODO-5920) builds PrimeScroll the way scripts/build_native_app.sh does. The
// case runs `primec build --dry-run` on a copy of that folder and compares the graph with
// tests/fixtures/build/primescroll_dry_run.golden.json, then checks the headless backend's link arguments. The cold
// compile of make.prime is the cost of the case (about 2.5 s); nothing is built, since linking the editor needs the
// UI backend libraries and takes longer than the gate allows for one test.
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

TEST_SUITE_BEGIN("primestruct.backend.build_primescroll");

namespace {
const std::string Primec = PRIMESTRUCT_PRIMEC_PATH;
const std::string SourceDir = PRIMESTRUCT_SOURCE_DIR;

std::string readFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

bool contains(const std::string &text, const std::string &part) {
  return text.find(part) != std::string::npos;
}

// Runs `primec build --dry-run <options>` in `dir`; the standard output goes to `out`, the diagnostics to `out.err`.
int dryRun(const std::string &dir, const std::string &options, const std::string &out) {
  const std::string command = "'" + Primec + "' build '" + dir + "' --dry-run " + options + " > '" + out +
                              "' 2> '" + out + ".err'";
  const int status = std::system(command.c_str());
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
} // namespace

TEST_CASE("the PrimeScroll make.prime prints the graph of the script's optexe build") {
  const std::filesystem::path dir = primec::testing::testScratchDir("build_primescroll");
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  std::filesystem::copy_file(SourceDir + "/examples/apps/text_editor/make.prime", dir / "make.prime");

  const std::string out = (dir / "dry_run.json").string();
  const int status = dryRun(dir.string(), "", out);
  CHECK_MESSAGE(status == 0, readFile(out + ".err"));
  CHECK(readFile(out) == readFile(SourceDir + "/tests/fixtures/build/primescroll_dry_run.golden.json"));

  // The headless backend links the scripted test backend and the include directory, and no framework.
  const std::string headless = (dir / "headless.json").string();
  CHECK_MESSAGE(dryRun(dir.string(), "--option backend=headless --option build_dir=out_libs", headless) == 0,
                readFile(headless + ".err"));
  const std::string text = readFile(headless);
  CHECK(contains(text, "\"link_args\":[\"-I../../../include\",\"out_libs/libprimec_ui_headless_lib.a\","
                       "\"out_libs/libprimec_ui_highlight_lib.a\"]"));
  CHECK_FALSE(contains(text, "-framework"));
}

TEST_SUITE_END();
