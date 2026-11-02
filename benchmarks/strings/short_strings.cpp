#include <cstdio>
#include <string>
int main() {
  int total = 0;
  for (int i = 0; i < 1000000; ++i) {
    std::string s("GET");
    s += " /index.html";
    volatile const char *keep = s.c_str();
    (void)keep;
    total += static_cast<int>(s.size()) + static_cast<unsigned char>(s[1]);
  }
  std::printf("%d\n", total);
}
