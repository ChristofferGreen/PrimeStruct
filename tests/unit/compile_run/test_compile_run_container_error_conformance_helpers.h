#pragma once

inline std::string makeContainerErrorConformanceSource() {
  return R"(
import /std/collections/*

[return<Result<ContainerError>>]
makeMissing() {
  return(/ContainerError/status(/ContainerError/missing_key()))
}

[return<Result<ContainerError>>]
makeUnknown() {
  return(99i32)
}

[return<int> effects(io_out)]
main() {
  [ContainerError] err{/ContainerError/missing_key()}
  [Result<ContainerError>] methodStatus{/ContainerError/status(err)}
  [Result<i32, ContainerError>] methodValueStatus{/ContainerError/result<i32>(err)}
  [i32] total{plus(plus(/ContainerError/missing_key().code, /ContainerError/index_out_of_bounds().code),
                  plus(/ContainerError/empty().code, /ContainerError/capacity_exceeded().code))}
  print_line(/ContainerError/why(/ContainerError/missing_key()))
  print_line(/ContainerError/why(/ContainerError/missing_key()))
  print_line(/ContainerError/why(err))
  print_line(Result.why(makeMissing()))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  print_line(Result.why(makeUnknown()))
  return(total)
}
)";
}

inline void expectContainerErrorConformance(const std::string &emitMode) {
  const std::string source = makeContainerErrorConformanceSource();
  const std::string srcPath = writeTemp("container_error_contract_" + emitMode + ".prime", source);
  const std::string outPath =
      (testScratchPath("") / ("primec_container_error_contract_" + emitMode + "_out.txt")).string();
  if (emitMode == "vm") {
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 10);
    CHECK(readFile(outPath) ==
          "container missing key\n"
          "container missing key\n"
          "container missing key\n"
          "container missing key\n"
          "container missing key\n"
          "container missing key\n"
          "container missing key\n"
          "container missing key\n"
          "container error\n");
    return;
  }

  const std::string exePath =
      (testScratchPath("") / ("primec_container_error_contract_" + emitMode + "_exe")).string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(runCmd) == 10);
  CHECK(readFile(outPath) ==
        "container missing key\n"
        "container missing key\n"
        "container missing key\n"
        "container missing key\n"
        "container missing key\n"
        "container missing key\n"
        "container missing key\n"
        "container missing key\n"
        "container error\n");
}
