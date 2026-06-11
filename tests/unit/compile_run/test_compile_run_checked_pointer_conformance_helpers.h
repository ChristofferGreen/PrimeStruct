#pragma once

inline std::string checkedPointerConformanceImportPath() {
  return "/std/collections/internal_buffer_checked/*";
}

inline std::string makeCheckedPointerHelperSurfaceSource() {
  return R"(
import /std/collections/internal_buffer_checked/*

[effects(io_out, heap_alloc), return<int>]
main() {
  [Pointer<i32> mut] ptr{bufferAlloc<i32>(2i32)}
  [Pointer<i32> mut] first{bufferOffsetChecked<i32>(ptr, 0i32, 2i32)}
  assign(dereference(first), 4i32)
  bufferWriteChecked<i32>(ptr, 1i32, 2i32, 7i32)
  [i32] total{plus(bufferReadChecked<i32>(ptr, 0i32, 2i32), bufferReadChecked<i32>(ptr, 1i32, 2i32))}
  print_line(bufferReadChecked<i32>(ptr, 0i32, 2i32))
  print_line(bufferReadChecked<i32>(ptr, 1i32, 2i32))
  print_line(total)
  bufferFree<i32>(ptr)
  return(total)
}
)";
}

inline std::string makeCheckedPointerGrowthSource() {
  return R"(
import /std/collections/internal_buffer_checked/*

[effects(io_out, heap_alloc), return<int>]
main() {
  [Pointer<i32> mut] ptr{bufferAlloc<i32>(2i32)}
  bufferWriteChecked<i32>(ptr, 0i32, 2i32, 4i32)
  bufferWriteChecked<i32>(ptr, 1i32, 2i32, 7i32)
  [Pointer<i32> mut] grown{bufferGrow<i32>(ptr, 3i32)}
  bufferWriteChecked<i32>(grown, 2i32, 3i32, 9i32)
  [i32] total{plus(plus(bufferReadChecked<i32>(grown, 0i32, 3i32), bufferReadChecked<i32>(grown, 1i32, 3i32)),
                   bufferReadChecked<i32>(grown, 2i32, 3i32))}
  print_line(bufferReadChecked<i32>(grown, 0i32, 3i32))
  print_line(bufferReadChecked<i32>(grown, 1i32, 3i32))
  print_line(bufferReadChecked<i32>(grown, 2i32, 3i32))
  print_line(total)
  bufferFree<i32>(grown)
  return(total)
}
)";
}

inline std::string makeCheckedPointerUninitializedPrefixMoveSource() {
  return R"(
import /std/collections/internal_buffer_checked/*
import /std/collections/internal_buffer_unchecked/*

[struct]
Token() {
  [i32] value{0i32}

  Destroy() {
  }
}

[return<Pointer<uninitialized<Token>>>]
token_slot([Pointer<uninitialized<Token>>] values, [i32] index) {
  return(bufferOffsetUnsafe<uninitialized<Token>>(values, index))
}

[effects(heap_alloc), return<void>]
move_prefix([Pointer<uninitialized<Token>> mut] dst,
            [Pointer<uninitialized<Token>> mut] src,
            [i32] limit) {
  [i32 mut] index{0i32}
  while(less_than(index, limit), do() {
    [Pointer<uninitialized<Token>> mut] dstSlot{token_slot(dst, index)}
    [Pointer<uninitialized<Token>> mut] srcSlot{token_slot(src, index)}
    init(dereference(dstSlot), take(dereference(srcSlot)))
    increment(index)
  })
}

[effects(io_out, heap_alloc), return<int>]
main() {
  [Pointer<uninitialized<Token>> mut] src{bufferAlloc<uninitialized<Token>>(3i32)}
  [Pointer<uninitialized<Token>> mut] dst{bufferAlloc<uninitialized<Token>>(3i32)}
  init(dereference(token_slot(src, 0i32)), Token{4i32})
  init(dereference(token_slot(src, 1i32)), Token{6i32})
  init(dereference(token_slot(src, 2i32)), Token{8i32})
  move_prefix(dst, src, 3i32)
  [Reference<Token>] borrowed{borrow(dereference(token_slot(dst, 1i32)))}
  [Token] tail{take(dereference(token_slot(dst, 2i32)))}
  [i32] total{plus(borrowed.value, tail.value)}
  print_line(borrowed.value)
  print_line(tail.value)
  print_line(total)
  drop(dereference(token_slot(dst, 0i32)))
  drop(dereference(token_slot(dst, 1i32)))
  bufferFree<uninitialized<Token>>(src)
  bufferFree<uninitialized<Token>>(dst)
  return(total)
}
)";
}

inline std::string makeCheckedPointerOutOfBoundsSource() {
  return R"(
import /std/collections/internal_buffer_checked/*

[effects(heap_alloc), return<int>]
main() {
  [Pointer<i32> mut] ptr{bufferAlloc<i32>(1i32)}
  return(bufferReadChecked<i32>(ptr, 1i32, 1i32))
}
)";
}

inline std::string makeCheckedPointerUninitializedOutOfBoundsSource() {
  return R"(
import /std/collections/internal_buffer_checked/*

[struct]
Token() {
  [i32] value{0i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [Pointer<uninitialized<Token>> mut] ptr{bufferAlloc<uninitialized<Token>>(1i32)}
  [Pointer<uninitialized<Token>> mut] slot{bufferOffsetChecked<uninitialized<Token>>(ptr, 1i32, 1i32)}
  init(dereference(slot), Token{1i32})
  return(0i32)
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
      (testScratchPath("") / (nameStem + "_exe")).string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == expectedExitCode);
}

inline void expectCheckedPointerProgramRunsWithOutput(const std::string &source,
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

inline void expectCheckedPointerProgramFails(const std::string &source,
                                             const std::string &nameStem,
                                             const std::string &emitMode,
                                             int expectedExitCode,
                                             const std::string &expectedError) {
  const std::string srcPath = writeTemp(nameStem + ".prime", source);
  const std::string errPath =
      (testScratchPath("") / (nameStem + "_err.txt")).string();
  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(runCmd) == expectedExitCode);
    CHECK(readFile(errPath) == expectedError);
    return;
  }

  const std::string exePath =
      (testScratchPath("") / (nameStem + "_exe")).string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runCmd) == expectedExitCode);
  CHECK(readFile(errPath) == expectedError);
}

inline void expectCheckedPointerHelperSurfaceConformance(const std::string &emitMode) {
  expectCheckedPointerProgramRunsWithOutput(
      makeCheckedPointerHelperSurfaceSource(),
      "checked_pointer_helper_surface_" + emitMode,
      emitMode,
      11,
      "4\n7\n11\n");
}

inline void expectCheckedPointerGrowthConformance(const std::string &emitMode) {
  expectCheckedPointerProgramRunsWithOutput(
      makeCheckedPointerGrowthSource(),
      "checked_pointer_growth_" + emitMode,
      emitMode,
      20,
      "4\n7\n9\n20\n");
}

inline void expectCheckedPointerUninitializedPrefixMoveConformance(const std::string &emitMode) {
  expectCheckedPointerProgramRunsWithOutput(
      makeCheckedPointerUninitializedPrefixMoveSource(),
      "checked_pointer_uninitialized_prefix_move_" + emitMode,
      emitMode,
      14,
      "6\n8\n14\n");
}

inline void expectCheckedPointerOutOfBoundsConformance(const std::string &emitMode) {
  expectCheckedPointerProgramFails(
      makeCheckedPointerOutOfBoundsSource(),
      "checked_pointer_oob_" + emitMode,
      emitMode,
      3,
      "pointer index out of bounds\n");
}

inline void expectCheckedPointerUninitializedOutOfBoundsConformance(const std::string &emitMode) {
  expectCheckedPointerProgramFails(
      makeCheckedPointerUninitializedOutOfBoundsSource(),
      "checked_pointer_uninitialized_oob_" + emitMode,
      emitMode,
      3,
      "pointer index out of bounds\n");
}
