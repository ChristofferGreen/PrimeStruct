#pragma once

TEST_CASE("import brings immediate children into root") {
  const std::string source = R"(
import /util
namespace util {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import resolves struct types and constructors") {
  const std::string source = R"(
import /util
namespace util {
  [struct]
  Widget() {
    [i32] value{1i32}
  }
}
[return<int>]
main() {
  [Widget] item{Widget()}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import resolves methods on struct types") {
  const std::string source = R"(
import /util
[struct]
/util/Widget() {
  [i32] value{1i32}
}
[return<int>]
/util/Widget/get([Widget] self, [i32] value) {
  return(value)
}
[return<int>]
main() {
  [Widget] item{Widget()}
  return(item.get(5i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import does not alias nested definitions") {
  const std::string source = R"(
import /util
namespace util {
  namespace nested {
    [return<int>]
    inner() {
      return(1i32)
    }
  }
}
[return<int>]
main() {
  return(inner())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: inner") != std::string::npos);
}

TEST_CASE("import conflicts with existing root definitions") {
  const std::string source = R"(
import /util
[return<int>]
dup() {
  return(1i32)
}
namespace util {
  [return<int>]
  dup() {
    return(2i32)
  }
}
[return<int>]
main() {
  return(dup())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import creates name conflict: dup") != std::string::npos);
}

TEST_CASE("import resolves execution targets") {
  const std::string source = R"(
import /util
namespace util {
  [return<void>]
  run([i32] count) {
    return()
  }
}
[return<int>]
main() {
  return(1i32)
}
run(1i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}
