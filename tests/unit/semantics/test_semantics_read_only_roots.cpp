#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.read_only_roots");

// A write to a field of, or a `[mut]` method call on, a read-only root (a local that is not `mut`, a `[T]` or
// `[T copy]` parameter, the `this` of a method that is not `[mut]`) is a diagnostic that names the fix
// (docs/spec/value-lifecycle.md, Parameter modes). A `Reference<T>` carries its own write access.

namespace {

const char *CounterStruct = R"(
[struct]
Counter() {
  [i32 mut] total{0i32}
  [mut] bump() {
    this.total = this.total + 1i32
  }
  [return<int>] peek() {
    return(this.total)
  }
}
)";

void checkRejectedAt(const std::string &body, const std::string &expected, int line, int column) {
  const std::string source = std::string(CounterStruct) + body;
  primec::SemanticDiagnosticInfo diagnosticInfo;
  std::string error;
  CHECK_FALSE(validateProgramReportingDiagnostic(source, "/main", error, diagnosticInfo));
  CHECK_MESSAGE(error.find(expected) != std::string::npos, error);
  REQUIRE(diagnosticInfo.hasPrimarySpan);
  CHECK(diagnosticInfo.primarySpan.line == line);
  CHECK(diagnosticInfo.primarySpan.column == column);
}

void checkAccepted(const std::string &body) {
  primec::SemanticDiagnosticInfo diagnosticInfo;
  std::string error;
  CHECK_MESSAGE(validateProgramReportingDiagnostic(std::string(CounterStruct) + body, "/main", error, diagnosticInfo),
                error);
}

// An element of a collection is part of the collection's owner, so `bag.items[i]` has the root of `bag.items`.
const char *BagProgram = R"(import /std/collections/*

[struct]
Item() {
  [i32 mut] n{0i32}
  [mut] bump() {
    this.n = this.n + 1i32
  }
}

[struct]
Bag() {
  [vector<Item> mut] items{vector<Item>{}}
  [array<Item> mut] fixed{array<Item>{Item{}}}
  [Vector<Item> mut] grown{/std/collections/vector/vector<Item>()}
  [vector<vector<Item>> mut] nested{vector<vector<Item>>{}}
}

[return<Bag>]
make_bag() {
  return(Bag{})
}

[return<int>]
main() {
  return(0i32)
}
)";

// `body` starts on line 28 of the program.
void checkBagRejectedAt(const std::string &body,
                        const std::string &expected,
                        int line,
                        int column) {
  primec::SemanticDiagnosticInfo diagnosticInfo;
  std::string error;
  CHECK_FALSE(validateProgramReportingDiagnostic(
      std::string(BagProgram) + body, "/main", error, diagnosticInfo));
  CHECK_MESSAGE(error.find(expected) != std::string::npos, error);
  REQUIRE(diagnosticInfo.hasPrimarySpan);
  CHECK(diagnosticInfo.primarySpan.line == line);
  CHECK(diagnosticInfo.primarySpan.column == column);
}

void checkBagAccepted(const std::string &body) {
  primec::SemanticDiagnosticInfo diagnosticInfo;
  std::string error;
  CHECK_MESSAGE(validateProgramReportingDiagnostic(
                    std::string(BagProgram) + body, "/main", error, diagnosticInfo),
                error);
}

} // namespace

TEST_CASE("a field is not written through a read-only parameter") {
  checkRejectedAt(R"(
[return<void>]
write([Counter] counter) {
  counter.total = 5i32
}

[return<int>]
main() {
  return(0i32)
}
)",
                  "assign target must be a mutable binding: counter.total (counter is a read-only parameter; "
                  "take it as a [T mut] parameter)",
                  15, 3);
}

TEST_CASE("a mut method is not called on a read-only parameter") {
  checkRejectedAt(R"(
[return<void>]
bump([Counter] counter) {
  counter.bump()
}

[return<int>]
main() {
  return(0i32)
}
)",
                  "mutating method needs a mutable receiver: counter.bump() (counter is a read-only parameter; "
                  "take it as a [T mut] parameter)",
                  15, 3);
}

TEST_CASE("a copy parameter is read-only too") {
  checkRejectedAt(R"(
[return<void>]
write([Counter copy] counter) {
  counter.total = 5i32
}

[return<int>]
main() {
  return(0i32)
}
)",
                  "counter is a read-only parameter", 15, 3);
}

TEST_CASE("a field is not written and a mut method is not called on a local that is not mut") {
  checkRejectedAt(R"(
[return<int>]
main() {
  [Counter] counter{Counter{}}
  counter.total = 3i32
  return(0i32)
}
)",
                  "assign target must be a mutable binding: counter.total (counter is read-only; declare it with "
                  "mut: [T mut] counter{...})",
                  16, 3);
  checkRejectedAt(R"(
[return<int>]
main() {
  [Counter] counter{Counter{}}
  counter.bump()
  return(0i32)
}
)",
                  "mutating method needs a mutable receiver: counter.bump() (counter is read-only; declare it "
                  "with mut: [T mut] counter{...})",
                  16, 3);
}

TEST_CASE("a nested field is not written through a read-only root") {
  checkRejectedAt(R"(
[struct]
Pair() {
  [Counter mut] first{Counter{}}
}

[return<int>]
main() {
  [Pair] pair{Pair{}}
  pair.first.total = 3i32
  return(0i32)
}
)",
                  "assign target must be a mutable binding: pair.first.total (pair is read-only", 21, 3);
}

TEST_CASE("a lambda parameter without mut is read-only") {
  checkRejectedAt(R"(
[effects(heap_alloc) return<int>]
main() {
  [Function<void, Counter>] f{[]([Counter] counter) { counter.bump() }}
  return(0i32)
}
)",
                  "mutating method needs a mutable receiver: counter.bump() (counter is a read-only parameter",
                  15, 55);
}

TEST_CASE("a method that is not mut cannot write this") {
  const std::string source = R"(
[struct]
Counter() {
  [i32 mut] total{0i32}
  [return<void>] reset() {
    this.total = 0i32
  }
}

[return<int>]
main() {
  return(0i32)
}
)";
  primec::SemanticDiagnosticInfo diagnosticInfo;
  std::string error;
  CHECK_FALSE(validateProgramReportingDiagnostic(source, "/main", error, diagnosticInfo));
  CHECK_MESSAGE(error.find("this is read-only in a method that is not [mut]; mark the method [mut]") !=
                    std::string::npos,
                error);
  REQUIRE(diagnosticInfo.hasPrimarySpan);
  CHECK(diagnosticInfo.primarySpan.line == 6);
}

TEST_CASE("mut roots, mut parameters and references still write and call mut methods") {
  checkAccepted(R"(
[return<void>]
write_param([Counter mut] counter) {
  counter.total = 5i32
  counter.bump()
}

[return<void>]
write_reference([Reference<Counter>] counter) {
  counter.total = 5i32
  counter.bump()
}

[struct]
Holder() {
  [Reference<Counter> mut] kept
  [mut] bumpKept() {
    this.kept.bump()
    this.kept.total = 2i32
  }
}

[return<int>]
main() {
  [Counter mut] counter{Counter{}}
  counter.total = 3i32
  counter.bump()
  write_param(counter)
  write_reference(location(counter))
  [Reference<Counter>] alias{location(counter)}
  alias.total = 4i32
  alias.bump()
  return(0i32)
}
)");
}

TEST_CASE("lifecycle helpers and mut methods write this") {
  checkAccepted(R"(
[struct]
Cell() {
  [i32 mut] value{0i32}
  Create() {
    this.value = 1i32
  }
  [mut] set([i32] next) {
    this.value = next
    this.set2(next)
  }
  [mut] set2([i32] next) {
    this.value = next
  }
}

[return<int>]
main() {
  [Cell mut] cell{Cell{}}
  cell.set(2i32)
  return(0i32)
}
)");
}

TEST_CASE("reading through a read-only root and calling a plain method are accepted") {
  checkAccepted(R"(
[return<int>]
read([Counter] counter) {
  [i32] seen{counter.total}
  return(plus(seen, counter.peek()))
}

[return<int>]
main() {
  [Counter] counter{Counter{}}
  return(minus(read(counter), 0i32))
}
)");
}

TEST_CASE("a field of a vector element is not written through a read-only parameter") {
  checkBagRejectedAt(
      R"([effects(heap_alloc) return<void>]
write([Bag] bag) {
  bag.items[0i32].n = 5i32
}
)",
      "assign target must be a mutable binding: bag.items[...].n (bag is a read-only parameter; "
      "take it as a [T mut] parameter)",
      30,
      3);
}

TEST_CASE("a mut method is not called on a vector element of a read-only parameter") {
  checkBagRejectedAt(
      R"([effects(heap_alloc) return<void>]
bump([Bag] bag) {
  bag.items[0i32].bump()
}
)",
      "mutating method needs a mutable receiver: bag.items[...].bump() (bag is a read-only "
      "parameter; take it as a [T mut] parameter)",
      30,
      3);
  checkBagRejectedAt(
      R"([effects(heap_alloc) return<void>]
bump([Bag] bag) {
  bag.items.at(0i32).bump()
}
)",
      "mutating method needs a mutable receiver: bag.items[...].bump() (bag is a read-only",
      30,
      3);
}

TEST_CASE("an element of an array field and of a Vector field has the root of the field") {
  checkBagRejectedAt(
      R"([effects(heap_alloc) return<void>]
write([Bag] bag) {
  bag.fixed[0i32].n = 5i32
}
)",
      "assign target must be a mutable binding: bag.fixed[...].n (bag is a read-only parameter",
      30,
      3);
  checkBagRejectedAt(
      R"([effects(heap_alloc) return<void>]
bump([Bag] bag) {
  bag.fixed[0i32].bump()
}
)",
      "mutating method needs a mutable receiver: bag.fixed[...].bump() (bag is a read-only",
      30,
      3);
  checkBagRejectedAt(
      R"([effects(heap_alloc) return<void>]
write([Bag] bag) {
  bag.grown[0i32].n = 5i32
}
)",
      "assign target must be a mutable binding: bag.grown[...].n (bag is a read-only parameter",
      30,
      3);
  checkBagRejectedAt(
      R"([effects(heap_alloc) return<void>]
bump([Bag] bag) {
  bag.grown[0i32].bump()
}
)",
      "mutating method needs a mutable receiver: bag.grown[...].bump() (bag is a read-only",
      30,
      3);
}

TEST_CASE("an element of an element, of a local and of this has the same root") {
  checkBagRejectedAt(
      R"([effects(heap_alloc) return<void>]
bump([Bag] bag) {
  bag.nested[0i32][1i32].bump()
}
)",
      "mutating method needs a mutable receiver: bag.nested[...][...].bump() (bag is a read-only",
      30,
      3);
  checkBagRejectedAt(
      R"([effects(heap_alloc) return<void>]
bump() {
  [vector<Item>] items{vector<Item>{}}
  items[0i32].bump()
}
)",
      "mutating method needs a mutable receiver: items[...].bump() (items is read-only; declare it "
      "with mut: [T mut] items{...})",
      31,
      3);
  checkBagRejectedAt(
      R"([struct]
Shelf() {
  [vector<Item> mut] items{vector<Item>{}}
  [effects(heap_alloc)] poke() {
    this.items[0i32].n = 2i32
  }
}
)",
      "assign target must be a mutable binding: this.items[...].n (this is read-only in a method "
      "that is not [mut]; mark the method [mut])",
      32,
      5);
}

TEST_CASE("elements reached through mut, a Reference or a call result are written") {
  checkBagAccepted(R"([effects(heap_alloc) return<void>]
write_mut([Bag mut] bag) {
  bag.items[0i32].n = 5i32
  bag.items[0i32].bump()
  bag.fixed[0i32].bump()
  bag.grown[0i32].bump()
  bag.nested[0i32][1i32].bump()
}

[effects(heap_alloc) return<void>]
write_reference([Reference<Bag>] bag) {
  bag.items[0i32].n = 5i32
  bag.items[0i32].bump()
}

[effects(heap_alloc) return<void>]
write_references([args<Reference<Item>>] values) {
  values[0i32].n = 5i32
  values[0i32].bump()
}

[effects(heap_alloc) return<int>]
read_and_write_temporary([Bag] bag) {
  make_bag().items[0i32].bump()
  [Bag mut] own{Bag{}}
  own.items[0i32].bump()
  return(bag.items[0i32].n)
}
)");
}

// The `call` of a `Function` is `mut`, and so is the `call` of a closure with a `mut` value capture
// (docs/spec/value-lifecycle.md, Function values): `f(x)` is `f.call(x)` and needs a mutable receiver.
TEST_CASE("a Function is not called through a read-only local or parameter") {
  checkRejectedAt(
      R"(
[return<int> effects(heap_alloc)]
main() {
  [Function<i32, i32>] g{[mut n{0i32}]([i32] x) {
    n = plus(n, 1i32)
    return(plus(x, n))
  }}
  return(g(1i32))
}
)",
      "mutating method needs a mutable receiver: g.call() (g is read-only; declare it with mut: "
      "[T mut] g{...})",
      19,
      10);
  checkRejectedAt(R"(
[return<int>]
apply([Function<i32, i32>] g) {
  return(g(1i32))
}

[return<int>]
main() {
  return(0i32)
}
)",
                  "mutating method needs a mutable receiver: g.call() (g is a read-only parameter; "
                  "take it as a "
                  "[T mut] parameter, or call a mut copy: [Function<...> mut] h{g})",
                  15,
                  10);
  checkRejectedAt(R"(
[return<int>]
apply_owned([Function<i32, i32> move] g) {
  return(g(1i32))
}

[return<int>]
main() {
  return(0i32)
}
)",
                  "g is a read-only parameter; take it as a [T move mut] parameter",
                  15,
                  10);
}

TEST_CASE("a Function field is not called through a read-only struct or this") {
  const std::string holder = R"(
[struct]
Holder() {
  [Function<i32, i32> mut] f{}
  [return<int>] run() {
    return(this.f(1i32))
  }
}
)";
  checkRejectedAt(
      holder + R"(
[return<int>]
main() {
  return(0i32)
}
)",
      "mutating method needs a mutable receiver: this.f.call() (this is read-only in a method that "
      "is "
      "not [mut]; mark the method [mut], or call a mut copy: [Function<...> mut] h{this.f})",
      17,
      12);
  checkRejectedAt(R"(
[struct]
Box() {
  [Function<i32, i32> mut] f{}
}

[return<int> effects(heap_alloc)]
main() {
  [Box] box{Box{[f] []([i32] x) { return(plus(x, 1i32)) }}}
  return(box.f(1i32))
}
)",
                  "mutating method needs a mutable receiver: box.f.call() (box is read-only; "
                  "declare it with mut: "
                  "[T mut] box{...}, or call a mut copy: [Function<...> mut] h{box.f})",
                  21,
                  10);
}

TEST_CASE("a closure with mut state and a captured Function need a mutable place to be called") {
  checkRejectedAt(R"(
[return<int>]
main() {
  [auto] counter{[mut n{0i32}]() {
    n = plus(n, 1i32)
    return(n)
  }}
  return(counter())
}
)",
                  "mutating method needs a mutable receiver: counter.call() (counter is read-only; "
                  "declare it with "
                  "mut: [T mut] counter{...})",
                  19,
                  10);
  checkRejectedAt(
      R"(
[return<int> effects(heap_alloc)]
main() {
  [Function<i32, i32> mut] next{[]([i32] x) { return(plus(x, 1i32)) }}
  [Function<i32, i32> mut] outer{[inner{next}]([i32] x) { return(inner(x)) }}
  return(outer(1i32))
}
)",
      "mutating method needs a mutable receiver: inner.call() (inner is a capture that is not mut; "
      "capture it with mut: [mut inner])",
      16,
      66);
}

TEST_CASE("a Function is called through mut, a mut copy, a Reference and a mut or ref capture") {
  checkAccepted(R"(
[struct]
Box() {
  [Function<i32, i32> mut] f{}
  [mut return<int>] run() {
    return(this.f(1i32))
  }
}

[return<int>]
apply([Function<i32, i32> mut] g) {
  return(g(1i32))
}

[return<int>]
apply_owned([Function<i32, i32> move mut] g) {
  return(g(1i32))
}

[return<int>]
apply_reference([Reference<Function<i32, i32>>] g) {
  return(g(1i32))
}

[return<int> effects(heap_alloc)]
apply_copy([Box] box) {
  [Function<i32, i32> mut] h{box.f}
  return(h(1i32))
}

[return<int> effects(heap_alloc)]
main() {
  [Function<i32, i32> mut] g{[mut n{0i32}]([i32] x) {
    n = plus(n, 1i32)
    return(plus(x, n))
  }}
  [Function<i32, i32> mut] wrapped{[mut inner{g}]([i32] x) { return(inner(x)) }}
  [auto] borrowed{[ref g]([i32] x) { return(g(x)) }}
  [Box mut] box{Box{[f] []([i32] x) { return(plus(x, 1i32)) }}}
  return(plus(plus(g(1i32), wrapped(1i32)), plus(plus(borrowed(1i32), box.f(1i32)), plus(box.run(), apply_copy(box)))))
}
)");
}

TEST_CASE("a closure without mut state is called through a read-only binding") {
  checkAccepted(R"(
[return<int>]
drive<Handler>([Handler] handler) {
  return(handler(2i32))
}

[return<int>]
main() {
  [i32] base{3i32}
  [auto] add{[base]([i32] x) { return(plus(x, base)) }}
  return(plus(add(1i32), drive(add)))
}
)");
}

TEST_SUITE_END();
