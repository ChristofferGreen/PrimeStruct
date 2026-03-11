TEST_CASE("execution effects transform validates") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[effects(io_out)]
task(1i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}


TEST_CASE("execution capabilities transform validates") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[effects(io_out), capabilities(io_out)]
task(1i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution capabilities rejects template arguments") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[capabilities<io>]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capabilities transform does not accept template arguments") != std::string::npos);
}

TEST_CASE("execution capabilities rejects invalid capability") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[capabilities("io"utf8)]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid capability") != std::string::npos);
}

TEST_CASE("execution capabilities rejects duplicate capability") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[capabilities(io_out, io_out)]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate capability") != std::string::npos);
}

TEST_CASE("execution effects rejects template arguments") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[effects<io>]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("effects transform does not accept template arguments") != std::string::npos);
}

TEST_CASE("execution effects rejects invalid capability") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[effects("io"utf8)]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid effects capability") != std::string::npos);
}

TEST_CASE("execution effects rejects duplicate capability") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[effects(io, io)]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate effects capability") != std::string::npos);
}

TEST_CASE("execution effects rejects duplicates") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[effects(io_out), effects(asset_read)]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate effects transform") != std::string::npos);
}

TEST_CASE("execution capabilities rejects duplicates") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[capabilities(io_out), capabilities(asset_read)]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate capabilities transform") != std::string::npos);
}

TEST_CASE("capabilities transform validates identifiers") {
  const std::string source = R"(
[effects(asset_read, gpu_queue), capabilities(asset_read, gpu_queue), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capabilities transform rejects template arguments") {
  const std::string source = R"(
[capabilities<io>, return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capabilities transform does not accept template arguments") != std::string::npos);
}

TEST_CASE("capabilities transform rejects invalid capability") {
  const std::string source = R"(
[capabilities("io"utf8), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid capability") != std::string::npos);
}

TEST_CASE("capabilities transform rejects duplicate capability") {
  const std::string source = R"(
[capabilities(gpu, gpu), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate capability") != std::string::npos);
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.semantics.struct_transforms");

TEST_CASE("align_bytes validates integer argument") {
  const std::string source = R"(
[align_bytes(16), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("align_bytes accepts digit separators") {
  const std::string source = R"(
[align_bytes(1,024), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("align_bytes rejects non-integer argument") {
  const std::string source = R"(
[align_bytes(foo), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("align_bytes requires a positive integer argument") != std::string::npos);
}

TEST_CASE("align_bytes rejects wrong argument count") {
  const std::string source = R"(
[align_bytes(4, 8), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("align_bytes requires exactly one integer argument") != std::string::npos);
}

TEST_CASE("align_kbytes rejects wrong argument count") {
  const std::string source = R"(
[align_kbytes(4, 8), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("align_kbytes requires exactly one integer argument") != std::string::npos);
}

TEST_CASE("align_kbytes rejects template arguments") {
  const std::string source = R"(
[align_kbytes<i32>(4), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("align_kbytes does not accept template arguments") != std::string::npos);
}

TEST_CASE("align_kbytes validates integer argument") {
  const std::string source = R"(
[align_kbytes(4), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("align_kbytes accepts hex literal") {
  const std::string source = R"(
[align_kbytes(0x10), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("align_kbytes rejects non-integer argument") {
  const std::string source = R"(
[align_kbytes(foo), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("align_kbytes requires a positive integer argument") != std::string::npos);
}

TEST_CASE("struct transform validates without args") {
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

TEST_CASE("no_padding transform validates without args") {
  const std::string source = R"(
[no_padding]
Thing() {
  [i32] a{1i32}
  [i32] b{2i32}
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

TEST_CASE("no_padding rejects alignment padding") {
  const std::string source = R"(
[no_padding]
Thing() {
  [i32] a{1i32}
  [i64] b{2i64}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("no_padding disallows alignment padding on field /Thing/b") != std::string::npos);
}

TEST_CASE("platform_independent_padding rejects implicit padding") {
  const std::string source = R"(
[platform_independent_padding]
Thing() {
  [i32] a{1i32}
  [i64] b{2i64}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("platform_independent_padding requires explicit alignment on field /Thing/b") != std::string::npos);
}

TEST_CASE("platform_independent_padding allows explicit alignment") {
  const std::string source = R"(
[platform_independent_padding]
Thing() {
  [i32] a{1i32}
  [align_bytes(8) i64] b{2i64}
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

TEST_CASE("struct alignment rejects smaller field requirement") {
  const std::string source = R"(
[struct]
Thing() {
  [align_bytes(4) i64] value{1i64}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("alignment requirement on field /Thing/value") != std::string::npos);
}

TEST_CASE("struct alignment rejects smaller struct requirement") {
  const std::string source = R"(
[struct align_bytes(4)]
Thing() {
  [i64] value{1i64}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("alignment requirement on struct /Thing") != std::string::npos);
}

TEST_CASE("static fields do not affect struct alignment") {
  const std::string source = R"(
[struct align_bytes(4)]
Thing() {
  [static i64] shared{1i64}
  [i32] value{2i32}
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

TEST_CASE("struct constructors ignore static fields") {
  const std::string source = R"(
[struct]
Thing() {
  [static i32] shared{1i32}
  [i32] value{2i32}
}

[return<int>]
main() {
  Thing(3i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pod transform validates without args") {
  const std::string source = R"(
[pod]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pod transform rejects handle tag") {
  const std::string source = R"(
[pod, handle]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pod definitions cannot be tagged as handle or gpu_lane") != std::string::npos);
}

TEST_CASE("pod transform rejects handle fields") {
  const std::string source = R"(
[pod]
main() {
  [handle<PathNode>] target{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pod definitions cannot contain handle or gpu_lane fields") != std::string::npos);
}

TEST_CASE("handle and gpu_lane tags conflict on definition") {
  const std::string source = R"(
[handle, gpu_lane]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("handle definitions cannot be tagged as gpu_lane") != std::string::npos);
}

TEST_CASE("handle and gpu_lane tags conflict on field") {
  const std::string source = R"(
[struct]
main() {
  [handle<PathNode> gpu_lane] target{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("fields cannot be tagged as handle and gpu_lane") != std::string::npos);
}

TEST_CASE("struct transform rejects template arguments") {
  const std::string source = R"(
[struct<i32>]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct transform does not accept template arguments") != std::string::npos);
}

TEST_CASE("struct transform rejects arguments") {
  const std::string source = R"(
[struct(foo)]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct transform does not accept arguments") != std::string::npos);
}

TEST_CASE("reflection transforms validate on struct definitions") {
  const std::string source = R"(
[struct reflect generate(Equal, DebugPrint)]
main() {
  [i32] value{1i32}
}

[return<int>]
use() {
  [main] item{main()}
  return(item.value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/use", error));
  CHECK(error.empty());
}

TEST_CASE("reflection transforms validate on field-only structs") {
  const std::string source = R"(
[reflect generate(Equal)]
main() {
  [i32] value{1i32}
}

[return<int>]
use() {
  [main] item{main()}
  return(item.value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/use", error));
  CHECK(error.empty());
}

TEST_CASE("reflect transform rejects non-struct definitions") {
  const std::string source = R"(
[reflect return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("reflection transforms are only valid on struct definitions") != std::string::npos);
}

TEST_CASE("generate transform requires reflect") {
  const std::string source = R"(
[struct generate(Equal)]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("generate transform requires reflect") != std::string::npos);
}

TEST_CASE("generate transform rejects unsupported reflection generators") {
  const std::string source = R"(
[struct reflect generate(UnknownHelper)]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported reflection generator on /main: UnknownHelper") != std::string::npos);
}

TEST_CASE("generate transform rejects duplicate reflection generators") {
  const std::string source = R"(
[struct reflect generate(Equal, Equal)]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate reflection generator on /main: Equal") != std::string::npos);
}

TEST_CASE("generate Equal emits reflection helper definition") {
  const std::string source = R"(
[struct reflect generate(Equal)]
Pair() {
  [i32] x{1i32}
  [bool] ok{true}
}

[return<bool>]
main() {
  [Pair] left{Pair()}
  [Pair] right{Pair()}
  return(/Pair/Equal(left, right))
}
)";
  auto program = parseProgram(source);
  primec::Semantics semantics;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults));
  CHECK(error.empty());

  const primec::Definition *generated = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == "/Pair/Equal") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->parameters.size() == 2);
  CHECK(generated->hasReturnStatement);
  REQUIRE(generated->returnExpr.has_value());
  const primec::Expr &returnExpr = *generated->returnExpr;
  REQUIRE(returnExpr.kind == primec::Expr::Kind::Call);
  CHECK(returnExpr.name == "and");
  REQUIRE(returnExpr.args.size() == 2);
  CHECK(returnExpr.args[0].kind == primec::Expr::Kind::Call);
  CHECK(returnExpr.args[1].kind == primec::Expr::Kind::Call);
  CHECK(returnExpr.args[0].name == "equal");
  CHECK(returnExpr.args[1].name == "equal");
}

TEST_CASE("generate Equal for empty reflected struct returns true") {
  const std::string source = R"(
[struct reflect generate(Equal)]
Marker() {
}

[return<bool>]
main() {
  [Marker] left{Marker()}
  [Marker] right{Marker()}
  return(/Marker/Equal(left, right))
}
)";
  auto program = parseProgram(source);
  primec::Semantics semantics;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults));
  CHECK(error.empty());

  const primec::Definition *generated = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == "/Marker/Equal") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->returnExpr.has_value());
  const primec::Expr &returnExpr = *generated->returnExpr;
  REQUIRE(returnExpr.kind == primec::Expr::Kind::BoolLiteral);
  CHECK(returnExpr.boolValue);
}

TEST_CASE("generate Equal ignores static fields") {
  const std::string source = R"(
[struct reflect generate(Equal)]
Marker() {
  [static i32] shared{1i32}
}

[return<bool>]
main() {
  [Marker] left{Marker()}
  [Marker] right{Marker()}
  return(/Marker/Equal(left, right))
}
)";
  auto program = parseProgram(source);
  primec::Semantics semantics;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults));
  CHECK(error.empty());

  const primec::Definition *generated = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == "/Marker/Equal") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->returnExpr.has_value());
  const primec::Expr &returnExpr = *generated->returnExpr;
  REQUIRE(returnExpr.kind == primec::Expr::Kind::BoolLiteral);
  CHECK(returnExpr.boolValue);
}

TEST_CASE("generate Equal rejects existing helper collision") {
  const std::string source = R"(
[struct reflect generate(Equal)]
Pair() {
  [i32] x{1i32}
}

[return<bool>]
/Pair/Equal([Pair] left, [Pair] right) {
  return(false)
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("generated reflection helper already exists: /Pair/Equal") != std::string::npos);
}

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

TEST_CASE("runtime reflection object paths are rejected") {
  const char *runtimeTargets[] = {"/meta/object", "/meta/table"};
  for (const auto *runtimeTarget : runtimeTargets) {
    CAPTURE(runtimeTarget);
    const std::string source =
        std::string("[return<int>]\n"
                    "main() {\n  ") +
        runtimeTarget + "()\n  return(0i32)\n}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("runtime reflection objects/tables are unsupported: " + std::string(runtimeTarget)) !=
          std::string::npos);
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
