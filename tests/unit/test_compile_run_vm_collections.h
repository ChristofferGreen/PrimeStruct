TEST_SUITE_BEGIN("primestruct.compile.run.vm.core");

TEST_CASE("runs vm with method call result") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
main() {
  [i32] value{5i32}
  return(plus(value.inc(), 2i32))
}
)";
  const std::string srcPath = writeTemp("vm_method_call.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("runs vm with raw string literal output") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("line\\nnext"raw_utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_raw_string_literal.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_raw_string_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "line\\\\nnext\n");
}

TEST_CASE("runs vm with raw single-quoted string literal output") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line('line\\nnext'raw_utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_raw_string_literal_single.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_raw_string_single_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "line\\\\nnext\n");
}

TEST_CASE("runs vm with string literal indexing") {
  const std::string source = R"(
[return<int>]
main() {
  return(at("hello"utf8, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_string_literal_index.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 101);
}

TEST_CASE("runs vm with literal method call") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
main() {
  return(1i32.inc())
}
)";
  const std::string srcPath = writeTemp("vm_method_literal.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with chained method calls") {
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
  const std::string srcPath = writeTemp("vm_method_chain.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("runs vm with import alias") {
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
  const std::string srcPath = writeTemp("vm_import_alias.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 5);
}

TEST_CASE("runs vm with multiple imports") {
  const std::string source = R"(
import /util, /math/*
namespace util {
  [return<int>]
  add([i32] a, [i32] b) {
    return(plus(a, b))
  }
}
[return<int>]
main() {
  return(plus(add(2i32, 3i32), min(7i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("vm_import_multiple.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("runs vm with whitespace-separated imports") {
  const std::string source = R"(
import /util /math/*
namespace util {
  [return<int>]
  add([i32] a, [i32] b) {
    return(plus(a, b))
  }
}
[return<int>]
main() {
  return(plus(add(2i32, 3i32), min(7i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("vm_import_whitespace.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("runs vm with capabilities and default effects") {
  const std::string source = R"(
[return<int> capabilities(io_out)]
main() {
  print_line("capabilities"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_capabilities_default_effects.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_capabilities_out.txt").string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --default-effects=default > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "capabilities\n");
}

TEST_CASE("default effects none requires explicit effects in vm") {
  const std::string source = R"(
[return<int>]
main() {
  print_line("no effects"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_print_no_effects.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_print_no_effects_err.txt").string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --default-effects=none 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("print_line requires io_out effect") != std::string::npos);
}

TEST_CASE("runs vm with implicit utf8 strings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("implicit")
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_implicit_utf8.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_implicit_utf8_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "implicit\n");
}

TEST_CASE("runs vm with implicit utf8 single-quoted strings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line('implicit')
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_implicit_utf8_single.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_implicit_utf8_single_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "implicit\n");
}

TEST_CASE("runs vm with escaped utf8 strings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("line\nnext"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_utf8_escaped.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_utf8_escaped_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "line\nnext\n");
}

TEST_CASE("runs vm with escaped utf8 single-quoted strings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line('line\nnext'utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_utf8_escaped_single.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_utf8_escaped_single_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "line\nnext\n");
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

TEST_CASE("runs vm with numeric array literals") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  print_line(values.count())
  print_line(values[1i64])
  print_line(values[2u64])
  return(values[0i32])
}
)";
  const std::string srcPath = writeTemp("vm_array_literals.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_array_literals_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 4);
  CHECK(readFile(outPath) == "3\n7\n9\n");
}

TEST_CASE("runs vm with numeric vector literals") {
  const std::string source = R"(
[return<int> effects(io_out, heap_alloc)]
main() {
  [vector<i32>] values{vector<i32>(4i32, 7i32, 9i32)}
  print_line(values.count())
  print_line(values[1i64])
  print_line(values[2u64])
  return(values[0i32])
}
)";
  const std::string srcPath = writeTemp("vm_vector_literals.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_vector_literals_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 4);
  CHECK(readFile(outPath) == "3\n7\n9\n");
}

TEST_CASE("runs vm with collection bracket literals") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [array<i32>] values{array<i32>[1i32, 2i32]}
  [vector<i32>] list{vector<i32>[3i32, 4i32]}
  [map<i32, i32>] table{map<i32, i32>[5i32=6i32]}
  return(plus(plus(values.count(), list.count()), count(table)))
}
)";
  const std::string srcPath = writeTemp("vm_collection_brackets.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 5);
}

TEST_CASE("runs vm with array literal count method") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32, 3i32).count())
}
)";
  const std::string srcPath = writeTemp("vm_array_literal_count.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with vector literal count method") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  return(vector<i32>(1i32, 2i32, 3i32).count())
}
)";
  const std::string srcPath = writeTemp("vm_vector_literal_count.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with vector method call") {
  const std::string source = R"(
[return<int>]
/vector/first([vector<i32>] items) {
  return(items[0i32])
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 7i32, 9i32)}
  return(values.first())
}
)";
  const std::string srcPath = writeTemp("vm_vector_method_call.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("runs vm with array literal unsafe access") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(4i32, 7i32, 9i32), 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_array_literal_unsafe.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm with vector literal unsafe access") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  return(at_unsafe(vector<i32>(4i32, 7i32, 9i32), 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_vector_literal_unsafe.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm with array count helper") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_array_count_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with array literal count helper") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(array<i32>(1i32, 2i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("vm_array_literal_count_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with vector count helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_vector_count_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with vector capacity helpers") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  [i32] a{capacity(values)}
  return(plus(a, values.capacity()))
}
)";
  const std::string srcPath = writeTemp("vm_vector_capacity_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("runs vm with vector capacity after pop") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  pop(values)
  return(capacity(values))
}
)";
  const std::string srcPath = writeTemp("vm_vector_capacity_after_pop.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("rejects vm vector growth helpers") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  push(values, 3i32)
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("vm_vector_push_unsupported.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_vector_push_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath) == "VM lowering error: vm backend does not support vector helper: push\n");
}

TEST_CASE("runs vm with vector shrink helpers") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32, 4i32)}
  pop(values)
  remove_at(values, 1i32)
  remove_swap(values, 0i32)
  last{at(values, 0i32)}
  size{count(values)}
  clear(values)
  return(plus(plus(last, size), count(values)))
}
)";
  const std::string srcPath = writeTemp("vm_vector_shrink_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("runs vm with math abs/sign/min/max") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [i32] a{abs(-5i32)}
  [i32] b{sign(-5i32)}
  [i32] c{min(7i32, 2i32)}
  [i32] d{max(7i32, 2i32)}
  return(plus(plus(a, b), plus(c, d)))
}
)";
  const std::string srcPath = writeTemp("vm_math_basic.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 13);
}

TEST_CASE("runs vm with qualified math names") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] a{/math/abs(-5i32)}
  [i32] b{/math/sign(-5i32)}
  [i32] c{/math/min(7i32, 2i32)}
  [i32] d{/math/max(7i32, 2i32)}
  [i32] e{convert<int>(/math/pi)}
  return(plus(plus(plus(a, b), plus(c, d)), e))
}
)";
  const std::string srcPath = writeTemp("vm_math_qualified.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 16);
}

TEST_CASE("runs vm with math saturate/lerp") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [i32] a{saturate(-2i32)}
  [i32] b{saturate(2i32)}
  [i32] c{lerp(0i32, 10i32, 1i32)}
  return(plus(plus(a, b), c))
}
)";
  const std::string srcPath = writeTemp("vm_math_saturate_lerp.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("runs vm with math clamp") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{clamp(2.5f32, 0.0f32, 1.0f32)}
  [u64] b{clamp(9u64, 2u64, 6u64)}
  return(plus(convert<int>(a), convert<int>(b)))
}
)";
  const std::string srcPath = writeTemp("vm_math_clamp.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm with math pow") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(pow(2i32, 6i32))
}
)";
  const std::string srcPath = writeTemp("vm_math_pow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 64);
}

TEST_CASE("runs vm with math pow rejects negative exponent") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(pow(2i32, -1i32))
}
)";
  const std::string srcPath = writeTemp("vm_math_pow_negative.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_math_pow_negative_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "pow exponent must be non-negative\n");
}

TEST_CASE("runs vm with math constant conversions") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(plus(convert<int>(pi), plus(convert<int>(tau), convert<int>(e))))
}
)";
  const std::string srcPath = writeTemp("vm_math_constants_convert.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("runs vm with math constants") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f64] sum{plus(pi, plus(tau, e))}
  return(convert<int>(sum))
}
)";
  const std::string srcPath = writeTemp("vm_math_constants_direct.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 12);
}

TEST_CASE("runs vm with math predicates") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] nan{divide(0.0f32, 0.0f32)}
  [f32] inf{divide(1.0f32, 0.0f32)}
  return(plus(
    plus(convert<int>(is_nan(nan)), convert<int>(is_inf(inf))),
    convert<int>(is_finite(1.0f32))
  ))
}
)";
  const std::string srcPath = writeTemp("vm_math_predicates.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with math rounding") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{floor(1.9f32)}
  [f32] b{ceil(1.1f32)}
  [f32] c{round(2.5f32)}
  [f32] d{trunc(-1.7f32)}
  [f32] e{fract(2.25f32)}
  return(plus(
    plus(convert<int>(a), convert<int>(b)),
    plus(convert<int>(c), plus(convert<int>(d), convert<int>(multiply(e, 4.0f32))))
  ))
}
)";
  const std::string srcPath = writeTemp("vm_math_rounding.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("runs vm with math roots") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{sqrt(9.0f32)}
  [f32] b{cbrt(27.0f32)}
  return(plus(convert<int>(a), convert<int>(b)))
}
)";
  const std::string srcPath = writeTemp("vm_math_roots.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("runs vm with math fma/hypot") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{fma(2.0f32, 3.0f32, 4.0f32)}
  [f32] b{hypot(1.0f32, 1.0f32)}
  return(plus(convert<int>(a), convert<int>(b)))
}
)";
  const std::string srcPath = writeTemp("vm_math_fma_hypot.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("runs vm with math copysign") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{copysign(3.0f32, -1.0f32)}
  [f32] b{copysign(4.0f32, 1.0f32)}
  return(convert<int>(plus(a, b)))
}
)";
  const std::string srcPath = writeTemp("vm_math_copysign.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm with math angle helpers") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{radians(90.0f32)}
  [f32] b{degrees(1.0f32)}
  return(plus(convert<int>(a), convert<int>(b)))
}
)";
  const std::string srcPath = writeTemp("vm_math_angles.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 58);
}

TEST_CASE("runs vm with math trig helpers") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{sin(0.0f32)}
  [f32] b{cos(0.0f32)}
  [f32] c{tan(0.0f32)}
  [f32] d{atan2(1.0f32, 0.0f32)}
  [f32] sum{plus(plus(a, b), c)}
  return(plus(convert<int>(sum), convert<int>(d)))
}
)";
  const std::string srcPath = writeTemp("vm_math_trig.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with math arc trig helpers") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{asin(0.0f32)}
  [f32] b{acos(0.0f32)}
  [f32] c{atan(1.0f32)}
  return(plus(plus(convert<int>(a), convert<int>(b)), convert<int>(c)))
}
)";
  const std::string srcPath = writeTemp("vm_math_arc_trig.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm with math exp/log") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{exp(0.0f32)}
  [f32] b{exp2(1.0f32)}
  [f32] c{log(1.0f32)}
  [f32] d{log2(2.0f32)}
  [f32] e{log10(1.0f32)}
  [f32] sum{plus(plus(a, b), plus(c, plus(d, e)))}
  return(convert<int>(plus(sum, 0.5f32)))
}
)";
  const std::string srcPath = writeTemp("vm_math_exp_log.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("runs vm with math hyperbolic") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{sinh(0.0f32)}
  [f32] b{cosh(0.0f32)}
  [f32] c{tanh(0.0f32)}
  [f32] d{asinh(0.0f32)}
  [f32] e{acosh(1.0f32)}
  [f32] f{atanh(0.0f32)}
  [f32] sum{plus(plus(a, b), plus(c, plus(d, plus(e, f))))}
  return(convert<int>(sum))
}
)";
  const std::string srcPath = writeTemp("vm_math_hyperbolic.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm with float pow") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(convert<int>(plus(pow(2.0f32, 3.0f32), 0.5f32)))
}
)";
  const std::string srcPath = writeTemp("vm_math_pow_float.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("runs vm with convert bool from integers") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(
    convert<int>(convert<bool>(0i32)),
    plus(convert<int>(convert<bool>(-1i32)), convert<int>(convert<bool>(5u64)))
  ))
}
)";
  const std::string srcPath = writeTemp("vm_convert_bool.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm float literals") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1.5f))
}
)";
  const std::string srcPath = writeTemp("vm_float_literal.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm float bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [float] value{1.5f}
  [float] other{2.0f32}
  return(convert<int>(plus(value, other)))
}
)";
  const std::string srcPath = writeTemp("vm_float_binding.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with vector literal count helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  return(count(vector<i32>(1i32, 2i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("vm_vector_literal_count_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with map literal") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>{1i32=2i32, 3i32=4i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("vm_map_literal.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm with map literal count helper") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(map<i32, i32>{1i32=2i32, 3i32=4i32}))
}
)";
  const std::string srcPath = writeTemp("vm_map_literal_count.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with map count helper") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_map_count_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with map method call") {
  const std::string source = R"(
[return<int>]
/map/size([map<i32, i32>] items) {
  return(count(items))
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(values.size())
}
)";
  const std::string srcPath = writeTemp("vm_map_method_call.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with map at helper") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(at(values, 3i32))
}
)";
  const std::string srcPath = writeTemp("vm_map_at.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("runs vm with map at_unsafe helper") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_map_at_unsafe.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with bool map access helpers") {
  const std::string source = R"(
[return<int>]
main() {
  [map<bool, i32>] values{map<bool, i32>{true=1i32, false=2i32}}
  return(plus(at(values, true), at_unsafe(values, false)))
}
)";
  const std::string srcPath = writeTemp("vm_map_bool_access.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with u64 map access helpers") {
  const std::string source = R"(
[return<int>]
main() {
  [map<u64, i32>] values{map<u64, i32>{2u64=7i32, 11u64=5i32}}
  return(plus(at(values, 2u64), at_unsafe(values, 11u64)))
}
)";
  const std::string srcPath = writeTemp("vm_map_u64_access.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 12);
}

TEST_CASE("vm map at rejects missing key") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(at(values, 9i32))
}
)";
  const std::string srcPath = writeTemp("vm_map_at_missing.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_map_at_missing_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "map key not found\n");
}

TEST_CASE("rejects vm map literal odd args") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>(1i32)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("vm_map_literal_odd.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm map literal type mismatch") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>(1i32, true)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("vm_map_literal_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with string-keyed map literals") {
  const std::string source = R"(
[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"raw_utf8, 1i32, "b"raw_utf8, 2i32)}
  [i32] a{at(values, "b"raw_utf8)}
  [i32] b{at_unsafe(values, "a"raw_utf8)}
  return(plus(plus(a, b), count(values)))
}
)";
  const std::string srcPath = writeTemp("vm_map_literal_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 5);
}

TEST_CASE("runs vm with string-keyed map binding lookup") {
  const std::string source = R"(
[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"raw_utf8, 1i32, "b"raw_utf8, 2i32)}
  [string] key{"b"raw_utf8}
  return(plus(at(values, key), count(values)))
}
)";
  const std::string srcPath = writeTemp("vm_map_literal_string_binding.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("rejects vm map literal string key from argv binding") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  [string] key{args[0i32]}
  map<string, i32>(key, 1i32)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("vm_map_literal_string_argv_key.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_map_literal_string_argv_key_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("map literal string keys") != std::string::npos);
}

TEST_CASE("vm array access checks bounds") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32)}
  return(plus(100i32, values[9i32]))
}
)";
  const std::string srcPath = writeTemp("vm_array_bounds.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_array_bounds_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm array access with u64 index") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(plus(100i32, values[1u64]))
}
)";
  const std::string srcPath = writeTemp("vm_array_u64.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 107);
}

TEST_CASE("vm array access rejects negative index") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32)}
  return(plus(100i32, values[-1i32]))
}
)";
  const std::string srcPath = writeTemp("vm_array_negative.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_array_negative_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm vector access checks bounds") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32)}
  return(plus(100i32, values[9i32]))
}
)";
  const std::string srcPath = writeTemp("vm_vector_bounds.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_vector_bounds_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm vector access rejects negative index") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32)}
  return(plus(100i32, values[-1i32]))
}
)";
  const std::string srcPath = writeTemp("vm_vector_negative.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_vector_negative_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm array unsafe access reads element") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  print_line(at_unsafe(values, 1i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_array_unsafe.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_array_unsafe_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "7\n");
}

TEST_CASE("vm array unsafe access with u64 index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  print_line(at_unsafe(values, 1u64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_array_unsafe_u64.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_array_unsafe_u64_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "7\n");
}

TEST_CASE("vm argv access checks bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[9i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_bounds.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_bounds_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm argv access rejects negative index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[-1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_negative.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_negative_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm argv unsafe access skips bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(at_unsafe(args, 9i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_unsafe_bounds.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_unsafe_out.txt").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_unsafe_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("vm argv unsafe access with u64 index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(at_unsafe(args, 1u64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_unsafe_u64.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_unsafe_u64_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("vm argv unsafe access skips negative index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(at_unsafe(args, -1i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_unsafe_negative.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_unsafe_negative_out.txt").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_unsafe_negative_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("vm argv binding checks bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  [string] value{args[9i32]}
  print_line(value)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_binding_bounds.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_binding_bounds_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm argv binding unsafe skips bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  [string] value{at_unsafe(args, 9i32)}
  print_line(value)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_binding_unsafe.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_binding_unsafe_out.txt").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_binding_unsafe_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("vm argv unsafe binding copy skips bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  [string] first{at_unsafe(args, 9i32)}
  [string] second{first}
  print_line(second)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_binding_unsafe_copy.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_binding_unsafe_copy_out.txt").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_binding_unsafe_copy_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("vm argv string binding count fails") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  [string] value{args[0i32]}
  return(count(value))
}
)";
  const std::string srcPath = writeTemp("vm_argv_binding_count.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_binding_count_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath) ==
        "VM lowering error: vm backend only supports count() on string literals or string bindings\n");
}

TEST_CASE("vm argv string binding index fails") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  [string] value{args[0i32]}
  return(value[0i32])
}
)";
  const std::string srcPath = writeTemp("vm_argv_binding_index.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_binding_index_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath) ==
        "VM lowering error: vm backend only supports indexing into string literals or string bindings\n");
}

TEST_CASE("vm argv call argument checks bounds") {
  const std::string source = R"(
[return<void> effects(io_out)]
echo([string] msg) {
  print_line(msg)
}

[return<int> effects(io_out)]
main([array<string>] args) {
  echo(args[9i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_call_bounds.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_call_bounds_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm argv call argument unsafe skips bounds") {
  const std::string source = R"(
[return<void> effects(io_out)]
echo([string] msg) {
  print_line(msg)
}

[return<int> effects(io_out)]
main([array<string>] args) {
  echo(at_unsafe(args, 9i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_call_unsafe.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_call_unsafe_out.txt").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_call_unsafe_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("writes serialized ir output") {
  const std::string source = R"(
[return<int>]
main() {
  return(3i32)
}
)";
  const std::string srcPath = writeTemp("compile_ir_simple.prime", source);
  const std::string irPath = (std::filesystem::temp_directory_path() / "primec_ir_simple.psir").string();
  const std::string compileCmd = "./primec --emit=ir " + srcPath + " -o " + irPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  std::ifstream file(irPath, std::ios::binary);
  REQUIRE(file.good());
  std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  primec::IrModule module;
  std::string error;
  REQUIRE(primec::deserializeIr(data, module, error));
  CHECK(error.empty());
  CHECK(module.entryIndex == 0);
  REQUIRE(module.functions.size() == 1);
  CHECK(module.functions[0].name == "/main");
}

TEST_CASE("no transforms rejects sugar") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_no_transforms.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_no_transforms_exe").string();

  const std::string compileCmd = "./primec --emit=cpp --no-transforms " + srcPath + " -o " + exePath;
  CHECK(runCommand(compileCmd) != 0);
}

TEST_CASE("no transforms rejects implicit utf8") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("implicit")
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_no_transforms_implicit_utf8.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_no_transforms_implicit_utf8_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=cpp --no-transforms " + srcPath + " -o /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("string literal requires utf8/ascii/raw_utf8/raw_ascii suffix") != std::string::npos);
}

TEST_CASE("writes outputs under out dir") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_out_dir.prime", source);
  const std::filesystem::path outDir = std::filesystem::temp_directory_path() / "primec_out_dir_test";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  const std::string outFile = "primec_out_dir.cpp";
  const std::string expectedPath = (outDir / outFile).string();
  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o " + outFile + " --out-dir " + outDir.string();
  CHECK(runCommand(compileCmd) == 0);
  CHECK(std::filesystem::exists(expectedPath));
}

TEST_CASE("defaults to cpp extension for emit=cpp") {
  const std::string source = R"(
[return<int>]
main() {
  return(2i32)
}
)";
  const std::string srcPath = writeTemp("compile_default_cpp.prime", source);
  const std::filesystem::path outDir = std::filesystem::temp_directory_path() / "primec_cpp_default_out";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " --out-dir " + outDir.string() + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  std::filesystem::path outputPath = outDir / std::filesystem::path(srcPath).stem();
  outputPath.replace_extension(".cpp");
  CHECK(std::filesystem::exists(outputPath));
}

TEST_CASE("defaults to psir extension for emit=ir") {
  const std::string source = R"(
[return<int>]
main() {
  return(3i32)
}
)";
  const std::string srcPath = writeTemp("compile_default_ir.prime", source);
  const std::filesystem::path outDir = std::filesystem::temp_directory_path() / "primec_ir_default_out";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::string compileCmd =
      "./primec --emit=ir " + srcPath + " --out-dir " + outDir.string() + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  std::filesystem::path outputPath = outDir / std::filesystem::path(srcPath).stem();
  outputPath.replace_extension(".psir");
  CHECK(std::filesystem::exists(outputPath));
}

TEST_CASE("compiles and runs void main") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] value{1i32}
}
)";
  const std::string srcPath = writeTemp("compile_void_main.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_void_main_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs explicit void return") {
  const std::string source = R"(
[return<void>]
main() {
  return()
}
)";
  const std::string srcPath = writeTemp("compile_void_return.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_void_return_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs implicit void main") {
  const std::string source = R"(
main() {
  [i32] value{1i32}
  value
}
)";
  const std::string srcPath = writeTemp("compile_void_implicit.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_void_implicit_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs argv count") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_args.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
  CHECK(runCommand(exePath + " alpha beta") == 3);
}

TEST_CASE("compiles and runs argv count helper") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(count(args))
}
)";
  const std::string srcPath = writeTemp("compile_args_count_helper.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
  CHECK(runCommand(exePath + " alpha beta") == 3);
}

TEST_CASE("compiles and runs argv error output in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_line_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_error_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_args_error_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("compiles and runs argv error output without newline in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error_no_newline.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_error_no_newline_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_args_error_no_newline_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs argv error output u64 index in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_error(args[1u64])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_error_u64_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_args_error_u64_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs argv unsafe error output in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_error(at_unsafe(args, 1i32))
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error_unsafe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_error_unsafe_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_args_error_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs argv unsafe line error output in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line_error(at_unsafe(args, 1i32))
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_line_error_unsafe.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_args_line_error_unsafe_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_args_line_error_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("compiles and runs argv print in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 2i32)) {
    print_line(args[1i32])
    print_line(args[2i32])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_print.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_print_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_args_print_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\nbeta\n");
}

TEST_CASE("compiles and runs argv print without newline in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print(args[1i32])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_print_no_newline.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_print_no_newline_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_args_print_no_newline_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha");
}

TEST_CASE("compiles and runs argv print with u64 index in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line(args[1u64])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_print_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_print_u64_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_args_print_u64_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs argv unsafe access in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 2i32)) {
    print_line(at_unsafe(args, 1i32))
    print_line(at_unsafe(args, 2i32))
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_unsafe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_unsafe_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_args_unsafe_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\nbeta\n");
}

TEST_CASE("compiles and runs argv unsafe access with u64 index in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line(at_unsafe(args, 1u64))
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_unsafe_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_unsafe_u64_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_args_unsafe_u64_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs array literal") {
  const std::string source = R"(
[return<int>]
main() {
  array<i32>{1i32, 2i32, 3i32}
  return(7i32)
}
)";
  const std::string srcPath = writeTemp("compile_array_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array literal count method") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32, 3i32).count())
}
)";
  const std::string srcPath = writeTemp("compile_array_literal_count.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_literal_count_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs array literal unsafe access") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(4i32, 7i32, 9i32), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_array_literal_unsafe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_literal_unsafe_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array count helper") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_array_count_helper.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs array literal count helper") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(array<i32>(1i32, 2i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("compile_array_literal_count_helper.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_array_literal_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs literal method call in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
main() {
  return(1i32.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_SUITE_END();
