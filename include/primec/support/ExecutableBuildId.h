#pragma once

#include <string>
#include <string_view>

namespace primec {

// The build identifier a linker stamps into an executable: `LC_UUID` of a thin 64-bit Mach-O file, or the
// `NT_GNU_BUILD_ID` note (name "GNU") of an ELF file's `PT_NOTE` segments. It is 8 or more bytes the linker derives
// from the content of the output, so it changes whenever the code does. Returns the bytes as lowercase hex, or an
// empty string when the image is of another format (fat Mach-O, PE, anything else) or carries no such identifier
// (`-no_uuid`, a link without `--build-id`). Only the headers are read, never the whole file.
std::string linkerBuildIdOfBytes(std::string_view image);
std::string linkerBuildIdOfFile(const std::string &path);

// How `executableBuildId` may identify an executable.
enum class BuildIdSource {
  LinkerIdFirst, // the linker identifier when present, the full hash otherwise
  FullHash,      // always the full hash (the fallback, and the test hook for it)
};

// The compiler build id of the executable at `path`: `uuid-<hex>` for a linker identifier and `sha256-<64 hex>` for
// the SHA-256 of the whole file. The prefix keeps an id of one form from ever equaling an id of the other, so an
// archive written under one is refused by a compiler that computes the other. False, with `error` set, when the file
// cannot be read.
bool executableBuildId(const std::string &path,
                       BuildIdSource source,
                       std::string &id,
                       std::string &error);

} // namespace primec
