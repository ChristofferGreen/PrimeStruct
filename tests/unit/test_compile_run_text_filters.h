TEST_CASE("compiles and runs implicit i32 suffix") {
  const std::string source = R"(
[return<int>]
main() {
  return(8)
}
)";
  const std::string srcPath = writeTemp("compile_suffix.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_suffix_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_suffix_native").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main --text-filters=default,implicit-i32";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main --text-filters=default,implicit-i32";
  CHECK(runCommand(runVmCmd) == 8);

  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main --text-filters=default,implicit-i32";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 8);
}

TEST_CASE("compiles and runs implicit utf8 suffix by default") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("hello")
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_implicit_utf8.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_implicit_utf8_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_implicit_utf8_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 0);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 0);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 0);
}

TEST_CASE("compiles and runs implicit hex literal") {
  const std::string source = R"(
[return<int>]
main() {
  return(0x2A)
}
)";
  const std::string srcPath = writeTemp("compile_hex.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_hex_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main --text-filters=default,implicit-i32";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 42);
}

TEST_CASE("compiles and runs float binding") {
  const std::string source = R"(
[return<int>]
main() {
  [float] value{1.5f}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_float.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_float_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs float comparison") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(1.5f, 0.5f))
}
)";
  const std::string srcPath = writeTemp("compile_float_compare.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_float_compare_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs string comparison") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal("alpha"utf8, "alpha"utf8))
}
)";
  const std::string srcPath = writeTemp("compile_string_compare.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_string_compare_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_string_compare_native").string();
  const std::string vmErrPath = (std::filesystem::temp_directory_path() / "primec_string_compare_vm_err.txt").string();
  const std::string nativeErrPath =
      (std::filesystem::temp_directory_path() / "primec_string_compare_native_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_mixed_int_float_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_array_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_pointer_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_reference_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects method call on map") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32}}
  return(values.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_map.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_map_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("implicit suffix disabled by default") {
  const std::string source = R"(
[return<int>]
main() {
  return(8)
}
)";
  const std::string srcPath = writeTemp("compile_suffix_off.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_suffix_off_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs if") {
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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_if_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs if expression") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(false, 4i32, 9i32))
}
)";
  const std::string srcPath = writeTemp("compile_if_expr.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_if_expr_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("runs if expression in vm") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(false, 4i32, 9i32))
}
)";
  const std::string srcPath = writeTemp("vm_if_expr.prime", source);
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 9);
}

TEST_CASE("compiles and runs if block sugar in return expression") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(true) { 4i32 } else { 9i32 })
}
)";
  const std::string srcPath = writeTemp("compile_if_expr_sugar.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_if_expr_sugar_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_if_expr_sugar_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 4);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 4);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 4);
}

TEST_CASE("compiles and runs if expr block statements") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(true) { [i32] x{4i32} plus(x, 1i32) } else { 0i32 })
}
)";
  const std::string srcPath = writeTemp("compile_if_expr_block_stmts.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_if_expr_block_stmts_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_if_expr_block_stmts_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 5);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 5);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 5);
}

TEST_CASE("compiles and runs lazy if expression taking then branch") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32)}
  [i32] n{values.count()}
  return(if(equal(n, 1i32), values[0i32], values[9i32]))
}
)";
  const std::string srcPath = writeTemp("compile_if_lazy_then.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_if_lazy_then_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_if_lazy_then_native").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_if_lazy_then_err.txt").string();

  const std::string compileCppCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runCmd) == 4);
  CHECK(readFile(errPath).empty());

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

TEST_CASE("compiles and runs lazy if expression taking else branch") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32)}
  [i32] n{values.count()}
  return(if(equal(n, 0i32), values[9i32], values[0i32]))
}
)";
  const std::string srcPath = writeTemp("compile_if_lazy_else.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_if_lazy_else_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_if_lazy_else_native").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_if_lazy_else_err.txt").string();

  const std::string compileCppCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runCmd) == 4);
  CHECK(readFile(errPath).empty());

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

TEST_CASE("compiles and runs with comments") {
  const std::string source = R"(
// comment at top
[return<int>]
main() {
  /* comment in body */
  return(4i32) // trailing comment
}
)";
  const std::string srcPath = writeTemp("compile_comments.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_comments_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_comments_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 4);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 4);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 4);
}

TEST_CASE("compiles and runs block expression with outer scope capture") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] y{2i32}
  return(block() { [i32] x{4i32} plus(x, y) })
}
)";
  const std::string srcPath = writeTemp("compile_block_expr_capture.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_block_expr_capture_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_block_expr_capture_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 6);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 6);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 6);
}

TEST_CASE("compiles and runs greater_than") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(2i32, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_greater_than.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_gt_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs less_than") {
  const std::string source = R"(
[return<bool>]
main() {
  return(less_than(1i32, 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_less_than.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_lt_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs equal") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(3i32, 3i32))
}
)";
  const std::string srcPath = writeTemp("compile_equal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_equal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs not_equal") {
  const std::string source = R"(
[return<bool>]
main() {
  return(not_equal(3i32, 4i32))
}
)";
  const std::string srcPath = writeTemp("compile_not_equal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_not_equal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs min") {
  const std::string source = R"(
import /math
[return<int>]
main() {
  return(min(5i32, 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_min.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_min_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs max f32") {
  const std::string source = R"(
import /math
[return<int>]
main() {
  return(convert<int>(max(1.25f, 2.5f)))
}
)";
  const std::string srcPath = writeTemp("compile_max_f32.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_max_f32_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs abs") {
  const std::string source = R"(
import /math
[return<int>]
main() {
  return(abs(negate(7i32)))
}
)";
  const std::string srcPath = writeTemp("compile_abs.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_abs_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs sign f32") {
  const std::string source = R"(
import /math
[return<int>]
main() {
  return(convert<int>(plus(sign(1.5f), sign(negate(2.0f)))))
}
)";
  const std::string srcPath = writeTemp("compile_sign_f32.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_sign_f32_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs saturate f32") {
  const std::string source = R"(
import /math
[return<int>]
main() {
  return(convert<int>(saturate(2.5f)))
}
)";
  const std::string srcPath = writeTemp("compile_saturate_f32.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_saturate_f32_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs clamp") {
  const std::string source = R"(
import /math
[return<int>]
main() {
  return(clamp(5i32, 1i32, 4i32))
}
)";
  const std::string srcPath = writeTemp("compile_clamp.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_clamp_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs clamp i64") {
  const std::string source = R"(
import /math
[return<i64>]
main() {
  return(clamp(9i64, 2i64, 6i64))
}
)";
  const std::string srcPath = writeTemp("compile_clamp_i64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_clamp_i64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs clamp mixed i32/i64") {
  const std::string source = R"(
import /math
[return<i64>]
main() {
  return(clamp(9i32, 2i64, 6i32))
}
)";
  const std::string srcPath = writeTemp("compile_clamp_i64_mixed.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_clamp_i64_mixed_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs clamp u64") {
  const std::string source = R"(
import /math
[return<u64>]
main() {
  return(clamp(9u64, 2u64, 6u64))
}
)";
  const std::string srcPath = writeTemp("compile_clamp_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_clamp_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs clamp f32") {
  const std::string source = R"(
import /math
[return<int>]
main() {
  return(convert<int>(clamp(1.5f, 0.5f, 1.2f)))
}
)";
  const std::string srcPath = writeTemp("compile_clamp_f32.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_clamp_f32_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs clamp f64") {
  const std::string source = R"(
import /math
[return<int>]
main() {
  return(convert<int>(clamp(2.5f64, 1.0f64, 2.0f64)))
}
)";
  const std::string srcPath = writeTemp("compile_clamp_f64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_clamp_f64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs boolean literal") {
  const std::string source = R"(
[return<bool>]
main() {
  return(true)
}
)";
  const std::string srcPath = writeTemp("compile_bool.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_bool_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs bool return") {
  const std::string source = R"(
[return<bool>]
main() {
  return(true)
}
)";
  const std::string srcPath = writeTemp("compile_bool_return.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_bool_return_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs bool comparison") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(true, false))
}
)";
  const std::string srcPath = writeTemp("compile_bool_compare.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_bool_compare_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs bool and signed int comparison") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(true, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_bool_int_compare.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_bool_int_compare_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("rejects bool and u64 comparison") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(true, 1u64))
}
)";
  const std::string srcPath = writeTemp("compile_bool_u64_compare.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_bool_u64_compare_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs string binding") {
  const std::string source = R"(
[return<int>]
main() {
  [string] message{"hello"utf8}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_string_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_string_binding_exe").string();
  const std::string cppPath =
      std::filesystem::path(exePath).replace_extension(".cpp").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
  CHECK(std::filesystem::exists(cppPath));
  CHECK(readFile(cppPath).find("const const char *") == std::string::npos);
}

TEST_CASE("compiles and runs array literal") {
  const std::string source = R"(
[return<int>]
main() {
  array<i32>{1i32, 2i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_array.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs map literal") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>{1i32, 2i32, 3i32, 4i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_map.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs map literal pairs") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>{1i32=2i32, 3i32=4i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_map_pairs.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_pairs_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs named-arg call") {
  const std::string source = R"(
[return<int>]
add([i32] a, [i32] b) {
  return(plus(a, b))
}

[return<int>]
main() {
  return(add(b = 2i32, a = 3i32))
}
)";
  const std::string srcPath = writeTemp("compile_named_args.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_named_args_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs convert builtin") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1.5f))
}
)";
  const std::string srcPath = writeTemp("compile_convert.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs mixed named args") {
  const std::string source = R"(
[return<int>]
sum3([i32] a, [i32] b, [i32] c) {
  return(plus(plus(a, b), c))
}

[return<int>]
main() {
  return(sum3(1i32, c = 3i32, b = 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_named_mixed.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_named_mixed_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs reordered named args") {
  const std::string source = R"(
[return<int>]
pack([i32] a, [i32] b, [i32] c) {
  return(plus(plus(multiply(a, 100i32), multiply(b, 10i32)), c))
}

[return<int>]
main() {
  return(pack(c = 3i32, a = 1i32, b = 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_named_reorder.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_named_reorder_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 123);
}

TEST_CASE("compiles and runs map literal with named-arg value") {
  const std::string source = R"(
[return<int>]
make_color([i32] hue, [i32] value) {
  return(plus(hue, value))
}

[return<int>]
main() {
  map<i32, i32>{1i32=make_color(hue = 2i32, value = 3i32)}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_map_named_value.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_named_value_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs if statement sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  if(false) {
    assign(value, 4i32)
  } else {
    assign(value, 9i32)
  }
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_if_sugar.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_if_sugar_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs early return in if") {
  const std::string source = R"(
[return<int>]
main() {
  if(true) {
    return(5i32)
  } else {
    return(2i32)
  }
}
)";
  const std::string srcPath = writeTemp("compile_return_if.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_return_if_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}
