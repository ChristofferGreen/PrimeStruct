#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

#include "src/semantics/SemanticsHelpers.h"

#include <string>
#include <vector>

// One depth-matching template type text parser serves semantics, the lowerer and the IR printer; these cases
// pin its behavior, in particular that `A<B>C<D>` is not a template type with base `A`.

TEST_SUITE_BEGIN("primestruct.semantics.type_text");

namespace {

bool splitName(const std::string &text, std::string &base, std::string &arg) {
  return primec::semantics::splitTemplateTypeName(text, base, arg);
}

bool splitArgs(const std::string &text, std::vector<std::string> &out) {
  return primec::semantics::splitTopLevelTemplateArgs(text, out);
}

} // namespace

TEST_CASE("a template type name splits into its base and its argument text") {
  std::string base;
  std::string arg;
  REQUIRE(splitName("map<i32, vector<i32>>", base, arg));
  CHECK(base == "map");
  CHECK(arg == "i32, vector<i32>");
  REQUIRE(splitName("Box< i32 >", base, arg));
  CHECK(base == "Box");
  CHECK(arg == " i32 ");
  REQUIRE(splitName("Box<>", base, arg));
  CHECK(base == "Box");
  CHECK(arg.empty());
}

TEST_CASE("text that is not one template type fails and leaves the whole text as the base") {
  for (const std::string text : {"A<B>C<D>", "A<B>C", "A<B>>", "A<B", "A", "", "<B>", "A>B<"}) {
    CAPTURE(text);
    std::string base = "stale";
    std::string arg = "stale";
    CHECK_FALSE(splitName(text, base, arg));
    CHECK(base == text);
    CHECK(arg.empty());
  }
}

TEST_CASE("top-level template arguments split at commas outside nested templates") {
  std::vector<std::string> args;
  REQUIRE(splitArgs("i32, map<K, V> ,  string", args));
  REQUIRE(args.size() == 3);
  CHECK(args[0] == "i32");
  CHECK(args[1] == "map<K, V>");
  CHECK(args[2] == "string");
  REQUIRE(splitArgs("i32", args));
  CHECK(args == std::vector<std::string>{"i32"});
}

TEST_CASE("top-level template arguments reject an empty argument and empty text") {
  std::vector<std::string> args;
  CHECK_FALSE(splitArgs("", args));
  CHECK_FALSE(splitArgs("i32,,i32", args));
  CHECK_FALSE(splitArgs("i32, ", args));
}

TEST_CASE("a return type spelled A<B>C<D> is rejected with a readable message") {
  const std::string source = R"(
[return<array<i32>array<i32>>]
f() {
  return(0i32)
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return transform requires a type on /f") != std::string::npos);
}

TEST_SUITE_END();
