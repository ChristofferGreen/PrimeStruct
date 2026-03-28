TEST_CASE("binding align_bytes validates") {
  const std::string source = R"(
[return<int>]
main() {
  [align_bytes(16) i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding align_bytes rejects template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [align_bytes<i32>(16) i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("align_bytes does not accept template arguments") != std::string::npos);
}

TEST_CASE("binding align_bytes rejects wrong argument count") {
  const std::string source = R"(
[return<int>]
main() {
  [align_bytes(4, 8) i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("align_bytes requires exactly one integer argument") != std::string::npos);
}

TEST_CASE("binding align_kbytes rejects template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [align_kbytes<i32>(4) i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("align_kbytes does not accept template arguments") != std::string::npos);
}

TEST_CASE("binding align_kbytes rejects invalid") {
  const std::string source = R"(
[return<int>]
main() {
  [align_kbytes(0) i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("align_kbytes requires a positive integer argument") != std::string::npos);
}

TEST_CASE("binding align_kbytes rejects wrong argument count") {
  const std::string source = R"(
[return<int>]
main() {
  [align_kbytes(4, 8) i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("align_kbytes requires exactly one integer argument") != std::string::npos);
}

TEST_CASE("binding align_kbytes validates") {
  const std::string source = R"(
[return<int>]
main() {
  [align_kbytes(4) i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding align_bytes rejects invalid") {
  const std::string source = R"(
[return<int>]
main() {
  [align_bytes(0) i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("align_bytes requires a positive integer argument") != std::string::npos);
}

TEST_CASE("binding rejects return transform with inferred type") {
  const std::string source = R"(
[return<int>]
main() {
  [return<int>] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding does not accept return transform") != std::string::npos);
}

TEST_CASE("binding qualifiers are allowed") {
  const std::string source = R"(
[return<int>]
main() {
  [public float] exposure{1.5f}
  [private align_bytes(16) i32 mut] count{1i32}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding rejects multiple visibility qualifiers") {
  const std::string source = R"(
[return<int>]
main() {
  [public private i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding visibility transforms are mutually exclusive") != std::string::npos);
}

TEST_SUITE_END();
