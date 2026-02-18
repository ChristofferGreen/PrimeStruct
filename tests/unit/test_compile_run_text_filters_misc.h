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
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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

TEST_CASE("compiles and runs map literal whitespace pairs") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>{1i32 2i32 3i32 4i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_map_whitespace_pairs.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_whitespace_pairs_exe").string();

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
  return(add([b] 2i32, [a] 3i32))
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
  return(sum3(1i32, [c] 3i32, [b] 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_named_mixed.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_named_mixed_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs interleaved named args") {
  const std::string source = R"(
[return<int>]
sum3([i32] a, [i32] b, [i32] c) {
  return(plus(plus(a, b), c))
}

[return<int>]
main() {
  return(sum3([c] 3i32, 1i32, [b] 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_named_interleaved.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_named_interleaved_exe").string();

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
  return(pack([c] 3i32, [a] 1i32, [b] 2i32))
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
  map<i32, i32>{1i32=make_color([hue] 2i32, [value] 3i32)}
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

TEST_SUITE_END();
