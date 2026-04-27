TEST_CASE("omitted initializer rejects Create with canonical map method precedence when constructor is not effect-free") {
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("omitted initializer requires effect-free zero-arg constructor: /Thing") != std::string::npos);
}

TEST_CASE("map method precedence now rejects omitted initializer through Create effectfulness gate") {
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
  CHECK(error.find("omitted initializer requires effect-free zero-arg constructor: /Thing") !=
        std::string::npos);
}

TEST_CASE("omitted initializer rejects Create with canonical map call precedence when constructor is not effect-free") {
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("omitted initializer requires effect-free zero-arg constructor: /Thing") != std::string::npos);
}

TEST_CASE("map call precedence now rejects omitted initializer through Create effectfulness gate") {
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
  CHECK(error.find("omitted initializer requires effect-free zero-arg constructor: /Thing") !=
        std::string::npos);
}

TEST_CASE("omitted initializer rejects Create with canonical slash-path map call helper when constructor is not effect-free") {
  const std::string source = R"(
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
  assign(this.value, /std/collections/map/count(items, true))
}

[return<int>]
main() {
  [Thing] value
  return(value.value)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("omitted initializer requires effect-free zero-arg constructor: /Thing") != std::string::npos);
}

TEST_CASE("omitted initializer rejects effect-free Create with map alias call helper fallback") {
  const std::string source = R"(
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
  assign(this.value, /map/count(items, true))
}

[return<int>]
main() {
  [Thing] value
  return(value.value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/count") != std::string::npos);
}

TEST_CASE("omitted initializer rejects wrapper-returned canonical map call helper fallback when constructor is not effect-free") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapItems() {
  return(map<i32, i32>(1i32, 2i32))
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
  assign(this.value, count(wrapItems(), true))
}

[return<int>]
main() {
  [Thing] value
  return(value.value)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("omitted initializer requires effect-free zero-arg constructor: /Thing") != std::string::npos);
}

TEST_CASE("omitted initializer keeps canonical diagnostics for wrapper-returned map call helper fallback") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapItems() {
  return(map<i32, i32>(1i32, 2i32))
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
  assign(this.value, count(wrapItems()))
}

[return<int>]
main() {
  [Thing] value
  return(value.value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/map/count") != std::string::npos);
  CHECK(error.find("effect-free zero-arg constructor") == std::string::npos);
}

TEST_CASE("omitted initializer rejects wrapper-returned canonical map method helper fallback when constructor is not effect-free") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapItems() {
  return(map<i32, i32>(1i32, 2i32))
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
  assign(this.value, wrapItems().count(true))
}

[return<int>]
main() {
  [Thing] value
  return(value.value)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("omitted initializer requires effect-free zero-arg constructor: /Thing") != std::string::npos);
}

TEST_CASE("omitted initializer keeps canonical diagnostics for wrapper-returned map method helper fallback") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapItems() {
  return(map<i32, i32>(1i32, 2i32))
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
  assign(this.value, wrapItems().count())
}

[return<int>]
main() {
  [Thing] value
  return(value.value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/map/count") != std::string::npos);
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
  [Thing] value{Thing{}}
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
  [Thing] value{Thing{}}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /Thing") != std::string::npos);
}

TEST_SUITE_END();
