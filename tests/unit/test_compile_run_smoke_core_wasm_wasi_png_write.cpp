#include "test_compile_run_helpers.h"

#include "test_compile_run_smoke_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

TEST_CASE("primec wasm wasi runs ppm write for deterministic rgb outputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_ppm_write_success_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, file_write), return<int>]
main() {
  [vector<i32>] pixels{vector<i32>(255i32, 0i32, 0i32, 0i32, 255i32, 128i32)}
  [Result<ImageError>] status{/std/image/ppm/write("output.ppm"utf8, 2i32, 1i32, pixels)}
  if(Result.error(status),
     then() { return(1i32) },
     else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_ppm_write_success.prime", source);
  const std::string wasmPath = (tempRoot / "ppm_write_success.wasm").string();
  const std::string compileErrPath = (tempRoot / "ppm_write_success_compile_err.txt").string();
  const std::string wasmCmd = "./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                              quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(compileErrPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  if (hasWasmtime()) {
    const std::string outPath = (tempRoot / "ppm_write_success_stdout.txt").string();
    const std::string wasmRunCmd = "wasmtime --invoke main --dir=" + quoteShellArg(tempRoot.string()) + " " +
                                   quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(wasmRunCmd) == 0);
    CHECK(readFile(outPath).empty());
    CHECK(readFile((tempRoot / "output.ppm").string()) ==
          "P3\n"
          "2 1\n"
          "255\n"
          "255\n"
          "0\n"
          "0\n"
          "0\n"
          "255\n"
          "128\n");
  }
}

TEST_CASE("primec wasm wasi png write requires heap_alloc for vector literal") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_write_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(file_write), return<int>]
main() {
  [vector<i32>] pixels{vector<i32>(255i32, 0i32, 0i32, 0i32, 255i32, 128i32)}
  [Result<ImageError>] status{/std/image/png/write("output.png"utf8, 2i32, 1i32, pixels)}
  if(Result.error(status),
     then() { return(1i32) },
     else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_write.prime", source);
  const std::string wasmPath = (tempRoot / "png_write.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_write_compile_err.txt").string();
  const std::string wasmCmd = "./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                              quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(compileErrPath);
  CHECK(runCommand(wasmCmd) == 2);
  CHECK(!std::filesystem::exists(wasmPath));
  CHECK(readFile(compileErrPath).find("vector literal requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("primec wasm wasi runs png write for deterministic rgb outputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_write_success_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, file_write), return<int>]
main() {
  [vector<i32>] pixels{vector<i32>(255i32, 0i32, 0i32, 0i32, 255i32, 128i32)}
  [Result<ImageError>] status{/std/image/png/write("output.png"utf8, 2i32, 1i32, pixels)}
  if(Result.error(status),
     then() { return(1i32) },
     else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_write_success.prime", source);
  const std::string wasmPath = (tempRoot / "png_write_success.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_write_success_compile_err.txt").string();
  const std::string wasmCmd = "./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                              quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(compileErrPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  if (hasWasmtime()) {
    const std::string outPath = (tempRoot / "png_write_success_stdout.txt").string();
    const std::string wasmRunCmd = "wasmtime --invoke main --dir=" + quoteShellArg(tempRoot.string()) + " " +
                                   quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(wasmRunCmd) == 0);
    CHECK(readFile(outPath).empty());
    const std::vector<unsigned char> expected = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x07, 0x00, 0xf8, 0xff, 0x00,
        0xff, 0x00, 0x00, 0x00, 0xff, 0x80, 0x08, 0x7f,
        0x02, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00, 0x00,
        0x00, 0x00,
    });
    CHECK(readBinaryFile((tempRoot / "output.png").string()) == expected);
  }
}

TEST_CASE("primec wasm wasi invalid png write requires heap_alloc for vector literal") {
  const std::filesystem::path tempRoot =
      testScratchPath("") / "primec_wasm_png_write_invalid_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(io_out, file_write), return<int>]
main() {
  [vector<i32>] pixels{vector<i32>(255i32, 0i32, 0i32)}
  [Result<ImageError>] status{/std/image/png/write("output.png"utf8, 2i32, 1i32, pixels)}
  print_line(Result.why(status))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_write_invalid.prime", source);
  const std::string wasmPath = (tempRoot / "png_write_invalid.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_write_invalid_compile_err.txt").string();
  const std::string wasmCmd = "./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                              quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(compileErrPath);
  CHECK(runCommand(wasmCmd) == 2);
  CHECK(!std::filesystem::exists(wasmPath));
  CHECK(readFile(compileErrPath).find("vector literal requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("primec wasm wasi rejects invalid png write inputs deterministically") {
  const std::filesystem::path tempRoot =
      testScratchPath("") / "primec_wasm_png_write_invalid_result_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [vector<i32>] pixels{vector<i32>(255i32, 0i32, 0i32)}
  [Result<ImageError>] status{/std/image/png/write("output.png"utf8, 2i32, 1i32, pixels)}
  print_line(Result.why(status))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_write_invalid_result.prime", source);
  const std::string wasmPath = (tempRoot / "png_write_invalid_result.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_write_invalid_result_compile_err.txt").string();
  const std::string wasmCmd = "./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                              quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(compileErrPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  if (hasWasmtime()) {
    const std::string outPath = (tempRoot / "png_write_invalid_result_stdout.txt").string();
    const std::string wasmRunCmd = "wasmtime --invoke main --dir=" + quoteShellArg(tempRoot.string()) + " " +
                                   quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(wasmRunCmd) == 0);
    CHECK(readFile(outPath) == "image_invalid_operation\n");
    CHECK(!std::filesystem::exists(tempRoot / "output.png"));
  }
}

TEST_CASE("primec wasm wasi runs stored rgb png inputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_read_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0f, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x01, 0x04, 0x00, 0xfb, 0xff, 0x00,
        0xff, 0x00, 0x00, 0x03, 0x01, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x49, 0x45, 0x4e, 0x44,
        0x00, 0x00, 0x00, 0x00,
    });
    std::ofstream input(tempRoot / "input.png", std::ios::binary);
    REQUIRE(input.good());
    input.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(input.good());
  }

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "input.png"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_read.prime", source);
  const std::string wasmPath = (tempRoot / "png_read.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_read_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "png_read_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot, wasmPath, outPath, 2, "1\n1\n3\n255\n0\n0\n");
}


TEST_SUITE_END();
