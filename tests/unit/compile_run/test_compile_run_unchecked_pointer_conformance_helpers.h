#pragma once

inline std::string uncheckedPointerConformanceImportPath() {
  return "/std/collections/internal_buffer_unchecked/*";
}

inline std::string makeUncheckedPointerHelperSurfaceSource() {
  return R"(
import /std/collections/internal_buffer_unchecked/*

[effects(io_out, heap_alloc), return<int>]
main() {
  [Pointer<i32> mut] ptr{bufferAlloc<i32>(2i32)}
  [Pointer<i32> mut] first{bufferOffsetUnsafe<i32>(ptr, 0i32)}
  assign(dereference(first), 4i32)
  bufferWriteUnsafe<i32>(ptr, 1i32, 7i32)
  [i32] total{plus(bufferReadUnsafe<i32>(ptr, 0i32), bufferReadUnsafe<i32>(ptr, 1i32))}
  print_line(bufferReadUnsafe<i32>(ptr, 0i32))
  print_line(bufferReadUnsafe<i32>(ptr, 1i32))
  print_line(total)
  bufferFree<i32>(ptr)
  return(total)
}
)";
}

inline std::string makeUncheckedPointerGrowthSource() {
  return R"(
import /std/collections/internal_buffer_unchecked/*

[effects(io_out, heap_alloc), return<int>]
main() {
  [Pointer<i32> mut] ptr{bufferAlloc<i32>(2i32)}
  bufferWriteUnsafe<i32>(ptr, 0i32, 4i32)
  bufferWriteUnsafe<i32>(ptr, 1i32, 7i32)
  [Pointer<i32> mut] grown{bufferGrow<i32>(ptr, 3i32)}
  bufferWriteUnsafe<i32>(grown, 2i32, 9i32)
  [i32] total{plus(plus(bufferReadUnsafe<i32>(grown, 0i32), bufferReadUnsafe<i32>(grown, 1i32)),
                   bufferReadUnsafe<i32>(grown, 2i32))}
  print_line(bufferReadUnsafe<i32>(grown, 0i32))
  print_line(bufferReadUnsafe<i32>(grown, 1i32))
  print_line(bufferReadUnsafe<i32>(grown, 2i32))
  print_line(total)
  bufferFree<i32>(grown)
  return(total)
}
)";
}

inline void expectUncheckedPointerProgramRuns(const std::string &source,
                                              const std::string &nameStem,
                                              const std::string &emitMode,
                                              int expectedExitCode) {
  const std::string srcPath = writeTemp(nameStem + ".prime", source);
  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
    CHECK(runCommand(runCmd) == expectedExitCode);
    return;
  }

  const std::string exePath =
      (testScratchPath("") / (nameStem + "_exe")).string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == expectedExitCode);
}

inline void expectUncheckedPointerProgramRunsWithOutput(const std::string &source,
                                                        const std::string &nameStem,
                                                        const std::string &emitMode,
                                                        int expectedExitCode,
                                                        const std::string &expectedOutput) {
  const std::string srcPath = writeTemp(nameStem + ".prime", source);
  const std::string outPath =
      (testScratchPath("") / (nameStem + "_" + emitMode + "_out.txt")).string();
  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == expectedExitCode);
    CHECK(readFile(outPath) == expectedOutput);
    return;
  }

  const std::string exePath =
      (testScratchPath("") / (nameStem + "_" + emitMode + "_exe")).string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(runCmd) == expectedExitCode);
  CHECK(readFile(outPath) == expectedOutput);
}

inline void expectUncheckedPointerHelperSurfaceConformance(const std::string &emitMode) {
  expectUncheckedPointerProgramRunsWithOutput(
      makeUncheckedPointerHelperSurfaceSource(),
      "unchecked_pointer_helper_surface_" + emitMode,
      emitMode,
      11,
      "4\n7\n11\n");
}

inline void expectUncheckedPointerGrowthConformance(const std::string &emitMode) {
  expectUncheckedPointerProgramRunsWithOutput(
      makeUncheckedPointerGrowthSource(),
      "unchecked_pointer_growth_" + emitMode,
      emitMode,
      20,
      "4\n7\n9\n20\n");
}
