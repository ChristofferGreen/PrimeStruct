#include <cstdio>
#include <string>
int main() {
  std::string s;
  for (int i = 0; i < 20000; ++i) s.insert(0, "x");
  std::printf("%d\n", static_cast<int>(s.size()) + static_cast<unsigned char>(s[19999]));
}
