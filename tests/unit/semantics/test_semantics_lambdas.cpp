#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.lambdas");

TEST_CASE("lambda expressions validate without captures") {
  const std::string source = R"(
[return<void>]
main() {
  []([i32] value) { return(plus(value, 1i32)) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("lambda captures allow outer bindings") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] base{1i32}
  [value base]([i32] offset) { return(plus(base, offset)) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("lambda captures accept ref qualifier") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] base{1i32}
  [ref base]([i32] offset) { return(plus(base, offset)) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("lambda without capture rejects outer binding") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] base{1i32}
  []([i32] offset) { return(plus(base, offset)) }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown identifier: base") != std::string::npos);
}

TEST_CASE("lambda rejects unknown capture name") {
  const std::string source = R"(
[return<void>]
main() {
  [value missing]([i32] offset) { return(offset) }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown capture: missing") != std::string::npos);
}

TEST_CASE("lambda rejects invalid capture qualifier") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] base{1i32}
  [foo base]([i32] offset) { return(offset) }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid lambda capture") != std::string::npos);
}

TEST_CASE("lambda parameter default rejects user-defined array call") {
  const std::string source = R"(
[return<i32>]
array([i32] value) {
  return(value)
}

[return<void>]
main() {
  []([i32] value{array(1i32)}) { return(value) }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("parameter default must be a literal or pure expression") != std::string::npos);
}

TEST_CASE("lambda parameter default allows builtin array literal") {
  const std::string source = R"(
[return<void>]
main() {
  []([array<i32>] value{array<i32>(1i32, 2i32)}) { return(at(value, 0i32)) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("lambda rejects conflicting capture-all tokens") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] base{1i32}
  [=, &]([i32] offset) { return(offset) }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid lambda capture") != std::string::npos);
}

TEST_CASE("lambda capture-all allows explicit known names") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] base{1i32}
  [=, value base]([i32] offset) { return(plus(base, offset)) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("lambda capture-all rejects explicit unknown names") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] base{1i32}
  [=, value missing]([i32] offset) { return(offset) }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown capture: missing") != std::string::npos);
}

TEST_CASE("lambda calls accept named arguments") {
  // A closure call is a call of its `call` method, so arguments may be labeled like any method call.
  const std::string source = R"(
[return<int>]
main() {
  holder{[]([i32] value) { return(value) }}
  return(holder([value] 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("lambda rejects writing a capture that is not mut") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] count{0i32}
  bump{[count]() {
    assign(count, plus(count, 1i32))
    return(count)
  }}
  return(bump())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("ref capture is a loan on the captured binding until the closure's last use") {
  const std::string source = R"(
import /std/collections/*

[return<int> effects(heap_alloc)]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  reader{[ref values]() { return(values.count()) }}
  values.push(3i32)
  return(reader())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: values (root: values, sink: reader)") != std::string::npos);
}

TEST_CASE("ref capture allows changes after the closure's last use") {
  const std::string source = R"(
import /std/collections/*

[return<int> effects(heap_alloc)]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  reader{[ref values]() { return(values.count()) }}
  [i32] before{reader()}
  values.push(3i32)
  return(plus(before, values.count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("closure holding a ref capture cannot become a Function") {
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [i32 mut] hits{0i32}
  reader{[ref hits]() { return(hits) }}
  [Function<i32> mut] erased{Function<i32>{reader}}
  return(erased())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  // A named closure reaches the conversion as a value: its capture is checked like any other (it is not sendable).
  CHECK_MESSAGE(error.find("closure cannot be a Function: capture `hits` is Reference<i32>") !=
                    std::string::npos,
                error);
}

TEST_CASE("inline ref lambda cannot become a Function") {
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [i32 mut] hits{0i32}
  [Function<i32> mut] erased{Function<i32>{[ref hits]() { return(hits) }}}
  return(erased())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("closure escapes via Function (root: hits)") != std::string::npos);
}

TEST_CASE("a closure converts to a Function parameter, field and binding") {
  const std::string source = R"(
[struct]
Hook {
  [Function<i32, i32> mut] run
}

[return<i32>]
apply([Function<i32, i32> mut] f, [i32] x) {
  return(f(x))
}

[return<i32>]
double([i32] x) {
  return(multiply(x, 2i32))
}

[return<int> effects(heap_alloc)]
main() {
  [i32] base{1i32}
  [Hook mut] hook{Hook{[run] double}}
  [Function<i32, i32> mut] named{Function<i32, i32>{/double}}
  [Function<i32, i32> mut] bound{[base]([i32] x) { return(plus(x, base)) }}
  return(plus(apply([base]([i32] x) { return(plus(x, base)) }, 1i32), apply(double, 2i32)))
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

TEST_CASE("a closure with the wrong parameter type cannot become a Function parameter") {
  const std::string source = R"(
[return<i32>]
apply([Function<i32, i32> mut] f, [i32] x) {
  return(f(x))
}

[return<int> effects(heap_alloc)]
main() {
  return(apply([]([string] text) { return(1i32) }, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_MESSAGE(error.find("Function<i32, i32> cannot hold /main__lambda0 in /main: its parameter text is string, "
                           "the Function passes i32") != std::string::npos,
                error);
}

TEST_CASE("a named definition with the wrong arity cannot become a Function") {
  const std::string source = R"(
[return<i32>]
apply([Function<i32, i32> mut] f, [i32] x) {
  return(f(x))
}

[return<i32>]
add([i32] a, [i32] b) {
  return(plus(a, b))
}

[return<int> effects(heap_alloc)]
main() {
  return(apply(add, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_MESSAGE(error.find("Function<i32, i32> cannot hold /add in /main: it takes 2 arguments, the Function passes 1") !=
                    std::string::npos,
                error);
}

TEST_CASE("a ref lambda passed to a Function parameter escapes") {
  const std::string source = R"(
[return<i32>]
apply([Function<i32, i32> mut] f, [i32] x) {
  return(f(x))
}

[return<int> effects(heap_alloc)]
main() {
  [i32 mut] hits{0i32}
  return(apply([ref hits]([i32] x) { return(plus(x, hits)) }, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_MESSAGE(error.find("closure escapes via Function (root: hits)") != std::string::npos, error);
}

TEST_CASE("a borrowing closure stored in a Function field escapes") {
  const std::string source = R"(
[struct]
Hook {
  [Function<i32, i32> mut] run
}

[return<int> effects(heap_alloc)]
main() {
  [i32 mut] hits{0i32}
  reader{[ref hits]([i32] x) { return(plus(x, hits)) }}
  [Hook mut] hook{Hook{[run] reader}}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_MESSAGE(error.find("closure cannot be a Function: capture `hits` is Reference<i32>") !=
                    std::string::npos,
                error);
}

TEST_CASE("closure capturing a view by value borrows the view's owner") {
  const std::string source = R"(
import /std/text/*
import /std/text/view/*

[return<int> effects(heap_alloc)]
main() {
  [String mut] text{/std/text/stringFrom("hello")}
  [StringView] view{text.view()}
  reader{[view]() { return(view.count()) }}
  text.append("!")
  return(reader())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: text (root: text, sink: reader)") != std::string::npos);
}

TEST_CASE("indexing a ref-captured vector resolves to the borrowed vector helper") {
  const std::string source = R"(
import /std/collections/*

[return<int> effects(heap_alloc)]
main() {
  [vector<i32> mut] values{vector<i32>(5i32, 7i32, 9i32)}
  [mut] pick{[ref values]([i32] input) { return(values[input]) }}
  return(pick(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("indexing a ref-captured vector checks the index type") {
  const std::string source = R"(
import /std/collections/*

[return<int> effects(heap_alloc)]
main() {
  [vector<i32> mut] values{vector<i32>(5i32, 7i32, 9i32)}
  [mut] pick{[ref values]([i32] input) { return(values[1.5f32]) }}
  return(pick(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("vectorAtRef") != std::string::npos);
  CHECK(error.find("expected i32 got f32") != std::string::npos);
}

TEST_CASE("callable fields reached through this are called") {
  const std::string source = R"(
import /std/collections/*

[struct]
Route {
  [i32] code
  [Function<i32, i32> mut] handler
}

[struct]
Router {
  [vector<Route> mut] routes
  [Function<i32, i32> mut] fallback

  [mut return<i32> effects(heap_alloc)]
  call([i32] input) {
    return(this.routes[0i32].handler(input) + this.fallback(input))
  }
}

[return<int> effects(heap_alloc)]
main() {
  [Router mut] router{Router{[routes] vector<Route>(), [fallback] Function<i32, i32>{[]([i32] x) { return(x) }}}}
  router.routes.push(Route{[code] 1i32, [handler] Function<i32, i32>{[]([i32] x) { return(x + 1i32) }}})
  return(router.call(2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("a missing callable field reached through this is an unknown method") {
  const std::string source = R"(
import /std/collections/*

[struct]
Route {
  [i32] code
  [Function<i32, i32> mut] handler
}

[struct]
Router {
  [vector<Route> mut] routes
  [Function<i32, i32> mut] fallback

  [mut return<i32> effects(heap_alloc)]
  call([i32] input) {
    return(this.routes[0i32].missing(input) + this.fallback(input))
  }
}

[return<int> effects(heap_alloc)]
main() {
  [Router mut] router{Router{[routes] vector<Route>(), [fallback] Function<i32, i32>{[]([i32] x) { return(x) }}}}
  router.routes.push(Route{[code] 1i32, [handler] Function<i32, i32>{[]([i32] x) { return(x + 1i32) }}})
  return(router.call(2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Route/missing") != std::string::npos);
}

TEST_CASE("a Function local moved into a brace constructor is used after the move") {
  const std::string source = R"(
[struct]
Wrap {
  [Function<i32, i32> mut] run
}

[return<int> effects(heap_alloc)]
main() {
  [i32] seven{7i32}
  [Function<i32, i32> mut] plain{[seven]([i32] x) { return(plus(x, seven)) }}
  [Wrap mut] wrap{Wrap{move(plain)}}
  return(plain(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("use-after-move: plain") != std::string::npos);
}

TEST_CASE("a lambda names the type parameters of its definition") {
  const std::string source = R"(
[return<i32>]
apply_value<T>([T] seed) {
  [i32] base{10i32}
  f{[base]([T] item) { return(base) }}
  return(f(seed))
}

[return<int>]
main() {
  return(apply_value<i64>(5i64))
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

TEST_CASE("a lambda cannot name a type parameter its definition does not have") {
  const std::string source = R"(
[return<i32>]
apply_value<T>([T] seed) {
  f{[]([U] item) { return(1i32) }}
  return(f(seed))
}

[return<int>]
main() {
  return(apply_value<i32>(5i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported binding type: U") != std::string::npos);
}

TEST_CASE("a lambda parameter names a struct of its namespace") {
  const std::string source = R"(
namespace zed {
  [public struct]
  Job {
    [i32] id{0i32}
  }

  [return<i32>]
  run_job() {
    [Job] job{Job{[id] 3i32}}
    f{[]([Reference<Job>] j) { return(j.id) }}
    return(f(job))
  }
}

[return<int>]
main() {
  return(/zed/run_job())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("a lambda parameter naming no struct of its namespace or the root is rejected") {
  const std::string source = R"(
namespace zed {
  [return<i32>]
  run_job() {
    f{[]([Reference<Nope>] j) { return(1i32) }}
    return(0i32)
  }
}

[return<int>]
main() {
  return(/zed/run_job())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported reference target type: Nope") != std::string::npos);
}

TEST_SUITE_END();
