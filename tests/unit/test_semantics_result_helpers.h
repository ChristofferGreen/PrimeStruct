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
  [Result<FileError>] status{fileErrorStatus(fileReadEof())}
  [Result<i32, FileError>] valueStatus{fileErrorResult<i32>(fileReadEof())}
  [bool] eof{fileErrorIsEof(fileReadEof())}
  [bool] otherEof{fileErrorIsEof(1i32)}
  [bool] statusError{Result.error(status)}
  [bool] valueError{Result.error(valueStatus)}
  [string] statusWhy{Result.why(status)}
  [string] valueWhy{Result.why(valueStatus)}
  if(and(and(statusError, valueError), and(eof, not(otherEof))), then(){ return() }, else(){ return() })
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
  [string] direct{/FileError/why(err)}
  [string] method{err.why()}
  [string] viaResult{Result.why(fileErrorStatus(err))}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib FileError why wrapper rejects non file errors") {
  const std::string source = R"(
import /std/file/*

[return<void>]
main() {
  [string] why{/FileError/why(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /FileError/why parameter err") != std::string::npos);
}

TEST_CASE("stdlib image error result helpers construct status and value results") {
  const std::string source = R"(
import /std/image/*

[return<void>]
main() {
  [Result<ImageError>] status{imageErrorStatus(imageReadUnsupported())}
  [Result<i32, ImageError>] valueStatus{imageErrorResult<i32>(imageInvalidOperation())}
  [bool] statusError{Result.error(status)}
  [bool] valueError{Result.error(valueStatus)}
  [string] statusWhy{Result.why(status)}
  [string] valueWhy{Result.why(valueStatus)}
  if(and(statusError, valueError), then(){ return() }, else(){ return() })
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

TEST_CASE("stdlib image error result helpers reject non image errors") {
  const std::string source = R"(
import /std/image/*

[return<void>]
main() {
  [Result<i32, ImageError>] status{imageErrorResult<i32>(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/image/imageErrorResult parameter err") != std::string::npos);
}

TEST_CASE("stdlib container error result helpers construct status and value results") {
  const std::string source = R"(
import /std/collections/*

[return<void>]
main() {
  [Result<ContainerError>] status{containerErrorStatus(containerMissingKey())}
  [Result<i32, ContainerError>] valueStatus{containerErrorResult<i32>(containerCapacityExceeded())}
  [bool] statusError{Result.error(status)}
  [bool] valueError{Result.error(valueStatus)}
  [string] statusWhy{Result.why(status)}
  [string] valueWhy{Result.why(valueStatus)}
  if(and(statusError, valueError), then(){ return() }, else(){ return() })
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

TEST_CASE("stdlib gfx error result helpers construct status and value results") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<void>]
main() {
  [Result<GfxError>] status{gfxErrorStatus(queueSubmitFailed())}
  [Result<i32, GfxError>] valueStatus{gfxErrorResult<i32>(framePresentFailed())}
  [bool] statusError{Result.error(status)}
  [bool] valueError{Result.error(valueStatus)}
  [string] statusWhy{Result.why(status)}
  [string] valueWhy{Result.why(valueStatus)}
  if(and(statusError, valueError), then(){ return() }, else(){ return() })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib gfx error status helper rejects non gfx errors") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<void>]
main() {
  [Result<GfxError>] status{gfxErrorStatus(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/gfx/experimental/gfxErrorStatus parameter err") !=
        std::string::npos);
}

TEST_CASE("canonical stdlib gfx error result helpers construct status and value results") {
  const std::string source = R"(
import /std/gfx/*

[return<void>]
main() {
  [Result<GfxError>] status{gfxErrorStatus(queueSubmitFailed())}
  [Result<i32, GfxError>] valueStatus{gfxErrorResult<i32>(framePresentFailed())}
  [bool] statusError{Result.error(status)}
  [bool] valueError{Result.error(valueStatus)}
  [string] statusWhy{Result.why(status)}
  [string] valueWhy{Result.why(valueStatus)}
  if(and(statusError, valueError), then(){ return() }, else(){ return() })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib gfx error status helper rejects non gfx errors") {
  const std::string source = R"(
import /std/gfx/*

[return<void>]
main() {
  [Result<GfxError>] status{gfxErrorStatus(true)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/gfx/gfxErrorStatus parameter err") !=
        std::string::npos);
}

TEST_CASE("Result.map inline lambda syntax parses with explicit diagnostics") {
  const std::string source = R"(
[return<void>]
main() {
  [Result<i32, FileError>] status{ Result.ok(1i32) }
  [Result<i32, FileError>] mapped{ Result.map(status, []([i32] v) { return(plus(v, 1i32)) }) }
  if(Result.error(mapped), then(){ return() }, else(){ return() })
}
)";
  std::string error;
  CHECK_FALSE(parseProgramWithError(source, error));
  CHECK(error.find("unexpected token in expression") != std::string::npos);
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
