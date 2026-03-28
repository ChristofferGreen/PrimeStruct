TEST_CASE("binding allows templated type") {
  const std::string source = R"(
[return<int>]
main() {
  [handle<Texture>] value{1i32}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding allows struct types") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [thing] item{1i32}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding allows untagged struct definitions") {
  const std::string source = R"(
thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [thing] item{thing([value] 2i32)}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct constructor accepts named arguments") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
  [i32] count{2i32}
}

[return<int>]
main() {
  thing([count] 3i32, [value] 4i32)
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct constructor allows positional after labels") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
  [i32] count{2i32}
}

[return<int>]
main() {
  thing([count] 3i32, 4i32)
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct constructor allows defaulted fields") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
  [i32] count{2i32}
}

[return<int>]
main() {
  thing([value] 5i32)
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct constructor accepts named arguments while skipping static fields") {
  const std::string source = R"(
[struct]
thing() {
  [static i32] shared{1i32}
  [i32] value{2i32}
  [i32] count{3i32}
}

[return<int>]
main() {
  thing([count] 4i32, [value] 5i32)
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct constructor rejects extra arguments") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  thing(1i32, 2i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
}

TEST_CASE("struct constructor keeps argument mismatch when static fields are skipped") {
  const std::string source = R"(
[struct]
thing() {
  [static i32] shared{1i32}
  [i32] value{2i32}
}

[return<int>]
main() {
  thing(1i32, 2i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
}

TEST_CASE("wrapper-returned struct constructor validates in resolved helper arguments") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
  [i32] count{2i32}
}

[return<thing>]
buildThing() {
  return(thing([count] 4i32, [value] 3i32))
}

[return<int>]
use([thing] item) {
  return(item.count)
}

[return<int>]
main() {
  return(use(buildThing()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned struct constructor keeps resolved helper mismatch diagnostics") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<thing>]
buildThing() {
  return(thing([value] 3i32))
}

[return<int>]
use([thing] item, [i32] extra) {
  return(extra)
}

[return<int>]
main() {
  return(use(buildThing()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /use") != std::string::npos);
}

TEST_CASE("struct brace constructor works in arguments") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
use([thing] item) {
  return(1i32)
}

[return<int>]
main() {
  return(use(thing{ 2i32 }))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct brace constructor accepts return value blocks") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
use([thing] item) {
  return(1i32)
}

[return<int>]
main() {
  return(use(thing{
    return(2i32)
  }))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct brace constructor uses last expression") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
use([thing] item) {
  return(1i32)
}

[return<int>]
main() {
  return(use(thing{
    [i32] temp{1i32}
    plus(temp, 2i32)
  }))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding initializer accepts labeled arguments") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
  [i32] count{2i32}
}

[return<int>]
main() {
  [thing] item{thing([count] 3i32 [value] 4i32)}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding initializer accepts return value blocks") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{
    [i32] temp{1i32}
    return(plus(temp, 2i32))
  }
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding initializer rejects named args for builtins") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>([first] 1i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("binding initializer allows struct constructor block") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [thing] item{block(){ thing() }}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding initializer allows struct constructor if") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [thing] item{if(true, then(){ thing() }, else(){ thing() })}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct constructor rejects unknown named arguments") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  thing([missing] 2i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown named argument: missing") != std::string::npos);
}

TEST_CASE("binding resolves struct types in namespace") {
  const std::string source = R"(
namespace demo {
  [struct]
  widget() {
    [i32] value{1i32}
  }

  [return<int>]
  main() {
    [widget] item{1i32}
    return(1i32)
  }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/demo/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding allows pointer types") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [Pointer<i32>] ptr{location(value)}
  [Reference<i32>] ref{location(value)}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("i64 and u64 bindings validate") {
  const std::string sourceSigned = R"(
[return<i64>]
main() {
  [i64] value{9i64}
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(sourceSigned, "/main", error));
  CHECK(error.empty());

  const std::string sourceUnsigned = R"(
[return<u64>]
main() {
  [u64] value{10u64}
  return(value)
}
)";
  CHECK(validateProgram(sourceUnsigned, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer plus validates") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{3i32}
  return(dereference(plus(location(value), 0i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer minus validates") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{3i32}
  [i32] second{4i32}
  return(dereference(minus(location(second), 16i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer minus accepts u64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{3i32}
  [i32] second{4i32}
  return(dereference(minus(location(second), 16u64)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer plus accepts i64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{3i32}
  return(dereference(plus(location(value), 16i64)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer minus accepts i64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{3i32}
  [i32] second{4i32}
  return(dereference(minus(location(second), 16i64)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer minus rejects pointer + pointer") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{3i32}
  [i32] second{4i32}
  return(minus(location(first), location(second)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pointer arithmetic does not support pointer + pointer") != std::string::npos);
}

TEST_CASE("pointer minus rejects pointer on right") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{3i32}
  [Pointer<i32>] ptr{location(value)}
  return(minus(1i32, ptr))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pointer arithmetic requires pointer on the left") != std::string::npos);
}

TEST_CASE("pointer plus accepts reference locations") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{3i32}
  [i32] second{4i32}
  [Reference<i32>] ref{location(first)}
  return(dereference(plus(location(ref), 16i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer plus rejects pointer + pointer") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{3i32}
  [Pointer<i32>] ptr{location(value)}
  return(plus(location(value), ptr))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pointer arithmetic does not support pointer + pointer") != std::string::npos);
}

TEST_CASE("pointer plus rejects pointer on right") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{3i32}
  [Pointer<i32>] ptr{location(value)}
  return(plus(1i32, ptr))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pointer arithmetic requires pointer on the left") != std::string::npos);
}

TEST_CASE("pointer plus rejects non-integer offset") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{3i32}
  return(plus(location(value), true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pointer arithmetic requires integer offset") != std::string::npos);
}
