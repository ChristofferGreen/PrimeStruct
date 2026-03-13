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

TEST_CASE("omitted initializer accepts builtin array field initializer") {
  const std::string source = R"(
[struct]
Thing() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
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

TEST_CASE("omitted initializer rejects effectful user-defined array field initializer") {
  const std::string source = R"(
[effects(io_out), return<i32>]
array<T>([T] value) {
  print_line("side effect"utf8)
  return(1i32)
}

[struct]
Thing() {
  [i32] value{array<i32>(7i32)}
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

TEST_CASE("omitted initializer rejects effect-free Create with vector alias call helper fallback") {
  const std::string source = R"(
[return<i32>]
/std/collections/vector/count([array<i32>] values, [bool] marker) {
  return(11i32)
}

[struct]
Thing() {
  [i32] value
}

[mut return<void>]
/Thing/Create() {
  [array<i32>] items{array<i32>(1i32, 2i32)}
  assign(this.value, /vector/count(items, true))
}

[return<int>]
main() {
  [Thing] value
  return(value.value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("omitted initializer rejects effect-free Create with vector alias method helper fallback") {
  const std::string source = R"(
[return<i32>]
/std/collections/vector/count([array<i32>] values, [bool] marker) {
  return(13i32)
}

[struct]
Thing() {
  [i32] value
}

[mut return<void>]
/Thing/Create() {
  [array<i32>] items{array<i32>(1i32, 2i32)}
  assign(this.value, items./vector/count(true))
}

[return<int>]
main() {
  [Thing] value
  return(value.value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /vector/count") != std::string::npos);
  CHECK(error.find("effect-free zero-arg constructor") == std::string::npos);
}

TEST_CASE("omitted initializer rejects effect-free Create with canonical slash-path vector method helper") {
  const std::string source = R"(
[return<i32>]
/std/collections/vector/count([array<i32>] values, [bool] marker) {
  return(15i32)
}

[struct]
Thing() {
  [i32] value
}

[mut return<void>]
/Thing/Create() {
  [array<i32>] items{array<i32>(1i32, 2i32)}
  assign(this.value, items./std/collections/vector/count(true))
}

[return<int>]
main() {
  [Thing] value
  return(value.value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /vector/count") != std::string::npos);
  CHECK(error.find("effect-free zero-arg constructor") == std::string::npos);
}

TEST_CASE("omitted initializer accepts effect-free Create with explicit-template vector alias call fallback") {
  const std::string source = R"(
[effects(io_out), return<i32>]
/vector/count([array<i32>] values, [bool] marker) {
  print_line("alias"utf8)
  return(9i32)
}

[return<i32>]
/std/collections/vector/count<T>([array<T>] values, [bool] marker) {
  return(17i32)
}

[struct]
Thing() {
  [i32] value
}

[mut return<void>]
/Thing/Create() {
  [array<i32>] items{array<i32>(1i32, 2i32)}
  assign(this.value, /vector/count<i32>(items, true))
}

[return<int>]
main() {
  [Thing] value
  return(value.value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("omitted initializer rejects effect-free Create with explicit-template vector alias method fallback") {
  const std::string source = R"(
[effects(io_out), return<i32>]
/vector/count([array<i32>] values, [bool] marker) {
  print_line("alias"utf8)
  return(9i32)
}

[return<i32>]
/std/collections/vector/count<T>([array<T>] values, [bool] marker) {
  return(19i32)
}

[struct]
Thing() {
  [i32] value
}

[mut return<void>]
/Thing/Create() {
  [array<i32>] items{array<i32>(1i32, 2i32)}
  assign(this.value, items./vector/count<i32>(true))
}

[return<int>]
main() {
  [Thing] value
  return(value.value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /vector/count") != std::string::npos);
  CHECK(error.find("effect-free zero-arg constructor") == std::string::npos);
}

TEST_CASE("omitted initializer accepts effect-free Create with bare array count method") {
  const std::string source = R"(
[return<i32>]
/array/count([array<i32>] values) {
  return(23i32)
}

[struct]
Thing() {
  [i32] value
}

[mut return<void>]
/Thing/Create() {
  [array<i32>] items{array<i32>(1i32, 2i32)}
  assign(this.value, items.count())
}

[return<int>]
main() {
  [Thing] value
  return(value.value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit-template vector alias fallback keeps mismatch diagnostics in Create") {
  const std::string source = R"(
[return<i32>]
/vector/count([array<i32>] values, [bool] marker) {
  return(7i32)
}

[return<i32>]
/std/collections/vector/count<T>([array<T>] values, [i32] marker) {
  return(marker)
}

[struct]
Thing() {
  [i32] value
}

[mut return<void>]
/Thing/Create() {
  [array<i32>] items{array<i32>(1i32, 2i32)}
  assign(this.value, /vector/count<i32>(items, true))
}

[return<int>]
main() {
  [Thing] value
  return(value.value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("effect-free zero-arg constructor") == std::string::npos);
}

TEST_CASE("vector alias method fallback keeps canonical mismatch diagnostics in Create") {
  const std::string source = R"(
[return<i32>]
/std/collections/vector/count([array<i32>] values, [i32] marker) {
  return(marker)
}

[struct]
Thing() {
  [i32] value
}

[mut return<void>]
/Thing/Create() {
  [array<i32>] items{array<i32>(1i32, 2i32)}
  assign(this.value, items./vector/count(true))
}

[return<int>]
main() {
  [Thing] value
  return(value.value)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("omitted initializer accepts effect-free Create with canonical map method precedence") {
  const std::string source = R"(
[effects(io_out), return<i32>]
/map/count([map<i32, i32>] values, [bool] marker) {
  print_line("alias"utf8)
  return(9i32)
}

[return<i32>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(27i32)
}

[struct]
Thing() {
  [i32] value
}

[mut return<void>]
/Thing/Create() {
  [map<i32, i32>] items{map<i32, i32>(1i32, 2i32)}
  assign(this.value, items.count(true))
}

[return<int>]
main() {
  [Thing] value
  return(value.value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map method precedence keeps canonical mismatch diagnostics in Create") {
  const std::string source = R"(
[return<i32>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(9i32)
}

[return<i32>]
/std/collections/map/count([map<i32, i32>] values, [i32] marker) {
  return(marker)
}

[struct]
Thing() {
  [i32] value
}

[mut return<void>]
/Thing/Create() {
  [map<i32, i32>] items{map<i32, i32>(1i32, 2i32)}
  assign(this.value, items.count(true))
}

[return<int>]
main() {
  [Thing] value
  return(value.value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("effect-free zero-arg constructor") == std::string::npos);
}

TEST_CASE("omitted initializer accepts effect-free Create with canonical map call precedence") {
  const std::string source = R"(
[effects(io_out), return<i32>]
/map/count([map<i32, i32>] values, [bool] marker) {
  print_line("alias"utf8)
  return(9i32)
}

[return<i32>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(27i32)
}

[struct]
Thing() {
  [i32] value
}

[mut return<void>]
/Thing/Create() {
  [map<i32, i32>] items{map<i32, i32>(1i32, 2i32)}
  assign(this.value, count(items, true))
}

[return<int>]
main() {
  [Thing] value
  return(value.value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map call precedence keeps canonical mismatch diagnostics in Create") {
  const std::string source = R"(
[return<i32>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(9i32)
}

[return<i32>]
/std/collections/map/count([map<i32, i32>] values, [i32] marker) {
  return(marker)
}

[struct]
Thing() {
  [i32] value
}

[mut return<void>]
/Thing/Create() {
  [map<i32, i32>] items{map<i32, i32>(1i32, 2i32)}
  assign(this.value, count(items, true))
}

[return<int>]
main() {
  [Thing] value
  return(value.value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("effect-free zero-arg constructor") == std::string::npos);
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
