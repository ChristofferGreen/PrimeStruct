#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.effects");


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
  CHECK(defaultValueBinding.args.front().isBraceConstructor);
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


TEST_SUITE_END();
