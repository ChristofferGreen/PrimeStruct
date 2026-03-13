#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace primestruct::software_surface {

struct SoftwareSurfaceFrame {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> pixelsBgra8;
};

inline SoftwareSurfaceFrame makeDemoFrame(int width = 64, int height = 64) {
  SoftwareSurfaceFrame frame;
  frame.width = width;
  frame.height = height;
  frame.pixelsBgra8.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);

  const int widthDenom = width > 1 ? width - 1 : 1;
  const int heightDenom = height > 1 ? height - 1 : 1;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const size_t pixelIndex = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
      const bool checker = (((x / 8) + (y / 8)) % 2) == 0;
      const std::uint8_t blue = checker ? static_cast<std::uint8_t>(56u) : static_cast<std::uint8_t>(136u);
      const std::uint8_t green =
          static_cast<std::uint8_t>(32 + ((static_cast<unsigned>(x) * 160u) / static_cast<unsigned>(widthDenom)));
      const std::uint8_t red =
          static_cast<std::uint8_t>(48 + ((static_cast<unsigned>(y) * 176u) / static_cast<unsigned>(heightDenom)));
      frame.pixelsBgra8[pixelIndex + 0u] = blue;
      frame.pixelsBgra8[pixelIndex + 1u] = green;
      frame.pixelsBgra8[pixelIndex + 2u] = red;
      frame.pixelsBgra8[pixelIndex + 3u] = 255u;
    }
  }

  return frame;
}

inline bool validateFrame(const SoftwareSurfaceFrame &frame, std::string &errorOut) {
  if (frame.width <= 0 || frame.height <= 0) {
    errorOut = "software surface dimensions must be positive";
    return false;
  }
  const size_t expectedBytes =
      static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height) * static_cast<size_t>(4);
  if (frame.pixelsBgra8.size() != expectedBytes) {
    errorOut = "software surface byte count did not match BGRA8 dimensions";
    return false;
  }
  return true;
}

} // namespace primestruct::software_surface
