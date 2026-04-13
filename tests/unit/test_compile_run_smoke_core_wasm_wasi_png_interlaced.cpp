#include "test_compile_run_helpers.h"

#include "test_compile_run_smoke_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

TEST_CASE("primec wasm wasi runs broader interlaced png decode programs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_png_interlaced_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05,
        0x08, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x61, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x01, 0x01, 0x56, 0x00, 0xa9, 0xff,
        0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x34, 0x68,
        0x00, 0x1c, 0xc8, 0x2c, 0xbc, 0xfc, 0x94, 0x00,
        0x50, 0x1a, 0xb4, 0x00, 0x6c, 0xe2, 0xe0, 0x00,
        0x0e, 0x64, 0x16, 0x5e, 0x7e, 0xca, 0xae, 0x98,
        0x7e, 0x00, 0x28, 0x0d, 0x5a, 0x78, 0x27, 0x0e,
        0x00, 0x36, 0x71, 0x70, 0x86, 0x8b, 0x24, 0x00,
        0x44, 0xd5, 0x86, 0x94, 0xef, 0x3a, 0x00, 0x07,
        0x32, 0x0b, 0x2f, 0x3f, 0x65, 0x57, 0x4c, 0xbf,
        0x7f, 0x59, 0x19, 0xa7, 0x66, 0x73, 0x00, 0x15,
        0x96, 0x21, 0x3d, 0xa3, 0x7b, 0x65, 0xb0, 0xd5,
        0x8d, 0xbd, 0x2f, 0xb5, 0xca, 0x89, 0xcf, 0x85,
        0x1f, 0x37,
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
  print_line(pixels[12i32])
  print_line(pixels[13i32])
  print_line(pixels[14i32])
  print_line(pixels[60i32])
  print_line(pixels[61i32])
  print_line(pixels[62i32])
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  print_line(pixels[9i32])
  print_line(pixels[10i32])
  print_line(pixels[11i32])
  print_line(pixels[30i32])
  print_line(pixels[31i32])
  print_line(pixels[32i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  print_line(pixels[15i32])
  print_line(pixels[16i32])
  print_line(pixels[17i32])
  print_line(pixels[54i32])
  print_line(pixels[55i32])
  print_line(pixels[56i32])
  print_line(pixels[72i32])
  print_line(pixels[73i32])
  print_line(pixels[74i32])
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_png_interlaced.prime", source);
  const std::string wasmPath = (tempRoot / "png_interlaced.wasm").string();
  const std::string compileErrPath = (tempRoot / "png_interlaced_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "png_interlaced_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            10,
                            "5\n"
                            "5\n"
                            "75\n"
                            "0\n"
                            "0\n"
                            "0\n"
                            "160\n"
                            "52\n"
                            "104\n"
                            "28\n"
                            "200\n"
                            "44\n"
                            "80\n"
                            "26\n"
                            "180\n"
                            "120\n"
                            "39\n"
                            "14\n"
                            "14\n"
                            "100\n"
                            "22\n"
                            "40\n"
                            "13\n"
                            "90\n"
                            "7\n"
                            "50\n"
                            "11\n"
                            "141\n"
                            "189\n"
                            "47\n"
                            "188\n"
                            "252\n"
                            "148\n");
}


TEST_SUITE_END();
