#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("vector namespaced capacity wrapper vector target without helper reports unknown target") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/capacity(wrapVector()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("vector namespaced capacity rejects named arguments as builtin alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/vector/capacity([values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("array namespaced vector capacity alias rejects named arguments with unknown-target diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/array/capacity([values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /array/capacity") != std::string::npos);
}

TEST_CASE("array namespaced vector capacity alias call is rejected") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/array/capacity(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /array/capacity") != std::string::npos);
}

TEST_CASE("array namespaced vector at alias wrapper call is rejected") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(4i32, 5i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/at(wrapVector(), 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /array/at") != std::string::npos);
}

TEST_CASE("array namespaced vector at_unsafe alias wrapper call is rejected") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(4i32, 5i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/at_unsafe(wrapVector(), 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /array/at_unsafe") != std::string::npos);
}

TEST_CASE("namespaced vector count and capacity allow named args for user helper receiver") {
  const std::string source = R"(
Counter {}

[return<int>]
/Counter/count([Counter] values) {
  return(4i32)
}

[return<int>]
/Counter/capacity([Counter] values) {
  return(5i32)
}

[return<int>]
main() {
  [Counter] values{Counter()}
  return(plus(/std/collections/vector/count([values] values),
              /vector/capacity([values] values)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector access helper accepts named arguments through imported stdlib helper" * doctest::skip(true)) {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(plus(/std/collections/vector/at([values] values, [index] 0i32),
              /std/collections/vector/at_unsafe([index] 1i32, [values] values)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("stdlib namespaced vector access helper requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/std/collections/vector/at(values, 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector at_unsafe helper requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/std/collections/vector/at_unsafe(values, 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/at_unsafe") != std::string::npos);
}

TEST_CASE("vector namespaced access helper rejects named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/vector/at([values] values, [index] 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/at") != std::string::npos);
}

TEST_CASE("vector namespaced at_unsafe helper rejects named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/vector/at_unsafe([values] values, [index] 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/at_unsafe") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity keeps non-vector target diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("vector namespaced capacity keeps non-vector target diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/vector/capacity(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("vector namespaced capacity array target without helper reports unknown target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(/vector/capacity(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("namespaced vector helper with named arguments is statement-only in expressions") {
  struct HelperCase {
    const char *name;
    const char *args;
  };
  const HelperCase helpers[] = {
      {"push", "[values] values, [value] 2i32"},
      {"reserve", "[values] values, [capacity] 8i32"},
      {"remove_at", "[values] values, [index] 0i32"},
      {"remove_swap", "[values] values, [index] 0i32"},
  };
  for (const auto &helper : helpers) {
    CAPTURE(helper.name);
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32)}\n"
        "  return(/vector/" +
        std::string(helper.name) + "(" + helper.args + "))\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("unknown call target: /vector/" + std::string(helper.name)) != std::string::npos);
  }
}

TEST_CASE("array namespaced vector helper with named arguments is statement-only in expressions") {
  struct HelperCase {
    const char *name;
    const char *args;
  };
  const HelperCase helpers[] = {
      {"push", "[values] values, [value] 2i32"},
      {"reserve", "[values] values, [capacity] 8i32"},
      {"remove_at", "[values] values, [index] 0i32"},
      {"remove_swap", "[values] values, [index] 0i32"},
  };
  for (const auto &helper : helpers) {
    CAPTURE(helper.name);
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32)}\n"
        "  return(/array/" +
        std::string(helper.name) + "(" + helper.args + "))\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("only supported as a statement") != std::string::npos);
  }
}

TEST_CASE("user definition named push is not treated as builtin vector helper") {
  const std::string source = R"(
[return<void>]
push([i32] value) {
}

[return<int>]
main() {
  push([value] 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct push helper statement call is not treated as builtin vector helper") {
  const std::string source = R"(
Counter {
  [i32 mut] value{0i32}
  push([i32] next) {
    assign(this.value, next)
  }
}

[return<int>]
main() {
  [Counter mut] counter{Counter()}
  counter.push(7i32)
  return(counter.value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct push helper expression call is not treated as builtin vector helper") {
  const std::string source = R"(
Counter {
  [i32 mut] value{0i32}
  [return<int>]
  push([i32] next) {
    assign(this.value, next)
    return(this.value)
  }
}

[return<int>]
main() {
  [Counter mut] counter{Counter()}
  return(counter.push(7i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct push helper assign to immutable field is rejected") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
  push([i32] next) {
    assign(this.value, next)
  }
}

[return<int>]
main() {
  [Counter mut] counter{Counter()}
  counter.push(7i32)
  return(counter.value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("struct push helper wrapper temporary statement call is not treated as builtin vector helper") {
  const std::string source = R"(
Counter {
  push([i32] next) {
  }
}

[return<Counter>]
makeCounter() {
  [Counter] counter{Counter()}
  return(counter)
}

[return<int>]
main() {
  makeCounter().push(7i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct push helper wrapper temporary expression call is not treated as builtin vector helper") {
  const std::string source = R"(
Counter {
  [return<int>]
  push([i32] next) {
    return(next)
  }
}

[return<Counter>]
makeCounter() {
  [Counter] counter{Counter()}
  return(counter)
}

[return<int>]
main() {
  return(makeCounter().push(7i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named push can be used in expressions") {
  const std::string source = R"(
[return<int>]
push([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(push(3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper call-form expression uses user-defined method target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(push(values, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper call-form expression user shadow accepts positional reordered arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(push(3i32, values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper call-form expression user shadow accepts bool positional reordered arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/vector/push([vector<i32> mut] values, [bool] value) {
  return(value)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(push(true, values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper call-form expression stays statement-only without user helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(push(values, 3i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push is only supported as a statement") != std::string::npos);
}

TEST_CASE("vector helper expression skips temp-leading positional receiver probing") {
  const std::string source = R"(
Counter {
}

[return<Counter>]
makeCounter() {
  return(Counter())
}

[effects(heap_alloc), return<Counter>]
/vector/push([vector<Counter> mut] values, [Counter] value) {
  return(value)
}

[effects(heap_alloc), return<Counter>]
main() {
  [vector<Counter> mut] values{vector<Counter>(Counter())}
  return(push(makeCounter(), values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push is only supported as a statement") != std::string::npos);
}

TEST_CASE("vector helper call-form expression user shadow accepts named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(push([value] 3i32, [values] values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper call-form expression user shadow keeps duplicate named diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(push([values] values, [values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate named argument: values") != std::string::npos);
}

TEST_CASE("vector helper call-form expression prefers labeled named receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [string] value) {
  return(11i32)
}

[effects(heap_alloc), return<int>]
/string/push([vector<i32> mut] values, [string] value) {
  return(99i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [string] payload{"payload"raw_utf8}
  return(push([value] payload, [values] values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper call-form expression infers auto binding from labeled receiver helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [string] value) {
  return(11i32)
}

[effects(heap_alloc), return<int>]
/string/push([vector<i32> mut] values, [string] value) {
  return(99i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [string] payload{"payload"raw_utf8}
  [auto] inferred{push([value] payload, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced helper auto inference follows alias precedence" * doctest::skip(true)) {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [string] value) {
  return(11i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/push([vector<i32> mut] values, [string] value) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [string] payload{"tag"raw_utf8}
  [auto] inferred{/std/collections/vector/push([value] payload, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced helper auto inference keeps explicit parameter order") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [string] value) {
  return(11i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/push([vector<i32> mut] values, [string] value) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [string] payload{"tag"raw_utf8}
  [auto] inferred{/std/collections/vector/push(payload, values)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/vector/push parameter values") !=
        std::string::npos);
}

TEST_CASE("vector stdlib namespaced helper keeps explicit parameter diagnostics over return mismatch") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [string] value) {
  return(11i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/push([vector<i32> mut] values, [string] value) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [string] payload{"tag"raw_utf8}
  [auto] inferred{/std/collections/vector/push(payload, values)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/vector/push parameter values") !=
        std::string::npos);
}

TEST_SUITE_END();
