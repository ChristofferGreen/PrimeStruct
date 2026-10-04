#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

TEST_SUITE_BEGIN("primestruct.ir.optimizer_cli");

// End-to-end checks of the optimizer's command-line surface. They run the
// primec and primevm binaries from the build directory, like the compile-run
// suites do.

namespace {

struct CommandResult {
  int exitCode = -1;
  std::string out;
  std::string err;
};

std::string readText(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

CommandResult run(const std::string &command) {
  const std::filesystem::path outPath =
      primec::testing::testScratchPath("optimizer_cli/stdout.txt");
  const std::filesystem::path errPath =
      primec::testing::testScratchPath("optimizer_cli/stderr.txt");
  std::filesystem::create_directories(outPath.parent_path());
  const int status = std::system(
      (command + " > '" + outPath.string() + "' 2> '" + errPath.string() + "'").c_str());
  CommandResult result;
#if defined(__unix__) || defined(__APPLE__)
  result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#else
  result.exitCode = status;
#endif
  result.out = readText(outPath);
  result.err = readText(errPath);
  return result;
}

std::string writeSource(const std::string &name, const std::string &text) {
  const std::filesystem::path path = primec::testing::testScratchPath("optimizer_cli/" + name);
  std::filesystem::create_directories(path.parent_path());
  std::ofstream(path) << text;
  return path.string();
}

const char *FoldableProgram = R"(
[return<int> effects(io_out)]
main() {
  [i32 mut] total{0i32}
  assign(total, plus(multiply(2i32, 3i32), 4i32))
  print_line(total)
  return(0i32)
}
)";

bool contains(const std::string &text, const std::string &needle) {
  return text.find(needle) != std::string::npos;
}

} // namespace

TEST_CASE("opt-list prints the pass manifest for primec and primevm without an input file") {
  for (const char *binary : {"./primec", "./primevm"}) {
    CAPTURE(binary);
    const CommandResult result = run(std::string(binary) + " --opt-list");
    CHECK(result.exitCode == 0);
    CHECK(result.out.rfind("name\tdefault_level\ttargets\tdescription\n", 0) == 0);
    const size_t cfg = result.out.find("cfg-simplify\t-O1\t");
    const size_t fold = result.out.find("const-fold\t-O1\t");
    const size_t peephole = result.out.find("peephole\t-O1\t");
    const size_t deadStore = result.out.find("dead-store\t-O1\t");
    REQUIRE(cfg != std::string::npos);
    REQUIRE(fold != std::string::npos);
    REQUIRE(peephole != std::string::npos);
    REQUIRE(deadStore != std::string::npos);
    // Manifest order is execution order.
    CHECK(cfg < fold);
    CHECK(fold < peephole);
    CHECK(peephole < deadStore);
    // Control-flow rewriting is limited to the unstructured targets.
    CHECK(contains(result.out, "cfg-simplify\t-O1\tany,serialized,vm,native\t"));
  }
}

TEST_CASE("unknown pass names and unsupported targets are diagnosed") {
  const std::string source = writeSource("fold_errors.prime", FoldableProgram);

  CommandResult result = run("./primevm " + source + " --opt-pass=nope");
  CHECK(result.exitCode == 2);
  CHECK(contains(result.err,
                 "IR optimization error: unknown optimization pass: nope (see --opt-list)"));

  result = run("./primec --emit=vm " + source + " --no-opt-pass=nope");
  CHECK(result.exitCode == 2);
  CHECK(contains(result.err, "unknown optimization pass: nope"));

  // Naming a pass for a target it cannot handle is an error; the level alone
  // just skips it for that target.
  const std::string wasmPath = primec::testing::testScratchPath("optimizer_cli/out.wasm").string();
  result = run("./primec --emit=wasm " + source + " -o " + wasmPath + " --opt-pass=cfg-simplify");
  CHECK(result.exitCode == 2);
  CHECK(contains(result.err, "optimization pass cfg-simplify does not support the wasm target"));
  result = run("./primec --emit=wasm " + source + " -o " + wasmPath + " -O1");
  CHECK(result.exitCode == 0);
}

TEST_CASE("opt-report describes the selected passes and what changed") {
  const std::string source = writeSource("fold_report.prime", FoldableProgram);

  CommandResult result = run("./primevm " + source + " -O1 --opt-report");
  CHECK(result.exitCode == 0);
  CHECK(result.out == "10\n");
  CHECK(result.err.rfind("optimization_report_v1\nlevel=1\n", 0) == 0);
  CHECK(contains(result.err, "selected_passes=cfg-simplify,const-fold,peephole,dead-store\n"));
  CHECK(contains(result.err, "instructions_before=16\ninstructions_after=6\n"));
  CHECK(contains(result.err, "round 1 const-fold: "));

  // primevm optimizes at -O2 when no level is given.
  result = run("./primevm " + source + " --opt-report");
  CHECK(result.exitCode == 0);
  CHECK(result.out == "10\n");
  CHECK(contains(result.err,
                 "level=2\nselected_passes=cfg-simplify,const-fold,peephole,copy-prop,dead-store,"
                 "loop-rotate\n"));

  // At -O0 nothing is selected, but the report is still produced on request.
  result = run("./primevm " + source + " -O0 --opt-report");
  CHECK(result.exitCode == 0);
  CHECK(contains(result.err, "level=0\nselected_passes=\n"));
  CHECK(contains(result.err, "instructions_before=16\ninstructions_after=16\n"));

  // --no-opt-pass wins over the level.
  result = run("./primevm " + source +
               " -O1 --no-opt-pass=const-fold --no-opt-pass=dead-store --opt-report");
  CHECK(contains(result.err, "selected_passes=cfg-simplify,peephole\n"));
}

TEST_CASE("dump-stage prints the lowered module before and after optimization") {
  const std::string source = writeSource("fold_dump.prime", FoldableProgram);
  const std::string header = "ir_module_v1 schema=27\n"
                             "entry=/main (function 0)\n"
                             "string_table: 0\n"
                             "host_imports: 0\n"
                             "struct_layouts: 0\n"
                             "functions: 1\n";

  const CommandResult lowered = run("./primec --dump-stage ir-lowered " + source);
  CHECK(lowered.exitCode == 0);
  CHECK(lowered.out ==
        header +
            "function 0 /main parameters=0 effects=0x1 capabilities=0x0 locals=1 instructions=16\n"
            "  0000  PushI32 0\n"
            "  0001  StoreLocal local 0\n"
            "  0002  PushI32 2\n"
            "  0003  PushI32 3\n"
            "  0004  MulI32\n"
            "  0005  SextI32\n"
            "  0006  PushI32 4\n"
            "  0007  AddI32\n"
            "  0008  SextI32\n"
            "  0009  Dup\n"
            "  0010  StoreLocal local 0\n"
            "  0011  Pop\n"
            "  0012  LoadLocal local 0\n"
            "  0013  PrintI32 flags=newline\n"
            "  0014  PushI32 0\n"
            "  0015  ReturnI32\n");

  const CommandResult optimized = run("./primec --dump-stage ir-optimized -O1 " + source);
  CHECK(optimized.exitCode == 0);
  CHECK(optimized.out ==
        header +
            "function 0 /main parameters=0 effects=0x1 capabilities=0x0 locals=1 instructions=6\n"
            "  0000  PushI32 10\n"
            "  0001  StoreLocal local 0\n"
            "  0002  LoadLocal local 0\n"
            "  0003  PrintI32 flags=newline\n"
            "  0004  PushI32 0\n"
            "  0005  ReturnI32\n");

  // ir-lowered never optimizes, whatever the level, and primevm prints the same text.
  CHECK(run("./primec --dump-stage ir-lowered -O3 " + source).out == lowered.out);
  CHECK(run("./primevm --dump-stage ir-lowered " + source).out == lowered.out);
  CHECK(run("./primevm --dump-stage ir-optimized -O1 " + source).out == optimized.out);
  // Without -O the optimized stage equals the lowered one.
  CHECK(run("./primec --dump-stage ir-optimized " + source).out == lowered.out);
}

TEST_CASE("serialized output is unchanged at -O0 and shrinks at -O1 with the same behavior") {
  const std::string source = writeSource("fold_psir.prime", FoldableProgram);
  const std::string plain = primec::testing::testScratchPath("optimizer_cli/plain.psir").string();
  const std::string zero = primec::testing::testScratchPath("optimizer_cli/zero.psir").string();
  const std::string one = primec::testing::testScratchPath("optimizer_cli/one.psir").string();

  REQUIRE(run("./primec --emit=ir " + source + " -o " + plain).exitCode == 0);
  REQUIRE(run("./primec --emit=ir " + source + " -O0 -o " + zero).exitCode == 0);
  REQUIRE(run("./primec --emit=ir " + source + " -O1 -o " + one).exitCode == 0);
  CHECK(readText(plain) == readText(zero));
  CHECK(readText(one) != readText(plain));
  CHECK(readText(one).size() < readText(plain).size());

  const CommandResult baseline = run("./primevm " + source);
  const CommandResult optimized = run("./primevm " + source + " -O1 --opt-verify-each");
  CHECK(baseline.exitCode == optimized.exitCode);
  CHECK(baseline.out == optimized.out);
  CHECK(baseline.out == "10\n");
}

TEST_CASE("optexe and optcpp emit kinds produce a fast executable and C++ source") {
  const std::string source = writeSource("fold_optexe.prime", FoldableProgram);
  const std::string exePath =
      primec::testing::testScratchPath("optimizer_cli/fold_optexe").string();
  const std::string cppPath =
      primec::testing::testScratchPath("optimizer_cli/fold_optcpp.cpp").string();

  // The default host level is -O2; the IR level only changes what the emitter sees.
  for (const char *level : {"", "-O0", "-O3"}) {
    CAPTURE(level);
    REQUIRE(run("./primec --emit=optexe " + source + " " + level + " -o " + exePath).exitCode == 0);
    const CommandResult executed = run("'" + exePath + "'");
    CHECK(executed.exitCode == 0);
    CHECK(executed.out == "10\n");
  }

  REQUIRE(run("./primec --emit=optcpp " + source + " -O1 -o " + cppPath).exitCode == 0);
  const std::string generated = readText(cppPath);
  CHECK(contains(generated, "goto L") == false); // straight-line program
  CHECK(contains(generated, "int main(int argc, char **argv)"));
  CHECK(contains(generated, "VM error: "));
  // Folding happened before emission: the constant 10 is in the source, the
  // multiplication is not.
  CHECK(contains(generated, "UINT64_C(0xa)"));
  CHECK_FALSE(contains(generated, "s0 * s1"));
}

#if defined(__x86_64__) && (defined(__linux__) || defined(__APPLE__))
TEST_CASE("opt-report lists the native functions that were register-allocated") {
  const std::string source = writeSource("fold_native.prime", FoldableProgram);
  const std::string exePath =
      primec::testing::testScratchPath("optimizer_cli/fold_native").string();

  // -O2 (the native default) allocates registers; -O1 keeps the template emitter.
  CommandResult result =
      run("./primec --emit=native " + source + " -o " + exePath + " --opt-report");
  CHECK(result.exitCode == 0);
  CHECK(contains(result.err,
                 "native_register_allocation_v1\nfunctions=1 register_allocated=1\n"
                 "function /main: registers spill_slots=0\n"));
  result = run("./primec --emit=native " + source + " -o " + exePath + " -O1 --opt-report");
  CHECK(result.exitCode == 0);
  CHECK(contains(result.err,
                 "native_register_allocation_v1\nfunctions=1 register_allocated=0\n"
                 "function /main: template\n"));

  // The benchmark loops must not silently fall back to the template emitter.
  const std::filesystem::path benchmarks =
      std::filesystem::current_path().parent_path() / "benchmarks";
  for (const char *name : {"aggregate", "json_scan", "json_parse"}) {
    CAPTURE(name);
    const std::string benchmark = (benchmarks / (std::string(name) + ".prime")).string();
    result = run("./primec --emit=native '" + benchmark + "' -o " + exePath + " --opt-report");
    CHECK(result.exitCode == 0);
    CHECK(contains(result.err, "native_register_allocation_v1\n"));
    CHECK(contains(result.err, "function /main: registers "));
    CHECK_FALSE(contains(result.err, ": template"));
  }
}
#endif
