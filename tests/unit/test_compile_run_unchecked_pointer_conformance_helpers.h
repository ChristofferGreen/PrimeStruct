#pragma once

inline std::string uncheckedPointerConformanceImportPath() {
  return "/std/collections/experimental_buffer_unchecked/*";
}

inline std::string makeUncheckedPointerHelperSurfaceSource() {
  return R"(
import /std/collections/experimental_buffer_unchecked/*

[effects(heap_alloc), return<int>]
main() {
  [Pointer<i32> mut] ptr{bufferAlloc<i32>(2i32)}
  [Pointer<i32> mut] first{bufferOffsetUnsafe<i32>(ptr, 0i32)}
  assign(dereference(first), 4i32)
  bufferWriteUnsafe<i32>(ptr, 1i32, 7i32)
  [i32] total{plus(bufferReadUnsafe<i32>(ptr, 0i32), bufferReadUnsafe<i32>(ptr, 1i32))}
  bufferFree<i32>(ptr)
  return(total)
}
)";
}

inline std::string makeUncheckedPointerGrowthSource() {
  return R"(
import /std/collections/experimental_buffer_unchecked/*

[effects(heap_alloc), return<int>]
main() {
  [Pointer<i32> mut] ptr{bufferAlloc<i32>(2i32)}
  bufferWriteUnsafe<i32>(ptr, 0i32, 4i32)
  bufferWriteUnsafe<i32>(ptr, 1i32, 7i32)
  [Pointer<i32> mut] grown{bufferGrow<i32>(ptr, 3i32)}
  bufferWriteUnsafe<i32>(grown, 2i32, 9i32)
  [i32] total{plus(plus(bufferReadUnsafe<i32>(grown, 0i32), bufferReadUnsafe<i32>(grown, 1i32)),
                   bufferReadUnsafe<i32>(grown, 2i32))}
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
      (std::filesystem::temp_directory_path() / (nameStem + "_exe")).string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == expectedExitCode);
}

inline void expectUncheckedPointerHelperSurfaceConformance(const std::string &emitMode) {
  expectUncheckedPointerProgramRuns(
      makeUncheckedPointerHelperSurfaceSource(),
      "unchecked_pointer_helper_surface_" + emitMode,
      emitMode,
      11);
}

inline void expectUncheckedPointerGrowthConformance(const std::string &emitMode) {
  expectUncheckedPointerProgramRuns(
      makeUncheckedPointerGrowthSource(),
      "unchecked_pointer_growth_" + emitMode,
      emitMode,
      20);
}
