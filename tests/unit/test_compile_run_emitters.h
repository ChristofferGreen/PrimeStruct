TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("compiles and runs chained method calls in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }

  [return<int>]
  dec([i32] value) {
    return(minus(value, 1i32))
  }
}

[return<int>]
make() {
  return(4i32)
}

[return<int>]
main() {
  return(make().inc().dec())
}
)";
  const std::string srcPath = writeTemp("compile_method_chain.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_chain_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("executions are ignored by C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void> effects(io_out)]
log([i32] value) {
  print_line("log"utf8)
  return()
}

[effects(io_out)]
log(1i32) {
  print_line("exec"utf8)
}
)";
  const std::string srcPath = writeTemp("compile_exec_ignored.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_exec_ignored_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_exec_ignored_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 1);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("struct Create/Destroy helpers run in C++ emitter") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{1i32}

  [effects(io_out)]
  Create() {
    print_line(1i32)
  }

  [effects(io_out)]
  Destroy() {
    print_line(2i32)
  }
}

[return<int>]
main() {
  Thing()
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_struct_lifecycle.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_struct_lifecycle_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_struct_lifecycle_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "1\n2\n");
}

TEST_CASE("compiles and runs import alias in C++ emitter") {
  const std::string source = R"(
import /util
namespace util {
  [return<int>]
  helper() {
    return(7i32)
  }
}
[return<int>]
main() {
  return(helper())
}
)";
  const std::string srcPath = writeTemp("compile_import_alias_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_import_alias_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array method calls in C++ emitter") {
  const std::string source = R"(
[return<int>]
/array/first([array<i32>] items) {
  return(items[0i32])
}

[return<int>]
main() {
  [array<i32>] items{array<i32>(7i32, 9i32)}
  return(items.first())
}
)";
  const std::string srcPath = writeTemp("compile_array_method.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_method_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array index sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(values[1i32])
}
)";
  const std::string srcPath = writeTemp("compile_array_index.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_index_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs argv helpers in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_emit_argv.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_emit_argv_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_emit_argv_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);

  const std::string runCmd = exePath + " alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs array index sugar with u64") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(values[1u64])
}
)";
  const std::string srcPath = writeTemp("compile_array_index_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_index_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs vector helpers in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  push(values, 4i32)
  remove_at(values, 1i32)
  remove_swap(values, 1i32)
  pop(values)
  reserve(values, 8i32)
  capacity(values)
  clear(values)
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("compile_vector_helpers_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_vector_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs string-keyed map literal in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"utf8, 1i32, "b"utf8, 2i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_map_string_keys_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_string_keys_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs lerp in C++ emitter") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(lerp(2i32, 6i32, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_lerp_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_lerp_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs math-qualified clamp in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  return(/math/clamp(9i32, 2i32, 6i32))
}
)";
  const std::string srcPath = writeTemp("compile_math_clamp_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_math_clamp_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs math-qualified trig in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(/math/sin(0.0f)))
}
)";
  const std::string srcPath = writeTemp("compile_math_trig_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_math_trig_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs math-qualified min/max in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  return(/math/min(9i32, /math/max(2i32, 6i32)))
}
)";
  const std::string srcPath = writeTemp("compile_math_minmax_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_math_minmax_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs math-qualified constants in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(convert<int>(/math/pi), plus(convert<int>(/math/tau), convert<int>(/math/e))))
}
)";
  const std::string srcPath = writeTemp("compile_math_constants_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_math_constants_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("compiles and runs imported math constants in C++ emitter") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(plus(convert<int>(pi), plus(convert<int>(tau), convert<int>(e))))
}
)";
  const std::string srcPath = writeTemp("compile_imported_math_constants_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_imported_math_constants_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("compiles and runs rounding builtins in C++ emitter") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] value{1.5f}
  return(plus(plus(plus(convert<int>(floor(value)), convert<int>(ceil(value))),
                   plus(convert<int>(round(value)), convert<int>(trunc(value)))),
              convert<int>(multiply(fract(value), 10.0f))))
}
)";
  const std::string srcPath = writeTemp("compile_rounding_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_rounding_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("compiles and runs convert<bool> from float in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(convert<int>(convert<bool>(0.0f)), convert<int>(convert<bool>(2.5f))))
}
)";
  const std::string srcPath = writeTemp("compile_convert_bool_float.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_bool_float_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs string comparisons in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(
    convert<int>(equal("alpha"raw_utf8, "alpha"raw_utf8)),
    convert<int>(less_than("alpha"raw_utf8, "beta"raw_utf8))
  ))
}
)";
  const std::string srcPath = writeTemp("compile_string_compare.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_string_compare_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs power/log builtins in C++ emitter") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(plus(
    plus(
      plus(convert<int>(sqrt(9.0f)), convert<int>(cbrt(27.0f))),
      plus(convert<int>(pow(2.0f, 3.0f)), convert<int>(exp(0.0f)))
    ),
    plus(
      plus(convert<int>(exp2(3.0f)), convert<int>(log(1.0f))),
      plus(convert<int>(log2(8.0f)), convert<int>(log10(1000.0f)))
    )
  ))
}
)";
  const std::string srcPath = writeTemp("compile_power_log_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_power_log_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 29);
}

TEST_CASE("compiles and runs integer pow negative exponent in C++ emitter") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(pow(2i32, -1i32))
}
)";
  const std::string srcPath = writeTemp("compile_int_pow_negative_exe.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_int_pow_negative_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_int_pow_negative_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "pow exponent must be non-negative\n");
}

TEST_CASE("compiles and runs trig builtins in C++ emitter") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] zero{0.0f}
  [f32] one{1.0f}
  return(plus(
    convert<int>(cos(zero)),
    plus(
      plus(convert<int>(sin(zero)), convert<int>(tan(zero))),
      plus(
        plus(convert<int>(asin(zero)), convert<int>(acos(one))),
        plus(
          plus(convert<int>(atan(zero)), convert<int>(atan2(zero, one))),
          convert<int>(round(degrees(radians(180.0f))))
        )
      )
    )
  ))
}
)";
  const std::string srcPath = writeTemp("compile_trig_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_trig_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 181);
}

TEST_CASE("compiles and runs hyperbolic builtins in C++ emitter") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] zero{0.0f}
  return(plus(
    plus(convert<int>(sinh(zero)), convert<int>(cosh(zero))),
    plus(
      plus(convert<int>(tanh(zero)), convert<int>(asinh(zero))),
      plus(convert<int>(acosh(1.0f)), convert<int>(atanh(zero)))
    )
  ))
}
)";
  const std::string srcPath = writeTemp("compile_hyperbolic_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_hyperbolic_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs float utils in C++ emitter") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(plus(
    plus(convert<int>(fma(2.0f, 3.0f, 1.0f)), convert<int>(hypot(3.0f, 4.0f))),
    convert<int>(copysign(2.0f, -1.0f))
  ))
}
)";
  const std::string srcPath = writeTemp("compile_float_utils_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_float_utils_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);
}

TEST_CASE("compiles and runs float predicates in C++ emitter") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] zero{0.0f}
  [f32] one{1.0f}
  [f32] nanValue{divide(zero, zero)}
  [f32] infValue{divide(one, zero)}
  return(plus(
    plus(convert<int>(is_nan(nanValue)), convert<int>(is_inf(infValue))),
    convert<int>(is_finite(one))
  ))
}
)";
  const std::string srcPath = writeTemp("compile_float_predicates_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_float_predicates_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs import aliases in C++ emitter") {
  const std::string source = R"(
import /util
namespace util {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
)";
  const std::string srcPath = writeTemp("compile_import_alias_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_import_alias_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs math constants in C++ emitter") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(plus(
    plus(convert<int>(round(pi)), convert<int>(round(tau))),
    convert<int>(round(e))
  ))
}
)";
  const std::string srcPath = writeTemp("compile_constants_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_constants_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("compiles and runs array unsafe access in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_array_unsafe_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_unsafe_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array unsafe access with u64 index in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(at_unsafe(values, 1u64))
}
)";
  const std::string srcPath = writeTemp("compile_array_unsafe_u64_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_unsafe_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs repeat loop") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{0i32}
  repeat(3i32) {
    assign(value, plus(value, 2i32))
  }
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_repeat_loop.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_repeat_loop_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs loop while for sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  loop(2i32) {
    assign(total, plus(total, 1i32))
  }
  while(less_than(i 3i32)) {
    assign(total, plus(total, i))
    assign(i, plus(i, 1i32))
  }
  for([i32 mut] j{0i32} less_than(j 2i32) assign(j, plus(j, 1i32))) {
    assign(total, plus(total, j))
  }
  return(total)
}
)";
  const std::string srcPath = writeTemp("compile_loop_while_for.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_loop_while_for_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs increment decrement sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  value++
  --value
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_increment_decrement.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_increment_decrement_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs brace constructor value") {
  const std::string source = R"(
[return<int>]
pick([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(pick{ 3i32 })
}
)";
  const std::string srcPath = writeTemp("compile_brace_constructor_value.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_brace_constructor_value_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs nested definition call") {
  const std::string source = R"(
[return<int>]
main() {
  [return<int>]
  helper() {
    return(5i32)
  }
  return(/main/helper())
}
)";
  const std::string srcPath = writeTemp("compile_nested_definition_call.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_nested_definition_call_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs map literal") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>{1i32=2i32, 3i32=4i32}
  return(9i32)
}
)";
  const std::string srcPath = writeTemp("compile_map_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_SUITE_END();
