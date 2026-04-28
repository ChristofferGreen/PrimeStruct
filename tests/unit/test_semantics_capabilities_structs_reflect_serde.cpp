#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.effects");


TEST_CASE("generate Serialize emits reflection helper definition") {
  const std::string source = R"(
[struct reflect generate(Serialize)]
Pair() {
  [i32] x{1i32}
  [bool] ok{true}
}

[return<int>]
main() {
  [Pair] value{Pair{[x] 2i32, [ok] true}}
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
  [Pair mut] value{Pair{[x] 0i32, [ok] false}}
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


TEST_SUITE_END();
