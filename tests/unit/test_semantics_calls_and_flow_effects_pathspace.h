#pragma once

TEST_CASE("top-level execution effects must be subset of target effects") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}

[effects(io_out) return<void>]
task() {
  return()
}

[effects(io_err)]
task()
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("execution effects must be a subset of enclosing effects") != std::string::npos);
}

TEST_CASE("execution effects scope vector literals") {
  const std::string source = R"(
[effects(heap_alloc io_out) return<void>]
main() {
  [effects(io_out)] vector<i32>(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("vector literal requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("implicit default effects allow print") {
  const std::string source = R"(
main() {
  print_line("hello"utf8)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print_error requires io_err effect") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print_error("oops"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("io_err") != std::string::npos);
}

TEST_CASE("print_line_error requires io_err effect") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print_line_error("oops"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("io_err") != std::string::npos);
}

TEST_CASE("notify requires pathspace_notify effect") {
  const std::string source = R"(
main() {
  notify("/events/test"utf8, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pathspace_notify") != std::string::npos);
}

TEST_CASE("notify rejects non-string path argument") {
  const std::string source = R"(
[effects(pathspace_notify)]
main() {
  notify(1i32, 2i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires string path argument") != std::string::npos);
}

TEST_CASE("notify rejects user-defined at on user-defined vector target") {
  const std::string source = R"(
[return<i32>]
vector<T>([T] value) {
  return(0i32)
}

[return<i32>]
at([i32] value, [i32] index) {
  return(plus(value, index))
}

[effects(pathspace_notify) return<int>]
main() {
  notify(at(vector<string>("/events/test"utf8), 0i32), 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires string path argument") != std::string::npos);
}

TEST_CASE("notify accepts string array access") {
  const std::string source = R"(
[effects(pathspace_notify)]
main() {
  [array<string>] values{array<string>("a"utf8)}
  notify(values[0i32], 1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("notify accepts string map access") {
  const std::string source = R"(
import /std/collections/*

[effects(pathspace_notify)]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "/events/test"utf8)}
  notify(values[1i32], 1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("notify rejects template arguments") {
  const std::string source = R"(
[effects(pathspace_notify)]
main() {
  notify<i32>("/events/test"utf8, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("notify does not accept template arguments") != std::string::npos);
}

TEST_CASE("notify rejects argument count mismatch") {
  const std::string source = R"(
[effects(pathspace_notify)]
main() {
  notify("/events/test"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("notify requires exactly 2 arguments") != std::string::npos);
}

TEST_CASE("notify rejects block arguments") {
  const std::string source = R"(
[effects(pathspace_notify)]
main() {
  notify("/events/test"utf8, 1i32) { 2i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("notify does not accept block arguments") != std::string::npos);
}

TEST_CASE("insert requires pathspace_insert effect") {
  const std::string source = R"(
main() {
  insert("/events/test"utf8, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pathspace_insert") != std::string::npos);
}

TEST_CASE("insert rejects template arguments") {
  const std::string source = R"(
[effects(pathspace_insert)]
main() {
  insert<i32>("/events/test"utf8, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("insert does not accept template arguments") != std::string::npos);
}

TEST_CASE("insert rejects non-string path argument") {
  const std::string source = R"(
[effects(pathspace_insert)]
main() {
  insert(1i32, 2i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires string path argument") != std::string::npos);
}

TEST_CASE("insert accepts string array access") {
  const std::string source = R"(
[effects(pathspace_insert)]
main() {
  [array<string>] values{array<string>("/events/test"utf8)}
  insert(values[0i32], 1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("insert rejects block arguments") {
  const std::string source = R"(
[effects(pathspace_insert)]
main() {
  insert("/events/test"utf8, 1i32) { 2i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("insert does not accept block arguments") != std::string::npos);
}

TEST_CASE("insert rejects argument count mismatch") {
  const std::string source = R"(
[effects(pathspace_insert)]
main() {
  insert("/events/test"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("insert requires exactly 2 arguments") != std::string::npos);
}

TEST_CASE("insert not allowed in expression context") {
  const std::string source = R"(
[return<int> effects(pathspace_insert)]
main() {
  return(insert("/events/test"utf8, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("statement-only") != std::string::npos);
}

TEST_CASE("take requires pathspace_take effect") {
  const std::string source = R"(
main() {
  take("/events/test"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pathspace_take") != std::string::npos);
}

TEST_CASE("take rejects template arguments") {
  const std::string source = R"(
[effects(pathspace_take)]
main() {
  take<i32>("/events/test"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("take does not accept template arguments") != std::string::npos);
}

TEST_CASE("take rejects non-string path argument") {
  const std::string source = R"(
[effects(pathspace_take)]
main() {
  take(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires string path argument") != std::string::npos);
}

TEST_CASE("take accepts string map access") {
  const std::string source = R"(
import /std/collections/*

[effects(pathspace_take)]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "/events/test"utf8)}
  take(values[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("take rejects block arguments") {
  const std::string source = R"(
[effects(pathspace_take)]
main() {
  take("/events/test"utf8) { 1i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("take does not accept block arguments") != std::string::npos);
}

TEST_CASE("take rejects argument count mismatch") {
  const std::string source = R"(
[effects(pathspace_take)]
main() {
  take()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("take requires exactly 1 argument") != std::string::npos);
}

TEST_CASE("take not allowed in expression context") {
  const std::string source = R"(
[return<int> effects(pathspace_take)]
main() {
  return(take("/events/test"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("statement-only") != std::string::npos);
}

TEST_CASE("bind requires pathspace_bind effect") {
  const std::string source = R"(
main() {
  bind("/events/test"utf8, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pathspace_bind") != std::string::npos);
}

TEST_CASE("unbind requires pathspace_bind effect") {
  const std::string source = R"(
main() {
  unbind("/events/test"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pathspace_bind") != std::string::npos);
}

TEST_CASE("schedule requires pathspace_schedule effect") {
  const std::string source = R"(
main() {
  schedule("/events/test"utf8, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pathspace_schedule") != std::string::npos);
}

TEST_CASE("schedule rejects non-string path argument") {
  const std::string source = R"(
[effects(pathspace_schedule)]
main() {
  schedule(1i32, 2i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires string path argument") != std::string::npos);
}

TEST_CASE("notify not allowed in expression context") {
  const std::string source = R"(
[return<int> effects(pathspace_notify)]
main() {
  return(notify("/events/test"utf8, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("statement-only") != std::string::npos);
}

TEST_CASE("print not allowed in expression context") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  return(print_line("hello"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("only supported as a statement") != std::string::npos);
}

TEST_CASE("print rejects pointer argument") {
  const std::string source = R"(
[effects(io_out) return<int>]
main() {
  [i32] value{1i32}
  print(location(value))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires an integer/bool or string literal/binding argument") != std::string::npos);
}

TEST_CASE("print rejects collection argument") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print(array<i32>(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires an integer/bool or string literal/binding argument") != std::string::npos);
}

TEST_CASE("print rejects missing arguments") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print requires exactly one argument") != std::string::npos);
}

TEST_CASE("print_line rejects block arguments") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print_line(1i32) { 2i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_line does not accept block arguments") != std::string::npos);
}

