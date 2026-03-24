TEST_SUITE_BEGIN("primestruct.semantics.result_helpers");

TEST_CASE("Result.error in if conditions is bool-compatible") {
  const std::string source = R"(
[return<Result<FileError>>]
main() {
  [Result<FileError>] status{ Result.ok() }
  if(Result.error(status), then(){ return(Result.ok()) }, else(){ return(Result.ok()) })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("direct Result.ok expressions participate in Result helpers") {
  const std::string source = R"(
swallow_file_error([FileError] err) {}

[return<int> on_error<FileError, /swallow_file_error>]
main() {
  [Result<i32, FileError>] status{ Result.ok(7i32) }
  [bool] is_error{ Result.error(status) }
  [string] why{ Result.why(status) }
  [i32] value{ try(status) }
  if(or(is_error, not(equal(value, 7i32))), then(){ return(1i32) }, else(){ return(0i32) })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("Result.error rejects non-result argument") {
  const std::string source = R"(
[return<void>]
main() {
  if(Result.error(1i32), then(){ return() }, else(){ return() })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("Result.error requires Result argument") != std::string::npos);
}

TEST_CASE("Result.why explicit string binding accepts FileError results") {
  const std::string source = R"(
[return<void>]
main() {
  [Result<FileError>] status{ Result.ok() }
  [string] message{ Result.why(status) }
  return()
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("Result.why explicit string binding accepts non-FileError results") {
  const std::string source = R"(
[struct]
OtherError() {
  [i32] code{1i32}
}

[return<void>]
main() {
  [Result<OtherError>] status{ Result.ok() }
  [string] message{ Result.why(status) }
  return()
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("Result.why infers string binding") {
  const std::string source = R"(
[return<void>]
main() {
  [Result<FileError>] status{ Result.ok() }
  [auto] message{ Result.why(status) }
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("Result.error infers bool binding") {
  const std::string source = R"(
[return<void>]
main() {
  [Result<FileError>] status{ Result.ok() }
  [auto] is_error{ Result.error(status) }
  if(is_error, then(){ return() }, else(){ return() })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib file error result helpers construct status and value results") {
  const std::string source = R"(
import /std/file/*

[return<void>]
main() {
  [FileError] err{fileReadEof()}
  [Result<FileError>] status{FileError.status(fileReadEof())}
  [Result<FileError>] directStatus{FileError.status(fileReadEof())}
  [Result<i32, FileError>] valueStatus{FileError.result<i32>(fileReadEof())}
  [Result<i32, FileError>] directValueStatus{FileError.result<i32>(fileReadEof())}
  [Result<FileError>] wrappedStatus{FileError.status(err)}
  [Result<i32, FileError>] wrappedValue{FileError.result<i32>(err)}
  [bool] eof{fileErrorIsEof(fileReadEof())}
  [bool] otherEof{fileErrorIsEof(1i32)}
  [bool] statusError{Result.error(status)}
  [bool] directStatusError{Result.error(directStatus)}
  [bool] wrappedStatusError{Result.error(wrappedStatus)}
  [bool] valueError{Result.error(valueStatus)}
  [bool] directValueError{Result.error(directValueStatus)}
  [bool] wrappedValueError{Result.error(wrappedValue)}
  [string] statusWhy{Result.why(status)}
  [string] directStatusWhy{Result.why(directStatus)}
  [string] wrappedStatusWhy{Result.why(wrappedStatus)}
  [string] valueWhy{Result.why(valueStatus)}
  [string] directValueWhy{Result.why(directValueStatus)}
  [string] wrappedValueWhy{Result.why(wrappedValue)}
  if(and(and(and(and(statusError, directStatusError), wrappedStatusError),
             and(and(valueError, directValueError), wrappedValueError)),
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
  [bool] eof{fileErrorIsEof(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/file/fileErrorIsEof parameter err") != std::string::npos);
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
  CHECK(error.find("unknown call target: /FileError/why") != std::string::npos);
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
  CHECK(error.find("unknown call target: /FileError/status") != std::string::npos);
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
  CHECK(error.find("unknown call target: fileErrorStatus") != std::string::npos);
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
  CHECK(error.find("unknown call target: fileErrorResult") != std::string::npos);
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
  CHECK(error.find("unknown call target: /FileError/is_eof") != std::string::npos);
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
  [Result<FileError>] status{FileError.status(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/file/FileError/status parameter err") != std::string::npos);
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

TEST_CASE("stdlib File multi-value helpers cover imported method and slash-call wrappers") {
  const std::string source = R"(
import /std/file/*

[effects(file_write), return<void>]
write_out([File<Write>] file) {
  [Result<FileError>] directWrite{/File/write<Write, string, i32, string>(file, "alpha"utf8, 7i32, "omega"utf8)}
  [Result<FileError>] directWriteLine{/File/write_line<Write, i32, string, i32, string, i32>(
      file,
      255i32,
      " "utf8,
      0i32,
      " "utf8,
      0i32)}
  [Result<FileError>] methodWrite{file.write("left"utf8, 1i32, "right"utf8)}
  [Result<FileError>] methodWriteLine{file.write_line()}
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

TEST_CASE("stdlib File close slash-call helper requires explicit template args") {
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("template arguments required for /File/close") != std::string::npos);
}

TEST_CASE("templated helper overload families resolve by exact arity") {
  const std::string source = R"(
[struct]
Marker() {}

[return<i32>]
/helper/value<T>([T] value) {
  return(1i32)
}

[return<i32>]
/helper/value<A, B>([A] first, [B] second) {
  return(2i32)
}

[return<i32>]
/Marker/mark<T>([Marker] self, [T] value) {
  return(/helper/value(value))
}

[return<i32>]
/Marker/mark<A, B>([Marker] self, [A] first, [B] second) {
  return(/helper/value(first, second))
}

[return<void>]
main() {
  [Marker] marker{Marker()}
  [i32] directOne{/helper/value(7i32)}
  [i32] directTwo{/helper/value(7i32, true)}
  [i32] methodOne{marker.mark(9i32)}
  [i32] methodTwo{marker.mark(9i32, false)}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("templated helper overload families still reject duplicate same-arity definitions") {
  const std::string source = R"(
[return<i32>]
/helper/value<T>([T] value) {
  return(1i32)
}

[return<i32>]
/helper/value<U>([U] other) {
  return(2i32)
}

[return<void>]
main() {
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate definition: /helper/value") != std::string::npos);
}

TEST_CASE("stdlib image error result helpers construct status and value results") {
  const std::string source = R"(
import /std/image/*

[return<void>]
main() {
  [ImageError] err{imageReadUnsupported()}
  [Result<ImageError>] status{imageErrorStatus(imageReadUnsupported())}
  [Result<ImageError>] directStatus{/ImageError/status(imageReadUnsupported())}
  [Result<i32, ImageError>] valueStatus{imageErrorResult<i32>(imageInvalidOperation())}
  [Result<i32, ImageError>] directValueStatus{/ImageError/result<i32>(imageInvalidOperation())}
  [Result<ImageError>] wrappedStatus{/ImageError/status(err)}
  [Result<i32, ImageError>] wrappedValue{/ImageError/result<i32>(err)}
  [bool] statusError{Result.error(status)}
  [bool] directStatusError{Result.error(directStatus)}
  [bool] wrappedStatusError{Result.error(wrappedStatus)}
  [bool] valueError{Result.error(valueStatus)}
  [bool] directValueError{Result.error(directValueStatus)}
  [bool] wrappedValueError{Result.error(wrappedValue)}
  [string] directWhy{/ImageError/why(err)}
  [string] statusWhy{Result.why(status)}
  [string] directStatusWhy{Result.why(directStatus)}
  [string] wrappedStatusWhy{Result.why(wrappedStatus)}
  [string] valueWhy{Result.why(valueStatus)}
  [string] directValueWhy{Result.why(directValueStatus)}
  [string] wrappedValueWhy{Result.why(wrappedValue)}
  if(and(and(and(statusError, directStatusError), wrappedStatusError),
         and(and(valueError, directValueError), wrappedValueError)),
     then(){ return() },
     else(){ return() })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib ImageError why wrapper covers direct and Result-based access") {
  const std::string source = R"(
import /std/image/*

[return<void>]
main() {
  [ImageError] err{imageReadUnsupported()}
  [string] direct{/ImageError/why(err)}
  [string] viaType{ImageError.why(err)}
  [string] viaResult{Result.why(imageErrorStatus(err))}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib ImageError constructor wrappers expose type-owned error values") {
  const std::string source = R"(
import /std/image/*

[return<void>]
main() {
  [ImageError] readErr{/ImageError/read_unsupported()}
  [ImageError] writeErr{/ImageError/write_unsupported()}
  [ImageError] invalidErr{/ImageError/invalid_operation()}
  [string] readWhy{Result.why(imageErrorStatus(readErr))}
  [string] writeWhy{Result.why(imageErrorStatus(writeErr))}
  [string] invalidWhy{Result.why(imageErrorStatus(invalidErr))}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib ImageError why wrapper rejects non image errors") {
  const std::string source = R"(
import /std/image/*

[return<void>]
main() {
  [string] why{/ImageError/why(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /ImageError/why parameter err") != std::string::npos);
}

TEST_CASE("stdlib ImageError constructor wrappers reject unexpected arguments") {
  const std::string source = R"(
import /std/image/*

[return<void>]
main() {
  [ImageError] err{/ImageError/read_unsupported(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /ImageError/read_unsupported") != std::string::npos);
}

TEST_CASE("stdlib image error result helpers reject non image errors") {
  const std::string source = R"(
import /std/image/*

[return<void>]
main() {
  [Result<i32, ImageError>] status{/ImageError/result<i32>(true)}
  return()
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /ImageError/result__") != std::string::npos);
  CHECK(error.find("parameter err") != std::string::npos);
}

TEST_CASE("stdlib ImageError receiver methods reject unexpected arguments") {
  const std::string source = R"(
import /std/image/*

[return<void>]
main() {
  [ImageError] err{imageReadUnsupported()}
  [Result<ImageError>] status{err.status(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/image/ImageError/status") != std::string::npos);
}

TEST_CASE("stdlib container error result helpers construct status and value results") {
  const std::string source = R"(
import /std/collections/*

[return<void>]
main() {
  [ContainerError] err{containerMissingKey()}
  [Result<ContainerError>] status{containerErrorStatus(containerMissingKey())}
  [Result<ContainerError>] directStatus{/ContainerError/status(containerMissingKey())}
  [Result<i32, ContainerError>] valueStatus{containerErrorResult<i32>(containerCapacityExceeded())}
  [Result<i32, ContainerError>] directValueStatus{/ContainerError/result<i32>(containerCapacityExceeded())}
  [Result<ContainerError>] wrappedStatus{/ContainerError/status(err)}
  [Result<i32, ContainerError>] wrappedValue{/ContainerError/result<i32>(err)}
  [bool] statusError{Result.error(status)}
  [bool] directStatusError{Result.error(directStatus)}
  [bool] wrappedStatusError{Result.error(wrappedStatus)}
  [bool] valueError{Result.error(valueStatus)}
  [bool] directValueError{Result.error(directValueStatus)}
  [bool] wrappedValueError{Result.error(wrappedValue)}
  [string] direct{/ContainerError/why(err)}
  [string] statusWhy{Result.why(status)}
  [string] directStatusWhy{Result.why(directStatus)}
  [string] wrappedStatusWhy{Result.why(wrappedStatus)}
  [string] valueWhy{Result.why(valueStatus)}
  [string] directValueWhy{Result.why(directValueStatus)}
  [string] wrappedValueWhy{Result.why(wrappedValue)}
  if(and(and(and(statusError, directStatusError), wrappedStatusError),
         and(and(valueError, directValueError), wrappedValueError)),
     then(){ return() },
     else(){ return() })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib ContainerError constructor wrappers expose type-owned error values") {
  const std::string source = R"(
import /std/collections/*

[return<void>]
main() {
  [ContainerError] missing{/ContainerError/missing_key()}
  [ContainerError] oob{/ContainerError/index_out_of_bounds()}
  [ContainerError] emptyErr{/ContainerError/empty()}
  [ContainerError] capacity{/ContainerError/capacity_exceeded()}
  [string] missingWhy{Result.why(containerErrorStatus(missing))}
  [string] oobWhy{Result.why(containerErrorStatus(oob))}
  [string] emptyWhy{Result.why(containerErrorStatus(emptyErr))}
  [string] capacityWhy{Result.why(containerErrorStatus(capacity))}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib container error status helper rejects non container errors") {
  const std::string source = R"(
import /std/collections/*

[return<void>]
main() {
  [Result<ContainerError>] status{containerErrorStatus(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/containerErrorStatus parameter err") != std::string::npos);
}

TEST_CASE("stdlib ContainerError status helper rejects non container errors") {
  const std::string source = R"(
import /std/collections/*

[return<void>]
main() {
  [Result<ContainerError>] status{ContainerError.status(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/ContainerError/status parameter err") !=
        std::string::npos);
}

TEST_CASE("stdlib ContainerError receiver methods reject unexpected arguments") {
  const std::string source = R"(
import /std/collections/*

[return<void>]
main() {
  [ContainerError] err{containerMissingKey()}
  [Result<ContainerError>] status{err.status(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/ContainerError/status") !=
        std::string::npos);
}

TEST_CASE("stdlib ContainerError constructor wrappers reject unexpected arguments") {
  const std::string source = R"(
import /std/collections/*

[return<void>]
main() {
  [ContainerError] err{/ContainerError/missing_key(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /ContainerError/missing_key") != std::string::npos);
}

TEST_CASE("stdlib ContainerError why wrapper rejects non container errors") {
  const std::string source = R"(
import /std/collections/*

[return<void>]
main() {
  [string] why{/ContainerError/why(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /ContainerError/why parameter err") != std::string::npos);
}

TEST_CASE("stdlib gfx error result helpers construct status and value results") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<void>]
main() {
  [GfxError] err{queueSubmitFailed()}
  [Result<GfxError>] status{GfxError.status(queueSubmitFailed())}
  [Result<GfxError>] wrappedStatus{GfxError.status(err)}
  [Result<i32, GfxError>] valueStatus{GfxError.result<i32>(framePresentFailed())}
  [Result<i32, GfxError>] wrappedValue{GfxError.result<i32>(err)}
  [bool] statusError{Result.error(status)}
  [bool] wrappedStatusError{Result.error(wrappedStatus)}
  [bool] valueError{Result.error(valueStatus)}
  [bool] wrappedValueError{Result.error(wrappedValue)}
  [string] directWhy{Result.why(GfxError.status(err))}
  [string] statusWhy{Result.why(status)}
  [string] wrappedStatusWhy{Result.why(wrappedStatus)}
  [string] valueWhy{Result.why(valueStatus)}
  [string] wrappedValueWhy{Result.why(wrappedValue)}
  if(and(and(statusError, wrappedStatusError),
         and(valueError, wrappedValueError)),
     then(){ return() },
     else(){ return() })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental stdlib gfx package status alias is removed") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<void>]
main() {
  [Result<GfxError>] status{gfxErrorStatus(queueSubmitFailed())}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: gfxErrorStatus") != std::string::npos);
}

TEST_CASE("stdlib GfxError status helper rejects non gfx errors") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<void>]
main() {
  [Result<GfxError>] status{GfxError.status(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/gfx/experimental/GfxError/status parameter err") !=
        std::string::npos);
}

TEST_CASE("stdlib GfxError receiver methods reject unexpected arguments") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<void>]
main() {
  [GfxError] err{queueSubmitFailed()}
  [Result<GfxError>] status{err.status(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/gfx/experimental/GfxError/status") !=
        std::string::npos);
}

TEST_CASE("canonical stdlib gfx error result helpers construct status and value results") {
  const std::string source = R"(
import /std/gfx/*

[return<void>]
main() {
  [GfxError] err{queueSubmitFailed()}
  [Result<GfxError>] status{GfxError.status(queueSubmitFailed())}
  [Result<GfxError>] directStatus{GfxError.status(queueSubmitFailed())}
  [Result<GfxError>] wrappedStatus{GfxError.status(err)}
  [Result<i32, GfxError>] valueStatus{GfxError.result<i32>(framePresentFailed())}
  [Result<i32, GfxError>] directValueStatus{GfxError.result<i32>(framePresentFailed())}
  [Result<i32, GfxError>] wrappedValue{GfxError.result<i32>(err)}
  [bool] statusError{Result.error(status)}
  [bool] directStatusError{Result.error(directStatus)}
  [bool] wrappedStatusError{Result.error(wrappedStatus)}
  [bool] valueError{Result.error(valueStatus)}
  [bool] directValueError{Result.error(directValueStatus)}
  [bool] wrappedValueError{Result.error(wrappedValue)}
  [string] directWhy{GfxError.why(err)}
  [string] statusWhy{Result.why(status)}
  [string] directStatusWhy{Result.why(directStatus)}
  [string] wrappedStatusWhy{Result.why(wrappedStatus)}
  [string] valueWhy{Result.why(valueStatus)}
  [string] directValueWhy{Result.why(directValueStatus)}
  [string] wrappedValueWhy{Result.why(wrappedValue)}
  if(and(and(and(statusError, directStatusError), wrappedStatusError),
         and(and(valueError, directValueError), wrappedValueError)),
     then(){ return() },
     else(){ return() })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib gfx error helpers return explicit strings") {
  const std::string source = R"(
import /std/gfx/*

[return<void>]
main() {
  [GfxError] err{queueSubmitFailed()}
  [Result<GfxError>] methodStatus{GfxError.status(err)}
  [Result<i32, GfxError>] methodValueStatus{GfxError.result<i32>(err)}
  [string] direct{GfxError.why(err)}
  [string] method{GfxError.why(err)}
  [string] receiver{err.why()}
  [string] viaResult{Result.why(GfxError.status(err))}
  [string] viaMethodStatus{Result.why(methodStatus)}
  [string] viaMethodValue{Result.why(methodValueStatus)}
  [string] viaDirectMethodStatus{Result.why(methodStatus)}
  [string] viaDirectMethodValue{Result.why(methodValueStatus)}
  if(and(greater_than(count(direct), 0i32),
         and(and(greater_than(count(method), 0i32), greater_than(count(receiver), 0i32)),
             and(and(greater_than(count(viaResult), 0i32),
                     greater_than(count(viaMethodStatus), 0i32)),
                 and(greater_than(count(viaMethodValue), 0i32),
                     and(greater_than(count(viaDirectMethodStatus), 0i32),
                         greater_than(count(viaDirectMethodValue), 0i32)))))),
     then(){ return() },
     else(){ return() })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib GfxError constructors expose type-owned error values") {
  const std::string source = R"(
import /std/gfx/*

[return<void>]
main() {
  [GfxError] windowErr{GfxError.window_create_failed()}
  [GfxError] deviceErr{GfxError.device_create_failed()}
  [GfxError] swapchainErr{GfxError.swapchain_create_failed()}
  [GfxError] meshErr{GfxError.mesh_create_failed()}
  [GfxError] pipelineErr{GfxError.pipeline_create_failed()}
  [GfxError] materialErr{GfxError.material_create_failed()}
  [GfxError] frameErr{GfxError.frame_acquire_failed()}
  [GfxError] queueErr{GfxError.queue_submit_failed()}
  [GfxError] presentErr{GfxError.frame_present_failed()}
  [string] windowWhy{Result.why(GfxError.status(windowErr))}
  [string] presentWhy{Result.why(GfxError.status(presentErr))}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib gfx package value alias is removed") {
  const std::string source = R"(
import /std/gfx/*

[return<void>]
main() {
  [Result<i32, GfxError>] value{gfxErrorResult<i32>(framePresentFailed())}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: gfxErrorResult") != std::string::npos);
}

TEST_CASE("canonical root GfxError compatibility wrappers are removed") {
  const std::string source = R"(
import /std/gfx/*

[return<void>]
main() {
  [Result<GfxError>] status{/GfxError/status(queueSubmitFailed())}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /GfxError/status") != std::string::npos);
}

TEST_CASE("canonical stdlib GfxError receiver methods reject unexpected arguments") {
  const std::string source = R"(
import /std/gfx/*

[return<void>]
main() {
  [GfxError] err{queueSubmitFailed()}
  [Result<GfxError>] status{err.status(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/gfx/GfxError/status") != std::string::npos);
}

TEST_CASE("canonical stdlib GfxError constructors reject unexpected arguments") {
  const std::string source = R"(
import /std/gfx/*

[return<void>]
main() {
  [GfxError] err{GfxError.window_create_failed(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/gfx/GfxError/window_create_failed") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx error why helper rejects non gfx errors") {
  const std::string source = R"(
import /std/gfx/*

[return<void>]
main() {
  [string] why{GfxError.why(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/gfx/GfxError/why parameter err") != std::string::npos);
}

TEST_CASE("stdlib gfx Buffer helpers cover experimental method and slash-call wrappers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<void>]
main() {
  [Buffer<i32>] emptyBuffer{Buffer<i32>([token] 0i32, [elementCount] 0i32)}
  [Buffer<i32>] fullBuffer{Buffer<i32>([token] 7i32, [elementCount] 3i32)}
  [i32] methodCount{fullBuffer.count()}
  [i32] directCount{/std/gfx/experimental/Buffer/count(emptyBuffer)}
  [bool] methodEmpty{emptyBuffer.empty()}
  [bool] directEmpty{/std/gfx/experimental/Buffer/empty(fullBuffer)}
  [bool] methodValid{fullBuffer.is_valid()}
  [bool] directValid{/std/gfx/experimental/Buffer/is_valid(emptyBuffer)}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib gfx Buffer helpers cover method and slash-call wrappers") {
  const std::string source = R"(
import /std/gfx/*

[return<void>]
main() {
  [Buffer<i32>] emptyBuffer{Buffer<i32>([token] 0i32, [elementCount] 0i32)}
  [Buffer<i32>] fullBuffer{Buffer<i32>([token] 7i32, [elementCount] 3i32)}
  [i32] methodCount{fullBuffer.count()}
  [i32] directCount{/std/gfx/Buffer/count(emptyBuffer)}
  [bool] methodEmpty{emptyBuffer.empty()}
  [bool] directEmpty{/std/gfx/Buffer/empty(fullBuffer)}
  [bool] methodValid{fullBuffer.is_valid()}
  [bool] directValid{/std/gfx/Buffer/is_valid(emptyBuffer)}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib gfx Buffer readback helpers cover experimental method and slash-call wrappers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gpu/buffer<i32>(2i32)}
  [array<i32>] viaMethod{data.readback()}
  [array<i32>] viaDirect{/std/gfx/experimental/Buffer/readback(data)}
  return(plus(viaMethod.count(), viaDirect.count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib gfx Buffer readback helpers cover method and slash-call wrappers") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gpu/buffer<i32>(2i32)}
  [array<i32>] viaMethod{data.readback()}
  [array<i32>] viaDirect{/std/gfx/Buffer/readback(data)}
  return(plus(viaMethod.count(), viaDirect.count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib gfx Buffer compute access helpers cover experimental method and slash-call wrappers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[compute workgroup_size(1, 1, 1)]
/copy_values([Buffer<i32>] input, [Buffer<i32>] output) {
  [i32] viaMethod{input.load(0i32)}
  [i32] viaDirect{/std/gfx/experimental/Buffer/load(input, 0i32)}
  output.store(0i32, viaMethod)
  /std/gfx/experimental/Buffer/store(output, 1i32, viaDirect)
  return()
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib gfx Buffer compute access helpers cover method and slash-call wrappers") {
  const std::string source = R"(
import /std/gfx/*

[compute workgroup_size(1, 1, 1)]
/copy_values([Buffer<i32>] input, [Buffer<i32>] output) {
  [i32] viaMethod{input.load(0i32)}
  [i32] viaDirect{/std/gfx/Buffer/load(input, 0i32)}
  output.store(0i32, viaMethod)
  /std/gfx/Buffer/store(output, 1i32, viaDirect)
  return()
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental stdlib gfx Buffer arg-pack method receivers cover value, borrowed, and pointer packs") {
  const std::string source = R"(
import /std/gfx/experimental/*

[compute workgroup_size(1, 1, 1)]
/copy_values([args<Buffer<i32>>] values) {
  [i32] viaMethod{values.at(0i32).load(0i32)}
  values[0i32].store(0i32, viaMethod)
  return()
}

[compute workgroup_size(1, 1, 1)]
/copy_refs([args<Reference<Buffer<i32>>>] values) {
  [i32] viaMethod{dereference(values.at(0i32)).load(0i32)}
  dereference(values[0i32]).store(0i32, viaMethod)
  return()
}

[compute workgroup_size(1, 1, 1)]
/copy_ptrs([args<Pointer<Buffer<i32>>>] values) {
  [i32] viaMethod{dereference(values.at(0i32)).load(0i32)}
  dereference(values[0i32]).store(0i32, viaMethod)
  return()
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib gfx Buffer allocation helpers cover experimental slash-call wrappers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gfx/experimental/Buffer/allocate<i32>(2i32)}
  [array<i32>] out{data.readback()}
  return(plus(/std/gfx/experimental/Buffer/count(data), out.count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib gfx Buffer allocation helpers cover slash-call wrappers") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gfx/Buffer/allocate<i32>(2i32)}
  [array<i32>] out{data.readback()}
  return(plus(/std/gfx/Buffer/count(data), out.count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib gfx Buffer upload helpers cover experimental slash-call wrappers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  [Buffer<i32>] data{/std/gfx/experimental/Buffer/upload(values)}
  [array<i32>] out{data.readback()}
  return(plus(out.count(), out[0i32]))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib gfx Buffer upload helpers cover slash-call wrappers") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  [Buffer<i32>] data{/std/gfx/Buffer/upload(values)}
  [array<i32>] out{data.readback()}
  return(plus(out.count(), out[0i32]))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib gfx Buffer slash-call helpers require stdlib import") {
  const std::string source = R"(
[effects(gpu_dispatch) return<int>]
main() {
  [Buffer<i32>] data{/std/gpu/buffer<i32>(2i32)}
  return(/std/gfx/Buffer/count(data))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/gfx/Buffer/count") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx Buffer upload slash-call helpers require stdlib import") {
  const std::string source = R"(
[effects(gpu_dispatch), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  [Buffer<i32>] data{/std/gfx/Buffer/upload(values)}
  return(data.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/gfx/Buffer/upload") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx Buffer readback slash-call helpers require stdlib import") {
  const std::string source = R"(
[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gpu/buffer<i32>(2i32)}
  [array<i32>] out{/std/gfx/Buffer/readback(data)}
  return(out.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/gfx/Buffer/readback") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx Buffer load slash-call helpers require stdlib import") {
  const std::string source = R"(
[compute workgroup_size(1, 1, 1)]
/noop([Buffer<i32>] data) {
  [i32] value{/std/gfx/Buffer/load(data, 0i32)}
  return()
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/gfx/Buffer/load") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx Buffer store slash-call helpers require stdlib import") {
  const std::string source = R"(
[compute workgroup_size(1, 1, 1)]
/noop([Buffer<i32>] data) {
  /std/gfx/Buffer/store(data, 0i32, 1i32)
  return()
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/gfx/Buffer/store") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx Buffer allocate slash-call helpers require stdlib import") {
  const std::string source = R"(
[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gfx/Buffer/allocate<i32>(2i32)}
  return(data.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/gfx/Buffer/allocate") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx Buffer allocate helper rejects non-integer size") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gfx/Buffer/allocate<i32>(1.5f32)}
  return(data.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("buffer size requires integer expression") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx Buffer allocate helper rejects unsupported element type") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<string>] data{/std/gfx/Buffer/allocate<string>(1i32)}
  return(data.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("buffer requires numeric/bool element type") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx Buffer readback helper rejects unsupported element type") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<string>] data{Buffer<string>([token] 1i32, [elementCount] 1i32)}
  [array<string>] out{data.readback()}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("readback requires numeric/bool element type") != std::string::npos);
}

TEST_CASE("canonical stdlib gfx Buffer upload helper rejects unsupported element type") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [array<string>] values{array<string>("x"utf8)}
  [Buffer<string>] data{/std/gfx/Buffer/upload(values)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("upload requires numeric/bool element type") != std::string::npos);
}

TEST_CASE("Result.map infers auto Result binding from lambda body") {
  const std::string source = R"(
[return<void>]
main() {
  [Result<i32, FileError>] status{ Result.ok(1i32) }
  [auto] mapped{ Result.map(status, []([i32] v) { return(plus(v, 1i32)) }) }
  [bool] failed{ Result.error(mapped) }
  [string] why{ Result.why(mapped) }
  if(failed, then(){ return() }, else(){ return() })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("Result.and_then and Result.map2 infer nested auto Result bindings") {
  const std::string source = R"(
[return<Result<i32, FileError>>]
lift([i32] value) {
  return(Result.ok(value))
}

[return<void>]
main() {
  [Result<i32, FileError>] first{ Result.ok(1i32) }
  [Result<i32, FileError>] second{ Result.ok(2i32) }
  [auto] summed{ Result.map2(first, second, []([i32] x, [i32] y) { return(plus(x, y)) }) }
  [auto] chained{ Result.and_then(summed, []([i32] total) { return(lift(plus(total, 1i32))) }) }
  [bool] failed{ Result.error(chained) }
  [string] why{ Result.why(chained) }
  if(failed, then(){ return() }, else(){ return() })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("Result.and_then infers direct Result.ok lambda return") {
  const std::string source = R"(
[effects(io_out)]
swallow_file_error([FileError] err) {}

[return<int> on_error<FileError, /swallow_file_error>]
main() {
  [Result<i32, FileError>] status{ Result.ok(4i32) }
  [auto] chained{ Result.and_then(status, []([i32] value) { return(Result.ok(plus(value, 3i32))) }) }
  [bool] failed{ Result.error(chained) }
  [string] why{ Result.why(chained) }
  [i32] value{ try(chained) }
  if(or(failed, not(equal(value, 7i32))), then(){ return(1i32) }, else(){ return(0i32) })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("Result.map requires lambda argument") {
  const std::string source = R"(
[return<void>]
main() {
  [Result<i32, FileError>] status{ Result.ok(1i32) }
  Result.map(status, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("Result.map requires a lambda argument") != std::string::npos);
}

TEST_CASE("Result.map2 requires matching error types") {
  const std::string source = R"(
[struct]
OtherError() {
  [i32] code{1i32}
}

[return<void>]
main() {
  [Result<i32, FileError>] first{ Result.ok(1i32) }
  [Result<i32, OtherError>] second{ Result.ok(2i32) }
  Result.map2(first, second, 0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("Result.map2 requires matching error types") != std::string::npos);
}

TEST_SUITE_END();
