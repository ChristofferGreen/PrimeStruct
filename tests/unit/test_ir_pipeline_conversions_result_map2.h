TEST_CASE("ir lowerer supports Result.map2 builtin lambdas") {
  const std::string source = R"(
swallow_file_error([FileError] err) {}

[return<int> on_error<FileError, /swallow_file_error>]
main() {
  [Result<i32, FileError>] first{Result.ok(2i32)}
  [Result<i32, FileError>] second{Result.ok(3i32)}
  [Result<i32, FileError>] summed{
    Result.map2(first, second, []([i32] left, [i32] right) { return(plus(left, right)) })
  }
  return(try(summed))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 5);
}

TEST_CASE("ir lowerer rejects Result.map2 wide payloads") {
  const std::string source = R"(
[return<int>]
main() {
  [Result<i32, FileError>] first{Result.ok(2i32)}
  [Result<i32, FileError>] second{Result.ok(3i32)}
  [Result<i64, FileError>] summed{
    Result.map2(first, second, []([i32] left, [i32] right) { return(3i64) })
  }
  if(Result.error(summed), then(){ return(1i32) }, else(){ return(0i32) })
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.find("IR backends only support Result.map2 with supported payload values") != std::string::npos);
}

TEST_CASE("ir lowerer supports Result.map2 f32 payloads") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<f32, FileError>] first{Result.ok(1.25f32)}
  [Result<f32, FileError>] second{Result.ok(0.75f32)}
  [Result<f32, FileError>] summed{
    Result.map2(first, second, []([f32] left, [f32] right) { return(plus(left, right)) })
  }
  return(convert<int>(multiply(try(summed), 10.0f32)))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 20);
}

TEST_CASE("ir lowerer supports Result.map2 with direct Result.ok sources") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] summed{
    Result.map2(Result.ok(2i32), Result.ok(3i32), []([i32] left, [i32] right) { return(plus(left, right)) })
  }
  return(try(summed))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 5);
}

TEST_CASE("ir lowerer supports direct ok Result combinator consumers") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
swallow_file_error([FileError] err) {}

[return<int> effects(io_err) on_error<FileError, /swallow_file_error>]
main() {
  [i32] mapped{try(Result.map(Result.ok(2i32), []([i32] value) { return(multiply(value, 4i32)) }))}
  [i32] chained{try(Result.and_then(Result.ok(2i32), []([i32] value) { return(Result.ok(plus(value, 3i32))) }))}
  [i32] summed{
    try(Result.map2(Result.ok(2i32), Result.ok(3i32), []([i32] left, [i32] right) { return(plus(left, right)) }))
  }
  return(plus(plus(mapped, chained), summed))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 18);
}

TEST_CASE("ir lowerer supports direct string Result combinator consumers") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return("parse failed"utf8)
  }
}

swallow_parse_error([ParseError] err) {}

[return<int> on_error<ParseError, /swallow_parse_error>]
main() {
  return(
    plus(
      plus(
        count(try(Result.map(Result.ok("alpha"utf8), []([string] value) { return(value) }))),
        count(try(Result.and_then(Result.ok("beta"utf8), []([string] value) { return(Result.ok(value)) })))
      ),
      count(try(Result.map2(Result.ok("gamma"utf8), Result.ok("delta"utf8), []([string] left, [string] right) {
        return(left)
      })))
    )
  )
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 14);
}

TEST_CASE("ir lowerer supports definition-backed string Result combinator sources") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return("parse failed"utf8)
  }
}

[struct]
Reader() {
  [i32] marker{0i32}
}

[return<Result<string, ParseError>>]
greeting() {
  return(Result.ok("alpha"utf8))
}

[return<Result<string, ParseError>>]
/Reader/read([Reader] self) {
  return(Result.ok("beta"utf8))
}

swallow_parse_error([ParseError] err) {}

[return<int> on_error<ParseError, /swallow_parse_error>]
main() {
  [Reader] reader{Reader()}
  return(
    plus(
      plus(
        count(try(Result.map(greeting(), []([string] value) { return(value) }))),
        count(try(Result.and_then(reader.read(), []([string] value) { return(Result.ok(value)) })))
      ),
      count(try(Result.map2(greeting(), reader.read(), []([string] left, [string] right) { return(left) })))
    )
  )
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 14);
}

TEST_CASE("ir lowerer preserves inline-call Result metadata from caller-scoped parameter defaults") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return("parse failed"utf8)
  }
}

[struct]
Reader() {
  [i32] marker{0i32}
}

[return<Result<string, ParseError>>]
greeting() {
  return(Result.ok("alpha"utf8))
}

[return<Result<string, ParseError>>]
/Reader/read([Reader] self) {
  return(Result.ok("beta"utf8))
}

[return<int> on_error<ParseError, /swallow_parse_error>]
consume([Result<string, ParseError>] status) {
  return(count(try(status)))
}

swallow_parse_error([ParseError] err) {}

[return<int>]
main() {
  [Reader] reader{Reader()}
  return(consume(greeting()))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  auto makeName = [](const std::string &name) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    return expr;
  };
  auto makeCall = [](const std::string &name, std::vector<primec::Expr> args, bool isMethodCall = false) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = name;
    expr.args = std::move(args);
    expr.isMethodCall = isMethodCall;
    return expr;
  };

  auto consumeIt =
      std::find_if(program.definitions.begin(), program.definitions.end(), [](const primec::Definition &def) {
        return def.fullPath == "/consume";
      });
  REQUIRE(consumeIt != program.definitions.end());
  REQUIRE(consumeIt->parameters.size() == 1);

  primec::Expr leftParam = makeName("left");
  primec::Expr rightParam = makeName("right");
  primec::Expr returnLeft = makeCall("return", {makeName("left")});

  primec::Expr map2Lambda;
  map2Lambda.kind = primec::Expr::Kind::Call;
  map2Lambda.isLambda = true;
  map2Lambda.hasBodyArguments = true;
  map2Lambda.args = {leftParam, rightParam};
  map2Lambda.bodyArguments = {returnLeft};

  consumeIt->parameters.front().args = {
      makeCall("map2",
               {
                   makeName("Result"),
                   makeCall("greeting", {}),
                   makeCall("read", {makeName("reader")}, true),
                   map2Lambda,
               },
               true),
  };

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 5);
}

TEST_CASE("ir lowerer rejects wide Result.ok payloads") {
  const std::string source = R"(
import /std/file/*

[return<Result<i64, FileError>>]
main() {
  return(Result.ok(3i64))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.find("IR backends only support Result.ok with supported payload values") != std::string::npos);
}
