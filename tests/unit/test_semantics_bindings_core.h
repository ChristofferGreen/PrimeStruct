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

TEST_CASE("binding rejects effects transform without explicit type") {
  const std::string source = R"(
[return<int>]
main() {
  [effects(io_out)] value{1i32}
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
  const char *placements[] = {"stack", "heap", "buffer"};
  for (const auto *placement : placements) {
    CAPTURE(placement);
    const std::string source =
        std::string("[return<int>]\n") +
        "main() {\n"
        "  [" + placement + " i32] value{1i32}\n"
        "  return(value)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("placement transforms are not supported on bindings") != std::string::npos);
  }
}

TEST_CASE("parameter rejects placement transforms") {
  const char *placements[] = {"stack", "heap", "buffer"};
  for (const auto *placement : placements) {
    CAPTURE(placement);
    const std::string source =
        std::string("[return<int>]\n") +
        "main([" + placement + " i32] value) {\n"
        "  return(value)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("placement transforms are not supported on bindings") != std::string::npos);
  }
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
