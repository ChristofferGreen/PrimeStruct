#include "test_compile_run_helpers.h"

#include "test_compile_run_native_backend_core_helpers.h"

#if PRIMESTRUCT_NATIVE_CORE_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.core");

TEST_CASE("compiles and rejects native png inputs with critical chunk crc mismatches") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_invalid_crc.png").string();
  {
    const std::vector<unsigned char> pngBytes = withCorruptedFirstPngChunkCrc({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0c, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x01, 0x01, 0x04, 0x00, 0xfb, 0xff,
        0x00, 0x11, 0x22, 0x33, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44,
        0x00, 0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{7i32}
  [i32 mut] height{9i32}
  [vector<i32> mut] pixels{vector<i32>(1i32, 2i32, 3i32)}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  print_line(Result.why(status))
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  return(0i32)
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_invalid_crc_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_invalid_crc_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_invalid_crc_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("compiles and rejects native png inputs with plte after idat") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_invalid_plte_order.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x08, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x04, 0x00, 0xfb, 0xff, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x03, 0x50, 0x4c, 0x54, 0x45,
        0xaa, 0xbb, 0xcc, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x07, 0x49, 0x44, 0x41, 0x54,
        0xff, 0x00, 0x00, 0x03, 0x01, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44,
        0x00, 0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{7i32}
  [i32 mut] height{9i32}
  [vector<i32> mut] pixels{vector<i32>(1i32, 2i32, 3i32)}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  print_line(Result.why(status))
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  return(0i32)
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_invalid_plte_order_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_invalid_plte_order_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_invalid_plte_order_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("compiles and runs if expression in native backend") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(false, then(){ 4i32 }, else(){ 9i32 }))
}
)";
  const std::string srcPath = writeTemp("compile_native_if_expr.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_if_expr_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs match cases in native backend") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{2i32}
  return(match(value, case(1i32) { 10i32 }, case(2i32) { 20i32 }, else() { 30i32 }))
}
)";
  const std::string srcPath = writeTemp("compile_native_match_cases.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_match_cases_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 20);
}

TEST_CASE("compiles and runs native definition call") {
  const std::string source = R"(
[return<int>]
inc([i32] x) {
  return(plus(x, 1i32))
}

[return<int>]
main() {
  return(inc(6i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_def_call.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_def_call_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native method call") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
main() {
  [i32] value{5i32}
  return(value.inc())
}
)";
  const std::string srcPath = writeTemp("compile_native_method_call.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_method_call_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs native method count call") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  count([i32] value) {
    return(plus(value, 2i32))
  }
}

[return<int>]
main() {
  [i32] value{3i32}
  return(value.count())
}
)";
  const std::string srcPath = writeTemp("compile_native_method_count.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_method_count_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs native literal method call") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
main() {
  return(1i32.inc())
}
)";
  const std::string srcPath = writeTemp("compile_native_method_literal.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_method_literal_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native chained method calls") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }

  [return<int>]
  dec([i32] value) {
    return(minus(value, 1i32))
  }
}

[return<int>]
make() {
  return(4i32)
}

[return<int>]
main() {
  return(make().inc().dec())
}
)";
  const std::string srcPath = writeTemp("compile_native_method_chain.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_method_chain_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs native Result.why hooks") {
  const std::string source = R"(
[struct]
MyError() {
  [i32] code{0i32}
}

namespace MyError {
  [return<string>]
  why([MyError] err) {
    return("custom error"utf8)
  }
}

[return<Result<MyError>>]
make_error() {
  return(1i32)
}

[return<int> effects(io_out)]
main() {
  print_line(Result.why(make_error()))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_result_why_custom.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_result_why_custom_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_result_why_custom_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "custom error\n");
}

TEST_CASE("native supports stdlib FileError result helpers") {
  const std::string source = R"(
import /std/file/*

[return<Result<FileError>>]
make_status() {
  return(FileError.status(fileReadEof()))
}

[return<Result<i32, FileError>>]
make_value() {
  return(FileError.result<i32>(fileReadEof()))
}

[return<int> effects(io_out)]
main() {
  [Result<FileError>] status{make_status()}
  [Result<i32, FileError>] valueStatus{make_value()}
  [FileError] err{fileReadEof()}
  [Result<FileError>] methodStatus{FileError.status(err)}
  [Result<i32, FileError>] methodValueStatus{FileError.result<i32>(err)}
  [bool] eof{fileErrorIsEof(fileReadEof())}
  [bool] otherEof{fileErrorIsEof(1i32)}
  [bool] directStatusError{Result.error(status)}
  [bool] methodStatusError{Result.error(methodStatus)}
  if(not(Result.error(status))) {
    return(1i32)
  }
  if(not(Result.error(valueStatus))) {
    return(2i32)
  }
  if(not(eof)) {
    return(3i32)
  }
  if(otherEof) {
    return(4i32)
  }
  if(not(directStatusError)) {
    return(5i32)
  }
  if(not(methodStatusError)) {
    return(6i32)
  }
  print_line(Result.why(status))
  print_line(Result.why(valueStatus))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  print_line(Result.why(FileError.status(fileReadEof())))
  print_line(Result.why(FileError.result<i32>(fileReadEof())))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_file_error_result_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_file_error_result_helpers").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_stdlib_file_error_result_helpers_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "EOF\nEOF\nEOF\nEOF\nEOF\nEOF\nEOF\nEOF\n");
}

TEST_CASE("native backend supports Result.map on IR-backed path") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<i32, FileError>] failed{FileError.result<i32>(fileReadEof())}
  [Result<i32, FileError>] mappedOk{
    Result.map(ok, []([i32] value) { return(multiply(value, 4i32)) })
  }
  [Result<i32, FileError>] mappedFailed{
    Result.map(failed, []([i32] value) { return(multiply(value, 4i32)) })
  }
  if(Result.error(mappedOk)) {
    return(1i32)
  }
  if(not(Result.error(mappedFailed))) {
    return(2i32)
  }
  print_line(Result.why(mappedFailed))
  return(try(mappedOk))
}
)";
  const std::string srcPath = writeTemp("compile_native_result_map_ir_backed.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_result_map_ir_backed").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_result_map_ir_backed_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 8);
  CHECK(readFile(outPath) == "EOF\n");
}

TEST_CASE("native backend supports Result.and_then on IR-backed path") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<i32, FileError>] failed{FileError.result<i32>(fileReadEof())}
  [Result<i32, FileError>] chainedOk{
    Result.and_then(ok, []([i32] value) { return(Result.ok(multiply(value, 4i32))) })
  }
  [Result<FileError>] chainedStatus{
    Result.and_then(ok, []([i32] value) { return(FileError.status(FileError.eof())) })
  }
  [Result<i32, FileError>] chainedFailed{
    Result.and_then(failed, []([i32] value) { return(Result.ok(multiply(value, 4i32))) })
  }
  if(Result.error(chainedOk)) {
    return(1i32)
  }
  if(not(Result.error(chainedStatus))) {
    return(2i32)
  }
  if(not(Result.error(chainedFailed))) {
    return(3i32)
  }
  print_line(Result.why(chainedStatus))
  print_line(Result.why(chainedFailed))
  return(try(chainedOk))
}
)";
  const std::string srcPath = writeTemp("compile_native_result_and_then_ir_backed.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_result_and_then_ir_backed").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_result_and_then_ir_backed_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 8);
  CHECK(readFile(outPath) == "EOF\nEOF\n");
}

TEST_CASE("native backend supports Result.map2 on IR-backed path") {
  const std::string source = R"(
import /std/collections/*

swallow_container_error([ContainerError] err) {}

[return<int> effects(io_out) on_error<ContainerError, /swallow_container_error>]
main() {
  [Result<i32, ContainerError>] first{Result.ok(2i32)}
  [Result<i32, ContainerError>] second{Result.ok(3i32)}
  [Result<i32, ContainerError>] leftFailed{/ContainerError/result<i32>(/ContainerError/missing_key())}
  [Result<i32, ContainerError>] rightFailed{/ContainerError/result<i32>(/ContainerError/empty())}
  [Result<i32, ContainerError>] summed{
    Result.map2(first, second, []([i32] left, [i32] right) { return(plus(left, right)) })
  }
  [Result<i32, ContainerError>] firstError{
    Result.map2(leftFailed, rightFailed, []([i32] left, [i32] right) { return(plus(left, right)) })
  }
  [Result<i32, ContainerError>] secondError{
    Result.map2(first, rightFailed, []([i32] left, [i32] right) { return(plus(left, right)) })
  }
  if(Result.error(summed)) {
    return(1i32)
  }
  if(not(Result.error(firstError))) {
    return(2i32)
  }
  if(not(Result.error(secondError))) {
    return(3i32)
  }
  print_line(Result.why(firstError))
  print_line(Result.why(secondError))
  return(try(summed))
}
)";
  const std::string srcPath = writeTemp("compile_native_result_map2_ir_backed.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_result_map2_ir_backed").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_result_map2_ir_backed_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 5);
  CHECK(readFile(outPath) == "container missing key\ncontainer empty\n");
}

TEST_CASE("native backend supports f32 Result payloads on IR-backed paths") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<f32, FileError>] ok{Result.ok(1.5f32)}
  [Result<f32, FileError>] mapped{
    Result.map(ok, []([f32] value) { return(plus(value, 1.0f32)) })
  }
  [Result<f32, FileError>] chained{
    Result.and_then(mapped, []([f32] value) { return(Result.ok(multiply(value, 2.0f32))) })
  }
  [Result<f32, FileError>] summed{
    Result.map2(chained, Result.ok(0.5f32), []([f32] left, [f32] right) { return(plus(left, right)) })
  }
  [f32] value{summed?}
  print_line(convert<int>(multiply(value, 10.0f32)))
  return(convert<int>(multiply(value, 10.0f32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_result_f32_ir_backed.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_result_f32_ir_backed").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_result_f32_ir_backed_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 55);
  CHECK(readFile(outPath) == "55\n");
}

TEST_CASE("native backend supports direct Result.ok combinator sources on IR-backed paths") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] mapped{
    Result.map(Result.ok(2i32), []([i32] value) { return(multiply(value, 4i32)) })
  }
  [Result<i32, FileError>] chained{
    Result.and_then(Result.ok(2i32), []([i32] value) { return(Result.ok(plus(value, 3i32))) })
  }
  [Result<i32, FileError>] summed{
    Result.map2(Result.ok(2i32), Result.ok(3i32), []([i32] left, [i32] right) { return(plus(left, right)) })
  }
  print_line(try(mapped))
  print_line(try(chained))
  return(try(summed))
}
)";
  const std::string srcPath = writeTemp("compile_native_result_direct_ok_ir_backed.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_result_direct_ok_ir_backed").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_result_direct_ok_ir_backed_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 5);
  CHECK(readFile(outPath) == "8\n5\n");
}

TEST_CASE("native backend compiles direct packed ContainerError and ImageError Result payloads on IR-backed paths") {
  const std::string source = R"(
import /std/file/*
import /std/collections/*
import /std/image/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [ContainerError] container{try(Result.ok(ContainerError{4i32}))}
  [ImageError] image{try(Result.ok(ImageError{3i32}))}
  return(plus(container.code, image.code))
}
)";
  const std::string srcPath = writeTemp("compile_native_result_packed_error_payloads_ir_backed.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_result_packed_error_payloads_ir_backed").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_result_packed_error_payloads_ir_backed_out.txt")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 7);
  CHECK(readFile(outPath).empty());
}


TEST_SUITE_END();
#endif
