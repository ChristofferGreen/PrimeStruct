#include <cstdio>
#include <string>
int main() {
  std::string source;
  for (int i = 0; i < 1000; ++i)
    source.push_back(static_cast<char>(97 + i - i / 26 * 26));
  std::string target;
  target.reserve(4000);
  int total = 0;
  for (int round = 0; round < 1000; ++round) {
    target.clear();
    for (int i = 0; i < 1000; ++i)
      target.push_back(source[i]);
    total += static_cast<unsigned char>(target[round]);
  }
  std::printf("%d\n", total);
}
