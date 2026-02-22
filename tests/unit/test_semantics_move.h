TEST_SUITE_BEGIN("primestruct.semantics.move");

TEST_CASE("move marks binding as moved") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [i32] other{move(value)}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("use-after-move: value") != std::string::npos);
}

TEST_CASE("assign reinitializes moved binding") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [i32] other{move(value)}
  assign(value, 2i32)
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("move requires binding name") {
  const std::string source = R"(
[return<int>]
main() {
  move(1i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("move requires a binding name") != std::string::npos);
}

TEST_CASE("move rejects reference bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32> mut] ref{location(value)}
  move(ref)
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("move does not support Reference bindings") != std::string::npos);
}

TEST_SUITE_END();
