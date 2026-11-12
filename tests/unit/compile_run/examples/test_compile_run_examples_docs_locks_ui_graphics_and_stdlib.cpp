#include "third_party/doctest.h"

#include "test_compile_run_examples_docs_locks_shared.h"

TEST_SUITE_BEGIN("primestruct.compile.run.examples");

TEST_CASE("image api docs and stdlib stay source locked") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path imageStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "image" / "image.prime";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(imageStdlibPath)) {
    imageStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "image" / "image.prime";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(imageStdlibPath));

  const std::string primeStructDoc = readPrimeStructSpec(primeStructPath.string());
  const std::string imageStdlib = readFile(imageStdlibPath.string());

  CHECK(primeStructDoc.find("the shared image file-I/O API currently lives under `/std/image/*`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ppm.read(width, height, pixels, path) -> Result<ImageError>`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ppm.write(path, width, height, pixels) -> Result<ImageError>`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`png.read(width, height, pixels, path) -> Result<ImageError>`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`png.write(path, width, height, pixels) -> Result<ImageError>`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`pixels` is a flat `vector<i32>` in RGB byte") != std::string::npos);
  CHECK(primeStructDoc.find("image file I/O follows `File<...>` behavior: `ppm.read(...)` and `png.read(...)`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("require `effects(file_read, heap_alloc)` because they reset/materialize the pixel buffer") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`png.write(...)` require `effects(file_write)`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`file_write` also implies `file_read` for compatibility") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ppm.read(...)` currently parses ASCII `P3` and binary `P6` PPM files in VM/native/Wasm") !=
        std::string::npos);
  CHECK(primeStructDoc.find("read contract now compiles through target validation") !=
        std::string::npos);
  CHECK(primeStructDoc.find("overflowed read-side size arithmetic") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ppm.write(...)` now emits ASCII `P3` PPM files in") !=
        std::string::npos);
  CHECK(primeStructDoc.find("invalid dimensions, payload mismatches, overflowed write-side size") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`png.read(...)` now validates PNG signatures/chunks,") !=
        std::string::npos);
  CHECK(primeStructDoc.find("current PNG read subset for both non-interlaced and Adam7-interlaced images:") !=
        std::string::npos);
  CHECK(primeStructDoc.find("The shared decoder accepts a single") !=
        std::string::npos);
  CHECK(primeStructDoc.find("dynamic-Huffman reads") !=
        std::string::npos);
  CHECK(primeStructDoc.find("reads materialize the public flat RGB buffer") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Malformed or missing PNGs,") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`png.write(...)` now emits non-interlaced 8-bit RGB PNG files in VM/native/Wasm") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ImageError.why()` currently returns") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ContainerError.missingKey()`, `ContainerError.indexOutOfBounds()`,") !=
        std::string::npos);
  CHECK(primeStructDoc.find("The image stdlib layer also defines `/ImageError/why([ImageError] err)` as the public wrapper over the") !=
        std::string::npos);
  CHECK(primeStructDoc.find("print_line(Result.why(ppm.read(width, height, pixels, \"input.ppm\"utf8)))") !=
        std::string::npos);
  CHECK(primeStructDoc.find("print_line(Result.why(png.write(\"output.png\"utf8, width, height, pixels)))") !=
        std::string::npos);

  const size_t resetReadOutputsStart = imageStdlib.find("resetReadOutputs(");
  const size_t imageRgbWritePixelCountStart =
      imageStdlib.find("\n  [return<bool>]\n  imageRgbWritePixelCount(", resetReadOutputsStart);
  REQUIRE(resetReadOutputsStart != std::string::npos);
  REQUIRE(imageRgbWritePixelCountStart != std::string::npos);
  REQUIRE(imageRgbWritePixelCountStart > resetReadOutputsStart);
  const std::string resetReadOutputsBody =
      imageStdlib.substr(resetReadOutputsStart, imageRgbWritePixelCountStart - resetReadOutputsStart);

  CHECK(imageStdlib.find("[public struct]\n  ImageError()") != std::string::npos);
  CHECK(imageStdlib.find("ImageError{1i32}") != std::string::npos);
  CHECK(imageStdlib.find("ImageError{2i32}") != std::string::npos);
  CHECK(imageStdlib.find("ImageError{3i32}") != std::string::npos);
  CHECK(imageStdlib.find("\"image_read_unsupported\"utf8") != std::string::npos);
  CHECK(imageStdlib.find("\"image_write_unsupported\"utf8") != std::string::npos);
  CHECK(imageStdlib.find("\"image_invalid_operation\"utf8") != std::string::npos);
  CHECK(imageStdlib.find("namespace ppm") != std::string::npos);
  CHECK(imageStdlib.find("namespace png") != std::string::npos);
  CHECK(imageStdlib.find("ppmReadAsciiInt") != std::string::npos);
  CHECK(imageStdlib.find("ppmReadBinaryRasterLead") != std::string::npos);
  CHECK(imageStdlib.find("imageRgbWritePixelCount") != std::string::npos);
  CHECK(imageStdlib.find("ppmWriteInputValid") != std::string::npos);
  CHECK(imageStdlib.find("ppmWriteHeader") != std::string::npos);
  CHECK(imageStdlib.find("ppmWriteComponent") != std::string::npos);
  CHECK(imageStdlib.find("pngInflateFixedHuffmanBlock") != std::string::npos);
  CHECK(imageStdlib.find("pngBuildFixedDistanceLengths") != std::string::npos);
  CHECK(imageStdlib.find("pngInflateCopyFromOutput") != std::string::npos);
  CHECK(imageStdlib.find("pngPackedSampleAt") != std::string::npos);
  CHECK(imageStdlib.find("pngScanlineBytesValid") != std::string::npos);
  CHECK(imageStdlib.find("pngScalePackedSampleToByte") != std::string::npos);
  CHECK(imageStdlib.find("pngScaleU16SampleToByte") != std::string::npos);
  CHECK(imageStdlib.find("The codec and deflate helpers below intentionally keep explicit") != std::string::npos);
  CHECK(imageStdlib.find("public-facing image wrapper layer\n  // above, which should prefer the readable surface syntax when possible.") !=
        std::string::npos);
  CHECK(resetReadOutputsBody.find("clear(pixels)") != std::string::npos);
  CHECK(resetReadOutputsBody.find("pixels.clear()") == std::string::npos);
  CHECK(imageStdlib.find("pngScanlineChannelByte") != std::string::npos);
  CHECK(imageStdlib.find("pngAdam7PassStartX") != std::string::npos);
  CHECK(imageStdlib.find("pngDecodeRows") != std::string::npos);
  CHECK(imageStdlib.find("pngChunkIsPlte(") != std::string::npos);
  CHECK(imageStdlib.find("pngChunkCrcMatches(") != std::string::npos);
  CHECK(imageStdlib.find("[public return<Result<ImageError>> effects(file_read, heap_alloc)]\n    read([i32 mut] width, [i32 mut] height, [vector<i32> mut] pixels, [string] path)") !=
        std::string::npos);
  CHECK(imageStdlib.find("[public return<Result<ImageError>> effects(file_write)]\n    write([string] path, [i32] width, [i32] height, [vector<i32>] pixels)") !=
        std::string::npos);

  const size_t ppmStart = imageStdlib.find("namespace ppm");
  const size_t pngStart = imageStdlib.find("namespace png");
  REQUIRE(ppmStart != std::string::npos);
  REQUIRE(pngStart != std::string::npos);
  REQUIRE(pngStart > ppmStart);
  const std::string ppmBody = imageStdlib.substr(ppmStart, pngStart - ppmStart);
  CHECK(ppmBody.find("readImpl(") != std::string::npos);
  CHECK(ppmBody.find("writeImpl(") != std::string::npos);
  CHECK(ppmBody.find("File<Read>(path)?") != std::string::npos);
  CHECK(ppmBody.find("File<Write>(path)?") != std::string::npos);
  CHECK(imageStdlib.find("if(err.code == 1i32)") != std::string::npos);
  CHECK(imageStdlib.find("if(err.code == 2i32)") != std::string::npos);
  CHECK(imageStdlib.find("return(\"image_invalid_operation\"utf8)") != std::string::npos);
  CHECK(imageStdlib.find("file.readByte(value)?") != std::string::npos);
  CHECK(imageStdlib.find("pixelCount{pixels.count()}") != std::string::npos);
  CHECK(imageStdlib.find("if(width <= 0i32 || height <= 0i32)") != std::string::npos);
  CHECK(imageStdlib.find("pixelCountWide{convert<i64>(width) * convert<i64>(height) * 3i64}") !=
        std::string::npos);
  CHECK(imageStdlib.find("if(pixelCountWide <= 0i64 || pixelCountWide > 2147483647i64)") !=
        std::string::npos);
  CHECK(imageStdlib.find("if(!imageRgbWritePixelCount(width, height, expectedPixelCount))") !=
        std::string::npos);
  CHECK(imageStdlib.find("if(pixelCount != expectedPixelCount)") != std::string::npos);
  CHECK(imageStdlib.find("while(index < pixelCount) {") != std::string::npos);
  CHECK(imageStdlib.find("if(component < 0i32 || component > 255i32)") != std::string::npos);
  CHECK(imageStdlib.find("++index") != std::string::npos);
  CHECK(ppmBody.find("return(invalidOperation())") != std::string::npos);
  CHECK(ppmBody.find("return(Result.ok())") != std::string::npos);
  CHECK(imageStdlib.find("return(value - 48i32)") != std::string::npos);
  CHECK(imageStdlib.find("value = pendingByte") != std::string::npos);
  CHECK(imageStdlib.find("value = value * 10i32 + ppmDigitValue(byte)") != std::string::npos);
  CHECK(imageStdlib.find("pixelCount = convert<i32>(pixelCountWide)") != std::string::npos);
  CHECK(ppmBody.find("status = ppmReadAsciiInt(file, hasPending, pendingByte, parsedWidth)") != std::string::npos);
  CHECK(ppmBody.find("status = ppmWriteComponent(file, pixels[index])") != std::string::npos);
  CHECK(ppmBody.find("if(status != 0i32)") != std::string::npos);
  CHECK(ppmBody.find("pixelCount{pixels.count()}") != std::string::npos);
  CHECK(ppmBody.find("assign(") == std::string::npos);
  CHECK(ppmBody.find("plus(") == std::string::npos);
  CHECK(ppmBody.find("minus(") == std::string::npos);
  CHECK(ppmBody.find("return(unsupported_write())") == std::string::npos);
  CHECK(ppmBody.find("file.read_byte(value)?") == std::string::npos);

  const std::string pngBody = imageStdlib.substr(pngStart);
  CHECK(pngBody.find("return(unsupported_read())") != std::string::npos);
  CHECK(imageStdlib.find("pngWriteByte([File<Write>] file, [i32] value)") != std::string::npos);
  CHECK(imageStdlib.find("if(value < 0i32 || value > 255i32)") != std::string::npos);
  CHECK(pngBody.find("if(status != 0i32 && status != 1000000i32)") != std::string::npos);
  CHECK(pngBody.find("if(status == 1000000i32)") != std::string::npos);
  CHECK(pngBody.find("if(status != 0i32)") != std::string::npos);
  CHECK(pngBody.find("file.write_byte(value)?") == std::string::npos);
  CHECK(pngBody.find("readImpl(") != std::string::npos);
  CHECK(pngBody.find("writeImpl(") != std::string::npos);
  CHECK(pngBody.find("pngValidateSignature") != std::string::npos);
  CHECK(pngBody.find("pngReadU32Be") != std::string::npos);
  CHECK(pngBody.find("pngReadChunkType") != std::string::npos);
  CHECK(pngBody.find("pngReadIhdr") != std::string::npos);
  CHECK(pngBody.find("pngInflateDeflateBlocks") != std::string::npos);
  CHECK(pngBody.find("pngDecodeScanlines") != std::string::npos);
  CHECK(pngBody.find("pngWriteSignature") != std::string::npos);
  CHECK(pngBody.find("pngWriteIdatChunk") != std::string::npos);
  CHECK(pngBody.find("pngWriteSizingValid") != std::string::npos);
  CHECK(pngBody.find("File<Read>(path)?") != std::string::npos);
  CHECK(pngBody.find("File<Write>(path)?") != std::string::npos);
  CHECK(pngBody.find("pngAppendBytes") != std::string::npos);
  const size_t pngPreludeStart = imageStdlib.find("pngReadU32Be(");
  const size_t pngDecodeStart = imageStdlib.find("pngPaethPredictor");
  REQUIRE(pngPreludeStart != std::string::npos);
  REQUIRE(pngDecodeStart != std::string::npos);
  REQUIRE(pngDecodeStart > pngPreludeStart);
  const std::string pngPreludeBody = imageStdlib.substr(pngPreludeStart, pngDecodeStart - pngPreludeStart);
  CHECK(pngPreludeBody.find("pngReadU32Be") != std::string::npos);
  CHECK(pngPreludeBody.find("pngReadIhdr") != std::string::npos);
  CHECK(pngPreludeBody.find("pngChunkCrc") != std::string::npos);
  CHECK(pngPreludeBody.find("pngWriteChunkOpen") != std::string::npos);
  CHECK(pngPreludeBody.find("pngWriteSizingValid") != std::string::npos);
  CHECK(pngPreludeBody.find("pngWriteIdatChunk") != std::string::npos);
  CHECK(pngPreludeBody.find("pngWriteIendChunk") != std::string::npos);
  CHECK(pngPreludeBody.find("value = value * 256i32 + byte") != std::string::npos);
  CHECK(pngPreludeBody.find("remaining = remaining - 1i32") != std::string::npos);
  CHECK(pngPreludeBody.find("width = parsedWidth") != std::string::npos);
  CHECK(pngPreludeBody.find("return(value - value / divisor * divisor)") != std::string::npos);
  CHECK(pngPreludeBody.find("crc = pngCrc32UpdateByte(crc, typeA)") != std::string::npos);
  CHECK(pngPreludeBody.find("return((rawByteCount + 65534i32) / 65535i32)") != std::string::npos);
  CHECK(pngPreludeBody.find("rawByteCount = convert<i32>(rawByteCountWide)") != std::string::npos);
  CHECK(pngPreludeBody.find("a = pngMod(a + byte, 65521i32)") != std::string::npos);
  CHECK(pngPreludeBody.find("pixelOffset = pixelOffset + 3i32") != std::string::npos);
  CHECK(pngPreludeBody.find("assign(") == std::string::npos);
  CHECK(pngPreludeBody.find("plus(") == std::string::npos);
  CHECK(pngPreludeBody.find("minus(") == std::string::npos);
  const size_t pngInflateStart = imageStdlib.find("pngZlibHeaderValid");
  REQUIRE(pngInflateStart != std::string::npos);
  REQUIRE(pngInflateStart > pngDecodeStart);
  const std::string pngScanlineBody = imageStdlib.substr(pngDecodeStart, pngInflateStart - pngDecodeStart);
  CHECK(pngScanlineBody.find("pngPaethPredictor") != std::string::npos);
  CHECK(pngScanlineBody.find("pngDecodeRows") != std::string::npos);
  CHECK(pngScanlineBody.find("predictor{left + up - upLeft}") != std::string::npos);
  CHECK(pngScanlineBody.find("return((pngColorTypeSamplesPerPixel(colorType) * bitDepth + 7i32) / 8i32)") != std::string::npos);
  CHECK(pngScanlineBody.find("scanlineBytes = 0i32") != std::string::npos);
  CHECK(pngScanlineBody.find("reconstructed = pngMod(reconstructed + leftByte, 256i32)") != std::string::npos);
  CHECK(pngScanlineBody.find("paletteBytes[paletteOffset + 1i32]") != std::string::npos);
  CHECK(pngScanlineBody.find("pixelByteIndex = pixelByteIndex + bytesPerPixel") != std::string::npos);
  CHECK(pngScanlineBody.find("offset = offset + scanlineBytes") != std::string::npos);
  CHECK(pngScanlineBody.find("assign(") == std::string::npos);
  CHECK(pngScanlineBody.find("plus(") == std::string::npos);
  CHECK(pngScanlineBody.find("minus(") == std::string::npos);
  const size_t pngInflateExecStart = imageStdlib.find("pngInflateCopyFromOutput");
  REQUIRE(pngInflateExecStart != std::string::npos);
  REQUIRE(pngInflateExecStart > pngInflateStart);
  const std::string pngBitstreamBody = imageStdlib.substr(pngInflateStart, pngInflateExecStart - pngInflateStart);
  CHECK(pngBitstreamBody.find("pngReadBits") != std::string::npos);
  CHECK(pngBitstreamBody.find("pngReadDynamicHuffmanLengths") != std::string::npos);
  CHECK(pngBitstreamBody.find("pngLengthInfo") != std::string::npos);
  CHECK(pngBitstreamBody.find("return(pngMod(cmf * 256i32 + flg, 31i32) == 0i32)") != std::string::npos);
  CHECK(pngBitstreamBody.find("value = value + pngMod(shifted, 2i32) * factor") != std::string::npos);
  CHECK(pngBitstreamBody.find("codeLengthLengths[codeLengthSymbol] = lengthValue") != std::string::npos);
  CHECK(pngBitstreamBody.find("totalCodeCount{literalCount + distanceCount}") != std::string::npos);
  CHECK(pngBitstreamBody.find("repeatCount{11i32 + repeatExtra}") != std::string::npos);
  CHECK(pngBitstreamBody.find("baseOut = 3i32 + (symbol - 257i32)") != std::string::npos);
  CHECK(pngBitstreamBody.find("if(symbol - currentSymbol == 1i32) {") != std::string::npos);
  CHECK(pngBitstreamBody.find("assign(") == std::string::npos);
  CHECK(pngBitstreamBody.find("plus(") == std::string::npos);
  CHECK(pngBitstreamBody.find("minus(") == std::string::npos);
  const size_t pngReadStart = imageStdlib.find("pngDecodeScanlines");
  REQUIRE(pngReadStart != std::string::npos);
  REQUIRE(pngReadStart > pngInflateExecStart);
  const std::string pngInflateExecBody = imageStdlib.substr(pngInflateExecStart, pngReadStart - pngInflateExecStart);
  CHECK(pngInflateExecBody.find("pngInflateStoredBlock") != std::string::npos);
  CHECK(pngInflateExecBody.find("pngInflateDynamicHuffmanBlock") != std::string::npos);
  CHECK(pngInflateExecBody.find("pngInflateDeflateBlocks") != std::string::npos);
  CHECK(pngInflateExecBody.find("output[count(output) - distance]") != std::string::npos);
  CHECK(pngInflateExecBody.find("trailerStart{count(compressed) - 4i32}") != std::string::npos);
  CHECK(pngInflateExecBody.find("byteIndex = byteIndex + 4i32") != std::string::npos);
  CHECK(pngInflateExecBody.find("matchLength{lengthBase + lengthExtraValue}") != std::string::npos);
  CHECK(pngInflateExecBody.find("code = code + bit * factor") != std::string::npos);
  CHECK(pngInflateExecBody.find("symbol = 280i32") != std::string::npos);
  CHECK(pngInflateExecBody.find("hclenBits + 4i32") != std::string::npos);
  CHECK(pngInflateExecBody.find("inflateStatus = pngInflateStoredBlock(compressed, byteIndex, bitIndex, output)") != std::string::npos);
  CHECK(pngInflateExecBody.find("if(compressedCount - byteIndex != 4i32) {") != std::string::npos);
  CHECK(pngInflateExecBody.find("assign(") == std::string::npos);
  CHECK(pngInflateExecBody.find("plus(") == std::string::npos);
  CHECK(pngInflateExecBody.find("minus(") == std::string::npos);
  const size_t pngWriteStart = imageStdlib.rfind(
      "[effects(file_write), return<int> on_error<FileError, ignoreFileError>]\n    writeImpl");
  REQUIRE(pngWriteStart != std::string::npos);
  REQUIRE(pngWriteStart > pngReadStart);
  const std::string pngReadBody = imageStdlib.substr(pngReadStart, pngWriteStart - pngReadStart);
  CHECK(pngReadBody.find("pixels[targetOffset] = passPixels[sourceOffset]") != std::string::npos);
  CHECK(pngReadBody.find("pixels[targetOffset + 1i32] = passPixels[sourceOffset + 1i32]") != std::string::npos);
  CHECK(pngReadBody.find("sawIhdr = 1i32") != std::string::npos);
  CHECK(pngReadBody.find("sawPlte = 1i32") != std::string::npos);
  CHECK(pngReadBody.find("sawPostIdatChunk = 1i32") != std::string::npos);
  CHECK(pngReadBody.find("sawIdat = 1i32") != std::string::npos);
  CHECK(pngReadBody.find("width = parsedWidth") != std::string::npos);
  CHECK(pngReadBody.find("height = parsedHeight") != std::string::npos);
  CHECK(pngReadBody.find("assign(") == std::string::npos);
  CHECK(pngReadBody.find("plus(") == std::string::npos);
  CHECK(pngReadBody.find("minus(") == std::string::npos);
  CHECK(pngBody.find("return(invalidOperation())") != std::string::npos);
  CHECK(pngBody.find("return(unsupported_write())") == std::string::npos);
}

TEST_CASE("file readByte docs and helpers stay source locked") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path lowererPath = std::filesystem::path("..") / "src" / "ir_lowerer" / "IrLowererFileWriteHelpers.cpp";
  std::filesystem::path fileStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "file" / "file.prime";
  std::filesystem::path fileErrorsPath = std::filesystem::path("..") / "stdlib" / "std" / "file" / "errors.prime";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(lowererPath)) {
    lowererPath = std::filesystem::current_path() / "src" / "ir_lowerer" / "IrLowererFileWriteHelpers.cpp";
  }
  if (!std::filesystem::exists(fileStdlibPath)) {
    fileStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "file" / "file.prime";
  }
  if (!std::filesystem::exists(fileErrorsPath)) {
    fileErrorsPath = std::filesystem::current_path() / "stdlib" / "std" / "file" / "errors.prime";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(lowererPath));
  REQUIRE(std::filesystem::exists(fileStdlibPath));
  REQUIRE(std::filesystem::exists(fileErrorsPath));

  const std::string primeStructDoc = readPrimeStructSpec(primeStructPath.string());
  const std::string lowerer = readFile(lowererPath.string());
  const std::string fileStdlib = readFile(fileStdlibPath.string());
  const std::string fileErrors = readFile(fileErrorsPath.string());

  CHECK(primeStructDoc.find("`readByte([i32 mut] value)`") != std::string::npos);
  CHECK(primeStructDoc.find("`readByte(...)` reports deterministic end-of-file as `EOF`") != std::string::npos);
  CHECK(primeStructDoc.find("`FileError.isEof(err)`") != std::string::npos);
  CHECK(primeStructDoc.find("`/File/openRead(...)`, `/File/openWrite(...)`, `/File/openAppend(...)`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Compatibility wrappers keep the older snake_case spellings") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`read_byte([i32 mut] value)`") == std::string::npos);
  CHECK(primeStructDoc.find("`FileError.is_eof(err)`") == std::string::npos);
  CHECK(primeStructDoc.find("read-only file operations require `effects(file_read)`") != std::string::npos);

  CHECK(fileStdlib.find("/File/openRead([string] path)") != std::string::npos);
  CHECK(fileStdlib.find("/File/open_read([string] path)") != std::string::npos);
  CHECK(fileStdlib.find("return(/File/openRead(path))") != std::string::npos);
  CHECK(fileStdlib.find("/File/readByte<Mode, T>([File<Mode>] self, [T mut] value)") != std::string::npos);
  CHECK(fileStdlib.find("/File/read_byte<Mode, T>([File<Mode>] self, [T mut] value)") != std::string::npos);
  CHECK(fileStdlib.find("return(/File/readByte(self, value))") != std::string::npos);
  CHECK(fileStdlib.find("/File/writeLine<Mode>([File<Mode>] self)") != std::string::npos);
  CHECK(fileStdlib.find("/File/write_line<Mode>([File<Mode>] self)") != std::string::npos);
  CHECK(fileStdlib.find("return(/File/writeLine(self))") != std::string::npos);
  CHECK(fileStdlib.find("/File/writeByte<Mode, T>([File<Mode>] self, [T] value)") != std::string::npos);
  CHECK(fileStdlib.find("return(/File/writeByte(self, value))") != std::string::npos);
  CHECK(fileStdlib.find("/File/writeBytes<Mode, T>([File<Mode>] self, [array<T>] bytes)") != std::string::npos);
  CHECK(fileStdlib.find("return(/File/writeBytes(self, bytes))") != std::string::npos);

  CHECK(fileErrors.find("isEof([FileError] err)") != std::string::npos);
  CHECK(fileErrors.find("return(/std/file/FileError/isEof(err))") != std::string::npos);


  CHECK(lowerer.find("read_byte requires exactly one argument") != std::string::npos);
  CHECK(lowerer.find("read_byte requires mutable integer binding") != std::string::npos);
  CHECK(lowerer.find("emitInstruction(IrOpcode::FileReadByte") != std::string::npos);
}

TEST_CASE("maybe stdlib control flow stays source locked to surface if syntax") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path maybeStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "maybe" / "maybe.prime";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(maybeStdlibPath)) {
    maybeStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "maybe" / "maybe.prime";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(maybeStdlibPath));

  const std::string primeStructDoc = readPrimeStructSpec(primeStructPath.string());
  const std::string maybeStdlib = readFile(maybeStdlibPath.string());

  CHECK(primeStructDoc.find("`isEmpty()` / `isSome()`") != std::string::npos);
  CHECK(primeStructDoc.find("compatibility wrappers `is_empty()` / `is_some()`") != std::string::npos);
  CHECK(primeStructDoc.find("**Helper surface (stdlib):** `is_empty()` / `is_some()`") == std::string::npos);
  CHECK(primeStructDoc.find("`Maybe<T>` is a stdlib-owned generic sum type") !=
        std::string::npos);
  CHECK(primeStructDoc.find("The old mutable struct helpers `set(value)`, `clear()`, and `take()`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("value.set(1i32) // error: sum-backed Maybe<T> has no mutable helper set") !=
        std::string::npos);
  CHECK(primeStructDoc.find("value.clear() // error: sum-backed Maybe<T> has no mutable helper clear") !=
        std::string::npos);
  CHECK(primeStructDoc.find("[i32] out{value.take()} // error: sum-backed Maybe<T> has no mutable helper take") !=
        std::string::npos);
  CHECK(maybeStdlib.find("[public sum]\n  Maybe<T> {\n    none\n    [T] some\n  }") !=
        std::string::npos);
  CHECK(maybeStdlib.find("[Maybe<T>] result{[some] value}") != std::string::npos);
  CHECK(maybeStdlib.find("[Maybe<T>] result{}") != std::string::npos);
  CHECK(maybeStdlib.find("return(result)") != std::string::npos);
  CHECK(maybeStdlib.find("/Maybe/isEmpty<T>([Maybe<T>] self)") != std::string::npos);
  CHECK(maybeStdlib.find("/Maybe/isSome<T>([Maybe<T>] self)") != std::string::npos);
  CHECK(maybeStdlib.find("/Maybe/is_empty<T>([Maybe<T>] self)") != std::string::npos);
  CHECK(maybeStdlib.find("/Maybe/is_some<T>([Maybe<T>] self)") != std::string::npos);
  CHECK(maybeStdlib.find("pick(self) {\n      none {\n        return(true)\n      }") !=
        std::string::npos);
  CHECK(maybeStdlib.find("pick(self) {\n      none {\n        return(false)\n      }") !=
        std::string::npos);
  CHECK(maybeStdlib.find("[public struct]") == std::string::npos);
  CHECK(maybeStdlib.find("uninitialized<T>") == std::string::npos);
  CHECK(maybeStdlib.find("drop(this.value)") == std::string::npos);
  CHECK(maybeStdlib.find("assign(this.empty, true)") == std::string::npos);
  CHECK(maybeStdlib.find("assign(this.empty, false)") == std::string::npos);
  CHECK(maybeStdlib.find("assign(ref.empty, false)") == std::string::npos);
  CHECK(maybeStdlib.find("this.empty = true") == std::string::npos);
  CHECK(maybeStdlib.find("this.empty = false") == std::string::npos);
  CHECK(maybeStdlib.find("ref.empty = false") == std::string::npos);
}

TEST_CASE("small stdlib wrappers stay source locked to inferred locals") {
  std::filesystem::path codeExamplesPath = std::filesystem::path("..") / "docs" / "CodeExamples.md";
  std::filesystem::path maybeStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "maybe" / "maybe.prime";
  std::filesystem::path vectorStdlibPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "vector.prime";
  std::filesystem::path collectionsStdlibPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "collections.prime";
  std::filesystem::path mapStdlibPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "map.prime";
  std::filesystem::path internalMapStdlibPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "internal_map.prime";
  std::filesystem::path experimentalVectorStdlibPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "experimental_vector.prime";
  std::filesystem::path internalVectorStdlibPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "internal_vector.prime";
  std::filesystem::path experimentalMapStdlibPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "experimental_map.prime";
  std::filesystem::path soaWrapperPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "soa_vector.prime";
  std::filesystem::path soaPublicPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "soa.prime";
  std::filesystem::path soaConversionsPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "soa_conversions.prime";
  std::filesystem::path internalSoaVectorPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "internal_soa.prime";
  std::filesystem::path internalSoaConversionsPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "internal_soa_conversions.prime";
  std::filesystem::path experimentalSoaVectorPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "experimental_soa.prime";
  std::filesystem::path experimentalSoaConversionsPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "experimental_soa_conversions.prime";
  if (!std::filesystem::exists(codeExamplesPath)) {
    codeExamplesPath = std::filesystem::current_path() / "docs" / "CodeExamples.md";
  }
  if (!std::filesystem::exists(maybeStdlibPath)) {
    maybeStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "maybe" / "maybe.prime";
  }
  if (!std::filesystem::exists(vectorStdlibPath)) {
    vectorStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "collections" / "vector.prime";
  }
  if (!std::filesystem::exists(collectionsStdlibPath)) {
    collectionsStdlibPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "collections.prime";
  }
  if (!std::filesystem::exists(mapStdlibPath)) {
    mapStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "collections" / "map.prime";
  }
  if (!std::filesystem::exists(internalMapStdlibPath)) {
    internalMapStdlibPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "internal_map.prime";
  }
  if (!std::filesystem::exists(experimentalVectorStdlibPath)) {
    experimentalVectorStdlibPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "experimental_vector.prime";
  }
  if (!std::filesystem::exists(internalVectorStdlibPath)) {
    internalVectorStdlibPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "internal_vector.prime";
  }
  if (!std::filesystem::exists(experimentalMapStdlibPath)) {
    experimentalMapStdlibPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "experimental_map.prime";
  }
  if (!std::filesystem::exists(soaWrapperPath)) {
    soaWrapperPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "soa_vector.prime";
  }
  if (!std::filesystem::exists(soaPublicPath)) {
    soaPublicPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "soa.prime";
  }
  if (!std::filesystem::exists(soaConversionsPath)) {
    soaConversionsPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "soa_conversions.prime";
  }
  if (!std::filesystem::exists(internalSoaVectorPath)) {
    internalSoaVectorPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "internal_soa.prime";
  }
  if (!std::filesystem::exists(internalSoaConversionsPath)) {
    internalSoaConversionsPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" /
        "internal_soa_conversions.prime";
  }
  if (!std::filesystem::exists(experimentalSoaVectorPath)) {
    experimentalSoaVectorPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "experimental_soa.prime";
  }
  if (!std::filesystem::exists(experimentalSoaConversionsPath)) {
    experimentalSoaConversionsPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" /
        "experimental_soa_conversions.prime";
  }
  REQUIRE(std::filesystem::exists(codeExamplesPath));
  REQUIRE(std::filesystem::exists(maybeStdlibPath));
  REQUIRE(std::filesystem::exists(vectorStdlibPath));
  CHECK(!std::filesystem::exists(collectionsStdlibPath));
  REQUIRE(std::filesystem::exists(mapStdlibPath));
  CHECK(!std::filesystem::exists(internalMapStdlibPath));
  CHECK(!std::filesystem::exists(experimentalVectorStdlibPath));
  CHECK(!std::filesystem::exists(internalVectorStdlibPath));
  CHECK(!std::filesystem::exists(experimentalMapStdlibPath));
  CHECK(!std::filesystem::exists(soaWrapperPath));
  REQUIRE(std::filesystem::exists(soaPublicPath));
  CHECK(!std::filesystem::exists(soaConversionsPath));
  // internal_soa*, experimental_soa* merged into soa.prime (TODO-4633)
  CHECK(!std::filesystem::exists(internalSoaVectorPath));
  CHECK(!std::filesystem::exists(internalSoaConversionsPath));
  CHECK(!std::filesystem::exists(experimentalSoaVectorPath));
  CHECK(!std::filesystem::exists(experimentalSoaConversionsPath));

  const std::string codeExamples = readFile(codeExamplesPath.string());
  const std::string maybeStdlib = readFile(maybeStdlibPath.string());
  const std::string vectorStdlib = readFile(vectorStdlibPath.string());
  const std::string mapStdlib = readFile(mapStdlibPath.string());
  // internal_map.prime and internal_vector.prime were merged into their public modules (TODO-4631, TODO-4632)
  const std::string internalMapStdlib = "";
  const std::string internalVectorStdlib = "";
  const std::string soaPublic = readFile(soaPublicPath.string());
  // internal_soa*, experimental_soa* merged into soa.prime (TODO-4633)
  const std::string internalSoaVector = "";
  const std::string internalSoaConversions = "";
  const std::string experimentalSoaVector = "";
  const std::string experimentalSoaConversions = "";

  CHECK(codeExamples.find("Internal implementation, bridge, or substrate-oriented code:") !=
        std::string::npos);
  CHECK(codeExamples.find("Intentionally canonical or substrate-oriented code:") == std::string::npos);
  CHECK(codeExamples.find("[mut] current{start}") != std::string::npos);
  CHECK(codeExamples.find("limit{5}") != std::string::npos);
  CHECK(codeExamples.find("return(counter.doubled() + Counter.defaultStep())") !=
        std::string::npos);
  CHECK(codeExamples.find("return(counter.doubled() + /Counter/defaultStep())") ==
        std::string::npos);
  CHECK(codeExamples.find("preferred type-qualified dot-call form for static\nhelpers together.") !=
        std::string::npos);
  CHECK(codeExamples.find("prefer `left == right` at the call site and\nkeep `/Type/Equal(left, right)` as the helper contract underneath.") !=
        std::string::npos);
  CHECK(codeExamples.find("return(left == right)") != std::string::npos);

  CHECK(maybeStdlib.find("[Maybe<T>] result{[some] value}") != std::string::npos);
  CHECK(maybeStdlib.find("[Maybe<T>] result{}") != std::string::npos);
  CHECK(maybeStdlib.find("return(result)") != std::string::npos);
  CHECK(maybeStdlib.find("some(value) {\n        return(true)\n      }") != std::string::npos);
  CHECK(maybeStdlib.find("out{take(this.value)}") == std::string::npos);
  CHECK(maybeStdlib.find("[mut] out{Maybe<T>{}}") == std::string::npos);
  CHECK(maybeStdlib.find("[mut] ref{location(out)}") == std::string::npos);
  CHECK(maybeStdlib.find("[T] out{take(this.value)}") == std::string::npos);
  CHECK(maybeStdlib.find("[Maybe<T> mut] out{Maybe<T>{}}") == std::string::npos);
  CHECK(maybeStdlib.find("[Reference<Maybe<T>> mut] ref{location(out)}") == std::string::npos);

  CHECK(vectorStdlib.find(
            "// Canonical vector module with merged implementation.") !=
        std::string::npos);
  CHECK(vectorStdlib.find("import /std/collections/buffer_checked/*") != std::string::npos);
  // vector<T> constructor uses alloc+initSlot pattern with typed locals (merged from internal_vector)
  CHECK(vectorStdlib.find("[i32] valueCount{values.count()}") != std::string::npos);
  CHECK(vectorStdlib.find("[i32 mut] index{0i32}") != std::string::npos);
  CHECK(vectorStdlib.find("vectorAlloc<T>(valueCount, valueCount)") != std::string::npos);
  CHECK(vectorStdlib.find("vectorInitSlot<T>(out, index, /at(values, index))") != std::string::npos);
  // Internal helpers defined directly; short-named push/count/at wrappers removed in merge (TODO-4632)
  CHECK(vectorStdlib.find("vectorCount<T>([Vector<T>] values)") != std::string::npos);
  CHECK(vectorStdlib.find("vectorPush<T>([Vector<T> mut] values, [T move] value)") != std::string::npos);
  CHECK(vectorStdlib.find("vectorAt<T>([Vector<T>] values, [i32] index)") != std::string::npos);
  // Old short-named wrapper call expressions gone (compiler manifest handles dispatch)
  CHECK(vectorStdlib.find("/std/collections/vector/push<T>(result, /at(values, index))") ==
        std::string::npos);
  CHECK(vectorStdlib.find("/std/collections/vector/push<T>(result, first)") == std::string::npos);
  CHECK(vectorStdlib.find("/std/collections/vector/push<T>(result, second)") == std::string::npos);
  // No experimental_vector references
  CHECK(vectorStdlib.find("/std/collections/experimental_vector/") == std::string::npos);
  CHECK(vectorStdlib.find("/std/collections/experimental_vector/vectorPair<T>(first, second)") ==
        std::string::npos);
  CHECK(vectorStdlib.find("/std/collections/experimental_vector/vectorPush<T>(out, values[index])") ==
        std::string::npos);
  // No old untyped inferred-local forms (replaced by typed merged implementation)
  CHECK(vectorStdlib.find("[mut] result{/std/collections/vector/vector<T>()}") == std::string::npos);
  CHECK(vectorStdlib.find("[mut] index{0i32}") == std::string::npos);
  CHECK(vectorStdlib.find("[mut] out{/std/collections/vector/vector<T>()}") == std::string::npos);
  CHECK(vectorStdlib.find("[Vector<T> mut] out{/std/collections/vector/vector<T>()}") ==
        std::string::npos);

  CHECK(mapStdlib.find(
            "// Standalone canonical stdlib-owned map implementation.") !=
        std::string::npos);
  CHECK(mapStdlib.find("import /std/collections/vector/*") !=
        std::string::npos);
  CHECK(mapStdlib.find("import /std/collections/internal_map") == std::string::npos);
  CHECK(mapStdlib.find("import /std/collections/experimental_map") == std::string::npos);
  CHECK(mapStdlib.find("import /std/collections/map2") == std::string::npos);
  CHECK(mapStdlib.find("/std/collections/map2/") == std::string::npos);
  CHECK(mapStdlib.find("[MapValue<K, V> mut] out{mapNew<K, V>()}") !=
        std::string::npos);
  CHECK(mapStdlib.find("[args<Entry<K, V>>] entries") != std::string::npos);
  CHECK(mapStdlib.find("[Entry<K, V>] current{entries[index]}") ==
        std::string::npos);
  // TODO-4683: the 8-signature pair constructor ladder (map(k, v, ...) up
  // to eight pairs) was deleted; surface calls now rewrite to the single
  // variadic entries constructor (map<K, V>([args<Entry<K, V>>] entries))
  // at monomorph time instead.
  CHECK(mapStdlib.find("[K] eighthKey, [V] eighthValue") == std::string::npos);
  CHECK(mapStdlib.find("/std/collections/mapSingle") == std::string::npos);
  CHECK(mapStdlib.find("/std/collections/mapPair") == std::string::npos);
  CHECK(mapStdlib.find("mapCount<K, V>") != std::string::npos);
  CHECK(mapStdlib.find("[MapValue<K, V> mut] values") == std::string::npos);
  CHECK(mapStdlib.find("[map<K, V> mut] out{/std/collections/map/mapNew<K, V>()}") ==
        std::string::npos);
  CHECK(mapStdlib.find("/std/collections/map/mapNew<K, V>()") ==
        std::string::npos);

  // internal_vector.prime merged into vector.prime (TODO-4631): verify merged content
  CHECK(vectorStdlib.find("[public struct collection_type sendable]\n  Vector<T>()") != std::string::npos);
  // internal_map.prime merged into map.prime (TODO-4632): verify merged content
  CHECK(mapStdlib.find("[Vector<K>] keys{this.keys}") != std::string::npos);
  CHECK(mapStdlib.find("return(vectorCount<K>(keys))") != std::string::npos);

  // soa.prime merged from internal_soa*, experimental_soa* (TODO-4633)
  CHECK(soaPublic.find("// Canonical standalone SoA module with merged implementation.") !=
        std::string::npos);
  CHECK(soaPublic.find("import /std/collections/soa_storage/*") !=
        std::string::npos);
  CHECK(soaPublic.find("import /std/collections/internal_soa") ==
        std::string::npos);
  CHECK(soaPublic.find("import /std/collections/experimental_soa/*") ==
        std::string::npos);
  CHECK(soaPublic.find("/std/collections/soa/soa<T>([args<T>] values)") !=
        std::string::npos);
  CHECK(soaPublic.find("/std/collections/soa/soaVectorPush<T>(out, /at(values, index))") !=
        std::string::npos);
  CHECK(soaPublic.find("/std/collections/soa/single<T>([T move] value)") !=
        std::string::npos);
  CHECK(soaPublic.find("/std/collections/soa/from_aos<T>([vector<T>] values)") !=
        std::string::npos);
  CHECK(soaPublic.find("/std/collections/soa/count<T>([SoaVector<T>] values)") !=
        std::string::npos);
  CHECK(soaPublic.find("/std/collections/soa/get<T>([SoaVector<T>] values, [i32] index)") !=
        std::string::npos);
  CHECK(soaPublic.find("/std/collections/soa/ref<T>([SoaVector<T>] values, [i32] index)") !=
        std::string::npos);
  CHECK(soaPublic.find("/std/collections/soa/reserve<T>([SoaVector<T> mut] values, [i32] capacity)") !=
        std::string::npos);
  CHECK(soaPublic.find("/std/collections/soa/push<T>([SoaVector<T> mut] values, [T move] value)") !=
        std::string::npos);
  CHECK(soaPublic.find("/std/collections/soa/to_aos<T>([SoaVector<T>] values)") !=
        std::string::npos);
  CHECK(soaPublic.find("return(/std/collections/soa/soaVectorToAos<T>(values))") !=
        std::string::npos);

  // internal_soa*, internal_soa_conversions*, experimental_soa*
  // all merged into soa.prime (TODO-4633): verify these old paths are absent from soa.prime
  CHECK(soaPublic.find("namespace internal_soa") == std::string::npos);
  CHECK(soaPublic.find("namespace experimental_soa") == std::string::npos);
  CHECK(soaPublic.find("/std/collections/internal_soa/") == std::string::npos);
  CHECK(soaPublic.find("/std/collections/experimental_soa/") == std::string::npos);
  // verify AoS conversion helpers are now directly in soa.prime
  CHECK(soaPublic.find("soaVectorToAos<T>([SoaVector<T>] values)") != std::string::npos);
  CHECK(soaPublic.find("soaVectorToAosRef<T>([Reference<SoaVector<T>>] values)") != std::string::npos);
  CHECK(soaPublic.find("valueCount{vectorCount<T>(values)}") != std::string::npos);
  CHECK(soaPublic.find("return(/std/collections/soa/soaVectorGet<T>(values, index))") != std::string::npos);
  CHECK(soaPublic.find("return(/std/collections/soa/soaVectorCountRef<T>(values))") != std::string::npos);
  CHECK(soaPublic.find("return(/std/collections/soa/soaVectorGetRef<T>(values, index))") != std::string::npos);
  // internalSoaVector, internalSoaConversions, experimentalSoaVector, experimentalSoaConversions
  // are all empty strings after TODO-4633 merge
  CHECK(internalSoaVector.empty());
  CHECK(internalSoaConversions.empty());
  CHECK(experimentalSoaVector.empty());
  CHECK(experimentalSoaConversions.empty());
}

TEST_CASE("surface examples stay source locked to lowering-compatible helper forms") {
  std::filesystem::path featuresOverviewPath =
      std::filesystem::path("..") / "examples" / "3.Surface" / "features_overview.prime";
  std::filesystem::path raytracerPath =
      std::filesystem::path("..") / "examples" / "3.Surface" / "raytracer.prime";
  if (!std::filesystem::exists(featuresOverviewPath)) {
    featuresOverviewPath =
        std::filesystem::current_path() / "examples" / "3.Surface" / "features_overview.prime";
  }
  if (!std::filesystem::exists(raytracerPath)) {
    raytracerPath = std::filesystem::current_path() / "examples" / "3.Surface" / "raytracer.prime";
  }
  REQUIRE(std::filesystem::exists(featuresOverviewPath));
  REQUIRE(std::filesystem::exists(raytracerPath));

  const std::string featuresOverview = readFile(featuresOverviewPath.string());
  const std::string raytracer = readFile(raytracerPath.string());

  CHECK(featuresOverview.find("/std/collections/vector/at(scores, idx)") != std::string::npos);
  CHECK(featuresOverview.find("scores.at(idx)") == std::string::npos);
  CHECK(featuresOverview.find("scores[idx]") == std::string::npos);
  CHECK(featuresOverview.find("if(value > best)") != std::string::npos);
  CHECK(featuresOverview.find("best = value") != std::string::npos);
  CHECK(featuresOverview.find("best = max(best, value)") == std::string::npos);

  CHECK(raytracer.find("[ColorRGB] clamped{") != std::string::npos);
  CHECK(raytracer.find("min(1.0, max(0.0, scaled.r))") != std::string::npos);
  CHECK(raytracer.find("min(1.0, max(0.0, scaled.g))") != std::string::npos);
  CHECK(raytracer.find("min(1.0, max(0.0, scaled.b))") != std::string::npos);
  CHECK(raytracer.find("[Vec3] refrVec{refract_dir(currentDir, hitNormal, ior)}") != std::string::npos);
  CHECK(raytracer.find("clamp(scaled.r, 0.0, 1.0)") == std::string::npos);
  CHECK(raytracer.find("return(scaled.clamp(0.0, 1.0))") == std::string::npos);
  CHECK(raytracer.find("currentDir.refract(hitNormal, ior)") == std::string::npos);
}

TEST_CASE("gfx stdlib compatibility shim stays source locked") {
  std::filesystem::path gfxStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "gfx" / "gfx.prime";
  std::filesystem::path gfxExperimentalPath =
      std::filesystem::path("..") / "stdlib" / "std" / "gfx" / "experimental.prime";
  if (!std::filesystem::exists(gfxStdlibPath)) {
    gfxStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "gfx" / "gfx.prime";
  }
  if (!std::filesystem::exists(gfxExperimentalPath)) {
    gfxExperimentalPath = std::filesystem::current_path() / "stdlib" / "std" / "gfx" / "experimental.prime";
  }
  REQUIRE(std::filesystem::exists(gfxStdlibPath));
  REQUIRE(std::filesystem::exists(gfxExperimentalPath));

  const std::string gfxStdlib = readFile(gfxStdlibPath.string());
  const std::string gfxExperimental = readFile(gfxExperimentalPath.string());

  CHECK(gfxExperimental.find("// Legacy compatibility shim over canonical /std/gfx/*.") !=
        std::string::npos);
  CHECK(gfxExperimental.find("New public gfx code should import /std/gfx/*; this namespace remains only") !=
        std::string::npos);
  CHECK(gfxStdlib.find("return(Queue{[token] this.token + 1i32})") != std::string::npos);
  CHECK(gfxStdlib.find("return(this.token > 0i32)") != std::string::npos);
  CHECK(gfxStdlib.find("if(this.height < 1i32)") != std::string::npos);
  CHECK(gfxStdlib.find("if(this.token < 1i32 || window.token < 1i32)") != std::string::npos);
  CHECK(gfxStdlib.find("if(this.token < 1i32 || vertexCount < 1i32 || indexCount < 1i32)") !=
        std::string::npos);
  CHECK(gfxStdlib.find("if(this.token < 1i32)") != std::string::npos);
  CHECK(gfxStdlib.find("if(drawToken == 0i32)") != std::string::npos);
  CHECK(gfxStdlib.find("if(endToken == 0i32)") != std::string::npos);
  CHECK(gfxStdlib.find("return(this.elementCount < 1i32)") != std::string::npos);
  CHECK(gfxStdlib.find("[swapchainToken] this.token + window.token + 1i32") != std::string::npos);
  CHECK(gfxStdlib.find("[meshToken] this.token + vertexCount + indexCount") != std::string::npos);
  CHECK(gfxStdlib.find("[pipelineToken] shader.value + this.token + 5i32") != std::string::npos);
  CHECK(gfxStdlib.find("[drawToken] this.token + mesh.token + material.token") != std::string::npos);
  CHECK(gfxStdlib.find("if(queueToken != deviceToken + 1i32) {") != std::string::npos);
  CHECK(gfxStdlib.find("return(less_than(0i32, this.token))") == std::string::npos);
  CHECK(gfxStdlib.find("if(less_than(this.height, 1i32))") == std::string::npos);
  CHECK(gfxStdlib.find("if(or(less_than(this.token, 1i32), less_than(window.token, 1i32)))") ==
        std::string::npos);
  CHECK(gfxStdlib.find("if(or(less_than(this.token, 1i32),\n"
                       "            or(less_than(vertexCount, 1i32), less_than(indexCount, 1i32))))") ==
        std::string::npos);
  CHECK(gfxStdlib.find("if(equal(drawToken, 0i32))") == std::string::npos);
  CHECK(gfxStdlib.find("if(equal(endToken, 0i32))") == std::string::npos);
  CHECK(gfxStdlib.find("plus(") == std::string::npos);
  CHECK(gfxExperimental.find("import /std/gfx/*") != std::string::npos);
  CHECK(gfxExperimental.find("targeted compatibility coverage and staged migration support.") != std::string::npos);
  CHECK(gfxExperimental.find("Route behavior through the canonical helper surface whenever the old type") !=
        std::string::npos);
  CHECK(gfxExperimental.find("return(greater_than(this.token, 0i32))") != std::string::npos);
  CHECK(gfxExperimental.find("return(Queue{[token] plus(this.token, 1i32)})") != std::string::npos);
  CHECK(gfxExperimental.find("return(RenderPass{[token] renderPassToken})") != std::string::npos);
  CHECK(gfxExperimental.find("[SubstrateDrawMeshConfig] config{\n        [renderPass] this,") !=
        std::string::npos);
  CHECK(gfxExperimental.find("SubstrateDrawMeshConfig{") == std::string::npos);
  CHECK(gfxExperimental.find("return(/std/gfx/Buffer/readback<T>(canonical))") !=
        std::string::npos);
  CHECK(gfxExperimental.find("[/std/gfx/Buffer<T>] canonical{/std/gfx/Buffer/upload<T>(values)}") !=
        std::string::npos);
  CHECK(gfxExperimental.find("[Buffer<T>] result{[token] canonical.token, [elementCount] canonical.elementCount}") !=
        std::string::npos);
  CHECK(gfxExperimental.find("return(canonicalWindow(this).is_open())") == std::string::npos);
  CHECK(gfxExperimental.find("return(experimentalQueue(canonicalDevice(this).default_queue()))") ==
        std::string::npos);
  CHECK(gfxExperimental.find("return(experimentalRenderPass(canonicalFrame(this).render_pass(clear_color, clear_depth)))") ==
        std::string::npos);
  CHECK(gfxExperimental.find("canonicalRenderPass(this).draw_mesh(canonicalMesh(mesh), canonicalMaterial(material))") ==
        std::string::npos);
  CHECK(gfxExperimental.find("return(/std/gfx/Buffer/readback<T>(canonicalBuffer<T>(self)))") ==
        std::string::npos);
  CHECK(gfxExperimental.find("return(experimentalBuffer<T>(/std/gfx/Buffer/upload<T>(values)))") ==
        std::string::npos);
  CHECK(gfxExperimental.find("return(/std/gfx/GfxError/why(canonicalGfxError(err)))") ==
        std::string::npos);
  CHECK(gfxExperimental.find("return(/std/gpu/readback(self))") == std::string::npos);
  CHECK(gfxExperimental.find("return(/std/gpu/upload(values))") == std::string::npos);
  CHECK(gfxExperimental.find("return(less_than(0i32, this.token))") == std::string::npos);
}

TEST_SUITE_END();
