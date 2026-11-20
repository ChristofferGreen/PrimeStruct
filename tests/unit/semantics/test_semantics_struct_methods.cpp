#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.struct_methods");

// Method calls inside the methods of a generic struct run in the specialization's own namespace; the receiver's type
// is still found where it is declared (docs/spec/language-core.md, Helper overloading).

TEST_CASE("a method named like a collection helper resolves on a receiver in a generic struct's method") {
  const std::string source = R"(
import /std/collections/*

[struct]
Cell() {
  [i32 mut] n{5i32}

  [mut]
  clear() {
    this.n = 0i32
  }
}

[struct]
Holder<T>() {
  [vector<i32> mut] items{vector<i32>()}
  [Cell mut] own{Cell{}}

  [mut]
  wipeParam([Reference<Cell>] cell) {
    cell.clear()
  }

  [mut]
  wipeValue([Cell mut] cell) {
    cell.clear()
  }

  [mut]
  wipeLocal() {
    [Cell mut] mine{Cell{}}
    mine.clear()
  }

  [mut effects(heap_alloc)]
  wipeField() {
    this.items.clear()
    this.own.clear()
  }
}

[return<int> effects(heap_alloc)]
main() {
  [Holder<i32> mut] holder{Holder<i32>{}}
  [Cell mut] cell{Cell{}}
  holder.wipeParam(cell)
  holder.wipeValue(cell)
  holder.wipeLocal()
  holder.wipeField()
  return(cell.n)
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

TEST_CASE("a method named like a collection helper resolves in a generic struct of an imported namespace") {
  const std::string source = R"(
import /std/collections/*

namespace lib {
  [public struct]
  Cell() {
    [public i32 mut] n{5i32}

    [public mut]
    clear() {
      this.n = 0i32
    }
  }

  [public struct]
  Holder<T>() {
    [public mut]
    wipeParam([Reference<Cell>] cell) {
      cell.clear()
    }
  }
}

import /lib/*

[return<int>]
main() {
  [Holder<i32> mut] holder{Holder<i32>{}}
  [Cell mut] cell{Cell{}}
  holder.wipeParam(cell)
  return(cell.n)
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

TEST_CASE("an unknown method on a reference parameter of a generic struct's method is still rejected") {
  const std::string source = R"(
import /std/collections/*

[struct]
Cell() {
  [i32 mut] n{5i32}

  [mut]
  wipe() {
    this.n = 0i32
  }
}

[struct]
Holder<T>() {
  [mut]
  wipeParam([Reference<Cell>] cell) {
    cell.clear()
  }
}

[return<int>]
main() {
  [Holder<i32> mut] holder{Holder<i32>{}}
  [Cell mut] cell{Cell{}}
  holder.wipeParam(cell)
  return(cell.n)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_MESSAGE(error.find("clear") != std::string::npos, error);
}

TEST_CASE("same-arity struct method overloads resolve by a literal, a local, a parameter and a call") {
  const std::string source = R"(
import /std/collections/*

[struct]
Tag() {
  [i32] n{0i32}
}

[struct]
Box() {
  [return<int>]
  add([string] label, [Tag] tag) {
    return(1i32)
  }

  [return<int>]
  add([string] label, [i32] count) {
    return(2i32)
  }
}

[struct]
Holder<T>() {
  [T mut] total{}

  [mut]
  on([Tag] tag, [i32] weight) {
  }

  [mut]
  on([Box] box, [i32] weight) {
  }
}

[return<Tag>]
make_tag() {
  return(Tag{})
}

[return<int>]
add_tag([Box] box, [Tag] tag) {
  return(box.add("a", tag))
}

[return<int>]
main() {
  [Box] box{Box{}}
  [Tag] tag{Tag{}}
  [Holder<i32> mut] holder{Holder<i32>{}}
  holder.on(tag, 1i32)
  holder.on(box, 2i32)
  return(box.add("a", 3i32) + box.add("a", tag) + box.add("a", make_tag()) + add_tag(box, tag))
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

TEST_CASE("a struct method call that fits no overload, or more than one, is rejected") {
  const std::string common = R"(
import /std/collections/*

[struct]
Tag() {
  [i32] n{0i32}
}

[struct]
Box() {
  [return<int>]
  add([string] label, [Tag] tag) {
    return(1i32)
  }

  [return<int>]
  add([string] label, [i32] count) {
    return(2i32)
  }
}

[return<auto>]
unknown_type() {
  return(3i32)
}
)";
  std::string error;
  const std::string noViable = common + R"(
[return<int>]
main() {
  [Box] box{Box{}}
  return(box.add("a", 2.5f32))
}
)";
  CHECK_FALSE(validateProgram(noViable, "/main", error));
  CHECK_MESSAGE(error.find("no viable overload for /Box/add") != std::string::npos, error);
  CHECK_MESSAGE(error.find("expected Tag got f32") != std::string::npos, error);
  const std::string ambiguous = common + R"(
[return<int>]
main() {
  [Box] box{Box{}}
  return(box.add("a", unknown_type()))
}
)";
  CHECK_FALSE(validateProgram(ambiguous, "/main", error));
  CHECK_MESSAGE(error.find("ambiguous call to /Box/add") != std::string::npos, error);
}

TEST_CASE("an argument that is not the generic struct of a parameter infers no template argument") {
  const std::string source = R"(
import /std/collections/*

[struct]
Holder<S>() {
  [S mut] state{}
}

[struct]
Other() {
  [i32] n{0i32}
}

[return<int>]
size_of_holder<S>([Holder<S>] holder) {
  return(1i32)
}

[return<int>]
main() {
  [Other] other{Other{}}
  return(size_of_holder(other))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_MESSAGE(error.find("unable to infer implicit template arguments for /size_of_holder") != std::string::npos,
                error);
}

TEST_CASE("a generic struct parameter infers its template arguments from a specialization") {
  const std::string source = R"(
import /std/collections/*

[struct]
Holder<S>() {
  [S mut] state{}
}

[return<S>]
state_of<S>([Holder<S>] holder) {
  return(holder.state)
}

[return<int>]
main() {
  [Holder<i32>] holder{Holder<i32>{}}
  return(state_of(holder))
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

TEST_CASE("a struct method is not found for a call on a vector field that has no such method") {
  // `size` is a method of `Bag`, not of the `vector` in its field: the call is on the field's type.
  const std::string source = R"(
import /std/collections/vector/*

[struct]
Bag() {
  [vector<i32> mut] nodes{vector<i32>{}}

  [return<i32>]
  size() {
    return(7i32)
  }

  [return<i32>]
  total() {
    return(this.nodes.size())
  }
}

[return<int>]
main() {
  [Bag mut] bag{Bag{}}
  return(bag.total())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method") != std::string::npos);
}

TEST_CASE("a dereferenced Vector struct pointer passes to the typed vector helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32> mut] values{Vector<i32>{0i32, 0i32}}
  [Pointer<Vector<i32>>] p{location(values)}
  return(/std/collections/vector/vectorCount<i32>(dereference(p)))
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

TEST_CASE("a dereferenced pointer to another struct is rejected by the typed vector helper") {
  const std::string source = R"(
import /std/collections/*

[struct]
Other() {
  [i32] size{0i32}
}

[effects(heap_alloc), return<int>]
main() {
  [Other mut] other{Other{}}
  [Pointer<Other>] p{location(other)}
  return(/std/collections/vector/vectorCount<i32>(dereference(p)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_MESSAGE(error.find("argument type mismatch for") != std::string::npos, error);
}

// An overload on a template instance with an unbound argument beats a bare template parameter in the same position,
// whichever is declared first (docs/spec/language-core.md, Helper overloading). The two return different types so a
// wrong pick fails to bind.

TEST_CASE("an overload on a generic struct beats a bare template parameter declared after it") {
  const std::string source = R"(
[struct]
App<S>() {
  [S mut] state{}
}

[return<i32>]
mode<S>([App<S>] h) {
  return(1i32)
}

[return<bool>]
mode<H>([H] h) {
  return(false)
}

[return<int>]
main() {
  [App<i32>] app{App<i32>{3i32}}
  [i32] picked{mode(app)}
  [bool] other{mode(5i32)}
  return(picked)
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

TEST_CASE("an overload on a generic struct beats a bare template parameter declared before it") {
  const std::string source = R"(
[struct]
App<S>() {
  [S mut] state{}
}

[return<bool>]
mode<H>([H] h) {
  return(false)
}

[return<i32>]
mode<S>([App<S>] h) {
  return(1i32)
}

[return<int>]
main() {
  [App<i32>] app{App<i32>{3i32}}
  [i32] picked{mode(app)}
  [bool] other{mode(5i32)}
  return(picked)
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

TEST_CASE("overloads that are each more specific in a different position are ambiguous") {
  const std::string source = R"(
[struct]
App<S>() {
  [S mut] state{}
}

[return<i32>]
pick<S>([App<S>] h, [S] value) {
  return(1i32)
}

[return<i32>]
pick<H>([H] h, [i32] value) {
  return(2i32)
}

[return<int>]
main() {
  [App<i32>] app{App<i32>{3i32}}
  return(pick(app, 4i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_MESSAGE(error.find("ambiguous call to /pick") != std::string::npos, error);
}

TEST_SUITE_END();
