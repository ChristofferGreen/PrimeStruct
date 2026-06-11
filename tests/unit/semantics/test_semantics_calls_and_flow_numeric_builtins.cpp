#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.numeric_builtins");

TEST_CASE("convert builtin validates") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1.5f))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("convert missing template arg fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("convert requires exactly one template argument") != std::string::npos);
}

TEST_CASE("convert rejects missing argument") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin convert") != std::string::npos);
}

TEST_CASE("convert rejects extra arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin convert") != std::string::npos);
}

TEST_CASE("convert unsupported template arg fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<u32>(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported convert target type") != std::string::npos);
}

TEST_CASE("convert rejects decimal target") {
  const std::string source = R"(
[return<decimal>]
main() {
  return(convert<decimal>(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported convert target type: decimal") != std::string::npos);
}

TEST_CASE("convert rejects integer target") {
  const std::string source = R"(
[return<integer>]
main() {
  return(convert<integer>(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported convert target type: integer") != std::string::npos);
}

TEST_CASE("convert<bool> accepts u64 literal") {
  const std::string source = R"(
[return<bool>]
main() {
  return(convert<bool>(1u64))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("convert<bool> accepts float operand") {
  const std::string source = R"(
[return<bool>]
main() {
  return(convert<bool>(1.5f))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("convert rejects string operand") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>("hi"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("convert requires numeric or bool operand") != std::string::npos);
}

TEST_CASE("convert accepts user-defined vector-named operand call") {
  const std::string source = R"(
[return<i32>]
vector<T>([T] value) {
  return(value)
}

[return<i32>]
main() {
  return(convert<i32>(vector<i32>(7i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("convert still rejects builtin vector literal operand") {
  const std::string source = R"(
[effects(heap_alloc), return<i32>]
main() {
  return(convert<i32>(vector<i32>(7i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("convert requires numeric or bool operand") != std::string::npos);
}

TEST_CASE("convert resolves struct helper") {
  const std::string source = R"(
[struct]
Widget() {
  [i32] value{1i32}

  [static return<Widget>]
  Convert([i32] raw) {
    return(Widget{ raw })
  }
}

[return<Widget>]
main() {
  return(convert<Widget>(5i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("convert resolves imported struct helper") {
  const std::string source = R"(
import /util
namespace util {
  [public struct]
  Widget() {
    [i32] value{1i32}

    [public static return<Widget>]
    Convert([i32] raw) {
      return(Widget{ raw })
    }
  }
}

[return<Widget>]
main() {
  return(convert<Widget>(9i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("convert rejects imported private helper") {
  const std::string source = R"(
import /util
namespace util {
  [public struct]
  Widget() {
    [i32] value{1i32}

    [static return<Widget>]
    Convert([i32] raw) {
      return(Widget{ raw })
    }
  }
}

[return<Widget>]
main() {
  return(convert<Widget>(9i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("no conversion found for convert<Widget>") != std::string::npos);
}

TEST_CASE("convert rejects ambiguous helper overloads") {
  const std::string source = R"(
[struct]
Widget() {
  [i32] value{1i32}
}

[static return<Widget>]
/Widget/Convert([i32] raw) {
  return(Widget{[value] raw})
}

[static return<Widget>]
/Widget/Convert__tdeadbeef([i32] raw) {
  return(Widget{[value] raw})
}

[return<Widget>]
main() {
  return(convert<Widget>(5i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("ambiguous conversion for convert<Widget>") != std::string::npos);
  CHECK(error.find("/Widget/Convert") != std::string::npos);
  CHECK(error.find("/Widget/Convert__tdeadbeef") != std::string::npos);
}

TEST_CASE("convert rejects helper with wrong return type") {
  const std::string source = R"(
[struct]
Widget() {
  [i32] value{1i32}

  [static return<Other>]
  Convert([i32] raw) {
    return(Other{})
  }
}

[struct]
Other() {
  [i32] value{2i32}
}

[return<Widget>]
main() {
  return(convert<Widget>(5i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("no conversion found for convert<Widget>") != std::string::npos);
}

TEST_CASE("convert rejects helper with wrong param count") {
  const std::string source = R"(
[struct]
Widget() {
  [i32] value{1i32}

  [static return<Widget>]
  Convert() {
    return(Widget{})
  }
}

[return<Widget>]
main() {
  return(convert<Widget>(5i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("no conversion found for convert<Widget>") != std::string::npos);
}

TEST_CASE("convert rejects missing struct helper") {
  const std::string source = R"(
[struct]
Widget() {
  [i32] value{1i32}
}

[return<Widget>]
main() {
  return(convert<Widget>(5i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("no conversion found for convert<Widget>") != std::string::npos);
}

TEST_CASE("abs rejects non-numeric operand") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(abs("hi"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("abs requires numeric operand") != std::string::npos);
}

TEST_CASE("sign rejects string operand") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(sign("hi"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("sign requires numeric operand") != std::string::npos);
}

TEST_CASE("saturate rejects string operand") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(saturate("hi"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("saturate requires numeric operand") != std::string::npos);
}

TEST_CASE("pow accepts integer operands") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(pow(2i32, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pow accepts float operands") {
  const std::string source = R"(
import /std/math/*
[return<float>]
main() {
  return(pow(2.0f32, 3.0f32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pow rejects mixed int/float operands") {
  const std::string source = R"(
import /std/math/*
[return<float>]
main() {
  return(pow(2i32, 3.0f32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pow does not support mixed int/float operands") != std::string::npos);
}

TEST_CASE("pow rejects mixed signed/unsigned operands") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(pow(2i32, 3u64))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pow does not support mixed signed/unsigned operands") != std::string::npos);
}

TEST_CASE("sin rejects integer operand") {
  const std::string source = R"(
import /std/math/*
[return<float>]
main() {
  return(sin(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("sin requires float operands") != std::string::npos);
}

TEST_CASE("rounding math builtins validate") {
  const std::string source = R"(
import /std/math/*
[return<float>]
main() {
  return(floor(1.25f))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("float predicate math builtins validate") {
  const std::string source = R"(
import /std/math/*
[return<bool>]
main() {
  return(is_finite(1.0f))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("min rejects non-numeric operand") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(min("hi"utf8, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("min requires numeric operands") != std::string::npos);
}

TEST_CASE("max rejects non-numeric operand") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(max(1i32, "hi"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("max requires numeric operands") != std::string::npos);
}

TEST_CASE("clamp rejects non-numeric operand") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(clamp("hi"utf8, 0i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("clamp requires numeric operands") != std::string::npos);
}

TEST_CASE("clamp rejects mixed signed/unsigned operands") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(clamp(1i32, 0u64, 2u64))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("clamp does not support mixed signed/unsigned operands") != std::string::npos);
}

TEST_SUITE_END();
