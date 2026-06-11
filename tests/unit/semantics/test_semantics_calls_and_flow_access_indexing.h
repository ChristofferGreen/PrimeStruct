TEST_CASE("count builtin rejects template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(count<i32>(array<i32>(1i32, 2i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("count does not accept template arguments") != std::string::npos);
}

TEST_CASE("count method rejects template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32).count<i32>())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("count does not accept template arguments") != std::string::npos);
}

TEST_CASE("count method rejects block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32).count() { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target") != std::string::npos);
}

TEST_CASE("array access rejects template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(at<i32>(array<i32>(1i32), 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at does not accept template arguments") != std::string::npos);
}

TEST_CASE("array literal access validates") {
  const std::string source = R"(
[return<int>]
main() {
  return(at(array<i32>(1i32, 2i32), 0i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array access rejects argument count mismatch") {
  const std::string source = R"(
[return<int>]
main() {
  return(at(array<i32>(1i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin at") != std::string::npos);
}

TEST_CASE("array access rejects block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(at(array<i32>(1i32), 0i32) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target") != std::string::npos);
}

TEST_CASE("unsafe array access rejects template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe<i32>(array<i32>(1i32), 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at_unsafe does not accept template arguments") != std::string::npos);
}

TEST_CASE("unsafe array access rejects argument count mismatch") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(1i32), 0i32, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin at_unsafe") != std::string::npos);
}

TEST_CASE("unsafe array access rejects block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(1i32), 0i32) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target") != std::string::npos);
}

TEST_CASE("array access rejects non-integer index") {
  const std::string source = R"(
[return<int>]
main() {
  return(at(array<i32>(1i32, 2i32), "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires integer index") != std::string::npos);
}

TEST_CASE("unsafe array access rejects non-integer index") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(1i32, 2i32), "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at_unsafe requires integer index") != std::string::npos);
}

TEST_CASE("string access rejects non-integer index") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"hello"utf8}
  return(at(text, "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires integer index") != std::string::npos);
}

TEST_CASE("string access validates integer index") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"hello"utf8}
  return(at(text, 0i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("string literal access rejects non-integer index") {
  const std::string source = R"(
[return<int>]
main() {
  return(at("hello"utf8, "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires integer index") != std::string::npos);
}

TEST_CASE("string literal access validates integer index") {
  const std::string source = R"(
[return<int>]
main() {
  return(at("hello"utf8, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe string access rejects non-integer index") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"hello"utf8}
  return(at_unsafe(text, "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at_unsafe requires integer index") != std::string::npos);
}

TEST_CASE("unsafe string access validates integer index") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"hello"utf8}
  return(at_unsafe(text, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe string literal access rejects non-integer index") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe("hello"utf8, "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at_unsafe requires integer index") != std::string::npos);
}

TEST_CASE("unsafe string literal access validates integer index") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe("hello"utf8, 0i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array access rejects non-collection target") {
  const std::string source = R"(
[return<int>]
main() {
  return(at(1i32, 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires array, vector, map, or string target") != std::string::npos);
}

TEST_CASE("unsafe array access rejects non-collection target") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(1i32, 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at_unsafe requires array, vector, map, or string target") != std::string::npos);
}

TEST_SUITE_END();
