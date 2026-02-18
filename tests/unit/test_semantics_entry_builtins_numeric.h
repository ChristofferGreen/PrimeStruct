TEST_SUITE_BEGIN("primestruct.semantics.builtins_numeric");

TEST_CASE("infers return type from builtin clamp") {
  const std::string source = R"(
import /math/*
main() {
  return(clamp(2i32, 1i32, 5i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("infers return type from builtin min") {
  const std::string source = R"(
import /math/*
main() {
  return(min(2i32, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("infers return type from builtin abs") {
  const std::string source = R"(
import /math/*
main() {
  return(abs(negate(2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("infers return type from builtin saturate") {
  const std::string source = R"(
import /math/*
main() {
  return(saturate(2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("clamp argument count fails") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(clamp(2i32, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count") != std::string::npos);
}

TEST_CASE("min argument count fails") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(min(2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count") != std::string::npos);
}

TEST_CASE("abs argument count fails") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(abs(2i32, 3i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count") != std::string::npos);
}

TEST_CASE("saturate argument count fails") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(saturate(2i32, 3i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count") != std::string::npos);
}

TEST_CASE("clamp rejects mixed int/float operands") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(clamp(1i32, 0.5f, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mixed int/float") != std::string::npos);
}

TEST_CASE("min rejects mixed int/float operands") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(min(1i32, 0.5f))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mixed int/float") != std::string::npos);
}

TEST_CASE("max rejects mixed signed/unsigned operands") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(max(2i64, 1u64))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mixed signed/unsigned") != std::string::npos);
}

TEST_CASE("sign rejects non-numeric operand") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(sign(true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("numeric") != std::string::npos);
}

TEST_CASE("saturate rejects non-numeric operand") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(saturate(true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("numeric") != std::string::npos);
}

TEST_CASE("assign through non-mut pointer fails") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [Pointer<i32>] ptr{location(value)}
  assign(dereference(ptr), 3i32)
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("assign through non-pointer dereference fails") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  assign(dereference(value), 3i32)
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable pointer binding") != std::string::npos);
}

TEST_CASE("not argument count fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(not(true, false))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  const bool hasCountMismatch = error.find("argument count mismatch") != std::string::npos;
  const bool hasExactArg = error.find("requires exactly one argument") != std::string::npos;
  if (!hasCountMismatch) {
    CHECK(hasExactArg);
  }
}

TEST_CASE("boolean operators accept bool operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(and(false, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("boolean operators reject integer operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(or(0i32, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("boolean operators require bool operands") != std::string::npos);
}

TEST_CASE("boolean operators reject unsigned operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(and(0u64, 1u64))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("boolean operators require bool operands") != std::string::npos);
}

TEST_CASE("not accepts bool operand") {
  const std::string source = R"(
[return<bool>]
main() {
  return(not(false))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("boolean operators reject float operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(and(1.0f, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("boolean operators require bool operands") != std::string::npos);
}

TEST_CASE("convert requires template argument") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("template") != std::string::npos);
}

TEST_CASE("infers void return type without transform") {
  const std::string source = R"(
main() {
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();
