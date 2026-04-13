#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.effects");


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


TEST_SUITE_END();
