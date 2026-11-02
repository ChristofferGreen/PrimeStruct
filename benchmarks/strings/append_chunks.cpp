#include <cstdio>
#include <string>
int main() {
  std::string s;
  for (int i = 0; i < 200000; ++i) s += "hello world, this is a longer chunk of text";
  std::printf("%d\n", static_cast<int>(s.size()) + static_cast<unsigned char>(s[4300000]));
}
