TEST_SUITE_BEGIN("primestruct.semantics.resolution");

TEST_CASE("unknown identifier fails") {
  const std::string source = R"(
[return<int>]
main([i32] x) {
  return(y)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown identifier") != std::string::npos);
}

TEST_CASE("binding inference from expression enables method call validation") {
  const std::string source = R"(
namespace i64 {
  [return<i64>]
  inc([i64] self) {
    return(plus(self, 1i64))
  }
}

[return<i64>]
main() {
  [mut] value{plus(1i64, 2i64)}
  return(value.inc())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("namespace blocks may be reopened") {
  const std::string source = R"(
namespace demo {
  [return<int>]
  first() {
    return(2i32)
  }
}

namespace demo {
  [return<int>]
  second() {
    return(3i32)
  }
}

[return<int>]
main() {
  return(plus(/demo/first(), /demo/second()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();
