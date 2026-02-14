TEST_SUITE_BEGIN("primestruct.semantics.bindings.core");

TEST_CASE("local binding requires initializer") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block expression requires a value") != std::string::npos);
}

TEST_CASE("local binding accepts brace initializer") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}


TEST_CASE("local binding infers type without transforms") {
  const std::string source = R"(
[return<int>]
main() {
  value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding initializer infers type from value block") {
  const std::string source = R"(
[return<int>]
main() {
  [mut] value{1i32 2i32}
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding initializer rejects void call") {
  const std::string source = R"(
[return<void>]
noop() {
  return()
}

[return<int>]
main() {
  [i32] value{noop()}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer requires a value") != std::string::npos);
}

TEST_CASE("binding infers type from user call") {
  const std::string source = R"(
namespace i64 {
  [return<i64>]
  inc([i64] self) {
    return(plus(self, 1i64))
  }
}

[return<i64>]
make() {
  return(3i64)
}

[return<i64>]
main() {
  value{make()}
  return(value.inc())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("local binding type must be supported") {
  const std::string source = R"(
[return<int>]
main() {
  [u32] value{1i32}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported binding type") != std::string::npos);
}

TEST_CASE("software numeric bindings are rejected") {
  const std::string source = R"(
[return<int>]
main() {
  [integer] value{1i32}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("software numeric types are not supported yet") != std::string::npos);
}

TEST_CASE("software numeric collection bindings are rejected") {
  const std::string source = R"(
[return<int>]
main() {
  [array<decimal>] values{array<decimal>(1.0f)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("software numeric types are not supported yet") != std::string::npos);
}

TEST_CASE("field-only definition can be used as a type") {
  const std::string source = R"(
Foo() {
  [i32] field{1i32}
}

[return<int>]
main() {
  [Foo] value{1i32}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("non-field definition is not a valid type") {
  const std::string source = R"(
[return<int>]
Bar() {
  return(1i32)
}

[return<int>]
main() {
  [Bar] value{1i32}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported binding type") != std::string::npos);
}

TEST_CASE("float binding validates") {
  const std::string source = R"(
[return<int>]
main() {
  [float] value{1.5f}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bool binding validates") {
  const std::string source = R"(
[return<int>]
main() {
  [bool] value{true}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("string binding validates") {
  const std::string source = R"(
[return<int>]
main() {
  [string] message{"hello"utf8}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("copy binding validates") {
  const std::string source = R"(
[return<int>]
main() {
  [copy i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding rejects effects transform arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [effects(io_out) i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding does not accept effects transform") != std::string::npos);
}

TEST_CASE("binding rejects capabilities transform arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [capabilities(io_out) i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding does not accept capabilities transform") != std::string::npos);
}

TEST_CASE("binding rejects return transform") {
  const std::string source = R"(
[return<int>]
main() {
  [return<int> i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding does not accept return transform") != std::string::npos);
}

TEST_CASE("binding rejects transform arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [mut(1) i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding transforms do not take arguments") != std::string::npos);
}

TEST_CASE("binding rejects placement transforms") {
  const std::string source = R"(
[return<int>]
main() {
  [stack i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("placement transforms are not supported on bindings") != std::string::npos);
}

TEST_CASE("parameter rejects placement transforms") {
  const std::string source = R"(
[return<int>]
main([stack i32] value) {
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("placement transforms are not supported on bindings") != std::string::npos);
}

TEST_CASE("binding rejects duplicate static transform") {
  const std::string source = R"(
[return<int>]
main() {
  [static static i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate static transform on binding") != std::string::npos);
}

TEST_CASE("restrict binding validates") {
  const std::string source = R"(
[return<int>]
main() {
  [restrict<i32> i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("restrict binding accepts int alias") {
  const std::string source = R"(
[return<int>]
main() {
  [restrict<int> i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("restrict requires template argument") {
  const std::string source = R"(
[return<int>]
main() {
  [restrict i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("restrict requires a template argument") != std::string::npos);
}

TEST_CASE("restrict rejects duplicate transform") {
  const std::string source = R"(
[return<int>]
main() {
  [restrict<i32> restrict<i32> i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate restrict transform") != std::string::npos);
}

TEST_CASE("restrict rejects mismatched type") {
  const std::string source = R"(
[return<int>]
main() {
  [restrict<i32> i64] value{1i64}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("restrict type does not match binding type") != std::string::npos);
}

TEST_CASE("restrict rejects software numeric types") {
  const std::string source = R"(
[return<int>]
main() {
  [restrict<decimal> i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("software numeric types are not supported yet") != std::string::npos);
}

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

TEST_CASE("binding rejects return transform") {
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
  CHECK(error.find("block arguments are only supported on statement calls") != std::string::npos);
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

TEST_SUITE_BEGIN("primestruct.semantics.bindings.assignments");

TEST_CASE("collections validate") {
  const std::string source = R"(
[return<int>]
main() {
  array<i32>(1i32, 2i32, 3i32)
  map<i32, i32>(1i32, 10i32, 2i32, 20i32)
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array requires one template argument") {
  const std::string source = R"(
[return<int>]
main() {
  array(1i32, 2i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("array literal requires exactly one template argument") != std::string::npos);
}

TEST_CASE("map requires even number of arguments") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>(1i32, 10i32, 2i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map literal requires an even number of arguments") != std::string::npos);
}

TEST_CASE("assign to mutable binding succeeds") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  assign(value, 4i32)
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("assign to immutable binding fails") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  assign(value, 2i32)
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("assign allowed in expression context") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  return(assign(value, 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer binding and dereference assignment validate") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{5i32}
  [Pointer<i32> mut] ptr{location(value)}
  assign(dereference(ptr), 4i32)
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reference participates in arithmetic") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{5i32}
  [Reference<i32> mut] ref{location(value)}
  assign(ref, plus(ref, 2i32))
  return(plus(1i32, ref))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("location accepts reference binding") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{8i32}
  [Reference<i32> mut] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  return(dereference(ptr))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("dereference requires pointer or reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{5i32}
  return(dereference(value))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("dereference requires a pointer or reference") != std::string::npos);
}

TEST_CASE("literal statement validates") {
  const std::string source = R"(
[return<int>]
main() {
  1i32
  return(2i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer assignment requires mutable binding") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{5i32}
  [Pointer<i32>] ptr{location(value)}
  assign(dereference(ptr), 4i32)
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("assign through non-pointer dereference fails") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  assign(dereference(value), 3i32)
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable pointer binding") != std::string::npos);
}

TEST_CASE("reference binding assigns to target") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{5i32}
  [Reference<i32> mut] ref{location(value)}
  assign(ref, 4i32)
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reference assignment requires mutable binding") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{5i32}
  [Reference<i32>] ref{location(value)}
  assign(ref, 4i32)
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("reference binding requires location") {
  const std::string source = R"(
[return<int>]
main() {
  [Reference<i32>] ref{5i32}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("Reference bindings require location") != std::string::npos);
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.semantics.bindings.control_flow");

TEST_CASE("if validates block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  if(true, then(){
    [i32] temp{2i32}
    assign(value, temp)
  }, else(){ assign(value, 3i32) })
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("if rejects float condition") {
  const std::string source = R"(
[return<int>]
main() {
  if(1.5f, then(){ return(1i32) }, else(){ return(2i32) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if condition requires bool") != std::string::npos);
}

TEST_CASE("if expression rejects void blocks") {
  const std::string source = R"(
[return<void>]
noop() {
  return()
}

[return<int>]
main() {
  return(if(true, then(){ noop() }, else(){ 1i32 }))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if branches must produce a value") != std::string::npos);
}

TEST_CASE("if expression rejects mixed string/numeric branches") {
  const std::string source = R"(
main() {
  [string] message{if(true, "hello"utf8, 1i32)}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if branches must return compatible types") != std::string::npos);
}

TEST_CASE("repeat validates block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{0i32}
  repeat(3i32) {
    assign(value, plus(value, 2i32))
  }
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("repeat validates bool count") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{0i32}
  repeat(true) {
    assign(value, 7i32)
  }
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block expression validates and introduces scope") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{block(){
    [i32] inner{1i32}
    plus(inner, 2i32)
  }}
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block expression must end with value") {
  const std::string source = R"(
[return<int>]
main() {
  return(block(){
    [i32] inner{1i32}
  })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block expression must end with an expression") != std::string::npos);
}

TEST_CASE("block expression rejects void tail") {
  const std::string source = R"(
[return<void>]
log() {
  return()
}

[return<int>]
main() {
  return(block(){
    log()
  })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block expression requires a value") != std::string::npos);
}

TEST_CASE("block expression rejects arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(block(1i32){
    1i32
  })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block expression does not accept arguments") != std::string::npos);
}

TEST_CASE("block expression requires body arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(block())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block requires block arguments") != std::string::npos);
}

TEST_CASE("block statement rejects arguments") {
  const std::string source = R"(
[return<int>]
main() {
  block(1i32) { 2i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block does not accept arguments") != std::string::npos);
}

TEST_CASE("block requires body arguments") {
  const std::string source = R"(
[return<int>]
main() {
  block()
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block requires block arguments") != std::string::npos);
}

TEST_CASE("block scope does not leak bindings") {
  const std::string source = R"(
[return<int>]
main() {
  block(){
    [i32] inner{1i32}
    inner
  }
  return(inner)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown identifier") != std::string::npos);
}

TEST_CASE("block bindings infer primitive type from initializer expressions") {
  const std::string source = R"(
namespace i64 {
  [return<i64>]
  inc([i64] self) {
    return(plus(self, 1i64))
  }
}

[return<i64>]
main() {
  return(block(){
    [mut] value{plus(1i64, 2i64)}
    value.inc()
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("repeat rejects float count") {
  const std::string source = R"(
[return<int>]
main() {
  repeat(1.5f) {
  }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("repeat count requires integer or bool") != std::string::npos);
}

TEST_CASE("repeat accepts bool count") {
  const std::string source = R"(
[return<int>]
main() {
  [bool] enabled{true}
  repeat(enabled) {
  }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("repeat rejects string count") {
  const std::string source = R"(
[return<int>]
main() {
  repeat("nope"utf8) {
  }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("repeat count requires integer or bool") != std::string::npos);
}

TEST_SUITE_END();
