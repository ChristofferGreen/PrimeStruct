#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.move");

TEST_CASE("move marks binding as moved") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [i32] other{move(value)}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("use-after-move: value") != std::string::npos);
}

TEST_CASE("assign reinitializes moved binding") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [i32] other{move(value)}
  assign(value, 2i32)
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("move marks experimental vector binding as moved") {
  const std::string source = R"(
import /std/collections/vector/*

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  [Vector<i32>] moved{move(values)}
  return(/std/collections/vector/count<i32>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("use-after-move: values") != std::string::npos);
}

TEST_CASE("assign reinitializes moved experimental vector binding") {
  const std::string source = R"(
import /std/collections/vector/*

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  [Vector<i32>] moved{move(values)}
  assign(values, vector<i32>(9i32))
  return(plus(/std/collections/vector/count<i32>(moved), /std/collections/vector/count<i32>(values)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("move requires binding name") {
  const std::string source = R"(
[return<int>]
main() {
  move(1i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("move requires a binding name") != std::string::npos);
}

TEST_CASE("move rejects reference bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32> mut] ref{location(value)}
  move(ref)
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("move does not support Reference bindings") != std::string::npos);
}

TEST_CASE("mut parameters borrow a mutable place") {
  // docs/spec/value-lifecycle.md, Parameter Passing: `[T mut]` is a mutable borrow of the argument.
  const std::string accepted = R"(
[return<void>]
bump([i32 mut] value) {
  assign(value, plus(value, 1i32))
}

[return<int>]
seed() {
  return(4i32)
}

[return<int>]
main() {
  [i32 mut] counter{1i32}
  bump(counter)
  bump(seed())
  bump(3i32)
  return(counter)
}
)";
  std::string error;
  CHECK(validateProgram(accepted, "/main", error));
  CHECK(error.empty());

  {
    const std::string rejected = R"(
[return<void>]
bump([i32 mut] value) {
  assign(value, plus(value, 1i32))
}

[return<int>]
main() {
  [i32] frozen{1i32}
  bump(frozen)
  return(frozen)
}
)";
    error.clear();
    CHECK_FALSE(validateProgram(rejected, "/main", error));
    CHECK(error.find("mut parameter requires a mutable place") != std::string::npos);
  }
}

TEST_CASE("a place passed to a mut parameter cannot be passed again in the same call") {
  const std::string source = R"(
[return<void>]
add_into([i32 mut] target, [i32] amount) {
  assign(target, plus(target, amount))
}

[return<int>]
main() {
  [i32 mut] total{1i32}
  add_into(total, total)
  return(total)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrow conflict: total (root: total, sink: target)") != std::string::npos);
}

TEST_CASE("copy and move are parameter modes") {
  const std::string parameters = R"(
[return<int>]
consume([i32 move] value, [i32 copy mut] scratch) {
  assign(scratch, plus(scratch, value))
  return(scratch)
}

[return<int>]
main() {
  return(consume(1i32, 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(parameters, "/main", error));
  CHECK(error.empty());

  const std::string local = R"(
[return<int>]
main() {
  [i32 move] value{1i32}
  return(value)
}
)";
  error.clear();
  CHECK_FALSE(validateProgram(local, "/main", error));
  CHECK(error.find("move transform is only supported on parameters") != std::string::npos);

  const std::string both = R"(
[return<int>]
take([i32 copy move] value) {
  return(value)
}

[return<int>]
main() {
  return(take(1i32))
}
)";
  error.clear();
  CHECK_FALSE(validateProgram(both, "/main", error));
  CHECK(error.find("binding cannot be both copy and move") != std::string::npos);
}

TEST_CASE("a borrowed parameter of an owning type cannot be returned") {
  const std::string owned = R"(
[struct]
Owned() {
  [i32] value{0i32}

  Destroy() {
  }
}

[return<Owned>]
pass_through([Owned] item) {
  return(item)
}

[return<int>]
main() {
  [Owned] item{Owned{}}
  [Owned] other{pass_through(item)}
  return(other.value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(owned, "/main", error));
  CHECK(error.find("borrowed parameter escapes via return: item") != std::string::npos);

  // A `move` parameter owns its value and may hand it back; plain values return by copy.
  std::string moved = owned;
  moved.replace(moved.find("[Owned] item)"), 13, "[Owned move] item)");
  error.clear();
  CHECK(validateProgram(moved, "/main", error));
  CHECK(error.empty());

  const std::string assigned = R"(
[struct]
Owned() {
  [i32] value{0i32}

  Destroy() {
  }
}

[return<void>]
keep([Owned mut] slot, [Owned] item) {
  assign(slot, item)
}

[return<int>]
main() {
  [Owned mut] slot{Owned{}}
  [Owned] item{Owned{}}
  keep(slot, item)
  return(slot.value)
}
)";
  error.clear();
  CHECK_FALSE(validateProgram(assigned, "/main", error));
  CHECK(error.find("borrowed parameter escapes via assignment: item") != std::string::npos);

  const std::string scalar = R"(
[return<int>]
same([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(same(3i32))
}
)";
  error.clear();
  CHECK(validateProgram(scalar, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();
