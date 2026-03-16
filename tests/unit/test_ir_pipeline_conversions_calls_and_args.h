  CHECK(result == 3);
}

TEST_CASE("ir lowerer supports entry args count helper") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(count(args))
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
  REQUIRE(module.functions.size() == 1);
  REQUIRE(module.functions[0].instructions.size() == 2);
  CHECK(module.functions[0].instructions[0].op == primec::IrOpcode::PushArgc);
  CHECK(module.functions[0].instructions[1].op == primec::IrOpcode::ReturnI32);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error, 4));
  CHECK(error.empty());
  CHECK(result == 4);
}

TEST_CASE("ir lowerer supports array count helper") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(count(values))
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
  CHECK(result == 3);
}

TEST_CASE("ir lowerer supports vector count helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(count(values))
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
  CHECK(result == 3);
}

TEST_CASE("ir lowerer supports array literal count helper") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(array<i32>(1i32, 2i32, 3i32)))
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
  CHECK(result == 3);
}

TEST_CASE("ir lowerer supports vector literal count helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  return(count(vector<i32>(1i32, 2i32)))
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
  CHECK(result == 2);
}

TEST_CASE("ir lowerer materializes variadic args packs for direct calls") {
  const std::string source = R"(
[return<int>]
score([i32] head, [args<i32>] values) {
  return(plus(head, plus(values.count(), values[1i32])))
}

[return<int>]
main() {
  return(score(10i32, 20i32, 30i32, 40i32))
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
  CHECK(result == 43);
}

TEST_CASE("ir lowerer forwards pure spread variadic args packs") {
  const std::string source = R"(
[return<int>]
count_values([args<i32>] values) {
  return(plus(count(values), values[0i32]))
}

[return<int>]
forward([args<i32>] values) {
  return(count_values([spread] values))
}

[return<int>]
main() {
  return(forward(7i32, 8i32))
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
  CHECK(result == 9);
}

TEST_CASE("ir lowerer materializes string variadic args packs and forwards pure spread") {
  const std::string source = R"(
[return<int>]
pack_score([args<string>] values) {
  return(plus(count(values), values[1i32].count()))
}

[return<int>]
forward([args<string>] values) {
  return(pack_score([spread] values))
}

[return<int>]
main() {
  return(forward("a"raw_utf8, "bbb"raw_utf8))
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

TEST_CASE("ir lowerer materializes mixed explicit and spread variadic forwarding") {
  const std::string source = R"(
[return<int>]
count_values([args<i32>] values) {
  return(plus(values[0i32], values[2i32]))
}

[return<int>]
forward([args<i32>] values) {
  return(count_values(99i32, [spread] values))
}

[return<int>]
main() {
  return(forward(1i32, 2i32))
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
  CHECK(result == 101);
}

TEST_CASE("ir lowerer materializes mixed string spread variadic forwarding") {
  const std::string source = R"(
[return<int>]
pack_score([args<string>] values) {
  return(plus(count(values), values[0i32].count()))
}

[return<int>]
forward([args<string>] values) {
  return(pack_score("zz"raw_utf8, [spread] values))
}

[return<int>]
main() {
  return(forward("a"raw_utf8, "bbb"raw_utf8))
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

TEST_CASE("ir lowerer materializes direct struct variadic args packs for count") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{0i32}
}

[return<int>]
count_values([args<Pair>] values) {
  return(count(values))
}

[return<int>]
main() {
  return(count_values(Pair(), Pair()))
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
  CHECK(result == 2);
}

TEST_CASE("ir lowerer forwards pure spread struct variadic packs for count") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{0i32}
}

[return<int>]
count_values([args<Pair>] values) {
  return(count(values))
}

[return<int>]
forward([args<Pair>] values) {
  return(count_values([spread] values))
}

[return<int>]
main() {
  return(forward(Pair(), Pair()))
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
  CHECK(result == 2);
}

TEST_CASE("ir lowerer materializes mixed struct spread variadic forwarding for count") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{0i32}
}

[return<int>]
count_values([args<Pair>] values) {
  return(count(values))
}

[return<int>]
forward([args<Pair>] values) {
  return(count_values(Pair(), [spread] values))
}

[return<int>]
main() {
  return(forward(Pair(), Pair()))
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
  CHECK(result == 3);
}

TEST_CASE("ir lowerer materializes direct struct variadic pack indexing and method access") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{7i32}
}

[return<int>]
/Pair/score([Pair] self) {
  return(plus(self.value, 1i32))
}

[return<int>]
score_pairs([args<Pair>] values) {
  return(plus(values[0i32].value, values[1i32].score()))
}

[return<int>]
main() {
  return(score_pairs(Pair(7i32), Pair(9i32)))
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
  CHECK(result == 17);
}

TEST_CASE("ir lowerer forwards pure spread struct pack indexing and method access") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{7i32}
}

[return<int>]
/Pair/score([Pair] self) {
  return(plus(self.value, 1i32))
}

[return<int>]
score_pairs([args<Pair>] values) {
  return(plus(values[0i32].value, values[1i32].score()))
}

[return<int>]
forward([args<Pair>] values) {
  return(score_pairs([spread] values))
}

[return<int>]
main() {
  return(forward(Pair(7i32), Pair(9i32)))
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
  CHECK(result == 17);
}

TEST_CASE("ir lowerer materializes mixed struct pack indexing and method access") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{7i32}
}

[return<int>]
/Pair/score([Pair] self) {
  return(plus(self.value, 1i32))
}

[return<int>]
score_pairs([args<Pair>] values) {
  return(plus(values[0i32].value, values[2i32].score()))
}

[return<int>]
forward([args<Pair>] values) {
  return(score_pairs(Pair(5i32), [spread] values))
}

[return<int>]
main() {
  return(forward(Pair(7i32), Pair(9i32)))
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
  CHECK(result == 15);
}

TEST_CASE("ir lowerer materializes variadic Result packs with indexed why and try access") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return(if(equal(err.code, 7i32), then() { "bad"utf8 }, else() { "other"utf8 }))
  }
}

swallow_parse_error([ParseError] err) {}

[return<Result<i32, ParseError>>]
ok_value([i32] value) {
  return(Result.ok(value))
}

[return<Result<i32, ParseError>>]
fail_bad() {
  return(7i64)
}

[return<int> on_error<ParseError, /swallow_parse_error>]
score_results([args<Result<i32, ParseError>>] values) {
  [i32] head{try(values[0i32])}
  [i32] tailWhyCount{count(Result.why(values[minus(count(values), 1i32)]))}
  return(plus(head, tailWhyCount))
}

[return<int>]
forward([args<Result<i32, ParseError>>] values) {
  return(score_results([spread] values))
}

[return<int>]
forward_mixed([args<Result<i32, ParseError>>] values) {
  return(score_results(ok_value(10i32), [spread] values))
}

[return<int>]
main() {
  return(plus(score_results(ok_value(2i32), fail_bad()),
              plus(forward(ok_value(3i32), fail_bad()),
                   forward_mixed(ok_value(4i32), fail_bad()))))
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
  CHECK(result == 30);
}

TEST_CASE("ir lowerer materializes variadic borrowed Result packs with indexed dereference why and try access") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return(if(equal(err.code, 7i32), then() { "bad"utf8 }, else() { "other"utf8 }))
  }
}

swallow_parse_error([ParseError] err) {}

[return<Result<i32, ParseError>>]
ok_value([i32] value) {
  return(Result.ok(value))
}

[return<Result<i32, ParseError>>]
fail_bad() {
  return(7i64)
}

[return<int> on_error<ParseError, /swallow_parse_error>]
score_results([args<Reference<Result<i32, ParseError>>>] values) {
  [i32] head{try(dereference(values[0i32]))}
  [i32] tailWhyCount{count(Result.why(dereference(values[minus(count(values), 1i32)])))}
  return(plus(head, tailWhyCount))
}

[return<int>]
forward([args<Reference<Result<i32, ParseError>>>] values) {
  return(score_results([spread] values))
}

[return<int>]
forward_mixed([args<Reference<Result<i32, ParseError>>>] values) {
  [Result<i32, ParseError>] extra{ok_value(10i32)}
  [Reference<Result<i32, ParseError>>] extra_ref{location(extra)}
  return(score_results(extra_ref, [spread] values))
}

[return<int>]
main() {
  [Result<i32, ParseError>] a0{ok_value(2i32)}
  [Result<i32, ParseError>] a1{fail_bad()}
  [Reference<Result<i32, ParseError>>] r0{location(a0)}
  [Reference<Result<i32, ParseError>>] r1{location(a1)}

  [Result<i32, ParseError>] b0{ok_value(3i32)}
  [Result<i32, ParseError>] b1{fail_bad()}
  [Reference<Result<i32, ParseError>>] s0{location(b0)}
  [Reference<Result<i32, ParseError>>] s1{location(b1)}

  [Result<i32, ParseError>] c0{ok_value(4i32)}
  [Result<i32, ParseError>] c1{fail_bad()}
  [Reference<Result<i32, ParseError>>] t0{location(c0)}
  [Reference<Result<i32, ParseError>>] t1{location(c1)}

  return(plus(score_results(r0, r1),
              plus(forward(s0, s1),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 30);
}

TEST_CASE("ir lowerer materializes variadic pointer Result packs with indexed dereference why and inferred try access") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return(if(equal(err.code, 7i32), then() { "bad"utf8 }, else() { "other"utf8 }))
  }
}

swallow_parse_error([ParseError] err) {}

[return<Result<i32, ParseError>>]
ok_value([i32] value) {
  return(Result.ok(value))
}

[return<Result<i32, ParseError>>]
fail_bad() {
  return(7i64)
}

[return<int> on_error<ParseError, /swallow_parse_error>]
score_results([args<Pointer<Result<i32, ParseError>>>] values) {
  [auto] head{try(dereference(values[0i32]))}
  [i32] tailWhyCount{count(Result.why(dereference(values[minus(count(values), 1i32)])))}
  return(plus(head, tailWhyCount))
}

[return<int>]
forward([args<Pointer<Result<i32, ParseError>>>] values) {
  return(score_results([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<Result<i32, ParseError>>>] values) {
  [Result<i32, ParseError>] extra{ok_value(10i32)}
  [Pointer<Result<i32, ParseError>>] extra_ptr{location(extra)}
  return(score_results(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [Result<i32, ParseError>] a0{ok_value(2i32)}
  [Result<i32, ParseError>] a1{fail_bad()}
  [Pointer<Result<i32, ParseError>>] r0{location(a0)}
  [Pointer<Result<i32, ParseError>>] r1{location(a1)}

  [Result<i32, ParseError>] b0{ok_value(3i32)}
  [Result<i32, ParseError>] b1{fail_bad()}
  [Pointer<Result<i32, ParseError>>] s0{location(b0)}
  [Pointer<Result<i32, ParseError>>] s1{location(b1)}

  [Result<i32, ParseError>] c0{ok_value(4i32)}
  [Result<i32, ParseError>] c1{fail_bad()}
  [Pointer<Result<i32, ParseError>>] t0{location(c0)}
  [Pointer<Result<i32, ParseError>>] t1{location(c1)}

  return(plus(score_results(r0, r1),
              plus(forward(s0, s1),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 30);
}

TEST_CASE("ir lowerer materializes variadic status-only Result packs with indexed error and why access") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return(if(equal(err.code, 7i32), then() { "bad"utf8 }, else() { "other"utf8 }))
  }
}

[return<Result<ParseError>>]
ok_status() {
  return(Result.ok())
}

[return<Result<ParseError>>]
fail_bad() {
  return(7i32)
}

[return<int>]
score_results([args<Result<ParseError>>] values) {
  [bool] tailHasError{Result.error(values[minus(count(values), 1i32)])}
  [i32] tailWhyCount{count(Result.why(values[minus(count(values), 1i32)]))}
  return(if(tailHasError, then() { plus(10i32, tailWhyCount) }, else() { 0i32 }))
}

[return<int>]
forward([args<Result<ParseError>>] values) {
  return(score_results([spread] values))
}

[return<int>]
forward_mixed([args<Result<ParseError>>] values) {
  return(score_results(ok_status(), [spread] values))
}

[return<int>]
main() {
  return(plus(score_results(ok_status(), fail_bad()),
              plus(forward(ok_status(), fail_bad()),
                   forward_mixed(ok_status(), fail_bad()))))
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
  CHECK(result == 39);
}

TEST_CASE("ir lowerer materializes variadic borrowed status-only Result packs with indexed dereference error and why access") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return(if(equal(err.code, 7i32), then() { "bad"utf8 }, else() { "other"utf8 }))
  }
}

[return<Result<ParseError>>]
ok_status() {
  return(Result.ok())
}

[return<Result<ParseError>>]
fail_bad() {
  return(7i32)
}

[return<int>]
score_results([args<Reference<Result<ParseError>>>] values) {
  [bool] tailHasError{Result.error(dereference(values[minus(count(values), 1i32)]))}
  [i32] tailWhyCount{count(Result.why(dereference(values[minus(count(values), 1i32)])))}
  return(if(tailHasError, then() { plus(10i32, tailWhyCount) }, else() { 0i32 }))
}

[return<int>]
forward([args<Reference<Result<ParseError>>>] values) {
  return(score_results([spread] values))
}

[return<int>]
forward_mixed([args<Reference<Result<ParseError>>>] values) {
  [Result<ParseError>] extra{ok_status()}
  [Reference<Result<ParseError>>] extra_ref{location(extra)}
  return(score_results(extra_ref, [spread] values))
}

[return<int>]
main() {
  [Result<ParseError>] a0{ok_status()}
  [Result<ParseError>] a1{fail_bad()}
  [Reference<Result<ParseError>>] r0{location(a0)}
  [Reference<Result<ParseError>>] r1{location(a1)}

  [Result<ParseError>] b0{ok_status()}
  [Result<ParseError>] b1{fail_bad()}
  [Reference<Result<ParseError>>] s0{location(b0)}
  [Reference<Result<ParseError>>] s1{location(b1)}

  [Result<ParseError>] c0{ok_status()}
  [Result<ParseError>] c1{fail_bad()}
  [Reference<Result<ParseError>>] t0{location(c0)}
  [Reference<Result<ParseError>>] t1{location(c1)}

  return(plus(score_results(r0, r1),
              plus(forward(s0, s1),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 39);
}

TEST_CASE("ir lowerer materializes variadic pointer status-only Result packs with indexed dereference error and why access") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return(if(equal(err.code, 7i32), then() { "bad"utf8 }, else() { "other"utf8 }))
  }
}

[return<Result<ParseError>>]
ok_status() {
  return(Result.ok())
}

[return<Result<ParseError>>]
fail_bad() {
  return(7i32)
}

[return<int>]
score_results([args<Pointer<Result<ParseError>>>] values) {
  [bool] tailHasError{Result.error(dereference(values[minus(count(values), 1i32)]))}
  [i32] tailWhyCount{count(Result.why(dereference(values[minus(count(values), 1i32)])))}
  return(if(tailHasError, then() { plus(10i32, tailWhyCount) }, else() { 0i32 }))
}

[return<int>]
forward([args<Pointer<Result<ParseError>>>] values) {
  return(score_results([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<Result<ParseError>>>] values) {
  [Result<ParseError>] extra{ok_status()}
  [Pointer<Result<ParseError>>] extra_ptr{location(extra)}
  return(score_results(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [Result<ParseError>] a0{ok_status()}
  [Result<ParseError>] a1{fail_bad()}
  [Pointer<Result<ParseError>>] r0{location(a0)}
  [Pointer<Result<ParseError>>] r1{location(a1)}

  [Result<ParseError>] b0{ok_status()}
  [Result<ParseError>] b1{fail_bad()}
  [Pointer<Result<ParseError>>] s0{location(b0)}
  [Pointer<Result<ParseError>>] s1{location(b1)}

  [Result<ParseError>] c0{ok_status()}
  [Result<ParseError>] c1{fail_bad()}
  [Pointer<Result<ParseError>>] t0{location(c0)}
  [Pointer<Result<ParseError>>] t1{location(c1)}

  return(plus(score_results(r0, r1),
              plus(forward(s0, s1),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 39);
}

#if defined(EACCES) && defined(ENOENT) && defined(EEXIST)
TEST_CASE("ir lowerer materializes variadic FileError packs with indexed why methods") {
  const std::string source =
      "[return<int>]\n"
      "score_errors([args<FileError>] values) {\n"
      "  return(plus(count(values[0i32].why()), count(values[2i32].why())))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward([args<FileError>] values) {\n"
      "  return(score_errors([spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_mixed([args<FileError>] values) {\n"
      "  [FileError] extra{" + std::to_string(EACCES) + "i32}\n"
      "  return(score_errors(extra, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "main() {\n"
      "  [FileError] a0{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] a1{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] a2{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] b0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] b1{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] b2{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] c0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] c1{" + std::to_string(EEXIST) + "i32}\n"
      "  return(plus(score_errors(a0, a1, a2),\n"
      "              plus(forward(b0, b1, b2),\n"
      "                   forward_mixed(c0, c1))))\n"
      "}\n";
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
  CHECK(result == 36);
}

TEST_CASE("ir lowerer materializes variadic borrowed FileError packs with indexed dereference why methods") {
  const std::string source =
      "[return<int>]\n"
      "score_refs([args<Reference<FileError>>] values) {\n"
      "  return(plus(count(dereference(values[0i32]).why()), count(dereference(values[2i32]).why())))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward([args<Reference<FileError>>] values) {\n"
      "  return(score_refs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_mixed([args<Reference<FileError>>] values) {\n"
      "  [FileError] extra{" + std::to_string(EACCES) + "i32}\n"
      "  [Reference<FileError>] extra_ref{location(extra)}\n"
      "  return(score_refs(extra_ref, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "main() {\n"
      "  [FileError] a0{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] a1{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] a2{" + std::to_string(EEXIST) + "i32}\n"
      "  [Reference<FileError>] r0{location(a0)}\n"
      "  [Reference<FileError>] r1{location(a1)}\n"
      "  [Reference<FileError>] r2{location(a2)}\n"
      "  [FileError] b0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] b1{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] b2{" + std::to_string(EACCES) + "i32}\n"
      "  [Reference<FileError>] s0{location(b0)}\n"
      "  [Reference<FileError>] s1{location(b1)}\n"
      "  [Reference<FileError>] s2{location(b2)}\n"
      "  [FileError] c0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] c1{" + std::to_string(EEXIST) + "i32}\n"
      "  [Reference<FileError>] t0{location(c0)}\n"
      "  [Reference<FileError>] t1{location(c1)}\n"
      "  return(plus(score_refs(r0, r1, r2),\n"
      "              plus(forward(s0, s1, s2),\n"
      "                   forward_mixed(t0, t1))))\n"
      "}\n";
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
  CHECK(result == 36);
}

TEST_CASE("ir lowerer materializes prefix spread borrowed FileError packs with indexed dereference why methods") {
  const std::string source =
      "[return<int>]\n"
      "score_refs([args<Reference<FileError>>] values) {\n"
      "  return(plus(count(dereference(values[0i32]).why()), count(dereference(values[2i32]).why())))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward([args<Reference<FileError>>] values) {\n"
      "  return(score_refs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_mixed([args<Reference<FileError>>] values) {\n"
      "  [FileError] extra{" + std::to_string(EACCES) + "i32}\n"
      "  [Reference<FileError>] extra_ref{location(extra)}\n"
      "  return(score_refs([spread] values, extra_ref))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "main() {\n"
      "  [FileError] a0{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] a1{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] a2{" + std::to_string(EEXIST) + "i32}\n"
      "  [Reference<FileError>] r0{location(a0)}\n"
      "  [Reference<FileError>] r1{location(a1)}\n"
      "  [Reference<FileError>] r2{location(a2)}\n"
      "  [FileError] b0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] b1{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] b2{" + std::to_string(EACCES) + "i32}\n"
      "  [Reference<FileError>] s0{location(b0)}\n"
      "  [Reference<FileError>] s1{location(b1)}\n"
      "  [Reference<FileError>] s2{location(b2)}\n"
      "  [FileError] c0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] c1{" + std::to_string(EEXIST) + "i32}\n"
      "  [Reference<FileError>] t0{location(c0)}\n"
      "  [Reference<FileError>] t1{location(c1)}\n"
      "  return(plus(score_refs(r0, r1, r2),\n"
      "              plus(forward(s0, s1, s2),\n"
      "                   forward_mixed(t0, t1))))\n"
      "}\n";
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
  CHECK(result == 36);
}

TEST_CASE("ir lowerer materializes variadic pointer FileError packs with indexed dereference why methods") {
  const std::string source =
      "[return<int>]\n"
      "score_ptrs([args<Pointer<FileError>>] values) {\n"
      "  return(plus(count(dereference(values[0i32]).why()), count(dereference(values[2i32]).why())))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward([args<Pointer<FileError>>] values) {\n"
      "  return(score_ptrs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_mixed([args<Pointer<FileError>>] values) {\n"
      "  [FileError] extra{" + std::to_string(EACCES) + "i32}\n"
      "  [Pointer<FileError>] extra_ptr{location(extra)}\n"
      "  return(score_ptrs(extra_ptr, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "main() {\n"
      "  [FileError] a0{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] a1{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] a2{" + std::to_string(EEXIST) + "i32}\n"
      "  [Pointer<FileError>] r0{location(a0)}\n"
      "  [Pointer<FileError>] r1{location(a1)}\n"
      "  [Pointer<FileError>] r2{location(a2)}\n"
      "  [FileError] b0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] b1{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] b2{" + std::to_string(EACCES) + "i32}\n"
      "  [Pointer<FileError>] s0{location(b0)}\n"
      "  [Pointer<FileError>] s1{location(b1)}\n"
      "  [Pointer<FileError>] s2{location(b2)}\n"
      "  [FileError] c0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] c1{" + std::to_string(EEXIST) + "i32}\n"
      "  [Pointer<FileError>] t0{location(c0)}\n"
      "  [Pointer<FileError>] t1{location(c1)}\n"
      "  return(plus(score_ptrs(r0, r1, r2),\n"
      "              plus(forward(s0, s1, s2),\n"
      "                   forward_mixed(t0, t1))))\n"
      "}\n";
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
  CHECK(result == 36);
}

TEST_CASE("ir lowerer materializes prefix spread pointer FileError packs with indexed dereference why methods") {
  const std::string source =
      "[return<int>]\n"
      "score_ptrs([args<Pointer<FileError>>] values) {\n"
      "  return(plus(count(dereference(values[0i32]).why()), count(dereference(values[2i32]).why())))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward([args<Pointer<FileError>>] values) {\n"
      "  return(score_ptrs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_mixed([args<Pointer<FileError>>] values) {\n"
      "  [FileError] extra{" + std::to_string(EACCES) + "i32}\n"
      "  [Pointer<FileError>] extra_ptr{location(extra)}\n"
      "  return(score_ptrs([spread] values, extra_ptr))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "main() {\n"
      "  [FileError] a0{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] a1{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] a2{" + std::to_string(EEXIST) + "i32}\n"
      "  [Pointer<FileError>] r0{location(a0)}\n"
      "  [Pointer<FileError>] r1{location(a1)}\n"
      "  [Pointer<FileError>] r2{location(a2)}\n"
      "  [FileError] b0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] b1{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] b2{" + std::to_string(EACCES) + "i32}\n"
      "  [Pointer<FileError>] s0{location(b0)}\n"
      "  [Pointer<FileError>] s1{location(b1)}\n"
      "  [Pointer<FileError>] s2{location(b2)}\n"
      "  [FileError] c0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] c1{" + std::to_string(EEXIST) + "i32}\n"
      "  [Pointer<FileError>] t0{location(c0)}\n"
      "  [Pointer<FileError>] t1{location(c1)}\n"
      "  return(plus(score_ptrs(r0, r1, r2),\n"
      "              plus(forward(s0, s1, s2),\n"
      "                   forward_mixed(t0, t1))))\n"
      "}\n";
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
  CHECK(result == 36);
}

TEST_CASE("ir lowerer materializes variadic File handle packs with indexed file methods") {
  auto escape = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
      if (c == '\\' || c == '"') {
        out.push_back('\\');
      }
      out.push_back(c);
    }
    return out;
  };
  const std::string pathA0 = "/tmp/primec_vm_variadic_file_handle_a0.txt";
  const std::string pathA1 = "/tmp/primec_vm_variadic_file_handle_a1.txt";
  const std::string pathA2 = "/tmp/primec_vm_variadic_file_handle_a2.txt";
  const std::string pathB0 = "/tmp/primec_vm_variadic_file_handle_b0.txt";
  const std::string pathB1 = "/tmp/primec_vm_variadic_file_handle_b1.txt";
  const std::string pathB2 = "/tmp/primec_vm_variadic_file_handle_b2.txt";
  const std::string pathC0 = "/tmp/primec_vm_variadic_file_handle_c0.txt";
  const std::string pathC1 = "/tmp/primec_vm_variadic_file_handle_c1.txt";
  const std::string pathExtra = "/tmp/primec_vm_variadic_file_handle_extra.txt";
  const auto readFile = [](const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
  };

  const std::string source =
      "[effects(file_write)]\n"
      "swallow_file_error([FileError] err) {}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "score_files([args<File<Write>>] values) {\n"
      "  values[0i32].write_line(\"alpha\"utf8)?\n"
      "  values[minus(count(values), 1i32)].write_line(\"omega\"utf8)?\n"
      "  values[0i32].flush()?\n"
      "  values[minus(count(values), 1i32)].flush()?\n"
      "  values[0i32].close()?\n"
      "  values[minus(count(values), 1i32)].close()?\n"
      "  return(plus(count(values), 10i32))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward([args<File<Write>>] values) {\n"
      "  return(score_files([spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward_mixed([args<File<Write>>] values) {\n"
      "  [File<Write>] extra{File<Write>(\"" + escape(pathExtra) + "\"utf8)?}\n"
      "  return(score_files(extra, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "main() {\n"
      "  [File<Write>] a0{File<Write>(\"" + escape(pathA0) + "\"utf8)?}\n"
      "  [File<Write>] a1{File<Write>(\"" + escape(pathA1) + "\"utf8)?}\n"
      "  [File<Write>] a2{File<Write>(\"" + escape(pathA2) + "\"utf8)?}\n"
      "  [File<Write>] b0{File<Write>(\"" + escape(pathB0) + "\"utf8)?}\n"
      "  [File<Write>] b1{File<Write>(\"" + escape(pathB1) + "\"utf8)?}\n"
      "  [File<Write>] b2{File<Write>(\"" + escape(pathB2) + "\"utf8)?}\n"
      "  [File<Write>] c0{File<Write>(\"" + escape(pathC0) + "\"utf8)?}\n"
      "  [File<Write>] c1{File<Write>(\"" + escape(pathC1) + "\"utf8)?}\n"
      "  return(plus(score_files(a0, a1, a2), plus(forward(b0, b1, b2), forward_mixed(c0, c1))))\n"
      "}\n";
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
  CHECK(result == 39);
  CHECK(readFile(pathA0) == "alpha\n");
  CHECK(readFile(pathA2) == "omega\n");
  CHECK(readFile(pathB0) == "alpha\n");
  CHECK(readFile(pathB2) == "omega\n");
  CHECK(readFile(pathC0).empty());
  CHECK(readFile(pathC1) == "omega\n");
  CHECK(readFile(pathExtra) == "alpha\n");
}

TEST_CASE("ir lowerer materializes variadic borrowed File handle packs with indexed dereference file methods") {
  auto escape = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
      if (c == '\\' || c == '"') {
        out.push_back('\\');
      }
      out.push_back(c);
    }
    return out;
  };
  const std::string pathA0 = "/tmp/primec_vm_variadic_borrowed_file_handle_a0.txt";
  const std::string pathA1 = "/tmp/primec_vm_variadic_borrowed_file_handle_a1.txt";
  const std::string pathA2 = "/tmp/primec_vm_variadic_borrowed_file_handle_a2.txt";
  const std::string pathB0 = "/tmp/primec_vm_variadic_borrowed_file_handle_b0.txt";
  const std::string pathB1 = "/tmp/primec_vm_variadic_borrowed_file_handle_b1.txt";
  const std::string pathB2 = "/tmp/primec_vm_variadic_borrowed_file_handle_b2.txt";
  const std::string pathC0 = "/tmp/primec_vm_variadic_borrowed_file_handle_c0.txt";
  const std::string pathC1 = "/tmp/primec_vm_variadic_borrowed_file_handle_c1.txt";
  const std::string pathExtra = "/tmp/primec_vm_variadic_borrowed_file_handle_extra.txt";
  const auto readFile = [](const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
  };

  const std::string source =
      "[effects(file_write)]\n"
      "swallow_file_error([FileError] err) {}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "score_refs([args<Reference<File<Write>>>] values) {\n"
      "  dereference(values[0i32]).write_line(\"alpha\"utf8)?\n"
      "  dereference(values[minus(count(values), 1i32)]).write_line(\"omega\"utf8)?\n"
      "  dereference(values[0i32]).flush()?\n"
      "  dereference(values[minus(count(values), 1i32)]).flush()?\n"
      "  return(plus(count(values), 10i32))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward([args<Reference<File<Write>>>] values) {\n"
      "  return(score_refs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward_mixed([args<Reference<File<Write>>>] values) {\n"
      "  [File<Write>] extra{File<Write>(\"" + escape(pathExtra) + "\"utf8)?}\n"
      "  [Reference<File<Write>>] extra_ref{location(extra)}\n"
      "  return(score_refs(extra_ref, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "main() {\n"
      "  [File<Write>] a0{File<Write>(\"" + escape(pathA0) + "\"utf8)?}\n"
      "  [File<Write>] a1{File<Write>(\"" + escape(pathA1) + "\"utf8)?}\n"
      "  [File<Write>] a2{File<Write>(\"" + escape(pathA2) + "\"utf8)?}\n"
      "  [Reference<File<Write>>] r0{location(a0)}\n"
      "  [Reference<File<Write>>] r1{location(a1)}\n"
      "  [Reference<File<Write>>] r2{location(a2)}\n"
      "  [File<Write>] b0{File<Write>(\"" + escape(pathB0) + "\"utf8)?}\n"
      "  [File<Write>] b1{File<Write>(\"" + escape(pathB1) + "\"utf8)?}\n"
      "  [File<Write>] b2{File<Write>(\"" + escape(pathB2) + "\"utf8)?}\n"
      "  [Reference<File<Write>>] s0{location(b0)}\n"
      "  [Reference<File<Write>>] s1{location(b1)}\n"
      "  [Reference<File<Write>>] s2{location(b2)}\n"
      "  [File<Write>] c0{File<Write>(\"" + escape(pathC0) + "\"utf8)?}\n"
      "  [File<Write>] c1{File<Write>(\"" + escape(pathC1) + "\"utf8)?}\n"
      "  [Reference<File<Write>>] t0{location(c0)}\n"
      "  [Reference<File<Write>>] t1{location(c1)}\n"
      "  return(plus(score_refs(r0, r1, r2), plus(forward(s0, s1, s2), forward_mixed(t0, t1))))\n"
      "}\n";
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
  CHECK(result == 39);
  CHECK(readFile(pathA0) == "alpha\n");
  CHECK(readFile(pathA2) == "omega\n");
  CHECK(readFile(pathB0) == "alpha\n");
  CHECK(readFile(pathB2) == "omega\n");
  CHECK(readFile(pathC0).empty());
  CHECK(readFile(pathC1) == "omega\n");
  CHECK(readFile(pathExtra) == "alpha\n");
}

TEST_CASE("ir lowerer materializes variadic pointer File handle packs with indexed dereference file methods") {
  auto escape = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
      if (c == '\\' || c == '"') {
        out.push_back('\\');
      }
      out.push_back(c);
    }
    return out;
  };
  const std::string pathA0 = "/tmp/primec_vm_variadic_pointer_file_handle_a0.txt";
  const std::string pathA1 = "/tmp/primec_vm_variadic_pointer_file_handle_a1.txt";
  const std::string pathA2 = "/tmp/primec_vm_variadic_pointer_file_handle_a2.txt";
  const std::string pathB0 = "/tmp/primec_vm_variadic_pointer_file_handle_b0.txt";
  const std::string pathB1 = "/tmp/primec_vm_variadic_pointer_file_handle_b1.txt";
  const std::string pathB2 = "/tmp/primec_vm_variadic_pointer_file_handle_b2.txt";
  const std::string pathC0 = "/tmp/primec_vm_variadic_pointer_file_handle_c0.txt";
  const std::string pathC1 = "/tmp/primec_vm_variadic_pointer_file_handle_c1.txt";
  const std::string pathExtra = "/tmp/primec_vm_variadic_pointer_file_handle_extra.txt";
  const auto readFile = [](const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
  };

  const std::string source =
      "[effects(file_write)]\n"
      "swallow_file_error([FileError] err) {}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "score_ptrs([args<Pointer<File<Write>>>] values) {\n"
      "  dereference(values[0i32]).write_line(\"alpha\"utf8)?\n"
      "  dereference(values[minus(count(values), 1i32)]).write_line(\"omega\"utf8)?\n"
      "  dereference(values[0i32]).flush()?\n"
      "  dereference(values[minus(count(values), 1i32)]).flush()?\n"
      "  return(plus(count(values), 10i32))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward([args<Pointer<File<Write>>>] values) {\n"
      "  return(score_ptrs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward_mixed([args<Pointer<File<Write>>>] values) {\n"
      "  [File<Write>] extra{File<Write>(\"" + escape(pathExtra) + "\"utf8)?}\n"
      "  [Pointer<File<Write>>] extra_ptr{location(extra)}\n"
      "  return(score_ptrs(extra_ptr, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "main() {\n"
      "  [File<Write>] a0{File<Write>(\"" + escape(pathA0) + "\"utf8)?}\n"
      "  [File<Write>] a1{File<Write>(\"" + escape(pathA1) + "\"utf8)?}\n"
      "  [File<Write>] a2{File<Write>(\"" + escape(pathA2) + "\"utf8)?}\n"
      "  [Pointer<File<Write>>] r0{location(a0)}\n"
      "  [Pointer<File<Write>>] r1{location(a1)}\n"
      "  [Pointer<File<Write>>] r2{location(a2)}\n"
      "  [File<Write>] b0{File<Write>(\"" + escape(pathB0) + "\"utf8)?}\n"
      "  [File<Write>] b1{File<Write>(\"" + escape(pathB1) + "\"utf8)?}\n"
      "  [File<Write>] b2{File<Write>(\"" + escape(pathB2) + "\"utf8)?}\n"
      "  [Pointer<File<Write>>] s0{location(b0)}\n"
      "  [Pointer<File<Write>>] s1{location(b1)}\n"
      "  [Pointer<File<Write>>] s2{location(b2)}\n"
      "  [File<Write>] c0{File<Write>(\"" + escape(pathC0) + "\"utf8)?}\n"
      "  [File<Write>] c1{File<Write>(\"" + escape(pathC1) + "\"utf8)?}\n"
      "  [Pointer<File<Write>>] t0{location(c0)}\n"
      "  [Pointer<File<Write>>] t1{location(c1)}\n"
      "  return(plus(score_ptrs(r0, r1, r2), plus(forward(s0, s1, s2), forward_mixed(t0, t1))))\n"
      "}\n";
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
  CHECK(result == 39);
  CHECK(readFile(pathA0) == "alpha\n");
  CHECK(readFile(pathA2) == "omega\n");
  CHECK(readFile(pathB0) == "alpha\n");
  CHECK(readFile(pathB2) == "omega\n");
  CHECK(readFile(pathC0).empty());
  CHECK(readFile(pathC1) == "omega\n");
  CHECK(readFile(pathExtra) == "alpha\n");
}
#endif

TEST_CASE("ir lowerer materializes variadic vector packs with indexed count methods") {
  const std::string source = R"(
[return<int>]
score_vectors([args<vector<i32>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<vector<i32>>] values) {
  return(score_vectors([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<vector<i32>>] values) {
  return(score_vectors(vector<i32>(1i32), [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(score_vectors(vector<i32>(1i32, 2i32), vector<i32>(3i32), vector<i32>(4i32, 5i32, 6i32)),
              plus(forward(vector<i32>(7i32), vector<i32>(8i32, 9i32), vector<i32>(10i32)),
                   forward_mixed(vector<i32>(11i32, 12i32), vector<i32>(13i32, 14i32, 15i32)))))
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
  CHECK(result == 11);
}

TEST_CASE("ir lowerer materializes variadic vector packs with indexed capacity methods") {
  const std::string source = R"(
[return<int>]
score_vectors([args<vector<i32>>] values) {
  [auto] head{capacity(at(values, 0i32))}
  return(plus(head, at(values, 2i32).capacity()))
}

[return<int>]
forward([args<vector<i32>>] values) {
  return(score_vectors([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<vector<i32>>] values) {
  return(score_vectors(vector<i32>(1i32), [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(score_vectors(vector<i32>(1i32, 2i32), vector<i32>(3i32), vector<i32>(4i32, 5i32, 6i32)),
              plus(forward(vector<i32>(7i32), vector<i32>(8i32, 9i32), vector<i32>(10i32)),
                   forward_mixed(vector<i32>(11i32, 12i32), vector<i32>(13i32, 14i32, 15i32)))))
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
  CHECK(result == 11);
}

TEST_CASE("ir lowerer materializes variadic vector packs with indexed statement mutators") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
mutate_vectors([args<vector<i32>>] values) {
  push(at(values, 0i32), 9i32)
  values[0i32].pop()
  reserve(values[1i32], 6i32)
  values[1i32].clear()
  remove_at(values[2i32], 1i32)
  values[2i32].remove_swap(0i32)
  return(plus(values[0i32].count(),
              plus(values[1i32].capacity(),
                   values[2i32].count())))
}

[effects(heap_alloc), return<int>]
forward([args<vector<i32>>] values) {
  return(mutate_vectors([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<vector<i32>>] values) {
  [vector<i32>] extra{vector<i32>(20i32)}
  return(mutate_vectors(extra, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vector<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}

  [vector<i32>] b0{vector<i32>(7i32)}
  [vector<i32>] b1{vector<i32>(8i32, 9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32, 12i32)}

  [vector<i32>] c0{vector<i32>(13i32, 14i32)}
  [vector<i32>] c1{vector<i32>(15i32, 16i32, 17i32)}

  return(plus(mutate_vectors(a0, a1, a2),
              plus(forward(b0, b1, b2),
                   forward_mixed(c0, c1))))
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
  CHECK(result == 25);
}

TEST_CASE("ir lowerer materializes variadic array packs with indexed count methods") {
  const std::string source = R"(
[return<int>]
score_arrays([args<array<i32>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<array<i32>>] values) {
  return(score_arrays([spread] values))
}

[return<int>]
forward_mixed([args<array<i32>>] values) {
  return(score_arrays(array<i32>(1i32), [spread] values))
}

[return<int>]
main() {
  return(plus(score_arrays(array<i32>(1i32, 2i32), array<i32>(3i32), array<i32>(4i32, 5i32, 6i32)),
              plus(forward(array<i32>(7i32), array<i32>(8i32, 9i32), array<i32>(10i32)),
                   forward_mixed(array<i32>(11i32, 12i32), array<i32>(13i32, 14i32, 15i32)))))
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
  CHECK(result == 11);
}

TEST_CASE("ir lowerer materializes variadic borrowed array packs with indexed count methods") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<array<i32>>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<Reference<array<i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<array<i32>>>] values) {
  [array<i32>] extra{array<i32>(1i32)}
  [Reference<array<i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [array<i32>] a0{array<i32>(1i32, 2i32)}
  [array<i32>] a1{array<i32>(3i32)}
  [array<i32>] a2{array<i32>(4i32, 5i32, 6i32)}
  [Reference<array<i32>>] r0{location(a0)}
  [Reference<array<i32>>] r1{location(a1)}
  [Reference<array<i32>>] r2{location(a2)}

  [array<i32>] b0{array<i32>(7i32)}
  [array<i32>] b1{array<i32>(8i32, 9i32)}
  [array<i32>] b2{array<i32>(10i32)}
  [Reference<array<i32>>] s0{location(b0)}
  [Reference<array<i32>>] s1{location(b1)}
  [Reference<array<i32>>] s2{location(b2)}

  [array<i32>] c0{array<i32>(11i32, 12i32)}
  [array<i32>] c1{array<i32>(13i32, 14i32, 15i32)}
  [Reference<array<i32>>] t0{location(c0)}
  [Reference<array<i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 11);
}

TEST_CASE("ir lowerer materializes variadic borrowed array packs with indexed dereference access helpers") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<array<i32>>>] values) {
  [auto] head{at_unsafe(dereference(at(values, 0i32)), 1i32)}
  return(plus(head, dereference(at(values, 2i32)).at(0i32)))
}

[return<int>]
forward([args<Reference<array<i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<array<i32>>>] values) {
  [array<i32>] extra{array<i32>(1i32, 2i32)}
  [Reference<array<i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [array<i32>] a0{array<i32>(1i32, 2i32)}
  [array<i32>] a1{array<i32>(3i32)}
  [array<i32>] a2{array<i32>(4i32, 5i32, 6i32)}
  [Reference<array<i32>>] r0{location(a0)}
  [Reference<array<i32>>] r1{location(a1)}
  [Reference<array<i32>>] r2{location(a2)}

  [array<i32>] b0{array<i32>(7i32, 8i32)}
  [array<i32>] b1{array<i32>(9i32)}
  [array<i32>] b2{array<i32>(10i32, 11i32)}
  [Reference<array<i32>>] s0{location(b0)}
  [Reference<array<i32>>] s1{location(b1)}
  [Reference<array<i32>>] s2{location(b2)}

  [array<i32>] c0{array<i32>(12i32)}
  [array<i32>] c1{array<i32>(13i32, 14i32, 15i32)}
  [Reference<array<i32>>] t0{location(c0)}
  [Reference<array<i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 39);
}

TEST_CASE("ir lowerer materializes variadic scalar reference packs with indexed dereference") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<i32>>] values) {
  return(plus(dereference(values[0i32]), dereference(values[2i32])))
}

[return<int>]
forward([args<Reference<i32>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<i32>>] values) {
  [i32] extra{1i32}
  [Reference<i32>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [i32] a0{1i32}
  [i32] a1{2i32}
  [i32] a2{3i32}
  [Reference<i32>] r0{location(a0)}
  [Reference<i32>] r1{location(a1)}
  [Reference<i32>] r2{location(a2)}

  [i32] b0{4i32}
  [i32] b1{5i32}
  [i32] b2{6i32}
  [Reference<i32>] s0{location(b0)}
  [Reference<i32>] s1{location(b1)}
  [Reference<i32>] s2{location(b2)}

  [i32] c0{7i32}
  [i32] c1{8i32}
  [Reference<i32>] t0{location(c0)}
  [Reference<i32>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 23);
}

TEST_CASE("ir lowerer materializes variadic struct reference packs with indexed field and helper access") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{7i32}
}

[return<int>]
/Pair/score([Pair] self) {
  return(plus(self.value, 1i32))
}

[return<int>]
score_refs([args<Reference<Pair>>] values) {
  return(plus(values[0i32].value, values[2i32].score()))
}

[return<int>]
forward([args<Reference<Pair>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<Pair>>] values) {
  [Pair] extra{Pair(5i32)}
  [Reference<Pair>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [Pair] a0{Pair(7i32)}
  [Pair] a1{Pair(8i32)}
  [Pair] a2{Pair(9i32)}
  [Reference<Pair>] r0{location(a0)}
  [Reference<Pair>] r1{location(a1)}
  [Reference<Pair>] r2{location(a2)}

  [Pair] b0{Pair(11i32)}
  [Pair] b1{Pair(12i32)}
  [Pair] b2{Pair(13i32)}
  [Reference<Pair>] s0{location(b0)}
  [Reference<Pair>] s1{location(b1)}
  [Reference<Pair>] s2{location(b2)}

  [Pair] c0{Pair(15i32)}
  [Pair] c1{Pair(17i32)}
  [Reference<Pair>] t0{location(c0)}
  [Reference<Pair>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 65);
}

TEST_CASE("ir lowerer materializes variadic struct pointer packs with indexed field and helper access") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{7i32}
}

[return<int>]
/Pair/score([Pair] self) {
  return(plus(self.value, 1i32))
}

[return<int>]
score_ptrs([args<Pointer<Pair>>] values) {
  return(plus(values[0i32].value, values[2i32].score()))
}

[return<int>]
forward([args<Pointer<Pair>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<Pair>>] values) {
  [Pair] extra{Pair(5i32)}
  [Pointer<Pair>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [Pair] a0{Pair(7i32)}
  [Pair] a1{Pair(8i32)}
  [Pair] a2{Pair(9i32)}
  [Pointer<Pair>] p0{location(a0)}
  [Pointer<Pair>] p1{location(a1)}
  [Pointer<Pair>] p2{location(a2)}

  [Pair] b0{Pair(11i32)}
  [Pair] b1{Pair(12i32)}
  [Pair] b2{Pair(13i32)}
  [Pointer<Pair>] q0{location(b0)}
  [Pointer<Pair>] q1{location(b1)}
  [Pointer<Pair>] q2{location(b2)}

  [Pair] c0{Pair(15i32)}
  [Pair] c1{Pair(17i32)}
  [Pointer<Pair>] r0{location(c0)}
  [Pointer<Pair>] r1{location(c1)}

  return(plus(score_ptrs(p0, p1, p2),
              plus(forward(q0, q1, q2),
                   forward_mixed(r0, r1))))
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
  CHECK(result == 65);
}

TEST_CASE("ir lowerer materializes variadic scalar pointer packs with indexed dereference") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer<i32>>] values) {
  return(plus(dereference(values[0i32]), dereference(values[2i32])))
}

[return<int>]
forward([args<Pointer<i32>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<i32>>] values) {
  [i32] extra{1i32}
  [Pointer<i32>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [i32] a0{1i32}
  [i32] a1{2i32}
  [i32] a2{3i32}
  [Pointer<i32>] p0{location(a0)}
  [Pointer<i32>] p1{location(a1)}
  [Pointer<i32>] p2{location(a2)}

  [i32] b0{4i32}
  [i32] b1{5i32}
  [i32] b2{6i32}
  [Pointer<i32>] q0{location(b0)}
  [Pointer<i32>] q1{location(b1)}
  [Pointer<i32>] q2{location(b2)}

  [i32] c0{7i32}
  [i32] c1{8i32}
  [Pointer<i32>] r0{location(c0)}
  [Pointer<i32>] r1{location(c1)}

  return(plus(score_ptrs(p0, p1, p2),
              plus(forward(q0, q1, q2),
                   forward_mixed(r0, r1))))
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
  CHECK(result == 23);
}

TEST_CASE("ir lowerer materializes variadic borrowed map packs with indexed count methods") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32)}
  [Reference</std/collections/map<i32, i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] r0{location(a0)}
  [Reference</std/collections/map<i32, i32>>] r1{location(a1)}
  [Reference</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32, 17i32, 18i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(19i32, 20i32)}
  [Reference</std/collections/map<i32, i32>>] s0{location(b0)}
  [Reference</std/collections/map<i32, i32>>] s1{location(b1)}
  [Reference</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32, 23i32, 24i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)}
  [Reference</std/collections/map<i32, i32>>] t0{location(c0)}
  [Reference</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 11);
}

TEST_CASE("ir lowerer materializes variadic borrowed map packs with indexed dereference count methods") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(plus(dereference(at(values, 0i32)).count(),
              dereference(at(values, 2i32)).count()))
}

[return<int>]
forward([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32)}
  [Reference</std/collections/map<i32, i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] r0{location(a0)}
  [Reference</std/collections/map<i32, i32>>] r1{location(a1)}
  [Reference</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32, 17i32, 18i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(19i32, 20i32)}
  [Reference</std/collections/map<i32, i32>>] s0{location(b0)}
  [Reference</std/collections/map<i32, i32>>] s1{location(b1)}
  [Reference</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32, 23i32, 24i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)}
  [Reference</std/collections/map<i32, i32>>] t0{location(c0)}
  [Reference</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 11);
}

TEST_CASE("ir lowerer materializes variadic borrowed map packs with indexed dereference lookup helpers") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference</std/collections/map<i32, i32>>>] values) {
  [auto] head{dereference(at(values, 0i32)).at_unsafe(3i32)}
  if(dereference(at(values, 2i32)).contains(11i32),
     then(){ return(plus(head, dereference(at(values, 2i32)).at(11i32))) },
     else(){ return(0i32) })
}

[return<int>]
forward([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(19i32, 20i32, 3i32, 4i32)}
  [Reference</std/collections/map<i32, i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] r0{location(a0)}
  [Reference</std/collections/map<i32, i32>>] r1{location(a1)}
  [Reference</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(17i32, 18i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] s0{location(b0)}
  [Reference</std/collections/map<i32, i32>>] s1{location(b1)}
  [Reference</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(23i32, 24i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] t0{location(c0)}
  [Reference</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 48);
}

TEST_CASE("ir lowerer materializes variadic borrowed map packs with indexed tryAt inference" * doctest::skip()) {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference</std/collections/map<i32, i32>>>] values) {
  [auto] head{try(at(values, 0i32).tryAt(3i32))}
  [auto] tailMissing{Result.error(/std/collections/map/tryAt(at(values, minus(count(values), 1i32)), 99i32))}
  return(if(tailMissing,
            then(){ return(plus(head, /std/collections/map/at_unsafe(at(values, minus(count(values), 1i32)), 11i32))) },
            else(){ return(0i32) }))
}

[return<int>]
forward([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32, 3i32, 8i32)}
  [Reference</std/collections/map<i32, i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] r0{location(a0)}
  [Reference</std/collections/map<i32, i32>>] r1{location(a1)}
  [Reference</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(1i32, 2i32, 3i32, 6i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 14i32)}
  [Reference</std/collections/map<i32, i32>>] s0{location(b0)}
  [Reference</std/collections/map<i32, i32>>] s1{location(b1)}
  [Reference</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 16i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(11i32, 16i32, 13i32, 14i32)}
  [Reference</std/collections/map<i32, i32>>] t0{location(c0)}
  [Reference</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 60);
}

TEST_CASE("ir lowerer materializes variadic pointer array packs with indexed count methods") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer<array<i32>>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<Pointer<array<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<array<i32>>>] values) {
  [array<i32>] extra{array<i32>(1i32)}
  [Pointer<array<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [array<i32>] a0{array<i32>(1i32, 2i32)}
  [array<i32>] a1{array<i32>(3i32)}
  [array<i32>] a2{array<i32>(4i32, 5i32, 6i32)}
  [Pointer<array<i32>>] r0{location(a0)}
  [Pointer<array<i32>>] r1{location(a1)}
  [Pointer<array<i32>>] r2{location(a2)}

  [array<i32>] b0{array<i32>(7i32)}
  [array<i32>] b1{array<i32>(8i32, 9i32)}
  [array<i32>] b2{array<i32>(10i32)}
  [Pointer<array<i32>>] s0{location(b0)}
  [Pointer<array<i32>>] s1{location(b1)}
  [Pointer<array<i32>>] s2{location(b2)}

  [array<i32>] c0{array<i32>(11i32, 12i32)}
  [array<i32>] c1{array<i32>(13i32, 14i32, 15i32)}
  [Pointer<array<i32>>] t0{location(c0)}
  [Pointer<array<i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 11);
}

TEST_CASE("ir lowerer materializes variadic pointer array packs with indexed dereference access helpers") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer<array<i32>>>] values) {
  [auto] head{at_unsafe(dereference(at(values, 0i32)), 1i32)}
  return(plus(head, dereference(at(values, 2i32)).at(0i32)))
}

[return<int>]
forward([args<Pointer<array<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<array<i32>>>] values) {
  [array<i32>] extra{array<i32>(1i32, 2i32)}
  [Pointer<array<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [array<i32>] a0{array<i32>(1i32, 2i32)}
  [array<i32>] a1{array<i32>(3i32)}
  [array<i32>] a2{array<i32>(4i32, 5i32, 6i32)}
  [Pointer<array<i32>>] r0{location(a0)}
  [Pointer<array<i32>>] r1{location(a1)}
  [Pointer<array<i32>>] r2{location(a2)}

  [array<i32>] b0{array<i32>(7i32, 8i32)}
  [array<i32>] b1{array<i32>(9i32)}
  [array<i32>] b2{array<i32>(10i32, 11i32)}
  [Pointer<array<i32>>] s0{location(b0)}
  [Pointer<array<i32>>] s1{location(b1)}
  [Pointer<array<i32>>] s2{location(b2)}

  [array<i32>] c0{array<i32>(12i32)}
  [array<i32>] c1{array<i32>(13i32, 14i32, 15i32)}
  [Pointer<array<i32>>] t0{location(c0)}
  [Pointer<array<i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 39);
}

TEST_CASE("ir lowerer materializes variadic pointer map packs with indexed count methods") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32)}
  [Pointer</std/collections/map<i32, i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] r0{location(a0)}
  [Pointer</std/collections/map<i32, i32>>] r1{location(a1)}
  [Pointer</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32, 17i32, 18i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(19i32, 20i32)}
  [Pointer</std/collections/map<i32, i32>>] s0{location(b0)}
  [Pointer</std/collections/map<i32, i32>>] s1{location(b1)}
  [Pointer</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32, 23i32, 24i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)}
  [Pointer</std/collections/map<i32, i32>>] t0{location(c0)}
  [Pointer</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 11);
}

TEST_CASE("ir lowerer materializes variadic pointer map packs with indexed dereference count methods") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(plus(dereference(at(values, 0i32)).count(),
              dereference(at(values, 2i32)).count()))
}

[return<int>]
forward([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32)}
  [Pointer</std/collections/map<i32, i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] r0{location(a0)}
  [Pointer</std/collections/map<i32, i32>>] r1{location(a1)}
  [Pointer</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32, 17i32, 18i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(19i32, 20i32)}
  [Pointer</std/collections/map<i32, i32>>] s0{location(b0)}
  [Pointer</std/collections/map<i32, i32>>] s1{location(b1)}
  [Pointer</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32, 23i32, 24i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)}
  [Pointer</std/collections/map<i32, i32>>] t0{location(c0)}
  [Pointer</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 11);
}

TEST_CASE("ir lowerer materializes variadic pointer map packs with indexed lookup helpers") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [auto] head{/std/collections/map/at_unsafe(at(values, 0i32), 3i32)}
  if(at(values, 2i32).contains(11i32),
     then(){ return(plus(head, at(values, 2i32).at(11i32))) },
     else(){ return(0i32) })
}

[return<int>]
forward([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(19i32, 20i32, 3i32, 4i32)}
  [Pointer</std/collections/map<i32, i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] r0{location(a0)}
  [Pointer</std/collections/map<i32, i32>>] r1{location(a1)}
  [Pointer</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(17i32, 18i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] s0{location(b0)}
  [Pointer</std/collections/map<i32, i32>>] s1{location(b1)}
  [Pointer</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(23i32, 24i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] t0{location(c0)}
  [Pointer</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 48);
}

TEST_CASE("ir lowerer materializes variadic pointer map packs with indexed dereference lookup helpers") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [auto] head{dereference(at(values, 0i32)).at_unsafe(3i32)}
  if(dereference(at(values, 2i32)).contains(11i32),
     then(){ return(plus(head, dereference(at(values, 2i32)).at(11i32))) },
     else(){ return(0i32) })
}

[return<int>]
forward([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(19i32, 20i32, 3i32, 4i32)}
  [Pointer</std/collections/map<i32, i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] r0{location(a0)}
  [Pointer</std/collections/map<i32, i32>>] r1{location(a1)}
  [Pointer</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(17i32, 18i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] s0{location(b0)}
  [Pointer</std/collections/map<i32, i32>>] s1{location(b1)}
  [Pointer</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(23i32, 24i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] t0{location(c0)}
  [Pointer</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 48);
}

TEST_CASE("ir lowerer materializes variadic pointer map packs with indexed tryAt inference" * doctest::skip()) {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [auto] head{try(at(values, 0i32).tryAt(3i32))}
  [auto] tailMissing{Result.error(/std/collections/map/tryAt(at(values, minus(count(values), 1i32)), 99i32))}
  return(if(tailMissing,
            then(){ return(plus(head, /std/collections/map/at_unsafe(at(values, minus(count(values), 1i32)), 11i32))) },
            else(){ return(0i32) }))
}

[return<int>]
forward([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32, 3i32, 8i32)}
  [Pointer</std/collections/map<i32, i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] r0{location(a0)}
  [Pointer</std/collections/map<i32, i32>>] r1{location(a1)}
  [Pointer</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(1i32, 2i32, 3i32, 6i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 14i32)}
  [Pointer</std/collections/map<i32, i32>>] s0{location(b0)}
  [Pointer</std/collections/map<i32, i32>>] s1{location(b1)}
  [Pointer</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 16i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(11i32, 16i32, 13i32, 14i32)}
  [Pointer</std/collections/map<i32, i32>>] t0{location(c0)}
  [Pointer</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 60);
}

TEST_CASE("ir lowerer materializes variadic pointer vector packs with indexed count methods") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer<vector<i32>>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<Pointer<vector<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] extra{vector<i32>(1i32)}
  [Pointer<vector<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vector<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32, 7i32)}
  [Pointer<vector<i32>>] r0{location(a0)}
  [Pointer<vector<i32>>] r1{location(a1)}
  [Pointer<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vector<i32>(8i32)}
  [vector<i32>] b1{vector<i32>(9i32, 10i32)}
  [vector<i32>] b2{vector<i32>(11i32, 12i32, 13i32)}
  [Pointer<vector<i32>>] s0{location(b0)}
  [Pointer<vector<i32>>] s1{location(b1)}
  [Pointer<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(14i32, 15i32)}
  [vector<i32>] c1{vector<i32>(16i32, 17i32, 18i32, 19i32, 20i32)}
  [Pointer<vector<i32>>] t0{location(c0)}
  [Pointer<vector<i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 16);
}

TEST_CASE("ir lowerer materializes variadic pointer vector packs with indexed dereference capacity methods") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer<vector<i32>>>] values) {
  [auto] head{capacity(dereference(at(values, 0i32)))}
  return(plus(head, dereference(at(values, 2i32)).capacity()))
}

[return<int>]
forward([args<Pointer<vector<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] extra{vector<i32>(1i32)}
  [Pointer<vector<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vector<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32, 7i32)}
  [Pointer<vector<i32>>] r0{location(a0)}
  [Pointer<vector<i32>>] r1{location(a1)}
  [Pointer<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vector<i32>(8i32)}
  [vector<i32>] b1{vector<i32>(9i32, 10i32)}
  [vector<i32>] b2{vector<i32>(11i32, 12i32, 13i32)}
  [Pointer<vector<i32>>] s0{location(b0)}
  [Pointer<vector<i32>>] s1{location(b1)}
  [Pointer<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(14i32, 15i32)}
  [vector<i32>] c1{vector<i32>(16i32, 17i32, 18i32, 19i32, 20i32)}
  [Pointer<vector<i32>>] t0{location(c0)}
  [Pointer<vector<i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 16);
}

TEST_CASE("ir lowerer materializes variadic pointer vector packs with indexed dereference access helpers") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer<vector<i32>>>] values) {
  [auto] head{at_unsafe(dereference(at(values, 0i32)), 1i32)}
  return(plus(head, dereference(at(values, 2i32)).at(0i32)))
}

[return<int>]
forward([args<Pointer<vector<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] extra{vector<i32>(1i32, 2i32)}
  [Pointer<vector<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vector<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}
  [Pointer<vector<i32>>] r0{location(a0)}
  [Pointer<vector<i32>>] r1{location(a1)}
  [Pointer<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vector<i32>(7i32, 8i32)}
  [vector<i32>] b1{vector<i32>(9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32)}
  [Pointer<vector<i32>>] s0{location(b0)}
  [Pointer<vector<i32>>] s1{location(b1)}
  [Pointer<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(12i32)}
  [vector<i32>] c1{vector<i32>(13i32, 14i32, 15i32)}
  [Pointer<vector<i32>>] t0{location(c0)}
  [Pointer<vector<i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 39);
}

TEST_CASE("ir lowerer materializes variadic pointer vector packs with indexed dereference statement mutators") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
mutate_ptrs([args<Pointer<vector<i32>>>] values) {
  push(dereference(at(values, 0i32)), 9i32)
  dereference(at(values, 0i32)).pop()
  reserve(dereference(at(values, 1i32)), 6i32)
  dereference(at(values, 1i32)).clear()
  remove_at(dereference(at(values, 2i32)), 1i32)
  dereference(at(values, 2i32)).remove_swap(0i32)
  return(plus(dereference(at(values, 0i32)).count(),
              plus(dereference(at(values, 1i32)).capacity(),
                   dereference(at(values, 2i32)).count())))
}

[effects(heap_alloc), return<int>]
forward([args<Pointer<vector<i32>>>] values) {
  return(mutate_ptrs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] extra{vector<i32>(20i32)}
  [Pointer<vector<i32>>] extra_ptr{location(extra)}
  return(mutate_ptrs(extra_ptr, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vector<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}
  [Pointer<vector<i32>>] r0{location(a0)}
  [Pointer<vector<i32>>] r1{location(a1)}
  [Pointer<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vector<i32>(7i32)}
  [vector<i32>] b1{vector<i32>(8i32, 9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32, 12i32)}
  [Pointer<vector<i32>>] s0{location(b0)}
  [Pointer<vector<i32>>] s1{location(b1)}
  [Pointer<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(13i32, 14i32)}
  [vector<i32>] c1{vector<i32>(15i32, 16i32, 17i32)}
  [Pointer<vector<i32>>] t0{location(c0)}
  [Pointer<vector<i32>>] t1{location(c1)}

  return(plus(mutate_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 25);
}

TEST_CASE("ir lowerer materializes variadic borrowed vector packs with indexed count methods") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<vector<i32>>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<Reference<vector<i32>>>] values) {
  return(score_refs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Reference<vector<i32>>>] values) {
  [vector<i32>] extra{vector<i32>(1i32)}
  [Reference<vector<i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vector<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32, 7i32)}
  [Reference<vector<i32>>] r0{location(a0)}
  [Reference<vector<i32>>] r1{location(a1)}
  [Reference<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vector<i32>(8i32)}
  [vector<i32>] b1{vector<i32>(9i32, 10i32)}
  [vector<i32>] b2{vector<i32>(11i32, 12i32, 13i32)}
  [Reference<vector<i32>>] s0{location(b0)}
  [Reference<vector<i32>>] s1{location(b1)}
  [Reference<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(14i32, 15i32)}
  [vector<i32>] c1{vector<i32>(16i32, 17i32, 18i32, 19i32, 20i32)}
  [Reference<vector<i32>>] t0{location(c0)}
  [Reference<vector<i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 16);
}

TEST_CASE("ir lowerer materializes variadic borrowed vector packs with indexed dereference capacity methods") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<vector<i32>>>] values) {
  [auto] head{capacity(dereference(at(values, 0i32)))}
  return(plus(head, dereference(at(values, 2i32)).capacity()))
}

[return<int>]
forward([args<Reference<vector<i32>>>] values) {
  return(score_refs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Reference<vector<i32>>>] values) {
  [vector<i32>] extra{vector<i32>(1i32)}
  [Reference<vector<i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vector<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32, 7i32)}
  [Reference<vector<i32>>] r0{location(a0)}
  [Reference<vector<i32>>] r1{location(a1)}
  [Reference<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vector<i32>(8i32)}
  [vector<i32>] b1{vector<i32>(9i32, 10i32)}
  [vector<i32>] b2{vector<i32>(11i32, 12i32, 13i32)}
  [Reference<vector<i32>>] s0{location(b0)}
  [Reference<vector<i32>>] s1{location(b1)}
  [Reference<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(14i32, 15i32)}
  [vector<i32>] c1{vector<i32>(16i32, 17i32, 18i32, 19i32, 20i32)}
  [Reference<vector<i32>>] t0{location(c0)}
  [Reference<vector<i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 16);
}

TEST_CASE("ir lowerer materializes variadic borrowed vector packs with indexed dereference access helpers") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<vector<i32>>>] values) {
  [auto] head{at_unsafe(dereference(at(values, 0i32)), 1i32)}
  return(plus(head, dereference(at(values, 2i32)).at(0i32)))
}

[return<int>]
forward([args<Reference<vector<i32>>>] values) {
  return(score_refs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Reference<vector<i32>>>] values) {
  [vector<i32>] extra{vector<i32>(1i32, 2i32)}
  [Reference<vector<i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vector<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}
  [Reference<vector<i32>>] r0{location(a0)}
  [Reference<vector<i32>>] r1{location(a1)}
  [Reference<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vector<i32>(7i32, 8i32)}
  [vector<i32>] b1{vector<i32>(9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32)}
  [Reference<vector<i32>>] s0{location(b0)}
  [Reference<vector<i32>>] s1{location(b1)}
  [Reference<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(12i32)}
  [vector<i32>] c1{vector<i32>(13i32, 14i32, 15i32)}
  [Reference<vector<i32>>] t0{location(c0)}
  [Reference<vector<i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 39);
}

TEST_CASE("ir lowerer materializes variadic borrowed vector packs with indexed dereference statement mutators") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
mutate_refs([args<Reference<vector<i32>>>] values) {
  push(dereference(at(values, 0i32)), 9i32)
  dereference(at(values, 0i32)).pop()
  reserve(dereference(at(values, 1i32)), 6i32)
  dereference(at(values, 1i32)).clear()
  remove_at(dereference(at(values, 2i32)), 1i32)
  dereference(at(values, 2i32)).remove_swap(0i32)
  return(plus(dereference(at(values, 0i32)).count(),
              plus(dereference(at(values, 1i32)).capacity(),
                   dereference(at(values, 2i32)).count())))
}

[effects(heap_alloc), return<int>]
forward([args<Reference<vector<i32>>>] values) {
  return(mutate_refs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Reference<vector<i32>>>] values) {
  [vector<i32>] extra{vector<i32>(20i32)}
  [Reference<vector<i32>>] extra_ref{location(extra)}
  return(mutate_refs(extra_ref, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vector<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}
  [Reference<vector<i32>>] r0{location(a0)}
  [Reference<vector<i32>>] r1{location(a1)}
  [Reference<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vector<i32>(7i32)}
  [vector<i32>] b1{vector<i32>(8i32, 9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32, 12i32)}
  [Reference<vector<i32>>] s0{location(b0)}
  [Reference<vector<i32>>] s1{location(b1)}
  [Reference<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(13i32, 14i32)}
  [vector<i32>] c1{vector<i32>(15i32, 16i32, 17i32)}
  [Reference<vector<i32>>] t0{location(c0)}
  [Reference<vector<i32>>] t1{location(c1)}

  return(plus(mutate_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 25);
}

TEST_CASE("ir lowerer materializes variadic borrowed soa_vector packs with indexed count methods") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
score_refs([args<Reference<soa_vector<Particle>>>] values) {
  return(plus(count(values), plus(values[0i32].count(), values[2i32].count())))
}

[return<int>]
forward([args<Reference<soa_vector<Particle>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<soa_vector<Particle>>>] values) {
  [soa_vector<Particle>] extra{soa_vector<Particle>()}
  [Reference<soa_vector<Particle>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [soa_vector<Particle>] a0{soa_vector<Particle>()}
  [soa_vector<Particle>] a1{soa_vector<Particle>()}
  [soa_vector<Particle>] a2{soa_vector<Particle>()}
  [Reference<soa_vector<Particle>>] r0{location(a0)}
  [Reference<soa_vector<Particle>>] r1{location(a1)}
  [Reference<soa_vector<Particle>>] r2{location(a2)}

  [soa_vector<Particle>] b0{soa_vector<Particle>()}
  [soa_vector<Particle>] b1{soa_vector<Particle>()}
  [soa_vector<Particle>] b2{soa_vector<Particle>()}
  [Reference<soa_vector<Particle>>] s0{location(b0)}
  [Reference<soa_vector<Particle>>] s1{location(b1)}
  [Reference<soa_vector<Particle>>] s2{location(b2)}

  [soa_vector<Particle>] c0{soa_vector<Particle>()}
  [soa_vector<Particle>] c1{soa_vector<Particle>()}
  [Reference<soa_vector<Particle>>] t0{location(c0)}
  [Reference<soa_vector<Particle>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 9);
}

TEST_CASE("ir lowerer materializes variadic pointer soa_vector packs with indexed count methods") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
score_ptrs([args<Pointer<soa_vector<Particle>>>] values) {
  return(plus(count(values), plus(values[0i32].count(), values[2i32].count())))
}

[return<int>]
forward([args<Pointer<soa_vector<Particle>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<soa_vector<Particle>>>] values) {
  [soa_vector<Particle>] extra{soa_vector<Particle>()}
  [Pointer<soa_vector<Particle>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [soa_vector<Particle>] a0{soa_vector<Particle>()}
  [soa_vector<Particle>] a1{soa_vector<Particle>()}
  [soa_vector<Particle>] a2{soa_vector<Particle>()}
  [Pointer<soa_vector<Particle>>] r0{location(a0)}
  [Pointer<soa_vector<Particle>>] r1{location(a1)}
  [Pointer<soa_vector<Particle>>] r2{location(a2)}

  [soa_vector<Particle>] b0{soa_vector<Particle>()}
  [soa_vector<Particle>] b1{soa_vector<Particle>()}
  [soa_vector<Particle>] b2{soa_vector<Particle>()}
  [Pointer<soa_vector<Particle>>] s0{location(b0)}
  [Pointer<soa_vector<Particle>>] s1{location(b1)}
  [Pointer<soa_vector<Particle>>] s2{location(b2)}

  [soa_vector<Particle>] c0{soa_vector<Particle>()}
  [soa_vector<Particle>] c1{soa_vector<Particle>()}
  [Pointer<soa_vector<Particle>>] t0{location(c0)}
  [Pointer<soa_vector<Particle>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
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
  CHECK(result == 9);
}

TEST_CASE("ir lowerer materializes variadic soa_vector packs with indexed count methods") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
score_soas([args<soa_vector<Particle>>] values) {
  return(plus(count(values), plus(values[0i32].count(), values[2i32].count())))
}

[return<int>]
forward([args<soa_vector<Particle>>] values) {
  return(score_soas([spread] values))
}

[return<int>]
forward_mixed([args<soa_vector<Particle>>] values) {
  return(score_soas(soa_vector<Particle>(), [spread] values))
}

[return<int>]
main() {
  return(plus(score_soas(soa_vector<Particle>(), soa_vector<Particle>(), soa_vector<Particle>()),
              plus(forward(soa_vector<Particle>(), soa_vector<Particle>(), soa_vector<Particle>()),
                   forward_mixed(soa_vector<Particle>(), soa_vector<Particle>()))))
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
  CHECK(result == 9);
}

TEST_CASE("ir lowerer materializes variadic map packs with indexed count methods") {
  const std::string source = R"(
[return<int>]
score_maps([args<map<i32, i32>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<map<i32, i32>>] values) {
  return(score_maps([spread] values))
}

[return<int>]
forward_mixed([args<map<i32, i32>>] values) {
  return(score_maps(map<i32, i32>(1i32, 2i32), [spread] values))
}

[return<int>]
main() {
  return(plus(score_maps(map<i32, i32>(1i32, 2i32, 3i32, 4i32),
                         map<i32, i32>(5i32, 6i32),
                         map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)),
              plus(forward(map<i32, i32>(13i32, 14i32),
                           map<i32, i32>(15i32, 16i32, 17i32, 18i32),
                           map<i32, i32>(19i32, 20i32)),
                   forward_mixed(map<i32, i32>(21i32, 22i32, 23i32, 24i32),
                                 map<i32, i32>(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)))))
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
  CHECK(result == 11);
}

TEST_CASE("ir lowerer materializes variadic map packs with indexed tryAt inference" * doctest::skip()) {
  const std::string source = R"(
[return<int>]
score_maps([args<map<i32, i32>>] values) {
  [auto] head{try(at(values, 0i32).tryAt(3i32))}
  [auto] tailMissing{Result.error(/std/collections/map/tryAt(at(values, minus(count(values), 1i32)), 99i32))}
  return(if(tailMissing,
            then(){ return(plus(head, /std/collections/map/at_unsafe(at(values, minus(count(values), 1i32)), 11i32))) },
            else(){ return(0i32) }))
}

[return<int>]
forward([args<map<i32, i32>>] values) {
  return(score_maps([spread] values))
}

[return<int>]
forward_mixed([args<map<i32, i32>>] values) {
  return(score_maps(map<i32, i32>(1i32, 2i32, 3i32, 8i32), [spread] values))
}

[return<int>]
main() {
  return(plus(score_maps(map<i32, i32>(1i32, 2i32, 3i32, 4i32),
                         map<i32, i32>(5i32, 6i32),
                         map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)),
              plus(forward(map<i32, i32>(1i32, 2i32, 3i32, 6i32),
                           map<i32, i32>(5i32, 6i32),
                           map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 14i32)),
                   forward_mixed(map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 16i32)))))
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
  CHECK(result == 60);
}

TEST_CASE("ir lowerer forwards count to method calls") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  count([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(count(4i32))
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

TEST_CASE("ir lowerer supports array method calls") {
  const std::string source = R"(
[return<int>]
/array/first([array<i32>] items) {
  return(items[0i32])
}

[return<int>]
main() {
  [array<i32>] items{array<i32>(5i32, 9i32)}
  return(items.first())
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

TEST_CASE("ir lowerer supports vector method calls") {
  const std::string source = R"(
[return<int>]
/vector/first([vector<i32>] items) {
  return(items[0i32])
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] items{vector<i32>(5i32, 9i32)}
  return(items.first())
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

TEST_CASE("ir lowerer supports map method calls") {
  const std::string source = R"(
[return<int>]
/map/size([map<i32, i32>] items) {
  return(/std/collections/map/count(items))
}

[return<int>]
/std/collections/map/count([map<i32, i32>] items) {
  return(1i32)
}

[return<int>]
main() {
  [map<i32, i32>] items{map<i32, i32>(1i32, 2i32)}
  return(items.size())
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
  CHECK(result == 1);
}

TEST_CASE("ir lowerer supports entry args print index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1i32])
  return(0i32)
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
  REQUIRE(module.functions.size() == 1);
  bool sawPrintArgv = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PrintArgv) {
      sawPrintArgv = true;
      break;
    }
  }
  CHECK(sawPrintArgv);
}

TEST_CASE("ir lowerer supports entry args print without newline") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print(args[1i32])
  return(0i32)
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
  REQUIRE(module.functions.size() == 1);
  bool sawPrintArgv = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PrintArgv) {
      sawPrintArgv = true;
      CHECK((primec::decodePrintFlags(inst.imm) & primec::PrintFlagNewline) == 0);
      CHECK((primec::decodePrintFlags(inst.imm) & primec::PrintFlagStderr) == 0);
      break;
    }
  }
  CHECK(sawPrintArgv);
}

TEST_CASE("ir lowerer supports entry args print_error without newline") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(args[1i32])
  return(0i32)
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
  REQUIRE(module.functions.size() == 1);
  bool sawPrintArgv = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PrintArgv) {
      sawPrintArgv = true;
      const uint64_t flags = primec::decodePrintFlags(inst.imm);
      CHECK((flags & primec::PrintFlagNewline) == 0);
      CHECK((flags & primec::PrintFlagStderr) != 0);
      break;
    }
  }
  CHECK(sawPrintArgv);
}

TEST_CASE("ir lowerer supports entry args print_line_error") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_line_error(args[1i32])
  return(0i32)
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
  REQUIRE(module.functions.size() == 1);
  bool sawPrintArgv = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PrintArgv) {
      sawPrintArgv = true;
      const uint64_t flags = primec::decodePrintFlags(inst.imm);
      CHECK((flags & primec::PrintFlagNewline) != 0);
      CHECK((flags & primec::PrintFlagStderr) != 0);
      break;
    }
  }
  CHECK(sawPrintArgv);
}

TEST_CASE("ir lowerer supports entry args print_line_error unsafe") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_line_error(at_unsafe(args, 1i32))
  return(0i32)
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
  REQUIRE(module.functions.size() == 1);
  bool sawPrintArgvUnsafe = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PrintArgvUnsafe) {
      sawPrintArgvUnsafe = true;
      const uint64_t flags = primec::decodePrintFlags(inst.imm);
      CHECK((flags & primec::PrintFlagNewline) != 0);
      CHECK((flags & primec::PrintFlagStderr) != 0);
      break;
    }
  }
  CHECK(sawPrintArgvUnsafe);
}


TEST_CASE("ir lowerer supports entry args print_error unsafe") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(at_unsafe(args, 1i32))
  return(0i32)
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
  REQUIRE(module.functions.size() == 1);
  bool sawPrintArgvUnsafe = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PrintArgvUnsafe) {
      sawPrintArgvUnsafe = true;
      const uint64_t flags = primec::decodePrintFlags(inst.imm);
      CHECK((flags & primec::PrintFlagNewline) == 0);
      CHECK((flags & primec::PrintFlagStderr) != 0);
      break;
    }
  }
  CHECK(sawPrintArgvUnsafe);
}

TEST_SUITE_END();
