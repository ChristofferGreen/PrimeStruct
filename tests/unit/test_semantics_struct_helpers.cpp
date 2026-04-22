#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.struct_helpers");

TEST_CASE("struct helper method call uses implicit this") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{3i32}

  [return<i32>]
  get() {
    return(this.value)
  }
}

[return<i32>]
main() {
  [Thing] t{Thing()}
  return(t.get())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct helper accepts explicit this argument") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{5i32}

  [return<i32>]
  get() {
    return(this.value)
  }
}

[return<i32>]
main() {
  [Thing] t{Thing()}
  return(/Thing/get(t))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("static helper rejects method-call sugar") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{1i32}

  [static return<i32>]
  id() {
    return(1i32)
  }
}

[return<i32>]
main() {
  [Thing] t{Thing()}
  return(t.id())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("static helper does not accept method-call syntax") != std::string::npos);
}

TEST_CASE("implicit auto helper inference works through method-call sugar") {
  const std::string source = R"(
[struct]
Box() {
  [i32] seed{1i32}

  [return<auto>]
  echo([auto] value) {
    return(value)
  }
}

[return<i32>]
main() {
  [Box] box{Box()}
  return(box.echo(7i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("static helper allows direct calls") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{1i32}

  [static return<i32>]
  id() {
    return(1i32)
  }
}

[return<i32>]
main() {
  return(/Thing/id())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("static helper allows type-qualified dot calls") {
  const std::string source = R"(
[struct]
Counter() {
  [i32] value{5i32}

  [static return<i32>]
  default_step() {
    return(2i32)
  }

  [return<i32>]
  doubled() {
    return(this.value * 2i32)
  }
}

[return<i32>]
main() {
  [Counter] counter{Counter()}
  return(counter.doubled() + Counter.default_step())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("mut helper allows assignment to this") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{1i32}

  [mut return<void>]
  set([i32] v) {
    assign(this, Thing(v))
  }
}

[return<i32>]
main() {
  [Thing] t{Thing()}
  t.set(7i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("mut is rejected on static helpers") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{1i32}

  [static mut return<void>]
  set([i32] v) {
    return()
  }
}

[return<i32>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mut transform is not allowed on static helpers") != std::string::npos);
}

TEST_SUITE_END();
