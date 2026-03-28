#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static inline std::vector<uint8_t> readBinaryFile(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

static inline std::string readTextFile(const std::filesystem::path &path) {
  std::ifstream input(path);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

static inline std::string readTextFiles(const std::vector<std::filesystem::path> &paths) {
  std::string combined;
  for (const auto &path : paths) {
    combined += readTextFile(path);
    combined.push_back('\n');
  }
  return combined;
}
