#include "test_compile_run_helpers.h"

#include "test_compile_run_native_backend_core_helpers.h"

#if PRIMESTRUCT_NATIVE_CORE_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.core");

TEST_CASE("native backend supports graphics-style int return propagation with on_error") {
  const std::string source = R"(
[struct]
GfxError() {
  [i32] code{0i32}
}

namespace GfxError {
  [return<string>]
  why([GfxError] err) {
    return(if(equal(err.code, 7i32), then() { "frame_acquire_failed"utf8 }, else() { "queue_submit_failed"utf8 }))
  }
}

[return<Result<i32, GfxError>>]
acquire_frame_ok() {
  return(Result.ok(9i32))
}

[return<Result<i32, GfxError>>]
acquire_frame_fail() {
  return(7i64)
}

[return<Result<GfxError>>]
submit_frame([i32] token) {
  return(Result.ok())
}

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(err.why())
}

[return<int> on_error<GfxError, /log_gfx_error>]
main_ok() {
  [i32] frameToken{acquire_frame_ok()?}
  submit_frame(frameToken)?
  return(frameToken)
}

[return<int> effects(io_err) on_error<GfxError, /log_gfx_error>]
main_fail() {
  [i32] frameToken{acquire_frame_fail()?}
  return(frameToken)
}
)";
  const std::string srcPath = writeTemp("native_graphics_int_on_error.prime", source);
  const std::string okExePath =
      (testScratchPath("") / "primec_native_graphics_int_on_error_ok").string();
  const std::string failExePath =
      (testScratchPath("") / "primec_native_graphics_int_on_error_fail").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_graphics_int_on_error_err.txt").string();

  const std::string compileOkCmd =
      "./primec --emit=native " + srcPath + " -o " + okExePath + " --entry /main_ok";
  const std::string compileFailCmd =
      "./primec --emit=native " + srcPath + " -o " + failExePath + " --entry /main_fail";
  CHECK(runCommand(compileOkCmd) == 0);
  CHECK(runCommand(compileFailCmd) == 0);
  CHECK(runCommand(okExePath) == 9);
  CHECK(runCommand(failExePath + " 2> " + errPath) == 7);
  CHECK(readFile(errPath) == "frame_acquire_failed\n");
}

TEST_CASE("native backend supports string Result.ok payloads through try") {
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

[return<Result<string, ParseError>>]
greeting() {
  return(Result.ok("alpha"utf8))
}

[effects(io_err)]
log_parse_error([ParseError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<ParseError, /log_parse_error>]
main() {
  [string] text{greeting()?}
  print_line(text)
  return(count(text))
}
)";
  const std::string srcPath = writeTemp("native_result_ok_string.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_result_ok_string_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_result_ok_string_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 5);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("native backend supports direct string Result combinator consumers") {
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

[return<int> effects(io_out) on_error<ParseError, /swallow_parse_error>]
main() {
  print_line(try(Result.map(Result.ok("alpha"utf8), []([string] value) { return(value) })))
  print_line(try(Result.and_then(Result.ok("beta"utf8), []([string] value) { return(Result.ok(value)) })))
  print_line(try(Result.map2(Result.ok("gamma"utf8), Result.ok("delta"utf8), []([string] left, [string] right) {
    return(left)
  })))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("native_result_combinators_string.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_result_combinators_string_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_result_combinators_string_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\nbeta\ngamma\n");
}

TEST_CASE("native backend supports definition-backed string Result combinator sources") {
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

[return<int> effects(io_out) on_error<ParseError, /swallow_parse_error>]
main() {
  [Reader] reader{Reader{}}
  print_line(try(Result.map(greeting(), []([string] value) { return(value) })))
  print_line(try(Result.and_then(reader.read(), []([string] value) { return(Result.ok(value)) })))
  print_line(try(Result.map2(greeting(), reader.read(), []([string] left, [string] right) { return(left) })))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("native_result_combinators_string_definition_sources.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_result_combinators_string_definition_sources_exe")
          .string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_result_combinators_string_definition_sources_out.txt")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\nbeta\nalpha\n");
}

#if defined(EACCES)
TEST_CASE("compiles and runs native FileError.why mapping") {
  const std::string source =
      "[return<Result<FileError>>]\n"
      "make_error() {\n"
      "  return(" + std::to_string(EACCES) + "i32)\n"
      "}\n"
      "\n"
      "[return<int> effects(io_out)]\n"
      "main() {\n"
      "  print_line(Result.why(make_error()))\n"
      "  return(0i32)\n"
      "}\n";
  const std::string srcPath = writeTemp("compile_native_file_error_why.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_file_error_why_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_file_error_why_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "EACCES\n");
}
#endif

TEST_CASE("native uses stdlib FileError why wrapper") {
  const std::string source = R"(
import /std/file/*

[return<int> effects(io_out)]
main() {
  [FileError] err{fileReadEof()}
  if(not(fileErrorIsEof(err))) {
    return(1i32)
  }
  print_line(FileError.why(err))
  print_line(FileError.why(err))
  print_line(err.why())
  print_line(Result.why(FileError.status(err)))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_file_error_why.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_file_error_why_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_stdlib_file_error_why_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "EOF\nEOF\nEOF\nEOF\n");
}

TEST_CASE("native uses stdlib FileError eof wrapper") {
  const std::string source = R"(
import /std/file/*

[return<int>]
main() {
  [FileError] eofErr{fileReadEof()}
  [FileError] otherErr{1i32}
  if(not(FileError.is_eof(eofErr))) {
    return(1i32)
  }
  if(not(FileError.is_eof(eofErr))) {
    return(2i32)
  }
  if(FileError.is_eof(otherErr)) {
    return(3i32)
  }
  if(FileError.is_eof(otherErr)) {
    return(4i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_file_error_is_eof.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_file_error_is_eof_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("native uses stdlib FileError eof constructor wrapper") {
  const std::string source = R"(
import /std/file/*

[return<int> effects(io_out)]
main() {
  [FileError] err{FileError.eof()}
  print_line(FileError.why(err))
  print_line(FileError.why(err))
  if(not(FileError.is_eof(err))) {
    return(1i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_file_error_eof_wrapper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_file_error_eof_wrapper_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_stdlib_file_error_eof_wrapper_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "EOF\nEOF\n");
}

#if defined(EACCES) && defined(ENOENT) && defined(EEXIST)
TEST_CASE("native materializes variadic FileError packs with indexed why methods") {
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
  const std::string srcPath = writeTemp("compile_native_variadic_args_file_error.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_file_error").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 36);
}

TEST_CASE("native rejects variadic borrowed FileError packs with indexed dereference why methods") {
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
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_file_error.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_file_error").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_file_error.err").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Native lowering error: variadic parameter type mismatch") !=
        std::string::npos);
}

TEST_CASE("native rejects variadic pointer FileError packs with indexed dereference why methods") {
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
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_file_error.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_file_error").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_file_error.err").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Native lowering error: variadic parameter type mismatch") !=
        std::string::npos);
}

TEST_CASE("native rejects variadic wrapped FileError packs with named free builtin at receivers") {
  const std::string source =
      "[return<int>]\n"
      "score_refs([args<Reference<FileError>>] values) {\n"
      "  return(plus(count(at([values] values, [index] 0i32).why()),\n"
      "              count(at([values] values, [index] minus(count(values), 1i32)).why())))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_refs([args<Reference<FileError>>] values) {\n"
      "  return(score_refs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_refs_mixed([args<Reference<FileError>>] values) {\n"
      "  [FileError] extra{" + std::to_string(EACCES) + "i32}\n"
      "  [Reference<FileError>] extra_ref{location(extra)}\n"
      "  return(score_refs(extra_ref, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "score_ptrs([args<Pointer<FileError>>] values) {\n"
      "  return(plus(count(at([values] values, [index] 0i32).why()),\n"
      "              count(at([values] values, [index] minus(count(values), 1i32)).why())))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_ptrs([args<Pointer<FileError>>] values) {\n"
      "  return(score_ptrs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_ptrs_mixed([args<Pointer<FileError>>] values) {\n"
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
      "  [FileError] d0{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] d1{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] d2{" + std::to_string(EEXIST) + "i32}\n"
      "  [Pointer<FileError>] p0{location(d0)}\n"
      "  [Pointer<FileError>] p1{location(d1)}\n"
      "  [Pointer<FileError>] p2{location(d2)}\n"
      "  [FileError] e0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] e1{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] e2{" + std::to_string(EACCES) + "i32}\n"
      "  [Pointer<FileError>] q0{location(e0)}\n"
      "  [Pointer<FileError>] q1{location(e1)}\n"
      "  [Pointer<FileError>] q2{location(e2)}\n"
      "  [FileError] f0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] f1{" + std::to_string(EEXIST) + "i32}\n"
      "  [Pointer<FileError>] u0{location(f0)}\n"
      "  [Pointer<FileError>] u1{location(f1)}\n"
      "  return(plus(score_refs(r0, r1, r2),\n"
      "              plus(forward_refs(s0, s1, s2),\n"
      "                   plus(forward_refs_mixed(t0, t1),\n"
      "                        plus(score_ptrs(p0, p1, p2),\n"
      "                             plus(forward_ptrs(q0, q1, q2),\n"
      "                                  forward_ptrs_mixed(u0, u1)))))))\n"
      "}\n";
  const std::string srcPath = writeTemp("compile_native_variadic_named_args_wrapped_file_error.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_variadic_named_args_wrapped_file_error").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_variadic_named_args_wrapped_file_error.err").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Native lowering error: variadic parameter type mismatch") !=
        std::string::npos);
}

TEST_CASE("native materializes variadic File handle packs with indexed file methods") {
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
  const std::string pathA0 = (testScratchPath("") / "primec_native_variadic_file_handle_a0.txt").string();
  const std::string pathA1 = (testScratchPath("") / "primec_native_variadic_file_handle_a1.txt").string();
  const std::string pathA2 = (testScratchPath("") / "primec_native_variadic_file_handle_a2.txt").string();
  const std::string pathB0 = (testScratchPath("") / "primec_native_variadic_file_handle_b0.txt").string();
  const std::string pathB1 = (testScratchPath("") / "primec_native_variadic_file_handle_b1.txt").string();
  const std::string pathB2 = (testScratchPath("") / "primec_native_variadic_file_handle_b2.txt").string();
  const std::string pathC0 = (testScratchPath("") / "primec_native_variadic_file_handle_c0.txt").string();
  const std::string pathC1 = (testScratchPath("") / "primec_native_variadic_file_handle_c1.txt").string();
  const std::string pathExtra =
      (testScratchPath("") / "primec_native_variadic_file_handle_extra.txt").string();

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
  const std::string srcPath = writeTemp("compile_native_variadic_args_file_handle.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_file_handle").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 39);
  CHECK(readFile(pathA0) == "alpha\n");
  CHECK(readFile(pathA2) == "omega\n");
  CHECK(readFile(pathB0) == "alpha\n");
  CHECK(readFile(pathB2) == "omega\n");
  CHECK(readFile(pathExtra) == "alpha\n");
  CHECK(readFile(pathC1) == "omega\n");
}

TEST_CASE("native materializes variadic borrowed File handle packs with indexed dereference file methods") {
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
  const std::string pathA0 =
      (testScratchPath("") / "primec_native_variadic_borrowed_file_handle_a0.txt").string();
  const std::string pathA1 =
      (testScratchPath("") / "primec_native_variadic_borrowed_file_handle_a1.txt").string();
  const std::string pathA2 =
      (testScratchPath("") / "primec_native_variadic_borrowed_file_handle_a2.txt").string();
  const std::string pathB0 =
      (testScratchPath("") / "primec_native_variadic_borrowed_file_handle_b0.txt").string();
  const std::string pathB1 =
      (testScratchPath("") / "primec_native_variadic_borrowed_file_handle_b1.txt").string();
  const std::string pathB2 =
      (testScratchPath("") / "primec_native_variadic_borrowed_file_handle_b2.txt").string();
  const std::string pathC0 =
      (testScratchPath("") / "primec_native_variadic_borrowed_file_handle_c0.txt").string();
  const std::string pathC1 =
      (testScratchPath("") / "primec_native_variadic_borrowed_file_handle_c1.txt").string();
  const std::string pathExtra =
      (testScratchPath("") / "primec_native_variadic_borrowed_file_handle_extra.txt").string();

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
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_file_handle.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_file_handle").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 39);
  CHECK(readFile(pathA0) == "alpha\n");
  CHECK(readFile(pathA2) == "omega\n");
  CHECK(readFile(pathB0) == "alpha\n");
  CHECK(readFile(pathB2) == "omega\n");
  CHECK(readFile(pathExtra) == "alpha\n");
  CHECK(readFile(pathC1) == "omega\n");
}

TEST_CASE("native materializes variadic pointer File handle packs with indexed dereference file methods") {
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
  const std::string pathA0 =
      (testScratchPath("") / "primec_native_variadic_pointer_file_handle_a0.txt").string();
  const std::string pathA1 =
      (testScratchPath("") / "primec_native_variadic_pointer_file_handle_a1.txt").string();
  const std::string pathA2 =
      (testScratchPath("") / "primec_native_variadic_pointer_file_handle_a2.txt").string();
  const std::string pathB0 =
      (testScratchPath("") / "primec_native_variadic_pointer_file_handle_b0.txt").string();
  const std::string pathB1 =
      (testScratchPath("") / "primec_native_variadic_pointer_file_handle_b1.txt").string();
  const std::string pathB2 =
      (testScratchPath("") / "primec_native_variadic_pointer_file_handle_b2.txt").string();
  const std::string pathC0 =
      (testScratchPath("") / "primec_native_variadic_pointer_file_handle_c0.txt").string();
  const std::string pathC1 =
      (testScratchPath("") / "primec_native_variadic_pointer_file_handle_c1.txt").string();
  const std::string pathExtra =
      (testScratchPath("") / "primec_native_variadic_pointer_file_handle_extra.txt").string();

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
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_file_handle.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_file_handle").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 39);
  CHECK(readFile(pathA0) == "alpha\n");
  CHECK(readFile(pathA2) == "omega\n");
  CHECK(readFile(pathB0) == "alpha\n");
  CHECK(readFile(pathB2) == "omega\n");
  CHECK(readFile(pathExtra) == "alpha\n");
  CHECK(readFile(pathC1) == "omega\n");
}

#endif

TEST_SUITE_END();
#endif
