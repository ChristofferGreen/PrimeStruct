#include "test_compile_run_helpers.h"

#include "test_compile_run_native_backend_core_helpers.h"

#if PRIMESTRUCT_NATIVE_CORE_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.core");

TEST_CASE("compiles and runs native png read for 16-bit rgb inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_rgb16_sub.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x18, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x0d, 0x00, 0xf2, 0xff, 0x01,
        0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0x5c, 0x17,
        0x11, 0x11, 0x11, 0x11, 0x17, 0xfb, 0x03, 0x23,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x49, 0x45, 0x4e, 0x44, 0x00, 0x00, 0x00, 0x00,
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
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
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
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_rgb16_sub_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_rgb16_sub_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_rgb16_sub_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "18\n"
        "86\n"
        "154\n"
        "109\n"
        "103\n"
        "171\n");
}

TEST_CASE("compiles and runs native png read for 16-bit rgba inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_rgba16_sub.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x10, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x1c, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x11, 0x00, 0xee, 0xff, 0x01,
        0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
        0x4a, 0xa7, 0xef, 0xef, 0xef, 0xef, 0x10, 0x00,
        0x47, 0x51, 0x08, 0xf7, 0x00, 0x00, 0x00, 0x00,
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
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
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
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_rgba16_sub_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_rgba16_sub_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_rgba16_sub_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "18\n"
        "86\n"
        "154\n"
        "92\n"
        "69\n"
        "137\n");
}

TEST_CASE("compiles and runs native png read for 16-bit grayscale-alpha inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_gray_alpha_16_sub.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x10, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x14, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x09, 0x00, 0xf6, 0xff, 0x01,
        0x12, 0x34, 0x56, 0x78, 0x5c, 0x17, 0x44, 0x44,
        0x08, 0xeb, 0x02, 0x11, 0x00, 0x00, 0x00, 0x00,
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
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
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
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_gray_alpha_16_sub_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_gray_alpha_16_sub_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_gray_alpha_16_sub_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "18\n"
        "18\n"
        "18\n"
        "109\n"
        "109\n"
        "109\n");
}

TEST_CASE("compiles and runs native png read for Adam7 interlaced rgb inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_interlaced_rgb.png").string();
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
        0x49, 0x45, 0x4e, 0x44, 0x00, 0x00, 0x00, 0x00,
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
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
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
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_interlaced_rgb_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_interlaced_rgb_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_interlaced_rgb_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 10);
  CHECK(readFile(outPath) ==
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

TEST_CASE("compiles and runs native png read for Adam7 interlaced indexed-color inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_interlaced_indexed.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05,
        0x02, 0x03, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0c, 0x50, 0x4c, 0x54,
        0x45, 0x00, 0x00, 0x00, 0x40, 0x20, 0x10, 0x80,
        0x60, 0x30, 0xff, 0xc8, 0x64, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x23, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x01, 0x01, 0x18, 0x00, 0xe7, 0xff,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
        0x00, 0x80, 0x00, 0x88, 0x00, 0x70, 0x00, 0xd0,
        0x00, 0x70, 0x00, 0x6c, 0x40, 0x00, 0xc6, 0xc0,
        0x2b, 0x98, 0x05, 0x6b, 0x00, 0x00, 0x00, 0x00,
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
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
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
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_interlaced_indexed_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_interlaced_indexed_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_interlaced_indexed_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 10);
  CHECK(readFile(outPath) ==
        "5\n"
        "5\n"
        "75\n"
        "0\n"
        "0\n"
        "0\n"
        "0\n"
        "0\n"
        "0\n"
        "0\n"
        "0\n"
        "0\n"
        "128\n"
        "96\n"
        "48\n"
        "255\n"
        "200\n"
        "100\n"
        "128\n"
        "96\n"
        "48\n"
        "64\n"
        "32\n"
        "16\n"
        "64\n"
        "32\n"
        "16\n"
        "128\n"
        "96\n"
        "48\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("compiles and rejects malformed Adam7 interlaced native png inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_interlaced_invalid.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05,
        0x08, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x60, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x01, 0x01, 0x55, 0x00, 0xaa, 0xff,
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
        0x8d, 0xbd, 0x2f, 0xb5, 0xca, 0xb0, 0x4e, 0x1e,
        0xae, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x49, 0x45, 0x4e, 0x44, 0x00, 0x00, 0x00,
        0x00,
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
  const std::string srcPath = writeTemp("compile_native_image_read_interlaced_invalid_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_interlaced_invalid_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_interlaced_invalid_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("compiles and runs native png read for optional plte and split idat inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_plte_split_idat.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x03, 0x50, 0x4c, 0x54, 0x45,
        0xaa, 0xbb, 0xcc, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x08, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x04, 0x00, 0xfb, 0xff, 0x00,
        0x00, 0x00, 0x00, 0x00,
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
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
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
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_plte_split_idat_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_plte_split_idat_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_plte_split_idat_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 2);
  CHECK(readFile(outPath) ==
        "1\n"
        "1\n"
        "3\n"
        "255\n"
        "0\n"
        "0\n");
}


TEST_SUITE_END();
#endif
