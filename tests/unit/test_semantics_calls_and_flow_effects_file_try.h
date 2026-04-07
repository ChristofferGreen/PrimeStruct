#pragma once

TEST_CASE("file read_byte accepts mutable integer binding") {
  const std::string source = R"(
[return<Result<FileError>> effects(file_read) on_error<FileError, /log_file_error>]
main() {
  [File<Read>] file{ File<Read>("file.txt"utf8)? }
  [i32 mut] value{0i32}
  file.read_byte(value)?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("file read_byte rejects immutable binding") {
  const std::string source = R"(
[return<Result<FileError>> effects(file_read) on_error<FileError, /log_file_error>]
main() {
  [File<Read>] file{ File<Read>("file.txt"utf8)? }
  [i32] value{0i32}
  file.read_byte(value)?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("read_byte requires mutable integer binding") != std::string::npos);
}

TEST_CASE("file write_bytes accepts builtin array argument") {
  const std::string source = R"(
[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Write>] file{ File<Write>("file.txt"utf8)? }
  file.write_bytes(array<i32>(1i32, 2i32))?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("file write_bytes rejects user definition named array") {
  const std::string source = R"(
[return<i32>]
array<T>([T] value) {
  return(0i32)
}

[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Write>] file{ File<Write>("file.txt"utf8)? }
  file.write_bytes(array<i32>(1i32))?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("write_bytes requires array argument") != std::string::npos);
}

TEST_CASE("try requires on_error") {
  const std::string source = R"(
[return<Result<FileError>> effects(file_write)]
main() {
  [File<Write>] file{ File<Write>("file.txt"utf8)? }
  return(Result.ok())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_CASE("user-defined try helper keeps named arguments") {
  const std::string source = R"(
[return<int>]
try([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(try([value] 7i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin try rejects named arguments") {
  const std::string source = R"(
[return<Result<FileError>> on_error<FileError, /log_file_error>]
main() {
  [Result<FileError>] status{Result.ok()}
  try([value] status)
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("on_error allows int return type for graphics-style flow") {
  const std::string source = R"(
[struct]
GfxError() {
  [i32] code{0i32}
}

[struct]
Frame() {
  [i32] token{0i32}
}

[return<Result<Frame, GfxError>>]
acquire_frame() {
  return(Result.ok(Frame([token] 9i32)))
}

namespace Frame {
  [return<Result<GfxError>>]
  submit([Frame] self) {
    return(Result.ok())
  }
}

[return<int> on_error<GfxError, /log_gfx_error>]
main() {
  frame{acquire_frame()?}
  frame.submit()?
  return(frame.token)
}

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error("gfx error"utf8)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("type namespace helper call drops synthetic receiver") {
  const std::string source = R"(
[struct]
Foo() {
  [i32] value{0i32}
}

namespace Foo {
  [return<i32>]
  project([Foo] item) {
    return(item.value)
  }
}

[return<int>]
main() {
  [Foo] foo{Foo([value] 7i32)}
  return(Foo.project(foo))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("type namespace helper auto inference drops synthetic receiver") {
  const std::string source = R"(
[struct]
Foo() {
  [i32] value{0i32}
}

namespace Foo {
  [return<i32>]
  project([Foo] item) {
    return(item.value)
  }
}

[return<int>]
main() {
  [Foo] foo{Foo([value] 7i32)}
  [auto] inferred{Foo.project(foo)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("on_error rejects non-Result non-int return type") {
  const std::string source = R"(
[struct]
GfxError() {
  [i32] code{0i32}
}

[return<bool> on_error<GfxError, /log_gfx_error>]
main() {
  return(false)
}

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error("gfx error"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("on_error requires Result or int return type") != std::string::npos);
}

TEST_CASE("statement on_error rejects non-block execution target") {
  const std::string source = R"(
[return<Result<FileError>> on_error<FileError, /log_file_error>]
main() {
  [on_error<FileError, /log_file_error>] print_line("x"utf8)
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("on_error transform is not allowed on executions: /print_line") != std::string::npos);
}

TEST_CASE("statement block on_error reports block return requirement") {
  const std::string source = R"(
[return<Result<FileError>> on_error<FileError, /log_file_error>]
main() {
  [on_error<FileError, /log_file_error>] block(){
    print_line("x"utf8)
  }
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("on_error requires Result or int return type on /main/block") != std::string::npos);
}

TEST_CASE("try rejects mismatched error type") {
  const std::string source = R"(
[struct]
MyError() {
  [public i32] code{0i32}
}

[return<Result<FileError>> on_error<FileError, /log_file_error>]
main() {
  [Result<MyError>] bad{Result.ok()}
  bad?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("try error type mismatch") != std::string::npos);
}

TEST_SUITE_END();
