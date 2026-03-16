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

[return<int>]
forward_mixed([args<Reference<vector<i32>>>] values) {
  [vector<i32>] extra{vector<i32>(1i32)}
  [Reference<vector<i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
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
