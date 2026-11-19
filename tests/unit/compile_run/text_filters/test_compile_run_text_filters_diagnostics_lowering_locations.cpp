#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

// Lowering errors point at the code the user wrote, name the target that lowers and spell generic types the way the
// program does. Every program here fails in lowering: a `try` over a Result whose error is a struct of three ints
// cannot be read back as an int.

namespace {

const char *const TryOverStructErrorSource = R"(import /std/result/*

[struct]
Oops() {
  [string] text{"x"utf8}
  [i32] a{1i32}
  [i32] b{2i32}
}

namespace Oops {
  [return<string>]
  why([Oops] err) {
    return("oops"utf8)
  }
}

[return<Result<i32, Oops>>]
f() {
  return(Result.ok(1i32))
}

swallow([Oops] err) {}

[return<int> on_error<Oops, /swallow>]
main() {
  [i32] v{try(f())}
  return(v)
}
)";

std::string
errorTextOf(const std::string &name, const std::string &emitKind, const std::string &source) {
  const std::string srcPath = writeTemp(name + ".prime", source);
  const std::string outPath = (testScratchPath("") / (name + ".out")).string();
  const std::string errPath = (testScratchPath("") / (name + "_err.txt")).string();
  const std::string cmd = "./primec --emit=" + emitKind + " " + quoteShellArg(srcPath) + " -o " +
                          quoteShellArg(outPath) + " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);
  return readFile(errPath);
}

} // namespace

TEST_CASE("a lowering error names the target that lowers, not the native backend") {
  struct Target {
    const char *emitKind;
    const char *name;
  };
  const Target targets[] = {
      {"cpp", "C++ backend requires int-backed"},
      {"exe", "exe backend requires int-backed"},
      {"optcpp", "optimized C++ backend requires int-backed"},
      {"wasm", "wasm backend requires int-backed"},
      {"ir", "IR backend requires int-backed"},
      {"native", "native backend requires int-backed"},
  };
  for (const Target &target : targets) {
    const std::string error = errorTextOf(std::string("lowering_names_") + target.emitKind,
                                          target.emitKind,
                                          TryOverStructErrorSource);
    INFO(target.emitKind);
    INFO(error);
    CHECK(error.find(target.name) != std::string::npos);
    if (std::string(target.emitKind) != "native") {
      CHECK(error.find("native backend") == std::string::npos);
    }
  }
}

TEST_SUITE_END();
