#pragma once

TEST_CASE("reflection core type primitives validate") {
  const std::string source = R"(
[struct reflect]
Item() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [string] typeName{meta.type_name<Item>()}
  [string] typeKind{meta.type_kind<Item>()}
  [bool] isStructType{meta.is_struct<Item>()}
  [i32] fieldCount{meta.field_count<Item>()}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("meta path core type primitives validate") {
  const std::string source = R"(
[return<int>]
main() {
  [string] typeName{/meta/type_name<i32>()}
  [string] typeKind{/meta/type_kind<i32>()}
  [bool] isStructType{/meta/is_struct<i32>()}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("field_count rejects non-struct type targets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] fieldCount{meta.field_count<i32>()}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.field_count requires struct type argument: i32") != std::string::npos);
}

TEST_CASE("field_count rejects non-reflect struct targets") {
  const std::string source = R"(
[struct]
Item() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [i32] fieldCount{meta.field_count<Item>()}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.field_count requires reflect-enabled struct type argument: /Item") !=
        std::string::npos);
}

TEST_CASE("field metadata core primitives validate") {
  const std::string source = R"(
[struct reflect]
Item() {
  [i32] x{1i32}
  [private string] label{"a"utf8}
}

[return<int>]
main() {
  [string] firstName{meta.field_name<Item>(0i32)}
  [string] firstType{meta.field_type<Item>(0i32)}
  [string] firstVisibility{meta.field_visibility<Item>(0i32)}
  [string] secondName{/meta/field_name<Item>(1i32)}
  [string] secondType{/meta/field_type<Item>(1i32)}
  [string] secondVisibility{/meta/field_visibility<Item>(1i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("field metadata queries reject non-constant index") {
  const std::string source = R"(
[struct reflect]
Item() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [string] name{meta.field_name<Item>(plus(0i32, 0i32))}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.field_name requires constant integer index argument") != std::string::npos);
}

TEST_CASE("field metadata queries reject out-of-range index") {
  const std::string source = R"(
[struct reflect]
Item() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [string] name{meta.field_name<Item>(1i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.field_name field index out of range for /Item: 1") != std::string::npos);
}

TEST_CASE("field metadata queries reject non-struct type targets") {
  const std::string source = R"(
[return<int>]
main() {
  [string] name{meta.field_name<i32>(0i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.field_name requires struct type argument: i32") != std::string::npos);
}

TEST_CASE("field metadata queries reject non-reflect struct targets") {
  const std::string source = R"(
[struct]
Item() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [string] name{meta.field_name<Item>(0i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.field_name requires reflect-enabled struct type argument: /Item") != std::string::npos);
}

TEST_CASE("has_transform query validates") {
  const std::string source = R"(
[struct reflect]
Item() {
  [i32] x{1i32}
}

[public return<int>]
Helper() {
  return(0i32)
}

[return<int>]
main() {
  [bool] itemHasReflect{meta.has_transform<Item>("reflect"utf8)}
  [bool] itemHasGenerate{meta.has_transform<Item>("generate"utf8)}
  [bool] helperIsPublic{meta.has_transform<Helper>(public)}
  [bool] helperIsCompute{/meta/has_transform<Helper>("compute"utf8)}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("has_transform rejects non-constant transform-name argument") {
  const std::string source = R"(
[struct reflect]
Item() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [bool] has{meta.has_transform<Item>(plus(1i32, 2i32))}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.has_transform requires constant string or identifier argument") != std::string::npos);
}

TEST_CASE("has_transform rejects invalid argument count") {
  const std::string source = R"(
[struct reflect]
Item() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [bool] has{meta.has_transform<Item>()}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.has_transform requires exactly one transform-name argument") != std::string::npos);
}

TEST_CASE("has_trait query validates") {
  const std::string source = R"(
[struct]
Vec2() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[return<Vec2>]
/Vec2/plus([Vec2] left, [Vec2] right) {
  return(Vec2())
}

[return<Vec2>]
/Vec2/multiply([Vec2] left, [Vec2] right) {
  return(Vec2())
}

[return<bool>]
/Vec2/equal([Vec2] left, [Vec2] right) {
  return(true)
}

[return<bool>]
/Vec2/less_than([Vec2] left, [Vec2] right) {
  return(false)
}

[return<i32>]
/Vec2/count([Vec2] value) {
  return(2i32)
}

[return<i32>]
/Vec2/at([Vec2] value, [i32] index) {
  return(0i32)
}

[return<int>]
main() {
  [bool] vecAdditive{meta.has_trait<Vec2>(Additive)}
  [bool] vecMultiplicative{meta.has_trait<Vec2>("Multiplicative"utf8)}
  [bool] vecComparable{meta.has_trait<Vec2>(Comparable)}
  [bool] vecIndexable{meta.has_trait<Vec2, i32>(Indexable)}
  [bool] i32Comparable{meta.has_trait<i32>(Comparable)}
  [bool] i32Additive{/meta/has_trait<i32>(Additive)}
  [bool] arrayIndexable{meta.has_trait<array<i32>, i32>("Indexable"utf8)}
  [bool] stringIndexable{/meta/has_trait<string, i32>(Indexable)}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("has_trait rejects non-constant trait-name argument") {
  const std::string source = R"(
[return<int>]
main() {
  [bool] has{meta.has_trait<i32>(plus(1i32, 2i32))}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.has_trait requires constant string or identifier argument") != std::string::npos);
}

TEST_CASE("has_trait rejects unsupported trait name") {
  const std::string source = R"(
[return<int>]
main() {
  [bool] has{meta.has_trait<i32>(Hash)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.has_trait does not support trait: Hash") != std::string::npos);
}

TEST_CASE("has_trait rejects invalid template argument count") {
  const std::string source = R"(
[return<int>]
main() {
  [bool] has{meta.has_trait<i32>(Indexable)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.has_trait Indexable requires type and element template arguments") !=
        std::string::npos);
}

TEST_CASE("unsupported reflection metadata queries are rejected") {
  struct Scenario {
    const char *label;
    const char *query;
    const char *expected;
  };
  const Scenario scenarios[] = {
      {"method", "meta.missing<Item>()", "unsupported reflection metadata query: meta.missing"},
      {"path", "/meta/missing<Item>()", "unsupported reflection metadata query: /meta/missing"},
  };
  for (const auto &scenario : scenarios) {
    CAPTURE(scenario.label);
    const std::string source =
        std::string("[struct reflect]\n"
                    "Item() {\n"
                    "  [i32] value{1i32}\n"
                    "}\n\n"
                    "[return<int>]\n"
                    "main() {\n"
                    "  [bool] has{") +
        scenario.query + "}\n  return(0i32)\n}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find(scenario.expected) != std::string::npos);
  }
}

TEST_CASE("runtime reflection object queries are rejected") {
  struct Scenario {
    const char *label;
    const char *query;
    const char *expected;
  };
  const Scenario scenarios[] = {
      {"method-object", "meta.object<Item>()",
       "runtime reflection objects/tables are unsupported: meta.object"},
      {"method-table", "meta.table<Item>()",
       "runtime reflection objects/tables are unsupported: meta.table"},
      {"path-object", "/meta/object()",
       "runtime reflection objects/tables are unsupported: /meta/object"},
      {"path-table", "/meta/table()",
       "runtime reflection objects/tables are unsupported: /meta/table"},
  };
  for (const auto &scenario : scenarios) {
    CAPTURE(scenario.label);
    const std::string source =
        std::string("[struct reflect]\n"
                    "Item() {\n"
                    "  [i32] value{1i32}\n"
                    "}\n\n"
                    "[return<int>]\n"
                    "main() {\n  ") +
        scenario.query + "\n  return(0i32)\n}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find(scenario.expected) != std::string::npos);
  }
}

TEST_CASE("struct transform rejects return transform") {
  const std::string source = R"(
[struct, return<int>]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct definitions cannot declare return types") != std::string::npos);
}

TEST_CASE("struct transform rejects return statements") {
  const std::string source = R"(
[struct]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct definitions cannot contain return statements") != std::string::npos);
}

TEST_CASE("placement transforms are rejected") {
  const char *placements[] = {"stack", "heap", "buffer"};
  for (const auto *placement : placements) {
    CAPTURE(placement);
    const std::string source =
        std::string("[") + placement +
        "]\n"
        "main() {\n"
        "  [i32] value{1i32}\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("placement transforms are not supported") != std::string::npos);
  }
}

TEST_CASE("struct definitions allow missing field initializers") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value
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

TEST_CASE("struct definitions allow initialized fields") {
  const std::string source = R"(
[struct]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct fields infer omitted envelopes from struct initializers") {
  const std::string source = R"(
[struct]
Vec3() {
  [i32] x{7i32}
  [return<i32>]
  getX() {
    return(this.x)
  }
}

[struct]
Sphere() {
  [mut] center{Vec3()}
}

[return<i32>]
main() {
  [Sphere] s{Sphere()}
  return(s.center.getX())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct fields reject ambiguous omitted envelopes") {
  const std::string source = R"(
[struct]
Vec2() {
  [i32] x{1i32}
}

[struct]
Vec3() {
  [i32] x{2i32}
}

[struct]
Shape() {
  center{if(true, then() { Vec2() }, else() { Vec3() })}
}

[return<i32>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unresolved or ambiguous omitted struct field envelope: /Shape/center") != std::string::npos);
}

TEST_CASE("recursive struct layouts are rejected") {
  const std::string source = R"(
[struct]
Node() {
  [Node] next{Node()}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("recursive struct layout not supported") != std::string::npos);
}

