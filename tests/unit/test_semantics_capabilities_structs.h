TEST_SUITE_BEGIN("primestruct.semantics.effects");

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

TEST_CASE("struct layout accepts explicit canonical map field") {
  const std::string source = R"(
[struct]
Thing() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [bool] ready{true}
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

TEST_CASE("no_padding keeps canonical map field padding diagnostics") {
  const std::string source = R"(
[no_padding]
Thing() {
  [bool] ready{true}
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("no_padding disallows alignment padding on field /Thing/values") != std::string::npos);
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

TEST_CASE("generate transform reports deferred ToString generator") {
  const std::string source = R"(
[struct reflect generate(ToString)]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("reflection generator ToString is deferred on /main") != std::string::npos);
  CHECK(error.find("use DebugPrint") != std::string::npos);
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

TEST_CASE("generated reflection helpers are public") {
  const std::string source = R"(
[struct reflect generate(Equal, Default)]
Pair() {
  [i32] x{1i32}
}

[return<bool>]
main() {
  [Pair] left{/Pair/Default()}
  [Pair] right{/Pair/Default()}
  return(/Pair/Equal(left, right))
}
)";
  auto program = parseProgram(source);
  primec::Semantics semantics;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults));
  CHECK(error.empty());

  for (const auto *helperPath : {"/Pair/Equal", "/Pair/Default"}) {
    CAPTURE(helperPath);
    const primec::Definition *generated = nullptr;
    for (const auto &def : program.definitions) {
      if (def.fullPath == helperPath) {
        generated = &def;
        break;
      }
    }
    REQUIRE(generated != nullptr);
    bool hasPublic = false;
    for (const auto &transform : generated->transforms) {
      if (transform.name == "public") {
        hasPublic = true;
      }
      CHECK(transform.name != "private");
    }
    CHECK(hasPublic);
  }
}

TEST_CASE("generated reflection helpers are importable via wildcard") {
  const std::string source = R"(
import /Pair/*

[struct reflect generate(Equal, Default)]
Pair() {
  [i32] x{1i32}
}

[return<bool>]
main() {
  return(Equal(Default(), Default()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("generate helper pack emits canonical helper order") {
  const std::string source = R"(
[struct reflect generate(Clone, DebugPrint, IsDefault, Default, NotEqual, Equal)]
Pair() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  auto program = parseProgram(source);
  primec::Semantics semantics;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults));
  CHECK(error.empty());

  std::vector<std::string> generatedHelpers;
  for (const auto &def : program.definitions) {
    if (def.fullPath.rfind("/Pair/", 0) != 0 || def.fullPath == "/Pair") {
      continue;
    }
    generatedHelpers.push_back(def.fullPath);
    const bool hasPublicVisibility =
        std::any_of(def.transforms.begin(), def.transforms.end(), [](const primec::Transform &transform) {
          return transform.name == "public";
        });
    CHECK(hasPublicVisibility);
  }

  const std::vector<std::string> expected = {"/Pair/Equal",
                                             "/Pair/NotEqual",
                                             "/Pair/Default",
                                             "/Pair/IsDefault",
                                             "/Pair/Clone",
                                             "/Pair/DebugPrint"};
  CHECK(generatedHelpers == expected);
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

TEST_CASE("generate NotEqual emits reflection helper definition") {
  const std::string source = R"(
[struct reflect generate(NotEqual)]
Pair() {
  [i32] x{1i32}
  [bool] ok{true}
}

[return<bool>]
main() {
  [Pair] left{Pair()}
  [Pair] right{Pair()}
  return(/Pair/NotEqual(left, right))
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
    if (def.fullPath == "/Pair/NotEqual") {
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
  CHECK(returnExpr.name == "or");
  REQUIRE(returnExpr.args.size() == 2);
  CHECK(returnExpr.args[0].kind == primec::Expr::Kind::Call);
  CHECK(returnExpr.args[1].kind == primec::Expr::Kind::Call);
  CHECK(returnExpr.args[0].name == "not_equal");
  CHECK(returnExpr.args[1].name == "not_equal");
}

TEST_CASE("generate NotEqual for empty reflected struct returns false") {
  const std::string source = R"(
[struct reflect generate(NotEqual)]
Marker() {
}

[return<bool>]
main() {
  [Marker] left{Marker()}
  [Marker] right{Marker()}
  return(/Marker/NotEqual(left, right))
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
    if (def.fullPath == "/Marker/NotEqual") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->returnExpr.has_value());
  const primec::Expr &returnExpr = *generated->returnExpr;
  REQUIRE(returnExpr.kind == primec::Expr::Kind::BoolLiteral);
  CHECK_FALSE(returnExpr.boolValue);
}

TEST_CASE("generate NotEqual ignores static fields") {
  const std::string source = R"(
[struct reflect generate(NotEqual)]
Marker() {
  [static i32] shared{1i32}
}

[return<bool>]
main() {
  [Marker] left{Marker()}
  [Marker] right{Marker()}
  return(/Marker/NotEqual(left, right))
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
    if (def.fullPath == "/Marker/NotEqual") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->returnExpr.has_value());
  const primec::Expr &returnExpr = *generated->returnExpr;
  REQUIRE(returnExpr.kind == primec::Expr::Kind::BoolLiteral);
  CHECK_FALSE(returnExpr.boolValue);
}

TEST_CASE("generate NotEqual rejects existing helper collision") {
  const std::string source = R"(
[struct reflect generate(NotEqual)]
Pair() {
  [i32] x{1i32}
}

[return<bool>]
/Pair/NotEqual([Pair] left, [Pair] right) {
  return(false)
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("generated reflection helper already exists: /Pair/NotEqual") != std::string::npos);
}

TEST_CASE("generate Default emits reflection helper definition") {
  const std::string source = R"(
[struct reflect generate(Default)]
Pair() {
  [i32] x{7i32}
  [bool] ok{true}
}

[return<int>]
main() {
  [Pair] value{/Pair/Default()}
  return(value.x)
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
    if (def.fullPath == "/Pair/Default") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  CHECK(generated->parameters.empty());
  CHECK(generated->hasReturnStatement);
  REQUIRE(generated->returnExpr.has_value());
  const primec::Expr &returnExpr = *generated->returnExpr;
  REQUIRE(returnExpr.kind == primec::Expr::Kind::Call);
  CHECK(returnExpr.name == "/Pair");
}

TEST_CASE("generate Default supports static-only structs") {
  const std::string source = R"(
[struct reflect generate(Default)]
Marker() {
  [static i32] shared{1i32}
}

[return<int>]
main() {
  [Marker] value{/Marker/Default()}
  return(0i32)
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
    if (def.fullPath == "/Marker/Default") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->returnExpr.has_value());
  const primec::Expr &returnExpr = *generated->returnExpr;
  REQUIRE(returnExpr.kind == primec::Expr::Kind::Call);
  CHECK(returnExpr.name == "/Marker");
}

TEST_CASE("generate Default rejects existing helper collision") {
  const std::string source = R"(
[struct reflect generate(Default)]
Pair() {
  [i32] x{1i32}
}

[return<Pair>]
/Pair/Default() {
  return(Pair())
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("generated reflection helper already exists: /Pair/Default") != std::string::npos);
}

TEST_CASE("generate IsDefault emits reflection helper definition") {
  const std::string source = R"(
[struct reflect generate(IsDefault)]
Pair() {
  [i32] x{7i32}
  [bool] ok{true}
}

[return<bool>]
main() {
  [Pair] value{Pair()}
  return(/Pair/IsDefault(value))
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
    if (def.fullPath == "/Pair/IsDefault") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->parameters.size() == 1);
  CHECK(generated->hasReturnStatement);
  REQUIRE(generated->statements.size() == 1);
  CHECK(generated->statements.front().isBinding);
  CHECK(generated->statements.front().name == "defaultValue");
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

TEST_CASE("generate IsDefault supports static-only structs") {
  const std::string source = R"(
[struct reflect generate(IsDefault)]
Marker() {
  [static i32] shared{1i32}
}

[return<bool>]
main() {
  [Marker] value{Marker()}
  return(/Marker/IsDefault(value))
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
    if (def.fullPath == "/Marker/IsDefault") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  CHECK(generated->statements.empty());
  REQUIRE(generated->returnExpr.has_value());
  const primec::Expr &returnExpr = *generated->returnExpr;
  REQUIRE(returnExpr.kind == primec::Expr::Kind::BoolLiteral);
  CHECK(returnExpr.boolValue);
}

TEST_CASE("generate IsDefault rejects existing helper collision") {
  const std::string source = R"(
[struct reflect generate(IsDefault)]
Pair() {
  [i32] x{1i32}
}

[return<bool>]
/Pair/IsDefault([Pair] value) {
  return(false)
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("generated reflection helper already exists: /Pair/IsDefault") != std::string::npos);
}

TEST_CASE("generate Clone emits reflection helper definition") {
  const std::string source = R"(
[struct reflect generate(Clone)]
Pair() {
  [i32] x{7i32}
  [bool] ok{true}
}

[return<int>]
main() {
  [Pair] value{Pair()}
  [Pair] copy{/Pair/Clone(value)}
  return(copy.x)
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
    if (def.fullPath == "/Pair/Clone") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->parameters.size() == 1);
  CHECK(generated->hasReturnStatement);
  REQUIRE(generated->returnExpr.has_value());
  const primec::Expr &returnExpr = *generated->returnExpr;
  REQUIRE(returnExpr.kind == primec::Expr::Kind::Call);
  CHECK(returnExpr.name == "/Pair");
  REQUIRE(returnExpr.args.size() == 2);
  CHECK(returnExpr.args[0].kind == primec::Expr::Kind::Call);
  CHECK(returnExpr.args[1].kind == primec::Expr::Kind::Call);
  CHECK(returnExpr.args[0].isFieldAccess);
  CHECK(returnExpr.args[1].isFieldAccess);
}

TEST_CASE("generate Clone supports static-only structs") {
  const std::string source = R"(
[struct reflect generate(Clone)]
Marker() {
  [static i32] shared{1i32}
}

[return<int>]
main() {
  [Marker] value{Marker()}
  [Marker] copy{/Marker/Clone(value)}
  return(0i32)
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
    if (def.fullPath == "/Marker/Clone") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->returnExpr.has_value());
  const primec::Expr &returnExpr = *generated->returnExpr;
  REQUIRE(returnExpr.kind == primec::Expr::Kind::Call);
  CHECK(returnExpr.name == "/Marker");
  CHECK(returnExpr.args.empty());
}

TEST_CASE("generate Clone rejects existing helper collision") {
  const std::string source = R"(
[struct reflect generate(Clone)]
Pair() {
  [i32] x{1i32}
}

[return<Pair>]
/Pair/Clone([Pair] value) {
  return(value)
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("generated reflection helper already exists: /Pair/Clone") != std::string::npos);
}

TEST_CASE("generate DebugPrint emits reflection helper definition") {
  const std::string source = R"(
[struct reflect generate(DebugPrint)]
Pair() {
  [i32] x{7i32}
  [bool] ok{true}
}

[return<int>]
main() {
  [Pair] value{Pair()}
  /Pair/DebugPrint(value)
  return(0i32)
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
    if (def.fullPath == "/Pair/DebugPrint") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->parameters.size() == 1);
  bool hasPublic = false;
  bool hasVoidReturn = false;
  for (const auto &transform : generated->transforms) {
    if (transform.name == "public") {
      hasPublic = true;
    }
    if (transform.name == "return" && transform.templateArgs.size() == 1 &&
        transform.templateArgs.front() == "void") {
      hasVoidReturn = true;
    }
  }
  CHECK(hasPublic);
  CHECK(hasVoidReturn);
  CHECK_FALSE(generated->hasReturnStatement);
  REQUIRE(generated->statements.size() == 4);
  for (const auto &stmt : generated->statements) {
    REQUIRE(stmt.kind == primec::Expr::Kind::Call);
    CHECK(stmt.name == "print_line");
    REQUIRE(stmt.args.size() == 1);
    CHECK(stmt.args.front().kind == primec::Expr::Kind::StringLiteral);
  }
}

TEST_CASE("generate DebugPrint supports static-only structs") {
  const std::string source = R"(
[struct reflect generate(DebugPrint)]
Marker() {
  [static i32] shared{1i32}
}

[return<int>]
main() {
  [Marker] value{Marker()}
  /Marker/DebugPrint(value)
  return(0i32)
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
    if (def.fullPath == "/Marker/DebugPrint") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->statements.size() == 1);
  REQUIRE(generated->statements.front().kind == primec::Expr::Kind::Call);
  CHECK(generated->statements.front().name == "print_line");
}

TEST_CASE("generate DebugPrint accepts non-printable field types") {
  const std::string source = R"(
[struct]
Nested() {
  [i32] id{1i32}
}

[struct reflect generate(DebugPrint)]
Container() {
  [Nested] nested{Nested()}
}

[return<int>]
main() {
  [Container] value{Container()}
  /Container/DebugPrint(value)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("generate DebugPrint rejects existing helper collision") {
  const std::string source = R"(
[struct reflect generate(DebugPrint)]
Pair() {
  [i32] x{1i32}
}

[return<void>]
/Pair/DebugPrint([Pair] value) {
  print_line("custom"utf8)
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("generated reflection helper already exists: /Pair/DebugPrint") != std::string::npos);
}

TEST_CASE("generate Compare emits reflection helper definition") {
  const std::string source = R"(
[struct reflect generate(Compare)]
Pair() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<int>]
main() {
  [Pair] left{Pair([x] 1i32, [y] 2i32)}
  [Pair] right{Pair([x] 1i32, [y] 3i32)}
  return(/Pair/Compare(left, right))
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
    if (def.fullPath == "/Pair/Compare") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->parameters.size() == 2);
  bool hasI32Return = false;
  for (const auto &transform : generated->transforms) {
    if (transform.name == "return" && transform.templateArgs.size() == 1 &&
        transform.templateArgs.front() == "i32") {
      hasI32Return = true;
    }
  }
  CHECK(hasI32Return);
  CHECK(generated->hasReturnStatement);
  REQUIRE(generated->returnExpr.has_value());
  CHECK(generated->returnExpr->kind == primec::Expr::Kind::Literal);
  CHECK(generated->returnExpr->literalValue == 0);
  REQUIRE(generated->statements.size() == 4);
  REQUIRE(generated->statements[0].kind == primec::Expr::Kind::Call);
  REQUIRE(generated->statements[0].args.size() == 3);
  REQUIRE(generated->statements[0].args[0].kind == primec::Expr::Kind::Call);
  CHECK(generated->statements[0].args[0].name == "less_than");
  REQUIRE(generated->statements[1].kind == primec::Expr::Kind::Call);
  REQUIRE(generated->statements[1].args.size() == 3);
  REQUIRE(generated->statements[1].args[0].kind == primec::Expr::Kind::Call);
  CHECK(generated->statements[1].args[0].name == "greater_than");
}

TEST_CASE("generate Compare for empty reflected struct returns zero") {
  const std::string source = R"(
[struct reflect generate(Compare)]
Marker() {
}

[return<int>]
main() {
  [Marker] left{Marker()}
  [Marker] right{Marker()}
  return(/Marker/Compare(left, right))
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
    if (def.fullPath == "/Marker/Compare") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  CHECK(generated->statements.empty());
  REQUIRE(generated->returnExpr.has_value());
  CHECK(generated->returnExpr->kind == primec::Expr::Kind::Literal);
  CHECK(generated->returnExpr->literalValue == 0);
}

TEST_CASE("generate Compare ignores static fields") {
  const std::string source = R"(
[struct reflect generate(Compare)]
Marker() {
  [static i32] shared{1i32}
}

[return<int>]
main() {
  [Marker] left{Marker()}
  [Marker] right{Marker()}
  return(/Marker/Compare(left, right))
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
    if (def.fullPath == "/Marker/Compare") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  CHECK(generated->statements.empty());
  REQUIRE(generated->returnExpr.has_value());
  CHECK(generated->returnExpr->kind == primec::Expr::Kind::Literal);
  CHECK(generated->returnExpr->literalValue == 0);
}

TEST_CASE("generate Compare rejects existing helper collision") {
  const std::string source = R"(
[struct reflect generate(Compare)]
Pair() {
  [i32] x{1i32}
}

[return<i32>]
/Pair/Compare([Pair] left, [Pair] right) {
  return(0i32)
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("generated reflection helper already exists: /Pair/Compare") != std::string::npos);
}

TEST_CASE("generate Hash64 emits reflection helper definition") {
  const std::string source = R"(
[struct reflect generate(Hash64)]
Pair() {
  [i32] x{1i32}
  [bool] ok{true}
}

[return<int>]
main() {
  [Pair] value{Pair([x] 2i32, [ok] true)}
  [u64] hash{/Pair/Hash64(value)}
  return(if(greater_than(hash, 0u64), then() { 0i32 }, else() { 1i32 }))
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
    if (def.fullPath == "/Pair/Hash64") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->parameters.size() == 1);
  bool hasU64Return = false;
  for (const auto &transform : generated->transforms) {
    if (transform.name == "return" && transform.templateArgs.size() == 1 &&
        transform.templateArgs.front() == "u64") {
      hasU64Return = true;
    }
  }
  CHECK(hasU64Return);
  CHECK(generated->hasReturnStatement);
  REQUIRE(generated->returnExpr.has_value());

  const auto assertConvertField = [](const primec::Expr &expr, const std::string &expectedField) {
    REQUIRE(expr.kind == primec::Expr::Kind::Call);
    CHECK(expr.name == "convert");
    REQUIRE(expr.templateArgs.size() == 1);
    CHECK(expr.templateArgs.front() == "u64");
    REQUIRE(expr.args.size() == 1);
    REQUIRE(expr.args.front().kind == primec::Expr::Kind::Call);
    CHECK(expr.args.front().isFieldAccess);
    CHECK(expr.args.front().name == expectedField);
  };

  const primec::Expr &outerPlus = *generated->returnExpr;
  REQUIRE(outerPlus.kind == primec::Expr::Kind::Call);
  CHECK(outerPlus.name == "plus");
  REQUIRE(outerPlus.args.size() == 2);

  const primec::Expr &outerMultiply = outerPlus.args[0];
  REQUIRE(outerMultiply.kind == primec::Expr::Kind::Call);
  CHECK(outerMultiply.name == "multiply");
  REQUIRE(outerMultiply.args.size() == 2);

  const primec::Expr &innerPlus = outerMultiply.args[0];
  REQUIRE(innerPlus.kind == primec::Expr::Kind::Call);
  CHECK(innerPlus.name == "plus");
  REQUIRE(innerPlus.args.size() == 2);

  const primec::Expr &seedMultiply = innerPlus.args[0];
  REQUIRE(seedMultiply.kind == primec::Expr::Kind::Call);
  CHECK(seedMultiply.name == "multiply");
  REQUIRE(seedMultiply.args.size() == 2);
  REQUIRE(seedMultiply.args[0].kind == primec::Expr::Kind::Literal);
  CHECK(seedMultiply.args[0].isUnsigned);
  CHECK(seedMultiply.args[0].intWidth == 64);
  CHECK(seedMultiply.args[0].literalValue == 1469598103934665603ULL);
  REQUIRE(seedMultiply.args[1].kind == primec::Expr::Kind::Literal);
  CHECK(seedMultiply.args[1].isUnsigned);
  CHECK(seedMultiply.args[1].intWidth == 64);
  CHECK(seedMultiply.args[1].literalValue == 1099511628211ULL);

  REQUIRE(outerMultiply.args[1].kind == primec::Expr::Kind::Literal);
  CHECK(outerMultiply.args[1].isUnsigned);
  CHECK(outerMultiply.args[1].intWidth == 64);
  CHECK(outerMultiply.args[1].literalValue == 1099511628211ULL);

  assertConvertField(innerPlus.args[1], "x");
  assertConvertField(outerPlus.args[1], "ok");
}

TEST_CASE("generate Hash64 for empty reflected struct returns seed literal") {
  const std::string source = R"(
[struct reflect generate(Hash64)]
Marker() {
}

[return<int>]
main() {
  [Marker] value{Marker()}
  [u64] hash{/Marker/Hash64(value)}
  return(if(greater_than(hash, 0u64), then() { 0i32 }, else() { 1i32 }))
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
    if (def.fullPath == "/Marker/Hash64") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  CHECK(generated->statements.empty());
  REQUIRE(generated->returnExpr.has_value());
  CHECK(generated->returnExpr->kind == primec::Expr::Kind::Literal);
  CHECK(generated->returnExpr->isUnsigned);
  CHECK(generated->returnExpr->intWidth == 64);
  CHECK(generated->returnExpr->literalValue == 1469598103934665603ULL);
}

TEST_CASE("generate Hash64 ignores static fields") {
  const std::string source = R"(
[struct reflect generate(Hash64)]
Marker() {
  [static i32] shared{1i32}
}

[return<int>]
main() {
  [Marker] value{Marker()}
  [u64] hash{/Marker/Hash64(value)}
  return(if(greater_than(hash, 0u64), then() { 0i32 }, else() { 1i32 }))
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
    if (def.fullPath == "/Marker/Hash64") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  CHECK(generated->statements.empty());
  REQUIRE(generated->returnExpr.has_value());
  CHECK(generated->returnExpr->kind == primec::Expr::Kind::Literal);
  CHECK(generated->returnExpr->isUnsigned);
  CHECK(generated->returnExpr->intWidth == 64);
  CHECK(generated->returnExpr->literalValue == 1469598103934665603ULL);
}

TEST_CASE("generate Compare and Hash64 helpers keep canonical v1.1 order") {
  const std::string source = R"(
[struct reflect generate(Hash64, Compare)]
Pair() {
  [i32] x{1i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  auto program = parseProgram(source);
  primec::Semantics semantics;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults));
  CHECK(error.empty());

  std::vector<std::string> generatedHelpers;
  for (const auto &def : program.definitions) {
    if (def.fullPath.rfind("/Pair/", 0) != 0 || def.fullPath == "/Pair") {
      continue;
    }
    generatedHelpers.push_back(def.fullPath);
  }
  const std::vector<std::string> expected = {"/Pair/Compare", "/Pair/Hash64"};
  CHECK(generatedHelpers == expected);
}

TEST_CASE("generate Hash64 rejects existing helper collision") {
  const std::string source = R"(
[struct reflect generate(Hash64)]
Pair() {
  [i32] x{1i32}
}

[return<u64>]
/Pair/Hash64([Pair] value) {
  return(0u64)
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("generated reflection helper already exists: /Pair/Hash64") != std::string::npos);
}

TEST_CASE("generate Compare rejects unsupported field envelope deterministically") {
  const std::string source = R"(
[struct reflect generate(Compare)]
Pair() {
  [array<i32>] values{array<i32>(1i32)}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("generated reflection helper /Pair/Compare does not support field envelope: values (array<i32>)") !=
        std::string::npos);
}

TEST_CASE("generate Hash64 rejects unsupported field envelope deterministically") {
  const std::string source = R"(
[struct reflect generate(Hash64)]
Pair() {
  [string] label{"x"utf8}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("generated reflection helper /Pair/Hash64 does not support field envelope: label (string)") !=
        std::string::npos);
}

TEST_CASE("generate Compare unsupported field diagnostics keep source order") {
  const std::string source = R"(
[struct reflect generate(Compare)]
Pair() {
  [array<i32>] first{array<i32>(1i32)}
  [vector<i32>] second{vector<i32>(2i32)}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("generated reflection helper /Pair/Compare does not support field envelope: first (array<i32>)") !=
        std::string::npos);
}

TEST_CASE("generate Clear emits reflection helper definition") {
  const std::string source = R"(
[struct reflect generate(Clear)]
Pair() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<int>]
main() {
  [Pair mut] value{Pair([x] 9i32, [y] 8i32)}
  /Pair/Clear(value)
  return(0i32)
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
    if (def.fullPath == "/Pair/Clear") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->parameters.size() == 1);
  const primec::Expr &valueParam = generated->parameters.front();
  CHECK(valueParam.name == "value");
  const bool hasPairType = std::any_of(valueParam.transforms.begin(),
                                       valueParam.transforms.end(),
                                       [](const primec::Transform &transform) {
                                         return transform.name == "/Pair";
                                       });
  const bool hasMut = std::any_of(valueParam.transforms.begin(),
                                  valueParam.transforms.end(),
                                  [](const primec::Transform &transform) {
                                    return transform.name == "mut";
                                  });
  CHECK(hasPairType);
  CHECK(hasMut);

  bool hasVoidReturn = false;
  for (const auto &transform : generated->transforms) {
    if (transform.name == "return" && transform.templateArgs.size() == 1 &&
        transform.templateArgs.front() == "void") {
      hasVoidReturn = true;
    }
  }
  CHECK(hasVoidReturn);
  CHECK_FALSE(generated->hasReturnStatement);
  CHECK_FALSE(generated->returnExpr.has_value());

  REQUIRE(generated->statements.size() == 3);
  const primec::Expr &defaultValueBinding = generated->statements[0];
  CHECK(defaultValueBinding.isBinding);
  CHECK(defaultValueBinding.name == "defaultValue");
  REQUIRE(defaultValueBinding.args.size() == 1);
  REQUIRE(defaultValueBinding.args.front().kind == primec::Expr::Kind::Call);
  CHECK(defaultValueBinding.args.front().name == "/Pair");

  const auto assertAssignField = [](const primec::Expr &assignExpr, const std::string &fieldName) {
    REQUIRE(assignExpr.kind == primec::Expr::Kind::Call);
    CHECK(assignExpr.name == "assign");
    REQUIRE(assignExpr.args.size() == 2);

    const primec::Expr &target = assignExpr.args[0];
    REQUIRE(target.kind == primec::Expr::Kind::Call);
    CHECK(target.isFieldAccess);
    CHECK(target.name == fieldName);
    REQUIRE(target.args.size() == 1);
    REQUIRE(target.args.front().kind == primec::Expr::Kind::Name);
    CHECK(target.args.front().name == "value");

    const primec::Expr &source = assignExpr.args[1];
    REQUIRE(source.kind == primec::Expr::Kind::Call);
    CHECK(source.isFieldAccess);
    CHECK(source.name == fieldName);
    REQUIRE(source.args.size() == 1);
    REQUIRE(source.args.front().kind == primec::Expr::Kind::Name);
    CHECK(source.args.front().name == "defaultValue");
  };

  assertAssignField(generated->statements[1], "x");
  assertAssignField(generated->statements[2], "y");
}

TEST_CASE("generate Clear for empty reflected struct emits no-op helper") {
  const std::string source = R"(
[struct reflect generate(Clear)]
Marker() {
}

[return<int>]
main() {
  [Marker mut] value{Marker()}
  /Marker/Clear(value)
  return(0i32)
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
    if (def.fullPath == "/Marker/Clear") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  CHECK(generated->statements.empty());
  CHECK_FALSE(generated->hasReturnStatement);
  CHECK_FALSE(generated->returnExpr.has_value());
}

TEST_CASE("generate Clear ignores static fields") {
  const std::string source = R"(
[struct reflect generate(Clear)]
Marker() {
  [static i32] shared{1i32}
}

[return<int>]
main() {
  [Marker mut] value{Marker()}
  /Marker/Clear(value)
  return(0i32)
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
    if (def.fullPath == "/Marker/Clear") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  CHECK(generated->statements.empty());
}

TEST_CASE("generate Compare Hash64 Clear helpers keep canonical v1.1 order") {
  const std::string source = R"(
[struct reflect generate(Clear, Hash64, Compare)]
Pair() {
  [i32] x{1i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  auto program = parseProgram(source);
  primec::Semantics semantics;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults));
  CHECK(error.empty());

  std::vector<std::string> generatedHelpers;
  for (const auto &def : program.definitions) {
    if (def.fullPath.rfind("/Pair/", 0) != 0 || def.fullPath == "/Pair") {
      continue;
    }
    generatedHelpers.push_back(def.fullPath);
  }
  const std::vector<std::string> expected = {"/Pair/Compare", "/Pair/Hash64", "/Pair/Clear"};
  CHECK(generatedHelpers == expected);
}

TEST_CASE("generate Clear rejects existing helper collision") {
  const std::string source = R"(
[struct reflect generate(Clear)]
Pair() {
  [i32] x{1i32}
}

[return<void>]
/Pair/Clear([Pair mut] value) {
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("generated reflection helper already exists: /Pair/Clear") != std::string::npos);
}

TEST_CASE("generate CopyFrom emits reflection helper definition") {
  const std::string source = R"(
[struct reflect generate(CopyFrom)]
Pair() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<int>]
main() {
  [Pair mut] target{Pair([x] 9i32, [y] 8i32)}
  [Pair] source{Pair([x] 3i32, [y] 5i32)}
  /Pair/CopyFrom(target, source)
  return(0i32)
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
    if (def.fullPath == "/Pair/CopyFrom") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->parameters.size() == 2);
  const primec::Expr &valueParam = generated->parameters[0];
  CHECK(valueParam.name == "value");
  const bool valueHasPairType = std::any_of(valueParam.transforms.begin(),
                                            valueParam.transforms.end(),
                                            [](const primec::Transform &transform) {
                                              return transform.name == "/Pair";
                                            });
  const bool valueHasMut = std::any_of(valueParam.transforms.begin(),
                                       valueParam.transforms.end(),
                                       [](const primec::Transform &transform) {
                                         return transform.name == "mut";
                                       });
  CHECK(valueHasPairType);
  CHECK(valueHasMut);

  const primec::Expr &otherParam = generated->parameters[1];
  CHECK(otherParam.name == "other");
  const bool otherHasPairType = std::any_of(otherParam.transforms.begin(),
                                            otherParam.transforms.end(),
                                            [](const primec::Transform &transform) {
                                              return transform.name == "/Pair";
                                            });
  const bool otherHasMut = std::any_of(otherParam.transforms.begin(),
                                       otherParam.transforms.end(),
                                       [](const primec::Transform &transform) {
                                         return transform.name == "mut";
                                       });
  CHECK(otherHasPairType);
  CHECK_FALSE(otherHasMut);

  bool hasVoidReturn = false;
  for (const auto &transform : generated->transforms) {
    if (transform.name == "return" && transform.templateArgs.size() == 1 &&
        transform.templateArgs.front() == "void") {
      hasVoidReturn = true;
    }
  }
  CHECK(hasVoidReturn);
  CHECK_FALSE(generated->hasReturnStatement);
  CHECK_FALSE(generated->returnExpr.has_value());

  REQUIRE(generated->statements.size() == 2);
  const auto assertAssignField = [](const primec::Expr &assignExpr, const std::string &fieldName) {
    REQUIRE(assignExpr.kind == primec::Expr::Kind::Call);
    CHECK(assignExpr.name == "assign");
    REQUIRE(assignExpr.args.size() == 2);

    const primec::Expr &target = assignExpr.args[0];
    REQUIRE(target.kind == primec::Expr::Kind::Call);
    CHECK(target.isFieldAccess);
    CHECK(target.name == fieldName);
    REQUIRE(target.args.size() == 1);
    REQUIRE(target.args.front().kind == primec::Expr::Kind::Name);
    CHECK(target.args.front().name == "value");

    const primec::Expr &source = assignExpr.args[1];
    REQUIRE(source.kind == primec::Expr::Kind::Call);
    CHECK(source.isFieldAccess);
    CHECK(source.name == fieldName);
    REQUIRE(source.args.size() == 1);
    REQUIRE(source.args.front().kind == primec::Expr::Kind::Name);
    CHECK(source.args.front().name == "other");
  };
  assertAssignField(generated->statements[0], "x");
  assertAssignField(generated->statements[1], "y");
}

TEST_CASE("generate CopyFrom for empty reflected struct emits no-op helper") {
  const std::string source = R"(
[struct reflect generate(CopyFrom)]
Marker() {
}

[return<int>]
main() {
  [Marker mut] target{Marker()}
  [Marker] source{Marker()}
  /Marker/CopyFrom(target, source)
  return(0i32)
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
    if (def.fullPath == "/Marker/CopyFrom") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  CHECK(generated->statements.empty());
  CHECK_FALSE(generated->hasReturnStatement);
  CHECK_FALSE(generated->returnExpr.has_value());
}

TEST_CASE("generate CopyFrom ignores static fields") {
  const std::string source = R"(
[struct reflect generate(CopyFrom)]
Marker() {
  [static i32] shared{1i32}
}

[return<int>]
main() {
  [Marker mut] target{Marker()}
  [Marker] source{Marker()}
  /Marker/CopyFrom(target, source)
  return(0i32)
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
    if (def.fullPath == "/Marker/CopyFrom") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  CHECK(generated->statements.empty());
}

TEST_CASE("generate Compare Hash64 Clear CopyFrom helpers keep canonical v1.1 order") {
  const std::string source = R"(
[struct reflect generate(CopyFrom, Clear, Hash64, Compare)]
Pair() {
  [i32] x{1i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  auto program = parseProgram(source);
  primec::Semantics semantics;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults));
  CHECK(error.empty());

  std::vector<std::string> generatedHelpers;
  for (const auto &def : program.definitions) {
    if (def.fullPath.rfind("/Pair/", 0) != 0 || def.fullPath == "/Pair") {
      continue;
    }
    generatedHelpers.push_back(def.fullPath);
  }
  const std::vector<std::string> expected = {"/Pair/Compare", "/Pair/Hash64", "/Pair/Clear", "/Pair/CopyFrom"};
  CHECK(generatedHelpers == expected);
}

TEST_CASE("generate CopyFrom rejects existing helper collision") {
  const std::string source = R"(
[struct reflect generate(CopyFrom)]
Pair() {
  [i32] x{1i32}
}

[return<void>]
/Pair/CopyFrom([Pair mut] value, [Pair] other) {
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("generated reflection helper already exists: /Pair/CopyFrom") != std::string::npos);
}

TEST_CASE("generate Validate emits reflection helper definition with field hooks") {
  const std::string source = R"(
[struct reflect generate(Validate)]
Pair() {
  [i32] x{1i32}
  [bool] ok{true}
}

[return<int>]
main() {
  [Pair] value{Pair([x] 2i32, [ok] true)}
  /Pair/Validate(value)
  return(0i32)
}
)";
  auto program = parseProgram(source);
  primec::Semantics semantics;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults));
  CHECK(error.empty());

  const primec::Definition *validateHelper = nullptr;
  const primec::Definition *xHook = nullptr;
  const primec::Definition *okHook = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == "/Pair/Validate") {
      validateHelper = &def;
    } else if (def.fullPath == "/Pair/ValidateField_x") {
      xHook = &def;
    } else if (def.fullPath == "/Pair/ValidateField_ok") {
      okHook = &def;
    }
  }

  REQUIRE(validateHelper != nullptr);
  REQUIRE(xHook != nullptr);
  REQUIRE(okHook != nullptr);

  REQUIRE(validateHelper->parameters.size() == 1);
  const primec::Expr &valueParam = validateHelper->parameters.front();
  CHECK(valueParam.name == "value");
  const bool hasPairType = std::any_of(valueParam.transforms.begin(),
                                       valueParam.transforms.end(),
                                       [](const primec::Transform &transform) {
                                         return transform.name == "/Pair";
                                       });
  const bool hasMut = std::any_of(valueParam.transforms.begin(),
                                  valueParam.transforms.end(),
                                  [](const primec::Transform &transform) {
                                    return transform.name == "mut";
                                  });
  CHECK(hasPairType);
  CHECK_FALSE(hasMut);

  bool hasResultReturn = false;
  for (const auto &transform : validateHelper->transforms) {
    if (transform.name == "return" && transform.templateArgs.size() == 1 &&
        transform.templateArgs.front() == "Result<FileError>") {
      hasResultReturn = true;
    }
  }
  CHECK(hasResultReturn);
  CHECK(validateHelper->hasReturnStatement);
  REQUIRE(validateHelper->returnExpr.has_value());
  CHECK(validateHelper->returnExpr->kind == primec::Expr::Kind::Call);
  CHECK(validateHelper->returnExpr->isMethodCall);
  CHECK(validateHelper->returnExpr->name == "ok");
  REQUIRE(validateHelper->returnExpr->args.size() == 1);
  CHECK(validateHelper->returnExpr->args.front().kind == primec::Expr::Kind::Name);
  CHECK(validateHelper->returnExpr->args.front().name == "Result");

  REQUIRE(validateHelper->statements.size() == 2);
  const auto assertValidateGuard = [](const primec::Expr &guardStmt,
                                      const std::string &expectedHookPath,
                                      uint64_t expectedCode) {
    REQUIRE(guardStmt.kind == primec::Expr::Kind::Call);
    CHECK(guardStmt.name == "if");
    REQUIRE(guardStmt.args.size() == 3);

    const primec::Expr &conditionExpr = guardStmt.args[0];
    REQUIRE(conditionExpr.kind == primec::Expr::Kind::Call);
    CHECK(conditionExpr.name == "not");
    REQUIRE(conditionExpr.args.size() == 1);
    const primec::Expr &hookCall = conditionExpr.args.front();
    REQUIRE(hookCall.kind == primec::Expr::Kind::Call);
    CHECK(hookCall.name == expectedHookPath);
    REQUIRE(hookCall.args.size() == 1);
    REQUIRE(hookCall.args.front().kind == primec::Expr::Kind::Name);
    CHECK(hookCall.args.front().name == "value");

    const primec::Expr &thenEnvelope = guardStmt.args[1];
    REQUIRE(thenEnvelope.kind == primec::Expr::Kind::Call);
    CHECK(thenEnvelope.name == "then");
    CHECK(thenEnvelope.hasBodyArguments);
    REQUIRE(thenEnvelope.bodyArguments.size() == 1);
    const primec::Expr &returnCall = thenEnvelope.bodyArguments.front();
    REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
    CHECK(returnCall.name == "return");
    REQUIRE(returnCall.args.size() == 1);
    REQUIRE(returnCall.args.front().kind == primec::Expr::Kind::Literal);
    CHECK_FALSE(returnCall.args.front().isUnsigned);
    CHECK(returnCall.args.front().intWidth == 32);
    CHECK(returnCall.args.front().literalValue == expectedCode);

    const primec::Expr &elseEnvelope = guardStmt.args[2];
    REQUIRE(elseEnvelope.kind == primec::Expr::Kind::Call);
    CHECK(elseEnvelope.name == "else");
    CHECK(elseEnvelope.hasBodyArguments);
    CHECK(elseEnvelope.bodyArguments.empty());
  };
  assertValidateGuard(validateHelper->statements[0], "/Pair/ValidateField_x", 1);
  assertValidateGuard(validateHelper->statements[1], "/Pair/ValidateField_ok", 2);

  const auto assertValidateHook = [](const primec::Definition &hookDef) {
    REQUIRE(hookDef.parameters.size() == 1);
    CHECK(hookDef.parameters.front().name == "value");
    bool hasBoolReturn = false;
    for (const auto &transform : hookDef.transforms) {
      if (transform.name == "return" && transform.templateArgs.size() == 1 &&
          transform.templateArgs.front() == "bool") {
        hasBoolReturn = true;
      }
    }
    CHECK(hasBoolReturn);
    CHECK(hookDef.statements.empty());
    CHECK(hookDef.hasReturnStatement);
    REQUIRE(hookDef.returnExpr.has_value());
    CHECK(hookDef.returnExpr->kind == primec::Expr::Kind::BoolLiteral);
    CHECK(hookDef.returnExpr->boolValue);
  };
  assertValidateHook(*xHook);
  assertValidateHook(*okHook);
}

TEST_CASE("generate Validate for empty reflected struct emits ok-only helper") {
  const std::string source = R"(
[struct reflect generate(Validate)]
Marker() {
}

[return<int>]
main() {
  [Marker] value{Marker()}
  /Marker/Validate(value)
  return(0i32)
}
)";
  auto program = parseProgram(source);
  primec::Semantics semantics;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults));
  CHECK(error.empty());

  const primec::Definition *validateHelper = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == "/Marker/Validate") {
      validateHelper = &def;
      break;
    }
  }
  REQUIRE(validateHelper != nullptr);
  CHECK(validateHelper->statements.empty());
  CHECK(validateHelper->hasReturnStatement);
  REQUIRE(validateHelper->returnExpr.has_value());
  CHECK(validateHelper->returnExpr->kind == primec::Expr::Kind::Call);
  CHECK(validateHelper->returnExpr->isMethodCall);
  CHECK(validateHelper->returnExpr->name == "ok");
  REQUIRE(validateHelper->returnExpr->args.size() == 1);
  CHECK(validateHelper->returnExpr->args.front().kind == primec::Expr::Kind::Name);
  CHECK(validateHelper->returnExpr->args.front().name == "Result");

  const bool hasFieldHooks = std::any_of(program.definitions.begin(),
                                         program.definitions.end(),
                                         [](const primec::Definition &def) {
                                           return def.fullPath.rfind("/Marker/ValidateField_", 0) == 0;
                                         });
  CHECK_FALSE(hasFieldHooks);
}

TEST_CASE("generate Validate ignores static fields") {
  const std::string source = R"(
[struct reflect generate(Validate)]
Marker() {
  [static i32] shared{1i32}
}

[return<int>]
main() {
  [Marker] value{Marker()}
  /Marker/Validate(value)
  return(0i32)
}
)";
  auto program = parseProgram(source);
  primec::Semantics semantics;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults));
  CHECK(error.empty());

  const primec::Definition *validateHelper = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == "/Marker/Validate") {
      validateHelper = &def;
      break;
    }
  }
  REQUIRE(validateHelper != nullptr);
  CHECK(validateHelper->statements.empty());

  const bool hasFieldHooks = std::any_of(program.definitions.begin(),
                                         program.definitions.end(),
                                         [](const primec::Definition &def) {
                                           return def.fullPath.rfind("/Marker/ValidateField_", 0) == 0;
                                         });
  CHECK_FALSE(hasFieldHooks);
}

TEST_CASE("generate Compare Hash64 Clear CopyFrom Validate helpers keep canonical v2 order") {
  const std::string source = R"(
[struct reflect generate(Validate, CopyFrom, Clear, Hash64, Compare)]
Pair() {
  [i32] x{1i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  auto program = parseProgram(source);
  primec::Semantics semantics;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults));
  CHECK(error.empty());

  std::vector<std::string> generatedHelpers;
  for (const auto &def : program.definitions) {
    if (def.fullPath.rfind("/Pair/", 0) != 0 || def.fullPath == "/Pair") {
      continue;
    }
    generatedHelpers.push_back(def.fullPath);
  }
  const std::vector<std::string> expected = {
      "/Pair/Compare", "/Pair/Hash64", "/Pair/Clear", "/Pair/CopyFrom", "/Pair/ValidateField_x", "/Pair/Validate"};
  CHECK(generatedHelpers == expected);
}

TEST_CASE("generate Validate rejects existing helper collision") {
  const std::string source = R"(
[struct reflect generate(Validate)]
Pair() {
  [i32] x{1i32}
}

[return<Result<FileError>>]
/Pair/Validate([Pair] value) {
  return(Result.ok())
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("generated reflection helper already exists: /Pair/Validate") != std::string::npos);
}

TEST_CASE("generate Validate rejects existing field hook collision") {
  const std::string source = R"(
[struct reflect generate(Validate)]
Pair() {
  [i32] x{1i32}
}

[return<bool>]
/Pair/ValidateField_x([Pair] value) {
  return(true)
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("generated reflection helper already exists: /Pair/ValidateField_x") != std::string::npos);
}

TEST_CASE("generate Serialize emits reflection helper definition") {
  const std::string source = R"(
[struct reflect generate(Serialize)]
Pair() {
  [i32] x{1i32}
  [bool] ok{true}
}

[return<int>]
main() {
  [Pair] value{Pair([x] 2i32, [ok] true)}
  [array<u64>] payload{/Pair/Serialize(value)}
  return(0i32)
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
    if (def.fullPath == "/Pair/Serialize") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->parameters.size() == 1);
  CHECK(generated->parameters.front().name == "value");

  bool hasArrayReturn = false;
  for (const auto &transform : generated->transforms) {
    if (transform.name == "return" && transform.templateArgs.size() == 1 &&
        transform.templateArgs.front() == "array<u64>") {
      hasArrayReturn = true;
    }
  }
  CHECK(hasArrayReturn);
  CHECK(generated->hasReturnStatement);
  REQUIRE(generated->returnExpr.has_value());
  REQUIRE(generated->returnExpr->kind == primec::Expr::Kind::Call);
  CHECK(generated->returnExpr->name == "array");
  REQUIRE(generated->returnExpr->templateArgs.size() == 1);
  CHECK(generated->returnExpr->templateArgs.front() == "u64");
  REQUIRE(generated->returnExpr->args.size() == 3);
  REQUIRE(generated->returnExpr->args[0].kind == primec::Expr::Kind::Literal);
  CHECK(generated->returnExpr->args[0].isUnsigned);
  CHECK(generated->returnExpr->args[0].intWidth == 64);
  CHECK(generated->returnExpr->args[0].literalValue == 1ULL);

  const auto assertConvertField = [](const primec::Expr &expr, const std::string &fieldName) {
    REQUIRE(expr.kind == primec::Expr::Kind::Call);
    CHECK(expr.name == "convert");
    REQUIRE(expr.templateArgs.size() == 1);
    CHECK(expr.templateArgs.front() == "u64");
    REQUIRE(expr.args.size() == 1);
    REQUIRE(expr.args.front().kind == primec::Expr::Kind::Call);
    CHECK(expr.args.front().isFieldAccess);
    CHECK(expr.args.front().name == fieldName);
  };
  assertConvertField(generated->returnExpr->args[1], "x");
  assertConvertField(generated->returnExpr->args[2], "ok");
}

TEST_CASE("generate Serialize for empty reflected struct returns version-only payload") {
  const std::string source = R"(
[struct reflect generate(Serialize)]
Marker() {
}

[return<int>]
main() {
  [Marker] value{Marker()}
  [array<u64>] payload{/Marker/Serialize(value)}
  return(0i32)
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
    if (def.fullPath == "/Marker/Serialize") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->returnExpr.has_value());
  REQUIRE(generated->returnExpr->kind == primec::Expr::Kind::Call);
  CHECK(generated->returnExpr->name == "array");
  REQUIRE(generated->returnExpr->args.size() == 1);
  REQUIRE(generated->returnExpr->args[0].kind == primec::Expr::Kind::Literal);
  CHECK(generated->returnExpr->args[0].isUnsigned);
  CHECK(generated->returnExpr->args[0].intWidth == 64);
  CHECK(generated->returnExpr->args[0].literalValue == 1ULL);
}

TEST_CASE("generate Serialize ignores static fields") {
  const std::string source = R"(
[struct reflect generate(Serialize)]
Marker() {
  [static i32] shared{1i32}
}

[return<int>]
main() {
  [Marker] value{Marker()}
  [array<u64>] payload{/Marker/Serialize(value)}
  return(0i32)
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
    if (def.fullPath == "/Marker/Serialize") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->returnExpr.has_value());
  REQUIRE(generated->returnExpr->kind == primec::Expr::Kind::Call);
  REQUIRE(generated->returnExpr->args.size() == 1);
}

TEST_CASE("generate Serialize rejects existing helper collision") {
  const std::string source = R"(
[struct reflect generate(Serialize)]
Pair() {
  [i32] x{1i32}
}

[return<array<u64>>]
/Pair/Serialize([Pair] value) {
  return(array<u64>(1u64))
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("generated reflection helper already exists: /Pair/Serialize") != std::string::npos);
}

TEST_CASE("generate Serialize rejects unsupported field envelope deterministically") {
  const std::string source = R"(
[struct reflect generate(Serialize)]
Pair() {
  [string] label{"x"utf8}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("generated reflection helper /Pair/Serialize does not support field envelope: label (string)") !=
        std::string::npos);
}

TEST_CASE("generate Deserialize emits reflection helper definition") {
  const std::string source = R"(
[struct reflect generate(Deserialize)]
Pair() {
  [i32] x{1i32}
  [bool] ok{true}
}

[return<int>]
main() {
  [Pair mut] value{Pair([x] 0i32, [ok] false)}
  [array<u64>] payload{array<u64>(1u64, 9u64, 1u64)}
  /Pair/Deserialize(value, payload)
  return(0i32)
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
    if (def.fullPath == "/Pair/Deserialize") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->parameters.size() == 2);
  CHECK(generated->parameters[0].name == "value");
  CHECK(generated->parameters[1].name == "payload");

  const bool valueHasMut = std::any_of(generated->parameters[0].transforms.begin(),
                                       generated->parameters[0].transforms.end(),
                                       [](const primec::Transform &transform) {
                                         return transform.name == "mut";
                                       });
  const bool payloadHasArrayType = std::any_of(generated->parameters[1].transforms.begin(),
                                               generated->parameters[1].transforms.end(),
                                               [](const primec::Transform &transform) {
                                                 return transform.name == "array" &&
                                                        transform.templateArgs.size() == 1 &&
                                                        transform.templateArgs.front() == "u64";
                                               });
  CHECK(valueHasMut);
  CHECK(payloadHasArrayType);

  bool hasResultReturn = false;
  for (const auto &transform : generated->transforms) {
    if (transform.name == "return" && transform.templateArgs.size() == 1 &&
        transform.templateArgs.front() == "Result<FileError>") {
      hasResultReturn = true;
    }
  }
  CHECK(hasResultReturn);
  CHECK(generated->hasReturnStatement);
  REQUIRE(generated->returnExpr.has_value());
  CHECK(generated->returnExpr->kind == primec::Expr::Kind::Call);
  CHECK(generated->returnExpr->isMethodCall);
  CHECK(generated->returnExpr->name == "ok");
  REQUIRE(generated->returnExpr->args.size() == 1);
  CHECK(generated->returnExpr->args.front().kind == primec::Expr::Kind::Name);
  CHECK(generated->returnExpr->args.front().name == "Result");

  REQUIRE(generated->statements.size() == 4);
  REQUIRE(generated->statements[0].kind == primec::Expr::Kind::Call);
  CHECK(generated->statements[0].name == "if");
  REQUIRE(generated->statements[0].args.size() == 3);
  REQUIRE(generated->statements[0].args[0].kind == primec::Expr::Kind::Call);
  CHECK(generated->statements[0].args[0].name == "not_equal");

  REQUIRE(generated->statements[1].kind == primec::Expr::Kind::Call);
  CHECK(generated->statements[1].name == "if");
  REQUIRE(generated->statements[1].args.size() == 3);
  REQUIRE(generated->statements[1].args[0].kind == primec::Expr::Kind::Call);
  CHECK(generated->statements[1].args[0].name == "not_equal");

  REQUIRE(generated->statements[2].kind == primec::Expr::Kind::Call);
  CHECK(generated->statements[2].name == "assign");
  REQUIRE(generated->statements[2].args.size() == 2);
  REQUIRE(generated->statements[2].args[1].kind == primec::Expr::Kind::Call);
  CHECK(generated->statements[2].args[1].name == "convert");
  REQUIRE(generated->statements[2].args[1].templateArgs.size() == 1);
  CHECK(generated->statements[2].args[1].templateArgs.front() == "i32");

  REQUIRE(generated->statements[3].kind == primec::Expr::Kind::Call);
  CHECK(generated->statements[3].name == "assign");
  REQUIRE(generated->statements[3].args.size() == 2);
  REQUIRE(generated->statements[3].args[1].kind == primec::Expr::Kind::Call);
  CHECK(generated->statements[3].args[1].name == "convert");
  REQUIRE(generated->statements[3].args[1].templateArgs.size() == 1);
  CHECK(generated->statements[3].args[1].templateArgs.front() == "bool");
}

TEST_CASE("generate Deserialize for empty reflected struct keeps version guards only") {
  const std::string source = R"(
[struct reflect generate(Deserialize)]
Marker() {
}

[return<int>]
main() {
  [Marker mut] value{Marker()}
  [array<u64>] payload{array<u64>(1u64)}
  /Marker/Deserialize(value, payload)
  return(0i32)
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
    if (def.fullPath == "/Marker/Deserialize") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->statements.size() == 2);
}

TEST_CASE("generate Deserialize ignores static fields") {
  const std::string source = R"(
[struct reflect generate(Deserialize)]
Marker() {
  [static i32] shared{1i32}
}

[return<int>]
main() {
  [Marker mut] value{Marker()}
  [array<u64>] payload{array<u64>(1u64)}
  /Marker/Deserialize(value, payload)
  return(0i32)
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
    if (def.fullPath == "/Marker/Deserialize") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
  REQUIRE(generated->statements.size() == 2);
}

TEST_CASE("generate Compare Hash64 Clear CopyFrom Validate Serialize Deserialize helpers keep canonical v2 order") {
  const std::string source = R"(
[struct reflect generate(Deserialize, Serialize, Validate, CopyFrom, Clear, Hash64, Compare)]
Pair() {
  [i32] x{1i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  auto program = parseProgram(source);
  primec::Semantics semantics;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults));
  CHECK(error.empty());

  std::vector<std::string> generatedHelpers;
  for (const auto &def : program.definitions) {
    if (def.fullPath.rfind("/Pair/", 0) != 0 || def.fullPath == "/Pair") {
      continue;
    }
    generatedHelpers.push_back(def.fullPath);
  }
  const std::vector<std::string> expected = {
      "/Pair/Compare",
      "/Pair/Hash64",
      "/Pair/Clear",
      "/Pair/CopyFrom",
      "/Pair/ValidateField_x",
      "/Pair/Validate",
      "/Pair/Serialize",
      "/Pair/Deserialize"};
  CHECK(generatedHelpers == expected);
}

TEST_CASE("generate Deserialize rejects existing helper collision") {
  const std::string source = R"(
[struct reflect generate(Deserialize)]
Pair() {
  [i32] x{1i32}
}

[return<Result<FileError>>]
/Pair/Deserialize([Pair mut] value, [array<u64>] payload) {
  return(Result.ok())
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("generated reflection helper already exists: /Pair/Deserialize") != std::string::npos);
}

TEST_CASE("generate Deserialize rejects unsupported field envelope deterministically") {
  const std::string source = R"(
[struct reflect generate(Deserialize)]
Pair() {
  [string] label{"x"utf8}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("generated reflection helper /Pair/Deserialize does not support field envelope: label (string)") !=
        std::string::npos);
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

TEST_SUITE_END();
