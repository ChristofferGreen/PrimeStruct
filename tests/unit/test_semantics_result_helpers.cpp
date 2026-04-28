#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.result_helpers");

TEST_CASE("Result.error in if conditions is bool-compatible") {
  const std::string source = R"(
[return<Result<FileError>>]
main() {
  [Result<FileError>] status{ Result.ok() }
  if(Result.error(status), then(){ return(Result.ok()) }, else(){ return(Result.ok()) })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("direct Result.ok expressions participate in Result helpers") {
  const std::string source = R"(
swallow_file_error([FileError] err) {}

[return<int> on_error<FileError, /swallow_file_error>]
main() {
  [Result<i32, FileError>] status{ Result.ok(7i32) }
  [bool] is_error{ Result.error(status) }
  [string] why{ Result.why(status) }
  [i32] value{ try(status) }
  if(or(is_error, not(equal(value, 7i32))), then(){ return(1i32) }, else(){ return(0i32) })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib result value sum validates explicit ok and error variants") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{5i32}
}

[return<int>]
main() {
  [Result<i32, MyError>] success{ok<i32, MyError>(7i32)}
  [Result<i32, MyError>] failure{error<i32, MyError>(MyError{})}
  [i32] left{pick(success) {
    ok(value) {
      value
    }
    error(err) {
      err.code
    }
  }}
  [i32] right{pick(failure) {
    ok(value) {
      value
    }
    error(err) {
      err.code
    }
  }}
  return(left + right)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib result value sum participates in Result.error") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{5i32}
}

[return<int>]
main() {
  [Result<i32, MyError>] success{ok<i32, MyError>(7i32)}
  [Result<i32, MyError>] failure{error<i32, MyError>(MyError{})}
  if(Result.error(success)) {
    return(1i32)
  }
  if(not(Result.error(failure))) {
    return(2i32)
  }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib result value sum participates in Result.why") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{5i32}
}

namespace MyError {
  [return<string>]
  why([MyError] err) {
    if(equal(err.code, 5i32)) {
      return("five"utf8)
    }
    return("other"utf8)
  }
}

[return<void>]
main() {
  [Result<i32, MyError>] success{ok<i32, MyError>(7i32)}
  [Result<i32, MyError>] failure{error<i32, MyError>(MyError{})}
  [string] successWhy{Result.why(success)}
  [string] failureWhy{Result.why(failure)}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib result value sum accepts legacy Result.ok") {
  const std::string source = R"(
import /std/result/*

[return<Result<i32, i32>>]
make_success() {
  return(Result.ok(7i32))
}

[return<int>]
main() {
  [Result<i32, i32>] localSuccess{Result.ok(5i32)}
  [Result<i32, i32>] returnedSuccess{make_success()}
  [Result<i32, i32>] failure{error<i32, i32>(3i32)}
  [i32] localValue{pick(localSuccess) {
    ok(value) {
      value
    }
    error(err) {
      100i32
    }
  }}
  [i32] returnedValue{pick(returnedSuccess) {
    ok(value) {
      value
    }
    error(err) {
      101i32
    }
  }}
  [i32] failureValue{pick(failure) {
    ok(value) {
      102i32
    }
    error(err) {
      err
    }
  }}
  return(plus(plus(localValue, returnedValue), failureValue))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("qualified stdlib Result sum participates in try") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{5i32}
}

[return<void>]
ignore([MyError] err) {
}

[return</std/result/Result<i32, MyError>> on_error<MyError, /ignore>]
main() {
  [/std/result/Result<i32, MyError>] status{ok<i32, MyError>(7i32)}
  [i32] value{try(status)}
  return(ok<i32, MyError>(value))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("qualified stdlib Result try checks error type") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{5i32}
}

[struct]
OtherError() {
  [i32] code{6i32}
}

[return<void>]
ignore([MyError] err) {
}

[return</std/result/Result<i32, MyError>> on_error<MyError, /ignore>]
main() {
  [/std/result/Result<i32, OtherError>] status{error<i32, OtherError>(OtherError{})}
  [i32] value{try(status)}
  return(ok<i32, MyError>(value))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("try error type mismatch") != std::string::npos);
}

TEST_CASE("stdlib result value sum accepts legacy Result.map") {
  const std::string source = R"(
import /std/result/*

[return<Result<i32, i32>>]
make_mapped_success() {
  [Result<i32, i32>] source{Result.ok(7i32)}
  return(Result.map(source, []([i32] value) { return(plus(value, 4i32)) }))
}

[return<int>]
main() {
  [Result<i32, i32>] okSource{Result.ok(5i32)}
  [Result<i32, i32>] errorSource{error<i32, i32>(3i32)}
  [Result<i32, i32>] mappedOk{
    Result.map(okSource, []([i32] value) { return(plus(value, 2i32)) })
  }
  [Result<i32, i32>] mappedError{
    Result.map(errorSource, []([i32] value) { return(plus(value, 2i32)) })
  }
  [Result<i32, i32>] returnedMapped{make_mapped_success()}
  [i32] okValue{pick(mappedOk) {
    ok(value) {
      value
    }
    error(err) {
      100i32
    }
  }}
  [i32] errorValue{pick(mappedError) {
    ok(value) {
      101i32
    }
    error(err) {
      err
    }
  }}
  [i32] returnedValue{pick(returnedMapped) {
    ok(value) {
      value
    }
    error(err) {
      102i32
    }
  }}
  return(plus(plus(okValue, errorValue), returnedValue))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib result value sum accepts legacy Result.and_then") {
  const std::string source = R"(
import /std/result/*

[return<Result<i32, i32>>]
make_chained_success() {
  [Result<i32, i32>] source{Result.ok(7i32)}
  return(Result.and_then(source, []([i32] value) { return(Result.ok(plus(value, 4i32))) }))
}

[return<int>]
main() {
  [Result<i32, i32>] okSource{Result.ok(5i32)}
  [Result<i32, i32>] errorSource{error<i32, i32>(3i32)}
  [Result<i32, i32>] chainedOk{
    Result.and_then(okSource, []([i32] value) { return(Result.ok(plus(value, 2i32))) })
  }
  [Result<i32, i32>] chainedToError{
    Result.and_then(okSource, []([i32] value) { return(Result<i32, i32>{[error] 4i32}) })
  }
  [Result<i32, i32>] chainedError{
    Result.and_then(errorSource, []([i32] value) { return(Result.ok(plus(value, 2i32))) })
  }
  [Result<i32, i32>] returnedChained{make_chained_success()}
  [i32] okValue{pick(chainedOk) {
    ok(value) {
      value
    }
    error(err) {
      100i32
    }
  }}
  [i32] toErrorValue{pick(chainedToError) {
    ok(value) {
      101i32
    }
    error(err) {
      err
    }
  }}
  [i32] errorValue{pick(chainedError) {
    ok(value) {
      102i32
    }
    error(err) {
      err
    }
  }}
  [i32] returnedValue{pick(returnedChained) {
    ok(value) {
      value
    }
    error(err) {
      103i32
    }
  }}
  return(plus(plus(okValue, toErrorValue), plus(errorValue, returnedValue)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib result value sum accepts legacy Result.map2") {
  const std::string source = R"(
import /std/result/*

[return<Result<i32, i32>>]
make_summed_success() {
  [Result<i32, i32>] left{Result.ok(7i32)}
  [Result<i32, i32>] right{Result.ok(4i32)}
  return(Result.map2(left, right, []([i32] a, [i32] b) { return(plus(a, b)) }))
}

[return<int>]
main() {
  [Result<i32, i32>] leftOk{Result.ok(5i32)}
  [Result<i32, i32>] rightOk{Result.ok(2i32)}
  [Result<i32, i32>] leftError{error<i32, i32>(3i32)}
  [Result<i32, i32>] rightError{error<i32, i32>(4i32)}
  [Result<i32, i32>] summed{
    Result.map2(leftOk, rightOk, []([i32] a, [i32] b) { return(plus(a, b)) })
  }
  [Result<i32, i32>] firstError{
    Result.map2(leftError, rightError, []([i32] a, [i32] b) { return(plus(a, b)) })
  }
  [Result<i32, i32>] secondError{
    Result.map2(leftOk, rightError, []([i32] a, [i32] b) { return(plus(a, b)) })
  }
  [Result<i32, i32>] returnedSummed{make_summed_success()}
  [i32] summedValue{pick(summed) {
    ok(value) {
      value
    }
    error(err) {
      100i32
    }
  }}
  [i32] firstErrorValue{pick(firstError) {
    ok(value) {
      101i32
    }
    error(err) {
      err
    }
  }}
  [i32] secondErrorValue{pick(secondError) {
    ok(value) {
      102i32
    }
    error(err) {
      err
    }
  }}
  [i32] returnedValue{pick(returnedSummed) {
    ok(value) {
      value
    }
    error(err) {
      103i32
    }
  }}
  return(plus(plus(summedValue, firstErrorValue), plus(secondErrorValue, returnedValue)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib result value sum rejects default construction") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{5i32}
}

[return<int>]
main() {
  [Result<i32, MyError>] status{}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("default sum construction requires first variant to be unit") !=
        std::string::npos);
}

TEST_CASE("status-only Result pick reports compatibility diagnostic") {
  auto checkDiagnostic = [](const std::string &source,
                            const std::string &expectedErrorDomain) {
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("status-only Result<") != std::string::npos);
    CHECK(error.find(expectedErrorDomain) != std::string::npos);
    CHECK(error.find("is not a stdlib Result sum") != std::string::npos);
  };

  checkDiagnostic(R"(
import /std/result/*

[return<int>]
main() {
  [Result<FileError>] status{ Result.ok() }
  [i32] value{pick(status) {
    ok {
      0i32
    }
    error(err) {
      1i32
    }
  }}
  return(value)
}
)",
                  "FileError");

  checkDiagnostic(R"(
import /std/result/*
import /std/collections/*

[return<int>]
main() {
  [Result<ContainerError>] status{ Result.ok() }
  [i32] value{pick(status) {
    ok {
      0i32
    }
    error(err) {
      1i32
    }
  }}
  return(value)
}
)",
                  "ContainerError");

  checkDiagnostic(R"(
import /std/result/*
import /std/gfx/*

[return<int>]
main() {
  [Result<GfxError>] status{ Result.ok() }
  [i32] value{pick(status) {
    ok {
      0i32
    }
    error(err) {
      1i32
    }
  }}
  return(value)
}
)",
                  "GfxError");
}

TEST_CASE("Result.error rejects non-result argument") {
  const std::string source = R"(
[return<void>]
main() {
  if(Result.error(1i32), then(){ return() }, else(){ return() })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("Result.error requires Result argument") != std::string::npos);
}

TEST_CASE("Result.why explicit string binding accepts FileError results") {
  const std::string source = R"(
[return<void>]
main() {
  [Result<FileError>] status{ Result.ok() }
  [string] message{ Result.why(status) }
  return()
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("Result.why explicit string binding accepts non-FileError results") {
  const std::string source = R"(
[struct]
OtherError() {
  [i32] code{1i32}
}

[return<void>]
main() {
  [Result<OtherError>] status{ Result.ok() }
  [string] message{ Result.why(status) }
  return()
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("Result.why infers string binding") {
  const std::string source = R"(
[return<void>]
main() {
  [Result<FileError>] status{ Result.ok() }
  [auto] message{ Result.why(status) }
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("Result.error infers bool binding") {
  const std::string source = R"(
[return<void>]
main() {
  [Result<FileError>] status{ Result.ok() }
  [auto] is_error{ Result.error(status) }
  if(is_error, then(){ return() }, else(){ return() })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("templated helper overload families resolve by exact arity") {
  const std::string source = R"(
[struct]
Marker() {}

[return<i32>]
/helper/value<T>([T] value) {
  return(1i32)
}

[return<i32>]
/helper/value<A, B>([A] first, [B] second) {
  return(2i32)
}

[return<i32>]
/Marker/mark<T>([Marker] self, [T] value) {
  return(/helper/value(value))
}

[return<i32>]
/Marker/mark<A, B>([Marker] self, [A] first, [B] second) {
  return(/helper/value(first, second))
}

[return<void>]
main() {
  [Marker] marker{Marker{}}
  [i32] directOne{/helper/value(7i32)}
  [i32] directTwo{/helper/value(7i32, true)}
  [i32] methodOne{marker.mark(9i32)}
  [i32] methodTwo{marker.mark(9i32, false)}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("templated helper overload families still reject duplicate same-arity definitions") {
  const std::string source = R"(
[return<i32>]
/helper/value<T>([T] value) {
  return(1i32)
}

[return<i32>]
/helper/value<U>([U] other) {
  return(2i32)
}

[return<void>]
main() {
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate definition: /helper/value") != std::string::npos);
}

TEST_CASE("Result.map infers auto Result binding from lambda body") {
  const std::string source = R"(
[return<void>]
main() {
  [Result<i32, FileError>] status{ Result.ok(1i32) }
  [auto] mapped{ Result.map(status, []([i32] v) { return(plus(v, 1i32)) }) }
  [bool] failed{ Result.error(mapped) }
  [string] why{ Result.why(mapped) }
  if(failed, then(){ return() }, else(){ return() })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("Result.and_then and Result.map2 infer nested auto Result bindings") {
  const std::string source = R"(
[return<Result<i32, FileError>>]
lift([i32] value) {
  return(Result.ok(value))
}

[return<void>]
main() {
  [Result<i32, FileError>] first{ Result.ok(1i32) }
  [Result<i32, FileError>] second{ Result.ok(2i32) }
  [auto] summed{ Result.map2(first, second, []([i32] x, [i32] y) { return(plus(x, y)) }) }
  [auto] chained{ Result.and_then(summed, []([i32] total) { return(lift(plus(total, 1i32))) }) }
  [bool] failed{ Result.error(chained) }
  [string] why{ Result.why(chained) }
  if(failed, then(){ return() }, else(){ return() })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("Result.and_then infers direct Result.ok lambda return") {
  const std::string source = R"(
[effects(io_out)]
swallow_file_error([FileError] err) {}

[return<int> on_error<FileError, /swallow_file_error>]
main() {
  [Result<i32, FileError>] status{ Result.ok(4i32) }
  [auto] chained{ Result.and_then(status, []([i32] value) { return(Result.ok(plus(value, 2i32))) }) }
  [bool] failed{ Result.error(chained) }
  [string] why{ Result.why(chained) }
  [i32] value{ try(chained) }
  if(or(failed, not(equal(value, 6i32))), then(){ return(1i32) }, else(){ return(0i32) })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();
