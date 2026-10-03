#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.control");

// The programs of tests/unit/compile_run/native_backend/test_compile_run_native_backend_control.cpp
// that only check an exit code and stdout, run through every execution config
// (VM step kernel and flat loop, native, and with PRIMESTRUCT_MATRIX_CONFIGS=all
// optexe and the old C++ emitter). They replaced their native-only copies in
// tests/unit/compile_run/native_backend/test_compile_run_native_backend_control.cpp
// (TODO-5466, batch 1).

TEST_CASE("native void executable") {
  program_matrix::ProgramCase program;
  program.name = "native_void_executable_0";
  program.source = R"(
[return<void>]
main() {
  [i32] value{1i32}
}
)";
  program.exitCode = 0;
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("native explicit void return") {
  program_matrix::ProgramCase program;
  program.name = "native_explicit_void_return_1";
  program.source = R"(
[return<void>]
main() {
  return()
}
)";
  program.exitCode = 0;
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("native exe reads and reassigns a mut local") {
  program_matrix::ProgramCase program;
  program.name = "native_exe_reads_and_reassigns_a_mut_local_2";
  program.source = R"(
[return<int>]
main() {
  [i32 mut] value{2i32}
  assign(value, plus(value, 3i32))
  return(value)
}
)";
  program.exitCode = 5;
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("native if/else selects the right branch") {
  program_matrix::ProgramCase program;
  program.name = "native_if_else_selects_the_right_branch_3";
  program.source = R"(
[return<int>]
main() {
  [i32] value{4i32}
  if(greater_equal(value, 4i32)) {
    return(9i32)
  } else {
    return(2i32)
  }
}
)";
  program.exitCode = 9;
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("native repeat() loop runs a fixed count") {
  program_matrix::ProgramCase program;
  program.name = "native_repeat_loop_runs_a_fixed_count_4";
  program.source = R"(
[return<int>]
main() {
  [i32 mut] value{0i32}
  repeat(3i32) {
    assign(value, plus(value, 2i32))
  }
  return(value)
}
)";
  program.exitCode = 6;
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("native for binding condition") {
  program_matrix::ProgramCase program;
  program.name = "native_for_binding_condition_5";
  program.source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  for([i32 mut] i{0i32} [bool] keep{less_than(i, 3i32)} assign(i, plus(i, 1i32))) {
    if(keep, then(){ assign(total, plus(total, 2i32)) }, else(){})
  }
  return(total)
}
)";
  program.exitCode = 6;
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("native shared_scope for binding condition") {
  program_matrix::ProgramCase program;
  program.name = "native_shared_scope_for_binding_condition_6";
  program.source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  [shared_scope]
  for([i32 mut] i{0i32} [bool] keep{less_than(i, 3i32)} assign(i, plus(i, 1i32))) {
    [i32 mut] acc{0i32}
    if(keep, then(){ assign(acc, plus(acc, 1i32)) }, else(){})
    assign(total, plus(total, acc))
  }
  return(total)
}
)";
  program.exitCode = 6;
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("native shared_scope while loop") {
  program_matrix::ProgramCase program;
  program.name = "native_shared_scope_while_loop_7";
  program.source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  [shared_scope]
  while(less_than(i, 3i32)) {
    [i32 mut] acc{0i32}
    assign(acc, plus(acc, 1i32))
    assign(total, plus(total, acc))
    assign(i, plus(i, 1i32))
  }
  return(total)
}
)";
  program.exitCode = 6;
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("native pointer helpers") {
  program_matrix::ProgramCase program;
  program.name = "native_pointer_helpers_8";
  program.source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [Pointer<i32> mut] ptr{location(value)}
  assign(dereference(ptr), 6i32)
  return(dereference(ptr))
}
)";
  program.exitCode = 6;
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("native plus() on a pointer offsets by zero") {
  program_matrix::ProgramCase program;
  program.name = "native_plus_on_a_pointer_offsets_by_zero_9";
  program.source = R"(
[return<int>]
main() {
  [i32] value{5i32}
  return(dereference(plus(location(value), 0i32)))
}
)";
  program.exitCode = 5;
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("default effects token enables io output") {
  program_matrix::ProgramCase program;
  program.name = "default_effects_token_enables_io_output_10";
  program.source = R"(
[return<int>]
main() {
  print_line("default effects"utf8)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "default effects\n";
  program.flags = {"--default-effects=default"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("entry defaults apply to helpers") {
  program_matrix::ProgramCase program;
  program.name = "entry_defaults_apply_to_helpers_11";
  program.source = R"(
[return<int>]
main() {
  log()
  return(0i32)
}

[return<void>]
log() {
  print_line("helper"utf8)
  return()
}
)";
  program.exitCode = 0;
  program.stdoutText = "helper\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("default effects allow capabilities in native") {
  program_matrix::ProgramCase program;
  program.name = "default_effects_allow_capabilities_in_native_12";
  program.source = R"(
[return<int> capabilities(io_out)]
main() {
  print_line("capabilities"utf8)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "capabilities\n";
  program.flags = {"--default-effects=default"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("native implicit utf8 strings") {
  program_matrix::ProgramCase program;
  program.name = "native_implicit_utf8_strings_13";
  program.source = R"(
[return<int> effects(io_out)]
main() {
  print_line("implicit")
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "implicit\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("native implicit utf8 single-quoted strings") {
  program_matrix::ProgramCase program;
  program.name = "native_implicit_utf8_single_quoted_strings_14";
  program.source = R"(
[return<int> effects(io_out)]
main() {
  print_line('implicit')
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "implicit\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("native escaped utf8 strings") {
  program_matrix::ProgramCase program;
  program.name = "native_escaped_utf8_strings_15";
  program.source = R"(
[return<int> effects(io_out)]
main() {
  print_line("line\nnext"utf8)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "line\nnext\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("native raw utf8 single-quoted strings") {
  program_matrix::ProgramCase program;
  program.name = "native_raw_utf8_single_quoted_strings_16";
  program.source = R"(
[return<int> effects(io_out)]
main() {
  print_line('line\nnext'utf8)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "line\\nnext\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("native string binding print") {
  program_matrix::ProgramCase program;
  program.name = "native_string_binding_print_17";
  program.source = R"(
[return<int> effects(io_out)]
main() {
  [string] greeting{"hi"ascii}
  print_line(greeting)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "hi\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("native raw string literal output") {
  program_matrix::ProgramCase program;
  program.name = "native_raw_string_literal_output_18";
  program.source = R"(
[return<int> effects(io_out)]
main() {
  print_line("line\\nnext"raw_utf8)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "line\\\\nnext\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("native raw single-quoted string output") {
  program_matrix::ProgramCase program;
  program.name = "native_raw_single_quoted_string_output_19";
  program.source = R"(
[return<int> effects(io_out)]
main() {
  print_line('line\\nnext'raw_utf8)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "line\\\\nnext\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("native string binding copy") {
  program_matrix::ProgramCase program;
  program.name = "native_string_binding_copy_20";
  program.source = R"(
[return<int> effects(io_out)]
main() {
  [string] greeting{"hey"utf8}
  [string] copy{greeting}
  print_line(copy)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "hey\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("native string count and indexing") {
  program_matrix::ProgramCase program;
  program.name = "native_string_count_and_indexing_21";
  program.source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  [i32] a{text[0i32]}
  [i32] b{at_unsafe(text, 1i32)}
  [i32] len{text.count()}
  return(plus(plus(a, b), len))
}
)";
  program.exitCode = (97 + 98 + 3);
  program_matrix::runProgramMatrix(program);
}

// i32 wraps at 32 bits on every backend (TODO-5477): lowering follows i32
// add/sub/mul/div/negate with SextI32, so comparisons and widening conversions
// after an overflow see the wrapped value, as in the C++ emitter (`exe`).
TEST_CASE("i32 arithmetic wraps and compares after overflow") {
  program_matrix::ProgramCase program;
  program.name = "i32_wrap_basic";
  program.source = R"(
[return<int> effects(io_out)]
main() {
  [i32 mut] total{2147483647i32}
  assign(total, plus(total, 1i32))
  print_line(total)
  print_line(if(less_than(total, 0i32), then() { 1i32 }, else() { 0i32 }))
  [i32 mut] square{65536i32}
  assign(square, multiply(square, square))
  print_line(square)
  print_line(if(equal(square, 0i32), then() { 1i32 }, else() { 0i32 }))
  [i64] wide{convert<i64>(total)}
  print_line(wide)
  [i32] lowest{minus(negate(2147483647i32), 1i32)}
  print_line(negate(lowest))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "-2147483648\n1\n0\n1\n-2147483648\n-2147483648\n";
  program.onlyConfigs = {
      "vm-step-O0", "vm-O0", "vm-O2", "native-O0", "native-O2", "optexe-O2", "exe"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("i32 loops keep wrapping in promoted locals and fused updates") {
  program_matrix::ProgramCase program;
  program.name = "i32_wrap_loops";
  program.source = R"(
[return<int> effects(io_out)]
main() {
  [i32 mut] x{2147483000i32}
  repeat(1000i32) {
    assign(x, plus(x, 1i32))
  }
  print_line(x)
  [i32 mut] h{1i32}
  repeat(40i32) {
    assign(h, multiply(h, 3i32))
  }
  print_line(h)
  print_line(if(less_than(h, 0i32), then() { 1i32 }, else() { 0i32 }))
  [i32 mut] sum{0i32}
  [i32 mut] i{1i32}
  repeat(100000i32) {
    assign(sum, plus(sum, multiply(i, i)))
    assign(i, plus(i, 1i32))
  }
  print_line(sum)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "-2147483296\n689956897\n0\n1626540144\n";
  program.onlyConfigs = {
      "vm-step-O0", "vm-O0", "vm-O2", "native-O0", "native-O2", "optexe-O2", "exe"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("i32 increment, decrement, abs and pow wrap like plus and multiply") {
  program_matrix::ProgramCase program;
  program.name = "i32_wrap_builtins";
  program.source = R"(
import /std/math/*

[return<int> effects(io_out)]
main() {
  [i32] lowest{minus(negate(2147483647i32), 1i32)}
  [i32 mut] up{2147483647i32}
  increment(up)
  print_line(up)
  print_line(if(less_than(up, 0i32), then() { 1i32 }, else() { 0i32 }))
  [i32 mut] down{lowest}
  decrement(down)
  print_line(down)
  print_line(if(less_than(down, 0i32), then() { 1i32 }, else() { 0i32 }))
  print_line(if(less_than(abs(lowest), 0i32), then() { 1i32 }, else() { 0i32 }))
  print_line(pow(3i32, 23i32))
  print_line(if(less_than(pow(3i32, 23i32), 0i32), then() { 1i32 }, else() { 0i32 }))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "-2147483648\n1\n2147483647\n0\n1\n-346101685\n1\n";
  program.onlyConfigs = {
      "vm-step-O0", "vm-O0", "vm-O2", "native-O0", "native-O2", "optexe-O2", "exe"};
  program_matrix::runProgramMatrix(program);
}
