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

namespace {

std::string
diagnosticsJsonOf(const std::string &name, const std::string &source, std::string &srcPathOut) {
  srcPathOut = writeTemp(name + ".prime", source);
  const std::string errPath = (testScratchPath("") / (name + "_err.json")).string();
  const std::string cmd = "./primec --emit=vm " + quoteShellArg(srcPathOut) +
                          " --entry /main --emit-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);
  return readFile(errPath);
}

} // namespace

TEST_CASE("a lowering error is reported at the try expression in the user's file") {
  std::string srcPath;
  const std::string diagnostics =
      diagnosticsJsonOf("lowering_location_try", TryOverStructErrorSource, srcPath);
  INFO(diagnostics);
  CHECK(diagnostics.find("\"code\":\"PSC2001\"") != std::string::npos);
  CHECK(diagnostics.find("\"primary_span\":{\"file\":\"" + srcPath +
                         "\",\"line\":26,\"column\":11,") != std::string::npos);
}

TEST_CASE("a lowering error names a generic type the way the program spells it") {
  std::string srcPath;
  const std::string diagnostics =
      diagnosticsJsonOf("lowering_name_try", TryOverStructErrorSource, srcPath);
  INFO(diagnostics);
  CHECK(diagnostics.find("vm backend requires int-backed stdlib Result error payloads (try over "
                         "Result<i32, Oops>)") != std::string::npos);
  CHECK(diagnostics.find("__t") == std::string::npos);
  CHECK(diagnostics.find("__arity") == std::string::npos);
}

TEST_CASE("a wrong on_error handler arity is reported at the try that reaches it") {
  const std::string source = R"(import /std/result/*

[struct]
Oops() {
  [i32] a{1i32}
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

onErr([Oops] err, [i32] extra) {}

[return<int> on_error<Oops, /onErr>]
main() {
  [i32] v{try(f())}
  return(v)
}
)";
  std::string srcPath;
  const std::string diagnostics =
      diagnosticsJsonOf("lowering_location_on_error_arity", source, srcPath);
  INFO(diagnostics);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /onErr\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"primary_span\":{\"file\":\"" + srcPath +
                         "\",\"line\":24,\"column\":11,") != std::string::npos);
}

TEST_CASE("a lowering error prints its location in the text form") {
  const std::string srcPath = writeTemp("lowering_location_text.prime", TryOverStructErrorSource);
  const std::string errPath = (testScratchPath("") / "lowering_location_text_err.txt").string();
  const std::string cmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " +
                          quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);
  const std::string error = readFile(errPath);
  INFO(error);
  CHECK(error.rfind(srcPath + ":26:11: error: VM lowering error: vm backend requires int-backed",
                    0) == 0);
}

TEST_SUITE_END();
