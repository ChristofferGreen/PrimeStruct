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
[return<i32> effects(none)]
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

[return<i32> effects(none)]
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
[return<i32> effects(none)]
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

TEST_CASE("a mut closure parameter needs a Reference parameter in the Function type") {
  const std::string source = R"(
[struct]
Response() {
  [i32 mut] status{0i32}
}

[return<int> effects(heap_alloc)]
main() {
  [Function<void, Response> mut] handler{[]([Response mut] response) { assign(response.status, 7i32) }}
  [Response mut] response{Response{}}
  handler(response)
  return(response.status)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_MESSAGE(
      error.find(
          "Function<void, Response> cannot hold /main__lambda0 in /main: its parameter response is "
          "mut, spell the Function's parameter Reference<Response>") != std::string::npos,
      error);
}

TEST_CASE("a mut closure parameter converts to a Function with a Reference parameter") {
  const std::string source = R"(
[struct]
Response() {
  [i32 mut] status{0i32}
}

[return<int> effects(heap_alloc)]
main() {
  [Function<void, Reference<Response>> mut] handler{[]([Response mut] response) { assign(response.status, 7i32) }}
  [Function<void, Reference<i32>> mut] bump{[]([i32 mut] value) { assign(value, plus(value, 1i32)) }}
  [Response mut] response{Response{}}
  handler(response)
  return(response.status)
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

TEST_CASE("a named definition with the wrong arity cannot become a Function") {
  const std::string source = R"(
[return<i32> effects(none)]
apply([Function<i32, i32> mut] f, [i32] x) {
  return(f(x))
}

[return<i32> effects(none)]
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
[return<i32> effects(none)]
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

TEST_CASE("a callable field wins over a struct named in another case") {
  const std::string source = R"(
[struct]
Holder {
  [Function<i32, i32> mut] step
}

[struct]
Step {
  [i32] size
}

[return<int> effects(heap_alloc)]
main() {
  [Holder mut] holder{Holder{[step] Function<i32, i32>{[]([i32] x) { return(plus(x, 1i32)) }}}}
  return(holder.step(10i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("a method call without a field or method of that name still names the struct path") {
  const std::string source = R"(
[struct]
Holder {
  [i32] size
}

[struct]
Step {
  [i32] size
}

[return<int>]
main() {
  [Holder] holder{Holder{[size] 1i32}}
  return(holder.step(10i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Holder/step") != std::string::npos);
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

TEST_CASE("a closure moved into a Function binding is used after the move") {
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [i32] seven{7i32}
  adder{[seven]([i32] x) { return(plus(x, seven)) }}
  [Function<i32, i32>] boxed{move(adder)}
  return(boxed(adder(1i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("use-after-move: adder") != std::string::npos);
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

TEST_CASE("lambda return type annotation matches the body") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] base{1i32}
  [base, return<i32>]([i32] offset) { return(plus(base, offset)) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("lambda return type annotation rejects a mismatched body") {
  const std::string source = R"(
[return<void>]
main() {
  [return<i32>]([i32] offset) { return(true) }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
}

namespace {

struct ReferenceFieldDiagnostic {
  std::string message;
  int line = 0;
  int column = 0;
};

ReferenceFieldDiagnostic firstDiagnostic(const std::string &source) {
  std::string error;
  primec::SemanticDiagnosticInfo info;
  CHECK_FALSE(validateProgramCollectingDiagnostics(source, "/main", error, info));
  ReferenceFieldDiagnostic found;
  if (!info.records.empty()) {
    found.message = info.records.front().message;
    found.line = info.records.front().primarySpan.line;
    found.column = info.records.front().primarySpan.column;
  }
  return found;
}

} // namespace

TEST_CASE("a Reference field is built by braces and read through") {
  const std::string source = R"(
[struct]
Keeper() {
  [Reference<i64>] kept
}

[return<int>]
main() {
  [i64 mut] target{1i64}
  [Keeper] keeper{Keeper{location(target)}}
  [i64] got{dereference(keeper.kept)}
  return(0i32)
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

TEST_CASE("assigning to a Reference field is a diagnostic naming the field") {
  const std::string source = R"(
[struct]
Keeper() {
  [Reference<i64>] kept
}

[return<int>]
main() {
  [i64 mut] target{1i64}
  [Keeper mut] keeper{Keeper{location(target)}}
  assign(keeper.kept, location(target))
  return(0i32)
}
)";
  const ReferenceFieldDiagnostic found = firstDiagnostic(source);
  CHECK(found.message.find("Reference field cannot be reassigned: kept") == 0);
  CHECK(found.line == 11);
  CHECK(found.column == 3);
}

TEST_CASE("a mut Reference field is written through with a dereference") {
  const std::string source = R"(
[struct]
Keeper() {
  [Reference<i64> mut] kept

  [return<void>]
  store([i64] value) {
    assign(dereference(this.kept), value)
  }
}

[return<int>]
main() {
  [i64 mut] target{1i64}
  [Keeper mut] keeper{Keeper{location(target)}}
  assign(dereference(keeper.kept), 3i64)
  keeper.store(7i64)
  return(0i32)
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

TEST_CASE("writing through a Reference field that is not mut is a diagnostic naming the field") {
  const std::string source = R"(
[struct]
Keeper() {
  [Reference<i64>] kept

  [return<void>]
  store([i64] value) {
    assign(dereference(this.kept), value)
  }
}

[return<int>]
main() {
  [i64 mut] target{1i64}
  [Keeper mut] keeper{Keeper{location(target)}}
  keeper.store(7i64)
  return(0i32)
}
)";
  const ReferenceFieldDiagnostic found = firstDiagnostic(source);
  CHECK(found.message.find("Reference field is not mutable: kept") == 0);
  CHECK(found.line == 8);
  // The column of the `assign` call; the surface `*this.kept = value` reports the operand of `*`.
  CHECK(found.column == 5);
}

TEST_CASE("a closure that assigns a Reference parameter to a Reference field is a diagnostic") {
  const std::string source = R"(
import /std/sync/*

[struct]
Keeper() {
  [Reference<i64> mut] kept
}

[effects(lock, heap_alloc), return<int>]
main() {
  cell{Mutex<i64>{1i64}}
  [i64 mut] target{1i64}
  [Keeper mut] keeper{Keeper{location(target)}}
  cell.locked([ref keeper]([Reference<i64>] value) { keeper.kept = value })
  return(0i32)
}
)";
  const ReferenceFieldDiagnostic found = firstDiagnostic(source);
  CHECK(found.message.find("Reference field cannot be reassigned: kept") == 0);
  CHECK(found.line == 14);
  CHECK(found.column == 54);
}

TEST_CASE("a closure that returns its Reference parameter is a diagnostic with a position") {
  const std::string source = R"(
import /std/sync/*

[effects(lock, heap_alloc), return<int>]
main() {
  cell{Mutex<i64>{1i64}}
  [i64] got{cell.locked([]([Reference<i64>] value) { return(value) })}
  return(0i32)
}
)";
  const ReferenceFieldDiagnostic found = firstDiagnostic(source);
  CHECK(found.message.find("borrowed parameter escapes via return: value") == 0);
  CHECK(found.line == 7);
  CHECK(found.column == 54);
}

TEST_CASE(
    "a closure that returns location of its Reference parameter is a diagnostic with a position") {
  const std::string source = R"(
import /std/sync/*

[effects(lock, heap_alloc), return<int>]
main() {
  cell{Mutex<i64>{1i64}}
  cell.locked([]([Reference<i64>] value) { return(location(value)) })
  return(0i32)
}
)";
  const ReferenceFieldDiagnostic found = firstDiagnostic(source);
  CHECK(found.message.find("unable to infer return type") == 0);
  CHECK(found.line == 7);
  CHECK(found.column == 51);
}

// The `call` of a `Function` is `mut` (docs/spec/value-lifecycle.md, Function values): through a read-only lambda
// parameter the diagnostic names the receiver and the fix, not an unknown method.
TEST_CASE("a Function field of a read-only lambda parameter is not called through the parameter") {
  const std::string box = R"(
[struct]
Box() {
  [Function<i32, i32> mut] f{}
  [i32] base{0i32}
}
)";
  const std::string expected =
      "mutating method needs a mutable receiver: b.f.call() (b is a read-only parameter; "
      "take it as a [T mut] parameter, or call a mut copy: [Function<...> mut] h{b.f})";
  SUBCASE("a lambda parameter by value") {
    const std::string source = box + R"(
[return<int> effects(heap_alloc)]
main() {
  [Box] b0{Box{[f] []([i32] x) { return(plus(x, 1i32)) }, [base] 4i32}}
  g{[]([Box] b) { return(b.f(b.base)) }}
  return(g(b0))
}
)";
    primec::SemanticDiagnosticInfo diagnosticInfo;
    std::string error;
    CHECK_FALSE(validateProgramReportingDiagnostic(source, "/main", error, diagnosticInfo));
    CHECK_MESSAGE(error.find(expected) != std::string::npos, error);
    CHECK(error.find("unknown method") == std::string::npos);
    REQUIRE(diagnosticInfo.hasPrimarySpan);
    CHECK(diagnosticInfo.primarySpan.line == 11);
    CHECK(diagnosticInfo.primarySpan.column == 26);
  }
  SUBCASE("a lambda nested in a generic Body call") {
    const std::string source = box + R"(
[return<int> effects(none)]
run<Body>([Body mut] body, [Box] box) {
  return(body(box))
}

[return<int> effects(heap_alloc)]
main() {
  [Box] b0{Box{[f] []([i32] x) { return(plus(x, 1i32)) }, [base] 4i32}}
  return(run([]([Box] b) { return(b.f(b.base)) }, b0))
}
)";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK_MESSAGE(error.find(expected) != std::string::npos, error);
  }
}

TEST_CASE("a Function field of a lambda parameter is called through a mut copy, a Reference or a "
          "mut parameter") {
  const std::string box = R"(
[struct]
Box() {
  [Function<i32, i32> mut] f{}
  [i32] base{0i32}
}
)";
  SUBCASE("a mut copy of the field") {
    const std::string source = box + R"(
[return<int> effects(heap_alloc)]
main() {
  [Box] b0{Box{[f] []([i32] x) { return(plus(x, 1i32)) }, [base] 4i32}}
  g{[]([Box] b) {
    [Function<i32, i32> mut] h{b.f}
    return(h(b.base))
  }}
  return(g(b0))
}
)";
    std::string error;
    CHECK_MESSAGE(validateProgram(source, "/main", error), error);
  }
  SUBCASE("a Reference lambda parameter") {
    const std::string source = box + R"(
[return<int> effects(heap_alloc)]
main() {
  [Box] b0{Box{[f] []([i32] x) { return(plus(x, 1i32)) }, [base] 4i32}}
  g{[]([Reference<Box>] b) { return(b.f(b.base)) }}
  return(g(location(b0)))
}
)";
    std::string error;
    CHECK_MESSAGE(validateProgram(source, "/main", error), error);
  }
  SUBCASE("a mut lambda parameter") {
    const std::string source = box + R"(
[return<int> effects(heap_alloc)]
main() {
  [Box mut] b0{Box{[f] []([i32] x) { return(plus(x, 1i32)) }, [base] 4i32}}
  g{[]([Box mut] b) { return(b.f(b.base)) }}
  return(g(b0))
}
)";
    std::string error;
    CHECK_MESSAGE(validateProgram(source, "/main", error), error);
  }
}

TEST_SUITE_END();
