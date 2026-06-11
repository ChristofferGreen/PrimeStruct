#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.enum");

namespace {
const primec::Definition *findDefinition(const primec::Program &program, const std::string &path) {
  for (const auto &def : program.definitions) {
    if (def.fullPath == path) {
      return &def;
    }
  }
  return nullptr;
}

const primec::Expr *findBinding(const primec::Definition &def, const std::string &name) {
  for (const auto &stmt : def.statements) {
    if (stmt.isBinding && stmt.name == name) {
      return &stmt;
    }
  }
  return nullptr;
}

bool hasTransform(const std::vector<primec::Transform> &transforms, const std::string &name) {
  for (const auto &transform : transforms) {
    if (transform.name == name) {
      return true;
    }
  }
  return false;
}
} // namespace

TEST_CASE("enum desugars to struct static bindings") {
  const std::string source = R"(
[enum]
Colors() {
  assign(Blue, 5i32)
  Red
  Green
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

  const primec::Definition *def = findDefinition(program, "/Colors");
  REQUIRE(def != nullptr);
  CHECK(hasTransform(def->transforms, "struct"));
  CHECK_FALSE(hasTransform(def->transforms, "enum"));

  const primec::Expr *valueField = findBinding(*def, "value");
  REQUIRE(valueField != nullptr);
  CHECK(hasTransform(valueField->transforms, "i32"));
  CHECK_FALSE(hasTransform(valueField->transforms, "static"));

  const struct EntryExpectation {
    const char *name;
    int64_t value;
  } entries[] = {{"Blue", 5}, {"Red", 6}, {"Green", 7}};

  for (const auto &entry : entries) {
    const primec::Expr *binding = findBinding(*def, entry.name);
    REQUIRE(binding != nullptr);
    CHECK(hasTransform(binding->transforms, "public"));
    CHECK(hasTransform(binding->transforms, "static"));
    CHECK(hasTransform(binding->transforms, "/Colors"));
    REQUIRE(binding->args.size() == 1);
    const primec::Expr &call = binding->args.front();
    REQUIRE(call.kind == primec::Expr::Kind::Call);
    CHECK(call.isBraceConstructor);
    CHECK(call.name == "/Colors");
    REQUIRE(call.args.size() == 1);
    const primec::Expr &literal = call.args.front();
    REQUIRE(literal.kind == primec::Expr::Kind::Literal);
    CHECK(static_cast<int64_t>(literal.literalValue) == entry.value);
  }
}

TEST_CASE("enum rejects non-literal values") {
  const std::string source = R"(
[enum]
Bad() {
  assign(First, plus(1i32, 2i32))
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("enum values must be integer literals") != std::string::npos);
}

TEST_SUITE_END();
