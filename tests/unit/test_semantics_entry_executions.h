TEST_SUITE_BEGIN("primestruct.semantics.executions");

TEST_CASE("argument count mismatch fails") {
  const std::string source = R"(
[return<int>]
callee([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(callee(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
}

TEST_CASE("execution target must exist") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

run()
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown execution target") != std::string::npos);
}

TEST_CASE("execution argument count mismatch fails") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

task()
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
}

TEST_CASE("execution accepts bracket-labeled arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
execute_repeat([i32] count) {
  return(count)
}

execute_repeat([count] 2i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution rejects unknown named argument") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
execute_repeat([i32] count) {
  return(count)
}

execute_repeat([missing] 2i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown named argument: missing") != std::string::npos);
}

TEST_CASE("execution rejects duplicate named argument") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
execute_repeat([i32] count) {
  return(count)
}

execute_repeat([count] 2i32, [count] 3i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate named argument: count") != std::string::npos);
}

TEST_CASE("execution body arguments are rejected") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

execute_repeat(3i32) {
  [i32] value{2i32}
  main()
}
)";
  std::string error;
  CHECK_FALSE(parseProgramWithError(source, error));
  CHECK(error.find("executions do not accept body blocks") != std::string::npos);
}

TEST_CASE("execution rejects copy transform") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[copy]
execute_repeat(2i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("copy transform is not allowed on executions") != std::string::npos);
}

TEST_CASE("execution rejects mut transform") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[mut]
execute_repeat(2i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mut transform is not allowed on executions") != std::string::npos);
}

TEST_CASE("execution rejects restrict transform") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[restrict]
execute_repeat(2i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("restrict transform is not allowed on executions") != std::string::npos);
}

TEST_CASE("execution rejects placement transforms") {
  const char *placements[] = {"stack", "heap", "buffer"};
  for (const auto *placement : placements) {
    CAPTURE(placement);
    const std::string source =
        std::string("[return<int>]\n") +
        "main() {\n"
        "  return(1i32)\n"
        "}\n"
        "\n"
        "[return<void>]\n"
        "execute_repeat([i32] x) {\n"
        "  return()\n"
        "}\n"
        "\n"
        "[" + placement + "]\n"
        "execute_repeat(2i32)\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("placement transforms are not supported") != std::string::npos);
  }
}

TEST_CASE("execution rejects duplicate effects transforms") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[effects(io_out) effects(asset_read)]
execute_repeat(2i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate effects transform on /execute_repeat") != std::string::npos);
}

TEST_CASE("execution rejects duplicate capabilities transforms") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[capabilities(io_out) capabilities(asset_read)]
execute_repeat(2i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate capabilities transform on /execute_repeat") != std::string::npos);
}

TEST_CASE("execution rejects invalid effects capability") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[effects("io"utf8)]
execute_repeat(2i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid effects capability") != std::string::npos);
}

TEST_CASE("execution rejects invalid capability name") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[capabilities("io"utf8)]
execute_repeat(2i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid capability") != std::string::npos);
}

TEST_CASE("execution rejects duplicate effects capability") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[effects(io_out, io_out)]
execute_repeat(2i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate effects capability") != std::string::npos);
}

TEST_CASE("execution rejects duplicate capability") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[capabilities(io_out, io_out)]
execute_repeat(2i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate capability") != std::string::npos);
}

TEST_CASE("execution rejects effects template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[effects<io_out>]
execute_repeat(2i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("effects transform does not accept template arguments on /execute_repeat") != std::string::npos);
}

TEST_CASE("execution rejects capabilities template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[capabilities<io_out>]
execute_repeat(2i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capabilities transform does not accept template arguments on /execute_repeat") != std::string::npos);
}

TEST_CASE("execution capabilities require matching effects") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[effects(io_out) capabilities(io_err)]
execute_repeat(2i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capability requires matching effect on /execute_repeat") != std::string::npos);
}

TEST_CASE("execution accepts effects and capabilities") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[effects(io_out) capabilities(io_out)]
execute_repeat(2i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution rejects struct transform") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[struct]
execute_repeat(2i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct transforms are not allowed on executions") != std::string::npos);
}

TEST_CASE("execution rejects visibility transform") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[public]
execute_repeat(2i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding visibility/static transforms are only valid on bindings") != std::string::npos);
}

TEST_CASE("execution rejects alignment transforms") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[align_bytes(16)]
execute_repeat(3i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("alignment transforms are not supported on executions") != std::string::npos);
}

TEST_SUITE_END();
