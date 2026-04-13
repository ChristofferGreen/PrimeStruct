#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("push and reserve named args validate through imported stdlib helpers") {
  const auto checkStatement = [](const std::string &stmtText) {
    const std::string source =
        "import /std/collections/*\n\n"
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK(validateProgram(source, "/main", error));
    CHECK(error.empty());
  };

  checkStatement("push([value] 3i32, [values] values)");
  checkStatement("reserve([capacity] 8i32, [values] values)");
}

TEST_CASE("indexed removal and discard named args still reject builtin call syntax") {
  const auto checkInvalidStatement = [](const std::string &stmtText) {
    const std::string source =
        "import /std/collections/*\n\n"
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
  };

  checkInvalidStatement("remove_at([index] 0i32, [values] values)");
  checkInvalidStatement("remove_swap([index] 0i32, [values] values)");
  checkInvalidStatement("pop([values] values)");
  checkInvalidStatement("clear([values] values)");
}

TEST_CASE("bare vector mutator named args require imported stdlib helpers") {
  const auto checkInvalidStatement = [](const std::string &stmtText, const std::string &expected) {
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find(expected) != std::string::npos);
  };

  checkInvalidStatement("push([value] 3i32, [values] values)",
                        "named arguments not supported for builtin calls");
  checkInvalidStatement("reserve([capacity] 8i32, [values] values)",
                        "named arguments not supported for builtin calls");
  checkInvalidStatement("remove_at([index] 0i32, [values] values)",
                        "named arguments not supported for builtin calls");
  checkInvalidStatement("remove_swap([index] 0i32, [values] values)",
                        "named arguments not supported for builtin calls");
  checkInvalidStatement("pop([values] values)", "unknown call target: /vector/pop");
  checkInvalidStatement("clear([values] values)", "unknown call target: /vector/clear");
}

TEST_CASE("vector helper expressions with named arguments stay statement-only") {
  struct HelperCase {
    const char *name;
    const char *args;
  };
  const HelperCase helpers[] = {
      {"push", "[values] values, [value] 2i32"},
      {"pop", "[values] values"},
      {"reserve", "[values] values, [capacity] 8i32"},
      {"clear", "[values] values"},
      {"remove_at", "[values] values, [index] 0i32"},
      {"remove_swap", "[values] values, [index] 0i32"},
  };
  for (const auto &helper : helpers) {
    CAPTURE(helper.name);
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32)}\n"
        "  return(" +
        std::string(helper.name) + "(" + helper.args + "))\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("only supported as a statement") != std::string::npos);
  }
}

TEST_CASE("vector helper method expressions with named arguments require helper resolution") {
  struct HelperCase {
    const char *name;
    const char *args;
  };
  const HelperCase helpers[] = {
      {"push", "[value] 2i32"},
      {"reserve", "[capacity] 8i32"},
      {"remove_at", "[index] 0i32"},
      {"remove_swap", "[index] 0i32"},
  };
  for (const auto &helper : helpers) {
    CAPTURE(helper.name);
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32)}\n"
        "  return(values." +
        std::string(helper.name) + "(" + helper.args + "))\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK_FALSE(error.empty());
  }
}

TEST_CASE("namespaced vector helper statement form requires same-path helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /vector/push(values, 2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("namespaced vector helper expression form stays statement-only") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/vector/push(values, 2i32))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector helper accepts statement form") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /std/collections/vector/push(values, 2i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("namespaced vector mutator statement helpers are accepted") {
  struct HelperCase {
    const char *name;
    const char *args;
  };
  const HelperCase helpers[] = {
      {"push", "values, 2i32"},
      {"pop", "values"},
      {"reserve", "values, 8i32"},
      {"clear", "values"},
      {"remove_at", "values, 0i32"},
      {"remove_swap", "values, 0i32"},
  };
  const char *prefixes[] = {"/std/collections/vector/"};
  for (const auto &helper : helpers) {
    for (const auto *prefix : prefixes) {
      CAPTURE(helper.name);
      CAPTURE(prefix);
      const std::string source =
          "import /std/collections/*\n\n"
          "[effects(heap_alloc), return<int>]\n"
          "main() {\n"
          "  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}\n"
          "  " + std::string(prefix) + helper.name + "(" + helper.args + ")\n"
          "  return(count(values))\n"
          "}\n";
      std::string error;
      CHECK(validateProgram(source, "/main", error));
      CHECK(error.empty());
    }
  }
}

TEST_CASE("bare vector mutator statement helper on builtin vector receiver requires same-path helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  push(values, 3i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("bare vector mutator method on builtin vector receiver requires same-path helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.push(3i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /vector/push") != std::string::npos);
}

TEST_CASE("vector namespaced mutator statement helper on builtin vector receiver requires same-path helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /vector/push(values, 3i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("vector namespaced mutator statement helper on builtin vector receiver ignores canonical-only helper") {
  const std::string source = R"(
[effects(heap_alloc)]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /vector/push(values, 3i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector mutator statement helper on builtin vector receiver ignores alias-only helper") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/push(values, 3i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/push") != std::string::npos);
}

TEST_CASE("vector namespaced mutator slash method on builtin vector receiver requires same-path helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values./vector/push(3i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /vector/push") != std::string::npos);
}

TEST_CASE("vector namespaced mutator slash method on builtin vector receiver ignores canonical-only helper") {
  const std::string source = R"(
[effects(heap_alloc)]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values./vector/push(3i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /vector/push") != std::string::npos);
}

TEST_CASE("vector namespaced mutator slash method on builtin vector receiver accepts same-path helper") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values./vector/push(3i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("namespaced vector mutator expression helpers stay statement-only") {
  struct HelperCase {
    const char *name;
    const char *args;
  };
  const HelperCase helpers[] = {
      {"push", "values, 2i32"},
      {"pop", "values"},
      {"reserve", "values, 8i32"},
      {"clear", "values"},
      {"remove_at", "values, 0i32"},
      {"remove_swap", "values, 0i32"},
  };
  const char *prefixes[] = {"/vector/"};
  for (const auto &helper : helpers) {
    for (const auto *prefix : prefixes) {
      CAPTURE(helper.name);
      CAPTURE(prefix);
      const std::string source =
          "[effects(heap_alloc), return<int>]\n"
          "main() {\n"
          "  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}\n"
          "  return(" + std::string(prefix) + helper.name + "(" + helper.args + "))\n"
          "}\n";
      std::string error;
      CHECK_FALSE(validateProgram(source, "/main", error));
      CHECK(error.find("unknown call target: " + std::string(prefix) + helper.name) != std::string::npos);
    }
  }
}

TEST_CASE("stdlib namespaced vector helper statement rejects compatibility helper definition fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /std/collections/vector/push(2i32, values)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("stdlib namespaced vector push accepts named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /std/collections/vector/push([value] 2i32, [values] values)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector pop accepts named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/pop([values] values)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector reserve accepts named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/reserve([capacity] 8i32, [values] values)
  return(plus(count(values), capacity(values)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector push requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /std/collections/vector/push(values, 2i32)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/push") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector pop requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/pop(values)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/pop") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector reserve requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/reserve(values, 8i32)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/reserve") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector clear accepts named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/clear([values] values)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector remove_at accepts named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  /std/collections/vector/remove_at([index] 1i32, [values] values)
  return(plus(count(values), at(values, 1i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector remove_swap accepts named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  /std/collections/vector/remove_swap([index] 0i32, [values] values)
  return(plus(count(values), at(values, 0i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector clear requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/clear(values)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/clear") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector remove_at requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  /std/collections/vector/remove_at(values, 1i32)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/remove_at") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector remove_swap requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  /std/collections/vector/remove_swap(values, 1i32)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/remove_swap") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector clear method requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values./std/collections/vector/clear()
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/clear") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector push method requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values./std/collections/vector/push(3i32)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/push") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector pop method requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values./std/collections/vector/pop()
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/pop") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector reserve method requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values./std/collections/vector/reserve(4i32)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/reserve") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector remove_at method requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values./std/collections/vector/remove_at(1i32)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/remove_at") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector remove_swap method requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values./std/collections/vector/remove_swap(1i32)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/remove_swap") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector push expression requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/std/collections/vector/push(values, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/push") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector push named args require imported stdlib helper in expressions") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/std/collections/vector/push([values] values, [value] 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push") != std::string::npos);
}

TEST_CASE("array namespaced vector mutator alias named args stay statement-only in expressions") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/array/push([values] values, [value] 2i32))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push is only supported as a statement") != std::string::npos);
}

TEST_CASE("namespaced vector helper duplicate named args stay statement-only in expressions") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/vector/push([values] values, [values] values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("array namespaced vector mutator alias duplicate named args stay statement-only in expressions") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/array/push([values] values, [values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("only supported as a statement") != std::string::npos);
}

TEST_CASE("array namespaced vector mutator alias statement is rejected") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /array/push(values, 2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /array/push") != std::string::npos);
}

TEST_CASE("vector mutator alias block args require same-path helpers") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /vector/push(values, 2i32) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector helper duplicate named args stay statement-only in expressions") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/std/collections/vector/push([values] values, [values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push") != std::string::npos);
}

TEST_CASE("vector helper method expression resolves through canonical stdlib helper") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(values.push(2i32))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push is only supported as a statement") != std::string::npos);
}

TEST_CASE("vector helper method expression reports canonical helper argument mismatch") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/push([vector<i32> mut] values, [bool] value) {
  return(convert<int>(value))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(values.push(2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push is only supported as a statement") != std::string::npos);
}

TEST_SUITE_END();
