#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.result_helpers");

TEST_CASE("stdlib file error result helpers construct status and value results") {
  const std::string source = R"(
import /std/file/*

[return<void>]
main() {
  [FileError] err{fileReadEof()}
  [Result<FileError>] status{FileError.status(fileReadEof())}
  [Result<FileError>] directStatus{FileError.status(fileReadEof())}
  [Result<FileError>] methodStatus{err.status()}
  [Result<i32, FileError>] valueStatus{FileError.result<i32>(fileReadEof())}
  [Result<i32, FileError>] directValueStatus{FileError.result<i32>(fileReadEof())}
  [Result<i32, FileError>] methodValueStatus{err.result<i32>()}
  [Result<FileError>] wrappedStatus{FileError.status(err)}
  [Result<i32, FileError>] wrappedValue{FileError.result<i32>(err)}
  [bool] eof{fileErrorIsEof(fileReadEof())}
  [bool] otherEof{fileErrorIsEof(1i32)}
  [bool] statusError{Result.error(status)}
  [bool] directStatusError{Result.error(directStatus)}
  [bool] methodStatusError{Result.error(methodStatus)}
  [bool] wrappedStatusError{Result.error(wrappedStatus)}
  [bool] valueError{Result.error(valueStatus)}
  [bool] directValueError{Result.error(directValueStatus)}
  [bool] methodValueError{Result.error(methodValueStatus)}
  [bool] wrappedValueError{Result.error(wrappedValue)}
  [string] statusWhy{Result.why(status)}
  [string] directStatusWhy{Result.why(directStatus)}
  [string] methodStatusWhy{Result.why(methodStatus)}
  [string] wrappedStatusWhy{Result.why(wrappedStatus)}
  [string] valueWhy{Result.why(valueStatus)}
  [string] directValueWhy{Result.why(directValueStatus)}
  [string] methodValueWhy{Result.why(methodValueStatus)}
  [string] wrappedValueWhy{Result.why(wrappedValue)}
  if(and(and(and(and(and(statusError, directStatusError), methodStatusError), wrappedStatusError),
             and(and(and(valueError, directValueError), methodValueError), wrappedValueError)),
         and(eof, not(otherEof))),
     then(){ return() },
     else(){ return() })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib file error result helpers reject non file errors") {
  const std::string source = R"(
import /std/file/*

[return<void>]
main() {
  [bool] eof{fileErrorIsEof("bad"utf8)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_CASE("stdlib FileError why wrapper covers direct and Result-based access") {
  const std::string source = R"(
import /std/file/*

[return<void>]
main() {
  [FileError] err{fileReadEof()}
  [string] direct{FileError.why(err)}
  [string] viaType{FileError.why(err)}
  [string] method{err.why()}
  [string] viaResult{Result.why(FileError.status(err))}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin FileError why method works without imported wrapper") {
  const std::string source = R"(
[return<void>]
main() {
  [FileError] err{13i32}
  [string] why{err.why()}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib FileError eof wrapper covers direct and method access") {
  const std::string source = R"(
import /std/file/*

[return<void>]
main() {
  [FileError] eofErr{fileReadEof()}
  [FileError] otherErr{1i32}
  [bool] directEof{FileError.is_eof(eofErr)}
  [bool] viaTypeEof{FileError.is_eof(eofErr)}
  [bool] methodEof{eofErr.is_eof()}
  [bool] helperOther{fileErrorIsEof(otherErr)}
  [bool] viaTypeOther{FileError.is_eof(otherErr)}
  [bool] methodOther{otherErr.is_eof()}
  if(and(and(and(directEof, viaTypeEof), methodEof),
         and(not(helperOther), and(not(viaTypeOther), not(methodOther)))),
     then(){ return() },
     else(){ return() })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin FileError why method rejects explicit arguments without imported wrapper") {
  const std::string source = R"(
[return<void>]
main() {
  [FileError] err{13i32}
  [string] why{err.why(1i32)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("why does not accept arguments") != std::string::npos);
}

TEST_CASE("stdlib FileError eof constructor wrapper returns FileError values") {
  const std::string source = R"(
import /std/file/*

[return<void>]
main() {
  [FileError] direct{FileError.eof()}
  [FileError] viaType{FileError.eof()}
  [Result<FileError>] status{FileError.status(FileError.eof())}
  [bool] eof{direct.is_eof()}
  [bool] typeEof{FileError.is_eof(viaType)}
  [bool] statusError{Result.error(status)}
  if(and(and(eof, typeEof), statusError), then(){ return() }, else(){ return() })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib FileError root why alias is removed") {
  const std::string source = R"(
import /std/file/*

[return<void>]
main() {
  [string] why{/FileError/why(fileReadEof())}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_CASE("stdlib FileError root result aliases are removed") {
  const std::string source = R"(
import /std/file/*

[return<void>]
main() {
  [Result<FileError>] status{/FileError/status(fileReadEof())}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_CASE("stdlib FileError package status alias is removed") {
  const std::string source = R"(
import /std/file/*

[return<void>]
main() {
  [Result<FileError>] status{fileErrorStatus(fileReadEof())}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_CASE("stdlib FileError package value alias is removed") {
  const std::string source = R"(
import /std/file/*

[return<void>]
main() {
  [Result<i32, FileError>] value{fileErrorResult<i32>(fileReadEof())}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_CASE("stdlib FileError result methods reject unexpected arguments") {
  const std::string source = R"(
import /std/file/*

[return<void>]
main() {
  [FileError] err{fileReadEof()}
  [Result<FileError>] status{err.status(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/file/FileError/status") != std::string::npos);
}

TEST_CASE("stdlib FileError root eof aliases are removed") {
  const std::string source = R"(
import /std/file/*

[return<void>]
main() {
  [bool] eof{/FileError/is_eof(fileReadEof())}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_CASE("stdlib FileError eof wrapper rejects unexpected arguments") {
  const std::string source = R"(
import /std/file/*

[return<void>]
main() {
  [FileError] err{FileError.eof(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/file/FileError/eof") != std::string::npos);
}

TEST_CASE("stdlib FileError status helper rejects non file errors") {
  const std::string source = R"(
import /std/file/*

[return<void>]
main() {
  [Result<FileError>] status{FileError.status("bad"utf8)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_CASE("stdlib File helpers cover imported method and slash-call wrappers") {
  const std::string source = R"(
import /std/file/*

[effects(file_write), return<void>]
write_out([File<Write>] file, [array<i32>] bytes, [string] text) {
  [Result<FileError>] directWrite{/File/write<Write, i32>(file, 65i32)}
  [Result<FileError>] directWriteLine{/File/write_line<Write, i32>(file, 7i32)}
  [Result<FileError>] directTextWrite{/File/write<Write, string>(file, text)}
  [Result<FileError>] directTextWriteLine{/File/write_line<Write, string>(file, text)}
  [Result<FileError>] methodByte{file.write_byte(65i32)}
  [Result<FileError>] directBytes{/File/write_bytes(file, bytes)}
  [Result<FileError>] methodFlush{file.flush()}
  [Result<FileError>] directFlush{/File/flush(file)}
  [Result<FileError>] methodClose{file.close()}
  [Result<FileError>] directClose{/File/close<Write>(file)}
  return()
}

[effects(file_read), return<void>]
read_in([File<Read>] file, [i32 mut] value) {
  [Result<FileError>] methodRead{file.read_byte(value)}
  [Result<FileError>] directRead{/File/read_byte(file, value)}
  return()
}

[return<void>]
main() {
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("exact stdlib file imports keep file and FileError method helpers") {
  const std::string source = R"(
import /std/file/File
import /std/file/FileError

[effects(file_write), return<void>]
write_out([File<Write>] file) {
  [FileError] err{FileError.eof()}
  [Result<FileError>] lineStatus{file.write_line(7i32)}
  [Result<FileError>] flushStatus{file.flush()}
  [Result<FileError>] closeStatus{file.close()}
  [Result<FileError>] methodStatus{err.status()}
  [Result<i32, FileError>] methodValue{err.result<i32>()}
  [string] whyText{err.why()}
  [bool] eof{err.is_eof()}
  return()
}

[return<void>]
main() {
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib File nine-value helpers cover imported method and slash-call wrappers") {
  const std::string source = R"(
import /std/file/*

[effects(file_write), return<void>]
write_out([File<Write>] file) {
  [Result<FileError>] directWrite{/File/write<Write, string, i32, string, i32, string, i32, string, i32, string>(
      file,
      "alpha"utf8,
      7i32,
      "omega"utf8,
      8i32,
      "delta"utf8,
      9i32,
      "!"utf8,
      10i32,
      "."utf8)}
  [Result<FileError>] directWriteLine{/File/write_line<Write, i32, string, i32, string, i32, string, i32, string, i32>(
      file,
      255i32,
      " "utf8,
      0i32,
      " "utf8,
      0i32,
      "?"utf8,
      1i32,
      "."utf8,
      2i32)}
  [Result<FileError>] methodWrite{file.write("left"utf8, 1i32, "mid"utf8, 2i32, "right"utf8, 3i32, "."utf8, 4i32, "!"utf8)}
  [Result<FileError>] methodWriteLine{file.write_line(4i32, " "utf8, 5i32, " "utf8, 6i32, "?"utf8, 7i32, "!"utf8, 8i32)}
  return()
}

[return<void>]
main() {
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib File broader helper calls fall back to builtin variadics") {
  const std::string source = R"(
import /std/file/*

[effects(file_write), return<void>]
write_out([File<Write>] file) {
  [Result<FileError>] directWrite{/File/write<Write, string, i32, string, i32, string, i32, string, i32, string, i32>(
      file,
      "alpha"utf8,
      1i32,
      "-"utf8,
      2i32,
      "-"utf8,
      3i32,
      "="utf8,
      4i32,
      "."utf8,
      10i32)}
  [Result<FileError>] directWriteLine{/File/write_line<Write, i32, string, i32, string, i32, string, i32, string, i32, string>(
      file,
      4i32,
      " "utf8,
      5i32,
      " "utf8,
      6i32,
      "?"utf8,
      7i32,
      "!"utf8,
      8i32,
      "#"utf8)}
  [Result<FileError>] methodWrite{file.write("left"utf8, 1i32, "mid"utf8, 2i32, "right"utf8, 3i32, "."utf8, 4i32, "!"utf8, 5i32)}
  [Result<FileError>] methodWriteLine{file.write_line(4i32, " "utf8, 5i32, " "utf8, 6i32, "?"utf8, 7i32, "!"utf8, 8i32, "#"utf8)}
  return()
}

[return<void>]
main() {
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib File text slash-call helpers require stdlib import") {
  const std::string source = R"(
[effects(file_write), return<void>]
write_out([File<Write>] file) {
  /File/write(file, "hello"utf8)
}

[return<void>]
main() {
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /File/write") != std::string::npos);
}

TEST_CASE("stdlib File open slash-call helpers require stdlib import") {
  const std::string source = R"(
[effects(file_write), return<void>]
write_out() {
  /File/open_write("hello.txt"utf8)
}

[return<void>]
main() {
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /File/open_write") != std::string::npos);
}

TEST_CASE("stdlib File close slash-call helper infers template args from File receiver") {
  const std::string source = R"(
import /std/file/*

[effects(file_write), return<void>]
write_out([File<Write>] file) {
  /File/close(file)
}

[return<void>]
main() {
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();
