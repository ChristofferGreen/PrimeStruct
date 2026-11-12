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

TEST_SUITE_END();
