TEST_SUITE_BEGIN("primestruct.semantics.bindings.struct_defaults");

TEST_CASE("struct binding omits initializer when effect-free") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [Thing] value
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("omitted initializer rejects non-struct binding") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("omitted initializer requires struct type") != std::string::npos);
}

TEST_CASE("omitted initializer rejects effectful Create") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{1i32}
}

[effects(io_out) return<void>]
/Thing/Create() {
  print_line("hi"utf8)
}

[return<int>]
main() {
  [Thing] value
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("effect-free zero-arg constructor") != std::string::npos);
}

TEST_CASE("omitted initializer accepts effect-free Create") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{1i32}
}

[mut return<void>]
/Thing/Create() {
  assign(this, this)
}

[return<int>]
main() {
  [Thing] value
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("omitted initializer allows Create to fill missing fields") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value
}

[mut return<void>]
/Thing/Create() {
  assign(this.value, 1i32)
}

[return<int>]
main() {
  [Thing] value
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("omitted initializer rejects Create missing fields") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value
}

[return<void>]
/Thing/Create() {
}

[return<int>]
main() {
  [Thing] value
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("omitted initializer requires zero-arg constructor") != std::string::npos);
}

TEST_CASE("zero-arg struct call allows Create to fill missing fields") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value
}

[mut return<void>]
/Thing/Create() {
  assign(this.value, 1i32)
}

[return<int>]
main() {
  [Thing] value{Thing()}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("zero-arg struct call rejects missing Create fields") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value
}

[return<void>]
/Thing/Create() {
}

[return<int>]
main() {
  [Thing] value{Thing()}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /Thing") != std::string::npos);
}

TEST_SUITE_END();
