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

TEST_CASE("convert rejects software numeric target") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<decimal>(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("software numeric types are not supported yet") != std::string::npos);
}

TEST_CASE("convert rejects integer target") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<integer>(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("software numeric types are not supported yet") != std::string::npos);
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

TEST_CASE("abs rejects non-numeric operand") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(abs("hi"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("abs requires numeric operand") != std::string::npos);
}

TEST_CASE("sign rejects non-numeric operand") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(sign("hi"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("sign requires numeric operand") != std::string::npos);
}

TEST_CASE("saturate rejects non-numeric operand") {
  const std::string source = R"(
import /math/*
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
import /math/*
[return<int>]
main() {
  return(pow(2i32, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("rounding math builtins validate") {
  const std::string source = R"(
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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
