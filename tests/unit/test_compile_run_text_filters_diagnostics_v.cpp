#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

TEST_CASE("primevm emit-diagnostics reports structured semantic payload") {
  const std::string source = R"(
[return<int>]
main() {
  return(nope(1i32))
}
)";
  const std::string srcPath = writeTemp("primevm_emit_diagnostics_semantic.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primevm_emit_diagnostics_semantic_err.json").string();

  const std::string cmd =
      "./primevm " + quoteShellArg(srcPath) + " --emit-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: nope\"") != std::string::npos);
  CHECK(diagnostics.find("\"line\":0") == std::string::npos);
  CHECK(diagnostics.find("\"column\":0") == std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /main\"") != std::string::npos);
  CHECK(diagnostics.find("\"notes\":[\"stage: semantic\"]") != std::string::npos);
}

TEST_CASE("primec emit-diagnostics reports structured lowering payload") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal("alpha"utf8, "alpha"utf8))
}
)";
  const std::string srcPath = writeTemp("primec_emit_diagnostics_lowering.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_emit_diagnostics_lowering_err.json").string();

  const std::string cmd =
      "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main --emit-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC2001\"") != std::string::npos);
  CHECK(diagnostics.find("vm backend does not support string comparisons") != std::string::npos);
  CHECK(diagnostics.find("\"notes\":[\"backend: vm\"]") != std::string::npos);
}

TEST_CASE("emit-diagnostics reports argument payload for removed text-filters option") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("emit_diagnostics_text_filters_removed.prime", source);
  const std::string primecErrPath =
      (testScratchPath("") / "primec_emit_diagnostics_text_filters_removed_err.json").string();
  const std::string primecBareErrPath =
      (testScratchPath("") / "primec_emit_diagnostics_text_filters_removed_bare_err.json")
          .string();
  const std::string primevmErrPath =
      (testScratchPath("") / "primevm_emit_diagnostics_text_filters_removed_err.json").string();
  const std::string primevmBareErrPath =
      (testScratchPath("") / "primevm_emit_diagnostics_text_filters_removed_bare_err.json")
          .string();

  const std::string primecCmd = "./primec " + quoteShellArg(srcPath) +
                                " --emit-diagnostics --text-filters=none 2> " + quoteShellArg(primecErrPath);
  CHECK(runCommand(primecCmd) == 2);
  const std::string primecDiagnostics = readFile(primecErrPath);
  CHECK(primecDiagnostics.find("\"code\":\"PSC0001\"") != std::string::npos);
  CHECK(primecDiagnostics.find("\"message\":\"unknown option: --text-filters=none\"") != std::string::npos);
  CHECK(primecDiagnostics.find("\"severity\":\"error\"") != std::string::npos);
  CHECK(primecDiagnostics.find("Usage: primec") == std::string::npos);

  const std::string primecBareCmd = "./primec " + quoteShellArg(srcPath) +
                                    " --emit-diagnostics --text-filters 2> " + quoteShellArg(primecBareErrPath);
  CHECK(runCommand(primecBareCmd) == 2);
  const std::string primecBareDiagnostics = readFile(primecBareErrPath);
  CHECK(primecBareDiagnostics.find("\"code\":\"PSC0001\"") != std::string::npos);
  CHECK(primecBareDiagnostics.find("\"message\":\"unknown option: --text-filters\"") != std::string::npos);
  CHECK(primecBareDiagnostics.find("\"severity\":\"error\"") != std::string::npos);
  CHECK(primecBareDiagnostics.find("Usage: primec") == std::string::npos);

  const std::string primevmCmd = "./primevm " + quoteShellArg(srcPath) +
                                 " --emit-diagnostics --text-filters=none 2> " + quoteShellArg(primevmErrPath);
  CHECK(runCommand(primevmCmd) == 2);
  const std::string primevmDiagnostics = readFile(primevmErrPath);
  CHECK(primevmDiagnostics.find("\"code\":\"PSC0001\"") != std::string::npos);
  CHECK(primevmDiagnostics.find("\"message\":\"unknown option: --text-filters=none\"") != std::string::npos);
  CHECK(primevmDiagnostics.find("\"severity\":\"error\"") != std::string::npos);
  CHECK(primevmDiagnostics.find("Usage: primevm") == std::string::npos);

  const std::string primevmBareCmd = "./primevm " + quoteShellArg(srcPath) +
                                     " --emit-diagnostics --text-filters 2> " + quoteShellArg(primevmBareErrPath);
  CHECK(runCommand(primevmBareCmd) == 2);
  const std::string primevmBareDiagnostics = readFile(primevmBareErrPath);
  CHECK(primevmBareDiagnostics.find("\"code\":\"PSC0001\"") != std::string::npos);
  CHECK(primevmBareDiagnostics.find("\"message\":\"unknown option: --text-filters\"") != std::string::npos);
  CHECK(primevmBareDiagnostics.find("\"severity\":\"error\"") != std::string::npos);
  CHECK(primevmBareDiagnostics.find("Usage: primevm") == std::string::npos);
}

TEST_CASE("primec list transforms prints metadata") {
  const std::string outPath =
      (testScratchPath("") / "primec_list_transforms.txt").string();
  const std::string listCmd = "./primec --list-transforms > " + quoteShellArg(outPath);

  CHECK(runCommand(listCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("name\tphase\taliases\tavailability\n") != std::string::npos);
  CHECK(output.find("append_operators\ttext\t-\tprimec,primevm\n") != std::string::npos);
  CHECK(output.find("on_error\tsemantic\t-\tprimec,primevm\n") != std::string::npos);
  CHECK(output.find("single_type_to_return\tsemantic\t-\tprimec,primevm\n") != std::string::npos);
}

TEST_CASE("primec and primevm list transforms match") {
  const std::string primecOut =
      (testScratchPath("") / "primec_list_transforms_parity.txt").string();
  const std::string primevmOut =
      (testScratchPath("") / "primevm_list_transforms_parity.txt").string();

  const std::string primecCmd = "./primec --list-transforms > " + quoteShellArg(primecOut);
  const std::string primevmCmd = "./primevm --list-transforms > " + quoteShellArg(primevmOut);
  CHECK(runCommand(primecCmd) == 0);
  CHECK(runCommand(primevmCmd) == 0);
  CHECK(readFile(primecOut) == readFile(primevmOut));
}


TEST_SUITE_END();
