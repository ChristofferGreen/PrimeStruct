#include <cstdio>
#include <string>
int main() {
  std::string s;
  for (int i = 0; i < 5000000; ++i) s.push_back(static_cast<char>(97 + i - i / 26 * 26));
  std::printf("%d\n", static_cast<int>(s.size()) + static_cast<unsigned char>(s[2500000]));
}
