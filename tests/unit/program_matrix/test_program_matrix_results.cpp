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

TEST_CASE("Result.ok calls pass as stdlib Result arguments") {
  program_matrix::ProgramCase program;
  program.name = "result_ok_call_arguments";
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

[return<i32>]
twice([Result<i32, i32>] a, [Result<i32, i32>] b) {
  return(show(a) + show(b))
}

[return<int> effects(io_out)]
main() {
  print_line(show(Result.ok(3i32)))
  print_line(failed(Result.ok(4i32)))
  print_line(twice(Result.ok(5i32), /std/result/error<i32, i32>(6i32)))
  print_line(twice(Result.ok(7i32), Result.ok(8i32)))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "3\n0\n111\n15\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("vectors of stdlib Result values keep their elements") {
  // Each element is the sum's storage (tag plus payload slots), stored inline like a struct element.
  program_matrix::ProgramCase program;
  program.name = "result_vector_elements";
  program.source = R"(
import /std/result/*
import /std/collections/*

[return<i32>]
show([Result<i32, i32>] r) {
  return(pick(r) {
    ok(v) { v }
    error(e) { 100i32 + e }
  })
}

[return<i32>]
total([vector<Result<i32, i32>>] rs) {
  [i32 mut] sum{0i32}
  [i32 mut] index{0i32}
  while(index < rs.count()) {
    sum = sum + show(rs[index])
    index = index + 1i32
  }
  return(sum)
}

[return<int> effects(io_out, heap_alloc)]
main() {
  [vector<Result<i32, i32>> mut] rs{vector<Result<i32, i32>>()}
  rs.push(/std/result/ok<i32, i32>(5i32))
  rs.push(/std/result/error<i32, i32>(7i32))
  rs.push(Result.ok(9i32))
  [Result<i32, i32>] named{/std/result/error<i32, i32>(2i32)}
  rs.push(named)
  rs.push(/std/result/ok<i32, i32>(11i32))
  print_line(rs.count())
  print_line(show(rs[0i32]))
  print_line(show(rs[1i32]))
  print_line(show(rs[2i32]))
  print_line(show(rs[3i32]))
  print_line(show(rs[4i32]))
  [Result<i32, i32>] first{rs[0i32]}
  [Result<i32, i32>] second{rs[1i32]}
  print_line(Result.error(first))
  print_line(Result.error(second))
  print_line(show(first))
  print_line(show(second))
  print_line(total(rs))
  rs.pop()
  print_line(rs.count())
  print_line(show(rs[3i32]))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "5\n5\n107\n9\n102\n11\n0\n1\n5\n107\n234\n4\n102\n";
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

// An int-backed error struct (one int field) is stored inline in the sum payload, so
// construction, pick, Result.why and try read the same storage.

TEST_CASE("int-backed error struct payload reads through pick and Result.why") {
  program_matrix::ProgramCase program;
  program.name = "result_error_struct_pick_why";
  program.source = R"(
import /std/result/*
import /std/collections/*

[return<i32>]
show([Result<i32, ContainerError>] r) {
  return(pick(r) {
    ok(v) { v }
    error(e) { 100i32 + e.code }
  })
}

[return<int> effects(io_out)]
main() {
  [Result<i32, ContainerError>] r{/std/result/error<i32, ContainerError>(ContainerError{2i32})}
  print_line(show(r))
  print_line(Result.why(r))
  print_line(Result.error(r))
  [Result<i32, ContainerError>] ok{/std/result/ok<i32, ContainerError>(5i32)}
  print_line(show(ok))
  print_line(Result.error(ok))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "102\ncontainer index out of bounds\n1\n5\n0\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("int-backed error struct payload reaches an on_error handler through try") {
  program_matrix::ProgramCase program;
  program.name = "result_error_struct_try_on_error";
  program.source = R"(
import /std/result/*
import /std/collections/*

[effects(io_out)]
on_err([ContainerError] err) {
  print_line(err.code)
}

[return<Result<i32, ContainerError>>]
fail() {
  return(/std/result/error<i32, ContainerError>(ContainerError{2i32}))
}

[return<Result<i32, ContainerError>>]
succeed() {
  return(/std/result/ok<i32, ContainerError>(7i32))
}

[return<Result<i32, ContainerError>> effects(io_out) on_error<ContainerError, /on_err>]
run_fail() {
  [i32] v{try(fail())}
  print_line(v)
  return(/std/result/ok<i32, ContainerError>(v))
}

[return<Result<i32, ContainerError>> effects(io_out) on_error<ContainerError, /on_err>]
run_ok() {
  [i32] v{try(succeed())}
  print_line(v)
  return(/std/result/ok<i32, ContainerError>(v))
}

[return<int> effects(io_out)]
main() {
  [Result<i32, ContainerError>] bad{run_fail()}
  print_line(Result.error(bad))
  print_line(Result.why(bad))
  [Result<i32, ContainerError>] good{run_ok()}
  print_line(Result.error(good))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "2\n1\ncontainer index out of bounds\n7\n0\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("map tryAt of a missing key reads its error struct through Result.why") {
  program_matrix::ProgramCase program;
  program.name = "result_map_try_at_why";
  program.source = R"(
import /std/result/*
import /std/collections/*

[return<int> effects(io_out, heap_alloc)]
main() {
  [map<i32, i32>] m{map<i32, i32>(1i32, 10i32)}
  [Result<i32, ContainerError>] hit{m.tryAt(1i32)}
  print_line(Result.error(hit))
  print_line(Result.why(hit))
  [Result<i32, ContainerError>] miss{m.tryAt(5i32)}
  print_line(Result.error(miss))
  print_line(Result.why(miss))
  print_line(pick(miss) {
    ok(v) { v }
    error(e) { 100i32 + e.code }
  })
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "0\n\n1\ncontainer missing key\n101\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("try over a call whose callee and caller hold a struct with a vector field") {
  // TODO-5697: the try path sized the struct from its fields and looked up the
  // vector<i32> field's /vector path as a struct.
  program_matrix::ProgramCase program;
  program.name = "result_try_vector_field_struct";
  program.source = R"(
import /std/collections/*

[struct]
Err() {
  [i32] code{0i32}
}

[effects(io_err)]
log_err([Err] err) {
  print_line_error("err")
}

[struct]
Box() {
  [vector<i32> mut] items{vector<i32>()}
}

[Result<Err> effects(heap_alloc)]
check_box([i32] value) {
  [Box mut] box{Box{}}
  box.items.push(value)
  box.items.push(value + 1i32)
  if(box.items.count() > value) {
    return(7i32)
  }
  return(Result.ok())
}

[return<int> effects(heap_alloc, io_err, io_out) on_error<Err, log_err>]
main() {
  [Box mut] outer{Box{}}
  outer.items.push(3i32)
  check_box(5i32)?
  print_line("first ok")
  check_box(1i32)?
  print_line("unreachable")
  return(0i32)
}
)";
  program.exitCode = 7;
  program.stdoutText = "first ok\n";
  program.vmStderr = "err\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("file open errors report errno, writeLine ends lines and a missing png is invalid") {
  program_matrix::ProgramCase program;
  program.name = "file_io_errno_newline_png";
  program.source = R"(
import /std/file/*
import /std/image/*
import /std/collections/*

[effects(io_out)]
on_file_err([FileError] err) {
  print_line(err.status())
}

[return<int> effects(io_out, file_read) on_error<FileError, /on_file_err>]
open_missing() {
  [File<Read>] file{File<Read>("/nonexistent_primestruct_dir/missing.txt")?}
  return(0i32)
}

[return<Result<FileError>> effects(file_write, io_out) on_error<FileError, /on_file_err>]
write_lines() {
  [File<Write>] file{File<Write>("program_matrix_file_io_probe.txt")?}
  file.write("ab")?
  file.writeLine("cd")?
  file.writeLine("e")?
  return(Result.ok())
}

[return<i32> effects(io_out, file_read) on_error<FileError, /on_file_err>]
read_one([File<Read>] file) {
  [i32 mut] byte{0i32}
  file.readByte(byte)?
  return(byte)
}

[return<int> effects(io_out, file_read) on_error<FileError, /on_file_err>]
read_lines() {
  [File<Read>] file{File<Read>("program_matrix_file_io_probe.txt")?}
  [i32 mut] index{0i32}
  while(index < 7i32) {
    print_line(read_one(file))
    index = index + 1i32
  }
  return(0i32)
}

[return<int> effects(io_out, file_read, file_write, heap_alloc)]
main() {
  print_line(open_missing())
  write_lines()
  print_line(read_lines())
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "/nonexistent_primestruct_dir/missing.png"utf8)}
  print_line(Result.why(status))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "2\n2\n97\n98\n99\n100\n10\n101\n10\n0\nimage_invalid_operation\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("try as an argument of a user function reports through the caller's on_error") {
  program_matrix::ProgramCase program;
  program.name = "result_try_call_argument";
  program.source = R"(
import /std/result/*

[return<Result<i32, i32>>]
fetch_ok() {
  [Result<i32, i32>] r{Result<i32, i32>{[ok] 3i32}}
  return(r)
}

[return<Result<i32, i32>>]
fetch_err() {
  [Result<i32, i32>] r{Result<i32, i32>{[error] 9i32}}
  return(r)
}

[return<i32>]
add([i32] left, [i32] right) {
  return(plus(left, right))
}

[effects(io_out)]
report([i32] err) {
  print_line(err)
}

[return<int> effects(io_out) on_error<i32, /report>]
sum_ok() {
  [i32] total{add(10i32, try(fetch_ok()))}
  print_line(total)
  return(total)
}

[return<int> effects(io_out) on_error<i32, /report>]
sum_err() {
  [i32] total{add(try(fetch_err()), 1i32)}
  print_line(total)
  return(total)
}

[return<int> effects(io_out)]
main() {
  print_line(sum_ok())
  print_line(sum_err())
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "13\n13\n9\n9\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a return builds a stdlib Result from a brace constructor") {
  program_matrix::ProgramCase program;
  program.name = "result_return_brace_constructor";
  program.source = R"(
import /std/result/*

[return<Result<i32, i32>>]
failed() {
  return(Result<i32, i32>{[error] 7i32})
}

[return<Result<i32, i32>>]
succeeded() {
  return(Result<i32, i32>{[ok] 5i32})
}

[return<int> effects(io_out)]
main() {
  print_line(pick(failed()) { ok(value) { value } error(err) { err } })
  print_line(pick(succeeded()) { ok(value) { value } error(err) { err } })
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "7\n5\n";
  program_matrix::runProgramMatrix(program);
}

TEST_SUITE_END();
