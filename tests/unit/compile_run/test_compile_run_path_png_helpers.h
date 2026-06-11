#pragma once

#include "third_party/doctest.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

inline std::string escapeStringLiteral(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    if (c == '\\' || c == '"') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}

inline uint32_t computePngCrc(const unsigned char *data, size_t size) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t index = 0; index < size; ++index) {
    crc ^= static_cast<uint32_t>(data[index]);
    for (int bit = 0; bit < 8; ++bit) {
      if ((crc & 1u) != 0u) {
        crc = (crc >> 1u) ^ 0xEDB88320u;
      } else {
        crc >>= 1u;
      }
    }
  }
  return ~crc;
}

inline bool isPngChunkTypeByte(unsigned char byte) {
  return std::isalpha(static_cast<unsigned char>(byte)) != 0;
}

inline bool hasPlausiblePngChunkHeader(const std::vector<unsigned char> &bytes,
                                       size_t offset) {
  if (offset + 8u > bytes.size()) {
    return false;
  }
  if (!isPngChunkTypeByte(bytes[offset + 4u]) ||
      !isPngChunkTypeByte(bytes[offset + 5u]) ||
      !isPngChunkTypeByte(bytes[offset + 6u]) ||
      !isPngChunkTypeByte(bytes[offset + 7u])) {
    return false;
  }
  const uint32_t length = (static_cast<uint32_t>(bytes[offset]) << 24u) |
                          (static_cast<uint32_t>(bytes[offset + 1u]) << 16u) |
                          (static_cast<uint32_t>(bytes[offset + 2u]) << 8u) |
                          static_cast<uint32_t>(bytes[offset + 3u]);
  return offset + static_cast<size_t>(length) + 12u <= bytes.size();
}

inline bool repairPngChunkBoundary(std::vector<unsigned char> &bytes, size_t offset) {
  if (hasPlausiblePngChunkHeader(bytes, offset)) {
    return true;
  }

  for (size_t zerosToErase = 1; zerosToErase <= 4u && offset + zerosToErase <= bytes.size();
       ++zerosToErase) {
    if (!std::all_of(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                     bytes.begin() + static_cast<std::ptrdiff_t>(offset + zerosToErase),
                     [](unsigned char byte) { return byte == 0u; })) {
      break;
    }
    std::vector<unsigned char> candidate = bytes;
    candidate.erase(candidate.begin() + static_cast<std::ptrdiff_t>(offset),
                    candidate.begin() + static_cast<std::ptrdiff_t>(offset + zerosToErase));
    if (hasPlausiblePngChunkHeader(candidate, offset)) {
      bytes = std::move(candidate);
      return true;
    }
  }

  for (size_t zerosToInsert = 1; zerosToInsert <= 4u; ++zerosToInsert) {
    std::vector<unsigned char> candidate = bytes;
    candidate.insert(candidate.begin() + static_cast<std::ptrdiff_t>(offset),
                     zerosToInsert, 0u);
    if (hasPlausiblePngChunkHeader(candidate, offset)) {
      bytes = std::move(candidate);
      return true;
    }
  }

  return false;
}

inline std::vector<unsigned char> withValidPngCrcs(std::vector<unsigned char> bytes) {
  if (bytes.size() < 8) {
    return bytes;
  }

  size_t offset = 8;
  while (offset < bytes.size()) {
    const bool repaired = repairPngChunkBoundary(bytes, offset);
    CHECK(repaired);
    if (!repaired) {
      return bytes;
    }
    if (offset + 12u > bytes.size()) {
      CHECK(offset == bytes.size());
      return bytes;
    }

    const uint32_t length = (static_cast<uint32_t>(bytes[offset]) << 24u) |
                            (static_cast<uint32_t>(bytes[offset + 1u]) << 16u) |
                            (static_cast<uint32_t>(bytes[offset + 2u]) << 8u) |
                            static_cast<uint32_t>(bytes[offset + 3u]);
    const size_t chunkSize = static_cast<size_t>(length) + 12u;
    CHECK(offset + chunkSize <= bytes.size());
    if (offset + chunkSize > bytes.size()) {
      return bytes;
    }

    const uint32_t crc =
        computePngCrc(bytes.data() + offset + 4u, static_cast<size_t>(length) + 4u);
    const size_t crcOffset = offset + 8u + static_cast<size_t>(length);
    bytes[crcOffset] = static_cast<unsigned char>((crc >> 24u) & 0xFFu);
    bytes[crcOffset + 1] = static_cast<unsigned char>((crc >> 16u) & 0xFFu);
    bytes[crcOffset + 2] = static_cast<unsigned char>((crc >> 8u) & 0xFFu);
    bytes[crcOffset + 3] = static_cast<unsigned char>(crc & 0xFFu);
    offset += chunkSize;
  }

  return bytes;
}

inline std::vector<unsigned char> withCorruptedFirstPngChunkCrc(
    std::vector<unsigned char> bytes) {
  bytes = withValidPngCrcs(bytes);
  if (bytes.size() >= 33) {
    bytes[29] ^= 0xFFu;
  }
  return bytes;
}

inline std::string injectEscapedPath(std::string source, const std::string &escapedPath) {
  const std::string needle = "__PATH__";
  const size_t pos = source.find(needle);
  CHECK(pos != std::string::npos);
  if (pos == std::string::npos) {
    return source;
  }
  source.replace(pos, needle.size(), escapedPath);
  return source;
}
