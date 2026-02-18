TEST_SUITE_BEGIN("primestruct.semantics.bindings.pointers");

TEST_CASE("pointer helpers validate") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  return(dereference(location(value)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer helpers reject template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  return(dereference<i32>(location(value)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pointer helpers do not accept template arguments") != std::string::npos);
}

TEST_CASE("pointer helpers reject block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  return(dereference(location(value)) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target") != std::string::npos);
}

TEST_CASE("binding array type requires one template argument") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32, i32>] value{array<i32>(1i32)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("array requires exactly one template argument") != std::string::npos);
}

TEST_CASE("binding map type requires two template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32>] value{map<i32, i32>(1i32, 2i32)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map requires exactly two template arguments") != std::string::npos);
}

TEST_CASE("pointer bindings require template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Pointer] ptr{location(value)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("Pointer requires a template argument") != std::string::npos);
}

TEST_CASE("pointer bindings accept struct targets") {
  const std::string source = R"(
[struct]
Foo() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [Foo] value{Foo()}
  [Pointer<Foo>] ptr{location(value)}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reference bindings require template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Reference] ref{location(value)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("Reference requires a template argument") != std::string::npos);
}

TEST_CASE("reference bindings accept struct targets") {
  const std::string source = R"(
[struct]
Foo() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [Foo] value{Foo()}
  [Reference<Foo>] ref{location(value)}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer bindings reject unknown targets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Pointer<Missing>] ptr{location(value)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported pointer target type") != std::string::npos);
}

TEST_CASE("reference bindings reject unknown targets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Reference<Missing>] ref{location(value)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported reference target type") != std::string::npos);
}

TEST_CASE("location requires local binding name") {
  const std::string source = R"(
[return<int>]
main() {
  return(dereference(location(plus(1i32, 2i32))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("location requires a local binding") != std::string::npos);
}

TEST_CASE("location rejects parameters") {
  const std::string source = R"(
[return<int>]
main([i32] x) {
  [Pointer<i32>] ptr{location(x)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("location requires a local binding") != std::string::npos);
}

TEST_CASE("dereference accepts pointer parameters") {
  const std::string source = R"(
[return<int>]
read([Pointer<i32>] ptr) {
  return(dereference(ptr))
}

[return<int>]
main() {
  [i32] value{7i32}
  return(read(location(value)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer arithmetic accepts pointer parameters") {
  const std::string source = R"(
[return<int>]
offset([Pointer<i32>] ptr) {
  return(dereference(plus(ptr, 0i32)))
}

[return<int>]
main() {
  [i32] value{9i32}
  return(offset(location(value)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("assign allows mutable parameters") {
  const std::string source = R"(
[return<int>]
increment([i32 mut] value) {
  assign(value, plus(value, 1i32))
  return(value)
}

[return<int>]
main() {
  return(increment(4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("assign allows mutable pointer parameters") {
  const std::string source = R"(
[return<int>]
write([Pointer<i32> mut] ptr) {
  assign(dereference(ptr), 9i32)
  return(dereference(ptr))
}

[return<int>]
main() {
  [i32 mut] value{3i32}
  return(write(location(value)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

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

TEST_SUITE_END();
