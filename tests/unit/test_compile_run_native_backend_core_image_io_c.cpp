#include "test_compile_run_helpers.h"

#include "test_compile_run_native_backend_core_helpers.h"

#if PRIMESTRUCT_NATIVE_CORE_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.core");

TEST_CASE("compiles and runs native png read for stored paeth-filter rgba inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_paeth_rgba.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x1d, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x01, 0x12, 0x00, 0xed, 0xff, 0x04,
        0x0a, 0x14, 0x1e, 0x28, 0x1e, 0x1e, 0x1e, 0x28,
        0x04, 0x05, 0x07, 0x09, 0x14, 0x05, 0x0f, 0x19,
        0x14, 0x0d, 0x9c, 0x01, 0x59, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e,
        0x44, 0x00, 0x00, 0x00, 0x00,
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
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  print_line(pixels[9i32])
  print_line(pixels[10i32])
  print_line(pixels[11i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_paeth_rgba_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_paeth_rgba_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_paeth_rgba_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 4);
  CHECK(readFile(outPath) ==
        "2\n"
        "2\n"
        "12\n"
        "10\n"
        "20\n"
        "30\n"
        "40\n"
        "50\n"
        "60\n"
        "15\n"
        "27\n"
        "39\n"
        "45\n"
        "65\n"
        "85\n");
}

TEST_CASE("compiles and runs native png read for fixed-huffman backreference rgb inputs") {
  const std::string inPath = (testScratchPath("") / "primec_native_image_read_fixed.png").string();
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
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_fixed_png.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_image_read_fixed_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_fixed_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 4);
  CHECK(readFile(outPath) ==
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

TEST_CASE("compiles and runs native png read for dynamic-huffman literal rgb inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_dynamic_literal.png").string();
  {
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
  const std::string srcPath = writeTemp("compile_native_image_read_dynamic_literal_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_dynamic_literal_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_dynamic_literal_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
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

TEST_CASE("compiles and runs native png read for dynamic-huffman backreference rgb inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_dynamic_backref.png").string();
  {
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
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_dynamic_backref_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_dynamic_backref_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_dynamic_backref_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 4);
  CHECK(readFile(outPath) ==
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

TEST_CASE("compiles and rejects malformed native png inputs deterministically") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_invalid.png").string();
  {
    const std::vector<unsigned char> pngBytes = {0x00, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
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
  const std::string srcPath = writeTemp("compile_native_image_read_invalid_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_invalid_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_invalid_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("compiles and runs native png read for sub-filter indexed-color inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_indexed_sub.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x09, 0x50, 0x4c, 0x54, 0x45,
        0x00, 0x00, 0x00, 0x0a, 0x14, 0x1e, 0x32, 0x46,
        0x5a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x0e, 0x49, 0x44, 0x41, 0x54, 0x78, 0x01, 0x01,
        0x03, 0x00, 0xfc, 0xff, 0x01, 0x01, 0x01, 0x00,
        0x09, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00,
        0x00, 0x00, 0x00,
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
  const std::string srcPath = writeTemp("compile_native_image_read_indexed_sub_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_indexed_sub_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_indexed_sub_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
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

TEST_CASE("compiles and rejects indexed native png inputs with out-of-range palette indexes") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_indexed_invalid_palette.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x03, 0x50, 0x4c, 0x54, 0x45,
        0x0a, 0x14, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x0d, 0x49, 0x44, 0x41, 0x54, 0x78,
        0x01, 0x01, 0x02, 0x00, 0xfd, 0xff, 0x00, 0x01,
        0x00, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
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
  const std::string srcPath = writeTemp("compile_native_image_read_indexed_invalid_palette_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_indexed_invalid_palette_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_indexed_invalid_palette_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("compiles and runs native png read for 2-bit indexed-color inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_indexed2.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
        0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x09, 0x50, 0x4c, 0x54, 0x45,
        0x00, 0x00, 0x00, 0x0a, 0x14, 0x1e, 0x28, 0x32,
        0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x0d, 0x49, 0x44, 0x41, 0x54, 0x78, 0x01, 0x01,
        0x02, 0x00, 0xfd, 0xff, 0x00, 0x18, 0x00, 0x1a,
        0x00, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00, 0x00,
        0x00, 0x00,
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
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_indexed2_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_indexed2_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_indexed2_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 4);
  CHECK(readFile(outPath) ==
        "3\n"
        "1\n"
        "9\n"
        "0\n"
        "0\n"
        "0\n"
        "10\n"
        "20\n"
        "30\n"
        "40\n"
        "50\n"
        "60\n");
}

TEST_CASE("compiles and rejects 1-bit indexed native png inputs with out-of-range palette indexes") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_indexed1_invalid_palette.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x03, 0x50, 0x4c, 0x54, 0x45,
        0x0a, 0x14, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x0d, 0x49, 0x44, 0x41, 0x54, 0x78,
        0x01, 0x01, 0x02, 0x00, 0xfd, 0xff, 0x00, 0x80,
        0x00, 0x82, 0x00, 0x81, 0x00, 0x00, 0x00, 0x00,
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
  const std::string srcPath = writeTemp("compile_native_image_read_indexed1_invalid_palette_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_indexed1_invalid_palette_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_indexed1_invalid_palette_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}


TEST_SUITE_END();
#endif
