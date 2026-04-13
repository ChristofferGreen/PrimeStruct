#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.effects");


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


TEST_SUITE_END();
