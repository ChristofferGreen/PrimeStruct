#include "third_party/doctest.h"

#include "primec/frontend/SemanticProduct.h"
#include "../test_semantics_helpers.h"

#include <algorithm>

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.effects");


TEST_CASE("insert requires pathspace_insert effect") {
  const std::string source = R"(
main() {
  insert("/events/test"utf8, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pathspace_insert") != std::string::npos);
}

TEST_CASE("insert rejects template arguments") {
  const std::string source = R"(
[effects(pathspace_insert)]
main() {
  insert<i32>("/events/test"utf8, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("insert does not accept template arguments") != std::string::npos);
}

TEST_CASE("insert rejects non-string path argument") {
  const std::string source = R"(
[effects(pathspace_insert)]
main() {
  insert(1i32, 2i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires string path argument") != std::string::npos);
}

TEST_CASE("insert accepts string array access") {
  const std::string source = R"(
[effects(pathspace_insert)]
main() {
  [array<string>] values{array<string>("/events/test"utf8)}
  insert(values[0i32], 1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("insert rejects block arguments") {
  const std::string source = R"(
[effects(pathspace_insert)]
main() {
  insert("/events/test"utf8, 1i32) { 2i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("insert does not accept block arguments") != std::string::npos);
}

TEST_CASE("insert rejects argument count mismatch") {
  const std::string source = R"(
[effects(pathspace_insert)]
main() {
  insert("/events/test"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("insert requires exactly 2 arguments") != std::string::npos);
}

TEST_CASE("insert not allowed in expression context") {
  const std::string source = R"(
[return<int> effects(pathspace_insert)]
main() {
  return(insert("/events/test"utf8, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("statement-only") != std::string::npos);
}

TEST_CASE("take requires pathspace_take effect") {
  const std::string source = R"(
main() {
  take("/events/test"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pathspace_take") != std::string::npos);
}

TEST_CASE("take rejects template arguments") {
  const std::string source = R"(
[effects(pathspace_take)]
main() {
  take<i32>("/events/test"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("take does not accept template arguments") != std::string::npos);
}

TEST_CASE("take rejects non-string path argument") {
  const std::string source = R"(
[effects(pathspace_take)]
main() {
  take(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires string path argument") != std::string::npos);
}

TEST_CASE("take rejects string map access without path inference") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc, pathspace_take)]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "/events/test"utf8)}
  take(/std/collections/map/at<i32, string>(values, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("take requires string path argument") != std::string::npos);
}

TEST_CASE("take rejects block arguments") {
  const std::string source = R"(
[effects(pathspace_take)]
main() {
  take("/events/test"utf8) { 1i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("take does not accept block arguments") != std::string::npos);
}

TEST_CASE("take rejects argument count mismatch") {
  const std::string source = R"(
[effects(pathspace_take)]
main() {
  take()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("take requires exactly 1 argument") != std::string::npos);
}

TEST_CASE("take not allowed in expression context") {
  const std::string source = R"(
[return<int> effects(pathspace_take)]
main() {
  return(take("/events/test"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("statement-only") != std::string::npos);
}

TEST_CASE("bind requires pathspace_bind effect") {
  const std::string source = R"(
main() {
  bind("/events/test"utf8, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pathspace_bind") != std::string::npos);
}

TEST_CASE("unbind requires pathspace_bind effect") {
  const std::string source = R"(
main() {
  unbind("/events/test"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pathspace_bind") != std::string::npos);
}

TEST_CASE("schedule requires pathspace_schedule effect") {
  const std::string source = R"(
main() {
  schedule("/events/test"utf8, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pathspace_schedule") != std::string::npos);
}

TEST_CASE("schedule rejects non-string path argument") {
  const std::string source = R"(
[effects(pathspace_schedule)]
main() {
  schedule(1i32, 2i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires string path argument") != std::string::npos);
}

TEST_CASE("notify not allowed in expression context") {
  const std::string source = R"(
[return<int> effects(pathspace_notify)]
main() {
  return(notify("/events/test"utf8, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("statement-only") != std::string::npos);
}

TEST_CASE("print not allowed in expression context") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  return(print_line("hello"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("only supported as a statement") != std::string::npos);
}

TEST_CASE("print rejects pointer argument") {
  const std::string source = R"(
[effects(io_out) return<int>]
main() {
  [i32] value{1i32}
  print(location(value))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires an integer/bool or string literal/binding argument") != std::string::npos);
}

TEST_CASE("print rejects collection argument") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print(array<i32>(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires an integer/bool or string literal/binding argument") != std::string::npos);
}

TEST_CASE("print rejects missing arguments") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print requires exactly one argument") != std::string::npos);
}

TEST_CASE("print_line rejects block arguments") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print_line(1i32) { 2i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_line does not accept block arguments") != std::string::npos);
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

TEST_CASE("map constructor block arguments require definition target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [auto] values{map<i32, i32>(1i32, 2i32) { 3i32 }}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /map") !=
        std::string::npos);
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
  CHECK(error.find("collection literal does not accept block arguments") != std::string::npos);
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

TEST_CASE("print accepts string vector literal access") {
  const std::string source = R"(
import /std/collections/*

[effects(io_out), effects(heap_alloc), return<int>]
main() {
  [vector<string>] values{vector<string>("hi"utf8)}
  print_line(/std/collections/vector/at(values, 0i32))
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

[return<i32> effects(none)]
vector<T>([T] value) {
  return(0i32)
}

[return<Thing> effects(none)]
at([i32] value, [i32] index) {
  return(Thing{})
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
    [Thing] item{Thing{}}
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
  CHECK(error.find("collection literal requires heap_alloc effect") != std::string::npos);
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
  CHECK(error.find("missing on_error for ? usage") != std::string::npos);
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
  CHECK(error.find("File requires file_read effect") != std::string::npos);
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
[return<i32> effects(none)]
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
  CHECK(error.find("missing on_error for ? usage") != std::string::npos);
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
  return(Result.ok(Frame{[token] 9i32}))
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
  [Foo] foo{Foo{[value] 7i32}}
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
  [Foo] foo{Foo{[value] 7i32}}
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

TEST_CASE("nested definition named block with on_error reports block return requirement") {
  const std::string source = R"(
[return<Result<FileError>> on_error<FileError, /log_file_error>]
main() {
  [return<void> on_error<FileError, /log_file_error>] block(){
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

TEST_CASE("try in nested blocks of a definition without on_error is reported at the try") {
  const std::string source = R"(
[return<Result<FileError>> effects(io_err)]
main() {
  [Result<FileError>] r{Result.ok()}
  if(true) {
    while(true) {
      r?
    }
  }
  return(Result.ok())
}
)";
  std::string error;
  primec::SemanticDiagnosticInfo diagnostics;
  CHECK_FALSE(validateProgramCollectingDiagnostics(source, "/main", error, diagnostics));
  REQUIRE_FALSE(diagnostics.records.empty());
  const auto &record = diagnostics.records.front();
  CHECK(record.message.find("missing on_error for ? usage") != std::string::npos);
  CHECK(record.primarySpan.line == 7);
  std::string related;
  for (const auto &span : record.relatedSpans) {
    related += span.label + "\n";
  }
  CHECK(related.find("/main") != std::string::npos);
}

TEST_CASE("an on_error block override ends with its block") {
  const std::string source = R"(
[return<Result<FileError>> effects(io_err)]
main() {
  [Result<FileError>] r{Result.ok()}
  if(true) {
    [on_error<FileError, /log_file_error>] block() {
      r?
    }
    r?
  }
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  std::string error;
  primec::SemanticDiagnosticInfo diagnostics;
  CHECK_FALSE(validateProgramCollectingDiagnostics(source, "/main", error, diagnostics));
  REQUIRE_FALSE(diagnostics.records.empty());
  CHECK(diagnostics.records.front().message.find("missing on_error for ? usage") !=
        std::string::npos);
  CHECK(diagnostics.records.front().primarySpan.line == 9);
}

TEST_CASE("the on_error of a definition covers nested blocks and a block overrides it") {
  const std::string source = R"(
[return<Result<FileError>> effects(io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<FileError>] r{Result.ok()}
  if(true) {
    r?
    while(true) {
      r?
    }
  } else {
    [on_error<FileError, /log_file_error>] block() {
      r?
      while(true) {
        r?
      }
    }
    r?
  }
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

TEST_CASE("print takes the count of a Vector field and still rejects its element") {
  // `bag.items.count()` yields an i32 like the same call bound to a local; a struct element is not printable.
  const std::string program = R"(import /std/collections/*

[struct]
Item() {
  [i32 mut] n{0i32}
}

[struct]
Bag() {
  [Vector<Item> mut] items{Vector<Item>{}}
  [Vector<i32> mut] numbers{Vector<i32>{}}
}

[struct]
Outer() {
  [Bag mut] bag{Bag{}}
}

[effects(heap_alloc, io_out) return<int>]
main() {
  [Bag mut] bag{Bag{}}
  [Outer mut] outer{Outer{}}
)";
  const std::string accepted = program + R"(  print_line(bag.items.count())
  print_line(outer.bag.items.count())
  print_line(bag.numbers.count())
  print_line(bag.items.capacity())
  print(outer.bag.numbers.capacity())
  return(0i32)
}
)";
  std::string acceptedError;
  CHECK_MESSAGE(validateProgram(accepted, "/main", acceptedError), acceptedError);

  const std::string rejected = program + R"(  bag.items.push(Item{})
  print_line(bag.items[0i32])
  return(0i32)
}
)";
  primec::SemanticDiagnosticInfo diagnosticInfo;
  std::string error;
  CHECK_FALSE(validateProgramReportingDiagnostic(rejected, "/main", error, diagnosticInfo));
  CHECK_MESSAGE(
      error.find("print_line requires an integer/bool or string literal/binding argument") !=
          std::string::npos,
      error);
  REQUIRE(diagnosticInfo.hasPrimarySpan);
  CHECK(diagnosticInfo.primarySpan.line == 24);
  CHECK(diagnosticInfo.primarySpan.column == 3);
}

TEST_SUITE_END();
