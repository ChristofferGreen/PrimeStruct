#include <cstring>
#include <string>
#include <vector>

#include "third_party/doctest.h"

#include "primec/Ast.h"
#include "primec/Ir.h"
#include "primec/IrSerializer.h"
#include "primec/Vm.h"
#include "test_ir_pipeline_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.conversions");

TEST_CASE("ir lowerer rejects variadic Result packs with indexed why and try access") {
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
  std::string error;
  primec::IrModule module;
  CHECK_FALSE(parseValidateAndLower(source, module, error));
  CHECK(error == "variadic parameter type mismatch");
}

TEST_CASE("ir lowerer materializes variadic borrowed Result packs with indexed dereference try and why access") {
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
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 24);
}

TEST_CASE("ir lowerer materializes variadic pointer Result packs with indexed dereference try and why access") {
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
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 24);
}

TEST_CASE("ir lowerer rejects variadic status-only Result packs with indexed error and why access") {
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
  std::string error;
  primec::IrModule module;
  CHECK_FALSE(parseValidateAndLower(source, module, error));
  CHECK(error == "variadic parameter type mismatch");
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
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
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
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 39);
}
