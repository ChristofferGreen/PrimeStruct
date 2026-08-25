#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.software_numeric");

TEST_CASE("rejects integer binding and return") {
  const std::string source = R"(
[return<integer>]
main() {
  [integer] value{convert<integer>(1i32)}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported convert target type: integer") != std::string::npos);
}

TEST_SUITE_END();
