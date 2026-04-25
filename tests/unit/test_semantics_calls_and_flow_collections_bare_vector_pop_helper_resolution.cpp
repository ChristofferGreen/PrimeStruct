#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("bare vector pop statement body arguments validate without imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  pop(values) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector pop template specialization keeps canonical unknown target without import") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  pop<i32>(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/pop") != std::string::npos);
}

TEST_CASE("bare vector pop arity mismatch keeps canonical unknown target without import") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  pop(values, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/pop") != std::string::npos);
}

TEST_CASE("pop call keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/pop([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  pop(values)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pop method keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/pop([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.pop()
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector clear alias requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  /vector/clear(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("clear requires mutable vector binding") != std::string::npos);
}

TEST_CASE("bare vector clear validates through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  clear(values)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("clear allows vector elements with nested drop requirements") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[struct]
Owned() {
  [i32] value{1i32}

  Destroy() {
  }
}

[struct]
Wrapper() {
  [Owned] value{Owned()}
}

[effects(heap_alloc), return<int>]
main() {
  [Vector<Wrapper> mut] values{/std/collections/vectorPair<Wrapper>(Wrapper(Owned()), Wrapper(Owned()))}
  /std/collections/vector/clear<Wrapper>(values)
  return(/std/collections/vector/count<Wrapper>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("direct experimental vector constructor binds as Vector under wildcard import") {
  const std::string source = R"(
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32>] values{vector<i32>(2i32, 4i32, 6i32)}
  return(vectorCount<i32>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("direct experimental vector constructor keeps temporary receiver inference") {
  const std::string source = R"(
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<int>]
main() {
  return(vectorCount<i32>(vector<i32>(2i32, 4i32, 6i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("clear call keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/clear([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  clear(values)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("clear method keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/clear([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.clear()
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector clear template specialization keeps canonical unknown target without import") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  clear<i32>(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/clear") != std::string::npos);
}

TEST_CASE("bare vector clear arity mismatch keeps canonical unknown target without import") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  clear(values, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/clear") != std::string::npos);
}

TEST_CASE("vector remove_at alias requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  /vector/remove_at(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("remove_at requires mutable vector binding") != std::string::npos);
}

TEST_CASE("vector remove_at alias requires integer index") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /vector/remove_at(values, "hi"utf8)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("remove_at requires integer index") != std::string::npos);
}

TEST_CASE("remove_at rejects bool index in call and method forms") {
  const auto checkInvalidRemoveAt = [](const std::string &stmtText) {
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
    CHECK(error.find("remove_at requires integer index") != std::string::npos);
  };

  checkInvalidRemoveAt("/vector/remove_at(values, true)");
  checkInvalidRemoveAt("values.remove_at(true)");
}

TEST_CASE("bare vector remove_at template specialization keeps canonical unknown target without import") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  remove_at<i32>(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/remove_at") != std::string::npos);
}

TEST_CASE("bare vector remove_at arity mismatch keeps canonical unknown target without import") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  remove_at(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/remove_at") != std::string::npos);
}

TEST_CASE("remove_at accepts ownership-sensitive vector element types once survivor motion is wired") {
  const std::string source = R"(
import /std/collections/*

[struct]
Owned() {
  [i32] value{4i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Owned> mut] values{vector<Owned>()}
  push(values, Owned())
  push(values, Owned(9i32))
  remove_at(values, 0i32)
  return(plus(count(values), at(values, 0i32).value))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("remove_at accepts nested ownership-sensitive vector element types once survivor motion is wired") {
  const std::string source = R"(
import /std/collections/*

[struct]
Mover() {
  [i32] value{1i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this, other)
  }
}

[struct]
Wrapper() {
  [Mover] value{Mover()}
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Wrapper> mut] values{vector<Wrapper>()}
  push(values, Wrapper(Mover(1i32)))
  push(values, Wrapper(Mover(7i32)))
  remove_at(values, 0i32)
  return(plus(count(values), at_unsafe(values, 0i32).value.value))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("remove_at call keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/remove_at([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_at(values, 1i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("remove_at method keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/remove_at([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.remove_at(1i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector remove_swap alias requires integer index") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /vector/remove_swap(values, "hi"utf8)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("remove_swap requires integer index") != std::string::npos);
}

TEST_CASE("remove_swap rejects bool index in call and method forms") {
  const auto checkInvalidRemoveSwap = [](const std::string &stmtText) {
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
    CHECK(error.find("remove_swap requires integer index") != std::string::npos);
  };

  checkInvalidRemoveSwap("/vector/remove_swap(values, true)");
  checkInvalidRemoveSwap("values.remove_swap(true)");
}

TEST_CASE("vector remove_swap alias requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  /vector/remove_swap(values, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("remove_swap requires mutable vector binding") != std::string::npos);
}

TEST_CASE("bare vector remove_swap template specialization keeps canonical unknown target without import") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_swap<i32>(values, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/remove_swap") != std::string::npos);
}

TEST_CASE("bare vector remove_swap arity mismatch keeps canonical unknown target without import") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_swap(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/remove_swap") != std::string::npos);
}

TEST_CASE("remove_swap accepts ownership-sensitive vector element types once survivor motion is wired") {
  const std::string source = R"(
import /std/collections/*

[struct]
Owned() {
  [i32] value{4i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Owned> mut] values{vector<Owned>()}
  push(values, Owned())
  push(values, Owned(9i32))
  remove_swap(values, 0i32)
  return(plus(count(values), at(values, 0i32).value))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("remove_swap accepts nested ownership-sensitive vector element types once survivor motion is wired") {
  const std::string source = R"(
import /std/collections/*

[struct]
Mover() {
  [i32] value{1i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this, other)
  }
}

[struct]
Wrapper() {
  [Mover] value{Mover()}
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Wrapper> mut] values{vector<Wrapper>()}
  push(values, Wrapper(Mover(1i32)))
  push(values, Wrapper(Mover(7i32)))
  remove_swap(values, 0i32)
  return(plus(count(values), at_unsafe(values, 0i32).value.value))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector remove_swap validates through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_swap(values, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("remove_swap call keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/remove_swap([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_swap(values, 1i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("remove_swap method keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/remove_swap([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.remove_swap(1i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helpers are statement-only in expressions") {
  struct HelperCase {
    const char *name;
    const char *args;
  };
  const HelperCase helpers[] = {
      {"push", "values, 2i32"},       {"pop", "values"},       {"reserve", "values, 8i32"},
      {"clear", "values"},            {"remove_at", "values, 0i32"}, {"remove_swap", "values, 0i32"}};
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

TEST_CASE("vector helper named args on array targets report vector binding") {
  const auto checkInvalidStatement = [](const std::string &stmtText) {
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [array<i32> mut] values{array<i32>(1i32, 2i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("requires vector binding") != std::string::npos);
  };

  checkInvalidStatement("push([values] values, [value] 3i32)");
  checkInvalidStatement("reserve([values] values, [capacity] 8i32)");
  checkInvalidStatement("remove_at([values] values, [index] 0i32)");
  checkInvalidStatement("remove_swap([values] values, [index] 0i32)");
  checkInvalidStatement("pop([values] values)");
  checkInvalidStatement("clear([values] values)");
  checkInvalidStatement("values.push([value] 3i32)");
  checkInvalidStatement("values.reserve([capacity] 8i32)");
  checkInvalidStatement("values.remove_at([index] 0i32)");
  checkInvalidStatement("values.remove_swap([index] 0i32)");
}

TEST_SUITE_END();
