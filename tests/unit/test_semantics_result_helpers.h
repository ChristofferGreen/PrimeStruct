TEST_SUITE_BEGIN("primestruct.semantics.result_helpers");

TEST_CASE("Result.error in if conditions requires bool-compatible output") {
  const std::string source = R"(
[return<Result<FileError>>]
main() {
  [Result<FileError>] status{ Result.ok() }
  if(Result.error(status), then(){ return(Result.ok()) }, else(){ return(Result.ok()) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if condition requires bool") != std::string::npos);
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
