TEST_CASE("struct definitions reject non-binding statements") {
  const std::string source = R"(
[struct]
main() {
  [i32] value{1i32}
  plus(1i32, 2i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct definitions may only contain field bindings") != std::string::npos);
}

TEST_CASE("handle transform marks struct definitions") {
  const std::string source = R"(
[handle]
main() {
  [i32] value{1i32}
}

[return<int>]
use() {
  [main] item{1i32}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/use", error));
  CHECK(error.empty());
}

TEST_CASE("gpu_lane transform marks struct definitions") {
  const std::string source = R"(
[gpu_lane]
main() {
  [i32] value{1i32}
}

[return<int>]
use() {
  [main] item{1i32}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/use", error));
  CHECK(error.empty());
}

TEST_CASE("struct transform rejects parameters") {
  const std::string source = R"(
[struct]
main([i32] x) {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct definitions cannot declare parameters") != std::string::npos);
}

TEST_CASE("lifecycle helpers require struct context") {
  const std::string source = R"(
[return<void>]
Create() {
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/Create", error));
  CHECK(error.find("lifecycle helper must be nested inside a struct") != std::string::npos);
}

TEST_CASE("lifecycle helpers allow struct parents") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<void>]
/thing/Create() {
}

[return<void>]
/thing/Destroy() {
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("lifecycle helpers allow nested definitions") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}

  Create() {
  }

  Destroy() {
  }
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("lifecycle helpers allow field-only struct parents") {
  const std::string source = R"(
thing() {
  [i32] value{1i32}
}

[return<void>]
/thing/Create() {
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("lifecycle helpers provide this") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[mut return<void>]
/thing/Create() {
  assign(this, this)
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("lifecycle helpers require mut for assignment") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<void>]
/thing/Create() {
  assign(this, this)
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("lifecycle helpers must return void") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
/thing/Create() {
  return(1i32)
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("lifecycle helpers must return void") != std::string::npos);
}

TEST_CASE("Create helpers reject parameters") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<void>]
/thing/Create([i32] x) {
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("lifecycle helpers do not accept parameters") != std::string::npos);
}

TEST_CASE("Copy helper accepts shorthand parameter") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}

  [mut]
  Copy(other) {
    assign(this, other)
  }
}

[return<int>]
main() {
  [thing mut] a{thing()}
  [thing] b{thing()}
  /thing/Copy(a, b)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("Copy helper accepts Reference<Self> parameter") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}

  [mut]
  Copy([Reference<Self>] other) {
    assign(this, other)
  }
}

[return<int>]
main() {
  [thing mut] a{thing()}
  [thing] b{thing()}
  /thing/Copy(a, b)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("Copy helper requires reference parameter") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}

  [mut]
  Copy([i32] other) {
    assign(this, other)
  }
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("Copy helper requires [Reference<Self>] parameter") != std::string::npos);
}

TEST_CASE("mut transform is rejected on non-helpers") {
  const std::string source = R"(
[mut return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mut transform is only supported on lifecycle helpers") != std::string::npos);
}

TEST_CASE("lifecycle helpers reject duplicate mut") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[mut mut return<void>]
/thing/Create() {
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate mut transform on /thing/Create") != std::string::npos);
}

TEST_CASE("lifecycle helpers reject mut template arguments") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[mut<i32> return<void>]
/thing/Create() {
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mut transform does not accept template arguments on /thing/Create") != std::string::npos);
}

TEST_CASE("lifecycle helpers reject mut arguments") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[mut(1) return<void>]
/thing/Create() {
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mut transform does not accept arguments on /thing/Create") != std::string::npos);
}

TEST_CASE("lifecycle helpers reject non-struct parents") {
  const std::string source = R"(
[return<int>]
thing() {
  return(1i32)
}

[return<void>]
/thing/Create() {
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/thing", error));
  CHECK(error.find("lifecycle helper must be nested inside a struct") != std::string::npos);
}

TEST_CASE("this is not available outside lifecycle helpers") {
  const std::string source = R"(
[return<int>]
main() {
  return(this)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown identifier") != std::string::npos);
}

TEST_CASE("lifecycle helpers accept placement variants") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<void>]
/thing/CreateStack() {
}

[return<void>]
/thing/DestroyStack() {
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}
