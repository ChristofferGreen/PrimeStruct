#include "test_compile_run_helpers.h"

#include "test_compile_run_smoke_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

TEST_CASE("primec wasm wasi runs stored sub-filter rgb png inputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_sub_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x01, 0x07, 0x00, 0xf8, 0xff, 0x01,
        0x0a, 0x14, 0x1e, 0x28, 0x32, 0x3c, 0x02, 0x3e,
        0x00, 0xd4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00, 0x00,
        0x00, 0x00,
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
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_sub.prime", source);
  const std::string wasmPath = (tempRoot / "png_sub.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_sub_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "png_sub_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            3,
                            "2\n"
                            "1\n"
                            "6\n"
                            "10\n"
                            "20\n"
                            "30\n"
                            "50\n"
                            "70\n"
                            "90\n");
}

TEST_CASE("primec wasm wasi runs stored up-filter rgb png inputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_up_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x13, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x01, 0x08, 0x00, 0xf7, 0xff, 0x00,
        0x0a, 0x14, 0x1e, 0x02, 0x05, 0x07, 0x09, 0x01,
        0x8a, 0x00, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00,
        0x00, 0x00, 0x00,
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
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_up.prime", source);
  const std::string wasmPath = (tempRoot / "png_up.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_up_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "png_up_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            3,
                            "1\n"
                            "2\n"
                            "6\n"
                            "10\n"
                            "20\n"
                            "30\n"
                            "15\n"
                            "27\n"
                            "39\n");
}

TEST_CASE("primec wasm wasi runs stored average-filter rgb png inputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_average_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x13, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x01, 0x08, 0x00, 0xf7, 0xff, 0x00,
        0x0a, 0x14, 0x1e, 0x03, 0x0a, 0x11, 0x18, 0x01,
        0xc0, 0x00, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00,
        0x00, 0x00, 0x00,
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
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_average.prime", source);
  const std::string wasmPath = (tempRoot / "png_average.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_average_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "png_average_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            3,
                            "1\n"
                            "2\n"
                            "6\n"
                            "10\n"
                            "20\n"
                            "30\n"
                            "15\n"
                            "27\n"
                            "39\n");
}

TEST_CASE("primec wasm wasi runs stored paeth-filter rgb png inputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_paeth_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x19, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x01, 0x0e, 0x00, 0xf1, 0xff, 0x04,
        0x0a, 0x14, 0x1e, 0x28, 0x32, 0x3c, 0x04, 0x05,
        0x07, 0x09, 0x0a, 0x14, 0x1e, 0x09, 0x19, 0x01,
        0x2c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x49, 0x45, 0x4e, 0x44, 0x00, 0x00, 0x00,
        0x00,
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
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  print_line(pixels[9i32])
  print_line(pixels[10i32])
  print_line(pixels[11i32])
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_paeth.prime", source);
  const std::string wasmPath = (tempRoot / "png_paeth.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_paeth_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "png_paeth_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            4,
                            "2\n"
                            "2\n"
                            "12\n"
                            "10\n"
                            "20\n"
                            "30\n"
                            "50\n"
                            "70\n"
                            "90\n"
                            "15\n"
                            "27\n"
                            "39\n"
                            "60\n"
                            "90\n"
                            "120\n");
}

TEST_CASE("primec wasm wasi runs fixed-huffman backreference rgb png inputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_fixed_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x63, 0xe0, 0x12, 0x91, 0x83, 0x20,
        0x00, 0x03, 0x52, 0x00, 0xb5, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e,
        0x44,
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
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_fixed.prime", source);
  const std::string wasmPath = (tempRoot / "png_fixed.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_fixed_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "png_fixed_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            4,
                            "3\n"
                            "1\n"
                            "9\n"
                            "10\n"
                            "20\n"
                            "30\n"
                            "10\n"
                            "20\n"
                            "30\n"
                            "10\n"
                            "20\n"
                            "30\n");
}

TEST_CASE("primec wasm wasi runs dynamic-huffman literal rgb png inputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_dynamic_literal_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    std::ofstream input(tempRoot / "input.png", std::ios::binary);
    REQUIRE(input.good());
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x1b, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x05, 0xc0, 0xa1, 0x00, 0x00, 0x00,
        0x00, 0x03, 0xb0, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7,
        0xe7, 0xfb, 0x73, 0x7d, 0xa0, 0x9c, 0xee, 0x02,
        0x37, 0x00, 0xd3, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00,
        0x00, 0x00, 0x00,
    });
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
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_dynamic_literal.prime", source);
  const std::string wasmPath = (tempRoot / "png_dynamic_literal.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_dynamic_literal_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "png_dynamic_literal_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            3,
                            "2\n"
                            "1\n"
                            "6\n"
                            "10\n"
                            "20\n"
                            "30\n"
                            "40\n"
                            "50\n"
                            "60\n");
}

TEST_CASE("primec wasm wasi runs dynamic-huffman backreference rgb png inputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_dynamic_backref_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    std::ofstream input(tempRoot / "input.png", std::ios::binary);
    REQUIRE(input.good());
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x1b, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x25, 0xc3, 0xa1, 0x00, 0x00, 0x00,
        0x00, 0xc3, 0x80, 0xff, 0xf1, 0xf9, 0xf9, 0xfe,
        0x98, 0x0f, 0xdf, 0x36, 0x38, 0x7d, 0x03, 0x03,
        0x52, 0x00, 0xb5, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00,
        0x00, 0x00, 0x00,
    });
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
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_dynamic_backref.prime", source);
  const std::string wasmPath = (tempRoot / "png_dynamic_backref.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_dynamic_backref_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "png_dynamic_backref_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            4,
                            "3\n"
                            "1\n"
                            "9\n"
                            "10\n"
                            "20\n"
                            "30\n"
                            "10\n"
                            "20\n"
                            "30\n"
                            "10\n"
                            "20\n"
                            "30\n");
}


TEST_SUITE_END();
