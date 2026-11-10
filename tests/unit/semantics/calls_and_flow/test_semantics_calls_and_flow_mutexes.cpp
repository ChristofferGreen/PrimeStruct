#include "third_party/doctest.h"

#include "../test_semantics_helpers.h"

// `/std/sync` `Mutex<T>` (docs/spec/stdlib-reference.md, std/sync): `locked` needs the `lock` effect, the element
// must be sendable, the body's borrow stays in the body, and a `Mutex` crosses to a task.

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.mutexes");

namespace {

void checkAccepted(const std::string &source) {
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

void checkRejected(const std::string &source, const std::string &message) {
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_MESSAGE(error.find(message) != std::string::npos, error);
}

} // namespace

TEST_CASE("locked is accepted with the lock effect and returns what its body returns") {
  checkAccepted(R"(
import /std/sync/*

[struct]
Tally() {
  [i64 mut] total{0i64}
}

[effects(lock), return<void>]
record([Mutex<Tally>] tally, [i64] amount) {
  tally.locked([amount]([Reference<Tally>] state) {
    state.total = state.total + amount
  })
}

[effects(lock, heap_alloc), return<int>]
main() {
  tally{Mutex<Tally>{Tally{}}}
  record(tally, 3i64)
  [i64] total{tally.locked([]([Reference<Tally>] state) { return(state.total) })}
  [i64 mut] seen{0i64}
  [mut] look{[ref seen]([Reference<Tally>] state) { seen = state.total }}
  tally.locked(look)
  return(0i32)
}
)");
}

TEST_CASE("locked without the lock effect is rejected") {
  checkRejected(R"(
import /std/sync/*

[effects(heap_alloc), return<int>]
main() {
  cell{Mutex<i64>{1i64}}
  [i64] got{cell.locked([]([Reference<i64>] value) { return(value + 0i64) })}
  return(0i32)
}
)",
                "mutex operation requires lock effect: ");
}

TEST_CASE("a Mutex of a non-sendable builtin is rejected") {
  checkRejected(R"(
import /std/sync/*
import /std/text/*
import /std/text/view/*

[effects(lock, heap_alloc), return<int>]
main() {
  [String] text{"abc"utf8}
  cell{Mutex<StringView>{text.view()}}
  return(0i32)
}
)",
                "Mutex element StringView is not sendable");
}

TEST_CASE("a Mutex of a struct with a Reference field names the field") {
  checkRejected(R"(
import /std/sync/*

[struct]
Record() {
  [Reference<Record>] next
}

[effects(lock, heap_alloc), return<void>]
hold([Reference<Record>] seed) {
  cell{Mutex<Record>{Record{seed}}}
}

[effects(lock, heap_alloc), return<int>]
main() {
  return(0i32)
}
)",
                "Mutex element Record is not sendable: field `next` is Reference<Record>");
}

TEST_CASE("a body that returns its borrowed value is the borrowed-parameter diagnostic") {
  checkRejected(R"(
import /std/collections/*
import /std/sync/*

[effects(lock, heap_alloc), return<int>]
main() {
  cell{Mutex<vector<i64>>{vector<i64>{}}}
  [vector<i64>] got{cell.locked([]([vector<i64>] values) { return(values) })}
  return(0i32)
}
)",
                "borrowed parameter escapes via return: values");
}

TEST_CASE("a spawned call takes a Mutex and a void callee with a closure is a Task<void>") {
  checkAccepted(R"(
import /std/sync/*

[effects(lock), return<void>]
bump([Mutex<i64>] cell) {
  cell.locked([]([Reference<i64> mut] value) { value = value + 1i64 })
}

[effects(task, lock, heap_alloc), return<int>]
main() {
  cell{Mutex<i64>{0i64}}
  [Task<void>] task{[spawn] bump(cell)}
  wait(task)
  return(0i32)
}
)");
}

TEST_CASE("return<auto> forwards a void call and a value call of one closure parameter") {
  // No import, so the helper parses this without the operator filters: canonical calls.
  checkAccepted(R"(
[return<auto>]
apply<Body>([Body mut] body, [i64] value) {
  return(body(value))
}

[return<int>]
main() {
  [i64 mut] seen{0i64}
  [mut] keep{[ref seen]([i64] value) { assign(seen, value) }}
  apply(keep, 4i64)
  [i64] doubled{apply([]([i64] value) { return(multiply(value, 2i64)) }, 4i64)}
  return(0i32)
}
)");
}

TEST_SUITE_END();
