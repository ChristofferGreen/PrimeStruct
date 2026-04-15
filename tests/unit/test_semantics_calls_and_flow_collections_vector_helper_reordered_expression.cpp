#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("vector namespaced helper reordered expression stays statement-only") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/vector/push(2i32, values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("stdlib namespaced helper reordered expression rejects compatibility receiver fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/std/collections/vector/push(2i32, values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/std/collections/vector/push") != std::string::npos);
  CHECK(error.find("parameter") != std::string::npos);
}

TEST_CASE("stdlib namespaced helper reordered expression keeps compatibility reject diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/std/collections/vector/push(true, values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/std/collections/vector/push") != std::string::npos);
  CHECK(error.find("parameter") != std::string::npos);
}

TEST_CASE("vector namespaced helper reordered statement keeps vector-binding diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /vector/push(2i32, values)
  return(count(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("stdlib namespaced helper reordered statement rejects compatibility receiver fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
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
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/std/collections/vector/push") != std::string::npos);
  CHECK(error.find("parameter") != std::string::npos);
}

TEST_CASE("stdlib namespaced helper reordered statement keeps compatibility reject diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /std/collections/vector/push(true, values)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/std/collections/vector/push") != std::string::npos);
  CHECK(error.find("parameter") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector constructor resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  return(/std/collections/vector/count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector constructor accepts named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto] values{/std/collections/vector/vector<i32>([first] 4i32, [second] 5i32)}
  return(/std/collections/vector/count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector constructor supports named-argument temporary receivers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  return(plus(/std/collections/vector/count(/std/collections/vector/vector<i32>([second] 5i32, [first] 4i32)),
              /std/collections/vector/vector<i32>([second] 8i32, [first] 7i32).count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector constructor rejects explicit builtin vector binding" * doctest::skip(true)) {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  return(count(values))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector constructor rejects named-argument explicit builtin vector binding") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>([second] 5i32, [first] 4i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mismatch") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector constructor requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [auto] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/vector") != std::string::npos);
}

TEST_CASE("array namespaced vector constructor alias is rejected") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/array/vector<i32>(4i32, 5i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /array/vector") != std::string::npos);
}

TEST_CASE("array namespaced vector constructor alias named arguments keep unknown-call-target diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/array/vector<i32>([first] 4i32, [second] 5i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /array/vector") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector access and count helpers are builtin-alias validated") {
  const std::string source = R"(
import /std/collections/*
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 5i32)}
  [i32] c{/std/collections/vector/count(values)}
  [i32] k{/std/collections/vector/capacity(values)}
  [i32] first{/std/collections/vector/at(values, 0i32)}
  [i32] second{/std/collections/vector/at_unsafe(values, 1i32)}
  return(plus(plus(c, k), plus(first, second)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector helpers accept explicit Vector bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32> mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  /std/collections/vector/push(values, 6i32)
  [i32] c{/std/collections/vector/count(values)}
  [i32] k{/std/collections/vector/capacity(values)}
  [i32] first{/std/collections/vector/at(values, 0i32)}
  [i32] second{/std/collections/vector/at_unsafe(values, 2i32)}
  /std/collections/vector/pop(values)
  return(plus(plus(plus(c, k), plus(first, second)), /std/collections/vector/count(values)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib wrapper vector helpers accept explicit Vector bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32> mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  vectorReserve<i32>(values, 6i32)
  vectorPush<i32>(values, 6i32)
  [i32 mut] total{plus(vectorCount<i32>(values), vectorCapacity<i32>(values))}
  assign(total, plus(total, vectorAt<i32>(values, 0i32)))
  assign(total, plus(total, vectorAtUnsafe<i32>(values, 2i32)))
  vectorPop<i32>(values)
  vectorRemoveAt<i32>(values, 0i32)
  vectorPush<i32>(values, 9i32)
  vectorRemoveSwap<i32>(values, 0i32)
  vectorClear<i32>(values)
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib wrapper vector helpers reject explicit Vector type mismatch") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32>] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  return(vectorCount<bool>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mismatch") != std::string::npos);
}

TEST_CASE("stdlib wrapper vector constructors accept explicit Vector destinations") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<Vector<i32>> effects(heap_alloc)]
buildValues() {
  return(/std/collections/vectorPair<i32>(2i32, 4i32))
}

[return<i32> effects(heap_alloc)]
scoreValues([Vector<i32>] values) {
  return(plus(/std/collections/vector/count<i32>(values), /std/collections/vector/at_unsafe<i32>(values, 1i32)))
}

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32> mut] direct{/std/collections/vectorPair<i32>(4i32, 5i32)}
  [Vector<i32>] wrapped{wrapValues(/std/collections/vectorSingle<i32>(6i32))}
  [Vector<i32> mut] assigned{/std/collections/vectorNew<i32>()}
  assign(assigned, buildValues())
  /std/collections/vector/push<i32>(direct, 7i32)
  [i32 mut] total{scoreValues(/std/collections/vectorPair<i32>(1i32, 3i32))}
  assign(total, plus(total, /std/collections/vector/count<i32>(direct)))
  assign(total, plus(total, /std/collections/vector/at<i32>(direct, 0i32)))
  assign(total, plus(total, /std/collections/vector/at_unsafe<i32>(direct, 2i32)))
  assign(total, plus(total, /std/collections/vector/at<i32>(wrapped, 0i32)))
  assign(total, plus(total, /std/collections/vector/count<i32>(assigned)))
  assign(total, plus(total, /std/collections/vector/at_unsafe<i32>(assigned, 1i32)))
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib wrapper vector constructors keep mismatch diagnostics on explicit Vector destinations" * doctest::skip(true)) {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<Vector<i32>> effects(heap_alloc)]
buildValues() {
  return(/std/collections/vectorPair<i32>(2i32, false))
}

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32>] values{wrapValues(buildValues())}
  return(/std/collections/vector/count<i32>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mismatch") != std::string::npos);
}

TEST_CASE("stdlib wrapper vector constructors infer experimental auto locals and auto returns") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[return<auto> effects(heap_alloc)]
buildValues([bool] wrapped) {
  if(wrapped,
    then() {
      return(/std/collections/vectorPair(11i32, 13i32))
    },
    else() {
      return(/std/collections/vectorSingle(19i32))
    })
}

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vectorNew<i32>()}
  /std/collections/vector/push<i32>(values, 3i32)
  [auto mut] built{buildValues(true)}
  /std/collections/vector/push<i32>(built, 17i32)
  [auto] single{buildValues(false)}
  return(plus(/std/collections/vector/count<i32>(values),
      plus(/std/collections/vector/at<i32>(built, 2i32),
          /std/collections/vector/at_unsafe<i32>(single, 0i32))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped vector constructors infer experimental auto locals") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{wrapValues(/std/collections/vectorPair(4i32, 7i32))}
  /std/collections/vector/push<i32>(values, 9i32)
  return(plus(/std/collections/vector/count<i32>(values), /std/collections/vector/at_unsafe<i32>(values, 2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped vector constructor auto inference keeps template conflict diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [auto] values{wrapValues(/std/collections/vectorPair(2i32, false))}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/vectorPair") != std::string::npos);
}

TEST_CASE("implicit vector constructor auto inference keeps template conflict diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<int>]
main() {
  [auto] values{/std/collections/vectorPair(2i32, false)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/vectorPair") != std::string::npos);
}

TEST_CASE("canonical vector helpers accept direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<int>]
main() {
  [i32 mut] total{plus(/std/collections/vector/count(/std/collections/vectorPair(11i32, 13i32)),
                       /std/collections/vector/at(/std/collections/vectorPair(17i32, 19i32), 1i32))}
  assign(total, plus(total, vectorAt<i32>(/std/collections/vectorPair(23i32, 29i32), 0i32)))
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("canonical vector helpers keep mismatch diagnostics on direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/count(/std/collections/vectorPair(2i32, false)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/vectorPair") != std::string::npos);
}

TEST_CASE("experimental vector methods accept direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<int>]
main() {
  [i32 mut] total{plus(/std/collections/vector/count(/std/collections/vectorPair(31i32, 37i32)),
                       /std/collections/vector/at_unsafe(/std/collections/vectorPair(41i32, 43i32), 1i32))}
  assign(total, plus(total, /std/collections/vector/at(/std/collections/vectorPair(47i32, 53i32), 0i32)))
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental vector methods keep mismatch diagnostics on direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vectorPair(2i32, false).count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/vectorPair") != std::string::npos);
}

TEST_CASE("helper-wrapped canonical vector helpers accept direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [i32 mut] total{plus(/std/collections/vector/count(wrapValues(/std/collections/vectorPair(11i32, 13i32))),
                       /std/collections/vector/at(wrapValues(/std/collections/vectorPair(17i32, 19i32)), 1i32))}
  assign(total, plus(total, vectorAt<i32>(wrapValues(/std/collections/vectorPair(23i32, 29i32)), 0i32)))
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped canonical vector helpers keep mismatch diagnostics on direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/count(wrapValues(/std/collections/vectorPair(2i32, false))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/vectorPair") != std::string::npos);
}

TEST_CASE("helper-wrapped experimental vector methods accept direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [i32 mut] total{plus(/std/collections/vector/count(wrapValues(/std/collections/vectorPair(31i32, 37i32))),
                       /std/collections/vector/at_unsafe(wrapValues(/std/collections/vectorPair(41i32, 43i32)), 1i32))}
  assign(total, plus(total, /std/collections/vector/at(wrapValues(/std/collections/vectorPair(47i32, 53i32)), 0i32)))
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped experimental vector methods keep mismatch diagnostics on direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapValues(/std/collections/vectorPair(2i32, false)).count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/vectorPair") != std::string::npos);
}


TEST_SUITE_END();
