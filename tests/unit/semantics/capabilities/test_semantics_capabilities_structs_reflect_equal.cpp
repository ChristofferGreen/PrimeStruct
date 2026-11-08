#include "third_party/doctest.h"

#include <algorithm>

#include "../test_semantics_helpers.h"

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
[generate(Equal)]
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

TEST_CASE("generate transform implies reflection on struct definitions") {
  const std::string source = R"(
[struct generate(Equal)]
main() {
  [i32] value{1i32}
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(validateProgramCapturingProgram(source, "/main", error, program));
  CHECK(error.empty());

  const primec::Definition *generated = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == "/main/Equal") {
      generated = &def;
      break;
    }
  }
  REQUIRE(generated != nullptr);
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
[struct generate(Equal, Default)]
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

[struct generate(Equal, Default)]
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

TEST_CASE("reflected Equal structs accept operator equality") {
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
  return(equal(left, right))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reflected Equal operator equality resolves through generated helper") {
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
  return(equal(left, right))
}
)";
  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false,
                             &semanticProgram));
  CHECK(error.empty());

  const primec::SemanticProgramDirectCallTarget *equalTarget = nullptr;
  for (const auto *entry : primec::semanticProgramDirectCallTargetView(semanticProgram)) {
    if (entry == nullptr) {
      continue;
    }
    if (entry->scopePath == "/main" && entry->callName == "equal") {
      equalTarget = entry;
      break;
    }
  }
  REQUIRE(equalTarget != nullptr);
  CHECK(primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, *equalTarget) ==
        "/Pair/Equal");
}

TEST_CASE("reflected NotEqual operator inequality resolves through generated helper") {
  const std::string source = R"(
[struct reflect generate(Equal, NotEqual)]
Pair() {
  [i32] x{1i32}
  [bool] ok{true}
}

[return<bool>]
main() {
  [Pair] left{Pair()}
  [Pair] right{Pair()}
  return(not_equal(left, right))
}
)";
  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(
      program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const primec::SemanticProgramDirectCallTarget *notEqualTarget = nullptr;
  for (const auto *entry : primec::semanticProgramDirectCallTargetView(semanticProgram)) {
    if (entry != nullptr && entry->scopePath == "/main" && entry->callName == "not_equal") {
      notEqualTarget = entry;
      break;
    }
  }
  REQUIRE(notEqualTarget != nullptr);
  CHECK(primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, *notEqualTarget) ==
        "/Pair/NotEqual");

  // Equal alone does not make `!=` valid: it needs NotEqual.
  const std::string equalOnly = R"(
[struct reflect generate(Equal)]
Pair() {
  [i32] x{1i32}
}

[return<bool>]
main() {
  [Pair] left{Pair()}
  [Pair] right{Pair()}
  return(not_equal(left, right))
}
)";
  std::string equalOnlyError;
  CHECK_FALSE(validateProgram(equalOnly, "/main", equalOnlyError));
  CHECK(equalOnlyError.find("comparisons require numeric, bool, or string operands") !=
        std::string::npos);
}

TEST_CASE("struct equality still rejects reflected structs without Equal generation") {
  const std::string source = R"(
[struct reflect]
Pair() {
  [i32] x{1i32}
}

[return<bool>]
main() {
  [Pair] left{Pair()}
  [Pair] right{Pair()}
  return(equal(left, right))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("comparisons require numeric, bool, or string operands") != std::string::npos);
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

TEST_CASE("reflected Equal and NotEqual operators resolve in struct methods and lambda bodies") {
  // A struct method's `==` resolves to the scoped `/Holder/equal`, and a lambda body is the `call` method of its
  // closure struct: both still reach the generated helpers.
  const std::string source = R"(
[struct generate(Equal, NotEqual)]
Pair() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [Pair] kept{Pair{}}

  [return<bool>]
  same([Pair] other) {
    return(equal(this.kept, other))
  }
}

[return<bool> effects(heap_alloc)]
main() {
  [Holder] holder{Holder{}}
  [Function<bool, Pair>] differs{[]([Pair] other) {
    [Pair] mine{Pair{[x] 2i32}}
    return(not_equal(mine, other))
  }}
  return(and(holder.same(Pair{}), differs(Pair{})))
}
)";
  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE_MESSAGE(
      semantics.validate(
          program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram),
      error);
  CHECK(error.empty());

  std::string methodTarget;
  std::string lambdaTarget;
  for (const auto *entry : primec::semanticProgramDirectCallTargetView(semanticProgram)) {
    if (entry == nullptr) {
      continue;
    }
    if (entry->scopePath == "/Holder/same" && entry->callName == "equal") {
      methodTarget =
          std::string(primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, *entry));
    }
    if (entry->scopePath.find("__lambda") != std::string::npos && entry->callName == "not_equal") {
      lambdaTarget =
          std::string(primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, *entry));
    }
  }
  CHECK(methodTarget == "/Pair/Equal");
  CHECK(lambdaTarget == "/Pair/NotEqual");
}

TEST_CASE("struct equality in a lambda body still rejects structs without Equal generation") {
  const std::string source = R"(
[struct reflect]
Pair() {
  [i32] x{1i32}
}

[return<bool> effects(heap_alloc)]
main() {
  [Function<bool, Pair>] same{[]([Pair] other) {
    [Pair] mine{Pair{}}
    return(equal(mine, other))
  }}
  return(same(Pair{}))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("comparisons require numeric, bool, or string operands") != std::string::npos);
}

TEST_CASE("a namespaced definition named equal keeps its own calls") {
  // The builtin fallback only applies when the scoped path names no definition.
  const std::string source = R"(
[struct generate(Equal)]
Pair() {
  [i32] x{1i32}
}

namespace geo {
  [return<bool>]
  equal([Pair] left, [Pair] right) {
    return(true)
  }

  [return<bool>]
  check([Pair] other) {
    return(equal(other, other))
  }
}

[return<bool>]
main() {
  return(/geo/check(Pair{}))
}
)";
  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE_MESSAGE(
      semantics.validate(
          program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram),
      error);
  std::string checkTarget;
  for (const auto *entry : primec::semanticProgramDirectCallTargetView(semanticProgram)) {
    if (entry != nullptr && entry->scopePath == "/geo/check" && entry->callName == "equal") {
      checkTarget =
          std::string(primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, *entry));
    }
  }
  CHECK(checkTarget == "/geo/equal");
}

TEST_SUITE_END();
