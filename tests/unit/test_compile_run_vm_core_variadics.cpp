#include "test_compile_run_helpers.h"

#include <cerrno>

TEST_SUITE_BEGIN("primestruct.compile.run.vm.core");


TEST_CASE("vm rejects recursive calls") {
  const std::string source = R"(
[return<int>]
main() {
  return(main())
}
)";
  const std::string srcPath = writeTemp("vm_recursive_call.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_recursive_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath) == "VM lowering error: vm backend does not support recursive calls: /main\n");
}

TEST_CASE("vm accepts string pointers") {
  const std::string source = R"(
[return<void>]
main() {
  [string] name{"hi"utf8}
  [Pointer<string>] ptr{location(name)}
}
)";
  const std::string srcPath = writeTemp("vm_string_pointer.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_string_pointer_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
}

TEST_CASE("vm rejects variadic pointer string packs") {
  const std::string source = R"(
[return<int>]
score([args<Pointer<string>>] values) {
  return(count(values))
}

[return<int>]
main() {
  [string] first{"first"utf8}
  [Pointer<string>] p0{location(first)}
  score(p0)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_variadic_args_pointer_string_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_variadic_args_pointer_string_reject_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("variadic args<T> does not support string pointers or references") != std::string::npos);
}

TEST_CASE("vm rejects variadic reference string packs") {
  const std::string source = R"(
[return<int>]
score([args<Reference<string>>] values) {
  return(count(values))
}

[return<int>]
main() {
  [string] first{"first"utf8}
  [Reference<string>] r0{location(first)}
  score(r0)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_variadic_args_reference_string_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_variadic_args_reference_string_reject_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("variadic args<T> does not support string pointers or references") != std::string::npos);
}

TEST_CASE("vm ignores top-level executions") {
  const std::string source = R"(
[return<bool>]
unused() {
  return(equal("a"utf8, "b"utf8))
}

[return<int>]
main() {
  return(0i32)
}

unused()
)";
  const std::string srcPath = writeTemp("vm_exec_ignored.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("vm materializes variadic Result packs with indexed why and try access") {
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
  const std::string srcPath = writeTemp("vm_variadic_args_result_value_pack.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 24);
}

TEST_CASE("vm materializes variadic status-only Result packs with indexed error and why access") {
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
  const std::string srcPath = writeTemp("vm_variadic_args_result_status_only_value_pack.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 39);
}

TEST_CASE("vm materializes variadic experimental map packs with indexed canonical count calls") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<int> effects(heap_alloc)]
score_maps([args<Map<i32, i32>>] values) {
  [Map<i32, i32>] head{at(values, 0i32)}
  [Map<i32, i32>] tail{at(values, 2i32)}
  return(plus(/std/collections/experimental_map/mapCount<i32, i32>(head),
              /std/collections/experimental_map/mapCount<i32, i32>(tail)))
}

[return<int> effects(heap_alloc)]
forward([args<Map<i32, i32>>] values) {
  return(score_maps([spread] values))
}

[return<int> effects(heap_alloc)]
forward_mixed([args<Map<i32, i32>>] values) {
  return(score_maps(mapSingle(1i32, 2i32), [spread] values))
}

[return<int> effects(heap_alloc)]
main() {
  return(plus(score_maps(mapPair(1i32, 2i32, 3i32, 4i32),
                         mapSingle(5i32, 6i32),
                         mapTriple(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)),
              plus(forward(mapSingle(13i32, 14i32),
                           mapPair(15i32, 16i32, 17i32, 18i32),
                           mapSingle(19i32, 20i32)),
                   forward_mixed(mapPair(21i32, 22i32, 23i32, 24i32),
                                 mapTriple(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)))))
}
)";
  const std::string srcPath = writeTemp("vm_variadic_args_experimental_map_count.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("vm forwards variadic Reference<Buffer> packs through helper methods") {
  const std::string source = R"(
import /std/gfx/*

[return<int> effects(gpu_dispatch)]
score([args<Reference<Buffer<i32>>>] values) {
  if(not(dereference(values[0i32]).empty())) {
    return(100i32)
  }
  if(not(dereference(values[1i32]).is_valid())) {
    return(101i32)
  }
  return(plus(dereference(values[1i32]).count(), dereference(values[2i32]).count()))
}

[return<int> effects(gpu_dispatch)]
main() {
  [Buffer<i32>] b0{/std/gfx/Buffer/allocate<i32>(0i32)}
  [Buffer<i32>] b1{/std/gfx/Buffer/allocate<i32>(3i32)}
  [Buffer<i32>] b2{/std/gfx/Buffer/allocate<i32>(2i32)}
  return(score(location(b0), location(b1), location(b2)))
}
)";
  const std::string srcPath = writeTemp("vm_variadic_args_buffer_reference_methods.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 5);
}

TEST_CASE("vm forwards variadic Pointer<Buffer> packs through helper methods") {
  const std::string source = R"(
import /std/gfx/*

[return<int> effects(gpu_dispatch)]
score([args<Pointer<Buffer<i32>>>] values) {
  if(not(dereference(values[0i32]).empty())) {
    return(100i32)
  }
  if(not(dereference(values[1i32]).is_valid())) {
    return(101i32)
  }
  return(plus(dereference(values[1i32]).count(), dereference(values[2i32]).count()))
}

[return<int> effects(gpu_dispatch)]
main() {
  [Buffer<i32>] b0{/std/gfx/Buffer/allocate<i32>(0i32)}
  [Buffer<i32>] b1{/std/gfx/Buffer/allocate<i32>(3i32)}
  [Buffer<i32>] b2{/std/gfx/Buffer/allocate<i32>(2i32)}
  return(score(location(b0), location(b1), location(b2)))
}
)";
  const std::string srcPath = writeTemp("vm_variadic_args_buffer_pointer_methods.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 5);
}

TEST_CASE("vm materializes variadic pointer uninitialized scalar packs with indexed init and take") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer<uninitialized<i32>>>] values) {
  init(dereference(values[0i32]), 2i32)
  init(dereference(values.at(1i32)), 3i32)
  init(dereference(values.at_unsafe(2i32)), 4i32)
  return(plus(take(dereference(values[0i32])),
              plus(take(dereference(values.at(1i32))),
                   take(dereference(values.at_unsafe(2i32))))))
}

[return<int>]
forward([args<Pointer<uninitialized<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<uninitialized<i32>>>] values) {
  [uninitialized<i32>] extra{uninitialized<i32>()}
  [Pointer<uninitialized<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [uninitialized<i32>] a0{uninitialized<i32>()}
  [uninitialized<i32>] a1{uninitialized<i32>()}
  [uninitialized<i32>] a2{uninitialized<i32>()}
  [Pointer<uninitialized<i32>>] p0{location(a0)}
  [Pointer<uninitialized<i32>>] p1{location(a1)}
  [Pointer<uninitialized<i32>>] p2{location(a2)}

  [uninitialized<i32>] b0{uninitialized<i32>()}
  [uninitialized<i32>] b1{uninitialized<i32>()}
  [uninitialized<i32>] b2{uninitialized<i32>()}
  [Pointer<uninitialized<i32>>] q0{location(b0)}
  [Pointer<uninitialized<i32>>] q1{location(b1)}
  [Pointer<uninitialized<i32>>] q2{location(b2)}

  [uninitialized<i32>] c0{uninitialized<i32>()}
  [uninitialized<i32>] c1{uninitialized<i32>()}
  [Pointer<uninitialized<i32>>] r0{location(c0)}
  [Pointer<uninitialized<i32>>] r1{location(c1)}

  return(plus(score_ptrs(p0, p1, p2),
              plus(forward(q0, q1, q2),
                   forward_mixed(r0, r1))))
}
)";
  const std::string srcPath = writeTemp("vm_variadic_args_pointer_uninitialized_scalar.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 27);
}

TEST_CASE("vm materializes variadic borrowed Result packs with indexed dereference try and why access") {
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
  const std::string srcPath = writeTemp("vm_variadic_args_borrowed_result.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 24);
}

TEST_CASE("vm materializes variadic pointer Result packs with indexed dereference try and why access") {
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
  [i32] head{try(dereference(values[0i32]))}
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
  const std::string srcPath = writeTemp("vm_variadic_args_pointer_result.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 24);
}


TEST_SUITE_END();
