#pragma once

inline std::string makeContainerErrorConformanceSource() {
  return R"(
import /std/collections/*

[return<Result<ContainerError>>]
makeMissing() {
  return(containerMissingKey().code)
}

[return<Result<ContainerError>>]
makeUnknown() {
  return(99i32)
}

[return<int> effects(io_out)]
main() {
  [i32] total{plus(plus(containerMissingKey().code, containerIndexOutOfBounds().code),
                  plus(containerEmpty().code, containerCapacityExceeded().code))}
  print_line(Result.why(makeMissing()))
  print_line(Result.why(makeUnknown()))
  return(total)
}
)";
}

inline void expectContainerErrorConformance(const std::string &emitMode) {
  const std::string source = makeContainerErrorConformanceSource();
  const std::string srcPath = writeTemp("container_error_contract_" + emitMode + ".prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / ("primec_container_error_contract_" + emitMode + "_out.txt")).string();
  if (emitMode == "vm") {
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 10);
    CHECK(readFile(outPath) == "container missing key\ncontainer error\n");
    return;
  }

  const std::string exePath =
      (std::filesystem::temp_directory_path() / ("primec_container_error_contract_" + emitMode + "_exe")).string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(runCmd) == 10);
  CHECK(readFile(outPath) == "container missing key\ncontainer error\n");
}
