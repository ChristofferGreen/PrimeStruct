#include "test_compile_run_helpers.h"

#include "test_compile_run_native_backend_core_helpers.h"

#if PRIMESTRUCT_NATIVE_CORE_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.core");

TEST_CASE("compiles and runs native png read for sub-filter grayscale-alpha inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_gray_alpha_sub.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x10, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x01, 0x01, 0x05, 0x00, 0xfa, 0xff,
        0x01, 0x0a, 0x5a, 0x14, 0x05, 0x01, 0x6d, 0x00,
        0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
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
  const std::string srcPath = writeTemp("compile_native_image_read_gray_alpha_sub_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_gray_alpha_sub_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_gray_alpha_sub_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "10\n"
        "10\n"
        "10\n"
        "30\n"
        "30\n"
        "30\n");
}

TEST_CASE("compiles and runs native png read for 1-bit grayscale inputs") {
  const std::string inPath = (testScratchPath("") / "primec_native_image_read_gray1.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x01, 0x01, 0x02, 0x00, 0xfd, 0xff,
        0x00, 0x40, 0x00, 0x42, 0x00, 0x41, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
        0x4e, 0x44, 0x00, 0x00, 0x00, 0x00,
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
  const std::string srcPath = writeTemp("compile_native_image_read_gray1_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_gray1_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_gray1_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "0\n"
        "0\n"
        "0\n"
        "255\n"
        "255\n"
        "255\n");
}

TEST_CASE("compiles and runs native png read for 4-bit grayscale inputs") {
  const std::string inPath = (testScratchPath("") / "primec_native_image_read_gray4.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x01, 0x01, 0x02, 0x00, 0xfd, 0xff,
        0x00, 0x1e, 0x00, 0x20, 0x00, 0x1f, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
        0x4e, 0x44, 0x00, 0x00, 0x00, 0x00,
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
  const std::string srcPath = writeTemp("compile_native_image_read_gray4_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_gray4_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_gray4_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "17\n"
        "17\n"
        "17\n"
        "238\n"
        "238\n"
        "238\n");
}

TEST_CASE("compiles and runs native png read for 16-bit grayscale inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_gray16_sub.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x10, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x01, 0x01, 0x05, 0x00, 0xfa, 0xff,
        0x01, 0x12, 0x34, 0x6e, 0x4b, 0x02, 0x15, 0x01,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
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
  const std::string srcPath = writeTemp("compile_native_image_read_gray16_sub_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_gray16_sub_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_gray16_sub_png.txt").string();

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
        "127\n"
        "127\n"
        "127\n");
}

TEST_CASE("compiles and runs native png read for stored sub-filter rgb inputs") {
  const std::string inPath = (testScratchPath("") / "primec_native_image_read_sub.png").string();
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
  const std::string srcPath = writeTemp("compile_native_image_read_sub_png.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_image_read_sub_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_sub_png.txt").string();

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

TEST_CASE("compiles and runs native png read for stored up-filter rgb inputs") {
  const std::string inPath = (testScratchPath("") / "primec_native_image_read_up.png").string();
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
  const std::string srcPath = writeTemp("compile_native_image_read_up_png.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_image_read_up_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_up_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
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

TEST_CASE("compiles and runs native png read for stored average-filter rgb inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_average.png").string();
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
  const std::string srcPath = writeTemp("compile_native_image_read_average_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_average_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_average_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
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

TEST_CASE("compiles and runs native png read for stored paeth-filter rgb inputs") {
  const std::string inPath = (testScratchPath("") / "primec_native_image_read_paeth.png").string();
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
  const std::string srcPath = writeTemp("compile_native_image_read_paeth_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_paeth_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_paeth_png.txt").string();

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


TEST_SUITE_END();
#endif
