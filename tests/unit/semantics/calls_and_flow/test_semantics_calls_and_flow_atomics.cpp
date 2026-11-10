#include "third_party/doctest.h"

#include "../test_semantics_helpers.h"

// `/std/sync` atomics (docs/spec/stdlib-reference.md, std/sync): every operation of an `Atomic` needs the `atomic`
// effect, an `Atomic` crosses to a task, and only `Atomic<i32>` and `Atomic<i64>` exist.

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.atomics");

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

// A program whose `touch` runs `operation` on an Atomic<i64> named `counter` with the effects `effects`.
std::string touching(const std::string &effects, const std::string &operation) {
  return R"(
import /std/sync/*

[)" + effects +
         R"(return<void>]
touch([Atomic<i64>] counter) {
  )" + operation +
         R"(
}

[effects(atomic, heap_alloc), return<int>]
main() {
  counter{Atomic<i64>{0i64}}
  touch(counter)
  return(0i32)
}
)";
}

} // namespace

TEST_CASE("Atomic<i32> and Atomic<i64> operations are accepted with the atomic effect") {
  checkAccepted(R"(
import /std/sync/*

[effects(atomic, heap_alloc), return<int>]
main() {
  wide{Atomic<i64>{0i64}}
  wide.store(4i64)
  [i64] before{wide.add(1i64)}
  [i64] old{wide.exchange(before)}
  [bool] stored{wide.compareExchange(old, 9i64)}
  small{Atomic<i32>{0i32}}
  small.store(4i32)
  [i32] smallBefore{small.add(1i32)}
  [i32] smallOld{small.exchange(smallBefore)}
  [bool] smallStored{small.compareExchange(smallOld, 9i32)}
  if(stored && smallStored) {
    return(small.load() + convert<i32>(wide.load()))
  }
  return(0i32)
}
)");
}

TEST_CASE("an Atomic crosses to a task") {
  checkAccepted(R"(
import /std/sync/*

[effects(atomic), return<void>]
bump([Atomic<i32>] counter) {
  counter.add(1i32)
}

[effects(atomic, heap_alloc, task), return<int>]
main() {
  counter{Atomic<i32>{0i32}}
  [Task<void>] a{[spawn] bump(counter)}
  [Task<void>] b{[spawn] bump(counter)}
  wait(a)
  wait(b)
  return(counter.load())
}
)");
}

TEST_CASE("every Atomic operation needs the atomic effect") {
  const char *const operations[] = {
      "counter.load()",
      "counter.store(1i64)",
      "counter.add(1i64)",
      "counter.exchange(1i64)",
      "counter.compareExchange(0i64, 1i64)",
  };
  for (const char *operation : operations) {
    CAPTURE(operation);
    checkAccepted(touching("effects(atomic), ", operation));
    checkRejected(touching("", operation), "atomic operation requires atomic effect");
    checkRejected(touching("effects(heap_alloc), ", operation),
                  "atomic operation requires atomic effect");
  }
}

TEST_CASE("the missing atomic effect names the operation and the caller") {
  std::string error;
  CHECK_FALSE(validateProgram(touching("", "counter.add(1i64)"), "/main", error));
  CHECK_MESSAGE(error.find("atomic operation requires atomic effect: /std/sync/Atomic") !=
                    std::string::npos,
                error);
  CHECK_MESSAGE(error.find("/add in /touch") != std::string::npos, error);
}

TEST_CASE("making and copying an Atomic needs no atomic effect") {
  checkAccepted(R"(
import /std/sync/*

[effects(heap_alloc), return<int>]
main() {
  counter{Atomic<i64>{3i64}}
  [Atomic<i64>] shared{counter}
  return(0i32)
}
)");
}

TEST_CASE("Atomic<f32> is rejected") {
  checkRejected(R"(
import /std/sync/*

[effects(atomic, heap_alloc), return<int>]
main() {
  level{Atomic<f32>{0.0f32}}
  return(0i32)
}
)",
                "Atomic requires i32 or i64: Atomic<f32>");
}

TEST_CASE("Atomic<String> is rejected") {
  checkRejected(R"(
import /std/sync/*
import /std/text/*

[effects(atomic, heap_alloc), return<int>]
main() {
  name{Atomic<String>{String{}}}
  return(0i32)
}
)",
                "Atomic requires i32 or i64: Atomic<");
}

TEST_CASE("an Atomic of another integer or of bool is rejected, also as a parameter type") {
  checkRejected(R"(
import /std/sync/*

[effects(atomic, heap_alloc), return<int>]
main() {
  flag{Atomic<bool>{false}}
  return(0i32)
}
)",
                "Atomic requires i32 or i64: Atomic<bool>");
  checkRejected(R"(
import /std/sync/*

[effects(atomic), return<u64>]
read([Atomic<u64>] counter) {
  return(counter.load())
}

[return<int>]
main() {
  return(0i32)
}
)",
                "Atomic requires i32 or i64: Atomic<u64>");
}

TEST_CASE("a host function outside /std/sync takes no Pointer<i64>") {
  checkRejected(R"(
[host return<i64>]
__psatomic_load_i64([Pointer<i64>] cell) {
}

[return<int>]
main() {
  return(0i32)
}
)",
                "host definition parameter must be");
}

TEST_SUITE_END();
