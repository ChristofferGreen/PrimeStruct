#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.effects");


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


TEST_SUITE_END();
