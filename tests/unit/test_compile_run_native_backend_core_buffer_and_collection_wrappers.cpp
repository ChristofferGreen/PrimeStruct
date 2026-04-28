#include "test_compile_run_helpers.h"

#include "test_compile_run_native_backend_core_helpers.h"

#if PRIMESTRUCT_NATIVE_CORE_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.core");

TEST_CASE("native materializes variadic Buffer packs with indexed count helpers") {
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
  const std::string srcPath = writeTemp("compile_native_variadic_args_buffer_count.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_buffer_count").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 26);
}

TEST_CASE("native forwards variadic Reference<Buffer> packs through location/dereference") {
  const std::string source = R"(
import /std/gfx/*

[return<int> effects(gpu_dispatch)]
score_buffers_reference([args<Reference<Buffer<i32>>>] values) {
  [Buffer<i32>] headValue{dereference(values[0i32])}
  [Buffer<i32>] tailValue{dereference(values[minus(count(values), 1i32)])}
  return(plus(headValue.count(), plus(tailValue.count(), count(values))))
}

[return<int> effects(gpu_dispatch)]
forward_reference([args<Reference<Buffer<i32>>>] values) {
  return(score_buffers_reference([spread] values))
}

[return<int> effects(gpu_dispatch)]
forward_reference_mixed([args<Reference<Buffer<i32>>>] values) {
  [Buffer<i32>] extra{Buffer<i32>(10i32)}
  return(score_buffers_reference(location(extra), [spread] values))
}

[return<int> effects(gpu_dispatch)]
main() {
  [Buffer<i32>] d0Value{Buffer<i32>(3i32)}
  [Buffer<i32>] d1Value{Buffer<i32>(1i32)}
  [Buffer<i32>] d2Value{Buffer<i32>(2i32)}
  [Buffer<i32>] f0Value{Buffer<i32>(4i32)}
  [Buffer<i32>] f1Value{Buffer<i32>(1i32)}
  [Buffer<i32>] f2Value{Buffer<i32>(5i32)}
  [Buffer<i32>] m0Value{Buffer<i32>(6i32)}
  [Buffer<i32>] m1Value{Buffer<i32>(2i32)}
  [Reference<Buffer<i32>>] d0{location(d0Value)}
  [Reference<Buffer<i32>>] d1{location(d1Value)}
  [Reference<Buffer<i32>>] d2{location(d2Value)}
  [Reference<Buffer<i32>>] f0{location(f0Value)}
  [Reference<Buffer<i32>>] f1{location(f1Value)}
  [Reference<Buffer<i32>>] f2{location(f2Value)}
  [Reference<Buffer<i32>>] m0{location(m0Value)}
  [Reference<Buffer<i32>>] m1{location(m1Value)}
  [int] direct{score_buffers_reference(d0, d1, d2)}
  [int] forwarded{forward_reference(f0, f1, f2)}
  [int] mixed{forward_reference_mixed(m0, m1)}
  return(plus(direct, plus(forwarded, mixed)))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_buffer_reference.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_buffer_reference").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 35);
}

TEST_CASE("native materializes variadic Pointer<Buffer> packs with dereference helpers") {
  const std::string source = R"(
import /std/gfx/*

[return<int> effects(gpu_dispatch)]
score_buffers_pointer([args<Pointer<Buffer<i32>>>] values) {
  [Pointer<Buffer<i32>>] head{at(values, 0i32)}
  [Pointer<Buffer<i32>>] tail{at(values, minus(count(values), 1i32))}
  [Buffer<i32>] headValue{dereference(head)}
  [Buffer<i32>] tailValue{dereference(tail)}
  return(plus(headValue.count(), plus(tailValue.count(), count(values))))
}

[return<int> effects(gpu_dispatch)]
forward_pointer([args<Pointer<Buffer<i32>>>] values) {
  return(score_buffers_pointer([spread] values))
}

[return<int> effects(gpu_dispatch)]
forward_pointer_mixed([args<Pointer<Buffer<i32>>>] values) {
  [Buffer<i32>] extra{Buffer<i32>(10i32)}
  return(score_buffers_pointer(location(extra), [spread] values))
}

[return<int> effects(gpu_dispatch)]
main() {
  [Buffer<i32>] d0Value{Buffer<i32>(3i32)}
  [Buffer<i32>] d1Value{Buffer<i32>(1i32)}
  [Buffer<i32>] d2Value{Buffer<i32>(2i32)}
  [Buffer<i32>] f0Value{Buffer<i32>(4i32)}
  [Buffer<i32>] f1Value{Buffer<i32>(1i32)}
  [Buffer<i32>] f2Value{Buffer<i32>(5i32)}
  [Buffer<i32>] m0Value{Buffer<i32>(6i32)}
  [Buffer<i32>] m1Value{Buffer<i32>(2i32)}
  [Pointer<Buffer<i32>>] d0{location(d0Value)}
  [Pointer<Buffer<i32>>] d1{location(d1Value)}
  [Pointer<Buffer<i32>>] d2{location(d2Value)}
  [Pointer<Buffer<i32>>] f0{location(f0Value)}
  [Pointer<Buffer<i32>>] f1{location(f1Value)}
  [Pointer<Buffer<i32>>] f2{location(f2Value)}
  [Pointer<Buffer<i32>>] m0{location(m0Value)}
  [Pointer<Buffer<i32>>] m1{location(m1Value)}
  [int] direct{score_buffers_pointer(d0, d1, d2)}
  [int] forwarded{forward_pointer(f0, f1, f2)}
  [int] mixed{forward_pointer_mixed(m0, m1)}
  return(plus(direct, plus(forwarded, mixed)))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_buffer_pointer.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_buffer_pointer").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 35);
}

TEST_CASE("native preserves if expression values in arithmetic context") {
  const std::string source = R"(
[return<int>]
main() {
  [bool] on{true}
  [bool] off{false}
  [int] whenOn{plus(0i32, if(on, then() { 1i32 }, else() { 0i32 }))}
  [int] whenOff{plus(0i32, if(off, then() { 1i32 }, else() { 0i32 }))}
  return(plus(whenOn, whenOff))
}
)";
  const std::string srcPath = writeTemp("compile_native_if_expr_stack_merge.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_if_expr_stack_merge").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native float arithmetic") {
  const std::string source = R"(
[return<int>]
main() {
  [f32] a{1.5f32}
  [f32] b{2.0f32}
  [f32] c{multiply(plus(a, b), 2.0f32)}
  return(convert<int>(c))
}
)";
  const std::string srcPath = writeTemp("compile_native_float.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_float_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native image api contract deterministically") {
  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  print_line(Result.why(/std/image/ppm/read(width, height, pixels, "input.ppm"utf8)))
  print_line(Result.why(/std/image/ppm/write("output.ppm"utf8, width, height, pixels)))
  print_line(Result.why(/std/image/png/read(width, height, pixels, "input.png"utf8)))
  print_line(Result.why(/std/image/png/write("output.png"utf8, width, height, pixels)))
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_native_image_api_unsupported.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_api_unsupported").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_api_unsupported.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "image_invalid_operation\n"
        "image_invalid_operation\n"
        "image_invalid_operation\n");
}

TEST_CASE("native keeps std image user helpers distinct from builtin aliases") {
  const std::string source = R"(
import /std/collections/*

namespace std {
namespace image {
  [effects(heap_alloc), return<int>]
  work() {
    [mut] values{vector<i32>()}
    /std/collections/vector/push(values, 1i32)
    values[0i32] = 9i32
    return(values[0i32])
  }
}
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/image/work())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_std_image_user_helper_vector.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_std_image_user_helper_vector").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath +
      " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("native uses stdlib ImageError result helpers") {
  const std::string source = R"(
import /std/image/*

[return<int> effects(io_out)]
main() {
  print_line(Result.why(/ImageError/status(imageReadUnsupported())))
  print_line(Result.why(/ImageError/result<i32>(imageWriteUnsupported())))
  print_line(Result.why(/ImageError/result<i32>(imageInvalidOperation())))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_image_error_result_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_error_result_helpers").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_error_result_helpers.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_read_unsupported\n"
        "image_write_unsupported\n"
        "image_invalid_operation\n");
}

TEST_CASE("native uses stdlib ImageError why wrapper") {
  const std::string source = R"(
import /std/image/*

[return<int> effects(io_out)]
main() {
  [ImageError] err{imageReadUnsupported()}
  [Result<ImageError>] methodStatus{/ImageError/status(err)}
  [Result<i32, ImageError>] methodValueStatus{/ImageError/result<i32>(err)}
  print_line(/ImageError/why(err))
  print_line(/ImageError/why(err))
  print_line(/ImageError/why(err))
  print_line(Result.why(imageErrorStatus(err)))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_image_error_why_wrapper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_error_why_wrapper").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_error_why_wrapper.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_read_unsupported\n"
        "image_read_unsupported\n"
        "image_read_unsupported\n"
        "image_read_unsupported\n"
        "image_read_unsupported\n"
        "image_read_unsupported\n"
        "image_read_unsupported\n"
        "image_read_unsupported\n");
}

TEST_CASE("native uses stdlib ImageError constructor wrappers") {
  const std::string source = R"(
import /std/image/*

[return<int> effects(io_out)]
main() {
  print_line(Result.why(imageErrorStatus(/ImageError/read_unsupported())))
  print_line(Result.why(imageErrorStatus(/ImageError/write_unsupported())))
  print_line(Result.why(imageErrorStatus(/ImageError/invalid_operation())))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_image_error_constructor_wrappers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_error_constructor_wrappers").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_error_constructor_wrappers.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_read_unsupported\n"
        "image_write_unsupported\n"
        "image_invalid_operation\n");
}

TEST_CASE("native uses stdlib GfxError result helpers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int> effects(io_out)]
main() {
  [GfxError] err{queueSubmitFailed()}
  [GfxError] valueErr{framePresentFailed()}
  print_line(Result.why(GfxError.status(queueSubmitFailed())))
  print_line(Result.why(valueErr.result<i32>()))
  print_line(Result.why(GfxError.status(err)))
  print_line(Result.why(GfxError.status(err)))
  print_line(Result.why(err.result<i32>()))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_gfx_error_result_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_gfx_error_result_helpers").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_gfx_error_result_helpers.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "queue_submit_failed\n"
        "frame_present_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n");
}

TEST_CASE("native uses canonical stdlib GfxError result helpers") {
  const std::string source = R"(
import /std/gfx/*

[return<int> effects(io_out)]
main() {
  [GfxError] err{queueSubmitFailed()}
  [GfxError] valueErr{framePresentFailed()}
  print_line(Result.why(GfxError.status(queueSubmitFailed())))
  print_line(Result.why(valueErr.result<i32>()))
  print_line(Result.why(GfxError.status(err)))
  print_line(Result.why(err.result<i32>()))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_canonical_gfx_error_result_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_canonical_gfx_error_result_helpers").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_canonical_gfx_error_result_helpers.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "queue_submit_failed\n"
        "frame_present_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n");
}

TEST_CASE("native uses canonical stdlib GfxError why helpers") {
  const std::string source = R"(
import /std/gfx/*

[return<int> effects(io_out)]
main() {
  [GfxError] err{queueSubmitFailed()}
  [Result<GfxError>] methodStatus{GfxError.status(err)}
  [Result<i32, GfxError>] methodValueStatus{err.result<i32>()}
  print_line(GfxError.why(err))
  print_line(GfxError.why(err))
  print_line(err.why())
  print_line(Result.why(GfxError.status(err)))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_canonical_gfx_error_why_wrapper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_canonical_gfx_error_why_wrapper").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_canonical_gfx_error_why_wrapper.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n");
}

TEST_CASE("native uses canonical stdlib GfxError constructors") {
  const std::string source = R"(
import /std/gfx/*

[return<int> effects(io_out)]
main() {
  [GfxError] windowErr{windowCreateFailed()}
  [GfxError] deviceErr{deviceCreateFailed()}
  [GfxError] presentErr{framePresentFailed()}
  print_line(Result.why(GfxError.status(windowErr)))
  print_line(Result.why(GfxError.status(deviceErr)))
  print_line(Result.why(GfxError.status(presentErr)))
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_gfx_error_constructor_wrappers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_canonical_gfx_error_constructor_wrappers").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_canonical_gfx_error_constructor_wrappers.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "window_create_failed\n"
        "device_create_failed\n"
        "frame_present_failed\n");
}

TEST_CASE("native uses stdlib experimental Buffer helper methods") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int>]
main() {
  [Buffer<i32>] emptyBuffer{Buffer<i32>{[token] 0i32, [elementCount] 0i32}}
  [Buffer<i32>] fullBuffer{Buffer<i32>{[token] 7i32, [elementCount] 4i32}}
  if(not(emptyBuffer.empty())) {
    return(90i32)
  }
  if(emptyBuffer.is_valid()) {
    return(91i32)
  }
  if(fullBuffer.empty()) {
    return(92i32)
  }
  if(not(fullBuffer.is_valid())) {
    return(93i32)
  }
  return(plus(fullBuffer.count(), /std/gfx/experimental/Buffer/count(emptyBuffer)))
}
)";
  const std::string srcPath = writeTemp("compile_native_experimental_gfx_buffer_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_experimental_gfx_buffer_helpers").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("native uses canonical stdlib Buffer helper methods") {
  const std::string source = R"(
import /std/gfx/*

[return<int>]
main() {
  [Buffer<i32>] emptyBuffer{Buffer<i32>{[token] 0i32, [elementCount] 0i32}}
  [Buffer<i32>] fullBuffer{Buffer<i32>{[token] 7i32, [elementCount] 5i32}}
  if(not(emptyBuffer.empty())) {
    return(90i32)
  }
  if(emptyBuffer.is_valid()) {
    return(91i32)
  }
  if(fullBuffer.empty()) {
    return(92i32)
  }
  if(not(fullBuffer.is_valid())) {
    return(93i32)
  }
  return(plus(fullBuffer.count(), /std/gfx/Buffer/count(emptyBuffer)))
}
)";
  const std::string srcPath = writeTemp("compile_native_canonical_gfx_buffer_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_canonical_gfx_buffer_helpers").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("native uses stdlib experimental Buffer readback helpers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gpu/buffer<i32>(3i32)}
  [array<i32>] viaMethod{data.readback()}
  [array<i32>] viaDirect{/std/gfx/experimental/Buffer/readback(data)}
  return(plus(plus(viaMethod.count(), viaDirect.count()), plus(viaMethod[0i32], viaDirect[2i32])))
}
)";
  const std::string srcPath = writeTemp("compile_native_experimental_gfx_buffer_readback_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_experimental_gfx_buffer_readback_helpers").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("native uses canonical stdlib Buffer readback helpers") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gpu/buffer<i32>(3i32)}
  [array<i32>] viaMethod{data.readback()}
  [array<i32>] viaDirect{/std/gfx/Buffer/readback(data)}
  return(plus(plus(viaMethod.count(), viaDirect.count()), plus(viaMethod[0i32], viaDirect[2i32])))
}
)";
  const std::string srcPath = writeTemp("compile_native_canonical_gfx_buffer_readback_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_canonical_gfx_buffer_readback_helpers").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("native uses stdlib experimental Buffer allocation helpers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gfx/experimental/Buffer/allocate<i32>(3i32)}
  [array<i32>] out{data.readback()}
  return(plus(/std/gfx/experimental/Buffer/count(data), out.count()))
}
)";
  const std::string srcPath = writeTemp("compile_native_experimental_gfx_buffer_allocate_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_experimental_gfx_buffer_allocate_helpers").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("native uses canonical stdlib Buffer allocation helpers") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gfx/Buffer/allocate<i32>(3i32)}
  [array<i32>] out{data.readback()}
  return(plus(/std/gfx/Buffer/count(data), out.count()))
}
)";
  const std::string srcPath = writeTemp("compile_native_canonical_gfx_buffer_allocate_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_canonical_gfx_buffer_allocate_helpers").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("native uses stdlib experimental Buffer constructor entry point") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{Buffer<i32>(3i32)}
  [array<i32>] out{data.readback()}
  return(plus(/std/gfx/experimental/Buffer/count(data), out.count()))
}
)";
  const std::string srcPath = writeTemp("compile_native_experimental_gfx_buffer_constructor.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_experimental_gfx_buffer_constructor").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("native uses canonical stdlib Buffer constructor entry point") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{Buffer<i32>(3i32)}
  [array<i32>] out{data.readback()}
  return(plus(/std/gfx/Buffer/count(data), out.count()))
}
)";
  const std::string srcPath = writeTemp("compile_native_canonical_gfx_buffer_constructor.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_canonical_gfx_buffer_constructor").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}


TEST_SUITE_END();
#endif
