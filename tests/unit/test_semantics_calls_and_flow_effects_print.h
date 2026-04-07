#pragma once

TEST_CASE("print_line rejects missing arguments") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print_line()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_line requires exactly one argument") != std::string::npos);
}

TEST_CASE("print_error rejects missing arguments") {
  const std::string source = R"(
[effects(io_err)]
main() {
  print_error()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_error requires exactly one argument") != std::string::npos);
}

TEST_CASE("print_error rejects block arguments") {
  const std::string source = R"(
[effects(io_err)]
main() {
  print_error("oops"utf8) { 1i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_error does not accept block arguments") != std::string::npos);
}

TEST_CASE("print_line_error rejects missing arguments") {
  const std::string source = R"(
[effects(io_err)]
main() {
  print_line_error()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_line_error requires exactly one argument") != std::string::npos);
}

TEST_CASE("print_line_error rejects block arguments") {
  const std::string source = R"(
[effects(io_err)]
main() {
  print_line_error("oops"utf8) { 1i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_line_error does not accept block arguments") != std::string::npos);
}

TEST_CASE("array literal rejects block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  array<i32>(1i32) { 2i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("array literal does not accept block arguments") != std::string::npos);
}

TEST_CASE("map literal rejects block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>(1i32, 2i32) { 3i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map literal does not accept block arguments") != std::string::npos);
}

TEST_CASE("vector literal rejects block arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  vector<i32>(1i32, 2i32) { 3i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("vector literal does not accept block arguments") != std::string::npos);
}

TEST_CASE("print accepts string array access") {
  const std::string source = R"(
[effects(io_out)]
main() {
  [array<string>] values{array<string>("hi"utf8)}
  print_line(values[0i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print accepts string map access") {
  const std::string source = R"(
import /std/collections/*

[effects(io_out)]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "hi"utf8)}
  print_line(values[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print accepts string vector literal access") {
  const std::string source = R"(
import /std/collections/*

[effects(io_out), effects(heap_alloc), return<int>]
main() {
  print_line(at(vector<string>("hi"utf8), 0i32))
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("definition rejects duplicate identical effects transforms") {
  const std::string source = R"(
[effects(io_out), effects(io_out), return<int>]
main() {
  print_line("hi"utf8)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate effects transform on /main") != std::string::npos);
}

TEST_CASE("print rejects user-defined at on user-defined vector target") {
  const std::string source = R"(
Thing() {
  [i32] value{1i32}
}

[return<i32>]
vector<T>([T] value) {
  return(0i32)
}

[return<Thing>]
at([i32] value, [i32] index) {
  return(Thing())
}

[effects(io_out) return<int>]
main() {
  print_line(at(vector<string>("hi"utf8), 0i32))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires an integer/bool or string literal/binding argument") != std::string::npos);
}

TEST_CASE("print rejects struct block value") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{1i32}
}

[effects(io_out)]
main() {
  print_line(block(){
    [Thing] item{Thing()}
    item
  })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires an integer/bool or string literal/binding argument") != std::string::npos);
}

TEST_CASE("print accepts string binding") {
  const std::string source = R"(
[effects(io_out)]
main() {
  [string] greeting{"hi"utf8}
  print_line(greeting)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print accepts bool binding") {
  const std::string source = R"(
[effects(io_out)]
main() {
  [bool] ready{true}
  print_line(ready)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print accepts bool literal") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print_line(true)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print rejects float literal") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print_line(1.0f32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires an integer/bool or string literal/binding argument") != std::string::npos);
}

TEST_CASE("print_error accepts bool binding") {
  const std::string source = R"(
[effects(io_err)]
main() {
  [bool] ready{true}
  print_error(ready)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print_line_error accepts bool binding") {
  const std::string source = R"(
[effects(io_err)]
main() {
  [bool] ready{true}
  print_line_error(ready)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("default effects allow print") {
  const std::string source = R"(
[return<void>]
main() {
  print_line("hello"utf8)
}
)";
  std::string error;
  CHECK(validateProgramWithDefaults(source, "/main", {"io_out"}, error));
  CHECK(error.empty());
}

TEST_CASE("default effects allow print_error") {
  const std::string source = R"(
[return<void>]
main() {
  print_line_error("oops"utf8)
}
)";
  std::string error;
  CHECK(validateProgramWithDefaults(source, "/main", {"io_err"}, error));
  CHECK(error.empty());
}

TEST_CASE("default effects allow vector literal") {
  const std::string source = R"(
[return<int>]
main() {
  vector<i32>(1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgramWithDefaults(source, "/main", {"heap_alloc"}, error));
  CHECK(error.empty());
}

TEST_CASE("default effects reject invalid names") {
  const std::string source = R"(
[return<void>]
main() {
}
)";
  std::string error;
  CHECK_FALSE(validateProgramWithDefaults(source, "/main", {"bad-effect"}, error));
  CHECK(error.find("invalid default effect: bad-effect") != std::string::npos);
}

TEST_CASE("definition validation context isolates moved bindings") {
  const std::string source = R"(
[return<void>]
consume() {
  [i32] value{1i32}
  move(value)
}

[return<i32>]
main() {
  [i32] value{2i32}
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("definition validation context isolates active effects") {
  const std::string source = R"(
[effects(heap_alloc), return<i32>]
warmup() {
  vector<i32>(1i32)
  return(0i32)
}

[return<i32>]
main() {
  vector<i32>(1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("vector literal requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("definition validation context isolates on_error handlers") {
  const std::string source = R"(
[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
warmup() {
  [File<Write>] file{ File<Write>("warmup.txt"utf8)? }
  return(Result.ok())
}

[return<Result<FileError>> effects(file_write)]
main() {
  [File<Write>] file{ File<Write>("main.txt"utf8)? }
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_CASE("File constructor requires file_read effect for read mode") {
  const std::string source = R"(
[return<Result<FileError>> on_error<FileError, /log_file_error>]
main() {
  [File<Read>] file{ File<Read>("file.txt"utf8)? }
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_CASE("file read methods require file_read effect") {
  const std::string source = R"(
[return<void>]
read_in([File<Read>] file, [i32 mut] value) {
  file.read_byte(value)
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("file operations require file_read effect") != std::string::npos);
}

TEST_CASE("file write methods still require file_write effect") {
  const std::string source = R"(
[effects(file_read), return<void>]
write_out([File<Write>] file) {
  file.write_line("hello"utf8)
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("file operations require file_write effect") != std::string::npos);
}

TEST_CASE("file_write effect allows file operations") {
  const std::string source = R"(
[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Write>] file{ File<Write>("file.txt"utf8)? }
  file.write_line("hello"utf8)?
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

TEST_CASE("file_write effect still allows read operations") {
  const std::string source = R"(
[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Read>] file{ File<Read>("file.txt"utf8)? }
  [i32 mut] value{0i32}
  file.read_byte(value)?
  file.close()?
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
