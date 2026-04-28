#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("compiles and runs repeat loop") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{0i32}
  repeat(3i32) {
    assign(value, plus(value, 2i32))
  }
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_repeat_loop.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_repeat_loop_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs loop while for sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  loop(2i32) {
    assign(total, plus(total, 1i32))
  }
  while(less_than(i 3i32)) {
    assign(total, plus(total, i))
    assign(i, plus(i, 1i32))
  }
  for([i32 mut] j{0i32} less_than(j 2i32) assign(j, plus(j, 1i32))) {
    assign(total, plus(total, j))
  }
  return(total)
}
)";
  const std::string srcPath = writeTemp("compile_loop_while_for.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_loop_while_for_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs for binding condition") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  for([i32 mut] i{0i32} [bool] keep{less_than(i, 3i32)} assign(i, plus(i, 1i32))) {
    if(keep, then(){ assign(total, plus(total, 2i32)) }, else(){})
  }
  return(total)
}
)";
  const std::string srcPath = writeTemp("compile_for_condition_binding.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_for_condition_binding_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs shared_scope loops") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] result{0i32}
  [shared_scope]
  loop(4i32) {
    [i32 mut] total{0i32}
    assign(total, plus(total, 1i32))
    assign(result, total)
  }
  [shared_scope]
  for([i32 mut] i{1i32} less_than(i 10i32) assign(i, multiply(i, 2i32))) {
    [i32 mut] total{i}
    assign(total, plus(total, 1i32))
    assign(result, total)
  }
  return(result)
}
)";
  const std::string srcPath = writeTemp("compile_shared_scope.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_shared_scope_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs shared_scope for binding condition") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  [shared_scope]
  for([i32 mut] i{0i32} [bool] keep{less_than(i, 3i32)} assign(i, plus(i, 1i32))) {
    [i32 mut] acc{0i32}
    if(keep, then(){ assign(acc, plus(acc, 1i32)) }, else(){})
    assign(total, plus(total, acc))
  }
  return(total)
}
)";
  const std::string srcPath = writeTemp("compile_shared_scope_for_cond.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_shared_scope_for_cond_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs shared_scope while loop") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  [shared_scope]
  while(less_than(i, 3i32)) {
    [i32 mut] acc{0i32}
    assign(acc, plus(acc, 1i32))
    assign(total, plus(total, acc))
    assign(i, plus(i, 1i32))
  }
  return(total)
}
)";
  const std::string srcPath = writeTemp("compile_shared_scope_while.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_shared_scope_while_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs increment decrement sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  value++
  --value
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_increment_decrement.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_increment_decrement_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs brace constructor value") {
  const std::string source = R"(
[return<int>]
pick([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(pick{ 3i32 })
}
)";
  const std::string srcPath = writeTemp("compile_brace_constructor_value.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_brace_constructor_value_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs nested definition call") {
  const std::string source = R"(
[return<int>]
main() {
  [return<int>]
  helper() {
    return(5i32)
  }
  return(/main/helper())
}
)";
  const std::string srcPath = writeTemp("compile_nested_definition_call.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_nested_definition_call_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs paired map literal") {
  const std::string source = R"(
[return<int>]
main() {
  return(at(map<i32, i32>{1i32=2i32, 3i32=4i32}, 3i32))
}
)";
  const std::string srcPath = writeTemp("compile_map_literal.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_map_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("C++ emitter runs variadic args body access and indexed method calls") {
  const std::string source = R"(
[return<int>]
packScore([args<string>] values) {
  return(plus(plus(values[0i32].count(), values.at(1i32).count()),
              values.at_unsafe(2i32).count()))
}

[return<int>]
main() {
  return(packScore("ab"utf8, "cde"utf8, "fghi"utf8))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_body_api.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_variadic_args_body_api_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("C++ emitter forwards variadic args packs with explicit prefix values") {
  const std::string source = R"(
[return<int>]
packScore([args<i32>] values) {
  return(plus(count(values), plus(plus(values[0i32], values.at(1i32)), values.at_unsafe(2i32))))
}

[return<int>]
forward([args<i32>] values) {
  return(plus(values.count(), packScore(4i32, values...)))
}

[return<int>]
main() {
  return(forward(5i32, 6i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_forward.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_variadic_args_forward_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 20);
}

TEST_CASE("C++ emitter materializes variadic Result value packs with spread forwarding") {
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
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_result.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_variadic_args_result_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 24);
}

TEST_CASE("C++ emitter materializes variadic status-only Result value packs with spread forwarding") {
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
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_status_result.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_variadic_args_status_result_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 39);
}

TEST_CASE("C++ emitter materializes variadic borrowed Result value packs with indexed dereference helpers") {
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
  [Result<i32, ParseError> mut] extra{ok_value(10i32)}
  [Reference<Result<i32, ParseError>>] extra_ref{location(extra)}
  return(score_results(extra_ref, [spread] values))
}

[return<int>]
main() {
  [Result<i32, ParseError> mut] a0{ok_value(2i32)}
  [Result<i32, ParseError> mut] a1{fail_bad()}
  [Reference<Result<i32, ParseError>>] r0{location(a0)}
  [Reference<Result<i32, ParseError>>] r1{location(a1)}

  [Result<i32, ParseError> mut] b0{ok_value(3i32)}
  [Result<i32, ParseError> mut] b1{fail_bad()}
  [Reference<Result<i32, ParseError>>] s0{location(b0)}
  [Reference<Result<i32, ParseError>>] s1{location(b1)}

  [Result<i32, ParseError> mut] c0{ok_value(4i32)}
  [Result<i32, ParseError> mut] c1{fail_bad()}
  [Reference<Result<i32, ParseError>>] t0{location(c0)}
  [Reference<Result<i32, ParseError>>] t1{location(c1)}

  return(plus(score_results(r0, r1),
              plus(forward(s0, s1),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_borrowed_args_result.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_variadic_borrowed_args_result_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 24);
}

TEST_CASE("C++ emitter materializes variadic borrowed status-only Result packs with indexed dereference helpers") {
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
  [Result<ParseError> mut] extra{ok_status()}
  [Pointer<Result<ParseError>>] extra_ptr{location(extra)}
  return(score_results(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [Result<ParseError> mut] a0{ok_status()}
  [Result<ParseError> mut] a1{fail_bad()}
  [Pointer<Result<ParseError>>] r0{location(a0)}
  [Pointer<Result<ParseError>>] r1{location(a1)}

  [Result<ParseError> mut] b0{ok_status()}
  [Result<ParseError> mut] b1{fail_bad()}
  [Pointer<Result<ParseError>>] s0{location(b0)}
  [Pointer<Result<ParseError>>] s1{location(b1)}

  [Result<ParseError> mut] c0{ok_status()}
  [Result<ParseError> mut] c1{fail_bad()}
  [Pointer<Result<ParseError>>] t0{location(c0)}
  [Pointer<Result<ParseError>>] t1{location(c1)}

  return(plus(score_results(r0, r1),
              plus(forward(s0, s1),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_borrowed_status_result.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_variadic_borrowed_status_result_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 39);
}

TEST_CASE("C++ emitter materializes variadic FileError value packs with indexed why methods") {
#if defined(EACCES) && defined(ENOENT) && defined(EEXIST)
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
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_file_error.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_variadic_args_file_error_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 36);
#else
  DOCTEST_INFO("FileError errno constants unavailable on this platform");
  CHECK(true);
#endif
}

TEST_CASE("C++ emitter materializes variadic map value packs with indexed count methods") {
  const std::string source = R"(
import /std/collections/*

[return<int> effects(heap_alloc)]
score_maps([args<map<i32, i32>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int> effects(heap_alloc)]
forward([args<map<i32, i32>>] values) {
  return(score_maps([spread] values))
}

[return<int> effects(heap_alloc)]
forward_mixed([args<map<i32, i32>>] values) {
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
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_map_count.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_variadic_args_map_count_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("C++ emitter materializes variadic Buffer value packs with indexed count helpers") {
  const std::string source = R"(
import /std/gfx/*

[return<int> effects(gpu_dispatch)]
score_buffers([args<Buffer<i32>>] values) {
  [Buffer<i32>] head{at(values, 0i32)}
  [Buffer<i32>] tail{at(values, minus(count(values), 1i32))}
  return(plus(head.count(), plus(tail.count(), count(values))))
}

[return<int> effects(gpu_dispatch)]
forward([args<Buffer<i32>>] values) {
  return(score_buffers([spread] values))
}

[return<int> effects(gpu_dispatch)]
forward_mixed([args<Buffer<i32>>] values) {
  return(score_buffers(Buffer<i32>(1i32), [spread] values))
}

[return<int> effects(gpu_dispatch)]
main() {
  return(plus(score_buffers(Buffer<i32>(3i32), Buffer<i32>(1i32), Buffer<i32>(2i32)),
              plus(forward(Buffer<i32>(4i32), Buffer<i32>(1i32), Buffer<i32>(5i32)),
                   forward_mixed(Buffer<i32>(6i32), Buffer<i32>(2i32)))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_buffer_count.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_variadic_args_buffer_count_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 26);
}

TEST_CASE("C++ emitter materializes variadic File handle packs with indexed file methods") {
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
  const std::string pathA0 = (testScratchPath("") / "primec_cpp_variadic_file_handle_a0.txt").string();
  const std::string pathA1 = (testScratchPath("") / "primec_cpp_variadic_file_handle_a1.txt").string();
  const std::string pathA2 = (testScratchPath("") / "primec_cpp_variadic_file_handle_a2.txt").string();
  const std::string pathB0 = (testScratchPath("") / "primec_cpp_variadic_file_handle_b0.txt").string();
  const std::string pathB1 = (testScratchPath("") / "primec_cpp_variadic_file_handle_b1.txt").string();
  const std::string pathB2 = (testScratchPath("") / "primec_cpp_variadic_file_handle_b2.txt").string();
  const std::string pathC0 = (testScratchPath("") / "primec_cpp_variadic_file_handle_c0.txt").string();
  const std::string pathC1 = (testScratchPath("") / "primec_cpp_variadic_file_handle_c1.txt").string();
  const std::string pathExtra =
      (testScratchPath("") / "primec_cpp_variadic_file_handle_extra.txt").string();

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
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_file_handle.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_variadic_args_file_handle_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 39);
  CHECK(readFile(pathA0) == "alpha\n");
  CHECK(readFile(pathA2) == "omega\n");
  CHECK(readFile(pathB0) == "alpha\n");
  CHECK(readFile(pathB2) == "omega\n");
  CHECK(readFile(pathExtra) == "alpha\n");
  CHECK(readFile(pathC1) == "omega\n");
}

TEST_CASE("C++ emitter materializes variadic borrowed FileError packs with indexed dereference why methods") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<FileError>>] values) {
  return(plus(count(dereference(values[0i32]).why()), count(dereference(values[2i32]).why())))
}

[return<int>]
forward([args<Reference<FileError>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<FileError>>] values) {
  [FileError] extra{13i32}
  [Reference<FileError>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [FileError] a0{13i32}
  [FileError] a1{2i32}
  [FileError] a2{17i32}
  [Reference<FileError>] r0{location(a0)}
  [Reference<FileError>] r1{location(a1)}
  [Reference<FileError>] r2{location(a2)}
  [FileError] b0{2i32}
  [FileError] b1{17i32}
  [FileError] b2{13i32}
  [Reference<FileError>] s0{location(b0)}
  [Reference<FileError>] s1{location(b1)}
  [Reference<FileError>] s2{location(b2)}
  [FileError] c0{2i32}
  [FileError] c1{17i32}
  [Reference<FileError>] t0{location(c0)}
  [Reference<FileError>] t1{location(c1)}
  return(plus(score_refs(r0, r1, r2), plus(forward(s0, s1, s2), forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_borrowed_file_error.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_variadic_args_borrowed_file_error_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 36);
}

TEST_SUITE_END();
