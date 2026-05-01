#include "test_compile_run_helpers.h"

#include "test_compile_run_native_backend_core_helpers.h"

#if PRIMESTRUCT_NATIVE_CORE_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.core");

TEST_CASE("native materializes pure spread struct pack indexing and method access") {
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
  return(forward(Pair{7i32}, Pair{9i32}))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_struct_access_spread.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_struct_access_spread").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}

TEST_CASE("native materializes mixed struct pack indexing and method access") {
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
  return(score_pairs(Pair{5i32}, [spread] values))
}

[return<int>]
main() {
  return(forward(Pair{7i32}, Pair{9i32}))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_struct_access_mixed.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_struct_access_mixed").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 15);
}

TEST_CASE("native rejects variadic Result packs with indexed why and try access") {
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
  const std::string srcPath = writeTemp("compile_native_variadic_args_result.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_result").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_variadic_args_result.err").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Native lowering error: variadic parameter type mismatch") !=
        std::string::npos);
}

TEST_CASE("native materializes variadic borrowed Result packs with indexed dereference try and why access") {
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
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_result.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_result").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 24);
}

TEST_CASE("native materializes variadic pointer Result packs with indexed dereference try and why access") {
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
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_result.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_result").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 24);
}

TEST_CASE("native rejects variadic status-only Result packs with indexed error and why access") {
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
  [auto] tailHasError{Result.error(values[minus(count(values), 1i32)])}
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
  const std::string srcPath = writeTemp("compile_native_variadic_args_status_result.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_status_result").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_variadic_args_status_result.err").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Native lowering error: variadic parameter type mismatch") !=
        std::string::npos);
}

TEST_CASE("native materializes variadic borrowed status-only Result packs with indexed dereference error and why access") {
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
  [auto] tailHasError{Result.error(dereference(values[minus(count(values), 1i32)]))}
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
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_status_result.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_status_result").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 39);
}

TEST_CASE("native materializes variadic pointer status-only Result packs with indexed dereference error and why access") {
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
  [auto] tailHasError{Result.error(dereference(values[minus(count(values), 1i32)]))}
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
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_status_result.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_status_result").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 39);
}

TEST_CASE("native materializes variadic vector packs with indexed count methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_vectors([args<vector<i32>>] values) {
  [vector<i32>] head{values[0i32]}
  [vector<i32>] tail{values[2i32]}
  return(plus(/std/collections/vector/count(head),
              /std/collections/vector/count(tail)))
}

[return<int>]
forward([args<vector<i32>>] values) {
  return(score_vectors([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<vector<i32>>] values) {
  [vector<i32>] extra{vector<i32>(1i32, 99i32)}
  return(score_vectors(extra, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vector<i32>(3i32, 4i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}
  [vector<i32>] b0{vector<i32>(7i32, 8i32)}
  [vector<i32>] b1{vector<i32>(8i32, 9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32)}
  [vector<i32>] c0{vector<i32>(11i32, 12i32)}
  [vector<i32>] c1{vector<i32>(13i32, 14i32, 15i32)}
  return(plus(score_vectors(a0, a1, a2),
              plus(forward(b0, b1, b2),
                   forward_mixed(c0, c1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_vector.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_vector").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("native materializes variadic vector packs with indexed capacity methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_vectors([args<vector<i32>>] values) {
  [vector<i32>] head{values[0i32]}
  [vector<i32>] tail{values[2i32]}
  return(plus(/std/collections/vector/capacity(head),
              /std/collections/vector/capacity(tail)))
}

[return<int>]
forward([args<vector<i32>>] values) {
  return(score_vectors([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<vector<i32>>] values) {
  [vector<i32>] extra{vector<i32>(1i32, 99i32)}
  return(score_vectors(extra, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vector<i32>(3i32, 4i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}
  [vector<i32>] b0{vector<i32>(7i32, 8i32)}
  [vector<i32>] b1{vector<i32>(8i32, 9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32)}
  [vector<i32>] c0{vector<i32>(11i32, 12i32)}
  [vector<i32>] c1{vector<i32>(13i32, 14i32, 15i32)}
  return(plus(score_vectors(a0, a1, a2),
              plus(forward(b0, b1, b2),
                   forward_mixed(c0, c1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_vector_capacity.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_vector_capacity").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("native materializes variadic vector packs with indexed statement mutators") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
mutate_vectors([args<vector<i32>>] values) {
  [vector<i32> mut] head{values[0i32]}
  [vector<i32> mut] middle{values[1i32]}
  [vector<i32> mut] tail{values[2i32]}
  head.push(9i32)
  head.pop()
  middle.reserve(6i32)
  middle.clear()
  tail.remove_at(1i32)
  tail.remove_swap(0i32)
  return(plus(/std/collections/vector/count(head),
              plus(/std/collections/vector/capacity(middle),
                   /std/collections/vector/count(tail))))
}

[effects(heap_alloc), return<int>]
forward([args<vector<i32>>] values) {
  return(mutate_vectors([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<vector<i32>>] values) {
  [vector<i32>] extra{vectorSingle<i32>(20i32)}
  return(mutate_vectors(extra, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vectorSingle<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}

  [vector<i32>] b0{vectorSingle<i32>(7i32)}
  [vector<i32>] b1{vector<i32>(8i32, 9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32, 12i32)}

  [vector<i32>] c0{vector<i32>(13i32, 14i32)}
  [vector<i32>] c1{vector<i32>(15i32, 16i32, 17i32)}

  return(plus(mutate_vectors(a0, a1, a2),
              plus(forward(b0, b1, b2),
                   forward_mixed(c0, c1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_vector_mutators.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_vector_mutators").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 25);
}


TEST_SUITE_END();
#endif
