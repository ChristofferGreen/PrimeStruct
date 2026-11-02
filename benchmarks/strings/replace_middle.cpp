#include <cstdio>
#include <string>
int main() {
  std::string s;
  for (int i = 0; i < 2000; ++i) s.push_back(static_cast<char>(97 + i - i / 26 * 26));
  for (int i = 0; i < 50000; ++i) {
    s.replace(100, 4, "abcdefgh");
    s.replace(100, 8, "wxyz");
  }
  std::printf("%d\n", static_cast<int>(s.size()) + static_cast<unsigned char>(s[101]));
}
