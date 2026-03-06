TEST_SUITE_BEGIN("primestruct.semantics.lambdas");

TEST_CASE("lambda expressions validate without captures") {
  const std::string source = R"(
[return<void>]
main() {
  []([i32] value) { return(plus(value, 1i32)) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("lambda captures allow outer bindings") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] base{1i32}
  [value base]([i32] offset) { return(plus(base, offset)) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("lambda captures accept ref qualifier") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] base{1i32}
  [ref base]([i32] offset) { return(plus(base, offset)) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("lambda without capture rejects outer binding") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] base{1i32}
  []([i32] offset) { return(plus(base, offset)) }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown identifier: base") != std::string::npos);
}

TEST_CASE("lambda rejects unknown capture name") {
  const std::string source = R"(
[return<void>]
main() {
  [value missing]([i32] offset) { return(offset) }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown capture: missing") != std::string::npos);
}

TEST_CASE("lambda rejects invalid capture qualifier") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] base{1i32}
  [foo base]([i32] offset) { return(offset) }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid lambda capture") != std::string::npos);
}

TEST_CASE("lambda parameter default rejects user-defined array call") {
  const std::string source = R"(
[return<i32>]
array([i32] value) {
  return(value)
}

[return<void>]
main() {
  []([i32] value{array(1i32)}) { return(value) }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("lambda parameter default must be a literal or pure expression") != std::string::npos);
}

TEST_CASE("lambda parameter default allows builtin array literal") {
  const std::string source = R"(
[return<void>]
main() {
  []([array<i32>] value{array<i32>(1i32, 2i32)}) { return(at(value, 0i32)) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("lambda rejects conflicting capture-all tokens") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] base{1i32}
  [=, &]([i32] offset) { return(offset) }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid lambda capture") != std::string::npos);
}

TEST_SUITE_END();
