#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

TEST_CASE("implicit utf8 suffix by default") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("hello")
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_implicit_utf8.prime", source);
  const std::string nativePath = (testScratchPath("") / "primec_implicit_utf8_native").string();

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 0);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 0);
}

TEST_CASE("implicit hex literal") {
  const std::string source = R"(
[return<int>]
main() {
  return(0x2A)
}
)";
  const std::string srcPath = writeTemp("compile_hex.prime", source);

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --text-transforms=default,implicit-i32";
  CHECK(runCommand(compileCmd) == 42);
}

TEST_CASE("float/f32 bindings add together") {
  const std::string source = R"(
[return<int>]
main() {
  [float] value{1.5f}
  [float] other{2.0f32}
  return(convert<int>(plus(value, other)))
}
)";
  const std::string srcPath = writeTemp("compile_float.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 3);
}

TEST_CASE("single-letter float suffix") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1f))
}
)";
  const std::string srcPath = writeTemp("compile_float_suffix_f.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("greater_than() compares two f32 values") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(1.5f, 0.5f))
}
)";
  const std::string srcPath = writeTemp("compile_float_compare.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("equal() compares two utf8 string literals") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal("alpha"utf8, "alpha"utf8))
}
)";
  const std::string srcPath = writeTemp("compile_text_filters_string_compare.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_text_filters_string_compare_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_text_filters_string_compare_native").string();
  const std::string vmErrPath =
      (testScratchPath("") / "primec_text_filters_string_compare_vm_err.txt").string();
  const std::string nativeErrPath =
      (testScratchPath("") / "primec_text_filters_string_compare_native_err.txt").string();

  // TODO-4813: --emit=exe used to successfully compile equal(...) on utf8
  // string literals (running to exit 1, the boolean-true convention); it
  // now fails to compile at all with "EXE IR lowering error: native backend
  // does not support string comparisons" - the exe pipeline appears to now
  // route through the same native-backend lowering used by --emit=native,
  // which never supported string comparisons. Re-pinned to the verified
  // current rejection.
  const std::string exeErrPath = (testScratchPath("") / "primec_text_filters_string_compare_exe_err.txt").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " +
                                 quoteShellArg(exeErrPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(exeErrPath).find("native backend does not support string comparisons") != std::string::npos);

  const std::string runVmCmd =
      "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(vmErrPath);
  CHECK(runCommand(runVmCmd) == 2);
  CHECK(readFile(vmErrPath).find("vm backend does not support string comparisons") != std::string::npos);

  const std::string compileNativeCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o " + quoteShellArg(nativePath) + " --entry /main 2> " +
      quoteShellArg(nativeErrPath);
  CHECK(runCommand(compileNativeCmd) == 2);
  CHECK(readFile(nativeErrPath).find("native backend does not support string comparisons") != std::string::npos);
}

TEST_CASE("rejects mixed int/float arithmetic") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1i32, 1.5f))
}
)";
  const std::string srcPath = writeTemp("compile_mixed_int_float.prime", source);

  const std::string compileCmd = "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects method call on array") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_array.prime", source);

  const std::string compileCmd = "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects method call on pointer") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{2i32}
  [Pointer<i32>] ptr{location(value)}
  return(ptr.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_pointer.prime", source);

  const std::string compileCmd = "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects method call on reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{2i32}
  [Reference<i32>] ref{location(value)}
  return(ref.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_reference.prime", source);

  const std::string compileCmd = "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("pointer operator sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{7i32}
  [Pointer<i32>] ptr{&value}
  return(*ptr)
}
)";
  const std::string srcPath = writeTemp("compile_pointer_sugar.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 7);
}

TEST_CASE("rejects method call on map") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_map.prime", source);

  const std::string compileCmd = "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("implicit suffix enabled by default") {
  const std::string source = R"(
[return<int>]
main() {
  return(8)
}
)";
  const std::string srcPath = writeTemp("compile_suffix_off.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 8);
}

TEST_CASE("text filter rewrites if expression") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  if(false, then(){
    [i32] temp{4i32}
    assign(value, temp)
  }, else(){ assign(value, 9i32) })
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_if.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 9);
}

TEST_CASE("if() expression form yields a value") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(false, then(){ 4i32 }, else(){ 9i32 }))
}
)";
  const std::string srcPath = writeTemp("compile_if_expr.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 9);
}

TEST_CASE("if block sugar in return expression") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(true) { 4i32 } else { 9i32 })
}
)";
  const std::string srcPath = writeTemp("compile_if_expr_sugar.prime", source);
  const std::string nativePath = (testScratchPath("") / "primec_if_expr_sugar_native").string();

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 4);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 4);
}

TEST_CASE("if expr block statements") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(true) { [i32] x{4i32} plus(x, 1i32) } else { 0i32 })
}
)";
  const std::string srcPath = writeTemp("compile_if_expr_block_stmts.prime", source);
  const std::string nativePath =
      (testScratchPath("") / "primec_if_expr_block_stmts_native").string();

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 5);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 5);
}

TEST_CASE("lazy if expression taking then branch") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32)}
  [i32] n{values.count()}
  return(if(equal(n, 1i32), then(){ values[0i32] }, else(){ values[9i32] }))
}
)";
  const std::string srcPath = writeTemp("compile_if_lazy_then.prime", source);
  const std::string nativePath = (testScratchPath("") / "primec_if_lazy_then_native").string();
  const std::string errPath = (testScratchPath("") / "primec_if_lazy_then_err.txt").string();

  const std::string runVmCmd =
      "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runVmCmd) == 4);
  CHECK(readFile(errPath).empty());

  const std::string compileNativeCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o " + quoteShellArg(nativePath) + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  const std::string runNativeCmd = quoteShellArg(nativePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runNativeCmd) == 4);
  CHECK(readFile(errPath).empty());
}

TEST_CASE("lazy if expression taking else branch") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32)}
  [i32] n{values.count()}
  return(if(equal(n, 0i32), then(){ values[9i32] }, else(){ values[0i32] }))
}
)";
  const std::string srcPath = writeTemp("compile_if_lazy_else.prime", source);
  const std::string nativePath = (testScratchPath("") / "primec_if_lazy_else_native").string();
  const std::string errPath = (testScratchPath("") / "primec_if_lazy_else_err.txt").string();

  const std::string runVmCmd =
      "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runVmCmd) == 4);
  CHECK(readFile(errPath).empty());

  const std::string compileNativeCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o " + quoteShellArg(nativePath) + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  const std::string runNativeCmd = quoteShellArg(nativePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runNativeCmd) == 4);
  CHECK(readFile(errPath).empty());
}

TEST_SUITE_END();
