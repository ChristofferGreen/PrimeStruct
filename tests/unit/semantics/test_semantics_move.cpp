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

namespace {

// An owning type (it defines Destroy) and a sink that takes ownership of it.
const char *const OwnedPrelude = R"(
[struct]
Owned() {
  [i32] value{0i32}

  Destroy() {
  }
}

[return<int>]
consume([Owned move] item) {
  return(item.value)
}

[return<int>]
count([i32 move] value) {
  return(value)
}
)";

bool validateWithOwned(const std::string &body, std::string &error) {
  return validateProgram(std::string(OwnedPrelude) + body, "/main", error);
}

} // namespace

TEST_CASE("passing an owning value to a move parameter moves it") {
  std::string error;
  CHECK_FALSE(validateWithOwned(R"(
[return<int>]
main() {
  [Owned] item{Owned{}}
  [i32] first{consume(item)}
  return(plus(first, item.value))
}
)",
                                error));
  CHECK(error.find("use-after-move: item") != std::string::npos);

  // Values of other types are copied; a moved binding can be reassigned.
  error.clear();
  CHECK(validateWithOwned(R"(
[return<int>]
main() {
  [i32] number{3i32}
  [Owned mut] item{Owned{}}
  [i32] first{consume(item)}
  assign(item, Owned{})
  return(plus(plus(first, count(number)), plus(number, item.value)))
}
)",
                          error));
  CHECK(error.empty());

  error.clear();
  CHECK_FALSE(validateWithOwned(R"(
[return<int>]
forward([Owned] item) {
  return(consume(item))
}

[return<int>]
main() {
  [Owned] item{Owned{}}
  return(forward(item))
}
)",
                                error));
  CHECK(error.find("borrowed parameter cannot be moved: item") != std::string::npos);
}

TEST_CASE("moves are tracked per control-flow path") {
  std::string error;
  // A move in one branch does not affect the other.
  CHECK(validateWithOwned(R"(
[return<int>]
main() {
  [Owned] item{Owned{}}
  [i32 mut] result{0i32}
  if(true, then() { assign(result, consume(item)) }, else() { assign(result, consume(item)) })
  return(result)
}
)",
                          error));
  CHECK(error.empty());

  // A move in a branch that falls through reaches the code after the `if`.
  error.clear();
  CHECK_FALSE(validateWithOwned(R"(
[return<int>]
main() {
  [Owned] item{Owned{}}
  [i32 mut] result{0i32}
  if(true, then() { assign(result, consume(item)) }, else() { assign(result, 1i32) })
  return(plus(result, item.value))
}
)",
                                error));
  CHECK(error.find("use-after-move: item") != std::string::npos);

  // A branch that returns does not.
  error.clear();
  CHECK(validateWithOwned(R"(
[return<int>]
main() {
  [Owned] item{Owned{}}
  if(true, then() { return(consume(item)) }, else() { })
  return(item.value)
}
)",
                          error));
  CHECK(error.empty());
}

TEST_CASE("a loop cannot move a binding from outside it") {
  std::string error;
  CHECK_FALSE(validateWithOwned(R"(
[return<int>]
main() {
  [Owned] item{Owned{}}
  [i32 mut] total{0i32}
  [i32 mut] round{0i32}
  while(less_than(round, 2i32)) {
    assign(total, plus(total, consume(item)))
    assign(round, plus(round, 1i32))
  }
  return(total)
}
)",
                                error));
  CHECK(error.find("use-after-move: item (moved in an earlier loop iteration)") !=
        std::string::npos);

  error.clear();
  CHECK(validateWithOwned(R"(
[return<int>]
main() {
  [Owned mut] item{Owned{}}
  [i32 mut] total{0i32}
  [i32 mut] round{0i32}
  while(less_than(round, 2i32)) {
    assign(total, plus(total, consume(item)))
    assign(item, Owned{})
    assign(round, plus(round, 1i32))
  }
  return(total)
}
)",
                          error));
  CHECK(error.empty());
}

TEST_CASE("changing a container while a view of it is used later is rejected") {
  const std::string pushWhileViewed = R"(
import /std/collections/*

[effects(heap_alloc) return<int>]
main() {
  [Vector<i32> mut] items{vector<i32>()}
  vectorPush<i32>(items, 1i32)
  [Vector<i32>] view{items}
  vectorPush<i32>(items, 2i32)
  return(vectorCount<i32>(view))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(pushWhileViewed, "/main", error));
  CHECK(error.find("borrowed binding: items (root: items, sink: view)") != std::string::npos);

  const std::string fieldWhileViewed = R"(
import /std/collections/*

[struct]
Bag() {
  [Vector<i32> mut] items{vector<i32>()}
}

[effects(heap_alloc) return<int>]
main() {
  [Bag mut] bag{Bag{}}
  [Vector<i32>] view{bag.items}
  vectorPush<i32>(bag.items, 2i32)
  return(vectorCount<i32>(view))
}
)";
  error.clear();
  CHECK_FALSE(validateProgram(fieldWhileViewed, "/main", error));
  CHECK(error.find("borrowed binding: bag (root: bag, sink: view)") != std::string::npos);
}

TEST_CASE("a container view ends at its last use") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc) return<int>]
main() {
  [Vector<i32> mut] items{vector<i32>()}
  vectorPush<i32>(items, 1i32)
  [Vector<i32>] view{items}
  [i32] seen{vectorCount<i32>(view)}
  vectorPush<i32>(items, 2i32)
  [Vector<i32> mut] copy{items}
  vectorPush<i32>(items, 3i32)
  return(plus(seen, vectorCount<i32>(copy)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

namespace {

// Builtin vectors whose elements own resources: vector<vector<i32>> and a vector of a struct
// holding a String.
const char *const PushPrelude = R"(
import /std/collections/*
import /std/text/*

[struct]
Named {
  [i32] code{0i32}
  [String] name{String{}}
}
)";

bool validateWithPush(const std::string &body, std::string &error) {
  return validateProgram(std::string(PushPrelude) + body, "/main", error);
}

} // namespace

TEST_CASE("pushing a named owner into a builtin vector moves it") {
  std::string error;
  CHECK_FALSE(validateWithPush(R"(
[effects(heap_alloc) return<int>]
main() {
  [vector<vector<i32>> mut] rows{vector<vector<i32>>()}
  [vector<i32> mut] row{vector<i32>(1i32, 2i32)}
  rows.push(row)
  row.push(3i32)
  return(row.count())
}
)",
                               error));
  CHECK(error.find("use-after-move: row was moved at 15:13; assign row a new value before using it "
                   "again, or "
                   "keep a copy before the move: [vector<i32> mut] kept{row}") !=
        std::string::npos);

  error.clear();
  CHECK_FALSE(validateWithPush(R"(
[effects(heap_alloc) return<int>]
main() {
  [vector<Named> mut] names{vector<Named>()}
  [Named mut] named{Named{[code] 4i32, [name] stringFrom("abc")}}
  names.push(named)
  return(named.code)
}
)",
                               error));
  CHECK(error.find("use-after-move: named was moved at 15:14") != std::string::npos);

  // The direct-call spelling moves too.
  error.clear();
  CHECK_FALSE(validateWithPush(R"(
[effects(heap_alloc) return<int>]
main() {
  [vector<vector<i32>> mut] rows{vector<vector<i32>>()}
  [vector<i32> mut] row{vector<i32>(1i32)}
  push(rows, row)
  return(row.count())
}
)",
                               error));
  CHECK(error.find("use-after-move: row") != std::string::npos);
}

TEST_CASE("a pushed owner can be reassigned, and views and borrowed values are copied") {
  std::string error;
  CHECK(validateWithPush(R"(
[effects(heap_alloc) return<int>]
add_param([vector<vector<i32>> mut] rows, [vector<i32>] row) {
  rows.push(row)
  return(row.count())
}

[effects(heap_alloc) return<int>]
main() {
  [vector<vector<i32>> mut] rows{vector<vector<i32>>()}
  [vector<i32> mut] row{vector<i32>(1i32, 2i32)}
  rows.push(row)
  row = vector<i32>(5i32)
  row.push(6i32)
  [vector<i32>] view{rows[0i32]}
  [vector<vector<i32>> mut] other{vector<vector<i32>>()}
  other.push(view)
  [vector<Named> mut] names{vector<Named>()}
  [Named mut] named{Named{[code] 4i32, [name] stringFrom("abc")}}
  names.push(named)
  assign(named, Named{[code] 5i32, [name] stringFrom("def")})
  return(row.count() + view.count() + add_param(other, view) + named.code)
}
)",
                         error));
  CHECK(error.empty());
}

TEST_CASE("a push moves on every control-flow path") {
  std::string error;
  // Moved in one branch, used after the join.
  CHECK_FALSE(validateWithPush(R"(
[effects(heap_alloc) return<int>]
main() {
  [vector<vector<i32>> mut] rows{vector<vector<i32>>()}
  [vector<i32> mut] row{vector<i32>(1i32)}
  if(rows.count() == 0i32) {
    rows.push(row)
  } else {
  }
  return(row.count())
}
)",
                               error));
  CHECK(error.find("use-after-move: row was moved at 16:15") != std::string::npos);

  // Moved in an earlier loop iteration.
  error.clear();
  CHECK_FALSE(validateWithPush(R"(
[effects(heap_alloc) return<int>]
main() {
  [vector<vector<i32>> mut] rows{vector<vector<i32>>()}
  [vector<i32> mut] row{vector<i32>(1i32)}
  repeat(3i32) {
    rows.push(row)
  }
  return(rows.count())
}
)",
                               error));
  CHECK(error.find("use-after-move: row (moved in an earlier loop iteration) at 16:15; assign row "
                   "a new value "
                   "later in the loop body") != std::string::npos);

  // Moved in both branches, or given a new value in the loop: accepted.
  error.clear();
  CHECK(validateWithPush(R"(
[effects(heap_alloc) return<int>]
main() {
  [vector<vector<i32>> mut] rows{vector<vector<i32>>()}
  [vector<i32> mut] row{vector<i32>(1i32)}
  if(rows.count() == 0i32) {
    rows.push(row)
  } else {
    rows.push(row)
  }
  repeat(3i32) {
    row = vector<i32>(2i32)
    rows.push(row)
  }
  return(rows.count())
}
)",
                         error));
  CHECK(error.empty());
}

TEST_CASE("move of a moved binding names where it was moved") {
  std::string error;
  CHECK_FALSE(validateWithPush(R"(
[effects(heap_alloc) return<int>]
main() {
  [vector<i32> mut] row{vector<i32>(1i32)}
  [vector<i32> mut] first{move(row)}
  [vector<i32> mut] second{move(row)}
  return(first.count() + second.count())
}
)",
                               error));
  CHECK(error.find("use-after-move: row was moved at 14:32") != std::string::npos);

  // A returned move(x) is validated twice (return inference) and is still one move.
  error.clear();
  CHECK(validateWithPush(R"(
[effects(heap_alloc) return<vector<i32>>]
pass_through() {
  [vector<i32> mut] row{vector<i32>(1i32)}
  return(move(row))
}

[effects(heap_alloc) return<int>]
main() {
  return(pass_through().count())
}
)",
                         error));
  CHECK(error.empty());
}

TEST_CASE("a builtin vector cannot change while a view of it or of an element is used") {
  std::string error;
  CHECK_FALSE(validateWithPush(R"(
[effects(heap_alloc) return<int>]
main() {
  [vector<vector<i32>> mut] rows{vector<vector<i32>>()}
  rows.push(vector<i32>(1i32, 2i32))
  [vector<i32>] view{rows[0i32]}
  rows.push(vector<i32>(3i32))
  return(view.count())
}
)",
                               error));
  CHECK(error.find("borrowed binding: rows (root: rows, sink: view); rows cannot change while view "
                   "still reads it: declare view `mut` to give it its own copy") !=
        std::string::npos);

  // Pushing a view into the vector it reads.
  error.clear();
  CHECK_FALSE(validateWithPush(R"(
[effects(heap_alloc) return<int>]
main() {
  [vector<vector<i32>> mut] rows{vector<vector<i32>>()}
  rows.push(vector<i32>(1i32, 2i32))
  [vector<i32>] view{rows[0i32]}
  rows.push(view)
  return(rows.count())
}
)",
                               error));
  CHECK(error.find("borrowed binding: rows (root: rows, sink: view)") != std::string::npos);

  // A view of an element of a field borrows the struct.
  error.clear();
  CHECK_FALSE(validateWithPush(R"(
[struct]
Grid {
  [vector<vector<i32>> mut] rows
}

[effects(heap_alloc) return<int>]
main() {
  [Grid mut] grid{Grid{[rows] vector<vector<i32>>()}}
  grid.rows.push(vector<i32>(1i32))
  [vector<i32>] view{grid.rows[0i32]}
  grid.rows.push(vector<i32>(2i32))
  return(view.count())
}
)",
                               error));
  CHECK(error.find("borrowed binding: grid (root: grid, sink: view)") != std::string::npos);
}

TEST_CASE("a builtin vector can push its own elements and views that ended") {
  std::string error;
  CHECK(validateWithPush(R"(
[effects(heap_alloc) return<int>]
main() {
  [vector<vector<i32>> mut] rows{vector<vector<i32>>()}
  rows.push(vector<i32>(1i32, 2i32))
  rows.push(rows[0i32])
  [vector<i32>] view{rows[0i32]}
  [i32] seen{view.count()}
  rows.push(vector<i32>(3i32))
  [vector<i32> mut] copied{rows[0i32]}
  rows.push(vector<i32>(4i32))
  rows.push(copied)
  [vector<Named> mut] names{vector<Named>()}
  names.push(Named{[code] 4i32, [name] stringFrom("abc")})
  names.push(names[0i32])
  return(seen + rows.count() + names.count())
}
)",
                         error));
  CHECK(error.empty());
}

TEST_CASE("an inner vector changes in place only through a mutable root") {
  std::string error;
  CHECK(validateWithPush(R"(
[struct]
Grid {
  [vector<vector<i32>> mut] rows

  [effects(heap_alloc)]
  grow([i32] row) {
    this.rows[row].push(1i32)
    this.rows[row].pop()
  }
}

[effects(heap_alloc) return<int>]
main() {
  [vector<vector<i32>> mut] rows{vector<vector<i32>>()}
  rows.push(vector<i32>(1i32))
  rows[0i32].push(2i32)
  rows[0i32].pop()
  rows[0i32].clear()
  [Grid mut] grid{Grid{[rows] vector<vector<i32>>()}}
  grid.rows.push(vector<i32>(1i32))
  grid.rows[0i32].push(3i32)
  grid.grow(0i32)
  return(rows[0i32].count() + grid.rows[0i32].count())
}
)",
                         error));
  CHECK(error.empty());

  error.clear();
  CHECK_FALSE(validateWithPush(R"(
[effects(heap_alloc) return<int>]
peek([vector<vector<i32>>] rows) {
  rows[0i32].push(1i32)
  return(rows.count())
}

[effects(heap_alloc) return<int>]
main() {
  [vector<vector<i32>> mut] rows{vector<vector<i32>>()}
  rows.push(vector<i32>(1i32))
  return(peek(rows))
}
)",
                               error));
  CHECK(error.find(
            "push changes rows, which is not mutable: declare it [vector<vector<i32>> mut] rows") !=
        std::string::npos);

  error.clear();
  CHECK_FALSE(validateWithPush(R"(
[effects(heap_alloc) return<int>]
main() {
  [vector<vector<i32>> mut] rows{vector<vector<i32>>()}
  rows.push(vector<i32>(1i32))
  [vector<i32>] view{rows[0i32]}
  rows[0i32].push(2i32)
  return(view.count())
}
)",
                               error));
  CHECK(error.find("borrowed binding: rows (root: rows, sink: view)") != std::string::npos);
}

TEST_SUITE_END();
