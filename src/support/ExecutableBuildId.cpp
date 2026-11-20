#include "primec/support/ExecutableBuildId.h"

#include "primec/support/Sha256.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>

namespace primec {

namespace {

// Reads `size` bytes at `offset` into `out`; false when the image is shorter.
using ImageReader = std::function<bool(uint64_t offset, size_t size, std::string &out)>;

// The largest header area or note segment read; real ones are well under a kilobyte.
constexpr size_t MaxHeaderBytes = 1u << 20;
constexpr size_t MaxNoteBytes = 1u << 16;

std::string hexOf(const char *bytes, size_t size) {
  static const char Digits[] = "0123456789abcdef";
  std::string out;
  out.reserve(size * 2);
  for (size_t i = 0; i < size; ++i) {
    const unsigned char byte = static_cast<unsigned char>(bytes[i]);
    out.push_back(Digits[byte >> 4]);
    out.push_back(Digits[byte & 15]);
  }
  return out;
}

struct ByteOrder {
  bool little = true;
  uint64_t get(const std::string &buffer, size_t offset, size_t width) const {
    uint64_t value = 0;
    for (size_t i = 0; i < width; ++i) {
      const uint64_t byte =
          static_cast<unsigned char>(buffer[offset + (little ? i : width - 1 - i)]);
      value |= byte << (8 * i);
    }
    return value;
  }
};

std::string machoUuid(const ImageReader &read, const std::string &head) {
  constexpr uint64_t MhMagic64 =
      0xfeedfacf; // little-endian 64-bit, the only Mach-O this compiler is built as
  constexpr uint64_t LcUuid = 0x1b;
  constexpr size_t HeaderSize = 32;
  const ByteOrder order;
  if (head.size() < HeaderSize || order.get(head, 0, 4) != MhMagic64) {
    return "";
  }
  const uint64_t commandCount = order.get(head, 16, 4);
  const uint64_t commandBytes = order.get(head, 20, 4);
  if (commandBytes == 0 || commandBytes > MaxHeaderBytes) {
    return "";
  }
  std::string commands;
  if (!read(HeaderSize, static_cast<size_t>(commandBytes), commands)) {
    return "";
  }
  size_t at = 0;
  for (uint64_t i = 0; i < commandCount && at + 8 <= commands.size(); ++i) {
    const uint64_t kind = order.get(commands, at, 4);
    const uint64_t size = order.get(commands, at + 4, 4);
    if (size < 8 || at + size > commands.size()) {
      return "";
    }
    if (kind == LcUuid) {
      return size >= 24 ? hexOf(commands.data() + at + 8, 16) : "";
    }
    at += static_cast<size_t>(size);
  }
  return "";
}

// The GNU build-id of one `PT_NOTE` segment: notes of (namesz, descsz, type, name, desc), name and desc padded to 4.
std::string gnuBuildIdInNotes(const std::string &notes, const ByteOrder &order) {
  constexpr uint64_t NtGnuBuildId = 3;
  size_t at = 0;
  while (at + 12 <= notes.size()) {
    const uint64_t nameSize = order.get(notes, at, 4);
    const uint64_t descSize = order.get(notes, at + 4, 4);
    const uint64_t type = order.get(notes, at + 8, 4);
    const uint64_t nameAt = at + 12;
    const uint64_t descAt = nameAt + ((nameSize + 3) & ~uint64_t(3));
    const uint64_t next = descAt + ((descSize + 3) & ~uint64_t(3));
    if (descAt > notes.size() || descSize > notes.size() - descAt) {
      return "";
    }
    if (type == NtGnuBuildId && nameSize == 4 && nameAt + 4 <= notes.size() &&
        std::memcmp(notes.data() + nameAt, "GNU", 4) == 0 && descSize >= 8) {
      return hexOf(notes.data() + descAt, static_cast<size_t>(descSize));
    }
    at = static_cast<size_t>(next);
  }
  return "";
}

std::string elfBuildId(const ImageReader &read, const std::string &head) {
  constexpr uint64_t PtNote = 4;
  if (head.size() < 20 || std::memcmp(head.data(),
                                      "\x7f"
                                      "ELF",
                                      4) != 0) {
    return "";
  }
  const bool is64 = head[4] == 2;
  if (!is64 && head[4] != 1) {
    return "";
  }
  ByteOrder order;
  order.little = head[5] == 1;
  if (!order.little && head[5] != 2) {
    return "";
  }
  const size_t headerSize = is64 ? 64 : 52;
  if (head.size() < headerSize) {
    return "";
  }
  const uint64_t programAt = is64 ? order.get(head, 32, 8) : order.get(head, 28, 4);
  const uint64_t entrySize = order.get(head, is64 ? 54 : 42, 2);
  const uint64_t entryCount = order.get(head, is64 ? 56 : 44, 2);
  if (entrySize < (is64 ? 56u : 32u) || entryCount == 0 ||
      entryCount * entrySize > MaxHeaderBytes) {
    return "";
  }
  std::string table;
  if (!read(programAt, static_cast<size_t>(entryCount * entrySize), table)) {
    return "";
  }
  for (uint64_t i = 0; i < entryCount; ++i) {
    const size_t entry = static_cast<size_t>(i * entrySize);
    if (order.get(table, entry, 4) != PtNote) {
      continue;
    }
    const uint64_t offset = is64 ? order.get(table, entry + 8, 8) : order.get(table, entry + 4, 4);
    const uint64_t size = is64 ? order.get(table, entry + 32, 8) : order.get(table, entry + 16, 4);
    if (size == 0 || size > MaxNoteBytes) {
      continue;
    }
    std::string notes;
    if (!read(offset, static_cast<size_t>(size), notes)) {
      continue;
    }
    std::string id = gnuBuildIdInNotes(notes, order);
    if (!id.empty()) {
      return id;
    }
  }
  return "";
}

std::string linkerBuildId(const ImageReader &read) {
  std::string head;
  if (!read(0, 64, head)) {
    return "";
  }
  std::string id = machoUuid(read, head);
  return id.empty() ? elfBuildId(read, head) : id;
}

} // namespace

std::string linkerBuildIdOfBytes(std::string_view image) {
  return linkerBuildId([image](uint64_t offset, size_t size, std::string &out) {
    if (offset > image.size() || size > image.size() - offset) {
      return false;
    }
    out.assign(image.substr(static_cast<size_t>(offset), size));
    return true;
  });
}

std::string linkerBuildIdOfFile(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return "";
  }
  return linkerBuildId([&in](uint64_t offset, size_t size, std::string &out) {
    in.clear();
    in.seekg(static_cast<std::streamoff>(offset));
    out.assign(size, '\0');
    in.read(out.data(), static_cast<std::streamsize>(size));
    return in.gcount() == static_cast<std::streamsize>(size);
  });
}

bool executableBuildId(const std::string &path,
                       BuildIdSource source,
                       std::string &id,
                       std::string &error) {
  if (source == BuildIdSource::LinkerIdFirst) {
    const std::string linkerId = linkerBuildIdOfFile(path);
    if (!linkerId.empty()) {
      id = "uuid-" + linkerId;
      return true;
    }
  }
  std::string hash;
  if (!sha256FileHex(path, hash)) {
    error = "cannot read the compiler executable to identify its build: " + path;
    return false;
  }
  id = "sha256-" + hash;
  return true;
}

} // namespace primec
