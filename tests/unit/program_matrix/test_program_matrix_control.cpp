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

// A guarded constant add `if (compare) { x = x + K }` is emitted branchless on x86_64
// (setcc scaled by K and added to the local), for register and frame locals alike.
TEST_CASE("guarded constant adds stay exact when emitted branchless") {
  program_matrix::ProgramCase program;
  program.name = "guarded_constant_adds";
  program.source = R"(
[return<int> effects(io_out)]
main() {
  [i64 mut] hits{0i64}
  [i64 mut] weighted{0i64}
  [i64 mut] misses{100i64}
  [i64 mut] c0{0i64}
  [i64 mut] c1{0i64}
  [i64 mut] c2{0i64}
  [i64 mut] c3{0i64}
  [i64 mut] c4{0i64}
  [i64 mut] c5{0i64}
  [i64 mut] c6{0i64}
  [i64 mut] c7{0i64}
  [i64 mut] c8{0i64}
  [i64 mut] c9{0i64}
  [i32 mut] i{0i32}
  repeat(20i32) {
    if(equal(i, 3i32)) { assign(hits, plus(hits, 1i64)) }
    if(less_than(i, 7i32)) { assign(weighted, plus(weighted, 5i64)) }
    if(greater_than(i, 10i32)) { assign(misses, minus(misses, 2i64)) }
    if(not_equal(i, 4i32)) { assign(hits, plus(hits, 2i64)) }
    if(equal(minus(i, multiply(divide(i, 2i32), 2i32)), 0i32)) { assign(c0, plus(c0, 1i64)) }
    if(equal(minus(i, multiply(divide(i, 3i32), 3i32)), 0i32)) { assign(c1, plus(c1, 2i64)) }
    if(equal(minus(i, multiply(divide(i, 4i32), 4i32)), 0i32)) { assign(c2, plus(c2, 3i64)) }
    if(equal(minus(i, multiply(divide(i, 5i32), 5i32)), 0i32)) { assign(c3, plus(c3, 4i64)) }
    if(equal(minus(i, multiply(divide(i, 6i32), 6i32)), 0i32)) { assign(c4, plus(c4, 5i64)) }
    if(equal(minus(i, multiply(divide(i, 7i32), 7i32)), 0i32)) { assign(c5, plus(c5, 6i64)) }
    if(equal(minus(i, multiply(divide(i, 8i32), 8i32)), 0i32)) { assign(c6, plus(c6, 7i64)) }
    if(equal(minus(i, multiply(divide(i, 9i32), 9i32)), 0i32)) { assign(c7, plus(c7, 8i64)) }
    if(equal(minus(i, multiply(divide(i, 10i32), 10i32)), 0i32)) { assign(c8, plus(c8, 9i64)) }
    if(equal(minus(i, multiply(divide(i, 11i32), 11i32)), 0i32)) { assign(c9, plus(c9, 10i64)) }
    assign(i, plus(i, 1i32))
  }
  print_line(hits)
  print_line(weighted)
  print_line(misses)
  print_line(c0)
  print_line(c1)
  print_line(c2)
  print_line(c3)
  print_line(c4)
  print_line(c5)
  print_line(c6)
  print_line(c7)
  print_line(c8)
  print_line(c9)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "39\n35\n82\n10\n14\n15\n16\n20\n18\n21\n24\n18\n20\n";
  program.onlyConfigs = {
      "vm-step-O0", "vm-O0", "vm-O2", "native-O0", "native-O2", "optexe-O2", "exe"};
  program_matrix::runProgramMatrix(program);
}

// Locals next to an array whose address is taken stay in their frame slots; the loop also
// calls a function, divides and prints, all of which run through templates with the live
// registers saved around them.
TEST_CASE("register allocation keeps frame locals, calls and prints in a loop exact") {
  program_matrix::ProgramCase program;
  program.name = "register_allocation_frame_locals";
  program.source = R"(
[return<int>]
mix([i32] a, [i32] b, [i32] c) {
  return(plus(multiply(a, 3i32), minus(b, c)))
}

[return<int>]
fact([i32] n) {
  if(less_than(n, 2i32), then() { return(1i32) }, else() { return(multiply(n, fact(minus(n, 1i32)))) })
}

[return<int> effects(io_out)]
main() {
  [array<i32>] table{array<i32>(3i32, 5i32, 7i32, 11i32)}
  [i64 mut] a0{1i64}
  [i64 mut] a1{2i64}
  [i64 mut] a2{3i64}
  [i64 mut] a3{4i64}
  [i64 mut] a4{5i64}
  [i64 mut] a5{6i64}
  [i64 mut] a6{7i64}
  [i64 mut] a7{8i64}
  [i64 mut] a8{9i64}
  [i64 mut] a9{10i64}
  [i64 mut] a10{11i64}
  [i64 mut] a11{12i64}
  [i32 mut] i{0i32}
  [i32 mut] calls{0i32}
  repeat(12i32) {
    assign(a0, plus(a0, a11))
    assign(a1, plus(a1, a0))
    assign(a2, minus(a2, a1))
    assign(a3, plus(a3, multiply(a2, 2i64)))
    assign(a4, plus(a4, a3))
    assign(a5, divide(plus(a5, a4), 3i64))
    assign(a6, plus(a6, a5))
    assign(a7, minus(a7, a6))
    assign(a8, plus(a8, a7))
    assign(a9, plus(a9, a8))
    assign(a10, plus(a10, a9))
    assign(a11, plus(a11, convert<i64>(table[minus(i, multiply(divide(i, 4i32), 4i32))])))
    assign(calls, plus(calls, mix(i, calls, 2i32)))
    if(equal(minus(i, multiply(divide(i, 5i32), 5i32)), 0i32)) {
      print_line(a3)
    }
    assign(i, plus(i, 1i32))
  }
  print_line(a0)
  print_line(a1)
  print_line(a2)
  print_line(a4)
  print_line(a5)
  print_line(a6)
  print_line(a7)
  print_line(a8)
  print_line(a9)
  print_line(a10)
  print_line(a11)
  print_line(calls)
  print_line(fact(10i32))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "-20\n-4276\n-45586\n535\n2578\n-9954\n-190465\n-81056\n-"
                       "210336\n503899\n1128628\n2386501\n4801017\n90\n4059\n3628800\n";
  program.onlyConfigs = {
      "vm-step-O0", "vm-O0", "vm-O2", "native-O0", "native-O2", "optexe-O2", "exe"};
  program_matrix::runProgramMatrix(program);
}

// Fourteen values live across the loop do not fit the allocator's registers: the coldest
// spill to frame slots, and calls, divisions and prints run with registers saved around them.
TEST_CASE("register allocation spills under pressure and stays exact") {
  program_matrix::ProgramCase program;
  program.name = "register_allocation_spills";
  program.source = R"(
[return<int>]
mix([i32] a, [i32] b, [i32] c) {
  return(plus(multiply(a, 3i32), minus(b, c)))
}

[return<int>]
fact([i32] n) {
  if(less_than(n, 2i32), then() { return(1i32) }, else() { return(multiply(n, fact(minus(n, 1i32)))) })
}

[return<int> effects(io_out)]
main() {
  [i64 mut] a0{1i64}
  [i64 mut] a1{2i64}
  [i64 mut] a2{3i64}
  [i64 mut] a3{4i64}
  [i64 mut] a4{5i64}
  [i64 mut] a5{6i64}
  [i64 mut] a6{7i64}
  [i64 mut] a7{8i64}
  [i64 mut] a8{9i64}
  [i64 mut] a9{10i64}
  [i64 mut] a10{11i64}
  [i64 mut] a11{12i64}
  [i32 mut] i{0i32}
  [i32 mut] calls{0i32}
  repeat(12i32) {
    assign(a0, plus(a0, a11))
    assign(a1, plus(a1, a0))
    assign(a2, minus(a2, a1))
    assign(a3, plus(a3, multiply(a2, 2i64)))
    assign(a4, plus(a4, a3))
    assign(a5, divide(plus(a5, a4), 3i64))
    assign(a6, plus(a6, a5))
    assign(a7, minus(a7, a6))
    assign(a8, plus(a8, a7))
    assign(a9, plus(a9, a8))
    assign(a10, plus(a10, a9))
    assign(a11, plus(a11, convert<i64>(minus(i, multiply(divide(i, 4i32), 4i32)))))
    assign(calls, plus(calls, mix(i, calls, 2i32)))
    if(equal(minus(i, multiply(divide(i, 5i32), 5i32)), 0i32)) {
      print_line(a3)
    }
    assign(i, plus(i, 1i32))
  }
  print_line(a0)
  print_line(a1)
  print_line(a2)
  print_line(a4)
  print_line(a5)
  print_line(a6)
  print_line(a7)
  print_line(a8)
  print_line(a9)
  print_line(a10)
  print_line(a11)
  print_line(calls)
  print_line(fact(10i32))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "-20\n-3340\n-28732\n229\n1289\n-5569\n-123761\n-53157\n-"
                       "145039\n361738\n837217\n1818883\n3742895\n30\n4059\n3628800\n";
  program.onlyConfigs = {
      "vm-step-O0", "vm-O0", "vm-O2", "native-O0", "native-O2", "optexe-O2", "exe"};
  program_matrix::runProgramMatrix(program);
}

// Float arithmetic, comparisons, negation and conversions run inline on the allocated registers
// (as bit patterns, through xmm0/xmm1), and calls return their results in rax.
TEST_CASE("register allocation keeps float values and call results exact") {
  program_matrix::ProgramCase program;
  program.name = "register_allocation_floats";
  program.source = R"(
[return<f64>]
scale([f64] x, [f64] factor) {
  return(multiply(x, factor))
}

[return<f32>]
half([f32] x) {
  return(divide(x, 2.0f32))
}

[return<i64>]
widen([i32] x) {
  return(multiply(convert<i64>(x), 3000000000i64))
}

[return<int> effects(io_out)]
main() {
  [f64 mut] x{0.0f64}
  [f64 mut] total{0.0f64}
  [f32 mut] small{1.5f32}
  [i32 mut] below{0i32}
  [i32 mut] i{0i32}
  while(less_than(i, 40i32)) {
    assign(x, minus(scale(convert<f64>(i), 0.75f64), 7.0f64))
    if(less_than(x, 0.0f64)) {
      assign(below, plus(below, 1i32))
    }
    if(greater_equal(x, 10.0f64)) {
      assign(total, plus(total, negate(x)))
    } else {
      assign(total, plus(total, x))
    }
    if(not_equal(negate(small), -3.0f32)) {
      assign(small, plus(half(small), convert<f32>(i)))
    }
    assign(i, plus(i, 1i32))
  }
  print_line(below)
  print_line(convert<i64>(multiply(total, 100.0f64)))
  print_line(convert<i32>(small))
  print_line(convert<i64>(multiply(convert<f64>(small), 1000.0f64)))
  print_line(widen(convert<i32>(negate(x))))
  if(equal(x, 22.25f64)) {
    return(3i32)
  }
  return(0i32)
}
)";
  program.exitCode = 3;
  program.stdoutText = "10\n-24750\n76\n76000\n-66000000000\n";
  program.onlyConfigs = {
      "vm-step-O0", "vm-O0", "vm-O2", "native-O0", "native-O2", "optexe-O2", "exe"};
  program_matrix::runProgramMatrix(program);
}

// Seventeen floats live across a loop with a call in it: more than the fourteen allocatable xmm
// registers, so some spill, and the xmm registers live across each call are saved around it.
TEST_CASE("register allocation spills and saves xmm registers") {
  program_matrix::ProgramCase program;
  program.name = "register_allocation_xmm_pressure";
  program.source = R"(
[return<f64>]
blend([f64] a, [f64] b) {
  return(plus(multiply(a, 0.5f64), b))
}

[return<int> effects(io_out)]
main() {
  [f64 mut] f0{1.0f64}
  [f64 mut] f1{2.0f64}
  [f64 mut] f2{3.0f64}
  [f64 mut] f3{4.0f64}
  [f64 mut] f4{5.0f64}
  [f64 mut] f5{6.0f64}
  [f64 mut] f6{7.0f64}
  [f64 mut] f7{8.0f64}
  [f64 mut] f8{9.0f64}
  [f64 mut] f9{10.0f64}
  [f64 mut] f10{11.0f64}
  [f64 mut] f11{12.0f64}
  [f64 mut] f12{13.0f64}
  [f64 mut] f13{14.0f64}
  [f64 mut] f14{15.0f64}
  [f64 mut] f15{16.0f64}
  [f64 mut] f16{17.0f64}
  [i32 mut] i{0i32}
  while(less_than(i, 25i32)) {
    assign(f0, plus(f0, multiply(f1, 0.125f64)))
    assign(f1, plus(f1, multiply(f2, 0.125f64)))
    assign(f2, blend(f2, f3))
    assign(f3, plus(f3, multiply(f4, 0.125f64)))
    assign(f4, divide(minus(f4, f5), 1.25f64))
    assign(f5, plus(f5, multiply(f6, 0.125f64)))
    assign(f6, plus(f6, multiply(f7, 0.125f64)))
    assign(f7, blend(f7, f8))
    assign(f8, plus(f8, multiply(f9, 0.125f64)))
    assign(f9, divide(minus(f9, f10), 1.25f64))
    assign(f10, plus(f10, multiply(f11, 0.125f64)))
    assign(f11, plus(f11, multiply(f12, 0.125f64)))
    assign(f12, blend(f12, f13))
    assign(f13, plus(f13, multiply(f14, 0.125f64)))
    assign(f14, divide(minus(f14, f15), 1.25f64))
    assign(f15, plus(f15, multiply(f16, 0.125f64)))
    assign(f16, plus(f16, multiply(f0, 0.125f64)))
    if(greater_than(f0, f16)) {
      assign(f16, negate(f16))
    }
    assign(i, plus(i, 1i32))
  }
  print_line(convert<i64>(multiply(f0, 1000.0f64)))
  print_line(convert<i64>(multiply(f1, 1000.0f64)))
  print_line(convert<i64>(multiply(f2, 1000.0f64)))
  print_line(convert<i64>(multiply(f3, 1000.0f64)))
  print_line(convert<i64>(multiply(f4, 1000.0f64)))
  print_line(convert<i64>(multiply(f5, 1000.0f64)))
  print_line(convert<i64>(multiply(f6, 1000.0f64)))
  print_line(convert<i64>(multiply(f7, 1000.0f64)))
  print_line(convert<i64>(multiply(f8, 1000.0f64)))
  print_line(convert<i64>(multiply(f9, 1000.0f64)))
  print_line(convert<i64>(multiply(f10, 1000.0f64)))
  print_line(convert<i64>(multiply(f11, 1000.0f64)))
  print_line(convert<i64>(multiply(f12, 1000.0f64)))
  print_line(convert<i64>(multiply(f13, 1000.0f64)))
  print_line(convert<i64>(multiply(f14, 1000.0f64)))
  print_line(convert<i64>(multiply(f15, 1000.0f64)))
  print_line(convert<i64>(multiply(f16, 1000.0f64)))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "-127334\n"
                       "-251363\n"
                       "-248478\n"
                       "-104572\n"
                       "220700\n"
                       "-188308\n"
                       "-432580\n"
                       "-499861\n"
                       "-251919\n"
                       "138157\n"
                       "-174480\n"
                       "-472127\n"
                       "-640996\n"
                       "-383474\n"
                       "-269777\n"
                       "73076\n"
                       "-30168\n";
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

TEST_CASE("i32 lerp, saturate, clamp, min, max and sign agree at the limits") {
  program_matrix::ProgramCase program;
  program.name = "i32_limit_builtins";
  program.source = R"(
import /std/math/*

[return<int> effects(io_out)]
main() {
  [i32] big{2147483647i32}
  [i32] low{minus(negate(2147483647i32), 1i32)}
  print_line(lerp(low, big, 1i32))
  print_line(if(less_than(lerp(low, big, 2i32), 0i32), then() { 1i32 }, else() { 0i32 }))
  print_line(saturate(big))
  print_line(clamp(big, low, big))
  print_line(min(big, low))
  print_line(max(big, low))
  print_line(sign(low))
  print_line(abs(big))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "2147483647\n0\n1\n2147483647\n-2147483648\n2147483647\n-1\n2147483647\n";
  program.onlyConfigs = {
      "vm-step-O0", "vm-O0", "vm-O2", "native-O0", "native-O2", "optexe-O2", "exe"};
  program_matrix::runProgramMatrix(program);
}
