#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.optimizer");

// Programs whose lowered code has the shapes the optimizer and the optimizing
// backends work on: counted and conditional loops, short-circuit conditions,
// calls, recursion, strings, floats, heap collections, and output between
// loop iterations. Each runs on every execution config; the exit code and
// output are pinned to what the checked VM produces, so a change in any config
// (or in the oracle) shows up.

TEST_CASE("short-circuit conditions and nested loops") {
  program_matrix::ProgramCase program;
  program.name = "branches_loops";
  program.source = R"([return<int> effects(io_out)]
main() {
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  while(less_than(i, 20i32)) {
    if(and(greater_than(i, 3i32), less_than(i, 15i32))) {
      assign(total, plus(total, i))
    } else {
      if(or(equal(i, 0i32), equal(i, 19i32))) {
        assign(total, plus(total, 100i32))
      }
    }
    assign(i, plus(i, 1i32))
  }
  print_line(total)
  [i32 mut] j{0i32}
  [i32 mut] acc{1i32}
  repeat(5i32) {
    repeat(3i32) {
      assign(acc, plus(multiply(acc, 3i32), j))
      assign(j, plus(j, 1i32))
    }
    assign(acc, minus(acc, 7i32))
  }
  print_line(acc)
  return(minus(total, multiply(divide(total, 256i32), 256i32)))
}
)";
  program.exitCode = 43;
  program.stdoutText = "299\n14072959\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("string scan loop") {
  program_matrix::ProgramCase program;
  program.name = "string_scan";
  program.source = R"([return<int> effects(io_out)]
main() {
  [string] text{"banana bandana"utf8}
  [i32 mut] a{0i32}
  [i32 mut] n{0i32}
  [i32] len{text.count()}
  [i32 mut] i{0i32}
  while(less_than(i, len)) {
    [i32] c{text[i]}
    if(equal(c, 97i32)) {
      assign(a, plus(a, 1i32))
    }
    if(greater_than(c, 109i32)) {
      assign(n, plus(n, c))
    }
    assign(i, plus(i, 1i32))
  }
  print_line(a)
  print_line(n)
  return(a)
}
)";
  program.exitCode = 6;
  program.stdoutText = "6\n440\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("helper calls in a loop") {
  program_matrix::ProgramCase program;
  program.name = "helper_calls";
  program.source = R"([return<i64>]
square([i64] x) {
  return(multiply(x, x))
}

[return<i64>]
sum_to([i64] n) {
  [i64 mut] total{0i64}
  [i64 mut] k{1i64}
  while(less_equal(k, n)) {
    assign(total, plus(total, square(k)))
    assign(k, plus(k, 1i64))
  }
  return(total)
}

[return<int> effects(io_out)]
main() {
  print_line(sum_to(10i64))
  print_line(sum_to(1000i64))
  print_line(minus(sum_to(50i64), sum_to(49i64)))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "385\n333833500\n2500\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("early return from a loop") {
  program_matrix::ProgramCase program;
  program.name = "early_return";
  program.source = R"([return<int>]
find_first([i32] limit) {
  [i32 mut] i{1i32}
  while(less_than(i, 1000i32)) {
    if(greater_than(multiply(i, i), limit)) {
      return(i)
    }
    assign(i, plus(i, 1i32))
  }
  return(0i32)
}

[return<int> effects(io_out)]
main() {
  print_line(find_first(50i32))
  print_line(find_first(10000i32))
  return(find_first(2i32))
}
)";
  program.exitCode = 2;
  program.stdoutText = "8\n101\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("floating point accumulation") {
  program_matrix::ProgramCase program;
  program.name = "float_loop";
  program.source = R"([return<int> effects(io_out)]
main() {
  [f64 mut] x{0.5f64}
  [f64 mut] sum{0.0f64}
  [i32 mut] i{0i32}
  while(less_than(i, 50i32)) {
    assign(sum, plus(sum, multiply(x, x)))
    assign(x, plus(x, 0.25f64))
    assign(i, plus(i, 1i32))
  }
  print_line(convert<i64>(sum))
  print_line(convert<i32>(multiply(sum, 100.0f64)))
  if(greater_than(sum, 1000.0f64)) {
    return(1i32)
  }
  return(0i32)
}
)";
  program.exitCode = 1;
  program.stdoutText = "2845\n284531\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("printing inside a loop keeps locals") {
  program_matrix::ProgramCase program;
  program.name = "print_in_loop";
  program.source = R"([return<int> effects(io_out)]
main() {
  [i32 mut] a{1i32}
  [i32 mut] b{1i32}
  [i32 mut] i{0i32}
  while(less_than(i, 12i32)) {
    print_line(a)
    [i32] next{plus(a, b)}
    assign(a, b)
    assign(b, next)
    assign(i, plus(i, 1i32))
  }
  print(a)
  print(","utf8)
  print_line(b)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1\n1\n2\n3\n5\n8\n13\n21\n34\n55\n89\n144\n233,377\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("recursive function") {
  program_matrix::ProgramCase program;
  program.name = "recursion";
  program.source = R"([return<int>]
fib([i32] n) {
  if(less_than(n, 2i32)) {
    return(n)
  }
  return(plus(fib(minus(n, 1i32)), fib(minus(n, 2i32))))
}

[return<int> effects(io_out)]
main() {
  print_line(fib(15i32))
  return(fib(10i32))
}
)";
  program.exitCode = 55;
  program.stdoutText = "610\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("vector push and index loop") {
  program_matrix::ProgramCase program;
  program.name = "vector_loop";
  program.source = R"(import /std/collections/*

[return<int> effects(io_out, heap_alloc)]
main() {
  [vector<i32> mut] values{vector<i32>()}
  [i32 mut] i{0i32}
  while(less_than(i, 200i32)) {
    values.push(multiply(i, 3i32))
    assign(i, plus(i, 1i32))
  }
  [i64 mut] total{0i64}
  [i32 mut] j{0i32}
  while(less_than(j, values.count())) {
    assign(total, plus(total, convert<i64>(values[j])))
    assign(j, plus(j, 1i32))
  }
  print_line(total)
  print_line(values.count())
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "59700\n200\n";
  program_matrix::runProgramMatrix(program);
}
