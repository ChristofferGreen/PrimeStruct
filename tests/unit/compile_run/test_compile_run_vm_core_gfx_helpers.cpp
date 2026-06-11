#include "test_compile_run_helpers.h"

#include <cerrno>

TEST_SUITE_BEGIN("primestruct.compile.run.vm.core");


#if defined(EACCES)
TEST_CASE("vm maps FileError.why codes") {
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
  const std::string srcPath = writeTemp("vm_file_error_why.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_file_error_why_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "EACCES\n");
}
#endif

TEST_CASE("vm uses stdlib ImageError result helpers") {
  const std::string source = R"(
import /std/image/*

[return<int> effects(io_out)]
main() {
  [Result<ImageError>] readStatus{/ImageError/status(imageReadUnsupported())}
  [Result<i32, ImageError>] writeStatus{/ImageError/result<i32>(imageWriteUnsupported())}
  [Result<i32, ImageError>] invalidStatus{/ImageError/result<i32>(imageInvalidOperation())}
  [string] readWhy{Result.why(readStatus)}
  [string] writeWhy{Result.why(writeStatus)}
  [string] invalidWhy{Result.why(invalidStatus)}
  print_line(readWhy)
  print_line(writeWhy)
  print_line(invalidWhy)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_image_error_result_helpers.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_image_error_result_helpers.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) ==
        "image_read_unsupported\n"
        "image_write_unsupported\n"
        "image_invalid_operation\n");
}

TEST_CASE("vm uses stdlib ImageError why wrapper") {
  const std::string source = R"(
import /std/image/*

[return<int> effects(io_out)]
main() {
  [ImageError] err{imageReadUnsupported()}
  [Result<ImageError>] compatibilityStatus{imageErrorStatus(err)}
  [Result<ImageError>] methodStatus{/ImageError/status(err)}
  [Result<i32, ImageError>] methodValueStatus{/ImageError/result<i32>(err)}
  [string] compatibilityWhy{Result.why(compatibilityStatus)}
  [string] methodWhy{Result.why(methodStatus)}
  [string] methodValueWhy{Result.why(methodValueStatus)}
  print_line(/ImageError/why(err))
  print_line(/ImageError/why(err))
  print_line(/ImageError/why(err))
  print_line(compatibilityWhy)
  print_line(methodWhy)
  print_line(methodValueWhy)
  print_line(methodWhy)
  print_line(methodValueWhy)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_image_error_why_wrapper.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_image_error_why_wrapper.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
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

TEST_CASE("vm uses stdlib ImageError constructor wrappers") {
  const std::string source = R"(
import /std/image/*

[return<int> effects(io_out)]
main() {
  [ImageError] readErr{/ImageError/read_unsupported()}
  [ImageError] writeErr{/ImageError/write_unsupported()}
  [ImageError] invalidErr{/ImageError/invalid_operation()}
  [Result<ImageError>] readStatus{imageErrorStatus(readErr)}
  [Result<ImageError>] writeStatus{imageErrorStatus(writeErr)}
  [Result<ImageError>] invalidStatus{imageErrorStatus(invalidErr)}
  [string] readWhy{Result.why(readStatus)}
  [string] writeWhy{Result.why(writeStatus)}
  [string] invalidWhy{Result.why(invalidStatus)}
  print_line(readWhy)
  print_line(writeWhy)
  print_line(invalidWhy)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_image_error_constructor_wrappers.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_image_error_constructor_wrappers.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) ==
        "image_read_unsupported\n"
        "image_write_unsupported\n"
        "image_invalid_operation\n");
}

TEST_CASE("vm uses stdlib GfxError result helpers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int> effects(io_out)]
main() {
  [GfxError] err{queueSubmitFailed()}
  [GfxError] valueErr{framePresentFailed()}
  [Result<GfxError>] directStatus{GfxError.status(queueSubmitFailed())}
  [Result<i32, GfxError>] directValueStatus{valueErr.result<i32>()}
  [Result<GfxError>] methodStatus{GfxError.status(err)}
  [Result<i32, GfxError>] methodValueStatus{err.result<i32>()}
  [string] directWhy{Result.why(directStatus)}
  [string] directValueWhy{Result.why(directValueStatus)}
  [string] methodWhy{Result.why(methodStatus)}
  [string] methodValueWhy{Result.why(methodValueStatus)}
  print_line(directWhy)
  print_line(directValueWhy)
  print_line(methodWhy)
  print_line(methodWhy)
  print_line(methodValueWhy)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_gfx_error_result_helpers.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_gfx_error_result_helpers.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) ==
        "queue_submit_failed\n"
        "frame_present_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n");
}

TEST_CASE("vm uses canonical stdlib GfxError result helpers") {
  const std::string source = R"(
import /std/gfx/*

[return<int> effects(io_out)]
main() {
  [GfxError] err{queueSubmitFailed()}
  [GfxError] valueErr{framePresentFailed()}
  [Result<GfxError>] directStatus{GfxError.status(queueSubmitFailed())}
  [Result<i32, GfxError>] directValueStatus{valueErr.result<i32>()}
  [Result<GfxError>] methodStatus{GfxError.status(err)}
  [Result<i32, GfxError>] methodValueStatus{err.result<i32>()}
  [string] directWhy{Result.why(directStatus)}
  [string] directValueWhy{Result.why(directValueStatus)}
  [string] methodWhy{Result.why(methodStatus)}
  [string] methodValueWhy{Result.why(methodValueStatus)}
  print_line(directWhy)
  print_line(directValueWhy)
  print_line(methodWhy)
  print_line(methodValueWhy)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_canonical_gfx_error_result_helpers.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_canonical_gfx_error_result_helpers.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) ==
        "queue_submit_failed\n"
        "frame_present_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n");
}

TEST_CASE("vm uses canonical stdlib GfxError why helpers") {
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
  [Result<GfxError>] directStatus{GfxError.status(err)}
  [string] directWhy{Result.why(directStatus)}
  [string] methodWhy{Result.why(methodStatus)}
  [string] methodValueWhy{Result.why(methodValueStatus)}
  print_line(directWhy)
  print_line(methodWhy)
  print_line(methodValueWhy)
  print_line(methodWhy)
  print_line(methodValueWhy)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_canonical_gfx_error_why_wrapper.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_canonical_gfx_error_why_wrapper.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
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

TEST_CASE("vm uses canonical stdlib GfxError constructors") {
  const std::string source = R"(
import /std/gfx/*

[return<int> effects(io_out)]
main() {
  [GfxError] windowErr{windowCreateFailed()}
  [GfxError] deviceErr{deviceCreateFailed()}
  [GfxError] presentErr{framePresentFailed()}
  [Result<GfxError>] windowStatus{GfxError.status(windowErr)}
  [Result<GfxError>] deviceStatus{GfxError.status(deviceErr)}
  [Result<GfxError>] presentStatus{GfxError.status(presentErr)}
  [string] windowWhy{Result.why(windowStatus)}
  [string] deviceWhy{Result.why(deviceStatus)}
  [string] presentWhy{Result.why(presentStatus)}
  print_line(windowWhy)
  print_line(deviceWhy)
  print_line(presentWhy)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_canonical_gfx_error_constructor_wrappers.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_canonical_gfx_error_constructor_wrappers.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) ==
        "window_create_failed\n"
        "device_create_failed\n"
        "frame_present_failed\n");
}

TEST_CASE("vm uses stdlib experimental Buffer helper methods") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int>]
main() {
  [Buffer<i32>] emptyBuffer{[token] 0i32, [elementCount] 0i32}
  [Buffer<i32>] fullBuffer{[token] 7i32, [elementCount] 4i32}
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
  [i32] fullCount{fullBuffer.count()}
  [i32] emptyCount{/std/gfx/experimental/Buffer/count(emptyBuffer)}
  return(plus(fullCount, emptyCount))
}
)";
  const std::string srcPath = writeTemp("vm_experimental_gfx_buffer_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("vm uses canonical stdlib Buffer helper methods") {
  const std::string source = R"(
import /std/gfx/*

[return<int>]
main() {
  [Buffer<i32>] emptyBuffer{[token] 0i32, [elementCount] 0i32}
  [Buffer<i32>] fullBuffer{[token] 7i32, [elementCount] 5i32}
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
  [i32] fullCount{fullBuffer.count()}
  [i32] emptyCount{/std/gfx/Buffer/count(emptyBuffer)}
  return(plus(fullCount, emptyCount))
}
)";
  const std::string srcPath = writeTemp("vm_canonical_gfx_buffer_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 5);
}

TEST_CASE("vm uses stdlib experimental Buffer readback helpers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gpu/buffer<i32>(3i32)}
  [array<i32>] viaMethod{data.readback()}
  [array<i32>] viaDirect{/std/gfx/experimental/Buffer/readback(data)}
  [i32] methodCount{viaMethod.count()}
  [i32] directCount{viaDirect.count()}
  [i32] methodFirst{viaMethod[0i32]}
  [i32] directLast{viaDirect[2i32]}
  [i32] countTotal{plus(methodCount, directCount)}
  [i32] valueTotal{plus(methodFirst, directLast)}
  return(plus(countTotal, valueTotal))
}
)";
  const std::string srcPath = writeTemp("vm_experimental_gfx_buffer_readback_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("vm uses canonical stdlib Buffer readback helpers") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gpu/buffer<i32>(3i32)}
  [array<i32>] viaMethod{data.readback()}
  [array<i32>] viaDirect{/std/gfx/Buffer/readback(data)}
  [i32] methodCount{viaMethod.count()}
  [i32] directCount{viaDirect.count()}
  [i32] methodFirst{viaMethod[0i32]}
  [i32] directLast{viaDirect[2i32]}
  [i32] countTotal{plus(methodCount, directCount)}
  [i32] valueTotal{plus(methodFirst, directLast)}
  return(plus(countTotal, valueTotal))
}
)";
  const std::string srcPath = writeTemp("vm_canonical_gfx_buffer_readback_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("vm uses stdlib experimental Buffer allocation helpers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gfx/experimental/Buffer/allocate<i32>(3i32)}
  [array<i32>] out{data.readback()}
  [i32] bufferCount{/std/gfx/experimental/Buffer/count(data)}
  [i32] outputCount{out.count()}
  return(plus(bufferCount, outputCount))
}
)";
  const std::string srcPath = writeTemp("vm_experimental_gfx_buffer_allocate_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("vm uses canonical stdlib Buffer allocation helpers") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gfx/Buffer/allocate<i32>(3i32)}
  [array<i32>] out{data.readback()}
  [i32] bufferCount{/std/gfx/Buffer/count(data)}
  [i32] outputCount{out.count()}
  return(plus(bufferCount, outputCount))
}
)";
  const std::string srcPath = writeTemp("vm_canonical_gfx_buffer_allocate_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("vm uses stdlib experimental Buffer allocation readback path") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gfx/experimental/Buffer/allocate<i32>(3i32)}
  [array<i32>] out{data.readback()}
  [i32] bufferCount{/std/gfx/experimental/Buffer/count(data)}
  [i32] outputCount{out.count()}
  return(plus(bufferCount, outputCount))
}
)";
  const std::string srcPath = writeTemp("vm_experimental_gfx_buffer_allocation_readback.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("vm uses canonical stdlib Buffer allocation readback path") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gfx/Buffer/allocate<i32>(3i32)}
  [array<i32>] out{data.readback()}
  [i32] bufferCount{/std/gfx/Buffer/count(data)}
  [i32] outputCount{out.count()}
  return(plus(bufferCount, outputCount))
}
)";
  const std::string srcPath = writeTemp("vm_canonical_gfx_buffer_allocation_readback.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("vm uses stdlib experimental Buffer upload helpers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  [Buffer<i32>] data{/std/gfx/experimental/Buffer/upload(values)}
  [array<i32>] out{data.readback()}
  [i32] outputCount{out.count()}
  [i32] first{out[0i32]}
  [i32] last{out[2i32]}
  [i32] headTotal{plus(outputCount, first)}
  return(plus(headTotal, last))
}
)";
  const std::string srcPath = writeTemp("vm_experimental_gfx_buffer_upload_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("vm uses canonical stdlib Buffer upload helpers") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  [Buffer<i32>] data{/std/gfx/Buffer/upload(values)}
  [array<i32>] out{data.readback()}
  [i32] outputCount{out.count()}
  [i32] first{out[0i32]}
  [i32] last{out[2i32]}
  [i32] headTotal{plus(outputCount, first)}
  return(plus(headTotal, last))
}
)";
  const std::string srcPath = writeTemp("vm_canonical_gfx_buffer_upload_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("vm uses stdlib FileError why wrapper") {
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
  const std::string srcPath = writeTemp("vm_stdlib_file_error_why.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_file_error_why_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "EOF\nEOF\nEOF\nEOF\n");
}

TEST_CASE("vm uses stdlib FileError eof wrapper") {
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
  if(not(eofErr.is_eof())) {
    return(3i32)
  }
  if(FileError.is_eof(otherErr)) {
    return(4i32)
  }
  if(FileError.is_eof(otherErr)) {
    return(5i32)
  }
  if(otherErr.is_eof()) {
    return(6i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_file_error_is_eof.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("vm uses stdlib FileError eof constructor wrapper") {
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
  if(not(err.is_eof())) {
    return(2i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_file_error_eof_wrapper.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_file_error_eof_wrapper_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "EOF\nEOF\n");
}


TEST_SUITE_END();
