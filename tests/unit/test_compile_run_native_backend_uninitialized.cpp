#include "test_compile_run_helpers.h"

#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.uninitialized");

TEST_CASE("compiles and runs native uninitialized local storage") {
  const std::string source = R"(
[return<int>]
main() {
  [uninitialized<i32>] storage{uninitialized<i32>()}
  init(storage, 3i32)
  drop(storage)
  init(storage, 5i32)
  return(take(storage))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_uninitialized_local.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_uninitialized_local_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs native uninitialized string storage") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [uninitialized<string>] storage{uninitialized<string>()}
  init(storage, "hello"utf8)
  print_line(take(storage))
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_uninitialized_string.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_uninitialized_string_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_uninitialized_string_out.txt").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_uninitialized_string_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " +
      errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("return requires uninitialized storage to be dropped") !=
        std::string::npos);
}

TEST_CASE("compiles and runs native pointer-backed uninitialized storage") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [Pointer<uninitialized<i32>>] ptr{/std/intrinsics/memory/alloc<uninitialized<i32>>(1i32)}
  init(dereference(ptr), 7i32)
  [i32] out{take(dereference(ptr))}
  /std/intrinsics/memory/free(ptr)
  return(out)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_uninitialized_pointer.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_uninitialized_pointer_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native reference-backed uninitialized storage") {
  const std::string source = R"(
[return<int>]
main() {
  [uninitialized<i32>] storage{uninitialized<i32>()}
  [Reference<uninitialized<i32>>] ref{location(storage)}
  init(dereference(ref), 7i32)
  return(take(dereference(ref)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_uninitialized_reference.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_uninitialized_reference_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native pointer-backed uninitialized struct storage") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] left{0i32}
  [i32] right{0i32}
}

[effects(heap_alloc), return<int>]
main() {
  [Pointer<uninitialized<Pair>>] ptr{/std/intrinsics/memory/alloc<uninitialized<Pair>>(1i32)}
  init(dereference(ptr), Pair{3i32, 9i32})
  [Pair] value{take(dereference(ptr))}
  /std/intrinsics/memory/free(ptr)
  return(value.right)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_uninitialized_pointer_struct.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_uninitialized_pointer_struct_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs native uninitialized struct field") {
  const std::string source = R"(
[struct]
Box() {
  [uninitialized<i32>] value{uninitialized<i32>()}

  [public]
  Create() {
  }
}

[return<int>]
main() {
  [Box mut] box{Box{}}
  [Reference<Box> mut] ref{location(box)}
  init(ref.value, 7i32)
  [i32] out{take(ref.value)}
  return(out)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_uninitialized_struct.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_uninitialized_struct_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_uninitialized_struct_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " +
      errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
        std::string::npos);
}

TEST_SUITE_END();
#endif
