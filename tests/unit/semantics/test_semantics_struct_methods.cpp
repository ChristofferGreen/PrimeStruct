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

TEST_SUITE_END();
