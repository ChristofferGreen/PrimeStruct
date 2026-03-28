#pragma once

TEST_CASE("runs vm png read for stored rgba inputs deterministically") {
  const std::string inPath = (testScratchPath("") / "primec_vm_image_read.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x10, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x05, 0x00, 0xfa, 0xff, 0x00,
        0x00, 0xff, 0x80, 0x40, 0x04, 0x42, 0x01, 0xc0,
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
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("vm_image_read_png.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_image_read_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath) ==
        "1\n"
        "1\n"
        "3\n"
        "0\n"
        "255\n"
        "128\n");
}

TEST_CASE("runs vm png read for stored sub-filter rgba inputs deterministically") {
  const std::string inPath = (testScratchPath("") / "primec_vm_image_read_sub.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x14, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x09, 0x00, 0xf6, 0xff, 0x01,
        0x0a, 0x14, 0x1e, 0x28, 0x05, 0x07, 0x09, 0x0b,
        0x02, 0xb0, 0x00, 0x86, 0x00, 0x00, 0x00, 0x00,
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
  const std::string srcPath = writeTemp("vm_image_read_sub_png.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_image_read_sub_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "10\n"
        "20\n"
        "30\n"
        "15\n"
        "27\n"
        "39\n");
}

TEST_CASE("runs vm png read for stored up-filter rgba inputs deterministically") {
  const std::string inPath = (testScratchPath("") / "primec_vm_image_read_up.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x15, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x0a, 0x00, 0xf5, 0xff, 0x00,
        0x0a, 0x14, 0x1e, 0xff, 0x02, 0x05, 0x07, 0x09,
        0x00, 0x08, 0x15, 0x01, 0x53, 0x00, 0x00, 0x00,
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
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("vm_image_read_up_png.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_image_read_up_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 3);
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

TEST_CASE("runs vm png read for stored average-filter rgba inputs deterministically") {
  const std::string inPath =
      (testScratchPath("") / "primec_vm_image_read_average.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x15, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x0a, 0x00, 0xf5, 0xff, 0x00,
        0x0a, 0x14, 0x1e, 0xff, 0x03, 0x0a, 0x11, 0x18,
        0x80, 0x08, 0xea, 0x01, 0xf2, 0x00, 0x00, 0x00,
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
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("vm_image_read_average_png.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_read_average_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 3);
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

TEST_CASE("runs vm png read for fixed-huffman backreference rgba inputs deterministically") {
  const std::string inPath = (testScratchPath("") / "primec_vm_image_read_fixed_sub.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0e, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x63, 0xe0, 0x12, 0x91, 0xd3, 0x80,
        0x61, 0x00, 0x07, 0x15, 0x01, 0x2d, 0x00, 0x00,
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
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("vm_image_read_fixed_sub_png.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_read_fixed_sub_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 4);
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

TEST_CASE("rejects vm png read for dynamic-huffman literal rgb inputs deterministically") {
  const std::string inPath =
      (testScratchPath("") / "primec_vm_image_read_dynamic_literal.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x19, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x05, 0xc0, 0xa1, 0x00, 0x00, 0x00,
        0x00, 0x03, 0xb0, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7,
        0xe7, 0xfb, 0x73, 0x7d, 0xa0, 0x9c, 0xee, 0x02,
        0x37, 0x00, 0xd3, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x49, 0x45, 0x4e, 0x44, 0x00, 0x00, 0x00, 0x00,
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
  const std::string srcPath = writeTemp("vm_image_read_dynamic_literal_png.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_read_dynamic_literal_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 1);
  CHECK(readFile(outPath) == "image_invalid_operation\n");
}

TEST_CASE("runs vm png read for dynamic-huffman backreference rgba inputs deterministically") {
  const std::string inPath =
      (testScratchPath("") / "primec_vm_image_read_dynamic_backref.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x1e, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x35, 0xc3, 0xa1, 0x00, 0x00, 0x00,
        0x0c, 0xc0, 0x80, 0xff, 0xff, 0xff, 0xff, 0xf9,
        0xf9, 0xfe, 0x96, 0x7f, 0xfa, 0xb6, 0x41, 0xf5,
        0xec, 0x7f, 0x13, 0xae, 0x03, 0xb2, 0x00, 0x00,
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
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("vm_image_read_dynamic_backref_png.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_read_dynamic_backref_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 4);
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

TEST_CASE("rejects malformed vm png inputs deterministically") {
  const std::string inPath = (testScratchPath("") / "primec_vm_image_read_invalid.png").string();
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
  const std::string srcPath = writeTemp("vm_image_read_invalid_png.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_read_invalid_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("rejects vm png inputs with critical chunk crc mismatches deterministically") {
  const std::string inPath =
      (testScratchPath("") / "primec_vm_image_read_invalid_crc.png").string();
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
  const std::string srcPath = writeTemp("vm_image_read_invalid_crc_png.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_read_invalid_crc_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("rejects vm png inputs with non-consecutive idat chunks deterministically") {
  const std::string inPath =
      (testScratchPath("") / "primec_vm_image_read_invalid_idat_order.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x08, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x04, 0x00, 0xfb, 0xff, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x72, 0x75, 0x53, 0x74,
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
  const std::string srcPath = writeTemp("vm_image_read_invalid_idat_order_png.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_read_invalid_idat_order_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

