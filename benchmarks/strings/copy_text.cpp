#include <cstdio>
#include <string>
int main() {
  std::string source;
  for (int i = 0; i < 1000; ++i) source.push_back(static_cast<char>(97 + i - i / 26 * 26));
  int total = 0;
  for (int round = 0; round < 200000; ++round) {
    std::string copy = source;
    total += static_cast<int>(copy.size()) + static_cast<unsigned char>(copy[round - round / 1000 * 1000]);
  }
  std::printf("%d\n", total);
}
