#pragma once

inline std::string checkedPointerConformanceImportPath() {
  return "/std/collections/experimental_buffer_checked/*";
}

inline std::string makeCheckedPointerHelperSurfaceSource() {
  return R"(
import /std/collections/experimental_buffer_checked/*

[effects(heap_alloc), return<int>]
main() {
  [Pointer<i32> mut] ptr{bufferAlloc<i32>(2i32)}
  [Pointer<i32> mut] first{bufferOffsetChecked<i32>(ptr, 0i32, 2i32)}
  assign(dereference(first), 4i32)
  bufferWriteChecked<i32>(ptr, 1i32, 2i32, 7i32)
  [i32] total{plus(bufferReadChecked<i32>(ptr, 0i32, 2i32), bufferReadChecked<i32>(ptr, 1i32, 2i32))}
  bufferFree<i32>(ptr)
  return(total)
}
)";
}

inline std::string makeCheckedPointerGrowthSource() {
  return R"(
import /std/collections/experimental_buffer_checked/*

[effects(heap_alloc), return<int>]
main() {
  [Pointer<i32> mut] ptr{bufferAlloc<i32>(2i32)}
  bufferWriteChecked<i32>(ptr, 0i32, 2i32, 4i32)
  bufferWriteChecked<i32>(ptr, 1i32, 2i32, 7i32)
  [Pointer<i32> mut] grown{bufferGrow<i32>(ptr, 3i32)}
  bufferWriteChecked<i32>(grown, 2i32, 3i32, 9i32)
  [i32] total{plus(plus(bufferReadChecked<i32>(grown, 0i32, 3i32), bufferReadChecked<i32>(grown, 1i32, 3i32)),
                   bufferReadChecked<i32>(grown, 2i32, 3i32))}
  bufferFree<i32>(grown)
  return(total)
}
)";
}

inline std::string makeCheckedPointerOutOfBoundsSource() {
  return R"(
import /std/collections/experimental_buffer_checked/*

[effects(heap_alloc), return<int>]
main() {
  [Pointer<i32> mut] ptr{bufferAlloc<i32>(1i32)}
  return(bufferReadChecked<i32>(ptr, 1i32, 1i32))
}
)";
}

inline void expectCheckedPointerProgramRuns(const std::string &source,
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
      (std::filesystem::temp_directory_path() / (nameStem + "_exe")).string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == expectedExitCode);
}

inline void expectCheckedPointerProgramFails(const std::string &source,
                                             const std::string &nameStem,
                                             const std::string &emitMode,
                                             const std::string &expectedError) {
  const std::string srcPath = writeTemp(nameStem + ".prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / (nameStem + "_err.txt")).string();
  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(runCmd) != 0);
    CHECK(readFile(errPath) == expectedError);
    return;
  }

  const std::string exePath =
      (std::filesystem::temp_directory_path() / (nameStem + "_exe")).string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(errPath) == expectedError);
}

inline void expectCheckedPointerHelperSurfaceConformance(const std::string &emitMode) {
  expectCheckedPointerProgramRuns(
      makeCheckedPointerHelperSurfaceSource(),
      "checked_pointer_helper_surface_" + emitMode,
      emitMode,
      11);
}

inline void expectCheckedPointerGrowthConformance(const std::string &emitMode) {
  expectCheckedPointerProgramRuns(
      makeCheckedPointerGrowthSource(),
      "checked_pointer_growth_" + emitMode,
      emitMode,
      20);
}

inline void expectCheckedPointerOutOfBoundsConformance(const std::string &emitMode) {
  expectCheckedPointerProgramFails(
      makeCheckedPointerOutOfBoundsSource(),
      "checked_pointer_oob_" + emitMode,
      emitMode,
      "pointer index out of bounds\n");
}
