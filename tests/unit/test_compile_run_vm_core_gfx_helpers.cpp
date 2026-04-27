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
  print_line(Result.why(/ImageError/status(imageReadUnsupported())))
  print_line(Result.why(/ImageError/result<i32>(imageWriteUnsupported())))
  print_line(Result.why(/ImageError/result<i32>(imageInvalidOperation())))
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
  print_line(Result.why(imageErrorStatus(/ImageError/read_unsupported())))
  print_line(Result.why(imageErrorStatus(/ImageError/write_unsupported())))
  print_line(Result.why(imageErrorStatus(/ImageError/invalid_operation())))
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
  print_line(Result.why(GfxError.status(queueSubmitFailed())))
  print_line(Result.why(valueErr.result<i32>()))
  print_line(Result.why(GfxError.status(err)))
  print_line(Result.why(GfxError.status(err)))
  print_line(Result.why(err.result<i32>()))
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
  print_line(Result.why(GfxError.status(queueSubmitFailed())))
  print_line(Result.why(valueErr.result<i32>()))
  print_line(Result.why(GfxError.status(err)))
  print_line(Result.why(err.result<i32>()))
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
  print_line(Result.why(GfxError.status(err)))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
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
  print_line(Result.why(GfxError.status(windowErr)))
  print_line(Result.why(GfxError.status(deviceErr)))
  print_line(Result.why(GfxError.status(presentErr)))
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
  const std::string srcPath = writeTemp("vm_experimental_gfx_buffer_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("vm uses canonical stdlib Buffer helper methods") {
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
  return(plus(plus(viaMethod.count(), viaDirect.count()), plus(viaMethod[0i32], viaDirect[2i32])))
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
  return(plus(plus(viaMethod.count(), viaDirect.count()), plus(viaMethod[0i32], viaDirect[2i32])))
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
  return(plus(/std/gfx/experimental/Buffer/count(data), out.count()))
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
  return(plus(/std/gfx/Buffer/count(data), out.count()))
}
)";
  const std::string srcPath = writeTemp("vm_canonical_gfx_buffer_allocate_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("vm uses stdlib experimental Buffer constructor entry point") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{Buffer<i32>(3i32)}
  [array<i32>] out{data.readback()}
  return(plus(/std/gfx/experimental/Buffer/count(data), out.count()))
}
)";
  const std::string srcPath = writeTemp("vm_experimental_gfx_buffer_constructor.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("vm uses canonical stdlib Buffer constructor entry point") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{Buffer<i32>(3i32)}
  [array<i32>] out{data.readback()}
  return(plus(/std/gfx/Buffer/count(data), out.count()))
}
)";
  const std::string srcPath = writeTemp("vm_canonical_gfx_buffer_constructor.prime", source);
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
  return(plus(plus(out.count(), out[0i32]), out[2i32]))
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
  return(plus(plus(out.count(), out[0i32]), out[2i32]))
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
