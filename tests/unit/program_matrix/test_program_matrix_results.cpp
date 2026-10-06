#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.results");

// With /std/result imported, every definition declared to return Result<...> returns a
// pointer to its sum storage (docs/spec/errors-and-file-io.md), whatever the payload types;
// these programs cross that boundary through returns, parameters, try and the readers.

TEST_CASE("stdlib Result named locals, forwarded calls and parameters return sum storage") {
  program_matrix::ProgramCase program;
  program.name = "result_named_forwarded_params";
  program.source = R"(
import /std/result/*

[return<Result<i32, i32>>]
named_ok() {
  [Result<i32, i32>] r{Result.ok(4i32)}
  return(r)
}

[return<Result<i32, i32>>]
named_err() {
  [Result<i32, i32>] r{/std/result/error<i32, i32>(6i32)}
  return(r)
}

[return<Result<i32, i32>>]
forward([bool] fail) {
  if(fail) {
    return(named_err())
  }
  return(named_ok())
}

[return<Result<i32, i32>>]
pass([Result<i32, i32>] r) {
  return(r)
}

[return<i32>]
show([Result<i32, i32>] r) {
  return(pick(r) {
    ok(v) { v }
    error(e) { 100i32 + e }
  })
}

[return<int> effects(io_out)]
main() {
  print_line(show(named_ok()))
  print_line(show(named_err()))
  print_line(show(forward(false)))
  print_line(show(forward(true)))
  print_line(show(pass(named_ok())))
  print_line(show(pass(named_err())))
  [Result<i32, i32>] e{named_err()}
  print_line(Result.error(e))
  print_line(Result.error(named_ok()))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "4\n106\n4\n106\n4\n106\n1\n0\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("stdlib Result error code zero stays an error") {
  program_matrix::ProgramCase program;
  program.name = "result_error_code_zero";
  program.source = R"(
import /std/result/*

[return<Result<i32, i32>>]
inner([bool] fail) {
  if(fail) {
    return(/std/result/error<i32, i32>(0i32))
  }
  return(Result.ok(9i32))
}

[effects(io_out)]
on_err([i32] err) {
  print_line(1000i32 + err)
}

[return<i32> effects(io_out) on_error<i32, /on_err>]
use([bool] fail) {
  [i32] v{try(inner(fail))}
  return(v)
}

[return<int> effects(io_out)]
main() {
  [Result<i32, i32>] r{inner(true)}
  print_line(Result.error(r))
  print_line(Result.error(inner(false)))
  print_line(use(false))
  print_line(use(true))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1\n0\n9\n1000\n0\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("stdlib Result keeps i64 and f64 payloads and errors") {
  program_matrix::ProgramCase program;
  program.name = "result_wide_payloads";
  program.source = R"(
import /std/result/*

[return<Result<i64, i64>>]
big([bool] fail) {
  if(fail) {
    return(/std/result/error<i64, i64>(7000000000i64))
  }
  return(Result.ok(5000000000i64))
}

[return<Result<f64, i32>>]
half([bool] fail) {
  if(fail) {
    return(/std/result/error<f64, i32>(4i32))
  }
  return(Result.ok(2.5f64))
}

[effects(io_out)]
on_err([i32] err) {
  print_line(1000i32 + err)
}

[return<i32> effects(io_out) on_error<i32, /on_err>]
scaled([bool] fail) {
  [f64] v{try(half(fail))}
  return(convert<i32>(v * 10.0f64))
}

[return<int> effects(io_out)]
main() {
  print_line(Result.error(big(false)))
  print_line(Result.error(big(true)))
  pick(big(false)) {
    ok(v) { print_line(v) }
    error(e) { print_line(e) }
  }
  pick(big(true)) {
    ok(v) { print_line(v) }
    error(e) { print_line(e) }
  }
  print_line(Result.error(half(false)))
  print_line(scaled(false))
  print_line(scaled(true))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "0\n1\n5000000000\n7000000000\n0\n25\n1004\n4\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("stdlib Result keeps string payloads and errors") {
  program_matrix::ProgramCase program;
  program.name = "result_string_payloads";
  program.source = R"(
import /std/result/*

[return<Result<string, string>>]
name([i32] k) {
  if(k == 0i32) {
    return(/std/result/error<string, string>("first"utf8))
  }
  if(k == 1i32) {
    return(/std/result/error<string, string>("second"utf8))
  }
  return(Result.ok("fine"utf8))
}

[return<int> effects(io_out)]
main() {
  print_line(Result.error(name(0i32)))
  print_line(Result.error(name(1i32)))
  print_line(Result.error(name(2i32)))
  pick(name(0i32)) {
    ok(v) { print_line(v) }
    error(e) { print_line(e) }
  }
  pick(name(2i32)) {
    ok(v) { print_line(v) }
    error(e) { print_line(e) }
  }
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1\n1\n0\nfirst\nfine\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("status-only stdlib Result propagates its error through try") {
  program_matrix::ProgramCase program;
  program.name = "result_status_only_propagation";
  program.source = R"(
import /std/result/*

[return<Result<i32>>]
check([i32] x) {
  if(x > 3i32) {
    [Result<i32>] bad{[error] x}
    return(bad)
  }
  return(/std/result/ok<i32>())
}

[effects(io_out)]
on_err([i32] err) {
  print_line(1000i32 + err)
}

[return<Result<i32>> effects(io_out) on_error<i32, /on_err>]
both([i32] x) {
  try(check(x))
  try(check(x + 1i32))
  return(/std/result/ok<i32>())
}

[return<int> effects(io_out)]
main() {
  print_line(Result.error(both(1i32)))
  print_line(Result.error(both(3i32)))
  pick(both(3i32)) {
    ok { print_line(0i32) }
    error(e) { print_line(e) }
  }
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "0\n1004\n1\n1004\n4\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("stdlib Result helper calls pass as arguments") {
  program_matrix::ProgramCase program;
  program.name = "result_helper_call_arguments";
  program.source = R"(
import /std/result/*

[return<i32>]
show([Result<i32, i32>] r) {
  return(pick(r) {
    ok(v) { v }
    error(e) { 100i32 + e }
  })
}

[return<bool>]
failed([Result<i32, i32>] r) {
  return(Result.error(r))
}

[return<int> effects(io_out)]
main() {
  print_line(show(/std/result/error<i32, i32>(5i32)))
  print_line(show(/std/result/ok<i32, i32>(6i32)))
  [Result<i32, i32>] good{Result.ok(8i32)}
  [Result<i32, i32>] bad{/std/result/error<i32, i32>(9i32)}
  print_line(failed(good))
  print_line(failed(bad))
  print_line(show(good))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "105\n6\n0\n1\n8\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("map tryAt with the Result import reads ok and missing keys") {
  program_matrix::ProgramCase program;
  program.name = "result_map_try_at";
  program.source = R"(
import /std/result/*
import /std/collections/*

[return<int> effects(io_out, heap_alloc)]
main() {
  [map<i32, i32>] m{map<i32, i32>(1i32, 10i32, 2i32, 20i32)}
  [Result<i32, ContainerError>] r{m.tryAt(2i32)}
  print_line(Result.error(r))
  [Result<i32, ContainerError>] q{m.tryAt(5i32)}
  print_line(Result.error(q))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "0\n1\n";
  program_matrix::runProgramMatrix(program);
}

TEST_SUITE_END();
