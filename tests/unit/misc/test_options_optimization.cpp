#include "primec/support/Options.h"
#include "primec/support/OptionsParser.h"

#include "third_party/doctest.h"

#include <string>
#include <vector>

TEST_SUITE_BEGIN("primestruct.options.optimization");

namespace {

bool parseWith(primec::OptionsParserMode mode,
               std::vector<std::string> args,
               primec::Options &options,
               std::string &error) {
  std::vector<char *> argv;
  argv.reserve(args.size());
  for (std::string &arg : args) {
    argv.push_back(arg.data());
  }
  return primec::parseOptions(static_cast<int>(argv.size()), argv.data(), mode, options, error);
}

bool parsePrimec(std::vector<std::string> args, primec::Options &options, std::string &error) {
  return parseWith(primec::OptionsParserMode::Primec, std::move(args), options, error);
}

bool parsePrimevm(std::vector<std::string> args, primec::Options &options, std::string &error) {
  return parseWith(primec::OptionsParserMode::Primevm, std::move(args), options, error);
}

} // namespace

TEST_CASE("optimization options have no passes or reports set by default") {
  primec::Options options;
  std::string error;
  REQUIRE(parsePrimec({"primec", "--emit=ir", "/tmp/input.prime"}, options, error));
  CHECK(error.empty());
  CHECK(options.optimization.level == 0);
  CHECK_FALSE(options.optimization.levelSpecified);
  CHECK(options.optimization.enabledPasses.empty());
  CHECK(options.optimization.disabledPasses.empty());
  CHECK_FALSE(options.optimization.verifyEachPass);
  CHECK_FALSE(options.optimization.report);
}

TEST_CASE("runs and executables default to -O2, everything else to -O0") {
  const auto levelFor = [](std::vector<std::string> args, bool primevm) {
    primec::Options options;
    std::string error;
    const bool ok = primevm ? parsePrimevm(std::move(args), options, error)
                            : parsePrimec(std::move(args), options, error);
    REQUIRE_MESSAGE(ok, error);
    return options.optimization.level;
  };
  // Optimized by default.
  CHECK(levelFor({"primevm", "/tmp/input.prime"}, true) == 2);
  CHECK(levelFor({"primec", "/tmp/input.prime"}, false) == 2); // native by default
  for (const char *kind : {"native", "vm", "optexe", "optexe-ir", "optcpp", "optcpp-ir"}) {
    CAPTURE(kind);
    CHECK(levelFor({"primec", std::string("--emit=") + kind, "/tmp/input.prime"}, false) == 2);
  }
  // Not optimized by default.
  for (const char *kind :
       {"ir", "wasm", "glsl", "glsl-ir", "spirv", "cpp", "cpp-ir", "exe", "exe-ir"}) {
    CAPTURE(kind);
    CHECK(levelFor({"primec", std::string("--emit=") + kind, "/tmp/input.prime"}, false) == 0);
  }
  // Dumps show the stage the flags select, and debug sessions need every instruction.
  CHECK(levelFor({"primec", "--dump-stage", "ir-optimized", "/tmp/input.prime"}, false) == 0);
  CHECK(levelFor({"primevm", "--dump-stage", "ir-optimized", "/tmp/input.prime"}, true) == 0);
  CHECK(levelFor({"primevm", "--debug-json", "/tmp/input.prime"}, true) == 0);
  CHECK(levelFor({"primevm", "--debug-dap", "/tmp/input.prime"}, true) == 0);
  CHECK(levelFor({"primevm", "--debug-trace", "/tmp/trace.json", "/tmp/input.prime"}, true) == 0);
  // An explicit level always wins, including -O0.
  CHECK(levelFor({"primevm", "-O0", "/tmp/input.prime"}, true) == 0);
  CHECK(levelFor({"primec", "--emit=ir", "-O3", "/tmp/input.prime"}, false) == 3);
}

TEST_CASE("optimization level flags select the level and the last one wins") {
  for (int level = 0; level <= 3; ++level) {
    CAPTURE(level);
    primec::Options options;
    std::string error;
    REQUIRE(
        parsePrimec({"primec", "-O" + std::to_string(level), "/tmp/input.prime"}, options, error));
    CHECK(options.optimization.level == level);
    CHECK(options.optimization.levelSpecified);
  }

  primec::Options options;
  std::string error;
  REQUIRE(parsePrimec({"primec", "-O3", "/tmp/input.prime", "-O1"}, options, error));
  CHECK(options.optimization.level == 1);

  primec::Options explicitZero;
  REQUIRE(parsePrimevm({"primevm", "-O2", "-O0", "/tmp/input.prime"}, explicitZero, error));
  CHECK(explicitZero.optimization.level == 0);
  CHECK(explicitZero.optimization.levelSpecified);
}

TEST_CASE("unsupported optimization levels fail with a precise message") {
  for (const char *flag : {"-O", "-O4", "-O9", "-O2x", "-Os", "-O-1", "-OO"}) {
    CAPTURE(flag);
    primec::Options options;
    std::string error;
    CHECK_FALSE(parsePrimec({"primec", flag, "/tmp/input.prime"}, options, error));
    CHECK(error ==
          std::string("unsupported optimization level: ") + flag + " (expected -O0|-O1|-O2|-O3)");

    primec::Options vmOptions;
    std::string vmError;
    CHECK_FALSE(parsePrimevm({"primevm", flag, "/tmp/input.prime"}, vmOptions, vmError));
    CHECK(vmError == error);
  }
}

TEST_CASE("optimization pass flags accumulate in command-line order in both spellings") {
  primec::Options options;
  std::string error;
  REQUIRE(parsePrimec({"primec",
                       "--opt-pass=const-fold",
                       "--no-opt-pass",
                       "peephole",
                       "--opt-pass",
                       "dce",
                       "--no-opt-pass=inline_leaf",
                       "/tmp/input.prime"},
                      options,
                      error));
  CHECK(error.empty());
  CHECK(options.optimization.enabledPasses == std::vector<std::string>{"const-fold", "dce"});
  CHECK(options.optimization.disabledPasses == std::vector<std::string>{"peephole", "inline_leaf"});
}

TEST_CASE("optimization pass flags reject missing and malformed names") {
  struct Bad {
    std::vector<std::string> args;
    std::string expected;
  };
  const std::vector<Bad> cases = {
      {{"primec", "/tmp/input.prime", "--opt-pass"}, "--opt-pass requires a value"},
      {{"primec", "/tmp/input.prime", "--no-opt-pass"}, "--no-opt-pass requires a value"},
      {{"primec", "--opt-pass=", "/tmp/input.prime"}, "invalid --opt-pass value: "},
      {{"primec", "--no-opt-pass=", "/tmp/input.prime"}, "invalid --no-opt-pass value: "},
      {{"primec", "--opt-pass=Const-Fold", "/tmp/input.prime"},
       "invalid --opt-pass value: Const-Fold"},
      {{"primec", "--no-opt-pass=a b", "/tmp/input.prime"}, "invalid --no-opt-pass value: a b"},
      {{"primec", "--opt-pass", "../x", "/tmp/input.prime"}, "invalid --opt-pass value: ../x"},
  };
  for (const Bad &bad : cases) {
    CAPTURE(bad.expected);
    primec::Options options;
    std::string error;
    CHECK_FALSE(parsePrimec(bad.args, options, error));
    CHECK(error == bad.expected);
  }
}

TEST_CASE("optimization report and verify flags work for primec and primevm") {
  primec::Options options;
  std::string error;
  REQUIRE(parsePrimec(
      {"primec", "--opt-report", "--opt-verify-each", "/tmp/input.prime"}, options, error));
  CHECK(options.optimization.report);
  CHECK(options.optimization.verifyEachPass);

  primec::Options vmOptions;
  REQUIRE(parsePrimevm(
      {"primevm", "-O2", "--opt-report", "--opt-pass=cse", "--ir-inline", "/tmp/input.prime"},
      vmOptions,
      error));
  CHECK(vmOptions.optimization.level == 2);
  CHECK(vmOptions.optimization.report);
  CHECK_FALSE(vmOptions.optimization.verifyEachPass);
  CHECK(vmOptions.optimization.enabledPasses == std::vector<std::string>{"cse"});
  CHECK(vmOptions.inlineIrCalls);
}

TEST_CASE("optimization flags after the program-args separator belong to the program") {
  primec::Options options;
  std::string error;
  REQUIRE(
      parsePrimevm({"primevm", "/tmp/input.prime", "--", "-O3", "--opt-report"}, options, error));
  CHECK_FALSE(options.optimization.levelSpecified);
  CHECK_FALSE(options.optimization.report);
  CHECK(options.programArgs == std::vector<std::string>{"-O3", "--opt-report"});
}
