#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.effects");


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
  CHECK(returnExpr.isBraceConstructor);
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
  CHECK(returnExpr.isBraceConstructor);
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
  REQUIRE(generated->statements.front().args.size() == 1);
  const primec::Expr &defaultCall = generated->statements.front().args.front();
  REQUIRE(defaultCall.kind == primec::Expr::Kind::Call);
  CHECK(defaultCall.isBraceConstructor);
  CHECK(defaultCall.name == "/Pair");
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
  CHECK(returnExpr.isBraceConstructor);
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
  CHECK(returnExpr.isBraceConstructor);
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
  [Nested] nested{Nested{}}
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


TEST_SUITE_END();
