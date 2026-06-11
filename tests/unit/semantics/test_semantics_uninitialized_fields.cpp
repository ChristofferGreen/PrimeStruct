#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.uninitialized_fields");

TEST_CASE("uninitialized field helpers accept this field access") {
  const std::string source = R"(
[struct]
Box() {
  [uninitialized<i32>] storage{uninitialized<i32>()}

  Create() {
    init(this.storage, 1i32)
  }

  Destroy() {
    drop(this.storage)
  }
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("uninitialized field init rejects non-storage field") {
  const std::string source = R"(
[struct]
Box() {
  [i32] storage{0i32}

  Create() {
    init(this.storage, 1i32)
  }
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("init requires uninitialized<T> storage") != std::string::npos);
}

TEST_CASE("uninitialized field take infers return kind") {
  const std::string source = R"(
[struct]
Box() {
  [uninitialized<i32>] storage{uninitialized<i32>()}

  [return<i32>]
  take_value() {
    return(take(this.storage))
  }
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("uninitialized field borrow infers return kind") {
  const std::string source = R"(
[struct]
Box() {
  [uninitialized<i32>] storage{uninitialized<i32>()}

  [return<i32>]
  peek() {
    return(borrow(this.storage))
  }
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("uninitialized local borrow validates reference binding") {
  const std::string source = R"(
[return<int>]
main() {
  [uninitialized<i32>] storage{uninitialized<i32>()}
  init(storage, 7i32)
  [Reference<i32>] ref{borrow(storage)}
  [i32] out{dereference(ref)}
  drop(storage)
  return(out)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("uninitialized field borrow validates reference binding") {
  const std::string source = R"(
[struct]
Box() {
  [uninitialized<i32>] storage{uninitialized<i32>()}

  [return<i32>]
  peek() {
    [Reference<i32>] ref{borrow(this.storage)}
    return(dereference(ref))
  }
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("uninitialized dereferenced borrow validates reference binding") {
  const std::string source = R"(
[return<int>]
main() {
  [uninitialized<i32>] storage{uninitialized<i32>()}
  [Reference<uninitialized<i32>>] storage_ref{location(storage)}
  init(dereference(storage_ref), 7i32)
  [Reference<i32>] ref{borrow(dereference(storage_ref))}
  [i32] out{dereference(ref)}
  drop(dereference(storage_ref))
  return(out)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("uninitialized dereferenced borrow rejects reference binding type mismatch") {
  const std::string source = R"(
[return<int>]
main() {
  [uninitialized<i64>] storage{uninitialized<i64>()}
  [Reference<uninitialized<i64>>] storage_ref{location(storage)}
  init(dereference(storage_ref), 7i64)
  [Reference<i32>] ref{borrow(dereference(storage_ref))}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("Reference binding type mismatch") != std::string::npos);
}

TEST_SUITE_END();
