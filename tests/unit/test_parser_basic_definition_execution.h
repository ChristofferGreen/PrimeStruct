#pragma once

TEST_CASE("parses transform-prefixed execution") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [effects(io_out)] print_line("hi"utf8)
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 2);
  const auto &stmt = program.definitions[0].statements[0];
  REQUIRE(stmt.kind == primec::Expr::Kind::Call);
  CHECK(stmt.name == "print_line");
  REQUIRE(stmt.transforms.size() == 1);
  CHECK(stmt.transforms[0].name == "effects");
}

TEST_CASE("parses binding-like transforms on calls") {
  const std::string source = R"(
[return<int>]
main() {
  [operators, collections] log(1i32)
  return(sum([operators, collections] add(1i32) 2i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &statements = program.definitions[0].statements;
  REQUIRE(statements.size() == 2);
  const auto &logCall = statements[0];
  REQUIRE(logCall.kind == primec::Expr::Kind::Call);
  REQUIRE(logCall.transforms.size() == 2);
  CHECK(logCall.transforms[0].name == "operators");
  const auto &returnCall = statements[1];
  REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
  REQUIRE(returnCall.args.size() == 1);
  const auto &sumCall = returnCall.args[0];
  REQUIRE(sumCall.kind == primec::Expr::Kind::Call);
  REQUIRE(sumCall.args.size() == 2);
  const auto &nestedCall = sumCall.args[0];
  REQUIRE(nestedCall.kind == primec::Expr::Kind::Call);
  REQUIRE(nestedCall.transforms.size() == 2);
  CHECK(nestedCall.transforms[0].name == "operators");
}

TEST_CASE("parses execution transforms in bodies and arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [effects(io_out)] log(1i32)
  return(add([effects(io_out)] log(2i32) 3i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &statements = program.definitions[0].statements;
  REQUIRE(statements.size() == 2);
  const auto &logCall = statements[0];
  REQUIRE(logCall.kind == primec::Expr::Kind::Call);
  REQUIRE(logCall.transforms.size() == 1);
  CHECK(logCall.transforms[0].name == "effects");
  REQUIRE(logCall.transforms[0].arguments.size() == 1);
  CHECK(logCall.transforms[0].arguments[0] == "io_out");
  const auto &returnCall = statements[1];
  REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
  REQUIRE(returnCall.args.size() == 1);
  const auto &addCall = returnCall.args[0];
  REQUIRE(addCall.kind == primec::Expr::Kind::Call);
  REQUIRE(addCall.args.size() == 2);
  const auto &nestedLog = addCall.args[0];
  REQUIRE(nestedLog.kind == primec::Expr::Kind::Call);
  REQUIRE(nestedLog.transforms.size() == 1);
  CHECK(nestedLog.transforms[0].name == "effects");
}

TEST_CASE("parses semicolon-separated transforms and lists") {
  const std::string source = R"(
[effects(io_out; io_err); return<int>]
sum([i32] a; [i32] b) {
  return(plus(a b))
}

[return<int>]
main() {
  return(sum(1i32; 2i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 2);
  const auto &sum = program.definitions[0];
  REQUIRE(sum.transforms.size() == 2);
  CHECK(sum.transforms[0].name == "effects");
  REQUIRE(sum.transforms[0].arguments.size() == 2);
  CHECK(sum.transforms[0].arguments[0] == "io_out");
  CHECK(sum.transforms[0].arguments[1] == "io_err");
  CHECK(sum.parameters.size() == 2);
  const auto &returnCall = program.definitions[1].statements[0];
  REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
  CHECK(returnCall.name == "return");
  REQUIRE(returnCall.args.size() == 1);
  const auto &call = returnCall.args[0];
  REQUIRE(call.kind == primec::Expr::Kind::Call);
  REQUIRE(call.args.size() == 2);
}
