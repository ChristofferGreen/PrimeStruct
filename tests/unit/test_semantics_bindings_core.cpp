#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

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

TEST_CASE("binding inference prefers user definition named array") {
  const std::string source = R"(
[return<int>]
array([i32] value) {
  return(value)
}

[return<int>]
main() {
  value{array(5i32)}
  return(plus(value, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding inference keeps builtin array collection type") {
  const std::string source = R"(
[return<int>]
main() {
  value{array<i32>(5i32)}
  return(count(value))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding inference prefers user definition named vector template call") {
  const std::string source = R"(
[return<i32>]
vector<T>([T] value) {
  return(value)
}

[return<int>]
main() {
  value{vector<i32>(1i32)}
  return(plus(value, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding inference keeps builtin vector literal type") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  value{vector<i32>(1i32)}
  return(capacity(value))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector binding accepts explicit mut type with constructor call") {
  const std::string source = R"(
import /std/collections/*

[struct]
Particle {
  [i32] id{0i32}
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Particle> mut] v{vector<Particle>()}
  return(v.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector binding infers mut type from constructor call") {
  const std::string source = R"(
import /std/collections/*

[struct]
Particle {
  [i32] id{0i32}
}

[effects(heap_alloc), return<int>]
main() {
  [mut] v{vector<Particle>()}
  return(v.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector binding accepts omitted explicit initializer") {
  const std::string source = R"(
import /std/collections/*

[struct]
Particle {
  [i32] id{0i32}
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Particle> mut] v{}
  return(v.count())
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
  [integer] value{convert<integer>(1i32)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported convert target type: integer") != std::string::npos);
}

TEST_CASE("software numeric collection bindings are rejected") {
  const std::string source = R"(
[return<int>]
main() {
  [array<decimal>] values{array<decimal>(convert<decimal>(1.0f32))}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_CASE("soa_vector binding validates with soa-safe struct element type") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("soa_vector binding requires struct element type") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [soa_vector<i32>] values{soa_vector<i32>()}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa_vector requires struct element type") != std::string::npos);
}

TEST_CASE("soa_vector binding rejects disallowed element field envelope") {
  const std::string source = R"(
Particle() {
  [string] name{"particle"utf8}
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa_vector field envelope is unsupported on /Particle/name: string") != std::string::npos);
}

TEST_CASE("soa_vector binding rejects nested struct disallowed envelope") {
  const std::string source = R"(
Meta() {
  [string] tag{"meta"utf8}
}

Particle() {
  [Meta] meta{Meta("default"utf8)}
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa_vector field envelope is unsupported on /Particle/meta/tag: string") != std::string::npos);
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

TEST_CASE("float binding accepts math expression with imported constants") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [f32] value{abs(sin(pi))}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit fixed numeric binding rejects incompatible kind") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1.5f32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer type mismatch") != std::string::npos);
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

TEST_CASE("binding rejects return transform with explicit type") {
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
  [restrict<decimal> decimal] value{convert<decimal>(1.0f32)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported convert target type: decimal") != std::string::npos);
}

TEST_CASE("stdlib-owned definitions keep direct stdlib constructor imports visible") {
  const std::string source = R"(
import /std/math/*

namespace std {
  namespace demo {
  [public struct]
  Swatch() {
    [public ColorRGBA] clear{ColorRGBA(0.0f32, 0.0f32, 0.0f32, 1.0f32)}
  }
  }
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();
