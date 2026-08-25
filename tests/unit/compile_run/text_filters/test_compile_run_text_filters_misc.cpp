#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

TEST_CASE("line/block/trailing comments are stripped before parsing") {
  const std::string source = R"(
// comment at top
[return<int>]
main() {
  /* comment in body */
  return(4i32) // trailing comment
}
)";
  const std::string srcPath = writeTemp("compile_comments.prime", source);
  const std::string compileCppCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 4);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 4);

#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
  const std::string nativePath = (testScratchPath("") / "primec_comments_native").string();
  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 4);
#endif
}

TEST_CASE("block expression with outer scope capture") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] y{2i32}
  return(block() { [i32] x{4i32} plus(x, y) })
}
)";
  const std::string srcPath = writeTemp("compile_block_expr_capture.prime", source);
  const std::string compileCppCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 6);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 6);

#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
  const std::string nativePath =
      (testScratchPath("") / "primec_block_expr_capture_native").string();
  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 6);
#endif
}

TEST_CASE("greater_than() compares two i32 values") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(2i32, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_greater_than.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("less_than() compares two i32 values") {
  const std::string source = R"(
[return<bool>]
main() {
  return(less_than(1i32, 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_less_than.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("equal() compares two i32 values") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(3i32, 3i32))
}
)";
  const std::string srcPath = writeTemp("compile_equal.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("not_equal() compares two i32 values") {
  const std::string source = R"(
[return<bool>]
main() {
  return(not_equal(3i32, 4i32))
}
)";
  const std::string srcPath = writeTemp("compile_not_equal.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("min() picks the smaller i32 operand") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(min(5i32, 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_min.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("max() picks the larger f32 operand") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(convert<int>(max(1.25f, 2.5f)))
}
)";
  const std::string srcPath = writeTemp("compile_max_f32.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("abs() of a negated i32 literal") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(abs(negate(7i32)))
}
)";
  const std::string srcPath = writeTemp("compile_abs.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 7);
}

TEST_CASE("sign() of positive and negative f32 values") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(convert<int>(plus(sign(1.5f), sign(negate(2.0f)))))
}
)";
  const std::string srcPath = writeTemp("compile_sign_f32.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("saturate() clamps an f32 to [0, 1]") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(convert<int>(saturate(2.5f)))
}
)";
  const std::string srcPath = writeTemp("compile_saturate_f32.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("clamp() bounds an i32 value") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(clamp(5i32, 1i32, 4i32))
}
)";
  const std::string srcPath = writeTemp("compile_clamp.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 4);
}

TEST_CASE("clamp() bounds an i64 value") {
  const std::string source = R"(
import /std/math/*
[return<i64>]
main() {
  return(clamp(9i64, 2i64, 6i64))
}
)";
  const std::string srcPath = writeTemp("compile_clamp_i64.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 6);
}

TEST_CASE("clamp() with mixed i32/i64 bound operands") {
  const std::string source = R"(
import /std/math/*
[return<i64>]
main() {
  return(clamp(9i32, 2i64, 6i32))
}
)";
  const std::string srcPath = writeTemp("compile_clamp_i64_mixed.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 6);
}

TEST_CASE("clamp() bounds a u64 value") {
  const std::string source = R"(
import /std/math/*
[return<u64>]
main() {
  return(clamp(9u64, 2u64, 6u64))
}
)";
  const std::string srcPath = writeTemp("compile_clamp_u64.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 6);
}

TEST_CASE("clamp() bounds an f32 value") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(convert<int>(clamp(1.5f, 0.5f, 1.2f)))
}
)";
  const std::string srcPath = writeTemp("compile_clamp_f32.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("clamp() bounds an f64 value") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(convert<int>(clamp(2.5f64, 1.0f64, 2.0f64)))
}
)";
  const std::string srcPath = writeTemp("compile_clamp_f64.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("bare true literal returns as bool") {
  const std::string source = R"(
[return<bool>]
main() {
  return(true)
}
)";
  const std::string srcPath = writeTemp("compile_bool.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("greater_than() compares two bool values") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(true, false))
}
)";
  const std::string srcPath = writeTemp("compile_bool_compare.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("bool and signed int comparison") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(true, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_bool_int_compare.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("rejects bool and u64 comparison") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(true, 1u64))
}
)";
  const std::string srcPath = writeTemp("compile_bool_u64_compare.prime", source);

  const std::string compileCmd = "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("string binding compiles without a stray const") {
  const std::string source = R"(
[return<int>]
main() {
  [string] message{"hello"utf8}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_string_binding.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_string_binding_exe").string();
  const std::string cppPath =
      std::filesystem::path(exePath).replace_extension(".cpp").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
  CHECK(std::filesystem::exists(cppPath));
  CHECK(readFile(cppPath).find("const const char *") == std::string::npos);
}

TEST_CASE("two-element array literal") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>{1i32, 2i32}, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_array.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("flat map constructor") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{/std/collections/map/map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  return(at(values, 3i32))
}
)";
  const std::string srcPath = writeTemp("compile_map.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 4);
}

TEST_CASE("map entry constructor") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{/std/collections/map/map<i32, i32>(
    /std/collections/map/entry<i32, i32>(3i32, 4i32))}
  return(at(values, 3i32))
}
)";
  const std::string srcPath = writeTemp("compile_map_pairs.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 4);
}

TEST_CASE("canonical map constructor") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{/std/collections/map/map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  return(at(values, 3i32))
}
)";
  const std::string srcPath = writeTemp("compile_map_whitespace_pairs.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 4);
}

TEST_CASE("named args can be passed out of declaration order") {
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
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 5);
}

TEST_CASE("convert<int>() truncates an f32 literal") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1.5f))
}
)";
  const std::string srcPath = writeTemp("compile_convert.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("positional and named args can be mixed in one call") {
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
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 6);
}

TEST_CASE("interleaved named args") {
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
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 6);
}

TEST_CASE("reordered named args") {
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
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 123);
}

TEST_CASE("map constructor with named-arg value") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
make_color([i32] hue, [i32] value) {
  return(plus(hue, value))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{/std/collections/map/map<i32, i32>(1i32, make_color([hue] 2i32, [value] 3i32))}
  return(at(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_map_named_value.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 5);
}

TEST_CASE("if/else statement sugar selects the right branch") {
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
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 9);
}

TEST_CASE("return() inside an if branch exits main early") {
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
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 5);
}

TEST_SUITE_END();
