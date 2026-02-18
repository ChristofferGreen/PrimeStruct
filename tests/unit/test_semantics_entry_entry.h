TEST_SUITE_BEGIN("primestruct.semantics.entry");

TEST_CASE("missing entry definition fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/missing", error));
  CHECK(error.find("missing entry definition") != std::string::npos);
}

TEST_CASE("entry definition rejects non-arg parameter") {
  const std::string source = R"(
[return<int>]
main([i32] value) {
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("entry definition must take a single array<string> parameter") != std::string::npos);
}

TEST_CASE("entry definition rejects multiple parameters") {
  const std::string source = R"(
[return<int>]
main([array<string>] args, [i32] value) {
  return(args.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("entry definition must take a single array<string> parameter") != std::string::npos);
}

TEST_CASE("entry definition without args parameter is allowed") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("entry definition accepts array<string> parameter") {
  const std::string source = R"(
[return<int>]
main([array<string>] argv) {
  return(argv.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("entry definition rejects default args parameter") {
  const std::string source = R"(
[return<int>]
main([array<string>] args{array<string>("hi"utf8)}) {
  return(args.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("entry parameter does not allow a default value") != std::string::npos);
}

TEST_CASE("entry definition rejects templated definition") {
  const std::string source = R"(
[return<int>]
main<T>() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("entry definition cannot be templated: /main") != std::string::npos);
}

TEST_CASE("entry default effects enable io_out") {
  const std::string source = R"(
[return<int>]
main() {
  print_line("hello"utf8)
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgramWithDefaults(source, "/main", {}, {"io_out"}, error));
  CHECK(error.empty());
}

TEST_CASE("non-entry defaults remain pure") {
  const std::string source = R"(
[return<int>]
main() {
  log()
  return(1i32)
}

[return<void>]
log() {
  print_line("hello"utf8)
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgramWithDefaults(source, "/main", {}, {"io_out"}, error));
  CHECK(error.find("print_line requires io_out effect") != std::string::npos);
}


TEST_SUITE_END();
